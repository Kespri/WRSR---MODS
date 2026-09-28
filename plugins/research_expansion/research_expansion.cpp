// research_expansion.cpp - Research Expansion 1.4 for WRSR.
//
// The plugin adds user-defined research entries from plugins\research_expansion.ini
// and scoped Vanilla edits to research.ini at runtime via the TesmioLoader VFS.
// The original game file is never modified.

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

#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <sstream>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#define PLUGIN_VERSION "0.4.2"
// The namespace of the plugin's own Localization text pack (localization.ini of
// plugins\localization\research_expansion); [research:] sections complete short
// name / desc values with it.
#define TEXT_NAMESPACE "research_expansion"

static const size_t MAX_INI_BYTES = 8u * 1024u * 1024u;   // 8 MB
static const size_t MAX_PNG_BYTES = 4u * 1024u * 1024u;   // 4 MB
static const int    MAX_NEW_RESEARCH = 256;
static const size_t MAX_MODIFY_BLOCKS = 256;
static const size_t MAX_EDIT_OPERATIONS = 4096;
static const size_t MAX_EDIT_LINE_BYTES = 4096;
static const size_t MAX_NEW_LINKS = 4096;
static const size_t REQUIRED_ICON_SIDE = 128;
static const size_t GENERAL_VALUE_CAPACITY = 256;

static const char* RESEARCH_INI_REL = "media_soviet\\research\\research.ini";
static const char* RESEARCH_DIR_REL = "media_soviet\\research";
static const char* PLUGIN_INI = "plugins\\research_expansion.ini";
static const char* PLUGIN_LOG_NAME = "tesmioloader.research_expansion.log";
static const char* RESEARCH_INSERT_MARKER = "ferfer";

struct DirectiveSpec
{
    const char* name;
    bool requiresValue;
    bool unique;
};

// Structural directives and flags are unique. Unlock directives may occur
// more than once when they use different values.
static const DirectiveSpec kDirectiveSpecs[] =
{
    // ParseNewBlocks retains the block-start line in NewBlock::lines, so the
    // $RESEARCH directive is validated here like every other directive.
    { "$RESEARCH", true, true },
    { "$TYPE_TECHNICAL", false, true },
    { "$TYPE_SOVIET", false, true },
    { "$TYPE_MEDICAL", false, true },
    { "$COST", true, true },
    { "$NAME", true, true },
    { "$DESC", true, true },
    { "$RESEARCH_ADD", false, true },
    { "$UNLOCK_RESEARCH", true, false },
    { "$UNLOCK_BUILDING_PRODUCTION", true, false },
    { "$UNLOCK_BUILDING_PRODUCTION_CONSUMPTION", true, false },
    { "$UNLOCK_BUILDING_PRODUCTION_1_CONSUMPTION", true, false },
    { "$UNLOCK_BUILDING_PRODUCTION_3", true, false },
    { "$UNLOCK_BUILDING_TYPE", true, false },
    { "$UNLOCK_BUILDING_TYPE_SUBTYPE", true, false },
    { "$UNLOCK_BUILDING_TYPE_ONEENOUGH", true, false },
    { "$UNLOCK_BUILDING_TYPE_ONLYSUBTYPE", true, false },
    { "$UNLOCK_BUILDING", true, false },
    { "$UNLOCK_BUILDING_HOTEL_3PLUS", true, false },
    { "$UNLOCK_TOOLS", true, false },
    { "$UNLOCK_TOOLS_ONLY_EA", true, false },
    { "$UNLOCK_TOOLS_NOT_EA", true, false },
    { "$UNLOCK_BUILDING_STYLE_FLAG", true, false },
    { "$UNLOCK_BUILDING_RESIDENTIAL_PANELS", true, false },
    { "$UNLOCK_BUILDING_RESIDENTIAL_TALL", true, false },
    { "$UNLOCK_BUILDING_RESIDENTIAL_QUALITY", true, false },
    { "$UNLOCK_MONUMENT_CONCRETE", true, false },
    { "$UNLOCK_BUILDING_CONNECTION_SUN", true, false },
    { "$UNLOCK_BUILDING_CONNECTION_WIND", true, false },
    { "$YEAR", true, true },
    { "$LOCK_AFTER_DAYS", true, true },
    { "$AVAILABLE", false, true },
    { "$AVAILABLE_NO_GARBAGE", false, true },
    { "$IGNORE_NO_POLLUTION", false, true },
    { "$IGNORE_NO_FIRES", false, true },
    { "$IGNORE_NO_ELECTRIC", false, true },
    { "$IGNORE_NO_EARLYSTART", false, true },
    { "$IGNORE_NO_GARBAGE", false, true },
    { "$IGNORE_NO_HEATING", false, true },
    { "$UNLOCK_BUILDING_FROM_PANELS_ONLY_EA", true, false },
    { "$UNLOCK_TOOLS_EA", true, false }
};

static const TsmLocalizationApi* g_localization = nullptr;

static HANDLE    g_detail = INVALID_HANDLE_VALUE;
static bool      g_bound = false;
static int       g_debug = 0;
static volatile LONG g_logWarnings;
static volatile LONG g_logErrors;
static volatile LONG g_logFatals;
static ULONGLONG g_logStarted;
static ULONGLONG g_phaseStarted;
static LONG g_phaseWarnings;
static LONG g_phaseErrors;
static LONG g_phaseFatals;
static DWORD g_lastWindowsError;

static std::string g_gameDir;
static std::string g_mediaDir;
static std::string g_vfsDir;
static std::string g_pluginDir;

struct UnlockPlacement
{
    std::string anchor;
    bool after = false;
    size_t sourceLine = 0;
};

struct EditOperation
{
    std::string command;
    std::string first;
    std::string second;
    size_t sourceLine = 0;
};

struct Modification
{
    std::string id;
    bool enabled = true;
    bool enabledSeen = false;
    size_t sourceLine = 0;
    std::vector<EditOperation> operations;
    std::string cost;       // cost = <points>, applied as a replace of the $COST line
    size_t costLine = 0;
};

struct NewBlock
{
    std::string id;
    std::string type;
    std::string cost;
    std::string nameKey;
    std::string descKey;
    int         nameId = 0;
    int         descId = 0;
    std::vector<std::string> lines;
    std::vector<size_t> sourceLines;
    std::vector<std::string> dependencies;
    std::vector<UnlockPlacement> placements;
    size_t sourceLine = 0;
    bool available = false;
    size_t validationLine = 0;
};

// [research:<id>] sections are the INI form of a new research block, written
// by Republic Mod Manager. They are expanded into NewBlock lines after the free
// blocks have been parsed and then validated by the very same rules.
struct ResearchRequire
{
    std::string dependency;
    std::string mode;        // "", "before" or "after"
    std::string anchor;
    size_t sourceLine = 0;
};

struct ResearchSection
{
    std::string id;
    std::string type;        // $TYPE_... line
    std::string cost;
    std::string nameKey;
    std::string descKey;
    bool enabled = true;
    bool enabledSeen = false;
    size_t sourceLine = 0, typeLine = 0, costLine = 0, nameLine = 0, descLine = 0;
    std::vector<ResearchRequire> requires;
    std::vector<std::pair<std::string, size_t> > unlocks;
    std::vector<std::pair<std::string, size_t> > raws;
};

struct OriginalBlock
{
    std::string id;
    size_t start = 0;
    size_t end = 0;
    std::vector<std::string> lines;
    std::vector<std::string> baseline;
    size_t editSourceLine = 0;
    bool deactivated = false;
};

struct IconPlan
{
    std::string researchId;
    std::string source;
    std::string destination;
    bool fallback = false;
    bool keep = false;       // a valid icon that is already in the VFS research folder
};

static std::vector<NewBlock>       g_newBlocks;
static std::vector<Modification>   g_modifications;
static std::vector<OriginalBlock>  g_originalBlocks;
static std::set<std::string>       g_allIds;
static std::map<std::string, size_t> g_originalIndex;
static std::set<std::string> g_ambiguousOriginalIds;

// ---------------------------------------------------------------- helpers

static void DetailWriteLine(const char* text)
{
    if (g_detail == INVALID_HANDLE_VALUE || !text) return;

    if (g_bound) EnterCriticalSection(&g_lock);

    SYSTEMTIME t;
    GetLocalTime(&t);

    char stamp[64];
    _snprintf_s(stamp, sizeof(stamp), _TRUNCATE,
        "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        t.wYear, t.wMonth, t.wDay,
        t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);

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
        level, where ? where : "-", rule ? rule : "general", body);

    CountLevel(level);
    if (H) Logf("research_expansion  %s", full);
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
        action, error, systemText, remedy ? remedy : "Review the preceding context and retry");
}

static void BeginLogPhase()
{
    g_phaseStarted = GetTickCount64();
    g_phaseWarnings = InterlockedCompareExchange(&g_logWarnings, 0, 0);
    g_phaseErrors = InterlockedCompareExchange(&g_logErrors, 0, 0);
    g_phaseFatals = InterlockedCompareExchange(&g_logFatals, 0, 0);
}

static void LogSummaryStatus(const char* phase, const char* status)
{
    ULONGLONG elapsed = g_phaseStarted ? GetTickCount64() - g_phaseStarted : 0;
    Info("%s %s after %llu ms; %ld warning(s), %ld error(s), %ld fatal error(s)",
        phase, status,
        (unsigned long long)elapsed,
        InterlockedCompareExchange(&g_logWarnings, 0, 0) - g_phaseWarnings,
        InterlockedCompareExchange(&g_logErrors, 0, 0) - g_phaseErrors,
        InterlockedCompareExchange(&g_logFatals, 0, 0) - g_phaseFatals);
}

static void LogSummary(const char* phase, bool success)
{
    LogSummaryStatus(phase, success ? "completed successfully" : "failed");
}

