// tesmio_config.h - one configuration rule for every plugin in my_plugins.
//
// A plugin of this fork runs in three settings, and its configuration must be
// found the same way in all of them:
//
//   TesmioLoader alone   the user copies <name>.dll and <name>.ini into
//                        <loader>\plugins\ by hand.
//   Soviet Mod Loader    the DLL is loaded straight out of the Workshop package;
//                        the INI shipped with it lies beside the DLL.
//   Tesmio Settings      personal values are written to
//                        <loader>\user_config\<name>.ini and nothing else.
//
// The rule, in order:
//
//   base     <loader>\plugins\<name>.ini if it exists, otherwise <name>.ini
//            beside this DLL. This is the plugin's original configuration and
//            is never written by anything but its author (or a user by hand).
//   overlay  <loader>\user_config\<name>.ini, if it exists. Every key found
//            there wins over the base. Only Tesmio Settings writes it; delete
//            it (or the launcher) and the plugin runs on its base values again.
//
// <loader> is the host's baseDir, which Soviet Mod Loader forwards unchanged,
// so the overlay lives in the same place under every loader.
//
// Include after tesmio_plugin.h (it uses H and Logf). Header-only, every
// function static, no allocation: the values are read with the same Win32
// profile API the loader itself uses, against absolute paths.
//
//   TsmConfigInit("walking");                       // once, in TsmPluginInit
//   int   d = TsmConfigInt("walking", "distance", 480);
//   float f = TsmConfigFloat("walking", "factor", 1.0f);
//   char  s[64]; TsmConfigString("walking", "mode", s, sizeof s, "auto");
//
// A package that uses this header announces it to Tesmio Settings with
// `[configuration] user_overlay = 1` in soviet.mod.ini, so the launcher deploys
// the base INI untouched and writes personal values to user_config only.

#ifndef TESMIO_CONFIG_H
#define TESMIO_CONFIG_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef TESMIO_PLUGIN_H
#error "include tesmio_plugin.h before tesmio_config.h"
#endif

struct TsmConfigState
{
    char name[64];
    char base[MAX_PATH];        // the INI actually used as the base
    char overlay[MAX_PATH];     // <loader>\user_config\<name>.ini
    bool hasBase, hasOverlay, baseIsLocal, initialized;
};
static TsmConfigState g_tsmConfig;

// The folder this DLL was loaded from - the Workshop package under SML, the
// loader's plugins\ folder otherwise. Taken from the address of a static that
// lives in this DLL, so it is never the host's module.
static bool TsmConfigOwnDirectory(char* out, size_t size)
{
    static const char anchor = 0;
    HMODULE module = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &anchor, &module))
        return false;
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(module, path, (DWORD)sizeof(path));
    if (!length || length >= sizeof(path)) return false;
    char* slash = strrchr(path, '\\');
    if (!slash) return false;
    *slash = 0;
    if (strlen(path) + 1 > size) return false;
    strcpy_s(out, size, path);
    return true;
}

