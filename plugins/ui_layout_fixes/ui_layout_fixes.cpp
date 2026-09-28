// ui_layout_fixes.cpp
//
// Window-specific UI corrections for Workers & Resources: Soviet Republic.
// Every module owns its addresses, validation, configuration and activation.
// There is intentionally no global row-spacing option.

// CUSTOMHOUSE module (SOVIET64.exe 1.1.1.9):
// The native resource-column renderer uses a 25-logical-pixel row pitch. The
// customs-house panel invokes that renderer once to measure its list and once
// to draw it. This module redirects the renderer's pitch operand to a neutral
// plugin-owned slot, then changes that slot only while either of those two
// verified CUSTOMHOUSE calls is executing. The measurement rectangle, scroll
// range and all sections below the list therefore continue to be positioned by
// the game's native layout code. Every other caller still reads exactly 25.0f.

#if __has_include("../../src/tesmio_plugin.h")
#include "../../src/tesmio_plugin.h"
#else
#include "../tesmio_plugin.h"
#endif

#include "../tesmio_config.h"
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <wchar.h>

#include <exception>

namespace UiLayoutFixes
{
static const char* const PLUGIN_VERSION = "0.3.0";
// Configuration comes through tesmio_config.h: <loader>\plugins\ui_layout_fixes.ini
static const char* const LOG_NAME = "tesmioloader.ui_layout_fixes.log";

static int g_enabled = 1;
static bool g_bound = false;
static HANDLE g_detail = INVALID_HANDLE_VALUE;
static volatile LONG g_logWarnings;
static volatile LONG g_logErrors;
static volatile LONG g_logFatals;
static ULONGLONG g_logStarted;
static ULONGLONG g_phaseStarted;
static LONG g_phaseWarnings;
static LONG g_phaseErrors;
static LONG g_phaseFatals;

// -------------------------------------------------------------------------
// Logging

static void DetailWriteLine(const char* text)
{
    if (g_detail == INVALID_HANDLE_VALUE || !text) return;

    if (g_bound) EnterCriticalSection(&g_lock);

    SYSTEMTIME time;
    GetLocalTime(&time);

    char stamp[64];
    _snprintf_s(stamp, sizeof(stamp), _TRUNCATE,
        "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        time.wYear, time.wMonth, time.wDay,
        time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

    DWORD written = 0;
    WriteFile(g_detail, stamp, (DWORD)strlen(stamp), &written, nullptr);
    WriteFile(g_detail, text, (DWORD)strlen(text), &written, nullptr);
    WriteFile(g_detail, "\r\n", 2, &written, nullptr);

    if (g_bound) LeaveCriticalSection(&g_lock);
}

static void CountLevel(const char* level)
{
    if (!level) return;
    if (_stricmp(level, "WARN") == 0)
        InterlockedIncrement(&g_logWarnings);
    else if (_stricmp(level, "ERROR") == 0)
        InterlockedIncrement(&g_logErrors);
    else if (_stricmp(level, "FATAL") == 0)
        InterlockedIncrement(&g_logFatals);
}

static void ReportV(const char* level, const char* module, const char* rule,
                    const char* format, va_list args)
{
    char body[2048];
    _vsnprintf_s(body, sizeof(body), _TRUNCATE, format, args);

    char full[3072];
    _snprintf_s(full, sizeof(full), _TRUNCATE, "%s: %s [%s] %s",
        level ? level : "INFO", module ? module : "-",
        rule ? rule : "general", body);

    CountLevel(level);
    if (H) Logf("ui_layout_fixes  %s", full);
    DetailWriteLine(full);
}

static void LogInfo(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    ReportV("INFO", nullptr, "status", format, args);
    va_end(args);
}

static void LogWarning(const char* module, const char* rule,
                       const char* format, ...)
{
    va_list args;
    va_start(args, format);
    ReportV("WARN", module, rule, format, args);
    va_end(args);
}

static void LogError(const char* module, const char* rule,
                     const char* format, ...)
{
    va_list args;
    va_start(args, format);
    ReportV("ERROR", module, rule, format, args);
    va_end(args);
}

static void LogFatal(const char* module, const char* rule,
                     const char* format, ...)
{
    va_list args;
    va_start(args, format);
    ReportV("FATAL", module, rule, format, args);
    va_end(args);
}

static void ReportWindows(const char* level, const char* module,
                          const char* rule, const char* action,
                          DWORD error, const char* remedy)
{
    char systemText[512] = {};
    DWORD length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, systemText, (DWORD)sizeof(systemText), nullptr);
    while (length &&
           (systemText[length - 1] == '\r' || systemText[length - 1] == '\n' ||
            systemText[length - 1] == ' ' || systemText[length - 1] == '.'))
    {
        systemText[--length] = 0;
    }
    if (!length)
        strcpy_s(systemText, sizeof(systemText), "Unknown Windows error");

    char detail[1536];
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
        "%s (Windows error %lu: %s). Action: %s",
        action, error, systemText,
        remedy ? remedy : "Review the preceding context and retry");

    if (level && _stricmp(level, "WARN") == 0)
        LogWarning(module, rule, "%s", detail);
    else if (level && _stricmp(level, "FATAL") == 0)
        LogFatal(module, rule, "%s", detail);
    else
        LogError(module, rule, "%s", detail);
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
    const ULONGLONG elapsed =
        g_phaseStarted ? GetTickCount64() - g_phaseStarted : 0;
    LogInfo("%s %s after %llu ms; %ld warning(s), %ld error(s), "
            "%ld fatal error(s)",
        phase, status, (unsigned long long)elapsed,
        AtomicRead(&g_logWarnings) - g_phaseWarnings,
        AtomicRead(&g_logErrors) - g_phaseErrors,
        AtomicRead(&g_logFatals) - g_phaseFatals);
}

static void LogSummary(const char* phase, bool success)
{
    LogSummaryStatus(phase,
        success ? "completed successfully" : "failed");
}

// -------------------------------------------------------------------------
// Configuration and executable validation

static bool ReadIniFloat(const char* section, const char* key,
                         float fallback, float minimum, float maximum,
                         float* output)
{
    if (!output) return false;

    char fallbackText[64];
    char valueText[128];
    _snprintf_s(fallbackText, sizeof(fallbackText), _TRUNCATE,
                "%.3f", fallback);
    if (!TsmConfigString(section, key, valueText, (int)sizeof(valueText), fallbackText))
    {
        *output = fallback;
        return true;
    }

    char* end = nullptr;
    errno = 0;
    const double parsed = strtod(valueText, &end);
    const int parseError = errno;
    while (end && (*end == ' ' || *end == '\t' ||
                   *end == '\r' || *end == '\n'))
    {
        ++end;
    }

    if (!end || end == valueText || *end != 0 || parseError == ERANGE ||
        _finite(parsed) == 0 ||
        parsed < minimum || parsed > maximum)
    {
        LogWarning(section, "invalid-config",
            "%s=%s is invalid; expected a finite value in %.1f..%.1f. "
            "Action: using %.1f",
            key, valueText, minimum, maximum, fallback);
        *output = fallback;
        return false;
    }

    *output = (float)parsed;
    return true;
}

