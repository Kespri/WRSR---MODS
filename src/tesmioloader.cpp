// tesmioloader - mod gate for Workers & Resources: Soviet Republic
//
// This file is the host, and only the host: code execution and a virtual file
// system over the game's data, both built out of import-table swaps; the
// generic IAT and inline-hooking primitives every plugin patches through; the
// crash guard that keeps a bad offset in injected UI code from taking the game
// down silently; and the plugin loader itself, which hands each DLL in
// plugins\ a versioned table of the above (tesmio_api.h) and nothing more.
//
// It used to be more than that. The very first version of this project hooked
// the game's own resource-name resolver inline, right here, to read out the
// real resource enum and hand back a reserved slot index for names the base
// game had never heard of - the resolver found by taking the one code
// reference to "ResourceGet - not found %s" and reading the function bounds
// out of the exception table. That hook, the resource record layout behind it
// and everything deposit-shaped moved out into plugins\resources and
// plugins\deposits as the plugin architecture took shape (see
// docs/09-plugins.md); nothing resource- or deposit-specific belongs here any
// more, and none of the addresses below are resource or deposit ones. The
// per-texel deposit-map probe further down (`probe_map`, `probe_texel`) looks
// like an exception, but it is not a feature - it is a diagnostic that hooks
// raw file and texture calls no plugin API exposes, kept here because only the
// host sees those calls at all, and left off by default.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <intrin.h>
#include <tlhelp32.h>

// The save-manifest warning box is the only user32 call in the DLL; a pragma
// here keeps build.bat's kernel32-only link line untouched.
#pragma comment(lib, "user32.lib")

// This build of the loader. Upstream is MaxLegend's b0.3.6 (API 4); the suffix
// counts the rounds of fixes on top of it. The host table, the API version and
// every log line other tools read are unchanged - a plugin built for b0.3.6
// runs here and one built here runs on b0.3.6.
#define LOADER_VERSION "b0.3.6-rmm.1"

// ---------------------------------------------------------------- exported names

#define SYM_READ_FILE   "?C3DHelp_ReadFileIntoBuffer@@YAHPEBDPEAPEADPEAI_N@Z"
#define SYM_FILE_EXISTS "?C3DHelp_CheckIfFileExist@@YA_NPEBD_N1@Z"
#define SYM_LOG_INFO    "?C3DLog_PrintInfo@@YAXPEADZZ"
#define SYM_LOG_WARN    "?C3DLog_PrintWarning@@YAXPEADZZ"
#define SYM_LOG_ERROR   "?C3DLog_PrintError@@YAXPEADZZ"

#define DLL_ENGINE "C3DDLL64.dll"
#define DLL_STDIO  "api-ms-win-crt-stdio-l1-1-0.dll"

// ---------------------------------------------------------------- state

static HMODULE          g_self;
static HMODULE          g_exe;
static BYTE*            g_exeBase;
static char             g_baseDir[MAX_PATH];
static char             g_iniPath[MAX_PATH];    // baseDir\tesmioloader.ini
static char             g_vfsRoot[MAX_PATH];
// The wide originals. The ANSI copies above exist for the plugin table and the
// A-flavoured file calls; every path the loader resolves for itself starts from
// these, so an install folder with a non-ASCII character in it (a user name with
// an umlaut, a Cyrillic library path) works for the wide file hooks as well.
static wchar_t          g_baseDirW[MAX_PATH];
static wchar_t          g_vfsRootW[MAX_PATH];
static LONG             g_hooksOk, g_hooksFailed;
static CRITICAL_SECTION g_lock;
static HANDLE           g_hLog   = INVALID_HANDLE_VALUE;
static HANDLE           g_hReads = INVALID_HANDLE_VALUE;

static int  g_traceReads  = 0;
// 0 off, 1 errors, 2 errors and warnings, 3 everything the game prints. The game
// says a lot during a load - 138 info lines and 58 warnings in one ordinary start
// here - and most of it is about other people's Workshop items, which nothing in
// this process can fix. 2 keeps what a player can act on.
static int  g_logGame     = 2;
static int  g_gameRepeat  = 3;    // how often one repeating message is written out; 0 = always
static int  g_vfsEnabled  = 1;
static int  g_probeMap    = 0;    // guard-page probe for the deposit map
static int  g_saveManifest = 1;   // write/check tesmioloader.save.ini in saves
static LONG g_mapSeen     = 0;    // raised the moment a deposit map is opened
static DWORD WINAPI ProbeThread(LPVOID);
static void  AddMapCopy(BYTE* start, SIZE_T len, int stride, const char* how);
// The map is opened more than once - the engine takes it as a texture and the
// game reads it for itself, back to back. Tracking a single stream meant the
// second open overwrote the first and one of the buffers was never captured.
#define MAX_MAP_STREAMS 6
static FILE* g_mapFiles[MAX_MAP_STREAMS];
static char g_traceFilter[128] = "buildings_types";

static LONG g_nRedirects = 0;

static bool ReadablePtr(const void* p, size_t n);
static int  IniInt(const char* ini, const char* section, const char* key, int fallback);
static bool IniReadValue(const char* ini, const char* section, const char* key, char* out, size_t n);

// ---------------------------------------------------------------- logging

static void WriteTo(HANDLE h, const char* s, int len)
{
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(h, s, (DWORD)len, &w, NULL);
}

static void Logf(const char* fmt, ...)
{
    char body[4096];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, ap);
    va_end(ap);

    SYSTEMTIME t;
    GetLocalTime(&t);

    char line[4200];
    int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "[%02d:%02d:%02d.%03d] %s\r\n",
                        t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, body);
    if (n <= 0) return;

    EnterCriticalSection(&g_lock);
    WriteTo(g_hLog, line, n);
    LeaveCriticalSection(&g_lock);
}

// ---------------------------------------------------------------- repeating lines
//
// Two things flood this log, and neither is news after the first time: the VFS
// naming the same overridden file at every single open (the engine opens one
// .nmf eight times), and the game repeating a complaint for every folder it
// walks ("Failed to open ...\script.ini", 47 of them in one start, all about
// other people's Workshop items). Both are counted here and written a few times
// instead of every time; the tally goes into the log at shutdown, so nothing is
// hidden - only said once.

#define MAX_PATTERNS 512

struct Pattern
{
    unsigned key;
    int      kind;        // 0 = a message from the game, 1 = a VFS path
    long     seen;
    long     shown;
    char     sample[200];
};
static Pattern g_pattern[MAX_PATTERNS];
static int     g_patternCount;
static long    g_patternOverflow;

static unsigned HashOf(const char* s)
{
    unsigned h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

// What makes two messages the same message: the words. A word holding a path
// separator becomes <path>, a word of digits becomes #, so
//   "Failed to open C:\...\Walls2\building.ini"
//   "Failed to open media_soviet/cwc/vehicles/ty45/script.ini"
// share one shape, and "Workshop subscribed items num 684" keeps its wording.
// Returns false when nothing but placeholders is left: the game prints bare
// numbers ("1", "2", "3") as separate messages, and counting those together
// would hide them behind a shape that says nothing. Those are always written.
static bool Shape(const char* in, char* out, size_t n)
{
    size_t o = 0;
    bool   wordIsPath = false, wordIsNumber = true, inWord = false, wording = false;
    char   word[256];
    size_t w = 0;

    for (size_t i = 0; ; i++)
    {
        char c = in[i];
        bool end = (c == 0 || c == ' ' || c == '\t');
        if (!end)
        {
            if (!inWord) { inWord = true; wordIsPath = false; wordIsNumber = true; w = 0; }
            if (c == '\\' || c == '/') wordIsPath = true;
            if (c < '0' || c > '9') wordIsNumber = false;
            if (w < sizeof(word) - 1) word[w++] = c;
        }
        else if (inWord)
        {
            word[w] = 0;
            const char* put = word;
            if (wordIsPath)        put = "<path>";
            else if (wordIsNumber) put = "#";
            else                   wording = true;
            for (const char* p = put; *p && o < n - 2; p++) out[o++] = *p;
            if (o < n - 2) out[o++] = ' ';
            inWord = false;
        }
        if (c == 0) break;
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = 0;
    return wording;
}

// How often this shape has been seen, this one included. The caller writes the
// line while the answer is at or below the limit. A shape that no longer fits
// the table is always written - too loud beats silently dropped.
static long NoteRepeat(int kind, const char* key, const char* sample)
{
    unsigned h = HashOf(key);
    long seen = 1;

    EnterCriticalSection(&g_lock);
    Pattern* found = NULL;
    for (int i = 0; i < g_patternCount; i++)
        if (g_pattern[i].key == h && g_pattern[i].kind == kind) { found = &g_pattern[i]; break; }

    if (!found && g_patternCount < MAX_PATTERNS)
    {
        found = &g_pattern[g_patternCount++];
        found->key = h; found->kind = kind; found->seen = 0; found->shown = 0;
        strncpy_s(found->sample, sizeof(found->sample), sample, _TRUNCATE);
    }
    if (found) seen = ++found->seen;
    else { g_patternOverflow++; seen = 1; }
    LeaveCriticalSection(&g_lock);
    return seen;
}

static void MarkShown(int kind, const char* key)
{
    unsigned h = HashOf(key);
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_patternCount; i++)
        if (g_pattern[i].key == h && g_pattern[i].kind == kind) { g_pattern[i].shown++; break; }
    LeaveCriticalSection(&g_lock);
}

// What was counted instead of written. Called once, at shutdown.
static void ReportRepeats()
{
    long vfsFiles = 0, quietGame = 0;
    for (int i = 0; i < g_patternCount; i++)
        if (g_pattern[i].kind == 1) vfsFiles++;
        else if (g_pattern[i].seen > g_pattern[i].shown) quietGame++;

    if (vfsFiles)
        Logf("vfs      %ld file(s) redirected, %ld read(s) in total", vfsFiles, g_nRedirects);

    if (quietGame)
    {
        Logf("repeats  %ld message(s) of the game were said once and then counted:", quietGame);
        for (int i = 0; i < g_patternCount; i++)
        {
            Pattern* p = &g_pattern[i];
            if (p->kind != 0 || p->seen <= p->shown) continue;
            Logf("repeats    %ld x  %s", p->seen, p->sample);
        }
        Logf("repeats  raise game_repeat_limit in tesmioloader.ini to see every one of them");
    }
    if (g_patternOverflow)
        Logf("repeats  %ld line(s) past the %d shapes this table holds were written in full",
             g_patternOverflow, MAX_PATTERNS);
}

static void TraceRead(const char* kind, const char* path, const char* note)
{
    if (!g_traceReads || g_hReads == INVALID_HANDLE_VALUE || !path) return;
    if (g_traceReads == 1 && g_traceFilter[0] && !strstr(path, g_traceFilter)) return;

    char line[1200];
    int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "%-10s %s%s\r\n",
                        kind, path, note ? note : "");
    if (n <= 0) return;

    EnterCriticalSection(&g_lock);
    WriteTo(g_hReads, line, n);
    LeaveCriticalSection(&g_lock);
}

// ---------------------------------------------------------------- virtual file system

// A ".." step in a relative path would let a modded ini point the VFS lookup
// outside the vfs folder. The game never writes one; a mod file could. Refused,
// so the override root is exactly that - a root.
static bool HasParentStep(const char* p)
{
    for (size_t i = 0; p[i]; i++)
        if (p[i] == '.' && p[i + 1] == '.' &&
            (i == 0 || p[i - 1] == '\\' || p[i - 1] == '/') &&
            (p[i + 2] == 0 || p[i + 2] == '\\' || p[i + 2] == '/'))
            return true;
    return false;
}

static bool HasParentStepW(const wchar_t* p)
{
    for (size_t i = 0; p[i]; i++)
        if (p[i] == L'.' && p[i + 1] == L'.' &&
            (i == 0 || p[i - 1] == L'\\' || p[i - 1] == L'/') &&
            (p[i + 2] == 0 || p[i + 2] == L'\\' || p[i + 2] == L'/'))
            return true;
    return false;
}

static bool VfsResolve(const char* path, char* out, size_t n)
{
    if (!g_vfsEnabled || !g_vfsRoot[0] || !path || !path[0]) return false;
    if (path[1] == ':' || path[0] == '\\' || path[0] == '/') return false;
    if (path[0] == '.' && (path[1] == '/' || path[1] == '\\')) path += 2;
    if (HasParentStep(path)) return false;

    char rel[MAX_PATH * 2];
    size_t i = 0;
    for (; path[i] && i < sizeof(rel) - 1; i++)
        rel[i] = (path[i] == '/') ? '\\' : path[i];
    rel[i] = 0;

    if (_snprintf_s(out, n, _TRUNCATE, "%s\\%s", g_vfsRoot, rel) < 0) return false;
    DWORD a = GetFileAttributesA(out);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool VfsResolveW(const wchar_t* path, wchar_t* out, size_t n)
{
    if (!g_vfsEnabled || !g_vfsRootW[0] || !path || !path[0]) return false;
    if (path[1] == L':' || path[0] == L'\\' || path[0] == L'/') return false;
    if (path[0] == L'.' && (path[1] == L'/' || path[1] == L'\\')) path += 2;
    if (HasParentStepW(path)) return false;

    wchar_t rel[MAX_PATH * 2];
    size_t i = 0;
    for (; path[i] && i < (sizeof(rel) / sizeof(rel[0])) - 1; i++)
        rel[i] = (path[i] == L'/') ? L'\\' : path[i];
    rel[i] = 0;

    // The wide root as resolved, not the ANSI copy re-decoded: that copy is in
    // the system codepage, and decoding it as UTF-8 lost every non-ASCII path.
    if (_snwprintf_s(out, n, _TRUNCATE, L"%s\\%s", g_vfsRootW, rel) < 0) return false;

    DWORD a = GetFileAttributesW(out);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// ---------------------------------------------------------------- IAT hooking

static void** FindIatSlot(HMODULE mod, const char* dll, const char* fn)
{
    BYTE* base = (BYTE*)mod;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

    IMAGE_DATA_DIRECTORY* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress) return NULL;

    IMAGE_IMPORT_DESCRIPTOR* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir->VirtualAddress);
    for (; imp->Name; imp++)
    {
        if (_stricmp((const char*)(base + imp->Name), dll) != 0) continue;
        DWORD nameRva = imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk;
        IMAGE_THUNK_DATA* oft = (IMAGE_THUNK_DATA*)(base + nameRva);
        IMAGE_THUNK_DATA* ft  = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; oft->u1.AddressOfData; oft++, ft++)
        {
            if (IMAGE_SNAP_BY_ORDINAL(oft->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)(base + oft->u1.AddressOfData);
            if (strcmp((const char*)ibn->Name, fn) == 0) return (void**)&ft->u1.Function;
        }
    }
    return NULL;
}

static bool PatchIat(HMODULE mod, const char* dll, const char* fn,
                     void* repl, void** origOut, const char* label)
{
    void** slot = FindIatSlot(mod, dll, fn);
    if (!slot) { Logf("hook FAILED  %-22s (no import slot)", label); InterlockedIncrement(&g_hooksFailed); return false; }

    DWORD prot = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &prot))
    { Logf("hook FAILED  %-22s (VirtualProtect %lu)", label, GetLastError()); InterlockedIncrement(&g_hooksFailed); return false; }

    *origOut = *slot;
    *slot = repl;
    VirtualProtect(slot, sizeof(void*), prot, &prot);
    Logf("hook ok      %-22s orig=%p", label, *origOut);
    InterlockedIncrement(&g_hooksOk);
    return true;
}

// ---------------------------------------------------------------- inline hooking