static bool FileExistsA(const std::string& p)
{
    DWORD a = GetFileAttributesA(p.c_str());
    if (a == INVALID_FILE_ATTRIBUTES)
    {
        g_lastWindowsError = GetLastError();
        return false;
    }
    return !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirExistsA(const std::string& p)
{
    DWORD a = GetFileAttributesA(p.c_str());
    if (a == INVALID_FILE_ATTRIBUTES)
    {
        g_lastWindowsError = GetLastError();
        return false;
    }
    return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static std::string JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    char c = a[a.size() - 1];
    return (c == '\\' || c == '/') ? a + b : a + "\\" + b;
}

static bool ReadTextFile(const std::string& path, std::string& out)
{
    out.clear();
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { g_lastWindowsError = GetLastError(); return false; }

    LARGE_INTEGER sz;
    BOOL sizeOk = GetFileSizeEx(h, &sz);
    if (!sizeOk || sz.QuadPart < 0 || (unsigned long long)sz.QuadPart > MAX_INI_BYTES)
    {
        g_lastWindowsError = sizeOk ? ERROR_FILE_TOO_LARGE : GetLastError();
        CloseHandle(h);
        return false;
    }

    out.resize((size_t)sz.QuadPart);
    size_t done = 0;
    while (done < out.size())
    {
        DWORD want = (DWORD)((out.size() - done > 1u << 20) ? 1u << 20 : out.size() - done);
        DWORD got = 0;
        if (!ReadFile(h, &out[done], want, &got, NULL))
        {
            g_lastWindowsError = GetLastError();
            CloseHandle(h);
            out.clear();
            return false;
        }
        if (got == 0)
        {
            g_lastWindowsError = ERROR_HANDLE_EOF;
            CloseHandle(h);
            out.clear();
            return false;
        }
        done += got;
    }
    CloseHandle(h);
    return true;
}

static bool ReadBinaryFile(const std::string& path, std::vector<unsigned char>& out)
{
    out.clear();
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { g_lastWindowsError = GetLastError(); return false; }

    LARGE_INTEGER sz;
    BOOL sizeOk = GetFileSizeEx(h, &sz);
    if (!sizeOk || sz.QuadPart < 0 || (unsigned long long)sz.QuadPart > MAX_PNG_BYTES)
    {
        g_lastWindowsError = sizeOk ? ERROR_FILE_TOO_LARGE : GetLastError();
        CloseHandle(h);
        return false;
    }

    out.resize((size_t)sz.QuadPart);
    size_t done = 0;
    while (done < out.size())
    {
        DWORD want = (DWORD)((out.size() - done > 1u << 20) ? 1u << 20 : out.size() - done);
        DWORD got = 0;
        if (!ReadFile(h, &out[done], want, &got, NULL))
        {
            g_lastWindowsError = GetLastError();
            CloseHandle(h);
            out.clear();
            return false;
        }
        if (got == 0)
        {
            g_lastWindowsError = ERROR_HANDLE_EOF;
            CloseHandle(h);
            out.clear();
            return false;
        }
        done += got;
    }
    CloseHandle(h);
    return true;
}

static bool WriteTextFileAtomic(const std::string& path, const std::string& data)
{
    std::string tmp = path + ".tmp";
    HANDLE h = CreateFileA(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (h == INVALID_HANDLE_VALUE) { g_lastWindowsError = GetLastError(); return false; }

    size_t done = 0;
    while (done < data.size())
    {
        DWORD want = (DWORD)((data.size() - done > 1u << 20) ? 1u << 20 : data.size() - done);
        DWORD put = 0;
        if (!WriteFile(h, data.data() + done, want, &put, NULL))
        {
            g_lastWindowsError = GetLastError();
            CloseHandle(h);
            DeleteFileA(tmp.c_str());
            return false;
        }
        if (put == 0)
        {
            g_lastWindowsError = ERROR_WRITE_FAULT;
            CloseHandle(h);
            DeleteFileA(tmp.c_str());
            return false;
        }
        done += put;
    }
    if (!FlushFileBuffers(h))
    {
        g_lastWindowsError = GetLastError();
        CloseHandle(h);
        DeleteFileA(tmp.c_str());
        return false;
    }
    CloseHandle(h);

    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        g_lastWindowsError = GetLastError();
        DeleteFileA(tmp.c_str());
        return false;
    }
    return true;
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

static bool EnsureDir(const std::string& dir)
{
    if (dir.empty()) return false;
    std::string cur;
    size_t pos = 0;
    while (pos <= dir.size())
    {
        size_t sep = dir.find_first_of("\\/", pos);
        if (sep == std::string::npos) sep = dir.size();
        if (sep > pos)
        {
            cur = cur.empty() ? dir.substr(0, sep) : cur + "\\" + dir.substr(pos, sep - pos);
            if (!DirExistsA(cur))
            {
                if (!CreateDirectoryA(cur.c_str(), NULL) &&
                    GetLastError() != ERROR_ALREADY_EXISTS)
                {
                    g_lastWindowsError = GetLastError();
                    return false;
                }
            }
        }
        pos = sep + 1;
    }
    return true;
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

static bool IsSafeRelativePath(const std::string& path, bool allowSubdirectories)
{
    if (path.empty() || IsAbsoluteWindowsPath(path) || path.find(':') != std::string::npos)
        return false;

    size_t start = 0;
    while (start <= path.size())
    {
        size_t end = path.find_first_of("\\/", start);
        if (end == std::string::npos) end = path.size();
        std::string part = path.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") return false;
        for (char c : part)
            if ((unsigned char)c < 32) return false;
        if (!allowSubdirectories && end != path.size()) return false;
        if (end == path.size()) break;
        start = end + 1;
    }
    return true;
}

static bool RemoveGeneratedResearch()
{
    std::string path = JoinPath(g_vfsDir, RESEARCH_INI_REL);
    if (DeleteFileA(path.c_str()))
    {
        Info("Previous generated research.ini removed; Vanilla fallback is active until generation completes");
        return true;
    }

    DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
    {
        Debug("No previous generated research.ini was present at %s", path.c_str());
        return true;
    }

    g_lastWindowsError = error;
    ReportWindows("FATAL", path.c_str(), "vanilla-fallback",
        "Previous generated research.ini could not be removed", error,
        "Close programs using the file and verify VFS write access; Vanilla fallback cannot be guaranteed while this file remains");
    return false;
}

static bool CopyFileAtomic(const std::string& source, const std::string& destination)
{
    std::string temporary = destination + ".tmp";
    if (!CopyFileA(source.c_str(), temporary.c_str(), FALSE))
    {
        g_lastWindowsError = GetLastError();
        return false;
    }
    if (!MoveFileExA(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        g_lastWindowsError = GetLastError();
        DeleteFileA(temporary.c_str());
        return false;
    }
    return true;
}

static uint32_t ReadBigEndian32(const unsigned char* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t PngCrc32(const unsigned char* data, size_t size)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc ^ 0xFFFFFFFFu;
}

static bool ValidatePng(const std::string& path, std::string& reason)
{
    reason.clear();
    std::vector<unsigned char> data;
    if (!ReadBinaryFile(path, data))
    {
        if (!FileExistsA(path)) reason = "file not found";
        else if (g_lastWindowsError == ERROR_FILE_TOO_LARGE) reason = "file exceeds 4 MB";
        else reason = "file could not be read";
        return false;
    }
    if (data.size() < 33)
    {
        reason = "file is too short to contain a complete PNG header";
        return false;
    }

    static const unsigned char pngSig[8] = { 0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A };
    if (memcmp(data.data(), pngSig, 8) != 0)
    {
        reason = "PNG signature is missing";
        return false;
    }

    bool sawHeader = false;
    bool sawImageData = false;
    bool sawEnd = false;
    size_t offset = 8;
    while (offset < data.size())
    {
        if (data.size() - offset < 12)
        {
            reason = "PNG chunk header is truncated";
            return false;
        }
        uint32_t length = ReadBigEndian32(data.data() + offset);
        if ((size_t)length > data.size() - offset - 12)
        {
            reason = "PNG chunk data is truncated";
            return false;
        }

        const unsigned char* type = data.data() + offset + 4;
        const unsigned char* chunkData = data.data() + offset + 8;
        uint32_t storedCrc = ReadBigEndian32(chunkData + length);
        uint32_t actualCrc = PngCrc32(type, (size_t)length + 4);
        if (storedCrc != actualCrc)
        {
            reason = "PNG chunk CRC is invalid";
            return false;
        }

        if (!sawHeader)
        {
            if (memcmp(type, "IHDR", 4) != 0 || length != 13)
            {
                reason = "IHDR is not the first valid PNG chunk";
                return false;
            }
            uint32_t width = ReadBigEndian32(chunkData);
            uint32_t height = ReadBigEndian32(chunkData + 4);
            if (width != REQUIRED_ICON_SIDE || height != REQUIRED_ICON_SIDE)
            {
                char dimensions[160];
                _snprintf_s(dimensions, sizeof(dimensions), _TRUNCATE,
                    "dimensions are %lux%lu; required dimensions are %Iux%Iu",
                    (unsigned long)width, (unsigned long)height,
                    REQUIRED_ICON_SIDE, REQUIRED_ICON_SIDE);
                reason = dimensions;
                return false;
            }
            sawHeader = true;
        }
        else if (memcmp(type, "IDAT", 4) == 0)
        {
            sawImageData = true;
        }
        else if (memcmp(type, "IEND", 4) == 0)
        {
            if (length != 0)
            {
                reason = "IEND chunk is malformed";
                return false;
            }
            sawEnd = true;
            offset += 12;
            if (offset != data.size())
            {
                reason = "unexpected data follows the IEND chunk";
                return false;
            }
            break;
        }
        offset += (size_t)length + 12;
    }

    if (!sawHeader || !sawImageData || !sawEnd)
    {
        reason = "PNG is missing IHDR, IDAT, or IEND data";
        return false;
    }
    return true;
}

static std::string TrimA(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return s.substr(b, e - b);
}

static std::string CanonicalId(const std::string& id)
{
    std::string result = id;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return (char)tolower(c); });
    return result;
}

static bool IsValidResearchId(const std::string& id)
{
    if (id.empty()) return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

static bool ParsePositiveInt(const std::string& text, int* value)
{
    if (!value || text.empty()) return false;
    errno = 0;
    char* end = NULL;
    long parsed = strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || !end || *end != 0 ||
        parsed <= 0 || parsed > INT_MAX)
        return false;
    *value = (int)parsed;
    return true;
}

static bool IsDirective(const std::string& s, std::string* cmd = nullptr,
    std::string* value = nullptr)
{
    std::string t = TrimA(s);
    if (t.empty() || t[0] != '$') return false;

    size_t space = t.find_first_of(" \t");
    if (cmd) *cmd = (space == std::string::npos) ? t : t.substr(0, space);
    if (value) *value = (space == std::string::npos) ? "" : TrimA(t.substr(space + 1));
    return true;
}

static const DirectiveSpec* FindDirectiveSpec(const std::string& cmd)
{
    for (size_t i = 0; i < sizeof(kDirectiveSpecs) / sizeof(kDirectiveSpecs[0]); ++i)
        if (cmd == kDirectiveSpecs[i].name) return &kDirectiveSpecs[i];
    return nullptr;
}

static bool IsDependencyLine(const std::string& s, std::string* dep)
{
    std::string t = TrimA(s);
    if (t.empty() || t[0] != '+') return false;
    if (dep) *dep = TrimA(t.substr(1));
    return true;
}

static bool IsOnlyDashes(const std::string& s)
{
    if (s.empty()) return false;
    for (char c : s)
        if (c != '-') return false;
    return true;
}

static bool IsDeactivatedResearchAdd(const std::string& s)
{
    size_t marker = s.find_first_not_of('-');
    return marker != std::string::npos && marker > 0 &&
        s.compare(marker, strlen("$RESEARCH_ADD"), "$RESEARCH_ADD") == 0 &&
        marker + strlen("$RESEARCH_ADD") == s.size();
}

// ---------------------------------------------------------------- config & base file

static bool EditError(const std::string& id, size_t line, const char* command,
    const char* rule, const std::string& reason)
{
    Report("ERROR", PLUGIN_INI, rule, "Research '%s', INI line %Iu, %s: %s",
        id.c_str(), line, command, reason.c_str());
    return false;
}

static bool IsSingleEditLine(const std::string& text)
{
    if (text.empty() || text.size() > MAX_EDIT_LINE_BYTES) return false;
    for (unsigned char c : text)
        if ((c < 32 && c != '\t') || c == 127) return false;
    return text[0] == '$' || text[0] == '+' || IsOnlyDashes(text);
}

static bool IsStructuralLine(const std::string& text)
{
    std::string cmd;
    return IsDirective(text, &cmd) &&
        (cmd == "$RESEARCH" || cmd == "$RESEARCH_ADD");
}

static bool ParseEditOperation(Modification& mod, const std::string& key,
    const std::string& value, size_t lineNo)
{
    if (key == "enabled")
    {
        if (mod.enabledSeen || (value != "0" && value != "1"))
            return EditError(mod.id, lineNo, "enabled", "modify-enabled",
                "enabled must occur once at most and must be exactly 0 or 1");
        mod.enabledSeen = true;
        mod.enabled = value == "1";
        return true;
    }
    // cost = <points> replaces the block's single $COST line; the Republic Mod
    // Manager offers it as a plain number instead of a replace command.
    if (key == "cost")
    {
        if (!mod.cost.empty() || value.empty() || value.size() > 10 ||
            value.find_first_not_of("0123456789") != std::string::npos || value[0] == '0')
            return EditError(mod.id, lineNo, "cost", "modify-cost",
                "cost must occur once at most and be a positive integer without leading zeros");
        mod.cost = value;
        mod.costLine = lineNo;
        return true;
    }
    bool single = key == "remove" || key == "add";
    bool pair = key == "replace" || key == "insert_before" ||
        key == "insert_after" || key == "move_before" || key == "move_after";
    if (!single && !pair)
        return EditError(mod.id, lineNo, key.c_str(), "modify-command",
            "Unknown modification command");

    EditOperation op;
    op.command = key;
    op.sourceLine = lineNo;
    size_t separator = value.find('|');
    if ((single && separator != std::string::npos) ||
        (pair && (separator == std::string::npos ||
            value.find('|', separator + 1) != std::string::npos)))
        return EditError(mod.id, lineNo, key.c_str(), "modify-format",
            pair ? "Expected exactly two complete lines separated by |" :
                "Expected one complete line without |");
    op.first = TrimA(value.substr(0, separator));
    if (pair) op.second = TrimA(value.substr(separator + 1));
    if (!IsSingleEditLine(op.first) || (pair && !IsSingleEditLine(op.second)))
        return EditError(mod.id, lineNo, key.c_str(), "modify-line",
            "Operands must be single game directives, +dependencies or dash separators (maximum 4096 bytes)");
    bool firstIsChanged = key == "remove" || key == "replace" ||
        key == "move_before" || key == "move_after" || key == "add";
    bool secondIsNew = key == "replace" || key == "insert_before" || key == "insert_after";
    if ((firstIsChanged && IsStructuralLine(op.first)) ||
        (secondIsNew && IsStructuralLine(op.second)))
        return EditError(mod.id, lineNo, key.c_str(), "modify-boundary",
            "$RESEARCH and $RESEARCH_ADD cannot be removed, replaced, added or moved");
    mod.operations.push_back(op);
    return true;
}

static bool IsKnownGeneralKey(const std::string& key)
{
    static const char* const keys[] = {
        "enabled", "debug", "icon_folder", "noimage_name"
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        if (key == keys[i]) return true;
    return false;
}

// The Windows profile API returns values but cannot report unknown or repeated
// keys. Scan the raw file once so those configuration mistakes fail closed
// before configInt/configString selects one of the ambiguous values.
static std::vector<ResearchSection> g_researchSections;

static std::string UpperA(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)toupper((unsigned char)s[i]);
    return s;
}

// One key of a [research:<id>] section. Format only; meaning is checked later by
// ValidateNewBlock on the expanded lines.
static bool ParseResearchKey(ResearchSection& rs, const std::string& key,
    const std::string& value, size_t lineNo)
{
    if (key == "enabled")
    {
        if (rs.enabledSeen || (value != "0" && value != "1"))
            return EditError(rs.id, lineNo, "enabled", "research-enabled",
                "enabled must occur once at most and must be exactly 0 or 1");
        rs.enabledSeen = true;
        rs.enabled = value == "1";
        return true;
    }
    if (key == "type")
    {
        if (rs.typeLine) return EditError(rs.id, lineNo, "type", "research-duplicate-key", "type may occur once");
        std::string t = UpperA(TrimA(value));
        if (t.rfind("$TYPE_", 0) != 0) t = "$TYPE_" + t;
        if (t != "$TYPE_TECHNICAL" && t != "$TYPE_SOVIET" && t != "$TYPE_MEDICAL")
            return EditError(rs.id, lineNo, "type", "research-type", "Expected technical, soviet or medical");
        rs.type = t; rs.typeLine = lineNo;
        return true;
    }
    if (key == "cost")
    {
        if (rs.costLine) return EditError(rs.id, lineNo, "cost", "research-duplicate-key", "cost may occur once");
        if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
            return EditError(rs.id, lineNo, "cost", "research-cost", "Expected a positive whole number");
        rs.cost = value; rs.costLine = lineNo;
        return true;
    }
    if (key == "name" || key == "desc")
    {
        size_t& seen = key == "name" ? rs.nameLine : rs.descLine;
        if (seen) return EditError(rs.id, lineNo, key.c_str(), "research-duplicate-key", key + " may occur once");
        // a bare word is completed to <namespace>.<word>.name / .desc by
        // ExpandResearchSections; a value with dots is a complete key.
        if (value.empty() || value.find_first_of(" \t|") != std::string::npos)
            return EditError(rs.id, lineNo, key.c_str(), "research-text-key", "Expected a Localization key (namespace.key) or a bare word");
        (key == "name" ? rs.nameKey : rs.descKey) = value; seen = lineNo;
        return true;
    }
    if (key == "requires")
    {
        // dependency | before/after/normal | anchor  (the last two are optional)
        std::vector<std::string> parts;
        size_t start = 0;
        for (;;)
        {
            size_t bar = value.find('|', start);
            parts.push_back(TrimA(value.substr(start, bar == std::string::npos ? std::string::npos : bar - start)));
            if (bar == std::string::npos) break;
            start = bar + 1;
        }
        if (parts.size() > 3 || !IsValidResearchId(parts[0]))
            return EditError(rs.id, lineNo, "requires", "research-requires", "Expected dependency | before/after | anchor");
        ResearchRequire r;
        r.dependency = parts[0];
        r.sourceLine = lineNo;
        std::string mode = parts.size() > 1 ? CanonicalId(parts[1]) : "";
        if (mode == "normal") mode.clear();
        if (mode != "" && mode != "before" && mode != "after")
            return EditError(rs.id, lineNo, "requires", "research-requires", "Position must be normal, before or after");
        std::string anchor = parts.size() > 2 ? parts[2] : "";
        if (mode.empty() != anchor.empty() || (!anchor.empty() && !IsValidResearchId(anchor)))
            return EditError(rs.id, lineNo, "requires", "research-requires", "before/after need an anchor research; normal takes none");
        r.mode = mode; r.anchor = anchor;
        rs.requires.push_back(r);
        return true;
    }
    if (key == "unlock")
    {
        if (value.empty() || value[0] != '$' || value.find('|') != std::string::npos)
            return EditError(rs.id, lineNo, "unlock", "research-unlock", "Expected one $UNLOCK_... line");
        rs.unlocks.push_back(std::make_pair(value, lineNo));
        return true;
    }
    if (key == "line")
    {
        if (value.empty() || (value[0] != '$' && value[0] != '+' && value[0] != '@' && !IsOnlyDashes(value)))
            return EditError(rs.id, lineNo, "line", "research-line", "Expected a research directive line");
        rs.raws.push_back(std::make_pair(value, lineNo));
        return true;
    }
    return EditError(rs.id, lineNo, key.c_str(), "research-key",
        "Unknown key; expected enabled, type, cost, name, desc, requires, unlock or line");
}

// Expands the enabled [research:] sections into NewBlock lines (after the free
// blocks, in INI order) so ValidateNewBlock treats both forms alike. Every
// generated line remembers the INI line of its key for the error messages.
static bool ExpandResearchSections(std::vector<NewBlock>& blocks)
{
    for (const ResearchSection& rs : g_researchSections)
    {
        if (!rs.enabled) continue;
        for (const NewBlock& existing : blocks)
            if (CanonicalId(existing.id) == CanonicalId(rs.id))
                return EditError(rs.id, rs.sourceLine, "section", "research-duplicate",
                    "A free $RESEARCH block with this id already exists");
        if (blocks.size() >= (size_t)MAX_NEW_RESEARCH)
        {
            Report("ERROR", PLUGIN_INI, "research-limit",
                "More than %d new research entries were provided; reduce the configuration size", MAX_NEW_RESEARCH);
            return false;
        }
        NewBlock nb;
        nb.id = rs.id;
        nb.sourceLine = rs.sourceLine;
        nb.lines.push_back("$RESEARCH " + rs.id); nb.sourceLines.push_back(rs.sourceLine);
        for (const ResearchRequire& r : rs.requires)
        {
            nb.lines.push_back("+" + r.dependency); nb.sourceLines.push_back(r.sourceLine);
            if (!r.mode.empty()) { nb.lines.push_back("@" + r.mode + "_" + r.anchor); nb.sourceLines.push_back(r.sourceLine); }
        }
        if (rs.typeLine) { nb.lines.push_back(rs.type); nb.sourceLines.push_back(rs.typeLine); }
        if (rs.costLine) { nb.lines.push_back("$COST " + rs.cost); nb.sourceLines.push_back(rs.costLine); }
        for (size_t i = 0; i < rs.unlocks.size(); ++i) { nb.lines.push_back(rs.unlocks[i].first); nb.sourceLines.push_back(rs.unlocks[i].second); }
        for (size_t i = 0; i < rs.raws.size(); ++i) { nb.lines.push_back(rs.raws[i].first); nb.sourceLines.push_back(rs.raws[i].second); }
        // name / desc default to the research id and are completed with the
        // plugin's text-pack namespace, so "quartz_smasher" (or nothing) becomes
        // research_expansion.quartz_smasher.name; a key with dots is used as it is.
        std::string nameKey = rs.nameLine ? rs.nameKey : rs.id;
        std::string descKey = rs.descLine ? rs.descKey : rs.id;
        if (nameKey.find('.') == std::string::npos) nameKey = std::string(TEXT_NAMESPACE) + "." + nameKey + ".name";
        if (descKey.find('.') == std::string::npos) descKey = std::string(TEXT_NAMESPACE) + "." + descKey + ".desc";
        nb.lines.push_back("$NAME " + nameKey); nb.sourceLines.push_back(rs.nameLine ? rs.nameLine : rs.sourceLine);
        nb.lines.push_back("$DESC " + descKey); nb.sourceLines.push_back(rs.descLine ? rs.descLine : rs.sourceLine);
        nb.lines.push_back("$RESEARCH_ADD"); nb.sourceLines.push_back(rs.sourceLine);
        blocks.push_back(nb);
    }
    return true;
}

static bool ValidateGeneralConfigLayout(const std::string& pluginIni,
    std::vector<Modification>* modifications = nullptr)
{
    if (modifications) modifications->clear();
    g_researchSections.clear();
    std::set<std::string> researchIds;
    if (pluginIni.find('\0') != std::string::npos)
        return EditError("configuration", 0, "parse", "embedded-null", "NUL bytes are not allowed");
    std::vector<Modification> parsedModifications;
    std::set<std::string> modificationIds;
    size_t operationCount = 0;
    std::istringstream input(pluginIni);
    std::string line;
    std::string currentSection;
    std::set<std::string> keys;
    size_t lineNo = 0;
    bool sawGeneralSection = false;
    bool inResearchBlock = false;

    while (std::getline(input, line))
    {
        ++lineNo;
        std::string text = TrimA(line);
        if (text.empty() || text[0] == ';' || text[0] == '#' ||
            IsOnlyDashes(text))
        {
            continue;
        }

        if (text.rfind("$RESEARCH ", 0) == 0)
        {
            inResearchBlock = true;
            currentSection.clear();
            continue;
        }
        if (inResearchBlock)
        {
            if (text == "$RESEARCH_ADD") inResearchBlock = false;
            continue;
        }

        if (text[0] == '[')
        {
            if (text.size() < 3 || text[text.size() - 1] != ']')
            {
                Report("ERROR", PLUGIN_INI, "malformed-section",
                    "Malformed section header at line %Iu: %s",
                    lineNo, text.c_str());
                return false;
            }

            std::string section = CanonicalId(
                TrimA(text.substr(1, text.size() - 2)));
            if (section.rfind("modify:", 0) == 0)
            {
                std::string id = TrimA(section.substr(7));
                if (!IsValidResearchId(id) || !modificationIds.insert(id).second)
                    return EditError(id, lineNo, "section", "modify-section",
                        "Expected a valid research ID and only one [modify:ID] section per ID");
                if (parsedModifications.size() >= MAX_MODIFY_BLOCKS)
                    return EditError(id, lineNo, "section", "modify-limit", "At most 256 modification sections are allowed");
                Modification mod;
                mod.id = id;
                mod.sourceLine = lineNo;
                parsedModifications.push_back(mod);
                currentSection = section;
                continue;
            }
            if (section.rfind("research:", 0) == 0)
            {
                // the INI form of a new research block; the id keeps its spelling.
                std::string id = TrimA(text.substr(1, text.size() - 2));
                id = TrimA(id.substr(id.find(':') + 1));
                if (!IsValidResearchId(id) || !researchIds.insert(CanonicalId(id)).second)
                    return EditError(id, lineNo, "section", "research-section",
                        "Expected a valid research ID and only one [research:ID] section per ID");
                if (g_researchSections.size() >= (size_t)MAX_NEW_RESEARCH)
                    return EditError(id, lineNo, "section", "research-limit", "Too many [research:] sections");
                ResearchSection rs;
                rs.id = id;
                rs.sourceLine = lineNo;
                g_researchSections.push_back(rs);
                currentSection = section;
                continue;
            }
            if (section != "general")
            {
                Report("ERROR", PLUGIN_INI, "unknown-section",
                    "Unknown section '%s' at line %Iu; expected [general], [research:id] or [modify:research_id]",
                    section.c_str(), lineNo);
                return false;
            }
            if (sawGeneralSection)
            {
                Report("ERROR", PLUGIN_INI, "duplicate-section",
                    "Section [general] appears more than once (line %Iu)",
                    lineNo);
                return false;
            }
            sawGeneralSection = true;
            currentSection = section;
            continue;
        }

        size_t equals = text.find('=');
        if (currentSection.rfind("research:", 0) == 0)
        {
            ResearchSection& rs = g_researchSections.back();
            if (equals == std::string::npos)
                return EditError(rs.id, lineNo, "parse", "research-format", "Expected key = value");
            std::string key = CanonicalId(TrimA(text.substr(0, equals)));
            if (!ParseResearchKey(rs, key, TrimA(text.substr(equals + 1)), lineNo)) return false;
            continue;
        }
        if (currentSection.rfind("modify:", 0) == 0)
        {
            Modification& mod = parsedModifications.back();
            if (equals == std::string::npos)
                return EditError(mod.id, lineNo, "parse", "modify-format", "Expected command = value");
            std::string key = CanonicalId(TrimA(text.substr(0, equals)));
            if (!ParseEditOperation(mod, key, TrimA(text.substr(equals + 1)), lineNo)) return false;
            if (key != "enabled" && ++operationCount > MAX_EDIT_OPERATIONS)
                return EditError(mod.id, lineNo, key.c_str(), "modify-limit", "At most 4096 edit operations are allowed");
            continue;
        }
        if (equals == std::string::npos)
            continue; // Research syntax is validated by ParseNewBlocks.

        if (currentSection != "general")
        {
            Report("ERROR", PLUGIN_INI, "assignment-outside-section",
                "Configuration assignment outside [general] at line %Iu: %s",
                lineNo, text.c_str());
            return false;
        }

        std::string key = CanonicalId(TrimA(text.substr(0, equals)));
        std::string value = TrimA(text.substr(equals + 1));
        if (key.empty() || !IsKnownGeneralKey(key))
        {
            Report("ERROR", PLUGIN_INI, "unknown-general-key",
                "Unknown [general] key '%s' at line %Iu",
                key.empty() ? "<empty>" : key.c_str(), lineNo);
            return false;
        }
        if (!keys.insert(key).second)
        {
            Report("ERROR", PLUGIN_INI, "duplicate-general-key",
                "[general] key '%s' appears more than once (line %Iu)",
                key.c_str(), lineNo);
            return false;
        }

        if ((key == "enabled" || key == "debug") &&
            value != "0" && value != "1")
        {
            Report("ERROR", PLUGIN_INI, "invalid-general-value",
                "[general] %s must be exactly 0 or 1 at line %Iu",
                key.c_str(), lineNo);
            return false;
        }
        if ((key == "icon_folder" || key == "noimage_name") &&
            value.size() >= GENERAL_VALUE_CAPACITY - 1)
        {
            Report("ERROR", PLUGIN_INI, "general-value-too-long",
                "[general] %s exceeds the maximum of %Iu characters at line %Iu",
                key.c_str(), GENERAL_VALUE_CAPACITY - 2, lineNo);
            return false;
        }
    }
    if (modifications) modifications->swap(parsedModifications);
    return true;
}

static bool ResolveGameFolders()
{
    char exe[MAX_PATH];
    if (!GetModuleFileNameA(g_exe, exe, sizeof(exe))) return false;
    char* slash = strrchr(exe, '\\');
    if (!slash) return false;
    *slash = 0;
    g_gameDir = exe;
    g_mediaDir = JoinPath(g_gameDir, "media_soviet");
    return DirExistsA(g_mediaDir);
}

static bool ReadGeneralString(const char* key, const char* fallback,
    std::string& output)
{
    char value[GENERAL_VALUE_CAPACITY] = {};
    int length = H->configString(PLUGIN_INI, "general", key,
        value, (int)sizeof(value), fallback);
    // GetPrivateProfileStringA reports capacity - 1 when truncation occurs.
    // Reject that boundary as ambiguous even when a value happens to fit it.
    if (length >= (int)sizeof(value) - 1)
    {
        Report("ERROR", PLUGIN_INI, "general-value-truncated",
            "[general] %s is too long or was truncated; maximum length is %Iu characters",
            key, sizeof(value) - 2);
        return false;
    }

    output = TrimA(value);
    if (output.empty()) output = fallback;
    return true;
}

static bool ReadGeneralConfig()
{
    // icon_folder and noimage_name are no longer used - the icons live in
    // the VFS research folder and noimage.png is looked up beside the DLL and in
    // plugins\research_expansion (see PlanIcons). Old INIs may still carry the
    // keys; a value other than the old default is reported once and ignored.
    std::string legacyFolder, legacyName;
    if (!ReadGeneralString("icon_folder", "icons", legacyFolder) ||
        !ReadGeneralString("noimage_name", "noimage.png", legacyName))
        return false;
    if (legacyFolder != "icons" || legacyName != "noimage.png")
        Report("WARN", PLUGIN_INI, "legacy-key",
            "icon_folder / noimage_name are not used: icons are read from the VFS research folder, noimage.png from research_expansion\\icons beside the DLL or plugins\\research_expansion");
    return true;
}

static bool ParseOriginalBlocks(const std::string& content)
{
    g_originalBlocks.clear();
    g_originalIndex.clear();
    g_ambiguousOriginalIds.clear();
    g_allIds.clear();

    std::istringstream iss(content);
    std::string line;
    size_t lineNo = 0;
    OriginalBlock cur;
    bool inBlock = false;

    while (std::getline(iss, line))
    {
        lineNo++;
        std::string t = TrimA(line);

        if (t.empty())
        {
            if (inBlock)
                cur.lines.push_back(line);
            continue;
        }

        bool isActiveAdd = (t == "$RESEARCH_ADD");
        // Deactivated records belong to the Vanilla file and must remain
        // exactly as supplied by the game. They are never reactivated here.
        bool isDeactivatedAdd = IsDeactivatedResearchAdd(t);

        if (t.rfind("$RESEARCH ", 0) == 0)
        {
            if (inBlock)
            {
                Report("ERROR", "research.ini", "block-format",
                    "Incomplete block (missing $RESEARCH_ADD) before line %Iu", lineNo);
                return false;
            }
            cur = OriginalBlock();
            cur.id = TrimA(t.substr(strlen("$RESEARCH ")));
            cur.start = lineNo - 1;
            cur.deactivated = false;
            inBlock = true;
            cur.lines.push_back(line);
        }
        else if (isActiveAdd || isDeactivatedAdd)
        {
            if (!inBlock)
            {
                Report("ERROR", "research.ini", "block-format",
                    "$RESEARCH_ADD without block start in line %Iu", lineNo);
                return false;
            }

            cur.lines.push_back(line);
            cur.end = lineNo - 1;
            cur.deactivated = isDeactivatedAdd;
            cur.baseline = cur.lines;
            g_originalBlocks.push_back(cur);
            std::string key = CanonicalId(cur.id);
            if (g_originalIndex.find(key) == g_originalIndex.end())
                g_originalIndex[key] = g_originalBlocks.size() - 1;
            else
            {
                g_ambiguousOriginalIds.insert(key);
                Report("WARN", "research.ini", "duplicate-vanilla-id",
                    "Vanilla research ID '%s' occurs more than once; modifications and dependency insertion into this ID are rejected",
                    cur.id.c_str());
            }
            g_allIds.insert(key);
            inBlock = false;
        }
        else if (inBlock)
        {
            cur.lines.push_back(line);
        }
    }

    if (inBlock)
    {
        Report("ERROR", "research.ini", "block-format",
            "Last block without $RESEARCH_ADD");
        return false;
    }
    return true;
}

static bool ParseNewBlocks(const std::string& pluginIni, std::vector<NewBlock>& out)
{
    out.clear();
    std::istringstream iss(pluginIni);
    std::string line;
    size_t lineNo = 0;
    NewBlock cur;
    bool inBlock = false;

    while (std::getline(iss, line))
    {
        ++lineNo;
        std::string t = TrimA(line);

        if (t.empty())
        {
            if (inBlock)
            {
                cur.lines.push_back(line);
                cur.sourceLines.push_back(lineNo);
            }
            continue;
        }

        if (t[0] == ';' || t[0] == '#')
            continue;

        // A line made only of dashes is safe game syntax used as a visual
        // separator. Preserve it inside a user research block so the generated
        // research.ini keeps the Vanilla layout; outside a block it is only a
        // harmless top-level separator.
        if (IsOnlyDashes(t))
        {
            if (inBlock)
            {
                cur.lines.push_back(line);
                cur.sourceLines.push_back(lineNo);
            }
            continue;
        }

        if (t[0] == '-')
        {
            Report("ERROR", PLUGIN_INI, "deactivated-user-research",
                "Line %Iu starts with '-'. Deactivated research and directives are reserved for the Vanilla research.ini; remove the user block or use active directives",
                lineNo);
            return false;
        }

        if (t.rfind("$RESEARCH ", 0) == 0)
        {
            if (inBlock)
            {
                Report("ERROR", PLUGIN_INI, "block-format",
                    "Incomplete block '%s' (missing $RESEARCH_ADD) before line %Iu",
                    cur.id.c_str(), lineNo);
                return false;
            }
            cur = NewBlock();
            cur.id = TrimA(t.substr(strlen("$RESEARCH ")));
            cur.sourceLine = lineNo;
            inBlock = true;
            cur.lines.push_back(line);
            cur.sourceLines.push_back(lineNo);
        }
        else if (t == "$RESEARCH_ADD")
        {
            if (!inBlock)
            {
                Report("ERROR", PLUGIN_INI, "block-format",
                    "$RESEARCH_ADD without block start at line %Iu", lineNo);
                return false;
            }
            cur.lines.push_back("$RESEARCH_ADD");
            cur.sourceLines.push_back(lineNo);
            if (out.size() >= (size_t)MAX_NEW_RESEARCH)
            {
                Report("ERROR", PLUGIN_INI, "research-limit",
                    "More than %d new research blocks were provided; reduce the configuration size",
                    MAX_NEW_RESEARCH);
                return false;
            }
            out.push_back(cur);
            inBlock = false;
        }
        else if (inBlock)
        {
            if (t[0] != '$' && t[0] != '+' && t[0] != '@')
            {
                Report("ERROR", PLUGIN_INI, "unexpected-block-line",
                    "Unexpected content in research '%s' at line %Iu: %s",
                    cur.id.c_str(), lineNo, t.c_str());
                return false;
            }
            cur.lines.push_back(line);
            cur.sourceLines.push_back(lineNo);
        }
        else if (t[0] == '[')
        {
            // General configuration sections are read through the host API.
        }
        else if (t.find('=') != std::string::npos)
        {
            // General configuration assignments are read through the host API.
        }
        else
        {
            Report("ERROR", PLUGIN_INI, "unexpected-top-level-line",
                "Unexpected content outside a research block at line %Iu: %s",
                lineNo, t.c_str());
            return false;
        }
    }
    if (inBlock)
    {
        Report("ERROR", PLUGIN_INI, "block-format",
            "Last new block '%s' has no $RESEARCH_ADD",
            cur.id.c_str());
        return false;
    }
    return true;
}

static bool ValidateNewBlock(NewBlock& nb, const std::set<std::string>& allIds,
    std::string& errRule, std::string& errReason, bool vanilla = false)
{
    bool hasType = false, hasCost = false, hasName = false, hasDesc = false, hasAdd = false;
    nb.dependencies.clear();
    nb.placements.clear();
    nb.available = false;
    std::map<std::string, int> directiveCount;
    std::set<std::string> directiveLines;

    for (size_t i = 0; i < nb.lines.size(); ++i)
    {
        nb.validationLine = i < nb.sourceLines.size() ? nb.sourceLines[i] : nb.sourceLine;
        std::string t = TrimA(nb.lines[i]);
        std::string cmd, val;
        if (IsDirective(t, &cmd, &val))
        {
            const DirectiveSpec* spec = FindDirectiveSpec(cmd);
            if (!spec)
            {
                // Existing, unrecognized Vanilla directives are retained. New
                // edit operands still pass the strict directive allow-list.
                if (vanilla) continue;
                errRule = "unknown-directive";
                errReason = "Unknown directive: " + cmd;
                return false;
            }

            if (spec->requiresValue && val.empty())
            {
                errRule = "missing-value";
                errReason = cmd + " requires a value";
                return false;
            }
            if (!spec->requiresValue && !val.empty())
            {
                errRule = "unexpected-value";
                errReason = cmd + " does not take a value";
                return false;
            }

            int count = ++directiveCount[cmd];
            if (spec->unique && count > 1)
            {
                errRule = "duplicate-directive";
                errReason = cmd + " appears multiple times";
                return false;
            }

            std::string directiveLine = cmd;
            if (!val.empty()) directiveLine += " " + val;
            if (!directiveLines.insert(directiveLine).second)
            {
                errRule = "duplicate-directive";
                errReason = "Duplicate directive: " + directiveLine;
                return false;
            }

            if (cmd == "$TYPE_TECHNICAL" || cmd == "$TYPE_SOVIET" || cmd == "$TYPE_MEDICAL")
            {
                if (hasType) { errRule = "duplicate-type"; errReason = "Multiple $TYPE directives"; return false; }
                hasType = true;
                nb.type = cmd;
            }
            else if (cmd == "$COST")
            {
                hasCost = true;
                nb.cost = val;
                int parsedCost = 0;
                if (!ParsePositiveInt(val, &parsedCost))
                {
                    errRule = "invalid-cost";
                    errReason = "$COST must be one complete integer in the range 1..2147483647";
                    return false;
                }
            }
            else if (cmd == "$NAME")
            {
                hasName = true;
                nb.nameKey = val;
            }
            else if (cmd == "$DESC")
            {
                hasDesc = true;
                nb.descKey = val;
            }
            else if (cmd == "$RESEARCH_ADD")
            {
                hasAdd = true;
            }
            else if (cmd == "$AVAILABLE" || cmd == "$AVAILABLE_NO_GARBAGE")
            {
                nb.available = true;
            }
        }
        std::string dep;
        if (IsDependencyLine(nb.lines[i], &dep))
        {
            nb.dependencies.push_back(dep);
            UnlockPlacement placement;
            placement.sourceLine = i < nb.sourceLines.size() ? nb.sourceLines[i] : nb.sourceLine;
            nb.placements.push_back(placement);
        }
        if (!t.empty() && t[0] == '@')
        {
            bool before = t.rfind("@before_", 0) == 0;
            bool after = t.rfind("@after_", 0) == 0;
            if ((!before && !after) || nb.placements.empty())
            {
                errRule = "placement-command";
                errReason = "Expected @before_ID or @after_ID following a +dependency";
                return false;
            }
            UnlockPlacement& placement = nb.placements.back();
            std::string anchor = t.substr(before ? 8 : 7);
            if (!placement.anchor.empty() || !IsValidResearchId(anchor) ||
                CanonicalId(anchor) == CanonicalId(nb.id))
            {
                errRule = "placement-anchor";
                errReason = "Only one position per +dependency is allowed; the anchor must be a valid, different research ID";
                return false;
            }
            placement.anchor = anchor;
            placement.after = after;
            placement.sourceLine = i < nb.sourceLines.size() ? nb.sourceLines[i] : nb.sourceLine;
        }
    }

    if (!hasType || !hasCost || !hasName || !hasDesc || !hasAdd)
    {
        errRule = "missing-fields";
        errReason = "Required fields missing: $TYPE, $COST, $NAME, $DESC, $RESEARCH_ADD";
        return false;
    }

    if (nb.available && !nb.dependencies.empty())
    {
        errRule = "available-conflict";
        errReason = "$AVAILABLE or $AVAILABLE_NO_GARBAGE together with +dependency is not allowed";
        return false;
    }

    if (!IsValidResearchId(nb.id))
    {
        errRule = nb.id.empty() ? "empty-id" : "invalid-id";
        errReason = nb.id.empty() ? "Research ID is empty" :
            "Research ID may contain only letters, digits, and underscores: " + nb.id;
        return false;
    }

    std::set<std::string> dependencyIds;
    for (size_t d = 0; d < nb.dependencies.size(); ++d)
    {
        const std::string& dep = nb.dependencies[d];
        nb.validationLine = nb.placements[d].sourceLine;
        if (!IsValidResearchId(dep))
        {
            errRule = "invalid-dependency";
            errReason = dep.empty() ? "Dependency ID is empty" :
                "Dependency ID may contain only letters, digits, and underscores: " + dep;
            return false;
        }
        std::string dependencyKey = CanonicalId(dep);
        if (!dependencyIds.insert(dependencyKey).second)
        {
            errRule = "duplicate-dependency";
            errReason = "Dependency is listed more than once: " + dep;
            return false;
        }
        if (allIds.find(dependencyKey) == allIds.end())
        {
            errRule = "dependency-not-found"; errReason = "Dependency not found: " + dep; return false;
        }
    }

    return true;
}

static bool IsSelfDependency(const NewBlock& nb)
{
    for (const std::string& d : nb.dependencies)
        if (_stricmp(d.c_str(), nb.id.c_str()) == 0) return true;
    return false;
}

static bool HasCircularDependency(const std::string& start,
    const std::map<std::string, std::vector<std::string>>& adj,
    std::set<std::string>& visited,
    std::set<std::string>& recursion)
{
    if (recursion.find(start) != recursion.end()) return true;
    if (visited.find(start) != visited.end()) return false;
    visited.insert(start);
    recursion.insert(start);

    auto it = adj.find(start);
    if (it != adj.end())
        for (const std::string& next : it->second)
            if (HasCircularDependency(next, adj, visited, recursion))
                return true;

    recursion.erase(start);
    return false;
}

static bool BuildGraphAndCheck()
{
    std::map<std::string, std::vector<std::string>> adj;
    for (const NewBlock& nb : g_newBlocks)
        for (const std::string& dep : nb.dependencies)
            adj[CanonicalId(nb.id)].push_back(CanonicalId(dep));

    std::set<std::string> visited, rec;
    for (const NewBlock& nb : g_newBlocks)
    {
        if (IsSelfDependency(nb))
        {
            Report("ERROR", PLUGIN_INI, "self-dependency",
                "Self dependency for research %s", nb.id.c_str());
            return false;
        }
        if (HasCircularDependency(CanonicalId(nb.id), adj, visited, rec))
        {
            Report("ERROR", PLUGIN_INI, "circular-dependency",
                "Circular dependency found for research %s", nb.id.c_str());
            return false;
        }
    }
    return true;
}

static bool ValidateLocalizationAndPlanIcons(std::vector<IconPlan>& plans)
{
    plans.clear();
    if (g_newBlocks.empty()) return true; // Vanilla-only edits need no new icons.
    if (!g_localization || !g_localization->resolveFull) return false;

    for (NewBlock& nb : g_newBlocks)
    {
        int nameId = g_localization->resolveFull(nb.nameKey.c_str());
        int descId = g_localization->resolveFull(nb.descKey.c_str());
        if (!nameId || !descId)
        {
            Report("ERROR", PLUGIN_INI, "localization",
                "Localization key for %s could not be resolved (name='%s', desc='%s')",
                nb.id.c_str(), nb.nameKey.c_str(), nb.descKey.c_str());
            return false;
        }
        if (nameId < (int)TSM_LOCALIZATION_ID_BASE ||
            nameId > (int)TSM_LOCALIZATION_ID_LIMIT ||
            descId < (int)TSM_LOCALIZATION_ID_BASE ||
            descId > (int)TSM_LOCALIZATION_ID_LIMIT)
        {
            Report("ERROR", PLUGIN_INI, "localization-range",
                "Localization IDs for %s are outside the reserved range %u..%u (name=%d, desc=%d)",
                nb.id.c_str(), TSM_LOCALIZATION_ID_BASE,
                TSM_LOCALIZATION_ID_LIMIT, nameId, descId);
            return false;
        }
        nb.nameId = nameId;
        nb.descId = descId;
    }

    // The VFS research folder is the icon store. An icon that is already
    // there is kept (it has to be a valid 128 x 128 PNG); a missing one is
    // seeded from <id>.png in research_expansion\icons beside the DLL, else
    // created from noimage.png. noimage.png is looked for beside the DLL (the
    // Workshop package or a local copy), then in plugins\research_expansion
    // (a hand copy for installs without the Workshop).
    std::string dstIconDir = JoinPath(g_vfsDir, RESEARCH_DIR_REL);
    if (!EnsureDir(dstIconDir))
    {
        Report("ERROR", dstIconDir.c_str(), "vfs-dir",
            "Could not create VFS research folder");
        return false;
    }
    std::string ownDir;
    std::string packageIconDir;
    if (OwnDirectory(ownDir)) packageIconDir = JoinPath(ownDir, "research_expansion\\icons");
    const std::string candidates[] = {
        packageIconDir.empty() ? std::string() : JoinPath(packageIconDir, "noimage.png"),
        JoinPath(g_pluginDir, "research_expansion\\noimage.png"),
        JoinPath(g_pluginDir, "research_expansion\\icons\\noimage.png")
    };
    std::string noimageSrc;
    std::string noimageReason = "no noimage.png found";
    bool noimageOk = false;
    for (const std::string& candidate : candidates)
    {
        if (candidate.empty()) continue;
        std::string reason;
        if (ValidatePng(candidate, reason)) { noimageSrc = candidate; noimageOk = true; break; }
        if (FileExistsA(candidate)) noimageReason = candidate + ": " + reason;
    }
    Info("Icon store: %s", dstIconDir.c_str());
    Info("Fallback icon: %s", noimageOk ? noimageSrc.c_str() : noimageReason.c_str());

    for (NewBlock& nb : g_newBlocks)
    {
        std::string dst = JoinPath(dstIconDir, nb.id + ".png");
        IconPlan plan;
        plan.researchId = nb.id;
        plan.destination = dst;
        if (FileExistsA(dst))
        {
            std::string reason;
            if (!ValidatePng(dst, reason))
            {
                Report("ERROR", dst.c_str(), "icon-invalid",
                    "Icon for %s in the VFS research folder is unusable (%s). Action: replace it with a 128 x 128 PNG or delete it so it is created from noimage.png",
                    nb.id.c_str(), reason.c_str());
                return false;
            }
            plan.keep = true;
            plans.push_back(plan);
            continue;
        }
        std::string seed = packageIconDir.empty() ? std::string() : JoinPath(packageIconDir, nb.id + ".png");
        std::string seedReason;
        if (!seed.empty() && ValidatePng(seed, seedReason))
        {
            plan.source = seed;
            plan.fallback = false;
        }
        else if (noimageOk)
        {
            plan.source = noimageSrc;
            plan.fallback = true;
            Report("WARN", dst.c_str(), "icon-fallback",
                "No icon for %s in the VFS research folder yet; creating %s.png from noimage.png. Action: replace it there with your own 128 x 128 PNG",
                nb.id.c_str(), nb.id.c_str());
        }
        else
        {
            Report("ERROR", PLUGIN_INI, "icon-missing",
                "No icon for %s and no usable noimage.png (%s). Action: put a 128 x 128 PNG named %s.png into the VFS research folder, or provide noimage.png in research_expansion\\icons beside the DLL or in plugins\\research_expansion",
                nb.id.c_str(), noimageReason.c_str(), nb.id.c_str());
            return false;
        }
        plans.push_back(plan);
    }
    return true;
}

static bool ApplyIconPlan(const std::vector<IconPlan>& plans)
{
    for (const IconPlan& plan : plans)
    {
        if (plan.keep)
        {
            Info("Icon for %s kept from the VFS research folder", plan.researchId.c_str());
            continue;
        }
        if (!CopyFileAtomic(plan.source, plan.destination))
        {
            ReportWindows("ERROR", plan.source.c_str(), "icon-copy",
                "Could not install the validated icon atomically in the VFS",
                g_lastWindowsError,
                "Verify that the source icon is readable and the VFS directory is writable");
            return false;
        }
        Info("Icon for %s created from %s",
            plan.researchId.c_str(), plan.fallback ? "noimage.png" : "the package icon");
    }
    return true;
}

static size_t FindUnlock(const std::vector<std::string>& lines,
    const std::string& id, size_t& position)
{
    size_t count = 0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        std::string cmd, value;
        if (IsDirective(lines[i], &cmd, &value) && cmd == "$UNLOCK_RESEARCH" &&
            CanonicalId(value) == CanonicalId(id))
        {
            position = i;
            ++count;
        }
    }
    return count;
}

static std::vector<std::string>* DependencyTarget(const std::string& id, size_t line)
{
    std::string key = CanonicalId(id);
    auto it = g_originalIndex.find(key);
    if (it != g_originalIndex.end())
    {
        OriginalBlock& ob = g_originalBlocks[it->second];
        if (ob.deactivated || g_ambiguousOriginalIds.count(key))
        {
            EditError(id, line, "+dependency", "dependency-target",
                "The target Vanilla block is deactivated or its ID is ambiguous");
            return nullptr;
        }
        ob.editSourceLine = line;
        return &ob.lines;
    }
    for (NewBlock& nb : g_newBlocks)
        if (CanonicalId(nb.id) == key) return &nb.lines;
    EditError(id, line, "+dependency", "dependency-not-found", "Research target not found");
    return nullptr;
}

static bool ApplyDependencyUnlocks()
{
    // First create every unlock, including anchors supplied by later new
    // blocks. Then position the links in INI order. The + lines are untouched.
    for (const NewBlock& nb : g_newBlocks)
        for (size_t d = 0; d < nb.dependencies.size(); ++d)
        {
            auto* lines = DependencyTarget(nb.dependencies[d], nb.placements[d].sourceLine);
            if (!lines) return false;
            size_t position = 0;
            size_t count = FindUnlock(*lines, nb.id, position);
            if (count > 1)
                return EditError(nb.id, nb.placements[d].sourceLine, "+dependency",
                    "duplicate-unlock", "The target block contains this unlock more than once");
            if (!count) lines->insert(lines->end() - 1, "$UNLOCK_RESEARCH " + nb.id);
        }

    std::map<std::string, std::string> afterTails;
    for (const NewBlock& nb : g_newBlocks)
        for (size_t d = 0; d < nb.dependencies.size(); ++d)
        {
            const UnlockPlacement& p = nb.placements[d];
            if (p.anchor.empty()) continue;
            auto* lines = DependencyTarget(nb.dependencies[d], p.sourceLine);
            if (!lines) return false;
            size_t anchor = 0, source = 0;
            if (FindUnlock(*lines, p.anchor, anchor) != 1 || FindUnlock(*lines, nb.id, source) != 1)
                return EditError(nb.id, p.sourceLine, "@position", "placement-target",
                    "Expected exactly one $UNLOCK_RESEARCH " + p.anchor + " in " + nb.dependencies[d]);
            std::string saved = (*lines)[source];
            lines->erase(lines->begin() + source);
            FindUnlock(*lines, p.anchor, anchor);
            std::string tailKey = CanonicalId(nb.dependencies[d]) + ":" + CanonicalId(p.anchor);
            if (p.after && afterTails.count(tailKey))
            {
                size_t tail = 0;
                if (FindUnlock(*lines, afterTails[tailKey], tail) == 1 && tail > anchor)
                    anchor = tail;
            }
            lines->insert(lines->begin() + anchor + (p.after ? 1 : 0), saved);
            if (p.after) afterTails[tailKey] = nb.id;
            Debug("Research '%s', INI line %Iu: inserted unlock in '%s' %s '%s'",
                nb.id.c_str(), p.sourceLine, nb.dependencies[d].c_str(),
                p.after ? "after" : "before", p.anchor.c_str());
        }

    // A later placement must not silently invalidate an earlier instruction.
    // Contradictory/circular position requests fail before any VFS publication.
    for (const NewBlock& nb : g_newBlocks)
        for (size_t d = 0; d < nb.dependencies.size(); ++d)
        {
            const UnlockPlacement& p = nb.placements[d];
            if (p.anchor.empty()) continue;
            auto* lines = DependencyTarget(nb.dependencies[d], p.sourceLine);
            if (!lines) return false;
            size_t anchor = 0, source = 0;
            if (FindUnlock(*lines, p.anchor, anchor) != 1 || FindUnlock(*lines, nb.id, source) != 1 ||
                (p.after ? source <= anchor : source >= anchor))
                return EditError(nb.id, p.sourceLine, "@position", "placement-conflict",
                    "Position instructions conflict; review their anchors and INI order");
        }
    return true;
}

// ---------------------------------------------------------------- scoped Vanilla edits

static size_t FindCompleteLine(const std::vector<std::string>& lines,
    const std::string& needle, size_t& position)
{
    size_t count = 0;
    for (size_t i = 0; i < lines.size(); ++i)
        if (TrimA(lines[i]) == TrimA(needle)) { position = i; ++count; }
    return count;
}

static bool PrepareEditLine(const Modification& mod, const EditOperation& op,
    std::string& line)
{
    std::string cmd, value;
    if (IsOnlyDashes(line)) return true;
    if (IsDependencyLine(line, &value))
    {
        if (IsValidResearchId(value)) return true;
        return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-dependency", "Invalid +dependency ID");
    }
    const DirectiveSpec* spec = IsDirective(line, &cmd, &value) ? FindDirectiveSpec(cmd) : nullptr;
    if (!spec || IsStructuralLine(line) || (spec->requiresValue == value.empty()))
        return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-directive",
            "New lines must use a supported non-structural directive with the required value/flag syntax: " + line);
    if (cmd == "$NAME" || cmd == "$DESC")
    {
        int id = 0;
        if (ParsePositiveInt(value, &id))
        {
            if (id >= (int)TSM_LOCALIZATION_ID_BASE)
                return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-localization",
                    "Use a namespace.key for plugin texts; numeric Vanilla IDs must be below 2000000");
        }
        else
        {
            id = g_localization && g_localization->resolveFull ? g_localization->resolveFull(value.c_str()) : 0;
            if (id < (int)TSM_LOCALIZATION_ID_BASE || id > (int)TSM_LOCALIZATION_ID_LIMIT)
                return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-localization",
                    "Localization key did not resolve inside 2000000..2999999: " + value);
        }
        line = cmd + " " + std::to_string(id);
    }
    return true;
}

