// technical_service_storage - native depot UI, grit operation, persistence and storage migration.
//
// Key design:
//   - Technical Services are identified by $TYPE_GARBAGE_OFFICE (type 49).
//   - The plugin observes storage blocks already rendered by the native panel.
//   - Resources already rendered by the game are never rendered a second time.
//   - Additional storage rows use the game's own native storage renderer.
//   - No cargo resource name is hard-coded; future resources work automatically.
//   - The supported building.ini contract is intentionally narrow:
//       $STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_COVERED <capacity> <resource>
//       $STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL <capacity> <resource>
//       $STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN    <capacity> <resource>
//       $STORAGE_FUEL           RESOURCE_TRANSPORT_OIL     <capacity>
//   - At runtime the parser token itself is no longer present in Storage, so the
//     policy is enforced structurally: one resource slot and transport class
//     COVERED, OPEN, GRAVEL or OIL. General multi-resource storages are ignored.
//   - Existing live storage order is preserved. At savegame load, v0.1.75
//     appends missing configured grit storages in current definition order.
//   - The same live layout Y value is advanced, so the vanilla scrollable panel
//     grows naturally when more resource rows are appended.
//   - UI enumeration is read-only. On a confirmed snowplow refill, the grit
//     spreader atomically debits the actually loaded amount from the selected
//     single-resource material slot in the vehicle's home depot. The old
//     per-road direct-depot consumption was removed in v0.1.76.
//
// Verified game build:
//   WRSR       1.1.1.9
//   SHA256     296644a9f207d609031fc2ae73fed2dcb34619a1d55a35d1c7b51965ce6841b8
//   timestamp  0x6A3EB6AD
//   image size 0xA9D000
//
// Verified UI path:
//   Technical Services panel        RVA 0x74C500
//   native storage renderer         RVA 0x706C00
//   post-storage section helper     RVA 0x6F4B40
//   native Technical Services call  RVA 0x74F880 -> 0x706C00
//   post-storage insertion call     RVA 0x74F899 -> 0x6F4B40
//
// Confirmed runtime layout:
//   building + 0x970/+0x978 = std::vector<Storage>, stride 0xE0
//   storage  + 0x00/+0x08   = std::vector<Slot>, stride 0x10
//   storage  + 0x8C         = capacity
//   storage  + 0x90         = transport class
//   slot     + 0x00         = Resource*
//   slot     + 0x08         = amount
//   resource + 0x00         = internal name
//   resource + 0x40         = game localization text ID
//
// All comments and log output are intentionally English.

#if __has_include("../../src/tesmio_plugin.h")
#include "../../src/tesmio_plugin.h"
#else
#include "../tesmio_plugin.h"
#endif

#include <float.h>
#include "../grit_spreader_api.h"

#ifndef TSM_SERVICE_LOCALIZATION
#define TSM_SERVICE_LOCALIZATION "localization"
#define TSM_LOCALIZATION_VERSION 1u
typedef struct TsmLocalizationApi
{
    int (*resolve)(const char* nameSpace, const char* key);
    int (*resolveFull)(const char* fullyQualifiedKey);
} TsmLocalizationApi;
#endif

#define PLUGIN_VERSION  "0.3.3"
#define SPREADER_DIAGNOSTIC_VERSION PLUGIN_VERSION
#define PLUGIN_LOG_NAME "tesmioloader.technical_service_storage.log"

#pragma comment(lib, "user32.lib")

// -----------------------------------------------------------------------------
// Verified WRSR v1.1.1.9 addresses and signatures

static const DWORD RVA_TECHNICAL_SERVICES_PANEL = 0x0074C500;
static const DWORD RVA_NATIVE_STORAGE_PANEL      = 0x00706C00;
static const DWORD RVA_POST_STORAGE_SECTION      = 0x006F4B40;
static const DWORD RVA_TECH_NATIVE_STORAGE_CALL  = 0x0074F880;
static const DWORD RVA_TECH_POST_STORAGE_CALL    = 0x0074F899;
// The native warning builder has completed at this instruction sequence, but
// its UTF-16 text at rbp+0x250 has not yet been measured or drawn.  v0.1.47
// appends the material warnings here so the game itself applies its warning
// font, wrapping, line spacing and dynamic panel-height calculation.
static const DWORD RVA_TECH_WARNING_APPEND_TRANSITION = 0x0074DAFD;
static const size_t TECH_WARNING_APPEND_OVERWRITE_LENGTH = 22;
static const size_t TECH_WARNING_BUFFER_CAPACITY = 1024;
// The stock full-row storage renderer uses one shared flag at rbp-0x40 to
// decide whether every per-resource discard button is visible. Technical
// Services model fuel and every configured material as separate one-slot
// Storage objects, so one empty object incorrectly hides the buttons belonging
// to all still-stocked objects. v0.1.47 preserves the stock decision in every
// other panel and bypasses only this gate while the Technical Services panel is
// being rendered. The original renderer still owns the per-row amount check,
// icon, localized tooltip, dynamic discard step and click operation.
static const DWORD RVA_STORAGE_DISCARD_VISIBILITY_GATE = 0x0070F826;
static const DWORD RVA_STORAGE_DISCARD_VISIBILITY_FALLTHROUGH = 0x0070F830;
static const DWORD RVA_STORAGE_DISCARD_VISIBILITY_SKIP = 0x0070FB96;
static const size_t STORAGE_DISCARD_VISIBILITY_OVERWRITE_LENGTH = 10;
// The executable contains three copies of the Empty weight / Engine power UI.
// v0.1.20 and v0.1.21 hooked the first two inactive copies. The visible vehicle
// window uses the third copy: r15 is the selected vehicle throughout this block
// and the engine-label call at 0x7DBF07 uses the shared formatted-text renderer.
static const DWORD RVA_VEHICLE_UI_EMPTY_WEIGHT_READ = 0x007C0AF9;
static const DWORD RVA_VEHICLE_UI_ENGINE_POWER_READ = 0x007C0B02;
static const DWORD RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL = 0x007DBF07;
static const DWORD RVA_OFFICE_PRIORITY_LABEL_DRAW_CALL = 0x002735EB;
static const DWORD RVA_OFFICE_PRIORITY_ROW = 0x002734F0;
static const DWORD RVA_OFFICE_PRIORITY_DRAW_TEXT_IAT = 0x0086C878;
static const DWORD RVA_OFFICE_PRIORITY_DRAW_FORMATTED_TEXT_IAT = 0x0086C880;
static const DWORD RVA_LOCALIZATION_GET_TEXT_IAT = 0x0086C890;
static const DWORD RVA_LOCALIZATION_CONTEXT = 0x00997590;
static const DWORD RVA_PANEL_COLLISION_IAT = 0x0086C850;
static const DWORD RVA_INPUT_GET_MOUSE_SOLID_IAT = 0x0086C868;
static const DWORD RVA_PANEL_CONTEXT = 0x009BE060;
static const DWORD RVA_PANEL_RECT_SIZE = 0x009BE2E8;
static const DWORD RVA_PANEL_RECT_POSITION = 0x009BE2F0;
static const DWORD RVA_PANEL_RECT_PADDING = 0x009BE2F8;
static const DWORD RVA_PANEL_RECT_COLOR = 0x009BE30C;
static const DWORD RVA_INPUT_CONTEXT = 0x00A54B90;
static const DWORD RVA_INPUT_CLICK_FLAG = 0x00A54E91;
static const DWORD RVA_FONT_MANAGER_CONTEXT = 0x00996FB0;
static const DWORD RVA_CURRENT_UI_FONT_POINTER = 0x00995220;
static const int NATIVE_TEXT_ID_PRIORITY = 20139;

static const DWORD RVA_OFFICE_PRIORITY_ROW_CALLS[9] = {
    0x0074EF42, 0x0074F02F, 0x0074F10B,
    0x0074F1E7, 0x0074F2C3, 0x0074F39F,
    0x0074F47B, 0x0074F557, 0x0074F633
};
static const DWORD RVA_OFFICE_PRIORITY_PERCENT_CALLS[9] = {
    0x0074EFA4, 0x0074F089, 0x0074F165,
    0x0074F241, 0x0074F31D, 0x0074F3F9,
    0x0074F4D5, 0x0074F5B1, 0x0074F68D
};
static const DWORD RVA_OFFICE_PRIORITY_CURSOR_STORES[9] = {
    0x0074EFDF, 0x0074F0BB, 0x0074F197,
    0x0074F273, 0x0074F34F, 0x0074F42B,
    0x0074F507, 0x0074F5E3, 0x0074F73A
};

// Values used by the verified native Technical Services storage call.
static const DWORD RVA_DPI           = 0x00992088;
static const DWORD RVA_STORAGE_ARG13 = 0x0090A960;
static const DWORD RVA_STORAGE_ARG14 = 0x0090A928;
static const DWORD RVA_TECH_WARNING_MEASURE_PADDING = 0x0090A8CC;
// The single-resource branch in the native renderer first advances its local
// row phase by 23 logical pixels (exe+0x707F7F), then draws the title 45
// logical pixels above that phase (exe+0x7084C6). Reading both verified game
// constants keeps the plugin title control coupled to the same layout math as
// the native title instead of guessing from the renderer's final cursor.
static const DWORD RVA_STORAGE_TITLE_PHASE_ADVANCE = 0x0090A95C;
static const DWORD RVA_STORAGE_TITLE_LEAD          = 0x0090AA0C;

static const DWORD EXPECTED_TIMESTAMP = 0x6A3EB6AD;
static const size_t EXPECTED_IMAGE_SIZE = 0xA9D000;

static const BYTE EXPECT_TECH_PANEL[] = {
    0x48,0x8B,0xC4,
    0x55,
    0x56,
    0x57,
    0x41,0x54,
    0x41,0x55,
    0x41,0x56,
    0x41,0x57
};

static const BYTE EXPECT_NATIVE_STORAGE_PANEL[] = {
    0x48,0x8B,0xC4,
    0x55,
    0x53,
    0x56,
    0x57,
    0x41,0x54,
    0x41,0x55,
    0x41,0x56,
    0x41,0x57
};

static const BYTE EXPECT_POST_STORAGE_SECTION[] = {
    0x48,0x89,0x5C,0x24,0x10,
    0x48,0x89,0x6C,0x24,0x18,
    0x56,
    0x57,
    0x41,0x56
};

static const BYTE EXPECT_VEHICLE_UI_EMPTY_WEIGHT_READ[] = {
    0xF3,0x44,0x0F,0x10,0x86,0x7C,0x86,0x00,0x00
};

static const BYTE EXPECT_VEHICLE_UI_ENGINE_POWER_READ[] = {
    0xF3,0x0F,0x10,0xBE,0x78,0x86,0x00,0x00
};

static const BYTE EXPECT_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL[] = {
    0xFF,0x15,0x73,0x09,0x09,0x00
};

static const BYTE EXPECT_TECH_WARNING_APPEND_TRANSITION[] = {
    0x41,0x0F,0x28,0xF0,
    0xF3,0x44,0x0F,0x10,0x0D,0xC2,0xCD,0x1B,0x00,
    0xF3,0x44,0x0F,0x10,0x15,0x15,0xCE,0x1B,0x00
};
static_assert(
    sizeof(EXPECT_TECH_WARNING_APPEND_TRANSITION) ==
        TECH_WARNING_APPEND_OVERWRITE_LENGTH,
    "The warning transition signature must cover every overwritten byte");

static const BYTE EXPECT_STORAGE_DISCARD_VISIBILITY_GATE[] = {
    0x80,0x7D,0xC0,0x00,
    0x0F,0x84,0x66,0x03,0x00,0x00
};
static_assert(
    sizeof(EXPECT_STORAGE_DISCARD_VISIBILITY_GATE) ==
        STORAGE_DISCARD_VISIBILITY_OVERWRITE_LENGTH,
    "The discard-visibility signature must cover every overwritten byte");

// C3DDLL64.dll export
// C3D_BITMAP_FONT::CalcRectLeftUnicodeNoArg(float,float,wchar_t const*)
// from the game-bundled engine used by WRSR 1.1.1.9. Its 16-byte C3DRECT
// result is returned through the hidden second argument on Windows x64.
static const char* const C3D_CALC_RECT_LEFT_UNICODE_NO_ARG =
    "?CalcRectLeftUnicodeNoArg@C3D_BITMAP_FONT@@UEAA?AUC3DRECT@@MMPEB_W@Z";
static const BYTE EXPECT_C3D_CALC_RECT_LEFT_UNICODE_NO_ARG[] = {
    0x48,0x8B,0xC4,                    // mov rax,rsp
    0x53,                              // push rbx
    0x57,                              // push rdi
    0x48,0x81,0xEC,0x98,0x08,0x00,0x00, // sub rsp,898h
    0x0F,0x29,0x70,0xD8,              // movaps [rax-28h],xmm6
    0x0F,0x29,0x78,0xC8               // movaps [rax-38h],xmm7
};

static const BYTE EXPECT_OFFICE_PRIORITY_LABEL_DRAW_CALL[] = {
    0xFF,0x15,0x87,0x92,0x5F,0x00
};

static const BYTE EXPECT_OFFICE_PRIORITY_CURSOR_STORE[] = {
    0xF3,0x0F,0x11,0x55,0x80
};

// -----------------------------------------------------------------------------
// Runtime layout

static const size_t W_HIDDEN   = 0x0001;
static const size_t W_BUILDING = 0x0240;

static const size_t B_TYPEDESC      = 0x0318;
static const size_t B_STORAGE_BEGIN = 0x0970;
static const size_t B_STORAGE_END   = 0x0978;
static const size_t B_NATIVE_ARG10  = 0x09E8;
static const size_t B_VEHICLE_BEGIN = 0x0C70;
static const size_t B_VEHICLE_END   = 0x0C78;

static const size_t TD_TYPE = 0x0360;
static const int BUILDING_GARBAGE_OFFICE = 0x31;

static const size_t STORAGE_SIZE       = 0x00E0;
static const size_t STORAGE_SLOT_BEGIN = 0x0000;
static const size_t STORAGE_SLOT_END   = 0x0008;
static const size_t STORAGE_CAPACITY   = 0x008C;
static const size_t STORAGE_CLASS      = 0x0090;

static const size_t SLOT_SIZE     = 0x0010;
static const size_t SLOT_RESOURCE = 0x0000;
static const size_t SLOT_AMOUNT   = 0x0008;

static const size_t RESOURCE_NAME = 0x0000;
static const size_t RESOURCE_TEXT_ID = 0x0040;
static const DWORD RVA_RESOURCE_VECTOR = 0x009E11C0;
static const size_t RESOURCE_RECORD_SIZE = 0x0340;
static const size_t MIN_RESOURCE_RECORDS = 57;
static const size_t MAX_RESOURCE_RECORDS = 512;

// RESOURCE_TRANSPORT_* follows the game's enum order.
static const int RESOURCE_TRANSPORT_COVERED = 0;
static const int RESOURCE_TRANSPORT_OPEN    = 1;
static const int RESOURCE_TRANSPORT_GRAVEL  = 2;
static const int RESOURCE_TRANSPORT_OIL     = 3;

static const size_t MAX_STORAGES = 64;
static const size_t MAX_SLOTS_PER_STORAGE = 64;
// A native storage can expose up to MAX_SLOTS_PER_STORAGE resources. Keep the
// duplicate tracker bounded to the same structural limits as the validated
// building data instead of allocating memory while a native UI hook is active.
static const int MAX_SEEN_RESOURCES =
    (int)(MAX_STORAGES * MAX_SLOTS_PER_STORAGE);
static const int MAX_SEEN_STORAGES = 64;

// -----------------------------------------------------------------------------
// Native calls

typedef void (__fastcall *NativeStoragePanelFn)(
    void* game,
    void* window,
    void* storage,
    float* layoutX,
    float* layoutY,
    int arg6,
    BYTE arg7,
    float arg8,
    int arg9,
    int arg10,
    int arg11,
    float arg12,
    float arg13,
    float arg14,
    BYTE arg15,
    BYTE arg16);

typedef void (__fastcall *TechnicalServicesPanelFn)(void* game, void* window);
typedef void (__fastcall *PostStorageSectionFn)(void* game, void* window,
                                                 float* layoutX, float* layoutY);
typedef void (__fastcall *NativeOfficePriorityDrawTextFn)(
    void* renderer,
    void* font,
    float rightX,
    float y,
    DWORD color,
    const wchar_t* text);
typedef const wchar_t* (__fastcall *NativeLocalizationGetTextFn)(
    void* localization,
    int textId);
typedef bool (__fastcall *NativeOfficePriorityRowFn)(
    void* context,
    float labelRightX,
    float rowY,
    int textId,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uintptr_t arg9,
    uintptr_t arg10,
    uintptr_t arg11);
typedef void (__fastcall *NativeOfficePriorityDrawFormattedTextFn)(
    void* renderer,
    void* font,
    float x,
    float y,
    DWORD color,
    const wchar_t* format,
    int value);
typedef void (__fastcall *NativeVehicleDrawFormattedTextFn)(
    void* renderer,
    void* font,
    float x,
    float y,
    DWORD color,
    const wchar_t* format,
    ...);
typedef void (__fastcall *NativeVehicleEngineLabelDrawFn)(
    void* renderer,
    void* font,
    float x,
    float y,
    DWORD color,
    const wchar_t* format,
    const wchar_t* localizedLabel);

struct NativeC3dRect
{
    int left;
    int top;
    int right;
    int bottom;
};
static_assert(sizeof(NativeC3dRect) == 16,
              "C3DRECT must remain four 32-bit integers");

typedef NativeC3dRect* (__fastcall *NativeVehicleCalcTextRectFn)(
    void* font,
    NativeC3dRect* result,
    float x,
    float y,
    const wchar_t* text);
typedef void* (__fastcall *NativeInputGetMouseSolidFn)(
    void* input,
    void* resultBuffer);
typedef bool (__fastcall *NativePanelCollisionFn)(
    void* panel,
    void* mousePosition,
    float halfWidth,
    float halfHeight);

static NativeStoragePanelFn g_originalNativeStoragePanel = nullptr;
static TechnicalServicesPanelFn g_originalTechnicalServicesPanel = nullptr;
static PostStorageSectionFn g_originalPostStorageSection = nullptr;
static NativeOfficePriorityDrawTextFn
    g_originalOfficePriorityDrawText = nullptr;
static NativeOfficePriorityRowFn g_originalOfficePriorityRow = nullptr;
static NativeOfficePriorityDrawFormattedTextFn
    g_originalOfficePriorityDrawFormattedText = nullptr;
static NativeLocalizationGetTextFn g_nativeLocalizationGetText = nullptr;
static NativeVehicleDrawFormattedTextFn
    g_nativeVehicleDrawFormattedText = nullptr;
static NativeVehicleCalcTextRectFn
    g_nativeVehicleCalcTextRect = nullptr;
static NativeInputGetMouseSolidFn
    g_nativeInputGetMouseSolid = nullptr;
static NativePanelCollisionFn
    g_nativePanelCollision = nullptr;
static const TsmLocalizationApi* g_localization = nullptr;
static int g_vehicleTankLabelTextId = 0;
static int g_vehicleTankNoMaterialTextId = 0;
static int g_vehicleTankDryPlowingTextId = 0;
static int g_vehicleTankDepotTextId = 0;
static int g_depotPriorityLabelTextId = NATIVE_TEXT_ID_PRIORITY;
static int g_depotPriorityOffTextId = 0;
static int g_depotWarningResourceTextId = 0;
static int g_depotWarningEmptyTextId = 0;
static int g_depotWarningOffTextId = 0;

static float ReadExeFloat(DWORD rva, float fallback);

// -----------------------------------------------------------------------------
// Wrapped Office Priority labels
//
// WRSR's native UI is UTF-16. The original draft used char strings, which
// would split translated labels in the middle of a multibyte character. Keep
// the helper independent of any guessed executable address: the native
// renderer bridge can use it only after its measure/draw call signatures and
// exact row boundaries have been verified for the supported game build.

static const int OFFICE_PRIORITY_MAX_LINES = 8;
static const size_t OFFICE_PRIORITY_LINE_CAPACITY = 256;

struct OfficePriorityLayout
{
    wchar_t lines[OFFICE_PRIORITY_MAX_LINES]
                 [OFFICE_PRIORITY_LINE_CAPACITY];
    int lineCount;
    float rowTop;
    float rowHeight;
    float labelTop;
    float controlsTop;
};

typedef float (__fastcall *OfficePriorityMeasureTextFn)(
    void* context,
    const wchar_t* text);

typedef void (__fastcall *OfficePriorityDrawTextFn)(
    void* context,
    const wchar_t* text,
    float rightX,
    float y);

typedef void (__fastcall *OfficePriorityDrawControlsFn)(
    void* context,
    float y);

static bool IsOfficePrioritySpace(wchar_t value)
{
    return value == L' ' || value == L'\t' ||
           value == L'\r' || value == L'\n';
}

static void CopyOfficePriorityText(
    wchar_t* destination,
    size_t destinationSize,
    const wchar_t* source)
{
    if (!destination || destinationSize == 0) return;
    destination[0] = 0;
    if (source)
        wcsncpy_s(destination, destinationSize, source, _TRUNCATE);
}

static bool AppendOfficePriorityWord(
    wchar_t* destination,
    size_t destinationSize,
    const wchar_t* word,
    size_t wordLength)
{
    if (!destination || destinationSize == 0 ||
        !word || wordLength == 0)
        return false;

    size_t used = wcslen(destination);
    size_t separator = used ? 1 : 0;
    if (used + separator + wordLength + 1 > destinationSize)
        return false;

    if (separator) destination[used++] = L' ';
    memcpy(destination + used, word, wordLength * sizeof(wchar_t));
    destination[used + wordLength] = 0;
    return true;
}

static float MeasureOfficePriorityText(
    OfficePriorityMeasureTextFn measureText,
    void* context,
    const wchar_t* text)
{
    if (!measureText || !text) return 0.0f;
    float width = measureText(context, text);
    if (!(width == width) || width < 0.0f || width > 100000.0f)
        return 0.0f;
    return width;
}

static void EllipsizeLastOfficePriorityLine(
    wchar_t line[OFFICE_PRIORITY_LINE_CAPACITY])
{
    if (!line) return;
    size_t used = wcslen(line);
    if (used + 4 < OFFICE_PRIORITY_LINE_CAPACITY)
    {
        line[used++] = L'.';
        line[used++] = L'.';
        line[used++] = L'.';
        line[used] = 0;
        return;
    }
    if (OFFICE_PRIORITY_LINE_CAPACITY >= 4)
    {
        size_t end = OFFICE_PRIORITY_LINE_CAPACITY - 1;
        line[end - 3] = L'.';
        line[end - 2] = L'.';
        line[end - 1] = L'.';
        line[end] = 0;
    }
}