// Places a 14-byte absolute "jmp [rip+0]" over the function entry and rebuilds
// the displaced instructions in a trampoline. Only valid because the stolen
// prologue is position independent - it is byte-compared before we touch memory.
static bool InstallInlineHook(void* target, void* detour, void** trampolineOut,
                              const BYTE* expect, size_t stolen, const char* label)
{
    // The jump written below is 14 bytes. Fewer stolen bytes than that is not a
    // tight fit, it is memory corruption: the jump runs past the prologue into
    // whatever follows, while the trampoline still returns to target+stolen -
    // which is now the middle of the jump's own address operand. The site then
    // executes a garbage instruction stream on a code path nobody was watching.
    // A whole debugging session went into one of these - see docs/07-pitfalls.md
    // - so it is checked here rather than left to the caller's arithmetic.
    if (stolen < 14)
    {
        Logf("hook FAILED  %-22s %zu bytes stolen, the jump needs 14 - refusing to patch",
             label, stolen);
        InterlockedIncrement(&g_hooksFailed);
        return false;
    }

    if (!target || !expect || !ReadablePtr(target, stolen))
    {
        Logf("hook FAILED  %-22s target %p is not readable - refusing to patch", label, target);
        InterlockedIncrement(&g_hooksFailed);
        return false;
    }

    if (memcmp(target, expect, stolen) != 0)
    {
        Logf("hook FAILED  %-22s prologue mismatch - wrong game build, refusing to patch", label);
        BYTE* t = (BYTE*)target;
        char hex[128]; int o = 0;
        for (size_t i = 0; i < stolen && o < 120; i++) o += _snprintf_s(hex + o, sizeof(hex) - o, _TRUNCATE, "%02X ", t[i]);
        Logf("             found: %s", hex);
        InterlockedIncrement(&g_hooksFailed);
        return false;
    }

    // Dynamically allocate enough space for stolen bytes plus the 14-byte jump back.
    // This avoids any fixed-size buffer overflow risk.
    BYTE* tramp = (BYTE*)VirtualAlloc(NULL, stolen + 14, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) { Logf("hook FAILED  %-22s (VirtualAlloc %lu)", label, GetLastError()); InterlockedIncrement(&g_hooksFailed); return false; }

    memcpy(tramp, target, stolen);
    BYTE* back = tramp + stolen;
    back[0] = 0xFF; back[1] = 0x25;                       // jmp qword ptr [rip+0]
    *(DWORD*)(back + 2) = 0;
    *(void**)(back + 6) = (BYTE*)target + stolen;

    DWORD prot = 0;
    if (!VirtualProtect(target, stolen, PAGE_EXECUTE_READWRITE, &prot))
    {
        Logf("hook FAILED  %-22s (VirtualProtect %lu)", label, GetLastError());
        VirtualFree(tramp, 0, MEM_RELEASE);
        InterlockedIncrement(&g_hooksFailed);
        return false;
    }

    BYTE* p = (BYTE*)target;
    p[0] = 0xFF; p[1] = 0x25;
    *(DWORD*)(p + 2) = 0;
    *(void**)(p + 6) = detour;
    for (size_t i = 14; i < stolen; i++) p[i] = 0x90;     // pad, keeps disassembly sane

    VirtualProtect(target, stolen, prot, &prot);
    FlushInstructionCache(GetCurrentProcess(), target, stolen);

    // The trampoline is finished code now; nothing writes it again, so it does
    // not stay writable for the life of the process.
    {
        DWORD tp = 0;
        VirtualProtect(tramp, stolen + 14, PAGE_EXECUTE_READ, &tp);
        FlushInstructionCache(GetCurrentProcess(), tramp, stolen + 14);
    }

    *trampolineOut = tramp;
    Logf("hook ok      %-22s target=%p tramp=%p (%zu bytes stolen)", label, target, tramp, stolen);
    InterlockedIncrement(&g_hooksOk);
    return true;
}

// ---------------------------------------------------------------- small helpers

static void Trim(char* s)
{
    char* p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    for (char* e = s + strlen(s); e > s; e--)
    {
        char c = e[-1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        e[-1] = 0;
    }
}

// Executable memory within +-2GB of an anchor, which is as far as a `call
// rel32` or a `jmp rel32` reaches. Walk allocation granularity outward from the
// anchor until VirtualAlloc takes a hinted address. Never freed - a cave holds
// live code for as long as the process runs.
static BYTE* AllocNear(BYTE* anchor, SIZE_T size)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    SIZE_T gran = si.dwAllocationGranularity;

    for (SIZE_T delta = gran; delta < 0x30000000; delta += gran)
    {
        for (int dir = 0; dir < 2; dir++)
        {
            BYTE* want = dir ? anchor + delta : anchor - delta;
            void* got = VirtualAlloc(want, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (got) return (BYTE*)got;
        }
    }
    return NULL;
}

// ---------------------------------------------------------------- safe pointer reads

static bool SafeReadStr(const void* p, char* out, size_t n)
{
    if (!out || n == 0) return false;

    out[0] = 0;
    if (!p) return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;

    const char* s = (const char*)p;
    size_t room = (size_t)(((BYTE*)mbi.BaseAddress + mbi.RegionSize) - (BYTE*)p);
    size_t lim  = (room < n - 1) ? room : n - 1;

    size_t i = 0;
    for (; i < lim; i++)
    {
        char c = s[i];
        if (c == 0) break;
        if ((unsigned char)c < 32 || (unsigned char)c > 126) return false;  // not a plain name
        out[i] = c;
    }
    out[i] = 0;
    return i > 0;
}

// ---------------------------------------------------------------- hooked engine calls

typedef int  (__cdecl* t_ReadFileIntoBuffer)(const char*, char**, unsigned int*, bool);
typedef bool (__cdecl* t_CheckIfFileExist)(const char*, bool, bool);
typedef void (__cdecl* t_LogPrint)(char*, ...);

static t_ReadFileIntoBuffer o_ReadFile;
static t_CheckIfFileExist   o_FileExists;
static t_LogPrint           o_LogInfo, o_LogWarn, o_LogError;

// The guard-page probe needs a thread, and DllMain runs under the loader lock,
// where CreateThread deadlocks. The first asset read is the earliest point that
// is safely outside it - and early enough, because the deposit maps are not
// opened until a world loads. This used to hang off the resource hook, which is
// no longer in the loader.
static void StartMapProbeOnce()
{
    if (!g_probeMap) return;
    static LONG started = 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) return;
    HANDLE h = CreateThread(NULL, 0, ProbeThread, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

static int __cdecl h_ReadFileIntoBuffer(const char* path, char** buf, unsigned int* size, bool flag)
{
    StartMapProbeOnce();
    char over[MAX_PATH * 2];
    if (VfsResolve(path, over, sizeof(over)))
    {
        InterlockedIncrement(&g_nRedirects);
        if (NoteRepeat(1, path, path) == 1) Logf("vfs  read   %s", path);
        return o_ReadFile(over, buf, size, flag);
    }
    TraceRead("read", path, NULL);
    return o_ReadFile(path, buf, size, flag);
}

static bool __cdecl h_CheckIfFileExist(const char* path, bool a, bool b)
{
    char over[MAX_PATH * 2];
    if (VfsResolve(path, over, sizeof(over))) return true;
    TraceRead("exists", path, NULL);
    return o_FileExists(path, a, b);
}

// The game's own log, forwarded at the level it was asked for and with a
// repeating message written `game_repeat_limit` times rather than always. The
// original call is made either way - the game's log.html never loses a line.
#define LOG_FORWARD(HOOK, ORIG, TAG, LEVEL)                           \
    static void __cdecl HOOK(char* fmt, ...)                          \
    {                                                                 \
        char body[4096];                                              \
        va_list ap;                                                   \
        va_start(ap, fmt);                                            \
        _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, ap);         \
        va_end(ap);                                                   \
        if (g_logGame >= (LEVEL))                                     \
        {                                                             \
            if (g_gameRepeat <= 0) Logf("%s %s", TAG, body);          \
            else                                                      \
            {                                                         \
                char shape[256];                                      \
                if (!Shape(body, shape, sizeof(shape)))               \
                    Logf("%s %s", TAG, body);                         \
                else if (NoteRepeat(0, shape, shape) <= g_gameRepeat) \
                {                                                     \
                    MarkShown(0, shape);                              \
                    Logf("%s %s", TAG, body);                         \
                }                                                     \
            }                                                         \
        }                                                             \
        if (ORIG) ORIG((char*)"%s", body);                            \
    }

LOG_FORWARD(h_LogInfo,  o_LogInfo,  "game.info ", 3)
LOG_FORWARD(h_LogWarn,  o_LogWarn,  "game.WARN ", 2)
LOG_FORWARD(h_LogError, o_LogError, "game.ERROR", 1)


// ---------------------------------------------------------------- hooked CRT opens

// The save manifest lives with the plugin loader, far below; the open hooks
// report every stats.ini they see, and it does the rest.
static void NoteSaveOpen(const char* path, bool writing);
static void NoteSaveOpenW(const wchar_t* path, bool writing);

typedef FILE*   (__cdecl* t_fopen)(const char*, const char*);
typedef errno_t (__cdecl* t_fopen_s)(FILE**, const char*, const char*);
typedef FILE*   (__cdecl* t_wfopen)(const wchar_t*, const wchar_t*);
typedef errno_t (__cdecl* t_wfopen_s)(FILE**, const wchar_t*, const wchar_t*);

static t_fopen    o_fopen;
static t_fopen_s  o_fopen_s;
static t_wfopen   o_wfopen;
static t_wfopen_s o_wfopen_s;

static FILE* __cdecl h_fopen(const char* path, const char* mode)
{
    bool isMap = (path && strstr(path, "resourcemap") != NULL);
    if (isMap) InterlockedExchange(&g_mapSeen, 1);

    if (mode && (mode[0] == 'w' || mode[0] == 'a')) NoteSaveOpen(path, true);
    else if (mode && mode[0] == 'r') NoteSaveOpen(path, false);

    char over[MAX_PATH * 2];
    FILE* f;
    if (mode && mode[0] == 'r' && VfsResolve(path, over, sizeof(over)))
    {
        InterlockedIncrement(&g_nRedirects);
        if (NoteRepeat(1, path, path) == 1) Logf("vfs  fopen  %s", path);
        f = o_fopen(over, mode);
    }
    else
    {
        TraceRead("fopen", path, NULL);
        f = o_fopen(path, mode);
    }

    // Remembering the stream lets the fread hook hand us the destination buffer
    // outright - far better than hunting for it afterwards, which took long
    // enough that the game had already finished with it.
    if (isMap && f)
    {
        EnterCriticalSection(&g_lock);
        for (int i = 0; i < MAX_MAP_STREAMS; i++)
            if (!g_mapFiles[i]) { g_mapFiles[i] = f; break; }
        LeaveCriticalSection(&g_lock);
    }
    return f;
}

typedef size_t (__cdecl* t_fread)(void*, size_t, size_t, FILE*);
static t_fread o_fread;

static size_t __cdecl h_fread(void* buf, size_t sz, size_t cnt, FILE* f)
{
    size_t r = o_fread(buf, sz, cnt, f);

    // The stream alone is not enough to go on: the CRT recycles FILE structures,
    // so a later file can be handed the very same pointer. Demand a payload the
    // size of a 1024x1024 BGRA image, and let go of the stream once it arrives.
    size_t bytes = sz * cnt;
    if (f && buf && bytes >= 0x400000 && bytes <= 0x410000)
    {
        bool mine = false;
        EnterCriticalSection(&g_lock);
        for (int i = 0; i < MAX_MAP_STREAMS; i++)
            if (g_mapFiles[i] == f) { g_mapFiles[i] = NULL; mine = true; break; }
        LeaveCriticalSection(&g_lock);

        if (mine) AddMapCopy((BYTE*)buf, bytes, 4, "read straight from the file");
    }
    return r;
}

static errno_t __cdecl h_fopen_s(FILE** f, const char* path, const char* mode)
{
    if (mode && (mode[0] == 'w' || mode[0] == 'a')) NoteSaveOpen(path, true);
    else if (mode && mode[0] == 'r') NoteSaveOpen(path, false);

    char over[MAX_PATH * 2];
    if (mode && mode[0] == 'r' && VfsResolve(path, over, sizeof(over)))
    {
        InterlockedIncrement(&g_nRedirects);
        if (NoteRepeat(1, path, path) == 1) Logf("vfs  fopen_s %s", path);
        return o_fopen_s(f, over, mode);
    }
    TraceRead("fopen_s", path, NULL);
    return o_fopen_s(f, path, mode);
}

static FILE* __cdecl h_wfopen(const wchar_t* path, const wchar_t* mode)
{
    if (mode && (mode[0] == L'w' || mode[0] == L'a')) NoteSaveOpenW(path, true);
    else if (mode && mode[0] == L'r') NoteSaveOpenW(path, false);

    wchar_t over[MAX_PATH * 2];
    if (mode && mode[0] == L'r' && VfsResolveW(path, over, MAX_PATH * 2))
    {
        InterlockedIncrement(&g_nRedirects);
        return o_wfopen(over, mode);
    }
    return o_wfopen(path, mode);
}

static errno_t __cdecl h_wfopen_s(FILE** f, const wchar_t* path, const wchar_t* mode)
{
    if (mode && (mode[0] == L'w' || mode[0] == L'a')) NoteSaveOpenW(path, true);
    else if (mode && mode[0] == L'r') NoteSaveOpenW(path, false);

    wchar_t over[MAX_PATH * 2];
    if (mode && mode[0] == L'r' && VfsResolveW(path, over, MAX_PATH * 2))
    {
        InterlockedIncrement(&g_nRedirects);
        return o_wfopen_s(f, over, mode);
    }
    return o_wfopen_s(f, path, mode);
}

// The engine opens plenty of files itself, and some of it goes through the Win32
// API rather than the CRT - textures in particular.
typedef HANDLE (WINAPI* t_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static t_CreateFileA o_CreateFileA;

static HANDLE WINAPI h_CreateFileA(LPCSTR path, DWORD access, DWORD share,
                                   LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                   DWORD flags, HANDLE tmpl)
{
    NoteSaveOpen(path, (access & GENERIC_WRITE) != 0);

    char over[MAX_PATH * 2];
    if (!(access & GENERIC_WRITE) && VfsResolve(path, over, sizeof(over)))
    {
        InterlockedIncrement(&g_nRedirects);
        if (NoteRepeat(1, path, path) == 1) Logf("vfs  CreateFileA %s", path);
        return o_CreateFileA(over, access, share, sa, disp, flags, tmpl);
    }
    TraceRead("CreateFileA", path, NULL);
    return o_CreateFileA(path, access, share, sa, disp, flags, tmpl);
}

static void TraceReadW(const char* kind, const wchar_t* path, const char* note)
{
    if (!g_traceReads || !path) return;
    char utf8[MAX_PATH * 2];
    if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), NULL, NULL)) return;
    TraceRead(kind, utf8, note);
}

