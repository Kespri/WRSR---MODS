// localization.cpp - virtual, composable language-file extensions.
//
// Content packs live below plugins\localization\<pack>\.  No shipped .btf is
// ever edited.  This plugin writes extended .btf files into the TesmioLoader
// VFS folder, so the game loads them instead of the originals. Every plugin
// initialization removes its previous generated overlays before new content is
// parsed. Set enabled=0 once before disabling the DLL so that cleanup can run.

#include "../../src/tesmio_plugin.h"

#ifndef TSM_SERVICE_LOCALIZATION

#define TSM_SERVICE_LOCALIZATION    "localization"
#define TSM_LOCALIZATION_VERSION    1u
#define TSM_LOCALIZATION_ID_BASE    2000000u
#define TSM_LOCALIZATION_ID_LIMIT   2999999u

typedef struct TsmLocalizationApi
{
    int (*resolve)(const char* nameSpace, const char* key);
    int (*resolveFull)(const char* fullyQualifiedKey);
} TsmLocalizationApi;

#endif

#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

#define PLUGIN_VERSION "0.4.0"
static const char* const PLUGIN_INI = "plugins\\localization.ini";
static const char* const PLUGIN_LOG_NAME = "tesmioloader.localization.log";
static const char* const VFS_MEDIA_REL = "media_soviet";

static const size_t MAX_INI_BYTES = 16u * 1024u * 1024u;
static const size_t MAX_BTF_BYTES = 256u * 1024u * 1024u;
static const size_t MAX_PACKS = 256;
static const size_t MAX_KEYS = 100000;
static const size_t MAX_TEXT_UNITS = 0xFFFF;

typedef std::vector<unsigned char> Bytes;

struct Pack
{
    std::string folder;
    std::string dir;
    std::string nameSpace;
    std::string fallback;
    std::wstring missingText;
    std::map<std::string, std::map<std::string, std::wstring> > languages;
    bool valid;
};

struct KeyEntry
{
    std::string full;
    std::string local;
    size_t      pack;
    unsigned    id;
};

struct BtfEntry
{
    unsigned     id;
    std::wstring text;
};

static std::vector<Pack>                         g_packs;
static std::vector<KeyEntry>                     g_keys;
static std::map<std::string, size_t>              g_keyIndex;
static std::set<unsigned>                         g_usedIds;
static unsigned                                   g_nextId = TSM_LOCALIZATION_ID_BASE;
static std::string                                g_gameDir;
static std::string                                g_mediaDir;
static std::string                                g_vfsDir;
static std::string                                g_packRoot;
static std::string                                g_packRootBeside;   // 1.2: <DLL folder>\\localization, empty when unused
static HANDLE                                     g_detail = INVALID_HANDLE_VALUE;
static volatile LONG                              g_operational;
static bool                                       g_bound;
static int                                        g_verbose;
static volatile LONG                              g_logWarnings;
static volatile LONG                              g_logErrors;
static volatile LONG                              g_logFatals;
static ULONGLONG                                  g_logStarted;
static ULONGLONG                                  g_phaseStarted;
static LONG                                       g_phaseWarnings;
static LONG                                       g_phaseErrors;
static LONG                                       g_phaseFatals;

// ---------------------------------------------------------------- diagnostics

static void DetailWriteLine(const char* text)
{
    if (g_detail == INVALID_HANDLE_VALUE || !text) return;

    if (g_bound) EnterCriticalSection(&g_lock);

    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[64];
    _snprintf_s(stamp, sizeof(stamp), _TRUNCATE,
        "[%04u-%02u-%02u %02u:%02u:%02u.%03u] ",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
        t.wMilliseconds);

    DWORD put = 0;
    WriteFile(g_detail, stamp, (DWORD)strlen(stamp), &put, NULL);
    WriteFile(g_detail, text, (DWORD)strlen(text), &put, NULL);
    WriteFile(g_detail, "\r\n", 2, &put, NULL);

    if (g_bound) LeaveCriticalSection(&g_lock);
}

static void CountLevel(const char* level)
{
    if (!level) return;
    if (_stricmp(level, "WARN") == 0) InterlockedIncrement(&g_logWarnings);
    else if (_stricmp(level, "ERROR") == 0) InterlockedIncrement(&g_logErrors);
    else if (_stricmp(level, "FATAL") == 0) InterlockedIncrement(&g_logFatals);
}

static void ReportV(const char* level, const char* file, unsigned line,
    const char* rule, const char* fmt, va_list ap)
{
    char body[3072];
    _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, ap);

    char full[4096];
    if (file && file[0])
    {
        if (line)
            _snprintf_s(full, sizeof(full), _TRUNCATE,
                "%s: %s:%u [%s] %s", level, file, line,
                rule ? rule : "general", body);
        else
            _snprintf_s(full, sizeof(full), _TRUNCATE,
                "%s: %s [%s] %s", level, file,
                rule ? rule : "general", body);
    }
    else
        _snprintf_s(full, sizeof(full), _TRUNCATE,
            "%s: [%s] %s", level, rule ? rule : "general", body);

    CountLevel(level);
    if (H) Logf("localization  %s", full);
    DetailWriteLine(full);
}

static void Report(const char* level, const char* file, unsigned line,
    const char* rule, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ReportV(level, file, line, rule, fmt, ap);
    va_end(ap);
}

static void Info(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ReportV("INFO", NULL, 0, "status", fmt, ap);
    va_end(ap);
}

static void ReportWindows(const char* level, const char* file, unsigned line,
    const char* rule, const char* action, DWORD error, const char* remedy)
{
    char systemText[512] = {};
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0,
        systemText, (DWORD)sizeof(systemText), NULL);
    while (n && (systemText[n - 1] == '\r' || systemText[n - 1] == '\n' ||
                 systemText[n - 1] == ' ' || systemText[n - 1] == '.'))
        systemText[--n] = 0;
    if (!n) strcpy_s(systemText, sizeof(systemText), "Unknown Windows error");

    Report(level, file, line, rule,
        "%s (Windows error %lu: %s). Action: %s",
        action, error, systemText, remedy ? remedy : "Review the preceding context and retry");
}

static LONG AtomicRead(volatile LONG* value)
{
    return InterlockedCompareExchange(value, 0, 0);
}

static void BeginLogPhase()
{
    g_phaseStarted = GetTickCount64();
    g_phaseWarnings = AtomicRead(&g_logWarnings);
    g_phaseErrors = AtomicRead(&g_logErrors);
    g_phaseFatals = AtomicRead(&g_logFatals);
}

static void LogSummaryStatus(const char* phase, const char* status)
{
    ULONGLONG elapsed =
        g_phaseStarted ? GetTickCount64() - g_phaseStarted : 0;
    Info("%s %s after %llu ms; %ld warning(s), %ld error(s), %ld fatal error(s)",
        phase, status,
        (unsigned long long)elapsed,
        AtomicRead(&g_logWarnings) - g_phaseWarnings,
        AtomicRead(&g_logErrors) - g_phaseErrors,
        AtomicRead(&g_logFatals) - g_phaseFatals);
}

static void LogSummary(const char* phase, bool success)
{
    LogSummaryStatus(phase, success ? "completed successfully" : "failed");
}

// ---------------------------------------------------------------- small helpers