static int WrapOfficePriorityText(
    const wchar_t* text,
    float maximumWidth,
    OfficePriorityMeasureTextFn measureText,
    void* context,
    wchar_t lines[OFFICE_PRIORITY_MAX_LINES]
                 [OFFICE_PRIORITY_LINE_CAPACITY])
{
    for (int i = 0; i < OFFICE_PRIORITY_MAX_LINES; ++i)
        lines[i][0] = 0;
    if (!text || !text[0]) return 0;

    if (!measureText || maximumWidth <= 0.0f ||
        MeasureOfficePriorityText(measureText, context, text) <= maximumWidth)
    {
        CopyOfficePriorityText(
            lines[0], OFFICE_PRIORITY_LINE_CAPACITY, text);
        return 1;
    }

    wchar_t currentLine[OFFICE_PRIORITY_LINE_CAPACITY] = {};
    const wchar_t* cursor = text;
    int lineCount = 0;
    bool truncated = false;

    while (*cursor)
    {
        while (*cursor && IsOfficePrioritySpace(*cursor)) ++cursor;
        if (!*cursor) break;

        const wchar_t* wordStart = cursor;
        while (*cursor && !IsOfficePrioritySpace(*cursor)) ++cursor;
        size_t wordLength = (size_t)(cursor - wordStart);

        wchar_t candidate[OFFICE_PRIORITY_LINE_CAPACITY] = {};
        CopyOfficePriorityText(candidate, _countof(candidate), currentLine);
        bool appended = AppendOfficePriorityWord(
            candidate, _countof(candidate), wordStart, wordLength);
        float width = appended
            ? MeasureOfficePriorityText(measureText, context, candidate)
            : maximumWidth + 1.0f;

        if (currentLine[0] && (!appended || width > maximumWidth))
        {
            if (lineCount >= OFFICE_PRIORITY_MAX_LINES)
            {
                truncated = true;
                break;
            }
            CopyOfficePriorityText(lines[lineCount++],
                                   OFFICE_PRIORITY_LINE_CAPACITY,
                                   currentLine);
            currentLine[0] = 0;
            appended = AppendOfficePriorityWord(
                currentLine, _countof(currentLine), wordStart, wordLength);
        }
        else if (appended)
        {
            CopyOfficePriorityText(
                currentLine, _countof(currentLine), candidate);
        }

        // A translated or modded label can contain a single word wider than
        // the row. Split it by UTF-16 code unit rather than dropping it.
        if (!appended ||
            MeasureOfficePriorityText(measureText, context, currentLine) >
                maximumWidth)
        {
            currentLine[0] = 0;
            for (size_t i = 0; i < wordLength; ++i)
            {
                size_t used = wcslen(currentLine);
                if (used + 2 >= _countof(currentLine))
                {
                    truncated = true;
                    break;
                }
                currentLine[used] = wordStart[i];
                currentLine[used + 1] = 0;
                if (used && MeasureOfficePriorityText(
                                measureText, context, currentLine) >
                                maximumWidth)
                {
                    currentLine[used] = 0;
                    if (lineCount >= OFFICE_PRIORITY_MAX_LINES)
                    {
                        truncated = true;
                        break;
                    }
                    CopyOfficePriorityText(lines[lineCount++],
                                           OFFICE_PRIORITY_LINE_CAPACITY,
                                           currentLine);
                    currentLine[0] = wordStart[i];
                    currentLine[1] = 0;
                }
            }
            if (truncated) break;
        }
    }

    if (currentLine[0] && lineCount < OFFICE_PRIORITY_MAX_LINES)
    {
        CopyOfficePriorityText(lines[lineCount++],
                               OFFICE_PRIORITY_LINE_CAPACITY,
                               currentLine);
    }
    else if (currentLine[0])
    {
        truncated = true;
    }

    if (lineCount == 0)
    {
        CopyOfficePriorityText(
            lines[0], OFFICE_PRIORITY_LINE_CAPACITY, text);
        lineCount = 1;
    }
    if (truncated)
        EllipsizeLastOfficePriorityLine(lines[lineCount - 1]);
    return lineCount;
}




// -----------------------------------------------------------------------------
// Configuration and runtime state

static const char* const INI_NAME = "plugins\\technical_service_storage.ini";

static int g_enabled = 1;
static int g_storageMigrationEnabled = 1;
// The native loader and post-load migration take this exclusively. The one
// plugin worker takes it shared for its whole iteration, never across Sleep.
static SRWLOCK g_storageLoadLock = SRWLOCK_INIT;
static int g_debug = 0;
static int g_debugLimit = 100;
static int g_debugCount = 0;
static SRWLOCK g_runtimeWarningLock = SRWLOCK_INIT;
struct RuntimeWarningKey { const char* where; const char* rule; ULONGLONG last; };
static RuntimeWarningKey g_runtimeWarningKeys[64] = {};
static size_t g_runtimeWarningKeyCount = 0;
static int g_resourceGap = 14;
static int g_runtimeActive = 0;
static int g_officePriorityWrapEnabled = 1;
static int g_officePriorityMaximumWidth = 245;
static int g_officePriorityLineHeight = 22;
static int g_depotMaterialPriorityEnabled = 1;
static int g_depotMaterialPriorityOffsetX = 390;
static int g_depotMaterialPriorityOffsetY = 0;
static int g_depotMaterialPriorityWidth = 150;
static int g_depotMaterialPriorityHeight = 24;
static int g_depotMaterialPriorityPersistenceEnabled = 1;
static int g_sandTankPersistenceEnabled = 1;
static int g_depotMaterialDividerEnabled = 1;
static const int DEFAULT_MATERIAL_DIVIDER_HEIGHT = 56;
static int g_depotMaterialDividerHeight = DEFAULT_MATERIAL_DIVIDER_HEIGHT;
static int g_depotMaterialWarningsEnabled = 1;
static volatile LONG g_depotMaterialPriorityUiActive = 0;
static volatile LONG g_depotMaterialPriorityFaultLogged = 0;
static volatile LONG g_depotMaterialPriorityFirstDrawLogged = 0;
static volatile LONG g_depotMaterialPriorityFirstHoverLogged = 0;
static volatile LONG g_depotMaterialDividerFirstDrawLogged = 0;
static volatile LONG g_depotMaterialWarningUiActive = 0;
static volatile LONG g_depotMaterialWarningFaultLogged = 0;
static volatile LONG g_depotMaterialWarningFirstDrawLogged = 0;
static volatile LONG g_depotMaterialWarningCapacityLogged = 0;
static volatile LONG g_depotStorageDeleteButtonFirstLogged = 0;
static volatile LONG g_officePriorityWrapActive = 0;
static volatile LONG g_officePriorityLayoutActive = 0;
static volatile LONG g_officePriorityWrapFaultLogged = 0;
static volatile LONG g_officePriorityWrappedMask = 0;
static float g_officePriorityPendingRowExtra = 0.0f;
static float g_officePriorityPendingRowCenterShift = 0.0f;
static const wchar_t* g_officePriorityLabelPointers[9] = {};
static const int g_officePriorityTextIds[9] = {
    1127, // Dirt road
    1128, // Gravel road
    1137, // Panel road
    1129, // Asphalt road
    1130, // Asphalt road with street lights
    1131, // Electrified roads (trolleybus)
    1135, // Road with tram tracks
    1132, // Bridges
    1133  // One-way roads
};

// Grit spreader v0.1.74 retains the material, UI and tank behaviour while
// loading the bounded material catalogue from [grit_materials]. Each depot
// receives unique priorities 1..N or OFF; catalogue order supplies the initial
// values before the native resource-row controls are used. v0.1.72 persists
// them in a save-folder sidecar and gives every rebuilt lifetime fresh defaults.
// v0.1.74 stores each assigned snowplow's exact plugin-owned grit amount and
// loaded material in a separate sidecar belonging to the current savegame.
// The return threshold is a one-shot request only: the visible tank continues
// smoothly to zero while every confirmed native clear is forwarded. At zero,
// later confirmed clears publish dry treatment until the depot refill occurs.
// Empty dry vehicles re-evaluate their own depot both on every unique clear
// and during the periodic global registry scan, then request a return as soon
// as an OFF or empty material becomes available again. The current work route
// is handed to vanilla through its native refuelling-destination latch.
// While the refill lock is active, only the verified fuel-building destination
// field is latched to the home depot; the game then owns the route home. The
// plugin never changes real fuel, route vectors or target fields. The snowplow
// job boundary is left untouched. Physical depot arrival remains the primary
// refill confirmation. A positive native-fuel delta while the persistent home
// refuelling latch is active is the fallback for vehicles whose transient
// current-building field is not observed. Both paths refill the shadow tank
// independently of open UI windows and initialize every skill-35 snowplow from
// the global building registry.
static const size_t SPREADER_MAX_MATERIALS = 32;

struct SpreaderMaterialDefinition
{
    char resourceName[64];
    float protectionStrength;
    int runtimeResourceIndex;
    int runtimeValidationState; // 0=pending, 1=resolved, 2=missing/disabled
};

// Safe fallback used only when the INI section is absent or contains no valid
// entries. LoadSpreaderMaterialCatalog replaces it during initialization.
static SpreaderMaterialDefinition
    g_spreaderMaterials[SPREADER_MAX_MATERIALS] = {
        { "sand", 0.50f, -1, 0 }
    };
static size_t g_spreaderMaterialCount = 1;

static bool IsReservedNonSpreaderResource(const char* resourceName)
{
    // `fuel` is the Technical Services vehicle-fuel storage. It must never
    // become selectable grit, even if it is accidentally listed in the INI.
    return resourceName && _stricmp(resourceName, "fuel") == 0;
}

static int FindSpreaderMaterialIndex(const char* resourceName)
{
    if (!resourceName || !resourceName[0]) return -1;
    if (IsReservedNonSpreaderResource(resourceName)) return -1;
    for (size_t i = 0; i < g_spreaderMaterialCount; ++i)
    {
        if (g_spreaderMaterials[i].runtimeValidationState == 2)
            continue;
        if (_stricmp(g_spreaderMaterials[i].resourceName,
                     resourceName) == 0)
            return (int)i;
    }
    return -1;
}

static const SpreaderMaterialDefinition* SpreaderMaterialAt(int index)
{
    if (index < 0 || (size_t)index >= g_spreaderMaterialCount)
        return nullptr;
    return &g_spreaderMaterials[index];
}

static int g_sandDiagnosticEnabled = 1;
static int g_sandFuelConsumptionFactorPercent = 110;
static int g_sandDuplicateWindowMs = 5000;
static int g_sandSampleIntervalMs = 1000;
static int g_sandMaxVehicles = 512;
static int g_sandClearLogIntervalMs = 500;
static int g_sandShadowTankEnabled = 1;
static int g_sandAutomaticReturnEnabled = 1;
static int g_sandReturnLockEnabled = 1;
static int g_sandReturnRetryIntervalMs = 250;
static int g_sandReturnArrivalSettleMs = 250;
static int g_sandReturnThresholdBasisPoints = 2000;
static int g_sandTankWeightPercent = 10;
static int g_sandTankPowerKgPerKw = 2;
static int g_sandTankCapacityMultiplierPercent = 100;
static int g_sandTankCapacityStepKg = 50;
static int g_sandTankMinimumCapacityKg = 250;
static int g_sandTankMaximumCapacityKg = 5000;
static int g_sandVehicleTankDisplayEnabled = 1;
static int g_sandVehicleTankDisplayOffsetX = 0;
static int g_sandVehicleTankDisplayOffsetY = 22;
static int g_sandGlobalInitializationIntervalMs = 1000;
static int g_sandBuildingLifecycleDiagnosticEnabled = 1;
static volatile LONG g_sandVehicleTankDisplayActive = 0;
static volatile LONG g_sandVehicleTankDisplayFirstDrawLogged = 0;
static volatile LONG g_sandVehicleTankDisplayFaultLogged = 0;
static volatile LONG g_sandVehicleTankDisplayAnchorCalls = 0;
static volatile LONG g_sandVehicleTankDisplayWrapperCalls = 0;
static volatile LONG g_sandVehicleTankMeasureFallbackLogged = 0;
static void* volatile g_activeVehicleInfoVehicle = nullptr;

static HANDLE g_detail = INVALID_HANDLE_VALUE;
static volatile LONG g_logWarnings;
static volatile LONG g_logErrors;
static volatile LONG g_logFatals;
static ULONGLONG g_logStarted;
static LONG g_phaseWarnings, g_phaseErrors, g_phaseFatals;
static SRWLOCK g_detailLock = SRWLOCK_INIT;
static volatile LONG g_detailFailureLogged;
static decltype(&WriteFile) g_detailWriteFile = &WriteFile;
static bool g_materialCatalogValid = true;
static bool g_roadTreatmentServiceActive = false;
static volatile LONG g_hookHealthWarnings;
static void CountLevel(const char* level);

// The panel hook and the nested native renderer calls are expected to run
// synchronously on one game UI thread. The first matching panel records that
// thread; a matching panel on another thread is left entirely to vanilla code.
// All state in this block belongs to that synchronous panel call tree.
static volatile LONG g_uiThreadId = 0;
static volatile LONG g_uiThreadWarningLogged = 0;
static volatile LONG g_resourceLimitWarningLogged = 0;
static int g_insideTechnicalPanel = 0;
static BYTE* g_currentBuilding = nullptr;
static BYTE* g_lastDebugBuilding = nullptr;

static void* g_seenResources[MAX_SEEN_RESOURCES];
static int g_seenResourceCount = 0;
static BYTE* g_seenStorages[MAX_SEEN_STORAGES];
static int g_seenStorageCount = 0;

struct NativeTemplate
{
    int valid;
    int arg6;
    BYTE arg7;
    float arg8;
    int arg9;
    int arg10;
    int arg11;
    float arg12;
    float arg13;
    float arg14;
    BYTE arg15;
    BYTE arg16;
};

static NativeTemplate g_nativeTemplate = {};

static void LoadSandDiagnosticConfig();
static void SandDiagnosticObserve(void* game, BYTE* building);
static bool SandDiagnosticInstallClearHooks();

struct AppendStats
{
    int totalStorages;
    int appended;
    int duplicates;
    int unsupportedClass;
    int nonSingleResource;
    int invalid;
};

// -----------------------------------------------------------------------------
// Diagnostics

static void DetailWriteLine(const char* text)
{
    if (g_detail == INVALID_HANDLE_VALUE || !text ||
        InterlockedCompareExchange(&g_detailFailureLogged,0,0)) return;
    SYSTEMTIME t; GetLocalTime(&t);
    char line[4352];
    _snprintf_s(line,sizeof(line),_TRUNCATE,
        "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\r\n",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,text);
    DWORD error = ERROR_SUCCESS, offset = 0, length = (DWORD)strlen(line);
    AcquireSRWLockExclusive(&g_detailLock);
    while (offset < length && !InterlockedCompareExchange(&g_detailFailureLogged,0,0)) {
        DWORD put = 0;
        if (!g_detailWriteFile(g_detail,line+offset,length-offset,&put,nullptr)) {
            error = GetLastError(); break;
        }
        if (!put || put > length-offset) { error = ERROR_WRITE_FAULT; break; }
        offset += put;
    }
    bool report = error && InterlockedCompareExchange(&g_detailFailureLogged,1,0) == 0;
    ReleaseSRWLockExclusive(&g_detailLock);
    // Never recurse into the failed detail writer or call the host while locked.
    if (report) {
        CountLevel("WARN");
        if (H && H->log) H->log(
            "technical_service_storage WARN: detail-log [write-failed] Windows error %lu; detail output disabled, host log and gameplay remain active",error);
    }
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
        level,
        where && where[0] ? where : "-",
        rule && rule[0] ? rule : "general",
        body);

    CountLevel(level);
    if (H && H->log) Logf("technical_service_storage  %s", full);
    DetailWriteLine(full);
}

static void Report(const char* level, const char* where, const char* rule,
                   const char* fmt, ...)
{
    // Routine vehicle/clear/lifecycle traces are opt-in. Warnings and errors
    // always pass, independently of the debug switch and its message budget.
    if (!g_debug && level && strcmp(level, "INFO") == 0 && where &&
        (strcmp(where, "Tank diagnostic") == 0 ||
         strcmp(where, "Grit diagnostic") == 0 ||
         strcmp(where, "Building lifecycle diagnostic") == 0))
        return;
    va_list ap;
    va_start(ap, fmt);
    ReportV(level, where, rule, fmt, ap);
    va_end(ap);
}

static void Info(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ReportV("INFO", nullptr, "status", fmt, ap);
    va_end(ap);
}

static void Debug(const char* where, const char* rule, const char* fmt, ...)
{
    if (!g_debug || g_debugCount >= g_debugLimit) return;
    ++g_debugCount;

    va_list ap;
    va_start(ap, fmt);
    ReportV("DEBUG", where, rule, fmt, ap);
    va_end(ap);
}

static bool RuntimeWarningDue(const char* where, const char* rule, ULONGLONG now)
{
    where = where ? where : "-"; rule = rule ? rule : "general";
    bool due = true;
    AcquireSRWLockExclusive(&g_runtimeWarningLock);
    size_t i = 0;
    for (; i < g_runtimeWarningKeyCount; ++i) {
        auto& key = g_runtimeWarningKeys[i];
        if (!strcmp(key.where,where) && !strcmp(key.rule,rule)) {
            due = now - key.last >= 60000;
            if (due) key.last = now;
            break;
        }
    }
    if (i == g_runtimeWarningKeyCount) {
        // Call sites use static context/rule literals. Overflow reports rather
        // than hiding a new cause; the current call sites fit this registry.
        if (i < _countof(g_runtimeWarningKeys)) {
            g_runtimeWarningKeys[i] = {where,rule,now}; ++g_runtimeWarningKeyCount;
        }
    }
    ReleaseSRWLockExclusive(&g_runtimeWarningLock);
    return due;
}
static void RuntimeWarning(const char* where, const char* rule, const char* fmt, ...)
{
    if (!RuntimeWarningDue(where,rule,GetTickCount64())) return;
    va_list ap; va_start(ap,fmt);
    ReportV("WARN",where,rule,fmt,ap);
    va_end(ap);
}

static bool IsExpectedUiThread()
{
    DWORD current = GetCurrentThreadId();
    LONG known = InterlockedCompareExchange(
        &g_uiThreadId, (LONG)current, 0);

    if (known == 0 || (DWORD)known == current) return true;

    if (InterlockedCompareExchange(&g_uiThreadWarningLogged, 1, 0) == 0)
    {
        RuntimeWarning("Technical Services", "ui-thread",
            "A Technical Services panel was opened on thread %lu after the "
            "plugin UI context was bound to thread %lu; dynamic storage "
            "insertion was skipped for this call",
            (unsigned long)current, (unsigned long)(DWORD)known);
    }
    return false;
}

static void ReportWindows(const char* level, const char* where,
                          const char* rule, const char* action,
                          DWORD error, const char* remedy)
{
    char systemText[512] = {};
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, error, 0,
                             systemText, (DWORD)sizeof(systemText), NULL);

    while (n && (systemText[n - 1] == '\r' || systemText[n - 1] == '\n' ||
                 systemText[n - 1] == ' ' || systemText[n - 1] == '.'))
        systemText[--n] = 0;

    if (!n) strcpy_s(systemText, sizeof(systemText), "Unknown Windows error");

    Report(level, where, rule,
        "%s (Windows error %lu: %s). Action: %s",
        action, error, systemText,
        remedy ? remedy : "Review the preceding context and retry");
}

// Hook preparation can fail before publishing native branches. Once a branch
// is installed, report degraded protection/cache health but keep code resident:
// returning startup failure would let the loader unload a referenced DLL.
static decltype(&VirtualProtect) g_codeVirtualProtect = &VirtualProtect;
static decltype(&FlushInstructionCache) g_codeFlush = &FlushInstructionCache;
static bool FlushCodeChecked(const void* address, size_t length, const char* rule)
{
    if (g_codeFlush(GetCurrentProcess(),address,length)) return true;
    DWORD error = GetLastError();
    InterlockedIncrement(&g_hookHealthWarnings);
    ReportWindows("WARN","SOVIET64.exe",rule,
        "Instruction cache flush failed",error,"Restart the game; any installed bridge remains resident");
    return false;
}
static bool MakeCodeWritable(void* address, size_t length, DWORD* old, const char* rule)
{
    if (g_codeVirtualProtect(address,length,PAGE_EXECUTE_READWRITE,old)) return true;
    DWORD error = GetLastError();
    InterlockedIncrement(&g_hookHealthWarnings);
    ReportWindows("WARN","SOVIET64.exe",rule,
        "Code page could not be made writable; this patch was not installed",error,
        "Check conflicting hooks or security software");
    return false;
}
static bool FinishCodePatch(void* address, size_t length, DWORD old, const char* rule)
{
    DWORD ignored = 0;
    bool restored = g_codeVirtualProtect(address,length,old,&ignored) != 0;
    DWORD error = restored ? ERROR_SUCCESS : GetLastError();
    if (!restored) {
        InterlockedIncrement(&g_hookHealthWarnings);
        ReportWindows("WARN","SOVIET64.exe",rule,
            "Patch installed but original page protection could not be restored",error,
            "Restart the game; do not unload this plugin");
    }
    bool flushed = FlushCodeChecked(address,length,rule);
    return restored && flushed;
}

static const char* FeatureStatus(bool configured, bool installed)
{
    return !configured ? "disabled" : installed ? "active" : "unavailable";
}

static void BeginLogPhase()
{
    g_logStarted = GetTickCount64();
    g_phaseWarnings = InterlockedCompareExchange(&g_logWarnings,0,0);
    g_phaseErrors = InterlockedCompareExchange(&g_logErrors,0,0);
    g_phaseFatals = InterlockedCompareExchange(&g_logFatals,0,0);
}
static void LogSummaryStatus(const char* phase, const char* status)
{
    ULONGLONG elapsed = g_logStarted ? GetTickCount64() - g_logStarted : 0;
    Info("%s %s after %llu ms; %ld warning(s), %ld error(s), %ld fatal error(s) in this phase",
        phase,status,(unsigned long long)elapsed,
        InterlockedCompareExchange(&g_logWarnings,0,0)-g_phaseWarnings,
        InterlockedCompareExchange(&g_logErrors,0,0)-g_phaseErrors,
        InterlockedCompareExchange(&g_logFatals,0,0)-g_phaseFatals);
}

static void LogSummary(const char* phase, bool success)
{
    LogSummaryStatus(phase, success ? "completed successfully" : "failed");
}

// -----------------------------------------------------------------------------
// Native Office Priority label bridge

static float __fastcall EstimateOfficePriorityTextWidth(
    void* context,
    const wchar_t* text)
{
    if (!text) return 0.0f;
    float dpi = context ? *(float*)context : 1.0f;
    float width = 0.0f;
    for (size_t i = 0; text[i] && i < OFFICE_PRIORITY_LINE_CAPACITY; ++i)
    {
        wchar_t value = text[i];
        if (IsOfficePrioritySpace(value)) width += 4.5f;
        else if (wcschr(L"ilI.,'`!|:;()[]", value)) width += 5.0f;
        else if (value >= 0x2E80) width += 16.0f;
        else if (value >= L'A' && value <= L'Z') width += 9.5f;
        else width += 8.5f;
    }
    return width * dpi;
}

