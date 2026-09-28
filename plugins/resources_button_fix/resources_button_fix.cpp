// resources_button_fix v1.1
//
// Compacts two terrain-editor grids without touching any other tool window:
//
//   * Resources       - paint above its matching erase button, in blocks
//   * Rocks/Gravel    - either P E P E or a paint row above an erase row
//
// The common editor button routine is shared by Trees and several other tabs.
// Names and the editor's tab field are therefore not a safe discriminator: the
// Rocks and Resources functions both write tab id 6.  This plugin accepts only
// calls whose return address is inside the exact Resources/Rocks panel function,
// plus the calls deposits.dll appends immediately after that same panel.  Every
// other caller passes through byte-for-byte unchanged.
//
// Addresses and layout constants are for SOVIET64.exe v1.1.1.9.  Both the PE
// identity and the hooked prologue are checked before anything is changed.

#include "../../src/tesmio_plugin.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <set>
#include <sstream>
#include <string>

#define PLUGIN_INI "plugins\\resources_button_fix.ini"
#define PLUGIN_VERSION "0.4.0"
#define PLUGIN_LOG_NAME "tesmioloader.resources_button_fix.log"

static const size_t MAX_INI_BYTES = 16u * 1024u * 1024u;
static const size_t MAX_CONFIG_VALUE = 63u;

// SOVIET64.exe v1.1.1.9
#define EXPECTED_TIMESTAMP  0x6A3EB6ADu
#define EXPECTED_IMAGE_SIZE 0x00A9D000u

// Draws one editor tool button. It handles drawing, hover and click detection,
// so changing these arguments moves the visible button and its hitbox together.
#define P_ED_DRAW_BUTTON 0x382760

// The Resources panel calculates the round "delete all resources" button's X
// and Y immediately before drawing it. These two RIP-relative float reads are
// repointed to plugin-owned values; the resulting coordinates are then reused
// by the game for the visible control and its hitbox.
#define P_ED_CANCEL_X_MUL 0x233633
#define P_ED_CANCEL_Y_MUL 0x233650

// Panel-local transitions from the tool grid to the controls below it.  They
// are repointed instead of changing the shared constants, so Trees and every
// other editor window retain the game's original layout.
#define P_ED_RESOURCES_BRUSH_GAP_RELOAD 0x2337D2
#define P_ED_ROCKS_BRUSH_Y_MUL          0x22F171

// Exact function ranges from the executable's .pdata table. Return addresses
// inside these ranges identify the two supported vanilla panels unambiguously.
#define P_ED_ROCKS_PANEL_BEGIN     0x22EEA0
#define P_ED_ROCKS_PANEL_END       0x230577
#define P_ED_RESOURCES_PANEL_BEGIN 0x233180
#define P_ED_RESOURCES_PANEL_END   0x233887

// Shared grid constants.
#define G_DPI          0x992088
#define G_ED_X_A       0x90AA28   // 50
#define G_ED_X_B       0x90AB14   // 85
#define G_ED_Y_BASE    0x90ADB8   // 250
#define G_ED_Y_CAPTION 0x90AAFC   // 80
#define G_ED_BUTTON    0x909ED4   // 0.85
#define G_ED_ROW_STEP  0x90AB2C   // 90
#define G_ED_RES_STEP  0x90AB84   // 105
#define G_ED_CANCEL_X  0x90A9FC   // 42: original cancel X base
#define G_ED_CANCEL_Y  0x90AC84   // 170: grid top -> original cancel button
#define G_ED_BRUSH_GAP 0x90AA78   // 60: original cancel -> brush controls
#define G_ED_ROCKS_BRUSH_Y 0x90AADC // 75: Rocks grid top -> brush controls

// Rocks uses the same 85 constant for its initial X offset and column step.
#define G_ED_ROCK_STEP G_ED_X_B

// The safe width is the proven v0.1 Resources layout: eleven buttons at 50%.
// The drawer's logical button is 100 units wide before its 0.85 size argument.
#define BUTTON_LOGICAL_WIDTH 100.0f
#define SAFE_REFERENCE_COLUMNS 11
#define SAFE_REFERENCE_SCALE   0.50f
#define AUTO_VERTICAL_MARGIN   15.0f

typedef float (*t_ED_DrawButton)(void* self, void* tool, void* accumulator,
                                 float x, float y, float size,
                                 int flag, char a, char b);

static t_ED_DrawButton o_ED_DrawButton;

struct ScaleSetting
{
    int   automatic;
    float value;
};

struct ResourcesConfig
{
    int enabled;
    int columnsPerBlock;
    int maximumBlocks;
    ScaleSetting scale;
    float horizontalSpacing;
    float verticalSpacing;
    float blockGap;
    float gridToCancelGap;
    float cancelToBrushGap;
    float xOffset;
    float yOffset;
    int expandWindow;
    int cancelBelowGrid;
    float cancelXOffset;
    float cancelYOffset;
};

struct RocksConfig
{
    int enabled;
    int blocks;                 // 1 = P E P E, 2 = P P / E E
    ScaleSetting scale;
    float horizontalSpacing;
    float verticalSpacing;
    float controlsGap;
    float xOffset;
    float yOffset;
    int expandWindow;
};

struct PluginConfig
{
    int enabled;
    int debug;
    ResourcesConfig resources;
    RocksConfig rocks;
};

static int g_enabled = 1;
static int g_debug;
static HANDLE g_detail = INVALID_HANDLE_VALUE;
static volatile LONG g_logWarnings;
static volatile LONG g_logErrors;
static volatile LONG g_logFatals;
static ULONGLONG g_phaseStarted;
static LONG g_phaseWarnings;
static LONG g_phaseErrors;
static LONG g_phaseFatals;
static DWORD g_lastWindowsError;
static bool g_bound;

static const ResourcesConfig kDefaultResources = {
    1, 9, 2, { 0, 0.50f }, 1.00f, 1.00f, 0.35f,
    10.00f, 40.00f, 0.00f, 0.00f,
    1, 1, 0.00f, 0.00f
};

static const RocksConfig kDefaultRocks = {
    1, 2, { 1, 1.00f }, 1.00f, 1.00f, 15.00f, 0.00f, 0.00f, 1
};

static ResourcesConfig g_resources = kDefaultResources;
static RocksConfig g_rocks = kDefaultRocks;

static volatile LONG g_runtimeEnabled = 1;
static volatile LONG g_faultLogged;
static volatile LONG g_resourceOverflowLogged;
static volatile LONG g_autoMinimumLogged;

static float* g_cancelSlots;
static int    g_cancelCoordinatesPatched;
static int    g_resourcesBrushGapPatched;
static int    g_rocksBrushYPatched;

static BYTE g_resourceDebugSeen[128];
static BYTE g_rocksDebugSeen[128];

static std::string g_iniPath;
static FILETIME g_iniWriteTime;
static int      g_iniTimeValid;
static ULONGLONG g_lastIniPoll;
static volatile LONG g_iniTimestampErrorLogged;

static BYTE*  g_depositsBase;
static SIZE_T g_depositsSize;

enum PanelKind
{
    PANEL_NONE,
    PANEL_RESOURCES,
    PANEL_ROCKS
};

static PanelKind g_appendContext;
static void*     g_appendContextSelf;

