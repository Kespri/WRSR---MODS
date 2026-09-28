// vanilla_buildings.cpp - Vanilla Buildings 1.2 for WRSR.
//
// Source files in media_soviet and the Steam Workshop are never modified. During Init each
// enabled section is validated against the unchanged source and written to a
// process-local temporary file. Chained CRT file-open and ReadFileIntoBuffer
// IAT hooks redirect only exact declared paths, and only read-only requests.

#include "../../src/tesmio_plugin.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cctype>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <climits>
#include <exception>

// Optional text service. A `name =` that looks like a localisation key becomes
// the id the game's own $NAME takes, so a renamed base-game building follows
// the game language. Without the service the name stays a literal.
#ifndef TSM_SERVICE_LOCALIZATION
#define TSM_SERVICE_LOCALIZATION "localization"
#define TSM_LOCALIZATION_VERSION 1u
typedef struct TsmLocalizationApi
{
    int (*resolve)(const char* nameSpace, const char* key);
    int (*resolveFull)(const char* fullyQualifiedKey);
} TsmLocalizationApi;
#endif
static const TsmLocalizationApi* g_localization = NULL;

static const size_t MAX_FILE_BYTES = 4u * 1024u * 1024u;
static const size_t MAX_OUTPUT_BYTES = 8u * 1024u * 1024u;
static const size_t MAX_CONFIG_LINE = 4096;
static const size_t MAX_BUILDINGS = 256;
static const size_t MAX_TARGETS = 256;
static const size_t MAX_OPERATIONS = 512;

#define SYM_READ_FILE "?C3DHelp_ReadFileIntoBuffer@@YAHPEBDPEAPEADPEAI_N@Z"

#define PLUGIN_VERSION "0.4.4"
#define PLUGIN_INI "plugins\\vanilla_buildings.ini"
#define PLUGIN_LOG_NAME "tesmioloader.vanilla_buildings.log"

enum OpKind
{
    OP_REPLACE,
    OP_REPLACE_CONNECTION,
    OP_REMOVE,
    OP_REMOVE_CONNECTION,
    OP_ADD,
    OP_ADD_CONNECTION,
    OP_INSERT
};

struct Point
{
    double v[3];
};

struct ConnectionBlock
{
    int start;
    int end;
    std::string token;
    Point first;
    Point second;
    // 2 = token line plus two point lines; 1 = a one-point entry such as
    // $CONNECTION_ROAD_DEAD, written inline ("$TOKEN x y z") or as token line plus one
    // point line. pointText keeps the point as written for a one-point entry.
    int points = 2;
    std::string pointText;
};

struct Operation
{
    OpKind kind;
    std::vector<std::string> field;
    int matchStart;
    int matchEnd;
    int insertAt;
    int anchorAt;         // insert: line index of the anchor (collision check)
    int anchorOp;         // insert: index of the earlier operation whose line is the anchor (-1 = original line)
    bool after;           // insert: 1 = the new line follows the anchor
    int points;           // connection commands: 2 = two-point block, 1 = one-point entry
    size_t configLine;

    Operation() : kind(OP_ADD), matchStart(-1), matchEnd(-1), insertAt(-1), anchorAt(-1), anchorOp(-1), after(false), points(2), configLine(0) {}
};

struct Decl
{
    std::string section;
    std::string target;
    int targetKind; // 0 = legacy/automatic, 1 = Vanilla, 2 = DLC, 3 = Workshop.
    std::vector<std::pair<int, std::string>> targets; // Expanded into independent declarations after parsing.
    std::string canonical;
    std::string sourcePath;
    std::string overlayPath;
    std::wstring overlayWide;
    LONG openedLogged;
    LONG unavailableLogged;
    bool enabled;
    bool valid;
    std::string error;
    std::string errorContext;
    std::string name;      // optional: replaces whatever $NAME line the target carries
    std::string nameLine;  // what that becomes, resolved once per declaration
    bool nameExpanded;     // the synthesised replace was already added
    std::vector<Operation> operations;

    Decl() : targetKind(0), openedLogged(0), unavailableLogged(0), enabled(true), valid(true),
             nameExpanded(false) {}
};

static std::vector<Decl> g_decl;
static bool g_enabled = true;
static bool g_debug = false;
static char g_gameDir[MAX_PATH];
static char g_mediaDir[MAX_PATH];
static char g_mediaCanonical[MAX_PATH * 2];
static char g_workshopDir[MAX_PATH * 2];
static char g_workshopCanonical[MAX_PATH * 2];
static HANDLE g_detail = INVALID_HANDLE_VALUE;
static volatile LONG g_logWarnings;
static volatile LONG g_logErrors;
static volatile LONG g_logFatals;
static bool g_bound;
static bool g_initialized;
static volatile LONG g_redirectEnabled = 1;
static volatile LONG g_runtimeErrorLogged;
static ULONGLONG g_phaseStarted;
static LONG g_phaseWarnings;
static LONG g_phaseErrors;
static LONG g_phaseFatals;
static DWORD g_lastWindowsError;
static int g_enabledSectionCount;

using t_ReadFileIntoBuffer = int(__cdecl*)(const char*, char**, unsigned int*, bool);
static t_ReadFileIntoBuffer o_ReadFile;
using t_Fopen = FILE*(__cdecl*)(const char*, const char*);
using t_FopenS = errno_t(__cdecl*)(FILE**, const char*, const char*);
using t_Wfopen = FILE*(__cdecl*)(const wchar_t*, const wchar_t*);
using t_WfopenS = errno_t(__cdecl*)(FILE**, const wchar_t*, const wchar_t*);
static t_Fopen o_Fopen;
static t_FopenS o_FopenS;
static t_Wfopen o_Wfopen;
static t_WfopenS o_WfopenS;
static bool g_hooksInstalled;

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
    char body[8192];
    _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, ap);
    char full[12288];
    _snprintf_s(full, sizeof(full), _TRUNCATE, "%s: %s [%s] %s",
        level, where && where[0] ? where : "-",
        rule && rule[0] ? rule : "general", body);
    CountLevel(level);
    if (H) Logf("vanilla_buildings  %s", full);
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

static void BeginLogPhase()
{
    g_phaseStarted = GetTickCount64();
    g_phaseWarnings = InterlockedExchangeAdd(&g_logWarnings, 0);
    g_phaseErrors = InterlockedExchangeAdd(&g_logErrors, 0);
    g_phaseFatals = InterlockedExchangeAdd(&g_logFatals, 0);
}

static void LogSummaryStatus(const char* phase, const char* status)
{
    ULONGLONG elapsed = g_phaseStarted ? GetTickCount64() - g_phaseStarted : 0;
    Info("%s %s after %llu ms; %ld warning(s), %ld error(s), %ld fatal error(s)",
        phase, status,
        (unsigned long long)elapsed,
        InterlockedExchangeAdd(&g_logWarnings, 0) - g_phaseWarnings,
        InterlockedExchangeAdd(&g_logErrors, 0) - g_phaseErrors,
        InterlockedExchangeAdd(&g_logFatals, 0) - g_phaseFatals);
}

static void LogSummary(const char* phase, bool success)
{
    LogSummaryStatus(phase, success ? "completed successfully" : "failed");
}

// ---------------------------------------------------------------- text helpers

static std::string Trimmed(const std::string& in)
{
    size_t a = 0;
    while (a < in.size() && (in[a] == ' ' || in[a] == '\t' || in[a] == '\r' || in[a] == '\n')) a++;
    size_t b = in.size();
    while (b > a && (in[b - 1] == ' ' || in[b - 1] == '\t' || in[b - 1] == '\r' || in[b - 1] == '\n')) b--;
    return in.substr(a, b - a);
}

static std::string Lower(std::string s)
{
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

static bool StartsWith(const std::string& s, const char* prefix)
{
    size_t n = strlen(prefix);
    return s.size() >= n && memcmp(s.data(), prefix, n) == 0;
}

static bool EndsWithI(const std::string& s, const char* suffix)
{
    size_t n = strlen(suffix);
    return s.size() >= n && _stricmp(s.c_str() + s.size() - n, suffix) == 0;
}

static std::string TokenOf(const std::string& line)
{
    std::string s = Trimmed(line);
    if (s.empty() || s[0] != '$') return std::string();
    size_t n = 1;
    while (n < s.size() && s[n] != ' ' && s[n] != '\t') n++;
    return s.substr(0, n);
}

static bool TokenOnly(const std::string& line)
{
    std::string s = Trimmed(line);
    return !s.empty() && TokenOf(s) == s;
}

static std::vector<std::string> SplitLines(const char* data, size_t size, bool* hadBom)
{
    std::vector<std::string> out;
    size_t p = 0;
    *hadBom = size >= 3 && (unsigned char)data[0] == 0xef &&
        (unsigned char)data[1] == 0xbb && (unsigned char)data[2] == 0xbf;
    if (*hadBom) p = 3;

    while (p < size)
    {
        size_t e = p;
        while (e < size && data[e] != '\r' && data[e] != '\n') e++;
        out.push_back(std::string(data + p, e - p));
        if (e < size && data[e] == '\r') e++;
        if (e < size && data[e] == '\n') e++;
        p = e;
    }
    if (out.empty()) out.push_back(std::string());
    return out;
}

// Own the handle across allocations and C++ exceptions.
class ScopedFile
{
public:
    explicit ScopedFile(HANDLE value) : handle(value) {}
    ~ScopedFile() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    HANDLE get() const { return handle; }
    void close() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); handle = INVALID_HANDLE_VALUE; }
private:
    HANDLE handle;
    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;
};

static bool ReadWholeFile(const char* path, std::vector<char>* out)
{
    out->clear();
    ScopedFile file(CreateFileA(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
    if (file.get() == INVALID_HANDLE_VALUE)
    { g_lastWindowsError = GetLastError(); return false; }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file.get(), &size))
    { g_lastWindowsError = GetLastError(); return false; }
    if (size.QuadPart < 0 || (unsigned long long)size.QuadPart > MAX_FILE_BYTES)
    { g_lastWindowsError = ERROR_FILE_TOO_LARGE; return false; }

    out->resize((size_t)size.QuadPart);
    size_t done = 0;
    while (done < out->size())
    {
        DWORD got = 0;
        if (!ReadFile(file.get(), &(*out)[done], (DWORD)(out->size() - done), &got, NULL))
        { g_lastWindowsError = GetLastError(); out->clear(); return false; }
        if (!got)
        { g_lastWindowsError = ERROR_HANDLE_EOF; out->clear(); return false; }
        done += got;
    }
    return true;
}

static bool WriteWholeFile(const char* path, const std::string& data)
{
    if (data.size() > MAX_OUTPUT_BYTES)
    { g_lastWindowsError = ERROR_FILE_TOO_LARGE; return false; }
    std::string temp = std::string(path) + ".tmp";
    ScopedFile file(CreateFileA(temp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, NULL));
    if (file.get() == INVALID_HANDLE_VALUE)
    { g_lastWindowsError = GetLastError(); return false; }

    size_t done = 0;
    DWORD error = ERROR_SUCCESS;
    while (done < data.size())
    {
        DWORD wrote = 0;
        if (!WriteFile(file.get(), data.data() + done, (DWORD)(data.size() - done), &wrote, NULL))
        { error = GetLastError(); break; }
        if (!wrote) { error = ERROR_WRITE_FAULT; break; }
        done += wrote;
    }
    if (error == ERROR_SUCCESS && !FlushFileBuffers(file.get()))
        error = GetLastError();
    file.close();
    if (error != ERROR_SUCCESS)
    {
        g_lastWindowsError = error;
        DeleteFileA(temp.c_str());
        return false;
    }
    if (!MoveFileExA(temp.c_str(), path, MOVEFILE_WRITE_THROUGH))
    {
        g_lastWindowsError = GetLastError();
        DeleteFileA(temp.c_str());
        return false;
    }
    return true;
}

static std::vector<std::string> SplitFields(const std::string& value)
{
    std::vector<std::string> out;
    size_t p = 0;
    for (;;)
    {
        size_t q = value.find('|', p);
        out.push_back(Trimmed(value.substr(p, q == std::string::npos ? q : q - p)));
        if (q == std::string::npos) break;
        p = q + 1;
    }
    return out;
}

// ----------------------------------------------------------- path validation