static void RefreshOfficePriorityLabelPointers()
{
    if (!g_nativeLocalizationGetText || !g_exeBase) return;
    void* localization = g_exeBase + RVA_LOCALIZATION_CONTEXT;
    for (int i = 0; i < (int)_countof(g_officePriorityTextIds); ++i)
    {
        g_officePriorityLabelPointers[i] =
            g_nativeLocalizationGetText(
                localization, g_officePriorityTextIds[i]);
    }
}

static int OfficePriorityLabelIndex(const wchar_t* text)
{
    if (!text) return -1;
    for (int i = 0; i < (int)_countof(g_officePriorityLabelPointers); ++i)
        if (g_officePriorityLabelPointers[i] == text) return i;

    // Resolve lazily on the UI thread and refresh after a language change so
    // cached localization pointers never decide which unrelated text to wrap.
    RefreshOfficePriorityLabelPointers();
    for (int i = 0; i < (int)_countof(g_officePriorityLabelPointers); ++i)
        if (g_officePriorityLabelPointers[i] == text) return i;
    return -1;
}

static int OfficePriorityLabelIndexFromTextId(int textId)
{
    for (int i = 0; i < (int)_countof(g_officePriorityTextIds); ++i)
        if (g_officePriorityTextIds[i] == textId) return i;
    return -1;
}

static int OfficePriorityWrappedLineCount(int labelIndex, float dpi)
{
    if (labelIndex < 0 ||
        labelIndex >= (int)_countof(g_officePriorityLabelPointers))
        return 1;
    if (!g_officePriorityLabelPointers[labelIndex])
        RefreshOfficePriorityLabelPointers();
    const wchar_t* text = g_officePriorityLabelPointers[labelIndex];
    if (!text) return 1;

    wchar_t lines[OFFICE_PRIORITY_MAX_LINES]
                 [OFFICE_PRIORITY_LINE_CAPACITY] = {};
    int lineCount = WrapOfficePriorityText(
        text,
        (float)g_officePriorityMaximumWidth * dpi,
        &EstimateOfficePriorityTextWidth,
        &dpi,
        lines);
    return lineCount > 1 ? 2 : 1;
}

static float OfficePriorityWrappedRowExtra(int lineCount, float dpi)
{
    if (lineCount <= 1) return 0.0f;
    // The verified native priority-row step is 35 logical pixels
    // (exe+0x90A9CC). Two wrapped baselines use the configured distance and
    // two logical pixels of padding above and below.
    float nativeRowHeight = 35.0f * dpi;
    float wrappedRowHeight =
        (float)lineCount * (float)g_officePriorityLineHeight * dpi +
        4.0f * dpi;
    return wrappedRowHeight > nativeRowHeight
         ? wrappedRowHeight - nativeRowHeight
         : 0.0f;
}

static bool __fastcall HookOfficePriorityRow(
    void* context,
    float labelRightX,
    float rowY,
    int textId,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uintptr_t arg9,
    uintptr_t arg10,
    uintptr_t arg11)
{
    g_officePriorityPendingRowExtra = 0.0f;
    g_officePriorityPendingRowCenterShift = 0.0f;

    __try
    {
        if (g_runtimeActive && g_insideTechnicalPanel &&
            g_officePriorityWrapEnabled &&
            InterlockedCompareExchange(
                &g_officePriorityLayoutActive, 0, 0))
        {
            int labelIndex = OfficePriorityLabelIndexFromTextId(textId);
            if (labelIndex >= 0)
            {
                float dpi = ReadExeFloat(RVA_DPI, 1.0f);
                if (!(dpi == dpi) || dpi <= 0.0f || dpi > 8.0f)
                    dpi = 1.0f;
                int lineCount =
                    OfficePriorityWrappedLineCount(labelIndex, dpi);
                float extra =
                    OfficePriorityWrappedRowExtra(lineCount, dpi);
                g_officePriorityPendingRowExtra = extra;
                g_officePriorityPendingRowCenterShift = extra * 0.5f;
                rowY += g_officePriorityPendingRowCenterShift;
            }
        }
    }
    __except(FaultFilter("office priority row layout",
                         GetExceptionInformation()))
    {
        g_officePriorityPendingRowExtra = 0.0f;
        g_officePriorityPendingRowCenterShift = 0.0f;
    }

    return g_originalOfficePriorityRow(
        context, labelRightX, rowY, textId,
        arg5, arg6, arg7, arg8, arg9, arg10, arg11);
}

static void __fastcall HookOfficePriorityDrawFormattedText(
    void* renderer,
    void* font,
    float x,
    float y,
    DWORD color,
    const wchar_t* format,
    int value)
{
    if (g_runtimeActive && g_insideTechnicalPanel &&
        InterlockedCompareExchange(&g_officePriorityLayoutActive, 0, 0))
        y += g_officePriorityPendingRowCenterShift;
    g_originalOfficePriorityDrawFormattedText(
        renderer, font, x, y, color, format, value);
}

static void __fastcall HookOfficePriorityDrawText(
    void* renderer,
    void* font,
    float rightX,
    float y,
    DWORD color,
    const wchar_t* text)
{
    if (!g_originalOfficePriorityDrawText) return;

    wchar_t lines[OFFICE_PRIORITY_MAX_LINES]
                 [OFFICE_PRIORITY_LINE_CAPACITY] = {};
    int lineCount = 0;
    int labelIndex = -1;
    float dpi = 1.0f;

    __try
    {
        if (g_runtimeActive && g_insideTechnicalPanel &&
            g_officePriorityWrapEnabled && text)
        {
            labelIndex = OfficePriorityLabelIndex(text);
            if (labelIndex >= 0)
            {
                dpi = ReadExeFloat(RVA_DPI, 1.0f);
                if (!(dpi == dpi) || dpi <= 0.0f || dpi > 8.0f)
                    dpi = 1.0f;
                lineCount = WrapOfficePriorityText(
                    text,
                    (float)g_officePriorityMaximumWidth * dpi,
                    &EstimateOfficePriorityTextWidth,
                    &dpi,
                    lines);
                if (lineCount > 2)
                {
                    lineCount = 2;
                    EllipsizeLastOfficePriorityLine(lines[1]);
                }
            }
        }
    }
    __except(FaultFilter("office priority label wrapping",
                         GetExceptionInformation()))
    {
        lineCount = 0;
        if (InterlockedCompareExchange(
                &g_officePriorityWrapFaultLogged, 1, 0) == 0)
        {
            Report("WARN", "Technical Services", "office-priority-wrap",
                "A translated priority label could not be wrapped safely; its native single-line rendering was retained");
        }
    }

    if (lineCount <= 1)
    {
        g_originalOfficePriorityDrawText(
            renderer, font, rightX, y, color, text);
        return;
    }

    float lineHeight = (float)g_officePriorityLineHeight * dpi;
    float firstY = y - lineHeight * 0.5f;
    g_originalOfficePriorityDrawText(
        renderer, font, rightX, firstY, color, lines[0]);
    g_originalOfficePriorityDrawText(
        renderer, font, rightX, firstY + lineHeight, color, lines[1]);

    LONG bit = labelIndex >= 0 && labelIndex < 30
             ? (LONG)(1u << labelIndex) : 0;
    if (bit && !(InterlockedOr(&g_officePriorityWrappedMask, bit) & bit))
    {
        Report("INFO", "Technical Services", "office-priority-wrap",
            "text_id=%d was wrapped into two centred lines (maximum_width=%d logical_px line_height=%d logical_px)",
            g_officePriorityTextIds[labelIndex],
            g_officePriorityMaximumWidth,
            g_officePriorityLineHeight);
    }
}

static bool OfficePriorityDirectCallMatches(DWORD callRva,
                                             DWORD targetRva)
{
    BYTE* callSite = g_exeBase + callRva;
    if (!ReadablePtr(callSite, 5) || callSite[0] != 0xE8)
        return false;
    int relative = 0;
    memcpy(&relative, callSite + 1, sizeof(relative));
    return callSite + 5 + relative == g_exeBase + targetRva;
}

static bool OfficePriorityIndirectCallMatches(DWORD callRva,
                                               DWORD iatRva)
{
    BYTE* callSite = g_exeBase + callRva;
    if (!ReadablePtr(callSite, 6) ||
        callSite[0] != 0xFF || callSite[1] != 0x15)
        return false;
    int relative = 0;
    memcpy(&relative, callSite + 2, sizeof(relative));
    return callSite + 6 + relative == g_exeBase + iatRva;
}

static bool PatchOfficePriorityCall(DWORD callRva, BYTE* stub,
                                    size_t originalLength,
                                    const char* rule)
{
    if (!stub || (originalLength != 5 && originalLength != 6))
        return false;
    BYTE* callSite = g_exeBase + callRva;
    INT64 distance = (INT64)(uintptr_t)stub -
                     (INT64)(uintptr_t)(callSite + 5);
    if (distance < INT_MIN || distance > INT_MAX)
    {
        Report("ERROR", "SOVIET64.exe", rule,
            "Generated hook stub for exe+0x%X is outside rel32 range",
            callRva);
        return false;
    }

    BYTE replacement[6] = { 0xE8,0,0,0,0,0x90 };
    int relative = (int)distance;
    memcpy(replacement + 1, &relative, sizeof(relative));
    DWORD oldProtect = 0;
    if (!MakeCodeWritable(callSite, originalLength, &oldProtect, rule)) return false;
    memcpy(callSite, replacement, originalLength);
    FinishCodePatch(callSite, originalLength, oldProtect, rule);
    return true;
}

static void BuildOfficePriorityJumpStub(BYTE* stub, void* handler)
{
    BYTE code[16] = {};
    memset(code, 0xCC, sizeof(code));
    code[0] = 0x48;
    code[1] = 0xB8; // mov rax, imm64
    memcpy(code + 2, &handler, sizeof(handler));
    code[10] = 0xFF;
    code[11] = 0xE0; // jmp rax
    memcpy(stub, code, sizeof(code));
}

static bool InstallOfficePriorityRowLayoutHooks()
{
    if (!g_officePriorityWrapEnabled) return false;
    InterlockedExchange(&g_officePriorityLayoutActive, 0);
    g_officePriorityPendingRowExtra = 0.0f;
    g_officePriorityPendingRowCenterShift = 0.0f;

    for (int i = 0; i < (int)_countof(RVA_OFFICE_PRIORITY_ROW_CALLS); ++i)
    {
        if (!OfficePriorityDirectCallMatches(
                RVA_OFFICE_PRIORITY_ROW_CALLS[i],
                RVA_OFFICE_PRIORITY_ROW))
        {
            Report("ERROR", "SOVIET64.exe", "office-priority-row-call",
                "Priority row %d call at exe+0x%X no longer targets exe+0x%X; dynamic row layout was not enabled",
                i, RVA_OFFICE_PRIORITY_ROW_CALLS[i],
                RVA_OFFICE_PRIORITY_ROW);
            return false;
        }
        if (!OfficePriorityIndirectCallMatches(
                RVA_OFFICE_PRIORITY_PERCENT_CALLS[i],
                RVA_OFFICE_PRIORITY_DRAW_FORMATTED_TEXT_IAT))
        {
            Report("ERROR", "SOVIET64.exe", "office-priority-percent-call",
                "Priority percentage %d call at exe+0x%X no longer uses IAT exe+0x%X; dynamic row layout was not enabled",
                i, RVA_OFFICE_PRIORITY_PERCENT_CALLS[i],
                RVA_OFFICE_PRIORITY_DRAW_FORMATTED_TEXT_IAT);
            return false;
        }
        BYTE* cursorStore =
            g_exeBase + RVA_OFFICE_PRIORITY_CURSOR_STORES[i];
        if (!ReadablePtr(cursorStore,
                         sizeof(EXPECT_OFFICE_PRIORITY_CURSOR_STORE)) ||
            memcmp(cursorStore, EXPECT_OFFICE_PRIORITY_CURSOR_STORE,
                   sizeof(EXPECT_OFFICE_PRIORITY_CURSOR_STORE)) != 0)
        {
            Report("ERROR", "SOVIET64.exe", "office-priority-cursor-store",
                "Priority row %d cursor store at exe+0x%X no longer matches; dynamic row layout was not enabled",
                i, RVA_OFFICE_PRIORITY_CURSOR_STORES[i]);
            return false;
        }
    }

    BYTE* formattedTextSlot =
        g_exeBase + RVA_OFFICE_PRIORITY_DRAW_FORMATTED_TEXT_IAT;
    if (!ReadablePtr(formattedTextSlot, sizeof(void*)))
    {
        Report("ERROR", "SOVIET64.exe", "office-priority-formatted-text",
            "The verified formatted-text import is unreadable; dynamic row layout was not enabled");
        return false;
    }
    g_originalOfficePriorityRow =
        (NativeOfficePriorityRowFn)(g_exeBase + RVA_OFFICE_PRIORITY_ROW);
    g_originalOfficePriorityDrawFormattedText =
        *(NativeOfficePriorityDrawFormattedTextFn*)formattedTextSlot;
    if (!g_originalOfficePriorityRow ||
        !g_originalOfficePriorityDrawFormattedText)
    {
        Report("ERROR", "SOVIET64.exe", "office-priority-layout-targets",
            "A verified Office Priority layout target is null; dynamic row layout was not enabled");
        return false;
    }

    BYTE* stubs = AllocNear(
        g_exeBase + RVA_OFFICE_PRIORITY_ROW_CALLS[0], 96);
    if (!stubs)
    {
        Report("ERROR", "SOVIET64.exe", "office-priority-layout-stubs",
            "Executable memory for the dynamic Office Priority row layout could not be allocated");
        return false;
    }
    BYTE* rowStub = stubs;
    BYTE* percentStub = stubs + 16;
    BYTE* cursorStub = stubs + 32;
    BuildOfficePriorityJumpStub(
        rowStub, (void*)&HookOfficePriorityRow);
    BuildOfficePriorityJumpStub(
        percentStub, (void*)&HookOfficePriorityDrawFormattedText);

    BYTE cursorCode[22] = {
        0x50,                         // push rax
        0x48,0xB8,                   // mov rax, imm64
        0,0,0,0,0,0,0,0,
        0xF3,0x0F,0x58,0x10,         // addss xmm2, dword ptr [rax]
        0x58,                         // pop rax
        0xF3,0x0F,0x11,0x55,0x80,    // movss dword ptr [rbp-80h],xmm2
        0xC3                          // ret
    };
    void* extraAddress = (void*)&g_officePriorityPendingRowExtra;
    memcpy(cursorCode + 3, &extraAddress, sizeof(extraAddress));
    memcpy(cursorStub, cursorCode, sizeof(cursorCode));
    if (!FlushCodeChecked(stubs, 96, "office-priority-layout-stubs")) return false;

    for (int i = 0; i < (int)_countof(RVA_OFFICE_PRIORITY_ROW_CALLS); ++i)
    {
        if (!PatchOfficePriorityCall(
                RVA_OFFICE_PRIORITY_ROW_CALLS[i], rowStub, 5,
                "office-priority-row-patch") ||
            !PatchOfficePriorityCall(
                RVA_OFFICE_PRIORITY_PERCENT_CALLS[i], percentStub, 6,
                "office-priority-percent-patch") ||
            !PatchOfficePriorityCall(
                RVA_OFFICE_PRIORITY_CURSOR_STORES[i], cursorStub, 5,
                "office-priority-cursor-patch"))
        {
            g_officePriorityPendingRowExtra = 0.0f;
            g_officePriorityPendingRowCenterShift = 0.0f;
            Report("ERROR", "SOVIET64.exe", "office-priority-layout-patch",
                "Dynamic row layout could not be installed completely; all installed bridges remain pass-through");
            return false;
        }
    }

    InterlockedExchange(&g_officePriorityLayoutActive, 1);
    return true;
}

static bool InstallOfficePriorityLabelHook()
{
    if (!g_officePriorityWrapEnabled) return false;

    BYTE* callSite = g_exeBase + RVA_OFFICE_PRIORITY_LABEL_DRAW_CALL;
    if (!ReadablePtr(callSite,
                     sizeof(EXPECT_OFFICE_PRIORITY_LABEL_DRAW_CALL)) ||
        memcmp(callSite, EXPECT_OFFICE_PRIORITY_LABEL_DRAW_CALL,
               sizeof(EXPECT_OFFICE_PRIORITY_LABEL_DRAW_CALL)) != 0)
    {
        Report("ERROR", "SOVIET64.exe", "office-priority-call",
            "The verified priority-label draw call at exe+0x%X no longer matches; wrapping was not installed",
            RVA_OFFICE_PRIORITY_LABEL_DRAW_CALL);
        return false;
    }

    BYTE* iatSlot = g_exeBase + RVA_OFFICE_PRIORITY_DRAW_TEXT_IAT;
    BYTE* localizationSlot =
        g_exeBase + RVA_LOCALIZATION_GET_TEXT_IAT;
    if (!ReadablePtr(iatSlot, sizeof(void*)) ||
        !ReadablePtr(localizationSlot, sizeof(void*)))
    {
        Report("ERROR", "SOVIET64.exe", "office-priority-imports",
            "A verified native text import is unreadable; wrapping was not installed");
        return false;
    }

    g_originalOfficePriorityDrawText =
        *(NativeOfficePriorityDrawTextFn*)iatSlot;
    g_nativeLocalizationGetText =
        *(NativeLocalizationGetTextFn*)localizationSlot;
    if (!g_originalOfficePriorityDrawText || !g_nativeLocalizationGetText)
    {
        Report("ERROR", "SOVIET64.exe", "office-priority-imports",
            "A verified native text import is null; wrapping was not installed");
        return false;
    }

    BYTE* stub = AllocNear(callSite, 16);
    if (!stub)
    {
        Report("ERROR", "SOVIET64.exe", "office-priority-stub",
            "Executable memory could not be allocated near the priority-label call");
        return false;
    }

    void* handler = (void*)&HookOfficePriorityDrawText;
    BYTE stubCode[16] = {};
    memset(stubCode, 0xCC, sizeof(stubCode));
    stubCode[0] = 0x48;
    stubCode[1] = 0xB8; // mov rax, imm64
    memcpy(stubCode + 2, &handler, sizeof(handler));
    stubCode[10] = 0xFF;
    stubCode[11] = 0xE0; // jmp rax
    memcpy(stub, stubCode, sizeof(stubCode));
    if (!FlushCodeChecked(stub, sizeof(stubCode), "office-priority-label-stub")) return false;

    INT64 distance = (INT64)(uintptr_t)stub -
                     (INT64)(uintptr_t)(callSite + 5);
    if (distance < INT_MIN || distance > INT_MAX)
    {
        Report("ERROR", "SOVIET64.exe", "office-priority-stub",
            "The generated priority-label stub is outside rel32 range");
        return false;
    }

    BYTE replacement[6] = { 0xE8,0,0,0,0,0x90 };
    int relative = (int)distance;
    memcpy(replacement + 1, &relative, sizeof(relative));

    DWORD oldProtect = 0;
    if (!MakeCodeWritable(callSite, sizeof(replacement), &oldProtect, "office-priority-patch")) return false;
    memcpy(callSite, replacement, sizeof(replacement));
    FinishCodePatch(callSite, sizeof(replacement), oldProtect, "office-priority-patch");
    InterlockedExchange(&g_officePriorityWrapActive, 1);
    return true;
}

// -----------------------------------------------------------------------------
// Configuration

#include "config_validation.h"





static bool IsValidSpreaderResourceName(const char* name)
{
    if (!name || !name[0] || strlen(name) >=
        sizeof(g_spreaderMaterials[0].resourceName))
        return false;

    for (const unsigned char* cursor =
             (const unsigned char*)name; *cursor; ++cursor)
    {
        if (*cursor < 33 || *cursor > 126 ||
            *cursor == '=' || *cursor == ';' || *cursor == '#' ||
            *cursor == '[' || *cursor == ']')
            return false;
    }
    return true;
}

static void ResetSpreaderMaterialCatalogToSand()
{
    memset(g_spreaderMaterials, 0, sizeof(g_spreaderMaterials));
    strcpy_s(g_spreaderMaterials[0].resourceName,
             sizeof(g_spreaderMaterials[0].resourceName), "sand");
    g_spreaderMaterials[0].protectionStrength = 0.50f;
    g_spreaderMaterials[0].runtimeResourceIndex = -1;
    g_spreaderMaterials[0].runtimeValidationState = 0;
    g_spreaderMaterialCount = 1;
}

static void LoadSpreaderMaterialCatalog()
{
    SpreaderMaterialDefinition candidate[SPREADER_MAX_MATERIALS] = {};
    size_t count = 0;
    g_materialCatalogValid = false;
    if (g_configComplete && !g_configMaterialsPresent) {
        ResetSpreaderMaterialCatalogToSand();
        g_materialCatalogValid = true;
        Report("WARN",INI_NAME,"grit-material-list",
            "[grit_materials] is absent; legacy documented default sand=0.50 is used");
        return;
    }
    // Validate the whole input before publishing. Preserve valid entry order.
    if (g_configComplete) for (const auto& entry : g_configEntries) {
        if (entry.section != CONFIG_MATERIAL_SECTION) continue;
        const char* resourceName = entry.key.c_str();
        if (entry.key.size()+entry.value.size()+1 >= 512 || !IsValidSpreaderResourceName(resourceName)) {
            Report("WARN",INI_NAME,"grit-material-list","line=%llu resource='%s' invalid name or entry exceeds 511 bytes; ignored",(unsigned long long)entry.line,resourceName);
            continue;
        }
        if (IsReservedNonSpreaderResource(resourceName)) {
            Report("WARN",INI_NAME,"grit-material-list","line=%llu resource='%s' is vehicle fuel, never grit; ignored",(unsigned long long)entry.line,resourceName);
            continue;
        }
        bool duplicate = false;
        for (size_t i=0;i<count;++i) if (!_stricmp(candidate[i].resourceName,resourceName)) duplicate = true;
        if (duplicate) {
            Report("WARN",INI_NAME,"grit-material-list","line=%llu resource='%s' duplicated; first valid entry retained",(unsigned long long)entry.line,resourceName);
            continue;
        }
        double strength = 0;
        if (!ParseMaterialStrength(entry.value.c_str(),&strength)) {
            Report("WARN",INI_NAME,"grit-material-list","line=%llu resource='%s' protection='%s' invalid; expected 0.00..1.00 using a dot; ignored",(unsigned long long)entry.line,resourceName,entry.value.c_str());
            continue;
        }
        if (count == SPREADER_MAX_MATERIALS) {
            Report("WARN",INI_NAME,"grit-material-list","line=%llu resource='%s' exceeds the %llu-material runtime limit; this and later entries ignored",(unsigned long long)entry.line,resourceName,(unsigned long long)SPREADER_MAX_MATERIALS);
            break;
        }
        auto& material = candidate[count++];
        strcpy_s(material.resourceName,sizeof(material.resourceName),resourceName);
        material.protectionStrength = (float)strength;
        material.runtimeResourceIndex = -1;
    }
    memcpy(g_spreaderMaterials,candidate,sizeof(candidate));
    g_spreaderMaterialCount = count;
    g_materialCatalogValid = count != 0;
    if (!count) {
        Report("WARN",INI_NAME,"grit-material-list",
            "Material configuration is unreadable/incomplete or explicitly contains no valid materials; no implicit sand substitute; grit operations/migration/sidecar writes disabled for this session, native storages and saved data retained");
        return;
    }
    for (size_t i=0;i<count;++i) Report("INFO",INI_NAME,"grit-material-list",
        "order=%llu resource=%s protection_strength=%.3f runtime_validation=pending",
        (unsigned long long)(i+1),g_spreaderMaterials[i].resourceName,g_spreaderMaterials[i].protectionStrength);
}