// The wide-character openers. Textures come through these, which is why the
// deposit map slipped past every hook we had.
typedef HANDLE (WINAPI* t_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE (WINAPI* t_CreateFile2)(LPCWSTR, DWORD, DWORD, DWORD, LPVOID);

static t_CreateFileW o_CreateFileW;
static t_CreateFile2 o_CreateFile2;

static HANDLE WINAPI h_CreateFileW(LPCWSTR path, DWORD access, DWORD share,
                                   LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                   DWORD flags, HANDLE tmpl)
{
    NoteSaveOpenW(path, (access & GENERIC_WRITE) != 0);

    wchar_t over[MAX_PATH * 2];
    if (!(access & GENERIC_WRITE) && VfsResolveW(path, over, MAX_PATH * 2))
    {
        InterlockedIncrement(&g_nRedirects);
        TraceReadW("CreateFileW", path, "   -> VFS");
        return o_CreateFileW(over, access, share, sa, disp, flags, tmpl);
    }
    TraceReadW("CreateFileW", path, NULL);
    return o_CreateFileW(path, access, share, sa, disp, flags, tmpl);
}

static HANDLE WINAPI h_CreateFile2(LPCWSTR path, DWORD access, DWORD share,
                                   DWORD disp, LPVOID ext)
{
    NoteSaveOpenW(path, (access & GENERIC_WRITE) != 0);

    wchar_t over[MAX_PATH * 2];
    if (!(access & GENERIC_WRITE) && VfsResolveW(path, over, MAX_PATH * 2))
    {
        InterlockedIncrement(&g_nRedirects);
        TraceReadW("CreateFile2", path, "   -> VFS");
        return o_CreateFile2(over, access, share, disp, ext);
    }
    TraceReadW("CreateFile2", path, NULL);
    return o_CreateFile2(path, access, share, disp, ext);
}

// ---------------------------------------------------------------- texture texels
//
// The deposit maps are plain textures. Nothing parses their pixels: the loader
// calls CreateManagedTexture, then TextureAccessInitTempResource to get a copy
// the CPU can reach, and the game reads individual texels through the texture's
// vtable. Virtual calls never appear in an import table, which is exactly why
// every hook we had - and the guard page - saw nothing.
//
// C3DAPI_D3D11_TEXTURE vtable, C3DDLL64.dll RVA 0x187BF0:
//   [ 2] +0x10  Load2DFromFile
//   [19] +0x98  TextureAccessInitTempResource
//   [20] +0xA0  TextureAccesGetTexel(x, y) -> colour
//   [23] +0xB8  TextureAccesSetTexel(x, y, colour)
//   [36] +0x120 SaveToDDS          - how depletion survives a save
#define TEX_VTABLE_RVA   0x187BF0
#define TEX_SLOT_LOAD    2
#define TEX_SLOT_GETTEXEL 20

static int   g_probeTexel;

typedef int   (*t_Load2DFromFile)(void*, const char*, int, int, unsigned int, int);
typedef DWORD (*t_GetTexel)(void*, int, int);

static t_Load2DFromFile o_Load2DFromFile;
static t_GetTexel       o_GetTexel;

// Texture objects whose file name marked them as deposit maps.
static void* g_depositTex[4];
static char  g_depositName[4][64];
static int   g_depositCount;

static LONG  g_texelLogged;
static BYTE* g_texelCallers[16];
static int   g_texelCallerCount;

static int h_Load2DFromFile(void* self, const char* file, int a, int b, unsigned int c, int fmt)
{
    int r = o_Load2DFromFile(self, file, a, b, c, fmt);

    if (file && strstr(file, "resourcemap"))
    {
        EnterCriticalSection(&g_lock);
        if (g_depositCount < 4)
        {
            g_depositTex[g_depositCount] = self;
            const char* slash = strrchr(file, '/');
            strncpy_s(g_depositName[g_depositCount], 64, slash ? slash + 1 : file, _TRUNCATE);
            Logf("texel  deposit map texture %d = %p (%s)", g_depositCount, self, file);
            g_depositCount++;
        }
        LeaveCriticalSection(&g_lock);
    }
    return r;
}

static DWORD h_GetTexel(void* self, int x, int y)
{
    DWORD r = o_GetTexel(self, x, y);

    int which = -1;
    for (int i = 0; i < g_depositCount; i++)
        if (g_depositTex[i] == self) { which = i; break; }
    if (which < 0) return r;

    BYTE* ret = (BYTE*)_ReturnAddress();
    bool  fresh = true;

    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_texelCallerCount; i++)
        if (g_texelCallers[i] == ret) { fresh = false; break; }
    if (fresh && g_texelCallerCount < 16) g_texelCallers[g_texelCallerCount++] = ret;
    LONG n = ++g_texelLogged;
    LeaveCriticalSection(&g_lock);

    // Every distinct call site, plus the first handful of samples for context.
    if (fresh || n <= 24)
    {
        size_t rva = (size_t)(ret - g_exeBase);
        Logf("texel  %s (%d,%d) -> %08lX   %sfrom SOVIET64.exe + 0x%zX",
             g_depositName[which], x, y, r, fresh ? "NEW SITE " : "", rva);
    }
    return r;
}

static void HookTextureVtable(HMODULE engine)
{
    if (!g_probeTexel || !engine) return;

    void** vt = (void**)((BYTE*)engine + TEX_VTABLE_RVA);
    DWORD  prot = 0;
    if (!VirtualProtect(vt, 64 * sizeof(void*), PAGE_READWRITE, &prot))
    {
        Logf("texel  cannot write the texture vtable (%lu)", GetLastError());
        return;
    }

    o_Load2DFromFile = (t_Load2DFromFile)vt[TEX_SLOT_LOAD];
    o_GetTexel       = (t_GetTexel)vt[TEX_SLOT_GETTEXEL];
    vt[TEX_SLOT_LOAD]     = (void*)h_Load2DFromFile;
    vt[TEX_SLOT_GETTEXEL] = (void*)h_GetTexel;

    VirtualProtect(vt, 64 * sizeof(void*), prot, &prot);
    FlushInstructionCache(GetCurrentProcess(), vt, 64 * sizeof(void*));

    Logf("texel  vtable hooked: Load2DFromFile=%p GetTexel=%p", o_Load2DFromFile, o_GetTexel);
}

// ---------------------------------------------------------------- deposit map probe
//
// The function that samples the deposit map touches no string literal, so the
// xref trick that found everything else cannot find it. Instead we let the game
// point at it: locate the loaded map in memory by a marker baked into its unused
// alpha channel, turn those pages into guard pages, and note who faults on them.
// Each hit hands us the address of an instruction that reads the map.

#ifndef STATUS_GUARD_PAGE_VIOLATION
#define STATUS_GUARD_PAGE_VIOLATION ((DWORD)0x80000001L)
#endif

// Written into the alpha channel of resourcemap2.dds at a known pixel. Values
// are arbitrary but unlikely to occur together in real deposit data.
static const BYTE kMapMarker[16] = {
    0xA5, 0x5A, 0xC3, 0x3C, 0x99, 0x66, 0xF0, 0x0F,
    0x11, 0xEE, 0x77, 0x88, 0xB4, 0x4B, 0xD2, 0x2D
};
#define MAP_MARKER_PIXEL (1024 * 3 + 100)   // row 3, column 100

// The file is read into a staging buffer and then converted into whatever the
// game actually samples, so a single buffer is not enough: guard every copy of
// the marker we can find and let the hits say which one is live.
#define MAX_MAP_COPIES 8

struct MapCopy
{
    BYTE* from;      // page-aligned, strictly inside the buffer - guarding
    BYTE* to;        // outside it yields faults we cannot attribute, and an
    DWORD prot;      // unattributed guard violation kills the process
    int   stride;
    LONG  armed;
};
static MapCopy g_maps[MAX_MAP_COPIES];
static int     g_mapCount;

static BYTE*  g_probeSeen[32];
static int    g_probeSeenCount;

static SIZE_T g_exeSize;

static const char* ExceptionName(DWORD c)
{
    switch (c)
    {
        case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
        default:                              return NULL;
    }
}

// Turns "it crashed" into an address. Passes everything through untouched -
// this only observes, it never swallows an exception.
static LONG volatile g_inCrashHandler;
static LONG          g_crashesReported;

static void ArmMapGuard();

// A guard page fires once and disarms itself, so the instruction that tripped it
// is exactly what we want to record. Returning CONTINUE_EXECUTION re-runs it,
// this time against an ordinary page, and the game carries on unaware.
static LONG OnGuardPage(PEXCEPTION_POINTERS ep)
{
    BYTE* at = (BYTE*)ep->ExceptionRecord->ExceptionInformation[1];

    int hit = -1;
    for (int i = 0; i < g_mapCount; i++)
        if (at >= g_maps[i].from && at < g_maps[i].to) { hit = i; break; }
    if (hit < 0) return EXCEPTION_CONTINUE_SEARCH;

    BYTE* rip = (BYTE*)ep->ContextRecord->Rip;

    // Our own scanner walking these pages is not a finding. Worse, every such
    // hit disarms the guard, leaving a window in which a real read goes unseen -
    // so put it straight back.
    {
        HMODULE m = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)rip, &m) && m == g_self)
        {
            InterlockedExchange(&g_maps[hit].armed, 0);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    bool fresh = true;
    for (int i = 0; i < g_probeSeenCount; i++)
        if (g_probeSeen[i] == rip) { fresh = false; break; }

    if (fresh && g_probeSeenCount < 32)
    {
        g_probeSeen[g_probeSeenCount++] = rip;

        int    st  = g_maps[hit].stride;
        size_t off = (size_t)(at - g_maps[hit].from);
        const char* where = "?";
        size_t rva = 0;
        if (rip >= g_exeBase && rip < g_exeBase + g_exeSize) { where = "SOVIET64.exe"; rva = (size_t)(rip - g_exeBase); }
        else
        {
            HMODULE m = NULL;
            char nm[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)rip, &m) && m)
            {
                GetModuleFileNameA(m, nm, MAX_PATH);
                const char* slash = strrchr(nm, '\\');
                where = slash ? slash + 1 : nm;
                rva = (size_t)(rip - (BYTE*)m);
            }
        }

        Logf("probe  copy%d read at +0x%zX (pixel %zu, channel %d) from %s + 0x%zX",
             hit, off, off / (st ? st : 1), st == 4 ? (int)(off % 4) : 0, where, rva);
    }

    InterlockedExchange(&g_maps[hit].armed, 0);
    return EXCEPTION_CONTINUE_EXECUTION;
}

static LONG CALLBACK CrashHandler(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION &&
        ep->ExceptionRecord->NumberParameters >= 2)
        return OnGuardPage(ep);

    const char* what = ExceptionName(ep->ExceptionRecord->ExceptionCode);
    if (!what) return EXCEPTION_CONTINUE_SEARCH;

    // Faults inside our own module are the memory scanner walking off the end of
    // a region; they are caught by its __except and are not worth reporting.
    {
        HMODULE m = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)ep->ExceptionRecord->ExceptionAddress, &m) && m == g_self)
            return EXCEPTION_CONTINUE_SEARCH;
    }

    // Never report from inside a report: a fault raised by this handler would
    // otherwise re-enter it and bury the original crash under its own noise.
    if (InterlockedCompareExchange(&g_inCrashHandler, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&g_crashesReported) > 8)
    {
        InterlockedExchange(&g_inCrashHandler, 0);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    BYTE* addr = (BYTE*)ep->ExceptionRecord->ExceptionAddress;
    Logf("=== CRASH: %s at %p ===", what, addr);

    if (addr >= g_exeBase && addr < g_exeBase + g_exeSize)
        Logf("    SOVIET64.exe + 0x%zX", (size_t)(addr - g_exeBase));
    else
    {
        HMODULE m = NULL;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)addr, &m) && m)
        {
            GetModuleFileNameA(m, name, MAX_PATH);
            Logf("    %s + 0x%zX", name, (size_t)(addr - (BYTE*)m));
        }
    }

    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        ep->ExceptionRecord->NumberParameters >= 2)
    {
        void* bad = (void*)ep->ExceptionRecord->ExceptionInformation[1];
        Logf("    %s address %p",
             ep->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading", bad);

    }

    CONTEXT* c = ep->ContextRecord;
    Logf("    rax=%016llX rbx=%016llX rcx=%016llX rdx=%016llX",
         c->Rax, c->Rbx, c->Rcx, c->Rdx);
    Logf("    rsi=%016llX rdi=%016llX r8 =%016llX r9 =%016llX",
         c->Rsi, c->Rdi, c->R8, c->R9);
    Logf("    rsp=%016llX rbp=%016llX rip=%016llX", c->Rsp, c->Rbp, c->Rip);

    // The module list, so a return address or a faulting pointer can be placed
    // without the process still being there to ask. Only on the first report.
    if (g_crashesReported == 1)
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        if (snap != INVALID_HANDLE_VALUE)
        {
            MODULEENTRY32 me;
            me.dwSize = sizeof(me);
            int shown = 0;
            Logf("    --- modules ---");
            if (Module32First(snap, &me))
                do
                {
                    Logf("      %p  %08lX  %s", (void*)me.modBaseAddr,
                         (unsigned long)me.modBaseSize, me.szModule);
                } while (++shown < 96 && Module32Next(snap, &me));
            CloseHandle(snap);
        }
    }

    // Cheap stack walk: any qword on the stack pointing into the executable is
    // very likely a return address.
    Logf("    --- return addresses on the stack ---");
    {
        // Bound the walk by the stack's own allocation. Reading past it faults,
        // which is exactly what turned the last report into a cascade.
        MEMORY_BASIC_INFORMATION smbi;
        BYTE* limit = NULL;
        if (VirtualQuery((void*)c->Rsp, &smbi, sizeof(smbi)))
            limit = (BYTE*)smbi.BaseAddress + smbi.RegionSize;

        BYTE** sp = (BYTE**)c->Rsp;
        int shown = 0;
        for (int i = 0; i < 768 && shown < 16; i++)
        {
            if (limit && (BYTE*)(sp + i + 1) > limit) break;
            BYTE* v = sp[i];
            if (v >= g_exeBase && v < g_exeBase + g_exeSize)
            {
                Logf("      SOVIET64.exe + 0x%zX", (size_t)(v - g_exeBase));
                shown++;
            }
        }
    }

    Logf("=== end crash report ===");
    FlushFileBuffers(g_hLog);
    InterlockedExchange(&g_inCrashHandler, 0);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void ArmMapGuards()
{
    for (int i = 0; i < g_mapCount; i++)
    {
        MapCopy& m = g_maps[i];
        if (m.to <= m.from) continue;
        if (InterlockedCompareExchange(&m.armed, 1, 0) != 0) continue;

        DWORD old = 0;
        if (!VirtualProtect(m.from, (SIZE_T)(m.to - m.from), m.prot | PAGE_GUARD, &old))
            InterlockedExchange(&m.armed, 0);
    }
}

static bool AlreadyKnown(BYTE* start)
{
    for (int i = 0; i < g_mapCount; i++)
        if (start + 0x1000 >= g_maps[i].from && start <= g_maps[i].to) return true;
    return false;
}

static void AddMapCopy(BYTE* start, SIZE_T len, int stride, const char* how)
{
    if (!g_probeMap || g_mapCount >= MAX_MAP_COPIES) return;

    EnterCriticalSection(&g_lock);
    if (!AlreadyKnown(start))
    {
        const uintptr_t PG = 0xFFF;
        MapCopy& m = g_maps[g_mapCount];
        m.from   = (BYTE*)(((uintptr_t)start + PG) & ~PG);
        m.to     = (BYTE*)(((uintptr_t)(start + len)) & ~PG);
        m.stride = stride;
        m.armed  = 0;

        MEMORY_BASIC_INFORMATION b2;
        m.prot = VirtualQuery(m.from, &b2, sizeof(b2)) ? b2.Protect : PAGE_READWRITE;

        if (m.to > m.from)
        {
            Logf("probe  copy%d at %p (%zu bytes, stride %d, %s), guarding %p..%p",
                 g_mapCount, start, len, stride, how, m.from, m.to);
            g_mapCount++;

            // Armed here and not on the probe thread's next tick: the code that
            // converts the pixels runs immediately after the read returns, well
            // inside the 50 ms we would otherwise wait.
            DWORD old = 0;
            if (VirtualProtect(m.from, (SIZE_T)(m.to - m.from), m.prot | PAGE_GUARD, &old))
                InterlockedExchange(&m.armed, 1);
        }
    }
    LeaveCriticalSection(&g_lock);
}