static DWORD ExeTimestamp()
{
    if (!g_exeBase || g_exeSize < sizeof(IMAGE_DOS_HEADER) ||
        !ReadablePtr(g_exeBase, sizeof(IMAGE_DOS_HEADER)))
        return 0;

    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)g_exeBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return 0;

    const SIZE_T ntOffset = (SIZE_T)dos->e_lfanew;
    if (ntOffset > g_exeSize ||
        sizeof(IMAGE_NT_HEADERS64) > g_exeSize - ntOffset)
    {
        return 0;
    }

    BYTE* ntAddress = g_exeBase + ntOffset;
    if (!ReadablePtr(ntAddress, sizeof(IMAGE_NT_HEADERS64)))
        return 0;

    const IMAGE_NT_HEADERS64* nt =
        (const IMAGE_NT_HEADERS64*)ntAddress;
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        return 0;
    }
    return nt->FileHeader.TimeDateStamp;
}

static bool VerifyBytes(DWORD rva, const BYTE* expected, size_t length,
                        const char* module, const char* rule,
                        const char* label)
{
    if (!g_exeBase || !expected || length == 0 || rva > g_exeSize ||
        length > g_exeSize - rva ||
        !ReadablePtr(g_exeBase + rva, length))
    {
        LogError(module, rule,
            "%s at SOVIET64.exe+0x%X is outside the readable image",
            label, rva);
        return false;
    }

    if (memcmp(g_exeBase + rva, expected, length) != 0)
    {
        LogError(module, rule,
            "%s signature changed at SOVIET64.exe+0x%X; no patch was written",
            label, rva);
        return false;
    }
    return true;
}

static bool VerifyRelativeCall(DWORD callRva, DWORD targetRva,
                               const char* module, const char* rule,
                               const char* label)
{
    static const BYTE directCall = 0xE8;
    if (!VerifyBytes(callRva, &directCall, 1, module, rule, label))
        return false;

    if (callRva > g_exeSize || 5 > g_exeSize - callRva ||
        targetRva >= g_exeSize)
    {
        LogError(module, rule,
            "%s or its expected target is outside SOVIET64.exe",
            label);
        return false;
    }

    BYTE* call = g_exeBase + callRva;
    if (!ReadablePtr(call, 5))
    {
        LogError(module, rule,
            "%s at SOVIET64.exe+0x%X is not fully readable",
            label, callRva);
        return false;
    }

    LONG displacement = 0;
    memcpy(&displacement, call + 1, sizeof(displacement));
    const INT64 actualTarget =
        (INT64)(uintptr_t)(call + 5) + (INT64)displacement;
    const INT64 expectedTarget =
        (INT64)(uintptr_t)(g_exeBase + targetRva);
    if (actualTarget != expectedTarget)
    {
        LogError(module, rule,
            "%s at SOVIET64.exe+0x%X no longer targets +0x%X; no patch was written",
            label, callRva, targetRva);
        return false;
    }
    return true;
}

static bool Rel32(BYTE* instruction, BYTE* target, LONG* displacement)
{
    if (!instruction || !target || !displacement) return false;
    const INT64 distance = (INT64)(uintptr_t)target -
                           (INT64)(uintptr_t)(instruction + 5);
    if (distance < INT_MIN || distance > INT_MAX) return false;
    *displacement = (LONG)distance;
    return true;
}

static bool Rip32(BYTE* instructionEnd, BYTE* target, LONG* displacement)
{
    if (!instructionEnd || !target || !displacement) return false;
    const INT64 distance = (INT64)(uintptr_t)target -
                           (INT64)(uintptr_t)instructionEnd;
    if (distance < INT_MIN || distance > INT_MAX) return false;
    *displacement = (LONG)distance;
    return true;
}