static void LoadConfig()
{
    LoadConfigFile();
    g_enabled = ConfigInt("general", "enabled");
    g_storageMigrationEnabled = ConfigInt("storage_migration", "enabled") ? 1 : 0;
    g_debug = ConfigInt("general", "debug");
    g_debugLimit = ConfigInt("general", "debug_limit");
    g_resourceGap = ConfigInt("ui", "resource_gap");
    g_officePriorityWrapEnabled =
        ConfigInt("ui", "office_priority_wrap") ? 1 : 0;
    g_officePriorityMaximumWidth =
        ConfigInt("ui", "office_priority_max_width");
    g_officePriorityLineHeight =
        ConfigInt("ui", "office_priority_line_height");
    g_depotMaterialPriorityEnabled =
        ConfigInt("ui", "material_priority_enabled") ? 1 : 0;
    g_depotMaterialPriorityOffsetX =
        ConfigInt("ui", "material_priority_offset_x");
    g_depotMaterialPriorityOffsetY =
        ConfigInt("ui", "material_priority_offset_y");
    g_depotMaterialPriorityWidth =
        ConfigInt("ui", "material_priority_width");
    g_depotMaterialPriorityHeight =
        ConfigInt("ui", "material_priority_height");
    g_depotMaterialPriorityPersistenceEnabled =
        ConfigInt("priority_persistence", "enabled") ? 1 : 0;
    g_sandTankPersistenceEnabled =
        ConfigInt("tank_persistence", "enabled") ? 1 : 0;
    g_depotMaterialDividerEnabled =
        ConfigInt("ui", "material_section_divider") ? 1 : 0;
    g_depotMaterialDividerHeight =
        ConfigInt("ui", "material_section_divider_height");
    g_depotMaterialWarningsEnabled =
        ConfigInt("ui", "material_status_warnings") ? 1 : 0;
    LoadSpreaderMaterialCatalog();
    LoadSandDiagnosticConfig();
    if (!g_materialCatalogValid) {
        g_sandDiagnosticEnabled = 0;
        g_sandShadowTankEnabled = 0;
        g_storageMigrationEnabled = 0;
        g_depotMaterialPriorityPersistenceEnabled = 0;
        g_sandTankPersistenceEnabled = 0;
    }

    if (g_debugLimit < 0)
    {
        Report("WARN", INI_NAME, "debug-limit",
            "debug_limit=%d is invalid and was clamped to 0. "
            "Action: use a value from 0 to 10000", g_debugLimit);
        g_debugLimit = 0;
    }
    else if (g_debugLimit > 10000)
    {
        Report("WARN", INI_NAME, "debug-limit",
            "debug_limit=%d exceeds the safety limit and was clamped to 10000. "
            "Action: use a value from 0 to 10000", g_debugLimit);
        g_debugLimit = 10000;
    }

    if (g_resourceGap < 0)
    {
        Report("WARN", INI_NAME, "resource-gap",
            "resource_gap=%d is invalid and was clamped to 0. "
            "Action: use a value from 0 to 50", g_resourceGap);
        g_resourceGap = 0;
    }
    else if (g_resourceGap > 50)
    {
        Report("WARN", INI_NAME, "resource-gap",
            "resource_gap=%d exceeds the UI safety limit and was clamped to 50. "
            "Action: use a value from 0 to 50", g_resourceGap);
        g_resourceGap = 50;
    }

    if (g_officePriorityMaximumWidth < 120)
    {
        Report("WARN", INI_NAME, "office-priority-width",
            "office_priority_max_width=%d is too narrow and was clamped to 120",
            g_officePriorityMaximumWidth);
        g_officePriorityMaximumWidth = 120;
    }
    else if (g_officePriorityMaximumWidth > 400)
    {
        Report("WARN", INI_NAME, "office-priority-width",
            "office_priority_max_width=%d is too wide and was clamped to 400",
            g_officePriorityMaximumWidth);
        g_officePriorityMaximumWidth = 400;
    }

    if (g_officePriorityLineHeight < 12)
    {
        Report("WARN", INI_NAME, "office-priority-line-height",
            "office_priority_line_height=%d is too small and was clamped to 12",
            g_officePriorityLineHeight);
        g_officePriorityLineHeight = 12;
    }
    else if (g_officePriorityLineHeight > 28)
    {
        Report("WARN", INI_NAME, "office-priority-line-height",
            "office_priority_line_height=%d is too large and was clamped to 28",
            g_officePriorityLineHeight);
        g_officePriorityLineHeight = 28;
    }

    if (g_depotMaterialPriorityOffsetX < 100)
        g_depotMaterialPriorityOffsetX = 100;
    else if (g_depotMaterialPriorityOffsetX > 800)
        g_depotMaterialPriorityOffsetX = 800;
    if (g_depotMaterialPriorityOffsetY < -50)
        g_depotMaterialPriorityOffsetY = -50;
    else if (g_depotMaterialPriorityOffsetY > 100)
        g_depotMaterialPriorityOffsetY = 100;
    if (g_depotMaterialPriorityWidth < 60)
        g_depotMaterialPriorityWidth = 60;
    else if (g_depotMaterialPriorityWidth > 300)
        g_depotMaterialPriorityWidth = 300;
    if (g_depotMaterialPriorityHeight < 12)
        g_depotMaterialPriorityHeight = 12;
    else if (g_depotMaterialPriorityHeight > 60)
        g_depotMaterialPriorityHeight = 60;
    if (g_depotMaterialDividerHeight < 48)
    {
        Report("WARN", INI_NAME, "material-divider-height",
            "material_section_divider_height=%d is too small and was clamped to 48; a smaller value can overlap the native first material title",
            g_depotMaterialDividerHeight);
        g_depotMaterialDividerHeight = 48;
    }
    else if (g_depotMaterialDividerHeight > 100)
    {
        Report("WARN", INI_NAME, "material-divider-height",
            "material_section_divider_height=%d exceeds the UI safety limit and was clamped to 100",
            g_depotMaterialDividerHeight);
        g_depotMaterialDividerHeight = 100;
    }
}

// -----------------------------------------------------------------------------
// Game-build validation

static DWORD ExeTimestamp()
{
    if (!g_exeBase || !ReadablePtr(g_exeBase, sizeof(IMAGE_DOS_HEADER))) return 0;

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)g_exeBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    BYTE* ntp = g_exeBase + dos->e_lfanew;
    if (!ReadablePtr(ntp, sizeof(IMAGE_NT_HEADERS64))) return 0;

    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)ntp;
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    return nt->FileHeader.TimeDateStamp;
}

static bool VerifyBytes(DWORD rva, const BYTE* expected, size_t size,
                        const char* rule, const char* label)
{
    if (!expected || !size || rva > g_exeSize || size > g_exeSize - rva ||
        !ReadablePtr(g_exeBase + rva, size))
    {
        Report("FATAL", "SOVIET64.exe", rule,
            "%s at exe+0x%X is outside the readable image. "
            "Action: verify the supported game version", label, rva);
        return false;
    }

    if (memcmp(g_exeBase + rva, expected, size) != 0)
    {
        Report("FATAL", "SOVIET64.exe", rule,
            "%s signature changed at exe+0x%X. "
            "Action: verify the supported game version and plugin compatibility",
            label, rva);
        return false;
    }

    return true;
}

static bool VerifyRelativeCall(DWORD callRva, DWORD expectedTargetRva,
                               const char* rule, const char* label)
{
    if (callRva > g_exeSize || 5 > g_exeSize - callRva ||
        !ReadablePtr(g_exeBase + callRva, 5))
    {
        Report("FATAL", "SOVIET64.exe", rule,
            "%s call site exe+0x%X is unreadable. "
            "Action: verify the supported game version", label, callRva);
        return false;
    }

    BYTE* p = g_exeBase + callRva;
    if (p[0] != 0xE8)
    {
        Report("FATAL", "SOVIET64.exe", rule,
            "%s is no longer a direct call at exe+0x%X. "
            "Action: verify the supported game version", label, callRva);
        return false;
    }

    LONG displacement = *(LONG*)(p + 1);
    BYTE* target = p + 5 + displacement;
    BYTE* expected = g_exeBase + expectedTargetRva;
    if (target != expected)
    {
        Report("FATAL", "SOVIET64.exe", rule,
            "%s now targets %p instead of expected %p (exe+0x%X). "
            "Action: verify the supported game version",
            label, target, expected, expectedTargetRva);
        return false;
    }

    return true;
}

static bool BuildMatches()
{
    if (g_exeSize != EXPECTED_IMAGE_SIZE)
    {
        Report("FATAL", "SOVIET64.exe", "unsupported-build",
            "Image size 0x%llX does not match expected 0x%llX. "
            "Action: use the supported WRSR 1.1.1.9 build",
            (unsigned long long)g_exeSize,
            (unsigned long long)EXPECTED_IMAGE_SIZE);
        return false;
    }

    DWORD timestamp = ExeTimestamp();
    if (timestamp != EXPECTED_TIMESTAMP)
    {
        Report("FATAL", "SOVIET64.exe", "unsupported-build",
            "Timestamp 0x%08X does not match expected 0x%08X. "
            "Action: use the supported WRSR 1.1.1.9 build",
            timestamp, EXPECTED_TIMESTAMP);
        return false;
    }

    if (!VerifyBytes(RVA_TECHNICAL_SERVICES_PANEL,
                     EXPECT_TECH_PANEL, sizeof(EXPECT_TECH_PANEL),
                     "technical-panel-prologue", "Technical Services panel"))
        return false;

    if (!VerifyBytes(RVA_NATIVE_STORAGE_PANEL,
                     EXPECT_NATIVE_STORAGE_PANEL, sizeof(EXPECT_NATIVE_STORAGE_PANEL),
                     "storage-renderer-prologue", "Native storage renderer"))
        return false;

    if (!VerifyBytes(
            RVA_STORAGE_DISCARD_VISIBILITY_GATE,
            EXPECT_STORAGE_DISCARD_VISIBILITY_GATE,
            sizeof(EXPECT_STORAGE_DISCARD_VISIBILITY_GATE),
            "storage-discard-visibility-gate",
            "Native per-resource discard visibility gate"))
        return false;

    if (!VerifyBytes(RVA_POST_STORAGE_SECTION,
                     EXPECT_POST_STORAGE_SECTION, sizeof(EXPECT_POST_STORAGE_SECTION),
                     "post-storage-prologue", "Post-storage section helper"))
        return false;

    if (!VerifyBytes(RVA_VEHICLE_UI_EMPTY_WEIGHT_READ,
                     EXPECT_VEHICLE_UI_EMPTY_WEIGHT_READ,
                     sizeof(EXPECT_VEHICLE_UI_EMPTY_WEIGHT_READ),
                     "vehicle-empty-weight-reader",
                     "Vehicle UI empty-weight reader"))
        return false;

    if (!VerifyBytes(RVA_VEHICLE_UI_ENGINE_POWER_READ,
                     EXPECT_VEHICLE_UI_ENGINE_POWER_READ,
                     sizeof(EXPECT_VEHICLE_UI_ENGINE_POWER_READ),
                     "vehicle-engine-power-reader",
                     "Vehicle UI engine-power reader"))
        return false;

    if (g_officePriorityWrapEnabled &&
        !VerifyBytes(RVA_OFFICE_PRIORITY_LABEL_DRAW_CALL,
                     EXPECT_OFFICE_PRIORITY_LABEL_DRAW_CALL,
                     sizeof(EXPECT_OFFICE_PRIORITY_LABEL_DRAW_CALL),
                     "office-priority-label-call",
                     "Office Priority native label draw call"))
        return false;

    if (g_depotMaterialWarningsEnabled &&
        !VerifyBytes(RVA_TECH_WARNING_APPEND_TRANSITION,
                     EXPECT_TECH_WARNING_APPEND_TRANSITION,
                     sizeof(EXPECT_TECH_WARNING_APPEND_TRANSITION),
                     "material-warning-native-buffer-transition",
                     "Technical Services native warning pre-measurement transition"))
        return false;

    if (!VerifyRelativeCall(RVA_TECH_NATIVE_STORAGE_CALL,
                            RVA_NATIVE_STORAGE_PANEL,
                            "native-storage-callsite",
                            "Technical Services native storage call"))
        return false;

    if (!VerifyRelativeCall(RVA_TECH_POST_STORAGE_CALL,
                            RVA_POST_STORAGE_SECTION,
                            "post-storage-callsite",
                            "Technical Services post-storage call"))
        return false;

    Info("Supported WRSR build verified: timestamp=0x%08X image_size=0x%llX",
         timestamp, (unsigned long long)g_exeSize);
    Info("Verified UI path: panel=exe+0x%X renderer=exe+0x%X insertion=exe+0x%X",
         RVA_TECHNICAL_SERVICES_PANEL,
         RVA_NATIVE_STORAGE_PANEL,
         RVA_POST_STORAGE_SECTION);
    Info("Verified call sites: native_storage=exe+0x%X post_storage=exe+0x%X",
         RVA_TECH_NATIVE_STORAGE_CALL,
         RVA_TECH_POST_STORAGE_CALL);
    Info("Verified Office Priority label draw call: exe+0x%X -> IAT exe+0x%X",
         RVA_OFFICE_PRIORITY_LABEL_DRAW_CALL,
         RVA_OFFICE_PRIORITY_DRAW_TEXT_IAT);
    Info("Verified vehicle UI field readers: empty_weight=exe+0x%X type+0x867C tonnes (text ID 1950), engine_power=exe+0x%X type+0x8678 kW (text ID 1951)",
         RVA_VEHICLE_UI_EMPTY_WEIGHT_READ,
         RVA_VEHICLE_UI_ENGINE_POWER_READ);
    return true;
}

// -----------------------------------------------------------------------------
// Runtime helpers

static bool IsFiniteNonNegative(float value)
{
    return value == value && value >= 0.0f && value < 1000000000.0f;
}

static float ReadExeFloat(DWORD rva, float fallback)
{
    if (!g_exeBase || rva > g_exeSize || sizeof(float) > g_exeSize - rva ||
        !ReadablePtr(g_exeBase + rva, sizeof(float)))
        return fallback;

    float value = *(float*)(g_exeBase + rva);
    return (value == value && value > -1000000.0f && value < 1000000.0f)
         ? value : fallback;
}

static int ReadTechnicalType(BYTE* building)
{
    if (!building || !ReadablePtr(building + B_TYPEDESC, sizeof(void*))) return -1;

    BYTE* typeDesc = *(BYTE**)(building + B_TYPEDESC);
    if (!typeDesc || !ReadablePtr(typeDesc + TD_TYPE, sizeof(int))) return -1;

    return *(int*)(typeDesc + TD_TYPE);
}

static BYTE* WindowBuilding(void* window)
{
    BYTE* win = (BYTE*)window;
    if (!win || !ReadablePtr(win + W_BUILDING, sizeof(void*))) return nullptr;
    return *(BYTE**)(win + W_BUILDING);
}

static bool IsVisibleTechnicalWindow(void* window, BYTE** buildingOut)
{
    if (buildingOut) *buildingOut = nullptr;

    BYTE* win = (BYTE*)window;
    if (!win || !ReadablePtr(win + W_HIDDEN, 1) || *(BYTE*)(win + W_HIDDEN) != 0)
        return false;

    BYTE* building = WindowBuilding(window);
    if (!building || ReadTechnicalType(building) != BUILDING_GARBAGE_OFFICE)
        return false;

    if (buildingOut) *buildingOut = building;
    return true;
}

static size_t DirectCallerRva(void* returnAddress)
{
    BYTE* p = (BYTE*)returnAddress;
    if (!g_exeBase || !p || p < g_exeBase || p >= g_exeBase + g_exeSize)
        return (size_t)-1;

    size_t offset = (size_t)(p - g_exeBase);
    if (offset >= 5 && p[-5] == 0xE8) return offset - 5;
    return offset;
}

static bool GetStorageSlots(BYTE* storage, BYTE** beginOut, size_t* countOut)
{
    if (beginOut) *beginOut = nullptr;
    if (countOut) *countOut = 0;

    if (!storage || !ReadablePtr(storage, STORAGE_SIZE)) return false;

    BYTE* begin = *(BYTE**)(storage + STORAGE_SLOT_BEGIN);
    BYTE* end = *(BYTE**)(storage + STORAGE_SLOT_END);
    if (!begin || !end || end < begin) return false;

    size_t bytes = (size_t)(end - begin);
    if ((bytes % SLOT_SIZE) != 0) return false;

    size_t count = bytes / SLOT_SIZE;
    if (count == 0 || count > MAX_SLOTS_PER_STORAGE) return false;
    if (!ReadablePtr(begin, bytes)) return false;

    if (beginOut) *beginOut = begin;
    if (countOut) *countOut = count;
    return true;
}

static bool IsSupportedTransportClass(int transportClass)
{
    return transportClass == RESOURCE_TRANSPORT_COVERED ||
           transportClass == RESOURCE_TRANSPORT_OPEN ||
           transportClass == RESOURCE_TRANSPORT_GRAVEL ||
           transportClass == RESOURCE_TRANSPORT_OIL;
}

static const char* TransportClassName(int transportClass)
{
    switch (transportClass)
    {
        case RESOURCE_TRANSPORT_COVERED: return "covered";
        case RESOURCE_TRANSPORT_OPEN:    return "open";
        case RESOURCE_TRANSPORT_GRAVEL:  return "gravel";
        case RESOURCE_TRANSPORT_OIL:     return "oil";
        default:                         return "unsupported";
    }
}

static void ResourceName(void* resource, char* out, size_t outSize)
{
    if (!out || outSize == 0) return;
    out[0] = 0;

    if (!resource || !SafeReadStr((BYTE*)resource + RESOURCE_NAME, out, outSize))
        strcpy_s(out, outSize, "<unavailable>");
}

// The treatment exists only while this thread is inside the exact native
// road-clear call that consumed it. weather_roads hooks the guarded body of
// that same call and reads the value synchronously through the service below.
// Clearing the TLS value in a __finally block prevents a later vehicle or road
// from inheriting stale material state.
static __declspec(thread) TsmGritRoadTreatment
    g_spreaderPublishedRoadTreatment = {};
static volatile LONG g_spreaderTreatmentSequence = 0;

static void SpreaderPublishRoadTreatment(
    const TsmGritRoadTreatment* treatment)
{
    memset(&g_spreaderPublishedRoadTreatment, 0,
           sizeof(g_spreaderPublishedRoadTreatment));
    if (!treatment) return;
    g_spreaderPublishedRoadTreatment = *treatment;
    g_spreaderPublishedRoadTreatment.structSize =
        sizeof(g_spreaderPublishedRoadTreatment);
    g_spreaderPublishedRoadTreatment.sequence = (unsigned)
        InterlockedIncrement(&g_spreaderTreatmentSequence);
}

static void SpreaderClearPublishedRoadTreatment()
{
    memset(&g_spreaderPublishedRoadTreatment, 0,
           sizeof(g_spreaderPublishedRoadTreatment));
}

static int SpreaderCurrentRoadTreatment(
    void* vehicle, void* road, void* selector,
    TsmGritRoadTreatment* out, unsigned outSize)
{
    if (!out || outSize < sizeof(TsmGritRoadTreatment)) return 0;
    const TsmGritRoadTreatment& treatment =
        g_spreaderPublishedRoadTreatment;
    if (treatment.structSize != sizeof(TsmGritRoadTreatment) ||
        !treatment.sequence || treatment.vehicle != vehicle ||
        treatment.road != road || treatment.selector != selector)
        return 0;
    *out = treatment;
    return 1;
}

static const TsmGritSpreaderApi g_spreaderApi = {
    sizeof(TsmGritSpreaderApi),
    &SpreaderCurrentRoadTreatment
};

#include "sand_spreader_diagnostic.h"
#include "storage_migration.h"

// -----------------------------------------------------------------------------
// Native vehicle-information tank display

static void ResolveVehicleTankLocalization()
{
    struct TextKey { const char* key; int* id; };
    const TextKey keys[] = {
        {"technical_service_storage.vehicle_tank.grit",&g_vehicleTankLabelTextId},
        {"technical_service_storage.vehicle_tank.none",&g_vehicleTankNoMaterialTextId},
        {"technical_service_storage.vehicle_tank.dry_plowing",&g_vehicleTankDryPlowingTextId},
        {"technical_service_storage.vehicle_tank.depot",&g_vehicleTankDepotTextId},
        {"technical_service_storage.material_priority.off",&g_depotPriorityOffTextId},
        {"technical_service_storage.building_warning.resource",&g_depotWarningResourceTextId},
        {"technical_service_storage.building_warning.empty",&g_depotWarningEmptyTextId},
        {"technical_service_storage.building_warning.off",&g_depotWarningOffTextId}
    };
    for (const auto& key : keys) *key.id = 0;
    g_localization = nullptr;
    if (H && H->structSize >= offsetof(TsmHost,consume)+sizeof(H->consume) && H->consume)
        g_localization = (const TsmLocalizationApi*)H->consume(
            TSM_SERVICE_LOCALIZATION,TSM_LOCALIZATION_VERSION);
    if (!g_localization || !g_localization->resolveFull) {
        Report("WARN","Localization","vehicle-tank-text",
            "Localization service/resolveFull unavailable; built-in labels retained, gameplay remains active");
        return;
    }
    for (const auto& key : keys) {
        int resolved = g_localization->resolveFull(key.key);
        // Only plugin-owned keys use this reserved range. Native resource and
        // priority text IDs must continue to bypass this validation.
        if (resolved < 2000000 || resolved > 2999999)
            Report("WARN","Localization","vehicle-tank-key",
                "key='%s' resolved_id=%d missing/outside 2000000..2999999; built-in fallback retained",key.key,resolved);
        else *key.id = resolved;
    }
    Info("Localization resolved; native priority/resource IDs unchanged; missing plugin keys use per-key fallbacks");
}

static const wchar_t* VehicleTankLocalizedText(
    int textId, const wchar_t* fallback)
{
    if (textId > 0 && g_nativeLocalizationGetText && g_exeBase)
    {
        const wchar_t* text = g_nativeLocalizationGetText(
            g_exeBase + RVA_LOCALIZATION_CONTEXT, textId);
        if (text && text[0]) return text;
    }
    return fallback;
}