// Counts from the previous complete frame make `auto` stable from the first
// button onward. New tools discovered later in a frame take effect next frame.
static int g_resourcePairCount = 5;
static int g_resourceObserved  = 5;
static int g_rocksPairCount    = 1;
static int g_rocksObserved     = 1;
static int g_rocksVanillaPairs = 1;

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
    if (H) Logf("resources_button_fix  %s", full);
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
        action, error, systemText, remedy ? remedy : "Review the preceding context and retry");
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

static float GameFloat(DWORD rva)
{
    return *(float*)(g_exeBase + rva);
}

static bool ParseIntStrict(const char* text, int* out)
{
    if (!text || !text[0]) return false;
    errno = 0;
    char* end = NULL;
    long value = strtol(text, &end, 10);
    while (end && (*end == ' ' || *end == '\t' || *end == '\r')) end++;
    if (errno == ERANGE || end == text || (end && *end)) return false;
    *out = (int)value;
    return true;
}

static bool ParseFloatStrict(const char* text, float* out)
{
    if (!text || !text[0]) return false;
    errno = 0;
    char* end = NULL;
    double value = strtod(text, &end);
    while (end && (*end == ' ' || *end == '\t' || *end == '\r')) end++;
    if (errno == ERANGE || end == text || (end && *end) || !_finite(value))
        return false;
    *out = (float)value;
    return true;
}

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

