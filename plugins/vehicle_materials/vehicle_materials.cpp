// vehicle_materials.cpp - Vehicle Materials 0.4.0 for WRSR.
//
// The plugin adds explicitly configured mod resources to the material list of
// newly produced vehicles. resources.ini remains the authoritative registry;
// vehicle_materials.ini only selects resources and their coefficients.
// The game files are never modified and configuration changes need no rebuild.
//
// Configuration follows the rule shared by every plugin in my_plugins
// (tesmio_config.h): the base INI is <loader>\plugins\vehicle_materials.ini if
// it exists, otherwise the INI beside this DLL (the Workshop package under SML
// or the Workshop Bridge). <loader>\user_config\vehicle_materials.ini, written
// by Tesmio Settings, is laid over it key by key. Both files go through the
// same strict reader; the merged result is validated as one configuration.
//
// Supported game build:
//   version     WRSR 1.1.1.9
//   SHA256      296644a9f207d609031fc2ae73fed2dcb34619a1d55a35d1c7b51965ce6841b8
//   timestamp   0x6A3EB6AD
//   image size  0xA9D000
//
// Verified hook and helpers:
//   vehicle requirement builder RVA 0x3F8D10
//   requirement vector           vehicle + 0x85C8
//   merge helper                 RVA 0x2A18C0
//   ResourceGet                  RVA 0x2AA830

#include "../../src/tesmio_plugin.h"
#include "../tesmio_config.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <exception>
#include <map>
#include <set>
#include <sstream>
#include <string>

#define PLUGIN_INI      "vehicle_materials.ini"
#define PLUGIN_OVERLAY  "user_config\\vehicle_materials.ini"
#define PLUGIN_VERSION  "0.4.0"
#define PLUGIN_LOG_NAME "tesmioloader.vehicle_materials.log"

static const size_t MAX_INI_BYTES = 1u * 1024u * 1024u;
static const size_t MAX_CONFIG_VALUE = 63u;
static const int MAX_MATERIALS = 32;
static const int MAX_NAME = 64;
static const int MAX_DEBUG_LIMIT = 10000;
static const float MAX_COEFFICIENT = 1000000.0f;

static const DWORD RVA_VEHICLE_MATERIAL_BUILDER = 0x003F8D10;
static const DWORD RVA_MERGE_REQUIREMENTS        = 0x002A18C0;
static const DWORD RVA_RESOURCE_GET              = 0x002AA830;
static const DWORD RVA_RESOURCE_MANAGER          = 0x009D4F10;

static const DWORD EXPECTED_TIMESTAMP = 0x6A3EB6AD;
static const size_t EXPECTED_IMAGE_SIZE = 0xA9D000;

static const unsigned char EXPECTED_PROLOGUE[15] =
{
    0x48,0x8B,0xC4,
    0x55,
    0x41,0x56,
    0x41,0x57,
    0x48,0x8B,0xEC,
    0x48,0x83,0xEC,0x70
};

struct MaterialEntry
{
    void* resource;
    float amount0;
    float amount1;
};

struct EntryVector
{
    MaterialEntry* begin;
    MaterialEntry* end;
    MaterialEntry* capacity;
};

typedef void (__fastcall *VehicleMaterialBuilderFn)(void* rcx, void* vehicle);
typedef void (__fastcall *MergeRequirementsFn)(void* dstVector,
                                               unsigned char useAmount1,
                                               EntryVector* srcVector,
                                               unsigned char srcUseAmount1);
typedef void* (__fastcall *ResourceGetFn)(void* resourceManager,
                                         const char* name);

enum Category
{
    CAT_ROAD = 0,
    CAT_RAIL = 1,
    CAT_SHIP = 2,
    CAT_AIRPLANE = 3,
    CAT_COUNT = 4
};

struct ConfigMaterial
{
    char name[MAX_NAME];
    float coeff[CAT_COUNT];
};

struct PluginConfig
{
    int enabled;
    int debug;
    int debugLimit;
    ConfigMaterial materials[MAX_MATERIALS];
    int materialCount;
    int typeOverride[16];
};

struct IniValue
{
    std::string text;
    size_t line;
    const char* origin;     // PLUGIN_INI or PLUGIN_OVERLAY, for messages
};

typedef std::map<std::string, IniValue> IniSection;
typedef std::map<std::string, IniSection> IniData;

static VehicleMaterialBuilderFn g_original;
static MergeRequirementsFn g_merge;
static ResourceGetFn g_resourceGet;
static const TsmResourceApi* g_resources;

static ConfigMaterial g_materials[MAX_MATERIALS];
static int g_materialCount;
static int g_typeOverride[16];
static int g_enabled = 1;
static int g_debug;
static int g_debugLimit = 80;

static HANDLE g_detail = INVALID_HANDLE_VALUE;
static bool g_bound;
static volatile LONG g_initialized;
static volatile LONG g_runtimeEnabled;
static volatile LONG g_faultLogged;
static volatile LONG g_debugCount;
static volatile LONG g_resourceManagerErrorLogged;
static volatile LONG g_vectorErrorLogged;
static volatile LONG g_logWarnings;
static volatile LONG g_logErrors;
static volatile LONG g_logFatals;
static ULONGLONG g_phaseStarted;
static LONG g_phaseWarnings;
static LONG g_phaseErrors;
static LONG g_phaseFatals;
static DWORD g_lastWindowsError;
static std::string g_iniPath;

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

static void ReportV(const char* level, const char* where, const char* rule,
    const char* fmt, va_list ap)
{
    char body[3072];
    _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, ap);
    char full[4096];
    _snprintf_s(full, sizeof(full), _TRUNCATE, "%s: %s [%s] %s",
        level, where && where[0] ? where : "-",
        rule && rule[0] ? rule : "general", body);
    CountLevel(level);
    if (H) Logf("vehicle_materials  %s", full);
    DetailWriteLine(full);
}