typedef std::map<std::string, std::vector<std::string>> InsertionTails;

static void InvalidateInsertionTail(InsertionTails& tails, const std::string& changed)
{
    tails.erase(TrimA(changed));
    for (auto& tail : tails)
        tail.second.erase(std::remove(tail.second.begin(), tail.second.end(), TrimA(changed)), tail.second.end());
}

static bool ApplyEditOperation(OriginalBlock& block, const Modification& mod,
    const EditOperation& op, InsertionTails& afterTails)
{
    auto& lines = block.lines;
    bool moving = op.command == "move_before" || op.command == "move_after";
    bool inserting = op.command == "insert_before" || op.command == "insert_after";
    size_t first = 0, anchor = 0;
    if (op.command != "add" && FindCompleteLine(lines, op.first, first) != 1)
        return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-match",
            "Expected exactly one complete matching line: " + op.first);
    if (moving && FindCompleteLine(lines, op.second, anchor) != 1)
        return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-anchor",
            "Expected exactly one complete anchor line: " + op.second);
    if (moving && first == anchor)
        return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-anchor", "A line cannot be moved relative to itself");

    std::string newLine = op.command == "add" ? op.first : op.second;
    if (op.command == "add" || op.command == "replace" || inserting)
    {
        if (!PrepareEditLine(mod, op, newLine)) return false;
        size_t existing = 0;
        size_t count = FindCompleteLine(lines, newLine, existing);
        if (!IsOnlyDashes(newLine) && count && !(op.command == "replace" && count == 1 && existing == first))
            return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-duplicate", "The new line already exists: " + newLine);
    }

    if (op.command == "remove")
    {
        InvalidateInsertionTail(afterTails, lines[first]);
        lines.erase(lines.begin() + first);
    }
    else if (op.command == "replace")
    {
        if (TrimA(lines[first]) != newLine) InvalidateInsertionTail(afterTails, lines[first]);
        lines[first] = newLine;
    }
    else if (op.command == "add") lines.insert(lines.end() - 1, newLine);
    else
    {
        bool after = op.command == "insert_after" || op.command == "move_after";
        std::string anchorLine = moving ? op.second : op.first;
        if (moving)
        {
            newLine = lines[first];
            InvalidateInsertionTail(afterTails, newLine);
            lines.erase(lines.begin() + first);
        }
        FindCompleteLine(lines, anchorLine, anchor);
        if (after && !moving && afterTails.count(anchorLine))
        {
            const auto& candidates = afterTails[anchorLine];
            for (auto it = candidates.rbegin(); it != candidates.rend(); ++it)
            {
                size_t tail = 0;
                if (FindCompleteLine(lines, *it, tail) == 1 && tail > anchor)
                { anchor = tail; break; }
            }
        }
        size_t destination = anchor + (after ? 1 : 0);
        if (destination == 0 || destination >= lines.size())
            return EditError(mod.id, op.sourceLine, op.command.c_str(), "modify-boundary",
                "Insertion/movement must stay between $RESEARCH and $RESEARCH_ADD");
        lines.insert(lines.begin() + destination, newLine);
        if (after && !moving) afterTails[anchorLine].push_back(TrimA(newLine));
    }
    block.editSourceLine = op.sourceLine;
    Debug("Research '%s', INI line %Iu: staged %s = %s%s%s",
        mod.id.c_str(), op.sourceLine, op.command.c_str(), op.first.c_str(),
        op.second.empty() ? "" : " | ", op.second.c_str());
    return true;
}