static bool FileExists(const std::string& path)
{
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirExists(const std::string& path)
{
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool IsReparsePoint(const std::string& path)
{
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES &&
        (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

static std::string Join(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    char c = a[a.size() - 1];
    return (c == '\\' || c == '/') ? a + b : a + "\\" + b;
}

// Folder of this DLL with a trailing backslash. A Workshop package
// loaded by Soviet Mod Loader or the Workshop Bridge carries its INI and its
// text packs beside the DLL instead of below plugins\.
static std::string OwnDirectory(void)
{
    HMODULE self = NULL;
    char buffer[MAX_PATH];
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)&OwnDirectory, &self) ||
        !GetModuleFileNameA(self, buffer, (DWORD)sizeof(buffer)))
        return std::string();
    std::string result(buffer);
    size_t slash = result.find_last_of('\\');
    return slash == std::string::npos ? std::string() : result.substr(0, slash + 1);
}

static bool IsAbsoluteWindowsPath(const std::string& path)
{
    if (path.size() >= 3 &&
        ((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
    {
        return true;
    }
    return path.size() >= 3 &&
        (path[0] == '\\' || path[0] == '/') &&
        (path[1] == '\\' || path[1] == '/');
}

static bool EnsureDir(const std::string& input, DWORD* error)
{
    if (error) *error = ERROR_SUCCESS;
    if (input.empty())
    {
        if (error) *error = ERROR_INVALID_NAME;
        return false;
    }

    std::string path = input;
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] == '/') path[i] = '\\';
    while (path.size() > 3 && path[path.size() - 1] == '\\')
        path.resize(path.size() - 1);

    size_t start = 0;
    if (path.size() >= 3 && path[1] == ':' && path[2] == '\\')
    {
        start = 3;
    }
    else if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\')
    {
        size_t serverEnd = path.find('\\', 2);
        size_t shareEnd = serverEnd == std::string::npos
            ? std::string::npos : path.find('\\', serverEnd + 1);
        if (serverEnd == std::string::npos)
        {
            if (error) *error = ERROR_INVALID_NAME;
            return false;
        }
        start = shareEnd == std::string::npos ? path.size() : shareEnd + 1;
    }

    size_t pos = start;
    while (pos <= path.size())
    {
        size_t separator = path.find('\\', pos);
        if (separator == std::string::npos) separator = path.size();
        std::string current = path.substr(0, separator);
        if (!current.empty() && !DirExists(current))
        {
            if (!CreateDirectoryA(current.c_str(), NULL))
            {
                DWORD currentError = GetLastError();
                if (currentError != ERROR_ALREADY_EXISTS)
                {
                    if (error) *error = currentError;
                    return false;
                }
            }
            if (!DirExists(current))
            {
                if (error) *error = ERROR_DIRECTORY;
                return false;
            }
        }
        if (separator == path.size()) break;
        pos = separator + 1;
    }
    return true;
}

static const char* BaseName(const char* path)
{
    const char* b = path ? path : "";
    if (!path) return b;
    for (const char* p = path; *p; ++p)
        if (*p == '\\' || *p == '/') b = p + 1;
    return b;
}

static std::string LowerAscii(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
    return s;
}

static std::wstring TrimWide(const std::wstring& in)
{
    size_t b = 0, e = in.size();
    while (b < e && (in[b] == L' ' || in[b] == L'\t')) ++b;
    while (e > b && (in[e - 1] == L' ' || in[e - 1] == L'\t')) --e;
    return in.substr(b, e - b);
}

static bool WideAscii(const std::wstring& in, std::string& out)
{
    out.clear();
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] > 0x7F) return false;
        out.push_back((char)in[i]);
    }
    return true;
}

static bool IsNamePart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '_' || c == '-';
}

static bool ValidNamespace(const std::string& s)
{
    if (s.empty() || s.size() > 80) return false;
    for (size_t i = 0; i < s.size(); ++i) if (!IsNamePart(s[i])) return false;
    return true;
}

static bool ValidKey(const std::string& s)
{
    if (s.empty() || s.size() > 160 || s[0] == '.' || s[s.size() - 1] == '.')
        return false;
    bool lastDot = false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '.')
        {
            if (lastDot) return false;
            lastDot = true;
        }
        else
        {
            if (!IsNamePart(s[i])) return false;
            lastDot = false;
        }
    }
    return true;
}

static bool ValidLanguage(const std::string& s)
{
    if (s.size() <= 6 || s.compare(0, 6, "soviet") != 0) return false;
    for (size_t i = 6; i < s.size(); ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') ||
            s[i] == '_' || s[i] == '-')) return false;
    return true;
}

static bool ReadBytes(const char* path, size_t limit, Bytes& out)
{
    out.clear();
    HANDLE h = CreateFileA(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        ReportWindows("ERROR", path, 0, "read", "File could not be opened",
            GetLastError(), "Verify that the file exists and that the game can read it");
        return false;
    }

    LARGE_INTEGER n;
    if (!GetFileSizeEx(h, &n))
    {
        DWORD error = GetLastError();
        ReportWindows("ERROR", path, 0, "size",
            "File size could not be read", error,
            "Verify that the file is readable and retry");
        CloseHandle(h);
        return false;
    }
    if (n.QuadPart < 0 || (unsigned long long)n.QuadPart > limit)
    {
        Report("ERROR", path, 0, "size", "File exceeds the safety limit of %Iu bytes",
            limit);
        CloseHandle(h);
        return false;
    }

    out.resize((size_t)n.QuadPart);
    size_t done = 0;
    while (done < out.size())
    {
        DWORD want = (DWORD)((out.size() - done > 1u << 20) ? 1u << 20 : out.size() - done);
        DWORD got = 0;
        BOOL read = ReadFile(h, &out[done], want, &got, NULL);
        if (!read || got == 0)
        {
            DWORD error = read ? ERROR_HANDLE_EOF : GetLastError();
            ReportWindows("ERROR", path, 0, "read", "File was only partially read",
                error, "Close programs that may be modifying the file and retry");
            CloseHandle(h);
            out.clear();
            return false;
        }
        done += got;
    }
    CloseHandle(h);
    return true;
}

static bool DecodeUtf8(const char* path, const Bytes& raw, std::wstring& out)
{
    size_t skip = raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF ? 3 : 0;
    size_t n = raw.size() - skip;
    out.clear();
    if (!n) return true;
    if (n > INT_MAX)
    {
        Report("ERROR", path, 0, "utf8", "File is too large for the UTF-8 decoder");
        return false;
    }
    int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        (const char*)&raw[skip], (int)n, NULL, 0);
    if (chars <= 0)
    {
        Report("ERROR", path, 0, "utf8",
            "Invalid UTF-8; save the file as UTF-8 with or without a BOM");
        return false;
    }
    out.resize((size_t)chars);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        (const char*)&raw[skip], (int)n, &out[0], chars);
    return true;
}

static bool LoadUtf8(const char* path, std::wstring& out)
{
    Bytes raw;
    return ReadBytes(path, MAX_INI_BYTES, raw) && DecodeUtf8(path, raw, out);
}

static bool Unescape(const std::wstring& in, const char* file, unsigned line,
    const char* rule, std::wstring& out)
{
    out.clear();
    for (size_t i = 0; i < in.size(); ++i)
    {
        wchar_t c = in[i];
        if (c == 0)
        {
            Report("ERROR", file, line, rule, "NUL characters are not allowed");
            return false;
        }
        if (c != L'\\')
        {
            out.push_back(c);
            continue;
        }
        if (++i >= in.size())
        {
            Report("ERROR", file, line, rule,
                "A trailing backslash is invalid; only \\n and \\\\ are supported");
            return false;
        }
        wchar_t e = in[i];
        if (e == L'n')
        {
            out.push_back(L'\r');
            out.push_back(L'\n');
        }
        else if (e == L'\\') out.push_back(L'\\');
        else
        {
            Report("ERROR", file, line, rule,
                "Invalid escape sequence \\%c; only \\n and \\\\ are supported", (char)e);
            return false;
        }
    }
    if (out.size() > MAX_TEXT_UNITS)
    {
        Report("ERROR", file, line, rule,
            "Text contains %Iu UTF-16 code units; the BTF limit is %Iu",
            out.size(), MAX_TEXT_UNITS);
        return false;
    }
    return true;
}

template<class Fn>
static void ForEachLine(const std::wstring& body, Fn fn)
{
    size_t pos = 0;
    unsigned line = 1;
    while (pos <= body.size())
    {
        size_t end = pos;
        while (end < body.size() && body[end] != L'\r' && body[end] != L'\n') ++end;
        fn(body.substr(pos, end - pos), line);
        if (end == body.size()) break;
        if (body[end] == L'\r' && end + 1 < body.size() && body[end + 1] == L'\n') ++end;
        pos = end + 1;
        ++line;
    }
}

static bool ValidateMainConfig(const std::string& path)
{
    Bytes raw;
    if (!ReadBytes(path.c_str(), MAX_INI_BYTES, raw)) return false;
    if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB &&
        raw[2] == 0xBF)
    {
        Report("ERROR", path.c_str(), 1, "utf8-bom",
            "The main plugin INI must be UTF-8 without a BOM because the "
            "TesmioLoader configuration API cannot match its first section");
        return false;
    }

    std::wstring body;
    if (!DecodeUtf8(path.c_str(), raw, body)) return false;

    bool ok = true;
    bool inSection = false;
    bool sawSection = false;
    std::set<std::string> keys;
    ForEachLine(body, [&](const std::wstring& rawLine, unsigned line)
        {
            std::wstring text = TrimWide(rawLine);
            if (text.empty() || text[0] == L';' || text[0] == L'#') return;

            if (text[0] == L'[')
            {
                if (text.size() < 3 || text[text.size() - 1] != L']')
                {
                    Report("ERROR", path.c_str(), line,
                        "malformed-section", "Invalid section header");
                    ok = false;
                    inSection = false;
                    return;
                }
                std::string section;
                if (!WideAscii(TrimWide(
                    text.substr(1, text.size() - 2)), section))
                {
                    Report("ERROR", path.c_str(), line, "section",
                        "Section name must be ASCII");
                    ok = false;
                    inSection = false;
                    return;
                }
                section = LowerAscii(section);
                if (section != "localization")
                {
                    Report("ERROR", path.c_str(), line, "unknown-section",
                        "Unknown section [%s]; only [localization] is supported",
                        section.c_str());
                    ok = false;
                    inSection = false;
                    return;
                }
                if (sawSection)
                {
                    Report("ERROR", path.c_str(), line, "duplicate-section",
                        "Section [localization] appears more than once");
                    ok = false;
                    inSection = false;
                    return;
                }
                sawSection = true;
                inSection = true;
                return;
            }

            if (!inSection)
            {
                Report("ERROR", path.c_str(), line,
                    "assignment-outside-section",
                    "Configuration entry is not inside [localization]");
                ok = false;
                return;
            }

            size_t equals = text.find(L'=');
            if (equals == std::wstring::npos)
            {
                Report("ERROR", path.c_str(), line, "assignment",
                    "Missing '='");
                ok = false;
                return;
            }

            std::string key;
            if (!WideAscii(TrimWide(text.substr(0, equals)), key))
            {
                Report("ERROR", path.c_str(), line, "key",
                    "Configuration key must be ASCII");
                ok = false;
                return;
            }
            key = LowerAscii(key);
            if (key != "enabled" && key != "verbose")
            {
                Report("ERROR", path.c_str(), line, "unknown-key",
                    "Unknown [localization] key '%s'", key.c_str());
                ok = false;
                return;
            }
            if (!keys.insert(key).second)
            {
                Report("ERROR", path.c_str(), line, "duplicate-key",
                    "[localization] key '%s' appears more than once",
                    key.c_str());
                ok = false;
                return;
            }

            std::string value;
            if (!WideAscii(TrimWide(text.substr(equals + 1)), value) ||
                (value != "0" && value != "1"))
            {
                Report("ERROR", path.c_str(), line, "invalid-value",
                    "[localization] %s must be exactly 0 or 1",
                    key.c_str());
                ok = false;
            }
        });

    if (!sawSection)
    {
        Report("ERROR", path.c_str(), 0, "section",
            "The [localization] section is missing");
        ok = false;
    }
    return ok;
}

