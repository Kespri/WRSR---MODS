// workshop_bridge 0.2.0 - loads Workshop hook DLLs without Soviet Mod Loader.
//
// TesmioLoader alone loads DLLs out of tesmioloader\plugins\ and nowhere else.
// A plugin published as a Workshop package (soviet.mod.ini with [hooks] dll)
// therefore needs either Soviet Mod Loader, which loads it straight out of the
// package, or a copy of the DLL in plugins\ made by hand. This plugin is the
// third way: it is an ordinary TesmioLoader plugin that walks the subscribed
// Workshop packages, loads the hook DLLs of the packages it is told to, and
// hands each of them the very same host table it received itself. A hook loaded
// this way sees exactly what it would see under Soviet Mod Loader.
//
// It stays out of the way in every case that is already handled:
//
//   * Soviet Mod Loader is loaded in this process (or installed and switched on
//     in tesmioloader.ini): SML loads the hooks itself, the bridge is idle.
//   * plugins\<name>.dll exists for a hook: the loader handles that copy - on
//     or off, as the launcher says - and the bridge never loads the package
//     copy on top of it. One line in the log says so.
//   * The DLL is already loaded in this process under the same file name.
//
// Which packages are loaded comes from workshop_bridge.ini, read through the
// same base/overlay rule as every plugin in my_plugins (tesmio_config.h): the
// base beside the DLL, personal values in user_config\workshop_bridge.ini.
// Republic Mod Manager writes the [packages] list into the overlay; without it the
// [bridge] policy decides.
//
// Two phases, as the API demands: every hook's Init runs inside the bridge's
// own Init, so anything a hook `provide`s is on the noticeboard before any
// plugin's Start. Every hook's Start runs inside the bridge's Start. The loader
// credits a hook's services to workshop_bridge.dll in its log; the bridge logs
// the real name beside it.
//
// Nothing here touches game code. No hook, no patch, no address.

#include "../../src/tesmio_plugin.h"
#include "../tesmio_config.h"

#define PLUGIN_NAME    "workshop_bridge"
#define PLUGIN_VERSION "0.2.0"
#define WRSR_APP_ID    "784150"

#define MAX_CHILDREN          32
#define MAX_HOOKS_PER_PACKAGE 8
#define MANIFEST_LIMIT        (64 * 1024)

struct Child
{
    HMODULE          module;
    TsmPluginStartFn start;
    char             file[64];
    char             folder[64];
    const char*      name;
    const char*      version;
};
static Child g_child[MAX_CHILDREN];
static int   g_childCount;

static char  g_workshop[MAX_PATH];
static char  g_policy[16];
static bool  g_verbose;
static int   g_packagesWithHooks, g_skipped;

// ---------------------------------------------------------------- small helpers