static bool IsAbsoluteWindowsPath(const std::string& path)
{
    if (path.size() >= 3 &&
        ((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
        return true;
    return path.size() >= 2 &&
           (path[0] == '\\' || path[0] == '/') &&
           (path[1] == '\\' || path[1] == '/');
}

static std::string JoinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) return right;
    if (right.empty()) return left;
    char last = left[left.size() - 1];
    return (last == '\\' || last == '/') ? left + right : left + "\\" + right;
}

static bool FileExists(const std::string& path)
{
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// The folder this DLL was loaded from: the Workshop package under Soviet Mod
// Loader or the Workshop Bridge, the loader's plugins\ folder otherwise. Taken
// from the address of a static inside this DLL, never the host module.
static bool OwnDirectory(std::string& out)
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
    out = path;
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
        DWORD wanted = (DWORD)(remaining > (1u << 20) ? (1u << 20) : remaining);
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
            "The main plugin INI must be UTF-8 without a BOM");
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

static bool ParseBooleanValue(const std::string& section,
    const std::string& key, const std::string& value, size_t line,
    int* output)
{
    if (value == "0" || value == "1")
    {
        *output = value[0] - '0';
        return true;
    }
    Report("ERROR", PLUGIN_INI, "invalid-boolean",
        "[%s] %s must be exactly 0 or 1 at line %Iu",
        section.c_str(), key.c_str(), line);
    return false;
}

static bool ParseIntegerValue(const std::string& section,
    const std::string& key, const std::string& value, size_t line,
    int minimum, int maximum, int* output)
{
    int parsed = 0;
    if (ParseIntStrict(value.c_str(), &parsed) &&
        parsed >= minimum && parsed <= maximum)
    {
        *output = parsed;
        return true;
    }
    Report("ERROR", PLUGIN_INI, "config-range",
        "[%s] %s must be an integer from %d to %d at line %Iu",
        section.c_str(), key.c_str(), minimum, maximum, line);
    return false;
}

static bool ParseDecimalValue(const std::string& section,
    const std::string& key, const std::string& value, size_t line,
    float minimum, float maximum, float* output)
{
    float parsed = 0.0f;
    if (ParseFloatStrict(value.c_str(), &parsed) &&
        parsed >= minimum && parsed <= maximum)
    {
        *output = parsed;
        return true;
    }
    Report("ERROR", PLUGIN_INI, "config-range",
        "[%s] %s must be a number from %.2f to %.2f at line %Iu",
        section.c_str(), key.c_str(), minimum, maximum, line);
    return false;
}

static bool ParseScaleValue(const std::string& section,
    const std::string& value, size_t line, ScaleSetting* output)
{
    if (LowerAscii(value) == "auto")
    {
        output->automatic = 1;
        return true;
    }
    float parsed = 0.0f;
    if (!ParseDecimalValue(section, "button_scale", value, line,
                           0.25f, 1.00f, &parsed))
        return false;
    output->automatic = 0;
    output->value = parsed;
    return true;
}

static bool AssignConfigValue(const std::string& section,
    const std::string& key, const std::string& value, size_t line,
    PluginConfig* config)
{
    if (section == "general")
    {
        if (key == "enabled")
            return ParseBooleanValue(section, key, value, line, &config->enabled);
        if (key == "debug")
            return ParseBooleanValue(section, key, value, line, &config->debug);
    }
    else if (section == "resources_window")
    {
        ResourcesConfig& r = config->resources;
        if (key == "enabled")
            return ParseBooleanValue(section, key, value, line, &r.enabled);
        if (key == "columns_per_block")
            return ParseIntegerValue(section, key, value, line, 1, 32,
                                     &r.columnsPerBlock);
        if (key == "maximum_blocks")
            return ParseIntegerValue(section, key, value, line, 1, 4,
                                     &r.maximumBlocks);
        if (key == "button_scale")
            return ParseScaleValue(section, value, line, &r.scale);
        if (key == "horizontal_spacing")
            return ParseDecimalValue(section, key, value, line, 0.50f, 2.00f,
                                     &r.horizontalSpacing);
        if (key == "vertical_spacing")
            return ParseDecimalValue(section, key, value, line, 0.50f, 2.00f,
                                     &r.verticalSpacing);
        if (key == "block_gap")
            return ParseDecimalValue(section, key, value, line, 0.00f, 2.00f,
                                     &r.blockGap);
        if (key == "grid_to_cancel_gap")
            return ParseDecimalValue(section, key, value, line, 0.00f, 100.00f,
                                     &r.gridToCancelGap);
        if (key == "cancel_to_brush_gap")
            return ParseDecimalValue(section, key, value, line, 0.00f, 100.00f,
                                     &r.cancelToBrushGap);
        if (key == "x_offset")
            return ParseDecimalValue(section, key, value, line, -100.00f, 100.00f,
                                     &r.xOffset);
        if (key == "y_offset")
            return ParseDecimalValue(section, key, value, line, -100.00f, 100.00f,
                                     &r.yOffset);
        if (key == "expand_window")
            return ParseBooleanValue(section, key, value, line, &r.expandWindow);
        if (key == "cancel_position")
        {
            std::string position = LowerAscii(value);
            if (position == "below_grid" || position == "original")
            {
                r.cancelBelowGrid = position == "below_grid" ? 1 : 0;
                return true;
            }
            Report("ERROR", PLUGIN_INI, "config-position",
                "[resources_window] cancel_position must be below_grid or original at line %Iu",
                line);
            return false;
        }
        if (key == "cancel_x_offset")
            return ParseDecimalValue(section, key, value, line, -100.00f, 100.00f,
                                     &r.cancelXOffset);
        if (key == "cancel_y_offset")
            return ParseDecimalValue(section, key, value, line, -100.00f, 100.00f,
                                     &r.cancelYOffset);
    }
    else if (section == "rocks_window")
    {
        RocksConfig& r = config->rocks;
        if (key == "enabled")
            return ParseBooleanValue(section, key, value, line, &r.enabled);
        if (key == "blocks")
            return ParseIntegerValue(section, key, value, line, 1, 2,
                                     &r.blocks);
        if (key == "button_scale")
            return ParseScaleValue(section, value, line, &r.scale);
        if (key == "horizontal_spacing")
            return ParseDecimalValue(section, key, value, line, 0.50f, 2.00f,
                                     &r.horizontalSpacing);
        if (key == "vertical_spacing")
            return ParseDecimalValue(section, key, value, line, 0.50f, 2.00f,
                                     &r.verticalSpacing);
        if (key == "controls_gap")
            return ParseDecimalValue(section, key, value, line, 0.00f, 100.00f,
                                     &r.controlsGap);
        if (key == "x_offset")
            return ParseDecimalValue(section, key, value, line, -100.00f, 100.00f,
                                     &r.xOffset);
        if (key == "y_offset")
            return ParseDecimalValue(section, key, value, line, -100.00f, 100.00f,
                                     &r.yOffset);
        if (key == "expand_window")
            return ParseBooleanValue(section, key, value, line, &r.expandWindow);
    }

    Report("ERROR", PLUGIN_INI, "unknown-key",
        "Unknown key '%s' in section [%s] at line %Iu",
        key.empty() ? "<empty>" : key.c_str(), section.c_str(), line);
    return false;
}

static bool ParseConfigText(const std::string& text, PluginConfig* output)
{
    PluginConfig parsed;
    parsed.enabled = 1;
    parsed.debug = 0;
    parsed.resources = kDefaultResources;
    parsed.rocks = kDefaultRocks;

    std::istringstream input(text);
    std::string rawLine;
    std::string currentSection;
    std::set<std::string> sections;
    std::set<std::string> keys;
    size_t line = 0;

    while (std::getline(input, rawLine))
    {
        ++line;
        std::string content = TrimString(rawLine);
        if (content.empty() || content[0] == ';' || content[0] == '#')
            continue;

        if (content[0] == '[')
        {
            if (content.size() < 3 || content[content.size() - 1] != ']' ||
                content.find(']') != content.size() - 1)
            {
                Report("ERROR", PLUGIN_INI, "malformed-section",
                    "Malformed section header at line %Iu: %s",
                    line, content.c_str());
                return false;
            }
            std::string section = LowerAscii(TrimString(
                content.substr(1, content.size() - 2)));
            if (section != "general" && section != "resources_window" &&
                section != "rocks_window")
            {
                Report("ERROR", PLUGIN_INI, "unknown-section",
                    "Unknown section '%s' at line %Iu",
                    section.empty() ? "<empty>" : section.c_str(), line);
                return false;
            }
            if (!sections.insert(section).second)
            {
                Report("ERROR", PLUGIN_INI, "duplicate-section",
                    "Section [%s] appears more than once (line %Iu)",
                    section.c_str(), line);
                return false;
            }
            currentSection = section;
            continue;
        }

        size_t equals = content.find('=');
        if (equals == std::string::npos)
        {
            Report("ERROR", PLUGIN_INI, "missing-assignment",
                "Expected key = value at line %Iu: %s", line, content.c_str());
            return false;
        }
        if (currentSection.empty())
        {
            Report("ERROR", PLUGIN_INI, "assignment-outside-section",
                "Configuration assignment outside a section at line %Iu",
                line);
            return false;
        }

        std::string key = LowerAscii(TrimString(content.substr(0, equals)));
        std::string value = TrimString(content.substr(equals + 1));
        if (key.empty() || value.empty())
        {
            Report("ERROR", PLUGIN_INI, "missing-value",
                "[%s] contains an empty key or value at line %Iu",
                currentSection.c_str(), line);
            return false;
        }
        if (value.size() > MAX_CONFIG_VALUE)
        {
            Report("ERROR", PLUGIN_INI, "value-too-long",
                "[%s] %s exceeds the maximum of %Iu characters at line %Iu",
                currentSection.c_str(), key.c_str(), MAX_CONFIG_VALUE, line);
            return false;
        }

        std::string qualified = currentSection + "." + key;
        if (!keys.insert(qualified).second)
        {
            Report("ERROR", PLUGIN_INI, "duplicate-key",
                "[%s] %s appears more than once (line %Iu)",
                currentSection.c_str(), key.c_str(), line);
            return false;
        }
        if (!AssignConfigValue(currentSection, key, value, line, &parsed))
            return false;
    }

    static const char* const requiredSections[] = {
        "general", "resources_window", "rocks_window"
    };
    for (size_t i = 0;
         i < sizeof(requiredSections) / sizeof(requiredSections[0]); ++i)
    {
        if (sections.find(requiredSections[i]) == sections.end())
        {
            Report("ERROR", PLUGIN_INI, "missing-section",
                "Required section [%s] is missing", requiredSections[i]);
            return false;
        }
    }

    *output = parsed;
    return true;
}

static bool LoadConfigFile(const std::string& path, PluginConfig* output)
{
    std::string text;
    if (!ReadTextFile(path, text))
    {
        ReportWindows("ERROR", path.c_str(), "config-read",
            "Configuration could not be read", g_lastWindowsError,
            "Verify that the file exists, is readable, and is not oversized");
        return false;
    }
    return ValidateUtf8WithoutBom(path, text) && ParseConfigText(text, output);
}

static void LogConfig(const char* reason)
{
    char resourceScale[32], rocksScale[32];
    if (g_resources.scale.automatic)
        strcpy_s(resourceScale, sizeof(resourceScale), "auto");
    else
        _snprintf_s(resourceScale, sizeof(resourceScale), _TRUNCATE,
                    "%.2f", g_resources.scale.value);
    if (g_rocks.scale.automatic)
        strcpy_s(rocksScale, sizeof(rocksScale), "auto");
    else
        _snprintf_s(rocksScale, sizeof(rocksScale), _TRUNCATE,
                    "%.2f", g_rocks.scale.value);

    Info("%s: Resources enabled=%d, %d columns x %d blocks, scale=%s, "
        "expand=%d, cancel=%s; Rocks enabled=%d, blocks=%d, scale=%s, expand=%d",
        reason, g_resources.enabled, g_resources.columnsPerBlock,
        g_resources.maximumBlocks, resourceScale, g_resources.expandWindow,
        g_resources.cancelBelowGrid ? "below_grid" : "original",
        g_rocks.enabled, g_rocks.blocks, rocksScale, g_rocks.expandWindow);
}

static void UpdateLayoutSlots();

static void ApplyConfig(const PluginConfig& config, const char* reason,
                        bool liveReload)
{
    if (liveReload && config.enabled != g_enabled)
        Report("WARN", PLUGIN_INI, "restart-required",
            "[general] enabled changed while the game is running; the change "
            "will take effect after a complete restart");
    else
        g_enabled = config.enabled;

    g_debug = config.debug;
    g_resources = config.resources;
    g_rocks = config.rocks;

    memset(g_resourceDebugSeen, 0, sizeof(g_resourceDebugSeen));
    memset(g_rocksDebugSeen, 0, sizeof(g_rocksDebugSeen));
    InterlockedExchange(&g_resourceOverflowLogged, 0);
    InterlockedExchange(&g_autoMinimumLogged, 0);
    UpdateLayoutSlots();
    LogConfig(reason);
}

static bool ReadIniWriteTime(FILETIME* out)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(g_iniPath.c_str(), GetFileExInfoStandard, &data))
    {
        g_lastWindowsError = GetLastError();
        return false;
    }
    *out = data.ftLastWriteTime;
    return true;
}