// ---------------------------------------------------------------- pack parsing

static bool ParsePackConfig(Pack& pack)
{
    std::string path = Join(pack.dir, "localization.ini");
    std::wstring body;
    if (!LoadUtf8(path.c_str(), body)) return false;

    bool ok = true, inSection = false, sawSection = false;
    bool gotNs = false, gotFallback = false, gotMissing = false;
    pack.missingText = L"[MISSING TEXT: {key}]";

    ForEachLine(body, [&](const std::wstring& raw, unsigned line)
        {
            std::wstring s = TrimWide(raw);
            if (s.empty() || s[0] == L';' || s[0] == L'#') return;
            if (s[0] == L'[')
            {
                if (s.size() < 3 || s[s.size() - 1] != L']')
                {
                    Report("ERROR", path.c_str(), line, "section", "Invalid section header");
                    ok = false; inSection = false; return;
                }
                std::string sec;
                if (!WideAscii(TrimWide(s.substr(1, s.size() - 2)), sec))
                {
                    Report("ERROR", path.c_str(), line, "section", "Section name must be ASCII");
                    ok = false; inSection = false; return;
                }
                sec = LowerAscii(sec);
                if (sec != "localization")
                {
                    Report("ERROR", path.c_str(), line, "unknown-section",
                        "Unknown section [%s]; only [localization] is supported",
                        sec.c_str());
                    ok = false;
                    inSection = false;
                    return;
                }
                if (sawSection)
                {
                    Report("ERROR", path.c_str(), line, "duplicate-section",
                        "Section [localization] appears more than once");
                    ok = false;
                    inSection = false;
                    return;
                }
                sawSection = true;
                inSection = true;
                return;
            }
            if (!inSection)
            {
                Report("ERROR", path.c_str(), line, "section", "Entry is not inside the [localization] section");
                ok = false; return;
            }
            size_t eq = s.find(L'=');
            if (eq == std::wstring::npos)
            {
                Report("ERROR", path.c_str(), line, "assignment", "Missing '='");
                ok = false; return;
            }
            std::string key;
            if (!WideAscii(TrimWide(s.substr(0, eq)), key))
            {
                Report("ERROR", path.c_str(), line, "key", "Configuration key must be ASCII");
                ok = false; return;
            }
            key = LowerAscii(key);
            std::wstring value = TrimWide(s.substr(eq + 1));

            if (key == "namespace")
            {
                if (gotNs) { Report("ERROR", path.c_str(), line, "namespace", "namespace is specified more than once"); ok = false; return; }
                gotNs = true;
                std::string v;
                if (!WideAscii(value, v) || !ValidNamespace(v))
                {
                    Report("ERROR", path.c_str(), line, "namespace",
                        "Only lowercase ASCII letters, digits, '_' and '-' are allowed");
                    ok = false; return;
                }
                pack.nameSpace = v;
            }
            else if (key == "fallback")
            {
                if (gotFallback) { Report("ERROR", path.c_str(), line, "fallback", "fallback is specified more than once"); ok = false; return; }
                gotFallback = true;
                std::string v;
                if (!WideAscii(value, v))
                {
                    Report("ERROR", path.c_str(), line, "fallback", "Language code must be ASCII");
                    ok = false; return;
                }
                v = LowerAscii(v);
                if (!ValidLanguage(v))
                {
                    Report("ERROR", path.c_str(), line, "fallback",
                        "Language code must use the format sovietEnglish, without .ini");
                    ok = false; return;
                }
                pack.fallback = v;
            }
            else if (key == "missingtext")
            {
                if (gotMissing) { Report("ERROR", path.c_str(), line, "missingText", "missingText is specified more than once"); ok = false; return; }
                gotMissing = true;
                std::wstring decoded;
                if (!Unescape(value, path.c_str(), line, "missingText", decoded)) { ok = false; return; }
                if (decoded.empty())
                {
                    Report("ERROR", path.c_str(), line, "missingText", "missingText must not be empty");
                    ok = false; return;
                }
                pack.missingText = decoded;
                if (decoded.find(L"{key}") == std::wstring::npos)
                    Report("WARN", path.c_str(), line, "missingText",
                        "{key} is missing; missing keys will be harder to identify in the game");
            }
            else
            {
                Report("ERROR", path.c_str(), line, "unknown-key",
                    "Unknown [localization] entry '%s'", key.c_str());
                ok = false;
            }
        });

    if (!sawSection) { Report("ERROR", path.c_str(), 0, "section", "The [localization] section is missing"); ok = false; }
    if (!gotNs) { Report("ERROR", path.c_str(), 0, "namespace", "namespace is missing"); ok = false; }
    if (!gotFallback) { Report("ERROR", path.c_str(), 0, "fallback", "fallback is missing"); ok = false; }
    return ok;
}

static bool ParseLanguageFile(const std::string& path,
    std::map<std::string, std::wstring>& strings)
{
    strings.clear();
    std::wstring body;
    if (!LoadUtf8(path.c_str(), body)) return false;

    bool ok = true, inStrings = false, sawStrings = false;
    ForEachLine(body, [&](const std::wstring& raw, unsigned line)
        {
            std::wstring s = TrimWide(raw);
            if (s.empty() || s[0] == L';' || s[0] == L'#') return;
            if (s[0] == L'[')
            {
                if (s.size() < 3 || s[s.size() - 1] != L']')
                {
                    Report("ERROR", path.c_str(), line, "section", "Invalid section header");
                    ok = false; inStrings = false; return;
                }
                std::string sec;
                if (!WideAscii(TrimWide(s.substr(1, s.size() - 2)), sec))
                {
                    Report("ERROR", path.c_str(), line, "section", "Section name must be ASCII");
                    ok = false; inStrings = false; return;
                }
                sec = LowerAscii(sec);
                if (sec != "strings")
                {
                    Report("ERROR", path.c_str(), line, "unknown-section",
                        "Unknown section [%s]; only [strings] is supported",
                        sec.c_str());
                    ok = false;
                    inStrings = false;
                    return;
                }
                if (sawStrings)
                {
                    Report("ERROR", path.c_str(), line, "duplicate-section",
                        "Section [strings] appears more than once");
                    ok = false;
                    inStrings = false;
                    return;
                }
                inStrings = true;
                sawStrings = true;
                return;
            }
            if (!inStrings)
            {
                Report("ERROR", path.c_str(), line, "section", "Text entry is not inside the [strings] section");
                ok = false; return;
            }
            size_t eq = s.find(L'=');
            if (eq == std::wstring::npos)
            {
                Report("ERROR", path.c_str(), line, "assignment", "Missing '='");
                ok = false; return;
            }
            std::string key;
            if (!WideAscii(TrimWide(s.substr(0, eq)), key) || !ValidKey(key))
            {
                Report("ERROR", path.c_str(), line, "string-key",
                    "Invalid key; use lowercase ASCII letters, digits, '_', '-' and single dots only");
                ok = false; return;
            }
            if (strings.find(key) != strings.end())
            {
                Report("ERROR", path.c_str(), line, "duplicate-key", "Key '%s' is specified more than once", key.c_str());
                ok = false; return;
            }
            std::wstring decoded;
            if (!Unescape(TrimWide(s.substr(eq + 1)), path.c_str(), line, key.c_str(), decoded))
            {
                ok = false; return;
            }
            if (decoded.empty())
            {
                Report("ERROR", path.c_str(), line, key.c_str(),
                    "Empty text is not allowed because the game treats it as missing");
                ok = false; return;
            }
            strings[key] = decoded;
        });

    if (!sawStrings)
    {
        Report("ERROR", path.c_str(), 0, "section", "The [strings] section is missing");
        ok = false;
    }
    if (!ok) strings.clear();
    return ok;
}