static void Report(const char* level, const char* where, const char* rule,
    const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ReportV(level, where, rule, fmt, ap);
    va_end(ap);
}

static void Info(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ReportV("INFO", NULL, "status", fmt, ap);
    va_end(ap);
}

static void Debug(const char* fmt, ...)
{
    if (!g_debug) return;
    va_list ap;
    va_start(ap, fmt);
    ReportV("DEBUG", NULL, "diagnostic", fmt, ap);
    va_end(ap);
}

static void ReportWindows(const char* level, const char* where,
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
    Report(level, where, rule, "%s (Windows error %lu: %s). Action: %s",
        action, error, systemText,
        remedy ? remedy : "Review the preceding context and retry");
}

static LONG AtomicRead(volatile LONG* value)
{
    return InterlockedExchangeAdd(value, 0);
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
    ULONGLONG elapsed = g_phaseStarted ? GetTickCount64() - g_phaseStarted : 0;
    Info("%s %s after %llu ms; %ld warning(s), %ld error(s), %ld fatal error(s)",
        phase, status, (unsigned long long)elapsed,
        AtomicRead(&g_logWarnings) - g_phaseWarnings,
        AtomicRead(&g_logErrors) - g_phaseErrors,
        AtomicRead(&g_logFatals) - g_phaseFatals);
}

static void LogSummary(const char* phase, bool success)
{
    LogSummaryStatus(phase, success ? "completed successfully" : "failed");
}

// ---------------------------------------------------------------- config

static std::string TrimString(const std::string& input)
{
    size_t begin = 0;
    while (begin < input.size() &&
           (input[begin] == ' ' || input[begin] == '\t' ||
            input[begin] == '\r' || input[begin] == '\n'))
        ++begin;
    size_t end = input.size();
    while (end > begin &&
           (input[end - 1] == ' ' || input[end - 1] == '\t' ||
            input[end - 1] == '\r' || input[end - 1] == '\n'))
        --end;
    return input.substr(begin, end - begin);
}

static std::string LowerAscii(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        if (value[i] >= 'A' && value[i] <= 'Z')
            value[i] = (char)(value[i] - 'A' + 'a');
    return value;
}

static bool IsOnlyDashes(const std::string& value)
{
    if (value.empty()) return false;
    for (size_t i = 0; i < value.size(); ++i)
        if (value[i] != '-') return false;
    return true;
}