static bool FileExists(const char* path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirExists(const char* path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool ParentDir(char* path)
{
    size_t len = strlen(path);
    while (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/')) path[--len] = 0;
    char* s1 = strrchr(path, '\\');
    char* s2 = strrchr(path, '/');
    char* s  = s1 > s2 ? s1 : s2;
    if (!s || s == path) return false;
    *s = 0;
    return true;
}

static const char* FileNameOf(const char* path)
{
    const char* s1 = strrchr(path, '\\');
    const char* s2 = strrchr(path, '/');
    const char* s  = s1 > s2 ? s1 : s2;
    return s ? s + 1 : path;
}

// A hook path from a manifest must stay inside its package: relative, no
// drive, no parent step. Anything else is refused before LoadLibrary sees it.
static bool SafeRelativePath(const char* p)
{
    if (!p || !p[0]) return false;
    if (p[0] == '\\' || p[0] == '/' || strchr(p, ':')) return false;
    for (const char* s = p; *s; s++)
        if (s[0] == '.' && s[1] == '.' && (s == p || s[-1] == '\\' || s[-1] == '/') &&
            (s[2] == 0 || s[2] == '\\' || s[2] == '/')) return false;
    return true;
}

// ---------------------------------------------------------------- the manifest
//
// Read line by line rather than through the profile API: [hooks] may carry
// the key `dll` more than once, and GetPrivateProfileString returns only the
// first. A leading UTF-8 BOM is tolerated here, unlike in a plugin INI.

struct Manifest
{
    char id[128];
    char name[128];
    bool enabled;
    char dll[MAX_HOOKS_PER_PACKAGE][MAX_PATH];
    int  dllCount;
};

static char g_manifestText[MANIFEST_LIMIT + 1];

static bool ReadManifest(const char* path, Manifest* m)
{
    memset(m, 0, sizeof(*m));
    m->enabled = true;

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD read = 0;
    BOOL ok = ReadFile(h, g_manifestText, MANIFEST_LIMIT, &read, NULL);
    CloseHandle(h);
    if (!ok) return false;
    g_manifestText[read] = 0;

    char* p = g_manifestText;
    if (read >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;

    char section[64] = "";
    while (*p)
    {
        char* line = p;
        char* end  = strpbrk(p, "\r\n");
        if (end) { *end = 0; p = end + 1; } else p = line + strlen(line);
        Trim(line);
        if (!line[0] || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[')
        {
            char* close = strchr(line, ']');
            if (!close) continue;
            *close = 0;
            strncpy_s(section, sizeof(section), line + 1, _TRUNCATE);
            Trim(section);
            continue;
        }
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* key = line; char* value = eq + 1;
        Trim(key); Trim(value);
        // "value ; comment" - the same rule tesmio_config.h applies.
        for (char* c = value; *c; c++)
            if ((*c == ';' || *c == '#') && (c == value || c[-1] == ' ' || c[-1] == '\t')) { *c = 0; Trim(value); break; }

        if (_stricmp(section, "mod") == 0)
        {
            if (_stricmp(key, "id") == 0)      strncpy_s(m->id, sizeof(m->id), value, _TRUNCATE);
            if (_stricmp(key, "name") == 0)    strncpy_s(m->name, sizeof(m->name), value, _TRUNCATE);
            if (_stricmp(key, "enabled") == 0) m->enabled = atoi(value) != 0;
        }
        else if (_stricmp(section, "hooks") == 0 && _stricmp(key, "dll") == 0 && value[0])
        {
            if (m->dllCount < MAX_HOOKS_PER_PACKAGE)
                strncpy_s(m->dll[m->dllCount++], MAX_PATH, value, _TRUNCATE);
        }
    }
    return true;
}

// ---------------------------------------------------------------- decisions

// Soviet Mod Loader in this process, or installed and switched on so that the
// loader (which walks plugins\ alphabetically, hence before this DLL) has
// loaded it or tried to. Either way the Workshop is SML's business.
static bool SmlActive(char* why, size_t n)
{
    static const char* names[] = { "soviet_mod_loader.dll", "000_soviet_mod_loader.dll" };
    for (int i = 0; i < 2; i++)
    {
        if (GetModuleHandleA(names[i])) { _snprintf_s(why, n, _TRUNCATE, "%s is loaded", names[i]); return true; }
        char local[MAX_PATH], ini[MAX_PATH], key[64];
        _snprintf_s(local, sizeof(local), _TRUNCATE, "%s\\plugins\\%s", g_baseDir, names[i]);
        if (!FileExists(local)) continue;
        strncpy_s(key, sizeof(key), names[i], _TRUNCATE);
        if (char* dot = strrchr(key, '.')) *dot = 0;
        _snprintf_s(ini, sizeof(ini), _TRUNCATE, "%s\\tesmioloader.ini", g_baseDir);
        if (GetPrivateProfileIntA("plugins", key, 1, ini) != 0)
        {
            _snprintf_s(why, n, _TRUNCATE, "plugins\\%s is installed and on in tesmioloader.ini", names[i]);
            return true;
        }
    }
    return false;
}

// `auto`: the Workshop folder of the Steam library the game sits in -
// <library>\steamapps\workshop\content\784150. Taken from the game's own
// path when there is one, else from the loader's folder (build sits in
// <game>\tesmioloader\build).
static bool ResolveWorkshopRoot(const char* setting)
{
    if (setting && setting[0] && _stricmp(setting, "auto") != 0)
    {
        strncpy_s(g_workshop, sizeof(g_workshop), setting, _TRUNCATE);
        return DirExists(g_workshop);
    }
    char base[MAX_PATH]; int ups;
    if (H->exeModule && GetModuleFileNameA((HMODULE)H->exeModule, base, sizeof(base))) ups = 3;   // exe, SovietRepublic, common
    else { strncpy_s(base, sizeof(base), g_baseDir ? g_baseDir : "", _TRUNCATE); ups = 4; }        // build, tesmioloader, SovietRepublic, common
    for (int i = 0; i < ups; i++) if (!ParentDir(base)) return false;
    _snprintf_s(g_workshop, sizeof(g_workshop), _TRUNCATE, "%s\\workshop\\content\\%s", base, WRSR_APP_ID);
    return DirExists(g_workshop);
}

// [packages] <folder> = 1/0 wins; then the policy: `list` loads nothing that
// is not listed, `all` loads every package with hooks that is not switched off.
static bool PackageWanted(const char* folder, const char** why)
{
    char value[16];
    if (TsmConfigRaw("packages", folder, value, sizeof(value), ""))
    {
        if (atoi(value) != 0) { *why = "listed"; return true; }
        *why = "off in [packages]"; return false;
    }
    if (_stricmp(g_policy, "all") == 0) { *why = "policy all"; return true; }
    *why = "not listed in [packages]"; return false;
}

// ---------------------------------------------------------------- calling a hook
//
// Plain functions with no C++ objects, so __try is legal, and the host's
// filter so a faulting hook is logged as module+rva like everything else.

static unsigned CallApiVersion(TsmPluginApiVersionFn fn)
{
    __try { return fn(); }
    __except (FaultFilter("bridge hook api version", GetExceptionInformation())) { return 0; }
}

static int CallInit(TsmPluginInitFn fn, TsmPluginInfo* info)
{
    __try { return fn(H, info); }
    __except (FaultFilter("bridge hook init", GetExceptionInformation())) { return -1; }
}

static int CallStart(TsmPluginStartFn fn)
{
    __try { return fn(); }
    __except (FaultFilter("bridge hook start", GetExceptionInformation())) { return -1; }
}

static void LoadHook(const char* folder, const char* relative)
{
    if (!SafeRelativePath(relative))
    {
        Logf("bridge   %-12s hook path refused: %s", folder, relative);
        g_skipped++;
        return;
    }
    char path[MAX_PATH];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s\\%s", g_workshop, folder, relative);
    for (char* c = path; *c; c++) if (*c == '/') *c = '\\';
    const char* file = FileNameOf(path);

    char local[MAX_PATH];
    _snprintf_s(local, sizeof(local), _TRUNCATE, "%s\\plugins\\%s", g_baseDir, file);
    if (FileExists(local))
    {
        Logf("bridge   %-12s %-20s skipped - plugins\\%s exists; the loader owns that copy (delete it to use the package)", folder, file, file);
        g_skipped++;
        return;
    }
    if (GetModuleHandleA(file))
    {
        Logf("bridge   %-12s %-20s skipped - already loaded in this process", folder, file);
        g_skipped++;
        return;
    }
    if (!FileExists(path))
    {
        Logf("bridge   %-12s %-20s missing: %s", folder, file, path);
        g_skipped++;
        return;
    }
    if (g_childCount >= MAX_CHILDREN)
    {
        Logf("bridge   %-12s %-20s skipped - only %d hooks fit", folder, file, MAX_CHILDREN);
        g_skipped++;
        return;
    }

    HMODULE mod = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!mod)
    {
        Logf("bridge   %-12s %-20s failed to load (%lu)", folder, file, GetLastError());
        g_skipped++;
        return;
    }
    TsmPluginApiVersionFn ver  = (TsmPluginApiVersionFn)GetProcAddress(mod, TSM_EXPORT_APIVERSION);
    TsmPluginInitFn       init = (TsmPluginInitFn)GetProcAddress(mod, TSM_EXPORT_INIT);
    if (!ver || !init)
    {
        Logf("bridge   %-12s %-20s is not a tesmioloader plugin - no %s/%s export", folder, file, TSM_EXPORT_APIVERSION, TSM_EXPORT_INIT);
        FreeLibrary(mod);
        g_skipped++;
        return;
    }
    unsigned got = CallApiVersion(ver);
    if (got < TSM_API_VERSION_MIN || got > H->apiVersion)
    {
        Logf("bridge   %-12s %-20s reports API %u, this host takes %u..%u - not initialised", folder, file, got, TSM_API_VERSION_MIN, H->apiVersion);
        FreeLibrary(mod);
        g_skipped++;
        return;
    }

    Child* c = &g_child[g_childCount];
    memset(c, 0, sizeof(*c));
    c->module = mod;
    strncpy_s(c->file, sizeof(c->file), file, _TRUNCATE);
    strncpy_s(c->folder, sizeof(c->folder), folder, _TRUNCATE);
    c->name = c->file; c->version = "";

    TsmPluginInfo info; memset(&info, 0, sizeof(info));
    int rc = CallInit(init, &info);
    if (info.name)    c->name    = info.name;
    if (info.version) c->version = info.version;
    if (rc != 0)
    {
        // Same rule as the loader: non-zero from Init means nothing was hooked.
        Logf("bridge   %-12s %-20s declined to install (%d)", folder, file, rc);
        FreeLibrary(mod);
        g_skipped++;
        return;
    }
    c->start = (TsmPluginStartFn)GetProcAddress(mod, TSM_EXPORT_START);
    g_childCount++;
    Logf("bridge   hook %-16s %-8s from %s", c->name, c->version, path + strlen(g_workshop) + 1);
}

static void LoadPackage(const char* folder)
{
    char manifest[MAX_PATH];
    _snprintf_s(manifest, sizeof(manifest), _TRUNCATE, "%s\\%s\\soviet.mod.ini", g_workshop, folder);
    if (!FileExists(manifest)) { if (g_verbose) Logf("bridge   %-12s no soviet.mod.ini", folder); return; }

    Manifest m;
    if (!ReadManifest(manifest, &m)) { Logf("bridge   %-12s soviet.mod.ini unreadable", folder); return; }
    if (m.dllCount == 0) { if (g_verbose) Logf("bridge   %-12s %s: content only, no hooks", folder, m.id); return; }
    g_packagesWithHooks++;
    if (!m.enabled) { Logf("bridge   %-12s %s: enabled = 0 in its manifest - skipped", folder, m.id); g_skipped += m.dllCount; return; }

    const char* why = "";
    if (!PackageWanted(folder, &why))
    {
        if (g_verbose || _stricmp(why, "off in [packages]") == 0) Logf("bridge   %-12s %s: %s - skipped", folder, m.id, why);
        g_skipped += m.dllCount;
        return;
    }
    for (int i = 0; i < m.dllCount; i++) LoadHook(folder, m.dll[i]);
}

// ---------------------------------------------------------------- exports

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion(void) { return TSM_API_VERSION; }

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host, TsmPluginInfo* info)
{
    TsmBind(host);
    info->name    = PLUGIN_NAME;
    info->version = PLUGIN_VERSION;

    TsmConfigInit(PLUGIN_NAME);
    if (!TsmConfigInt("bridge", "enabled", 1))
    {
        Logf("bridge   disabled in workshop_bridge.ini");
        return 0;
    }
    char why[160];
    if (SmlActive(why, sizeof(why)))
    {
        Logf("bridge   idle - %s; Soviet Mod Loader loads the Workshop hooks itself", why);
        return 0;
    }
    g_verbose = TsmConfigInt("bridge", "log_verbose", 0) != 0;
    TsmConfigString("bridge", "policy", g_policy, sizeof(g_policy), "list");
    if (_stricmp(g_policy, "all") != 0 && _stricmp(g_policy, "list") != 0)
    {
        Logf("bridge   policy \"%s\" unknown - using list", g_policy);
        strcpy_s(g_policy, sizeof(g_policy), "list");
    }
    char rootSetting[MAX_PATH];
    TsmConfigString("bridge", "workshop_root", rootSetting, sizeof(rootSetting), "auto");
    if (!ResolveWorkshopRoot(rootSetting))
    {
        Logf("bridge   Workshop folder not found (%s) - nothing to load", g_workshop[0] ? g_workshop : rootSetting);
        return 0;
    }
    Logf("bridge   Workshop %s, policy %s", g_workshop, g_policy);

    char pattern[MAX_PATH];
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*", g_workshop);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            LoadPackage(fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    Logf("bridge   %d hook(s) loaded, %d skipped, %d package(s) with hooks", g_childCount, g_skipped, g_packagesWithHooks);
    return 0;
}

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    for (int i = 0; i < g_childCount; i++)
    {
        Child* c = &g_child[i];
        if (!c->start) continue;
        int rc = CallStart(c->start);
        if (rc != 0) Logf("bridge   hook %s started with %d - inactive", c->name, rc);
    }
    return 0;
}