static bool HasActiveModifications()
{
    for (const Modification& mod : g_modifications)
        if (mod.enabled && (!mod.operations.empty() || !mod.cost.empty())) return true;
    return false;
}

static bool ApplyVanillaModifications()
{
    size_t edits = 0, blocks = 0;
    for (const Modification& mod : g_modifications)
    {
        if (!mod.enabled) { Debug("Modification '%s' disabled", mod.id.c_str()); continue; }
        auto found = g_originalIndex.find(CanonicalId(mod.id));
        if (found == g_originalIndex.end() || g_ambiguousOriginalIds.count(CanonicalId(mod.id)))
            return EditError(mod.id, mod.sourceLine, "section", "modify-target",
                "Expected exactly one existing Vanilla research block");
        OriginalBlock& block = g_originalBlocks[found->second];
        if (block.deactivated)
            return EditError(mod.id, mod.sourceLine, "section", "modify-deactivated", "Deactivated Vanilla research is protected");
        InsertionTails afterTails;
        if (!mod.cost.empty())
        {
            // the cost key becomes a replace of the block's single $COST line.
            size_t hits = 0; std::string current;
            for (const std::string& line : block.lines)
            {
                std::string t = TrimA(line);
                if (t.size() > 6 && t.compare(0, 6, "$COST ") == 0) { ++hits; current = t; }
            }
            if (hits != 1)
                return EditError(mod.id, mod.costLine, "cost", "modify-cost",
                    "Expected exactly one $COST line in the Vanilla block");
            EditOperation op;
            op.command = "replace";
            op.first = current;
            op.second = "$COST " + mod.cost;
            op.sourceLine = mod.costLine;
            if (!ApplyEditOperation(block, mod, op, afterTails)) return false;
            ++edits;
        }
        for (const EditOperation& op : mod.operations)
        {
            if (!ApplyEditOperation(block, mod, op, afterTails)) return false;
            ++edits;
        }
        if (!mod.operations.empty() || !mod.cost.empty()) ++blocks;
    }
    if (blocks) Info("Staged %Iu edit operation(s) in %Iu Vanilla block(s); final validation pending", edits, blocks);
    return true;
}