// Called only from a recognised target panel. A changed file is re-read before
// that panel's first visible button is adjusted. Closing the window is not
// technically required, but save + close + reopen is the simple user workflow.
static void MaybeReloadConfig()
{
    ULONGLONG now = GetTickCount64();
    if (now - g_lastIniPoll < 250) return;
    g_lastIniPoll = now;

    FILETIME current;
    if (!ReadIniWriteTime(&current))
    {
        if (InterlockedCompareExchange(&g_iniTimestampErrorLogged, 1, 0) == 0)
            ReportWindows("WARN", g_iniPath.c_str(), "config-watch",
                "Configuration timestamp could not be read", g_lastWindowsError,
                "Keep the last valid settings active and restore access to the INI");
        return;
    }
    InterlockedExchange(&g_iniTimestampErrorLogged, 0);
    if (g_iniTimeValid && CompareFileTime(&current, &g_iniWriteTime) == 0) return;

    g_iniWriteTime = current;
    g_iniTimeValid = 1;
    try
    {
        PluginConfig config;
        if (!LoadConfigFile(g_iniPath, &config))
        {
            Report("ERROR", PLUGIN_INI, "reload-rejected",
                "Configuration reload was rejected; all previous valid settings remain active");
            return;
        }
        ApplyConfig(config, "configuration reloaded", true);
    }
    catch (const std::exception& error)
    {
        Report("ERROR", PLUGIN_INI, "reload-exception",
            "Configuration reload raised a C++ exception: %s; previous settings remain active",
            error.what());
    }
    catch (...)
    {
        Report("ERROR", PLUGIN_INI, "reload-exception",
            "Configuration reload raised an unknown exception; previous settings remain active");
    }
}

static bool SupportedGameBuild()
{
    if (!g_exeBase || g_exeSize < sizeof(IMAGE_DOS_HEADER))
    {
        Report("FATAL", "SOVIET64.exe", "pe-header",
            "Executable image is unavailable or too small. Action: verify the "
            "game installation and TesmioLoader compatibility");
        return false;
    }

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)g_exeBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        (SIZE_T)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > g_exeSize)
    {
        Report("FATAL", "SOVIET64.exe", "pe-header",
            "Executable PE header is invalid. Action: verify the game files");
        return false;
    }

    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(g_exeBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        Report("FATAL", "SOVIET64.exe", "pe-signature",
            "Executable PE signature, machine, or optional-header format is invalid. "
            "Action: verify the 64-bit game files");
        return false;
    }

    DWORD timestamp = nt->FileHeader.TimeDateStamp;
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    if (timestamp == EXPECTED_TIMESTAMP && imageSize == EXPECTED_IMAGE_SIZE)
        return true;

    Report("FATAL", "SOVIET64.exe", "unsupported-build",
        "Unsupported game build: timestamp=0x%08lX, image=0x%08lX; expected "
        "v1.1.1.9 (0x%08X, 0x%08X). Action: use the supported game version",
        (unsigned long)timestamp, (unsigned long)imageSize,
        EXPECTED_TIMESTAMP, EXPECTED_IMAGE_SIZE);
    return false;
}

static void DescribeModule(HMODULE module, BYTE** baseOut, SIZE_T* sizeOut)
{
    *baseOut = NULL;
    *sizeOut = 0;
    if (!module) return;

    BYTE* base = (BYTE*)module;
    if (!ReadablePtr(base, sizeof(IMAGE_DOS_HEADER))) return;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        dos->e_lfanew > 0x100000) return;
    if (!ReadablePtr(base + dos->e_lfanew, sizeof(IMAGE_NT_HEADERS64))) return;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.SizeOfImage < sizeof(IMAGE_DOS_HEADER)) return;

    *baseOut = base;
    *sizeOut = nt->OptionalHeader.SizeOfImage;
}

static bool InRange(const BYTE* address, const BYTE* base, SIZE_T size)
{
    if (!address || !base || !size) return false;
    UINT_PTR candidate = (UINT_PTR)address;
    UINT_PTR begin = (UINT_PTR)base;
    if (candidate < begin) return false;
    return candidate - begin < size;
}

// The vanilla caller sets the context. deposits.dll calls the same drawer only
// after the original panel returns, with the same editor object, so those calls
// inherit that exact context. Calls from Trees or any other game function clear
// it and can never be mistaken for one of the two supported panels.
static PanelKind ClassifyCaller(void* self, void* returnAddress, bool* fromGame)
{
    BYTE* caller = (BYTE*)returnAddress;
    *fromGame = false;

    if (InRange(caller, g_exeBase, g_exeSize))
    {
        DWORD rva = (DWORD)(caller - g_exeBase);
        PanelKind kind = PANEL_NONE;
        if (rva >= P_ED_RESOURCES_PANEL_BEGIN && rva < P_ED_RESOURCES_PANEL_END)
            kind = PANEL_RESOURCES;
        else if (rva >= P_ED_ROCKS_PANEL_BEGIN && rva < P_ED_ROCKS_PANEL_END)
            kind = PANEL_ROCKS;

        g_appendContext = kind;
        g_appendContextSelf = kind == PANEL_NONE ? NULL : self;
        *fromGame = kind != PANEL_NONE;
        return kind;
    }

    if (InRange(caller, g_depositsBase, g_depositsSize) &&
        self == g_appendContextSelf)
        return g_appendContext;

    g_appendContext = PANEL_NONE;
    g_appendContextSelf = NULL;
    return PANEL_NONE;
}

static bool ToolKind(const char* name, bool* paint, bool* erase)
{
    *paint = name && strncmp(name, "paint_", 6) == 0;
    *erase = name && strncmp(name, "erase_", 6) == 0;
    return *paint || *erase;
}

static int NearestSlot(float value)
{
    return (int)floorf(value + 0.5f);
}

static float SafeGridWidth()
{
    float button = GameFloat(G_ED_BUTTON);
    float step   = GameFloat(G_ED_RES_STEP) * button;
    float width  = BUTTON_LOGICAL_WIDTH * button;
    return ((SAFE_REFERENCE_COLUMNS - 1) * step + width) * SAFE_REFERENCE_SCALE;
}

static float ClampAutomaticScale(float scale)
{
    if (scale > 1.0f) scale = 1.0f;
    if (scale < 0.25f)
    {
        scale = 0.25f;
        if (InterlockedCompareExchange(&g_autoMinimumLogged, 1, 0) == 0)
            Report("WARN", PLUGIN_INI, "automatic-scale",
                "Automatic scale reached its safe minimum of 0.25. "
                "Action: reduce the number of buttons per row or block");
    }
    return scale;
}

static float AutomaticScale(int slots, float unscaledStep, float spacing)
{
    if (slots <= 1) return 1.0f;

    float buttonWidth = BUTTON_LOGICAL_WIDTH * GameFloat(G_ED_BUTTON);
    float required = (float)(slots - 1) * unscaledStep * spacing + buttonWidth;
    float scale = required > 0.0f ? SafeGridWidth() / required : 1.0f;
    return ClampAutomaticScale(scale);
}