static std::string SlashesLower(std::string s)
{
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == '/') s[i] = '\\';
        s[i] = (char)tolower((unsigned char)s[i]);
    }
    return s;
}

static bool PositiveDecimal(const std::string& s)
{
    if (s.empty() || s.size() > 20 || s[0] == '0') return false;
    return s.find_first_not_of("0123456789") == std::string::npos;
}

static bool WorkshopTarget(const std::string& canonical)
{
    return PositiveDecimal(canonical.substr(0, canonical.find('\\')));
}

static bool CanonicalTarget(const std::string& input, std::string* out, std::string* why)
{
    std::string s = SlashesLower(Trimmed(input));
    if (s.empty()) { *why = "target is missing"; return false; }
    if (s.size() > 220) { *why = "target is too long"; return false; }
    if (s[0] == '\\' || s[0] == '/' || s.find(':') != std::string::npos)
    {
        *why = "target must be relative to media_soviet or workshop\\content\\784150";
        return false;
    }

    std::vector<std::string> parts;
    size_t p = 0;
    while (p <= s.size())
    {
        size_t q = s.find('\\', p);
        std::string part = s.substr(p, q == std::string::npos ? q : q - p);
        if (part.empty() || part == "." || part == ".." ||
            part.back() == '.' || part.back() == ' ')
        { *why = "target contains an empty, dot or ambiguous path component"; return false; }
        for (unsigned char c : part)
            if (c < 32 || c == 127 || c == '<' || c == '>' || c == '"' ||
                c == '|' || c == '*' || c == '?')
            { *why = "target contains an invalid Windows path character"; return false; }
        std::string stem = part.substr(0, part.find('.'));
        if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
            stem == "conin$" || stem == "conout$" ||
            (stem.size() == 4 && (StartsWith(stem, "com") || StartsWith(stem, "lpt")) &&
             stem[3] >= '1' && stem[3] <= '9'))
        { *why = "target contains a reserved Windows device name"; return false; }
        parts.push_back(part);
        if (q == std::string::npos) break;
        p = q + 1;
    }
    s.clear();
    for (size_t i = 0; i < parts.size(); i++)
    {
        if (i) s += '\\';
        s += parts[i];
    }

    bool vanilla = parts.size() >= 2 && parts[0] == "buildings_types";
    bool dlc = parts.size() >= 4 && StartsWith(parts[0], "dlc") &&
        PositiveDecimal(parts[0].substr(3)) && parts[1] == "buildings" &&
        parts.back() == "building.ini";
    bool workshop = parts.size() >= 3 && PositiveDecimal(parts[0]) &&
        parts.back() == "building.ini";
    if (!vanilla && !dlc && !workshop)
    {
        *why = "expected buildings_types\\*.ini, dlcN\\buildings\\...\\building.ini, or WorkshopID\\...\\building.ini";
        return false;
    }
    if (!EndsWithI(s, ".ini")) { *why = "target must be an .ini file"; return false; }
    *out = s;
    return true;
}

static bool ResolveWorkshopDir(const std::string& gameDir)
{
    g_workshopDir[0] = g_workshopCanonical[0] = 0;
    std::string game = SlashesLower(gameDir);
    size_t end = game.find_last_of('\\');
    if (end == std::string::npos) return false;
    std::string common = game.substr(0, end);
    end = common.find_last_of('\\');
    if (end == std::string::npos || common.substr(end + 1) != "common") return false;
    std::string steamapps = common.substr(0, end);
    end = steamapps.find_last_of('\\');
    if (end == std::string::npos || steamapps.substr(end + 1) != "steamapps") return false;
    std::string workshop = steamapps + "\\workshop\\content\\784150";
    if (workshop.size() >= sizeof(g_workshopDir)) return false;
    strcpy_s(g_workshopDir, workshop.c_str());
    strcpy_s(g_workshopCanonical, workshop.c_str());
    return true;
}

static std::wstring PathToWide(const char* path)
{
    if (!path) return std::wstring();
    int length = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
    if (length <= 0) return std::wstring();
    std::wstring result((size_t)length, L'\0');
    if (!MultiByteToWideChar(CP_ACP, 0, path, -1, &result[0], length))
        return std::wstring();
    result.resize((size_t)length - 1);
    return result;
}

static bool ResolveGameDir(void)
{
    char exe[MAX_PATH];
    DWORD length = GetModuleFileNameA((HMODULE)H->exeModule, exe, sizeof(exe));
    if (!length || length >= sizeof(exe))
    {
        g_lastWindowsError = length ? ERROR_INSUFFICIENT_BUFFER : GetLastError();
        return false;
    }
    char* slash = strrchr(exe, '\\');
    if (!slash) { g_lastWindowsError = ERROR_BAD_PATHNAME; return false; }
    *slash = 0;
    strcpy_s(g_gameDir, sizeof(g_gameDir), exe);
    if (_snprintf_s(g_mediaDir, sizeof(g_mediaDir), _TRUNCATE, "%s\\media_soviet", g_gameDir) < 0)
    { g_lastWindowsError = ERROR_INSUFFICIENT_BUFFER; return false; }
    DWORD attr = GetFileAttributesA(g_mediaDir);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        g_lastWindowsError = attr == INVALID_FILE_ATTRIBUTES
            ? GetLastError() : ERROR_DIRECTORY;
        return false;
    }
    std::string canon = SlashesLower(g_mediaDir);
    strncpy_s(g_mediaCanonical, sizeof(g_mediaCanonical), canon.c_str(), _TRUNCATE);
    ResolveWorkshopDir(g_gameDir); // Only Workshop targets require a Steam-library layout.
    return true;
}

// ------------------------------------------------------ connection validation

static bool ParsePoint(const std::string& line, Point* point)
{
    const char* p = line.c_str();
    for (int i = 0; i < 3; i++)
    {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) return false;
        errno = 0;
        char* end = NULL;
        point->v[i] = strtod(p, &end);
        if (end == p || errno == ERANGE || !_finite(point->v[i])) return false;
        if (i < 2 && *end != ' ' && *end != '\t') return false;
        p = end;
    }
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return *p == 0;
}

static bool SameNumber(double a, double b)
{
    double scale = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    if (scale < 1.0) scale = 1.0;
    return fabs(a - b) <= 1e-6 * scale;
}

static bool SamePoint(const Point& a, const Point& b)
{
    return SameNumber(a.v[0], b.v[0]) && SameNumber(a.v[1], b.v[1]) &&
        SameNumber(a.v[2], b.v[2]);
}

static bool IsAllowPass(const std::string& token)
{
    return Lower(token).find("_allowpass") != std::string::npos;
}

// Any $CONNECTION_* token name (uppercase letters, digits, '_'); the one-point commands
// accept every such token, the two-point ones the narrower list below.
static bool IsConnectionTokenName(const std::string& token)
{
    if (!StartsWith(token, "$CONNECTION_") || StartsWith(token, "$CONNECTIONS_")) return false;
    for (size_t i = 1; i < token.size(); ++i)
        if (!((token[i] >= 'A' && token[i] <= 'Z') ||
              (token[i] >= '0' && token[i] <= '9') || token[i] == '_'))
            return false;
    return true;
}

static bool IsTwoPointConnectionToken(const std::string& token)
{
    if (!StartsWith(token, "$CONNECTION_")) return false;
    for (size_t i = 1; i < token.size(); ++i)
        if (!((token[i] >= 'A' && token[i] <= 'Z') ||
              (token[i] >= '0' && token[i] <= '9') || token[i] == '_'))
            return false;
    if (StartsWith(token, "$CONNECTIONS_")) return false;
    if (token == "$CONNECTION_ADVANCED_POINT") return false;
    if (token.find("_DEAD") != std::string::npos) return false;
    return TokenOf(token) == token;
}

static std::vector<ConnectionBlock> FindConnections(const std::vector<std::string>& lines)
{
    std::vector<ConnectionBlock> out;
    for (int i = 0; i < (int)lines.size(); i++)
    {
        std::string line = Trimmed(lines[i]);
        std::string token = TokenOf(line);
        if (!IsConnectionTokenName(token)) continue;
        ConnectionBlock b;
        b.start = i;
        b.token = token;
        if (TokenOnly(line))
        {
            // Two-point block: token line plus two point lines.
            if (IsTwoPointConnectionToken(token) && i + 2 < (int)lines.size() &&
                ParsePoint(lines[i + 1], &b.first) && ParsePoint(lines[i + 2], &b.second))
            {
                b.end = i + 2;
                b.points = 2;
                out.push_back(b);
                i += 2;
                continue;
            }
            // One-point entry on two lines: token line, then the point.
            if (i + 1 < (int)lines.size() && ParsePoint(lines[i + 1], &b.first))
            {
                b.end = i + 1;
                b.points = 1;
                b.pointText = Trimmed(lines[i + 1]);
                out.push_back(b);
                i += 1;
            }
            continue;
        }
        // One-point entry inline: "$CONNECTION_ROAD_DEAD x y z".
        std::string rest = Trimmed(line.substr(token.size()));
        if (ParsePoint(rest, &b.first))
        {
            b.end = i;
            b.points = 1;
            b.pointText = rest;
            out.push_back(b);
        }
    }
    return out;
}

static bool ParseOnePointFields(const Operation& op, int tokenField, int pointField,
    std::string* token, Point* point, std::string* why)
{
    *token = Trimmed(op.field[tokenField]);
    if (!TokenOnly(*token) || !IsConnectionTokenName(*token))
    {
        *why = "connection token is not a $CONNECTION_... token";
        return false;
    }
    if (!ParsePoint(op.field[pointField], point))
    {
        *why = "the connection point must contain exactly three numbers";
        return false;
    }
    return true;
}

static bool ParseConnectionFields(const Operation& op, int tokenField,
    int firstField, int secondField,
    std::string* token, Point* first, Point* second,
    std::string* why)
{
    *token = Trimmed(op.field[tokenField]);
    if (!TokenOnly(*token) || !IsTwoPointConnectionToken(*token))
    {
        *why = "connection token is not a supported two-point $CONNECTION_...";
        return false;
    }
    if (!ParsePoint(op.field[firstField], first) || !ParsePoint(op.field[secondField], second))
    {
        *why = "each connection coordinate line must contain exactly three numbers";
        return false;
    }
    if (SamePoint(*first, *second))
    {
        *why = "the two connection points must not be identical";
        return false;
    }
    return true;
}

// ---------------------------------------------------------- operation safety

static bool IsBlockToken(const std::string& token)
{
    static const char* prefix[] = {
        "$CONNECTION", "$TEXT_CAPTION", "$VEHICLE_", "$STATION_",
        "$RESOURCE_VISUALIZATION", "$RESOURCE_INCREASE", "$PARTICLE",
        "$MOVEABLE_DOOR", "$TURNPIKE", "$WORKER_RENDERING_AREA",
        "$UNDERGROUND", "$SHIP_STATION", "$AIRPLANE_STATION", "$HELIPORT",
        "$CONVEYOR"
    };
    for (size_t i = 0; i < sizeof(prefix) / sizeof(prefix[0]); i++)
        if (StartsWith(token, prefix[i])) return true;
    return false;
}

static bool ValidateSingleLine(const std::string& line, bool forAdd,
    bool forInsert, std::string* why)
{
    std::string token = TokenOf(line);
    if (token.size() < 2) { *why = "entry must start with a $TOKEN"; return false; }
    for (size_t i = 1; i < token.size(); ++i)
        if (!((token[i] >= 'A' && token[i] <= 'Z') ||
              (token[i] >= '0' && token[i] <= '9') || token[i] == '_'))
        { *why = "directive names must use uppercase ASCII letters, digits or '_'"; return false; }
    if (IsBlockToken(token))
    {
        *why = "multi-line/geometry entries are not allowed with this command";
        return false;
    }
    if (StartsWith(token, "$COST_WORK_"))
    {
        *why = "$COST_WORK_* geometry and phase entries are not allowed as a single value";
        return false;
    }
    if (forAdd && StartsWith(token, "$COST_"))
    {
        *why = "$COST_ lines require a unique phase anchor with insert_before";
        return false;
    }
    if (forInsert && StartsWith(token, "$COST_") && token != "$COST_RESOURCE_AUTO")
    {
        *why = "insert_before only accepts $COST_RESOURCE_AUTO as a new $COST_ entry";
        return false;
    }
    return true;
}