static bool IsAbsoluteWindowsPath(const std::string& path)
{
    if (path.size() >= 4 && path[0] == '\\' && path[1] == '\\' &&
        (path[2] == '?' || path[2] == '.') && path[3] == '\\')
        return true;
    if (path.size() >= 3 &&
        ((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
        return true;
    return path.size() >= 2 &&
           (path[0] == '\\' || path[0] == '/') &&
           (path[1] == '\\' || path[1] == '/');
}

static std::string JoinPath(const std::string& left,
                            const std::string& right)
{
    if (left.empty()) return right;
    if (right.empty()) return left;
    char last = left[left.size() - 1];
    return (last == '\\' || last == '/') ? left + right
                                          : left + "\\" + right;
}

static bool ParseIntStrict(const std::string& text, int* output)
{
    if (!output || text.empty()) return false;
    errno = 0;
    char* end = NULL;
    long parsed = strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || !end || *end ||
        parsed < INT_MIN || parsed > INT_MAX)
        return false;
    *output = (int)parsed;
    return true;
}

static bool ParseFloatStrict(const std::string& text, float* output)
{
    if (!output || text.empty()) return false;
    errno = 0;
    char* end = NULL;
    double parsed = strtod(text.c_str(), &end);
    if (errno == ERANGE || end == text.c_str() || !end || *end ||
        !_finite(parsed) || parsed < -FLT_MAX || parsed > FLT_MAX)
        return false;
    *output = (float)parsed;
    return _finite(*output) != 0;
}

static bool IsKnownSection(const std::string& section)
{
    return section == "general" || section == "resources" ||
           section == "road" || section == "rail" ||
           section == "ship" || section == "airplane" ||
           section == "mapping";
}

static bool IsSafeResourceName(const std::string& name)
{
    if (name.empty() || name.size() >= MAX_NAME) return false;
    for (size_t i = 0; i < name.size(); ++i)
    {
        unsigned char c = (unsigned char)name[i];
        if (c < 33 || c > 126 || c == '=' || c == ';' || c == '#' ||
            c == '[' || c == ']' || c == ':' || c == '/' || c == '\\' ||
            c == '"' || c == '\'')
            return false;
    }
    return true;
}

static bool ReadTextFile(const std::string& path, std::string& output)
{
    output.clear();
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        g_lastWindowsError = GetLastError();
        return false;
    }

    LARGE_INTEGER size;
    BOOL sizeOk = GetFileSizeEx(file, &size);
    if (!sizeOk || size.QuadPart < 0 ||
        (unsigned long long)size.QuadPart > MAX_INI_BYTES)
    {
        g_lastWindowsError = sizeOk ? ERROR_FILE_TOO_LARGE : GetLastError();
        CloseHandle(file);
        return false;
    }

    output.resize((size_t)size.QuadPart);
    size_t done = 0;
    while (done < output.size())
    {
        size_t remaining = output.size() - done;
        DWORD wanted = (DWORD)(remaining > (1u << 20) ? (1u << 20)
                                                        : remaining);
        DWORD received = 0;
        if (!ReadFile(file, &output[done], wanted, &received, NULL))
        {
            g_lastWindowsError = GetLastError();
            CloseHandle(file);
            output.clear();
            return false;
        }
        if (!received)
        {
            g_lastWindowsError = ERROR_HANDLE_EOF;
            CloseHandle(file);
            output.clear();
            return false;
        }
        done += received;
    }
    CloseHandle(file);
    return true;
}

static bool ValidateUtf8WithoutBom(const std::string& path,
                                   const std::string& text)
{
    if (text.size() >= 3 &&
        (unsigned char)text[0] == 0xEF &&
        (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
    {
        Report("ERROR", path.c_str(), "utf8-bom",
            "The plugin INI must be UTF-8 without a BOM");
        return false;
    }
    if (text.empty()) return true;
    if (text.size() > (size_t)INT_MAX ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), (int)text.size(), NULL, 0) <= 0)
    {
        Report("ERROR", path.c_str(), "utf8",
            "The configuration is not valid UTF-8 without a BOM");
        return false;
    }
    return true;
}

// One file into (section, key) -> value. The base INI must carry every known
// section; the overlay may carry any subset, and its values are laid over the
// base by MergeOverlay. Sections and keys may repeat across the two files,
// never within one.
static bool ParseIniLayout(const std::string& text, IniData* output,
                           bool requireSections, const char* origin)
{
    if (!output) return false;
    output->clear();
    std::istringstream input(text);
    std::string rawLine;
    std::string currentSection;
    std::set<std::string> sections;
    size_t line = 0;

    while (std::getline(input, rawLine))
    {
        ++line;
        std::string content = TrimString(rawLine);
        if (content.empty() || content[0] == ';' || content[0] == '#' ||
            IsOnlyDashes(content))
            continue;

        if (content[0] == '[')
        {
            if (content.size() < 3 || content[content.size() - 1] != ']' ||
                content.find(']') != content.size() - 1)
            {
                Report("ERROR", origin, "malformed-section",
                    "Malformed section header at line %Iu: %s",
                    line, content.c_str());
                return false;
            }
            std::string section = LowerAscii(TrimString(
                content.substr(1, content.size() - 2)));
            if (!IsKnownSection(section))
            {
                Report("ERROR", origin, "unknown-section",
                    "Unknown section '%s' at line %Iu",
                    section.empty() ? "<empty>" : section.c_str(), line);
                return false;
            }
            if (!sections.insert(section).second)
            {
                Report("ERROR", origin, "duplicate-section",
                    "Section [%s] appears more than once (line %Iu)",
                    section.c_str(), line);
                return false;
            }
            (*output)[section] = IniSection();
            currentSection = section;
            continue;
        }

        size_t equals = content.find('=');
        if (equals == std::string::npos)
        {
            Report("ERROR", origin, "missing-assignment",
                "Expected key = value at line %Iu: %s", line,
                content.c_str());
            return false;
        }
        if (currentSection.empty())
        {
            Report("ERROR", origin, "assignment-outside-section",
                "Configuration assignment outside a section at line %Iu",
                line);
            return false;
        }

        std::string key = LowerAscii(TrimString(content.substr(0, equals)));
        std::string value = TrimString(content.substr(equals + 1));
        if (key.empty() || value.empty())
        {
            Report("ERROR", origin, "missing-value",
                "[%s] contains an empty key or value at line %Iu",
                currentSection.c_str(), line);
            return false;
        }
        if (key.size() > MAX_CONFIG_VALUE || value.size() > MAX_CONFIG_VALUE)
        {
            Report("ERROR", origin, "value-too-long",
                "[%s] key or value exceeds %Iu characters at line %Iu",
                currentSection.c_str(), MAX_CONFIG_VALUE, line);
            return false;
        }

        IniSection& target = (*output)[currentSection];
        if (target.find(key) != target.end())
        {
            Report("ERROR", origin, "duplicate-key",
                "[%s] %s appears more than once (line %Iu)",
                currentSection.c_str(), key.c_str(), line);
            return false;
        }
        IniValue iniValue = { value, line, origin };
        target[key] = iniValue;
    }

    if (!requireSections) return true;
    static const char* const required[] = {
        "general", "resources", "road", "rail", "ship", "airplane",
        "mapping"
    };
    for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i)
    {
        if (sections.find(required[i]) == sections.end())
        {
            Report("ERROR", origin, "missing-section",
                "Required section [%s] is missing", required[i]);
            return false;
        }
    }
    return true;
}

// Every overlay value replaces or adds the same (section, key) in the base.
// Returns how many values came from the overlay.
static int MergeOverlay(IniData* base, const IniData& overlay)
{
    int applied = 0;
    for (IniData::const_iterator s = overlay.begin(); s != overlay.end(); ++s)
    {
        IniSection& target = (*base)[s->first];
        for (IniSection::const_iterator k = s->second.begin();
             k != s->second.end(); ++k)
        {
            target[k->first] = k->second;
            ++applied;
        }
    }
    return applied;
}

static bool FromOverlay(const IniValue& value)
{
    return value.origin && strcmp(value.origin, PLUGIN_OVERLAY) == 0;
}

static const IniValue* FindIniValue(const IniData& data,
                                    const char* section,
                                    const std::string& key)
{
    IniData::const_iterator sectionIt = data.find(section);
    if (sectionIt == data.end()) return NULL;
    IniSection::const_iterator valueIt = sectionIt->second.find(key);
    return valueIt == sectionIt->second.end() ? NULL : &valueIt->second;
}

static bool ParseRequiredInt(const IniData& data, const char* section,
    const char* key, int minimum, int maximum, int* output)
{
    const IniValue* value = FindIniValue(data, section, key);
    int parsed = 0;
    if (value && ParseIntStrict(value->text, &parsed) &&
        parsed >= minimum && parsed <= maximum)
    {
        *output = parsed;
        return true;
    }
    if (!value)
    {
        Report("ERROR", PLUGIN_INI, "missing-key",
            "Required key [%s] %s is missing", section, key);
    }
    else
    {
        Report("ERROR", value->origin, "config-range",
            "[%s] %s must be an integer from %d to %d at line %Iu",
            section, key, minimum, maximum, value->line);
    }
    return false;
}