static float ResourcesVerticalScale(int blocks)
{
    if (blocks <= 0) return 1.0f;

    float button = GameFloat(G_ED_BUTTON);
    float step = GameFloat(G_ED_ROW_STEP) * button
               * g_resources.verticalSpacing;
    float buttonHeight = BUTTON_LOGICAL_WIDTH * button;
    float lastRow = (float)(blocks * 2 - 1)
                  + (float)(blocks - 1) * g_resources.blockGap;
    float required = lastRow * step + buttonHeight;

    // The game starts its brush controls after the cancel-button offset plus
    // the configured controls gap. Keep a small margin above that boundary.
    float controlsGap = g_resourcesBrushGapPatched
                      ? g_resources.cancelToBrushGap : GameFloat(G_ED_BRUSH_GAP);
    float available = GameFloat(G_ED_CANCEL_Y) + controlsGap
                    - g_resources.yOffset - AUTO_VERTICAL_MARGIN;
    float scale = required > 0.0f ? available / required : 1.0f;
    return ClampAutomaticScale(scale);
}

static int ResourcesUsedBlocks(int count)
{
    if (count < 1) count = 1;
    int blocks = (count + g_resources.columnsPerBlock - 1)
               / g_resources.columnsPerBlock;
    if (blocks > g_resources.maximumBlocks)
        blocks = g_resources.maximumBlocks;
    return blocks;
}

static float ResourcesScaleForCount(int count)
{
    if (!g_resources.scale.automatic)
        return g_resources.scale.value;

    int usedColumns = count < g_resources.columnsPerBlock
                    ? count : g_resources.columnsPerBlock;
    float horizontal = AutomaticScale(
        usedColumns,
        GameFloat(G_ED_RES_STEP) * GameFloat(G_ED_BUTTON),
        g_resources.horizontalSpacing);

    // A growing window provides real vertical space. Compact mode retains the
    // earlier behaviour and also fits the grid above the vanilla brush area.
    if (g_resources.expandWindow)
        return horizontal;

    float vertical = ResourcesVerticalScale(ResourcesUsedBlocks(count));
    return horizontal < vertical ? horizontal : vertical;
}

static float RocksVerticalScale()
{
    float button = GameFloat(G_ED_BUTTON);
    float rowStep = GameFloat(G_ED_ROW_STEP) * button
                  * g_rocks.verticalSpacing;
    float buttonHeight = BUTTON_LOGICAL_WIDTH * button;
    float lastRow = g_rocks.blocks == 2 ? rowStep : 0.0f;
    float required = lastRow + buttonHeight;
    float available = GameFloat(G_ED_ROCKS_BRUSH_Y)
                    - g_rocks.yOffset - AUTO_VERTICAL_MARGIN;
    float scale = required > 0.0f ? available / required : 1.0f;
    return ClampAutomaticScale(scale);
}

static float RocksScaleForCount(int count)
{
    if (!g_rocks.scale.automatic)
        return g_rocks.scale.value;

    if (count < 1) count = 1;
    int slots = g_rocks.blocks == 1 ? count * 2 : count;
    float horizontal = AutomaticScale(
        slots,
        GameFloat(G_ED_ROCK_STEP) * GameFloat(G_ED_BUTTON),
        g_rocks.horizontalSpacing);

    // A growing window makes room below the grid. Compact mode keeps the
    // vanilla transition and therefore also fits the buttons vertically.
    if (g_rocks.expandWindow)
        return horizontal;

    float vertical = RocksVerticalScale();
    return horizontal < vertical ? horizontal : vertical;
}

static void UpdateLayoutSlots()
{
    if (!g_cancelSlots) return;

    g_cancelSlots[0] = GameFloat(G_ED_CANCEL_X);
    g_cancelSlots[1] = GameFloat(G_ED_CANCEL_Y);
    g_cancelSlots[2] = GameFloat(G_ED_BRUSH_GAP);
    g_cancelSlots[3] = GameFloat(G_ED_ROCKS_BRUSH_Y);

    if (g_resourcesBrushGapPatched && g_resources.enabled)
        g_cancelSlots[2] = g_resources.cancelToBrushGap;

    if (g_cancelCoordinatesPatched && g_resources.enabled)
    {
        g_cancelSlots[0] += g_resources.cancelXOffset;

        if (g_resources.expandWindow && g_resources.cancelBelowGrid)
        {
            int blocks = ResourcesUsedBlocks(g_resourcePairCount);
            float scale = ResourcesScaleForCount(g_resourcePairCount);
            float button = GameFloat(G_ED_BUTTON);
            float rowStep = GameFloat(G_ED_ROW_STEP) * button
                          * g_resources.verticalSpacing;
            float lastRow = (float)(blocks * 2 - 1)
                          + (float)(blocks - 1) * g_resources.blockGap;
            float belowGrid = g_resources.yOffset
                            + lastRow * rowStep * scale
                            + BUTTON_LOGICAL_WIDTH * button * scale
                            + g_resources.gridToCancelGap;
            if (belowGrid > g_cancelSlots[1])
                g_cancelSlots[1] = belowGrid;
        }
        g_cancelSlots[1] += g_resources.cancelYOffset;
    }

    if (g_rocksBrushYPatched && g_rocks.enabled && g_rocks.expandWindow)
    {
        float scale = RocksScaleForCount(g_rocksPairCount);
        float button = GameFloat(G_ED_BUTTON);
        float rowStep = GameFloat(G_ED_ROW_STEP) * button
                      * g_rocks.verticalSpacing;
        float lastRow = g_rocks.blocks == 2 ? rowStep * scale : 0.0f;
        // The common brush panel places its visible heading another 60 units
        // below the transition coordinate. Subtract that internal padding so
        // controls_gap describes the space the player actually sees.
        float transition = g_rocks.yOffset
                         + lastRow
                         + BUTTON_LOGICAL_WIDTH * button * scale
                         + g_rocks.controlsGap
                         - GameFloat(G_ED_BRUSH_GAP);
        if (transition < 0.0f) transition = 0.0f;
        g_cancelSlots[3] = transition;
    }

    if (g_debug && g_cancelCoordinatesPatched)
        Report("DEBUG", NULL, "resources-layout",
            "Resources cancel/layout inputs: x-base %.2f, y-offset %.2f, "
            "grid-gap %.2f, brush-gap %.2f, blocks %d",
            g_cancelSlots[0], g_cancelSlots[1],
            g_resources.gridToCancelGap, g_cancelSlots[2],
            ResourcesUsedBlocks(g_resourcePairCount));
    if (g_debug && g_rocksBrushYPatched)
        Report("DEBUG", NULL, "rocks-layout",
            "Rocks layout inputs: brush-offset %.2f, pairs %d, blocks=%d",
            g_cancelSlots[3], g_rocksPairCount, g_rocks.blocks);
}