// Records every copy of the map it can find. The marker lives in the alpha
// channel, so it shows up at stride 4 while the data is still interleaved as
// loaded, and at stride 1 once the channel is pulled into a plane of its own.
static int FindMapCopies()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    BYTE* p   = (BYTE*)si.lpMinimumApplicationAddress;
    BYTE* top = (BYTE*)si.lpMaximumApplicationAddress;
    int added = 0;

    while (p < top && g_mapCount < MAX_MAP_COPIES)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;

        bool usable = mbi.State == MEM_COMMIT &&
                      mbi.RegionSize >= 0x100000 &&
                      !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                      (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_WRITECOPY));

        // Never read a region we are already guarding: that trips our own trap.
        if (usable)
            for (int i = 0; i < g_mapCount; i++)
                if ((BYTE*)mbi.BaseAddress < g_maps[i].to &&
                    (BYTE*)mbi.BaseAddress + mbi.RegionSize > g_maps[i].from)
                { usable = false; break; }

        if (usable)
        {
            BYTE*  q = (BYTE*)mbi.BaseAddress;
            SIZE_T n = mbi.RegionSize;
            // memchr rather than a byte loop, and both strides checked from one
            // position. The previous version needed 39 seconds over this
            // process's few gigabytes, by which time the map had been converted
            // and the staging buffer abandoned.
            __try
            {
                BYTE* cur = q;
                BYTE* end = q + n - 64;
                while (cur < end && g_mapCount < MAX_MAP_COPIES)
                {
                    BYTE* f = (BYTE*)memchr(cur, kMapMarker[0], (size_t)(end - cur));
                    if (!f) break;

                    for (int t = 0; t < 2; t++)
                    {
                        int st = t == 0 ? 4 : 1;
                        int k  = 1;
                        for (; k < 16; k++)
                            if (f[(SIZE_T)k * st] != kMapMarker[k]) break;
                        if (k != 16) continue;

                        // The marker lives in the alpha byte, three past the
                        // start of its pixel while the data is interleaved.
                        SIZE_T back = (SIZE_T)MAP_MARKER_PIXEL * st + (st == 4 ? 3 : 0);
                        if ((SIZE_T)(f - q) < back) continue;

                        BYTE*  start = f - back;
                        SIZE_T want  = (SIZE_T)1024 * 1024 * st;
                        SIZE_T avail = (SIZE_T)(q + n - start);
                        AddMapCopy(start, want < avail ? want : avail, st, "marker scan");
                        added++;
                    }
                    cur = f + 1;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { }
        }
        p = (BYTE*)mbi.BaseAddress + mbi.RegionSize;
    }
    return added;
}

static DWORD WINAPI ProbeThread(LPVOID)
{
    // Wait on the file rather than on the clock. Scanning has to begin the
    // moment the map exists: last time it started half a minute late and by
    // then the pass that converts the raw pixels was long finished.
    for (int i = 0; i < 3000 && !g_mapSeen; i++) Sleep(100);
    if (!g_mapSeen)
    {
        Logf("probe  no deposit map was ever opened");
        return 0;
    }
    Logf("probe  deposit map opened - scanning for it in memory");

    for (int round = 0; round < 700 && g_probeSeenCount < 32; round++)
    {
        if (round < 30) FindMapCopies();   // early rounds only: the scan is slow
        ArmMapGuards();
        Sleep(round < 30 ? 50 : 400);
    }
    Logf("probe  finished: %d copies watched, %d distinct readers", g_mapCount, g_probeSeenCount);
    return 0;
}

// ---------------------------------------------------------------- guards for injected UI
//
// Everything below this point runs inside the game's own draw and input paths,
// reading structures this project mapped by inference. A wrong offset there is
// not a wrong pixel, it is a dead process - and the vectored crash handler
// deliberately ignores faults raised inside this module, so such a crash would
// leave nothing in the log at all.
//
// So every injected entry point runs under __try, and a fault logs where it
// happened and switches that feature off for the rest of the session. The game
// keeps running without the mod's addition instead of dying with it.

static LONG FaultFilter(const char* what, PEXCEPTION_POINTERS ep)
{
    BYTE* addr = (BYTE*)ep->ExceptionRecord->ExceptionAddress;
    Logf("FAULT    %s: code %08lX at %p", what,
         (unsigned long)ep->ExceptionRecord->ExceptionCode, addr);
    if (addr >= g_exeBase && addr < g_exeBase + g_exeSize)
        Logf("         SOVIET64.exe + 0x%zX", (size_t)(addr - g_exeBase));
    else
        Logf("         inside tesmioloader.dll (base %p)", g_self);
    if (ep->ExceptionRecord->NumberParameters >= 2)
        Logf("         %s of %p",
             ep->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from",
             (void*)ep->ExceptionRecord->ExceptionInformation[1]);
    return EXCEPTION_EXECUTE_HANDLER;
}

// True when the whole range is committed and readable. Used before following a
// pointer that came out of a game structure rather than out of a call we made:
// a stale resource record hands back a plausible-looking icon pointer, and
// dereferencing it is the difference between a missing icon and a crash.
//
// It has to **walk** the regions rather than trust one VirtualQuery, because a
// region is a run of pages sharing state and protection - not an allocation.
// The engine's big globals live in .data, which the image loader maps
// PAGE_WRITECOPY; the first write to a page turns it into a private
// PAGE_READWRITE one and splits the region there. So the reported RegionSize
// around any long-lived object shrinks as the game runs, and a single-query
// check on a 54 KB structure starts failing partway through a session even
// though every byte of it is perfectly readable.
//
// That is exactly what silently disabled the terrain-editor brushes: the panel
// hook kept drawing the buttons because it checks nothing, while the cursor and
// dispatch hooks - which asked whether all 0xD430 bytes of the editor object
// were readable before reading one pointer out of it - began answering no. The
// symptom was a tool that selected but had no brush.
static bool ReadablePtr(const void* p, size_t n)
{
    if (!p) return false;

    const BYTE* at   = (const BYTE*)p;
    const BYTE* want = at + n;
    while (at < want)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(at, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;

        const BYTE* end = (const BYTE*)mbi.BaseAddress + mbi.RegionSize;
        if (end <= at) return false;    // no progress: refuse rather than spin
        at = end;
    }
    return true;
}

// ---------------------------------------------------------------- plugins
//
// Everything above this line is compiled into the loader because it is
// infrastructure: the VFS, the log, the hooking primitives, and the crash
// guard. A *feature* does not have to be - resources, deposits and everything
// built on them are plugins, not part of this file.
//
// A plugin is an ordinary DLL in `plugins\`. The loader finds it, checks it was
// built against a compatible `tesmio_api.h`, and hands it a table of the things
// the loader already knows how to do - where the executable is, how to swap an
// import, how to splice a hook, how to log, and what deposits.ini declared. The
// plugin patches the game itself from there.
//
// The point is not isolation; a plugin is in the same address space and can
// corrupt the process exactly as easily as the loader can. The point is that a
// feature can be written, rebuilt and removed without touching this file, and
// that shipping one is copying a DLL.

#include "tesmio_api.h"

#define MAX_PLUGINS 64

struct LoadedPlugin
{
    HMODULE          module;
    char             file[64];
    const char*      name;
    const char*      version;
    TsmPluginStartFn start;      // optional second phase; null when absent
};
static LoadedPlugin g_plugin[MAX_PLUGINS];
static int          g_pluginCount;
static int          g_plugins = 1;
static char         g_pluginDir[MAX_PATH];

// --- the host table -------------------------------------------------------
//
// Thin adapters rather than the internals directly: the internals use C++
// types and their signatures are free to change, while everything here is
// frozen by TSM_API_VERSION.

static void**  api_FindIatSlot(void* mod, const char* dll, const char* fn)
{
    return FindIatSlot((HMODULE)mod, dll, fn);
}

static int api_PatchIat(void* mod, const char* dll, const char* fn,
                        void* detour, void** original, const char* label)
{
    return PatchIat((HMODULE)mod, dll, fn, detour, original, label) ? 1 : 0;
}

static int api_InstallInlineHook(void* target, void* detour, void** tramp,
                                 const unsigned char* expect, size_t stolen,
                                 const char* label)
{
    return InstallInlineHook(target, detour, tramp, expect, stolen, label) ? 1 : 0;
}

static unsigned char* api_AllocNear(unsigned char* anchor, size_t size)
{
    return AllocNear(anchor, size);
}

static int api_ReadablePtr(const void* p, size_t n)
{
    return ReadablePtr(p, n) ? 1 : 0;
}

static long api_FaultFilter(const char* what, void* ep)
{
    return FaultFilter(what, (PEXCEPTION_POINTERS)ep);
}

static void api_IniPath(const char* iniName, char* out, size_t n)
{
    _snprintf_s(out, n, _TRUNCATE, "%s\\%s", g_baseDir, iniName ? iniName : "tesmioloader.ini");
}

static int api_ConfigInt(const char* iniName, const char* section,
                         const char* key, int fallback)
{
    char ini[MAX_PATH];
    api_IniPath(iniName, ini, sizeof(ini));
    return GetPrivateProfileIntA(section, key, fallback, ini);
}

static int api_ConfigString(const char* iniName, const char* section,
                            const char* key, char* out, int outSize,
                            const char* fallback)
{
    char ini[MAX_PATH];
    api_IniPath(iniName, ini, sizeof(ini));
    DWORD n = GetPrivateProfileStringA(section, key, fallback ? fallback : "",
                                       out, (DWORD)outSize, ini);
    Trim(out);
    return (int)n;
}

// --- the service noticeboard ----------------------------------------------
//
// The host never looks inside an interface and has no opinion on what any of
// them mean. A name, a version and a pointer - that is the whole mechanism, and
// it is what lets `depletion` use `deposits` without either knowing the other
// exists at build time.
//
// Publishing is done from Init and looking up from Start, so by the time anyone
// asks, everyone has published.

#define MAX_SERVICES 64

struct Service
{
    char        name[32];
    unsigned    version;
    const void* iface;
    const char* provider;
};
static Service g_service[MAX_SERVICES];
static int     g_serviceCount;
static const char* g_currentPlugin = "?";

static int api_Provide(const char* name, unsigned version, const void* iface)
{
    if (!name || !iface) return 0;
    if (g_serviceCount >= MAX_SERVICES)
    {
        Logf("plugin   service \"%s\" refused - only %d fit", name, MAX_SERVICES);
        return 0;
    }
    for (int i = 0; i < g_serviceCount; i++)
        if (_stricmp(g_service[i].name, name) == 0 && g_service[i].version == version)
        {
            Logf("plugin   service \"%s\" v%u already provided by %s",
                 name, version, g_service[i].provider);
            return 0;
        }

    Service* s = &g_service[g_serviceCount++];
    strncpy_s(s->name, sizeof(s->name), name, _TRUNCATE);
    s->version  = version;
    s->iface    = iface;
    s->provider = g_currentPlugin;
    Logf("plugin   service \"%s\" v%u from %s", name, version, s->provider);
    return 1;
}

static const void* api_Consume(const char* name, unsigned version)
{
    if (!name) return NULL;
    for (int i = 0; i < g_serviceCount; i++)
        if (_stricmp(g_service[i].name, name) == 0 && g_service[i].version == version)
            return g_service[i].iface;
    return NULL;
}

// Which plugin file published a service - for the log and the save manifest.
static const char* ServiceProvider(const char* name, unsigned version)
{
    for (int i = 0; i < g_serviceCount; i++)
        if (_stricmp(g_service[i].name, name) == 0 && g_service[i].version == version)
            return g_service[i].provider;
    return NULL;
}

// A plugin that published in Init and then declined is about to be unloaded.
// Its interfaces point into a DLL that will not be there any more, so they come
// off the noticeboard first - a consume() after FreeLibrary would otherwise
// hand out a pointer into unmapped memory.
static void DropServices(const char* provider)
{
    for (int i = 0; i < g_serviceCount; )
    {
        if (_stricmp(g_service[i].provider, provider) == 0)
        {
            Logf("plugin   service \"%s\" v%u withdrawn - %s is not staying loaded",
                 g_service[i].name, g_service[i].version, provider);
            for (int j = i + 1; j < g_serviceCount; j++) g_service[j - 1] = g_service[j];
            g_serviceCount--;
        }
        else i++;
    }
}

static TsmHost g_host;

static void BuildHostTable()
{
    memset(&g_host, 0, sizeof(g_host));
    g_host.apiVersion   = TSM_API_VERSION;
    g_host.structSize   = (unsigned)sizeof(TsmHost);

    g_host.exeModule    = (void*)g_exe;
    g_host.exeBase      = g_exeBase;
    g_host.exeSize      = g_exeSize;
    g_host.engineModule = (void*)GetModuleHandleA(DLL_ENGINE);
    g_host.baseDir      = g_baseDir;
    g_host.pluginDir    = g_pluginDir;
    g_host.vfsRoot      = g_vfsRoot;

    g_host.log               = Logf;
    g_host.findIatSlot       = api_FindIatSlot;
    g_host.patchIat          = api_PatchIat;
    g_host.installInlineHook = api_InstallInlineHook;
    g_host.allocNear         = api_AllocNear;
    g_host.readablePtr       = api_ReadablePtr;
    g_host.faultFilter       = api_FaultFilter;
    g_host.configInt         = api_ConfigInt;
    g_host.configString      = api_ConfigString;
    g_host.provide           = api_Provide;
    g_host.consume           = api_Consume;
}

// --- loading --------------------------------------------------------------

// One key per DLL in the `[plugins]` section of tesmioloader.ini, keyed by the
// file name without its extension, and absent means on. That is what the
// launcher's checkboxes write, and the reason the choice is a config key rather
// than a renamed file: a plugin the user turned off is still on disk, still
// listed by the launcher, and still one keystroke from coming back.
static bool PluginEnabled(const char* file)
{
    char key[64];
    strncpy_s(key, sizeof(key), file, _TRUNCATE);
    if (char* dot = strrchr(key, '.')) *dot = 0;

    // The loader's own reader, not the profile API: a UTF-8 BOM at the top of
    // tesmioloader.ini made the profile API miss every section header, and
    // every plugin then counted as on.
    return IniInt(g_iniPath, "plugins", key, 1) != 0;
}

// The loaded list, keyed the same way PluginEnabled is - the file name
// without its extension. A DLL deleted from disk and one switched off in the
// launcher look identical from here, which is exactly right for the save
// manifest: either way the content is not in the game.
static bool PluginLoaded(const char* key)
{
    for (int i = 0; i < g_pluginCount; i++)
    {
        char file[64];
        strncpy_s(file, sizeof(file), g_plugin[i].file, _TRUNCATE);
        if (char* dot = strrchr(file, '.')) *dot = 0;
        if (_stricmp(file, key) == 0) return true;
    }
    return false;
}