static const wchar_t* VehicleTankMaterialText(
    int resourceTextId,
    const char* resourceName,
    wchar_t* fallbackBuffer,
    size_t fallbackCapacity)
{
    if (resourceTextId > 0 && g_nativeLocalizationGetText && g_exeBase)
    {
        const wchar_t* text = g_nativeLocalizationGetText(
            g_exeBase + RVA_LOCALIZATION_CONTEXT, resourceTextId);
        if (text && text[0]) return text;
    }

    if (fallbackBuffer && fallbackCapacity &&
        resourceName && resourceName[0])
    {
        fallbackBuffer[0] = 0;
        int converted = MultiByteToWideChar(
            CP_UTF8, 0, resourceName, -1,
            fallbackBuffer, (int)fallbackCapacity);
        if (converted > 0)
        {
            if (fallbackBuffer[0] >= L'a' && fallbackBuffer[0] <= L'z')
                fallbackBuffer[0] -= L'a' - L'A';
            return fallbackBuffer;
        }
    }

    return VehicleTankLocalizedText(
        g_vehicleTankNoMaterialTextId, L"Kein Streugut");
}

// -----------------------------------------------------------------------------
// Native Technical Services building-warning extension

struct DepotMaterialWarningObservation
{
    int present;
    int resourceTextId;
    char resourceName[64];
    float totalAmount;
};

static int ObserveDepotMaterialWarnings(
    BYTE* building,
    DepotMaterialWarningObservation
        observations[SPREADER_MAX_MATERIALS],
    int priorities[SPREADER_MAX_MATERIALS])
{
    if (observations)
        memset(observations, 0,
               sizeof(DepotMaterialWarningObservation) *
                   SPREADER_MAX_MATERIALS);
    if (priorities)
        memset(priorities, 0,
               sizeof(int) * SPREADER_MAX_MATERIALS);
    if (!building || !observations || !priorities ||
        !ReadablePtr(building + B_STORAGE_BEGIN, 16))
        return 0;

    BYTE* begin = *(BYTE**)(building + B_STORAGE_BEGIN);
    BYTE* end = *(BYTE**)(building + B_STORAGE_END);
    if (!begin || !end || end < begin) return 0;
    size_t bytes = (size_t)(end - begin);
    if ((bytes % STORAGE_SIZE) != 0) return 0;
    size_t storageCount = bytes / STORAGE_SIZE;
    if (storageCount > MAX_STORAGES ||
        (bytes && !ReadablePtr(begin, bytes)))
        return 0;

    DWORD presentMask = 0;
    for (size_t storageIndex = 0;
         storageIndex < storageCount; ++storageIndex)
    {
        BYTE* storage = begin + storageIndex * STORAGE_SIZE;
        BYTE* slots = nullptr;
        size_t slotCount = 0;
        if (!GetStorageSlots(storage, &slots, &slotCount)) continue;
        for (size_t slotIndex = 0;
             slotIndex < slotCount; ++slotIndex)
        {
            BYTE* slot = slots + slotIndex * SLOT_SIZE;
            void* resource = *(void**)(slot + SLOT_RESOURCE);
            char resourceName[64] = {};
            ResourceName(resource, resourceName,
                         sizeof(resourceName));
            int materialIndex =
                FindSpreaderMaterialIndex(resourceName);
            if (materialIndex < 0 ||
                materialIndex >= (int)SPREADER_MAX_MATERIALS)
                continue;

            float amount = *(float*)(slot + SLOT_AMOUNT);
            if (!IsFiniteNonNegative(amount)) continue;

            DepotMaterialWarningObservation* item =
                &observations[materialIndex];
            item->present = 1;
            if (!item->resourceName[0])
            {
                strncpy_s(item->resourceName,
                          sizeof(item->resourceName),
                          resourceName, _TRUNCATE);
                if (resource &&
                    ReadablePtr((BYTE*)resource + RESOURCE_TEXT_ID,
                                sizeof(int)))
                    item->resourceTextId =
                        *(int*)((BYTE*)resource + RESOURCE_TEXT_ID);
            }
            double combined =
                (double)item->totalAmount + (double)amount;
            item->totalAmount =
                combined <= (double)FLT_MAX
                    ? (float)combined : FLT_MAX;
            presentMask |=
                (DWORD)(1u << (unsigned)materialIndex);
        }
    }

    int availableCount = 0;
    SpreaderGetDepotPrioritySnapshot(
        building, presentMask, priorities, &availableCount);
    return availableCount;
}

static bool AppendNativeWarningLine(
    wchar_t* buffer, size_t capacity, const wchar_t* message)
{
    if (!buffer || capacity < 4 || !message || !message[0])
        return false;

    size_t used = wcsnlen_s(buffer, capacity);
    size_t messageLength = wcsnlen_s(message, 512);
    if (used >= capacity || messageLength == 0 || messageLength >= 512)
        return false;

    bool needsLeadingLineBreak =
        used > 0 && buffer[used - 1] != L'\n';
    size_t leadingLength = needsLeadingLineBreak ? 2 : 0;
    size_t required = used + leadingLength + messageLength + 2 + 1;
    if (required > capacity) return false;

    wchar_t* destination = buffer + used;
    if (needsLeadingLineBreak)
    {
        *destination++ = L'\r';
        *destination++ = L'\n';
    }
    memcpy(destination, message, messageLength * sizeof(wchar_t));
    destination += messageLength;
    *destination++ = L'\r';
    *destination++ = L'\n';
    *destination = 0;
    return true;
}

static int AppendDepotMaterialWarningsToNativeBuffer(
    BYTE* building, wchar_t* nativeBuffer, size_t nativeCapacity)
{
    if (!building || !nativeBuffer || nativeCapacity == 0)
        return 0;

    DepotMaterialWarningObservation
        observations[SPREADER_MAX_MATERIALS] = {};
    int priorities[SPREADER_MAX_MATERIALS] = {};
    int availableCount = ObserveDepotMaterialWarnings(
        building, observations, priorities);
    if (availableCount <= 0) return 0;

    const wchar_t* resourceWord = VehicleTankLocalizedText(
        g_depotWarningResourceTextId, L"Ressource");
    const wchar_t* emptySuffix = VehicleTankLocalizedText(
        g_depotWarningEmptyTextId, L"ist leer.");
    const wchar_t* offSuffix = VehicleTankLocalizedText(
        g_depotWarningOffTextId, L"ist AUS.");
    int lineCount = 0;
    int emptyCount = 0;
    int offCount = 0;

    for (size_t materialIndex = 0;
         materialIndex < g_spreaderMaterialCount &&
         materialIndex < SPREADER_MAX_MATERIALS;
         ++materialIndex)
    {
        DepotMaterialWarningObservation* item =
            &observations[materialIndex];
        if (!item->present) continue;

        const wchar_t* suffix = nullptr;
        bool isOff = priorities[materialIndex] <= 0;
        if (isOff)
            suffix = offSuffix;
        else if (item->totalAmount <= 0.000001f)
            suffix = emptySuffix;
        else
            continue;

        wchar_t materialFallback[64] = {};
        const wchar_t* materialText = VehicleTankMaterialText(
            item->resourceTextId,
            item->resourceName,
            materialFallback,
            _countof(materialFallback));
        wchar_t message[512] = {};
        _snwprintf_s(message, _countof(message), _TRUNCATE,
                     L"-%ls %ls %ls",
                     resourceWord, materialText, suffix);

        if (!AppendNativeWarningLine(
                nativeBuffer, nativeCapacity, message))
        {
            if (InterlockedCompareExchange(
                    &g_depotMaterialWarningCapacityLogged, 1, 0) == 0)
            {
                Report("WARN", "Technical Services",
                    "material-status-warning-capacity",
                    "The native 1024-character building-warning buffer had no room for every material warning; only complete lines that fit were appended");
            }
            break;
        }

        ++lineCount;
        if (isOff) ++offCount;
        else ++emptyCount;
    }

    if (lineCount > 0 &&
        InterlockedCompareExchange(
            &g_depotMaterialWarningFirstDrawLogged, 1, 0) == 0)
    {
        Report("INFO", "Technical Services",
            "material-status-warnings",
            "active=1 building=%p appended_to_native_buffer=1 lines=%d empty=%d off=%d present_materials=%d fuel_selectable=0 native_transition=exe+0x%X native_font_measurement_and_layout=1",
            building, lineCount, emptyCount, offCount,
            availableCount,
            RVA_TECH_WARNING_APPEND_TRANSITION);
    }
    return lineCount;
}

__declspec(noinline)
static BYTE __fastcall HookTechnicalServicesNativeWarnings(
    void* panelFrame)
{
    if (!g_runtimeActive || !g_insideTechnicalPanel ||
        !g_currentBuilding || !panelFrame ||
        !InterlockedCompareExchange(
            &g_depotMaterialWarningUiActive, 0, 0))
        return 0;

    BYTE appended = 0;
    __try
    {
        wchar_t* nativeBuffer =
            (wchar_t*)((BYTE*)panelFrame + 0x250);
        if (ReadablePtr(
                nativeBuffer,
                TECH_WARNING_BUFFER_CAPACITY * sizeof(wchar_t)))
        {
            appended =
                AppendDepotMaterialWarningsToNativeBuffer(
                    g_currentBuilding,
                    nativeBuffer,
                    TECH_WARNING_BUFFER_CAPACITY) > 0
                ? 1 : 0;
        }
    }
    __except(FaultFilter(
        "technical_service_storage native material warning append",
        GetExceptionInformation()))
    {
        if (InterlockedCompareExchange(
                &g_depotMaterialWarningFaultLogged, 1, 0) == 0)
        {
            Report("WARN", "Technical Services",
                "material-status-warnings",
                "Appending a depot material warning to the native warning buffer faulted and this extension was disabled for the session; the vanilla warning list remains active");
        }
        InterlockedExchange(
            &g_depotMaterialWarningUiActive, 0);
        appended = 0;
    }
    return appended;
}

static bool BuildTechnicalServicesWarningAppendStub(
    BYTE* stub, size_t capacity, void* handler,
    const void* xmm9Source, const void* xmm10Source,
    const void* continuation)
{
    if (!stub || capacity < 192 || !handler ||
        !xmm9Source || !xmm10Source || !continuation)
        return false;

    memset(stub, 0xCC, capacity);
    BYTE* p = stub;

    *p++ = 0x50;                         // push rax
    *p++ = 0x51;                         // push rcx
    *p++ = 0x52;                         // push rdx
    *p++ = 0x41; *p++ = 0x50;           // push r8
    *p++ = 0x41; *p++ = 0x51;           // push r9
    *p++ = 0x41; *p++ = 0x52;           // push r10
    *p++ = 0x41; *p++ = 0x53;           // push r11
    *p++ = 0x48; *p++ = 0x81; *p++ = 0xEC;
    *p++ = 0x88; *p++ = 0x00;
    *p++ = 0x00; *p++ = 0x00;           // sub rsp,88h

    for (int index = 0; index < 6; ++index)
    {
        *p++ = 0xF3; *p++ = 0x0F; *p++ = 0x7F;
        *p++ = (BYTE)(0x44 + index * 8);
        *p++ = 0x24;
        *p++ = (BYTE)(0x20 + index * 0x10);
    }

    *p++ = 0x48; *p++ = 0x8B; *p++ = 0xCD; // mov rcx,rbp
    *p++ = 0x48; *p++ = 0xB8;              // mov rax,handler
    memcpy(p, &handler, sizeof(handler)); p += sizeof(handler);
    *p++ = 0xFF; *p++ = 0xD0;              // call rax
    *p++ = 0x84; *p++ = 0xC0;              // test al,al
    *p++ = 0x74; *p++ = 0x03;              // je +3
    *p++ = 0x41; *p++ = 0xB4; *p++ = 0x01; // mov r12b,1

    for (int index = 0; index < 6; ++index)
    {
        *p++ = 0xF3; *p++ = 0x0F; *p++ = 0x6F;
        *p++ = (BYTE)(0x44 + index * 8);
        *p++ = 0x24;
        *p++ = (BYTE)(0x20 + index * 0x10);
    }

    *p++ = 0x48; *p++ = 0x81; *p++ = 0xC4;
    *p++ = 0x88; *p++ = 0x00;
    *p++ = 0x00; *p++ = 0x00;           // add rsp,88h
    *p++ = 0x41; *p++ = 0x5B;           // pop r11
    *p++ = 0x41; *p++ = 0x5A;           // pop r10
    *p++ = 0x41; *p++ = 0x59;           // pop r9
    *p++ = 0x41; *p++ = 0x58;           // pop r8
    *p++ = 0x5A;                         // pop rdx
    *p++ = 0x59;                         // pop rcx
    *p++ = 0x58;                         // pop rax

    *p++ = 0x41; *p++ = 0x0F;
    *p++ = 0x28; *p++ = 0xF0;           // movaps xmm6,xmm8
    *p++ = 0x48; *p++ = 0xB8;
    memcpy(p, &xmm9Source, sizeof(xmm9Source)); p += sizeof(xmm9Source);
    *p++ = 0xF3; *p++ = 0x44; *p++ = 0x0F;
    *p++ = 0x10; *p++ = 0x08;           // movss xmm9,[rax]
    *p++ = 0x48; *p++ = 0xB8;
    memcpy(p, &xmm10Source, sizeof(xmm10Source)); p += sizeof(xmm10Source);
    *p++ = 0xF3; *p++ = 0x44; *p++ = 0x0F;
    *p++ = 0x10; *p++ = 0x10;           // movss xmm10,[rax]
    *p++ = 0x48; *p++ = 0xB8;
    memcpy(p, &continuation, sizeof(continuation)); p += sizeof(continuation);
    *p++ = 0xFF; *p++ = 0xE0;           // jmp rax

    if ((size_t)(p - stub) > capacity) return false;
    if (!FlushCodeChecked(stub, capacity, "warning-append-stub")) return false;
    return true;
}

static bool PatchTechnicalServicesWarningTransition(BYTE* stub)
{
    if (!stub) return false;
    BYTE* patchSite =
        g_exeBase + RVA_TECH_WARNING_APPEND_TRANSITION;
    INT64 distance = (INT64)(uintptr_t)stub -
                     (INT64)(uintptr_t)(patchSite + 5);
    if (distance < INT_MIN || distance > INT_MAX)
        return false;

    BYTE replacement[TECH_WARNING_APPEND_OVERWRITE_LENGTH] = {};
    memset(replacement, 0x90, sizeof(replacement));
    replacement[0] = 0xE9;
    int relative = (int)distance;
    memcpy(replacement + 1, &relative, sizeof(relative));

    DWORD oldProtect = 0;
    if (!MakeCodeWritable(patchSite, sizeof(replacement), &oldProtect, "material-status-warning-transition-patch")) return false;
    memcpy(patchSite, replacement, sizeof(replacement));
    FinishCodePatch(patchSite, sizeof(replacement), oldProtect, "material-status-warning-transition-patch");
    return true;
}

static bool InstallTechnicalServicesNativeDiscardGate()
{
    BYTE* patchSite =
        g_exeBase + RVA_STORAGE_DISCARD_VISIBILITY_GATE;
    if (!ReadablePtr(
            patchSite,
            sizeof(EXPECT_STORAGE_DISCARD_VISIBILITY_GATE)) ||
        memcmp(patchSite,
               EXPECT_STORAGE_DISCARD_VISIBILITY_GATE,
               sizeof(EXPECT_STORAGE_DISCARD_VISIBILITY_GATE)) != 0)
    {
        Report("WARN", "Technical Services",
            "native-resource-discard-visibility",
            "The verified per-resource discard visibility gate at exe+0x%X no longer matches; no replacement controls will be drawn",
            RVA_STORAGE_DISCARD_VISIBILITY_GATE);
        return false;
    }

    BYTE* stub = AllocNear(patchSite, 64);
    if (!stub)
    {
        Report("WARN", "Technical Services",
            "native-resource-discard-visibility",
            "Executable memory near the native per-resource discard visibility gate could not be allocated; no replacement controls will be drawn");
        return false;
    }

    BYTE* p = stub;
    uintptr_t technicalPanelFlag =
        (uintptr_t)&g_insideTechnicalPanel;
    *p++ = 0x48; *p++ = 0xB8;             // mov rax,imm64
    memcpy(p, &technicalPanelFlag,
           sizeof(technicalPanelFlag)); p += sizeof(technicalPanelFlag);
    *p++ = 0x83; *p++ = 0x38; *p++ = 0x00; // cmp dword ptr [rax],0
    *p++ = 0x75; *p++ = 0x0B;             // jne force_fallthrough
    *p++ = 0x80; *p++ = 0x7D;
    *p++ = 0xC0; *p++ = 0x00;             // cmp byte ptr [rbp-40h],0
    *p++ = 0x75; *p++ = 0x05;             // jne force_fallthrough

    BYTE* skipJump = p;
    *p++ = 0xE9;
    INT64 skipDistance =
        (INT64)(uintptr_t)(g_exeBase +
            RVA_STORAGE_DISCARD_VISIBILITY_SKIP) -
        (INT64)(uintptr_t)(skipJump + 5);
    if (skipDistance < INT_MIN || skipDistance > INT_MAX)
        return false;
    int skipRelative = (int)skipDistance;
    memcpy(p, &skipRelative, sizeof(skipRelative)); p += sizeof(skipRelative);

    BYTE* fallthroughJump = p;
    *p++ = 0xE9;
    INT64 fallthroughDistance =
        (INT64)(uintptr_t)(g_exeBase +
            RVA_STORAGE_DISCARD_VISIBILITY_FALLTHROUGH) -
        (INT64)(uintptr_t)(fallthroughJump + 5);
    if (fallthroughDistance < INT_MIN || fallthroughDistance > INT_MAX)
        return false;
    int fallthroughRelative = (int)fallthroughDistance;
    memcpy(p, &fallthroughRelative,
           sizeof(fallthroughRelative)); p += sizeof(fallthroughRelative);

    if ((size_t)(p - stub) != 31)
        return false;
    if (!FlushCodeChecked(stub, (size_t)(p - stub), "native-discard-stub")) return false;

    INT64 stubDistance =
        (INT64)(uintptr_t)stub -
        (INT64)(uintptr_t)(patchSite + 5);
    if (stubDistance < INT_MIN || stubDistance > INT_MAX)
        return false;

    BYTE replacement[STORAGE_DISCARD_VISIBILITY_OVERWRITE_LENGTH] = {};
    memset(replacement, 0x90, sizeof(replacement));
    replacement[0] = 0xE9;
    int stubRelative = (int)stubDistance;
    memcpy(replacement + 1, &stubRelative, sizeof(stubRelative));

    DWORD oldProtect = 0;
    if (!MakeCodeWritable(patchSite, sizeof(replacement), &oldProtect, "native-resource-discard-visibility")) return false;
    memcpy(patchSite, replacement, sizeof(replacement));
    FinishCodePatch(patchSite, sizeof(replacement), oldProtect, "native-resource-discard-visibility");
    Report("INFO", "Technical Services",
        "native-resource-discard-visibility",
        "active=1 gate=exe+0x%X technical_services_force_native_per_resource_discard=1 other_panels_original_gate=1 native_icon_tooltip_step_and_click=1 tooltip_localization_id=0x873",
        RVA_STORAGE_DISCARD_VISIBILITY_GATE);
    return true;
}

static bool InstallDepotMaterialWarningUi()
{
    if (!g_depotMaterialWarningsEnabled) return false;

    BYTE* localizationSlot =
        g_exeBase + RVA_LOCALIZATION_GET_TEXT_IAT;
    if (!ReadablePtr(localizationSlot, sizeof(void*)))
    {
        Report("WARN", "Technical Services",
            "material-status-warnings",
            "The native localization helper is unreadable; material status warnings are disabled while the native building warning list remains active");
        return false;
    }
    g_nativeLocalizationGetText =
        *(NativeLocalizationGetTextFn*)localizationSlot;
    if (!g_nativeLocalizationGetText)
    {
        Report("WARN", "Technical Services",
            "material-status-warnings",
            "The native localization helper is null; material status warnings are disabled while the native building warning list remains active");
        return false;
    }

    BYTE* patchSite =
        g_exeBase + RVA_TECH_WARNING_APPEND_TRANSITION;
    if (!ReadablePtr(
            patchSite,
            sizeof(EXPECT_TECH_WARNING_APPEND_TRANSITION)) ||
        memcmp(patchSite,
               EXPECT_TECH_WARNING_APPEND_TRANSITION,
               sizeof(EXPECT_TECH_WARNING_APPEND_TRANSITION)) != 0)
    {
        Report("WARN", "Technical Services",
            "material-status-warnings",
            "The verified pre-measurement warning transition at exe+0x%X no longer matches; material status warnings are disabled",
            RVA_TECH_WARNING_APPEND_TRANSITION);
        return false;
    }

    BYTE* stub = AllocNear(patchSite, 192);
    const void* continuation =
        patchSite + TECH_WARNING_APPEND_OVERWRITE_LENGTH;
    const void* xmm9Source =
        g_exeBase + RVA_TECH_WARNING_MEASURE_PADDING;
    const void* xmm10Source = g_exeBase + RVA_STORAGE_ARG14;
    if (!stub || !BuildTechnicalServicesWarningAppendStub(
            stub, 192,
            (void*)&HookTechnicalServicesNativeWarnings,
            xmm9Source, xmm10Source, continuation))
    {
        Report("WARN", "Technical Services",
            "material-status-warnings",
            "Executable memory for the native warning-buffer bridge could not be prepared; material status warnings are disabled");
        return false;
    }
    if (!PatchTechnicalServicesWarningTransition(stub))
    {
        Report("WARN", "Technical Services",
            "material-status-warnings",
            "The native warning-buffer bridge could not be installed; material status warnings are disabled");
        return false;
    }

    InterlockedExchange(&g_depotMaterialWarningUiActive, 1);
    Report("INFO", "Technical Services",
        "material-status-warnings",
        "active=1 transition=exe+0x%X states=enabled_empty_or_OFF one_line_per_material fuel_selectable=0 localization=plugin resource_name=game native_utf16_buffer=rbp+0x250 buffer_capacity=%llu native_pre_measurement_append=1 native_font_wrapping_spacing_and_layout=1",
        RVA_TECH_WARNING_APPEND_TRANSITION,
        (unsigned long long)TECH_WARNING_BUFFER_CAPACITY);
    return true;
}

static bool ResolveNativeVehicleTextMeasurement()
{
    if (g_nativeVehicleCalcTextRect) return true;
    if (!g_engine)
    {
        Report("WARN", "Vehicle information", "tank-text-measure",
            "C3DDLL64.dll is unavailable; translated tank labels use the bounded fallback width estimator");
        return false;
    }

    FARPROC exported = GetProcAddress(
        g_engine, C3D_CALC_RECT_LEFT_UNICODE_NO_ARG);
    if (!exported ||
        !ReadablePtr((const void*)exported,
                     sizeof(EXPECT_C3D_CALC_RECT_LEFT_UNICODE_NO_ARG)) ||
        memcmp((const void*)exported,
               EXPECT_C3D_CALC_RECT_LEFT_UNICODE_NO_ARG,
               sizeof(EXPECT_C3D_CALC_RECT_LEFT_UNICODE_NO_ARG)) != 0)
    {
        Report("WARN", "Vehicle information", "tank-text-measure",
            "The native C3D bitmap-font rectangle export is unavailable or no longer matches its verified signature; translated tank labels use the bounded fallback width estimator");
        return false;
    }

    g_nativeVehicleCalcTextRect =
        (NativeVehicleCalcTextRectFn)exported;
    Report("INFO", "Vehicle information", "tank-text-measure",
        "active=1 export=%s c3d_rva=0x%llX signature_bytes=%llu result_layout=left_top_right_bottom_int32 hidden_result_argument=rdx localized_width=1",
        C3D_CALC_RECT_LEFT_UNICODE_NO_ARG,
        (unsigned long long)((BYTE*)exported - (BYTE*)g_engine),
        (unsigned long long)
            sizeof(EXPECT_C3D_CALC_RECT_LEFT_UNICODE_NO_ARG));
    return true;
}