static int CountLine(const std::vector<std::string>& lines, const std::string& wanted, int* where)
{
    int count = 0;
    *where = -1;
    std::string w = Trimmed(wanted);
    for (int i = 0; i < (int)lines.size(); i++)
    {
        if (Trimmed(lines[i]) == w) { count++; *where = i; }
    }
    return count;
}

static bool IntervalsOverlap(int a0, int a1, int b0, int b1)
{
    return a0 <= b1 && b0 <= a1;
}

static bool Fail(Decl* d, const std::string& why)
{
    if (d->valid) d->error = why + (d->errorContext.empty() ? "" : "; " + d->errorContext);
    d->valid = false;
    return false;
}

static const char* OpName(OpKind kind)
{
    switch (kind)
    {
    case OP_REPLACE: return "replace";
    case OP_REPLACE_CONNECTION: return "replace_connection";
    case OP_REMOVE: return "remove";
    case OP_REMOVE_CONNECTION: return "remove_connection";
    case OP_ADD: return "add";
    case OP_ADD_CONNECTION: return "add_connection";
    case OP_INSERT: return "insert";
    }
    return "?";
}

static std::string RuleContext(const Operation& op, int index)
{
    std::string context = "command=" + std::to_string(index + 1);
    if (op.configLine) context += " ini_line=" + std::to_string(op.configLine);
    context += " rule={" + std::string(OpName(op.kind)) + " = ";
    for (size_t i = 0; i < op.field.size(); ++i)
    {
        if (i) context += " | ";
        context += op.field[i];
    }
    return context + "}";
}

static bool ValidateOperations(Decl* d, const std::vector<std::string>& lines)
{
    d->errorContext.clear();
    if (d->operations.size() > MAX_OPERATIONS)
        return Fail(d, "too many operations in one building section");
    std::vector<ConnectionBlock> blocks = FindConnections(lines);
    std::vector<std::pair<int, int> > changed;
    std::vector<std::string> plannedAdd;
    std::vector<Point> plannedFirst;
    std::vector<Point> plannedSecond;
    std::vector<std::pair<int, std::string> > plannedInsert;

    int lastEnd = -1;
    for (int i = 0; i < (int)lines.size(); i++)
        if (Trimmed(lines[i]) == "end") lastEnd = i;
    if (lastEnd < 0) return Fail(d, "no closing 'end' found");

    int connectionInsert = lastEnd;
    if (!blocks.empty()) connectionInsert = blocks.back().end + 1;
    else
    {
        for (int i = 0; i < (int)lines.size(); i++)
            if (TokenOf(lines[i]) == "$COST_WORK") { connectionInsert = i; break; }
    }

    for (int oi = 0; oi < (int)d->operations.size(); oi++)
    {
        Operation& op = d->operations[oi];
        d->errorContext = RuleContext(op, oi);
        std::string why;

        if (op.kind == OP_REPLACE || op.kind == OP_REMOVE)
        {
            const std::string& oldLine = op.field[0];
            if (!ValidateSingleLine(oldLine, false, false, &why))
                return Fail(d, std::string(OpName(op.kind)) + ": " + why);
            if (TokenOf(oldLine) == "$COST_WORK" && op.kind == OP_REMOVE)
                return Fail(d, "remove: a $COST_WORK phase cannot be removed individually");
            int at = -1;
            int count = CountLine(lines, oldLine, &at);
            if (count != 1)
                return Fail(d, std::string(OpName(op.kind)) + ": source line has " +
                    std::to_string(count) + " matches instead of exactly one");
            op.matchStart = op.matchEnd = at;

            if (op.kind == OP_REPLACE)
            {
                if (!ValidateSingleLine(op.field[1], false, false, &why))
                    return Fail(d, "replace (new line): " + why);
                if ((TokenOf(oldLine) == "$COST_WORK" || TokenOf(op.field[1]) == "$COST_WORK") &&
                    TokenOf(oldLine) != TokenOf(op.field[1]))
                    return Fail(d, "replace: a $COST_WORK phase header must remain a phase header");
                if (Trimmed(op.field[0]) == Trimmed(op.field[1]))
                    return Fail(d, "replace: old and new line are identical");
            }
        }
        else if (op.kind == OP_REPLACE_CONNECTION || op.kind == OP_REMOVE_CONNECTION)
        {
            // One-point form: remove = token | point; replace = old token | point | new token [| new point].
            bool onePoint = op.kind == OP_REMOVE_CONNECTION ? op.field.size() == 2 :
                (op.field.size() == 3 || (op.field.size() == 4 && StartsWith(Trimmed(op.field[2]), "$")));
            if (onePoint)
            {
                std::string token;
                Point point;
                if (!ParseOnePointFields(op, 0, 1, &token, &point, &why))
                    return Fail(d, std::string(OpName(op.kind)) + ": " + why);
                int count = 0;
                int at = -1;
                for (int bi = 0; bi < (int)blocks.size(); bi++)
                    if (blocks[bi].points == 1 && blocks[bi].token == token && SamePoint(blocks[bi].first, point))
                    {
                        count++;
                        at = bi;
                    }
                if (count != 1)
                    return Fail(d, std::string(OpName(op.kind)) + ": one-point connection has " +
                        std::to_string(count) + " matches instead of exactly one");
                op.matchStart = blocks[at].start;
                op.matchEnd = blocks[at].end;
                op.points = 1;
                if (op.kind == OP_REPLACE_CONNECTION)
                {
                    std::string newToken = Trimmed(op.field[2]);
                    if (!TokenOnly(newToken) || !IsConnectionTokenName(newToken))
                        return Fail(d, "replace_connection: new token is not a $CONNECTION_... token");
                    if (IsAllowPass(newToken))
                        return Fail(d, "replace_connection: *_ALLOWPASS is not accepted for safety reasons");
                    bool moves = op.field.size() == 4;
                    Point newPoint = point;
                    if (moves && !ParsePoint(op.field[3], &newPoint))
                        return Fail(d, "replace_connection: the new connection point must contain exactly three numbers");
                    if (newToken == token && (!moves || SamePoint(newPoint, point)))
                        return Fail(d, "replace_connection: old and new token are identical");
                }
            }
            else
            {
            std::string token;
            Point first, second;
            if (!ParseConnectionFields(op, 0, 1, 2, &token, &first, &second, &why))
                return Fail(d, std::string(OpName(op.kind)) + ": " + why);

            int count = 0;
            int at = -1;
            for (int bi = 0; bi < (int)blocks.size(); bi++)
            {
                if (blocks[bi].token == token && SamePoint(blocks[bi].first, first) &&
                    SamePoint(blocks[bi].second, second))
                {
                    count++;
                    at = bi;
                }
            }
            if (count != 1)
                return Fail(d, std::string(OpName(op.kind)) + ": connection block has " +
                    std::to_string(count) + " matches instead of exactly one");
            op.matchStart = blocks[at].start;
            op.matchEnd = blocks[at].end;

            if (op.kind == OP_REPLACE_CONNECTION)
            {
                std::string newToken = Trimmed(op.field[3]);
                if (!TokenOnly(newToken) || !IsTwoPointConnectionToken(newToken))
                    return Fail(d, "replace_connection: new token is not a supported two-point connection");
                if (IsAllowPass(newToken))
                    return Fail(d, "replace_connection: *_ALLOWPASS is not accepted for safety reasons");
                // Six fields: the connection also moves to two new points. They must parse,
                // differ, and stay clear of every other connection (existing or planned);
                // the block's own old points are free again. The token may then stay.
                bool moves = op.field.size() == 6;
                Point newFirst = first, newSecond = second;
                if (moves)
                {
                    if (!ParsePoint(op.field[4], &newFirst) || !ParsePoint(op.field[5], &newSecond))
                        return Fail(d, "replace_connection: each new coordinate line must contain exactly three numbers");
                    if (SamePoint(newFirst, newSecond))
                        return Fail(d, "replace_connection: the two new connection points must not be identical");
                    for (int bi = 0; bi < (int)blocks.size(); bi++)
                    {
                        if (bi == at) continue;
                        if (SamePoint(blocks[bi].first, newFirst) || SamePoint(blocks[bi].second, newFirst) ||
                            SamePoint(blocks[bi].first, newSecond) || SamePoint(blocks[bi].second, newSecond))
                            return Fail(d, "replace_connection: one of the new connection points is already occupied");
                    }
                    for (int pi = 0; pi < (int)plannedFirst.size(); pi++)
                        if (SamePoint(plannedFirst[pi], newFirst) || SamePoint(plannedFirst[pi], newSecond) ||
                            SamePoint(plannedSecond[pi], newFirst) || SamePoint(plannedSecond[pi], newSecond))
                            return Fail(d, "replace_connection: a new connection point collides with another new connection");
                    plannedFirst.push_back(newFirst);
                    plannedSecond.push_back(newSecond);
                }
                if (newToken == token && (!moves || (SamePoint(newFirst, first) && SamePoint(newSecond, second))))
                    return Fail(d, "replace_connection: old and new token are identical");
            }
            }
        }
        else if (op.kind == OP_ADD_CONNECTION && op.field.size() == 2)
        {
            // One-point entry: token | point. Dead ends share their point with a two-point
            // connection by design, so only an exact duplicate is refused.
            std::string token;
            Point point;
            if (!ParseOnePointFields(op, 0, 1, &token, &point, &why))
                return Fail(d, "add_connection: " + why);
            if (IsAllowPass(token))
                return Fail(d, "add_connection: *_ALLOWPASS cannot be inserted safely at block end");
            for (int bi = 0; bi < (int)blocks.size(); bi++)
                if (blocks[bi].points == 1 && blocks[bi].token == token && SamePoint(blocks[bi].first, point))
                    return Fail(d, "add_connection: the same one-point connection already exists");
            op.points = 1;
            op.insertAt = connectionInsert;
        }
        else if (op.kind == OP_ADD)
        {
            if (!ValidateSingleLine(op.field[0], true, false, &why)) return Fail(d, "add: " + why);
            int dummy = -1;
            if (CountLine(lines, op.field[0], &dummy) != 0)
                return Fail(d, "add: this line already exists in the source file");
            std::string normalized = Trimmed(op.field[0]);
            if (std::find(plannedAdd.begin(), plannedAdd.end(), normalized) != plannedAdd.end())
                return Fail(d, "add: the same new line is specified more than once");
            plannedAdd.push_back(normalized);
            op.insertAt = lastEnd;
        }
        else if (op.kind == OP_ADD_CONNECTION)
        {
            std::string token;
            Point first, second;
            if (!ParseConnectionFields(op, 0, 1, 2, &token, &first, &second, &why))
                return Fail(d, "add_connection: " + why);
            if (IsAllowPass(token))
                return Fail(d, "add_connection: *_ALLOWPASS cannot be inserted safely at block end");

            for (int bi = 0; bi < (int)blocks.size(); bi++)
            {
                if (blocks[bi].token == token && SamePoint(blocks[bi].first, first) &&
                    SamePoint(blocks[bi].second, second))
                    return Fail(d, "add_connection: the same connection block already exists");
                if (SamePoint(blocks[bi].first, first) || SamePoint(blocks[bi].second, first) ||
                    SamePoint(blocks[bi].first, second) || SamePoint(blocks[bi].second, second))
                    return Fail(d, "add_connection: one of the connection points is already occupied");
            }
            for (int pi = 0; pi < (int)plannedFirst.size(); pi++)
                if (SamePoint(plannedFirst[pi], first) || SamePoint(plannedFirst[pi], second))
                    return Fail(d, "add_connection: two new connections share an endpoint");
            for (int pi = 0; pi < (int)plannedSecond.size(); pi++)
                if (SamePoint(plannedSecond[pi], second) || SamePoint(plannedSecond[pi], first))
                    return Fail(d, "add_connection: two new connections share an endpoint");

            plannedFirst.push_back(first);
            plannedSecond.push_back(second);
            op.insertAt = connectionInsert;
        }
        else if (op.kind == OP_INSERT)
        {
            // insert = position | anchor | new line; position 0 puts the new line
            // before the anchor, 1 after it. A $COST_RESOURCE_AUTO line belongs to
            // the $COST_WORK line above it in the game's format, so position 1
            // attaches it to the anchor's phase and 0 to the phase before.
            const std::string position = Trimmed(op.field[0]);
            if (position != "0" && position != "1")
                return Fail(d, "insert: position must be 0 (before the anchor) or 1 (after the anchor)");
            op.after = position == "1";
            std::string anchor = Trimmed(op.field[1]);
            if (anchor != "end" && TokenOf(anchor).empty())
                return Fail(d, "insert: anchor must be a complete $TOKEN line or 'end'");
            if (op.after && anchor == "end")
                return Fail(d, "insert: nothing can follow the final end; use position 0");
            if (!ValidateSingleLine(op.field[2], false, true, &why))
                return Fail(d, "insert (new line): " + why);
            if (TokenOf(op.field[2]) == "$COST_RESOURCE_AUTO" &&
                TokenOf(anchor) != "$COST_WORK")
                return Fail(d, "insert: $COST_RESOURCE_AUTO requires a $COST_WORK phase anchor");
            int at = -1;
            int count = CountLine(lines, anchor, &at);
            if (count == 0)
            {
                // The anchor may be a line an earlier command of this section
                // produces (add, insert, replace). The new line is then placed
                // next to that produced line in the output, see ApplyOperations.
                int producer = -1;
                for (int pi = 0; pi < oi && producer < 0; pi++)
                {
                    const Operation& other = d->operations[pi];
                    if (other.kind == OP_ADD && Trimmed(other.field[0]) == anchor) producer = pi;
                    else if (other.kind == OP_INSERT && Trimmed(other.field[2]) == anchor) producer = pi;
                    else if (other.kind == OP_REPLACE && Trimmed(other.field[1]) == anchor) producer = pi;
                }
                if (producer < 0)
                    return Fail(d, "insert: anchor has 0 matches in the original and no earlier add, insert or replace of this section produces it");
                for (int pi = 0; pi < oi; pi++)
                {
                    const Operation& other = d->operations[pi];
                    if (other.kind == OP_INSERT && other.anchorOp == producer && other.after == op.after &&
                        Trimmed(other.field[2]) == Trimmed(op.field[2]))
                        return Fail(d, "insert: the same line is specified more than once for this anchor");
                }
                op.anchorOp = producer;
                op.insertAt = -1;
                op.anchorAt = -1;
            }
            else
            {
                if (count != 1)
                    return Fail(d, "insert: anchor has " + std::to_string(count) +
                        " matches instead of exactly one");
                const int slot = op.after ? at + 1 : at;
                const int neighbour = op.after ? at + 1 : at - 1;
                if (neighbour >= 0 && neighbour < (int)lines.size() &&
                    Trimmed(lines[neighbour]) == Trimmed(op.field[2]))
                    return Fail(d, op.after
                        ? "insert: the new line already stands directly after the anchor"
                        : "insert: the new line already stands directly before the anchor");
                for (int pi = 0; pi < (int)plannedInsert.size(); pi++)
                    if (plannedInsert[pi].first == slot && plannedInsert[pi].second == Trimmed(op.field[2]))
                        return Fail(d, "insert: the same line is specified more than once for this anchor");
                plannedInsert.push_back(std::make_pair(slot, Trimmed(op.field[2])));
                op.insertAt = slot;
                op.anchorAt = at;
            }
        }

        if (op.matchStart >= 0)
        {
            for (int ci = 0; ci < (int)changed.size(); ci++)
                if (IntervalsOverlap(op.matchStart, op.matchEnd, changed[ci].first, changed[ci].second))
                    return Fail(d, std::string(OpName(op.kind)) + ": overlaps another change");
            changed.push_back(std::make_pair(op.matchStart, op.matchEnd));
        }
    }

    for (int oi = 0; oi < (int)d->operations.size(); oi++)
    {
        Operation& op = d->operations[oi];
        d->errorContext = RuleContext(op, oi);
        if (op.kind == OP_ADD || op.kind == OP_INSERT)
        {
            const std::string value = Trimmed(op.field[op.kind == OP_ADD ? 0 : 2]);
            for (int previous = 0; previous < oi; ++previous)
            {
                const Operation& other = d->operations[previous];
                if ((other.kind == OP_ADD || other.kind == OP_INSERT) &&
                    op.insertAt >= 0 && other.insertAt == op.insertAt &&
                    Trimmed(other.field[other.kind == OP_ADD ? 0 : 2]) == value)
                    return Fail(d, "the same line would be inserted twice at one position");
            }
        }
        if (op.kind != OP_INSERT) continue;
        for (int ci = 0; ci < (int)changed.size(); ci++)
            if (op.anchorAt >= changed[ci].first && op.anchorAt <= changed[ci].second)
                return Fail(d, "insert: the anchor is modified or removed by another rule");
    }
    d->errorContext.clear();
    return true;
}