// ---------------------------------------------------------------- final-state validation

typedef std::map<std::string, std::set<std::string>> ResearchGraph;

static void CollectResearchEdges(const std::string& id,
    const std::vector<std::string>& lines, ResearchGraph& graph)
{
    for (const std::string& line : lines)
    {
        std::string cmd, value;
        if (IsDirective(line, &cmd, &value) && cmd == "$UNLOCK_RESEARCH")
            graph[CanonicalId(id)].insert(CanonicalId(value));
        else if (IsDependencyLine(line, &value))
            graph[CanonicalId(value)].insert(CanonicalId(id));
    }
}

static bool ReachesResearch(const ResearchGraph& graph, const std::string& from,
    const std::string& target)
{
    // Iterative traversal also handles pre-existing Vanilla cycles without
    // risking unbounded recursion on user-controlled links.
    std::vector<std::string> pending(1, from);
    std::set<std::string> visited;
    while (!pending.empty())
    {
        std::string current = pending.back();
        pending.pop_back();
        if (current == target) return true;
        if (!visited.insert(current).second) continue;
        auto found = graph.find(current);
        if (found != graph.end())
            for (const std::string& next : found->second) pending.push_back(next);
    }
    return false;
}

static bool ValidateFinalBlock(const std::string& id, const std::vector<std::string>& lines,
    size_t sourceLine, bool vanilla)
{
    NewBlock check;
    check.id = id;
    check.sourceLine = sourceLine;
    for (const std::string& line : lines)
    {
        std::string t = TrimA(line);
        if (!t.empty() && t[0] == '@') continue; // Already validated internal placement.
        check.lines.push_back(line);
    }
    std::string rule, reason;
    if (!ValidateNewBlock(check, g_allIds, rule, reason, vanilla))
        return EditError(id, sourceLine, "final validation", rule.c_str(), reason);
    std::set<std::string> unlocks;
    for (const std::string& line : lines)
    {
        std::string cmd, value;
        if (IsDirective(line, &cmd, &value) && cmd == "$UNLOCK_RESEARCH" &&
            (!IsValidResearchId(value) || !unlocks.insert(CanonicalId(value)).second))
            return EditError(id, sourceLine, "final validation", "invalid-unlock",
                "Invalid or duplicate research unlock: " + value);
    }
    return true;
}