static LONG FloatBits(float value)
{
    LONG bits = 0;
    static_assert(sizeof(bits) == sizeof(value),
                  "float and LONG must both contain 32 bits");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// -------------------------------------------------------------------------
// CUSTOMHOUSE

namespace Customhouse
{
static const DWORD EXPECTED_IMAGE_SIZE = 0x00A9D000;
static const DWORD EXPECTED_EXE_TIMESTAMP = 0x6A3EB6AD;

static const DWORD RVA_PANEL = 0x0071B410;
static const DWORD RVA_LIST_MEASURE_CALL = 0x0071C475;
static const DWORD RVA_LIST_DRAW_CALL = 0x0071C4B5;
static const DWORD RVA_RESOURCE_COLUMNS = 0x007C6720;
static const DWORD RVA_ROW_PITCH_INSTRUCTION = 0x007C678C;
static const DWORD RVA_ROW_PITCH_DISPLACEMENT = 0x007C6791;
static const DWORD RVA_ROW_PITCH_INSTRUCTION_END = 0x007C6795;

static const float NATIVE_ROW_PITCH = 25.0f;
static const float DEFAULT_ROW_PITCH = 30.0f;

static const BYTE EXPECT_PANEL[] = {
    0x48,0x8B,0xC4,0x55,0x56,0x57,0x41,0x54,
    0x41,0x55,0x41,0x56,0x41,0x57
};
static const BYTE EXPECT_ROW_PITCH_INSTRUCTION[] = {
    0xF3,0x44,0x0F,0x59,0x35,0xD3,0x41,0x14,0x00
};

// The first four parameters use the Windows x64 register positions. The
// native function obtains resources, optional bounds and the display flag from
// stack parameters five through seven.
typedef void (__fastcall *NativeResourceColumnsFn)(
    void* ui, float x, float y, void* reserved,
    void* resources, void* bounds, bool displayPrices);

static int g_enabled = 1;
static float g_configuredRowPitch = DEFAULT_ROW_PITCH;
static volatile LONG* g_rowPitchBits = nullptr;
static NativeResourceColumnsFn g_nativeResourceColumns = nullptr;

__declspec(noinline)
static void __fastcall ScopedResourceColumns(
    void* ui, float x, float y, void* reserved,
    void* resources, void* bounds, bool displayPrices)
{
    // Both hooked sites belong exclusively to CUSTOMHOUSE. The replacement is
    // synchronous; the original value is restored before control returns to
    // the panel. WRSR renders its windows on the owning UI thread.
    const LONG customBits = FloatBits(g_configuredRowPitch);
    const LONG previousBits =
        InterlockedExchange(g_rowPitchBits, customBits);

    g_nativeResourceColumns(ui, x, y, reserved,
                            resources, bounds, displayPrices);

    InterlockedExchange(g_rowPitchBits, previousBits);
}

static bool VerifyTargetBuild()
{
    const DWORD timestamp = ExeTimestamp();
    if (g_exeSize != EXPECTED_IMAGE_SIZE ||
        timestamp != EXPECTED_EXE_TIMESTAMP)
    {
        LogError("CUSTOMHOUSE", "unsupported-build",
            "expected SOVIET64.exe 1.1.1.9 "
            "(image=0x%X timestamp=0x%08X), found image=0x%llX timestamp=0x%08X; "
            "the module remains inactive",
            EXPECTED_IMAGE_SIZE, EXPECTED_EXE_TIMESTAMP,
            (unsigned long long)g_exeSize, timestamp);
        return false;
    }

    return VerifyBytes(RVA_PANEL, EXPECT_PANEL, sizeof(EXPECT_PANEL),
                       "CUSTOMHOUSE", "panel-signature",
                       "CUSTOMHOUSE panel") &&
           VerifyRelativeCall(RVA_LIST_MEASURE_CALL, RVA_RESOURCE_COLUMNS,
                       "CUSTOMHOUSE", "measure-call",
                       "resource-list measurement call") &&
           VerifyRelativeCall(RVA_LIST_DRAW_CALL, RVA_RESOURCE_COLUMNS,
                       "CUSTOMHOUSE", "draw-call",
                       "resource-list drawing call") &&
           VerifyBytes(RVA_ROW_PITCH_INSTRUCTION,
                       EXPECT_ROW_PITCH_INSTRUCTION,
                       sizeof(EXPECT_ROW_PITCH_INSTRUCTION),
                       "CUSTOMHOUSE", "row-pitch-signature",
                       "native resource-row pitch instruction");
}

static void BuildAbsoluteJump(BYTE* destination, void* target)
{
    BYTE code[16];
    memset(code, 0xCC, sizeof(code));
    code[0] = 0x48;
    code[1] = 0xB8; // mov rax, imm64
    memcpy(code + 2, &target, sizeof(target));
    code[10] = 0xFF;
    code[11] = 0xE0; // jmp rax
    memcpy(destination, code, sizeof(code));
}

static bool Install()
{
    if (!g_enabled)
    {
        LogInfo("[CUSTOMHOUSE] disabled by configuration");
        return true;
    }

    if (!VerifyTargetBuild()) return false;

    BYTE* measureCall = g_exeBase + RVA_LIST_MEASURE_CALL;
    BYTE* drawCall = g_exeBase + RVA_LIST_DRAW_CALL;
    BYTE* rowPitchInstruction =
        g_exeBase + RVA_ROW_PITCH_INSTRUCTION;

    // One nearby page holds a shared absolute-jump bridge and the aligned
    // float operand. The loader guarantees rel32 reach from the anchor.
    BYTE* bridgePage = AllocNear(measureCall, 128);
    if (!bridgePage)
    {
        LogError("CUSTOMHOUSE", "near-allocation",
            "could not allocate the verified call bridge; no patch was written");
        return false;
    }

    BYTE* callBridge = bridgePage;
    BYTE* pitchStorage = bridgePage + 64;
    BuildAbsoluteJump(callBridge, (void*)&ScopedResourceColumns);

    const LONG nativeBits = FloatBits(NATIVE_ROW_PITCH);
    *(LONG*)pitchStorage = nativeBits;
    g_rowPitchBits = (volatile LONG*)pitchStorage;
    g_nativeResourceColumns =
        (NativeResourceColumnsFn)(g_exeBase + RVA_RESOURCE_COLUMNS);

    LONG measureDisplacement = 0;
    LONG drawDisplacement = 0;
    LONG pitchDisplacement = 0;
    if (!Rel32(measureCall, callBridge, &measureDisplacement) ||
        !Rel32(drawCall, callBridge, &drawDisplacement) ||
        !Rip32(g_exeBase + RVA_ROW_PITCH_INSTRUCTION_END,
               pitchStorage, &pitchDisplacement))
    {
        LogError("CUSTOMHOUSE", "bridge-range",
            "the allocated bridge or row-pitch slot is outside rel32/RIP32 range; no patch was written");
        g_rowPitchBits = nullptr;
        g_nativeResourceColumns = nullptr;
        return false;
    }

    if (!FlushInstructionCache(GetCurrentProcess(), bridgePage, 128))
    {
        const DWORD error = GetLastError();
        ReportWindows("ERROR", "CUSTOMHOUSE", "bridge-cache",
            "Could not publish the generated call bridge to the instruction cache",
            error,
            "Restart the game and verify that security software is not blocking the plugin; no patch was written");
        g_rowPitchBits = nullptr;
        g_nativeResourceColumns = nullptr;
        return false;
    }

    // Both call sites share a page. Make every destination writable before
    // changing a byte so a protection failure cannot leave measurement and
    // drawing with different spacing.
    DWORD callPageProtection = 0;
    DWORD pitchPageProtection = 0;
    const SIZE_T callRange =
        (SIZE_T)((drawCall + 5) - measureCall);

    if (!VirtualProtect(measureCall, callRange, PAGE_EXECUTE_READWRITE,
                        &callPageProtection))
    {
        const DWORD error = GetLastError();
        ReportWindows("ERROR", "CUSTOMHOUSE", "call-protection",
            "Could not make the two verified CUSTOMHOUSE call sites writable",
            error,
            "Restart the game and check security software or conflicting UI plugins; no patch was written");
        g_rowPitchBits = nullptr;
        g_nativeResourceColumns = nullptr;
        return false;
    }

    if (!VirtualProtect(rowPitchInstruction,
                        sizeof(EXPECT_ROW_PITCH_INSTRUCTION),
                        PAGE_EXECUTE_READWRITE, &pitchPageProtection))
    {
        const DWORD error = GetLastError();
        DWORD ignored = 0;
        if (!VirtualProtect(measureCall, callRange,
                            callPageProtection, &ignored))
        {
            const DWORD restoreError = GetLastError();
            ReportWindows("WARN", "CUSTOMHOUSE", "call-protection-restore",
                "The call-site page could not be restored after patch preparation failed",
                restoreError,
                "No code was changed, but restart the game before changing plugins");
        }
        ReportWindows("ERROR", "CUSTOMHOUSE", "pitch-protection",
            "Could not make the verified native row-pitch operand writable",
            error,
            "Restart the game and check security software or conflicting UI plugins; no patch was written");
        g_rowPitchBits = nullptr;
        g_nativeResourceColumns = nullptr;
        return false;
    }

    // Redirect only the float operand; the native mulss instruction remains.
    memcpy(g_exeBase + RVA_ROW_PITCH_DISPLACEMENT,
           &pitchDisplacement, sizeof(pitchDisplacement));
    memcpy(measureCall + 1, &measureDisplacement,
           sizeof(measureDisplacement));
    memcpy(drawCall + 1, &drawDisplacement,
           sizeof(drawDisplacement));

    DWORD ignored = 0;
    const BOOL pitchRestored = VirtualProtect(
        rowPitchInstruction, sizeof(EXPECT_ROW_PITCH_INSTRUCTION),
        pitchPageProtection, &ignored);
    const DWORD pitchRestoreError =
        pitchRestored ? ERROR_SUCCESS : GetLastError();
    const BOOL callsRestored = VirtualProtect(
        measureCall, callRange, callPageProtection, &ignored);
    const DWORD callsRestoreError =
        callsRestored ? ERROR_SUCCESS : GetLastError();

    const BOOL pitchFlushed = FlushInstructionCache(
        GetCurrentProcess(), rowPitchInstruction,
        sizeof(EXPECT_ROW_PITCH_INSTRUCTION));
    const DWORD pitchFlushError =
        pitchFlushed ? ERROR_SUCCESS : GetLastError();
    const BOOL callsFlushed = FlushInstructionCache(
        GetCurrentProcess(), measureCall, callRange);
    const DWORD callsFlushError =
        callsFlushed ? ERROR_SUCCESS : GetLastError();

    // The patch is active at this point. A restoration or cache-flush failure
    // must therefore keep the DLL loaded and be reported as a warning rather
    // than making Start claim that no patch exists.
    if (!pitchRestored)
        ReportWindows("WARN", "CUSTOMHOUSE", "pitch-protection-restore",
            "The patch is active, but the row-pitch page protection could not be restored",
            pitchRestoreError,
            "Restart the game before changing plugins");
    if (!callsRestored)
        ReportWindows("WARN", "CUSTOMHOUSE", "call-protection-restore",
            "The patch is active, but the call-site page protection could not be restored",
            callsRestoreError,
            "Restart the game before changing plugins");
    if (!pitchFlushed)
        ReportWindows("WARN", "CUSTOMHOUSE", "pitch-cache",
            "The row-pitch patch could not be flushed from the instruction cache",
            pitchFlushError,
            "Restart the game if the layout correction is not visible");
    if (!callsFlushed)
        ReportWindows("WARN", "CUSTOMHOUSE", "call-cache",
            "The redirected call sites could not be flushed from the instruction cache",
            callsFlushError,
            "Restart the game if the layout correction is not visible");

    LogInfo("[CUSTOMHOUSE] active: resource row pitch %.1f -> %.1f logical px; scope=measure+draw calls only; native layout/scroll propagation retained",
            NATIVE_ROW_PITCH, g_configuredRowPitch);
    return true;
}

static void LoadConfig()
{
    g_enabled = TsmConfigInt("customhouse", "enabled", 1) != 0;
    ReadIniFloat("customhouse", "resource_row_pitch",
                 DEFAULT_ROW_PITCH, NATIVE_ROW_PITCH, 60.0f,
                 &g_configuredRowPitch);
}
} // namespace Customhouse

// -------------------------------------------------------------------------
// TEXT_WRAP module
//
// Long game captions such as the route hint of the vehicle window (text id
// 1970, "View area where a possible issue exists on route! ...") are stored
// as one or two overlong lines and drawn with the engine's single-line print
// functions, which ignore line breaks, so they run past the window edge.
//
// Two hooks, both through the import table of SOVIET64.exe, no executable
// code changed, no dependency on a particular game build:
//  1. C3D_LANGUAGE::GetString answers every id listed in [text_wrap_ids]
//     with a word-wrapped copy from a plugin-owned buffer.
//  2. The engine's print imports (PrintLeft/Center/RightUnicode on
//     C3D_FONTMANAGER and C3D_FONT, PrintLeftUnicodeNoArg) are redirected
//     through small generated stubs. A stub looks at the text pointer only:
//     inside the plugin's wrap buffers it jumps to a handler that prints one
//     line per '\n' with y advanced by font size * line_spacing; every other
//     call jumps straight on to the original with all its arguments intact
//     (the print functions are variadic, so a C detour could not forward
//     them; the stub never touches the stack).
//
// Other plugins may hook the same imports (the resources plugin hooks
// GetString). The loader hands back the previous slot value, so the hooks
// chain in load order and each answers only its own texts.
//
// Limit: a caption the game copies into its own buffer before printing is
// no longer recognised by the pointer check and comes out as one line.

namespace TextWrap
{
static const char* const MODULE = "TEXT_WRAP";
static const char* const SECTION = "text_wrap";
static const char* const LIST_SECTION = "text_wrap_ids";
static const char* const SYM_GET_STRING = "?GetString@C3D_LANGUAGE@@QEAAPEA_WH@Z";
static const char* const SYM_FONT_SIZE = "?GetSize@C3D_FONT@@QEAAMXZ";

static const int DEFAULT_TEXT_ID = 1970;      // used when [text_wrap_ids] is absent
static const int DEFAULT_MAX_CHARS = 58;
static const int DEFAULT_MAX_LINES = 4;
static const float DEFAULT_LINE_SPACING = 1.15f;
static const int MIN_TEXT_ID = 1;
static const int MAX_TEXT_ID = 100000;
static const int MIN_MAX_CHARS = 20;
static const int MAX_MAX_CHARS = 200;
static const int MIN_MAX_LINES = 0;
static const int MAX_MAX_LINES = 12;
static const float MIN_LINE_SPACING = 0.5f;
static const float MAX_LINE_SPACING = 3.0f;
static const int MAX_LOG_LONG = 400;
static const int MAX_ENTRIES = 64;
static const size_t BUFFER_CHARS = 1024;      // per text, including terminator
static const size_t STUB_BYTES = 64;
static const size_t STUB_PAGE = 4096;

static int g_enabled = 1;
static int g_maxChars = DEFAULT_MAX_CHARS;
static int g_maxLines = DEFAULT_MAX_LINES;
static int g_keepBreaks = 0;
static int g_logLong = 0;
static float g_lineSpacing = DEFAULT_LINE_SPACING;
static bool g_installed = false;
static bool g_stepLogged = false;

struct Entry
{
    int id;
    int maxChars;        // 0 = [text_wrap] max_chars
    bool haveWrapped;
    bool tooLongReported;
};
static Entry g_entries[MAX_ENTRIES];
static int g_entryCount = 0;

// One source copy and one wrapped copy per entry. g_wrapped is one
// contiguous block: the print stubs recognise the plugin's texts by
// checking whether the text pointer lies inside it.
static wchar_t g_sources[MAX_ENTRIES][BUFFER_CHARS];
static wchar_t g_wrapped[MAX_ENTRIES][BUFFER_CHARS];
static unsigned char g_longSeen[65536 / 8];

typedef wchar_t* (*GetStringFn)(void* self, int id);
static GetStringFn o_GetString = nullptr;
typedef float (*FontSizeFn)(void* font);
static FontSizeFn g_fontSize = nullptr;

// void C3D_FONTMANAGER::Print*Unicode(C3D_FONT*, float x, float y, unsigned long colour, const wchar_t* format, ...)
typedef void (*PrintMgrFn)(void* manager, void* font, float x, float y,
                           unsigned long colour, const wchar_t* format, ...);
// void C3D_FONTMANAGER::PrintLeftUnicodeNoArg(C3D_FONT*, float x, float y, unsigned long colour, const wchar_t* text)
typedef void (*PrintMgrNoArgFn)(void* manager, void* font, float x, float y,
                                unsigned long colour, const wchar_t* text);
// void C3D_FONT::Print*Unicode(float x, float y, unsigned long colour, const wchar_t* format, ...)
typedef void (*PrintFontFn)(void* font, float x, float y,
                            unsigned long colour, const wchar_t* format, ...);

enum HookIndex
{
    HOOK_MGR_LEFT = 0, HOOK_MGR_CENTER, HOOK_MGR_RIGHT, HOOK_MGR_LEFT_NOARG,
    HOOK_FONT_LEFT, HOOK_FONT_RIGHT, HOOK_COUNT
};

struct PrintHook
{
    const char* symbol;
    const char* label;
    int formatOffset;     // stack offset of the text pointer at stub entry
    void* handler;
    void* original;
    bool installed;
};
static PrintHook g_hooks[HOOK_COUNT];
static BYTE* g_stubPage = nullptr;

static bool ReadIniInt(const char* section, const char* key,
                       int fallback, int minimum, int maximum, int* output)
{
    if (!output) return false;

    char fallbackText[32];
    char valueText[128];
    _snprintf_s(fallbackText, sizeof(fallbackText), _TRUNCATE, "%d", fallback);
    if (!TsmConfigString(section, key, valueText, (int)sizeof(valueText), fallbackText))
    {
        *output = fallback;
        return true;
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = strtol(valueText, &end, 10);
    const int parseError = errno;
    while (end && (*end == ' ' || *end == '\t' ||
                   *end == '\r' || *end == '\n'))
    {
        ++end;
    }

    if (!end || end == valueText || *end != 0 || parseError == ERANGE ||
        parsed < minimum || parsed > maximum)
    {
        LogWarning(section, "invalid-config",
            "%s=%s is invalid; expected a whole number in %d..%d. "
            "Action: using %d",
            key, valueText, minimum, maximum, fallback);
        *output = fallback;
        return false;
    }

    *output = (int)parsed;
    return true;
}

struct Writer
{
    wchar_t* out;
    size_t capacity;
    size_t length;
    int lines;
    size_t widest;
};

static bool Put(Writer& w, wchar_t c)
{
    if (w.length + 1 >= w.capacity) return false;
    w.out[w.length++] = c;
    return true;
}

// Greedy word wrap of one paragraph. Words are never split; a word longer
// than the width stands on a line of its own.
static bool WrapParagraph(Writer& w, const wchar_t* text, size_t length,
                          size_t width)
{
    size_t lineLength = 0;
    size_t i = 0;
    while (i < length)
    {
        while (i < length && text[i] == L' ') ++i;
        if (i >= length) break;

        const size_t wordStart = i;
        while (i < length && text[i] != L' ') ++i;
        const size_t wordLength = i - wordStart;

        if (lineLength > 0)
        {
            if (lineLength + 1 + wordLength > width)
            {
                if (!Put(w, L'\n')) return false;
                ++w.lines;
                if (lineLength > w.widest) w.widest = lineLength;
                lineLength = 0;
            }
            else
            {
                if (!Put(w, L' ')) return false;
                ++lineLength;
            }
        }

        for (size_t k = 0; k < wordLength; ++k)
            if (!Put(w, text[wordStart + k])) return false;
        lineLength += wordLength;
    }
    if (lineLength > w.widest) w.widest = lineLength;
    return true;
}

// Dry run of WrapParagraph: how many lines does the paragraph take at `width`?
static int CountLines(const wchar_t* text, size_t length, size_t width)
{
    int lines = 1;
    size_t lineLength = 0;
    size_t i = 0;
    while (i < length)
    {
        while (i < length && text[i] == L' ') ++i;
        if (i >= length) break;
        const size_t wordStart = i;
        while (i < length && text[i] != L' ') ++i;
        const size_t wordLength = i - wordStart;
        if (lineLength > 0)
        {
            if (lineLength + 1 + wordLength > width)
            {
                ++lines;
                lineLength = 0;
            }
            else
            {
                ++lineLength;
            }
        }
        lineLength += wordLength;
    }
    return lines;
}

// The narrowest width that still wraps the paragraph into the same number of
// lines as `width` does. Greedy wrapping alone leaves a long first line and a
// short tail ("...Problem auf der" / "Route besteht!"); balancing spreads the
// words evenly without adding a line.
static size_t BalancedWidth(const wchar_t* text, size_t length, size_t width)
{
    const int lines = CountLines(text, length, width);
    if (lines < 2) return width;
    size_t best = width;
    for (size_t candidate = width - 1; candidate > 0; --candidate)
    {
        if (CountLines(text, length, candidate) != lines) break;
        best = candidate;
    }
    return best;
}

// Wraps the whole text at `width`. The game's own line breaks stay as
// paragraph breaks. Returns the line count, or -1 when the copy did not fit.
static int WrapText(const wchar_t* source, size_t width, wchar_t* out,
                    size_t capacity, size_t* widest)
{
    Writer w = { out, capacity, 0, 1, 0 };
    const wchar_t* p = source;
    bool first = true;
    for (;;)
    {
        const wchar_t* newline = wcschr(p, L'\n');
        size_t length = newline ? (size_t)(newline - p) : wcslen(p);
        if (length > 0 && p[length - 1] == L'\r') --length;

        if (!first)
        {
            if (!Put(w, L'\n')) return -1;
            ++w.lines;
        }
        first = false;
        if (!WrapParagraph(w, p, length, BalancedWidth(p, length, width)))
            return -1;
        if (!newline) break;
        p = newline + 1;
    }
    out[w.length] = 0;
    if (widest) *widest = w.widest;
    return w.lines;
}

static void MeasureSource(const wchar_t* source, int* lines, size_t* longest)
{
    *lines = 1;
    *longest = 0;
    size_t current = 0;
    for (const wchar_t* p = source; *p; ++p)
    {
        if (*p == L'\n')
        {
            ++*lines;
            current = 0;
        }
        else if (*p != L'\r')
        {
            ++current;
            if (current > *longest) *longest = current;
        }
    }
}

static int FindEntry(int id)
{
    for (int i = 0; i < g_entryCount; ++i)
        if (g_entries[i].id == id) return i;
    return -1;
}

// Returns the wrapped copy of `original` for entry `index`, rebuilding it
// when the game's string changed (language switch). Falls back to the
// original when the text is too long for the buffer.
static const wchar_t* Wrapped(int index, const wchar_t* original)
{
    Entry& entry = g_entries[index];
    wchar_t* source = g_sources[index];
    wchar_t* wrapped = g_wrapped[index];

    EnterCriticalSection(&g_lock);
    if (!entry.haveWrapped || wcscmp(original, source) != 0)
    {
        entry.haveWrapped = false;
        const size_t sourceLength = wcslen(original);
        if (sourceLength + 1 > BUFFER_CHARS)
        {
            if (!entry.tooLongReported)
            {
                entry.tooLongReported = true;
                LogWarning(MODULE, "text-length",
                    "text id %d is %u characters long, the wrap buffer holds %u. "
                    "Action: the native text is shown unchanged",
                    entry.id, (unsigned)sourceLength, (unsigned)(BUFFER_CHARS - 1));
            }
        }
        else
        {
            wcscpy_s(source, BUFFER_CHARS, original);

            int sourceLines = 0;
            size_t sourceLongest = 0;
            MeasureSource(source, &sourceLines, &sourceLongest);

            // The game's own line breaks either stay as paragraph breaks or
            // become spaces so the whole text reflows into fewer lines.
            wchar_t flow[BUFFER_CHARS];
            wcscpy_s(flow, BUFFER_CHARS, source);
            if (!g_keepBreaks)
                for (wchar_t* p = flow; *p; ++p)
                    if (*p == L'\n' || *p == L'\r') *p = L' ';
            int flowLines = 0;
            size_t longest = 0;
            MeasureSource(flow, &flowLines, &longest);

            // Start at the entry's width (or the module default); when the
            // line limit is exceeded widen step by step, never beyond the
            // native line length.
            size_t width = (size_t)(entry.maxChars > 0 ? entry.maxChars : g_maxChars);
            size_t widest = 0;
            int lines = -1;
            for (;;)
            {
                lines = WrapText(flow, width, wrapped, BUFFER_CHARS, &widest);
                if (lines < 0) break;
                if (g_maxLines <= 0 || lines <= g_maxLines || width >= longest) break;
                width += 4;
                if (width > longest) width = longest;
            }

            if (lines > 0)
            {
                entry.haveWrapped = true;
                LogInfo("[%s] text id %d: native %d line(s), longest %u chars -> "
                        "%d line(s), widest %u chars at width %u",
                    MODULE, entry.id, sourceLines, (unsigned)sourceLongest,
                    lines, (unsigned)widest, (unsigned)width);
            }
        }
    }
    const wchar_t* result = entry.haveWrapped ? wrapped : original;
    LeaveCriticalSection(&g_lock);
    return result;
}

// Diagnostic: every id the game asks for whose longest line exceeds
// log_long_texts is reported once, so candidates for the list can be found
// while playing.
static void NoteLongText(int id, const wchar_t* text)
{
    if (id < 0 || id >= 65536) return;
    const unsigned index = (unsigned)id;
    if (g_longSeen[index >> 3] & (1u << (index & 7))) return;
    g_longSeen[index >> 3] |= (unsigned char)(1u << (index & 7));

    int lines = 0;
    size_t longest = 0;
    MeasureSource(text, &lines, &longest);
    if (longest <= (size_t)g_logLong) return;
    LogInfo("[%s] long text id %d: %d line(s), longest %u chars%s: %.80ls",
        MODULE, id, lines, (unsigned)longest,
        FindEntry(id) >= 0 ? " (listed)" : "", text);
}

static wchar_t* h_GetString(void* self, int id)
{
    wchar_t* text = o_GetString(self, id);
    if (!text || !text[0]) return text;
    if (g_logLong > 0) NoteLongText(id, text);
    const int index = FindEntry(id);
    if (index < 0) return text;
    return const_cast<wchar_t*>(Wrapped(index, text));
}

static float LineStep(void* font)
{
    float size = 0.0f;
    if (g_fontSize && font) size = g_fontSize(font);
    if (!(size > 1.0f && size < 500.0f)) size = 16.0f;
    const float step = size * g_lineSpacing;
    if (!g_stepLogged)
    {
        g_stepLogged = true;
        LogInfo("[%s] first wrapped print: font size %.1f, line spacing %.2f -> line step %.1f",
            MODULE, size, g_lineSpacing, step);
    }
    return step;
}

// Splits `text` at '\n' into `line` (one call per line, index counts up).
// Returns false after the last line.
static bool NextLine(const wchar_t** cursor, wchar_t* line)
{
    const wchar_t* p = *cursor;
    if (!p) return false;
    const wchar_t* newline = wcschr(p, L'\n');
    size_t length = newline ? (size_t)(newline - p) : wcslen(p);
    if (length > 0 && p[length - 1] == L'\r') --length;
    if (length >= BUFFER_CHARS) length = BUFFER_CHARS - 1;
    memcpy(line, p, length * sizeof(wchar_t));
    line[length] = 0;
    *cursor = newline ? newline + 1 : nullptr;
    return true;
}

// The handlers receive exactly the fixed arguments of the print function
// they replace; the stubs only send texts from g_wrapped here, and those
// never carry format arguments.
static void PrintMgrLines(int hook, void* manager, void* font, float x, float y,
                          unsigned long colour, const wchar_t* text)
{
    void* original = g_hooks[hook].original;
    if (!original) return;
    const float step = LineStep(font);
    wchar_t line[BUFFER_CHARS];
    const wchar_t* cursor = text;
    for (int index = 0; NextLine(&cursor, line); ++index)
    {
        const float lineY = y + step * (float)index;
        if (hook == HOOK_MGR_LEFT_NOARG)
            ((PrintMgrNoArgFn)original)(manager, font, x, lineY, colour, line);
        else
            ((PrintMgrFn)original)(manager, font, x, lineY, colour, L"%ls", line);
    }
}

static void PrintFontLines(int hook, void* font, float x, float y,
                           unsigned long colour, const wchar_t* text)
{
    void* original = g_hooks[hook].original;
    if (!original) return;
    const float step = LineStep(font);
    wchar_t line[BUFFER_CHARS];
    const wchar_t* cursor = text;
    for (int index = 0; NextLine(&cursor, line); ++index)
        ((PrintFontFn)original)(font, x, y + step * (float)index, colour, L"%ls", line);
}

static void HandleMgrLeft(void* m, void* f, float x, float y, unsigned long c, const wchar_t* t)
{ PrintMgrLines(HOOK_MGR_LEFT, m, f, x, y, c, t); }
static void HandleMgrCenter(void* m, void* f, float x, float y, unsigned long c, const wchar_t* t)
{ PrintMgrLines(HOOK_MGR_CENTER, m, f, x, y, c, t); }
static void HandleMgrRight(void* m, void* f, float x, float y, unsigned long c, const wchar_t* t)
{ PrintMgrLines(HOOK_MGR_RIGHT, m, f, x, y, c, t); }
static void HandleMgrLeftNoArg(void* m, void* f, float x, float y, unsigned long c, const wchar_t* t)
{ PrintMgrLines(HOOK_MGR_LEFT_NOARG, m, f, x, y, c, t); }
static void HandleFontLeft(void* f, float x, float y, unsigned long c, const wchar_t* t)
{ PrintFontLines(HOOK_FONT_LEFT, f, x, y, c, t); }
static void HandleFontRight(void* f, float x, float y, unsigned long c, const wchar_t* t)
{ PrintFontLines(HOOK_FONT_RIGHT, f, x, y, c, t); }

// Emits one stub:
//   mov rax, [rsp+formatOffset]   ; the text pointer of this call
//   mov r10, begin ; cmp rax, r10 ; jb pass
//   mov r10, end   ; cmp rax, r10 ; jae pass
//   mov rax, handler ; jmp rax    ; our text: print line by line
// pass:
//   mov rax, original ; jmp rax   ; anything else: untouched, arguments intact
// rax, r10 are scratch registers in the x64 calling convention; nothing is
// pushed, so the callee sees the caller's frame exactly as built.
static size_t BuildStub(BYTE* out, int formatOffset, const void* begin,
                        const void* end, void* handler, void* original)
{
    size_t n = 0;
    out[n++] = 0x48; out[n++] = 0x8B; out[n++] = 0x44; out[n++] = 0x24; out[n++] = (BYTE)formatOffset;
    out[n++] = 0x49; out[n++] = 0xBA; memcpy(out + n, &begin, 8); n += 8;
    out[n++] = 0x4C; out[n++] = 0x39; out[n++] = 0xD0;
    out[n++] = 0x72; const size_t jb = n++;
    out[n++] = 0x49; out[n++] = 0xBA; memcpy(out + n, &end, 8); n += 8;
    out[n++] = 0x4C; out[n++] = 0x39; out[n++] = 0xD0;
    out[n++] = 0x73; const size_t jae = n++;
    out[n++] = 0x48; out[n++] = 0xB8; memcpy(out + n, &handler, 8); n += 8;
    out[n++] = 0xFF; out[n++] = 0xE0;
    const size_t pass = n;
    out[jb] = (BYTE)(pass - (jb + 1));
    out[jae] = (BYTE)(pass - (jae + 1));
    out[n++] = 0x48; out[n++] = 0xB8; memcpy(out + n, &original, 8); n += 8;
    out[n++] = 0xFF; out[n++] = 0xE0;
    return n;
}

static void DefineHooks()
{
    g_hooks[HOOK_MGR_LEFT]       = { "?PrintLeftUnicode@C3D_FONTMANAGER@@QEAAXPEAVC3D_FONT@@MMKPEB_WZZ",   "C3D_FONTMANAGER::PrintLeftUnicode",      0x30, (void*)&HandleMgrLeft,      nullptr, false };
    g_hooks[HOOK_MGR_CENTER]     = { "?PrintCenterUnicode@C3D_FONTMANAGER@@QEAAXPEAVC3D_FONT@@MMKPEB_WZZ", "C3D_FONTMANAGER::PrintCenterUnicode",    0x30, (void*)&HandleMgrCenter,    nullptr, false };
    g_hooks[HOOK_MGR_RIGHT]      = { "?PrintRightUnicode@C3D_FONTMANAGER@@QEAAXPEAVC3D_FONT@@MMKPEB_WZZ",  "C3D_FONTMANAGER::PrintRightUnicode",     0x30, (void*)&HandleMgrRight,     nullptr, false };
    g_hooks[HOOK_MGR_LEFT_NOARG] = { "?PrintLeftUnicodeNoArg@C3D_FONTMANAGER@@QEAAXPEAVC3D_FONT@@MMKPEB_W@Z", "C3D_FONTMANAGER::PrintLeftUnicodeNoArg", 0x30, (void*)&HandleMgrLeftNoArg, nullptr, false };
    g_hooks[HOOK_FONT_LEFT]      = { "?PrintLeftUnicode@C3D_FONT@@QEAAXMMKPEB_WZZ",                        "C3D_FONT::PrintLeftUnicode",             0x28, (void*)&HandleFontLeft,     nullptr, false };
    g_hooks[HOOK_FONT_RIGHT]     = { "?PrintRightUnicode@C3D_FONT@@QEAAXMMKPEB_WZZ",                       "C3D_FONT::PrintRightUnicode",            0x28, (void*)&HandleFontRight,    nullptr, false };
}

// Redirects every print import that SOVIET64.exe actually has. An import
// that is missing is skipped with a note; a failed patch is an error but
// leaves the other hooks in place.
static int InstallPrintHooks()
{
    DefineHooks();
    g_stubPage = (BYTE*)VirtualAlloc(nullptr, STUB_PAGE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_stubPage)
    {
        ReportWindows("ERROR", MODULE, "stub-allocation",
            "Could not allocate the page for the print stubs", GetLastError(),
            "Restart the game; the wrapped texts are drawn as one line until then");
        return 0;
    }
    memset(g_stubPage, 0xCC, STUB_PAGE);

    const void* begin = &g_wrapped[0][0];
    const void* end = &g_wrapped[MAX_ENTRIES - 1][BUFFER_CHARS - 1] + 1;
    int installed = 0;
    for (int i = 0; i < HOOK_COUNT; ++i)
    {
        PrintHook& hook = g_hooks[i];
        void** slot = FindIatSlot((HMODULE)g_exeBase, DLL_ENGINE, hook.symbol);
        if (!slot || !*slot)
        {
            LogInfo("[%s] %s is not imported by SOVIET64.exe; skipped", MODULE, hook.label);
            continue;
        }
        hook.original = *slot;
        BYTE* stub = g_stubPage + (size_t)i * STUB_BYTES;
        BuildStub(stub, hook.formatOffset, begin, end, hook.handler, hook.original);
        FlushInstructionCache(GetCurrentProcess(), stub, STUB_BYTES);

        void* previous = nullptr;
        if (!PatchIat((HMODULE)g_exeBase, DLL_ENGINE, hook.symbol, (void*)stub, &previous, hook.label) || previous != hook.original)
        {
            LogError(MODULE, "print-import",
                "the import %s!%s could not be redirected; texts drawn through it stay one line",
                DLL_ENGINE, hook.symbol);
            hook.original = nullptr;
            continue;
        }
        hook.installed = true;
        ++installed;
    }
    return installed;
}

static bool Install()
{
    if (!g_enabled)
    {
        LogInfo("[%s] disabled by configuration", MODULE);
        return true;
    }
    if (g_entryCount == 0)
    {
        LogInfo("[%s] no text ids listed in [%s]; nothing to do", MODULE, LIST_SECTION);
        return true;
    }

    HMODULE engine = GetModuleHandleA(DLL_ENGINE);
    if (engine) g_fontSize = (FontSizeFn)GetProcAddress(engine, SYM_FONT_SIZE);
    if (!g_fontSize)
        LogWarning(MODULE, "font-size",
            "%s!%s not found; the line step falls back to 16 * line_spacing", DLL_ENGINE, SYM_FONT_SIZE);

    // The print stubs go first: should the caption hook fail afterwards, a
    // native text with line breaks is at least drawn as separate lines
    // whenever it is one of ours.
    const int printHooks = InstallPrintHooks();
    if (printHooks == 0)
    {
        LogError(MODULE, "print-hooks",
            "none of the print imports could be redirected; wrapped texts would come out as one line, the module stays inactive");
        return false;
    }

    if (!PatchIat((HMODULE)g_exeBase, DLL_ENGINE, SYM_GET_STRING,
                  (void*)h_GetString, (void**)&o_GetString,
                  "C3D_LANGUAGE::GetString (ui_layout_fixes)") ||
        !o_GetString)
    {
        LogError(MODULE, "iat-patch",
            "The import %s!%s of SOVIET64.exe could not be redirected; no text is wrapped. "
            "Action: check tesmioloader.log for the loader's reason and conflicting plugins",
            DLL_ENGINE, SYM_GET_STRING);
        return false;
    }

    g_installed = true;
    char ids[512];
    ids[0] = 0;
    for (int i = 0; i < g_entryCount; ++i)
    {
        char one[32];
        _snprintf_s(one, sizeof(one), _TRUNCATE, "%s%d", i ? "," : "", g_entries[i].id);
        strcat_s(ids, sizeof(ids), one);
        if (g_entries[i].maxChars > 0)
        {
            _snprintf_s(one, sizeof(one), _TRUNCATE, "=%d", g_entries[i].maxChars);
            strcat_s(ids, sizeof(ids), one);
        }
    }
    LogInfo("[%s] active: %d text id(s) [%s], default %d chars, line limit %d, line spacing %.2f, keep_breaks=%d, log_long_texts=%d; "
            "print hooks %d/%d; scope=caption lookup + print imports; executable code unchanged",
        MODULE, g_entryCount, ids, g_maxChars, g_maxLines, g_lineSpacing, g_keepBreaks, g_logLong,
        printHooks, HOOK_COUNT);
    return true;
}

static void AddEntry(const char* keyText, const char* valueText)
{
    char* end = nullptr;
    const long id = strtol(keyText, &end, 10);
    if (!end || end == keyText || *end != 0 || id < MIN_TEXT_ID || id > MAX_TEXT_ID)
    {
        LogWarning(LIST_SECTION, "invalid-config",
            "\"%s\" is not a text id in %d..%d. Action: entry skipped", keyText, MIN_TEXT_ID, MAX_TEXT_ID);
        return;
    }
    long chars = 0;
    if (valueText && valueText[0])
    {
        chars = strtol(valueText, &end, 10);
        while (end && (*end == ' ' || *end == '\t')) ++end;
        if (!end || end == valueText || *end != 0 || (chars != 0 && (chars < MIN_MAX_CHARS || chars > MAX_MAX_CHARS)))
        {
            LogWarning(LIST_SECTION, "invalid-config",
                "%s=%s is invalid; expected 0 or a whole number in %d..%d. Action: using the default width",
                keyText, valueText, MIN_MAX_CHARS, MAX_MAX_CHARS);
            chars = 0;
        }
    }
    if (FindEntry((int)id) >= 0)
    {
        LogWarning(LIST_SECTION, "invalid-config", "text id %ld is listed twice. Action: second entry skipped", id);
        return;
    }
    if (g_entryCount >= MAX_ENTRIES)
    {
        LogWarning(LIST_SECTION, "invalid-config", "more than %d text ids listed. Action: %ld skipped", MAX_ENTRIES, id);
        return;
    }
    Entry& entry = g_entries[g_entryCount++];
    entry.id = (int)id;
    entry.maxChars = (int)chars;
    entry.haveWrapped = false;
    entry.tooLongReported = false;
}

// Reads [text_wrap_ids] as "id = chars" lines. The user_config overlay wins
// as a whole when it carries the section; otherwise the base INI is read.
// A file without the section keeps the route hint (1970) as the one entry.
static bool ReadIdList(const char* path)
{
    char buffer[16384];
    const DWORD got = GetPrivateProfileSectionA(LIST_SECTION, buffer, sizeof(buffer), path);
    if (got == 0 || got >= sizeof(buffer) - 2) return false;
    bool any = false;
    for (char* p = buffer; *p; )
    {
        char* next = p + strlen(p) + 1;      // measured before the entry is cut at '='
        char* eq = strchr(p, '=');
        char* value = nullptr;
        if (eq) { *eq = 0; value = eq + 1; }
        Trim(p);
        if (value)
        {
            for (char* c = value; *c; ++c)
                if ((*c == ';' || *c == '#') && (c == value || c[-1] == ' ' || c[-1] == '\t')) { *c = 0; break; }
            Trim(value);
        }
        if (*p && *p != ';' && *p != '#')
        {
            any = true;
            AddEntry(p, value);
        }
        p = next;
    }
    return any;
}

static void LoadConfig()
{
    g_enabled = TsmConfigInt(SECTION, "enabled", 1) != 0;
    ReadIniInt(SECTION, "max_chars", DEFAULT_MAX_CHARS, MIN_MAX_CHARS, MAX_MAX_CHARS, &g_maxChars);
    ReadIniInt(SECTION, "max_lines", DEFAULT_MAX_LINES, MIN_MAX_LINES, MAX_MAX_LINES, &g_maxLines);
    ReadIniInt(SECTION, "keep_breaks", 0, 0, 1, &g_keepBreaks);
    ReadIniInt(SECTION, "log_long_texts", 0, 0, MAX_LOG_LONG, &g_logLong);
    ReadIniFloat(SECTION, "line_spacing", DEFAULT_LINE_SPACING, MIN_LINE_SPACING, MAX_LINE_SPACING, &g_lineSpacing);

    g_entryCount = 0;
    bool listed = false;
    if (g_tsmConfig.hasOverlay) listed = ReadIdList(g_tsmConfig.overlay);
    if (!listed && g_tsmConfig.hasBase) listed = ReadIdList(g_tsmConfig.base);
    if (!listed && g_entryCount == 0)
    {
        AddEntry("1970", "0");
        LogInfo("[%s] no [%s] section found; using the built-in entry %d (vehicle window route hint)",
            MODULE, LIST_SECTION, DEFAULT_TEXT_ID);
    }
}
} // namespace TextWrap

static void LoadConfig()
{
    g_enabled = TsmConfigInt("general", "enabled", 1) != 0;
    if (g_enabled)
    {
        Customhouse::LoadConfig();
        TextWrap::LoadConfig();
    }
}

static bool HasEnabledModule()
{
    return Customhouse::g_enabled != 0 || TextWrap::g_enabled != 0;
}

static const char* ActiveModules(char* out, size_t capacity)
{
    out[0] = 0;
    if (Customhouse::g_enabled)
        strcat_s(out, capacity, "CUSTOMHOUSE");
    if (TextWrap::g_installed)
    {
        if (out[0]) strcat_s(out, capacity, "+");
        strcat_s(out, capacity, "TEXT_WRAP");
    }
    if (!out[0]) strcpy_s(out, capacity, "none");
    return out;
}
} // namespace UiLayoutFixes

extern "C" __declspec(dllexport)
unsigned TsmPluginApiVersion(void)
{
    return TSM_API_VERSION;
}

extern "C" __declspec(dllexport)
int TsmPluginInit(const TsmHost* host, TsmPluginInfo* info)
{
    if (!host || !info) return 1;
    TsmBind(host);
    UiLayoutFixes::g_bound = true;

    info->name = "UI Layout Fixes";
    info->version = UiLayoutFixes::PLUGIN_VERSION;

    UiLayoutFixes::g_logStarted = GetTickCount64();
    UiLayoutFixes::BeginLogPhase();
    UiLayoutFixes::g_detail = TsmOpenLog(UiLayoutFixes::LOG_NAME);
    if (UiLayoutFixes::g_detail == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        UiLayoutFixes::ReportWindows("WARN", "core", "log-open",
            "The detail log could not be opened", error,
            "Verify write access to the TesmioLoader directory; the shared loader log remains available");
    }
    UiLayoutFixes::LogInfo(
        "TesmioLoader ui_layout_fixes %s starting; detail log: %s",
        UiLayoutFixes::PLUGIN_VERSION, UiLayoutFixes::LOG_NAME);
    TsmConfigInit("ui_layout_fixes");
    UiLayoutFixes::LogInfo("configuration: base=%s (%s); overlay=%s",
        g_tsmConfig.hasBase ? g_tsmConfig.base : "(none; compiled defaults)",
        g_tsmConfig.baseIsLocal ? "loader folder" : "beside the DLL",
        g_tsmConfig.hasOverlay ? g_tsmConfig.overlay : "(none)");

    try
    {
        UiLayoutFixes::LoadConfig();
        UiLayoutFixes::LogInfo(
            "initialized; architecture=window-specific modules; global layout changes=0");

        if (!UiLayoutFixes::g_enabled)
        {
            UiLayoutFixes::LogInfo("disabled by [general] enabled=0; no patch was written");
            UiLayoutFixes::LogSummaryStatus(
                "Initialization", "was skipped because the plugin is disabled");
            return 1;
        }

        if (!UiLayoutFixes::HasEnabledModule())
        {
            UiLayoutFixes::LogInfo(
                "all window modules are disabled; no patch was written");
            UiLayoutFixes::LogSummaryStatus(
                "Initialization", "was skipped because no module is enabled");
            return 1;
        }

        UiLayoutFixes::LogSummary("Initialization", true);
        return 0;
    }
    catch (const std::exception& error)
    {
        UiLayoutFixes::LogFatal("core", "exception",
            "C++ exception in TsmPluginInit: %s. Action: preserve the log and report the failure; no patch was written",
            error.what());
        UiLayoutFixes::LogSummary("Initialization", false);
        return 1;
    }
    catch (...)
    {
        UiLayoutFixes::LogFatal("core", "exception",
            "Unknown C++ exception in TsmPluginInit. Action: preserve the log and report the failure; no patch was written");
        UiLayoutFixes::LogSummary("Initialization", false);
        return 1;
    }
}

extern "C" __declspec(dllexport)
int TsmPluginStart(void)
{
    UiLayoutFixes::BeginLogPhase();
    try
    {
        if (!H || !UiLayoutFixes::g_bound || !g_exeBase ||
            !UiLayoutFixes::g_enabled)
        {
            UiLayoutFixes::LogFatal("core", "startup-state",
                "Start was called without a valid initialized plugin state. Action: preserve the log and restart the game; no patch was written");
            UiLayoutFixes::LogSummary("Startup", false);
            return 1;
        }

        if (!UiLayoutFixes::Customhouse::Install())
        {
            UiLayoutFixes::LogFatal(
                "CUSTOMHOUSE", "startup",
                "Module activation failed. Action: verify the supported game build and conflicting UI plugins, then restart the game");
            UiLayoutFixes::LogSummary("Startup", false);
            return 1;
        }

        // A failed import redirection leaves this module inactive but hooks
        // nothing, so the other modules and the startup result are unaffected.
        UiLayoutFixes::TextWrap::Install();

        char active[64];
        UiLayoutFixes::LogInfo(
            "startup complete; active modules=%s; unrelated windows=unchanged; total_elapsed_ms=%llu",
            UiLayoutFixes::ActiveModules(active, sizeof(active)),
            (unsigned long long)(GetTickCount64() - UiLayoutFixes::g_logStarted));
        UiLayoutFixes::LogSummary("Startup", true);
        return 0;
    }
    catch (const std::exception& error)
    {
        UiLayoutFixes::LogFatal("core", "exception",
            "C++ exception in TsmPluginStart: %s. Action: preserve the log and restart the game; patch state may be incomplete",
            error.what());
        UiLayoutFixes::LogSummary("Startup", false);
        return 1;
    }
    catch (...)
    {
        UiLayoutFixes::LogFatal("core", "exception",
            "Unknown C++ exception in TsmPluginStart. Action: preserve the log and restart the game; patch state may be incomplete");
        UiLayoutFixes::LogSummary("Startup", false);
        return 1;
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH)
    {
        if (UiLayoutFixes::g_detail != INVALID_HANDLE_VALUE)
        {
            CloseHandle(UiLayoutFixes::g_detail);
            UiLayoutFixes::g_detail = INVALID_HANDLE_VALUE;
        }
        if (UiLayoutFixes::g_bound)
        {
            DeleteCriticalSection(&g_lock);
            UiLayoutFixes::g_bound = false;
        }
    }
    return TRUE;
}