// ------------------------------------------------------------- transformation

static std::string ApplyOperations(const Decl& d, const std::vector<std::string>& lines, bool hadBom)
{
    // Every output line remembers the operation that produced it (-1 = an
    // original line), so an insert whose anchor is itself a produced line can
    // be spliced next to it afterwards.
    std::vector<std::pair<int, std::string> > emitted;
    emitted.reserve(lines.size() + d.operations.size() * 3);

    for (int i = 0; i < (int)lines.size(); )
    {
        // Insertions at one position retain their order in the INI section.
        // Lines that follow an anchor (insert = 1) come first, so they stay
        // adjacent to it when additions or before-anchor lines share the slot.
        for (int pass = 0; pass < 2; pass++)
        for (int oi = 0; oi < (int)d.operations.size(); oi++)
        {
            const Operation& op = d.operations[oi];
            if (op.insertAt != i) continue;
            const bool followsAnchor = op.kind == OP_INSERT && op.after;
            if (followsAnchor != (pass == 0)) continue;
            if (op.kind == OP_ADD || op.kind == OP_INSERT)
                emitted.push_back(std::make_pair(oi, Trimmed(op.field[op.kind == OP_INSERT ? 2 : 0])));
            else if (op.kind == OP_ADD_CONNECTION && op.points == 1)
                emitted.push_back(std::make_pair(oi, Trimmed(op.field[0]) + " " + Trimmed(op.field[1])));
            else if (op.kind == OP_ADD_CONNECTION)
            {
                emitted.push_back(std::make_pair(oi, Trimmed(op.field[0])));
                emitted.push_back(std::make_pair(oi, Trimmed(op.field[1])));
                emitted.push_back(std::make_pair(oi, Trimmed(op.field[2])));
            }
        }

        const Operation* action = NULL;
        int actionIndex = -1;
        for (int oi = 0; oi < (int)d.operations.size(); oi++)
            if (d.operations[oi].matchStart == i) { action = &d.operations[oi]; actionIndex = oi; break; }

        if (!action)
        {
            emitted.push_back(std::make_pair(-1, lines[i]));
            i++;
            continue;
        }

        if (action->kind == OP_REPLACE)
            emitted.push_back(std::make_pair(actionIndex, Trimmed(action->field[1])));
        else if (action->kind == OP_REPLACE_CONNECTION && action->points == 1)
        {
            // One-point entry: the result is always written inline, "$TOKEN x y z".
            std::string point = action->field.size() == 4 ? Trimmed(action->field[3]) :
                action->matchEnd > i ? Trimmed(lines[i + 1]) : Trimmed(Trimmed(lines[i]).substr(TokenOf(lines[i]).size()));
            emitted.push_back(std::make_pair(actionIndex, Trimmed(action->field[2]) + " " + point));
        }
        else if (action->kind == OP_REPLACE_CONNECTION)
        {
            emitted.push_back(std::make_pair(actionIndex, Trimmed(action->field[3])));
            // Optional new coordinates: fields 4 and 5 replace the two point lines.
            bool moves = action->field.size() == 6;
            emitted.push_back(std::make_pair(-1, moves ? Trimmed(action->field[4]) : lines[i + 1]));
            emitted.push_back(std::make_pair(-1, moves ? Trimmed(action->field[5]) : lines[i + 2]));
        }
        // remove and remove_connection deliberately emit nothing.
        i = action->matchEnd + 1;
    }

    // Inserts anchored on a produced line, in INI order so a chain of them works.
    for (int oi = 0; oi < (int)d.operations.size(); oi++)
    {
        const Operation& op = d.operations[oi];
        if (op.kind != OP_INSERT || op.anchorOp < 0) continue;
        for (size_t k = 0; k < emitted.size(); k++)
        {
            if (emitted[k].first != op.anchorOp) continue;
            emitted.insert(emitted.begin() + (op.after ? k + 1 : k), std::make_pair(oi, Trimmed(op.field[2])));
            break;
        }
    }

    std::string out;
    size_t estimated = 0;
    for (size_t k = 0; k < emitted.size(); k++) estimated += emitted[k].second.size() + 2;
    out.reserve(estimated + (hadBom ? 3 : 0));
    if (hadBom) out.append("\xef\xbb\xbf", 3);
    for (size_t k = 0; k < emitted.size(); k++)
    {
        out += emitted[k].second;
        out += "\r\n";
    }
    return out;
}

// --------------------------------------------------------------- INI parser

static bool AddOperation(Decl* d, const std::string& key, const std::string& value, size_t configLine = 0)
{
    Operation op;
    op.configLine = configLine;
    int expected = 0;
    if (key == "replace") { op.kind = OP_REPLACE; expected = 2; }
    else if (key == "replace_connection") { op.kind = OP_REPLACE_CONNECTION; expected = 4; }
    else if (key == "remove") { op.kind = OP_REMOVE; expected = 1; }
    else if (key == "remove_connection") { op.kind = OP_REMOVE_CONNECTION; expected = 3; }
    else if (key == "add") { op.kind = OP_ADD; expected = 1; }
    else if (key == "add_connection") { op.kind = OP_ADD_CONNECTION; expected = 3; }
    else if (key == "insert") { op.kind = OP_INSERT; expected = 3; }
    else if (key == "insert_before") { op.kind = OP_INSERT; expected = 2; }   // 1.3 spelling of "insert = 0 | ..."
    else return false;

    op.field = SplitFields(value);
    if (key == "insert_before" && op.field.size() == 2) { op.field.insert(op.field.begin(), "0"); expected = 3; }
    // replace_connection takes two more fields when the connection also moves:
    // old token | point 1 | point 2 | new token | new point 1 | new point 2
    if (key == "replace_connection" && op.field.size() == 6) expected = 6;
    // One-point connections ($CONNECTION_ROAD_DEAD x y z): remove/add take token | point,
    // replace takes old token | point | new token [| new point].
    if ((key == "remove_connection" || key == "add_connection") && op.field.size() == 2) expected = 2;
    if (key == "replace_connection" && op.field.size() == 3) expected = 3;
    if ((int)op.field.size() != expected)
    {
        Fail(d, key + ": expected " + std::to_string(expected) + " fields separated by |" +
            (key == "replace_connection" ? " (or 6 with new coordinates; one-point: 3 or 4)" :
             key == "remove_connection" || key == "add_connection" ? " (or 2 for a one-point connection)" : ""));
        return true;
    }
    for (int i = 0; i < expected; i++)
        if (op.field[i].empty())
        {
            Fail(d, key + ": a field is empty");
            return true;
        }
    if (d->operations.size() >= MAX_OPERATIONS)
    { Fail(d, "maximum operations per building exceeded"); return true; }
    for (const Operation& previous : d->operations)
        if (previous.kind == op.kind && previous.field == op.field)
        { Fail(d, key + ": identical command appears more than once"); return true; }
    d->operations.push_back(op);
    return true;
}

struct RegistryConfig
{
    std::vector<Decl> declarations;
    bool enabled = true;
    bool debug = false;
};

static bool ConfigError(size_t line, const char* rule, const std::string& message)
{
    Report("ERROR", PLUGIN_INI, rule, "Line %Iu: %s. Action: correct the configuration",
        line, message.c_str());
    return false;
}

static bool ParseBoolean(const std::string& value, bool* result)
{
    if (value != "0" && value != "1") return false;
    *result = value == "1";
    return true;
}