static bool ValidateFixedKeys(const IniSection& section,
                              const char* sectionName,
                              const std::set<std::string>& allowed)
{
    for (IniSection::const_iterator it = section.begin(); it != section.end();
         ++it)
    {
        if (allowed.find(it->first) == allowed.end())
        {
            Report("ERROR", it->second.origin, "unknown-key",
                "Unknown key '%s' in section [%s] at line %Iu",
                it->first.c_str(), sectionName, it->second.line);
            return false;
        }
    }
    return true;
}

static bool ParseCoefficient(const IniData& data, const char* section,
    const std::string& resourceKey, float* output)
{
    const IniValue* value = FindIniValue(data, section, resourceKey);
    if (!value)
    {
        *output = 0.0f;
        return true;
    }
    float parsed = 0.0f;
    if (!ParseFloatStrict(value->text, &parsed) || parsed < 0.0f ||
        parsed > MAX_COEFFICIENT)
    {
        Report("ERROR", value->origin, "coefficient-range",
            "[%s] %s must be a finite number from 0 to %.0f at line %Iu",
            section, resourceKey.c_str(), MAX_COEFFICIENT, value->line);
        return false;
    }
    *output = parsed;
    return true;
}

// Validates the merged (section, key) data as one configuration.
// `overlayListsResources` is true when the overlay carried [resources] count:
// the material list then belongs to the overlay, and a category coefficient
// the base still holds for a material no longer listed is dropped with a
// warning instead of rejecting everything.
static bool ParseConfigData(IniData& data, bool overlayListsResources,
                            PluginConfig* output)
{
    if (!output) return false;

    std::set<std::string> generalKeys;
    generalKeys.insert("enabled");
    generalKeys.insert("debug");
    generalKeys.insert("debug_limit");
    if (!ValidateFixedKeys(data["general"], "general", generalKeys))
        return false;

    PluginConfig parsed = {};
    for (int i = 0; i < 16; ++i) parsed.typeOverride[i] = -1;

    if (!ParseRequiredInt(data, "general", "enabled", 0, 1,
                          &parsed.enabled) ||
        !ParseRequiredInt(data, "general", "debug", 0, 1,
                          &parsed.debug) ||
        !ParseRequiredInt(data, "general", "debug_limit", 0,
                          MAX_DEBUG_LIMIT, &parsed.debugLimit))
        return false;

    int requested = 0;
    if (!ParseRequiredInt(data, "resources", "count", 0, MAX_MATERIALS,
                          &requested))
        return false;

    std::set<std::string> resourceKeys;
    resourceKeys.insert("count");
    for (int i = 0; i < MAX_MATERIALS; ++i)
    {
        char key[32];
        _snprintf_s(key, sizeof(key), _TRUNCATE, "resource%d", i);
        resourceKeys.insert(key);
    }
    if (!ValidateFixedKeys(data["resources"], "resources", resourceKeys))
        return false;

    std::string resourceNames[MAX_MATERIALS];
    std::string resourceCanonical[MAX_MATERIALS];
    std::set<std::string> uniqueResources;
    for (int i = 0; i < requested; ++i)
    {
        char key[32];
        _snprintf_s(key, sizeof(key), _TRUNCATE, "resource%d", i);
        const IniValue* value = FindIniValue(data, "resources", key);
        if (!value)
        {
            Report("ERROR", PLUGIN_INI, "missing-resource",
                "[resources] %s is required because count=%d", key,
                requested);
            return false;
        }
        if (!IsSafeResourceName(value->text))
        {
            Report("ERROR", value->origin, "resource-name",
                "[resources] %s has an invalid resource name at line %Iu",
                key, value->line);
            return false;
        }
        resourceNames[i] = value->text;
        resourceCanonical[i] = LowerAscii(value->text);
        if (!uniqueResources.insert(resourceCanonical[i]).second)
        {
            Report("ERROR", value->origin, "duplicate-resource",
                "Resource '%s' is configured more than once (line %Iu)",
                value->text.c_str(), value->line);
            return false;
        }
    }

    for (int i = requested; i < MAX_MATERIALS; ++i)
    {
        char key[32];
        _snprintf_s(key, sizeof(key), _TRUNCATE, "resource%d", i);
        const IniValue* extra = FindIniValue(data, "resources", key);
        if (!extra) continue;
        if (overlayListsResources && !FromOverlay(*extra))
        {
            // The overlay shortened the list; the base entry is superseded.
            Report("WARN", PLUGIN_INI, "resource-superseded",
                "[resources] %s at line %Iu is beyond the overlay's count=%d and is ignored",
                key, extra->line, requested);
            data["resources"].erase(key);
            continue;
        }
        Report("ERROR", extra->origin, "resource-count",
            "[resources] %s is present at line %Iu but count=%d",
            key, extra->line, requested);
        return false;
    }

    static const char* const categoryNames[CAT_COUNT] = {
        "road", "rail", "ship", "airplane"
    };
    for (int c = 0; c < CAT_COUNT; ++c)
    {
        IniSection& section = data[categoryNames[c]];
        for (IniSection::iterator it = section.begin(); it != section.end();)
        {
            if (uniqueResources.find(it->first) != uniqueResources.end())
            {
                ++it;
                continue;
            }
            if (overlayListsResources && !FromOverlay(it->second))
            {
                Report("WARN", PLUGIN_INI, "resource-superseded",
                    "[%s] key '%s' at line %Iu names a material the overlay no longer lists and is ignored",
                    categoryNames[c], it->first.c_str(), it->second.line);
                section.erase(it++);
                continue;
            }
            Report("ERROR", it->second.origin, "unknown-resource",
                "[%s] key '%s' is not listed in [resources] (line %Iu)",
                categoryNames[c], it->first.c_str(), it->second.line);
            return false;
        }
    }

    for (int i = 0; i < requested; ++i)
    {
        ConfigMaterial material = {};
        strncpy_s(material.name, sizeof(material.name),
                  resourceNames[i].c_str(), _TRUNCATE);
        bool active = false;
        for (int c = 0; c < CAT_COUNT; ++c)
        {
            if (!ParseCoefficient(data, categoryNames[c],
                                  resourceCanonical[i], &material.coeff[c]))
                return false;
            if (material.coeff[c] > 0.0f) active = true;
        }
        if (!active)
        {
            Report("WARN", PLUGIN_INI, "zero-coefficient",
                "Resource '%s' has coefficient 0 in every category and is ignored",
                material.name);
            continue;
        }
        parsed.materials[parsed.materialCount++] = material;
    }

    std::set<std::string> mappingKeys;
    for (int i = 0; i < 16; ++i)
    {
        char key[32];
        _snprintf_s(key, sizeof(key), _TRUNCATE, "type%d", i);
        mappingKeys.insert(key);
    }
    if (!ValidateFixedKeys(data["mapping"], "mapping", mappingKeys))
        return false;

    for (int i = 0; i < 16; ++i)
    {
        char key[32];
        _snprintf_s(key, sizeof(key), _TRUNCATE, "type%d", i);
        const IniValue* value = FindIniValue(data, "mapping", key);
        if (!value) continue;
        int mapping = 0;
        if (!ParseIntStrict(value->text, &mapping) || mapping < -1 ||
            mapping >= CAT_COUNT)
        {
            Report("ERROR", value->origin, "mapping-range",
                "[mapping] %s must be an integer from -1 to 3 at line %Iu",
                key, value->line);
            return false;
        }
        parsed.typeOverride[i] = mapping;
    }

    *output = parsed;
    return true;
}