static bool AdjustResources(bool fromGame, const char* name,
                            float* x, float* y, float* size)
{
    bool paint = false, erase = false;
    if (!ToolKind(name, &paint, &erase)) return false;

    float dpi      = GameFloat(G_DPI);
    float button   = GameFloat(G_ED_BUTTON);
    float x0       = dpi * (GameFloat(G_ED_X_A) + GameFloat(G_ED_X_B));
    float xStep    = dpi * GameFloat(G_ED_RES_STEP) * button;
    float yPaint   = dpi * (GameFloat(G_ED_Y_BASE) + GameFloat(G_ED_Y_CAPTION));
    float yStep    = dpi * GameFloat(G_ED_ROW_STEP) * button;
    if (!_finite(dpi) || dpi <= 0.0f || xStep <= 0.0f || yStep <= 0.0f)
        return false;

    int index = NearestSlot((*x - x0) / xStep);
    if (index < 0 || index >= 128) return false;
    if (fabsf(*x - (x0 + (float)index * xStep)) > xStep * 0.20f) return false;

    if (fromGame && paint && index == 0)
    {
        g_resourcePairCount = g_resourceObserved < 5 ? 5 : g_resourceObserved;
        g_resourceObserved = 5;
        MaybeReloadConfig();
        UpdateLayoutSlots();
    }
    if (index + 1 > g_resourceObserved) g_resourceObserved = index + 1;

    if (!g_resources.enabled) return false;

    int capacity = g_resources.columnsPerBlock * g_resources.maximumBlocks;
    if (index >= capacity)
    {
        if (InterlockedCompareExchange(&g_resourceOverflowLogged, 1, 0) == 0)
            Report("WARN", PLUGIN_INI, "resources-capacity",
                "Resources capacity exceeded at pair %d; capacity is %d "
                "(%d columns x %d blocks). Excess buttons keep their original "
                "position. Action: increase capacity or reduce the number of buttons",
                index + 1, capacity, g_resources.columnsPerBlock,
                g_resources.maximumBlocks);
        return false;
    }

    int count = g_resourcePairCount;
    if (index + 1 > count) count = index + 1;
    float scale = ResourcesScaleForCount(count);

    int block  = index / g_resources.columnsPerBlock;
    int column = index % g_resources.columnsPerBlock;
    int row    = erase ? 1 : 0;
    float packedX = xStep * scale * g_resources.horizontalSpacing;
    float packedY = yStep * scale * g_resources.verticalSpacing;

    float oldX = *x, oldY = *y;
    *x = x0 + dpi * g_resources.xOffset + (float)column * packedX;
    *y = yPaint + dpi * g_resources.yOffset
       + (float)(block * 2 + row) * packedY
       + (float)block * packedY * g_resources.blockGap;
    *size *= scale;

    if (g_debug && paint && !g_resourceDebugSeen[index])
    {
        g_resourceDebugSeen[index] = 1;
        Report("DEBUG", NULL, "resources-pair",
            "Resources pair %d \"%s\": (%.1f,%.1f) -> block %d column %d "
            "(%.1f,%.1f), scale %.3f", index + 1, name, oldX, oldY,
            block + 1, column + 1, *x, *y, scale);
    }
    return true;
}

static bool ExactTool(const char* name, const char* expected)
{
    return name && strcmp(name, expected) == 0;
}

static bool AdjustRocks(bool fromGame, const char* name,
                        float* x, float* y, float* size)
{
    bool paint = false, erase = false;
    if (!ToolKind(name, &paint, &erase)) return false;

    float dpi    = GameFloat(G_DPI);
    float button = GameFloat(G_ED_BUTTON);
    float x0     = dpi * (GameFloat(G_ED_X_A) + GameFloat(G_ED_X_B));
    float xStep  = dpi * GameFloat(G_ED_ROCK_STEP) * button;
    float yPaint = dpi * (GameFloat(G_ED_Y_BASE) + GameFloat(G_ED_Y_CAPTION));
    float yStep  = dpi * GameFloat(G_ED_ROW_STEP) * button;
    if (!_finite(dpi) || dpi <= 0.0f || xStep <= 0.0f || yStep <= 0.0f)
        return false;

    int pair = -1;
    if (fromGame)
    {
        if (ExactTool(name, "paint_rock"))
        {
            g_rocksPairCount = g_rocksObserved < 1 ? 1 : g_rocksObserved;
            g_rocksObserved = 1;
            g_rocksVanillaPairs = 1;
            MaybeReloadConfig();
            UpdateLayoutSlots();
            pair = 0;
        }
        else if (ExactTool(name, "erase_rock"))
            pair = 0;
        else if (ExactTool(name, "paint_oasis") || ExactTool(name, "erase_oasis"))
        {
            g_rocksVanillaPairs = 2;
            if (g_rocksObserved < 2) g_rocksObserved = 2;
            pair = 1;
        }
        else
            return false;
    }
    else
    {
        // deposits.dll lays terrain-mask pairs out as P E on its second row:
        // pair 0 uses slots 0/1, pair 1 slots 2/3, and so on.
        int slot = NearestSlot((*x - x0) / xStep);
        if (slot < 0 || slot >= 254) return false;
        if (fabsf(*x - (x0 + (float)slot * xStep)) > xStep * 0.20f) return false;
        if ((paint && (slot & 1)) || (erase && !(slot & 1))) return false;
        pair = g_rocksVanillaPairs + slot / 2;
    }

    if (pair < 0 || pair >= 128) return false;
    if (pair + 1 > g_rocksObserved) g_rocksObserved = pair + 1;
    if (!g_rocks.enabled) return false;

    int count = g_rocksPairCount;
    if (pair + 1 > count) count = pair + 1;
    float scale = RocksScaleForCount(count);

    int slot = g_rocks.blocks == 1 ? pair * 2 + (erase ? 1 : 0) : pair;
    int row  = g_rocks.blocks == 2 && erase ? 1 : 0;
    float packedX = xStep * scale * g_rocks.horizontalSpacing;
    float packedY = yStep * scale * g_rocks.verticalSpacing;

    float oldX = *x, oldY = *y;
    *x = x0 + dpi * g_rocks.xOffset + (float)slot * packedX;
    *y = yPaint + dpi * g_rocks.yOffset + (float)row * packedY;
    *size *= scale;

    if (g_debug && paint && !g_rocksDebugSeen[pair])
    {
        g_rocksDebugSeen[pair] = 1;
        Report("DEBUG", NULL, "rocks-pair",
            "Rocks pair %d \"%s\": (%.1f,%.1f) -> blocks=%d slot=%d "
            "(%.1f,%.1f), scale %.3f", pair + 1, name, oldX, oldY,
            g_rocks.blocks, slot + 1, *x, *y, scale);
    }
    return true;
}

__declspec(noinline)
static float h_ED_DrawButton(void* self, void* tool, void* accumulator,
                             float x, float y, float size,
                             int flag, char a, char b)
{
    void* caller = _ReturnAddress();
    if (AtomicRead(&g_runtimeEnabled))
    {
        __try
        {
            bool fromGame = false;
            PanelKind panel = ClassifyCaller(self, caller, &fromGame);
            char name[128];
            // The descriptor begins with its ASCII tool name. Copy it through
            // the host's bounded readable-memory helper before comparing it.
            if (panel != PANEL_NONE && SafeReadStr(tool, name, sizeof(name)))
            {
                if (panel == PANEL_RESOURCES)
                    AdjustResources(fromGame, name, &x, &y, &size);
                else if (panel == PANEL_ROCKS)
                    AdjustRocks(fromGame, name, &x, &y, &size);
            }
        }
        __except (FaultFilter("resources_button_fix button layout",
                              GetExceptionInformation()))
        {
            InterlockedExchange(&g_runtimeEnabled, 0);
            if (InterlockedCompareExchange(&g_faultLogged, 1, 0) == 0)
                Report("FATAL", NULL, "layout-fault",
                    "Plugin disabled for this session after a layout fault. "
                    "Action: preserve this log and report the failure");
        }
    }

    return o_ED_DrawButton(self, tool, accumulator, x, y, size, flag, a, b);
}