// Parse into a temporary registry. Global/layout errors reject the file;
// errors confined to one building reject that building, preserving siblings.
static bool ParseRegistryText(const std::string& text, RegistryConfig* output)
{
    if (text.size() > MAX_FILE_BYTES)
        return ConfigError(0, "config-size", "maximum INI size is 4 MiB");
    bool bom = false;
    std::vector<std::string> lines = SplitLines(text.data(), text.size(), &bom);
    if (bom)
        return ConfigError(1, "utf8-bom", "save the plugin INI as UTF-8 without BOM");
    if (!text.empty() && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), (int)text.size(), NULL, 0) <= 0)
        return ConfigError(0, "utf8", "configuration is not valid UTF-8");
    for (unsigned char c : text)
        if ((c < 32 && c != '\t' && c != '\r' && c != '\n') || c == 127)
            return ConfigError(0, "control-character", "configuration contains a control character");

    RegistryConfig parsed;
    std::map<std::string, size_t> buildingNames;
    std::set<std::string> globalKeys;
    std::set<std::string> buildingKeys;
    bool sawGeneral = false;
    bool inGeneral = false;
    Decl* current = NULL;

    for (size_t lineNo = 0; lineNo < lines.size(); ++lineNo)
    {
        std::string line = Trimmed(lines[lineNo]);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.find_first_not_of('-') == std::string::npos) continue;
        if (line.size() > MAX_CONFIG_LINE)
            return ConfigError(lineNo + 1, "line-limit", "maximum active line length is 4096 bytes");

        if (line[0] == '[')
        {
            if (line.size() < 3 || line.back() != ']' ||
                line.find(']') != line.size() - 1)
                return ConfigError(lineNo + 1, "section-header", "malformed section header");
            std::string section = Trimmed(line.substr(1, line.size() - 2));
            if (section.empty() || section.size() > 128 || section.find('[') != std::string::npos)
                return ConfigError(lineNo + 1, "section-name", "invalid or oversized section name");
            std::string canonical = Lower(section);
            current = NULL;
            buildingKeys.clear();
            inGeneral = canonical == "general" || canonical == "vanilla_buildings";
            if (inGeneral)
            {
                if (sawGeneral)
                    return ConfigError(lineNo + 1, "duplicate-general", "general settings appear more than once");
                sawGeneral = true;
                if (canonical == "vanilla_buildings")
                    Report("WARN", PLUGIN_INI, "legacy-section",
                        "[vanilla_buildings] remains supported; use [general] for new configurations");
            }
            else
            {
                if (parsed.declarations.size() >= MAX_BUILDINGS)
                    return ConfigError(lineNo + 1, "building-limit", "maximum building sections is 256");
                auto found = buildingNames.find(canonical);
                bool duplicate = found != buildingNames.end();
                if (duplicate) Fail(&parsed.declarations[found->second], "duplicate section name");
                else buildingNames[canonical] = parsed.declarations.size();
                parsed.declarations.push_back(Decl());
                current = &parsed.declarations.back();
                current->section = section;
                if (duplicate) Fail(current, "duplicate section name");
            }
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            if (current) { Fail(current, "line " + std::to_string(lineNo + 1) + ": missing '='"); continue; }
            return ConfigError(lineNo + 1, "missing-assignment", "expected key = value");
        }
        std::string key = Lower(Trimmed(line.substr(0, eq)));
        std::string value = Trimmed(line.substr(eq + 1));
        if (key.empty() || value.empty())
        {
            if (current) { Fail(current, "line " + std::to_string(lineNo + 1) + ": empty key or value"); continue; }
            return ConfigError(lineNo + 1, "missing-value", "empty key or value");
        }
        if (inGeneral)
        {
            if (key == "verbose")
            {
                Report("WARN", PLUGIN_INI, "legacy-key",
                    "verbose remains supported; use debug in new configurations");
                key = "debug";
            }
            if (key != "enabled" && key != "debug")
                return ConfigError(lineNo + 1, "unknown-key", "unknown general key: " + key);
            if (!globalKeys.insert(key).second)
                return ConfigError(lineNo + 1, "duplicate-key", "repeated general key: " + key);
            bool valueBool = false;
            if (!ParseBoolean(value, &valueBool))
                return ConfigError(lineNo + 1, "boolean", key + " must be exactly 0 or 1");
            if (key == "enabled") parsed.enabled = valueBool;
            else parsed.debug = valueBool;
            continue;
        }
        if (!current)
            return ConfigError(lineNo + 1, "outside-section", "assignment outside a section");
        bool targetKey = key == "target" || key == "target1" || key == "target2" || key == "target3";
        if (key == "name")
        {
            if (!buildingKeys.insert(key).second)
            { Fail(current, "name is specified more than once"); continue; }
            if (value.empty() || value.size() > 128 || value.find('"') != std::string::npos)
                Fail(current, "name must be plain text without quotes (at most 128 characters)");
            else current->name = value;
        }
        else if (key == "enabled" || targetKey)
        {
            if (key == "enabled")
            {
                if (!buildingKeys.insert(key).second)
                { Fail(current, key + " is specified more than once"); continue; }
                if (!ParseBoolean(value, &current->enabled))
                    Fail(current, "enabled must be exactly 0 or 1");
            }
            else
            {
                if (current->targets.size() >= MAX_TARGETS)
                    return ConfigError(lineNo + 1, "target-limit", "maximum targets is 256");
                current->targets.push_back(std::make_pair(key == "target" ? 0 : key.back() - '0', value));
            }
        }
        else
        {
            current->errorContext = "ini_line=" + std::to_string(lineNo + 1) + " rule={" + key + " = " + value + "}";
            if (!AddOperation(current, key, value, lineNo + 1))
                Fail(current, "unknown key: " + key);
            current->errorContext.clear();
        }
    }
    if (!sawGeneral)
        return ConfigError(0, "missing-general", "required [general] section is missing");
    // Each target gets a private operation list: match positions must never leak
    // from one source file into the next. A source-specific failure affects only it.
    std::vector<Decl> expanded;
    for (const Decl& group : parsed.declarations)
    {
        size_t count = group.targets.empty() ? 1 : group.targets.size();
        if (expanded.size() + count > MAX_TARGETS)
            return ConfigError(0, "target-limit", "maximum total target files is 256");
        if (group.targets.empty()) expanded.push_back(group);
        else for (const auto& target : group.targets)
        {
            Decl d = group;
            d.targets.clear();
            d.targetKind = target.first;
            d.target = target.second;
            expanded.push_back(std::move(d));
        }
    }
    parsed.declarations.swap(expanded);
    *output = std::move(parsed);
    return true;
}

static bool LoadRegistry(const char* path)
{
    std::vector<char> raw;
    if (!ReadWholeFile(path, &raw))
    {
        ReportWindows("ERROR", path, "config-read", "Plugin INI could not be read",
            g_lastWindowsError, "Verify that the file exists, is readable, and does not exceed 4 MiB");
        return false;
    }
    RegistryConfig config;
    if (!ParseRegistryText(std::string(raw.begin(), raw.end()), &config)) return false;
    g_decl.swap(config.declarations);
    g_enabled = config.enabled;
    g_debug = config.debug;
    return true;
}

// ---------------------------------------------------------- overlay creation

static bool MakeDirectory(const char* path)
{
    if (CreateDirectoryA(path, NULL)) return true;
    DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS)
    {
        DWORD attr = GetFileAttributesA(path);
        if (attr == INVALID_FILE_ATTRIBUTES) error = GetLastError();
        else if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) error = ERROR_DIRECTORY;
        else if (attr & FILE_ATTRIBUTE_REPARSE_POINT) error = ERROR_ACCESS_DENIED;
        else return true;
    }
    g_lastWindowsError = error;
    return false;
}

static bool PrepareTempDir(char* out, size_t outSize)
{
    char temp[MAX_PATH];
    DWORD length = GetTempPathA(sizeof(temp), temp);
    if (!length || length >= sizeof(temp))
    {
        g_lastWindowsError = length ? ERROR_INSUFFICIENT_BUFFER : GetLastError();
        return false;
    }
    char parent[MAX_PATH], plugin[MAX_PATH];
    if (_snprintf_s(parent, sizeof(parent), _TRUNCATE, "%sTesmioLoader", temp) < 0 ||
        _snprintf_s(plugin, sizeof(plugin), _TRUNCATE, "%s\\vanilla_buildings", parent) < 0)
    { g_lastWindowsError = ERROR_INSUFFICIENT_BUFFER; return false; }
    if (!MakeDirectory(parent) || !MakeDirectory(plugin)) return false;

    // Never adopt an existing process directory: Windows can reuse process IDs.
    ULONGLONG stamp = GetTickCount64();
    for (unsigned attempt = 0; attempt < 64; ++attempt)
    {
        if (_snprintf_s(out, outSize, _TRUNCATE, "%s\\%lu-%llu-%u", plugin,
            GetCurrentProcessId(), (unsigned long long)stamp, attempt) < 0)
        { g_lastWindowsError = ERROR_INSUFFICIENT_BUFFER; return false; }
        if (CreateDirectoryA(out, NULL)) return true;
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        { g_lastWindowsError = error; return false; }
    }
    g_lastWindowsError = ERROR_ALREADY_EXISTS;
    return false;
}

// Reject every enabled declaration of an ambiguous target, not just the later
// one. Configuration order must never decide which building patch wins.
static void ValidateTargets(std::vector<Decl>* declarations)
{
    std::map<std::string, size_t> targets;
    for (size_t i = 0; i < declarations->size(); ++i)
    {
        Decl& d = (*declarations)[i];
        if (!d.enabled) continue;
        std::string why;
        if (!CanonicalTarget(d.target, &d.canonical, &why))
        { Fail(&d, why); continue; }
        int actualKind = WorkshopTarget(d.canonical) ? 3 :
            (StartsWith(d.canonical, "buildings_types\\") ? 1 : 2);
        if (d.targetKind && d.targetKind != actualKind)
        { Fail(&d, "target1 requires buildings_types; target2 requires dlcN/buildings; target3 requires a Workshop item ID"); continue; }
        auto previous = targets.find(d.canonical);
        if (previous != targets.end())
        {
            Decl& other = (*declarations)[previous->second];
            Fail(&other, "duplicate target; also declared in section [" + d.section + "] as " + d.target);
            Fail(&d, "duplicate target; also declared in section [" + other.section + "] as " + other.target);
        }
        else targets[d.canonical] = i;
        if (d.operations.empty()) Fail(&d, "no patch commands specified");
    }
}

// ---------------------------------------------------------------- the name

// A name that is a localisation key rather than a caption: at least one dot and
// nothing but the characters a key may carry, so a caption with spaces can
// never be mistaken for one.
static bool LooksLikeKey(const std::string& s)
{
    if (s.empty() || s.find('.') == std::string::npos) return false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return s[0] != '.' && s[s.size() - 1] != '.';
}

static std::string KeyTail(const std::string& s)
{
    size_t dot = s.rfind('.');
    return dot == std::string::npos ? s : s.substr(dot + 1);
}

// The one line `name =` produces: $NAME with a resolved id for a localisation
// key, $NAME_STR with a literal otherwise. An unresolved key falls back to the
// part after the last dot, so a missing text never leaves a building nameless.
static std::string NameLineFor(const Decl& d)
{
    if (!LooksLikeKey(d.name)) return "$NAME_STR \"" + d.name + "\"";
    int id = 0;
    if (g_localization && g_localization->resolveFull) id = g_localization->resolveFull(d.name.c_str());
    if (id >= 2000000 && id <= 2999999) return "$NAME " + std::to_string(id);
    Report("WARN", d.section.c_str(), "name-key",
        "[%s] name '%s' did not resolve%s - using \"%s\"", d.section.c_str(), d.name.c_str(),
        g_localization ? "" : " (Localization is not loaded, or it loads after this plugin)",
        KeyTail(d.name).c_str());
    return "$NAME_STR \"" + KeyTail(d.name) + "\"";
}