static void LoadOnePlugin(const char* file)
{
    if (g_pluginCount >= MAX_PLUGINS)
    {
        Logf("plugin   \"%s\" skipped - only %d fit", file, MAX_PLUGINS);
        return;
    }

    char path[MAX_PATH];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", g_pluginDir, file);

    // The full path, and LOAD_WITH_ALTERED_SEARCH_PATH so a plugin's own
    // dependencies resolve next to it rather than out of the game folder.
    HMODULE mod = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!mod)
    {
        Logf("plugin   \"%s\" failed to load (%lu)", file, GetLastError());
        return;
    }

    TsmPluginApiVersionFn ver =
        (TsmPluginApiVersionFn)GetProcAddress(mod, TSM_EXPORT_APIVERSION);
    TsmPluginInitFn init =
        (TsmPluginInitFn)GetProcAddress(mod, TSM_EXPORT_INIT);

    if (!ver || !init)
    {
        Logf("plugin   \"%s\" is not a tesmioloader plugin - no %s/%s export",
             file, TSM_EXPORT_APIVERSION, TSM_EXPORT_INIT);
        FreeLibrary(mod);
        return;
    }

    // Asked before Init, and Init is not called on a mismatch: a plugin built
    // against an incompatible header would be reading moved fields out of the
    // host table, which corrupts the process rather than misbehaving.
    //
    // A RANGE, not an equality. TSM_API_VERSION_MIN is the oldest plugin this
    // host still honours, and a change to tesmio_api.h that did not have to
    // break an old plugin is not allowed to - see the rule spelled out there.
    // So a plugin built against an older but still-compatible header loads
    // exactly as it always did. Newer than this host is still refused, because
    // it may read a field off the end of the table it is handed.
    unsigned got = 0;
    __try { got = ver(); }
    __except (FaultFilter("plugin api version", GetExceptionInformation())) { got = 0; }

    if (got < TSM_API_VERSION_MIN || got > TSM_API_VERSION)
    {
        Logf("plugin   \"%s\" reports API %u, this loader takes %u..%u - not initialised",
             file, got, TSM_API_VERSION_MIN, TSM_API_VERSION);
        FreeLibrary(mod);
        return;
    }
    if (got != TSM_API_VERSION)
        Logf("plugin   \"%s\" built against API %u, running on %u",
             file, got, TSM_API_VERSION);

    // Reserved before Init runs, so anything the plugin publishes is credited
    // to it in the log.
    LoadedPlugin* p = &g_plugin[g_pluginCount];
    memset(p, 0, sizeof(*p));
    p->module = mod;
    strncpy_s(p->file, sizeof(p->file), file, _TRUNCATE);
    p->name    = p->file;
    p->version = "";
    g_currentPlugin = p->file;

    TsmPluginInfo info;
    memset(&info, 0, sizeof(info));
    int rc = 1;
    __try { rc = init(&g_host, &info); }
    __except (FaultFilter("plugin init", GetExceptionInformation())) { rc = 1; }
    g_currentPlugin = "?";

    if (info.name)    p->name    = info.name;
    if (info.version) p->version = info.version;

    if (rc != 0)
    {
        // Only legitimate when nothing was hooked - a plugin that installed an
        // inline hook can never be unloaded, because the game would jump into
        // freed memory. Any service it published points into the DLL, so those
        // go too, which is why FreeLibrary only happens on this path.
        Logf("plugin   \"%s\" declined to install (%d)", file, rc);
        DropServices(p->file);
        FreeLibrary(mod);
        return;
    }

    p->start = (TsmPluginStartFn)GetProcAddress(mod, TSM_EXPORT_START);
    g_pluginCount++;

    Logf("plugin   %-16s %-8s from %s", p->name, p->version, file);
}

// Second phase. Every plugin's Init has run, so every service that will ever
// exist is on the noticeboard and consume() can see all of it.
static void StartPlugins()
{
    for (int i = 0; i < g_pluginCount; i++)
    {
        LoadedPlugin* p = &g_plugin[i];
        if (!p->start) continue;

        int rc = 0;
        g_currentPlugin = p->file;
        __try { rc = p->start(); }
        __except (FaultFilter("plugin start", GetExceptionInformation())) { rc = -1; }
        g_currentPlugin = "?";

        if (rc != 0) Logf("plugin   %s started with %d - inactive", p->name, rc);
    }
}

#define MAX_PLUGIN_FILES 128

static int __cdecl CompareNames(const void* a, const void* b)
{
    return _stricmp((const char*)a, (const char*)b);
}

static void LoadPlugins()
{
    _snprintf_s(g_pluginDir, sizeof(g_pluginDir), _TRUNCATE, "%s\\plugins", g_baseDir);

    DWORD attr = GetFileAttributesA(g_pluginDir);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        Logf("plugin   no plugins folder at %s", g_pluginDir);
        return;
    }

    BuildHostTable();

    char pattern[MAX_PATH];
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*.dll", g_pluginDir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        Logf("plugin   no plugins in %s", g_pluginDir);
        return;
    }

    // Name order, sorted here rather than trusted: FindFirstFile is alphabetical
    // on NTFS and on nothing else, and the order is a promise plugins rely on
    // (resources_plus has to init before soviet_mod_loader, for one).
    static char names[MAX_PLUGIN_FILES][64];
    int count = 0;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (count >= MAX_PLUGIN_FILES)
        {
            Logf("plugin   \"%s\" not considered - only %d files fit", fd.cFileName, MAX_PLUGIN_FILES);
            continue;
        }
        strncpy_s(names[count++], sizeof(names[0]), fd.cFileName, _TRUNCATE);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    qsort(names, (size_t)count, sizeof(names[0]), CompareNames);

    for (int i = 0; i < count; i++)
    {
        if (!PluginEnabled(names[i]))
        {
            Logf("plugin   %-16s off in tesmioloader.ini [plugins]", names[i]);
            continue;
        }
        LoadOnePlugin(names[i]);
    }

    StartPlugins();
    Logf("plugin   %d loaded", g_pluginCount);
}

// ---------------------------------------------------------------- save manifest
//
// A save made with modded content does not load without it: the resource
// count is part of the save format, an unknown deposit type has no sampler,
// and a missing Workshop building is an id nothing declares. The game dies
// halfway through such a load without a word about why. The formats cannot be
// made compatible, but the save can say what it expects - one small ini next
// to stats.ini, written when the game saves and checked when it reads one
// back:
//
//   ...\save\<name>\tesmioloader.save.ini
//
// The check lives here in the host rather than in any plugin for the one
// reason that matters: it has to fire exactly when the plugin is OFF.

#define SAVE_MANIFEST_NAME "tesmioloader.save.ini"

// ...\save\<name>\stats.ini -> the <name> folder. Both separator styles; the
// game opens saves through relative and absolute paths alike.
static bool SaveDirOf(const char* path, char* out, size_t n)
{
    if (!path) return false;
    size_t len = strlen(path);
    if (len < 12 || len >= n) return false;
    if (_stricmp(path + len - 9, "stats.ini") != 0) return false;
    if (path[len - 10] != '\\' && path[len - 10] != '/') return false;

    memcpy(out, path, len - 10);
    out[len - 10] = 0;

    // The folder above <name> must be "save".
    char* name = out + (len - 10);
    while (name > out && name[-1] != '\\' && name[-1] != '/') name--;
    if (name - out < 6) return false;
    char* above = name - 1;                              // separator before <name>
    return _strnicmp(above - 4, "save", 4) == 0 &&
           (above - 4 == out || above[-5] == '\\' || above[-5] == '/');
}

// <game> out of ...\save\<name>, three levels up (<name>, save, media_soviet).
// The buildings check needs it because the game loads workshop_wip itself.
static bool GameDirOf(const char* saveDir, char* out, size_t n)
{
    if (_snprintf_s(out, n, _TRUNCATE, "%s", saveDir) < 0) return false;
    for (int up = 0; up < 3; up++)
    {
        char* s1 = strrchr(out, '\\');
        char* s2 = strrchr(out, '/');
        char* s  = s1 > s2 ? s1 : s2;
        if (!s || s == out) return false;
        *s = 0;
    }
    return true;
}

// The loader's own ini reader. The Win32 profile API is what plugins get
// through the host table and it stays that way; the loader reads its own files
// here because the profile API trips over a UTF-8 BOM (the section header does
// not match and every key silently falls back) and cannot list the keys of a
// section without the buffer dance. Whole file into memory, BOM skipped,
// `;` and `#` lines ignored, keys compared without case, values trimmed and
// stripped of one pair of quotes - the same rules the profile API applies.
//
// Returns the malloc'd buffer (free it) and points `text` past the BOM, or
// NULL when the file is missing, empty, unreadable or over 4 MB.
static char* LoadIniText(const char* ini, char** text)
{
    *text = NULL;
    if (!ini || !*ini) return NULL;

    HANDLE h = CreateFileA(ini, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    LARGE_INTEGER sz = { 0 };
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 4 * 1024 * 1024)
    {
        CloseHandle(h);
        return NULL;
    }

    char* buf = (char*)malloc((size_t)sz.QuadPart + 1);
    if (!buf) { CloseHandle(h); return NULL; }

    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, (DWORD)sz.QuadPart, &got, NULL);
    CloseHandle(h);
    if (!ok) { free(buf); return NULL; }
    buf[got] = 0;

    char* first = buf;
    if (got >= 3 &&
        (unsigned char)first[0] == 0xEF &&
        (unsigned char)first[1] == 0xBB &&
        (unsigned char)first[2] == 0xBF)
        first += 3;

    *text = first;
    return buf;
}