static bool ReadIniFile(const std::string& path, bool requireSections,
                        const char* origin, IniData* output)
{
    std::string text;
    if (!ReadTextFile(path, text))
    {
        ReportWindows("ERROR", path.c_str(), "config-read",
            "Configuration could not be read", g_lastWindowsError,
            "Verify that the file exists, is readable, and is not oversized");
        return false;
    }
    return ValidateUtf8WithoutBom(path, text) &&
           ParseIniLayout(text, output, requireSections, origin);
}

// Base INI, then the personal overlay on top, then one validation pass.
static bool LoadConfig(const std::string& basePath,
                       const std::string& overlayPath, PluginConfig* output)
{
    IniData data;
    if (!ReadIniFile(basePath, true, PLUGIN_INI, &data)) return false;

    bool overlayListsResources = false;
    if (!overlayPath.empty())
    {
        IniData overlay;
        if (!ReadIniFile(overlayPath, false, PLUGIN_OVERLAY, &overlay))
            return false;
        overlayListsResources =
            FindIniValue(overlay, "resources", "count") != NULL;
        int applied = MergeOverlay(&data, overlay);
        Info("Personal overlay applied: %d value(s) from %s", applied,
             overlayPath.c_str());
    }
    return ParseConfigData(data, overlayListsResources, output);
}

static void ApplyConfig(const PluginConfig& config)
{
    g_enabled = config.enabled;
    g_debug = config.debug;
    g_debugLimit = config.debugLimit;
    g_materialCount = config.materialCount;
    memcpy(g_materials, config.materials, sizeof(g_materials));
    memcpy(g_typeOverride, config.typeOverride, sizeof(g_typeOverride));
}

static void LogConfig()
{
    Info("Configuration: enabled=%d debug=%d debug_limit=%d active_materials=%d",
        g_enabled, g_debug, g_debugLimit, g_materialCount);
    for (int i = 0; i < g_materialCount; ++i)
    {
        const ConfigMaterial& m = g_materials[i];
        Debug("material=%s road=%.4f rail=%.4f ship=%.4f airplane=%.4f",
            m.name, m.coeff[CAT_ROAD], m.coeff[CAT_RAIL],
            m.coeff[CAT_SHIP], m.coeff[CAT_AIRPLANE]);
    }
}

// ---------------------------------------------------------------- game validation and helpers

static bool HostIsUsable(const TsmHost* host)
{
    if (!host) return false;
    const size_t requiredSize = offsetof(TsmHost, consume) +
                                sizeof(host->consume);
    return host->structSize >= requiredSize && host->exeModule &&
           host->exeBase && host->exeSize && host->baseDir &&
           host->baseDir[0] && host->log && host->installInlineHook &&
           host->readablePtr && host->faultFilter && host->consume;
}