// `name =` becomes an ordinary replace of the single $NAME or $NAME_STR line the
// target carries. Going through the normal operation list keeps every existing
// guarantee: the unique-match rule, overlap detection against your own rules and
// the per-target error reporting. The caller never has to know the old line.
static bool ExpandNameOperation(Decl* d, const std::vector<std::string>& lines)
{
    if (d->nameExpanded) return true;
    if (d->nameLine.empty()) d->nameLine = NameLineFor(*d);
    int found = -1, count = 0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string token = TokenOf(lines[i]);
        if (token == "$NAME" || token == "$NAME_STR") { ++count; if (found < 0) found = (int)i; }
    }
    if (count != 1)
    {
        Fail(d, "name: the target has " + std::to_string(count) + " $NAME lines instead of exactly one");
        return false;
    }
    d->nameExpanded = true;
    if (Trimmed(lines[found]) == Trimmed(d->nameLine)) return true;   // already exactly this name
    if (d->operations.size() >= MAX_OPERATIONS)
    {
        Fail(d, "name: this section already has the maximum number of rules");
        return false;
    }
    Operation op;
    op.kind = OP_REPLACE;
    op.field.push_back(Trimmed(lines[found]));
    op.field.push_back(d->nameLine);
    d->operations.insert(d->operations.begin(), op);
    return true;
}

static bool BuildOverlay(Decl* d, const char* tempDir, size_t index)
{
    const char* root = WorkshopTarget(d->canonical) ? g_workshopDir : g_mediaDir;
    if (!root[0])
    {
        Fail(d, "Steam Workshop root could not be resolved from the game's Steam library");
        Report("ERROR", d->section.c_str(), "workshop-root", "%s", d->error.c_str());
        return false;
    }
    d->sourcePath = std::string(root) + "\\" + d->canonical;
    std::vector<char> raw;
    if (!ReadWholeFile(d->sourcePath.c_str(), &raw))
    {
        Fail(d, "could not read building source file: " + d->sourcePath + " (Windows error " + std::to_string(g_lastWindowsError) + ")");
        ReportWindows("ERROR", d->sourcePath.c_str(), "source-read",
            "Building source file could not be read", g_lastWindowsError,
            "Verify the target path and that the DLC or Workshop item is installed in this Steam library");
        return false;
    }
    if (std::find(raw.begin(), raw.end(), '\0') != raw.end())
    {
        Fail(d, "source file contains a NUL character");
        Report("ERROR", d->sourcePath.c_str(), "source-format",
            "Source is not a plain text building INI");
        return false;
    }
    bool bom = false;
    std::vector<std::string> lines = SplitLines(raw.empty() ? "" : raw.data(), raw.size(), &bom);
    if (!d->name.empty() && !ExpandNameOperation(d, lines))
    {
        Report("ERROR", PLUGIN_INI, "name-target",
            "[%s] target=%s was rejected: %s. Action: check the target's name line",
            d->section.c_str(), d->target.c_str(), d->error.c_str());
        return false;
    }
    if (!ValidateOperations(d, lines))
    {
        Report("ERROR", PLUGIN_INI, "rules-invalid",
                "[%s] target=%s was rejected: %s. No rule for this target was applied. Action: correct the patch rules",
            d->section.c_str(), d->canonical.c_str(), d->error.c_str());
        return false;
    }

    std::string overlay = std::string(tempDir) + "\\" + std::to_string(index + 1) + ".ini";
    std::string patched = ApplyOperations(*d, lines, bom);
    if (!WriteWholeFile(overlay.c_str(), patched))
    {
        Fail(d, "could not write temporary patch file");
        ReportWindows("ERROR", overlay.c_str(), "overlay-write",
            "Temporary overlay file could not be written", g_lastWindowsError,
            "Verify temporary-directory access, free space and the 8 MiB output limit");
        return false;
    }
    // Publish the path only after the complete file has been installed.
    d->overlayPath = overlay;
    d->overlayWide = PathToWide(overlay.c_str());
    d->openedLogged = 0;
    Info("[%s] %Iu rule(s) -> %s", d->section.c_str(),
        d->operations.size(), d->canonical.c_str());
    return true;
}

static int g_activeSectionCount;

static bool BuildOverlays(void)
{
    g_enabledSectionCount = 0;
    g_activeSectionCount = 0;
    for (const Decl& d : g_decl)
        if (d.enabled) ++g_enabledSectionCount;
    if (!g_enabledSectionCount) return false;
    ValidateTargets(&g_decl);

    char tempDir[MAX_PATH];
    if (!PrepareTempDir(tempDir, sizeof(tempDir)))
    {
        ReportWindows("FATAL", "temporary directory", "temp-directory",
            "Could not create an exclusive temporary overlay directory", g_lastWindowsError,
            "Verify temporary-directory access, path length and available disk space");
        return false;
    }
    for (size_t i = 0; i < g_decl.size(); ++i)
    {
        Decl& d = g_decl[i];
        if (!d.enabled)
        {
            if (g_debug) Report("DEBUG", PLUGIN_INI, "section-disabled",
                "[%s] is disabled", d.section.c_str());
            continue;
        }
        if (!d.valid)
        {
            Report("WARN", PLUGIN_INI, "target-rejected",
                "[%s] target=%s was rejected: %s. No rule for this target was applied. Action: correct this target or its shared rules",
                d.section.c_str(), d.target.c_str(), d.error.c_str());
            continue;
        }
        if (BuildOverlay(&d, tempDir, i)) ++g_activeSectionCount;
        else Report("WARN", d.section.c_str(), "target-rejected",
            "target=%s reason=%s; no rule for this target was applied; other valid targets continue",
            d.target.c_str(), d.error.c_str());
    }
    Info("Buildings: %d enabled, %d active, %d rejected",
        g_enabledSectionCount, g_activeSectionCount,
        g_enabledSectionCount - g_activeSectionCount);
    return g_activeSectionCount > 0;
}

// ------------------------------------------------------------------- redirect

static std::string RelativeUnder(const std::string& path, const char* root)
{
    size_t length = strlen(root);
    if (length && path.size() > length && path.compare(0, length, root) == 0 &&
        path[length] == '\\') return path.substr(length + 1);
    return std::string();
}

static std::string RequestCanonical(const char* path)
{
    if (!path || !*path) return std::string();
    std::string s = SlashesLower(path);
    while (StartsWith(s, ".\\")) s.erase(0, 2);
    if (StartsWith(s, "\\\\?\\unc\\")) s = "\\\\" + s.substr(8);
    else if (StartsWith(s, "\\\\?\\")) s.erase(0, 4);

    std::string canonical, why;
    std::string relative = RelativeUnder(s, g_mediaCanonical);
    if (!relative.empty())
        return CanonicalTarget(relative, &canonical, &why) && !WorkshopTarget(canonical)
            ? canonical : std::string();
    relative = RelativeUnder(s, g_workshopCanonical);
    if (!relative.empty())
        return CanonicalTarget(relative, &canonical, &why) && WorkshopTarget(canonical)
            ? canonical : std::string();
    if (StartsWith(s, "media_soviet\\"))
    {
        relative = s.substr(strlen("media_soviet\\"));
        return CanonicalTarget(relative, &canonical, &why) && !WorkshopTarget(canonical)
            ? canonical : std::string();
    }
    // Bare names are the documented engine-relative forms, not arbitrary suffix matches.
    if (CanonicalTarget(s, &canonical, &why)) return canonical;
    // The game also uses ../../workshop/content/784150/... from its own directory.
    // Resolve ONLY traversal-form requests; never reinterpret an unrelated absolute path.
    if (StartsWith(s, "..\\"))
    {
        char absolute[MAX_PATH * 2];
        DWORD n = GetFullPathNameA(path, (DWORD)sizeof(absolute), absolute, NULL);
        if (n && n < sizeof(absolute))
        {
            relative = RelativeUnder(SlashesLower(absolute), g_workshopCanonical);
            if (!relative.empty() && CanonicalTarget(relative, &canonical, &why) &&
                WorkshopTarget(canonical)) return canonical;
        }
    }
    return std::string();
}

static Decl* FindOverlay(const char* path)
{
    std::string canonical = RequestCanonical(path);
    if (canonical.empty()) return NULL;
    for (Decl& d : g_decl)
        if (d.enabled && d.valid && !d.overlayPath.empty() && canonical == d.canonical)
            return &d;
    return NULL;
}

static Decl* SelectOverlay(const char* path)
{
    if (InterlockedExchangeAdd(&g_redirectEnabled, 0))
    {
        try
        {
            Decl* d = FindOverlay(path);
            if (d)
            {
                DWORD attr = GetFileAttributesA(d->overlayPath.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    return d;
                }
                else if (InterlockedCompareExchange(&d->unavailableLogged, 1, 0) == 0)
                    Report("WARN", d->section.c_str(), "overlay-unavailable",
                        "target=%s temporary_file=%s is missing or is not a file; using original request; other targets remain active",
                        d->target.c_str(), d->overlayPath.c_str());
            }
        }
        catch (...)
        {
            InterlockedExchange(&g_redirectEnabled, 0);
            if (InterlockedCompareExchange(&g_runtimeErrorLogged, 1, 0) == 0)
                Report("ERROR", PLUGIN_INI, "redirect-exception",
                    "Exception while selecting an overlay; redirects disabled for this session");
        }
    }
    return NULL;
}

static void OverlayOpened(Decl* d, const char* reader)
{
    if (!d) return;
    DWORD savedError = GetLastError();
    bool first = InterlockedCompareExchange(&d->openedLogged, 1, 0) == 0;
    if (first || g_debug)
        Report(first ? "INFO" : "DEBUG", d->section.c_str(), "overlay-opened",
            "reader=%s target=%s source=%s actual_open=1 original_unchanged=1",
            reader, d->canonical.c_str(), d->sourcePath.c_str());
    SetLastError(savedError);
}

template<typename Char> static bool ReadOnlyMode(const Char* mode)
{
    if (!mode || mode[0] != (Char)'r') return false;
    for (const Char* p = mode; *p; ++p)
        if (*p == (Char)'+') return false;
    return true;
}

static Decl* SelectWideOverlay(const wchar_t* path)
{
    if (!path || !InterlockedExchangeAdd(&g_redirectEnabled, 0)) return NULL;
    // Match the encoding of the game's ANSI paths without best-fit aliasing.
    char narrow[MAX_PATH * 2];
    int n = WideCharToMultiByte(CP_ACP, 0, path, -1, narrow, (int)sizeof(narrow), NULL, NULL);
    if (!n) return NULL;
    try
    {
        if (PathToWide(narrow) != path) return NULL;
        Decl* d = SelectOverlay(narrow);
        return d && !d->overlayWide.empty() ? d : NULL;
    }
    catch (...) { return NULL; }
}

static int __cdecl h_ReadFileIntoBuffer(const char* path, char** buf, unsigned int* size, bool flag)
{
    Decl* d = SelectOverlay(path);
    int result = o_ReadFile(d ? d->overlayPath.c_str() : path, buf, size, flag);
    if (result && buf && *buf) OverlayOpened(d, "ReadFileIntoBuffer");
    return result;
}

static FILE* __cdecl h_Fopen(const char* path, const char* mode)
{
    Decl* d = ReadOnlyMode(mode) ? SelectOverlay(path) : NULL;
    FILE* result = o_Fopen(d ? d->overlayPath.c_str() : path, mode);
    if (result) OverlayOpened(d, "fopen");
    return result;
}

static errno_t __cdecl h_FopenS(FILE** stream, const char* path, const char* mode)
{
    Decl* d = stream && ReadOnlyMode(mode) ? SelectOverlay(path) : NULL;
    errno_t result = o_FopenS(stream, d ? d->overlayPath.c_str() : path, mode);
    if (!result && stream && *stream) OverlayOpened(d, "fopen_s");
    return result;
}

static FILE* __cdecl h_Wfopen(const wchar_t* path, const wchar_t* mode)
{
    Decl* d = ReadOnlyMode(mode) ? SelectWideOverlay(path) : NULL;
    FILE* result = o_Wfopen(d ? d->overlayWide.c_str() : path, mode);
    if (result) OverlayOpened(d, "_wfopen");
    return result;
}

static errno_t __cdecl h_WfopenS(FILE** stream, const wchar_t* path, const wchar_t* mode)
{
    Decl* d = stream && ReadOnlyMode(mode) ? SelectWideOverlay(path) : NULL;
    errno_t result = o_WfopenS(stream, d ? d->overlayWide.c_str() : path, mode);
    if (!result && stream && *stream) OverlayOpened(d, "_wfopen_s");
    return result;
}

// -------------------------------------------------------------------- exports