static std::string LanguageFromIniName(const std::string& file)
{
    std::string low = LowerAscii(file);
    if (low.size() <= 4 || low.compare(low.size() - 4, 4, ".ini") != 0) return std::string();
    low.resize(low.size() - 4);
    return ValidLanguage(low) ? low : std::string();
}

static bool LoadOnePack(const std::string& root, const std::string& folder, Pack& out)
{
    out = Pack();
    out.folder = folder;
    out.dir = Join(root, folder);
    out.valid = false;
    std::string configPath = Join(out.dir, "localization.ini");
    if (IsReparsePoint(configPath))
    {
        Report("ERROR", configPath.c_str(), 0, "reparse-point",
            "Symbolic-link and reparse-point configuration files are not "
            "loaded for security reasons");
        return false;
    }
    if (!ParsePackConfig(out))
    {
        Report("ERROR", out.dir.c_str(), 0, "pack", "Pack was rejected because its localization.ini is invalid");
        return false;
    }

    std::string pattern = Join(out.dir, "soviet*.ini");
    WIN32_FIND_DATAA fd;
    HANDLE find = FindFirstFileA(pattern.c_str(), &fd);
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string file = fd.cFileName;
            std::string path = Join(out.dir, file);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                Report("WARN", path.c_str(), 0, "reparse-point",
                    "Symbolic-link and reparse-point language files are ignored "
                    "for security reasons");
                continue;
            }
            std::string lang = LanguageFromIniName(file);
            if (lang.empty())
            {
                Report("WARN", Join(out.dir, file).c_str(), 0, "language-file",
                    "Filename is not a valid soviet<Language>.ini name and is ignored");
                continue;
            }
            std::map<std::string, std::wstring> values;
            if (!ParseLanguageFile(path, values))
            {
                Report("ERROR", path.c_str(), 0, "language-file",
                    "Entire language file was discarded; the fallback will be used for this language");
                continue;
            }
            if (out.languages.find(lang) != out.languages.end())
            {
                Report("ERROR", path.c_str(), 0, "language-file",
                    "Language '%s' is defined more than once in this pack", lang.c_str());
                continue;
            }
            out.languages[lang] = values;
        } while (FindNextFileA(find, &fd));
        DWORD scanError = GetLastError();
        FindClose(find);
        if (scanError != ERROR_NO_MORE_FILES)
        {
            ReportWindows("ERROR", pattern.c_str(), 0, "language-file",
                "Language-file enumeration ended unexpectedly", scanError,
                "Verify access to the text-pack directory and retry");
            return false;
        }
    }
    else
    {
        DWORD scanError = GetLastError();
        if (scanError != ERROR_FILE_NOT_FOUND)
        {
            ReportWindows("ERROR", pattern.c_str(), 0, "language-file",
                "Language files could not be enumerated", scanError,
                "Verify access to the text-pack directory and retry");
            return false;
        }
    }

    // 1.3: fallback and text checks run in FinishPack, after a local folder
    // may have been merged over this one (LoadPacks).
    return true;
}

// The checks that need the complete text of a pack: the fallback language must
// exist, at least one key, keys missing from the fallback are reported.
static bool FinishPack(Pack& out)
{
    if (out.languages.find(out.fallback) == out.languages.end())
    {
        Report("ERROR", Join(out.dir, "localization.ini").c_str(), 0, "fallback",
            "Fallback '%s' has no valid language file; the pack was rejected",
            out.fallback.c_str());
        return false;
    }

    std::set<std::string> all;
    for (std::map<std::string, std::map<std::string, std::wstring> >::const_iterator l = out.languages.begin();
        l != out.languages.end(); ++l)
        for (std::map<std::string, std::wstring>::const_iterator k = l->second.begin(); k != l->second.end(); ++k)
            all.insert(k->first);

    if (all.empty())
    {
        Report("ERROR", out.dir.c_str(), 0, "pack", "Pack does not contain any valid text");
        return false;
    }

    const std::map<std::string, std::wstring>& fallback = out.languages[out.fallback];
    for (std::set<std::string>::const_iterator k = all.begin(); k != all.end(); ++k)
        if (fallback.find(*k) == fallback.end())
            Report("WARN", Join(out.dir, "localization.ini").c_str(), 0, "fallback-key",
                "Key '%s' is missing from fallback '%s'; missingText will be used",
                k->c_str(), out.fallback.c_str());

    out.valid = true;
    return true;
}

static bool EnumeratePackFolders(const std::string& root, std::vector<std::string>& folders)
{
    bool limitExceeded = false;
    WIN32_FIND_DATAA fd;
    HANDLE find = FindFirstFileA(Join(root, "*").c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE)
    {
        ReportWindows("ERROR", root.c_str(), 0, "pack-scan",
            "Pack directories could not be enumerated", GetLastError(),
            "Verify access to the localization directory and retry");
        return false;
    }
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            Report("WARN", Join(root, fd.cFileName).c_str(), 0, "reparse-point",
                "Junctions and symbolic-link directories are not scanned for security reasons");
            continue;
        }
        if (folders.size() >= MAX_PACKS)
        {
            Report("ERROR", root.c_str(), 0, "pack-limit",
                "More than %Iu pack directories were found; no packs are "
                "loaded until the limit is respected", MAX_PACKS);
            limitExceeded = true;
            break;
        }
        folders.push_back(fd.cFileName);
    } while (FindNextFileA(find, &fd));
    DWORD scanError = GetLastError();
    FindClose(find);
    if (limitExceeded) return false;
    if (scanError != ERROR_NO_MORE_FILES)
    {
        ReportWindows("ERROR", root.c_str(), 0, "pack-scan",
            "Pack-directory enumeration ended unexpectedly", scanError,
            "Verify access to the localization directory and retry");
        return false;
    }
    std::sort(folders.begin(), folders.end());
    return true;
}

static void LoadPacksInto(const std::string& root, const std::vector<std::string>& folders,
                          std::vector<Pack>& out)
{
    for (size_t i = 0; i < folders.size(); ++i)
    {
        std::string cfg = Join(Join(root, folders[i]), "localization.ini");
        if (!FileExists(cfg))
        {
            Report("WARN", cfg.c_str(), 0, "pack", "No localization.ini found; directory is ignored");
            continue;
        }
        Pack p;
        if (LoadOnePack(root, folders[i], p)) out.push_back(p);
    }
}

// 1.3: a local pack folder is merged over the shipped folder of the same name,
// key by key: the local localization.ini decides namespace, fallback and
// missingText, every local language file adds or replaces keys, languages that
// only ship with the package stay. Before 1.3 the local folder replaced the
// shipped one completely, so texts added by a package update never reached a
// player who had once copied the pack.
static void MergePack(Pack& shipped, const Pack& local)
{
    size_t added = 0, replaced = 0;
    for (std::map<std::string, std::map<std::string, std::wstring> >::const_iterator l = local.languages.begin();
        l != local.languages.end(); ++l)
    {
        std::map<std::string, std::wstring>& target = shipped.languages[l->first];
        for (std::map<std::string, std::wstring>::const_iterator k = l->second.begin(); k != l->second.end(); ++k)
        {
            if (target.find(k->first) == target.end()) ++added; else ++replaced;
            target[k->first] = k->second;
        }
    }
    if (shipped.nameSpace != local.nameSpace)
        Report("WARN", Join(local.dir, "localization.ini").c_str(), 0, "namespace",
            "Local pack '%s' uses namespace '%s' while the shipped pack uses '%s'; the local one applies",
            local.folder.c_str(), local.nameSpace.c_str(), shipped.nameSpace.c_str());
    shipped.nameSpace = local.nameSpace;
    shipped.fallback = local.fallback;
    shipped.missingText = local.missingText;
    shipped.dir = local.dir;
    Info("Pack '%s': local folder merged over the shipped one (%Iu keys added, %Iu replaced, %Iu local language file(s))",
        local.folder.c_str(), added, replaced, local.languages.size());
}