static bool ReadPeIdentity(DWORD* timestamp, DWORD* imageSize)
{
    if (!timestamp || !imageSize || !g_exeBase ||
        g_exeSize < sizeof(IMAGE_DOS_HEADER) ||
        !ReadablePtr(g_exeBase, sizeof(IMAGE_DOS_HEADER)))
        return false;

    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)g_exeBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew < (LONG)sizeof(IMAGE_DOS_HEADER))
        return false;

    size_t ntOffset = (size_t)dos->e_lfanew;
    if (ntOffset > g_exeSize ||
        g_exeSize - ntOffset < sizeof(IMAGE_NT_HEADERS64))
        return false;

    const IMAGE_NT_HEADERS64* nt =
        (const IMAGE_NT_HEADERS64*)(g_exeBase + ntOffset);
    if (!ReadablePtr(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;

    *timestamp = nt->FileHeader.TimeDateStamp;
    *imageSize = nt->OptionalHeader.SizeOfImage;
    return true;
}

static bool RvaRangeIsValid(DWORD rva, size_t length)
{
    return (size_t)rva <= g_exeSize && length <= g_exeSize - (size_t)rva;
}

static bool BuildMatches()
{
    DWORD timestamp = 0;
    DWORD imageSize = 0;
    if (!ReadPeIdentity(&timestamp, &imageSize))
    {
        Report("FATAL", "SOVIET64.exe", "pe-identity",
            "The executable has no readable 64-bit PE identity. Action: verify the game executable and loader");
        return false;
    }
    if (g_exeSize != EXPECTED_IMAGE_SIZE || imageSize != EXPECTED_IMAGE_SIZE ||
        timestamp != EXPECTED_TIMESTAMP)
    {
        Report("FATAL", "SOVIET64.exe", "unsupported-build",
            "Build mismatch: mapped=0x%llX PE-size=0x%08X timestamp=0x%08X; expected size=0x%llX timestamp=0x%08X. Action: use WRSR 1.1.1.9",
            (unsigned long long)g_exeSize, imageSize, timestamp,
            (unsigned long long)EXPECTED_IMAGE_SIZE, EXPECTED_TIMESTAMP);
        return false;
    }
    if (!RvaRangeIsValid(RVA_VEHICLE_MATERIAL_BUILDER,
                         sizeof(EXPECTED_PROLOGUE)) ||
        !RvaRangeIsValid(RVA_MERGE_REQUIREMENTS, 16) ||
        !RvaRangeIsValid(RVA_RESOURCE_GET, 16) ||
        !RvaRangeIsValid(RVA_RESOURCE_MANAGER, sizeof(void*)))
    {
        Report("FATAL", "SOVIET64.exe", "rva-range",
            "A required game address lies outside the verified image. Action: verify plugin compatibility");
        return false;
    }

    BYTE* site = g_exeBase + RVA_VEHICLE_MATERIAL_BUILDER;
    if (!ReadablePtr(site, sizeof(EXPECTED_PROLOGUE)) ||
        memcmp(site, EXPECTED_PROLOGUE, sizeof(EXPECTED_PROLOGUE)) != 0)
    {
        Report("FATAL", "SOVIET64.exe", "builder-prologue",
            "Material-builder prologue changed. Action: verify WRSR 1.1.1.9 and check for a conflicting plugin");
        return false;
    }
    return true;
}

static bool ResourceServiceContains(const char* name)
{
    if (!g_resources || !g_resources->count || !g_resources->name || !name)
        return false;
    int count = g_resources->count();
    if (count < 0 || count > 4096) return false;
    for (int i = 0; i < count; ++i)
    {
        const char* candidate = g_resources->name(i);
        if (candidate && _stricmp(candidate, name) == 0) return true;
    }
    return false;
}

static void* GetResource(const char* name)
{
    if (!g_resourceGet || !name || !name[0]) return NULL;
    void* manager = g_exeBase + RVA_RESOURCE_MANAGER;
    if (!ReadablePtr(manager, sizeof(void*)))
    {
        if (InterlockedCompareExchange(&g_resourceManagerErrorLogged, 1, 0) == 0)
            Report("ERROR", "SOVIET64.exe", "resource-manager",
                "Resource manager address became unreadable; custom materials are skipped for this session");
        InterlockedExchange(&g_runtimeEnabled, 0);
        return NULL;
    }
    return g_resourceGet(manager, name);
}

enum VectorResourceState
{
    VECTOR_INVALID = -1,
    VECTOR_ABSENT = 0,
    VECTOR_PRESENT = 1
};

static VectorResourceState FindResourceInVector(void* vehicle, void* resource)
{
    if (!vehicle || !resource) return VECTOR_INVALID;
    EntryVector* vector = (EntryVector*)((BYTE*)vehicle + 0x85C8);
    if (!ReadablePtr(vector, sizeof(*vector))) return VECTOR_INVALID;

    uintptr_t begin = (uintptr_t)vector->begin;
    uintptr_t end = (uintptr_t)vector->end;
    uintptr_t capacity = (uintptr_t)vector->capacity;
    if (!begin && !end && !capacity) return VECTOR_ABSENT;
    if (!begin || !end || !capacity || end < begin || capacity < end)
        return VECTOR_INVALID;
    size_t span = (size_t)(end - begin);
    size_t capacitySpan = (size_t)(capacity - begin);
    if (span % sizeof(MaterialEntry) != 0 ||
        capacitySpan % sizeof(MaterialEntry) != 0)
        return VECTOR_INVALID;
    size_t count = span / sizeof(MaterialEntry);
    size_t capacityCount = capacitySpan / sizeof(MaterialEntry);
    if (count > 256 || capacityCount > 4096 ||
        (count && !ReadablePtr((void*)begin, span)))
        return VECTOR_INVALID;

    const MaterialEntry* entries = (const MaterialEntry*)begin;
    for (size_t i = 0; i < count; ++i)
        if (entries[i].resource == resource) return VECTOR_PRESENT;
    return VECTOR_ABSENT;
}

static Category AutomaticCategory(int rawType)
{
    // Verified for WRSR 1.1.1.9: 1=road, 6=ship, 7=airplane; remaining
    // production vehicle types use the rail/rolling-stock coefficients.
    if (rawType == 1) return CAT_ROAD;
    if (rawType == 6) return CAT_SHIP;
    if (rawType == 7) return CAT_AIRPLANE;
    return CAT_RAIL;
}

static Category CategoryForType(int rawType)
{
    if (rawType >= 0 && rawType < 16)
    {
        int mapping = g_typeOverride[rawType];
        if (mapping >= CAT_ROAD && mapping <= CAT_AIRPLANE)
            return (Category)mapping;
    }
    return AutomaticCategory(rawType);
}

static const char* CategoryName(Category category)
{
    switch (category)
    {
        case CAT_ROAD: return "road";
        case CAT_RAIL: return "rail";
        case CAT_SHIP: return "ship";
        case CAT_AIRPLANE: return "airplane";
        default: return "unknown";
    }
}

static bool ReserveLimitedLog()
{
    if (g_debugLimit <= 0) return false;
    return InterlockedIncrement(&g_debugCount) <= g_debugLimit;
}

// ---------------------------------------------------------------- hook

static void __fastcall HookVehicleMaterials(void* rcx, void* vehicle)
{
    if (!g_original) return;

    // Native requirements are always built first and remain untouched.
    g_original(rcx, vehicle);
    if (!InterlockedExchangeAdd(&g_runtimeEnabled, 0)) return;

    LONG faulted = 0;
    __try
    {
        if (!vehicle || !ReadablePtr(vehicle, 0x8684)) return;

        int rawType = *(int*)((BYTE*)vehicle + 0x294);
        float scale = *(float*)((BYTE*)vehicle + 0x867C);
        if (!_finite(scale) || !(scale > 0.0f) || scale > 1000000.0f)
            return;

        Category category = CategoryForType(rawType);
        MaterialEntry entries[MAX_MATERIALS] = {};
        int count = 0;

        for (int i = 0; i < g_materialCount; ++i)
        {
            const ConfigMaterial& material = g_materials[i];
            float coefficient = material.coeff[(int)category];
            if (!(coefficient > 0.0f)) continue;

            void* resource = GetResource(material.name);
            if (!resource)
            {
                if (!InterlockedExchangeAdd(&g_runtimeEnabled, 0)) return;
                if (ReserveLimitedLog())
                    Report("WARN", NULL, "resource-unavailable",
                        "Resource '%s' is unavailable at vehicle-build time. Action: verify resources.ini and plugin load order",
                        material.name);
                continue;
            }
            VectorResourceState state = FindResourceInVector(vehicle, resource);
            if (state == VECTOR_INVALID)
            {
                if (InterlockedCompareExchange(&g_vectorErrorLogged, 1, 0) == 0)
                    Report("ERROR", NULL, "requirement-vector",
                        "The vehicle requirement vector is invalid; custom materials are skipped for this vehicle");
                return;
            }
            if (state == VECTOR_PRESENT) continue;

            float amount = scale * coefficient;
            if (!_finite(amount) || !(amount > 0.000001f)) continue;

            entries[count].resource = resource;
            entries[count].amount0 = amount;
            entries[count].amount1 = 0.0f;
            if (++count >= MAX_MATERIALS) break;
        }

        if (!count) return;
        EntryVector source = { entries, entries + count, entries + count };
        g_merge((BYTE*)vehicle + 0x85C8, 0, &source, 0);

        if (g_debug && ReserveLimitedLog())
            Report("DEBUG", NULL, "vehicle-build",
                "type=%d category=%s scale=%.4f custom_materials_added=%d",
                rawType, CategoryName(category), scale, count);
    }
    __except(FaultFilter("vehicle_materials vehicle requirement hook",
                         GetExceptionInformation()))
    {
        faulted = 1;
    }

    if (faulted)
    {
        InterlockedExchange(&g_runtimeEnabled, 0);
        if (InterlockedCompareExchange(&g_faultLogged, 1, 0) == 0)
            Report("FATAL", "vehicle_materials", "runtime-fault",
                "The custom-material hook faulted and was disabled for the rest of this session; native vehicle materials remain active");
    }
}

// ---------------------------------------------------------------- plugin entry

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion(void)
{
    return TSM_API_VERSION;
}

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host,
                                                   TsmPluginInfo* info)
{
    if (!info || !HostIsUsable(host)) return 1;
    TsmBind(host);
    g_bound = true;
    info->name = "vehicle_materials";
    info->version = PLUGIN_VERSION;

    BeginLogPhase();
    g_detail = TsmOpenLog(PLUGIN_LOG_NAME);
    if (g_detail == INVALID_HANDLE_VALUE)
        ReportWindows("WARN", PLUGIN_LOG_NAME, "log-open",
            "Detail log could not be opened", GetLastError(),
            "Verify write access to the TesmioLoader directory");
    Info("TesmioLoader vehicle_materials %s starting; loader API=%u; detail log: %s",
        PLUGIN_VERSION, TSM_API_VERSION, PLUGIN_LOG_NAME);

    try
    {
        std::string baseDir = g_baseDir ? g_baseDir : "";
        if (!IsAbsoluteWindowsPath(baseDir))
        {
            Report("FATAL", NULL, "base-directory",
                "TesmioLoader did not provide an absolute base directory. Action: verify the loader installation");
            LogSummary("Initialization", false);
            return 1;
        }
        // Base: <loader>\plugins\vehicle_materials.ini, else the INI beside
        // this DLL. Overlay: <loader>\user_config\vehicle_materials.ini.
        if (!TsmConfigInit("vehicle_materials"))
        {
            Report("FATAL", PLUGIN_INI, "configuration-missing",
                "No vehicle_materials.ini in %s\\plugins and none beside this DLL. Action: keep the INI beside the DLL; no hook was installed",
                baseDir.c_str());
            LogSummary("Initialization", false);
            return 1;
        }
        g_iniPath = g_tsmConfig.base;
        Info("Configuration %s: %s", g_tsmConfig.baseIsLocal ? "in the loader folder" : "beside plugin DLL", g_iniPath.c_str());
        std::string overlayPath = g_tsmConfig.hasOverlay ? g_tsmConfig.overlay : "";
        if (!overlayPath.empty()) Info("Personal overlay: %s", overlayPath.c_str());

        PluginConfig config;
        if (!LoadConfig(g_iniPath, overlayPath, &config))
        {
            Report("FATAL", PLUGIN_INI, "configuration",
                "Configuration is invalid. Action: correct the preceding error; no hook was installed");
            LogSummary("Initialization", false);
            return 1;
        }
        ApplyConfig(config);
        LogConfig();

        if (!g_enabled)
        {
            Info("enabled = 0; plugin remains inactive");
            LogSummaryStatus("Initialization",
                             "was skipped because the plugin is disabled");
            return 1;
        }
        if (g_materialCount <= 0)
        {
            Report("FATAL", PLUGIN_INI, "no-materials",
                "No active material is configured. Action: assign a positive coefficient to at least one listed resource");
            LogSummary("Initialization", false);
            return 1;
        }

        InterlockedExchange(&g_initialized, 1);
        Info("%d active custom vehicle material(s) configured",
             g_materialCount);
        LogSummary("Initialization", true);
        return 0;
    }
    catch (const std::exception& e)
    {
        Report("FATAL", "vehicle_materials", "exception",
            "C++ exception in TsmPluginInit: %s. Action: preserve this log and report the failure; no hook was installed",
            e.what());
        LogSummary("Initialization", false);
        return 1;
    }
    catch (...)
    {
        Report("FATAL", "vehicle_materials", "exception",
            "Unknown C++ exception in TsmPluginInit. Action: preserve this log and report the failure; no hook was installed");
        LogSummary("Initialization", false);
        return 1;
    }
}

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    BeginLogPhase();
    try
    {
        if (!H || !g_bound || !InterlockedExchangeAdd(&g_initialized, 0) ||
            g_materialCount <= 0)
        {
            Report("FATAL", NULL, "startup-state",
                "Required initialized plugin state is unavailable. Action: review the initialization log; no hook was installed");
            LogSummary("Startup", false);
            return 1;
        }

        g_resources = (const TsmResourceApi*)H->consume(
            TSM_SERVICE_RESOURCES, TSM_RESOURCES_VERSION);
        if (!g_resources || !ReadablePtr(g_resources, sizeof(*g_resources)) ||
            !g_resources->count || !g_resources->name)
        {
            Report("FATAL", NULL, "resources-service",
                "Resources service is unavailable or invalid. Action: enable the Resources plugin; no hook was installed");
            LogSummary("Startup", false);
            return 1;
        }
        for (int i = 0; i < g_materialCount; ++i)
        {
            if (!ResourceServiceContains(g_materials[i].name))
            {
                Report("FATAL", "plugins\\resources.ini", "resource-not-registered",
                    "Configured resource '%s' is not published by the Resources plugin. Action: add or correct it in resources.ini; no hook was installed",
                    g_materials[i].name);
                LogSummary("Startup", false);
                return 1;
            }
        }

        if (!BuildMatches())
        {
            LogSummary("Startup", false);
            return 1;
        }

        g_merge = (MergeRequirementsFn)(g_exeBase + RVA_MERGE_REQUIREMENTS);
        g_resourceGet = (ResourceGetFn)(g_exeBase + RVA_RESOURCE_GET);
        void* resourceManager = g_exeBase + RVA_RESOURCE_MANAGER;
        if (!ReadablePtr((void*)g_merge, 16) ||
            !ReadablePtr((void*)g_resourceGet, 16) ||
            !ReadablePtr(resourceManager, sizeof(void*)))
        {
            Report("FATAL", "SOVIET64.exe", "helper-validation",
                "A required helper or the resource manager is unreadable. Action: verify WRSR 1.1.1.9; no hook was installed");
            LogSummary("Startup", false);
            return 1;
        }

        BYTE* target = g_exeBase + RVA_VEHICLE_MATERIAL_BUILDER;
        void* trampoline = NULL;
        if (!InstallInlineHook(target, (void*)&HookVehicleMaterials,
                               &trampoline, EXPECTED_PROLOGUE,
                               sizeof(EXPECTED_PROLOGUE),
                               "vehicle_materials vehicle requirement builder") ||
            !trampoline)
        {
            Report("FATAL", "SOVIET64.exe", "hook-install",
                "Vehicle-material hook was refused. Action: check for a conflicting plugin or game update; no hook was installed");
            LogSummary("Startup", false);
            return 1;
        }

        // From this point the DLL must remain loaded: the game now branches
        // into HookVehicleMaterials. Runtime faults switch the custom part to
        // pass-through mode but never return a failure that could unload it.
        g_original = (VehicleMaterialBuilderFn)trampoline;
        InterlockedExchange(&g_runtimeEnabled, 1);

        Info("Hook active at SOVIET64.exe+0x%X", RVA_VEHICLE_MATERIAL_BUILDER);
        Info("Automatic mapping: type1=road, type6=ship, type7=airplane, other=rail");
        Info("v%s ready", PLUGIN_VERSION);
        LogSummary("Startup", true);
        return 0;
    }
    catch (const std::exception& e)
    {
        if (g_original)
        {
            InterlockedExchange(&g_runtimeEnabled, 0);
            Report("FATAL", "vehicle_materials", "exception",
                "C++ exception after hook installation: %s. Custom additions are disabled; native vehicle materials remain active",
                e.what());
            LogSummary("Startup", false);
            return 0;
        }
        Report("FATAL", "vehicle_materials", "exception",
            "C++ exception in TsmPluginStart: %s. Action: preserve this log and report the failure; no hook was installed",
            e.what());
        LogSummary("Startup", false);
        return 1;
    }
    catch (...)
    {
        if (g_original)
        {
            InterlockedExchange(&g_runtimeEnabled, 0);
            Report("FATAL", "vehicle_materials", "exception",
                "Unknown exception after hook installation. Custom additions are disabled; native vehicle materials remain active");
            LogSummary("Startup", false);
            return 0;
        }
        Report("FATAL", "vehicle_materials", "exception",
            "Unknown exception in TsmPluginStart. Action: preserve this log and report the failure; no hook was installed");
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
        if (g_bound) DeleteCriticalSection(&g_lock);
    }
    return TRUE;
}