static bool HostIsUsable(const TsmHost* host)
{
    return host && host->structSize >= offsetof(TsmHost, consume) + sizeof(host->consume) &&
        host->exeModule && host->baseDir && host->baseDir[0] &&
        host->pluginDir && host->pluginDir[0] && host->log &&
        host->findIatSlot && host->patchIat && host->readablePtr;
}

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion(void)
{
    return TSM_API_VERSION;
}

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host, TsmPluginInfo* info)
{
    if (!info || !HostIsUsable(host)) return 1;
    TsmBind(host);
    g_bound = true;
    info->name = "vanilla_buildings";
    info->version = PLUGIN_VERSION;
    BeginLogPhase();
    try
    {
        if (strlen(g_baseDir) + 1 + strlen(PLUGIN_LOG_NAME) >= MAX_PATH)
            Report("WARN", PLUGIN_LOG_NAME, "log-path", "Detail log path exceeds the supported length");
        else
        {
            g_detail = TsmOpenLog(PLUGIN_LOG_NAME);
            if (g_detail == INVALID_HANDLE_VALUE)
                ReportWindows("WARN", PLUGIN_LOG_NAME, "log-open",
                    "Detail log could not be opened", GetLastError(),
                    "Verify write access to the TesmioLoader directory");
        }
        Info("TesmioLoader vanilla_buildings %s starting; API=%u; detail log: %s",
            PLUGIN_VERSION, TSM_API_VERSION, PLUGIN_LOG_NAME);

        // 1.3: <loader>\plugins\vanilla_buildings.ini when it exists (the classic
        // install, or the effective INI Republic Mod Manager writes), otherwise
        // the INI beside the DLL - the Workshop package under Soviet Mod Loader
        // or the Workshop Bridge.
        std::string ini = std::string(host->pluginDir) + "\\vanilla_buildings.ini";
        if (GetFileAttributesA(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            static const char anchor = 0;
            HMODULE self = NULL;
            char own[MAX_PATH];
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &anchor, &self) &&
                GetModuleFileNameA(self, own, (DWORD)sizeof(own)) > 0)
            {
                char* slash = strrchr(own, '\\');
                if (slash)
                {
                    *slash = 0;
                    std::string beside = std::string(own) + "\\vanilla_buildings.ini";
                    if (GetFileAttributesA(beside.c_str()) != INVALID_FILE_ATTRIBUTES) ini = beside;
                }
            }
        }
        Info("Configuration file: %s", ini.c_str());
        if (!LoadRegistry(ini.c_str()))
        {
            Report("FATAL", PLUGIN_INI, "configuration",
                "Configuration was rejected. Action: correct the preceding error; no hook was installed");
            LogSummary("Initialization", false);
            return 1;
        }
        Info("Configuration: enabled=%d debug=%d target_files=%Iu",
            g_enabled ? 1 : 0, g_debug ? 1 : 0, g_decl.size());
        if (!g_enabled)
        {
            LogSummaryStatus("Initialization", "was skipped because the plugin is disabled");
            return 1;
        }
        if (!ResolveGameDir())
        {
            ReportWindows("FATAL", "media_soviet", "game-folder",
                "Game data directory could not be resolved", g_lastWindowsError,
                "Verify the selected game directory and path length");
            LogSummary("Initialization", false);
            return 1;
        }
        // Optional and never fatal: a `name =` with dots is resolved through it.
        // The Workshop Bridge loads Localization before this plugin, so the
        // service is already on the noticeboard here.
        if (host->consume)
            g_localization = (const TsmLocalizationApi*)host->consume(TSM_SERVICE_LOCALIZATION,
                                                                     TSM_LOCALIZATION_VERSION);
        if (!BuildOverlays())
        {
            if (!g_enabledSectionCount)
            {
                Info("No building section is enabled; original requests remain unchanged");
                LogSummaryStatus("Initialization", "was skipped because no building section is enabled");
            }
            else
            {
                Report("FATAL", PLUGIN_INI, "no-active-section",
                    "No valid active building section remains. Action: correct the preceding section errors");
                LogSummary("Initialization", false);
            }
            return 1;
        }
        g_initialized = true;
        if (g_activeSectionCount < g_enabledSectionCount)
            LogSummaryStatus("Initialization", "completed with rejected building sections");
        else LogSummary("Initialization", true);
        return 0;
    }
    catch (const std::exception& e)
    {
        Report("FATAL", PLUGIN_INI, "initialization-exception",
            "C++ exception: %s. Action: preserve the log; no hook was installed", e.what());
    }
    catch (...)
    {
        Report("FATAL", PLUGIN_INI, "initialization-exception",
            "Unknown C++ exception; no hook was installed");
    }
    LogSummary("Initialization", false);
    return 1;
}

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    BeginLogPhase();
    try
    {
        if (!H || !g_bound || !g_initialized || !g_activeSectionCount)
        {
            Report("FATAL", PLUGIN_INI, "startup-state", "No initialized overlays are available");
            LogSummary("Startup", false);
            return 1;
        }
        if (g_hooksInstalled) return 0;
        // Building definitions use CRT fopen. Validate it before installing anything.
        void** required = FindIatSlot(g_exe, DLL_STDIO, "fopen");
        if (!required || !ReadablePtr(required, sizeof(*required)) || !*required ||
            *required == (void*)h_Fopen)
        {
            Report("FATAL", "SOVIET64.exe", "import-validation",
                "No usable fopen import is available; building redirects cannot work. Action: verify loader and game compatibility");
            LogSummary("Startup", false);
            return 1;
        }
        struct Hook {
            const char* dll;
            const char* symbol;
            void* detour;
            void** original;
            const char* label;
            bool required;
        };
        Hook hooks[] = {
            { DLL_STDIO, "fopen", (void*)h_Fopen, (void**)&o_Fopen, "vanilla_buildings fopen", true },
            { DLL_STDIO, "fopen_s", (void*)h_FopenS, (void**)&o_FopenS, "vanilla_buildings fopen_s", false },
            { DLL_STDIO, "_wfopen", (void*)h_Wfopen, (void**)&o_Wfopen, "vanilla_buildings _wfopen", false },
            { DLL_STDIO, "_wfopen_s", (void*)h_WfopenS, (void**)&o_WfopenS, "vanilla_buildings _wfopen_s", false },
            { DLL_ENGINE, SYM_READ_FILE, (void*)h_ReadFileIntoBuffer, (void**)&o_ReadFile,
                "vanilla_buildings ReadFileIntoBuffer", false }
        };
        size_t installed = 0;
        for (const Hook& hook : hooks)
        {
            void** slot = FindIatSlot(g_exe, hook.dll, hook.symbol);
            if (!slot && !hook.required) continue;
            if (!slot || !ReadablePtr(slot, sizeof(*slot)) || !*slot || *slot == hook.detour ||
                !PatchIat(g_exe, hook.dll, hook.symbol, hook.detour, hook.original, hook.label))
            {
                InterlockedExchange(&g_redirectEnabled, 0);
                Report("ERROR", hook.label, "hook-install",
                    "File-reader hook failed; redirects disabled. Action: inspect the loader log");
                LogSummary("Startup", false);
                // Previous successful patches still point into this DLL: keep it loaded.
                return g_hooksInstalled ? 0 : 1;
            }
            g_hooksInstalled = true;
            ++installed;
        }
        // A successful IAT hook must never be unloaded while the process lives.
        Info("Read-only redirects installed for %d building(s), readers=%Iu; actual usage is reported by [overlay-opened]; originals remain unchanged",
            g_activeSectionCount, installed);
        LogSummary("Startup", true);
        return 0;
    }
    catch (...)
    {
        InterlockedExchange(&g_redirectEnabled, 0);
        Report("FATAL", PLUGIN_INI, "startup-exception",
            "C++ exception during startup; custom redirects are disabled");
        LogSummary("Startup", false);
        return g_hooksInstalled ? 0 : 1;
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH)
    {
        if (g_detail != INVALID_HANDLE_VALUE)
        { CloseHandle(g_detail); g_detail = INVALID_HANDLE_VALUE; }
        if (g_bound) DeleteCriticalSection(&g_lock);
    }
    return TRUE;
}

#ifdef VANILLA_BUILDINGS_TEST

static Operation TestOp(OpKind kind, const char* a, const char* b = NULL,
    const char* c = NULL, const char* e = NULL, const char* f = NULL, const char* g = NULL)
{
    Operation op;
    op.kind = kind;
    op.field.push_back(a);
    if (b) op.field.push_back(b);
    if (c) op.field.push_back(c);
    if (e) op.field.push_back(e);
    if (f) op.field.push_back(f);
    if (g) op.field.push_back(g);
    return op;
}

static bool Has(const std::string& text, const char* wanted)
{
    return text.find(wanted) != std::string::npos;
}

// A stand-in for the Localization plugin: exactly one key resolves.
static int TestResolveFull(const char* key)
{
    return (key && strcmp(key, "localization.lang.medicine_factory") == 0) ? 2000123 : 0;
}
static int TestResolve(const char*, const char*) { return 0; }
static const TsmLocalizationApi kTestLocalization = { TestResolve, TestResolveFull };