// 1.2/1.3: text packs come from plugins\localization (classic installation, or
// the folder Republic Mod Manager writes) and, when the DLL runs from a Workshop
// package, additionally from the folder localization beside the DLL. Shipped
// packs are loaded first; a local folder of the same name is merged over it.
static bool LoadPacks(void)
{
    const bool primary = DirExists(g_packRoot);
    if (!primary && g_packRootBeside.empty())
    {
        Report("ERROR", g_packRoot.c_str(), 0, "pack-root", "Pack directory is missing");
        return false;
    }

    std::vector<std::string> primaryFolders, besideFolders;
    if (primary && !EnumeratePackFolders(g_packRoot, primaryFolders)) return false;
    if (!g_packRootBeside.empty() && !EnumeratePackFolders(g_packRootBeside, besideFolders)) return false;
    if (primaryFolders.size() + besideFolders.size() > MAX_PACKS)
    {
        Report("ERROR", g_packRoot.c_str(), 0, "pack-limit",
            "More than %Iu pack directories were found across both pack folders; no packs are "
            "loaded until the limit is respected", MAX_PACKS);
        return false;
    }

    std::vector<Pack> shipped, local;
    if (!g_packRootBeside.empty()) LoadPacksInto(g_packRootBeside, besideFolders, shipped);
    if (primary) LoadPacksInto(g_packRoot, primaryFolders, local);
    for (size_t i = 0; i < local.size(); ++i)
    {
        size_t match = shipped.size();
        for (size_t j = 0; j < shipped.size(); ++j)
            if (LowerAscii(shipped[j].folder) == LowerAscii(local[i].folder)) { match = j; break; }
        if (match < shipped.size()) MergePack(shipped[match], local[i]);
        else shipped.push_back(local[i]);
    }
    g_packs.reserve(shipped.size());
    for (size_t i = 0; i < shipped.size(); ++i)
        if (FinishPack(shipped[i])) g_packs.push_back(shipped[i]);

    std::map<std::string, std::vector<size_t> > owners;
    for (size_t i = 0; i < g_packs.size(); ++i) owners[g_packs[i].nameSpace].push_back(i);
    for (std::map<std::string, std::vector<size_t> >::iterator it = owners.begin(); it != owners.end(); ++it)
    {
        if (it->second.size() < 2) continue;
        for (size_t n = 0; n < it->second.size(); ++n)
        {
            Pack& p = g_packs[it->second[n]];
            p.valid = false;
            Report("ERROR", Join(p.dir, "localization.ini").c_str(), 0, "namespace-collision",
                "Namespace '%s' is used by %Iu packs; all conflicting packs were rejected",
                it->first.c_str(), it->second.size());
        }
    }
    return true;
}

// ---------------------------------------------------------------- BTF container

static unsigned Be32(const unsigned char* p)
{
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
        ((unsigned)p[2] << 8) | (unsigned)p[3];
}

static unsigned Be16(const unsigned char* p)
{
    return ((unsigned)p[0] << 8) | (unsigned)p[1];
}

static void PutBe32(Bytes& out, unsigned v)
{
    out.push_back((unsigned char)(v >> 24));
    out.push_back((unsigned char)(v >> 16));
    out.push_back((unsigned char)(v >> 8));
    out.push_back((unsigned char)v);
}

static void PutBe16(Bytes& out, unsigned v)
{
    out.push_back((unsigned char)(v >> 8));
    out.push_back((unsigned char)v);
}

static bool ParseBtf(const char* path, const Bytes& raw, std::vector<BtfEntry>& out)
{
    out.clear();
    if (raw.size() < 12)
    {
        Report("ERROR", path, 0, "btf-header", "File is shorter than the 12-byte BTF header");
        return false;
    }
    unsigned count = Be32(&raw[0]);
    unsigned units = Be32(&raw[8]);
    unsigned long long base = 12ull + (unsigned long long)count * 10ull;
    unsigned long long need = base + (unsigned long long)units * 2ull;
    if (count > 1000000 || need > raw.size())
    {
        Report("ERROR", path, 0, "btf-header",
            "Invalid sizes: count=%u, calculated=%llu, file=%Iu",
            count, need, raw.size());
        return false;
    }

    const unsigned char* payload = &raw[(size_t)base];
    out.reserve(count);
    for (unsigned i = 0; i < count; ++i)
    {
        const unsigned char* rec = &raw[12 + (size_t)i * 10];
        unsigned off = Be32(rec + 4);
        unsigned len = Be16(rec + 8);
        if ((unsigned long long)off + len >= units)
        {
            Report("ERROR", path, 0, "btf-record",
                "Entry %u (ID %u) is outside the text data", i, Be32(rec));
            return false;
        }
        if (Be16(payload + ((size_t)off + len) * 2) != 0)
        {
            Report("ERROR", path, 0, "btf-record",
                "Entry %u (ID %u) is not NUL-terminated", i, Be32(rec));
            return false;
        }
        BtfEntry e;
        e.id = Be32(rec);
        e.text.resize(len);
        for (unsigned k = 0; k < len; ++k)
            e.text[k] = (wchar_t)Be16(payload + ((size_t)off + k) * 2);
        out.push_back(e);
    }
    return true;
}

static bool LoadBtf(const char* path, std::vector<BtfEntry>& out)
{
    Bytes raw;
    return ReadBytes(path, MAX_BTF_BYTES, raw) && ParseBtf(path, raw, out);
}

static bool BuildBtf(const char* name, const std::vector<BtfEntry>& in, Bytes& out)
{
    if (in.size() > 0xFFFFFFFFull)
    {
        Report("ERROR", name, 0, "btf-build",
            "Combined BTF contains too many entries");
        return false;
    }

    unsigned long long totalUnits = 0;
    unsigned long long total = 12ull +
        (unsigned long long)in.size() * 10ull;
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i].text.size() > MAX_TEXT_UNITS)
        {
            Report("ERROR", name, 0, "btf-build",
                "ID %u has an invalid text length of %Iu",
                in[i].id, in[i].text.size());
            return false;
        }
        unsigned long long entryUnits =
            (unsigned long long)in[i].text.size() + 1ull;
        if (totalUnits + entryUnits > 0xFFFFFFFFull ||
            total + entryUnits * 2ull > MAX_BTF_BYTES)
        {
            Report("ERROR", name, 0, "btf-build",
                "Combined BTF exceeds the safety limit of %Iu bytes",
                MAX_BTF_BYTES);
            return false;
        }
        totalUnits += entryUnits;
        total += entryUnits * 2ull;
    }

    Bytes records, payload;
    records.reserve(in.size() * 10);
    payload.reserve((size_t)totalUnits * 2u);
    unsigned long long unit = 0;
    for (size_t i = 0; i < in.size(); ++i)
    {
        PutBe32(records, in[i].id);
        PutBe32(records, (unsigned)unit);
        PutBe16(records, (unsigned)in[i].text.size());
        for (size_t k = 0; k < in[i].text.size(); ++k)
            PutBe16(payload, (unsigned)(unsigned short)in[i].text[k]);
        PutBe16(payload, 0);
        unit += in[i].text.size() + 1;
    }
    if (total > 0xFFFFFFFFull || unit > 0xFFFFFFFFull)
    {
        Report("ERROR", name, 0, "btf-build", "Combined BTF is too large");
        return false;
    }
    out.clear();
    out.reserve((size_t)total);
    PutBe32(out, (unsigned)in.size());
    PutBe32(out, (unsigned)total);
    PutBe32(out, (unsigned)unit);
    out.insert(out.end(), records.begin(), records.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return true;
}

static bool WriteAtomic(const std::string& path, const Bytes& data)
{
    std::string tmp = path + ".tmp";
    HANDLE h = CreateFileA(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        ReportWindows("ERROR", path.c_str(), 0, "temp-write",
            "Temporary BTF could not be created", GetLastError(),
            "Verify write access and available disk space in the VFS directory");
        return false;
    }
    size_t done = 0;
    while (done < data.size())
    {
        DWORD want = (DWORD)((data.size() - done > 1u << 20) ? 1u << 20 : data.size() - done);
        DWORD put = 0;
        BOOL written = WriteFile(h, &data[done], want, &put, NULL);
        if (!written || put != want)
        {
            DWORD error = written ? ERROR_WRITE_FAULT : GetLastError();
            ReportWindows("ERROR", path.c_str(), 0, "temp-write",
                "Temporary BTF was only partially written", error,
                "Verify available disk space and write access, then retry");
            CloseHandle(h);
            DeleteFileA(tmp.c_str());
            return false;
        }
        done += put;
    }
    if (!FlushFileBuffers(h))
    {
        DWORD error = GetLastError();
        ReportWindows("ERROR", path.c_str(), 0, "temp-flush",
            "Temporary BTF could not be flushed to disk", error,
            "Verify the storage device and available disk space, then retry");
        CloseHandle(h);
        DeleteFileA(tmp.c_str());
        return false;
    }
    if (!CloseHandle(h))
    {
        DWORD error = GetLastError();
        ReportWindows("ERROR", path.c_str(), 0, "temp-close",
            "Temporary BTF could not be closed", error,
            "Close programs using the VFS directory and retry");
        DeleteFileA(tmp.c_str());
        return false;
    }
    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        ReportWindows("ERROR", path.c_str(), 0, "temp-write",
            "Temporary BTF could not be activated", GetLastError(),
            "Close programs using the generated file and retry");
        DeleteFileA(tmp.c_str());
        return false;
    }
    return true;
}