static bool ValidateFinalResearch()
{
    ResearchGraph baseline, effective;
    std::set<std::string> activeIds;
    for (const OriginalBlock& ob : g_originalBlocks)
    {
        if (ob.deactivated) continue;
        activeIds.insert(CanonicalId(ob.id));
        CollectResearchEdges(ob.id, ob.baseline, baseline);
        CollectResearchEdges(ob.id, ob.lines, effective);
        if (ob.lines != ob.baseline && !ValidateFinalBlock(ob.id, ob.lines, ob.editSourceLine, true)) return false;
    }
    for (const NewBlock& nb : g_newBlocks)
    {
        activeIds.insert(CanonicalId(nb.id));
        CollectResearchEdges(nb.id, nb.lines, effective);
        if (!ValidateFinalBlock(nb.id, nb.lines, nb.sourceLine, false)) return false;
    }

    // Preserve untouched Vanilla peculiarities, but reject every newly added
    // dangling/ambiguous link and every cycle created by an added graph edge.
    // Both native + lines and $UNLOCK_RESEARCH contribute to the final graph.
    for (const auto& source : effective)
        for (const std::string& target : source.second)
        {
            auto old = baseline.find(source.first);
            if (old != baseline.end() && old->second.count(target)) continue;
            size_t line = 0;
            auto original = g_originalIndex.find(source.first);
            if (original != g_originalIndex.end()) line = g_originalBlocks[original->second].editSourceLine;
            for (const NewBlock& nb : g_newBlocks)
                if (CanonicalId(nb.id) == source.first || (!line && CanonicalId(nb.id) == target)) line = nb.sourceLine;
            if (!activeIds.count(source.first) || !activeIds.count(target) ||
                g_ambiguousOriginalIds.count(source.first) || g_ambiguousOriginalIds.count(target))
                return EditError(source.first, line, "final links", "research-target",
                    "New link has a missing, deactivated or ambiguous endpoint: " + source.first + " -> " + target);
            if (ReachesResearch(effective, target, source.first))
                return EditError(source.first, line, "final links", "research-cycle",
                    "New link creates a cycle: " + source.first + " -> " + target);
        }
    return true;
}