static bool WriteCode(void* at, const void* bytes, size_t size,
                      const char* label)
{
    if (!at || !bytes || !size)
    {
        Report("ERROR", label, "patch-arguments",
            "Patch received an empty address or byte range");
        return false;
    }
    DWORD protection = 0;
    if (!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &protection))
    {
        ReportWindows("ERROR", label, "memory-protection",
            "Patch memory is not writable", GetLastError(),
            "Check for a game update or another plugin modifying the same code");
        return false;
    }
    memcpy(at, bytes, size);
    DWORD ignored = 0;
    if (!VirtualProtect(at, size, protection, &ignored))
    {
        ReportWindows("ERROR", label, "memory-protection",
            "Original patch memory protection could not be restored",
            GetLastError(), "Stop the game and report this failure");
        return false;
    }
    if (!FlushInstructionCache(GetCurrentProcess(), at, size))
    {
        ReportWindows("ERROR", label, "instruction-cache",
            "Instruction cache could not be flushed", GetLastError(),
            "Stop the game and report this failure");
        return false;
    }
    return true;
}

static bool Relative32(const BYTE* fromNext, const BYTE* target, int* value)
{
    __int64 distance = (__int64)(UINT_PTR)target - (__int64)(UINT_PTR)fromNext;
    if (distance < (__int64)INT_MIN || distance > (__int64)INT_MAX)
        return false;
    *value = (int)distance;
    return true;
}

// The original Resources code keeps 60 in xmm8 for two different purposes:
// the red delete tool's interaction rectangle and the following vertical gap.
// Reload only xmm8 immediately before the gap calculation, leaving the button
// and its full hitbox at the original size. Eight verified bytes are replaced
// by a near jump to a tiny cave, which repeats the displaced DPI load, reloads
// the configured gap, and returns to the next untouched instruction.
static bool InstallResourcesBrushGapReload(BYTE* cave, float* gapSlot)
{
    static const BYTE kMovssXmm2[] = { 0xF3, 0x0F, 0x10, 0x15 };
    static const BYTE kMovssXmm8[] = { 0xF3, 0x44, 0x0F, 0x10, 0x05 };
    const size_t stolen = 8;
    BYTE* site = g_exeBase + P_ED_RESOURCES_BRUSH_GAP_RELOAD;

    if (!ReadablePtr(site, stolen) ||
        memcmp(site, kMovssXmm2, sizeof(kMovssXmm2)) != 0)
    {
        Report("ERROR", "SOVIET64.exe", "brush-gap-instruction",
            "Resources brush-gap instruction changed; patch refused. "
            "Action: verify the supported game version and plugin compatibility");
        return false;
    }

    DWORD resolved = (DWORD)(P_ED_RESOURCES_BRUSH_GAP_RELOAD + stolen
                   + *(const int*)(site + sizeof(kMovssXmm2)));
    if (resolved != G_DPI)
    {
        Report("ERROR", "SOVIET64.exe", "brush-gap-source",
            "Resources brush-gap DPI source changed; patch refused. "
            "Action: verify the supported game version and plugin compatibility");
        return false;
    }

    BYTE code[22];
    memset(code, 0x90, sizeof(code));
    memcpy(code, kMovssXmm2, sizeof(kMovssXmm2));
    int relative = 0;
    if (!Relative32(cave + 8, g_exeBase + G_DPI, &relative))
        return false;
    memcpy(code + 4, &relative, sizeof(relative));

    memcpy(code + 8, kMovssXmm8, sizeof(kMovssXmm8));
    if (!Relative32(cave + 17, (BYTE*)gapSlot, &relative))
        return false;
    memcpy(code + 13, &relative, sizeof(relative));

    code[17] = 0xE9;
    if (!Relative32(cave + 22, site + stolen, &relative))
        return false;
    memcpy(code + 18, &relative, sizeof(relative));
    if (!WriteCode(cave, code, sizeof(code), "Resources brush-gap cave"))
        return false;

    BYTE jump[stolen];
    memset(jump, 0x90, sizeof(jump));
    jump[0] = 0xE9;
    if (!Relative32(site + 5, cave, &relative))
        return false;
    memcpy(jump + 1, &relative, sizeof(relative));
    return WriteCode(site, jump, sizeof(jump), "Resources brush-gap reload");
}

// Repoint one verified RIP-relative float operand to a plugin-owned slot. The
// shared constants themselves remain untouched because unrelated game UI also
// reads them.
static bool RepointFloat(DWORD siteRva, const BYTE* opcode, size_t opcodeSize,
                         DWORD originalRva, float expected,
                         float* slot, const char* label)
{
    const size_t instructionSize = opcodeSize + 4;
    BYTE* site = g_exeBase + siteRva;
    if (!ReadablePtr(site, instructionSize) ||
        memcmp(site, opcode, opcodeSize) != 0)
    {
        Report("ERROR", "SOVIET64.exe", "patch-instruction",
            "%s instruction changed; patch refused. Action: verify the supported "
            "game version and plugin compatibility", label);
        return false;
    }

    DWORD resolved = (DWORD)(siteRva + instructionSize
                   + *(const int*)(site + opcodeSize));
    if (resolved != originalRva || GameFloat(originalRva) != expected)
    {
        Report("ERROR", "SOVIET64.exe", "patch-source",
            "%s source changed; patch refused. Action: verify the supported "
            "game version and plugin compatibility", label);
        return false;
    }

    __int64 displacement = (BYTE*)slot - (site + instructionSize);
    if (displacement < (__int64)INT_MIN || displacement > (__int64)INT_MAX)
    {
        Report("ERROR", NULL, "patch-range",
            "%s slot is out of range. Action: disable conflicting plugins and retry",
            label);
        return false;
    }
    int relative = (int)displacement;
    return WriteCode(site + opcodeSize, &relative, sizeof(relative), label);
}

static const BYTE kMulssXmm6[] = { 0xF3, 0x0F, 0x59, 0x35 };
static const BYTE kMulssXmm1[] = { 0xF3, 0x0F, 0x59, 0x0D };
static const BYTE kMulssXmm2[] = { 0xF3, 0x0F, 0x59, 0x15 };