static bool DeleteGeneratedFile(const std::string& path, int* removed)
{
    DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return true;
        ReportWindows("ERROR", path.c_str(), 0, "cleanup",
            "Generated VFS file could not be inspected", error,
            "Verify VFS access and remove the stale file before retrying");
        return false;
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        Report("ERROR", path.c_str(), 0, "cleanup",
            "Expected a generated file but found a directory");
        return false;
    }
    if (!DeleteFileA(path.c_str()))
    {
        ReportWindows("ERROR", path.c_str(), 0, "cleanup",
            "Generated VFS file could not be removed", GetLastError(),
            "Close programs using the file and remove it before retrying");
        return false;
    }
    if (removed) ++*removed;
    return true;
}

// Earlier builds wrote one VFS file for every original soviet*.btf without a
// manifest. The intersection with the original filenames is therefore the
// narrowest ownership rule that cleans legacy output without touching
// unrelated custom VFS language files.
static bool RemoveGeneratedOverlays(void)
{
    std::string vfsMediaSoviet = Join(g_vfsDir, VFS_MEDIA_REL);
    std::string pattern = Join(g_mediaDir, "soviet*.btf");
    WIN32_FIND_DATAA fd;
    HANDLE find = FindFirstFileA(pattern.c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) return true;
        ReportWindows("ERROR", pattern.c_str(), 0, "cleanup-scan",
            "Original language files could not be enumerated", error,
            "Verify the game installation and retry");
        return false;
    }

    bool ok = true;
    int removed = 0;
    BOOL more = TRUE;
    while (more)
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            std::string output = Join(vfsMediaSoviet, fd.cFileName);
            if (!DeleteGeneratedFile(output, &removed)) ok = false;
            if (!DeleteGeneratedFile(output + ".tmp", &removed)) ok = false;
        }
        more = FindNextFileA(find, &fd);
    }
    DWORD scanError = GetLastError();
    FindClose(find);
    if (scanError != ERROR_NO_MORE_FILES)
    {
        ReportWindows("ERROR", pattern.c_str(), 0, "cleanup-scan",
            "Original language-file enumeration ended unexpectedly",
            scanError, "Verify the game installation and retry");
        ok = false;
    }

    if (ok)
        Info("Removed %d previous generated VFS language file(s)", removed);
    return ok;
}

// ---------------------------------------------------------------- ids and service

static bool ScanIdsInFile(const std::string& path)
{
    std::vector<BtfEntry> entries;
    if (!LoadBtf(path.c_str(), entries)) return false;
    for (size_t i = 0; i < entries.size(); ++i) g_usedIds.insert(entries[i].id);
    return true;
}

static bool ScanBtfFolder(const std::string& dir, bool required, int* files)
{
    DWORD attributes = GetFileAttributesA(dir.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return !required;
        ReportWindows("ERROR", dir.c_str(), 0, "btf-scan",
            "BTF directory could not be inspected", error,
            "Verify directory access and retry");
        return false;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        Report("ERROR", dir.c_str(), 0, "btf-scan",
            "Expected a BTF directory but found a file");
        return false;
    }
    WIN32_FIND_DATAA fd;
    HANDLE find = FindFirstFileA(Join(dir, "soviet*.btf").c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) return !required;
        ReportWindows("ERROR", dir.c_str(), 0, "btf-scan",
            "BTF files could not be enumerated", error,
            "Verify directory access and retry");
        return false;
    }
    bool ok = true;
    BOOL more = TRUE;
    while (more)
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            std::string path = Join(dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                Report("ERROR", path.c_str(), 0, "reparse-point",
                    "Symbolic-link and reparse-point BTF files are not scanned "
                    "for security reasons");
                ok = false;
            }
            else
            {
                ++*files;
                if (!ScanIdsInFile(path)) ok = false;
            }
        }
        more = FindNextFileA(find, &fd);
    }
    DWORD scanError = GetLastError();
    FindClose(find);
    if (scanError != ERROR_NO_MORE_FILES)
    {
        ReportWindows("ERROR", dir.c_str(), 0, "btf-scan",
            "BTF enumeration ended unexpectedly", scanError,
            "Verify directory access and retry");
        ok = false;
    }
    return ok;
}

static bool ReserveSourceIds(void)
{
    g_usedIds.clear();
    g_nextId = TSM_LOCALIZATION_ID_BASE;
    int gameFiles = 0, vfsFiles = 0;
    bool ok = ScanBtfFolder(g_mediaDir, true, &gameFiles);
    if (!g_vfsDir.empty())
        ok = ScanBtfFolder(Join(g_vfsDir, "media_soviet"), false, &vfsFiles) && ok;
    if (!gameFiles)
    {
        Report("ERROR", g_mediaDir.c_str(), 0, "btf-scan", "No soviet*.btf language file found");
        return false;
    }
    if (!ok)
    {
        Report("ERROR", NULL, 0, "btf-scan", "At least one language file is invalid; no IDs will be allocated");
        return false;
    }
    Info("Scanned %d original and %d virtual BTF files; %Iu IDs are already in use", gameFiles, vfsFiles, g_usedIds.size());
    return true;
}

static bool TakeId(unsigned& out)
{
    while (g_nextId <= TSM_LOCALIZATION_ID_LIMIT && g_usedIds.find(g_nextId) != g_usedIds.end()) ++g_nextId;
    if (g_nextId > TSM_LOCALIZATION_ID_LIMIT) return false;
    out = g_nextId++;
    g_usedIds.insert(out);
    return true;
}

static bool BuildKeyRegistry(void)
{
    std::vector<KeyEntry> pending;
    for (size_t p = 0; p < g_packs.size(); ++p)
    {
        if (!g_packs[p].valid) continue;
        std::set<std::string> locals;
        for (std::map<std::string, std::map<std::string, std::wstring> >::const_iterator l = g_packs[p].languages.begin();
            l != g_packs[p].languages.end(); ++l)
            for (std::map<std::string, std::wstring>::const_iterator k = l->second.begin(); k != l->second.end(); ++k)
                locals.insert(k->first);
        for (std::set<std::string>::const_iterator k = locals.begin(); k != locals.end(); ++k)
        {
            KeyEntry e;
            e.full = g_packs[p].nameSpace + "." + *k;
            e.local = *k;
            e.pack = p;
            e.id = 0;
            pending.push_back(e);
        }
    }
    std::sort(pending.begin(), pending.end(), [](const KeyEntry& a, const KeyEntry& b) { return a.full < b.full; });
    if (pending.size() > MAX_KEYS)
    {
        Report("ERROR", NULL, 0, "key-limit", "%Iu texts exceed the safety limit of %Iu", pending.size(), MAX_KEYS);
        return false;
    }
    for (size_t i = 0; i < pending.size(); ++i)
    {
        if (!TakeId(pending[i].id))
        {
            Report("ERROR", NULL, 0, "id-range", "Reserved ID range %u..%u is exhausted",
                TSM_LOCALIZATION_ID_BASE, TSM_LOCALIZATION_ID_LIMIT);
            return false;
        }
        g_keyIndex[pending[i].full] = g_keys.size();
        g_keys.push_back(pending[i]);
    }
    return !g_keys.empty();
}

static size_t FindPack(const std::string& ns)
{
    for (size_t i = 0; i < g_packs.size(); ++i)
        if (g_packs[i].valid && g_packs[i].nameSpace == ns) return i;
    return (size_t)-1;
}

static int ResolveKey(const std::string& ns, const std::string& key)
{
    if (!InterlockedCompareExchange(&g_operational, 0, 0)) return 0;
    if (!ValidNamespace(ns) || !ValidKey(key))
    {
        Report("ERROR", NULL, 0, "resolve", "Invalid namespace/key '%s.%s'", ns.c_str(), key.c_str());
        return 0;
    }
    std::string full = ns + "." + key;
    std::map<std::string, size_t>::const_iterator have = g_keyIndex.find(full);
    if (have != g_keyIndex.end()) return (int)g_keys[have->second].id;

    size_t pack = FindPack(ns);
    if (pack == (size_t)-1)
    {
        Report("ERROR", NULL, 0, "resolve", "Unknown namespace '%s' for key '%s'", ns.c_str(), key.c_str());
        return 0;
    }
    Report("ERROR", Join(g_packs[pack].dir, "localization.ini").c_str(),
        0, "missing-key",
        "Key '%s' is not defined in any language file and cannot be resolved",
        full.c_str());
    return 0;
}

static int svc_Resolve(const char* nameSpace, const char* key)
{
    if (!nameSpace || !key) return 0;
    return ResolveKey(nameSpace, key);
}

static int svc_ResolveFull(const char* full)
{
    if (!full) return 0;
    const char* dot = strchr(full, '.');
    if (!dot || dot == full || !dot[1])
    {
        Report("ERROR", NULL, 0, "resolve", "Fully qualified key '%s' must use namespace.key", full);
        return 0;
    }
    return ResolveKey(std::string(full, dot), dot + 1);
}

static const TsmLocalizationApi kLocalizationApi = { svc_Resolve, svc_ResolveFull };

// ---------------------------------------------------------------- language overlay