static bool LoadAndParseVanilla(std::string& baseContent)
{
    std::string basePath = JoinPath(g_mediaDir, "research\\research.ini");
    if (!FileExistsA(basePath))
    {
        Report("ERROR", "research.ini", "file", "Original research.ini not found");
        return false;
    }
    if (!ReadTextFile(basePath, baseContent))
    {
        ReportWindows("ERROR", basePath.c_str(), "file",
            "Original research.ini could not be read", g_lastWindowsError,
            "Verify that the Vanilla research file is readable and unmodified");
        return false;
    }
    return ParseOriginalBlocks(baseContent);
}

static bool ValidateAllNewBlocks()
{
    std::set<std::string> newIds;
    size_t links = 0;
    for (const NewBlock& nb : g_newBlocks)
    {
        for (const std::string& line : nb.lines)
        {
            std::string cmd;
            if (IsDependencyLine(line, nullptr) ||
                (IsDirective(line, &cmd) && cmd == "$UNLOCK_RESEARCH"))
                if (++links > MAX_NEW_LINKS)
                    return EditError(nb.id, nb.sourceLine, "new research", "link-limit",
                        "At most 4096 +dependencies and $UNLOCK_RESEARCH lines are allowed across new blocks");
        }
        std::string key = CanonicalId(nb.id);
        if (g_allIds.find(key) != g_allIds.end() || !newIds.insert(key).second)
        {
            Report("ERROR", PLUGIN_INI, "duplicate-id",
                "Research ID '%s' already exists; ID comparisons are case-insensitive",
                nb.id.c_str());
            return false;
        }
    }

    g_allIds.insert(newIds.begin(), newIds.end());

    for (NewBlock& nb : g_newBlocks)
    {
        std::string rule, reason;
        if (!ValidateNewBlock(nb, g_allIds, rule, reason))
        {
            return EditError(nb.id, nb.validationLine ? nb.validationLine : nb.sourceLine,
                "new research", rule.c_str(), reason);
        }
    }
    return BuildGraphAndCheck();
}

static bool MergeResearchContent(const std::string& baseContent,
    std::string& out, size_t iconPlanCount)
{
    std::vector<std::string> originalLines;
    std::istringstream input(baseContent);
    std::string line;
    while (std::getline(input, line))
        originalLines.push_back(line);

    std::vector<std::string> lines;
    size_t lineIdx = 0;
    for (const OriginalBlock& ob : g_originalBlocks)
    {
        while (lineIdx < ob.start && lineIdx < originalLines.size())
            lines.push_back(originalLines[lineIdx++]);

        lineIdx = ob.end + 1;
        if (!ob.deactivated)
        {
            lines.insert(lines.end(), ob.lines.begin(), ob.lines.end());
        }
        else
        {
            for (size_t i = ob.start; i <= ob.end && i < originalLines.size(); ++i)
                lines.push_back(originalLines[i]);
        }
    }

    while (lineIdx < originalLines.size())
        lines.push_back(originalLines[lineIdx++]);

    size_t insertPos = lines.size();
    for (size_t i = lines.size(); i > 0; --i)
    {
        if (TrimA(lines[i - 1]) == "$RESEARCH_ADD")
        {
            insertPos = i;
            break;
        }
    }

    for (size_t i = 0; i < lines.size(); ++i)
    {
        // The game file's marker takes precedence over the last research block.
        if (TrimA(lines[i]) == RESEARCH_INSERT_MARKER)
        {
            insertPos = i;
            break;
        }
    }

    std::vector<std::string> newBlockLines;
    for (const NewBlock& nb : g_newBlocks)
    {
        if (!newBlockLines.empty()) newBlockLines.push_back("");
        for (std::string outputLine : nb.lines)
        {
            std::string trimmed = TrimA(outputLine);
            // Native +dependencies remain byte-for-byte as configured. Only
            // @placement commands are internal and omitted from game output.
            if (!trimmed.empty() && trimmed[0] == '@') continue;
            if (trimmed.rfind("$NAME", 0) == 0)
                outputLine = "$NAME " + std::to_string(nb.nameId);
            else if (trimmed.rfind("$DESC", 0) == 0)
                outputLine = "$DESC " + std::to_string(nb.descId);
            newBlockLines.push_back(outputLine);
        }
    }

    lines.insert(lines.begin() + insertPos, newBlockLines.begin(), newBlockLines.end());

    out.clear();
    out.reserve(baseContent.size() + 4096u * g_newBlocks.size());
    for (const std::string& outputLine : lines)
        out += outputLine + "\n";

    Debug("Generated research.ini in memory: vanilla_bytes=%Iu output_bytes=%Iu insert_line=%Iu icon_plans=%Iu",
        baseContent.size(), out.size(), insertPos, iconPlanCount);
    return true;
}