static int RunSelfTests(void)
{
    int failed = 0;
    const char* sample =
        "$NAME_STR sample\n"
        "$PRODUCTION plastic 0.11\n"
        "$CONSUMPTION chemicals 0.20\n"
        "$WORKERS_NEEDED 10\n"
        "$CONNECTION_CONNECTION\n"
        "-7 -3.0 33\n"
        "-7 -3.0 31\n"
        "$CONNECTION_PIPE_INPUT\n"
        "14.5000 0 -0.0000\n"
        "14.5 0 2\n"
        "$CONNECTION_ROAD_DEAD 14.5 0 4\n"
        "$CONNECTION_ROAD_DEAD\n"
        "14.5 0 6\n"
        "$COST_WORK SOVIET_CONSTRUCTION_GROUNDWORKS 1.0\n"
        "$COST_RESOURCE_AUTO ground_asphalt 1.0\n"
        "$COST_WORK SOVIET_CONSTRUCTION_BUILDING_NODE 1.0\n"
        "end\n";
    bool bom = false;
    std::vector<std::string> lines = SplitLines(sample, strlen(sample), &bom);

    Decl d;
    d.operations.push_back(TestOp(OP_REPLACE, "$PRODUCTION plastic 0.11", "$PRODUCTION glass 0.45"));
    d.operations.push_back(TestOp(OP_REPLACE_CONNECTION, "$CONNECTION_CONNECTION", "-7 -3 33", "-7 -3 31", "$CONNECTION_WATERPIPE_INPUT"));
    d.operations.push_back(TestOp(OP_REMOVE, "$CONSUMPTION chemicals 0.20"));
    d.operations.push_back(TestOp(OP_REMOVE_CONNECTION, "$CONNECTION_PIPE_INPUT", "14.5 0 0", "14.5 0 2"));
    d.operations.push_back(TestOp(OP_ADD, "$PRODUCTION alcohol 0.10"));
    d.operations.push_back(TestOp(OP_ADD, "$WORKERS_NEEDED 20"));
    // One-point connections: remove the inline one, replace and move the two-line one, add one.
    d.operations.push_back(TestOp(OP_REMOVE_CONNECTION, "$CONNECTION_ROAD_DEAD", "14.5 0 4"));
    d.operations.push_back(TestOp(OP_REPLACE_CONNECTION, "$CONNECTION_ROAD_DEAD", "14.5 0 6", "$CONNECTION_RAIL_DEAD", "16 0 6"));
    d.operations.push_back(TestOp(OP_ADD_CONNECTION, "$CONNECTION_ROAD_DEAD", "30 0 0"));
    d.operations.push_back(TestOp(OP_ADD_CONNECTION, "$CONNECTION_WATERPIPE_OUTPUT", "20 0 0", "22 0 0"));
    d.operations.push_back(TestOp(OP_INSERT, "0", "$COST_WORK SOVIET_CONSTRUCTION_BUILDING_NODE 1.0", "$COST_RESOURCE_AUTO steel 2.0"));
    d.operations.push_back(TestOp(OP_INSERT, "0", "$COST_WORK SOVIET_CONSTRUCTION_BUILDING_NODE 1.0", "$COST_RESOURCE_AUTO bricks 3.0"));
    d.operations.push_back(TestOp(OP_INSERT, "1", "$COST_WORK SOVIET_CONSTRUCTION_BUILDING_NODE 1.0", "$COST_RESOURCE_AUTO gravel 1.0"));
    d.operations.push_back(TestOp(OP_ADD, "$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 sand"));
    d.operations.push_back(TestOp(OP_INSERT, "1", "$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 sand", "$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 gravel"));
    d.operations.push_back(TestOp(OP_INSERT, "0", "$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 gravel", "$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 road_salt"));
    if (!ValidateOperations(&d, lines))
    {
        fprintf(stderr, "FATAL: self-test [valid-rules] %s. Action: correct the validator or test data\n",
            d.error.c_str());
        failed++;
    }
    else
    {
        std::string out = ApplyOperations(d, lines, false);
        if (!Has(out, "$PRODUCTION glass 0.45\r\n") ||
            Has(out, "$CONSUMPTION chemicals 0.20") ||
            !Has(out, "$CONNECTION_WATERPIPE_INPUT\r\n-7 -3.0 33\r\n-7 -3.0 31") ||
            Has(out, "$CONNECTION_PIPE_INPUT") ||
            !Has(out, "$CONNECTION_ROAD_DEAD 30 0 0\r\n$CONNECTION_WATERPIPE_OUTPUT\r\n20 0 0\r\n22 0 0\r\n$COST_WORK") ||
            Has(out, "$CONNECTION_ROAD_DEAD 14.5 0 4") || Has(out, "14.5 0 6") ||
            !Has(out, "-7 -3.0 31\r\n$CONNECTION_RAIL_DEAD 16 0 6\r\n$CONNECTION_ROAD_DEAD 30 0 0\r\n") ||
            !Has(out, "$COST_RESOURCE_AUTO steel 2.0\r\n$COST_RESOURCE_AUTO bricks 3.0\r\n$COST_WORK SOVIET_CONSTRUCTION_BUILDING_NODE 1.0\r\n$COST_RESOURCE_AUTO gravel 1.0\r\n") ||
            !Has(out, "$PRODUCTION alcohol 0.10\r\n$WORKERS_NEEDED 20\r\n$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 sand\r\n$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 road_salt\r\n$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 gravel\r\nend"))
        {
            fprintf(stderr, "FATAL: self-test [transformed-output] Output mismatch. "
                "Action: inspect the generated text below\n%s\n", out.c_str());
            failed++;
        }
    }

    // A one-point remove with a point nobody has is rejected; a two-point remove of a one-point
    // entry too (a dead end never matches the two-point form).
    Decl deadMissing;
    deadMissing.operations.push_back(TestOp(OP_REMOVE_CONNECTION, "$CONNECTION_ROAD_DEAD", "99 0 0"));
    if (ValidateOperations(&deadMissing, lines)) { fprintf(stderr,
        "FATAL: self-test [dead-missing] Unknown one-point connection was accepted. Action: correct validation\n"); failed++; }
    Decl deadTwoPoint;
    deadTwoPoint.operations.push_back(TestOp(OP_REMOVE_CONNECTION, "$CONNECTION_ROAD_DEAD", "14.5 0 4", "14.5 0 6"));
    if (ValidateOperations(&deadTwoPoint, lines)) { fprintf(stderr,
        "FATAL: self-test [dead-two-point] Two-point remove of a dead end was accepted. Action: correct validation\n"); failed++; }

    Decl ambiguous;
    ambiguous.operations.push_back(TestOp(OP_REMOVE, "$COST_WORK SOVIET_CONSTRUCTION_BUILDING_NODE 1.0"));
    if (ValidateOperations(&ambiguous, lines)) { fprintf(stderr,
        "FATAL: self-test [ambiguous-match] Ambiguous match was accepted. Action: correct validation\n"); failed++; }

    Decl duplicate;
    duplicate.operations.push_back(TestOp(OP_ADD, "$WORKERS_NEEDED 10"));
    if (ValidateOperations(&duplicate, lines)) { fprintf(stderr,
        "FATAL: self-test [duplicate-add] Duplicate add was accepted. Action: correct validation\n"); failed++; }

    Decl occupied;
    occupied.operations.push_back(TestOp(OP_ADD_CONNECTION, "$CONNECTION_WATERPIPE_INPUT", "-7 -3 33", "-8 -3 33"));
    if (ValidateOperations(&occupied, lines)) { fprintf(stderr,
        "FATAL: self-test [occupied-point] Occupied point was accepted. Action: correct validation\n"); failed++; }

    Decl allowpass;
    allowpass.operations.push_back(TestOp(OP_ADD_CONNECTION, "$CONNECTION_ROAD_ALLOWPASS", "30 0 0", "31 0 0"));
    if (ValidateOperations(&allowpass, lines)) { fprintf(stderr,
        "FATAL: self-test [allowpass] Invalid allowpass rule was accepted. Action: correct validation\n"); failed++; }

    // replace_connection with new coordinates: token and both point lines change.
    Decl moved;
    moved.operations.push_back(TestOp(OP_REPLACE_CONNECTION, "$CONNECTION_PIPE_INPUT", "14.5 0 0", "14.5 0 2",
        "$CONNECTION_WATERPIPE_OUTPUT", "14.5 -2.15 0", "14.5 -2.15 2"));
    if (!ValidateOperations(&moved, lines))
    {
        fprintf(stderr, "FATAL: self-test [moved-connection] %s. Action: correct the validator\n", moved.error.c_str());
        failed++;
    }
    else
    {
        std::string out = ApplyOperations(moved, lines, false);
        if (!Has(out, "$CONNECTION_WATERPIPE_OUTPUT\r\n14.5 -2.15 0\r\n14.5 -2.15 2\r\n$CONNECTION_ROAD_DEAD 14.5 0 4") ||
            Has(out, "$CONNECTION_PIPE_INPUT") || Has(out, "14.5000 0 -0.0000"))
        {
            fprintf(stderr, "FATAL: self-test [moved-connection-output] Output mismatch. "
                "Action: inspect the generated text below\n%s\n", out.c_str());
            failed++;
        }
    }
    // The same token with new coordinates is a plain move and stays allowed.
    Decl movedSame;
    movedSame.operations.push_back(TestOp(OP_REPLACE_CONNECTION, "$CONNECTION_PIPE_INPUT", "14.5 0 0", "14.5 0 2",
        "$CONNECTION_PIPE_INPUT", "14.5 -1 0", "14.5 -1 2"));
    if (!ValidateOperations(&movedSame, lines)) { fprintf(stderr,
        "FATAL: self-test [moved-same-token] %s. Action: correct the validator\n", movedSame.error.c_str()); failed++; }
    // New coordinates on a point another connection uses are rejected.
    Decl movedOccupied;
    movedOccupied.operations.push_back(TestOp(OP_REPLACE_CONNECTION, "$CONNECTION_PIPE_INPUT", "14.5 0 0", "14.5 0 2",
        "$CONNECTION_WATERPIPE_OUTPUT", "-7 -3 33", "14.5 -2 2"));
    if (ValidateOperations(&movedOccupied, lines)) { fprintf(stderr,
        "FATAL: self-test [moved-occupied] Occupied new point was accepted. Action: correct validation\n"); failed++; }

    // 0.4.4: `name =` replaces whatever $NAME line the target carries, without
    // the caller knowing the old line.
    Decl plainName;
    plainName.section = "name_plain";
    plainName.name = "Medicine Factory";
    if (!ExpandNameOperation(&plainName, lines) || !ValidateOperations(&plainName, lines))
    { fprintf(stderr, "FATAL: self-test [name-plain] %s. Action: correct the name expansion\n", plainName.error.c_str()); failed++; }
    else
    {
        std::string out = ApplyOperations(plainName, lines, false);
        if (!Has(out, "$NAME_STR \"Medicine Factory\"") || Has(out, "$NAME_STR sample"))
        { fprintf(stderr, "FATAL: self-test [name-plain-output] Output mismatch\n%s\n", out.c_str()); failed++; }
    }
    // A key is resolved through the service and becomes the numbered form.
    Decl keyName;
    keyName.section = "name_key";
    keyName.name = "localization.lang.medicine_factory";
    g_localization = &kTestLocalization;
    bool keyOk = ExpandNameOperation(&keyName, lines) && ValidateOperations(&keyName, lines);
    g_localization = NULL;
    if (!keyOk)
    { fprintf(stderr, "FATAL: self-test [name-key] %s. Action: correct the name expansion\n", keyName.error.c_str()); failed++; }
    else
    {
        std::string out = ApplyOperations(keyName, lines, false);
        if (!Has(out, "$NAME 2000123") || Has(out, "$NAME_STR") || Has(out, "localization.lang"))
        { fprintf(stderr, "FATAL: self-test [name-key-output] Output mismatch\n%s\n", out.c_str()); failed++; }
    }
    // Without the service the key falls back to the part after the last dot.
    Decl keyMissing;
    keyMissing.section = "name_missing";
    keyMissing.name = "localization.lang.medicine_factory";
    if (!ExpandNameOperation(&keyMissing, lines) || !ValidateOperations(&keyMissing, lines))
    { fprintf(stderr, "FATAL: self-test [name-fallback] %s\n", keyMissing.error.c_str()); failed++; }
    else
    {
        std::string out = ApplyOperations(keyMissing, lines, false);
        if (!Has(out, "$NAME_STR \"medicine_factory\""))
        { fprintf(stderr, "FATAL: self-test [name-fallback-output] Output mismatch\n%s\n", out.c_str()); failed++; }
    }
    // A source without a name line is rejected instead of silently gaining one.
    const char* noName = "$PRODUCTION plastic 0.11\n$WORKERS_NEEDED 10\nend\n";
    bool nb = false;
    std::vector<std::string> noNameLines = SplitLines(noName, strlen(noName), &nb);
    Decl nameless;
    nameless.section = "name_absent";
    nameless.name = "Whatever";
    if (ExpandNameOperation(&nameless, noNameLines))
    { fprintf(stderr, "FATAL: self-test [name-absent] A target without a $NAME line was accepted\n"); failed++; }

    if (!failed) printf("INFO: self-test [summary] All vanilla_buildings self-tests passed\n");
    return failed ? 1 : 0;
}

static int TestConfigAgainstMedia(const char* media, const char* ini)
{
    std::string game = media;
    size_t slash = game.find_last_of("\\/");
    if (slash != std::string::npos) ResolveWorkshopDir(game.substr(0, slash));
    if (!LoadRegistry(ini))
    {
        fprintf(stderr, "FATAL: config-test [parse] Could not parse %s. "
            "Action: verify that the file exists and is valid\n", ini);
        return 1;
    }
    int failed = 0;
    for (int i = 0; i < (int)g_decl.size(); i++)
    {
        Decl& d = g_decl[i];
        std::string why;
        if (!d.valid || !CanonicalTarget(d.target, &d.canonical, &why))
        {
            fprintf(stderr, "FATAL: config-test [%s] %s. Action: correct this section\n",
                d.section.c_str(),
                d.valid ? why.c_str() : d.error.c_str());
            failed++;
            continue;
        }
        std::string source = std::string(WorkshopTarget(d.canonical) ? g_workshopDir : media) + "\\" + d.canonical;
        std::vector<char> raw;
        if (!ReadWholeFile(source.c_str(), &raw))
        {
            fprintf(stderr, "FATAL: config-test [%s] Cannot read %s. "
                "Action: verify the target path and permissions\n",
                d.section.c_str(), source.c_str());
            failed++;
            continue;
        }
        bool bom = false;
        std::vector<std::string> lines = SplitLines(raw.empty() ? "" : &raw[0], raw.size(), &bom);
        if (!ValidateOperations(&d, lines))
        {
            fprintf(stderr, "FATAL: config-test [%s] %s. Action: correct the patch rules\n",
                d.section.c_str(), d.error.c_str());
            failed++;
        }
        else
            printf("INFO: config-test [%s] Validation passed (%d rules)\n",
                d.section.c_str(), (int)d.operations.size());
    }
    return failed ? 1 : 0;
}

int main(int argc, char** argv)
{
    int rc = RunSelfTests();
    if (argc == 3) rc |= TestConfigAgainstMedia(argv[1], argv[2]);
    return rc;
}

#endif