static std::wstring MissingFor(const Pack& pack, const std::string& full)
{
    std::wstring key;
    key.reserve(full.size());
    for (size_t i = 0; i < full.size(); ++i) key.push_back((wchar_t)(unsigned char)full[i]);
    std::wstring out = pack.missingText;
    size_t at = 0;
    while ((at = out.find(L"{key}", at)) != std::wstring::npos)
    {
        out.replace(at, 5, key);
        at += key.size();
    }
    return out;
}

static std::wstring TextFor(const KeyEntry& e, const std::string& language, int* source)
{
    const Pack& p = g_packs[e.pack];
    std::map<std::string, std::map<std::string, std::wstring> >::const_iterator l = p.languages.find(language);
    if (l != p.languages.end())
    {
        std::map<std::string, std::wstring>::const_iterator k = l->second.find(e.local);
        if (k != l->second.end()) { *source = 0; return k->second; }
    }
    l = p.languages.find(p.fallback);
    if (l != p.languages.end())
    {
        std::map<std::string, std::wstring>::const_iterator k = l->second.find(e.local);
        if (k != l->second.end()) { *source = 1; return k->second; }
    }
    *source = 2;
    return MissingFor(p, e.full);
}

static std::string LanguageFromBtfName(const std::string& file)
{
    std::string low = LowerAscii(file);
    if (low.size() <= 4 || low.compare(low.size() - 4, 4, ".btf") != 0) return std::string();
    low.resize(low.size() - 4);
    return ValidLanguage(low) ? low : std::string();
}

static bool BuildOverlayForLanguage(const std::string& sourcePath,
    const std::string& language,
    std::string& generatedPath)
{
    generatedPath.clear();

    std::vector<BtfEntry> entries;
    if (!LoadBtf(sourcePath.c_str(), entries)) return false;

    std::set<unsigned> ids;
    for (size_t i = 0; i < entries.size(); ++i) ids.insert(entries[i].id);
    for (size_t i = 0; i < g_keys.size(); ++i)
        if (ids.find(g_keys[i].id) != ids.end())
        {
            Report("ERROR", sourcePath.c_str(), 0, "id-collision",
                "ID %u for '%s' already exists in this BTF; the original file will be loaded unchanged",
                g_keys[i].id, g_keys[i].full.c_str());
            return false;
        }

    int direct = 0, fallback = 0, missing = 0;
    entries.reserve(entries.size() + g_keys.size());
    for (size_t i = 0; i < g_keys.size(); ++i)
    {
        int kind = 0;
        BtfEntry e;
        e.id = g_keys[i].id;
        e.text = TextFor(g_keys[i], language, &kind);
        if (e.text.empty() || e.text.size() > MAX_TEXT_UNITS)
        {
            Report("ERROR", sourcePath.c_str(), 0, "overlay-text", "Invalid text for '%s'", g_keys[i].full.c_str());
            return false;
        }
        if (kind == 0) ++direct; else if (kind == 1) ++fallback; else ++missing;
        entries.push_back(e);
    }

    // NOTE: Do NOT sort entries here. The game expects the original order plus appended entries.

    Bytes raw;
    if (!BuildBtf(sourcePath.c_str(), entries, raw)) return false;

    std::string base = BaseName(sourcePath.c_str());
    std::string vfsMediaSoviet = Join(g_vfsDir, VFS_MEDIA_REL);
    if (!DirExists(vfsMediaSoviet))
    {
        Report("ERROR", vfsMediaSoviet.c_str(), 0, "vfs-dir",
            "VFS media_soviet directory is missing");
        return false;
    }

    generatedPath = Join(vfsMediaSoviet, base);
    if (!WriteAtomic(generatedPath, raw)) { generatedPath.clear(); return false; }

    Info("%s: %Iu vanilla + %Iu plugin texts (%d direct, %d fallback, %d missingText)",
        base.c_str(), entries.size() - g_keys.size(), g_keys.size(), direct, fallback, missing);
    if (g_verbose) Info("Extended BTF: %s (source %s)", generatedPath.c_str(), sourcePath.c_str());
    return true;
}

// ---------------------------------------------------------------- plugin entry

static bool ResolveGameFolders(void)
{
    char exe[MAX_PATH];
    DWORD length = GetModuleFileNameA(g_exe, exe, (DWORD)sizeof(exe));
    if (!length || length >= sizeof(exe)) return false;
    char* slash = strrchr(exe, '\\');
    if (!slash) return false;
    *slash = 0;
    g_gameDir = exe;
    g_mediaDir = Join(g_gameDir, "media_soviet");
    return DirExists(g_mediaDir);
}

static void ResetRuntimeState(void)
{
    InterlockedExchange(&g_operational, 0);
    g_packs.clear();
    g_keys.clear();
    g_keyIndex.clear();
    g_usedIds.clear();
    g_nextId = TSM_LOCALIZATION_ID_BASE;
    g_gameDir.clear();
    g_mediaDir.clear();
    g_vfsDir.clear();
    g_packRoot.clear();
    g_packRootBeside.clear();
    g_verbose = 0;
}