static bool BuildCombinedOutput(std::string& out, std::vector<IconPlan>& iconPlans)
{
    try
    {
        out.clear();
        iconPlans.clear();

        std::string baseContent;
        if (!LoadAndParseVanilla(baseContent))
            return false;
        if (!ValidateAllNewBlocks())
            return false;
        if (!ApplyDependencyUnlocks())
            return false;
        if (!ApplyVanillaModifications() || !ValidateFinalResearch())
            return false;
        if (!ValidateLocalizationAndPlanIcons(iconPlans))
            return false;
        return MergeResearchContent(baseContent, out, iconPlans.size());
    }
    catch (const std::exception& e)
    {
        Report("ERROR", "research_expansion", "exception",
            "C++ exception during output generation: %s", e.what());
        return false;
    }
    catch (...)
    {
        Report("ERROR", "research_expansion", "exception",
            "Unknown C++ exception during output generation");
        return false;
    }
}

// ---------------------------------------------------------------- plugin entry

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion(void)
{
    return TSM_API_VERSION;
}

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host, TsmPluginInfo* info)
{
    if (!host || !info) return 1;
    TsmBind(host);
    g_bound = true;
    info->name = "research_expansion";
    info->version = PLUGIN_VERSION;

    g_logStarted = GetTickCount64();
    BeginLogPhase();
    g_detail = TsmOpenLog(PLUGIN_LOG_NAME);
    if (g_detail == INVALID_HANDLE_VALUE)
        ReportWindows("WARN", PLUGIN_LOG_NAME, "log-open",
            "Detail log could not be opened", GetLastError(),
            "Verify write access to the TesmioLoader log directory");
    Info("TesmioLoader research_expansion %s starting; detail log: %s",
        PLUGIN_VERSION, PLUGIN_LOG_NAME);

    try
    {
        g_vfsDir = g_vfsRoot ? g_vfsRoot : "";
        if (g_vfsDir.empty() || !IsAbsoluteWindowsPath(g_vfsDir))
        {
            Report("FATAL", NULL, "vfs-root",
                "TesmioLoader did not provide an absolute VFS root. Action: verify the loader version and VFS configuration");
            LogSummary("Initialization", false);
            return 1;
        }

        std::string vfsResearchDir = JoinPath(g_vfsDir, RESEARCH_DIR_REL);
        if (!EnsureDir(vfsResearchDir))
        {
            ReportWindows("FATAL", vfsResearchDir.c_str(), "vfs-dir",
                "Could not prepare the VFS research directory", g_lastWindowsError,
                "Verify write access and the TesmioLoader VFS configuration");
            LogSummary("Initialization", false);
            return 1;
        }

        // Fail closed before parsing user content. If initialization, parsing,
        // localization, icon validation, or generation fails later, no stale
        // VFS override remains and the game reads its Vanilla research.ini.
        if (!RemoveGeneratedResearch())
        {
            LogSummary("Initialization", false);
            return 1;
        }

        if (!g_baseDir || !*g_baseDir)
        {
            Report("FATAL", NULL, "plugin-folder",
                "TesmioLoader did not provide its base directory. Action: verify the loader installation");
            LogSummary("Initialization", false);
            return 1;
        }
        g_pluginDir = JoinPath(g_baseDir, "plugins");

        // <loader>\plugins\research_expansion.ini when it exists (the classic
        // install, or the effective INI Republic Mod Manager writes), otherwise the
        // INI beside the DLL - the Workshop package under Soviet Mod Loader or the
        // Workshop Bridge. The icon folder follows the same rule (see icons).
        std::string pluginIniPath = JoinPath(g_pluginDir, "research_expansion.ini");
        std::string ownDir;
        if (!FileExistsA(pluginIniPath) && OwnDirectory(ownDir) && FileExistsA(JoinPath(ownDir, "research_expansion.ini")))
            pluginIniPath = JoinPath(ownDir, "research_expansion.ini");
        Info("Configuration file: %s", pluginIniPath.c_str());
        std::string pluginIni;
        if (!ReadTextFile(pluginIniPath, pluginIni))
        {
            ReportWindows("FATAL", PLUGIN_INI, "file",
                "Plugin INI could not be read", g_lastWindowsError,
                "Verify that plugins\\research_expansion.ini exists and is readable");
            LogSummary("Initialization", false);
            return 1;
        }
        if (!ValidateGeneralConfigLayout(pluginIni, &g_modifications))
        {
            Report("FATAL", PLUGIN_INI, "general-config",
                "General configuration is invalid. Action: correct the preceding section, key, or value error; Vanilla research remains active");
            LogSummary("Initialization", false);
            return 1;
        }

        const bool enabled =
            H->configInt(PLUGIN_INI, "general", "enabled", 1) != 0;
        g_debug = H->configInt(PLUGIN_INI, "general", "debug", 0);

        if (!enabled)
        {
            Info("enabled = 0; generated override removed and Vanilla research remains active");
            LogSummaryStatus("Initialization", "was skipped because the plugin is disabled");
            return 1;
        }

        if (!ResolveGameFolders())
        {
            Report("FATAL", NULL, "game-folder",
                "media_soviet was not found next to SOVIET64.exe. Action: verify the selected game directory");
            LogSummary("Initialization", false);
            return 1;
        }

        if (!ReadGeneralConfig())
        {
            Report("FATAL", PLUGIN_INI, "general-config",
                "General configuration is invalid. Action: correct the preceding path or length error");
            LogSummary("Initialization", false);
            return 1;
        }

        Info("Configuration: enabled=1 debug=%d max_research=%d required_icon=%Iux%Iu; icons live in the VFS research folder (" PLUGIN_VERSION ")",
            g_debug, MAX_NEW_RESEARCH, REQUIRED_ICON_SIDE, REQUIRED_ICON_SIDE);
        Debug("Paths: game='%s' plugin='%s' vfs='%s'",
            g_gameDir.c_str(), g_pluginDir.c_str(), g_vfsDir.c_str());

        std::vector<NewBlock> blocks;
        if (!ParseNewBlocks(pluginIni, blocks) || !ExpandResearchSections(blocks))
        {
            Report("FATAL", PLUGIN_INI, "parse",
                "Research configuration is invalid. Action: correct the preceding configuration errors; Vanilla research remains active");
            LogSummary("Initialization", false);
            return 1;
        }
        g_newBlocks.swap(blocks);

        if (g_newBlocks.empty() && !HasActiveModifications())
        {
            Info("No active research blocks or edit operations; generated override removed and Vanilla research remains active");
            LogSummaryStatus("Initialization", "was skipped because no changes are configured");
            return 1;
        }

        Info("%Iu new research entr(y/ies) (%Iu of them [research:] sections), %Iu modification section(s) loaded; deactivated Vanilla research remains protected",
            g_newBlocks.size(), g_researchSections.size(), g_modifications.size());
        LogSummary("Initialization", true);
        return 0;
    }
    catch (const std::exception& e)
    {
        Report("FATAL", "research_expansion", "exception",
            "C++ exception in TsmPluginInit: %s. Action: preserve this log and report the failure; Vanilla research remains active",
            e.what());
        LogSummary("Initialization", false);
        return 1;
    }
    catch (...)
    {
        Report("FATAL", "research_expansion", "exception",
            "Unknown C++ exception in TsmPluginInit. Action: preserve this log and report the failure; Vanilla research remains active");
        LogSummary("Initialization", false);
        return 1;
    }
}

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    BeginLogPhase();
    try
    {
        if (!H || !g_bound || g_vfsDir.empty() ||
            (g_newBlocks.empty() && !HasActiveModifications()))
        {
            Report("FATAL", NULL, "startup-state",
                "Required initialized plugin state is unavailable. Action: verify the loader log; Vanilla research remains active");
            LogSummary("Startup", false);
            return 1;
        }

        g_localization = H->consume ? (const TsmLocalizationApi*)H->consume(
            TSM_SERVICE_LOCALIZATION, TSM_LOCALIZATION_VERSION) : nullptr;
        if (!g_localization || !g_localization->resolveFull)
        {
            Report("FATAL", NULL, "localization",
                "Localization service is missing or incomplete. Action: enable the Localization plugin; Vanilla research remains active");
            LogSummary("Startup", false);
            return 1;
        }

        std::string vfsResearchDir = JoinPath(g_vfsDir, RESEARCH_DIR_REL);
        if (!EnsureDir(vfsResearchDir))
        {
            ReportWindows("FATAL", vfsResearchDir.c_str(), "vfs-dir",
                "Could not create the VFS research directory", g_lastWindowsError,
                "Verify write access and the TesmioLoader VFS configuration");
            LogSummary("Startup", false);
            return 1;
        }
        std::string output;
        std::vector<IconPlan> iconPlans;
        if (!BuildCombinedOutput(output, iconPlans))
        {
            Report("FATAL", NULL, "generation",
                "Research expansion was not applied. Action: correct the preceding research, icon, or localization errors; Vanilla research remains active");
            LogSummary("Startup", false);
            return 1;
        }

        if (!ApplyIconPlan(iconPlans))
        {
            Report("FATAL", NULL, "icon-installation",
                "Research expansion was not applied because an icon could not be installed. Action: correct the preceding file error; Vanilla research remains active");
            LogSummary("Startup", false);
            return 1;
        }

        std::string outPath = JoinPath(g_vfsDir, RESEARCH_INI_REL);
        if (!WriteTextFileAtomic(outPath, output))
        {
            ReportWindows("FATAL", outPath.c_str(), "write",
                "Could not write the extended research.ini", g_lastWindowsError,
                "Close programs using the file and verify write access to the VFS directory; Vanilla research remains active");
            LogSummary("Startup", false);
            return 1;
        }

        Info("%d new research entries and configured Vanilla edits activated successfully; output='%s' bytes=%Iu total_elapsed_ms=%llu",
            (int)g_newBlocks.size(), outPath.c_str(), output.size(),
            (unsigned long long)(GetTickCount64() - g_logStarted));
        LogSummary("Startup", true);
        return 0;
    }
    catch (const std::exception& e)
    {
        Report("FATAL", "research_expansion", "exception",
            "C++ exception in TsmPluginStart: %s. Action: preserve this log and report the failure; Vanilla research remains active",
            e.what());
        LogSummary("Startup", false);
        return 1;
    }
    catch (...)
    {
        Report("FATAL", "research_expansion", "exception",
            "Unknown C++ exception in TsmPluginStart. Action: preserve this log and report the failure; Vanilla research remains active");
        LogSummary("Startup", false);
        return 1;
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH)
    {
        if (g_detail != INVALID_HANDLE_VALUE) { CloseHandle(g_detail); g_detail = INVALID_HANDLE_VALUE; }
        if (g_bound) DeleteCriticalSection(&g_lock);
    }
    return TRUE;
}