static bool MeasureNativeVehicleTextWidth(
    void* font, const wchar_t* text, float* widthOut)
{
    if (widthOut) *widthOut = 0.0f;
    if (!font || !text || !text[0] || !widthOut ||
        !g_nativeVehicleCalcTextRect)
        return false;

    __try
    {
        NativeC3dRect rect = {};
        NativeC3dRect* returned = g_nativeVehicleCalcTextRect(
            font, &rect, 0.0f, 0.0f, text);
        long long width =
            (long long)rect.right - (long long)rect.left;
        if (returned != &rect || width <= 0 || width > 10000 ||
            rect.bottom < rect.top)
            return false;
        *widthOut = (float)width;
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void DrawVehicleTankAtActiveEngineLabel(
    void* vehicle, void* renderer, void* font,
    float anchorX, float anchorY,
    DWORD nativeLabelColor, size_t callerRva)
{
    if (!g_runtimeActive || !g_sandVehicleTankDisplayEnabled ||
        !vehicle || !renderer || !font ||
        !g_nativeVehicleDrawFormattedText)
        return;

    __try
    {
        float remainingKg = 0.0f;
        float capacityKg = 0.0f;
        int dryPlowing = 0;
        int returningToDepot = 0;
        int resourceTextId = 0;
        char resourceName[64] = {};
        bool tankAvailable = SandGetShadowTankDisplayDetails(
            vehicle, &remainingKg, &capacityKg,
            &dryPlowing, &returningToDepot,
            &resourceTextId, resourceName, sizeof(resourceName));

        LONG anchorCall = InterlockedIncrement(
            &g_sandVehicleTankDisplayAnchorCalls);
        if (anchorCall <= 8)
        {
            Report("INFO", "Vehicle information", "tank-display-anchor-call",
                "call=%ld vehicle=%p tank_available=%d remaining_kg=%.3f capacity_kg=%.3f dry_plowing=%d anchor_x=%.3f anchor_y=%.3f caller_rva=0x%llX selected_vehicle_register=r15",
                anchorCall, vehicle,
                tankAvailable ? 1 : 0, remainingKg, capacityKg,
                dryPlowing,
                anchorX, anchorY,
                (unsigned long long)callerRva);
        }
        if (!tankAvailable) return;

        float remainingDisplayKg = remainingKg;
        int capacityWholeKg = (int)(capacityKg + 0.5f);
        if (!(remainingDisplayKg == remainingDisplayKg) ||
            remainingDisplayKg < 0.0f)
            remainingDisplayKg = 0.0f;
        if (capacityWholeKg < 0) capacityWholeKg = 0;
        if (remainingDisplayKg > (float)capacityWholeKg)
            remainingDisplayKg = (float)capacityWholeKg;
        if (remainingDisplayKg < 0.005f)
            remainingDisplayKg = 0.0f;

        wchar_t materialFallback[64] = {};
        const wchar_t* label = VehicleTankLocalizedText(
            g_vehicleTankLabelTextId, L"Streugut");
        const wchar_t* material = VehicleTankMaterialText(
            resourceTextId, resourceName,
            materialFallback, _countof(materialFallback));
        const wchar_t* dryPlowingText = VehicleTankLocalizedText(
            g_vehicleTankDryPlowingTextId, L"Trockenpfl\x00FCgen");
        const wchar_t* depotText = VehicleTankLocalizedText(
            g_vehicleTankDepotTextId, L"Depot");
        float dpi = ReadExeFloat(RVA_DPI, 1.0f);
        float drawX = anchorX +
            (float)g_sandVehicleTankDisplayOffsetX * dpi;
        float drawY = anchorY +
            (float)g_sandVehicleTankDisplayOffsetY * dpi;

        wchar_t labelWithColon[256] = {};
        wchar_t labelWithSeparator[256] = {};
        wchar_t valueText[256] = {};
        wchar_t completeText[512] = {};
        _snwprintf_s(labelWithColon, _countof(labelWithColon), _TRUNCATE,
                     L"%ls:", label);
        _snwprintf_s(labelWithSeparator, _countof(labelWithSeparator),
                     _TRUNCATE, L"%ls: ", label);
        if (dryPlowing && returningToDepot)
            _snwprintf_s(valueText, _countof(valueText), _TRUNCATE,
                         L"0 / %d kg - %ls", capacityWholeKg, depotText);
        else if (dryPlowing)
            _snwprintf_s(valueText, _countof(valueText), _TRUNCATE,
                         L"%ls", dryPlowingText);
        else
            _snwprintf_s(valueText, _countof(valueText), _TRUNCATE,
                         L"%.2f / %d kg - %ls",
                         (double)remainingDisplayKg,
                         capacityWholeKg, material);
        _snwprintf_s(completeText, _countof(completeText), _TRUNCATE,
                     L"%ls %ls", labelWithColon, valueText);

        float labelAdvance = 0.0f;
        float completeWidth = 0.0f;
        float valueWidth = 0.0f;
        bool nativeMeasured =
            MeasureNativeVehicleTextWidth(
                font, completeText, &completeWidth) &&
            MeasureNativeVehicleTextWidth(
                font, valueText, &valueWidth);
        if (nativeMeasured)
        {
            labelAdvance = completeWidth - valueWidth;
            if (!(labelAdvance == labelAdvance) ||
                labelAdvance <= 0.0f || labelAdvance > 10000.0f)
                nativeMeasured = false;
        }
        if (!nativeMeasured)
        {
            labelAdvance = EstimateOfficePriorityTextWidth(
                &dpi, labelWithSeparator);
            if (InterlockedCompareExchange(
                    &g_sandVehicleTankMeasureFallbackLogged, 1, 0) == 0)
            {
                Report("WARN", "Vehicle information", "tank-text-measure",
                    "Native measurement failed for the active vehicle font; the current localized label uses the bounded DPI-aware fallback width %.3f",
                    labelAdvance);
            }
        }
        if (!(labelAdvance == labelAdvance) ||
            labelAdvance <= 0.0f || labelAdvance > 10000.0f)
            return;

        // The two text runs never overlap. The first uses the exact colour of
        // the adjacent native Engine power label. The second begins after the
        // native complete-line rectangle minus the native value-only rectangle.
        // C3D excludes a trailing blank from a tight label rectangle; this
        // differential measurement retains the actual separator advance.
        g_nativeVehicleDrawFormattedText(
            renderer,
            font,
            drawX,
            drawY,
            nativeLabelColor,
            L"%ls",
            labelWithColon);
        g_nativeVehicleDrawFormattedText(
            renderer,
            font,
            drawX + labelAdvance,
            drawY,
            0xFF000000,
            L"%ls",
            valueText);

        if (InterlockedCompareExchange(
                &g_sandVehicleTankDisplayFirstDrawLogged, 1, 0) == 0)
        {
            Report("INFO", "Vehicle information", "tank-display-first-draw",
                "vehicle=%p remaining_kg=%.2f capacity_kg=%d display_decimal_places=2 dry_plowing=%d returning_to_depot=%d resource_text_id=%d resource_name=%s displayed_value=%s native_label_color=0x%08X value_color=0xFF000000 complete_width=%.3f value_width=%.3f label_advance=%.3f native_measure=%d offset_x=%d offset_y=%d selected_vehicle_register=r15 visible_anchor=exe+0x%X caller_rva=0x%llX resources_button_fix_style_caller_validation=1",
                vehicle,
                (double)remainingDisplayKg, capacityWholeKg,
                dryPlowing,
                returningToDepot,
                resourceTextId,
                resourceName[0] ? resourceName : "none",
                dryPlowing && returningToDepot ?
                    "zero-capacity-depot" :
                (dryPlowing ? "localized-dry-plowing" :
                              "amount-capacity-material"),
                (unsigned int)nativeLabelColor,
                completeWidth,
                valueWidth,
                labelAdvance,
                nativeMeasured ? 1 : 0,
                g_sandVehicleTankDisplayOffsetX,
                g_sandVehicleTankDisplayOffsetY,
                RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL,
                (unsigned long long)callerRva);
        }
    }
    __except(FaultFilter("technical_service_storage vehicle tank display",
                         GetExceptionInformation()))
    {
        if (InterlockedCompareExchange(
                &g_sandVehicleTankDisplayFaultLogged, 1, 0) == 0)
        {
            Report("WARN", "Vehicle information", "tank-display-fault",
                "The first confirmed tank display draw faulted and was skipped; the native vehicle-information renderer remains active");
        }
    }
}

__declspec(noinline)
static void __fastcall HookActiveVehicleEngineLabelDraw(
    void* renderer,
    void* font,
    float x,
    float y,
    DWORD color,
    const wchar_t* format,
    const wchar_t* localizedLabel)
{
    // The generated bridge stores the caller's non-volatile r15 before it
    // tail-jumps here. Consume the value once so no later UI call can reuse a
    // stale vehicle pointer.
    void* vehicle = InterlockedExchangePointer(
        (PVOID volatile*)&g_activeVehicleInfoVehicle, nullptr);
    void* returnAddress = _ReturnAddress();
    size_t callerRva = DirectCallerRva(returnAddress);
    size_t rawReturnRva = (size_t)-1;
    BYTE* rawReturn = (BYTE*)returnAddress;
    if (g_exeBase && rawReturn && rawReturn >= g_exeBase &&
        rawReturn < g_exeBase + g_exeSize)
        rawReturnRva = (size_t)(rawReturn - g_exeBase);

    // Preserve the native engine-power label exactly. Calling the original IAT
    // target directly also prevents recursion through this one patched call.
    if (g_nativeVehicleDrawFormattedText)
    {
        g_nativeVehicleDrawFormattedText(
            renderer, font, x, y, color, format, localizedLabel);
    }

    LONG wrapperCall = InterlockedIncrement(
        &g_sandVehicleTankDisplayWrapperCalls);
    if (wrapperCall <= 8)
    {
        Report("INFO", "Vehicle information", "tank-display-wrapper-call",
            "call=%ld vehicle=%p raw_return_rva=0x%llX resolved_caller_rva=0x%llX expected_caller_rva=0x%X native_label_color=0x%08X selected_vehicle_register=r15",
            wrapperCall, vehicle,
            (unsigned long long)rawReturnRva,
            (unsigned long long)callerRva,
            RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL,
            (unsigned int)color);
    }

    // resources_button_fix uses the same rule for shared renderers: only the
    // exact verified caller is allowed to alter the visible target window.
    // PatchOfficePriorityCall emits a five-byte call followed by one NOP, hence
    // the raw return address is anchor+5. DirectCallerRva deliberately folds a
    // direct E8 return address back to the call instruction itself.
    if (callerRva ==
            (size_t)RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL)
    {
        DrawVehicleTankAtActiveEngineLabel(
            vehicle, renderer, font, x, y, color, callerRva);
    }
}

static bool BuildActiveVehicleDrawJumpStub(BYTE* stub, size_t capacity,
                                           void* handler)
{
    if (!stub || capacity < 32 || !handler) return false;

    // Adapted from resources_button_fix's common-renderer bridge. At the
    // verified visible vehicle call, r15 is the selected vehicle. rax is
    // volatile and is not an argument, so it can carry the two absolute
    // addresses without disturbing rcx/rdx/r8/r9 or the stack varargs.
    memset(stub, 0xCC, capacity);
    BYTE* p = stub;
    void* activeSlot = (void*)&g_activeVehicleInfoVehicle;
    *p++ = 0x48; *p++ = 0xB8;               // mov rax,&activeVehicle
    memcpy(p, &activeSlot, sizeof(activeSlot)); p += sizeof(activeSlot);
    *p++ = 0x4C; *p++ = 0x89; *p++ = 0x38; // mov [rax],r15
    *p++ = 0x48; *p++ = 0xB8;               // mov rax,handler
    memcpy(p, &handler, sizeof(handler)); p += sizeof(handler);
    *p++ = 0xFF; *p++ = 0xE0;               // jmp rax (tail-call)

    if (!FlushCodeChecked(stub, capacity, "vehicle-display-stub")) return false;
    return (size_t)(p - stub) <= capacity;
}

static bool InstallVehicleTankDisplayHook()
{
    if (!g_sandVehicleTankDisplayEnabled) return false;

    BYTE* drawSlot = g_exeBase +
        RVA_OFFICE_PRIORITY_DRAW_FORMATTED_TEXT_IAT;
    if (!ReadablePtr(drawSlot, sizeof(void*)))
    {
        Report("WARN", "Vehicle information", "tank-display-helper",
            "The native formatted-text pointer slot is unreadable; the tank display is disabled");
        return false;
    }
    g_nativeVehicleDrawFormattedText =
        (NativeVehicleDrawFormattedTextFn)*(void**)drawSlot;
    if (!g_nativeVehicleDrawFormattedText)
    {
        Report("WARN", "Vehicle information", "tank-display-helper",
            "The native formatted-text function is unavailable; the tank display is disabled");
        return false;
    }

    bool nativeTextMeasurement =
        ResolveNativeVehicleTextMeasurement();

    if (!g_nativeLocalizationGetText)
    {
        BYTE* localizationSlot =
            g_exeBase + RVA_LOCALIZATION_GET_TEXT_IAT;
        if (ReadablePtr(localizationSlot, sizeof(void*)))
            g_nativeLocalizationGetText =
                *(NativeLocalizationGetTextFn*)localizationSlot;
    }
    if (!g_nativeLocalizationGetText)
    {
        Report("WARN", "Vehicle information", "tank-display-localization",
            "The native localization reader is unavailable; tank labels use safe fallback text and resource internal names");
    }

    if (!OfficePriorityIndirectCallMatches(
            RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL,
            RVA_OFFICE_PRIORITY_DRAW_FORMATTED_TEXT_IAT) ||
        memcmp(g_exeBase + RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL,
               EXPECT_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL,
               sizeof(EXPECT_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL)) != 0)
    {
        Report("WARN", "Vehicle information", "tank-display-anchor",
            "The live engine-label draw call at exe+0x%X no longer matches; the tank display is disabled",
            RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL);
        return false;
    }

    BYTE* drawStub = AllocNear(
        g_exeBase + RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL, 32);
    if (!drawStub)
    {
        Report("WARN", "Vehicle information", "tank-display-stub",
            "Executable memory for the visible vehicle display bridge could not be allocated; the tank display is disabled");
        return false;
    }

    if (!BuildActiveVehicleDrawJumpStub(
            drawStub, 32,
            (void*)&HookActiveVehicleEngineLabelDraw))
    {
        Report("WARN", "Vehicle information", "tank-display-stub",
            "The r15 selected-vehicle display bridge could not be generated; the tank display is disabled");
        return false;
    }
    if (!PatchOfficePriorityCall(
            RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL, drawStub, 6,
            "vehicle-tank-display-anchor-patch"))
    {
        Report("WARN", "Vehicle information", "tank-display-anchor-patch",
            "The visible engine-label draw call could not be bridged; the tank display is disabled");
        return false;
    }

    InterlockedExchange(&g_sandVehicleTankDisplayActive, 1);
    Report("INFO", "Vehicle information", "tank-display-hook",
        "active=1 selected_vehicle_register=r15 visible_anchor=exe+0x%X expected_caller_rva=0x%X offset_x=%d offset_y=%d text=localized_label_current_capacity_kg_localized_resource label_color=native_engine_label_argument value_color=0xFF000000 native_text_measurement=%d native_localization=%d visible_vehicle_copy=third resources_button_fix_style_caller_validation=1",
        RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL,
        RVA_ACTIVE_VEHICLE_ENGINE_LABEL_DRAW_CALL,
        g_sandVehicleTankDisplayOffsetX,
        g_sandVehicleTankDisplayOffsetY,
        nativeTextMeasurement ? 1 : 0,
        g_nativeLocalizationGetText ? 1 : 0);
    return true;
}

// -----------------------------------------------------------------------------
// Depot-specific grit-material priority controls

static int StorageSingleSpreaderMaterial(BYTE* storage)
{
    if (!storage) return -1;
    BYTE* slots = nullptr;
    size_t slotCount = 0;
    if (!GetStorageSlots(storage, &slots, &slotCount) || slotCount != 1)
        return -1;
    void* resource = *(void**)(slots + SLOT_RESOURCE);
    char resourceName[64] = {};
    ResourceName(resource, resourceName, sizeof(resourceName));
    return FindSpreaderMaterialIndex(resourceName);
}

static bool DepotPriorityHitTest(
    float x, float y, float width, float height,
    bool* hoveredOut, bool* clickedOut)
{
    if (hoveredOut) *hoveredOut = false;
    if (clickedOut) *clickedOut = false;
    if (!g_exeBase || !g_nativeInputGetMouseSolid ||
        !g_nativePanelCollision || width <= 0.0f || height <= 0.0f)
        return false;

    BYTE* positionAddress = g_exeBase + RVA_PANEL_RECT_POSITION;
    BYTE* sizeAddress = g_exeBase + RVA_PANEL_RECT_SIZE;
    BYTE* paddingAddress = g_exeBase + RVA_PANEL_RECT_PADDING;
    BYTE* colorAddress = g_exeBase + RVA_PANEL_RECT_COLOR;
    BYTE* clickAddress = g_exeBase + RVA_INPUT_CLICK_FLAG;
    if (!ReadablePtr(positionAddress, sizeof(float) * 2) ||
        !ReadablePtr(sizeAddress, sizeof(float) * 2) ||
        !ReadablePtr(paddingAddress, sizeof(int)) ||
        !ReadablePtr(colorAddress, sizeof(float) * 4) ||
        !ReadablePtr(clickAddress, sizeof(BYTE)))
        return false;

    float savedPosition[2] = {};
    float savedSize[2] = {};
    float savedColor[4] = {};
    int savedPadding = 0;
    memcpy(savedPosition, positionAddress, sizeof(savedPosition));
    memcpy(savedSize, sizeAddress, sizeof(savedSize));
    memcpy(savedColor, colorAddress, sizeof(savedColor));
    memcpy(&savedPadding, paddingAddress, sizeof(savedPadding));

    bool hovered = false;
    __try
    {
        float position[2] = {
            x + width * 0.5f,
            y + height * 0.5f
        };
        float size[2] = { width, height };
        memcpy(positionAddress, position, sizeof(position));
        memcpy(sizeAddress, size, sizeof(size));
        *(int*)paddingAddress = 0;

        BYTE mouseBuffer[16] = {};
        void* mouse = g_nativeInputGetMouseSolid(
            g_exeBase + RVA_INPUT_CONTEXT, mouseBuffer);
        if (mouse)
        {
            // Native storage buttons pass zero padding here; the panel's
            // registered size above is the complete hit rectangle.
            hovered = g_nativePanelCollision(
                g_exeBase + RVA_PANEL_CONTEXT,
                mouse, 0.0f, 0.0f);
        }
    }
    __finally
    {
        memcpy(positionAddress, savedPosition, sizeof(savedPosition));
        memcpy(sizeAddress, savedSize, sizeof(savedSize));
        memcpy(colorAddress, savedColor, sizeof(savedColor));
        memcpy(paddingAddress, &savedPadding, sizeof(savedPadding));
    }

    if (hoveredOut) *hoveredOut = hovered;
    if (clickedOut) *clickedOut =
        hovered && (*(BYTE*)clickAddress != 0);
    return true;
}

static void DrawDepotMaterialPriorityControl(
    void* window, BYTE* building, BYTE* storage,
    float rowX, float rowTop, float rowBottom)
{
    (void)window;
    if (!InterlockedCompareExchange(
            &g_depotMaterialPriorityUiActive, 0, 0) ||
        !building || !storage || !g_nativeVehicleDrawFormattedText)
        return;

    int materialIndex = StorageSingleSpreaderMaterial(storage);
    if (materialIndex < 0) return;

    DWORD presentMask = SpreaderObservePresentMaterialMask(building);
    if (!(presentMask & (DWORD)(1u << (unsigned)materialIndex)))
        return;

    int priorities[SPREADER_MAX_MATERIALS] = {};
    int availableCount = 0;
    SpreaderGetDepotPrioritySnapshot(
        building, presentMask, priorities, &availableCount);
    if (availableCount < 1) return;
    int priority = priorities[materialIndex];

    float dpi = ReadExeFloat(RVA_DPI, 1.0f);
    if (!(dpi == dpi) || dpi <= 0.0f || dpi > 8.0f) dpi = 1.0f;
    if (!(rowX == rowX) || !(rowTop == rowTop) ||
        !(rowBottom == rowBottom) || rowBottom < rowTop ||
        rowBottom - rowTop > 1000.0f)
        return;

    float drawX = rowX +
        (float)g_depotMaterialPriorityOffsetX * dpi;
    // Mirror the verified single-resource branch of the native renderer. Its
    // local phase advances by 23 logical pixels and the title is then drawn 45
    // pixels above that phase. The renderer's final rowBottom includes later
    // icon work and is therefore not a valid title anchor.
    float nativePhaseAdvance = ReadExeFloat(
        RVA_STORAGE_TITLE_PHASE_ADVANCE, 23.0f);
    float nativeTitleLead = ReadExeFloat(
        RVA_STORAGE_TITLE_LEAD, 45.0f);
    if (!(nativePhaseAdvance == nativePhaseAdvance) ||
        nativePhaseAdvance < 0.0f || nativePhaseAdvance > 100.0f)
        nativePhaseAdvance = 23.0f;
    if (!(nativeTitleLead == nativeTitleLead) ||
        nativeTitleLead < nativePhaseAdvance || nativeTitleLead > 150.0f)
        nativeTitleLead = 45.0f;
    float nativeTitleOffset = nativeTitleLead - nativePhaseAdvance;
    float nativeTitleY = rowTop - nativeTitleOffset * dpi;
    float drawY = nativeTitleY +
        (float)g_depotMaterialPriorityOffsetY * dpi;
    float hitWidth = (float)g_depotMaterialPriorityWidth * dpi;
    float hitHeight = (float)g_depotMaterialPriorityHeight * dpi;

    bool hovered = false;
    bool clicked = false;
    DepotPriorityHitTest(
        drawX, drawY, hitWidth, hitHeight,
        &hovered, &clicked);

    if (hovered && InterlockedCompareExchange(
            &g_depotMaterialPriorityFirstHoverLogged, 1, 0) == 0)
    {
        const SpreaderMaterialDefinition* material =
            SpreaderMaterialAt(materialIndex);
        Report("INFO", "Technical Services", "material-priority-first-hover",
            "building=%p resource=%s priority=%d draw_x=%.3f draw_y=%.3f hit_width=%.3f hit_height=%.3f collision_padding=0",
            building,
            material ? material->resourceName : "unknown",
            priority, drawX, drawY, hitWidth, hitHeight);
    }

    if (clicked)
    {
        int previous = 0;
        int current = 0;
        int swappedMaterial = -1;
        int cycleCount = 0;
        if (SpreaderCycleDepotPriority(
                building, presentMask, materialIndex,
                &previous, &current, &swappedMaterial, &cycleCount))
        {
            priority = current;
            const SpreaderMaterialDefinition* selected =
                SpreaderMaterialAt(materialIndex);
            const SpreaderMaterialDefinition* swapped =
                SpreaderMaterialAt(swappedMaterial);
            Report("INFO", "Technical Services", "material-priority-change",
                "building=%p resource=%s previous=%d current=%d available_priorities=%d swapped_resource=%s persistence=next-game-save tank_reload=next-fill",
                building,
                selected ? selected->resourceName : "unknown",
                previous, current, cycleCount,
                swapped ? swapped->resourceName : "none");
        }
    }

    void* renderer = g_exeBase + RVA_FONT_MANAGER_CONTEXT;
    void* font = nullptr;
    BYTE* fontSlot = g_exeBase + RVA_CURRENT_UI_FONT_POINTER;
    if (ReadablePtr(fontSlot, sizeof(void*)))
        font = *(void**)fontSlot;
    if (!renderer || !font) return;

    const wchar_t* label = VehicleTankLocalizedText(
        g_depotPriorityLabelTextId, L"Priorit\x00E4t");
    const wchar_t* offText = VehicleTankLocalizedText(
        g_depotPriorityOffTextId, L"AUS");
    wchar_t labelText[160] = {};
    wchar_t valueText[64] = {};
    wchar_t completeText[256] = {};
    _snwprintf_s(labelText, _countof(labelText), _TRUNCATE,
                 L"%ls:", label);
    if (priority > 0)
        _snwprintf_s(valueText, _countof(valueText), _TRUNCATE,
                     L"%d", priority);
    else
        _snwprintf_s(valueText, _countof(valueText), _TRUNCATE,
                     L"%ls", offText);
    _snwprintf_s(completeText, _countof(completeText), _TRUNCATE,
                 L"%ls %ls", labelText, valueText);

    float completeWidth = 0.0f;
    float valueWidth = 0.0f;
    float labelAdvance = 0.0f;
    if (MeasureNativeVehicleTextWidth(
            font, completeText, &completeWidth) &&
        MeasureNativeVehicleTextWidth(
            font, valueText, &valueWidth) &&
        completeWidth > valueWidth)
    {
        labelAdvance = completeWidth - valueWidth;
    }
    else
    {
        labelAdvance = EstimateOfficePriorityTextWidth(
            &dpi, labelText) + 4.0f * dpi;
    }

    g_nativeVehicleDrawFormattedText(
        renderer, font, drawX, drawY,
        0xFFFF0000, L"%ls", labelText);
    g_nativeVehicleDrawFormattedText(
        renderer, font, drawX + labelAdvance, drawY,
        hovered ? 0xFFFF0000 : 0xFF000000,
        L"%ls", valueText);

    if (InterlockedCompareExchange(
            &g_depotMaterialPriorityFirstDrawLogged, 1, 0) == 0)
    {
        const SpreaderMaterialDefinition* material =
            SpreaderMaterialAt(materialIndex);
        Report("INFO", "Technical Services", "material-priority-first-draw",
            "building=%p resource=%s priority=%d available_priorities=%d fuel_counted=0 catalogue_only=1 row_x=%.3f row_top=%.3f row_bottom=%.3f native_phase_advance=%.3f native_title_lead=%.3f native_title_offset=%.3f native_title_y=%.3f draw_x=%.3f draw_y=%.3f hit_width=%.3f hit_height=%.3f",
            building,
            material ? material->resourceName : "unknown",
            priority, availableCount,
            rowX, rowTop, rowBottom,
            nativePhaseAdvance, nativeTitleLead, nativeTitleOffset,
            nativeTitleY,
            drawX, drawY, hitWidth, hitHeight);
    }
}

static void DrawDepotMaterialSectionDivider(
    float rowX, float dividerTop, float dpi)
{
    if (!g_depotMaterialDividerEnabled ||
        !g_nativeVehicleDrawFormattedText || !g_exeBase)
        return;

    void* renderer = g_exeBase + RVA_FONT_MANAGER_CONTEXT;
    void* font = nullptr;
    BYTE* fontSlot = g_exeBase + RVA_CURRENT_UI_FONT_POINTER;
    if (ReadablePtr(fontSlot, sizeof(void*)))
        font = *(void**)fontSlot;
    if (!renderer || !font) return;

    const wchar_t* localizedGrit = VehicleTankLocalizedText(
        g_vehicleTankLabelTextId, L"Streugut");
    wchar_t label[160] = {};
    _snwprintf_s(label, _countof(label), _TRUNCATE,
                 L"%ls:", localizedGrit);

    float labelWidth = 0.0f;
    if (!MeasureNativeVehicleTextWidth(font, label, &labelWidth) ||
        labelWidth <= 0.0f)
    {
        labelWidth = EstimateOfficePriorityTextWidth(&dpi, label);
    }

    float drawX = rowX + 10.0f * dpi;
    float drawY = dividerTop + 2.0f * dpi;
    g_nativeVehicleDrawFormattedText(
        renderer, font, drawX, drawY,
        0xFFFF0000, L"%ls", label);
    g_nativeVehicleDrawFormattedText(
        renderer, font, drawX + labelWidth + 8.0f * dpi, drawY,
        0xFF9C8B78, L"------------------------------");

    if (InterlockedCompareExchange(
            &g_depotMaterialDividerFirstDrawLogged, 1, 0) == 0)
    {
        Report("INFO", "Technical Services", "material-section-divider",
            "active=1 title=localized_grit native_storage_section_first=1 vehicle_fuel_above=1 fuel_selectable=0 divider_top=%.3f draw_x=%.3f draw_y=%.3f reserved_height=%d",
            dividerTop, drawX, drawY,
            g_depotMaterialDividerHeight);
    }
}

static bool InstallDepotMaterialPriorityUi()
{
    if (!g_depotMaterialPriorityEnabled) return false;

    BYTE* drawSlot = g_exeBase +
        RVA_OFFICE_PRIORITY_DRAW_FORMATTED_TEXT_IAT;
    BYTE* mouseSlot = g_exeBase +
        RVA_INPUT_GET_MOUSE_SOLID_IAT;
    BYTE* collisionSlot = g_exeBase + RVA_PANEL_COLLISION_IAT;
    if (!ReadablePtr(drawSlot, sizeof(void*)) ||
        !ReadablePtr(mouseSlot, sizeof(void*)) ||
        !ReadablePtr(collisionSlot, sizeof(void*)))
    {
        Report("WARN", "Technical Services", "material-priority-ui",
            "One or more native UI import slots are unreadable; depot priority controls are disabled while catalogue-order defaults remain active");
        return false;
    }

    if (!g_nativeVehicleDrawFormattedText)
        g_nativeVehicleDrawFormattedText =
            *(NativeVehicleDrawFormattedTextFn*)drawSlot;
    g_nativeInputGetMouseSolid =
        *(NativeInputGetMouseSolidFn*)mouseSlot;
    g_nativePanelCollision =
        *(NativePanelCollisionFn*)collisionSlot;

    if (!g_nativeLocalizationGetText)
    {
        BYTE* localizationSlot =
            g_exeBase + RVA_LOCALIZATION_GET_TEXT_IAT;
        if (ReadablePtr(localizationSlot, sizeof(void*)))
            g_nativeLocalizationGetText =
                *(NativeLocalizationGetTextFn*)localizationSlot;
    }
    if (!g_nativeVehicleDrawFormattedText ||
        !g_nativeInputGetMouseSolid || !g_nativePanelCollision)
    {
        Report("WARN", "Technical Services", "material-priority-ui",
            "One or more native UI helpers are unavailable; depot priority controls are disabled while catalogue-order defaults remain active");
        return false;
    }

    ResolveNativeVehicleTextMeasurement();
    InterlockedExchange(&g_depotMaterialPriorityUiActive, 1);
    Report("INFO", "Technical Services", "material-priority-ui",
        "active=1 values=1..N_or_OFF uniqueness=swap runtime_scope=building-lifetime persistence_scope=save-folder-position-plus-resource-name offset_x=%d offset_y=%d width=%d height=%d native_priority_text_id=%d collision_padding=0 click_source=verified_global_edge_flag internal_scroll_helper=0",
        g_depotMaterialPriorityOffsetX,
        g_depotMaterialPriorityOffsetY,
        g_depotMaterialPriorityWidth,
        g_depotMaterialPriorityHeight,
        g_depotPriorityLabelTextId);
    return true;
}

// -----------------------------------------------------------------------------
// Per-panel duplicate tracking

static void ResetPanelState(BYTE* building)
{
    g_currentBuilding = building;
    // The ninth priority row reaches a shared final cursor store. Reset its
    // pending addition before every panel render so the collapsed-priority
    // path can never reuse a wrapped-row offset from the previous frame.
    g_officePriorityPendingRowExtra = 0.0f;
    g_officePriorityPendingRowCenterShift = 0.0f;
    g_seenResourceCount = 0;
    g_seenStorageCount = 0;
    g_nativeTemplate = NativeTemplate{};
}

static bool SeenResource(void* resource)
{
    if (!resource) return false;
    for (int i = 0; i < g_seenResourceCount; ++i)
        if (g_seenResources[i] == resource) return true;
    return false;
}

static void MarkResource(void* resource)
{
    if (!resource || SeenResource(resource)) return;

    if (g_seenResourceCount >= MAX_SEEN_RESOURCES)
    {
        if (InterlockedCompareExchange(
                &g_resourceLimitWarningLogged, 1, 0) == 0)
        {
            RuntimeWarning("Technical Services", "resource-tracking-limit",
                "The duplicate tracker reached its structural limit of %d "
                "resources; additional resources will not be tracked",
                MAX_SEEN_RESOURCES);
        }
        return;
    }

    g_seenResources[g_seenResourceCount++] = resource;
}

static bool SeenStorage(BYTE* storage)
{
    if (!storage) return false;
    for (int i = 0; i < g_seenStorageCount; ++i)
        if (g_seenStorages[i] == storage) return true;
    return false;
}

static void MarkStorage(BYTE* storage)
{
    if (!storage || SeenStorage(storage) || g_seenStorageCount >= MAX_SEEN_STORAGES)
        return;
    g_seenStorages[g_seenStorageCount++] = storage;
}

static void MarkStorageResources(BYTE* storage)
{
    if (!storage) return;
    MarkStorage(storage);

    BYTE* slots = nullptr;
    size_t count = 0;
    if (!GetStorageSlots(storage, &slots, &count)) return;

    for (size_t i = 0; i < count; ++i)
    {
        void* resource = *(void**)(slots + i * SLOT_SIZE + SLOT_RESOURCE);
        if (resource && ReadablePtr(resource, 0x50)) MarkResource(resource);
    }
}

// -----------------------------------------------------------------------------
// Native renderer template and dynamic insertion

static NativeTemplate BuildFallbackTemplate(BYTE* building)
{
    // This fallback starts from the verified Technical Services call in
    // WRSR 1.1.1.9.  The stock call uses the compact arg11=9 path, which exits
    // before the native per-resource controls are constructed.  v0.1.47 uses
    // the normal arg11=-1 resource-row path, but keeps arg15 disabled: arg15
    // adds the separate storage-wide clear control, which is wrong for the
    // individually appended Technical Services rows. It is used only when the
    // vanilla panel did not render any storage row from which an exact runtime
    // template could be captured.
    // BuildMatches validates the exact executable identity, the relevant
    // function prologues and both direct call targets before this path can run.
    // Any future game build must be re-analysed instead of reusing these values.
    NativeTemplate t = {};
    if (!building || !ReadablePtr(building + B_NATIVE_ARG10, sizeof(int))) return t;

    t.valid = 1;
    t.arg6 = 0;
    t.arg7 = 1;
    t.arg8 = 0.0f;
    t.arg9 = -1;
    t.arg10 = *(int*)(building + B_NATIVE_ARG10);
    t.arg11 = -1;
    t.arg12 = 0.0f;
    t.arg13 = ReadExeFloat(RVA_STORAGE_ARG13, 1.0f);
    t.arg14 = ReadExeFloat(RVA_STORAGE_ARG14, 1.0f);
    // Keep the full row's small per-resource remove control, but suppress the
    // additional storage-wide clear control.
    t.arg15 = 0;
    t.arg16 = 0;
    return t;
}

static bool ShouldAppendStorage(BYTE* storage,
                                void** resourceOut,
                                float* amountOut,
                                float* capacityOut,
                                int* transportClassOut,
                                AppendStats* stats)
{
    if (resourceOut) *resourceOut = nullptr;
    if (amountOut) *amountOut = 0.0f;
    if (capacityOut) *capacityOut = 0.0f;
    if (transportClassOut) *transportClassOut = -1;

    if (!storage || SeenStorage(storage))
    {
        if (stats) ++stats->duplicates;
        return false;
    }

    if (!ReadablePtr(storage, STORAGE_SIZE))
    {
        if (stats) ++stats->invalid;
        return false;
    }

    float capacity = *(float*)(storage + STORAGE_CAPACITY);
    int transportClass = *(int*)(storage + STORAGE_CLASS);
    if (!IsFiniteNonNegative(capacity) || capacity <= 0.0f)
    {
        if (stats) ++stats->invalid;
        return false;
    }

    if (!IsSupportedTransportClass(transportClass))
    {
        if (stats) ++stats->unsupportedClass;
        return false;
    }

    BYTE* slots = nullptr;
    size_t count = 0;
    if (!GetStorageSlots(storage, &slots, &count))
    {
        if (stats) ++stats->invalid;
        return false;
    }

    // This is the structural form produced by the supported *_SPECIAL policy.
    // A general $STORAGE_IMPORT creates multiple resource slots and is ignored.
    if (count != 1)
    {
        if (stats) ++stats->nonSingleResource;
        return false;
    }

    void* resource = *(void**)(slots + SLOT_RESOURCE);
    float amount = *(float*)(slots + SLOT_AMOUNT);
    if (!resource || !ReadablePtr(resource, 0x50) || !IsFiniteNonNegative(amount))
    {
        if (stats) ++stats->invalid;
        return false;
    }

    if (SeenResource(resource))
    {
        if (stats) ++stats->duplicates;
        return false;
    }

    if (resourceOut) *resourceOut = resource;
    if (amountOut) *amountOut = amount;
    if (capacityOut) *capacityOut = capacity;
    if (transportClassOut) *transportClassOut = transportClass;
    return true;
}

static void AppendDynamicStorages(void* game, void* window,
                                  float* layoutX, float* layoutY,
                                  BYTE* building)
{
    if (!g_originalNativeStoragePanel || !building || !layoutX || !layoutY ||
        !ReadablePtr(layoutX, sizeof(float)) ||
        !ReadablePtr(layoutY, sizeof(float)) ||
        !ReadablePtr(building + B_STORAGE_BEGIN, 16))
    {
        RuntimeWarning("Technical Services", "append-precondition",
            "Dynamic storage insertion was skipped because a required pointer is invalid");
        return;
    }

    BYTE* begin = *(BYTE**)(building + B_STORAGE_BEGIN);
    BYTE* end = *(BYTE**)(building + B_STORAGE_END);
    if (!begin || !end || end < begin)
    {
        RuntimeWarning("Technical Services", "storage-vector",
            "Storage vector pointers are invalid; dynamic storage insertion was skipped");
        return;
    }

    size_t bytes = (size_t)(end - begin);
    if ((bytes % STORAGE_SIZE) != 0)
    {
        RuntimeWarning("Technical Services", "storage-vector",
            "Storage vector byte size %llu is not aligned to stride 0x%llX; insertion was skipped",
            (unsigned long long)bytes, (unsigned long long)STORAGE_SIZE);
        return;
    }

    size_t count = bytes / STORAGE_SIZE;
    if (count > MAX_STORAGES || (bytes && !ReadablePtr(begin, bytes)))
    {
        RuntimeWarning("Technical Services", "storage-vector",
            "Storage vector count %llu is outside the safe limit of %llu or is unreadable; insertion was skipped",
            (unsigned long long)count, (unsigned long long)MAX_STORAGES);
        return;
    }

    NativeTemplate t = g_nativeTemplate.valid
                     ? g_nativeTemplate
                     : BuildFallbackTemplate(building);
    if (!t.valid)
    {
        RuntimeWarning("Technical Services", "native-template",
            "Native storage renderer arguments could not be captured or reconstructed; insertion was skipped");
        return;
    }

    float dpi = ReadExeFloat(RVA_DPI, 1.0f);
    if (!(dpi == dpi) || dpi <= 0.0f || dpi > 8.0f) dpi = 1.0f;
    float gap = dpi * (float)g_resourceGap;

    AppendStats stats = {};
    stats.totalStorages = (int)count;
    bool materialSectionStarted = false;

    for (size_t i = 0; i < count; ++i)
    {
        BYTE* storage = begin + i * STORAGE_SIZE;
        void* resource = nullptr;
        float amount = 0.0f;
        float capacity = 0.0f;
        int transportClass = -1;

        if (!ShouldAppendStorage(storage, &resource, &amount, &capacity,
                                 &transportClass, &stats))
            continue;

        int materialIndex = StorageSingleSpreaderMaterial(storage);
        bool beginsMaterialSection =
            materialIndex >= 0 && !materialSectionStarted;
        if (beginsMaterialSection)
        {
            materialSectionStarted = true;
            if (g_depotMaterialDividerEnabled)
            {
                DrawDepotMaterialSectionDivider(*layoutX, *layoutY, dpi);
                *layoutY += dpi *
                    (float)g_depotMaterialDividerHeight;
            }
            else if (gap > 0.0f)
            {
                *layoutY += gap;
            }
        }
        else if (gap > 0.0f)
        {
            *layoutY += gap;
        }

        float rowX = *layoutX;
        float rowTop = *layoutY;

        g_originalNativeStoragePanel(
            game, window, storage, layoutX, layoutY,
            t.arg6, t.arg7, t.arg8, t.arg9, t.arg10, t.arg11,
            t.arg12, t.arg13, t.arg14, t.arg15, t.arg16);

        DrawDepotMaterialPriorityControl(
            window, building, storage,
            rowX, rowTop, *layoutY);

        MarkStorage(storage);
        MarkResource(resource);
        ++stats.appended;

        if (g_debug && g_lastDebugBuilding != building)
        {
            char name[64];
            ResourceName(resource, name, sizeof(name));
            Debug("Technical Services", "storage-appended",
                "index=%llu resource=%s class=%s(%d) amount=%.3f capacity=%.3f",
                (unsigned long long)i, name,
                TransportClassName(transportClass), transportClass,
                amount, capacity);
        }
    }

    if (g_debug && g_lastDebugBuilding != building)
    {
        Debug("Technical Services", "panel-summary",
            "building=%p storages=%d appended=%d duplicates=%d unsupported_class=%d non_single=%d invalid=%d native_seen_resources=%d gap=%d template=%s",
            building,
            stats.totalStorages,
            stats.appended,
            stats.duplicates,
            stats.unsupportedClass,
            stats.nonSingleResource,
            stats.invalid,
            g_seenResourceCount,
            g_resourceGap,
            g_nativeTemplate.valid ? "captured" : "verified-fallback");
        g_lastDebugBuilding = building;
    }
}

// -----------------------------------------------------------------------------
// Hooks

static void __fastcall HookNativeStoragePanel(
    void* game,
    void* window,
    void* storage,
    float* layoutX,
    float* layoutY,
    int arg6,
    BYTE arg7,
    float arg8,
    int arg9,
    int arg10,
    int arg11,
    float arg12,
    float arg13,
    float arg14,
    BYTE arg15,
    BYTE arg16)
{
    if (!g_originalNativeStoragePanel) return;

    bool drawMaterialPriority = false;
    int effectiveArg11 = arg11;
    BYTE effectiveArg15 = arg15;
    BYTE* priorityBuilding = nullptr;
    float rowX = 0.0f;
    float rowTop = 0.0f;

    if (g_runtimeActive && g_insideTechnicalPanel && storage)
    {
        __try
        {
            // Record every resource the native panel is already drawing. Fuel
            // is therefore skipped only when it is actually present natively.
            MarkStorageResources((BYTE*)storage);

            BYTE* deleteButtonSlots = nullptr;
            size_t deleteButtonSlotCount = 0;
            if (GetStorageSlots(
                    (BYTE*)storage,
                    &deleteButtonSlots,
                    &deleteButtonSlotCount) &&
                deleteButtonSlotCount == 1)
            {
                // The Technical Services caller uses the compact arg11=9 path.
                // That path returns before the native per-resource remove
                // controls are built. Select the stock full resource-row path
                // for the single-resource rows supported here. Keep arg15 at
                // zero because it adds a second, larger storage-wide clear
                // control; the separately appended rows must not share that
                // aggregate control state. The game retains the small control's
                // own amount check, hover state, tooltip and click handler.
                effectiveArg11 = -1;
                effectiveArg15 = 0;
                if (InterlockedCompareExchange(
                        &g_depotStorageDeleteButtonFirstLogged,
                        1, 0) == 0)
                {
                    Report("INFO", "Technical Services",
                        "native-resource-delete-button",
                        "active=1 storage=%p single_resource_slots=1 native_full_row_mode_arg11=-1 per_resource_remove_controls=1 storage_wide_clear_arg15=0 native_stock_visibility_and_click_handler=1",
                        storage);
                }
            }

            if (g_currentBuilding && layoutX && layoutY &&
                ReadablePtr(layoutX, sizeof(float)) &&
                ReadablePtr(layoutY, sizeof(float)))
            {
                priorityBuilding = g_currentBuilding;
                rowX = *layoutX;
                rowTop = *layoutY;
                drawMaterialPriority = true;
            }

            size_t callerRva = DirectCallerRva(_ReturnAddress());
            if (callerRva == RVA_TECH_NATIVE_STORAGE_CALL)
            {
                g_nativeTemplate.valid = 1;
                g_nativeTemplate.arg6 = arg6;
                g_nativeTemplate.arg7 = arg7;
                g_nativeTemplate.arg8 = arg8;
                g_nativeTemplate.arg9 = arg9;
                g_nativeTemplate.arg10 = arg10;
                g_nativeTemplate.arg11 = effectiveArg11;
                g_nativeTemplate.arg12 = arg12;
                g_nativeTemplate.arg13 = arg13;
                g_nativeTemplate.arg14 = arg14;
                g_nativeTemplate.arg15 = effectiveArg15;
                g_nativeTemplate.arg16 = arg16;
            }
        }
        __except(FaultFilter("technical_service_storage native storage observer",
                             GetExceptionInformation()))
        {
            (void)0;
        }
    }

    g_originalNativeStoragePanel(
        game, window, storage, layoutX, layoutY,
        arg6, arg7, arg8, arg9, arg10, effectiveArg11,
        arg12, arg13, arg14, effectiveArg15, arg16);

    if (drawMaterialPriority && priorityBuilding && layoutY &&
        ReadablePtr(layoutY, sizeof(float)))
    {
        __try
        {
            DrawDepotMaterialPriorityControl(
                window, priorityBuilding, (BYTE*)storage,
                rowX, rowTop, *layoutY);
        }
        __except(FaultFilter(
            "technical_service_storage material priority control",
            GetExceptionInformation()))
        {
            if (InterlockedCompareExchange(
                    &g_depotMaterialPriorityFaultLogged, 1, 0) == 0)
            {
                Report("WARN", "Technical Services", "material-priority-fault",
                    "A depot priority control faulted and all such controls were disabled for this session; native storage rendering remains active");
            }
            InterlockedExchange(
                &g_depotMaterialPriorityUiActive, 0);
        }
    }
}

static void __fastcall HookPostStorageSection(void* game, void* window,
                                               float* layoutX, float* layoutY)
{
    if (!g_originalPostStorageSection) return;

    if (g_runtimeActive && g_insideTechnicalPanel && g_currentBuilding)
    {
        __try
        {
            size_t callerRva = DirectCallerRva(_ReturnAddress());
            if (callerRva == RVA_TECH_POST_STORAGE_CALL &&
                IsVisibleTechnicalWindow(window, nullptr))
            {
                AppendDynamicStorages(game, window, layoutX, layoutY,
                                      g_currentBuilding);
            }
        }
        __except(FaultFilter("technical_service_storage dynamic insertion",
                             GetExceptionInformation()))
        {
            (void)0;
        }
    }

    g_originalPostStorageSection(game, window, layoutX, layoutY);
}

static void __fastcall HookTechnicalServicesPanel(void* game, void* window)
{
    if (!g_originalTechnicalServicesPanel) return;

    BYTE* building = nullptr;
    bool isTarget = false;

    if (g_runtimeActive)
    {
        __try
        {
            isTarget = IsVisibleTechnicalWindow(window, &building);
            if (isTarget && !IsExpectedUiThread())
            {
                isTarget = false;
                building = nullptr;
            }

            if (isTarget)
            {
                SandDiagnosticObserve(game, building);
                g_insideTechnicalPanel = 1;
                ResetPanelState(building);
            }
        }
        __except(FaultFilter("technical_service_storage panel context",
                             GetExceptionInformation()))
        {
            isTarget = false;
            g_insideTechnicalPanel = 0;
            g_currentBuilding = nullptr;
        }
    }

    if (!isTarget)
    {
        g_originalTechnicalServicesPanel(game, window);
        return;
    }

    // __finally is deliberate here. With /EHsc, a C++ RAII guard is not
    // guaranteed to run for a native SEH exception. If the game handles such an
    // exception above this hook, no stale panel context may survive the call.
    __try
    {
        g_originalTechnicalServicesPanel(game, window);
    }
    __finally
    {
        g_insideTechnicalPanel = 0;
        g_currentBuilding = nullptr;
    }
}

// -----------------------------------------------------------------------------
// TesmioLoader API

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

    info->name = "Technical Service Storage + Grit Spreader";
    info->version = PLUGIN_VERSION;

    g_detail = TsmOpenLog(PLUGIN_LOG_NAME);
    DWORD detailOpenError = g_detail == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    BeginLogPhase();

    if (g_detail == INVALID_HANDLE_VALUE)
    {
        ReportWindows("WARN", PLUGIN_LOG_NAME, "log-open",
            "Detail log could not be opened", detailOpenError,
            "Verify write access to the TesmioLoader log directory");
    }

    Info("TesmioLoader technical_service_storage %s starting; detail log: %s",
         PLUGIN_VERSION, PLUGIN_LOG_NAME);

    LoadConfig();

    Info("Initialization: TesmioLoader host API %u", TSM_API_VERSION);
    Info("Configuration: enabled=%d debug=%d debug_limit=%d resource_gap=%d office_priority_wrap=%d office_priority_max_width=%d office_priority_line_height=%d material_priority_enabled=%d material_priority_persistence=%d tank_persistence=%d material_priority_offset=%d,%d material_priority_size=%d,%d material_section_divider=%d material_section_divider_height=%d material_status_warnings=%d",
         g_enabled, g_debug, g_debugLimit, g_resourceGap,
         g_officePriorityWrapEnabled,
         g_officePriorityMaximumWidth,
         g_officePriorityLineHeight,
         g_depotMaterialPriorityEnabled,
         g_depotMaterialPriorityPersistenceEnabled,
         g_sandTankPersistenceEnabled,
         g_depotMaterialPriorityOffsetX,
         g_depotMaterialPriorityOffsetY,
         g_depotMaterialPriorityWidth,
         g_depotMaterialPriorityHeight,
         g_depotMaterialDividerEnabled,
         g_depotMaterialDividerHeight,
         g_depotMaterialWarningsEnabled);
    Info("Storage policy: single-resource COVERED, OPEN and GRAVEL plus OIL/FUEL; general multi-resource storages are ignored");
    Info("Duplicate policy: resources already rendered by the native Technical Services panel are skipped automatically");
    Info("UI context policy: matching panels are bound to the first observed game UI thread and processed synchronously");
    Info("Duplicate tracker capacity: %d resources across at most %llu storages with %llu slots each",
         MAX_SEEN_RESOURCES,
         (unsigned long long)MAX_STORAGES,
         (unsigned long long)MAX_SLOTS_PER_STORAGE);
    Info("Resource policy: no cargo resource names are hard-coded; building storage order is preserved");
    Info("Grit spreader v%s: enabled=%d refill_depot_debit=atomic-loaded-amount native_fuel_factor_percent=%d duplicate_window_ms=%d max_vehicles=%d fixed_per_road_debit=removed",
         SPREADER_DIAGNOSTIC_VERSION, g_sandDiagnosticEnabled,
         g_sandFuelConsumptionFactorPercent, g_sandDuplicateWindowMs,
         g_sandMaxVehicles);
    Info("Vehicle tanks: enabled=%d capacity=weight-percent:%d+kg-per-kW:%d multiplier_percent=%d round_kg=%d range_kg=%d..%d consumption=native-fuel-delta factor_percent=%d automatic_return=%d threshold_basis_points=%d reserve_preserved=1 return_lock=%d real_fuel_mutation=0",
         g_sandShadowTankEnabled, g_sandTankWeightPercent,
         g_sandTankPowerKgPerKw, g_sandTankCapacityMultiplierPercent,
         g_sandTankCapacityStepKg, g_sandTankMinimumCapacityKg,
         g_sandTankMaximumCapacityKg, g_sandFuelConsumptionFactorPercent,
         g_sandAutomaticReturnEnabled, g_sandReturnThresholdBasisPoints,
         g_sandReturnLockEnabled);
    Info("Beta diagnostics: detailed_runtime=%d discovery_scans=removed manual_route_hotkeys=removed lifecycle_snapshot=debug-only-CTRL_F8", g_debug);
    Info("Grit spreader policy: [grit_materials] provides the bounded material catalogue (entries=%llu first=%s runtime_validation=per-world-live-resource-vector reserved_vehicle_fuel_excluded=1); each depot receives unique priorities 1..N or OFF, initialized from catalogue order and used by refilling; priorities and v0.1.74 vehicle grit tanks use separate versioned sidecars inside the resolved current save folder; tank restore matches home position, depot vehicle slot and verified power/weight signature, restores exact remaining kg/material without depot debit and safely reconstructs unmatched vehicles; exact building position and configured resource names restore an unchanged depot priority set; the first native going-away event deletes the old depot priority record for both demolition and collapse; a reused pointer or the post-rubble construction site starts a new lifetime from [grit_materials] defaults; world unload resets the bound UI thread so dynamically appended grit storages remain visible after loading another world in the same process; normal world unload/load is not a deletion event; the instant-storage routine is never used",
         (unsigned long long)g_spreaderMaterialCount,
         g_spreaderMaterialCount ?
             g_spreaderMaterials[0].resourceName : "none");
    Info("Office Priority UTF-16 wrapper prepared: max_lines=%d line_capacity=%llu; verified native label bridge will be installed during startup when enabled",
         OFFICE_PRIORITY_MAX_LINES,
         (unsigned long long)OFFICE_PRIORITY_LINE_CAPACITY);

    if (!g_enabled)
    {
        Info("enabled = 0; plugin remains inactive");
        LogSummaryStatus("Initialization", "was skipped because the plugin is disabled");
        return 1;
    }

    if (g_sandDiagnosticEnabled && g_sandShadowTankEnabled)
    {
        g_roadTreatmentServiceActive = H->provide && H->provide(
            TSM_SERVICE_GRIT_SPREADER, TSM_GRIT_SPREADER_VERSION, &g_spreaderApi);
        if (!g_roadTreatmentServiceActive)
        {
            Report("WARN", "Grit spreader", "road-treatment-service",
                "The synchronous road-treatment service could not be registered; tank, material consumption and dry plowing remain active but weather_roads will retain its legacy uniform protection");
        }
        else
        {
            Report("INFO", "Grit spreader", "road-treatment-service",
                "provided=1 service=%s version=%u synchronous_current_native_clear_only=1 stale_event_retention=0 dry_strength=0.000 material_strength=ini",
                TSM_SERVICE_GRIT_SPREADER,
                TSM_GRIT_SPREADER_VERSION);
        }
    }

    LogSummary("Initialization", true);
    return 0;
}

extern "C" __declspec(dllexport)
int TsmPluginStart(void)
{
    BeginLogPhase();
    const bool configuredOfficeWrap = g_officePriorityWrapEnabled != 0;
    if (!H || !g_exeBase || !g_enabled)
    {
        Report("FATAL", nullptr, "startup-state",
            "Required TesmioLoader host state is unavailable. "
            "Action: verify the loader and plugin installation");
        LogSummary("Startup", false);
        return 1;
    }

    if (!BuildMatches())
    {
        LogSummary("Startup", false);
        return 1;
    }

    ResolveVehicleTankLocalization();

    // Validate helper data before installing any hook. A game update must make
    // this plugin refuse safely rather than leave a partially trusted UI path.
    if (!ReadablePtr(g_exeBase + RVA_DPI, sizeof(float)) ||
        !ReadablePtr(g_exeBase + RVA_STORAGE_ARG13, sizeof(float)) ||
        !ReadablePtr(g_exeBase + RVA_STORAGE_ARG14, sizeof(float)) ||
        !ReadablePtr(
            g_exeBase + RVA_TECH_WARNING_MEASURE_PADDING,
            sizeof(float)))
    {
        Report("FATAL", "SOVIET64.exe", "helper-validation",
            "Native storage renderer helper data is unreadable. "
            "Action: verify the supported game version and plugin compatibility");
        LogSummary("Startup", false);
        return 1;
    }

    void* trampoline = nullptr;

    if (!InstallInlineHook(g_exeBase + RVA_TECHNICAL_SERVICES_PANEL,
                           (void*)&HookTechnicalServicesPanel,
                           &trampoline,
                           EXPECT_TECH_PANEL,
                           sizeof(EXPECT_TECH_PANEL),
                           "technical services dynamic panel context"))
    {
        Report("FATAL", "SOVIET64.exe", "panel-hook",
            "Technical Services panel hook was refused by TesmioLoader. "
            "Action: check for a conflicting plugin or game update");
        LogSummary("Startup", false);
        return 1;
    }
    g_originalTechnicalServicesPanel = (TechnicalServicesPanelFn)trampoline;

    trampoline = nullptr;
    if (!InstallInlineHook(g_exeBase + RVA_NATIVE_STORAGE_PANEL,
                           (void*)&HookNativeStoragePanel,
                           &trampoline,
                           EXPECT_NATIVE_STORAGE_PANEL,
                           sizeof(EXPECT_NATIVE_STORAGE_PANEL),
                           "technical services native storage observer"))
    {
        // The first hook is already installed and therefore this DLL must remain
        // loaded. Keep it in pass-through mode rather than returning a failure
        // that would allow the loader to unload code still referenced by a hook.
        g_runtimeActive = 0;
        Report("FATAL", "SOVIET64.exe", "storage-renderer-hook",
            "Native storage renderer hook was refused after the panel hook was installed. "
            "The plugin will remain loaded in pass-through mode. "
            "Action: remove conflicting UI hooks or use the supported game build");
        LogSummaryStatus("Startup", "entered safe pass-through mode");
        return 0;
    }
    g_originalNativeStoragePanel = (NativeStoragePanelFn)trampoline;

    trampoline = nullptr;
    if (!InstallInlineHook(g_exeBase + RVA_POST_STORAGE_SECTION,
                           (void*)&HookPostStorageSection,
                           &trampoline,
                           EXPECT_POST_STORAGE_SECTION,
                           sizeof(EXPECT_POST_STORAGE_SECTION),
                           "technical services dynamic storage insertion point"))
    {
        g_runtimeActive = 0;
        Report("FATAL", "SOVIET64.exe", "insertion-hook",
            "Post-storage insertion hook was refused after earlier hooks were installed. "
            "The plugin will remain loaded in pass-through mode. "
            "Action: remove conflicting UI hooks or use the supported game build");
        LogSummaryStatus("Startup", "entered safe pass-through mode");
        return 0;
    }
    g_originalPostStorageSection = (PostStorageSectionFn)trampoline;

    bool nativeResourceDiscardActive =
        InstallTechnicalServicesNativeDiscardGate();

    bool officePriorityWrapActive = false;
    bool officePriorityLayoutActive = false;
    if (g_officePriorityWrapEnabled)
    {
        officePriorityWrapActive = InstallOfficePriorityLabelHook();
        if (officePriorityWrapActive)
            officePriorityLayoutActive =
                InstallOfficePriorityRowLayoutHooks();

        if (!officePriorityWrapActive)
        {
            Report("WARN", "Technical Services", "office-priority-wrap",
                "The plugin remains active, but Office Priority labels retain native single-line rendering");
        }
        else if (!officePriorityLayoutActive)
        {
            // The label bridge is already installed and cannot safely be
            // removed. Make it a native pass-through so wrapped text can
            // never overlap rows when the coupled layout bridges are absent.
            g_officePriorityWrapEnabled = 0;
            Report("WARN", "Technical Services", "office-priority-layout",
                "The label bridge is active, but dynamic row layout could not be installed; wrapping was disabled for this session to preserve native spacing");
        }
    }

    g_runtimeActive = 1;

    bool vehicleTankDisplayActive =
        InstallVehicleTankDisplayHook();
    bool depotMaterialPriorityUiActive =
        InstallDepotMaterialPriorityUi();
    bool depotMaterialWarningUiActive =
        InstallDepotMaterialWarningUi();
    bool clearHooksActive = false;
    if (g_sandDiagnosticEnabled)
        clearHooksActive = SandDiagnosticInstallClearHooks();
    // Install before the observer can retain any live storage pointers.
    bool storageMigrationActive = InstallStorageMigrationHook();
    bool returnObserverActive = false;
    if (g_sandDiagnosticEnabled && clearHooksActive)
        returnObserverActive = SandDiagnosticStartReturnObserver();
    bool persistenceHooksActive =
        SpreaderInstallPriorityPersistenceHooks();

    Info("Dynamic Technical Services storage UI active for building type 49");
    if (g_sandDiagnosticEnabled)
        Info("Grit spreader v%s active: runtime skill classification=%s; confirmed clear-call observation=%s; material_catalogue=%llu; shadow_vehicle_tanks=%s; global_world_initialization=%s; building_lifecycle_diagnostic=%s; depot_priority_persistence=%s; vehicle_tank_persistence=%s; native_refuel_destination_return=%s; window_independent_arrival_observer=%s; refill_depot_consumption=%s",
             SPREADER_DIAGNOSTIC_VERSION,
             InterlockedCompareExchange(
              &g_sandSkillLayoutVerified, 0, 0) ? "active" : "unavailable",
              clearHooksActive ? "active" : "unavailable",
              (unsigned long long)g_spreaderMaterialCount,
              g_sandShadowTankEnabled ? (clearHooksActive ? "active-plugin-owned" : "unavailable") : "disabled",
              InterlockedCompareExchange(
               &g_sandGlobalRegistryVerified, 0, 0) ? "active" : "unavailable",
              InterlockedCompareExchange(
               &g_sandBuildingLifecycleLayoutVerified, 0, 0) ?
                  "active-read-only" : "unavailable",
              persistenceHooksActive &&
                  g_depotMaterialPriorityPersistenceEnabled ?
                  "active-save-sidecar" : "unavailable",
              persistenceHooksActive && g_sandTankPersistenceEnabled ?
                  "active-save-sidecar" : "unavailable",
              clearHooksActive && g_sandAutomaticReturnEnabled &&
                  g_sandReturnLockEnabled ? "active" : "unavailable",
              returnObserverActive ? "active" : "unavailable",
              g_sandShadowTankEnabled ? "atomic-loaded-amount" : "disabled");
    Info("Native renderer active at exe+0x%X; dynamic insertion call verified at exe+0x%X",
         RVA_NATIVE_STORAGE_PANEL, RVA_TECH_POST_STORAGE_CALL);
    Info("Office Priority label wrapping: %s (dynamic_row_layout=%s maximum_width=%d logical_px line_height=%d logical_px native_single_line_height=35 logical_px max_rendered_lines=2)",
         (officePriorityWrapActive && officePriorityLayoutActive) ? "active" :
         (configuredOfficeWrap ? "unavailable" : "disabled"),
         officePriorityLayoutActive ? "active" : "unavailable",
         g_officePriorityMaximumWidth,
         g_officePriorityLineHeight);
    Info("Vehicle tank display: %s (label_localization_id=%d none_localization_id=%d dry_plowing_localization_id=%d depot_localization_id=%d material=resource_text_id dry_state=explicit_per_vehicle return_state=compact-zero-capacity-depot label_color=native_engine_label_argument value_color=0xFF000000 text_runs=separate native_text_measurement=%d offset_x=%d offset_y=%d logical_px remaining_decimal_places=2 capacity_whole_kg=1)",
         vehicleTankDisplayActive ? "active" :
         (g_sandVehicleTankDisplayEnabled ? "unavailable" : "disabled"),
         g_vehicleTankLabelTextId,
         g_vehicleTankNoMaterialTextId,
         g_vehicleTankDryPlowingTextId,
         g_vehicleTankDepotTextId,
         g_nativeVehicleCalcTextRect ? 1 : 0,
         g_sandVehicleTankDisplayOffsetX,
         g_sandVehicleTankDisplayOffsetY);
    Info("Depot material priorities: %s (label_localization_id=%d off_localization_id=%d values=1..N_or_OFF uniqueness=swap selection=lowest_enabled_number persistence=%s collapse_and_demolition_delete=1 rebuilt_defaults=1 fuel_excluded=1 native_title_alignment=row_top_minus_22_from_verified_phase23_title45 divider=%d divider_height=%d)",
         depotMaterialPriorityUiActive ? "active" :
         (g_depotMaterialPriorityEnabled ? "unavailable" : "disabled"),
         g_depotPriorityLabelTextId,
         g_depotPriorityOffTextId,
         persistenceHooksActive &&
             g_depotMaterialPriorityPersistenceEnabled ?
                 "save-folder-sidecar" : "session-only",
         g_depotMaterialDividerEnabled,
         g_depotMaterialDividerHeight);
    Info("Depot material status warnings: %s (resource_localization_id=%d empty_localization_id=%d off_localization_id=%d one_line_per_present_configured_material=1 priority_OFF_precedes_empty=1 fuel_excluded=1 native_utf16_buffer=rbp+0x250 pre_measurement_append=1 native_font_wrapping_spacing_and_layout=1)",
         depotMaterialWarningUiActive ? "active" :
         (g_depotMaterialWarningsEnabled ? "unavailable" : "disabled"),
         g_depotWarningResourceTextId,
         g_depotWarningEmptyTextId,
         g_depotWarningOffTextId);
    Info("Technical Services native per-resource discard buttons: %s (single_resource_rows_only=1 native_full_row_mode_arg11=-1 shared_visibility_gate_overridden_only_inside_technical_services=1 other_panels_original_gate=1 native_icon=1 native_tooltip_id=0x873 native_dynamic_step=1 native_click=1 fuel_included=1 empty_row_button=0 storage_wide_clear_arg15=0 notification_checkbox_arg16_unchanged=1 plugin_fallback_control=0)",
         nativeResourceDiscardActive ? "active" : "unavailable");
    Info("Supported runtime classes: COVERED=%d OPEN=%d GRAVEL=%d OIL=%d",
         RESOURCE_TRANSPORT_COVERED,
         RESOURCE_TRANSPORT_OPEN,
         RESOURCE_TRANSPORT_GRAVEL,
         RESOURCE_TRANSPORT_OIL);
    Info("UI spacing: resource_gap=%d logical pixel(s), scaled by the game's current DPI factor",
         g_resourceGap);
    bool gritReady = g_sandDiagnosticEnabled && clearHooksActive &&
        InterlockedCompareExchange(&g_sandSkillLayoutVerified,0,0);
    Info("Feature status: material_catalog=%s storage_migration=%s grit_clearing=%s vehicle_tanks=%s road_treatment_service=%s native_return=%s return_observer=%s priority_save=%s tank_save=%s hook_health=%s",
        g_materialCatalogValid ? "valid" : "invalid-safe-native-only",
        FeatureStatus(g_storageMigrationEnabled != 0,storageMigrationActive),
        FeatureStatus(g_sandDiagnosticEnabled != 0,gritReady),
        FeatureStatus(g_sandShadowTankEnabled != 0,gritReady),
        FeatureStatus(g_sandDiagnosticEnabled && g_sandShadowTankEnabled,gritReady && g_roadTreatmentServiceActive),
        FeatureStatus(g_sandAutomaticReturnEnabled && g_sandReturnLockEnabled,gritReady),
        FeatureStatus(g_sandDiagnosticEnabled != 0,returnObserverActive),
        FeatureStatus(g_depotMaterialPriorityPersistenceEnabled != 0,persistenceHooksActive),
        FeatureStatus(g_sandTankPersistenceEnabled != 0,persistenceHooksActive),
        g_hookHealthWarnings ? "degraded-see-warnings" : "ok");
    bool degraded = !g_materialCatalogValid || g_hookHealthWarnings ||
        !nativeResourceDiscardActive ||
        (g_storageMigrationEnabled && !storageMigrationActive) ||
        (g_sandDiagnosticEnabled && (!gritReady || !returnObserverActive)) ||
        (g_sandDiagnosticEnabled && g_sandShadowTankEnabled && !g_roadTreatmentServiceActive) ||
        (configuredOfficeWrap && (!officePriorityWrapActive || !officePriorityLayoutActive)) ||
        (g_sandVehicleTankDisplayEnabled && !vehicleTankDisplayActive) ||
        (g_depotMaterialPriorityEnabled && !depotMaterialPriorityUiActive) ||
        (g_depotMaterialWarningsEnabled && !depotMaterialWarningUiActive) ||
        ((g_depotMaterialPriorityPersistenceEnabled || g_sandTankPersistenceEnabled) && !persistenceHooksActive);
    Info("v%s ready%s",PLUGIN_VERSION,degraded ? " with restrictions; see feature status and warnings" : "");
    LogSummaryStatus("Startup",degraded ? "completed with restrictions" : "completed successfully");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH && g_detail != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_detail);
        g_detail = INVALID_HANDLE_VALUE;
    }
    return TRUE;
}