static void CleanupGeneratedAfterFailure(void)
{
    InterlockedExchange(&g_operational, 0);
    if (g_mediaDir.empty() || g_vfsDir.empty()) return;
    try
    {
        if (!RemoveGeneratedOverlays())
            Report("FATAL", NULL, 0, "cleanup",
                "Generated VFS language files could not be removed after a "
                "failure. Action: close the game and remove the reported "
                "files before retrying");
    }
    catch (...)
    {
        Report("FATAL", NULL, 0, "cleanup-exception",
            "Cleanup raised an unexpected exception. Action: close the game "
            "and inspect vfs\\media_soviet for generated soviet*.btf files");
    }
}

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion(void)
{
    return TSM_API_VERSION;
}

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host, TsmPluginInfo* info)
{
    if (!host || !info) return 1;
    TsmBind(host);
    g_bound = true;
    ResetRuntimeState();
    info->name = "localization";
    info->version = PLUGIN_VERSION;
    g_logStarted = GetTickCount64();
    BeginLogPhase();
    g_detail = TsmOpenLog(PLUGIN_LOG_NAME);
    if (g_detail == INVALID_HANDLE_VALUE)
        ReportWindows("WARN", PLUGIN_LOG_NAME, 0, "log-open",
            "Detail log could not be opened", GetLastError(),
            "Verify write access to the TesmioLoader log directory");
    Info("TesmioLoader localization %s starting; detail log: %s",
        PLUGIN_VERSION, PLUGIN_LOG_NAME);
    Info("Escapes: \\n = newline, \\\\ = backslash. Original BTF files are never changed");

    try
    {
        if (!g_baseDir || !*g_baseDir)
        {
            Report("FATAL", NULL, 0, "plugin-folder",
                "TesmioLoader did not provide its base directory. Action: "
                "verify the loader installation");
            LogSummary("Initialization", false);
            return 1;
        }

        g_vfsDir = g_vfsRoot ? g_vfsRoot : "";
        if (g_vfsDir.empty() || !IsAbsoluteWindowsPath(g_vfsDir))
        {
            Report("FATAL", NULL, 0, "vfs-root",
                "TesmioLoader did not provide an absolute VFS root. Action: "
                "verify the loader version and VFS configuration; stale "
                "generated language files may require manual removal");
            LogSummary("Initialization", false);
            return 1;
        }

        if (!ResolveGameFolders())
        {
            Report("FATAL", NULL, 0, "game-folder",
                "media_soviet was not found next to SOVIET64.exe. Action: "
                "verify the selected game directory");
            LogSummary("Initialization", false);
            return 1;
        }

        std::string vfsMediaSoviet = Join(g_vfsDir, VFS_MEDIA_REL);
        DWORD directoryError = ERROR_SUCCESS;
        if (!EnsureDir(vfsMediaSoviet, &directoryError))
        {
            ReportWindows("FATAL", vfsMediaSoviet.c_str(), 0, "vfs-dir",
                "VFS media_soviet directory could not be prepared",
                directoryError,
                "Verify write access and the TesmioLoader VFS configuration");
            LogSummary("Initialization", false);
            return 1;
        }

        // Fail closed before parsing user content. This removes legacy output
        // before IDs are reserved, keeping assignments deterministic.
        if (!RemoveGeneratedOverlays())
        {
            Report("FATAL", vfsMediaSoviet.c_str(), 0, "cleanup",
                "Previous generated language files could not be removed. "
                "Action: close programs using the VFS files and retry");
            LogSummary("Initialization", false);
            return 1;
        }

        // 1.2: plugins\localization.ini when present (classic installation, or
        // the effective INI Republic Mod Manager writes), otherwise the INI
        // beside this DLL (Workshop package under Soviet Mod Loader or the
        // Workshop Bridge). The same strict validation applies to both.
        const std::string ownDir = OwnDirectory();
        std::string mainIniPath = Join(g_baseDir, PLUGIN_INI);
        if (!FileExists(mainIniPath) && !ownDir.empty() &&
            FileExists(Join(ownDir, "localization.ini")))
            mainIniPath = Join(ownDir, "localization.ini");
        Info("Configuration file: %s", mainIniPath.c_str());
        if (!ValidateMainConfig(mainIniPath))
        {
            Report("FATAL", PLUGIN_INI, 0, "main-config",
                "Main configuration is invalid. Action: correct the preceding "
                "section, key, value, encoding, or file error; Vanilla "
                "language files remain active");
            LogSummary("Initialization", false);
            return 1;
        }

        // Read from the validated file itself; for the classic path this is the
        // same Win32 profile call the loader's configInt makes.
        const bool enabled =
            GetPrivateProfileIntA("localization", "enabled", 1, mainIniPath.c_str()) != 0;
        g_verbose =
            GetPrivateProfileIntA("localization", "verbose", 0, mainIniPath.c_str());
        if (!enabled)
        {
            Info("enabled = 0; generated overlays removed and Vanilla language "
                 "files remain active");
            LogSummaryStatus("Initialization",
                "was skipped because the plugin is disabled");
            return 1;
        }

        g_packRoot = Join(g_baseDir, "plugins\\localization");
        g_packRootBeside.clear();
        if (!ownDir.empty())
        {
            std::string beside = Join(ownDir, "localization");
            if (DirExists(beside) && _stricmp(beside.c_str(), g_packRoot.c_str()) != 0)
                g_packRootBeside = beside;
        }
        Info("Pack folders: '%s'%s%s", g_packRoot.c_str(),
            g_packRootBeside.empty() ? "" : " and ",
            g_packRootBeside.c_str());
        Info("Configuration: enabled=1 verbose=%d max_packs=%Iu max_keys=%Iu "
             "id_range=%u..%u",
            g_verbose, MAX_PACKS, MAX_KEYS,
            TSM_LOCALIZATION_ID_BASE, TSM_LOCALIZATION_ID_LIMIT);
        if (g_verbose)
            Info("Paths: game='%s' packs='%s' vfs='%s'",
                g_gameDir.c_str(), g_packRoot.c_str(), g_vfsDir.c_str());

        if (!LoadPacks())
        {
            Report("FATAL", g_packRoot.c_str(), 0, "pack-load",
                "Localization packs could not be loaded. Action: correct the "
                "preceding pack or directory errors");
            LogSummary("Initialization", false);
            return 1;
        }
        if (!ReserveSourceIds())
        {
            Report("FATAL", NULL, 0, "id-scan",
                "Localization IDs could not be reserved. Action: correct the "
                "preceding BTF errors");
            LogSummary("Initialization", false);
            return 1;
        }
        if (!BuildKeyRegistry())
        {
            Report("FATAL", g_packRoot.c_str(), 0, "registry",
                "No valid localization text is available. Action: add at "
                "least one valid key to a fallback language");
            LogSummary("Initialization", false);
            return 1;
        }

        if (!H->provide(TSM_SERVICE_LOCALIZATION,
                        TSM_LOCALIZATION_VERSION, &kLocalizationApi))
        {
            Report("FATAL", NULL, 0, "service",
                "Service name '%s' is already registered; the plugin remains "
                "inactive. Action: disable the duplicate provider",
                TSM_SERVICE_LOCALIZATION);
            LogSummary("Initialization", false);
            return 1;
        }

        InterlockedExchange(&g_operational, 1);
        size_t livePacks = 0;
        for (size_t i = 0; i < g_packs.size(); ++i)
            if (g_packs[i].valid) ++livePacks;
        Info("Ready: %Iu pack(s), %Iu keys, IDs %u..%u; original files "
             "remain unchanged; total_elapsed_ms=%llu",
            livePacks, g_keys.size(), TSM_LOCALIZATION_ID_BASE,
            TSM_LOCALIZATION_ID_LIMIT,
            (unsigned long long)(GetTickCount64() - g_logStarted));
        LogSummary("Initialization", true);
        return 0;
    }
    catch (const std::exception& e)
    {
        Report("FATAL", "localization", 0, "exception",
            "C++ exception in TsmPluginInit: %s. Action: preserve this log "
            "and report the failure; Vanilla language files remain active",
            e.what());
        CleanupGeneratedAfterFailure();
        LogSummary("Initialization", false);
        return 1;
    }
    catch (...)
    {
        Report("FATAL", "localization", 0, "exception",
            "Unknown C++ exception in TsmPluginInit. Action: preserve this "
            "log and report the failure; Vanilla language files remain active");
        CleanupGeneratedAfterFailure();
        LogSummary("Initialization", false);
        return 1;
    }
}

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    BeginLogPhase();
    try
    {
        if (!H || !g_bound || !AtomicRead(&g_operational) ||
            g_mediaDir.empty() || g_vfsDir.empty() || g_keys.empty())
        {
            Report("FATAL", NULL, 0, "startup-state",
                "Required initialized plugin state is unavailable. Action: "
                "verify the loader log; Vanilla language files remain active");
            CleanupGeneratedAfterFailure();
            LogSummary("Startup", false);
            return 1;
        }

        std::string vfsMediaSoviet = Join(g_vfsDir, VFS_MEDIA_REL);
        DWORD directoryError = ERROR_SUCCESS;
        if (!EnsureDir(vfsMediaSoviet, &directoryError))
        {
            ReportWindows("FATAL", vfsMediaSoviet.c_str(), 0, "vfs-dir",
                "VFS media_soviet directory could not be prepared",
                directoryError,
                "Verify write access and the TesmioLoader VFS configuration");
            CleanupGeneratedAfterFailure();
            LogSummary("Startup", false);
            return 1;
        }
        if (!RemoveGeneratedOverlays())
        {
            Report("FATAL", vfsMediaSoviet.c_str(), 0, "cleanup",
                "Generated language files could not be cleared before "
                "generation. Action: close programs using the VFS files and "
                "retry");
            CleanupGeneratedAfterFailure();
            LogSummary("Startup", false);
            return 1;
        }

        WIN32_FIND_DATAA fd;
        std::string pattern = Join(g_mediaDir, "soviet*.btf");
        HANDLE find = FindFirstFileA(pattern.c_str(), &fd);
        if (find == INVALID_HANDLE_VALUE)
        {
            Report("FATAL", pattern.c_str(), 0, "btf-scan",
                "No soviet*.btf files were found in media_soviet. Action: verify the game installation");
            CleanupGeneratedAfterFailure();
            LogSummary("Startup", false);
            return 1;
        }

        int count = 0;
        BOOL more = TRUE;
        while (more)
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                std::string base = fd.cFileName;
                std::string language = LanguageFromBtfName(base);
                if (!language.empty())
                {
                    std::string sourcePath = Join(g_mediaDir, base);
                    std::string generatedPath;
                    if (!BuildOverlayForLanguage(
                        sourcePath, language, generatedPath))
                    {
                        Report("FATAL", sourcePath.c_str(), 0, "overlay",
                            "Could not generate an extended BTF for %s. "
                            "Action: correct the preceding BTF or localization "
                            "errors; all generated overlays will be removed",
                            base.c_str());
                        FindClose(find);
                        CleanupGeneratedAfterFailure();
                        LogSummary("Startup", false);
                        return 1;
                    }
                    ++count;
                }
            }
            more = FindNextFileA(find, &fd);
        }
        DWORD scanError = GetLastError();
        FindClose(find);
        if (scanError != ERROR_NO_MORE_FILES)
        {
            ReportWindows("FATAL", pattern.c_str(), 0, "btf-scan",
                "Language-file enumeration ended unexpectedly", scanError,
                "Verify the game installation and retry");
            CleanupGeneratedAfterFailure();
            LogSummary("Startup", false);
            return 1;
        }
        if (!count)
        {
            Report("FATAL", pattern.c_str(), 0, "btf-scan",
                "No valid soviet<Language>.btf source file was found. Action: "
                "verify the game installation");
            CleanupGeneratedAfterFailure();
            LogSummary("Startup", false);
            return 1;
        }

        Info("%d language file(s) extended and provided through VFS; "
             "total_elapsed_ms=%llu",
            count, (unsigned long long)(GetTickCount64() - g_logStarted));
        LogSummary("Startup", true);
        return 0;
    }
    catch (const std::exception& e)
    {
        Report("FATAL", "localization", 0, "exception",
            "C++ exception in TsmPluginStart: %s. Action: preserve this log "
            "and report the failure; generated overlays will be removed",
            e.what());
        CleanupGeneratedAfterFailure();
        LogSummary("Startup", false);
        return 1;
    }
    catch (...)
    {
        Report("FATAL", "localization", 0, "exception",
            "Unknown C++ exception in TsmPluginStart. Action: preserve this "
            "log and report the failure; generated overlays will be removed");
        CleanupGeneratedAfterFailure();
        LogSummary("Startup", false);
        return 1;
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH)
    {
        if (g_detail != INVALID_HANDLE_VALUE)
        {
            CloseHandle(g_detail);
            g_detail = INVALID_HANDLE_VALUE;
        }
        if (g_bound)
            DeleteCriticalSection(&g_lock);
    }
    return TRUE;
}