static const BYTE kDrawButtonPrologue[] = {
    0x48, 0x8B, 0xC4,                   // mov rax,rsp
    0x48, 0x89, 0x58, 0x10,             // mov [rax+0x10],rbx
    0x55,                               // push rbp
    0x56,                               // push rsi
    0x57,                               // push rdi
    0x41, 0x54,                         // push r12
    0x41, 0x55                          // push r13
};

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion(void)
{
    return TSM_API_VERSION;
}

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host,
                                                    TsmPluginInfo* info)
{
    if (!info) return 1;
    info->name = "resources_button_fix";
    info->version = PLUGIN_VERSION;

    const size_t requiredHostSize =
        offsetof(TsmHost, faultFilter) + sizeof(host->faultFilter);
    if (!host || host->structSize < requiredHostSize || !host->log ||
        !host->installInlineHook || !host->allocNear || !host->readablePtr ||
        !host->faultFilter || !host->baseDir || !host->baseDir[0])
        return 1;

    TsmBind(host);
    g_bound = true;
    BeginLogPhase();
    g_detail = TsmOpenLog(PLUGIN_LOG_NAME);
    if (g_detail == INVALID_HANDLE_VALUE)
        ReportWindows("WARN", PLUGIN_LOG_NAME, "log-open",
            "Detail log could not be opened", GetLastError(),
            "Verify write access to the TesmioLoader log directory");
    Info("TesmioLoader resources_button_fix %s starting; detail log: %s",
        PLUGIN_VERSION, PLUGIN_LOG_NAME);

    try
    {
        if (!IsAbsoluteWindowsPath(g_baseDir))
        {
            Report("FATAL", NULL, "plugin-folder",
                "TesmioLoader did not provide an absolute base directory. "
                "Action: verify the loader installation");
            LogSummary("Initialization", false);
            return 1;
        }

        // 1.1: <loader>\plugins\resources_button_fix.ini when it exists (the
        // classic install, or the effective INI Republic Mod Manager writes),
        // otherwise the INI beside the DLL - the Workshop package under Soviet
        // Mod Loader or the Workshop Bridge. Live reload follows the chosen file.
        g_iniPath = JoinPath(g_baseDir, PLUGIN_INI);
        if (!FileExists(g_iniPath))
        {
            std::string own;
            if (OwnDirectory(own) && FileExists(JoinPath(own, "resources_button_fix.ini")))
                g_iniPath = JoinPath(own, "resources_button_fix.ini");
        }
        Info("Configuration file: %s", g_iniPath.c_str());
        PluginConfig config;
        if (!LoadConfigFile(g_iniPath, &config))
        {
            Report("FATAL", PLUGIN_INI, "configuration",
                "Configuration is invalid. Action: correct the preceding file, "
                "section, key, encoding, or value error");
            LogSummary("Initialization", false);
            return 1;
        }
        ApplyConfig(config, "v1.1 configuration", false);

        g_iniTimeValid = ReadIniWriteTime(&g_iniWriteTime) ? 1 : 0;
        if (!g_iniTimeValid)
            ReportWindows("WARN", g_iniPath.c_str(), "config-watch",
                "Configuration timestamp could not be read", g_lastWindowsError,
                "Live reload is unavailable until file access is restored");

        InterlockedExchange(&g_runtimeEnabled, 1);
        InterlockedExchange(&g_faultLogged, 0);
        if (!g_enabled)
        {
            Info("enabled = 0; plugin remains inactive and no game memory is changed");
            LogSummaryStatus("Initialization",
                "was skipped because the plugin is disabled");
            return 1;
        }
        LogSummary("Initialization", true);
        return 0;
    }
    catch (const std::exception& error)
    {
        Report("FATAL", "resources_button_fix", "exception",
            "C++ exception in TsmPluginInit: %s. Action: preserve this log and report the failure",
            error.what());
        LogSummary("Initialization", false);
        return 1;
    }
    catch (...)
    {
        Report("FATAL", "resources_button_fix", "exception",
            "Unknown exception in TsmPluginInit. Action: preserve this log and report the failure");
        LogSummary("Initialization", false);
        return 1;
    }
}

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    BeginLogPhase();
    if (!H || !g_bound || !g_enabled || !g_exeBase || !g_exeSize)
    {
        Report("FATAL", NULL, "startup-state",
            "Required initialized plugin or executable state is unavailable. "
            "Action: verify the loader log and plugin configuration");
        LogSummary("Startup", false);
        return 1;
    }

    bool hookInstalled = false;
    try
    {
        if (!SupportedGameBuild())
        {
            LogSummary("Startup", false);
            return 1;
        }

        DescribeModule(GetModuleHandleA("deposits.dll"),
                       &g_depositsBase, &g_depositsSize);
        if (g_depositsBase)
            Info("deposits.dll recognized; appended Resources and Rocks pairs "
                "will follow their own panel");
        else
            Info("deposits.dll is not loaded or did not expose a valid 64-bit "
                "module image; only vanilla buttons are present");

        if (!InstallInlineHook(g_exeBase + P_ED_DRAW_BUTTON,
                               (void*)h_ED_DrawButton,
                               (void**)&o_ED_DrawButton,
                               kDrawButtonPrologue,
                               sizeof(kDrawButtonPrologue),
                               "resources_button_fix editor button"))
        {
            Report("FATAL", "SOVIET64.exe", "hook-install",
                "Editor button hook was refused. Action: check for a game update or "
                "another plugin modifying the same routine");
            LogSummary("Startup", false);
            return 1;
        }
        hookInstalled = true;

        g_cancelSlots = (float*)AllocNear(g_exeBase, 64);
        if (!g_cancelSlots)
        {
            Report("WARN", NULL, "near-memory",
                "No nearby memory is available for dynamic layout values; panel "
                "transitions retain their original positions. Action: restart the "
                "game and disable conflicting plugins if the layout is incorrect");
        }
        else
        {
            g_cancelSlots[0] = GameFloat(G_ED_CANCEL_X);
            g_cancelSlots[1] = GameFloat(G_ED_CANCEL_Y);
            g_cancelSlots[2] = GameFloat(G_ED_BRUSH_GAP);
            g_cancelSlots[3] = GameFloat(G_ED_ROCKS_BRUSH_Y);
            bool xPatched = RepointFloat(
                P_ED_CANCEL_X_MUL, kMulssXmm6, sizeof(kMulssXmm6),
                G_ED_CANCEL_X, 42.0f,
                &g_cancelSlots[0], "Resources cancel X");
            bool yPatched = RepointFloat(
                P_ED_CANCEL_Y_MUL, kMulssXmm1, sizeof(kMulssXmm1),
                G_ED_CANCEL_Y, 170.0f,
                &g_cancelSlots[1], "Resources cancel Y");
            g_resourcesBrushGapPatched = InstallResourcesBrushGapReload(
                (BYTE*)g_cancelSlots + 32, &g_cancelSlots[2]) ? 1 : 0;
            g_rocksBrushYPatched = RepointFloat(
                P_ED_ROCKS_BRUSH_Y_MUL,
                kMulssXmm2, sizeof(kMulssXmm2),
                G_ED_ROCKS_BRUSH_Y, 75.0f,
                &g_cancelSlots[3], "Rocks brush transition") ? 1 : 0;
            if (xPatched && yPatched)
            {
                g_cancelCoordinatesPatched = 1;
                Info("Resources cancel coordinates redirected at their visible drawing source");
            }
            else
                Report("WARN", "SOVIET64.exe", "cancel-coordinate",
                    "Resources cancel-coordinate patch is incomplete; vanilla "
                    "coordinates are retained. Action: check plugin compatibility");
            UpdateLayoutSlots();
            if (g_resourcesBrushGapPatched)
                Info("Resources controls gap is dynamic");
            if (g_rocksBrushYPatched)
                Info("Rocks brush transition is dynamic");
        }

        Info("v%s active; Trees and every non-target window pass through unchanged; "
            "original game files are unchanged", PLUGIN_VERSION);
        LogSummary("Startup", true);
        return 0;
    }
    catch (const std::exception& error)
    {
        InterlockedExchange(&g_runtimeEnabled, 0);
        Report("FATAL", "resources_button_fix", "exception",
            "C++ exception in TsmPluginStart: %s. Action: preserve this log and report the failure",
            error.what());
    }
    catch (...)
    {
        InterlockedExchange(&g_runtimeEnabled, 0);
        Report("FATAL", "resources_button_fix", "exception",
            "Unknown exception in TsmPluginStart. Action: preserve this log and report the failure");
    }

    LogSummary("Startup", false);
    // Once the inline hook exists, the DLL must remain loaded. Runtime changes
    // are disabled and the hook forwards unchanged arguments to the trampoline.
    return hookInstalled ? 0 : 1;
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
        {
            DeleteCriticalSection(&g_lock);
            g_bound = false;
        }
    }
    return TRUE;
}