// One value of one key. False when the file, the section or the key is absent.
static bool IniReadValue(const char* ini, const char* section, const char* key, char* out, size_t n)
{
    if (!out || n == 0) return false;
    out[0] = 0;
    if (!section || !key || !*key) return false;

    char* text = NULL;
    char* buf  = LoadIniText(ini, &text);
    if (!buf) return false;

    bool inWanted = false, found = false;
    char* ctx = NULL;
    for (char* line = strtok_s(text, "\n", &ctx); line; line = strtok_s(NULL, "\n", &ctx))
    {
        Trim(line);
        if (!line[0] || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[')
        {
            char* close = strchr(line + 1, ']');
            if (!close) { inWanted = false; continue; }
            *close = 0;
            char* name = line + 1;
            Trim(name);
            inWanted = (_stricmp(name, section) == 0);
            continue;
        }
        if (!inWanted) continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        Trim(line);
        if (_stricmp(line, key) != 0) continue;

        char* v = eq + 1;
        Trim(v);
        size_t len = strlen(v);
        if (len >= 2 && ((v[0] == '"' && v[len - 1] == '"') || (v[0] == '\'' && v[len - 1] == '\'')))
        { v[len - 1] = 0; v++; }
        strncpy_s(out, n, v, _TRUNCATE);
        found = true;
        break;
    }
    free(buf);
    return found;
}

// An integer the way GetPrivateProfileInt reads one: leading digits with an
// optional sign, anything else is the fallback. Hex with 0x is taken as well.
static int IniInt(const char* ini, const char* section, const char* key, int fallback)
{
    char v[64];
    if (!IniReadValue(ini, section, key, v, sizeof(v)) || !v[0]) return fallback;
    char* end = NULL;
    long x = strtol(v, &end, 0);
    if (end == v) return fallback;
    return (int)x;
}

// Reads only the KEY names of one INI section without relying on the Win32
// profile API. Returns the number of keys copied to `out`. Each slot is
// `stride` bytes.
static int ReadIniSectionKeys(const char* ini, const char* wantedSection,
                              char* out, int maxKeys, int stride)
{
    if (!ini || !wantedSection || !out || maxKeys <= 0 || stride <= 1)
        return 0;

    char* first = NULL;
    char* buf   = LoadIniText(ini, &first);
    if (!buf) return 0;

    bool inWanted = false;
    int count = 0;
    char* ctx = NULL;

    for (char* line = strtok_s(first, "\n", &ctx);
         line;
         line = strtok_s(NULL, "\n", &ctx))
    {
        Trim(line);
        if (!line[0] || line[0] == ';' || line[0] == '#')
            continue;

        if (line[0] == '[')
        {
            char* close = strchr(line + 1, ']');
            if (!close)
            {
                inWanted = false;
                continue;
            }

            *close = 0;
            char* section = line + 1;
            Trim(section);
            inWanted = (_stricmp(section, wantedSection) == 0);
            continue;
        }

        if (!inWanted)
            continue;

        char* eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = 0;
        Trim(line);
        if (!line[0])
            continue;

        bool duplicate = false;
        for (int i = 0; i < count; ++i)
        {
            const char* prev = out + i * stride;
            if (_stricmp(prev, line) == 0)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        if (count >= maxKeys)
            break;

        strncpy_s(out + count * stride, (size_t)stride, line, _TRUNCATE);
        ++count;
    }

    free(buf);
    return count;
}

// True when `key` names a key of `section` in `ini`.
static bool IniHasKey(const char* ini, const char* section, const char* key)
{
    if (!ini || !section || !key || !*key)
        return false;

    char keys[256][128] = {};
    int n = ReadIniSectionKeys(ini, section, &keys[0][0], 256, sizeof(keys[0]));

    for (int i = 0; i < n; ++i)
        if (_stricmp(keys[i], key) == 0)
            return true;

    return false;
}

static bool FileExistsA(const char* p)
{
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirExistsA(const char* p)
{
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// The game folder for a save path. Absolute paths climb three levels
// (<name>, save, media_soviet); the game opens most saves through relative
// paths, and then the working directory IS the game folder.
static void GameDirForSave(const char* saveDir, char* out, size_t n)
{
    if (GameDirOf(saveDir, out, n) && out[0]) return;
    bool absolute = saveDir[0] == '\\' || saveDir[0] == '/' || (saveDir[0] && saveDir[1] == ':');
    _snprintf_s(out, n, _TRUNCATE, "%s", absolute ? "" : ".");
}

// Every folder generated buildings can land in: the plugin default, and
// whatever `out` the buildings and buildings_plus configs name.
#define MAX_OUT_DIRS 4
static int CollectOutDirs(char dirs[MAX_OUT_DIRS][256])
{
    int n = 0;
    const char* candidates[4] = { "media_soviet\\workshop_wip", NULL, NULL, NULL };
    char ini[MAX_PATH], v1[256] = "", v2[256] = "", v3[256] = "";

    api_IniPath("plugins\\buildings.ini", ini, sizeof(ini));
    if (IniReadValue(ini, "buildings", "out", v1, sizeof(v1)) && v1[0]) candidates[1] = v1;
    api_IniPath("plugins\\buildings_plus.ini", ini, sizeof(ini));
    if (IniReadValue(ini, "buildings_plus", "out", v2, sizeof(v2)) && v2[0]) candidates[2] = v2;
    else if (IniReadValue(ini, "buildings", "out", v3, sizeof(v3)) && v3[0]) candidates[3] = v3;

    for (int c = 0; c < 4; c++)
    {
        const char* d = candidates[c];
        if (!d || !*d) continue;
        bool dup = false;
        for (int i = 0; i < n; i++) if (_stricmp(dirs[i], d) == 0) { dup = true; break; }
        if (dup || n >= MAX_OUT_DIRS) continue;
        strncpy_s(dirs[n++], 256, d, _TRUNCATE);
    }
    return n;
}

static bool WorkshopFolderExists(const char* gameDir, const char* outDir, const char* id)
{
    char folder[MAX_PATH * 2];
    _snprintf_s(folder, sizeof(folder), _TRUNCATE, "%s\\%s\\%s", gameDir, outDir, id);
    return DirExistsA(folder);
}

static bool AllDigits(const char* s)
{
    if (!s || !*s) return false;
    for (; *s; s++) if (*s < '0' || *s > '9') return false;
    return true;
}

// Every generated building folder the game will load with this save: a
// numbered folder under one of the out dirs that carries a generator stamp.
// buildings, Buildings Plus and Soviet Mod Loader all write one, so the folder
// on disk is the test - not any one plugin's config.
static int WriteGeneratedBuildings(const char* gameDir, const char* path)
{
    char dirs[MAX_OUT_DIRS][256];
    int nd = CollectOutDirs(dirs), written = 0;
    for (int d = 0; d < nd; d++)
    {
        char pattern[MAX_PATH * 2];
        _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\%s\\*", gameDir, dirs[d]);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (!AllDigits(fd.cFileName)) continue;
            char stamp[MAX_PATH * 2];
            _snprintf_s(stamp, sizeof(stamp), _TRUNCATE, "%s\\%s\\%s\\tesmioloader.stamp",
                        gameDir, dirs[d], fd.cFileName);
            if (!FileExistsA(stamp)) continue;
            WritePrivateProfileStringA("buildings", fd.cFileName, "1", path);
            written++;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    return written;
}

// The resources / deposits config the running provider reads: the fork's own
// file when one exists (resources_plus.ini, deposits_plus.ini), else the
// original's. Only a fallback - the live service is asked first.
static void ProviderIniPath(const char* forkIni, const char* baseIni, char* out, size_t n)
{
    api_IniPath(forkIni, out, n);
    if (FileExistsA(out)) return;
    api_IniPath(baseIni, out, n);
}

static bool SmlLoaded()
{
    for (int i = 0; i < g_pluginCount; i++)
        if (strstr(g_plugin[i].file, "soviet_mod_loader")) return true;
    return false;
}

static void WriteSaveManifest(const char* saveDir)
{
    char path[MAX_PATH * 2];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", saveDir, SAVE_MANIFEST_NAME);

    // A fresh file, so a name removed from a config does not linger from an
    // earlier save written over the same slot.
    DeleteFileA(path);
    WritePrivateProfileStringA("tesmioloader", "version", "1", path);
    WritePrivateProfileStringA("tesmioloader", "loader", LOADER_VERSION, path);

    // Which plugins were on. Informative only - the check below works off the
    // content sections, not this one.
    for (int i = 0; i < g_pluginCount; i++)
    {
        char key[64];
        strncpy_s(key, sizeof(key), g_plugin[i].file, _TRUNCATE);
        if (char* dot = strrchr(key, '.')) *dot = 0;
        WritePrivateProfileStringA("plugins", key, "1", path);
    }

    // Mod resources, from whoever publishes the "resources" service this run -
    // the original plugin, Resources Plus or Soviet Mod Loader all do, and the
    // service knows exactly what was registered, which no ini can promise.
    {
        const TsmResourceApi* res =
            (const TsmResourceApi*)api_Consume(TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION);
        if (res)
        {
            int n = 0, w = 0;
            __try
            {
                n = res->count();
                for (int i = 0; i < n; i++)
                {
                    const char* nm = res->name(i);
                    if (nm && *nm) { WritePrivateProfileStringA("resources", nm, "1", path); w++; }
                }
            }
            __except (FaultFilter("save manifest resources", GetExceptionInformation())) {}
            const char* who = ServiceProvider(TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION);
            Logf("save   manifest resources: %d name(s) from the \"%s\" service of %s",
                 w, TSM_SERVICE_RESOURCES, who ? who : "?");
        }
        else if (PluginLoaded("resources") || PluginLoaded("resources_plus"))
        {
            char ini[MAX_PATH];
            ProviderIniPath("plugins\\resources_plus.ini", "plugins\\resources.ini", ini, sizeof(ini));
            char keys[256][128] = {};
            int n = ReadIniSectionKeys(ini, "list", &keys[0][0], 256, sizeof(keys[0]));
            for (int i = 0; i < n; ++i)
                WritePrivateProfileStringA("resources", keys[i], "1", path);
            Logf("save   manifest resources: %d key(s) written from [list] of %s (no service)", n, ini);
        }
        else Logf("save   manifest resources: no provider is on - none written");
    }

    // Mod deposits, by section name - the deposit type number is what the
    // save's maps and mines key on, and the name is its human spelling. Same
    // rule: the "deposits" service first, the config only as a fallback.
    {
        const TsmDepositApi* dep =
            (const TsmDepositApi*)api_Consume(TSM_SERVICE_DEPOSITS, TSM_DEPOSITS_VERSION);
        if (dep)
        {
            int n = 0, w = 0;
            __try
            {
                n = dep->count();
                for (int i = 0; i < n; i++)
                {
                    TsmDeposit d;
                    memset(&d, 0, sizeof(d));
                    if (dep->get(i, &d) && d.name && *d.name)
                    { WritePrivateProfileStringA("deposits", d.name, "1", path); w++; }
                }
            }
            __except (FaultFilter("save manifest deposits", GetExceptionInformation())) {}
            const char* who = ServiceProvider(TSM_SERVICE_DEPOSITS, TSM_DEPOSITS_VERSION);
            Logf("save   manifest deposits: %d name(s) from the \"%s\" service of %s",
                 w, TSM_SERVICE_DEPOSITS, who ? who : "?");
        }
        else if (PluginLoaded("deposits") || PluginLoaded("deposits_plus"))
        {
            char ini[MAX_PATH];
            ProviderIniPath("plugins\\deposits_plus.ini", "plugins\\deposits.ini", ini, sizeof(ini));
            char names[4096];
            DWORD r = GetPrivateProfileSectionNamesA(names, sizeof(names), ini);
            int w = 0;
            if (r && r < sizeof(names) - 2)
                for (char* p = names; *p; p += strlen(p) + 1)
                    if (IniHasKey(ini, p, "token"))
                    { WritePrivateProfileStringA("deposits", p, "1", path); w++; }
            Logf("save   manifest deposits: %d section(s) written from %s (no service)", w, ini);
        }
    }

    // Generated buildings: every stamped folder the game loads with this save.
    {
        char gameDir[MAX_PATH];
        GameDirForSave(saveDir, gameDir, sizeof(gameDir));
        int w = WriteGeneratedBuildings(gameDir, path);
        if (w) Logf("save   manifest buildings: %d generated folder(s)", w);
    }

    Logf("save   manifest written to %s", path);
}

// One check per save per session: the game reads stats.ini when a save is
// browsed as well as when it is loaded, and the warning must not repeat.
#define MAX_MANIFEST_CHECKED 16
static char g_manifestChecked[MAX_MANIFEST_CHECKED][MAX_PATH * 2];
static int  g_manifestCheckedCount;

static bool ManifestChecked(const char* saveDir)
{
    EnterCriticalSection(&g_lock);
    bool seen = false;
    for (int i = 0; i < g_manifestCheckedCount; i++)
        if (_stricmp(g_manifestChecked[i], saveDir) == 0) { seen = true; break; }
    if (!seen && g_manifestCheckedCount < MAX_MANIFEST_CHECKED)
        strncpy_s(g_manifestChecked[g_manifestCheckedCount++],
                  sizeof(g_manifestChecked[0]), saveDir, _TRUNCATE);
    LeaveCriticalSection(&g_lock);
    return seen;
}

static void AppendMissing(char* missing, size_t n, const char* what,
                          const char* name, const char* why)
{
    size_t used = strlen(missing);
    if (used + 96 >= n) return;                  // out of room; the log has all of it
    _snprintf_s(missing + used, n - used, _TRUNCATE, "- %s \"%s\": %s\r\n", what, name, why);
}

// Set by the offline self-test, which has no desktop to put a box on.
static bool g_noMessageBox;

// Returns how many items are missing (0 = the save can load), -1 when this
// save was already checked this session or carries no manifest.
static int CheckSaveManifest(const char* saveDir)
{
    char path[MAX_PATH * 2];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", saveDir, SAVE_MANIFEST_NAME);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return -1;   // pre-manifest save
    if (ManifestChecked(saveDir)) return -1;

    char missing[2048];
    missing[0] = 0;

    // resources: every name must be registered by whoever provides resources
    // now. The live service is the truth; the provider's ini is the fallback
    // for a provider that publishes no service.
    {
        char keys[256][128] = {};
        int n = ReadIniSectionKeys(path, "resources", &keys[0][0], 256, sizeof(keys[0]));

        if (n > 0)
        {
            const TsmResourceApi* res =
                (const TsmResourceApi*)api_Consume(TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION);
            bool on = res != NULL || PluginLoaded("resources") || PluginLoaded("resources_plus") || SmlLoaded();
            const char* who = ServiceProvider(TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION);
            char ini[MAX_PATH];
            ProviderIniPath("plugins\\resources_plus.ini", "plugins\\resources.ini", ini, sizeof(ini));
            char why[256];

            for (int i = 0; i < n; ++i)
            {
                const char* p = keys[i];
                if (!on)
                    AppendMissing(missing, sizeof(missing), "resource", p,
                                  "no resources plugin is on (resources, Resources Plus or Soviet Mod Loader)");
                else if (res)
                {
                    int idx = 0;
                    __try { idx = res->indexOf(p); }
                    __except (FaultFilter("save manifest resources check", GetExceptionInformation())) { idx = 0; }
                    if (idx < 0)
                    {
                        _snprintf_s(why, sizeof(why), _TRUNCATE, "not registered by %s", who ? who : "the resources plugin");
                        AppendMissing(missing, sizeof(missing), "resource", p, why);
                    }
                }
                else if (!IniHasKey(ini, "list", p))
                {
                    _snprintf_s(why, sizeof(why), _TRUNCATE, "no longer in %s", ini);
                    AppendMissing(missing, sizeof(missing), "resource", p, why);
                }
            }
        }
    }

    // deposits: the same, against the "deposits" service.
    {
        char keys[256][128] = {};
        int n = ReadIniSectionKeys(path, "deposits", &keys[0][0], 256, sizeof(keys[0]));

        if (n > 0)
        {
            const TsmDepositApi* dep =
                (const TsmDepositApi*)api_Consume(TSM_SERVICE_DEPOSITS, TSM_DEPOSITS_VERSION);
            bool on = dep != NULL || PluginLoaded("deposits") || PluginLoaded("deposits_plus") || SmlLoaded();
            const char* who = ServiceProvider(TSM_SERVICE_DEPOSITS, TSM_DEPOSITS_VERSION);
            char ini[MAX_PATH];
            ProviderIniPath("plugins\\deposits_plus.ini", "plugins\\deposits.ini", ini, sizeof(ini));
            char why[256];

            // The service's names, read once.
            char known[128][64] = {};
            int nk = 0;
            if (dep)
            {
                __try
                {
                    int c = dep->count();
                    for (int i = 0; i < c && nk < 128; i++)
                    {
                        TsmDeposit d;
                        memset(&d, 0, sizeof(d));
                        if (dep->get(i, &d) && d.name && *d.name)
                            strncpy_s(known[nk++], sizeof(known[0]), d.name, _TRUNCATE);
                    }
                }
                __except (FaultFilter("save manifest deposits check", GetExceptionInformation())) {}
            }

            for (int i = 0; i < n; ++i)
            {
                const char* p = keys[i];
                if (!on)
                    AppendMissing(missing, sizeof(missing), "deposit", p,
                                  "no deposits plugin is on (deposits, Deposits Plus or Soviet Mod Loader)");
                else if (dep)
                {
                    bool have = false;
                    for (int k = 0; k < nk; k++) if (_stricmp(known[k], p) == 0) { have = true; break; }
                    if (!have)
                    {
                        _snprintf_s(why, sizeof(why), _TRUNCATE, "not declared by %s", who ? who : "the deposits plugin");
                        AppendMissing(missing, sizeof(missing), "deposit", p, why);
                    }
                }
                else if (!IniHasKey(ini, p, "token"))
                {
                    _snprintf_s(why, sizeof(why), _TRUNCATE, "no longer in %s", ini);
                    AppendMissing(missing, sizeof(missing), "deposit", p, why);
                }
            }
        }
    }

    // buildings: the generated folder must still exist in one of the out dirs.
    {
        char keys[256][128] = {};
        int n = ReadIniSectionKeys(path, "buildings", &keys[0][0], 256, sizeof(keys[0]));

        if (n > 0)
        {
            char gameDir[MAX_PATH];
            GameDirForSave(saveDir, gameDir, sizeof(gameDir));
            char dirs[MAX_OUT_DIRS][256];
            int nd = CollectOutDirs(dirs);

            for (int i = 0; i < n; ++i)
            {
                const char* p = keys[i];
                bool have = false;
                for (int d = 0; d < nd && !have; d++)
                    if (WorkshopFolderExists(gameDir, dirs[d], p)) have = true;
                if (!have)
                    AppendMissing(missing, sizeof(missing), "building", p,
                                  "no generated folder with this number in workshop_wip");
            }
        }
    }

    if (!missing[0]) return 0;
    int count = 0;
    for (const char* q = missing; (q = strstr(q, "\r\n")) != NULL; q += 2) count++;

    Logf("save   %s expects content that is not enabled:", saveDir);
    for (char* p = missing; *p; )
    {
        char* e = strstr(p, "\r\n");
        if (e) *e = 0;
        Logf("save     %s", p);
        if (!e) break;
        *e = '\r';
        p = e + 2;
    }

    char text[2300];
    _snprintf_s(text, sizeof(text), _TRUNCATE,
        "This save was written with tesmioloader content that is not enabled now:\r\n\r\n"
        "%s\r\nLoading it will most likely crash the game. Enable the missing items "
        "in Republic Mod Manager or the launcher, or load a different save.", missing);
    if (!g_noMessageBox)
        MessageBoxA(NULL, text, "tesmioloader - save needs mods",
                    MB_ICONWARNING | MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
    return count;
}

static void NoteSaveOpen(const char* path, bool writing)
{
    if (!g_saveManifest) return;

    char dir[MAX_PATH * 2];
    if (!SaveDirOf(path, dir, sizeof(dir))) return;

    __try
    {
        if (writing) WriteSaveManifest(dir);
        else         CheckSaveManifest(dir);
    }
    __except (FaultFilter("save manifest", GetExceptionInformation())) {}
}

static void NoteSaveOpenW(const wchar_t* path, bool writing)
{
    if (!g_saveManifest || !path) return;

    // CP_ACP, not UTF-8: every file call the manifest makes goes through the
    // ANSI profile APIs, which decode in the system codepage. UTF-8 bytes in a
    // Cyrillic save name would reach CreateFileA as mojibake and the manifest
    // would be written next to a folder that does not exist.
    char ansi[MAX_PATH * 2];
    if (!WideCharToMultiByte(CP_ACP, 0, path, -1, ansi, sizeof(ansi), NULL, NULL)) return;
    NoteSaveOpen(ansi, writing);
}

// ---------------------------------------------------------------- main menu version line
//
// The line along the bottom of the main menu is one call at the very end of the
// menu builder at rva 0x28AEF0:
//
//   lea  rax,[rip+0x60A26D]              ; L"v%d.%d.%d.%d (64 bit DX11.1 - GPU: %ls)"
//   mov  [rsp+0x48],7 / [rsp+0x40],1 / [rsp+0x38],1 / [rsp+0x30],1
//   mov  [rsp+0x28],rax                  ; the format string argument
//   mov  [rsp+0x20],0xFFAA0000           ; colour
//   call C3D_FONTMANAGER::PrintLeftUnicode
//
// so the four version numbers are immediates on the stack and the GPU name is
// the wide string the C3D_MIDDLEPOINT call above returned. The whole line is
// decided by which string that one `lea` computes.
//
// Which is why this is a **displacement rewrite, not a hook**. Hooking
// PrintLeftUnicode through the import table would be the usual first choice,
// but it is a variadic every label in the game goes through, and a va_list
// cannot be forwarded to a variadic callee - the hook would have to re-format
// every string in the UI through its own CRT to pass anything on. Four bytes of
// operand at a single call site cost nothing at runtime and touch nothing else.
//
// The suffix is appended to the format string rather than printed separately,
// so the game does the drawing and the line stays one string with one layout.
#define P_MENU_VERSION_SITE 0x28B5CC     // v1.1.1.9; was 0x28B55C. LEA RAX,[rip+disp32] - the format argument
#define MENU_LEA_LEN        7

static const BYTE kMenuLeaOrig[3] = { 0x48, 0x8D, 0x05 };   // lea rax,[rip+disp32]

// What the displacement must resolve to. Compared before anything is written:
// on any other build this is a different string and the patch refuses, which
// beats redirecting an argument whose callee expects something else.
static const wchar_t kMenuVersionFmt[] = L"v%d.%d.%d.%d (64 bit DX11.1 - GPU: %ls)";

static int     g_menuPatch = 1;

// No compiled-in default: tesmiolauncher's SaveConfig recomputes this from
// `version` before every Inject and writes it into this same [tesmioloader]
// section, so a value never reaches here except freshly derived. An empty
// read means only "never launched through tesmiolauncher yet" - which
// PatchMenuVersion already treats as "leave the line alone", rather than
// showing a number nobody chose for this run.
static wchar_t g_menuTag[96] = L"";

static void PatchMenuVersion()
{
    if (!g_menuTag[0]) { Logf("menu     tag is empty - version line left alone"); return; }

    BYTE* site = g_exeBase + P_MENU_VERSION_SITE;
    if (memcmp(site, kMenuLeaOrig, sizeof(kMenuLeaOrig)) != 0)
    {
        Logf("menu     FAILED  no LEA RAX at +0x%X - wrong game build, refusing to patch",
             P_MENU_VERSION_SITE);
        return;
    }

    int      disp = *(int*)(site + 3);
    wchar_t* fmt  = (wchar_t*)(site + MENU_LEA_LEN + disp);
    if (!ReadablePtr(fmt, sizeof(kMenuVersionFmt)) ||
        wcscmp(fmt, kMenuVersionFmt) != 0)
    {
        Logf("menu     FAILED  the LEA at +0x%X does not compute the version format string",
             P_MENU_VERSION_SITE);
        return;
    }

    // The replacement has to be reachable by a 32-bit displacement from the call
    // site, which the loader's own image is not guaranteed to be.
    size_t   chars = wcslen(kMenuVersionFmt) + 3 + wcslen(g_menuTag) + 1;
    wchar_t* mine  = (wchar_t*)AllocNear(site, chars * sizeof(wchar_t));
    if (!mine) { Logf("menu     FAILED  no allocation within reach of the call site"); return; }

    _snwprintf_s(mine, chars, _TRUNCATE, L"%s | %s", kMenuVersionFmt, g_menuTag);
    {
        // Data the game only ever reads; it does not need to stay writable, let
        // alone executable, for the rest of the session.
        DWORD mp = 0;
        VirtualProtect(mine, chars * sizeof(wchar_t), PAGE_READONLY, &mp);
    }

    __int64 rel = (BYTE*)mine - (site + MENU_LEA_LEN);
    if (rel != (int)rel) { Logf("menu     FAILED  displacement out of range"); return; }

    DWORD prot = 0;
    if (!VirtualProtect(site, MENU_LEA_LEN, PAGE_EXECUTE_READWRITE, &prot))
    { Logf("menu     FAILED  VirtualProtect %lu", GetLastError()); return; }

    int newDisp = (int)rel;
    memcpy(site + 3, &newDisp, 4);
    VirtualProtect(site, MENU_LEA_LEN, prot, &prot);
    FlushInstructionCache(GetCurrentProcess(), site, MENU_LEA_LEN);

    Logf("menu     version line -> \"%ls\"", mine);
}

static void ReadConfig()
{
    const char* ini = g_iniPath;
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES) return;

    // The loader's own reader throughout (BOM-tolerant, see LoadIniText); the
    // keys and their meaning are exactly what the profile API read before.
    g_traceReads = IniInt(ini, "tesmioloader", "trace_reads",  g_traceReads);
    g_logGame    = IniInt(ini, "tesmioloader", "log_game",     g_logGame);
    g_gameRepeat = IniInt(ini, "tesmioloader", "game_repeat_limit", g_gameRepeat);
    if (g_gameRepeat < 0) g_gameRepeat = 0;
    g_vfsEnabled = IniInt(ini, "tesmioloader", "vfs",          g_vfsEnabled);
    {
        char filter[128];
        if (IniReadValue(ini, "tesmioloader", "trace_filter", filter, sizeof(filter)))
            strncpy_s(g_traceFilter, sizeof(g_traceFilter), filter, _TRUNCATE);
    }

    g_probeMap   = IniInt(ini, "tesmioloader", "probe_map", g_probeMap);
    g_probeTexel = IniInt(ini, "tesmioloader", "probe_texel", g_probeTexel);

    g_saveManifest = IniInt(ini, "tesmioloader", "save_manifest", g_saveManifest);

    g_plugins   = IniInt(ini, "tesmioloader", "plugins", g_plugins);
    g_menuPatch = IniInt(ini, "tesmioloader", "menu_patch", g_menuPatch);
    {
        // Read as bytes and widened here rather than through the W profile API,
        // which would decode the file as ANSI. Keep the tag to ASCII: this is a
        // UTF-8 file and anything past 0x7F would arrive as its raw bytes.
        char tag[96] = {0};
        IniReadValue(ini, "tesmioloader", "menu_tag", tag, sizeof(tag));
        Trim(tag);
        if (tag[0])
            MultiByteToWideChar(CP_UTF8, 0, tag, -1, g_menuTag,
                                sizeof(g_menuTag) / sizeof(g_menuTag[0]));
    }
}

static HANDLE OpenLog(const wchar_t* name)
{
    wchar_t p[MAX_PATH];
    _snwprintf_s(p, MAX_PATH, _TRUNCATE, L"%s\\%s", g_baseDirW, name);
    return CreateFileW(p, GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

// The previous run's log survives one more start as logs\tesmioloader.previous.log.
// It has been the file that was needed and gone more than once - a crash that
// only shows on the second look, a comparison between two starts.
static void RotateLog()
{
    wchar_t cur[MAX_PATH], dir[MAX_PATH], prev[MAX_PATH];
    _snwprintf_s(cur,  MAX_PATH, _TRUNCATE, L"%s\\tesmioloader.log", g_baseDirW);
    _snwprintf_s(dir,  MAX_PATH, _TRUNCATE, L"%s\\logs", g_baseDirW);
    _snwprintf_s(prev, MAX_PATH, _TRUNCATE, L"%s\\logs\\tesmioloader.previous.log", g_baseDirW);
    if (GetFileAttributesW(cur) == INVALID_FILE_ATTRIBUTES) return;
    CreateDirectoryW(dir, NULL);
    MoveFileExW(cur, prev, MOVEFILE_REPLACE_EXISTING);
}

// g_baseDir is "<...>\tesmioloader\build" (the folder tesmioloader.dll lives
// in) when run from a source checkout, or wherever a distributed copy was
// unpacked to otherwise. The checked-in assets live in "<...>\tesmioloader\vfs",
// one level up from build\ - so a vfs folder is looked for there first, and
// only a plain "vfs" folder beside the dll itself (baseDir\vfs) is used as the
// fallback, for a distribution that ships build\ on its own with vfs inside
// it. No link of any kind is required between the two; whichever one exists
// is read directly, so there is nothing to fall out of sync.
static void ResolveVfsRoot()
{
    wchar_t parent[MAX_PATH];
    _snwprintf_s(parent, MAX_PATH, _TRUNCATE, L"%s", g_baseDirW);
    wchar_t* slash = wcsrchr(parent, L'\\');
    bool found = false;
    if (slash)
    {
        *slash = 0;
        wchar_t sibling[MAX_PATH];
        _snwprintf_s(sibling, MAX_PATH, _TRUNCATE, L"%s\\vfs", parent);
        DWORD attr = GetFileAttributesW(sibling);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            _snwprintf_s(g_vfsRootW, MAX_PATH, _TRUNCATE, L"%s", sibling);
            found = true;
        }
    }
    if (!found) _snwprintf_s(g_vfsRootW, MAX_PATH, _TRUNCATE, L"%s\\vfs", g_baseDirW);

    // The ANSI copy in the system codepage: it is what the A-flavoured file
    // calls and the plugin table (host->vfsRoot) expect.
    WideCharToMultiByte(CP_ACP, 0, g_vfsRootW, -1, g_vfsRoot, MAX_PATH, NULL, NULL);
}

static void Init()
{
    InitializeCriticalSection(&g_lock);

    GetModuleFileNameW(g_self, g_baseDirW, MAX_PATH);
    if (wchar_t* s = wcsrchr(g_baseDirW, L'\\')) *s = 0;
    WideCharToMultiByte(CP_ACP, 0, g_baseDirW, -1, g_baseDir, MAX_PATH, NULL, NULL);
    ResolveVfsRoot();
    _snprintf_s(g_iniPath, sizeof(g_iniPath), _TRUNCATE, "%s\\tesmioloader.ini", g_baseDir);

    ReadConfig();

    RotateLog();
    g_hLog = OpenLog(L"tesmioloader.log");
    if (g_traceReads) g_hReads = OpenLog(L"tesmioloader.reads.log");

    g_exe     = GetModuleHandleW(NULL);
    g_exeBase = (BYTE*)g_exe;
    DWORD exeStamp = 0;
    {
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)g_exeBase;
        IMAGE_NT_HEADERS* nt  = (IMAGE_NT_HEADERS*)(g_exeBase + dos->e_lfanew);
        g_exeSize = nt->OptionalHeader.SizeOfImage;
        exeStamp  = nt->FileHeader.TimeDateStamp;
    }
    AddVectoredExceptionHandler(1, CrashHandler);
    HMODULE engine = GetModuleHandleA(DLL_ENGINE);

    // The header: which loader, when it was built, which game it found. The PE
    // stamp is what tells a game update apart from anything else - every RVA
    // below and in every plugin was taken from one particular build.
    {
        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t exePath[MAX_PATH] = L"?";
        GetModuleFileNameW(g_exe, exePath, MAX_PATH);
        Logf("tesmioloader %s  built %s %s  API %u (accepts %u..%u)  started %04d-%02d-%02d",
             LOADER_VERSION, __DATE__, __TIME__, TSM_API_VERSION, TSM_API_VERSION_MIN, TSM_API_VERSION,
             t.wYear, t.wMonth, t.wDay);
        Logf("game      %ls", exePath);
        Logf("game      image size 0x%zX  pe-stamp 0x%08lX%s", g_exeSize, (unsigned long)exeStamp,
             exeStamp == 0x6A3EB6AD ? "  (v1.1.1.9, the build the addresses were taken from)" : "  (NOT the build the addresses were taken from - patches that check their bytes will refuse)");
        Logf("base dir  %ls", g_baseDirW);
    }
    Logf("tesmioloader phase B");
    Logf("exe base  %p    engine base %p", (void*)g_exe, (void*)engine);
    Logf("vfs root  %s (%s)", g_vfsRoot, g_vfsEnabled ? "on" : "off");
    {
        // What this log will and will not carry, so nobody hunts for a line that
        // was never meant to be here.
        const char* what = g_logGame <= 0 ? "nothing" :
                           g_logGame == 1 ? "errors" :
                           g_logGame == 2 ? "errors and warnings" : "everything";
        char repeat[96];
        if (g_gameRepeat > 0)
            _snprintf_s(repeat, sizeof(repeat), _TRUNCATE,
                        ", a repeating one %d time(s) and then counted", g_gameRepeat);
        else
            strncpy_s(repeat, sizeof(repeat), ", every repeat written out", _TRUNCATE);
        Logf("game log  %s (log_game = %d)%s", what, g_logGame, g_logGame > 0 ? repeat : "");
    }
    Logf("---");

    PatchIat(g_exe, DLL_ENGINE, SYM_READ_FILE,   (void*)h_ReadFileIntoBuffer, (void**)&o_ReadFile,   "ReadFileIntoBuffer");
    PatchIat(g_exe, DLL_ENGINE, SYM_FILE_EXISTS, (void*)h_CheckIfFileExist,   (void**)&o_FileExists, "CheckIfFileExist");
    PatchIat(g_exe, DLL_ENGINE, SYM_LOG_INFO,    (void*)h_LogInfo,            (void**)&o_LogInfo,    "C3DLog_PrintInfo");
    PatchIat(g_exe, DLL_ENGINE, SYM_LOG_WARN,    (void*)h_LogWarn,            (void**)&o_LogWarn,    "C3DLog_PrintWarning");
    PatchIat(g_exe, DLL_ENGINE, SYM_LOG_ERROR,   (void*)h_LogError,           (void**)&o_LogError,   "C3DLog_PrintError");

    PatchIat(g_exe, DLL_STDIO,  "fopen",         (void*)h_fopen,              (void**)&o_fopen,      "fopen");
    PatchIat(g_exe, DLL_STDIO,  "fopen_s",       (void*)h_fopen_s,            (void**)&o_fopen_s,    "fopen_s");
    PatchIat(g_exe, DLL_STDIO,  "_wfopen",       (void*)h_wfopen,             (void**)&o_wfopen,     "_wfopen");
    PatchIat(g_exe, DLL_STDIO,  "_wfopen_s",     (void*)h_wfopen_s,           (void**)&o_wfopen_s,   "_wfopen_s");
    PatchIat(g_exe, DLL_STDIO,  "fread",         (void*)h_fread,              (void**)&o_fread,      "fread");

    // C3DDLL64.dll carries its own import table, so anything the engine opens
    // for itself - textures above all - never touches the executable's copy of
    // fopen. Patching only the exe left the whole engine side invisible.
    if (engine)
    {
        void* discard = NULL;
        PatchIat(engine, DLL_STDIO,   "fopen",       (void*)h_fopen,      &discard, "engine fopen");
        PatchIat(engine, DLL_STDIO,   "fopen_s",     (void*)h_fopen_s,    &discard, "engine fopen_s");
        PatchIat(engine, DLL_STDIO,   "_wfopen",     (void*)h_wfopen,     &discard, "engine _wfopen");
        PatchIat(engine, DLL_STDIO,   "fread",       (void*)h_fread,      (void**)&o_fread, "engine fread");
        PatchIat(engine, "KERNEL32.dll", "CreateFileA", (void*)h_CreateFileA,
                 (void**)&o_CreateFileA, "engine CreateFileA");
        PatchIat(engine, "KERNEL32.dll", "CreateFile2", (void*)h_CreateFile2,
                 (void**)&o_CreateFile2, "engine CreateFile2");
    }

    // The executable opens files through the wide API as well.
    PatchIat(g_exe, "KERNEL32.dll", "CreateFileW", (void*)h_CreateFileW,
             (void**)&o_CreateFileW, "CreateFileW");

    HookTextureVtable(engine);

    if (g_menuPatch) PatchMenuVersion();

    // Everything that is a feature rather than infrastructure lives out here
    // now - resources, deposits, depletion. They load last so each sees a fully
    // built loader: every import already swapped, the VFS live, the crash
    // handler armed.
    if (g_plugins) LoadPlugins();

    Logf("summary  %ld hook(s) ok, %ld could not be placed, %d plugin(s) loaded, %d service(s) published",
         g_hooksOk, g_hooksFailed, g_pluginCount, g_serviceCount);
    Logf("--- hooks installed ---");
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = mod;
        DisableThreadLibraryCalls(mod);
        Init();
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        ReportRepeats();
        Logf("--- shutdown: %ld VFS redirects, %d plugin(s) ---",
             g_nRedirects, g_pluginCount);
        if (g_hLog   != INVALID_HANDLE_VALUE) CloseHandle(g_hLog);
        if (g_hReads != INVALID_HANDLE_VALUE) CloseHandle(g_hReads);
    }
    return TRUE;
}

// ---------------------------------------------------------------- offline self-test
//
// Built as an EXE, never as the DLL:
//   cl /nologo /O2 /MT /W3 /EHsc /DTESMIO_SELFTEST /Fe"tesmioloader_test.exe" src\tesmioloader.cpp /link kernel32.lib
//   tesmioloader_test.exe <fresh scratch folder>
// Exercises the parts that do not need a running game: the ini reader, the VFS
// path rule, the plugin name order, the service noticeboard and the whole save
// manifest (write and check) against fake resources/deposits services.
#ifdef TESMIO_SELFTEST

static int t_failed = 0;
static void Check(bool ok, const char* what)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) t_failed++;
}

static bool t_hideCable = false;
static int         t_resCount(void) { return 3; }
static const char* t_resName(int i) { static const char* n[3] = { "cable", "rocksalt", "table_salt" }; return (i >= 0 && i < 3) ? n[i] : NULL; }
static int         t_resIndex(int i) { return 60 + i; }
static int         t_resIndexOf(const char* nm)
{
    for (int i = 0; i < 3; i++)
        if (_stricmp(t_resName(i), nm) == 0) return (t_hideCable && i == 0) ? -1 : 60 + i;
    return -1;
}
static TsmResourceApi t_res = { t_resCount, t_resName, t_resIndex, t_resIndexOf };

static int         t_depCount(void) { return 1; }
static int         t_depGet(int i, TsmDeposit* d) { if (i != 0) return 0; d->name = "rocksalt"; d->token = "$TYPE_MINE_ROCKSALT"; return 1; }
static const char* t_depSetting(int, const char*) { return NULL; }
static void*       t_depTexture(int) { return NULL; }
static TsmDepositApi t_dep = { t_depCount, t_depGet, t_depSetting, t_depTexture };

static void t_write(const char* path, const char* text)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("cannot write %s\n", path); t_failed++; return; }
    DWORD w = 0;
    WriteFile(h, text, (DWORD)strlen(text), &w, NULL);
    CloseHandle(h);
}

static void t_copyManifest(const char* fromDir, const char* toDir)
{
    char a[MAX_PATH * 2], b[MAX_PATH * 2];
    _snprintf_s(a, sizeof(a), _TRUNCATE, "%s\\%s", fromDir, SAVE_MANIFEST_NAME);
    _snprintf_s(b, sizeof(b), _TRUNCATE, "%s\\%s", toDir, SAVE_MANIFEST_NAME);
    CreateDirectoryA(toDir, NULL);
    CopyFileA(a, b, FALSE);
}

int main(int argc, char** argv)
{
    if (argc < 2) { printf("usage: tesmioloader_test <fresh scratch folder>\n"); return 2; }
    const char* root = argv[1];
    InitializeCriticalSection(&g_lock);
    g_noMessageBox = true;

    char build[MAX_PATH], plugins[MAX_PATH], game[MAX_PATH], saves[MAX_PATH], wip[MAX_PATH], p[MAX_PATH * 2];
    _snprintf_s(build,   sizeof(build),   _TRUNCATE, "%s\\build", root);
    _snprintf_s(plugins, sizeof(plugins), _TRUNCATE, "%s\\build\\plugins", root);
    _snprintf_s(game,    sizeof(game),    _TRUNCATE, "%s\\game", root);
    _snprintf_s(saves,   sizeof(saves),   _TRUNCATE, "%s\\game\\media_soviet\\save", root);
    _snprintf_s(wip,     sizeof(wip),     _TRUNCATE, "%s\\game\\media_soviet\\workshop_wip", root);
    CreateDirectoryA(root, NULL); CreateDirectoryA(build, NULL); CreateDirectoryA(plugins, NULL);
    CreateDirectoryA(game, NULL);
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\game\\media_soviet", root); CreateDirectoryA(p, NULL);
    CreateDirectoryA(saves, NULL); CreateDirectoryA(wip, NULL);
    strncpy_s(g_baseDir, sizeof(g_baseDir), build, _TRUNCATE);
    MultiByteToWideChar(CP_ACP, 0, build, -1, g_baseDirW, MAX_PATH);
    _snprintf_s(g_iniPath, sizeof(g_iniPath), _TRUNCATE, "%s\\tesmioloader.ini", g_baseDir);
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\test.log", root);
    g_hLog = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // 1. the ini reader, with a BOM in front of the first section
    t_write(g_iniPath, "\xEF\xBB\xBF[tesmioloader]\r\nvfs = 1\r\nmenu_tag = \"hello world\"\r\n; comment\r\nsave_manifest=0x10\r\n[plugins]\r\nresources = 0\r\nResources_Plus=1\r\n");
    Check(IniInt(g_iniPath, "tesmioloader", "vfs", 0) == 1, "IniInt reads a value behind a BOM");
    Check(IniInt(g_iniPath, "tesmioloader", "save_manifest", 0) == 16, "IniInt takes hex");
    Check(IniInt(g_iniPath, "tesmioloader", "absent", 7) == 7, "IniInt falls back for a missing key");
    { char v[64]; Check(IniReadValue(g_iniPath, "tesmioloader", "menu_tag", v, sizeof(v)) && strcmp(v, "hello world") == 0, "IniReadValue strips one pair of quotes"); }
    Check(!PluginEnabled("resources.dll"), "[plugins] resources = 0 switches the plugin off");
    Check(PluginEnabled("resources_plus.dll"), "[plugins] key compares without case");
    Check(PluginEnabled("unlisted.dll"), "an unlisted plugin is on");
    printf("info the Win32 profile API reads resources=%d through the BOM (1 = fooled, the reason the loader has its own reader)\n",
           GetPrivateProfileIntA("plugins", "resources", 1, g_iniPath));

    // 2. the VFS path rule
    Check(HasParentStep("..\\x.ini") && HasParentStep("a/../b") && HasParentStep("a\\b\\.."), "a parent step is recognised");
    Check(!HasParentStep("a..b\\c") && !HasParentStep("media_soviet\\x.ini") && !HasParentStep("..x\\y"), "dots inside a name are not a parent step");
    Check(HasParentStepW(L"..\\x") && !HasParentStepW(L"vehicles\\a.nmf"), "the wide rule matches the narrow one");

    // 3. plugin name order
    {
        char names[3][64] = { "soviet_mod_loader.dll", "Resources_plus.dll", "deposits_plus.dll" };
        qsort(names, 3, sizeof(names[0]), CompareNames);
        Check(_stricmp(names[0], "deposits_plus.dll") == 0 && _stricmp(names[1], "Resources_plus.dll") == 0 && _stricmp(names[2], "soviet_mod_loader.dll") == 0,
              "plugins load in case-insensitive name order regardless of the file system");
    }

    // 4. the service noticeboard
    strncpy_s(g_plugin[0].file, sizeof(g_plugin[0].file), "resources_plus.dll", _TRUNCATE);
    strncpy_s(g_plugin[1].file, sizeof(g_plugin[1].file), "deposits_plus.dll", _TRUNCATE);
    strncpy_s(g_plugin[2].file, sizeof(g_plugin[2].file), "buildings_plus.dll", _TRUNCATE);
    g_pluginCount = 3;
    g_currentPlugin = g_plugin[0].file; api_Provide(TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION, &t_res);
    g_currentPlugin = g_plugin[1].file; api_Provide(TSM_SERVICE_DEPOSITS, TSM_DEPOSITS_VERSION, &t_dep);
    g_currentPlugin = "?";
    Check(strcmp(ServiceProvider(TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION), "resources_plus.dll") == 0, "a service remembers its provider");
    DropServices("deposits_plus.dll");
    Check(api_Consume(TSM_SERVICE_DEPOSITS, TSM_DEPOSITS_VERSION) == NULL && api_Consume(TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION) == &t_res,
          "the services of a declined plugin are withdrawn, the others stay");
    g_currentPlugin = g_plugin[1].file; api_Provide(TSM_SERVICE_DEPOSITS, TSM_DEPOSITS_VERSION, &t_dep); g_currentPlugin = "?";

    // 5. the manifest, written from the services and the generated folders
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\9300000001", wip); CreateDirectoryA(p, NULL);
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\9300000001\\tesmioloader.stamp", wip); t_write(p, "buildings_plus generated this folder\n");
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\9300000002", wip); CreateDirectoryA(p, NULL);        // no stamp: a folder the editor made
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\notanumber", wip); CreateDirectoryA(p, NULL);
    char slot1[MAX_PATH]; _snprintf_s(slot1, sizeof(slot1), _TRUNCATE, "%s\\slot1", saves); CreateDirectoryA(slot1, NULL);
    WriteSaveManifest(slot1);
    char manifest[MAX_PATH * 2]; _snprintf_s(manifest, sizeof(manifest), _TRUNCATE, "%s\\%s", slot1, SAVE_MANIFEST_NAME);
    {
        char keys[256][128] = {};
        int n = ReadIniSectionKeys(manifest, "resources", &keys[0][0], 256, sizeof(keys[0]));
        Check(n == 3 && _stricmp(keys[0], "cable") == 0 && _stricmp(keys[2], "table_salt") == 0, "resources come from the service, in its order");
        n = ReadIniSectionKeys(manifest, "deposits", &keys[0][0], 256, sizeof(keys[0]));
        Check(n == 1 && _stricmp(keys[0], "rocksalt") == 0, "deposits come from the service");
        n = ReadIniSectionKeys(manifest, "buildings", &keys[0][0], 256, sizeof(keys[0]));
        Check(n == 1 && strcmp(keys[0], "9300000001") == 0, "only a stamped, numbered folder counts as a generated building");
        n = ReadIniSectionKeys(manifest, "plugins", &keys[0][0], 256, sizeof(keys[0]));
        Check(n == 3 && _stricmp(keys[0], "resources_plus") == 0, "the plugin list names the files without extension");
        char v[64]; Check(IniReadValue(manifest, "tesmioloader", "loader", v, sizeof(v)) && strcmp(v, LOADER_VERSION) == 0, "the manifest names the loader build");
    }
    Check(CheckSaveManifest(slot1) == 0, "the save loads while everything is still provided");
    Check(CheckSaveManifest(slot1) == -1, "a save is checked once per session");

    // 6. one resource gone from the provider
    char slot2[MAX_PATH]; _snprintf_s(slot2, sizeof(slot2), _TRUNCATE, "%s\\slot2", saves); t_copyManifest(slot1, slot2);
    t_hideCable = true;
    Check(CheckSaveManifest(slot2) == 1, "a resource the provider no longer registers is reported");
    t_hideCable = false;

    // 7. no services at all, providers loaded: the ini fallback
    DropServices("resources_plus.dll"); DropServices("deposits_plus.dll");
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\resources_plus.ini", plugins);
    t_write(p, "[list]\r\ncable = steel, Cable\r\nrocksalt = rawgravel, Rock Salt\r\ntable_salt = food, Table Salt\r\n");
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\deposits_plus.ini", plugins);
    t_write(p, "[deposits_plus]\r\nenabled = 1\r\n[rocksalt]\r\ntoken = $TYPE_MINE_ROCKSALT\r\n[sand_tile:meadow]\r\nbase = grass.dds\r\n");
    char slot3[MAX_PATH]; _snprintf_s(slot3, sizeof(slot3), _TRUNCATE, "%s\\slot3", saves); t_copyManifest(slot1, slot3);
    Check(CheckSaveManifest(slot3) == 0, "without a service the ini of the provider answers");
    char slot3b[MAX_PATH]; _snprintf_s(slot3b, sizeof(slot3b), _TRUNCATE, "%s\\slot3b", saves); CreateDirectoryA(slot3b, NULL);
    WriteSaveManifest(slot3b);
    {
        char keys[256][128] = {};
        char m[MAX_PATH * 2]; _snprintf_s(m, sizeof(m), _TRUNCATE, "%s\\%s", slot3b, SAVE_MANIFEST_NAME);
        int n = ReadIniSectionKeys(m, "deposits", &keys[0][0], 256, sizeof(keys[0]));
        Check(n == 1 && _stricmp(keys[0], "rocksalt") == 0, "the ini fallback writes only sections with a token (no [sand_tile:], no settings)");
    }

    // 8. no provider loaded at all
    g_pluginCount = 1;
    strncpy_s(g_plugin[0].file, sizeof(g_plugin[0].file), "buildings_plus.dll", _TRUNCATE);
    char slot4[MAX_PATH]; _snprintf_s(slot4, sizeof(slot4), _TRUNCATE, "%s\\slot4", saves); t_copyManifest(slot1, slot4);
    Check(CheckSaveManifest(slot4) == 4, "with no resources and no deposits provider every name is reported (3 + 1)");

    // 9. the generated folder is gone
    g_pluginCount = 3;
    strncpy_s(g_plugin[0].file, sizeof(g_plugin[0].file), "resources_plus.dll", _TRUNCATE);
    g_currentPlugin = g_plugin[0].file; api_Provide(TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION, &t_res);
    g_currentPlugin = g_plugin[1].file; api_Provide(TSM_SERVICE_DEPOSITS, TSM_DEPOSITS_VERSION, &t_dep); g_currentPlugin = "?";
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\9300000001\\tesmioloader.stamp", wip); DeleteFileA(p);
    _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\9300000001", wip); RemoveDirectoryA(p);
    char slot5[MAX_PATH]; _snprintf_s(slot5, sizeof(slot5), _TRUNCATE, "%s\\slot5", saves); t_copyManifest(slot1, slot5);
    Check(CheckSaveManifest(slot5) == 1, "a generated building whose folder is gone is reported");

    // 10. a relative save path resolves the game folder to the working directory
    {
        char out[MAX_PATH];
        GameDirForSave("media_soviet\\save\\slot1", out, sizeof(out));
        Check(strcmp(out, ".") == 0, "a relative save path means the game folder is the working directory");
        GameDirForSave(slot1, out, sizeof(out));
        Check(_stricmp(out, game) == 0, "an absolute save path climbs to the game folder");
    }

    // 11. the shape of a repeating message
    {
        char s[256], s2[256];
        Shape("Failed to open C:\\Steam\\workshop\\content\\784150\\3776534001\\Walls2\\building.ini", s, sizeof(s));
        Shape("Failed to open media_soviet/cwc/vehicles/ty45/script.ini", s2, sizeof(s2));
        Check(strcmp(s, "Failed to open <path>") == 0 && strcmp(s, s2) == 0,
              "two complaints about different folders share one shape");
        Shape("Workshop subscribed items num 684", s, sizeof(s));
        Check(strcmp(s, "Workshop subscribed items num #") == 0, "a number is what varies, the wording is what counts");
        Shape("ResourceGet - not found waste", s, sizeof(s));
        Shape("ResourceGet - not found cargovehicles", s2, sizeof(s2));
        Check(strcmp(s, s2) != 0, "two different names stay two different messages");
        Check(Shape("Monitor 0", s, sizeof(s)) && strcmp(s, "Monitor #") == 0,
              "one word of wording is enough to group a message");
        Check(Shape("Size 3840 2160", s, sizeof(s)) && strcmp(s, "Size # #") == 0,
              "numbers around a word still leave the word to group by");
        Check(!Shape("1", s, sizeof(s)) && !Shape("12", s2, sizeof(s2)),
              "the game's bare numbers have no wording, so they are never counted together");
    }

    // 12. saying a repeat a few times and counting the rest
    {
        long shown = 0;
        for (int i = 0; i < 47; i++)
        {
            char msg[128], shape[256];
            _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Failed to open media_soviet/cwc/vehicles/t%d/script.ini", i);
            Shape(msg, shape, sizeof(shape));
            if (NoteRepeat(0, shape, shape) <= 3) { MarkShown(0, shape); shown++; }
        }
        Check(shown == 3, "a message repeated 47 times is written 3 times");
        char path[] = "media_soviet/resources/rocksalt1.nmf";
        long wrote = 0;
        for (int i = 0; i < 8; i++) if (NoteRepeat(1, path, path) == 1) wrote++;
        Check(wrote == 1, "a file the engine opens eight times is named once");
    }

    CloseHandle(g_hLog);
    printf("RESULT %s (%d failed)\n", t_failed ? "FAILED" : "ALL PASSED", t_failed);
    return t_failed ? 1 : 0;
}
#endif