static bool TsmConfigFileExists(const char* path)
{
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// Resolves base and overlay for <name> and logs what was chosen. Returns true
// when a base INI exists; a plugin may still run on compiled defaults without
// one, so the return value is information, not a verdict.
static bool TsmConfigInit(const char* name)
{
    memset(&g_tsmConfig, 0, sizeof(g_tsmConfig));
    strncpy_s(g_tsmConfig.name, sizeof(g_tsmConfig.name), name ? name : "", _TRUNCATE);
    g_tsmConfig.initialized = true;

    char local[MAX_PATH], adjacent[MAX_PATH] = "";
    _snprintf_s(local, sizeof(local), _TRUNCATE, "%s\\plugins\\%s.ini", g_baseDir ? g_baseDir : "", g_tsmConfig.name);
    char own[MAX_PATH];
    if (TsmConfigOwnDirectory(own, sizeof(own)))
        _snprintf_s(adjacent, sizeof(adjacent), _TRUNCATE, "%s\\%s.ini", own, g_tsmConfig.name);

    if (TsmConfigFileExists(local))            { strcpy_s(g_tsmConfig.base, sizeof(g_tsmConfig.base), local); g_tsmConfig.hasBase = true; g_tsmConfig.baseIsLocal = true; }
    else if (adjacent[0] && TsmConfigFileExists(adjacent)) { strcpy_s(g_tsmConfig.base, sizeof(g_tsmConfig.base), adjacent); g_tsmConfig.hasBase = true; }
    else                                       strcpy_s(g_tsmConfig.base, sizeof(g_tsmConfig.base), local);

    _snprintf_s(g_tsmConfig.overlay, sizeof(g_tsmConfig.overlay), _TRUNCATE, "%s\\user_config\\%s.ini", g_baseDir ? g_baseDir : "", g_tsmConfig.name);
    g_tsmConfig.hasOverlay = TsmConfigFileExists(g_tsmConfig.overlay);

    if (H && H->log)
    {
        if (g_tsmConfig.hasBase)
            Logf("%s  config base=%s (%s)%s%s", g_tsmConfig.name, g_tsmConfig.base,
                 g_tsmConfig.baseIsLocal ? "loader folder" : "beside the DLL",
                 g_tsmConfig.hasOverlay ? " overlay=" : "", g_tsmConfig.hasOverlay ? g_tsmConfig.overlay : "");
        else
            Logf("%s  config no INI found (looked for %s%s%s); compiled defaults apply%s",
                 g_tsmConfig.name, local, adjacent[0] ? " and " : "", adjacent,
                 g_tsmConfig.hasOverlay ? " under the user_config overlay" : "");
    }
    return g_tsmConfig.hasBase;
}

// Reads one raw value: overlay first, then base. Returns false when neither
// file has the key; `out` then holds `fallback`.
static bool TsmConfigRaw(const char* section, const char* key, char* out, int outSize, const char* fallback)
{
    static const char kMissing[] = "\x01";
    if (!out || outSize <= 0) return false;
    const char* files[2] = { g_tsmConfig.hasOverlay ? g_tsmConfig.overlay : NULL, g_tsmConfig.hasBase ? g_tsmConfig.base : NULL };
    for (int i = 0; i < 2; i++)
    {
        if (!files[i]) continue;
        DWORD n = GetPrivateProfileStringA(section, key, kMissing, out, (DWORD)outSize, files[i]);
        if (n == 1 && out[0] == '\x01') continue;       // key absent in this file
        // Trailing inline comment ("130 ; (stock 121)") is not part of the value.
        for (char* p = out; *p; p++)
            if ((*p == ';' || *p == '#') && (p == out || p[-1] == ' ' || p[-1] == '\t')) { *p = 0; break; }
        for (size_t len = strlen(out); len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\t' || out[len - 1] == '\r' || out[len - 1] == '\n'); len--) out[len - 1] = 0;
        return true;
    }
    strncpy_s(out, (size_t)outSize, fallback ? fallback : "", _TRUNCATE);
    return false;
}

static bool TsmConfigString(const char* section, const char* key, char* out, int outSize, const char* fallback)
{
    return TsmConfigRaw(section, key, out, outSize, fallback);
}

static int TsmConfigInt(const char* section, const char* key, int fallback)
{
    char text[64];
    if (!TsmConfigRaw(section, key, text, sizeof(text), NULL)) return fallback;
    char* end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text) return fallback;
    return (int)value;
}

static float TsmConfigFloat(const char* section, const char* key, float fallback)
{
    char text[64];
    if (!TsmConfigRaw(section, key, text, sizeof(text), NULL)) return fallback;
    char* end = NULL;
    double value = strtod(text, &end);
    if (end == text) return fallback;
    return (float)value;
}

// True when the key exists in the overlay - for a log line that says which
// values are personal.
static bool TsmConfigIsPersonal(const char* section, const char* key)
{
    if (!g_tsmConfig.hasOverlay) return false;
    char text[8];
    DWORD n = GetPrivateProfileStringA(section, key, "\x01", text, (DWORD)sizeof(text), g_tsmConfig.overlay);
    return !(n == 1 && text[0] == '\x01');
}

#endif // TESMIO_CONFIG_H
