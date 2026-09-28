// weather_roads - road snow, melting and material-aware protection.
// Beta 0.2.9 retains verified weather/road/terrain hooks and save sidecars.
// Discovery breakpoints, memcpy/memset observers and unverified fade reads
// are removed. Detailed event logging is opt-in; real warnings always remain.

#include "../../src/tesmio_plugin.h"
#include "../grit_spreader_api.h"

#include <errno.h>
#include <float.h>
#include <intrin.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define PLUGIN_NAME       "weather_roads"
#define PLUGIN_VERSION    "0.3.3"
#define PLUGIN_INI        "plugins\\weather_roads.ini"
#define PLUGIN_LOG        "tesmioloader.weather_roads.log"

#define SYM_EDIT_MASK     "?EditMask@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@HMMHH_N@Z"
#define SYM_MASK_TEXTURE_OPEN "?MaskTextureOpen@C3D_TERRAIN@@QEAAXXZ"
#define SYM_MASK_TEXTURE_CLOSE "?MaskTextureClose@C3D_TERRAIN@@QEAAXXZ"

#define EXPECTED_TIMESTAMP  0x6A3EB6ADu
#define EXPECTED_IMAGE_SIZE 0x00A9D000u
#define EXPECTED_ENGINE_TIMESTAMP  0x6A3E75BCu
#define EXPECTED_ENGINE_IMAGE_SIZE 0x001F2000u

// C3DAPI_D3D11_TEXTURE::TextureAccesSetTexel is the confirmed virtual writer
// at vtable slot 0xB8. EditMask calls it from RVA 0xFBC83 and returns at
// 0xFBC8A. The 15-byte prologue below ends exactly on an instruction boundary.
#define RVA_ENGINE_TEXTURE_SET_TEXEL 0x19100
#define RVA_ENGINE_EDIT_MASK_SET_TEXEL_RETURN 0xFBC8A
#define RVA_ENGINE_TEXTURE_ACCESS_OPEN 0x18D40
#define RVA_ENGINE_TEXTURE_ACCESS_OPEN2 0x18E80
#define RVA_ENGINE_TEXTURE_ACCESS_CLOSE 0x18FC0
#define RVA_ENGINE_TEXTURE_VTABLE 0x187BF0
#define TEXTURE_VTABLE_SLOT_ACCESS_OPEN 16
#define TEXTURE_VTABLE_SLOT_ACCESS_OPEN2 17
#define TEXTURE_VTABLE_SLOT_ACCESS_CLOSE 18

static const BYTE kEngineTextureSetTexel[] = {
    0x4C, 0x8B, 0xD9,
    0x41, 0x8B, 0xC1,
    0x8B, 0x89, 0x60, 0x01, 0x00, 0x00,
    0xC1, 0xE8, 0x10
};

// Normal per-update caller of the weather tick.  The complete overwritten
// instruction sequence is:
//
//   xor edx,edx
//   mov rcx,r14            ; the real weather/world object
//   call 0x333BD0          ; weather tick (possibly detoured by daynight.dll)
//   xor edx,edx
//   mov rcx,r14
//
// The generated stub captures r14, executes the weather call through its
// public entry (therefore preserving any daynight hook), restores the final
// rcx/edx setup and resumes at the following light-factor call.
#define RVA_WEATHER_CALL_SITE   0x30D9D8
#define RVA_WEATHER_TICK        0x333BD0
#define RVA_WEATHER_CALL_RESUME 0x30D9E7

static const BYTE kWeatherCallSite[] = {
    0x33, 0xD2,
    0x49, 0x8B, 0xCE,
    0xE8, 0xEE, 0x61, 0x02, 0x00,
    0x33, 0xD2,
    0x49, 0x8B, 0xCE
};

// Actual road-snow clearing routine.  The public entry at 0x6BC210 first
// refuses a null road in rdx.  Hooking the body at 0x6BC219 keeps that guard
// untouched.  The displaced 17-byte prologue is position independent and is
// therefore safe in the loader's trampoline.
#define RVA_ROAD_SNOW_CLEAR_BODY 0x6BC219

static const BYTE kRoadSnowClearBody[] = {
    0x48, 0x8B, 0xC4,
    0x55,
    0x53,
    0x56,
    0x48, 0x8D, 0x68, 0xA1,
    0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00
};

// Confirmed global internal road-snow adjustment function.  It iterates every
// road snow vector and performs clamp(old + delta, 0, 255). Positive weather
// calls pass +30; verified negative callers pass -30 or -255 and must remain
// untouched by the accumulation experiment.
#define RVA_ROAD_SNOW_ADJUST 0x42AF60

// The renderer calls this verified world-mask update once per requested mask
// refresh. Weather writes 0x0101 to world + 0x5E4 immediately before its +30
// road-snow calls; the render caller clears +0x5E4 before entering this
// function and +0x5E5 after it returns. Hooking the complete function lets the
// protected plow texels be restored after the GPU snowfall command and before
// the caller can present the completed frame.
#define RVA_GLOBAL_MASK_UPDATE 0x40E7E0

static const BYTE kGlobalMaskUpdate[] = {
    0x48, 0x8B, 0xC4,
    0x48, 0x89, 0x58, 0x08,
    0x57,
    0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00
};

// Both verified positive +30 weather branches return at one of these two
// addresses.  Only these callers may use the gradual pre-mask queue; other
// positive callers retain the existing immediate, scaled behaviour.
#define RVA_WEATHER_SNOW_ADJUST_RETURN_1 0x5D0807
#define RVA_WEATHER_SNOW_ADJUST_RETURN_2 0x5D08F5

static const BYTE kRoadSnowAdjust[] = {
    0x48, 0x89, 0x5C, 0x24, 0x10,
    0x57,
    0x4C, 0x8B, 0x81, 0xA0, 0xF7, 0x00, 0x00,
    0x33, 0xDB
};

// The clearing routine contains four loops that execute one of these exact
// zero stores.  Each 14-byte sequence is position independent and ends on an
// instruction boundary, matching the loader's absolute-jump requirement.
#define RVA_ROAD_SNOW_WRITE_1 0x6BC2A0
#define RVA_ROAD_SNOW_WRITE_2 0x6BC370
#define RVA_ROAD_SNOW_WRITE_3 0x6BC440
#define RVA_ROAD_SNOW_WRITE_4 0x6BC4E0

// The queued terrain edits are flushed through the executable's EditMask IAT
// call at 0x40EE3B.  A direct call returns here.  The parameter signature is
// also checked, so a hook chained through another plugin remains observable.
#define RVA_EDIT_MASK_FLUSH_RETURN 0x40EE41

// Every verified executable caller of GetMaskPixelOverPoint loads the terrain
// through this global game-world pointer and then world + 0xED8.  The regular
// weather/update object captured at RVA_WEATHER_CALL_SITE is a different,
// larger object and must not be used as the owner of OFF_TERRAIN.
#define RVA_GLOBAL_WORLD_PTR 0x9941F0

static const BYTE kRoadSnowWriteRdi[] = {
    0x48, 0x8B, 0x83, 0xE8, 0x00, 0x00, 0x00,
    0xC6, 0x04, 0x07, 0x00,
    0x48, 0x85, 0xFF
};

static const BYTE kRoadSnowWriteRsi[] = {
    0x48, 0x8B, 0x83, 0xE8, 0x00, 0x00, 0x00,
    0xC6, 0x04, 0x06, 0x00,
    0x48, 0x85, 0xF6
};

// World object fields confirmed by the game's weather/light functions.
#define OFF_DAY           0x590
#define OFF_YEAR          0x594
#define OFF_DAYTIME       0x59C
#define OFF_MASK_DIRTY_REQUEST 0x5E4
#define OFF_SNOW_MASK_REQUEST  0x5E5
#define OFF_WEATHER_ON    0x5C4
#define OFF_PRECIPITATION_STATE 0xE28
// +0xE28 is the winter weather roll (exe+0x5D06B8..0x5D08FA, build 1.1.1.9):
// when the period timer at +0xE2C expires the game rolls rand % 3 on climate 3
// and rand % 8 on every other climate, so the field holds 0..7. Only the
// value 1 means snowfall (the +30 road-snow ticks and the snow flags at
// +0x5E4 run only then); 0 and 2..7 are all "no snow". Before 0.3.2 the
// plugin accepted 0..2 only and refused the snapshot 5 out of 8 rolls.
#define OFF_WEATHER       0xE70
#define OFF_NIGHT         0xE74
#define OFF_TERRAIN       0xED8
#define OFF_TERRAIN_MASK_TEXTURE 0x158
#define MIN_TERRAIN_READ_SIZE 0x89C

// Fields used directly by GetMaskPixelOverPoint and the verified D3D11
// texture access functions.  They are read only after both PE images and the
// surrounding objects have passed their safety checks.
#define OFF_TERRAIN_SIZE_X 0x87C
#define OFF_TERRAIN_SIZE_Z 0x880
#define OFF_TERRAIN_ORIGIN_X 0x890
#define OFF_TERRAIN_ORIGIN_Z 0x898
#define OFF_TEXTURE_WIDTH 0x14
#define OFF_TEXTURE_HEIGHT 0x18
#define OFF_TEXTURE_RESOURCE 0x130
#define OFF_TEXTURE_DATA 0x158
#define OFF_TEXTURE_ROW_PITCH 0x160
#define OFF_TEXTURE_ACCESS_RESOURCE 0x168
#define OFF_TEXTURE_ACCESS_OPEN 0x170
#define MIN_TEXTURE_READ_SIZE 0x178

// Road snow-vector fields used directly by the real clearing function.
#define OFF_ROAD_SNOW_BEG 0xE8
#define OFF_ROAD_SNOW_END 0xF0

struct WeatherRoadsConfig
{
    int enabled;
    int weatherProbe;
    int snowplowProbe;
    int experimentalRecoverageLimiter;
    int maximumGreenDecreasePerSecond;
    int internalSnowLimiter;
    double snowAccumulationMultiplier;
    int maximumSnowAccumulationPerBurst;
    int snowBurstResetAfterMs;
    int gradualSnowAccumulation;
    int releaseFollowsWeather;    // 0.3.1: drop the queued snow once the weather roll is no longer 1 (snowing)
    int gradualSnowStepUnits;
    int gradualSnowStepIntervalMs;
    int gradualVisualBatchUnits;
    int naturalMelting;
    double snowReductionMultiplier;
    int visualSnowLevels;
    double visualShaderRange;
    double visualCurve;
    int plowEffect;
    double plowProtectionMinutes;
    double plowSaltHours;
    double plowSaltAccumulationMultiplier;
    int dryPlowingPreservesTreatment;
    int overlayEnabled;
    int overlayUpdateIntervalMs;
    int overlayOffsetX;
    int overlayOffsetY;
    int overlayWidth;
    int overlayOpacity;
    int overlayToggleKey;
    int overlayForegroundOnly;
    int mirrorEvents;
    int detailedEvents;
    int sampleIntervalMs;
    int weatherHeartbeatSeconds;
    int roadHeartbeatSeconds;
    int roadStateSampleIntervalMs;
    int roadStateHeartbeatSeconds;
    int roadStateTrackingSeconds;
    int maximumTrackedRoads;
    int maximumRoadBytes;
    int maximumTrackedPlowPoints;
    int maskBatchIntervalMs;
    int maskSampleIntervalMs;
    int maskSampleHeartbeatSeconds;
    int maximumTrackedMaskPoints;
};

static WeatherRoadsConfig g_cfg = {
    1, 1, 1,
    0, 12, 1, 0.35, 95, 1000, 1, 1, 1, 125, 10,   // 0.3.3: multiplier 0.35, burst cap 95 (was 0.30 / 50);
                                                  // the second 1 is releaseFollowsWeather (0.3.1 added the
                                                  // field without this entry and shifted every later default)
    1, 0.45, 0, 0.85, 1.00,
    1, 120.0, 12.0, 0.50, 1,
    1, 250, 20, 80, 350, 220, VK_F10, 0,
    0, 0,
    500, 60,
    30, 500, 30, 900, 16,
    16384, 8192,
    5000, 2000, 30, 8
};
static int g_protectionPersistenceEnabled = 1;

// Converts the plugin's logical 0..255 snow amount into the smaller input
// range that the game's existing terrain shader actually uses visibly.  The
// table keeps pow() out of the per-texel path.
static BYTE g_visualSnowShaderAmount[256];

#define MAX_TRACKED_MASK_POINTS 16
#define MAX_TRACKED_ROADS 32
#define MAX_RECENT_MASK_TEXTURE_CALLS 128
#define MAX_RECENT_TEXTURE_ACCESS_CALLS 256
#define MAX_DEEP_MASK_CALLERS 16
#define MAX_PLOW_EFFECT_POINTS 8192
#define PLOW_EFFECT_BUCKET_SIZE 4
#define PLOW_EFFECT_MASK_WINDOW_MS 2000ULL
#define MAX_PENDING_VISIBLE_BRUSHES 512
#define MAX_CAPTURED_VISIBLE_TEXELS_PER_BRUSH 512
#define MAX_VISIBLE_BRUSH_TRACKS 128
#define VISIBLE_TRACK_BASE_RADIUS_METRES 12.0f
#define VISIBLE_TRACK_RADIUS_PER_MS 0.008f
#define GRIT_FACTOR_MILLION 1000000L
#define MAX_PENDING_PLOW_POINTS 1024
#define MASK_CHANGE_EPSILON 0.0025f

// Capture every texel touched by one verified brush. The world position forms
// a spatially continuous trajectory that v0.2.6 can bind across the confirmed
// render-thread/vehicle-thread boundary without mixing nearby vehicles.
struct VisibleBrushCapture
{
    int active;
    int positionValid;
    float position[3];
    void* texture;
    void* resource;
    LONG generation;
    DWORD texelCount;
    DWORD truncatedTexels;
    DWORD texelIndices[MAX_CAPTURED_VISIBLE_TEXELS_PER_BRUSH];
    LONG priorStrengthMillion[MAX_CAPTURED_VISIBLE_TEXELS_PER_BRUSH];
    LONG64 priorClearMinuteMilli[MAX_CAPTURED_VISIBLE_TEXELS_PER_BRUSH];
};
#define COORDINATED_MASK_POINT_MINIMUM 2
#define PLOW_VISUAL_APPLY_REGULAR 0
#define PLOW_VISUAL_APPLY_PRE_UPLOAD 1
#define PLOW_VISUAL_APPLY_EVENT_SYNC 2
#define PLOW_VISUAL_APPLY_INLINE_SYNC 3
#define PLOW_VISUAL_MARGIN_TEXELS 1
#define PLOW_VISUAL_LINK_MAX_TEXEL_DISTANCE 4
#define PLOW_VISUAL_LINK_MAX_AGE_MS 2000ULL
#define PLOW_VISUAL_TEXEL_CORE 0
#define PLOW_VISUAL_TEXEL_MARGIN 1
#define PLOW_VISUAL_TEXEL_GAP 2
#define PLOW_VISUAL_EVENT_PASSES 1
#define PLOW_VISUAL_EVENT_DELAY_MS 16ULL
#define PLOW_VISUAL_EVENT_COOLDOWN_MS 16ULL
#define PLOW_VISUAL_MAX_ACTIVE_TEXELS (1024u * 1024u)
#define MAX_GRADUAL_SNOW_PENDING_UNITS 255

struct MaskColor
{
    float value[4];
};

struct TrackedMaskPoint
{
    int active;
    unsigned id;
    float pos[3];
    ULONGLONG firstPaintTick;
    ULONGLONG lastPaintTick;
    ULONGLONG lastSampleTick;
    ULONGLONG lastLogTick;
    unsigned paints;
    unsigned loggedPaints;
    int hasSample;
    int hasLoggedValue;
    MaskColor lastSample;
    MaskColor lastLoggedValue;
    int hasRawSample;
    void* lastRawTexture;
    void* lastRawData;
    int lastRawX;
    int lastRawY;
    BYTE lastRawGreen[4];
    ULONGLONG lastRawSampleTick;
    LONG64 lastPaintGameMinuteMilli;
    float clearReferenceGreen;
    int hasClearReference;
    int visualOnlySnowLogged;
};

struct MaskCallBatch
{
    ULONGLONG firstTick;
    ULONGLONG lastTick;
    unsigned plowCalls;
    unsigned verifiedPlowCalls;
    unsigned chainedPlowCalls;
    unsigned otherChannel2Calls;
    int hasBounds;
    float minimum[3];
    float maximum[3];
    int otherDeltaMin;
    int otherDeltaMax;
    float otherInnerMin;
    float otherInnerMax;
    float otherOuterMin;
    float otherOuterMax;
    DWORD lastOtherCallerRva;
    int lastOtherLimit;
    int lastOtherOn;
};

struct MaskTextureCallObservation
{
    ULONGLONG tick;
    void* terrain;
    DWORD callerRva;
    char operation;
};

struct TextureStorageState
{
    int valid;
    void* texture;
    void* data;
    void* resource;
    void* accessResource;
    int width;
    int height;
    unsigned rowPitch;
    unsigned accessOpen;
};

static bool WeatherPersistenceConsumeReset();
static bool WeatherPersistenceSuspended();
static bool WeatherPersistenceVisualPending();
static void WeatherPersistenceRestoreVisualLocked(const TextureStorageState& storage);
static void WeatherPersistenceTick(void* world);

struct RawMaskSample
{
    int valid;
    void* texture;
    void* data;
    int x;
    int y;
    BYTE green[4];
};

struct TextureAccessObservation
{
    ULONGLONG tick;
    void* texture;
    void* dataBefore;
    void* dataAfter;
    DWORD callerRva;
    char callerModule;
    char operation;
    int result;
    unsigned rawBeforePoints;
    unsigned rawAfterPoints;
    unsigned rawBeforeSum;
    unsigned rawAfterSum;
};

struct TextureAccessRawState
{
    int valid;
    unsigned points;
    unsigned greenSum;
};

struct DeepMaskCallerCounter
{
    volatile LONG callerRva;
    volatile LONG64 writes;
    volatile LONG64 decreases;
    volatile LONG64 increases;
    volatile LONG64 zeroed;
    volatile LONG64 unreadableBefore;
};

struct DeepMaskCallerPrevious
{
    LONG64 writes;
    LONG64 decreases;
    LONG64 increases;
    LONG64 zeroed;
    LONG64 unreadableBefore;
};

struct DeepMaskWriteDelta
{
    LONG64 writes;
    LONG64 decreases;
    LONG64 increases;
    LONG64 zeroed;
    LONG64 unreadableBefore;
    LONG64 outsideEditMask;
    unsigned callers;
    char callerSummary[1536];
};

static HANDLE g_detail = INVALID_HANDLE_VALUE;
static volatile LONG g_initReady;
static volatile LONG g_logWarnings, g_logErrors, g_logFatals;
static volatile LONG g_detailWriteFailed;
// Reporting only: never used to enable/disable weather or change timing.
static volatile LONG g_overlayState; // 0=disabled, 1=starting, 2=active, -1=failed, 3=stopped
struct LogPhase { ULONGLONG started; LONG warnings, errors, fatals; };
static LogPhase g_logPhase = {};
static HANDLE g_stopEvent;
static HANDLE g_worker;
static HMODULE g_module;
static HANDLE g_overlayStopEvent;
static HANDLE g_overlayThread;
static HWND g_overlayWindow;
static HFONT g_overlayFont;
static volatile LONG g_overlayUserVisible = 1;
static volatile LONG g_overlayWeatherValid;
static volatile LONG g_overlayPrecipitationState = -1;
static volatile LONG g_overlayTrackedRoads;
static volatile LONG64 g_overlayTrackedBytes;
static volatile LONG64 g_overlayTrackedSnowSum;
static volatile LONG g_overlayTrackedSnowMaximum;
static volatile LONG64 g_overlayVisualSnowSum;
static volatile LONG64 g_overlayVisualSnowCount;
static volatile LONG g_overlayVisualSnowMaximum;
static volatile LONG g_overlayLastCorrectionMs = -1;
static volatile LONG64 g_overlayLastDuplicateSnowWrites;
static volatile LONG g_overlaySnowBurstForwarded;
static volatile LONG g_overlaySnowBurstCapped;
static volatile LONG g_overlaySnowDeltaValid;
static volatile LONG g_overlaySnowDeltaRaw;
static volatile LONG g_overlaySnowDeltaEffective;
static char g_overlayLanguage[16] = "auto";
static char g_overlayPosition[16] = "top_right";
static volatile LONG g_active;
static volatile LONG g_faultLogged;
static volatile LONG g_invalidWeatherLogged;
static volatile LONG g_invalidRoadLogged;
static void* volatile g_weatherWorld;
static volatile LONG64 g_weatherCaptureTick;
static volatile LONG64 g_weatherCaptureCount;
static volatile LONG g_weatherThreadId;
static volatile LONG64 g_snowplowClearCalls;
static volatile LONG64 g_snowplowCompleteSamples;
static volatile LONG64 g_snowplowRejectedSamples;
static volatile LONG64 g_snowplowStatusTick;
static const TsmGritSpreaderApi* g_gritSpreaderApi;
static volatile LONG64 g_gritTreatmentMatches;
static volatile LONG64 g_gritTreatmentMaterialEvents;
static volatile LONG64 g_gritTreatmentDryEvents;
static volatile LONG64 g_gritTreatmentFallbackEvents;
static volatile LONG64 g_gritVisibleTreatmentMatches;
static volatile LONG64 g_gritVisibleTreatmentFallbacks;
static volatile LONG64 g_gritVisibleTreatmentClearMatches;
static volatile LONG64 g_gritVisibleTreatmentTexels;
static volatile LONG64 g_visibleCorrelationClearSamples;
static volatile LONG64 g_visibleCorrelationClearsWithCandidates;
static volatile LONG64 g_visibleCorrelationPrecedingCandidates;
static volatile LONG64 g_visibleCorrelationPrecedingSameThread;
static volatile LONG64 g_visibleCorrelationPrecedingCrossThread;
static volatile LONG64 g_visibleCorrelationIdentityMatches;
static volatile LONG64 g_visibleCorrelationIdentityMismatches;
static volatile LONG64 g_visibleCorrelationPositionValid;
static volatile LONG64 g_visibleCorrelationBrushesAfterClear;
static volatile LONG64 g_visibleCorrelationAfterClearSameThread;
static volatile LONG64 g_visibleCorrelationAfterClearCrossThread;
static volatile LONG64 g_visibleCorrelationBrushSequence;
static volatile LONG g_visibleCorrelationClearDetailLogs;
static volatile LONG g_visibleCorrelationBrushDetailLogs;
static volatile LONG64 g_visibleTrackNextId;
static volatile LONG64 g_visibleTracksCreated;
static volatile LONG64 g_visibleTracksBound;
static volatile LONG64 g_visibleTracksRebound;
static volatile LONG64 g_visibleTrackClearMatches;
static volatile LONG64 g_visibleTrackProvisionalBrushes;
static volatile LONG64 g_visibleTrackFinalizedBrushes;
static volatile LONG64 g_visibleTrackCorrectedBrushes;
static volatile LONG64 g_visibleTrackUnownedBrushes;
static volatile LONG64 g_visibleTrackExpiredBrushes;
static volatile LONG g_visibleTrackDetailLogs;
static volatile LONG64 g_gritDryInternalPointsCleared;
static volatile LONG64 g_gritDryVisualTexelsCleared;
static volatile LONG64 g_gritDryInternalPointsPreserved;
static volatile LONG64 g_gritDryVisualTexelsPreserved;
static volatile LONG64 g_gritLowerInternalPointsPreserved;
static volatile LONG64 g_gritLowerVisualTexelsPreserved;
static volatile LONG64 g_plowMaskCalls;
static volatile LONG64 g_lastVerifiedPlowMaskTick;
static void* g_weatherCallTrampoline;
typedef void (*t_RoadSnowClearBody)(void* vehicle, void* road, void* selector);
static t_RoadSnowClearBody o_RoadSnowClearBody;
typedef void (*t_RoadSnowAdjust)(void* world, int delta);
static t_RoadSnowAdjust o_RoadSnowAdjust;
typedef void (*t_GlobalMaskUpdate)(void* world);
static t_GlobalMaskUpdate o_GlobalMaskUpdate;
typedef void (*t_EditMask)(void* terrain, float* pos, int channel,
                           float innerR, float outerR, int delta,
                           int limit, char on);
static t_EditMask o_EditMask;
typedef void (*t_MaskTextureAccess)(void* terrain);
static t_MaskTextureAccess o_MaskTextureOpen;
static t_MaskTextureAccess o_MaskTextureClose;
typedef void (*t_TextureSetTexel)(void* texture, int x, int y, DWORD value);
static t_TextureSetTexel o_TextureSetTexel;
typedef void (*t_TextureAccess)(void* texture);
static t_TextureAccess o_TextureAccessOpen;
static t_TextureAccess o_TextureAccessClose;
typedef bool (*t_TextureAccessOpen2)(void* texture);
static t_TextureAccessOpen2 o_TextureAccessOpen2;
static DWORD g_clearTls = TLS_OUT_OF_INDEXES;
static void* g_roadWriteTrampolines[4];
static CRITICAL_SECTION g_maskDataLock;
static volatile LONG g_maskDataLockReady;
static TrackedMaskPoint g_maskPoint[MAX_TRACKED_MASK_POINTS];
static unsigned g_nextMaskPointId = 1;
static MaskCallBatch g_maskBatch;
static ULONGLONG g_nextMaskBatchTick;
static ULONGLONG g_nextMaskSampleTick;
static ULONGLONG g_lastMaskPointSampleTick;
static MaskTextureCallObservation
    g_recentMaskTextureCalls[MAX_RECENT_MASK_TEXTURE_CALLS];
static unsigned g_nextMaskTextureCall;
static unsigned g_maskTextureCallCount;
static TextureAccessObservation
    g_recentTextureAccessCalls[MAX_RECENT_TEXTURE_ACCESS_CALLS];
static unsigned g_nextTextureAccessCall;
static unsigned g_textureAccessCallCount;
static void* volatile g_terrainMaskTexture;
static TextureStorageState g_previousMaskStorage;
static int g_hasPreviousMaskStorage;
static SIZE_T g_engineSize;
static DeepMaskCallerCounter g_deepMaskCaller[MAX_DEEP_MASK_CALLERS];
static DeepMaskCallerPrevious g_deepMaskPrevious[MAX_DEEP_MASK_CALLERS];
static volatile LONG64 g_deepMaskCallerOverflow;
static LONG64 g_deepMaskCallerOverflowPrevious;
static BYTE* g_limiterGreenShadow;
static SIZE_T g_limiterGreenShadowBytes;
static void* g_limiterShadowTexture;
static void* g_limiterShadowResource;
static int g_limiterShadowWidth;
static int g_limiterShadowHeight;
static unsigned g_limiterShadowRowPitch;
static ULONGLONG g_limiterLastTick;
static double g_limiterDecreaseBudget;
static volatile LONG g_limiterBusy;
static volatile LONG g_limiterAllocationFailed;
static volatile LONG g_limiterFaultLogged;
static volatile LONG64 g_limiterShadowResets;
static volatile LONG64 g_limiterDecreaseBatches;
static volatile LONG64 g_limiterRequestedPixels;
static volatile LONG64 g_limiterLimitedPixels;
static volatile LONG64 g_limiterPreventedUnits;
static volatile LONG64 g_limiterAllowedUnits;
static volatile LONG64 g_limiterEnforcementPasses;
static volatile LONG64 g_limiterOpenFailures;
static ULONGLONG g_nextLimiterStatusTick;

// Visible counterpart of the per-road plow after-effect. The green shadow
// stores the last value that was allowed to reach the terrain mask. A game-
// time stamp is assigned only to texels written by the verified snowplow
// EditMask call, so unrelated terrain and resource brushes are never touched.
static BYTE* g_plowVisualGreenShadow;
static BYTE* g_plowVisualClearGreen;
static BYTE* g_plowVisualSnowAmount;
static LONG* g_plowVisualStrengthMillion;
static DWORD* g_plowVisualSnowBurstGeneration;
static BYTE* g_plowVisualSnowBurstAppliedUnits;
static LONG64* g_plowVisualClearMinuteMilli;
static DWORD* g_plowVisualActiveIndices;
static SIZE_T g_plowVisualCapacity;
static SIZE_T g_plowVisualActiveCapacity;
static SIZE_T g_plowVisualActiveCount;
static volatile LONG64 g_plowVisualActivePublished;
static void* g_plowVisualTexture;
static void* g_plowVisualResource;
static int g_plowVisualWidth;
static int g_plowVisualHeight;
static unsigned g_plowVisualRowPitch;
static LONG g_plowVisualGeneration;
static volatile LONG g_plowVisualEnabled;
static volatile LONG g_plowVisualBusy;
static volatile LONG g_plowVisualAllocationFailed;
static volatile LONG g_plowVisualFaultLogged;
static volatile LONG64 g_plowVisualShadowResets;
static volatile LONG64 g_plowVisualMarked;
static volatile LONG64 g_plowVisualRefreshed;
static volatile LONG64 g_plowVisualCoreTexels;
static volatile LONG64 g_plowVisualMarginTexels;
static volatile LONG64 g_plowVisualGapTexels;
static volatile LONG64 g_plowVisualGapLinks;
static volatile LONG64 g_plowVisualSkippedGapLinks;
static volatile LONG64 g_plowVisualDroppedActiveTexels;
static volatile LONG64 g_plowVisualOnlySnowEvents;
static volatile LONG64 g_plowVisualProtectionPixels;
static volatile LONG64 g_plowVisualProtectedUnits;
static volatile LONG64 g_plowVisualSaltPixels;
static volatile LONG64 g_plowVisualSaltPreventedUnits;
static volatile LONG64 g_plowVisualDuplicateSnowWrites;
static volatile LONG64 g_plowVisualBurstLimitedPixels;
static volatile LONG64 g_plowVisualBurstPreventedUnits;
static volatile LONG64 g_plowVisualStagePixels;
static volatile LONG64 g_plowVisualMeltPixels;
static volatile LONG64 g_plowVisualMeltUnits;
static volatile LONG64 g_plowVisualPendingMeltUnits;
static volatile LONG64 g_plowVisualExpired;
static volatile LONG64 g_plowVisualEnforcementPasses;
static volatile LONG64 g_plowVisualOpenFailures;
static volatile LONG64 g_plowVisualPreUploadProtectionPixels;
static volatile LONG64 g_plowVisualPreUploadSaltPixels;
static volatile LONG64 g_plowVisualEventRequests;
static volatile LONG64 g_plowVisualEventPasses;
static volatile LONG64 g_plowVisualEventProtectionPixels;
static volatile LONG64 g_plowVisualEventSaltPixels;
static volatile LONG64 g_plowVisualEventCoalesced;
static volatile LONG64 g_plowVisualEventSkippedInactive;
static volatile LONG64 g_plowVisualInlinePasses;
static volatile LONG64 g_plowVisualInlineCorrectedPixels;
static volatile LONG64 g_plowVisualEventNextTick;
static volatile LONG64 g_plowVisualEventStartTick;
static volatile LONG64 g_plowVisualEventCooldownUntilTick;
static volatile LONG64 g_plowVisualEventFirstCorrectionMs = -1;
static volatile LONG64 g_plowVisualEnforcementElapsedMs;
static volatile LONG64 g_plowVisualEnforcementMaximumMs;
static volatile LONG g_plowVisualEventPassesRemaining;
static volatile LONG g_plowVisualEventPassIndex;
static volatile LONG g_plowVisualEventBurstGeneration;
static volatile LONG g_plowVisualEventHasSnow;
static SRWLOCK g_plowVisualLogicalBurstLock = SRWLOCK_INIT;
static ULONGLONG g_plowVisualLogicalBurstLastTick;
static volatile LONG g_plowVisualLogicalBurstGeneration;
static volatile LONG g_plowVisualLogicalBurstReleasedUnits;
static void* g_plowVisualLastCoreTexture;
static LONG g_plowVisualLastCoreGeneration;
static int g_plowVisualLastCoreX;
static int g_plowVisualLastCoreY;
static ULONGLONG g_plowVisualLastCoreTick;
static int g_plowVisualLastCoreValid;
static ULONGLONG g_nextPlowVisualStatusTick;
static __declspec(thread) int g_verifiedPlowEditMaskDepth;
static __declspec(thread) LONG
    g_verifiedPlowEditMaskStrengthMillion = GRIT_FACTOR_MILLION;
static __declspec(thread) int g_verifiedPlowEditMaskTreatmentDeferred;
static __declspec(thread) VisibleBrushCapture g_visibleBrushCapture = {};
static ULONGLONG g_nextRoadStateSampleTick;
static ULONGLONG g_nextRoadStateHeartbeatTick;
static unsigned g_nextTrackedRoadId = 1;

struct TrackedRoadSnow
{
    volatile LONG active;
    unsigned id;
    void* road;
    BYTE* begin;
    SIZE_T length;
    BYTE* snapshot;
    uint64_t sum;
    unsigned minimum;
    unsigned maximum;
    uint32_t hash;
    LONG64 lastClearCall;
    LONG treatmentStrengthMillion;
    int treatmentMaterialIndex;
    unsigned treatmentFlags;
    ULONGLONG registeredTick;
    ULONGLONG lastSeenTick;
    ULONGLONG lastSampleTick;
};

static TrackedRoadSnow g_trackedRoads[MAX_TRACKED_ROADS];
static void SampleTrackedRoadSnow(ULONGLONG now);
static void FlushInternalSnowLimiterStatus(void);
static void FlushNaturalMeltingStatus(void);
static void FlushPlowEffectStatus(void);
static LONG64 PlowProtectionDuration(void);
static LONG64 PlowSaltDuration(void);
static LONG64 BlendGritPhaseFactor(LONG64 legacyFactor,
                                   LONG strengthMillion);
static void FlushPlowVisualStatus(ULONGLONG now);

static volatile LONG64 g_snowAccumulationFactorMillion;
static volatile LONG64 g_snowAccumulationRemainder;
static volatile LONG64 g_snowReductionFactorMillion;
static volatile LONG64 g_snowReductionRemainder;
static volatile LONG64 g_plowSaltFactorMillion;
static volatile LONG64 g_internalSnowCalls;
static volatile LONG64 g_internalSnowInputUnits;
static volatile LONG64 g_internalSnowForwardedUnits;
static volatile LONG64 g_internalSnowZeroCalls;
static volatile LONG g_internalSnowLastInput;
static volatile LONG g_internalSnowLastForwarded;
static volatile LONG g_internalSnowLastCallerRva;
static LONG64 g_internalSnowLoggedCalls;
static LONG64 g_internalSnowLoggedInput;
static LONG64 g_internalSnowLoggedForwarded;
static LONG64 g_internalSnowLoggedZeroCalls;
static SRWLOCK g_internalSnowBurstLock = SRWLOCK_INIT;
static ULONGLONG g_internalSnowBurstLastCallTick;
static int g_internalSnowBurstForwarded;
static int g_internalSnowBurstCapped;
static volatile LONG64 g_internalSnowBurstCappedUnits;
static LONG64 g_internalSnowLoggedBurstCappedUnits;
static ULONGLONG g_nextInternalSnowStatusTick;
static void ResetInternalSnowBurstState(void);
static volatile LONG g_gradualSnowPendingUnits;
static volatile LONG64 g_gradualSnowQueuedUnits;
static volatile LONG64 g_gradualSnowReleasedUnits;
static volatile LONG64 g_gradualSnowCancelledUnits;
static volatile LONG64 g_gradualSnowDroppedUnits;
static volatile LONG64 g_gradualSnowNextStepTick;
static volatile LONG64 g_gradualSnowLastGameMinuteMilli;
static volatile LONG g_gradualVisualPendingUnits;
static volatile LONG64 g_gradualVisualBatches;
static volatile LONG64 g_gradualVisualReleasedUnits;
static volatile LONG64 g_gradualMaskFlagsSuppressed;
static volatile LONG64 g_gradualMaskFlagsReplayed;
static volatile LONG64 g_gradualMaskFlagsUnexpected;
static volatile LONG64 g_globalMaskUpdateCalls;
static volatile LONG64 g_globalSnowMaskUpdateCalls;
static volatile LONG64 g_globalMaskInlinePasses;
static volatile LONG64 g_globalMaskInlineFailures;
static volatile LONG g_globalMaskLastRequestWord;
static volatile LONG g_globalMaskUpdateBusy;
static LONG64 g_internalSnowLoggedGradualReleased;
static LONG64 g_internalSnowLoggedMaskFlagsSuppressed;
static LONG64 g_internalSnowLoggedMaskFlagsReplayed;
static LONG64 g_internalSnowLoggedMaskFlagsUnexpected;
static LONG64 g_internalSnowLoggedGlobalSnowMaskCalls;
static LONG64 g_internalSnowLoggedGlobalMaskInlinePasses;
static LONG64 g_internalSnowLoggedGlobalMaskInlineFailures;
static LONG64 g_internalSnowLoggedVisualBatches;
static LONG64 g_internalSnowLoggedVisualReleasedUnits;
static void ResetGradualSnowState(void);
static void ServiceGradualRoadSnow(void* world, ULONGLONG now);
static volatile LONG64 g_naturalMeltCalls;
static volatile LONG64 g_naturalMeltInputUnits;
static volatile LONG64 g_naturalMeltForwardedUnits;
static volatile LONG64 g_naturalMeltZeroCalls;
static volatile LONG g_naturalMeltLastInput;
static volatile LONG g_naturalMeltLastForwarded;
static LONG64 g_naturalMeltLoggedCalls;
static LONG64 g_naturalMeltLoggedInput;
static LONG64 g_naturalMeltLoggedForwarded;
static LONG64 g_naturalMeltLoggedZeroCalls;
static ULONGLONG g_nextNaturalMeltStatusTick;
static volatile LONG64 g_gameMinuteMilli;
static volatile LONG g_gameWorldGeneration;
static CRITICAL_SECTION g_plowEffectLock;
static volatile LONG g_plowEffectLockReady;
static volatile LONG g_exactRoadWriteSitesActive;

struct PlowEffectPoint
{
    int active;
    LONG generation;
    void* road;
    BYTE* vectorBegin;
    SIZE_T vectorLength;
    SIZE_T index;
    LONG64 clearedGameMinuteMilli;
    LONG64 saltRemainderMillion;
    LONG treatmentStrengthMillion;
    int treatmentMaterialIndex;
    unsigned treatmentFlags;
    LONG64 sampledClearTime;
    BYTE sampledBefore;
    BYTE sampledPhase;
};

struct PendingPlowPoint
{
    int active;
    void* road;
    BYTE* vectorBegin;
    SIZE_T vectorLength;
    SIZE_T index;
    ULONGLONG detectedTick;
    LONG treatmentStrengthMillion;
    int treatmentMaterialIndex;
    unsigned treatmentFlags;
};

struct PendingVisibleBrush
{
    int active;
    LONG64 sequence;
    LONG64 trackId;
    DWORD threadId;
    ULONGLONG tick;
    int positionValid;
    float position[3];
    void* texture;
    void* resource;
    LONG generation;
    DWORD texelCount;
    DWORD truncatedTexels;
    int treatmentApplied;
    LONG appliedStrengthMillion;
    unsigned appliedTreatmentSequence;
    DWORD texelIndices[MAX_CAPTURED_VISIBLE_TEXELS_PER_BRUSH];
    LONG priorStrengthMillion[MAX_CAPTURED_VISIBLE_TEXELS_PER_BRUSH];
    LONG64 priorClearMinuteMilli[MAX_CAPTURED_VISIBLE_TEXELS_PER_BRUSH];
};

struct VisibleBrushTrack
{
    int active;
    LONG64 id;
    void* ownerVehicle;
    void* texture;
    void* resource;
    LONG generation;
    ULONGLONG previousTick;
    ULONGLONG lastTick;
    float previousPosition[3];
    float lastPosition[3];
    int treatmentValid;
    LONG treatmentStrengthMillion;
    int treatmentMaterialIndex;
    unsigned treatmentFlags;
    unsigned treatmentSequence;
    LONG64 lastClearCall;
    ULONGLONG lastClearTick;
};

struct RecentVisibleClear
{
    int valid;
    LONG64 call;
    DWORD threadId;
    ULONGLONG tick;
    void* vehicle;
    void* road;
    void* selector;
    LONG treatmentStrengthMillion;
    int treatmentMaterialIndex;
    unsigned treatmentFlags;
    unsigned treatmentSequence;
};

static PlowEffectPoint g_plowEffectPoints[MAX_PLOW_EFFECT_POINTS];
static PendingPlowPoint g_pendingPlowPoints[MAX_PENDING_PLOW_POINTS];
static PendingVisibleBrush
    g_pendingVisibleBrushes[MAX_PENDING_VISIBLE_BRUSHES];
static VisibleBrushTrack g_visibleBrushTracks[MAX_VISIBLE_BRUSH_TRACKS];
static RecentVisibleClear g_recentVisibleClear;
static SRWLOCK g_pendingVisibleBrushLock = SRWLOCK_INIT;
static volatile LONG64 g_plowEffectRegistered;
static volatile LONG64 g_plowEffectRefreshed;
static volatile LONG64 g_plowEffectDecreaseCandidates;
static volatile LONG64 g_plowEffectInitialZeroCandidates;
static volatile LONG64 g_plowEffectRejectedNoRecentMask;
static volatile LONG64 g_plowEffectExpired;
static volatile LONG64 g_plowEffectEvicted;
static volatile LONG64 g_plowEffectProtectionWrites;
static volatile LONG64 g_plowEffectSaltWrites;
static volatile LONG64 g_plowEffectProtectionUnits;
static volatile LONG64 g_plowEffectSaltPreventedUnits;
static LONG64 g_plowEffectLoggedRegistered;
static LONG64 g_plowEffectLoggedDecreaseCandidates;
static LONG64 g_plowEffectLoggedInitialZeroCandidates;
static LONG64 g_plowEffectLoggedRejectedNoRecentMask;
static LONG64 g_plowEffectLoggedProtectionWrites;
static LONG64 g_plowEffectLoggedSaltWrites;
static LONG64 g_gritLoggedTreatmentMatches;
static LONG64 g_gritLoggedTreatmentMaterialEvents;
static LONG64 g_gritLoggedTreatmentDryEvents;
static LONG64 g_gritLoggedTreatmentFallbackEvents;
static ULONGLONG g_nextPlowEffectStatusTick;

struct WeatherSnapshot
{
    int valid;
    int day;
    int year;
    int weatherMode;
    int precipitationState;
    int weather;
    float time;
    float night;
};

// ---------------------------------------------------------------- logging

static DWORD DetailWriteRaw(const char* text)
{
    if (g_detail == INVALID_HANDLE_VALUE || !text ||
        InterlockedCompareExchange(&g_detailWriteFailed,0,0)) return ERROR_SUCCESS;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[4096];
    _snprintf_s(line,sizeof(line),_TRUNCATE,
        "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\r\n",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,text);
    // Caller holds the existing detail-log lock throughout this whole line.
    DWORD length = (DWORD)strlen(line), offset = 0;
    while (offset < length) {
        DWORD put = 0;
        if (!WriteFile(g_detail,line+offset,length-offset,&put,NULL))
            return GetLastError();
        if (!put || put > length-offset) return ERROR_WRITE_FAULT;
        offset += put;
    }
    return ERROR_SUCCESS;
}

static void ProbeLogV(int mirror, const char* level, const char* fmt, va_list ap)
{
    char body[3072];
    _vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, ap);
    char line[3584];
    _snprintf_s(line, sizeof(line), _TRUNCATE, "%s  %s", level, body);
    if (!strcmp(level,"WARN")) InterlockedIncrement(&g_logWarnings);
    else if (!strcmp(level,"ERROR")) InterlockedIncrement(&g_logErrors);
    else if (!strcmp(level,"FATAL")) InterlockedIncrement(&g_logFatals);

    EnterCriticalSection(&g_lock);
    DWORD writeError = DetailWriteRaw(line);
    bool reportWriteError = writeError &&
        InterlockedCompareExchange(&g_detailWriteFailed,1,0) == 0;
    LeaveCriticalSection(&g_lock);

    // No recursive detail logging, and never call the host under the file lock.
    if (reportWriteError) {
        InterlockedIncrement(&g_logWarnings);
        if (H && H->log) Logf(PLUGIN_NAME
            "  WARN: logging [detail-write] Windows error %lu; detail output stopped; main log and weather remain active. Action: check disk space and permissions",
            writeError);
    }
    if (mirror && H && H->log)
        Logf(PLUGIN_NAME "  %s", line);
}


static void Info(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ProbeLogV(1, "INFO", fmt, ap);
    va_end(ap);
}

static void Event(const char* fmt, ...)
{
    if (!g_cfg.detailedEvents) return;
    va_list ap;
    va_start(ap, fmt);
    ProbeLogV(g_cfg.mirrorEvents, "EVENT", fmt, ap);
    va_end(ap);
}

static void Warn(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ProbeLogV(1, "WARN", fmt, ap);
    va_end(ap);
}

static void ReportWindows(const char* area, const char* rule,
                          const char* action, DWORD error, const char* remedy)
{
    char description[512] = {};
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,NULL,error,0,description,
        (DWORD)sizeof(description),NULL);
    while (n && (description[n-1]=='\r' || description[n-1]=='\n' ||
                 description[n-1]==' ')) description[--n]=0;
    Warn("%s [%s] %s (Windows error %lu: %s). Action: %s",
        area,rule,action,error,n ? description : "unknown",remedy);
}

static void BeginLogPhase()
{
    g_logPhase.started = GetTickCount64();
    g_logPhase.warnings = InterlockedCompareExchange(&g_logWarnings,0,0);
    g_logPhase.errors = InterlockedCompareExchange(&g_logErrors,0,0);
    g_logPhase.fatals = InterlockedCompareExchange(&g_logFatals,0,0);
}
static void EndLogPhase(const char* phase, const char* result)
{
    Info("%s [%s] duration_ms=%llu warnings=%ld errors=%ld fatal=%ld (this phase)",
        phase,result,(unsigned long long)(GetTickCount64()-g_logPhase.started),
        InterlockedCompareExchange(&g_logWarnings,0,0)-g_logPhase.warnings,
        InterlockedCompareExchange(&g_logErrors,0,0)-g_logPhase.errors,
        InterlockedCompareExchange(&g_logFatals,0,0)-g_logPhase.fatals);
}
static void Error(const char* fmt, ...)
{
    va_list ap; va_start(ap,fmt);
    ProbeLogV(1,"ERROR",fmt,ap);
    va_end(ap);
}
static void StartError(const char* reason)
{
    Error("Startup [failed] %s",reason);
    EndLogPhase("Startup","failed");
}
static bool WorkerWaitContinues(DWORD result, const char* worker)
{
    if (result == WAIT_FAILED) {
        DWORD error = GetLastError();
        ReportWindows(worker,"wait-failed","Wait operation failed; this worker exits",
            error,"Restart the game and inspect the preceding errors");
        return false;
    }
    return result != WAIT_OBJECT_0;
}
static const char* OverlayStatus()
{
    LONG state = InterlockedCompareExchange(&g_overlayState,0,0);
    return state == 1 ? "starting" : state == 2 ? "active" :
           state == -1 ? "failed" : state == 3 ? "stopped" : "disabled";
}

// ------------------------------------------------------------- configuration

static bool ParseIntStrict(const char* text, int* out)
{
    if (!text || !text[0] || !out) return false;
    errno = 0;
    char* end = NULL;
    long value = strtol(text, &end, 10);
    while (end && (*end == ' ' || *end == '\t' || *end == '\r')) end++;
    if (errno == ERANGE || end == text || (end && *end)) return false;
    if (value < INT_MIN || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool ParseDoubleStrict(const char* text, double* out)
{
    if (!text || !text[0] || !out) return false;
    errno = 0;
    char* end = NULL;
    double value = strtod(text, &end);
    while (end && (*end == ' ' || *end == '\t' || *end == '\r')) end++;
    if (errno == ERANGE || end == text || (end && *end) || !_finite(value))
        return false;
    *out = value;
    return true;
}

// ---------------------------------------------------------------------------
// Configuration file: <baseDir>\plugins\weather_roads.ini when it
// exists (classic installation, or the effective INI Republic Mod Manager
// writes), otherwise weather_roads.ini beside this DLL (Workshop package under
// Soviet Mod Loader or the Workshop Bridge). Values are read exactly the way
// the loader's configString reads them - GetPrivateProfileStringA plus the
// same trim - so an installed plugins\weather_roads.ini behaves as before.
// ---------------------------------------------------------------------------
static char g_iniPath[MAX_PATH];

static void TrimConfigValue(char* s)
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

static void ResolveConfigPath()
{
    _snprintf_s(g_iniPath, sizeof(g_iniPath), _TRUNCATE, "%s\\%s",
                g_baseDir ? g_baseDir : "", PLUGIN_INI);
    if (GetFileAttributesA(g_iniPath) != INVALID_FILE_ATTRIBUTES) return;
    HMODULE self = NULL;
    char own[MAX_PATH];
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)&ResolveConfigPath, &self) ||
        !GetModuleFileNameA(self, own, (DWORD)sizeof(own)))
        return;
    char* slash = strrchr(own, '\\');
    if (!slash) return;
    slash[1] = 0;
    char candidate[MAX_PATH];
    _snprintf_s(candidate, sizeof(candidate), _TRUNCATE,
                "%sweather_roads.ini", own);
    if (GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
        strncpy_s(g_iniPath, sizeof(g_iniPath), candidate, _TRUNCATE);
}

static void ConfigString(const char* section, const char* key,
                         char* out, int outSize, const char* fallback)
{
    GetPrivateProfileStringA(section, key, fallback ? fallback : "",
                             out, (DWORD)outSize, g_iniPath);
    TrimConfigValue(out);
}

static int ReadInt(const char* section, const char* key,
                   int fallback, int lo, int hi)
{
    char fallbackText[32];
    char text[64];
    _snprintf_s(fallbackText, sizeof(fallbackText), _TRUNCATE,
                "%d", fallback);
    ConfigString(section, key,
                    text, (int)sizeof(text), fallbackText);

    int value = 0;
    if (!ParseIntStrict(text, &value) || value < lo || value > hi)
    {
        Warn("invalid [%s] %s=\"%s\"; using %d (allowed %d..%d)",
             section, key, text, fallback, lo, hi);
        return fallback;
    }
    return value;
}

static double ReadDouble(const char* section, const char* key,
                          double fallback, double lo, double hi)
{
    char fallbackText[64];
    char text[128];
    _snprintf_s(fallbackText, sizeof(fallbackText), _TRUNCATE,
                "%.6f", fallback);
    ConfigString(section, key,
                    text, (int)sizeof(text), fallbackText);

    double value = 0.0;
    if (!ParseDoubleStrict(text, &value) || value < lo || value > hi)
    {
        Warn("invalid [%s] %s=\"%s\"; using %.6f (allowed %.2f..%.2f)",
             section, key, text, fallback, lo, hi);
        return fallback;
    }
    return value;
}

static void BuildVisualSnowShaderTable()
{
    for (unsigned i = 0; i < 256; ++i)
    {
        double logical = (double)i / 255.0;
        double curved = i == 0 ? 0.0 : pow(logical, g_cfg.visualCurve);
        double shaderAmount = curved * g_cfg.visualShaderRange;
        if (shaderAmount < 0.0) shaderAmount = 0.0;
        if (shaderAmount > 1.0) shaderAmount = 1.0;
        g_visualSnowShaderAmount[i] =
            (BYTE)(shaderAmount * 255.0 + 0.5);
    }
}

static void ReadSettings()
{
    ResolveConfigPath();
    Info("configuration file: %s", g_iniPath);
    g_cfg.enabled = ReadInt("general", "enabled", g_cfg.enabled, 0, 1);
    // Production builds always keep the verified weather and snowplow
    // boundaries available. The invasive writer and hardware-breakpoint
    // research probes are intentionally never enabled here.
    g_cfg.weatherProbe = 1;
    g_cfg.snowplowProbe = 1;
    g_cfg.experimentalRecoverageLimiter = 0;
    g_cfg.internalSnowLimiter =
        ReadInt("snow", "enabled",
                g_cfg.internalSnowLimiter, 0, 1);
    g_cfg.snowAccumulationMultiplier =
        ReadDouble("snow", "accumulation_multiplier",
                   g_cfg.snowAccumulationMultiplier, 0.0, 10.0);
    g_cfg.maximumSnowAccumulationPerBurst =
        ReadInt("snow",
                "maximum_accumulation_per_burst",
                g_cfg.maximumSnowAccumulationPerBurst, 0, 255);
    g_cfg.snowBurstResetAfterMs =
        ReadInt("advanced", "burst_reset_after_ms",
                g_cfg.snowBurstResetAfterMs, 100, 10000);
    g_cfg.gradualSnowAccumulation =
        ReadInt("snow", "gradual_accumulation",
                g_cfg.gradualSnowAccumulation, 0, 1);
    g_cfg.releaseFollowsWeather =
        ReadInt("snow", "release_follows_weather", 1, 0, 1);
    g_cfg.gradualSnowStepUnits =
        ReadInt("advanced", "gradual_step_units",
                g_cfg.gradualSnowStepUnits, 1, 32);
    g_cfg.gradualSnowStepIntervalMs =
        ReadInt("advanced", "gradual_step_interval_ms",
                g_cfg.gradualSnowStepIntervalMs, 16, 5000);
    g_cfg.gradualVisualBatchUnits =
        ReadInt("advanced", "visual_update_batch_units",
                g_cfg.gradualVisualBatchUnits, 1, 255);
    InterlockedExchange64(
        &g_snowAccumulationFactorMillion,
        (LONG64)(g_cfg.snowAccumulationMultiplier * 1000000.0 + 0.5));
    InterlockedExchange64(&g_snowAccumulationRemainder, 0);
    ResetInternalSnowBurstState();
    ResetGradualSnowState();
    g_cfg.naturalMelting =
        ReadInt("melting", "enabled", g_cfg.naturalMelting, 0, 1);
    g_cfg.snowReductionMultiplier =
        ReadDouble("melting", "reduction_multiplier",
                   g_cfg.snowReductionMultiplier, 0.0, 10.0);
    InterlockedExchange64(
        &g_snowReductionFactorMillion,
        (LONG64)(g_cfg.snowReductionMultiplier * 1000000.0 + 0.5));
    InterlockedExchange64(&g_snowReductionRemainder, 0);
    g_cfg.visualSnowLevels =
        ReadInt("visual_snow", "snow_levels", g_cfg.visualSnowLevels, 0, 8);
    g_cfg.visualShaderRange =
        ReadDouble("visual_snow", "shader_range",
                   g_cfg.visualShaderRange, 0.0, 1.0);
    g_cfg.visualCurve =
        ReadDouble("visual_snow", "visual_curve",
                   g_cfg.visualCurve, 0.10, 5.0);
    BuildVisualSnowShaderTable();
    g_cfg.plowEffect =
        ReadInt("snowplow", "enabled", g_cfg.plowEffect, 0, 1);
    g_cfg.plowProtectionMinutes =
        ReadDouble("snowplow", "protection_minutes",
                   g_cfg.plowProtectionMinutes, 0.0, 1440.0);
    g_cfg.plowSaltHours =
        ReadDouble("snowplow", "salt_effect_hours",
                   g_cfg.plowSaltHours, 0.0, 168.0);
    g_cfg.plowSaltAccumulationMultiplier =
        ReadDouble("snowplow", "salt_accumulation_multiplier",
                   g_cfg.plowSaltAccumulationMultiplier, 0.0, 1.0);
    g_cfg.dryPlowingPreservesTreatment =
        ReadInt("snowplow", "dry_plowing_preserves_treatment",
                g_cfg.dryPlowingPreservesTreatment, 0, 1);
    g_protectionPersistenceEnabled =
        ReadInt("persistence", "enabled", 1, 0, 1);
    InterlockedExchange64(
        &g_plowSaltFactorMillion,
        (LONG64)(g_cfg.plowSaltAccumulationMultiplier * 1000000.0 + 0.5));
    g_cfg.overlayEnabled = ReadInt("overlay", "enabled",
                                   g_cfg.overlayEnabled, 0, 1);
    g_cfg.overlayUpdateIntervalMs =
        ReadInt("overlay", "update_interval_ms",
                g_cfg.overlayUpdateIntervalMs, 100, 2000);
    g_cfg.overlayOffsetX = ReadInt("overlay", "offset_x",
                                   g_cfg.overlayOffsetX, 0, 4000);
    g_cfg.overlayOffsetY = ReadInt("overlay", "offset_y",
                                   g_cfg.overlayOffsetY, 0, 4000);
    g_cfg.overlayWidth = ReadInt("overlay", "window_width",
                                 g_cfg.overlayWidth, 280, 600);
    g_cfg.overlayOpacity = ReadInt("overlay", "opacity",
                                   g_cfg.overlayOpacity, 80, 255);
    g_cfg.overlayToggleKey = ReadInt("overlay", "toggle_key",
                                     g_cfg.overlayToggleKey, 0, 255);
    g_cfg.overlayForegroundOnly =
        ReadInt("overlay", "foreground_only",
                g_cfg.overlayForegroundOnly, 0, 1);
    ConfigString("overlay", "language",
                    g_overlayLanguage, (int)sizeof(g_overlayLanguage), "auto");
    if (_stricmp(g_overlayLanguage, "auto") &&
        _stricmp(g_overlayLanguage, "de") &&
        _stricmp(g_overlayLanguage, "en"))
    {
        Warn("invalid [overlay] language=\"%s\"; using auto "
             "(allowed auto, de, en)", g_overlayLanguage);
        strncpy_s(g_overlayLanguage, sizeof(g_overlayLanguage),
                  "auto", _TRUNCATE);
    }
    ConfigString("overlay", "position",
                    g_overlayPosition, (int)sizeof(g_overlayPosition),
                    "top_right");
    if (_stricmp(g_overlayPosition, "top_right") &&
        _stricmp(g_overlayPosition, "top_left"))
    {
        Warn("invalid [overlay] position=\"%s\"; using top_right "
             "(allowed top_right, top_left)", g_overlayPosition);
        strncpy_s(g_overlayPosition, sizeof(g_overlayPosition),
                  "top_right", _TRUNCATE);
    }
    g_cfg.mirrorEvents = ReadInt("logging", "mirror_events_to_loader_log",
                                 g_cfg.mirrorEvents, 0, 1);
    g_cfg.detailedEvents = ReadInt("logging", "detailed_events",
                                   g_cfg.detailedEvents, 0, 1);
    g_cfg.sampleIntervalMs = ReadInt("logging", "weather_sample_interval_ms",
                                     g_cfg.sampleIntervalMs, 100, 60000);
    g_cfg.weatherHeartbeatSeconds = ReadInt("logging", "weather_heartbeat_seconds",
                                            g_cfg.weatherHeartbeatSeconds, 0, 3600);
    g_cfg.roadHeartbeatSeconds = ReadInt("logging", "road_heartbeat_seconds",
                                         g_cfg.roadHeartbeatSeconds, 0, 3600);
    g_cfg.roadStateSampleIntervalMs =
        ReadInt("logging", "road_state_sample_interval_ms",
                g_cfg.roadStateSampleIntervalMs, 50, 60000);
    g_cfg.roadStateHeartbeatSeconds =
        ReadInt("logging", "road_state_heartbeat_seconds",
                g_cfg.roadStateHeartbeatSeconds, 0, 3600);
    g_cfg.roadStateTrackingSeconds =
        ReadInt("safety", "road_state_tracking_seconds",
                g_cfg.roadStateTrackingSeconds, 10, 86400);
    g_cfg.maximumTrackedRoads =
        ReadInt("safety", "maximum_tracked_roads",
                g_cfg.maximumTrackedRoads, 1, MAX_TRACKED_ROADS);
    g_cfg.maskBatchIntervalMs = ReadInt("logging", "mask_batch_interval_ms",
                                        g_cfg.maskBatchIntervalMs, 500, 60000);
    g_cfg.maskSampleIntervalMs = ReadInt("logging", "mask_sample_interval_ms",
                                         g_cfg.maskSampleIntervalMs, 250, 60000);
    g_cfg.maskSampleHeartbeatSeconds =
        ReadInt("logging", "mask_sample_heartbeat_seconds",
                g_cfg.maskSampleHeartbeatSeconds, 0, 3600);
    g_cfg.maximumRoadBytes = ReadInt("safety", "maximum_road_bytes",
                                     g_cfg.maximumRoadBytes, 16, 1048576);
    g_cfg.maximumTrackedPlowPoints =
        ReadInt("safety", "maximum_tracked_plow_points",
                g_cfg.maximumTrackedPlowPoints, 128,
                MAX_PLOW_EFFECT_POINTS);
    g_cfg.maximumTrackedMaskPoints =
        ReadInt("safety", "maximum_tracked_mask_points",
                g_cfg.maximumTrackedMaskPoints, 1, MAX_TRACKED_MASK_POINTS);
}

// --------------------------------------------------------------- PE identity

static bool VerifyExecutable()
{
    if (!g_exeBase || g_exeSize < sizeof(IMAGE_DOS_HEADER))
    {
        Warn("SOVIET64.exe image is unavailable; probe refused");
        return false;
    }

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)g_exeBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        (SIZE_T)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > g_exeSize)
    {
        Warn("SOVIET64.exe has invalid PE headers; probe refused");
        return false;
    }

    IMAGE_NT_HEADERS64* nt =
        (IMAGE_NT_HEADERS64*)(g_exeBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        Warn("SOVIET64.exe is not the expected 64-bit PE image; probe refused");
        return false;
    }

    DWORD timestamp = nt->FileHeader.TimeDateStamp;
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    if (timestamp != EXPECTED_TIMESTAMP || imageSize != EXPECTED_IMAGE_SIZE)
    {
        Warn("unsupported SOVIET64.exe: TimeDateStamp=0x%08X, "
             "SizeOfImage=0x%08X; expected WRSR 1.1.1.9 "
             "(0x%08X, 0x%08X). No worker or hook was installed",
             timestamp, imageSize, EXPECTED_TIMESTAMP, EXPECTED_IMAGE_SIZE);
        return false;
    }

    Info("executable verified: WRSR 1.1.1.9, TimeDateStamp=0x%08X, "
         "SizeOfImage=0x%08X", timestamp, imageSize);
    return true;
}

static bool VerifyEngine()
{
    BYTE* base = (BYTE*)g_engine;
    if (!base || !ReadablePtr(base, sizeof(IMAGE_DOS_HEADER)))
    {
        Warn("C3DDLL64.dll image is unavailable; deep mask-write probe refused");
        return false;
    }

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        !ReadablePtr(base + dos->e_lfanew, sizeof(IMAGE_NT_HEADERS64)))
    {
        Warn("C3DDLL64.dll has invalid PE headers; deep mask-write probe refused");
        return false;
    }

    IMAGE_NT_HEADERS64* nt =
        (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        Warn("C3DDLL64.dll is not the expected 64-bit image; deep mask-write "
             "probe refused");
        return false;
    }

    DWORD timestamp = nt->FileHeader.TimeDateStamp;
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    if (timestamp != EXPECTED_ENGINE_TIMESTAMP ||
        imageSize != EXPECTED_ENGINE_IMAGE_SIZE)
    {
        Warn("unsupported C3DDLL64.dll: TimeDateStamp=0x%08X, "
             "SizeOfImage=0x%08X; expected WRSR 1.1.1.9 engine "
             "(0x%08X, 0x%08X). No engine inline hook was installed",
             timestamp, imageSize, EXPECTED_ENGINE_TIMESTAMP,
             EXPECTED_ENGINE_IMAGE_SIZE);
        return false;
    }

    g_engineSize = imageSize;
    Info("engine verified: C3DDLL64.dll TimeDateStamp=0x%08X, "
         "SizeOfImage=0x%08X", timestamp, imageSize);
    return true;
}

// -------------------------------------------------------- terrain-mask state

static float AbsMaskFloat(float value)
{
    return value < 0.0f ? -value : value;
}

static bool ValidMaskColor(const MaskColor& color)
{
    for (unsigned i = 0; i < 4; ++i)
        if (!_finite(color.value[i]) || color.value[i] < -16.0f ||
            color.value[i] > 16.0f)
            return false;
    return true;
}

static void ExtendMaskBounds(MaskCallBatch& batch, const float pos[3])
{
    if (!batch.hasBounds)
    {
        for (unsigned i = 0; i < 3; ++i)
            batch.minimum[i] = batch.maximum[i] = pos[i];
        batch.hasBounds = 1;
        return;
    }
    for (unsigned i = 0; i < 3; ++i)
    {
        if (pos[i] < batch.minimum[i]) batch.minimum[i] = pos[i];
        if (pos[i] > batch.maximum[i]) batch.maximum[i] = pos[i];
    }
}

// Keep a small, spatially distributed set of points.  Nearby paints refresh
// an existing point instead of filling every slot with consecutive 4 m road
// samples.  A point may be replaced only after two minutes without a repaint.
static void RegisterTrackedMaskPointLocked(const float pos[3], ULONGLONG now)
{
    int match = -1;
    int freeSlot = -1;
    int staleSlot = -1;
    ULONGLONG stalestPaint = ~(ULONGLONG)0;
    bool farEnough = true;

    int limit = g_cfg.maximumTrackedMaskPoints;
    if (limit > MAX_TRACKED_MASK_POINTS) limit = MAX_TRACKED_MASK_POINTS;
    for (int i = 0; i < limit; ++i)
    {
        TrackedMaskPoint& point = g_maskPoint[i];
        if (!point.active)
        {
            if (freeSlot < 0) freeSlot = i;
            continue;
        }

        float dx = point.pos[0] - pos[0];
        float dy = point.pos[1] - pos[1];
        float dz = point.pos[2] - pos[2];
        float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared <= 4.0f)
        {
            match = i;
            break;
        }
        if (distanceSquared < 64.0f) farEnough = false;
        if (now - point.lastPaintTick >= 120000ULL &&
            point.lastPaintTick < stalestPaint)
        {
            staleSlot = i;
            stalestPaint = point.lastPaintTick;
        }
    }

    if (match >= 0)
    {
        TrackedMaskPoint& point = g_maskPoint[match];
        point.lastPaintTick = now;
        point.lastPaintGameMinuteMilli = InterlockedCompareExchange64(
            &g_gameMinuteMilli, 0, 0);
        point.paints++;
        return;
    }
    if (!farEnough) return;

    int slot = freeSlot >= 0 ? freeSlot : staleSlot;
    if (slot < 0) return;

    TrackedMaskPoint& point = g_maskPoint[slot];
    memset(&point, 0, sizeof(point));
    point.active = 1;
    point.id = g_nextMaskPointId++;
    point.pos[0] = pos[0];
    point.pos[1] = pos[1];
    point.pos[2] = pos[2];
    point.firstPaintTick = now;
    point.lastPaintTick = now;
    point.lastPaintGameMinuteMilli = InterlockedCompareExchange64(
        &g_gameMinuteMilli, 0, 0);
    point.paints = 1;
}

static void RecordEditMaskObservation(bool plowSignature, bool directFlush,
                                      bool positionValid, const float pos[3],
                                      int channel, float innerR, float outerR,
                                      int delta, int limit, char on,
                                      DWORD callerRva)
{
    if (!plowSignature && channel != 2) return;
    if (!InterlockedCompareExchange(&g_maskDataLockReady, 0, 0)) return;

    ULONGLONG now = GetTickCount64();
    EnterCriticalSection(&g_maskDataLock);
    MaskCallBatch& batch = g_maskBatch;
    if (!batch.firstTick) batch.firstTick = now;
    batch.lastTick = now;

    if (plowSignature)
    {
        batch.plowCalls++;
        if (directFlush) batch.verifiedPlowCalls++;
        else             batch.chainedPlowCalls++;
        if (positionValid)
        {
            ExtendMaskBounds(batch, pos);
            RegisterTrackedMaskPointLocked(pos, now);
        }
    }
    else if (channel == 2)
    {
        if (!batch.otherChannel2Calls)
        {
            batch.otherDeltaMin = batch.otherDeltaMax = delta;
            batch.otherInnerMin = batch.otherInnerMax = innerR;
            batch.otherOuterMin = batch.otherOuterMax = outerR;
        }
        else
        {
            if (delta < batch.otherDeltaMin) batch.otherDeltaMin = delta;
            if (delta > batch.otherDeltaMax) batch.otherDeltaMax = delta;
            if (innerR < batch.otherInnerMin) batch.otherInnerMin = innerR;
            if (innerR > batch.otherInnerMax) batch.otherInnerMax = innerR;
            if (outerR < batch.otherOuterMin) batch.otherOuterMin = outerR;
            if (outerR > batch.otherOuterMax) batch.otherOuterMax = outerR;
        }
        batch.otherChannel2Calls++;
        batch.lastOtherCallerRva = callerRva;
        batch.lastOtherLimit = limit;
        batch.lastOtherOn = (int)on;
    }
    LeaveCriticalSection(&g_maskDataLock);
}

static void FlushMaskCallBatch(ULONGLONG now)
{
    if (!InterlockedCompareExchange(&g_maskDataLockReady, 0, 0)) return;
    if (g_nextMaskBatchTick && now < g_nextMaskBatchTick) return;
    g_nextMaskBatchTick = now + (ULONGLONG)g_cfg.maskBatchIntervalMs;

    MaskCallBatch batch = {};
    EnterCriticalSection(&g_maskDataLock);
    if (g_maskBatch.plowCalls || g_maskBatch.otherChannel2Calls)
    {
        batch = g_maskBatch;
        memset(&g_maskBatch, 0, sizeof(g_maskBatch));
    }
    LeaveCriticalSection(&g_maskDataLock);

    if (batch.plowCalls)
    {
        ULONGLONG duration = batch.lastTick >= batch.firstTick
                           ? batch.lastTick - batch.firstTick : 0;
        if (batch.hasBounds)
        {
            Event("visible snow-mask batch: points=%u verified=%u chained=%u "
                  "duration_ms=%llu bounds=[%.3f..%.3f,%.3f..%.3f,%.3f..%.3f] "
                  "total_points=%lld",
                  batch.plowCalls, batch.verifiedPlowCalls,
                  batch.chainedPlowCalls, (unsigned long long)duration,
                  (double)batch.minimum[0], (double)batch.maximum[0],
                  (double)batch.minimum[1], (double)batch.maximum[1],
                  (double)batch.minimum[2], (double)batch.maximum[2],
                  (long long)InterlockedCompareExchange64(&g_plowMaskCalls, 0, 0));
        }
        else
        {
            Event("visible snow-mask batch: points=%u verified=%u chained=%u "
                  "duration_ms=%llu bounds=<unreadable> total_points=%lld",
                  batch.plowCalls, batch.verifiedPlowCalls,
                  batch.chainedPlowCalls, (unsigned long long)duration,
                  (long long)InterlockedCompareExchange64(&g_plowMaskCalls, 0, 0));
        }
    }

    if (batch.otherChannel2Calls)
    {
        Event("other channel-2 EditMask batch: calls=%u delta=%d..%d "
              "inner=%.3f..%.3f outer=%.3f..%.3f last_limit=%d "
              "last_on=%d last_caller_rva=%s0x%X",
              batch.otherChannel2Calls,
              batch.otherDeltaMin, batch.otherDeltaMax,
              (double)batch.otherInnerMin, (double)batch.otherInnerMax,
              (double)batch.otherOuterMin, (double)batch.otherOuterMax,
              batch.lastOtherLimit, batch.lastOtherOn,
              batch.lastOtherCallerRva == 0xFFFFFFFFu ? "external/" : "",
              batch.lastOtherCallerRva);
    }
}

static DWORD ExecutableCallerRva(void* caller)
{
    uintptr_t callerAddress = (uintptr_t)caller;
    uintptr_t imageBegin = (uintptr_t)g_exeBase;
    uintptr_t imageEnd = imageBegin + g_exeSize;
    return callerAddress >= imageBegin && callerAddress < imageEnd
         ? (DWORD)(callerAddress - imageBegin) : 0xFFFFFFFFu;
}

static DWORD EngineCallerRva(void* caller)
{
    uintptr_t callerAddress = (uintptr_t)caller;
    uintptr_t imageBegin = (uintptr_t)g_engine;
    uintptr_t imageEnd = imageBegin + g_engineSize;
    return g_engineSize && callerAddress >= imageBegin && callerAddress < imageEnd
         ? (DWORD)(callerAddress - imageBegin) : 0xFFFFFFFFu;
}

static char ResolveCallerModule(void* caller, DWORD* callerRva)
{
    if (!callerRva) return 'X';
    DWORD rva = ExecutableCallerRva(caller);
    if (rva != 0xFFFFFFFFu)
    {
        *callerRva = rva;
        return 'S';
    }
    rva = EngineCallerRva(caller);
    if (rva != 0xFFFFFFFFu)
    {
        *callerRva = rva;
        return 'E';
    }
    *callerRva = 0xFFFFFFFFu;
    return 'X';
}

static bool ReadTextureStorageState(void* texture,
                                    TextureStorageState* state)
{
    if (!state) return false;
    memset(state, 0, sizeof(*state));
    state->texture = texture;
    if (!texture || !ReadablePtr(texture, MIN_TEXTURE_READ_SIZE)) return false;

    __try
    {
        BYTE* object = (BYTE*)texture;
        state->width = *(int*)(object + OFF_TEXTURE_WIDTH);
        state->height = *(int*)(object + OFF_TEXTURE_HEIGHT);
        state->resource = *(void**)(object + OFF_TEXTURE_RESOURCE);
        state->data = *(void**)(object + OFF_TEXTURE_DATA);
        state->rowPitch = *(unsigned*)(object + OFF_TEXTURE_ROW_PITCH);
        state->accessResource =
            *(void**)(object + OFF_TEXTURE_ACCESS_RESOURCE);
        state->accessOpen = *(BYTE*)(object + OFF_TEXTURE_ACCESS_OPEN);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        memset(state, 0, sizeof(*state));
        state->texture = texture;
        return false;
    }

    if (state->width <= 1 || state->height <= 1 ||
        state->width > 32768 || state->height > 32768)
        return false;
    if (state->rowPitch &&
        (state->rowPitch < (unsigned)state->width * sizeof(DWORD) ||
         state->rowPitch > 0x40000000u))
        return false;
    state->valid = 1;
    return true;
}

static bool ReadRawMaskSample(void* terrain,
                              const TextureStorageState& storage,
                              const float pos[3], RawMaskSample* sample)
{
    if (!sample) return false;
    memset(sample, 0, sizeof(*sample));
    sample->texture = storage.texture;
    sample->data = storage.data;
    if (!terrain || !pos || !storage.valid || !storage.accessOpen ||
        !storage.data ||
        !storage.rowPitch || !ReadablePtr(terrain, MIN_TERRAIN_READ_SIZE))
        return false;

    float originX = 0.0f, originZ = 0.0f;
    float sizeX = 0.0f, sizeZ = 0.0f;
    __try
    {
        BYTE* object = (BYTE*)terrain;
        sizeX = *(float*)(object + OFF_TERRAIN_SIZE_X);
        sizeZ = *(float*)(object + OFF_TERRAIN_SIZE_Z);
        originX = *(float*)(object + OFF_TERRAIN_ORIGIN_X);
        originZ = *(float*)(object + OFF_TERRAIN_ORIGIN_Z);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    if (!_finite(sizeX) || !_finite(sizeZ) || !_finite(originX) ||
        !_finite(originZ) || sizeX <= 0.0f || sizeZ <= 0.0f)
        return false;

    float u = (pos[0] - originX) / sizeX;
    float v = (pos[2] - originZ) / sizeZ;
    if (!_finite(u) || !_finite(v) || u < 0.0f || v < 0.0f ||
        u >= 1.0f || v >= 1.0f)
        return false;

    int x0 = (int)(u * (float)storage.width);
    int y0 = (int)(v * (float)storage.height);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x0 >= storage.width) x0 = storage.width - 1;
    if (y0 >= storage.height) y0 = storage.height - 1;
    int x1 = x0 + 1 < storage.width ? x0 + 1 : x0;
    int y1 = y0 + 1 < storage.height ? y0 + 1 : y0;
    const int xs[4] = { x0, x1, x0, x1 };
    const int ys[4] = { y0, y0, y1, y1 };

    BYTE values[4] = {};
    for (unsigned i = 0; i < 4; ++i)
    {
        SIZE_T offset = (SIZE_T)ys[i] * storage.rowPitch +
                        (SIZE_T)xs[i] * sizeof(DWORD);
        BYTE* pixel = (BYTE*)storage.data + offset;
        if (!ReadablePtr(pixel, sizeof(DWORD))) return false;
        __try
        {
            // Red and blue are exchanged by SetTexel, but green stays in the
            // second byte in both representations.
            values[i] = pixel[1];
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    sample->valid = 1;
    sample->x = x0;
    sample->y = y0;
    memcpy(sample->green, values, sizeof(values));
    return true;
}

static unsigned RawGreenSum(const RawMaskSample& sample)
{
    return (unsigned)sample.green[0] + sample.green[1] +
           sample.green[2] + sample.green[3];
}

// Read only the four green bytes around each road position that is already
// tracked by the regular sampler.  This deliberately avoids hashing the whole
// 16 MiB mask in a sensitive texture-access hook.  The aggregate is sufficient
// to tell whether Open2 imported a changed buffer or whether the change happened
// later, between Open2 and Close.
static void ReadTrackedTextureAccessRawState(
    const TextureStorageState& storage, TextureAccessRawState* state)
{
    if (!g_cfg.detailedEvents) { if (state) memset(state, 0, sizeof(*state)); return; }
    if (!state) return;
    memset(state, 0, sizeof(*state));
    if (!storage.valid || !storage.texture || !storage.data ||
        !storage.rowPitch || storage.width <= 0 || storage.height <= 0 ||
        !InterlockedCompareExchange(&g_maskDataLockReady, 0, 0))
        return;

    struct Coordinate { int x; int y; };
    Coordinate coordinates[MAX_TRACKED_MASK_POINTS] = {};
    unsigned coordinateCount = 0;

    EnterCriticalSection(&g_maskDataLock);
    int limit = g_cfg.maximumTrackedMaskPoints;
    if (limit > MAX_TRACKED_MASK_POINTS) limit = MAX_TRACKED_MASK_POINTS;
    for (int i = 0; i < limit; ++i)
    {
        const TrackedMaskPoint& point = g_maskPoint[i];
        if (!point.active || !point.hasRawSample ||
            point.lastRawTexture != storage.texture)
            continue;
        coordinates[coordinateCount].x = point.lastRawX;
        coordinates[coordinateCount].y = point.lastRawY;
        coordinateCount++;
    }
    LeaveCriticalSection(&g_maskDataLock);

    unsigned total = 0;
    unsigned points = 0;
    for (unsigned i = 0; i < coordinateCount; ++i)
    {
        int x0 = coordinates[i].x;
        int y0 = coordinates[i].y;
        if (x0 < 0 || y0 < 0 || x0 >= storage.width ||
            y0 >= storage.height)
            continue;
        int x1 = x0 + 1 < storage.width ? x0 + 1 : x0;
        int y1 = y0 + 1 < storage.height ? y0 + 1 : y0;
        const int xs[4] = { x0, x1, x0, x1 };
        const int ys[4] = { y0, y0, y1, y1 };
        unsigned pointSum = 0;
        bool readable = true;
        for (unsigned p = 0; p < 4; ++p)
        {
            SIZE_T offset = (SIZE_T)ys[p] * storage.rowPitch +
                            (SIZE_T)xs[p] * sizeof(DWORD);
            BYTE* pixel = (BYTE*)storage.data + offset;
            if (!ReadablePtr(pixel, sizeof(DWORD)))
            {
                readable = false;
                break;
            }
            __try
            {
                pointSum += pixel[1];
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                readable = false;
            }
            if (!readable) break;
        }
        if (!readable) continue;
        total += pointSum;
        points++;
    }

    state->valid = points != 0;
    state->points = points;
    state->greenSum = total;
}

static bool CopyGreenChannelToLimiterShadow(
    const TextureStorageState& storage)
{
    if (!g_limiterGreenShadow || !storage.data || storage.width <= 0 ||
        storage.height <= 0 ||
        storage.rowPitch < (unsigned)storage.width * sizeof(DWORD))
        return false;

    __try
    {
        for (int y = 0; y < storage.height; ++y)
        {
            BYTE* row = (BYTE*)storage.data + (SIZE_T)y * storage.rowPitch;
            BYTE* shadow = g_limiterGreenShadow +
                           (SIZE_T)y * (SIZE_T)storage.width;
            if (!ReadablePtr(row, (SIZE_T)storage.width * sizeof(DWORD)))
                return false;
            for (int x = 0; x < storage.width; ++x)
                shadow[x] = row[(SIZE_T)x * sizeof(DWORD) + 1];
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return true;
}

static bool PrepareExperimentalLimiterShadow(
    const TextureStorageState& storage, ULONGLONG now)
{
    if (!storage.valid || !storage.texture || !storage.resource ||
        !storage.data || storage.width <= 0 || storage.height <= 0)
        return false;

    SIZE_T width = (SIZE_T)storage.width;
    SIZE_T height = (SIZE_T)storage.height;
    if (height > SIZE_MAX / width) return false;
    SIZE_T bytes = width * height;
    const SIZE_T maximumShadowBytes = 64u * 1024u * 1024u;
    if (!bytes || bytes > maximumShadowBytes) return false;

    bool identityChanged = g_limiterShadowTexture != storage.texture ||
        g_limiterShadowResource != storage.resource ||
        g_limiterShadowWidth != storage.width ||
        g_limiterShadowHeight != storage.height ||
        g_limiterShadowRowPitch != storage.rowPitch;
    if (!identityChanged && g_limiterGreenShadow &&
        g_limiterGreenShadowBytes >= bytes)
        return true;

    if (!g_limiterGreenShadow || g_limiterGreenShadowBytes < bytes)
    {
        BYTE* replacement = (BYTE*)VirtualAlloc(
            NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!replacement)
        {
            InterlockedExchange(&g_limiterAllocationFailed, 1);
            return false;
        }
        if (g_limiterGreenShadow)
            VirtualFree(g_limiterGreenShadow, 0, MEM_RELEASE);
        g_limiterGreenShadow = replacement;
        g_limiterGreenShadowBytes = bytes;
    }

    g_limiterShadowTexture = storage.texture;
    g_limiterShadowResource = storage.resource;
    g_limiterShadowWidth = storage.width;
    g_limiterShadowHeight = storage.height;
    g_limiterShadowRowPitch = storage.rowPitch;
    if (!CopyGreenChannelToLimiterShadow(storage))
    {
        g_limiterShadowTexture = NULL;
        g_limiterShadowResource = NULL;
        return false;
    }

    g_limiterLastTick = now;
    g_limiterDecreaseBudget = 0.0;
    InterlockedIncrement64(&g_limiterShadowResets);
    return false; // first pass establishes a baseline and changes nothing
}

// Rate-limit only decreases in the green channel. Increases, including every
// snowplow write, pass through unchanged. The shadow contains one byte per
// texel, so the comparison adds 4 MiB for the verified 2048x2048 mask instead
// of duplicating the complete BGRA texture.
static void ApplyExperimentalRecoverageLimiter(
    const TextureStorageState& storage)
{
    if (!g_cfg.experimentalRecoverageLimiter ||
        InterlockedCompareExchange(&g_limiterAllocationFailed, 0, 0))
        return;
    if (InterlockedCompareExchange(&g_limiterBusy, 1, 0) != 0)
        return;

    ULONGLONG now = GetTickCount64();
    if (!PrepareExperimentalLimiterShadow(storage, now))
    {
        InterlockedExchange(&g_limiterBusy, 0);
        return;
    }

    ULONGLONG elapsed = now >= g_limiterLastTick
                      ? now - g_limiterLastTick : 0;
    if (elapsed > 1000ULL) elapsed = 1000ULL;
    g_limiterLastTick = now;
    double rate = (double)g_cfg.maximumGreenDecreasePerSecond;
    g_limiterDecreaseBudget += rate * (double)elapsed / 1000.0;
    if (g_limiterDecreaseBudget > rate)
        g_limiterDecreaseBudget = rate;
    unsigned allowedDrop = (unsigned)g_limiterDecreaseBudget;
    if (allowedDrop > 255u) allowedDrop = 255u;

    LONG64 requestedPixels = 0;
    LONG64 limitedPixels = 0;
    LONG64 preventedUnits = 0;
    LONG64 appliedUnits = 0;
    unsigned maximumAppliedDrop = 0;
    bool scanValid = true;

    __try
    {
        for (int y = 0; y < storage.height; ++y)
        {
            BYTE* row = (BYTE*)storage.data + (SIZE_T)y * storage.rowPitch;
            BYTE* shadow = g_limiterGreenShadow +
                           (SIZE_T)y * (SIZE_T)storage.width;
            if (!ReadablePtr(row, (SIZE_T)storage.width * sizeof(DWORD)))
            {
                scanValid = false;
                __leave;
            }
            for (int x = 0; x < storage.width; ++x)
            {
                BYTE* green = row + (SIZE_T)x * sizeof(DWORD) + 1;
                unsigned previous = shadow[x];
                unsigned current = *green;
                unsigned finalValue = current;
                if (current < previous)
                {
                    requestedPixels++;
                    unsigned minimumValue = previous > allowedDrop
                                          ? previous - allowedDrop : 0;
                    if (finalValue < minimumValue)
                    {
                        finalValue = minimumValue;
                        *green = (BYTE)finalValue;
                        limitedPixels++;
                        preventedUnits += (LONG64)(finalValue - current);
                    }
                    unsigned appliedDrop = previous - finalValue;
                    appliedUnits += appliedDrop;
                    if (appliedDrop > maximumAppliedDrop)
                        maximumAppliedDrop = appliedDrop;
                }
                shadow[x] = (BYTE)finalValue;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        scanValid = false;
    }

    if (!scanValid)
    {
        InterlockedExchange(&g_limiterAllocationFailed, 1);
        InterlockedExchange(&g_limiterBusy, 0);
        return;
    }

    if (requestedPixels)
    {
        if (maximumAppliedDrop <= g_limiterDecreaseBudget)
            g_limiterDecreaseBudget -= maximumAppliedDrop;
        else
            g_limiterDecreaseBudget = 0.0;
        InterlockedIncrement64(&g_limiterDecreaseBatches);
        InterlockedAdd64(&g_limiterRequestedPixels, requestedPixels);
        InterlockedAdd64(&g_limiterLimitedPixels, limitedPixels);
        InterlockedAdd64(&g_limiterPreventedUnits, preventedUnits);
        InterlockedAdd64(&g_limiterAllowedUnits, appliedUnits);
    }
    InterlockedExchange(&g_limiterBusy, 0);
}

static void UpdateExperimentalLimiterShadowTexel(void* texture, int x, int y,
                                                  unsigned green)
{
    if (!g_cfg.experimentalRecoverageLimiter || !g_limiterGreenShadow ||
        texture != g_limiterShadowTexture || x < 0 || y < 0 ||
        x >= g_limiterShadowWidth || y >= g_limiterShadowHeight)
        return;
    if (InterlockedCompareExchange(&g_limiterBusy, 1, 0) != 0)
        return;
    g_limiterGreenShadow[(SIZE_T)y * (SIZE_T)g_limiterShadowWidth +
                         (SIZE_T)x] = (BYTE)green;
    InterlockedExchange(&g_limiterBusy, 0);
}

static void FlushExperimentalLimiterStatus(ULONGLONG now)
{
    if (!g_cfg.experimentalRecoverageLimiter) return;
    if (InterlockedCompareExchange(&g_limiterAllocationFailed, 0, 0) &&
        InterlockedCompareExchange(&g_limiterFaultLogged, 1, 0) == 0)
        Warn("base visible-snow limiter was disabled after an invalid or "
             "unavailable terrain-mask buffer");
    if (g_nextLimiterStatusTick && now < g_nextLimiterStatusTick) return;
    g_nextLimiterStatusTick = now + 1000ULL;

    LONG64 resets = InterlockedExchange64(&g_limiterShadowResets, 0);
    LONG64 batches = InterlockedExchange64(&g_limiterDecreaseBatches, 0);
    LONG64 requested = InterlockedExchange64(&g_limiterRequestedPixels, 0);
    LONG64 limited = InterlockedExchange64(&g_limiterLimitedPixels, 0);
    LONG64 prevented = InterlockedExchange64(&g_limiterPreventedUnits, 0);
    LONG64 allowed = InterlockedExchange64(&g_limiterAllowedUnits, 0);
    LONG64 passes = InterlockedExchange64(&g_limiterEnforcementPasses, 0);
    LONG64 failures = InterlockedExchange64(&g_limiterOpenFailures, 0);
    if (!resets && !batches && !passes && !failures)
        return;

    Event("base visible-snow limiter: enforcement_passes=%lld "
          "open_failures=%lld shadow_resets=%lld decrease_batches=%lld "
          "requested_pixels=%lld limited_pixels=%lld allowed_green_units=%lld "
          "prevented_green_units=%lld maximum_decrease_per_second=%d "
          "sync_mode=coalesced-16ms",
          (long long)passes, (long long)failures, (long long)resets,
          (long long)batches, (long long)requested, (long long)limited,
          (long long)allowed, (long long)prevented,
          g_cfg.maximumGreenDecreasePerSecond);
}

// Prepare a separate visual shadow for the plow effect. Keeping this state
// independent from the optional global re-coverage experiment prevents either
// feature from changing the other's baseline or timing. Only marked texels
// need a baseline, and MarkPlowVisualTexelLocked captures those directly; a
// world change therefore does not scan the complete texture.
static bool PreparePlowVisualShadow(const TextureStorageState& storage)
{
    if (!InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) ||
        !storage.valid || !storage.texture || !storage.resource ||
        !storage.data || storage.width <= 0 || storage.height <= 0)
        return false;

    SIZE_T width = (SIZE_T)storage.width;
    SIZE_T height = (SIZE_T)storage.height;
    if (height > SIZE_MAX / width) return false;
    SIZE_T count = width * height;
    const SIZE_T maximumTexels = 16u * 1024u * 1024u;
    if (!count || count > maximumTexels ||
        count > SIZE_MAX / sizeof(LONG64))
        return false;
    SIZE_T activeCapacity = count;
    if (activeCapacity > PLOW_VISUAL_MAX_ACTIVE_TEXELS)
        activeCapacity = PLOW_VISUAL_MAX_ACTIVE_TEXELS;

    LONG generation = InterlockedCompareExchange(
        &g_gameWorldGeneration, 0, 0);
    bool identityChanged = g_plowVisualTexture != storage.texture ||
        g_plowVisualResource != storage.resource ||
        g_plowVisualWidth != storage.width ||
        g_plowVisualHeight != storage.height ||
        g_plowVisualRowPitch != storage.rowPitch ||
        g_plowVisualGeneration != generation;

    if (g_plowVisualCapacity < count || !g_plowVisualGreenShadow ||
        !g_plowVisualClearGreen || !g_plowVisualSnowAmount ||
        !g_plowVisualStrengthMillion ||
        !g_plowVisualSnowBurstGeneration ||
        !g_plowVisualSnowBurstAppliedUnits ||
        !g_plowVisualClearMinuteMilli || !g_plowVisualActiveIndices ||
        g_plowVisualActiveCapacity < activeCapacity)
    {
        BYTE* newShadow = (BYTE*)VirtualAlloc(
            NULL, count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        BYTE* newClearGreen = (BYTE*)VirtualAlloc(
            NULL, count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        BYTE* newSnowAmount = (BYTE*)VirtualAlloc(
            NULL, count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        LONG* newStrengthMillion = (LONG*)VirtualAlloc(
            NULL, count * sizeof(LONG), MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE);
        DWORD* newSnowBurstGeneration = (DWORD*)VirtualAlloc(
            NULL, count * sizeof(DWORD), MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE);
        BYTE* newSnowBurstAppliedUnits = (BYTE*)VirtualAlloc(
            NULL, count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        LONG64* newTimes = (LONG64*)VirtualAlloc(
            NULL, count * sizeof(LONG64), MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE);
        DWORD* newActive = (DWORD*)VirtualAlloc(
            NULL, activeCapacity * sizeof(DWORD),
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!newShadow || !newClearGreen || !newSnowAmount ||
            !newStrengthMillion ||
            !newSnowBurstGeneration || !newSnowBurstAppliedUnits ||
            !newTimes || !newActive)
        {
            if (newShadow) VirtualFree(newShadow, 0, MEM_RELEASE);
            if (newClearGreen) VirtualFree(newClearGreen, 0, MEM_RELEASE);
            if (newSnowAmount) VirtualFree(newSnowAmount, 0, MEM_RELEASE);
            if (newStrengthMillion)
                VirtualFree(newStrengthMillion, 0, MEM_RELEASE);
            if (newSnowBurstGeneration)
                VirtualFree(newSnowBurstGeneration, 0, MEM_RELEASE);
            if (newSnowBurstAppliedUnits)
                VirtualFree(newSnowBurstAppliedUnits, 0, MEM_RELEASE);
            if (newTimes) VirtualFree(newTimes, 0, MEM_RELEASE);
            if (newActive) VirtualFree(newActive, 0, MEM_RELEASE);
            InterlockedExchange(&g_plowVisualAllocationFailed, 1);
            return false;
        }
        if (g_plowVisualGreenShadow)
            VirtualFree(g_plowVisualGreenShadow, 0, MEM_RELEASE);
        if (g_plowVisualClearGreen)
            VirtualFree(g_plowVisualClearGreen, 0, MEM_RELEASE);
        if (g_plowVisualSnowAmount)
            VirtualFree(g_plowVisualSnowAmount, 0, MEM_RELEASE);
        if (g_plowVisualStrengthMillion)
            VirtualFree(g_plowVisualStrengthMillion, 0, MEM_RELEASE);
        if (g_plowVisualSnowBurstGeneration)
            VirtualFree(g_plowVisualSnowBurstGeneration, 0, MEM_RELEASE);
        if (g_plowVisualSnowBurstAppliedUnits)
            VirtualFree(g_plowVisualSnowBurstAppliedUnits, 0, MEM_RELEASE);
        if (g_plowVisualClearMinuteMilli)
            VirtualFree(g_plowVisualClearMinuteMilli, 0, MEM_RELEASE);
        if (g_plowVisualActiveIndices)
            VirtualFree(g_plowVisualActiveIndices, 0, MEM_RELEASE);
        g_plowVisualGreenShadow = newShadow;
        g_plowVisualClearGreen = newClearGreen;
        g_plowVisualSnowAmount = newSnowAmount;
        g_plowVisualStrengthMillion = newStrengthMillion;
        g_plowVisualSnowBurstGeneration = newSnowBurstGeneration;
        g_plowVisualSnowBurstAppliedUnits = newSnowBurstAppliedUnits;
        g_plowVisualClearMinuteMilli = newTimes;
        g_plowVisualActiveIndices = newActive;
        g_plowVisualCapacity = count;
        g_plowVisualActiveCapacity = activeCapacity;
        g_plowVisualActiveCount = 0;
        InterlockedExchange64(&g_plowVisualActivePublished, 0);
        identityChanged = true;
    }

    if (!identityChanged)
    {
        WeatherPersistenceRestoreVisualLocked(storage);
        return true;
    }

    g_plowVisualTexture = storage.texture;
    g_plowVisualResource = storage.resource;
    g_plowVisualWidth = storage.width;
    g_plowVisualHeight = storage.height;
    g_plowVisualRowPitch = storage.rowPitch;
    g_plowVisualGeneration = generation;
    memset(g_plowVisualGreenShadow, 0, count);
    memset(g_plowVisualClearGreen, 0, count);
    memset(g_plowVisualSnowAmount, 0, count);
    memset(g_plowVisualStrengthMillion, 0, count * sizeof(LONG));
    memset(g_plowVisualSnowBurstGeneration, 0, count * sizeof(DWORD));
    memset(g_plowVisualSnowBurstAppliedUnits, 0, count);
    memset(g_plowVisualClearMinuteMilli, 0, count * sizeof(LONG64));
    g_plowVisualActiveCount = 0;
    InterlockedExchange64(&g_plowVisualActivePublished, 0);
    g_plowVisualLastCoreTexture = NULL;
    g_plowVisualLastCoreGeneration = 0;
    g_plowVisualLastCoreX = 0;
    g_plowVisualLastCoreY = 0;
    g_plowVisualLastCoreTick = 0;
    g_plowVisualLastCoreValid = 0;
    InterlockedIncrement64(&g_plowVisualShadowResets);
    WeatherPersistenceRestoreVisualLocked(storage);
    return true;
}

static bool MarkPlowVisualTexelLocked(const TextureStorageState& storage,
                                      int x, int y, LONG64 gameMinuteMilli,
                                      int kind, int suppliedGreen = -1)
{
    if (x < 0 || y < 0 || x >= storage.width || y >= storage.height ||
        !storage.data ||
        storage.rowPitch < (unsigned)storage.width * sizeof(DWORD))
        return false;

    BYTE* row = (BYTE*)storage.data + (SIZE_T)y * storage.rowPitch;
    if (!ReadablePtr(row, (SIZE_T)storage.width * sizeof(DWORD))) return false;
    BYTE* greenAddress = row + (SIZE_T)x * sizeof(DWORD) + 1;
    unsigned green = suppliedGreen >= 0 ? (unsigned)suppliedGreen
                                        : (unsigned)*greenAddress;
    SIZE_T index = (SIZE_T)y * (SIZE_T)storage.width + (SIZE_T)x;
    LONG64 previousState = g_plowVisualClearMinuteMilli[index];
    LONG previousStrength = g_plowVisualStrengthMillion[index];

    if (g_visibleBrushCapture.active)
    {
        if (!g_visibleBrushCapture.texture)
        {
            g_visibleBrushCapture.texture = storage.texture;
            g_visibleBrushCapture.resource = storage.resource;
            g_visibleBrushCapture.generation = g_plowVisualGeneration;
        }
        if (g_visibleBrushCapture.texture == storage.texture &&
            g_visibleBrushCapture.resource == storage.resource &&
            g_visibleBrushCapture.generation == g_plowVisualGeneration &&
            index <= MAXDWORD)
        {
            if (g_visibleBrushCapture.texelCount <
                    MAX_CAPTURED_VISIBLE_TEXELS_PER_BRUSH)
            {
                DWORD captureIndex = g_visibleBrushCapture.texelCount++;
                g_visibleBrushCapture.texelIndices[captureIndex] =
                    (DWORD)index;
                g_visibleBrushCapture.priorStrengthMillion[captureIndex] =
                    previousStrength;
                g_visibleBrushCapture.priorClearMinuteMilli[captureIndex] =
                    previousState;
            }
            else
            {
                g_visibleBrushCapture.truncatedTexels++;
            }
        }
    }

    LONG strengthMillion = g_verifiedPlowEditMaskStrengthMillion;
    if (strengthMillion < 0) strengthMillion = 0;
    if (strengthMillion > GRIT_FACTOR_MILLION)
        strengthMillion = GRIT_FACTOR_MILLION;
    bool alreadyActive = previousState > 0;
    bool alreadyListed = previousState != 0;

    // With the material service active, the render-thread brush arrives before
    // the vehicle-thread clear identifies Sand, Gravel or dry operation. Clear
    // the visible snow baseline now, but defer all protection-state changes so
    // a temporary legacy strength cannot overwrite a stronger earlier layer.
    if (g_verifiedPlowEditMaskTreatmentDeferred)
    {
        g_plowVisualGreenShadow[index] = (BYTE)green;
        g_plowVisualClearGreen[index] = (BYTE)green;
        g_plowVisualSnowAmount[index] = 0;
        g_plowVisualSnowBurstGeneration[index] = (DWORD)
            InterlockedCompareExchange(
                &g_plowVisualLogicalBurstGeneration, 0, 0);
        LONG releasedUnits = InterlockedCompareExchange(
            &g_plowVisualLogicalBurstReleasedUnits, 0, 0);
        if (releasedUnits < 0) releasedUnits = 0;
        if (releasedUnits > 255) releasedUnits = 255;
        g_plowVisualSnowBurstAppliedUnits[index] = (BYTE)releasedUnits;
        return true;
    }

    if (strengthMillion == 0)
    {
        g_plowVisualGreenShadow[index] = (BYTE)green;
        g_plowVisualClearGreen[index] = (BYTE)green;
        g_plowVisualSnowAmount[index] = 0;
        g_plowVisualStrengthMillion[index] = 0;
        g_plowVisualClearMinuteMilli[index] = 0;
        if (alreadyActive)
            InterlockedIncrement64(&g_gritDryVisualTexelsCleared);
        return true;
    }
    if (!alreadyListed)
    {
        if (!g_plowVisualActiveIndices ||
            g_plowVisualActiveCount >= g_plowVisualActiveCapacity ||
            index > MAXDWORD)
        {
            InterlockedIncrement64(&g_plowVisualDroppedActiveTexels);
            return false;
        }
        g_plowVisualActiveIndices[g_plowVisualActiveCount++] = (DWORD)index;
        InterlockedExchange64(
            &g_plowVisualActivePublished, (LONG64)g_plowVisualActiveCount);
    }

    g_plowVisualGreenShadow[index] = (BYTE)green;
    g_plowVisualClearGreen[index] = (BYTE)green;
    g_plowVisualSnowAmount[index] = 0;
    g_plowVisualStrengthMillion[index] = strengthMillion;
    g_plowVisualSnowBurstGeneration[index] = (DWORD)
        InterlockedCompareExchange(
            &g_plowVisualLogicalBurstGeneration, 0, 0);
    LONG releasedUnits = InterlockedCompareExchange(
        &g_plowVisualLogicalBurstReleasedUnits, 0, 0);
    if (releasedUnits < 0) releasedUnits = 0;
    if (releasedUnits > 255) releasedUnits = 255;
    g_plowVisualSnowBurstAppliedUnits[index] = (BYTE)releasedUnits;
    if (alreadyActive)
        InterlockedIncrement64(&g_plowVisualRefreshed);
    else
        InterlockedIncrement64(&g_plowVisualMarked);
    g_plowVisualClearMinuteMilli[index] = gameMinuteMilli;

    if (kind == PLOW_VISUAL_TEXEL_CORE)
        InterlockedIncrement64(&g_plowVisualCoreTexels);
    else if (kind == PLOW_VISUAL_TEXEL_MARGIN)
        InterlockedIncrement64(&g_plowVisualMarginTexels);
    else
        InterlockedIncrement64(&g_plowVisualGapTexels);
    return true;
}

static void MarkPlowVisualMarginLocked(const TextureStorageState& storage,
                                       int centerX, int centerY,
                                       LONG64 gameMinuteMilli)
{
    for (int y = centerY - PLOW_VISUAL_MARGIN_TEXELS;
         y <= centerY + PLOW_VISUAL_MARGIN_TEXELS; ++y)
    {
        for (int x = centerX - PLOW_VISUAL_MARGIN_TEXELS;
             x <= centerX + PLOW_VISUAL_MARGIN_TEXELS; ++x)
        {
            if (x == centerX && y == centerY) continue;
            MarkPlowVisualTexelLocked(storage, x, y, gameMinuteMilli,
                                      PLOW_VISUAL_TEXEL_MARGIN);
        }
    }
}

static void ConnectPlowVisualTexelsLocked(const TextureStorageState& storage,
                                          int fromX, int fromY,
                                          int toX, int toY,
                                          LONG64 gameMinuteMilli)
{
    int dx = toX >= fromX ? toX - fromX : fromX - toX;
    int dy = toY >= fromY ? toY - fromY : fromY - toY;
    int distance = dx > dy ? dx : dy;
    if (distance <= 0) return;
    if (distance > PLOW_VISUAL_LINK_MAX_TEXEL_DISTANCE)
    {
        InterlockedIncrement64(&g_plowVisualSkippedGapLinks);
        return;
    }

    InterlockedIncrement64(&g_plowVisualGapLinks);
    int stepX = fromX < toX ? 1 : -1;
    int stepY = fromY < toY ? 1 : -1;
    int error = dx - dy;
    int x = fromX;
    int y = fromY;
    for (;;)
    {
        int doubled = error * 2;
        if (doubled > -dy)
        {
            error -= dy;
            x += stepX;
        }
        if (doubled < dx)
        {
            error += dx;
            y += stepY;
        }
        if (x == toX && y == toY) break;
        if (!MarkPlowVisualTexelLocked(storage, x, y, gameMinuteMilli,
                                       PLOW_VISUAL_TEXEL_GAP))
            break;
        MarkPlowVisualMarginLocked(storage, x, y, gameMinuteMilli);
    }
}

static void UpdatePlowVisualShadowTexel(void* texture, int x, int y,
                                        unsigned green,
                                        bool verifiedPlowWrite)
{
    if (WeatherPersistenceSuspended() ||
        !InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) || !texture ||
        InterlockedCompareExchange(&g_plowVisualAllocationFailed, 0, 0) ||
        InterlockedCompareExchange(&g_plowVisualBusy, 1, 0) != 0)
        return;

    TextureStorageState storage = {};
    bool ready = ReadTextureStorageState(texture, &storage) &&
                 PreparePlowVisualShadow(storage);
    if (ready && texture == g_plowVisualTexture && x >= 0 && y >= 0 &&
        x < g_plowVisualWidth && y < g_plowVisualHeight)
    {
        SIZE_T index = (SIZE_T)y * (SIZE_T)g_plowVisualWidth + (SIZE_T)x;
        g_plowVisualGreenShadow[index] = (BYTE)green;
        if (verifiedPlowWrite)
        {
            LONG64 gameMinuteMilli = InterlockedCompareExchange64(
                &g_gameMinuteMilli, 0, 0);
            ULONGLONG realTick = GetTickCount64();
            if (gameMinuteMilli > 0)
            {
                bool recentPrevious = g_plowVisualLastCoreValid &&
                    g_plowVisualLastCoreTexture == texture &&
                    g_plowVisualLastCoreGeneration == g_plowVisualGeneration &&
                    realTick >= g_plowVisualLastCoreTick &&
                    realTick - g_plowVisualLastCoreTick <=
                        PLOW_VISUAL_LINK_MAX_AGE_MS;
                if (recentPrevious)
                {
                    ConnectPlowVisualTexelsLocked(
                        storage, g_plowVisualLastCoreX,
                        g_plowVisualLastCoreY, x, y, gameMinuteMilli);
                }

                MarkPlowVisualTexelLocked(
                    storage, x, y, gameMinuteMilli,
                    PLOW_VISUAL_TEXEL_CORE, (int)green);
                MarkPlowVisualMarginLocked(storage, x, y, gameMinuteMilli);
                g_plowVisualLastCoreTexture = texture;
                g_plowVisualLastCoreGeneration = g_plowVisualGeneration;
                g_plowVisualLastCoreX = x;
                g_plowVisualLastCoreY = y;
                g_plowVisualLastCoreTick = realTick;
                g_plowVisualLastCoreValid = 1;
            }
        }
        else
        {
            // A different direct brush owns this texel now. Snow re-coverage
            // is imported through the mapped buffer and therefore does not
            // take this path. Only the directly written texel loses its plow
            // ownership; a neighbouring verified road corridor stays intact.
            if (g_plowVisualClearMinuteMilli[index] > 0)
                g_plowVisualClearMinuteMilli[index] = -1;
            g_plowVisualSnowAmount[index] = 0;
            g_plowVisualSnowBurstGeneration[index] = 0;
            g_plowVisualSnowBurstAppliedUnits[index] = 0;
            g_plowVisualLastCoreValid = 0;
        }
    }
    InterlockedExchange(&g_plowVisualBusy, 0);
}

static void ApplyPlowVisualAfterEffect(const TextureStorageState& storage,
                                       int applySite =
                                           PLOW_VISUAL_APPLY_REGULAR)
{
    if (WeatherPersistenceSuspended() ||
        !InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) ||
        (InterlockedCompareExchange64(&g_plowVisualActivePublished, 0, 0) <= 0 &&
         !WeatherPersistenceVisualPending()) ||
        InterlockedCompareExchange(&g_plowVisualAllocationFailed, 0, 0) ||
        InterlockedCompareExchange(&g_plowVisualBusy, 1, 0) != 0)
        return;

    if (!PreparePlowVisualShadow(storage))
    {
        InterlockedExchange(&g_plowVisualBusy, 0);
        return;
    }

    LONG64 now = InterlockedCompareExchange64(&g_gameMinuteMilli, 0, 0);
    LONG64 protectionDuration = PlowProtectionDuration();
    LONG64 saltDuration = PlowSaltDuration();
    LONG64 totalDuration = protectionDuration + saltDuration;
    LONG64 saltFactor = InterlockedCompareExchange64(
        &g_plowSaltFactorMillion, 0, 0);
    LONG snowBurstGeneration = InterlockedCompareExchange(
        &g_plowVisualEventBurstGeneration, 0, 0);
    bool snowBurstActive = snowBurstGeneration > 0 &&
        InterlockedCompareExchange(&g_plowVisualEventHasSnow, 0, 0) != 0 &&
        InterlockedCompareExchange(
            &g_plowVisualEventPassesRemaining, 0, 0) > 0;
    LONG logicalBurstGeneration = InterlockedCompareExchange(
        &g_plowVisualLogicalBurstGeneration, 0, 0);
    LONG logicalBurstReleasedUnits = InterlockedCompareExchange(
        &g_plowVisualLogicalBurstReleasedUnits, 0, 0);
    if (logicalBurstReleasedUnits < 0) logicalBurstReleasedUnits = 0;
    if (logicalBurstReleasedUnits > 255) logicalBurstReleasedUnits = 255;
    if (logicalBurstGeneration <= 0 || logicalBurstReleasedUnits <= 0)
        snowBurstActive = false;
    // Whichever verified mask pass runs first owns this pulse. Consuming it
    // here lets that same pass see whether the engine already lightened each
    // texel, which prevents applying the natural reduction twice.
    LONG64 pendingMeltUnits = InterlockedExchange64(
        &g_plowVisualPendingMeltUnits, 0);
    LONG64 protectionPixels = 0;
    LONG64 protectionUnits = 0;
    LONG64 saltPixels = 0;
    LONG64 saltPreventedUnits = 0;
    LONG64 duplicateSnowWrites = 0;
    LONG64 burstLimitedPixels = 0;
    LONG64 burstPreventedUnits = 0;
    LONG64 stagePixels = 0;
    LONG64 meltPixels = 0;
    LONG64 meltUnits = 0;
    LONG64 expired = 0;
    LONG64 visualSnowSum = 0;
    LONG64 visualSnowCount = 0;
    LONG visualSnowMaximum = 0;
    bool valid = true;
    SIZE_T totalTexels = (SIZE_T)storage.width * (SIZE_T)storage.height;

    if (now <= 0)
    {
        for (SIZE_T i = 0; i < g_plowVisualActiveCount; ++i)
        {
            SIZE_T index = (SIZE_T)g_plowVisualActiveIndices[i];
            if (index < totalTexels)
                g_plowVisualClearMinuteMilli[index] = 0;
        }
        g_plowVisualActiveCount = 0;
        InterlockedExchange64(&g_plowVisualActivePublished, 0);
        InterlockedExchange(&g_plowVisualBusy, 0);
        return;
    }

    __try
    {
        SIZE_T activeIndex = 0;
        int cachedY = -1;
        BYTE* cachedRow = NULL;
        while (valid && activeIndex < g_plowVisualActiveCount)
        {
            SIZE_T index =
                (SIZE_T)g_plowVisualActiveIndices[activeIndex];
            if (index >= totalTexels)
            {
                valid = false;
                break;
            }

            LONG64 cleared = g_plowVisualClearMinuteMilli[index];
            if (cleared <= 0)
            {
                g_plowVisualClearMinuteMilli[index] = 0;
                g_plowVisualActiveIndices[activeIndex] =
                    g_plowVisualActiveIndices[--g_plowVisualActiveCount];
                continue;
            }

            LONG64 age = now - cleared;
            if (age < 0)
            {
                g_plowVisualClearMinuteMilli[index] = 0;
                g_plowVisualActiveIndices[activeIndex] =
                    g_plowVisualActiveIndices[--g_plowVisualActiveCount];
                expired++;
                continue;
            }
            bool effectActive = totalDuration > 0 && age < totalDuration;

            int y = (int)(index / (SIZE_T)storage.width);
            int x = (int)(index % (SIZE_T)storage.width);
            if (y != cachedY)
            {
                cachedRow = (BYTE*)storage.data +
                            (SIZE_T)y * storage.rowPitch;
                if (!ReadablePtr(
                        cachedRow,
                        (SIZE_T)storage.width * sizeof(DWORD)))
                {
                    valid = false;
                    break;
                }
                cachedY = y;
            }

            if (!cachedRow)
            {
                valid = false;
                break;
            }
            BYTE* green = cachedRow + (SIZE_T)x * sizeof(DWORD) + 1;
            unsigned previous = g_plowVisualGreenShadow[index];
            unsigned current = *green;
            unsigned finalValue = current;

            // snow_levels=0 keeps the exact continuous 0..255 amount. Values
            // from 1 to 8 deliberately quantise it into visible stages.
            if (g_cfg.visualSnowLevels >= 0)
            {
                unsigned clearGreen = g_plowVisualClearGreen[index];
                unsigned snowAmount = g_plowVisualSnowAmount[index];
                DWORD logicalGeneration =
                    (DWORD)logicalBurstGeneration;
                unsigned appliedReleasedUnits =
                    g_plowVisualSnowBurstAppliedUnits[index];
                if (snowBurstActive &&
                    g_plowVisualSnowBurstGeneration[index] !=
                        logicalGeneration)
                {
                    // This is the first released unit of a genuinely new
                    // snowfall for this texel. Keep the snow reached by the
                    // previous snowfall as the new baseline and consume only
                    // units released in the new logical burst.
                    g_plowVisualSnowBurstGeneration[index] =
                        logicalGeneration;
                    appliedReleasedUnits = 0;
                    g_plowVisualSnowBurstAppliedUnits[index] = 0;
                }

                bool hasNewReleasedSnow = snowBurstActive &&
                    (unsigned)logicalBurstReleasedUnits >
                        appliedReleasedUnits;
                if (snowBurstActive && !hasNewReleasedSnow)
                    duplicateSnowWrites++;

                // The road simulation has already applied the global
                // accumulation multiplier and burst cap before these units
                // are released. Drive the visual value from that exact
                // authoritative count instead of reinterpreting the GPU's
                // absolute mask value. This gives each later snowfall a new
                // +40 budget while repeated readbacks remain idempotent.
                if (hasNewReleasedSnow)
                {
                    LONG64 phaseFactor = 1000000LL;
                    LONG strengthMillion =
                        g_plowVisualStrengthMillion[index];
                    if (effectActive && age < protectionDuration)
                    {
                        phaseFactor = BlendGritPhaseFactor(
                            0, strengthMillion);
                        protectionPixels++;
                    }
                    else if (effectActive)
                    {
                        phaseFactor = BlendGritPhaseFactor(
                            saltFactor, strengthMillion);
                        saltPixels++;
                    }

                    unsigned releasedNow =
                        (unsigned)logicalBurstReleasedUnits;
                    unsigned phaseBefore = (unsigned)(
                        ((LONG64)appliedReleasedUnits * phaseFactor +
                         500000LL) / 1000000LL);
                    unsigned phaseNow = (unsigned)(
                        ((LONG64)releasedNow * phaseFactor + 500000LL) /
                        1000000LL);
                    unsigned addedSnow = phaseNow >= phaseBefore
                        ? phaseNow - phaseBefore : 0u;
                    unsigned unsaltedSnow =
                        releasedNow - appliedReleasedUnits;

                    if (effectActive && unsaltedSnow > addedSnow)
                    {
                        if (age < protectionDuration)
                            protectionUnits += unsaltedSnow - addedSnow;
                        else
                            saltPreventedUnits += unsaltedSnow - addedSnow;
                    }

                    snowAmount += addedSnow;
                    if (snowAmount > 255) snowAmount = 255;
                    g_plowVisualSnowBurstAppliedUnits[index] =
                        (BYTE)releasedNow;

                    unsigned rawSnowAmount = 0;
                    if (clearGreen > 0 && current < clearGreen)
                    {
                        rawSnowAmount = (unsigned)(
                            ((LONG64)(clearGreen - current) * 255LL +
                             (LONG64)clearGreen / 2LL) /
                            (LONG64)clearGreen);
                        if (rawSnowAmount > 255) rawSnowAmount = 255;
                    }
                    if (rawSnowAmount > snowAmount)
                    {
                        burstPreventedUnits +=
                            rawSnowAmount - snowAmount;
                        burstLimitedPixels++;
                    }
                }

                // The engine's internal -30 natural reduction is scaled by
                // the configured multiplier. The precise visual amount uses
                // that same scaled pulse and deliberately ignores a larger
                // vanilla GPU lightening observed in the same event.
                if (pendingMeltUnits > 0 && snowAmount > 0)
                {
                    unsigned reduction = pendingMeltUnits > 255
                        ? 255u : (unsigned)pendingMeltUnits;
                    if (reduction > snowAmount) reduction = snowAmount;
                    snowAmount -= reduction;
                    meltPixels++;
                    meltUnits += reduction;
                }
                else if (!snowBurstActive && pendingMeltUnits == 0 &&
                         current > previous)
                {
                    // Preserve genuine visual lightening that was not caused
                    // by the separately scaled natural-melt pulse.
                    if (clearGreen > 0 && current < clearGreen)
                    {
                        snowAmount = (unsigned)(
                            ((LONG64)(clearGreen - current) * 255LL +
                             (LONG64)clearGreen / 2LL) /
                            (LONG64)clearGreen);
                        if (snowAmount > 255) snowAmount = 255;
                    }
                    else
                    {
                        snowAmount = 0;
                    }
                }

                unsigned levels = (unsigned)g_cfg.visualSnowLevels;
                unsigned displayedSnowAmount = snowAmount;
                if (levels == 0)
                {
                    displayedSnowAmount = snowAmount;
                }
                else
                {
                    unsigned level = snowAmount == 0 ? 0u :
                        (snowAmount * levels + 254u) / 255u;
                    if (level > levels) level = levels;
                    displayedSnowAmount = (unsigned)(
                        ((LONG64)level * 255LL + (LONG64)levels / 2LL) /
                        (LONG64)levels);
                }
                unsigned shaderSnowAmount =
                    g_visualSnowShaderAmount[displayedSnowAmount];
                unsigned stagedValue = (unsigned)(
                    ((LONG64)clearGreen *
                     (LONG64)(255u - shaderSnowAmount) + 127LL) / 255LL);
                if (stagedValue != current)
                {
                    *green = (BYTE)stagedValue;
                    stagePixels++;
                }
                finalValue = stagedValue;
                g_plowVisualSnowAmount[index] = (BYTE)snowAmount;
                visualSnowSum += snowAmount;
                visualSnowCount++;
                if ((LONG)snowAmount > visualSnowMaximum)
                    visualSnowMaximum = (LONG)snowAmount;

                // A verified road texel remains known until the world or mask
                // changes. Protection and salt may expire, but forgetting the
                // road itself would let every later snowfall return to the
                // unrestricted vanilla visual mask on that texel.
            }

            g_plowVisualGreenShadow[index] = (BYTE)finalValue;
            activeIndex++;
        }
        InterlockedExchange64(
            &g_plowVisualActivePublished, (LONG64)g_plowVisualActiveCount);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        valid = false;
    }

    if (!valid)
        InterlockedExchange(&g_plowVisualAllocationFailed, 1);
    else
    {
        InterlockedAdd64(&g_plowVisualProtectionPixels, protectionPixels);
        InterlockedAdd64(&g_plowVisualProtectedUnits, protectionUnits);
        InterlockedAdd64(&g_plowVisualSaltPixels, saltPixels);
        InterlockedAdd64(&g_plowVisualSaltPreventedUnits,
                         saltPreventedUnits);
        InterlockedAdd64(&g_plowVisualDuplicateSnowWrites,
                         duplicateSnowWrites);
        InterlockedAdd64(&g_plowVisualBurstLimitedPixels,
                         burstLimitedPixels);
        InterlockedAdd64(&g_plowVisualBurstPreventedUnits,
                         burstPreventedUnits);
        if (duplicateSnowWrites > 0)
            InterlockedExchange64(&g_overlayLastDuplicateSnowWrites,
                                  duplicateSnowWrites);
        InterlockedExchange64(&g_overlayVisualSnowSum, visualSnowSum);
        InterlockedExchange64(&g_overlayVisualSnowCount, visualSnowCount);
        InterlockedExchange(&g_overlayVisualSnowMaximum, visualSnowMaximum);
        InterlockedAdd64(&g_plowVisualStagePixels, stagePixels);
        InterlockedAdd64(&g_plowVisualMeltPixels, meltPixels);
        InterlockedAdd64(&g_plowVisualMeltUnits, meltUnits);
        InterlockedAdd64(&g_plowVisualExpired, expired);
        if (applySite == PLOW_VISUAL_APPLY_PRE_UPLOAD)
        {
            InterlockedAdd64(&g_plowVisualPreUploadProtectionPixels,
                             protectionPixels);
            InterlockedAdd64(&g_plowVisualPreUploadSaltPixels, saltPixels);
        }
        else if (applySite == PLOW_VISUAL_APPLY_EVENT_SYNC ||
                 applySite == PLOW_VISUAL_APPLY_INLINE_SYNC)
        {
            InterlockedAdd64(&g_plowVisualEventProtectionPixels,
                             protectionPixels);
            InterlockedAdd64(&g_plowVisualEventSaltPixels, saltPixels);
            if (applySite == PLOW_VISUAL_APPLY_INLINE_SYNC)
                InterlockedAdd64(&g_plowVisualInlineCorrectedPixels,
                                 stagePixels);
            if (protectionPixels || saltPixels || stagePixels || meltPixels)
            {
                LONG64 start = InterlockedCompareExchange64(
                    &g_plowVisualEventStartTick, 0, 0);
                ULONGLONG realNow = GetTickCount64();
                if (start > 0 && realNow >= (ULONGLONG)start)
                {
                    LONG64 latency = (LONG64)(realNow - (ULONGLONG)start);
                    if (InterlockedCompareExchange64(
                            &g_plowVisualEventFirstCorrectionMs,
                            latency, -1) == -1)
                        InterlockedExchange(
                            &g_overlayLastCorrectionMs, (LONG)latency);
                }
            }
        }
    }
    InterlockedExchange(&g_plowVisualBusy, 0);
}

static void FlushPlowVisualStatus(ULONGLONG now)
{
    if (!InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0)) return;
    if (InterlockedCompareExchange(&g_plowVisualAllocationFailed, 0, 0) &&
        InterlockedCompareExchange(&g_plowVisualFaultLogged, 1, 0) == 0)
        Warn("visible plow after-effect was disabled after an invalid or "
             "unavailable terrain-mask buffer; the internal road effect "
             "remains active");
    if (g_nextPlowVisualStatusTick && now < g_nextPlowVisualStatusTick) return;
    g_nextPlowVisualStatusTick = now + 1000ULL;

    LONG64 resets = InterlockedExchange64(&g_plowVisualShadowResets, 0);
    LONG64 marked = InterlockedExchange64(&g_plowVisualMarked, 0);
    LONG64 refreshed = InterlockedExchange64(&g_plowVisualRefreshed, 0);
    LONG64 coreTexels = InterlockedExchange64(&g_plowVisualCoreTexels, 0);
    LONG64 marginTexels = InterlockedExchange64(&g_plowVisualMarginTexels, 0);
    LONG64 gapTexels = InterlockedExchange64(&g_plowVisualGapTexels, 0);
    LONG64 gapLinks = InterlockedExchange64(&g_plowVisualGapLinks, 0);
    LONG64 skippedGapLinks = InterlockedExchange64(
        &g_plowVisualSkippedGapLinks, 0);
    LONG64 droppedActiveTexels = InterlockedExchange64(
        &g_plowVisualDroppedActiveTexels, 0);
    LONG64 visualOnlySnow = InterlockedExchange64(
        &g_plowVisualOnlySnowEvents, 0);
    LONG64 protectionPixels =
        InterlockedExchange64(&g_plowVisualProtectionPixels, 0);
    LONG64 protectedUnits =
        InterlockedExchange64(&g_plowVisualProtectedUnits, 0);
    LONG64 saltPixels = InterlockedExchange64(&g_plowVisualSaltPixels, 0);
    LONG64 saltPrevented =
        InterlockedExchange64(&g_plowVisualSaltPreventedUnits, 0);
    LONG64 duplicateSnowWrites = InterlockedExchange64(
        &g_plowVisualDuplicateSnowWrites, 0);
    LONG64 burstLimitedPixels = InterlockedExchange64(
        &g_plowVisualBurstLimitedPixels, 0);
    LONG64 burstPreventedUnits = InterlockedExchange64(
        &g_plowVisualBurstPreventedUnits, 0);
    LONG64 stagePixels =
        InterlockedExchange64(&g_plowVisualStagePixels, 0);
    LONG64 meltPixels =
        InterlockedExchange64(&g_plowVisualMeltPixels, 0);
    LONG64 meltUnits =
        InterlockedExchange64(&g_plowVisualMeltUnits, 0);
    LONG64 expired = InterlockedExchange64(&g_plowVisualExpired, 0);
    LONG64 passes = InterlockedExchange64(&g_plowVisualEnforcementPasses, 0);
    LONG64 failures = InterlockedExchange64(&g_plowVisualOpenFailures, 0);
    LONG64 preUploadProtection = InterlockedExchange64(
        &g_plowVisualPreUploadProtectionPixels, 0);
    LONG64 preUploadSalt = InterlockedExchange64(
        &g_plowVisualPreUploadSaltPixels, 0);
    LONG64 eventRequests = InterlockedExchange64(
        &g_plowVisualEventRequests, 0);
    LONG64 eventPasses = InterlockedExchange64(
        &g_plowVisualEventPasses, 0);
    LONG64 eventProtection = InterlockedExchange64(
        &g_plowVisualEventProtectionPixels, 0);
    LONG64 eventSalt = InterlockedExchange64(
        &g_plowVisualEventSaltPixels, 0);
    LONG64 eventCoalesced = InterlockedExchange64(
        &g_plowVisualEventCoalesced, 0);
    LONG64 eventSkippedInactive = InterlockedExchange64(
        &g_plowVisualEventSkippedInactive, 0);
    LONG64 inlinePasses = InterlockedExchange64(
        &g_plowVisualInlinePasses, 0);
    LONG64 inlineCorrectedPixels = InterlockedExchange64(
        &g_plowVisualInlineCorrectedPixels, 0);
    LONG64 eventFirstCorrectionMs = InterlockedExchange64(
        &g_plowVisualEventFirstCorrectionMs, -1);
    LONG64 enforcementElapsedMs = InterlockedExchange64(
        &g_plowVisualEnforcementElapsedMs, 0);
    LONG64 enforcementMaximumMs = InterlockedExchange64(
        &g_plowVisualEnforcementMaximumMs, 0);
    LONG64 activeTexels = InterlockedCompareExchange64(
        &g_plowVisualActivePublished, 0, 0);
    LONG logicalBurstGeneration = InterlockedCompareExchange(
        &g_plowVisualLogicalBurstGeneration, 0, 0);
    LONG logicalBurstReleasedUnits = InterlockedCompareExchange(
        &g_plowVisualLogicalBurstReleasedUnits, 0, 0);
    LONG64 visualSnowSum = InterlockedCompareExchange64(
        &g_overlayVisualSnowSum, 0, 0);
    LONG64 visualSnowCount = InterlockedCompareExchange64(
        &g_overlayVisualSnowCount, 0, 0);
    LONG visualSnowMaximum = InterlockedCompareExchange(
        &g_overlayVisualSnowMaximum, 0, 0);
    unsigned visualAveragePercent = visualSnowCount > 0
        ? (unsigned)((visualSnowSum * 100LL + visualSnowCount * 127LL) /
                     (visualSnowCount * 255LL)) : 0u;
    unsigned visualMaximumPercent = (unsigned)(
        ((LONG64)visualSnowMaximum * 100LL + 127LL) / 255LL);
    if (!resets && !marked && !refreshed && !protectionPixels &&
        !saltPixels && !duplicateSnowWrites && !burstLimitedPixels &&
        !burstPreventedUnits && !stagePixels && !meltPixels &&
        !expired && !failures &&
        !preUploadProtection &&
        !preUploadSalt && !eventRequests && !eventPasses &&
        !eventProtection && !eventSalt && !coreTexels && !marginTexels &&
        !gapTexels && !gapLinks && !skippedGapLinks && !droppedActiveTexels &&
        !visualOnlySnow && !eventCoalesced && !eventSkippedInactive &&
        !inlinePasses && !inlineCorrectedPixels &&
        !enforcementElapsedMs && !enforcementMaximumMs)
        return;

    Event("EXPERIMENT visible plow after-effect: marked_texels=%lld "
          "refreshed_texels=%lld core_texels=%lld margin_texels=%lld "
          "gap_texels=%lld gap_links=%lld skipped_gap_links=%lld "
          "dropped_active_texels=%lld active_texels=%lld "
          "logical_burst_generation=%ld logical_burst_released_units=%ld "
          "visual_only_snow=%lld protection_pixels=%lld "
          "protected_green_units=%lld salt_pixels=%lld "
          "salt_prevented_green_units=%lld "
          "duplicate_snow_writes_suppressed=%lld "
          "visual_burst_limited_pixels=%lld "
          "visual_burst_prevented_units=%lld staged_pixels=%lld "
          "natural_melt_pixels=%lld natural_melt_units=%lld snow_levels=%d "
          "logical_visible_snow_avg_percent=%u "
          "logical_visible_snow_max_percent=%u visible_sample_texels=%lld "
          "expired_texels=%lld "
          "shadow_resets=%lld enforcement_passes=%lld open_failures=%lld "
          "pre_upload_protection_pixels=%lld pre_upload_salt_pixels=%lld "
          "event_sync_requests=%lld event_sync_passes=%lld "
          "event_sync_coalesced=%lld event_sync_skipped_inactive=%lld "
          "inline_sync_passes=%lld inline_sync_corrected_pixels=%lld "
          "event_sync_protection_pixels=%lld event_sync_salt_pixels=%lld "
          "event_sync_first_correction_ms=%lld enforcement_elapsed_ms=%lld "
          "enforcement_maximum_ms=%lld",
          (long long)marked, (long long)refreshed,
          (long long)coreTexels, (long long)marginTexels,
          (long long)gapTexels, (long long)gapLinks,
          (long long)skippedGapLinks, (long long)droppedActiveTexels,
          (long long)activeTexels, logicalBurstGeneration,
          logicalBurstReleasedUnits, (long long)visualOnlySnow,
          (long long)protectionPixels, (long long)protectedUnits,
          (long long)saltPixels, (long long)saltPrevented,
          (long long)duplicateSnowWrites, (long long)burstLimitedPixels,
          (long long)burstPreventedUnits, (long long)stagePixels,
          (long long)meltPixels,
          (long long)meltUnits, g_cfg.visualSnowLevels,
          visualAveragePercent, visualMaximumPercent,
          (long long)visualSnowCount,
          (long long)expired, (long long)resets, (long long)passes,
          (long long)failures, (long long)preUploadProtection,
          (long long)preUploadSalt, (long long)eventRequests,
          (long long)eventPasses, (long long)eventCoalesced,
          (long long)eventSkippedInactive, (long long)inlinePasses,
          (long long)inlineCorrectedPixels, (long long)eventProtection,
          (long long)eventSalt, (long long)eventFirstCorrectionMs,
          (long long)enforcementElapsedMs,
          (long long)enforcementMaximumMs);
}

static bool RunExperimentalLimiterEnforcementPass(
    void* texture, int plowVisualApplySite = PLOW_VISUAL_APPLY_REGULAR)
{
    bool experimental = g_cfg.experimentalRecoverageLimiter != 0;
    bool plowVisual =
        InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) != 0 &&
        InterlockedCompareExchange64(
            &g_plowVisualActivePublished, 0, 0) > 0;
    if ((!experimental && !plowVisual) || !texture ||
        !o_TextureAccessOpen2 || !o_TextureAccessClose)
        return false;

    ULONGLONG started = GetTickCount64();
    bool opened = o_TextureAccessOpen2(texture);
    if (!opened)
    {
        if (experimental) InterlockedIncrement64(&g_limiterOpenFailures);
        if (plowVisual) InterlockedIncrement64(&g_plowVisualOpenFailures);
        return false;
    }

    __try
    {
        TextureStorageState storage = {};
        if (ReadTextureStorageState(texture, &storage))
        {
            if (experimental) ApplyExperimentalRecoverageLimiter(storage);
            if (plowVisual)
                ApplyPlowVisualAfterEffect(storage, plowVisualApplySite);
        }
    }
    __finally
    {
        // This function owns exactly one successful Open2. Failed opens
        // return above; ordinary arguments, ordering and exceptions are kept.
        o_TextureAccessClose(texture);
    }
    if (experimental) InterlockedIncrement64(&g_limiterEnforcementPasses);
    if (plowVisual)
    {
        InterlockedIncrement64(&g_plowVisualEnforcementPasses);
        LONG64 elapsed = (LONG64)(GetTickCount64() - started);
        InterlockedAdd64(&g_plowVisualEnforcementElapsedMs, elapsed);
        LONG64 maximum = InterlockedCompareExchange64(
            &g_plowVisualEnforcementMaximumMs, 0, 0);
        while (elapsed > maximum)
        {
            LONG64 observed = InterlockedCompareExchange64(
                &g_plowVisualEnforcementMaximumMs, elapsed, maximum);
            if (observed == maximum) break;
            maximum = observed;
        }
    }
    return true;
}

// Track the authoritative positive units that have actually been released to
// the road simulation. A quiet gap starts a new logical snowfall. The visual
// layer consumes this cumulative value once per known road texel, so repeated
// GPU readbacks cannot apply the same unit twice and a later snowfall starts
// from the value reached by the previous one.
static void PublishPlowVisualLogicalSnowRelease(ULONGLONG now, int delta)
{
    if (delta <= 0) return;

    AcquireSRWLockExclusive(&g_plowVisualLogicalBurstLock);
    LONG generation = InterlockedCompareExchange(
        &g_plowVisualLogicalBurstGeneration, 0, 0);
    LONG released = InterlockedCompareExchange(
        &g_plowVisualLogicalBurstReleasedUnits, 0, 0);
    if (!g_plowVisualLogicalBurstLastTick ||
        now - g_plowVisualLogicalBurstLastTick >
            (ULONGLONG)g_cfg.snowBurstResetAfterMs)
    {
        generation++;
        if (generation <= 0) generation = 1;
        released = 0;
    }
    g_plowVisualLogicalBurstLastTick = now;
    if (released < 0) released = 0;
    if (released > 255 - delta)
        released = 255;
    else
        released += delta;
    InterlockedExchange(&g_plowVisualLogicalBurstReleasedUnits, released);
    InterlockedExchange(&g_plowVisualLogicalBurstGeneration, generation);
    ReleaseSRWLockExclusive(&g_plowVisualLogicalBurstLock);
}

static void ResetPlowVisualLogicalSnowBurst(void)
{
    AcquireSRWLockExclusive(&g_plowVisualLogicalBurstLock);
    g_plowVisualLogicalBurstLastTick = 0;
    InterlockedExchange(&g_plowVisualLogicalBurstReleasedUnits, 0);
    LONG generation = InterlockedIncrement(
        &g_plowVisualLogicalBurstGeneration);
    if (generation <= 0)
        InterlockedExchange(&g_plowVisualLogicalBurstGeneration, 1);
    ReleaseSRWLockExclusive(&g_plowVisualLogicalBurstLock);
}

// The global snow overlay is produced directly on the GPU and therefore does
// not pass through the observed CPU-side TextureAccessClose upload. A positive
// authoritative road-snow adjustment or a scaled -30 natural-melt pulse
// schedules one coalesced pass on the already verified game-update thread.
// Version 0.1.38 deliberately waits 16 ms: the old inline pass usually ran
// before the GPU write, while the pre-Present pass stalled rendering and was
// still too late for the completed frame. There is no render-thread work and
// no permanent one-second full-mask scan. The plow-only layer still waits for
// verified plow texels. Each burst receives a generation number so repeated
// observations of the same absolute GPU snow write cannot advance the plow
// visual stages more than once.
static void SchedulePlowVisualEventSync(void* world, int delta)
{
    bool baseVisual = g_cfg.experimentalRecoverageLimiter != 0;
    bool plowVisual =
        InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) != 0;
    if (delta == 0 || delta == -255 || (!baseVisual && !plowVisual) ||
        world != InterlockedCompareExchangePointer(&g_weatherWorld, NULL, NULL))
        return;

    if (!baseVisual && InterlockedCompareExchange64(
            &g_plowVisualActivePublished, 0, 0) <= 0)
    {
        InterlockedIncrement64(&g_plowVisualEventSkippedInactive);
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (delta > 0)
        PublishPlowVisualLogicalSnowRelease(now, delta);
    InterlockedIncrement64(&g_plowVisualEventRequests);
    if (InterlockedCompareExchange(
            &g_plowVisualEventPassesRemaining, 0, 0) != 0)
    {
        if (delta > 0)
            InterlockedExchange(&g_plowVisualEventHasSnow, 1);
        InterlockedIncrement64(&g_plowVisualEventCoalesced);
        return;
    }

    if (InterlockedCompareExchange(
            &g_plowVisualEventPassesRemaining, -1, 0) == 0)
    {
        LONG burstGeneration = InterlockedIncrement(
            &g_plowVisualEventBurstGeneration);
        if (burstGeneration <= 0)
        {
            InterlockedExchange(&g_plowVisualEventBurstGeneration, 1);
            burstGeneration = 1;
        }
        InterlockedExchange(&g_plowVisualEventHasSnow, delta > 0 ? 1 : 0);
        LONG64 cooldown = InterlockedCompareExchange64(
            &g_plowVisualEventCooldownUntilTick, 0, 0);
        ULONGLONG firstPass = now + PLOW_VISUAL_EVENT_DELAY_MS;
        if (cooldown > 0 && (ULONGLONG)cooldown > firstPass)
        {
            firstPass = (ULONGLONG)cooldown;
            InterlockedIncrement64(&g_plowVisualEventCoalesced);
        }
        InterlockedExchange64(&g_plowVisualEventStartTick, (LONG64)now);
        InterlockedExchange64(&g_plowVisualEventFirstCorrectionMs, -1);
        InterlockedExchange(&g_plowVisualEventPassIndex, 0);
        InterlockedExchange64(
            &g_plowVisualEventNextTick, (LONG64)firstPass);
        InterlockedExchange(&g_plowVisualEventPassesRemaining,
                            PLOW_VISUAL_EVENT_PASSES);
    }
}

static void ServicePlowVisualEventSync(
    ULONGLONG now,
    int applySite = PLOW_VISUAL_APPLY_EVENT_SYNC)
{
    static const ULONGLONG passDelayMs[PLOW_VISUAL_EVENT_PASSES] = {
        PLOW_VISUAL_EVENT_DELAY_MS
    };
    LONG remaining = InterlockedCompareExchange(
        &g_plowVisualEventPassesRemaining, 0, 0);
    if (remaining <= 0) return;
    bool baseVisual = g_cfg.experimentalRecoverageLimiter != 0;
    if (!baseVisual && InterlockedCompareExchange64(
            &g_plowVisualActivePublished, 0, 0) <= 0)
    {
        InterlockedExchange(&g_plowVisualEventPassesRemaining, 0);
        InterlockedExchange64(&g_plowVisualEventNextTick, 0);
        InterlockedExchange64(&g_plowVisualEventStartTick, 0);
        InterlockedExchange(&g_plowVisualEventHasSnow, 0);
        return;
    }

    LONG64 due = InterlockedCompareExchange64(
        &g_plowVisualEventNextTick, 0, 0);
    if (due <= 0 || now < (ULONGLONG)due) return;

    void* texture = InterlockedCompareExchangePointer(
        &g_terrainMaskTexture, NULL, NULL);
    if (!texture || !ReadablePtr(texture, MIN_TEXTURE_READ_SIZE))
    {
        InterlockedExchange(&g_plowVisualEventPassesRemaining, 0);
        InterlockedExchange64(&g_plowVisualEventNextTick, 0);
        InterlockedExchange64(&g_plowVisualEventStartTick, 0);
        InterlockedExchange(&g_plowVisualEventHasSnow, 0);
        return;
    }

    if (RunExperimentalLimiterEnforcementPass(texture, applySite))
    {
        InterlockedIncrement64(&g_plowVisualEventPasses);
        if (applySite == PLOW_VISUAL_APPLY_INLINE_SYNC)
            InterlockedIncrement64(&g_plowVisualInlinePasses);
    }

    LONG after = InterlockedDecrement(&g_plowVisualEventPassesRemaining);
    if (after > 0)
    {
        LONG nextIndex = InterlockedIncrement(&g_plowVisualEventPassIndex);
        LONG64 start = InterlockedCompareExchange64(
            &g_plowVisualEventStartTick, 0, 0);
        ULONGLONG next = now;
        if (start > 0 && nextIndex >= 0 &&
            nextIndex < PLOW_VISUAL_EVENT_PASSES)
        {
            ULONGLONG scheduled = (ULONGLONG)start + passDelayMs[nextIndex];
            if (scheduled > next) next = scheduled;
        }
        InterlockedExchange64(&g_plowVisualEventNextTick, (LONG64)next);
    }
    else
    {
        InterlockedExchange64(&g_plowVisualEventNextTick, 0);
        InterlockedExchange64(&g_plowVisualEventStartTick, 0);
        InterlockedExchange64(
            &g_plowVisualEventCooldownUntilTick,
            (LONG64)(now + PLOW_VISUAL_EVENT_COOLDOWN_MS));
    }
}

static void RecordDeepMaskWrite(DWORD callerRva, bool oldValueValid,
                                unsigned oldGreen, unsigned newGreen)
{
    if (!g_cfg.detailedEvents) return;
    LONG key = (LONG)(callerRva ? callerRva : 0xFFFFFFFFu);
    DeepMaskCallerCounter* counter = NULL;
    for (unsigned i = 0; i < MAX_DEEP_MASK_CALLERS; ++i)
    {
        LONG current = InterlockedCompareExchange(
            &g_deepMaskCaller[i].callerRva, 0, 0);
        if (current == key)
        {
            counter = &g_deepMaskCaller[i];
            break;
        }
        if (!current && InterlockedCompareExchange(
                &g_deepMaskCaller[i].callerRva, key, 0) == 0)
        {
            counter = &g_deepMaskCaller[i];
            break;
        }
    }

    if (!counter)
    {
        InterlockedIncrement64(&g_deepMaskCallerOverflow);
        return;
    }

    InterlockedIncrement64(&counter->writes);
    if (!oldValueValid)
    {
        InterlockedIncrement64(&counter->unreadableBefore);
        return;
    }
    if (newGreen < oldGreen)
    {
        InterlockedIncrement64(&counter->decreases);
        if (!newGreen && oldGreen)
            InterlockedIncrement64(&counter->zeroed);
    }
    else if (newGreen > oldGreen)
    {
        InterlockedIncrement64(&counter->increases);
    }
}

static DeepMaskWriteDelta CaptureDeepMaskWriteDelta()
{
    DeepMaskWriteDelta result = {};
    size_t used = 0;

    for (unsigned i = 0; i < MAX_DEEP_MASK_CALLERS; ++i)
    {
        LONG key = InterlockedCompareExchange(
            &g_deepMaskCaller[i].callerRva, 0, 0);
        if (!key) continue;

        LONG64 writes = InterlockedCompareExchange64(
            &g_deepMaskCaller[i].writes, 0, 0);
        LONG64 decreases = InterlockedCompareExchange64(
            &g_deepMaskCaller[i].decreases, 0, 0);
        LONG64 increases = InterlockedCompareExchange64(
            &g_deepMaskCaller[i].increases, 0, 0);
        LONG64 zeroed = InterlockedCompareExchange64(
            &g_deepMaskCaller[i].zeroed, 0, 0);
        LONG64 unreadable = InterlockedCompareExchange64(
            &g_deepMaskCaller[i].unreadableBefore, 0, 0);

        LONG64 writeDelta = writes - g_deepMaskPrevious[i].writes;
        LONG64 decreaseDelta = decreases - g_deepMaskPrevious[i].decreases;
        LONG64 increaseDelta = increases - g_deepMaskPrevious[i].increases;
        LONG64 zeroedDelta = zeroed - g_deepMaskPrevious[i].zeroed;
        LONG64 unreadableDelta = unreadable -
                                 g_deepMaskPrevious[i].unreadableBefore;
        g_deepMaskPrevious[i].writes = writes;
        g_deepMaskPrevious[i].decreases = decreases;
        g_deepMaskPrevious[i].increases = increases;
        g_deepMaskPrevious[i].zeroed = zeroed;
        g_deepMaskPrevious[i].unreadableBefore = unreadable;
        if (writeDelta <= 0) continue;

        DWORD callerRva = (DWORD)key;
        result.writes += writeDelta;
        result.decreases += decreaseDelta;
        result.increases += increaseDelta;
        result.zeroed += zeroedDelta;
        result.unreadableBefore += unreadableDelta;
        if (callerRva != RVA_ENGINE_EDIT_MASK_SET_TEXEL_RETURN)
            result.outsideEditMask += writeDelta;
        result.callers++;

        if (used + 1 < sizeof(result.callerSummary))
        {
            int written = _snprintf_s(
                result.callerSummary + used,
                sizeof(result.callerSummary) - used, _TRUNCATE,
                "%s%s0x%X:w=%lld,d=%lld,i=%lld,z=%lld,u=%lld",
                used ? "," : "",
                callerRva == 0xFFFFFFFFu ? "external/" : "",
                callerRva,
                (long long)writeDelta, (long long)decreaseDelta,
                (long long)increaseDelta, (long long)zeroedDelta,
                (long long)unreadableDelta);
            if (written > 0 &&
                (size_t)written < sizeof(result.callerSummary) - used)
                used += (size_t)written;
            else
                used = sizeof(result.callerSummary) - 1;
        }
    }

    LONG64 overflow = InterlockedCompareExchange64(
        &g_deepMaskCallerOverflow, 0, 0);
    LONG64 overflowDelta = overflow - g_deepMaskCallerOverflowPrevious;
    g_deepMaskCallerOverflowPrevious = overflow;
    if (overflowDelta > 0 && used + 1 < sizeof(result.callerSummary))
    {
        _snprintf_s(result.callerSummary + used,
                    sizeof(result.callerSummary) - used, _TRUNCATE,
                    "%soverflow:w=%lld", used ? "," : "",
                    (long long)overflowDelta);
    }
    if (!result.callerSummary[0])
        strncpy_s(result.callerSummary, sizeof(result.callerSummary),
                  "none", _TRUNCATE);
    return result;
}

// MaskTextureOpen/Close bracket several terrain-mask operations in the game.
// The hooks only preserve a short in-memory history.  File logging stays on
// the regular update path, avoiding I/O and engine getter calls inside these
// sensitive texture-access functions.
static void RecordMaskTextureCall(char operation, void* terrain,
                                  DWORD callerRva)
{
    if (!g_cfg.detailedEvents) return;
    if (!InterlockedCompareExchange(&g_maskDataLockReady, 0, 0)) return;

    EnterCriticalSection(&g_maskDataLock);
    MaskTextureCallObservation& observation =
        g_recentMaskTextureCalls[g_nextMaskTextureCall];
    observation.tick = GetTickCount64();
    observation.terrain = terrain;
    observation.callerRva = callerRva;
    observation.operation = operation;
    g_nextMaskTextureCall = (g_nextMaskTextureCall + 1) %
                            MAX_RECENT_MASK_TEXTURE_CALLS;
    if (g_maskTextureCallCount < MAX_RECENT_MASK_TEXTURE_CALLS)
        g_maskTextureCallCount++;
    LeaveCriticalSection(&g_maskDataLock);
}

static unsigned DescribeRecentMaskTextureCalls(void* terrain,
                                               ULONGLONG since,
                                               ULONGLONG until,
                                               char* text,
                                               size_t textSize)
{
    if (!text || !textSize) return 0;
    text[0] = 0;
    size_t used = 0;
    unsigned matched = 0;

    EnterCriticalSection(&g_maskDataLock);
    unsigned oldest = (g_nextMaskTextureCall + MAX_RECENT_MASK_TEXTURE_CALLS -
                       g_maskTextureCallCount) % MAX_RECENT_MASK_TEXTURE_CALLS;
    for (unsigned i = 0; i < g_maskTextureCallCount; ++i)
    {
        const MaskTextureCallObservation& observation =
            g_recentMaskTextureCalls[(oldest + i) %
                                     MAX_RECENT_MASK_TEXTURE_CALLS];
        if (observation.tick <= since || observation.tick > until ||
            observation.terrain != terrain)
            continue;

        matched++;
        if (used + 1 >= textSize) continue;
        int written = _snprintf_s(
            text + used, textSize - used, _TRUNCATE,
            "%s%c@%s0x%X+%llums",
            used ? "," : "", observation.operation,
            observation.callerRva == 0xFFFFFFFFu ? "external/" : "",
            observation.callerRva,
            (unsigned long long)(observation.tick - since));
        if (written < 0 || (size_t)written >= textSize - used)
        {
            used = textSize - 1;
            text[used] = 0;
            continue;
        }
        used += (size_t)written;
    }
    LeaveCriticalSection(&g_maskDataLock);

    if (!matched)
        strncpy_s(text, textSize, "none", _TRUNCATE);
    return matched;
}

static void RecordTextureAccessCall(char operation, void* texture,
                                    void* caller,
                                    const TextureStorageState& before,
                                    const TextureStorageState& after,
                                    const TextureAccessRawState& rawBefore,
                                    const TextureAccessRawState& rawAfter,
                                    int result)
{
    if (!g_cfg.detailedEvents) return;
    if (!InterlockedCompareExchange(&g_maskDataLockReady, 0, 0)) return;

    DWORD callerRva = 0xFFFFFFFFu;
    char callerModule = ResolveCallerModule(caller, &callerRva);
    EnterCriticalSection(&g_maskDataLock);
    TextureAccessObservation& observation =
        g_recentTextureAccessCalls[g_nextTextureAccessCall];
    observation.tick = GetTickCount64();
    observation.texture = texture;
    observation.dataBefore = before.data;
    observation.dataAfter = after.data;
    observation.callerRva = callerRva;
    observation.callerModule = callerModule;
    observation.operation = operation;
    observation.result = result;
    observation.rawBeforePoints = rawBefore.points;
    observation.rawAfterPoints = rawAfter.points;
    observation.rawBeforeSum = rawBefore.greenSum;
    observation.rawAfterSum = rawAfter.greenSum;
    g_nextTextureAccessCall = (g_nextTextureAccessCall + 1) %
                              MAX_RECENT_TEXTURE_ACCESS_CALLS;
    if (g_textureAccessCallCount < MAX_RECENT_TEXTURE_ACCESS_CALLS)
        g_textureAccessCallCount++;
    LeaveCriticalSection(&g_maskDataLock);
}

static unsigned DescribeRecentTextureAccessCalls(void* oldTexture,
                                                 void* currentTexture,
                                                 ULONGLONG since,
                                                 ULONGLONG until,
                                                 char* text,
                                                 size_t textSize)
{
    if (!text || !textSize) return 0;
    text[0] = 0;
    size_t used = 0;
    unsigned matched = 0;

    EnterCriticalSection(&g_maskDataLock);
    unsigned oldest =
        (g_nextTextureAccessCall + MAX_RECENT_TEXTURE_ACCESS_CALLS -
         g_textureAccessCallCount) % MAX_RECENT_TEXTURE_ACCESS_CALLS;
    for (unsigned i = 0; i < g_textureAccessCallCount; ++i)
    {
        const TextureAccessObservation& observation =
            g_recentTextureAccessCalls[(oldest + i) %
                                       MAX_RECENT_TEXTURE_ACCESS_CALLS];
        if (observation.tick <= since || observation.tick > until ||
            (observation.texture != oldTexture &&
             observation.texture != currentTexture))
            continue;

        matched++;
        if (used + 1 >= textSize) continue;
        const char* module = observation.callerModule == 'S' ? "exe" :
                             observation.callerModule == 'E' ? "engine" :
                             "external";
        const char* operation = observation.operation == '2' ? "O2" :
                                observation.operation == 'O' ? "O" : "C";
        int written = _snprintf_s(
            text + used, textSize - used, _TRUNCATE,
            "%s%s@%s/0x%X+%llums(data=%p>%p,raw=%u/%u>%u/%u,result=%d)",
            used ? "," : "", operation, module,
            observation.callerRva,
            (unsigned long long)(observation.tick - since),
            observation.dataBefore, observation.dataAfter,
            observation.rawBeforePoints, observation.rawBeforeSum,
            observation.rawAfterPoints, observation.rawAfterSum,
            observation.result);
        if (written < 0 || (size_t)written >= textSize - used)
        {
            used = textSize - 1;
            text[used] = 0;
            continue;
        }
        used += (size_t)written;
    }
    LeaveCriticalSection(&g_maskDataLock);

    if (!matched)
        strncpy_s(text, textSize, "none", _TRUNCATE);
    return matched;
}

static bool ReadMaskPixel(void* terrain, const TextureStorageState& storage,
                          const float pos[3], MaskColor* out)
{
    if (!out || WeatherPersistenceSuspended()) return false;
    RawMaskSample raw = {};
    if (!ReadRawMaskSample(terrain, storage, pos, &raw)) return false;
    // Diagnostic only: read one already mapped texel. Never enter the engine
    // getter or open/map a texture just to obtain a periodic observation.
    BYTE* pixel = (BYTE*)storage.data + (SIZE_T)raw.y * storage.rowPitch + raw.x * 4;
    __try
    {
        out->value[0] = pixel[2] / 255.0f;
        out->value[1] = pixel[1] / 255.0f;
        out->value[2] = pixel[0] / 255.0f;
        out->value[3] = pixel[3] / 255.0f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return ValidMaskColor(*out);
}

struct MaskPointSnapshot
{
    int slot;
    unsigned id;
    float pos[3];
    int hasLoggedValue;
    MaskColor previousLogged;
    unsigned paints;
    unsigned loggedPaints;
    ULONGLONG lastPaintTick;
    MaskColor current;
    int sampleValid;
    float component1Delta;
    RawMaskSample previousRaw;
    RawMaskSample currentRaw;
    ULONGLONG previousRawTick;
    LONG64 lastPaintGameMinuteMilli;
    float clearReferenceGreen;
    int hasClearReference;
    int visualOnlySnowLogged;
};

static void SampleTrackedMaskPoints(void* weatherWorld, ULONGLONG now)
{
    if (!weatherWorld || WeatherPersistenceSuspended()) return;
    if (g_nextMaskSampleTick && now < g_nextMaskSampleTick) return;
    g_nextMaskSampleTick = now + (ULONGLONG)g_cfg.maskSampleIntervalMs;

    void* gameWorld = NULL;
    void* terrain = NULL;
    void* maskTexture = NULL;
    int precipitationState = -1;
    BYTE* worldSlot = g_exeBase + RVA_GLOBAL_WORLD_PTR;
    if (!ReadablePtr(worldSlot, sizeof(void*)) ||
        !ReadablePtr(weatherWorld, OFF_PRECIPITATION_STATE + sizeof(int)))
        return;
    __try
    {
        gameWorld = *(void**)worldSlot;
        precipitationState = *(int*)((BYTE*)weatherWorld +
                                      OFF_PRECIPITATION_STATE);
    }
    __except (FaultFilter(PLUGIN_NAME " global world pointer sample",
                           GetExceptionInformation()))
    {
        return;
    }
    if (!gameWorld || !ReadablePtr(gameWorld, OFF_TERRAIN + sizeof(void*)))
        return;

    __try
    {
        terrain = *(void**)((BYTE*)gameWorld + OFF_TERRAIN);
    }
    __except (FaultFilter(PLUGIN_NAME " terrain pointer sample",
                           GetExceptionInformation()))
    {
        return;
    }
    if (!terrain || !ReadablePtr(terrain, MIN_TERRAIN_READ_SIZE)) return;

    // Resolve the current terrain's texture, never a previous world's object.
    __try
    {
        maskTexture = *(void**)((BYTE*)terrain + OFF_TERRAIN_MASK_TEXTURE);
    }
    __except (FaultFilter(PLUGIN_NAME " terrain mask texture sample",
                           GetExceptionInformation()))
    {
        return;
    }
    if (!maskTexture || !ReadablePtr(maskTexture, 0x1C)) return;
    InterlockedExchangePointer(&g_terrainMaskTexture, maskTexture);

    // Visible correction is event-driven in v0.1.38. A permanent full-mask
    // readback here caused redundant work between snow impulses and could
    // visibly pull an already rendered mask backwards on the next sample.
    FlushExperimentalLimiterStatus(now);
    FlushPlowVisualStatus(now);
    // Only identity publication and operational warnings are needed in beta.
    // Raw-point snapshots/caller correlation below are diagnostic only.
    if (!g_cfg.detailedEvents) return;

    TextureStorageState currentStorage = {};
    ReadTextureStorageState(maskTexture, &currentStorage);
    TextureStorageState previousStorage = g_previousMaskStorage;
    int hadPreviousStorage = g_hasPreviousMaskStorage;
    if (currentStorage.valid)
    {
        g_previousMaskStorage = currentStorage;
        g_hasPreviousMaskStorage = 1;
    }

    MaskPointSnapshot points[MAX_TRACKED_MASK_POINTS] = {};
    int pointCount = 0;
    int trackedRoadCount = 0;
    int trackedSnowedRoadCount = 0;
    EnterCriticalSection(&g_maskDataLock);
    int limit = g_cfg.maximumTrackedMaskPoints;
    if (limit > MAX_TRACKED_MASK_POINTS) limit = MAX_TRACKED_MASK_POINTS;
    for (int i = 0; i < limit; ++i)
    {
        if (!g_maskPoint[i].active) continue;
        points[pointCount].slot = i;
        points[pointCount].id = g_maskPoint[i].id;
        memcpy(points[pointCount].pos, g_maskPoint[i].pos,
               sizeof(points[pointCount].pos));
        points[pointCount].hasLoggedValue = g_maskPoint[i].hasLoggedValue;
        points[pointCount].previousLogged = g_maskPoint[i].lastLoggedValue;
        points[pointCount].paints = g_maskPoint[i].paints;
        points[pointCount].loggedPaints = g_maskPoint[i].loggedPaints;
        points[pointCount].lastPaintTick = g_maskPoint[i].lastPaintTick;
        points[pointCount].previousRaw.valid =
            g_maskPoint[i].hasRawSample;
        points[pointCount].previousRaw.texture =
            g_maskPoint[i].lastRawTexture;
        points[pointCount].previousRaw.data =
            g_maskPoint[i].lastRawData;
        points[pointCount].previousRaw.x = g_maskPoint[i].lastRawX;
        points[pointCount].previousRaw.y = g_maskPoint[i].lastRawY;
        memcpy(points[pointCount].previousRaw.green,
               g_maskPoint[i].lastRawGreen,
               sizeof(points[pointCount].previousRaw.green));
        points[pointCount].previousRawTick =
            g_maskPoint[i].lastRawSampleTick;
        points[pointCount].lastPaintGameMinuteMilli =
            g_maskPoint[i].lastPaintGameMinuteMilli;
        points[pointCount].clearReferenceGreen =
            g_maskPoint[i].clearReferenceGreen;
        points[pointCount].hasClearReference =
            g_maskPoint[i].hasClearReference;
        points[pointCount].visualOnlySnowLogged =
            g_maskPoint[i].visualOnlySnowLogged;
        pointCount++;
    }
    int roadLimit = g_cfg.maximumTrackedRoads;
    if (roadLimit > MAX_TRACKED_ROADS) roadLimit = MAX_TRACKED_ROADS;
    for (int i = 0; i < roadLimit; ++i)
    {
        if (!g_trackedRoads[i].active) continue;
        trackedRoadCount++;
        if (g_trackedRoads[i].sum > 0) trackedSnowedRoadCount++;
    }
    LeaveCriticalSection(&g_maskDataLock);

    ULONGLONG correlationStart = g_lastMaskPointSampleTick;
    if (!correlationStart || correlationStart > now)
    {
        ULONGLONG fallbackWindow =
            (ULONGLONG)g_cfg.maskSampleIntervalMs * 2ULL;
        correlationStart = now > fallbackWindow ? now - fallbackWindow : 0;
    }
    g_lastMaskPointSampleTick = now;

    int sampledCount = 0;
    int coordinatedDecreaseCount = 0;
    float coordinatedDecreaseSum = 0.0f;
    float coordinatedDecreaseMin = 0.0f;
    float coordinatedDecreaseMax = 0.0f;

    // Read every selected point first.  Analysing the set as one observation
    // lets us distinguish an isolated change from a simultaneous drop across
    // multiple positions that were not repainted by the snowplow.
    for (int i = 0; i < pointCount; ++i)
    {
        if (!ReadMaskPixel(terrain, currentStorage, points[i].pos, &points[i].current)) continue;

        points[i].sampleValid = 1;
        ReadRawMaskSample(terrain, currentStorage, points[i].pos,
                          &points[i].currentRaw);
        sampledCount++;
        if (!points[i].hasLoggedValue) continue;

        points[i].component1Delta = points[i].current.value[1] -
                                    points[i].previousLogged.value[1];
        bool noNewPaint = points[i].paints == points[i].loggedPaints;
        if (noNewPaint &&
            points[i].component1Delta <= -MASK_CHANGE_EPSILON)
        {
            float delta = points[i].component1Delta;
            if (!coordinatedDecreaseCount)
                coordinatedDecreaseMin = coordinatedDecreaseMax = delta;
            else
            {
                if (delta < coordinatedDecreaseMin) coordinatedDecreaseMin = delta;
                if (delta > coordinatedDecreaseMax) coordinatedDecreaseMax = delta;
            }
            coordinatedDecreaseCount++;
            coordinatedDecreaseSum += delta;
        }
    }

    DeepMaskWriteDelta deepWrites = CaptureDeepMaskWriteDelta();
    bool coordinatedDecrease = coordinatedDecreaseCount >=
                               COORDINATED_MASK_POINT_MINIMUM;
    if (coordinatedDecrease)
    {
        char textureTrace[2048];
        unsigned textureCalls = DescribeRecentMaskTextureCalls(
            terrain, correlationStart, now, textureTrace,
            sizeof(textureTrace));
        char accessTrace[3072];
        void* previousTexture = hadPreviousStorage
                              ? previousStorage.texture : maskTexture;
        unsigned accessCalls = DescribeRecentTextureAccessCalls(
            previousTexture, maskTexture, correlationStart, now,
            accessTrace, sizeof(accessTrace));

        int rawComparable = 0;
        int rawDecreases = 0;
        int rawIncreases = 0;
        int rawZeroed = 0;
        int rawUnchanged = 0;
        int rawUnavailable = 0;
        char rawTrace[1536] = {};
        size_t rawTraceUsed = 0;
        for (int i = 0; i < pointCount; ++i)
        {
            bool contributed = points[i].sampleValid &&
                points[i].hasLoggedValue &&
                points[i].paints == points[i].loggedPaints &&
                points[i].component1Delta <= -MASK_CHANGE_EPSILON;
            if (!contributed) continue;

            const RawMaskSample& before = points[i].previousRaw;
            const RawMaskSample& after = points[i].currentRaw;
            bool comparable = before.valid && after.valid &&
                points[i].previousRawTick >= correlationStart &&
                before.texture == after.texture &&
                before.data == after.data &&
                before.x == after.x && before.y == after.y;
            if (!comparable)
            {
                rawUnavailable++;
                continue;
            }

            unsigned beforeSum = RawGreenSum(before);
            unsigned afterSum = RawGreenSum(after);
            rawComparable++;
            if (afterSum < beforeSum)
            {
                rawDecreases++;
                if (!afterSum && beforeSum) rawZeroed++;
            }
            else if (afterSum > beforeSum)
                rawIncreases++;
            else
                rawUnchanged++;

            if (rawTraceUsed + 1 < sizeof(rawTrace))
            {
                int written = _snprintf_s(
                    rawTrace + rawTraceUsed,
                    sizeof(rawTrace) - rawTraceUsed, _TRUNCATE,
                    "%sp%u:%u>%u", rawTraceUsed ? "," : "",
                    points[i].id, beforeSum, afterSum);
                if (written > 0 &&
                    (size_t)written < sizeof(rawTrace) - rawTraceUsed)
                    rawTraceUsed += (size_t)written;
                else
                    rawTraceUsed = sizeof(rawTrace) - 1;
            }
        }
        if (!rawTrace[0])
            strncpy_s(rawTrace, sizeof(rawTrace), "none", _TRUNCATE);

        bool textureChanged = hadPreviousStorage && currentStorage.valid &&
            previousStorage.texture != currentStorage.texture;
        bool dataChanged = hadPreviousStorage && currentStorage.valid &&
            previousStorage.data != currentStorage.data;
        bool resourceChanged = hadPreviousStorage && currentStorage.valid &&
            previousStorage.resource != currentStorage.resource;
        bool accessResourceChanged = hadPreviousStorage &&
            currentStorage.valid && previousStorage.accessResource !=
            currentStorage.accessResource;
        bool dimensionsChanged = hadPreviousStorage && currentStorage.valid &&
            (previousStorage.width != currentStorage.width ||
             previousStorage.height != currentStorage.height ||
             previousStorage.rowPitch != currentStorage.rowPitch);
        const char* storageClassification = "insufficient_raw_data";
        if (textureChanged)
            storageClassification = "texture_object_replaced";
        else if (resourceChanged || accessResourceChanged ||
                 dimensionsChanged)
            storageClassification = "backing_storage_changed";
        else if (dataChanged)
            storageClassification = "mapped_data_pointer_changed";
        else if (rawDecreases >= COORDINATED_MASK_POINT_MINIMUM)
            storageClassification = rawZeroed == rawDecreases
                                  ? "same_buffer_bulk_zero"
                                  : "same_buffer_bulk_decrease";
        else if (rawComparable >= COORDINATED_MASK_POINT_MINIMUM)
            storageClassification = "getter_change_without_raw_change";
        else if (hadPreviousStorage && currentStorage.valid)
            storageClassification = "stable_storage_non_settexel_reset";

        Event("terrain-mask coordinated decrease: points=%d sampled=%d "
              "without_new_paint=%d precipitation_state=%d "
              "mask_component1_delta_sum=%+.5f delta_range=[%+.5f..%+.5f] "
              "classification=road_recoverage",
              coordinatedDecreaseCount, sampledCount,
              coordinatedDecreaseCount, precipitationState,
              (double)coordinatedDecreaseSum,
              (double)coordinatedDecreaseMin,
              (double)coordinatedDecreaseMax);
        Event("mask-texture correlation: trigger=road_recoverage "
              "window_ms=%llu calls=%u trace=[%s]",
              (unsigned long long)(now - correlationStart),
              textureCalls, textureTrace);
        Event("texture-access correlation: trigger=road_recoverage "
              "window_ms=%llu calls=%u trace=[%s]",
              (unsigned long long)(now - correlationStart),
              accessCalls, accessTrace);
        Event("terrain-mask writer correlation: trigger=road_recoverage "
              "window_ms=%llu writes=%lld decreases=%lld increases=%lld "
              "zeroed=%lld unreadable_before=%lld outside_editmask=%lld "
              "callers=[%s]",
              (unsigned long long)(now - correlationStart),
              (long long)deepWrites.writes,
              (long long)deepWrites.decreases,
              (long long)deepWrites.increases,
              (long long)deepWrites.zeroed,
              (long long)deepWrites.unreadableBefore,
              (long long)deepWrites.outsideEditMask,
              deepWrites.callerSummary);
        Event("terrain-mask storage correlation: trigger=road_recoverage "
              "classification=%s previous_valid=%d current_valid=%d "
              "texture_changed=%d data_changed=%d resource_changed=%d "
              "access_resource_changed=%d dimensions_changed=%d "
              "raw_comparable=%d raw_decreases=%d raw_increases=%d "
              "raw_zeroed=%d raw_unchanged=%d raw_unavailable=%d "
              "texture=%p data=%p size=%dx%d pitch=%u mapped=%u "
              "raw_trace=[%s]",
              storageClassification, hadPreviousStorage,
              currentStorage.valid, textureChanged ? 1 : 0,
              dataChanged ? 1 : 0, resourceChanged ? 1 : 0,
              accessResourceChanged ? 1 : 0,
              dimensionsChanged ? 1 : 0, rawComparable, rawDecreases,
              rawIncreases, rawZeroed, rawUnchanged, rawUnavailable,
              currentStorage.texture, currentStorage.data,
              currentStorage.width, currentStorage.height,
              currentStorage.rowPitch, currentStorage.accessOpen,
              rawTrace);
    }
    else if (deepWrites.writes &&
             (deepWrites.decreases || deepWrites.outsideEditMask))
    {
        Event("terrain-mask writer activity: window_ms=%llu writes=%lld "
              "decreases=%lld increases=%lld zeroed=%lld "
              "unreadable_before=%lld outside_editmask=%lld callers=[%s]",
              (unsigned long long)(now - correlationStart),
              (long long)deepWrites.writes,
              (long long)deepWrites.decreases,
              (long long)deepWrites.increases,
              (long long)deepWrites.zeroed,
              (long long)deepWrites.unreadableBefore,
              (long long)deepWrites.outsideEditMask,
              deepWrites.callerSummary);
    }

    for (int i = 0; i < pointCount; ++i)
    {
        if (!points[i].sampleValid) continue;
        const MaskColor& current = points[i].current;

        bool shouldLog = false;
        bool initialLog = false;
        const char* reason = "heartbeat";
        const char* change = "none";
        MaskColor previousLogged = {};
        ULONGLONG paintTick = now;
        unsigned paints = 0;
        unsigned newPaints = 0;
        float component1Delta = 0.0f;
        bool visualOnlySnow = false;
        float clearReferenceGreen = 0.0f;
        LONG64 paintGameMinuteMilli = 0;

        EnterCriticalSection(&g_maskDataLock);
        TrackedMaskPoint& point = g_maskPoint[points[i].slot];
        if (point.active && point.id == points[i].id)
        {
            bool initial = !point.hasLoggedValue;
            bool changed = false;
            if (point.hasLoggedValue)
            {
                for (unsigned component = 0; component < 4; ++component)
                    if (AbsMaskFloat(current.value[component] -
                                     point.lastLoggedValue.value[component]) >=
                        MASK_CHANGE_EPSILON)
                        changed = true;
            }
            bool heartbeat = g_cfg.maskSampleHeartbeatSeconds > 0 &&
                (!point.lastLogTick || now - point.lastLogTick >=
                 (ULONGLONG)g_cfg.maskSampleHeartbeatSeconds * 1000ULL);

            previousLogged = point.lastLoggedValue;
            point.lastSample = current;
            point.hasSample = 1;
            point.lastSampleTick = now;
            if (points[i].currentRaw.valid)
            {
                point.hasRawSample = 1;
                point.lastRawTexture = points[i].currentRaw.texture;
                point.lastRawData = points[i].currentRaw.data;
                point.lastRawX = points[i].currentRaw.x;
                point.lastRawY = points[i].currentRaw.y;
                memcpy(point.lastRawGreen, points[i].currentRaw.green,
                       sizeof(point.lastRawGreen));
                point.lastRawSampleTick = now;
            }
            paintTick = point.lastPaintTick;
            paints = point.paints;
            newPaints = point.paints >= point.loggedPaints
                      ? point.paints - point.loggedPaints : 0;
            component1Delta = initial ? 0.0f
                : current.value[1] - previousLogged.value[1];

            // The first post-plow sample is the visual clear reference for
            // this world position. A later lower value while every tracked
            // road vector is internally clear is visual-only snow: it can be
            // seen, but it cannot slow traffic or dispatch a snowplow.
            if (!point.hasClearReference || initial || newPaints)
            {
                point.clearReferenceGreen = current.value[1];
                point.hasClearReference = 1;
                point.visualOnlySnowLogged = 0;
            }
            else if (current.value[1] > point.clearReferenceGreen)
            {
                point.clearReferenceGreen = current.value[1];
                point.visualOnlySnowLogged = 0;
            }
            bool visualOnlyNow = trackedRoadCount > 0 &&
                trackedSnowedRoadCount == 0 && point.hasClearReference &&
                current.value[1] <= point.clearReferenceGreen -
                    MASK_CHANGE_EPSILON;
            if (visualOnlyNow && !point.visualOnlySnowLogged)
            {
                point.visualOnlySnowLogged = 1;
                visualOnlySnow = true;
                InterlockedIncrement64(&g_plowVisualOnlySnowEvents);
            }
            else if (!visualOnlyNow)
            {
                point.visualOnlySnowLogged = 0;
            }
            clearReferenceGreen = point.clearReferenceGreen;
            paintGameMinuteMilli = point.lastPaintGameMinuteMilli;

            if (initial || changed || heartbeat || newPaints)
            {
                shouldLog = true;
                initialLog = initial;
                reason = initial ? "initial" : (changed ? "changed" : "heartbeat");
                if (initial)
                    change = "initial";
                else if (AbsMaskFloat(component1Delta) >= MASK_CHANGE_EPSILON)
                {
                    if (component1Delta > 0.0f)
                        change = newPaints ? "plow_increase"
                                           : "unattributed_increase";
                    else if (newPaints)
                        change = "plow_overlap_decrease";
                    else if (coordinatedDecrease)
                        change = "coordinated_road_recoverage";
                    else
                        change = "possible_road_recoverage";
                }
                else if (changed)
                    change = "other_component_change";
                point.lastLoggedValue = current;
                point.hasLoggedValue = 1;
                point.lastLogTick = now;
                point.loggedPaints = point.paints;
            }
        }
        LeaveCriticalSection(&g_maskDataLock);

        if (visualOnlySnow)
        {
            LONG64 gameNow = InterlockedCompareExchange64(
                &g_gameMinuteMilli, 0, 0);
            LONG64 gameAge = gameNow >= paintGameMinuteMilli
                           ? gameNow - paintGameMinuteMilli : -1;
            LONG64 protectionDuration = PlowProtectionDuration();
            LONG64 totalDuration = protectionDuration + PlowSaltDuration();
            const char* phase = gameAge < 0 ? "unknown" :
                (gameAge < protectionDuration ? "protection" :
                 (gameAge < totalDuration ? "salt" : "expired"));
            Event("visual-only road snow: point=%u internal_active_roads=%d "
                  "internal_snowed_roads=%d clear_component1=%.5f "
                  "current_component1=%.5f deficit=%.5f effect_phase=%s "
                  "game_age_minutes=%.3f pos=[%.3f,%.3f,%.3f]",
                  points[i].id, trackedRoadCount, trackedSnowedRoadCount,
                  (double)clearReferenceGreen, (double)current.value[1],
                  (double)(clearReferenceGreen - current.value[1]), phase,
                  gameAge >= 0 ? (double)gameAge / 1000.0 : -1.0,
                  (double)points[i].pos[0], (double)points[i].pos[1],
                  (double)points[i].pos[2]);
        }

        if (shouldLog)
        {
            // EditMask's channel number 2 maps to C3DFCOLOR component 1 (the
            // green RGBA value), as confirmed by the v0.1.6 runtime samples.
            // Initial samples intentionally report a zero delta.
            if (initialLog) component1Delta = 0.0f;
            Event("terrain-mask sample: point=%u reason=%s age_since_paint_ms=%llu "
                  "paints=%u new_paints=%u precipitation_state=%d "
                  "pos=[%.3f,%.3f,%.3f] rgba=[%.5f,%.5f,%.5f,%.5f] "
                  "editmask_channel=2 mask_component1=%.5f "
                  "component1_delta=%+.5f change=%s",
                  points[i].id, reason,
                  (unsigned long long)(now >= paintTick ? now - paintTick : 0),
                  paints, newPaints, precipitationState,
                  (double)points[i].pos[0], (double)points[i].pos[1],
                  (double)points[i].pos[2],
                  (double)current.value[0], (double)current.value[1],
                  (double)current.value[2], (double)current.value[3],
                  (double)current.value[1], (double)component1Delta, change);
        }
    }
}

static void TickTerrainMaskObservation(void* world)
{
    if (WeatherPersistenceSuspended() ||
        !InterlockedCompareExchange(&g_maskDataLockReady, 0, 0)) return;
    ULONGLONG now = GetTickCount64();
    ServiceGradualRoadSnow(world, now);
    FlushMaskCallBatch(now);
    ServicePlowVisualEventSync(now);
    SampleTrackedRoadSnow(now);
    SampleTrackedMaskPoints(world, now);
}

// ------------------------------------------------------------- weather probe

static void ReleaseTrackedRoadSnow(TrackedRoadSnow* tracked);

static void ResetDiagnosticWorldState()
{
    if (InterlockedCompareExchange(&g_maskDataLockReady, 0, 0))
    {
        EnterCriticalSection(&g_maskDataLock);
        memset(g_maskPoint, 0, sizeof(g_maskPoint));
        memset(&g_maskBatch, 0, sizeof(g_maskBatch));
        memset(g_recentMaskTextureCalls, 0, sizeof(g_recentMaskTextureCalls));
        memset(g_recentTextureAccessCalls, 0, sizeof(g_recentTextureAccessCalls));
        g_nextMaskTextureCall = g_maskTextureCallCount = 0;
        g_nextTextureAccessCall = g_textureAccessCallCount = 0;
        for (int i = 0; i < MAX_TRACKED_ROADS; ++i) ReleaseTrackedRoadSnow(&g_trackedRoads[i]);
        LeaveCriticalSection(&g_maskDataLock);
    }
    memset(&g_previousMaskStorage, 0, sizeof(g_previousMaskStorage));
    g_hasPreviousMaskStorage = 0;
    g_nextMaskSampleTick = g_lastMaskPointSampleTick = g_nextMaskBatchTick = 0;
    InterlockedExchangePointer(&g_terrainMaskTexture, NULL);
    InterlockedExchange64(&g_lastVerifiedPlowMaskTick, 0);
}

// The game represents one day as 0..60. Convert that value to thousandths of
// an in-game minute so protection durations remain independent of real time,
// frame rate and pauses. A new world pointer or a backwards time jump starts a
// new registry generation, making stale road pointers unreachable.
static void PublishGameClock(void* world, void* previousWorld)
{
    if (!world || !ReadablePtr(world, OFF_DAYTIME + sizeof(float))) return;

    int day = 0;
    int year = 0;
    float time = 0.0f;
    __try
    {
        BYTE* w = (BYTE*)world;
        day = *(int*)(w + OFF_DAY);
        year = *(int*)(w + OFF_YEAR);
        time = *(float*)(w + OFF_DAYTIME);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }

    if (day < -1 || day > 366 || year < 0 || year > 100000 ||
        !_finite(time) || time < 0.0f || time > 60.1f)
        return;

    double absoluteMinutes =
        ((double)year * 367.0 + (double)day) * 1440.0 +
        (double)time * 24.0;
    LONG64 current = (LONG64)(absoluteMinutes * 1000.0 + 0.5);
    LONG64 previous = InterlockedCompareExchange64(&g_gameMinuteMilli, 0, 0);
    bool newGeneration = WeatherPersistenceConsumeReset() || previousWorld != world;
    if (!newGeneration && previous && current + 60000LL < previous)
        newGeneration = true;
    if (newGeneration)
    {
        InterlockedIncrement(&g_gameWorldGeneration);
        ResetDiagnosticWorldState();
        if (InterlockedCompareExchange(&g_plowEffectLockReady, 0, 0))
        {
            EnterCriticalSection(&g_plowEffectLock);
            memset(g_plowEffectPoints, 0, sizeof(g_plowEffectPoints));
            LeaveCriticalSection(&g_plowEffectLock);
        }
        AcquireSRWLockExclusive(&g_pendingVisibleBrushLock);
        memset(g_pendingVisibleBrushes, 0,
               sizeof(g_pendingVisibleBrushes));
        memset(g_visibleBrushTracks, 0,
               sizeof(g_visibleBrushTracks));
        memset(&g_recentVisibleClear, 0,
               sizeof(g_recentVisibleClear));
        ReleaseSRWLockExclusive(&g_pendingVisibleBrushLock);
        if (InterlockedCompareExchange(&g_maskDataLockReady, 0, 0))
        {
            EnterCriticalSection(&g_maskDataLock);
            memset(g_pendingPlowPoints, 0, sizeof(g_pendingPlowPoints));
            LeaveCriticalSection(&g_maskDataLock);
        }
        InterlockedExchange(&g_plowVisualEventPassesRemaining, 0);
        InterlockedExchange(&g_plowVisualEventPassIndex, 0);
        InterlockedExchange(&g_plowVisualEventHasSnow, 0);
        InterlockedExchange64(&g_plowVisualEventNextTick, 0);
        InterlockedExchange64(&g_plowVisualEventStartTick, 0);
        InterlockedExchange64(&g_plowVisualEventCooldownUntilTick, 0);
        InterlockedExchange64(&g_plowVisualEventFirstCorrectionMs, -1);
        InterlockedExchange64(&g_plowVisualPendingMeltUnits, 0);
        InterlockedExchange(&g_overlayTrackedRoads, 0);
        InterlockedExchange64(&g_overlayTrackedBytes, 0);
        InterlockedExchange64(&g_overlayTrackedSnowSum, 0);
        InterlockedExchange(&g_overlayTrackedSnowMaximum, 0);
        InterlockedExchange64(&g_overlayVisualSnowSum, 0);
        InterlockedExchange64(&g_overlayVisualSnowCount, 0);
        InterlockedExchange(&g_overlayVisualSnowMaximum, 0);
        InterlockedExchange(&g_overlayLastCorrectionMs, -1);
        InterlockedExchange64(&g_overlayLastDuplicateSnowWrites, 0);
        InterlockedExchange(&g_overlaySnowDeltaValid, 0);
        InterlockedExchange(&g_overlaySnowDeltaRaw, 0);
        InterlockedExchange(&g_overlaySnowDeltaEffective, 0);
        ResetInternalSnowBurstState();
        ResetGradualSnowState();
        ResetPlowVisualLogicalSnowBurst();
    }
    InterlockedExchange64(&g_gameMinuteMilli, current);
}

// Called from the generated call-site stub on the game's update thread.  The
// pointer capture stays tiny; the terrain-mask observation is strictly
// rate-limited and uses the same engine getter the game itself calls.
static __declspec(noinline) void CaptureWeatherWorld(void* world)
{
    if (!InterlockedCompareExchange(&g_active, 0, 0) || !world)
        return;

    void* previousWorld = InterlockedExchangePointer(&g_weatherWorld, world);
    PublishGameClock(world, previousWorld);
    WeatherPersistenceTick(world);
    InterlockedExchange64(&g_weatherCaptureTick, (LONG64)GetTickCount64());
    InterlockedIncrement64(&g_weatherCaptureCount);
    InterlockedExchange(&g_weatherThreadId, (LONG)GetCurrentThreadId());
}

// Runs after the untouched weather tick. This ordering is important for the
// gradual mask experiment: verified +30 callers first queue their internal
// snow and clear the vanilla all-at-once mask request. A due one-unit release
// can then publish exactly one replacement request which survives until the
// renderer consumes it later in the same update.
static __declspec(noinline) void CompleteWeatherWorldUpdate(void* world)
{
    if (!InterlockedCompareExchange(&g_active, 0, 0) || !world ||
        world != InterlockedCompareExchangePointer(
            &g_weatherWorld, NULL, NULL))
        return;
    TickTerrainMaskObservation(world);
}

static void WarnInvalidWeatherOnce(const WeatherSnapshot& s)
{
    if (InterlockedCompareExchange(&g_invalidWeatherLogged, 1, 0) != 0)
        return;

    Warn("captured weather object produced implausible core values and was "
         "refused: "
         "year=%d day=%d time=%.3f weather_mode=%d precipitation_state=%d "
         "lighting=%d night=%.3f",
         s.year, s.day, s.time, s.weatherMode, s.precipitationState, s.weather,
         s.night);
}


static bool ReadWeatherSnapshot(WeatherSnapshot* out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    LONG64 captureTick = InterlockedCompareExchange64(&g_weatherCaptureTick, 0, 0);
    ULONGLONG now = GetTickCount64();
    ULONGLONG maximumAge = (ULONGLONG)g_cfg.sampleIntervalMs * 10ULL;
    // Menus, pauses and loading transitions can suspend the regular weather
    // caller for several seconds while the world is still valid.  A generous
    // floor avoids misreporting those short player-controlled pauses as an
    // unloaded world.
    if (maximumAge < 30000ULL) maximumAge = 30000ULL;
    if (!captureTick || now - (ULONGLONG)captureTick > maximumAge)
        return false;

    BYTE* world = (BYTE*)InterlockedCompareExchangePointer(
        &g_weatherWorld, NULL, NULL);

    if (!world || !ReadablePtr(world, OFF_NIGHT + sizeof(float)))
        return false;

    __try
    {
        out->day         = *(int*)  (world + OFF_DAY);
        out->year        = *(int*)  (world + OFF_YEAR);
        out->time        = *(float*)(world + OFF_DAYTIME);
        out->weatherMode = *(int*)  (world + OFF_WEATHER_ON);
        out->precipitationState = *(int*)(world + OFF_PRECIPITATION_STATE);
        out->weather     = *(int*)  (world + OFF_WEATHER);
        out->night       = *(float*)(world + OFF_NIGHT);
    }
    __except (FaultFilter(PLUGIN_NAME " weather snapshot", GetExceptionInformation()))
    {
        if (InterlockedCompareExchange(&g_faultLogged, 1, 0) == 0)
            Warn("fault while reading the world object; weather sampling will retry");
        return false;
    }

    bool corePlausible =
        out->day >= -1 && out->day <= 366 &&
        out->year >= 0 && out->year <= 100000 &&
        out->weatherMode >= 0 && out->weatherMode <= 16 &&
        out->precipitationState >= 0 && out->precipitationState <= 7 &&  // roll 0..7, see OFF_PRECIPITATION_STATE
        out->weather >= 0 && out->weather <= 3 &&
        _finite(out->time) && out->time >= -0.01f && out->time <= 60.01f &&
        _finite(out->night) && out->night >= -0.01f && out->night <= 1.01f;
    if (!corePlausible)
    {
        WarnInvalidWeatherOnce(*out);
        return false;
    }

    out->valid = 1;
    return true;
}

static const char* WeatherName(int value)
{
    static const char* names[] = {
        "day1", "sunset2", "night", "overcast1"
    };
    return (value >= 0 && value < 4) ? names[value] : "unknown";
}

// Name of a winter weather roll for the logs and the overlay (English).
static const char* WeatherRollName(int state)
{
    if (state == 1) return "snow";
    if (state == 0) return "dry";
    if (state >= 2 && state <= 7) return "no snow";
    return "unknown";
}

static bool WeatherChanged(const WeatherSnapshot& a,
                           const WeatherSnapshot& b)
{
    return a.day != b.day || a.year != b.year ||
           a.weatherMode != b.weatherMode ||
           a.precipitationState != b.precipitationState ||
           a.weather != b.weather;
}

static void LogWeather(const char* reason, const WeatherSnapshot& s)
{
    Event("weather %s: year=%d day=%d time=%.3f/60, weather_mode=%d "
          "(%s), precipitation_state=%d (%s), lighting=%d (%s), night=%.3f",
          reason, s.year, s.day, s.time, s.weatherMode,
          s.weatherMode ? "active" : "inactive", s.precipitationState,
          WeatherRollName(s.precipitationState),
          s.weather, WeatherName(s.weather), s.night);
}

static DWORD WINAPI WeatherWorker(LPVOID)
{
    WeatherSnapshot previous = {};
    ULONGLONG heartbeatAt = 0;

    while (InterlockedCompareExchange(&g_active, 0, 0))
    {
        FlushInternalSnowLimiterStatus();
        FlushNaturalMeltingStatus();
        FlushPlowEffectStatus();

        WeatherSnapshot current = {};
        bool valid = g_cfg.weatherProbe && ReadWeatherSnapshot(&current);

        if (g_cfg.weatherProbe && valid)
        {
            InterlockedExchange(&g_overlayPrecipitationState,
                                current.precipitationState);
            InterlockedExchange(&g_overlayWeatherValid, 1);
            ULONGLONG now = GetTickCount64();
            if (!previous.valid)
            {
                LogWeather("world loaded", current);
                heartbeatAt = now;
            }
            else if (WeatherChanged(previous, current))
            {
                LogWeather("changed", current);
                heartbeatAt = now;
            }
            else if (g_cfg.weatherHeartbeatSeconds > 0 &&
                     now - heartbeatAt >=
                         (ULONGLONG)g_cfg.weatherHeartbeatSeconds * 1000ULL)
            {
                LogWeather("heartbeat", current);
                heartbeatAt = now;
            }
            previous = current;
        }
        else if (g_cfg.weatherProbe && previous.valid)
        {
            InterlockedExchange(&g_overlayWeatherValid, 0);
            Event("weather tick unavailable for at least 30 seconds; the game "
                  "may be paused, in a menu, loading, or without a world");
            memset(&previous, 0, sizeof(previous));
        }

        DWORD wait = WaitForSingleObject(g_stopEvent,
                                         (DWORD)g_cfg.sampleIntervalMs);
        if (!WorkerWaitContinues(wait,"weather worker")) break;
    }

    Event("weather worker stopped");
    return 0;
}

// ---------------------------------------------------------- compact overlay

#define OVERLAY_CLASS_NAME "TesmioWeatherRoadsOverlay"
#define OVERLAY_WINDOW_HEIGHT 244

struct GameWindowSearch
{
    HWND window;
    LONG64 area;
};

static BOOL CALLBACK FindGameWindowCallback(HWND window, LPARAM parameter)
{
    GameWindowSearch* search = (GameWindowSearch*)parameter;
    if (!search || !IsWindowVisible(window) || IsIconic(window) ||
        window == g_overlayWindow)
        return TRUE;

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId()) return TRUE;

    RECT client = {};
    if (!GetClientRect(window, &client)) return TRUE;
    LONG64 width = (LONG64)client.right - client.left;
    LONG64 height = (LONG64)client.bottom - client.top;
    LONG64 area = width > 0 && height > 0 ? width * height : 0;
    if (area > search->area)
    {
        search->window = window;
        search->area = area;
    }
    return TRUE;
}

static HWND FindGameWindow(void)
{
    HWND foreground = GetForegroundWindow();
    if (foreground && foreground != g_overlayWindow)
    {
        DWORD processId = 0;
        GetWindowThreadProcessId(foreground, &processId);
        if (processId == GetCurrentProcessId()) return foreground;
    }

    GameWindowSearch search = {};
    EnumWindows(FindGameWindowCallback, (LPARAM)&search);
    return search.window;
}

static bool OverlayUsesGerman(void)
{
    if (!_stricmp(g_overlayLanguage, "de")) return true;
    if (!_stricmp(g_overlayLanguage, "en")) return false;
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN;
}

static const char* OverlayPrecipitationName(int state, bool german)
{
    // 0.3.2: the field is the winter weather roll 0..7; only 1 is snowfall.
    if (german)
    {
        if (state == 1) return "Schnee";
        if (state == 0) return "trocken";
        if (state >= 2 && state <= 7) return "kein Schnee";
        return "unbekannt";
    }
    return WeatherRollName(state);
}

static void CountOverlayPlowPhases(int* protectedPoints, int* saltedPoints)
{
    if (protectedPoints) *protectedPoints = 0;
    if (saltedPoints) *saltedPoints = 0;
    if (!protectedPoints || !saltedPoints ||
        !InterlockedCompareExchange(&g_plowEffectLockReady, 0, 0))
        return;

    LONG64 now = InterlockedCompareExchange64(&g_gameMinuteMilli, 0, 0);
    LONG generation = InterlockedCompareExchange(
        &g_gameWorldGeneration, 0, 0);
    LONG64 protection = PlowProtectionDuration();
    LONG64 total = protection + PlowSaltDuration();
    if (now <= 0 || generation <= 0 || total <= 0) return;

    EnterCriticalSection(&g_plowEffectLock);
    int limit = g_cfg.maximumTrackedPlowPoints;
    if (limit > MAX_PLOW_EFFECT_POINTS) limit = MAX_PLOW_EFFECT_POINTS;
    for (int i = 0; i < limit; ++i)
    {
        const PlowEffectPoint& point = g_plowEffectPoints[i];
        if (!point.active || point.generation != generation) continue;
        LONG64 age = now - point.clearedGameMinuteMilli;
        if (age < 0 || age >= total) continue;
        if (age < protection)
            (*protectedPoints)++;
        else
            (*saltedPoints)++;
    }
    LeaveCriticalSection(&g_plowEffectLock);
}

static void DrawOverlayText(HDC dc, int x, int y, COLORREF color,
                            const char* text)
{
    if (!dc || !text) return;
    SetTextColor(dc, color);
    RECT line = {x, y, g_cfg.overlayWidth - 10, y + 22};
    DrawTextA(dc, text, -1, &line,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static void PaintOverlay(HWND window)
{
    PAINTSTRUCT paint = {};
    HDC dc = BeginPaint(window, &paint);
    if (!dc) return;

    RECT client = {};
    GetClientRect(window, &client);
    HBRUSH background = CreateSolidBrush(RGB(22, 27, 32));
    HBRUSH border = CreateSolidBrush(RGB(104, 132, 148));
    FillRect(dc, &client, background);
    FrameRect(dc, &client, border);
    DeleteObject(border);
    DeleteObject(background);

    SetBkMode(dc, TRANSPARENT);
    HFONT previousFont = g_overlayFont
        ? (HFONT)SelectObject(dc, g_overlayFont) : NULL;
    bool german = OverlayUsesGerman();
    char line[256];

    DrawOverlayText(dc, 12, 6, RGB(164, 218, 244),
                    "Weather Roads " PLUGIN_VERSION);

    bool weatherValid =
        InterlockedCompareExchange(&g_overlayWeatherValid, 0, 0) != 0;
    int precipitation = InterlockedCompareExchange(
        &g_overlayPrecipitationState, 0, 0);
    if (weatherValid)
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "Niederschlag: %d (%s)" :
                             "Precipitation: %d (%s)",
                    precipitation,
                    OverlayPrecipitationName(precipitation, german));
    else
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "Niederschlag: keine Weltdaten" :
                             "Precipitation: no world data");
    DrawOverlayText(dc, 12, 30, RGB(235, 235, 235), line);

    LONG64 trackedBytes = InterlockedCompareExchange64(
        &g_overlayTrackedBytes, 0, 0);
    LONG64 trackedSum = InterlockedCompareExchange64(
        &g_overlayTrackedSnowSum, 0, 0);
    LONG trackedMaximum = InterlockedCompareExchange(
        &g_overlayTrackedSnowMaximum, 0, 0);
    LONG trackedRoads = InterlockedCompareExchange(
        &g_overlayTrackedRoads, 0, 0);
    double trackedAverage = trackedBytes > 0
        ? (double)trackedSum / (double)trackedBytes : 0.0;
    if (trackedRoads > 0)
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "Intern M/Max: %.1f / %ld (%ld Str.)" :
                             "Internal avg/max: %.1f / %ld (%ld)",
                    trackedAverage, trackedMaximum, trackedRoads);
    else
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "Strassenschnee: lernt Strassen noch" :
                             "Road snow: still learning roads");
    DrawOverlayText(dc, 12, 52, RGB(235, 235, 235), line);

    LONG64 visualCount = InterlockedCompareExchange64(
        &g_overlayVisualSnowCount, 0, 0);
    LONG64 visualSum = InterlockedCompareExchange64(
        &g_overlayVisualSnowSum, 0, 0);
    LONG visualMaximum = InterlockedCompareExchange(
        &g_overlayVisualSnowMaximum, 0, 0);
    unsigned visualAveragePercent = visualCount > 0
        ? (unsigned)((visualSum * 100LL + visualCount * 127LL) /
                     (visualCount * 255LL)) : 0u;
    unsigned visualMaximumPercent = (unsigned)(
        ((LONG64)visualMaximum * 100LL + 127LL) / 255LL);
    if (visualCount > 0)
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "Sichtbar: Mittel %u%% | Max %u%%" :
                             "Visible: avg %u%% | max %u%%",
                    visualAveragePercent, visualMaximumPercent);
    else
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "Sichtbarer Schnee: noch keine Strassenpixel" :
                             "Visible snow: no known road texels yet");
    DrawOverlayText(dc, 12, 74, RGB(235, 235, 235), line);

    int protectedPoints = 0;
    int saltedPoints = 0;
    CountOverlayPlowPhases(&protectedPoints, &saltedPoints);
    _snprintf_s(line, sizeof(line), _TRUNCATE,
                german ? "Pflugeffekt: %d geschuetzt | %d gesalzen" :
                         "Plow effect: %d protected | %d salted",
                protectedPoints, saltedPoints);
    DrawOverlayText(dc, 12, 96, RGB(235, 235, 235), line);

    double snowFactor = g_cfg.internalSnowLimiter
        ? g_cfg.snowAccumulationMultiplier : 1.0;
    double saltedFactor = snowFactor *
        (g_cfg.plowEffect ? g_cfg.plowSaltAccumulationMultiplier : 1.0);
    double meltFactor = g_cfg.naturalMelting
        ? g_cfg.snowReductionMultiplier : 1.0;
    _snprintf_s(line, sizeof(line), _TRUNCATE,
                german ? "Faktoren: Schnee %.3f | Salz %.3f | Schmelze %.3f" :
                         "Factors: snow %.3f | salt %.3f | melt %.3f",
                snowFactor, saltedFactor, meltFactor);
    DrawOverlayText(dc, 12, 118, RGB(235, 235, 235), line);

    LONG burstForwarded = InterlockedCompareExchange(
        &g_overlaySnowBurstForwarded, 0, 0);
    LONG burstCapped = InterlockedCompareExchange(
        &g_overlaySnowBurstCapped, 0, 0);
    LONG gradualPending = InterlockedCompareExchange(
        &g_gradualSnowPendingUnits, 0, 0);
    _snprintf_s(line, sizeof(line), _TRUNCATE,
                german ? "Schnee: %ld/%d | Puffer %ld | verw. %ld" :
                         "Snow: %ld/%d | queued %ld | capped %ld",
                burstForwarded, g_cfg.maximumSnowAccumulationPerBurst,
                gradualPending, burstCapped);
    DrawOverlayText(dc, 12, 140, RGB(235, 235, 235), line);

    bool snowDeltaValid = InterlockedCompareExchange(
        &g_overlaySnowDeltaValid, 0, 0) != 0;
    LONG snowDeltaRaw = InterlockedCompareExchange(
        &g_overlaySnowDeltaRaw, 0, 0);
    LONG snowDeltaEffective = InterlockedCompareExchange(
        &g_overlaySnowDeltaEffective, 0, 0);
    if (snowDeltaValid)
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "Schneemaske Delta: Roh %+ld | Effektiv %+ld" :
                             "Snow mask delta: raw %+ld | effective %+ld",
                    snowDeltaRaw, snowDeltaEffective);
    else
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "Schneemaske Delta: noch kein Wert" :
                             "Snow mask delta: no value yet");
    DrawOverlayText(dc, 12, 162, RGB(235, 235, 235), line);

    LONG correctionMs = InterlockedCompareExchange(
        &g_overlayLastCorrectionMs, 0, 0);
    LONG64 duplicates = InterlockedCompareExchange64(
        &g_overlayLastDuplicateSnowWrites, 0, 0);
    if (correctionMs >= 0)
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "GPU: %ld ms | Wiederholungen: %lld" :
                             "GPU: %ld ms | repeats: %lld",
                    correctionMs, (long long)duplicates);
    else
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    german ? "GPU: noch keine Korrektur" :
                             "GPU: no correction yet");
    DrawOverlayText(dc, 12, 184, RGB(235, 235, 235), line);

    _snprintf_s(line, sizeof(line), _TRUNCATE,
                german ? "F10: Fenster ein-/ausblenden" :
                         "F10: show/hide window");
    DrawOverlayText(dc, 12, 212, RGB(157, 169, 176), line);

    if (previousFont) SelectObject(dc, previousFont);
    EndPaint(window, &paint);
}

static LRESULT CALLBACK OverlayWindowProc(HWND window, UINT message,
                                          WPARAM wParam, LPARAM lParam)
{
    if (message == WM_PAINT)
    {
        PaintOverlay(window);
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_CLOSE)
    {
        InterlockedExchange(&g_overlayUserVisible, 0);
        ShowWindow(window, SW_HIDE);
        return 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

static bool IsGameWindowForeground(HWND gameWindow)
{
    if (!gameWindow || !IsWindow(gameWindow)) return false;
    HWND foreground = GetForegroundWindow();
    DWORD processId = 0;
    if (foreground)
        GetWindowThreadProcessId(foreground, &processId);
    return processId == GetCurrentProcessId();
}

static bool PositionOverlay(HWND overlay, HWND gameWindow)
{
    RECT anchor = {};
    RECT bounds = {};
    HMONITOR monitor = NULL;
    bool anchoredToGame = false;

    if (gameWindow && IsWindow(gameWindow))
    {
        RECT client = {};
        POINT origin = {};
        if (GetClientRect(gameWindow, &client) &&
            ClientToScreen(gameWindow, &origin) &&
            client.right > client.left && client.bottom > client.top)
        {
            anchor.left = origin.x;
            anchor.top = origin.y;
            anchor.right = origin.x + client.right - client.left;
            anchor.bottom = origin.y + client.bottom - client.top;
            monitor = MonitorFromWindow(gameWindow,
                                        MONITOR_DEFAULTTONEAREST);
            anchoredToGame = true;
        }
    }

    if (!monitor)
    {
        POINT cursor = {};
        if (!GetCursorPos(&cursor))
        {
            cursor.x = 0;
            cursor.y = 0;
        }
        monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    }

    MONITORINFO info = {sizeof(info)};
    if (!monitor || !GetMonitorInfoA(monitor, &info)) return false;
    bounds = info.rcWork;
    if (!anchoredToGame) anchor = bounds;

    int width = g_cfg.overlayWidth;
    int x = anchor.left + g_cfg.overlayOffsetX;
    if (!_stricmp(g_overlayPosition, "top_right"))
        x = anchor.right - width - g_cfg.overlayOffsetX;
    int y = anchor.top + g_cfg.overlayOffsetY;

    if (x < bounds.left) x = bounds.left;
    if (y < bounds.top) y = bounds.top;
    if (x + width > bounds.right) x = bounds.right - width;
    if (y + OVERLAY_WINDOW_HEIGHT > bounds.bottom)
        y = bounds.bottom - OVERLAY_WINDOW_HEIGHT;

    return SetWindowPos(overlay, HWND_TOPMOST, x, y, width,
                        OVERLAY_WINDOW_HEIGHT,
                        SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
}

static DWORD WINAPI OverlayWorker(LPVOID)
{
    WNDCLASSEXA windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = OverlayWindowProc;
    windowClass.hInstance = g_module;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = OVERLAY_CLASS_NAME;
    if (!RegisterClassExA(&windowClass))
    {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS)
        {
            InterlockedExchange(&g_overlayState,-1);
            ReportWindows("overlay","window-class","Window class could not be registered",
                          error,"Weather stays active; restart to retry the optional overlay");
            return 0;
        }
    }

    g_overlayWindow = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        OVERLAY_CLASS_NAME, "Weather Roads", WS_POPUP,
        0, 0, g_cfg.overlayWidth, OVERLAY_WINDOW_HEIGHT,
        NULL, NULL, g_module, NULL);
    if (!g_overlayWindow)
    {
        DWORD windowError = GetLastError();
        InterlockedExchange(&g_overlayState,-1);
        ReportWindows("overlay","window-create","Window could not be created",
                      windowError,"Weather stays active; restart to retry the optional overlay");
        UnregisterClassA(OVERLAY_CLASS_NAME, g_module);
        return 0;
    }

    SetLayeredWindowAttributes(g_overlayWindow, 0,
                               (BYTE)g_cfg.overlayOpacity, LWA_ALPHA);
    g_overlayFont = CreateFontA(
        17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");

    InterlockedExchange(&g_overlayState,2);
    Info("diagnostic overlay active: window created hwnd=%p size=%dx%d",
         g_overlayWindow, g_cfg.overlayWidth, OVERLAY_WINDOW_HEIGHT);

    HWND gameWindow = NULL;
    HWND ownedWindow = NULL;
    bool toggleDown = false;
    bool missingWindowLogged = false;
    int lastDisplayState = -1;
    ULONGLONG searchStarted = GetTickCount64();
    while (InterlockedCompareExchange(&g_active, 0, 0))
    {
        MSG message = {};
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT) goto overlay_exit;
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        HWND foundWindow = FindGameWindow();
        if (foundWindow != gameWindow)
        {
            gameWindow = foundWindow;
            if (gameWindow)
            {
                char title[160] = {};
                char className[96] = {};
                RECT client = {};
                GetWindowTextA(gameWindow, title, (int)sizeof(title));
                GetClassNameA(gameWindow, className, (int)sizeof(className));
                GetClientRect(gameWindow, &client);
                Info("diagnostic overlay found game window: hwnd=%p "
                     "class=\"%s\" title=\"%s\" client=%ldx%ld",
                     gameWindow, className, title,
                     client.right - client.left, client.bottom - client.top);
                missingWindowLogged = false;
            }
        }

        if (ownedWindow != gameWindow)
        {
            SetWindowLongPtrA(g_overlayWindow, GWLP_HWNDPARENT,
                              (LONG_PTR)gameWindow);
            ownedWindow = gameWindow;
        }

        if (!gameWindow && !missingWindowLogged &&
            GetTickCount64() - searchStarted >= 5000)
        {
            Warn("diagnostic overlay did not find the game window after 5 s; "
                 "using the current monitor as a safe display fallback");
            missingWindowLogged = true;
        }

        bool gameForeground = IsGameWindowForeground(gameWindow);
        bool keyDown = g_cfg.overlayToggleKey > 0 &&
            (GetAsyncKeyState(g_cfg.overlayToggleKey) & 0x8000) != 0;
        if ((gameForeground || !g_cfg.overlayForegroundOnly) &&
            keyDown && !toggleDown)
        {
            LONG visible = InterlockedCompareExchange(
                &g_overlayUserVisible, 0, 0);
            InterlockedExchange(&g_overlayUserVisible, visible ? 0 : 1);
        }
        toggleDown = keyDown;

        bool userVisible = InterlockedCompareExchange(
            &g_overlayUserVisible, 0, 0) != 0;
        bool shouldShow = userVisible &&
            (!g_cfg.overlayForegroundOnly || gameForeground);
        if (shouldShow)
        {
            PositionOverlay(g_overlayWindow, gameWindow);
            RedrawWindow(g_overlayWindow, NULL, NULL,
                         RDW_INVALIDATE | RDW_UPDATENOW);
        }
        else
        {
            ShowWindow(g_overlayWindow, SW_HIDE);
        }

        int displayState = shouldShow ? 1 : 0;
        if (displayState != lastDisplayState)
        {
            Event("diagnostic overlay display: visible=%s user-enabled=%s "
                  "foreground=%s foreground-only=%s game-window=%p "
                  "fallback=%s",
                  shouldShow ? "yes" : "no",
                  userVisible ? "yes" : "no",
                  gameForeground ? "yes" : "no",
                  g_cfg.overlayForegroundOnly ? "yes" : "no",
                  gameWindow, gameWindow ? "no" : "monitor");
            lastDisplayState = displayState;
        }

        DWORD wait = MsgWaitForMultipleObjects(
            1, &g_overlayStopEvent, FALSE,
            (DWORD)g_cfg.overlayUpdateIntervalMs, QS_ALLINPUT);
        if (!WorkerWaitContinues(wait,"overlay worker")) {
            if (wait == WAIT_FAILED) InterlockedExchange(&g_overlayState,-1);
            break;
        }
    }

overlay_exit:
    if (g_overlayWindow)
    {
        DestroyWindow(g_overlayWindow);
        g_overlayWindow = NULL;
    }
    if (g_overlayFont)
    {
        DeleteObject(g_overlayFont);
        g_overlayFont = NULL;
    }
    UnregisterClassA(OVERLAY_CLASS_NAME, g_module);
    if (InterlockedCompareExchange(&g_overlayState,0,0) != -1)
        InterlockedExchange(&g_overlayState,3);
    Event("diagnostic overlay stopped");
    return 0;
}

static bool StartOverlay(void)
{
    if (!g_cfg.overlayEnabled) return false;
    InterlockedExchange(&g_overlayState,1);
    g_overlayStopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!g_overlayStopEvent)
    {
        DWORD error = GetLastError();
        InterlockedExchange(&g_overlayState,-1);
        ReportWindows("overlay","event-create","Stop event could not be created",error,"Weather stays active; restart to retry the optional overlay");
        return false;
    }
    g_overlayThread = CreateThread(NULL, 0, OverlayWorker, NULL, 0, NULL);
    if (!g_overlayThread)
    {
        DWORD error = GetLastError();
        InterlockedExchange(&g_overlayState,-1);
        ReportWindows("overlay","thread-create","Worker could not be created",error,"Weather stays active; restart to retry the optional overlay");
        CloseHandle(g_overlayStopEvent);
        g_overlayStopEvent = NULL;
        return false;
    }
    Info("diagnostic overlay thread started (window result reported separately): position=%s offset=%d,%d width=%d "
         "opacity=%d update=%d ms toggle_vk=%d language=%s "
         "foreground_only=%d; the window is click-through and uses a "
         "monitor fallback when no game window is found",
         g_overlayPosition, g_cfg.overlayOffsetX, g_cfg.overlayOffsetY,
         g_cfg.overlayWidth, g_cfg.overlayOpacity,
         g_cfg.overlayUpdateIntervalMs, g_cfg.overlayToggleKey,
         g_overlayLanguage, g_cfg.overlayForegroundOnly);
    return true;
}

// --------------------------------------------------------- road-snow observer

static uint32_t Fnv1a(const BYTE* data, SIZE_T length,
                      uint64_t* sum, unsigned* minimum, unsigned* maximum)
{
    uint32_t hash = 2166136261u;
    uint64_t total = 0;
    unsigned lo = 255;
    unsigned hi = 0;
    for (SIZE_T i = 0; i < length; ++i)
    {
        unsigned value = data[i];
        hash ^= value;
        hash *= 16777619u;
        total += value;
        if (value < lo) lo = value;
        if (value > hi) hi = value;
    }
    *sum = total;
    *minimum = length ? lo : 0;
    *maximum = length ? hi : 0;
    return hash;
}

static void LogInvalidRoadOnce(const char* reason, void* vehicle, void* road,
                               SIZE_T length)
{
    InterlockedIncrement64(&g_snowplowRejectedSamples);

    ULONGLONG now = GetTickCount64();
    LONG previous = InterlockedExchange(&g_invalidRoadLogged, (LONG)(now / 5000));
    LONG bucket = (LONG)(now / 5000);
    if (previous == bucket) return;

    Event("road-snow clearing sample refused: %s; vehicle=%p road=%p "
          "length=%llu",
          reason, vehicle, road, (unsigned long long)length);
}

static void NoteSnowplowClearCall(LONG64 calls, void* vehicle, void* road)
{
    ULONGLONG now = GetTickCount64();
    bool shouldLog = calls == 1;

    if (shouldLog)
    {
        InterlockedExchange64(&g_snowplowStatusTick, (LONG64)now);
    }
    else if (g_cfg.roadHeartbeatSeconds > 0)
    {
        LONG64 previous = InterlockedCompareExchange64(
            &g_snowplowStatusTick, 0, 0);
        ULONGLONG interval =
            (ULONGLONG)g_cfg.roadHeartbeatSeconds * 1000ULL;
        if (now - (ULONGLONG)previous >= interval &&
            InterlockedCompareExchange64(&g_snowplowStatusTick,
                                         (LONG64)now, previous) == previous)
            shouldLog = true;
    }

    if (shouldLog)
    {
        LONG64 valid = InterlockedCompareExchange64(
            &g_snowplowCompleteSamples, 0, 0);
        LONG64 rejected = InterlockedCompareExchange64(
            &g_snowplowRejectedSamples, 0, 0);
        Event("road-snow clearing status: calls=%lld complete_samples=%lld "
              "rejected_samples=%lld current_vehicle=%p current_road=%p",
              (long long)calls, (long long)valid, (long long)rejected,
              vehicle, road);
    }
}

struct RoadSnowState
{
    BYTE* begin = NULL;
    SIZE_T length = 0;
    uint64_t sum = 0;
    unsigned minimum = 0;
    unsigned maximum = 0;
    uint32_t hash = 0;
};

#define MAX_CLEAR_WRITE_RECORDS 64

struct ClearWriteRecord
{
    SIZE_T index;
    BYTE oldValue;
    BYTE site;
};

struct ClearCallContext
{
    LONG64 call;
    void* vehicle;
    void* road;
    void* selector;
    SIZE_T attempts;
    SIZE_T nonzeroWrites;
    SIZE_T invalidObservations;
    uint64_t removedSum;
    SIZE_T siteAttempts[4];
    SIZE_T siteNonzero[4];
    SIZE_T recordCount;
    SIZE_T truncatedRecords;
    int treatmentMatched;
    LONG treatmentStrengthMillion;
    int treatmentMaterialIndex;
    unsigned treatmentFlags;
    unsigned treatmentSequence;
    char treatmentResourceName[TSM_GRIT_RESOURCE_NAME_CAPACITY];
    ClearWriteRecord records[MAX_CLEAR_WRITE_RECORDS];
};

static LONG ClampGritStrengthMillion(float strength)
{
    if (!_finite(strength) || strength <= 0.0f) return 0;
    if (strength >= 1.0f) return GRIT_FACTOR_MILLION;
    return (LONG)(strength * (float)GRIT_FACTOR_MILLION + 0.5f);
}

// Material strength scales prevention rather than duration. A strength of 1
// preserves the previous weather_roads behaviour, 0 permits all accumulation,
// and intermediate values blend between the two. Thus strength 0.50 gives a
// factor of 0.50 during the complete-protection phase and 0.75 while the
// configured legacy salt factor is 0.50.
static LONG64 BlendGritPhaseFactor(LONG64 legacyFactor,
                                    LONG strengthMillion)
{
    if (legacyFactor < 0) legacyFactor = 0;
    if (legacyFactor > GRIT_FACTOR_MILLION)
        legacyFactor = GRIT_FACTOR_MILLION;
    if (strengthMillion < 0) strengthMillion = 0;
    if (strengthMillion > GRIT_FACTOR_MILLION)
        strengthMillion = GRIT_FACTOR_MILLION;
    LONG64 preventedByLegacy =
        (LONG64)GRIT_FACTOR_MILLION - legacyFactor;
    LONG64 scaledPrevention =
        (preventedByLegacy * (LONG64)strengthMillion + 500000LL) /
        (LONG64)GRIT_FACTOR_MILLION;
    return (LONG64)GRIT_FACTOR_MILLION - scaledPrevention;
}

static bool ReadCurrentGritTreatment(
    void* vehicle, void* road, void* selector,
    TsmGritRoadTreatment* treatment,
    LONG* strengthMillion)
{
    if (treatment) memset(treatment, 0, sizeof(*treatment));
    if (strengthMillion) *strengthMillion = GRIT_FACTOR_MILLION;
    if (!treatment || !strengthMillion || !g_gritSpreaderApi ||
        g_gritSpreaderApi->structSize < sizeof(TsmGritSpreaderApi) ||
        !g_gritSpreaderApi->currentRoadTreatment)
        return false;

    if (!g_gritSpreaderApi->currentRoadTreatment(
            vehicle, road, selector, treatment, sizeof(*treatment)) ||
        treatment->structSize < sizeof(TsmGritRoadTreatment) ||
        treatment->vehicle != vehicle || treatment->road != road ||
        treatment->selector != selector)
    {
        memset(treatment, 0, sizeof(*treatment));
        return false;
    }

    LONG strength = ClampGritStrengthMillion(
        treatment->protectionStrength);
    if (treatment->flags & TSM_GRIT_TREATMENT_DRY)
        strength = 0;
    *strengthMillion = strength;
    return true;
}

static void BeginVisibleBrushCapture(bool positionValid,
                                     float x, float y, float z)
{
    memset(&g_visibleBrushCapture, 0, sizeof(g_visibleBrushCapture));
    g_visibleBrushCapture.active = 1;
    g_visibleBrushCapture.positionValid = positionValid ? 1 : 0;
    if (positionValid)
    {
        g_visibleBrushCapture.position[0] = x;
        g_visibleBrushCapture.position[1] = y;
        g_visibleBrushCapture.position[2] = z;
    }
}

static bool AcquirePlowVisualLockBounded()
{
    for (int attempt = 0; attempt < 256; ++attempt)
    {
        if (InterlockedCompareExchange(&g_plowVisualBusy, 1, 0) == 0)
            return true;
        YieldProcessor();
    }
    return false;
}

static bool VisibleTrackIdentityMatches(const VisibleBrushTrack& track,
                                        void* texture, void* resource,
                                        LONG generation)
{
    return track.active && track.texture == texture &&
        track.resource == resource && track.generation == generation;
}

static void ExpireVisibleBrushTracksLocked(ULONGLONG now)
{
    for (int i = 0; i < MAX_VISIBLE_BRUSH_TRACKS; ++i)
    {
        VisibleBrushTrack& track = g_visibleBrushTracks[i];
        if (!track.active) continue;
        if (now < track.lastTick ||
            now - track.lastTick > PLOW_EFFECT_MASK_WINDOW_MS)
            memset(&track, 0, sizeof(track));
    }
}

static VisibleBrushTrack* AssignVisibleBrushTrackLocked(
    const VisibleBrushCapture& capture, ULONGLONG now)
{
    if (!capture.positionValid || !capture.texture || !capture.resource)
        return NULL;

    ExpireVisibleBrushTracksLocked(now);
    int bestSlot = -1;
    float bestScore = FLT_MAX;
    for (int i = 0; i < MAX_VISIBLE_BRUSH_TRACKS; ++i)
    {
        VisibleBrushTrack& track = g_visibleBrushTracks[i];
        if (!VisibleTrackIdentityMatches(
                track, capture.texture, capture.resource,
                capture.generation) || now < track.lastTick)
            continue;

        ULONGLONG age = now - track.lastTick;
        if (age > PLOW_EFFECT_MASK_WINDOW_MS) continue;
        float ageFloat = (float)age;
        float maximumDistance = VISIBLE_TRACK_BASE_RADIUS_METRES +
            VISIBLE_TRACK_RADIUS_PER_MS * ageFloat;

        float predictedX = track.lastPosition[0];
        float predictedZ = track.lastPosition[2];
        if (track.previousTick &&
            track.lastTick > track.previousTick)
        {
            float ratio = ageFloat /
                (float)(track.lastTick - track.previousTick);
            if (ratio > 3.0f) ratio = 3.0f;
            predictedX += (track.lastPosition[0] -
                track.previousPosition[0]) * ratio;
            predictedZ += (track.lastPosition[2] -
                track.previousPosition[2]) * ratio;
        }

        float lastDx = capture.position[0] - track.lastPosition[0];
        float lastDz = capture.position[2] - track.lastPosition[2];
        float predictedDx = capture.position[0] - predictedX;
        float predictedDz = capture.position[2] - predictedZ;
        float lastDistanceSquared = lastDx * lastDx + lastDz * lastDz;
        float predictedDistanceSquared =
            predictedDx * predictedDx + predictedDz * predictedDz;
        float maximumDistanceSquared = maximumDistance * maximumDistance;
        if (lastDistanceSquared > maximumDistanceSquared &&
            predictedDistanceSquared > maximumDistanceSquared)
            continue;

        float score = predictedDistanceSquared +
            lastDistanceSquared * 0.25f;
        if (score < bestScore)
        {
            bestScore = score;
            bestSlot = i;
        }
    }

    if (bestSlot < 0)
    {
        int freeSlot = -1;
        int oldestUnownedSlot = -1;
        ULONGLONG oldestUnownedTick = ULLONG_MAX;
        int oldestSlot = 0;
        ULONGLONG oldestTick = ULLONG_MAX;
        for (int i = 0; i < MAX_VISIBLE_BRUSH_TRACKS; ++i)
        {
            VisibleBrushTrack& track = g_visibleBrushTracks[i];
            if (!track.active)
            {
                freeSlot = i;
                break;
            }
            if (!track.ownerVehicle && track.lastTick < oldestUnownedTick)
            {
                oldestUnownedTick = track.lastTick;
                oldestUnownedSlot = i;
            }
            if (track.lastTick < oldestTick)
            {
                oldestTick = track.lastTick;
                oldestSlot = i;
            }
        }
        bestSlot = freeSlot >= 0 ? freeSlot :
            (oldestUnownedSlot >= 0 ? oldestUnownedSlot : oldestSlot);
        VisibleBrushTrack& track = g_visibleBrushTracks[bestSlot];
        memset(&track, 0, sizeof(track));
        track.active = 1;
        track.id = InterlockedIncrement64(&g_visibleTrackNextId);
        track.texture = capture.texture;
        track.resource = capture.resource;
        track.generation = capture.generation;
        track.lastTick = now;
        memcpy(track.lastPosition, capture.position,
               sizeof(track.lastPosition));
        InterlockedIncrement64(&g_visibleTracksCreated);
        LONG detail = InterlockedIncrement(&g_visibleTrackDetailLogs);
        if (detail <= 48)
            Event("visible track created: track=%lld position=[%.3f,%.3f,%.3f] "
                 "texture=%p resource=%p generation=%ld",
                 (long long)track.id,
                 (double)track.lastPosition[0],
                 (double)track.lastPosition[1],
                 (double)track.lastPosition[2],
                 track.texture, track.resource, track.generation);
        return &track;
    }

    VisibleBrushTrack& track = g_visibleBrushTracks[bestSlot];
    track.previousTick = track.lastTick;
    memcpy(track.previousPosition, track.lastPosition,
           sizeof(track.previousPosition));
    track.lastTick = now;
    memcpy(track.lastPosition, capture.position,
           sizeof(track.lastPosition));
    return &track;
}

static LONG64 ApplyVisibleTreatmentToIndicesLocked(
    const DWORD* indices, DWORD count, LONG strengthMillion,
    const LONG* priorStrengthMillion = NULL,
    const LONG64* priorClearMinuteMilli = NULL)
{
    if (!indices || !count) return 0;
    if (strengthMillion < 0) strengthMillion = 0;
    if (strengthMillion > GRIT_FACTOR_MILLION)
        strengthMillion = GRIT_FACTOR_MILLION;

    const bool exactCorrection = priorStrengthMillion &&
        priorClearMinuteMilli;
    LONG64 now = InterlockedCompareExchange64(&g_gameMinuteMilli, 0, 0);
    LONG64 totalDuration = PlowProtectionDuration() + PlowSaltDuration();
    LONG64 applied = 0;
    for (DWORD i = 0; i < count; ++i)
    {
        SIZE_T index = (SIZE_T)indices[i];
        if (index >= g_plowVisualCapacity) continue;
        LONG64 currentTime = g_plowVisualClearMinuteMilli[index];
        LONG currentStrength = g_plowVisualStrengthMillion[index];
        LONG64 baselineTime = exactCorrection
            ? priorClearMinuteMilli[i] : currentTime;
        LONG baselineStrength = exactCorrection
            ? priorStrengthMillion[i] : currentStrength;
        if (baselineStrength < 0) baselineStrength = 0;
        if (baselineStrength > GRIT_FACTOR_MILLION)
            baselineStrength = GRIT_FACTOR_MILLION;
        bool baselineProtectionActive = baselineTime > 0 && now > 0 &&
            now >= baselineTime && totalDuration > 0 &&
            now - baselineTime < totalDuration;

        LONG64 targetTime = baselineTime;
        LONG targetStrength = baselineStrength;
        bool applyIncoming = false;

        if (strengthMillion == 0)
        {
            // A provisional dry track never mutates committed state. The exact
            // clear below either restores the captured prior state or, when
            // the compatibility option is disabled, deliberately removes it.
            if (!exactCorrection)
            {
                applied++;
                continue;
            }
            if (g_cfg.dryPlowingPreservesTreatment)
            {
                if (baselineProtectionActive)
                    InterlockedIncrement64(
                        &g_gritDryVisualTexelsPreserved);
            }
            else
            {
                if (baselineProtectionActive)
                    InterlockedIncrement64(&g_gritDryVisualTexelsCleared);
                targetTime = 0;
                targetStrength = 0;
            }
        }
        else if (baselineProtectionActive &&
                 baselineStrength > strengthMillion)
        {
            InterlockedIncrement64(&g_gritLowerVisualTexelsPreserved);
        }
        else
        {
            targetTime = now;
            targetStrength = strengthMillion;
            applyIncoming = now > 0;
        }

        bool targetListed = targetTime != 0;
        bool currentListed = currentTime != 0;
        if (targetListed && !currentListed)
        {
            if (!g_plowVisualActiveIndices ||
                g_plowVisualActiveCount >= g_plowVisualActiveCapacity ||
                index > MAXDWORD)
            {
                InterlockedIncrement64(&g_plowVisualDroppedActiveTexels);
                applied++;
                continue;
            }
            g_plowVisualActiveIndices[g_plowVisualActiveCount++] =
                (DWORD)index;
            InterlockedExchange64(
                &g_plowVisualActivePublished,
                (LONG64)g_plowVisualActiveCount);
        }

        g_plowVisualClearMinuteMilli[index] =
            !targetListed && currentListed ? -1 : targetTime;
        g_plowVisualStrengthMillion[index] = targetStrength;
        if (!targetListed)
        {
            g_plowVisualSnowBurstGeneration[index] = 0;
            g_plowVisualSnowBurstAppliedUnits[index] = 0;
        }
        if (applyIncoming)
        {
            if (baselineTime > 0)
                InterlockedIncrement64(&g_plowVisualRefreshed);
            else
                InterlockedIncrement64(&g_plowVisualMarked);
        }
        applied++;
    }
    return applied;
}

static void QueueCapturedVisibleBrush()
{
    VisibleBrushCapture capture = g_visibleBrushCapture;
    memset(&g_visibleBrushCapture, 0, sizeof(g_visibleBrushCapture));
    if (!g_gritSpreaderApi || !capture.texelCount || !capture.texture)
        return;

    ULONGLONG now = GetTickCount64();
    DWORD threadId = GetCurrentThreadId();
    LONG64 brushSequence = InterlockedIncrement64(
        &g_visibleCorrelationBrushSequence);
    RecentVisibleClear recentClear = {};
    bool followsRecentClear = false;
    ULONGLONG ageAfterClear = 0;
    int pendingSlot = -1;
    LONG64 trackId = 0;
    AcquireSRWLockExclusive(&g_pendingVisibleBrushLock);
    VisibleBrushTrack* track = AssignVisibleBrushTrackLocked(capture, now);
    if (track) trackId = track->id;
    int freeSlot = -1;
    int oldestSlot = 0;
    ULONGLONG oldestTick = ULLONG_MAX;
    for (int i = 0; i < MAX_PENDING_VISIBLE_BRUSHES; ++i)
    {
        PendingVisibleBrush& pending = g_pendingVisibleBrushes[i];
        if (pending.active &&
            (now < pending.tick ||
             now - pending.tick > PLOW_EFFECT_MASK_WINDOW_MS))
        {
            pending.active = 0;
            if (pending.treatmentApplied)
                InterlockedIncrement64(&g_visibleTrackExpiredBrushes);
            else
                InterlockedIncrement64(&g_gritVisibleTreatmentFallbacks);
        }
        if (!pending.active)
        {
            if (freeSlot < 0) freeSlot = i;
            continue;
        }
        if (pending.tick < oldestTick)
        {
            oldestTick = pending.tick;
            oldestSlot = i;
        }
    }

    int slot = freeSlot >= 0 ? freeSlot : oldestSlot;
    pendingSlot = slot;
    PendingVisibleBrush& pending = g_pendingVisibleBrushes[slot];
    if (pending.active)
    {
        if (pending.treatmentApplied)
            InterlockedIncrement64(&g_visibleTrackExpiredBrushes);
        else
            InterlockedIncrement64(&g_gritVisibleTreatmentFallbacks);
    }
    memset(&pending, 0, sizeof(pending));
    pending.active = 1;
    pending.sequence = brushSequence;
    pending.trackId = trackId;
    pending.threadId = threadId;
    pending.tick = now;
    pending.positionValid = capture.positionValid;
    if (capture.positionValid)
        memcpy(pending.position, capture.position, sizeof(pending.position));
    pending.texture = capture.texture;
    pending.resource = capture.resource;
    pending.generation = capture.generation;
    pending.texelCount = capture.texelCount;
    pending.truncatedTexels = capture.truncatedTexels;
    memcpy(pending.texelIndices, capture.texelIndices,
           (SIZE_T)capture.texelCount * sizeof(DWORD));
    memcpy(pending.priorStrengthMillion, capture.priorStrengthMillion,
           (SIZE_T)capture.texelCount * sizeof(LONG));
    memcpy(pending.priorClearMinuteMilli, capture.priorClearMinuteMilli,
           (SIZE_T)capture.texelCount * sizeof(LONG64));
    recentClear = g_recentVisibleClear;
    followsRecentClear = recentClear.valid && now >= recentClear.tick &&
        now - recentClear.tick <= PLOW_EFFECT_MASK_WINDOW_MS;
    if (followsRecentClear)
        ageAfterClear = now - recentClear.tick;
    ReleaseSRWLockExclusive(&g_pendingVisibleBrushLock);

    // A bound trajectory already knows its most recent material. Apply that
    // value immediately, but keep the brush pending until the next native
    // clear finalises the interval. If the tank changes to dry on that clear,
    // only these still-open brushes are corrected.
    bool provisionalApplied = false;
    if (trackId && pendingSlot >= 0 && AcquirePlowVisualLockBounded())
    {
        AcquireSRWLockExclusive(&g_pendingVisibleBrushLock);
        PendingVisibleBrush& current =
            g_pendingVisibleBrushes[pendingSlot];
        VisibleBrushTrack* currentTrack = NULL;
        for (int i = 0; i < MAX_VISIBLE_BRUSH_TRACKS; ++i)
        {
            if (g_visibleBrushTracks[i].active &&
                g_visibleBrushTracks[i].id == trackId)
            {
                currentTrack = &g_visibleBrushTracks[i];
                break;
            }
        }
        if (current.active && current.sequence == brushSequence &&
            currentTrack && currentTrack->treatmentValid &&
            current.texture == g_plowVisualTexture &&
            current.resource == g_plowVisualResource &&
            current.generation == g_plowVisualGeneration)
        {
            ApplyVisibleTreatmentToIndicesLocked(
                current.texelIndices, current.texelCount,
                currentTrack->treatmentStrengthMillion);
            current.treatmentApplied = 1;
            current.appliedStrengthMillion =
                currentTrack->treatmentStrengthMillion;
            current.appliedTreatmentSequence =
                currentTrack->treatmentSequence;
            provisionalApplied = true;
        }
        ReleaseSRWLockExclusive(&g_pendingVisibleBrushLock);
        InterlockedExchange(&g_plowVisualBusy, 0);
    }
    if (provisionalApplied)
        InterlockedIncrement64(&g_visibleTrackProvisionalBrushes);
    else
        InterlockedIncrement64(&g_visibleTrackUnownedBrushes);

    if (followsRecentClear)
    {
        InterlockedIncrement64(&g_visibleCorrelationBrushesAfterClear);
        bool sameThread = recentClear.threadId == threadId;
        InterlockedIncrement64(sameThread
            ? &g_visibleCorrelationAfterClearSameThread
            : &g_visibleCorrelationAfterClearCrossThread);
        LONG detail = InterlockedIncrement(
            &g_visibleCorrelationBrushDetailLogs);
        if (detail <= 32)
        {
            Event("visible correlation diagnostic: direction=clear-before-brush "
                 "brush=%lld brush_thread=%lu clear=%lld clear_thread=%lu "
                 "same_thread=%d age_ms=%llu position=%s[%.3f,%.3f,%.3f] "
                 "texels=%lu truncated=%lu material_index=%d strength=%.3f "
                 "flags=0x%X treatment_sequence=%u",
                 (long long)brushSequence, (unsigned long)threadId,
                 (long long)recentClear.call,
                 (unsigned long)recentClear.threadId,
                 sameThread ? 1 : 0,
                 (unsigned long long)ageAfterClear,
                 capture.positionValid ? "" : "unavailable/",
                 (double)capture.position[0],
                 (double)capture.position[1],
                 (double)capture.position[2],
                 (unsigned long)capture.texelCount,
                 (unsigned long)capture.truncatedTexels,
                 recentClear.treatmentMaterialIndex,
                 (double)recentClear.treatmentStrengthMillion /
                     (double)GRIT_FACTOR_MILLION,
                 recentClear.treatmentFlags,
                 recentClear.treatmentSequence);
        }
    }
}

static LONG64 CountPendingBrushesForTrackLocked(LONG64 trackId,
                                                ULONGLONG now,
                                                ULONGLONG* newestAge)
{
    LONG64 count = 0;
    ULONGLONG bestAge = ULLONG_MAX;
    for (int i = 0; i < MAX_PENDING_VISIBLE_BRUSHES; ++i)
    {
        PendingVisibleBrush& pending = g_pendingVisibleBrushes[i];
        if (!pending.active || pending.trackId != trackId ||
            now < pending.tick ||
            now - pending.tick > PLOW_EFFECT_MASK_WINDOW_MS)
            continue;
        ULONGLONG age = now - pending.tick;
        if (age < bestAge) bestAge = age;
        count++;
    }
    if (newestAge) *newestAge = bestAge;
    return count;
}

static VisibleBrushTrack* SelectVisibleTrackForClearLocked(
    const ClearCallContext& context, ULONGLONG now,
    const char** bindingReason, LONG64* pendingBrushes)
{
    if (bindingReason) *bindingReason = "none";
    if (pendingBrushes) *pendingBrushes = 0;
    ExpireVisibleBrushTracksLocked(now);

    VisibleBrushTrack* owned = NULL;
    for (int i = 0; i < MAX_VISIBLE_BRUSH_TRACKS; ++i)
    {
        VisibleBrushTrack& track = g_visibleBrushTracks[i];
        if (!track.active || track.ownerVehicle != context.vehicle)
            continue;
        if (VisibleTrackIdentityMatches(
                track, g_plowVisualTexture, g_plowVisualResource,
                g_plowVisualGeneration))
        {
            owned = &track;
            break;
        }
        track.ownerVehicle = NULL;
        track.treatmentValid = 0;
    }

    if (owned)
    {
        if (bindingReason) *bindingReason = "existing-owner";
        if (pendingBrushes)
            *pendingBrushes = CountPendingBrushesForTrackLocked(
                owned->id, now, NULL);
        return owned;
    }

    VisibleBrushTrack* best = NULL;
    ULONGLONG bestAge = ULLONG_MAX;
    LONG64 bestPending = 0;
    for (int i = 0; i < MAX_VISIBLE_BRUSH_TRACKS; ++i)
    {
        VisibleBrushTrack& track = g_visibleBrushTracks[i];
        if (!VisibleTrackIdentityMatches(
                track, g_plowVisualTexture, g_plowVisualResource,
                g_plowVisualGeneration) || track.ownerVehicle ||
            now < track.lastTick ||
            now - track.lastTick > PLOW_EFFECT_MASK_WINDOW_MS)
            continue;

        ULONGLONG newestAge = ULLONG_MAX;
        LONG64 count = CountPendingBrushesForTrackLocked(
            track.id, now, &newestAge);
        if (!count) continue;
        if (!best || newestAge < bestAge ||
            (newestAge == bestAge && count > bestPending))
        {
            best = &track;
            bestAge = newestAge;
            bestPending = count;
        }
    }
    if (!best) return NULL;

    // No active trajectory may be owned by two vehicles. Clear a stale
    // duplicate binding for this vehicle before publishing the new owner.
    bool rebound = false;
    for (int i = 0; i < MAX_VISIBLE_BRUSH_TRACKS; ++i)
    {
        VisibleBrushTrack& track = g_visibleBrushTracks[i];
        if (&track != best && track.active &&
            track.ownerVehicle == context.vehicle)
        {
            track.ownerVehicle = NULL;
            track.treatmentValid = 0;
            rebound = true;
        }
    }
    best->ownerVehicle = context.vehicle;
    InterlockedIncrement64(rebound
        ? &g_visibleTracksRebound : &g_visibleTracksBound);
    if (bindingReason)
        *bindingReason = rebound ? "rebound-nearest-unowned" :
                                   "new-nearest-unowned";
    if (pendingBrushes) *pendingBrushes = bestPending;
    return best;
}

// v0.2.6 binds the spatially continuous render-thread trajectory to one
// vehicle. Only still-open brushes on that trajectory are finalised here;
// already finalised older road sections are never changed by a later dry pass.
static void ApplyTreatmentToPendingVisibleBrushes(
    const ClearCallContext& context)
{
    if (!g_gritSpreaderApi || !context.treatmentMatched ||
        !InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0))
        return;

    if (!AcquirePlowVisualLockBounded())
        return;

    ULONGLONG now = GetTickCount64();
    DWORD threadId = GetCurrentThreadId();
    LONG strengthMillion = context.treatmentStrengthMillion;
    if (strengthMillion < 0) strengthMillion = 0;
    if (strengthMillion > GRIT_FACTOR_MILLION)
        strengthMillion = GRIT_FACTOR_MILLION;
    LONG64 matchedBrushes = 0;
    LONG64 matchedTexels = 0;

    LONG64 candidateCount = 0;
    LONG64 sameThreadCandidates = 0;
    LONG64 crossThreadCandidates = 0;
    LONG64 identityMatches = 0;
    LONG64 identityMismatches = 0;
    LONG64 positionValidCandidates = 0;
    ULONGLONG minimumAge = ULLONG_MAX;
    ULONGLONG maximumAge = 0;
    ULONGLONG nearestAge = ULLONG_MAX;
    LONG64 nearestSequence = 0;
    int nearestPositionValid = 0;
    float nearestPosition[3] = {};
    int hasPositionBounds = 0;
    float minimumPosition[3] = {};
    float maximumPosition[3] = {};
    DWORD candidateThreads[8] = {};
    int candidateThreadCount = 0;
    int candidateThreadOverflow = 0;
    const char* bindingReason = "none";
    LONG64 selectedTrackPending = 0;
    LONG64 selectedTrackId = 0;
    float selectedTrackPosition[3] = {};
    LONG64 correctedBrushes = 0;

    InterlockedIncrement64(&g_visibleCorrelationClearSamples);
    AcquireSRWLockExclusive(&g_pendingVisibleBrushLock);
    VisibleBrushTrack* selectedTrack = SelectVisibleTrackForClearLocked(
        context, now, &bindingReason, &selectedTrackPending);
    if (selectedTrack)
    {
        selectedTrackId = selectedTrack->id;
        memcpy(selectedTrackPosition, selectedTrack->lastPosition,
               sizeof(selectedTrackPosition));
    }
    for (int i = 0; i < MAX_PENDING_VISIBLE_BRUSHES; ++i)
    {
        PendingVisibleBrush& pending = g_pendingVisibleBrushes[i];
        if (!pending.active) continue;
        if (now < pending.tick ||
            now - pending.tick > PLOW_EFFECT_MASK_WINDOW_MS)
        {
            pending.active = 0;
            if (pending.treatmentApplied)
                InterlockedIncrement64(&g_visibleTrackExpiredBrushes);
            else
                InterlockedIncrement64(&g_gritVisibleTreatmentFallbacks);
            continue;
        }

        ULONGLONG age = now - pending.tick;
        candidateCount++;
        if (age < minimumAge) minimumAge = age;
        if (age > maximumAge) maximumAge = age;
        if (age < nearestAge)
        {
            nearestAge = age;
            nearestSequence = pending.sequence;
            nearestPositionValid = pending.positionValid;
            memcpy(nearestPosition, pending.position,
                   sizeof(nearestPosition));
        }

        bool threadKnown = false;
        for (int threadIndex = 0;
             threadIndex < candidateThreadCount; ++threadIndex)
        {
            if (candidateThreads[threadIndex] == pending.threadId)
            {
                threadKnown = true;
                break;
            }
        }
        if (!threadKnown)
        {
            if (candidateThreadCount <
                    (int)(sizeof(candidateThreads) /
                          sizeof(candidateThreads[0])))
                candidateThreads[candidateThreadCount++] =
                    pending.threadId;
            else
                candidateThreadOverflow = 1;
        }

        bool sameThread = pending.threadId == threadId;
        if (sameThread) sameThreadCandidates++;
        else            crossThreadCandidates++;

        bool identityMatch =
            pending.texture == g_plowVisualTexture &&
            pending.resource == g_plowVisualResource &&
            pending.generation == g_plowVisualGeneration;
        if (identityMatch) identityMatches++;
        else               identityMismatches++;

        if (pending.positionValid)
        {
            positionValidCandidates++;
            if (!hasPositionBounds)
            {
                memcpy(minimumPosition, pending.position,
                       sizeof(minimumPosition));
                memcpy(maximumPosition, pending.position,
                       sizeof(maximumPosition));
                hasPositionBounds = 1;
            }
            else
            {
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (pending.position[axis] < minimumPosition[axis])
                        minimumPosition[axis] =
                            pending.position[axis];
                    if (pending.position[axis] > maximumPosition[axis])
                        maximumPosition[axis] =
                            pending.position[axis];
                }
            }
        }

        if (!selectedTrackId || pending.trackId != selectedTrackId)
            continue;

        // A texture/world replacement invalidates the captured flat indices.
        if (!identityMatch)
        {
            pending.active = 0;
            InterlockedIncrement64(&g_gritVisibleTreatmentFallbacks);
            continue;
        }

        if (!pending.treatmentApplied ||
            pending.appliedStrengthMillion != strengthMillion)
            correctedBrushes++;
        matchedTexels += ApplyVisibleTreatmentToIndicesLocked(
            pending.texelIndices, pending.texelCount, strengthMillion,
            pending.priorStrengthMillion,
            pending.priorClearMinuteMilli);
        pending.treatmentApplied = 1;
        pending.appliedStrengthMillion = strengthMillion;
        pending.appliedTreatmentSequence = context.treatmentSequence;
        matchedBrushes++;
        pending.active = 0;
    }

    if (selectedTrack)
    {
        selectedTrack->treatmentValid = 1;
        selectedTrack->treatmentStrengthMillion = strengthMillion;
        selectedTrack->treatmentMaterialIndex =
            context.treatmentMaterialIndex;
        selectedTrack->treatmentFlags = context.treatmentFlags;
        selectedTrack->treatmentSequence = context.treatmentSequence;
        selectedTrack->lastClearCall = context.call;
        selectedTrack->lastClearTick = now;
    }

    memset(&g_recentVisibleClear, 0, sizeof(g_recentVisibleClear));
    g_recentVisibleClear.valid = 1;
    g_recentVisibleClear.call = context.call;
    g_recentVisibleClear.threadId = threadId;
    g_recentVisibleClear.tick = now;
    g_recentVisibleClear.vehicle = context.vehicle;
    g_recentVisibleClear.road = context.road;
    g_recentVisibleClear.selector = context.selector;
    g_recentVisibleClear.treatmentStrengthMillion = strengthMillion;
    g_recentVisibleClear.treatmentMaterialIndex =
        context.treatmentMaterialIndex;
    g_recentVisibleClear.treatmentFlags = context.treatmentFlags;
    g_recentVisibleClear.treatmentSequence = context.treatmentSequence;
    ReleaseSRWLockExclusive(&g_pendingVisibleBrushLock);
    InterlockedExchange(&g_plowVisualBusy, 0);

    if (selectedTrackId)
    {
        InterlockedIncrement64(&g_visibleTrackClearMatches);
        InterlockedIncrement64(&g_gritVisibleTreatmentClearMatches);
        InterlockedExchangeAdd64(&g_visibleTrackFinalizedBrushes,
                                 matchedBrushes);
        InterlockedExchangeAdd64(&g_visibleTrackCorrectedBrushes,
                                 correctedBrushes);
        InterlockedExchangeAdd64(&g_gritVisibleTreatmentMatches,
                                 matchedBrushes);
        InterlockedExchangeAdd64(&g_gritVisibleTreatmentTexels,
                                 matchedTexels);
        LONG trackDetail = InterlockedIncrement(&g_visibleTrackDetailLogs);
        if (trackDetail <= 96)
            Event("visible track treatment: clear=%lld vehicle=%p track=%lld "
                 "binding=%s position=[%.3f,%.3f,%.3f] "
                 "pending_before=%lld finalized_brushes=%lld "
                 "corrected_brushes=%lld assigned_texels=%lld "
                 "material_index=%d strength=%.3f dry=%d flags=0x%X "
                 "treatment_sequence=%u",
                 (long long)context.call, context.vehicle,
                 (long long)selectedTrackId, bindingReason,
                 (double)selectedTrackPosition[0],
                 (double)selectedTrackPosition[1],
                 (double)selectedTrackPosition[2],
                 (long long)selectedTrackPending,
                 (long long)matchedBrushes,
                 (long long)correctedBrushes,
                 (long long)matchedTexels,
                 context.treatmentMaterialIndex,
                 (double)strengthMillion / (double)GRIT_FACTOR_MILLION,
                 strengthMillion == 0 ? 1 : 0,
                 context.treatmentFlags, context.treatmentSequence);
    }
    else
    {
        LONG trackDetail = InterlockedIncrement(&g_visibleTrackDetailLogs);
        if (trackDetail <= 96)
            Event("visible track treatment: clear=%lld vehicle=%p track=none "
                 "binding=no-unowned-spatial-candidate material_index=%d "
                 "strength=%.3f dry=%d treatment_sequence=%u",
                 (long long)context.call, context.vehicle,
                 context.treatmentMaterialIndex,
                 (double)strengthMillion / (double)GRIT_FACTOR_MILLION,
                 strengthMillion == 0 ? 1 : 0,
                 context.treatmentSequence);
    }

    if (candidateCount > 0)
    {
        InterlockedIncrement64(
            &g_visibleCorrelationClearsWithCandidates);
        InterlockedExchangeAdd64(
            &g_visibleCorrelationPrecedingCandidates,
            candidateCount);
        InterlockedExchangeAdd64(
            &g_visibleCorrelationPrecedingSameThread,
            sameThreadCandidates);
        InterlockedExchangeAdd64(
            &g_visibleCorrelationPrecedingCrossThread,
            crossThreadCandidates);
        InterlockedExchangeAdd64(
            &g_visibleCorrelationIdentityMatches,
            identityMatches);
        InterlockedExchangeAdd64(
            &g_visibleCorrelationIdentityMismatches,
            identityMismatches);
        InterlockedExchangeAdd64(
            &g_visibleCorrelationPositionValid,
            positionValidCandidates);
    }

    LONG detail = InterlockedIncrement(
        &g_visibleCorrelationClearDetailLogs);
    if (detail <= 64)
    {
        char threadSummary[128] = {};
        size_t used = 0;
        for (int i = 0; i < candidateThreadCount; ++i)
        {
            int written = _snprintf_s(
                threadSummary + used, sizeof(threadSummary) - used,
                _TRUNCATE, "%s%lu", i ? "," : "",
                (unsigned long)candidateThreads[i]);
            if (written < 0 ||
                (size_t)written >= sizeof(threadSummary) - used)
                break;
            used += (size_t)written;
        }
        if (candidateThreadOverflow && used < sizeof(threadSummary) - 2)
            strncpy_s(threadSummary + used,
                      sizeof(threadSummary) - used, ",+", _TRUNCATE);
        if (!threadSummary[0])
            strncpy_s(threadSummary, sizeof(threadSummary),
                      "none", _TRUNCATE);

        Event("visible correlation diagnostic: direction=brush-before-clear "
             "clear=%lld clear_thread=%lu vehicle=%p road=%p selector=%p "
             "material_index=%d strength=%.3f flags=0x%X "
             "treatment_sequence=%u candidates=%lld same_thread=%lld "
             "cross_thread=%lld identity_ok=%lld identity_bad=%lld "
             "position_valid=%lld age_ms=%s%llu..%llu "
             "nearest_brush=%lld nearest_age_ms=%s%llu "
             "nearest_position=%s[%.3f,%.3f,%.3f] "
             "position_bounds=%s[%.3f..%.3f,%.3f..%.3f,%.3f..%.3f] "
             "brush_threads=[%s]",
             (long long)context.call, (unsigned long)threadId,
             context.vehicle, context.road, context.selector,
             context.treatmentMaterialIndex,
             (double)strengthMillion / (double)GRIT_FACTOR_MILLION,
             context.treatmentFlags, context.treatmentSequence,
             (long long)candidateCount,
             (long long)sameThreadCandidates,
             (long long)crossThreadCandidates,
             (long long)identityMatches,
             (long long)identityMismatches,
             (long long)positionValidCandidates,
             candidateCount ? "" : "unavailable/",
             (unsigned long long)(candidateCount ? minimumAge : 0),
             (unsigned long long)(candidateCount ? maximumAge : 0),
             (long long)nearestSequence,
             candidateCount ? "" : "unavailable/",
             (unsigned long long)(candidateCount ? nearestAge : 0),
             nearestPositionValid ? "" : "unavailable/",
             (double)nearestPosition[0],
             (double)nearestPosition[1],
             (double)nearestPosition[2],
             hasPositionBounds ? "" : "unavailable/",
             (double)minimumPosition[0],
             (double)maximumPosition[0],
             (double)minimumPosition[1],
             (double)maximumPosition[1],
             (double)minimumPosition[2],
             (double)maximumPosition[2],
             threadSummary);
    }

}

static LONG64 PlowProtectionDuration(void)
{
    return (LONG64)(g_cfg.plowProtectionMinutes * 1000.0 + 0.5);
}

static LONG64 PlowSaltDuration(void)
{
    return (LONG64)(g_cfg.plowSaltHours * 60.0 * 1000.0 + 0.5);
}

static void RecordPlowedRoadPoint(void* road, BYTE* begin, SIZE_T length,
                                  SIZE_T index, LONG strengthMillion,
                                  int materialIndex, unsigned treatmentFlags)
{
    if (!g_cfg.plowEffect ||
        !InterlockedCompareExchange(&g_plowEffectLockReady, 0, 0) ||
        !road || !begin || index >= length)
        return;

    if (strengthMillion < 0) strengthMillion = 0;
    if (strengthMillion > GRIT_FACTOR_MILLION)
        strengthMillion = GRIT_FACTOR_MILLION;
    LONG64 now = InterlockedCompareExchange64(&g_gameMinuteMilli, 0, 0);
    LONG generation = InterlockedCompareExchange(&g_gameWorldGeneration, 0, 0);
    LONG64 totalDuration = PlowProtectionDuration() + PlowSaltDuration();
    if (now <= 0 || generation <= 0 || totalDuration <= 0) return;

    EnterCriticalSection(&g_plowEffectLock);
    int freeSlot = -1;
    int oldestSlot = -1;
    LONG64 oldestTime = LLONG_MAX;
    const int limit = g_cfg.maximumTrackedPlowPoints;
    uint64_t hash = ((uint64_t)(uintptr_t)road >> 4) ^
                    ((uint64_t)(uintptr_t)begin >> 5) ^
                    ((uint64_t)index * 11400714819323198485ull) ^
                    ((uint64_t)(unsigned long)generation * 0x9E3779B1u);
    int bucketCount = limit / PLOW_EFFECT_BUCKET_SIZE;
    int first = (int)(hash % (uint64_t)bucketCount) *
                PLOW_EFFECT_BUCKET_SIZE;
    // Linear probing retains all entries after pointer rehashing on reload.
    // generation==0 is a never-used slot; expired/deleted slots keep their
    // generation as tombstones so later entries remain discoverable.
    for (int probe = 0; probe < limit; ++probe)
    {
        int i = (first + probe) % limit;
        PlowEffectPoint& point = g_plowEffectPoints[i];
        if (point.generation == 0)
        {
            if (freeSlot < 0) freeSlot = i;
            break;
        }
        if (point.active &&
            (point.generation != generation ||
             now >= point.clearedGameMinuteMilli + totalDuration))
        {
            point.active = 0;
            InterlockedIncrement64(&g_plowEffectExpired);
        }

        if (!point.active)
        {
            if (freeSlot < 0) freeSlot = i;
            continue;
        }

        if (point.generation == generation && point.road == road &&
            point.vectorBegin == begin && point.vectorLength == length &&
            point.index == index)
        {
            if (strengthMillion == 0)
            {
                if (g_cfg.dryPlowingPreservesTreatment)
                {
                    InterlockedIncrement64(
                        &g_gritDryInternalPointsPreserved);
                }
                else
                {
                    point.active = 0;
                    InterlockedIncrement64(
                        &g_gritDryInternalPointsCleared);
                }
                LeaveCriticalSection(&g_plowEffectLock);
                return;
            }
            if (strengthMillion < point.treatmentStrengthMillion)
            {
                InterlockedIncrement64(
                    &g_gritLowerInternalPointsPreserved);
                LeaveCriticalSection(&g_plowEffectLock);
                return;
            }
            point.clearedGameMinuteMilli = now;
            point.saltRemainderMillion = 0;
            point.treatmentStrengthMillion = strengthMillion;
            point.treatmentMaterialIndex = materialIndex;
            point.treatmentFlags = treatmentFlags;
            point.sampledPhase = 0;
            InterlockedIncrement64(&g_plowEffectRefreshed);
            LeaveCriticalSection(&g_plowEffectLock);
            return;
        }

        if (point.clearedGameMinuteMilli < oldestTime)
        {
            oldestTime = point.clearedGameMinuteMilli;
            oldestSlot = i;
        }
    }

    if (strengthMillion == 0)
    {
        LeaveCriticalSection(&g_plowEffectLock);
        return;
    }

    int slot = freeSlot >= 0 ? freeSlot : oldestSlot;
    if (slot >= 0)
    {
        PlowEffectPoint& point = g_plowEffectPoints[slot];
        if (point.active) InterlockedIncrement64(&g_plowEffectEvicted);
        memset(&point, 0, sizeof(point));
        point.active = 1;
        point.generation = generation;
        point.road = road;
        point.vectorBegin = begin;
        point.vectorLength = length;
        point.index = index;
        point.clearedGameMinuteMilli = now;
        point.treatmentStrengthMillion = strengthMillion;
        point.treatmentMaterialIndex = materialIndex;
        point.treatmentFlags = treatmentFlags;
        InterlockedIncrement64(&g_plowEffectRegistered);
    }
    LeaveCriticalSection(&g_plowEffectLock);
}

// Called only while g_maskDataLock is held. A zero transition can precede the
// matching visible brush by one update sample, so it is retained briefly and
// confirmed only if the verified plow brush follows within the same window.
static void QueuePendingPlowPointLocked(void* road, BYTE* begin,
                                        SIZE_T length, SIZE_T index,
                                        ULONGLONG now,
                                        LONG strengthMillion,
                                        int materialIndex,
                                        unsigned treatmentFlags)
{
    int freeSlot = -1;
    int oldestSlot = 0;
    ULONGLONG oldestTick = ULLONG_MAX;
    for (int i = 0; i < MAX_PENDING_PLOW_POINTS; ++i)
    {
        PendingPlowPoint& point = g_pendingPlowPoints[i];
        if (point.active &&
            now - point.detectedTick > PLOW_EFFECT_MASK_WINDOW_MS)
        {
            point.active = 0;
            InterlockedIncrement64(&g_plowEffectRejectedNoRecentMask);
        }
        if (!point.active)
        {
            if (freeSlot < 0) freeSlot = i;
            continue;
        }
        if (point.road == road && point.vectorBegin == begin &&
            point.vectorLength == length && point.index == index)
        {
            point.detectedTick = now;
            if ((strengthMillion == 0 &&
                 g_cfg.dryPlowingPreservesTreatment) ||
                (strengthMillion > 0 &&
                 point.treatmentStrengthMillion > strengthMillion))
            {
                return;
            }
            point.treatmentStrengthMillion = strengthMillion;
            point.treatmentMaterialIndex = materialIndex;
            point.treatmentFlags = treatmentFlags;
            return;
        }
        if (point.detectedTick < oldestTick)
        {
            oldestTick = point.detectedTick;
            oldestSlot = i;
        }
    }

    int slot = freeSlot >= 0 ? freeSlot : oldestSlot;
    PendingPlowPoint& point = g_pendingPlowPoints[slot];
    if (point.active)
        InterlockedIncrement64(&g_plowEffectRejectedNoRecentMask);
    point.active = 1;
    point.road = road;
    point.vectorBegin = begin;
    point.vectorLength = length;
    point.index = index;
    point.detectedTick = now;
    point.treatmentStrengthMillion = strengthMillion;
    point.treatmentMaterialIndex = materialIndex;
    point.treatmentFlags = treatmentFlags;
}

static void ConfirmPendingPlowPoints(ULONGLONG now)
{
    if (!g_cfg.plowEffect ||
        !InterlockedCompareExchange(&g_maskDataLockReady, 0, 0))
        return;

    EnterCriticalSection(&g_maskDataLock);
    for (int i = 0; i < MAX_PENDING_PLOW_POINTS; ++i)
    {
        PendingPlowPoint& point = g_pendingPlowPoints[i];
        if (!point.active) continue;
        ULONGLONG age = now >= point.detectedTick
                     ? now - point.detectedTick : ULLONG_MAX;
        if (age <= PLOW_EFFECT_MASK_WINDOW_MS)
        {
            RecordPlowedRoadPoint(point.road, point.vectorBegin,
                                  point.vectorLength, point.index,
                                  point.treatmentStrengthMillion,
                                  point.treatmentMaterialIndex,
                                  point.treatmentFlags);
        }
        else
        {
            InterlockedIncrement64(&g_plowEffectRejectedNoRecentMask);
        }
        point.active = 0;
    }
    LeaveCriticalSection(&g_maskDataLock);
}

// Called by the generated write-site stubs immediately before the original
// game instruction stores zero.  It only reads the byte and updates memory
// owned by this plugin.  Logging is deferred until the complete clearing call
// returns, keeping file I/O out of the game's inner clearing loops.
static __declspec(noinline) void ObserveRoadSnowWrite(void* road,
                                                       intptr_t index,
                                                       unsigned site)
{
    if (!InterlockedCompareExchange(&g_active, 0, 0) ||
        g_clearTls == TLS_OUT_OF_INDEXES || !road || index < 0 ||
        site < 1 || site > 4)
        return;

    ClearCallContext* context =
        (ClearCallContext*)TlsGetValue(g_clearTls);
    if (!context || context->road != road)
        return;

    context->attempts++;
    context->siteAttempts[site - 1]++;

    BYTE* begin = NULL;
    BYTE* end = NULL;
    BYTE oldValue = 0;
    SIZE_T vectorLength = 0;
    bool valid = false;

    __try
    {
        BYTE* r = (BYTE*)road;
        begin = *(BYTE**)(r + OFF_ROAD_SNOW_BEG);
        end = *(BYTE**)(r + OFF_ROAD_SNOW_END);
        if (begin && end && end >= begin)
        {
            vectorLength = (SIZE_T)(end - begin);
            if ((SIZE_T)index < vectorLength &&
                vectorLength <= (SIZE_T)g_cfg.maximumRoadBytes)
            {
                oldValue = begin[index];
                valid = true;
            }
        }
    }
    __except (FaultFilter(PLUGIN_NAME " road snow zero-write observation",
                          GetExceptionInformation()))
    {
        valid = false;
    }

    if (!valid)
    {
        context->invalidObservations++;
        return;
    }

    if (context->recordCount < MAX_CLEAR_WRITE_RECORDS)
    {
        ClearWriteRecord& record = context->records[context->recordCount++];
        record.index = (SIZE_T)index;
        record.oldValue = oldValue;
        record.site = (BYTE)site;
    }
    else
    {
        context->truncatedRecords++;
    }

    // Zero-valued attempts matter for material integration as well: a dry
    // plow passing a previously protected, already-clear point must remove its
    // old protection instead of silently retaining salt from an earlier pass.
    if (!oldValue)
        return;

    context->nonzeroWrites++;
    context->siteNonzero[site - 1]++;
    context->removedSum += oldValue;
}

static void LogClearWriteContext(const ClearCallContext& context)
{
    if (!g_cfg.detailedEvents) return;
    char samples[1536];
    samples[0] = 0;
    size_t used = 0;

    for (SIZE_T i = 0; i < context.recordCount; ++i)
    {
        const ClearWriteRecord& record = context.records[i];
        int written = _snprintf_s(samples + used, sizeof(samples) - used,
                                  _TRUNCATE, "%s%llu:%u@%u",
                                  i ? "," : "",
                                  (unsigned long long)record.index,
                                  (unsigned)record.oldValue,
                                  (unsigned)record.site);
        if (written < 0 || (size_t)written >= sizeof(samples) - used)
            break;
        used += (size_t)written;
    }

    Event("road-snow zero writes: call=%lld vehicle=%p road=%p "
          "attempts=%u nonzero_writes=%u removed_sum=%llu "
          "invalid_observations=%u site_attempts=[%u,%u,%u,%u] "
          "site_nonzero=[%u,%u,%u,%u] samples=[%s] truncated=%u "
          "grit_matched=%d grit_resource=%s grit_strength=%.3f "
          "grit_material_index=%d grit_flags=0x%X grit_sequence=%u",
          (long long)context.call, context.vehicle, context.road,
          (unsigned)context.attempts, (unsigned)context.nonzeroWrites,
          (unsigned long long)context.removedSum,
          (unsigned)context.invalidObservations,
          (unsigned)context.siteAttempts[0],
          (unsigned)context.siteAttempts[1],
          (unsigned)context.siteAttempts[2],
          (unsigned)context.siteAttempts[3],
          (unsigned)context.siteNonzero[0],
          (unsigned)context.siteNonzero[1],
          (unsigned)context.siteNonzero[2],
          (unsigned)context.siteNonzero[3],
          samples,
          (unsigned)context.truncatedRecords,
          context.treatmentMatched,
          context.treatmentResourceName[0]
              ? context.treatmentResourceName : "legacy",
          (double)context.treatmentStrengthMillion / 1000000.0,
          context.treatmentMaterialIndex, context.treatmentFlags,
          context.treatmentSequence);
}

static bool ReadRoadSnowState(void* road, RoadSnowState* out,
                              const char** reason)
{
    if (!out || !reason) return false;
    memset(out, 0, sizeof(*out));
    *reason = "unknown road-snow state";

    BYTE* r = (BYTE*)road;
    if (!r || !ReadablePtr(r, OFF_ROAD_SNOW_END + sizeof(void*)))
    {
        *reason = "road pointer is unreadable";
        return false;
    }

    BYTE* end = NULL;
    __try
    {
        out->begin = *(BYTE**)(r + OFF_ROAD_SNOW_BEG);
        end = *(BYTE**)(r + OFF_ROAD_SNOW_END);
    }
    __except (FaultFilter(PLUGIN_NAME " road snow-vector pointer",
                          GetExceptionInformation()))
    {
        *reason = "fault reading snow-vector pointers";
        return false;
    }

    if (!out->begin || !end || end < out->begin)
    {
        *reason = "snow-vector bounds are invalid";
        return false;
    }

    out->length = (SIZE_T)(end - out->begin);
    if (!out->length || out->length > (SIZE_T)g_cfg.maximumRoadBytes)
    {
        *reason = "snow-vector length is outside the configured safety range";
        return false;
    }
    if (!ReadablePtr(out->begin, out->length))
    {
        *reason = "snow-vector bytes are unreadable";
        return false;
    }

    __try
    {
        out->hash = Fnv1a(out->begin, out->length, &out->sum,
                          &out->minimum, &out->maximum);
    }
    __except (FaultFilter(PLUGIN_NAME " road snow-byte scan",
                          GetExceptionInformation()))
    {
        *reason = "fault scanning snow-vector bytes";
        memset(out, 0, sizeof(*out));
        return false;
    }

    return true;
}

static void ReleaseTrackedRoadSnow(TrackedRoadSnow* tracked)
{
    if (!tracked) return;
    InterlockedExchange(&tracked->active, 0);
    if (tracked->snapshot)
        HeapFree(GetProcessHeap(), 0, tracked->snapshot);
    memset(tracked, 0, sizeof(*tracked));
}

static bool CopyRoadSnowBytes(const RoadSnowState& state, BYTE* destination)
{
    if (!destination || !state.begin || !state.length) return false;
    __try
    {
        memcpy(destination, state.begin, state.length);
    }
    __except (FaultFilter(PLUGIN_NAME " tracked road-snow copy",
                          GetExceptionInformation()))
    {
        return false;
    }
    return true;
}

// The first clearing call for a road can finish before the periodic tracker
// has ever seen its non-zero state. In that case the freshly copied vector is
// already zero and there is no later non-zero -> zero transition to observe.
// Queue those zero bytes once, at initial registration or verified storage
// reset, and still require the same two-second verified visible brush window.
static void RegisterInitialZeroRoadPointsLocked(void* road, BYTE* begin,
                                                 SIZE_T length,
                                                 const BYTE* initial,
                                                 ULONGLONG now,
                                                 LONG strengthMillion,
                                                 int materialIndex,
                                                 unsigned treatmentFlags)
{
    if (!g_cfg.plowEffect || !road || !begin || !initial || !length) return;

    LONG64 lastMask = InterlockedCompareExchange64(
        &g_lastVerifiedPlowMaskTick, 0, 0);
    bool recentMask = lastMask > 0 && now >= (ULONGLONG)lastMask &&
        now - (ULONGLONG)lastMask <= PLOW_EFFECT_MASK_WINDOW_MS;

    for (SIZE_T index = 0; index < length; ++index)
    {
        if (initial[index] != 0) continue;
        InterlockedIncrement64(&g_plowEffectInitialZeroCandidates);
        if (recentMask)
            RecordPlowedRoadPoint(road, begin, length, index,
                                  strengthMillion, materialIndex,
                                  treatmentFlags);
        else
            QueuePendingPlowPointLocked(road, begin, length, index, now,
                                        strengthMillion, materialIndex,
                                        treatmentFlags);
    }
}

// Register only roads that have reached the verified clearing function.  A
// bounded registry and expiry time avoid retaining arbitrary game pointers
// indefinitely.  Existing snapshots are deliberately preserved: if snow
// changed before the next sample, the periodic observer must still see it.
static void RegisterTrackedRoadSnow(void* road, LONG64 clearCall,
                                    LONG strengthMillion,
                                    int materialIndex,
                                    unsigned treatmentFlags)
{
    RoadSnowState state = {};
    const char* reason = NULL;
    if (!ReadRoadSnowState(road, &state, &reason))
        return;

    BYTE* initial = (BYTE*)HeapAlloc(GetProcessHeap(), 0, state.length);
    if (!initial || !CopyRoadSnowBytes(state, initial))
    {
        if (initial) HeapFree(GetProcessHeap(), 0, initial);
        Warn("internal road-snow tracker could not copy road=%p bytes=%llu",
             road, (unsigned long long)state.length);
        return;
    }

    uint64_t initialSum = 0;
    unsigned initialMinimum = 0;
    unsigned initialMaximum = 0;
    uint32_t initialHash = Fnv1a(initial, state.length, &initialSum,
                                  &initialMinimum, &initialMaximum);
    ULONGLONG now = GetTickCount64();

    EnterCriticalSection(&g_maskDataLock);
    int limit = g_cfg.maximumTrackedRoads;
    if (limit > MAX_TRACKED_ROADS) limit = MAX_TRACKED_ROADS;
    int slot = -1;
    int freeSlot = -1;
    int oldestSlot = 0;
    ULONGLONG oldestTick = ULLONG_MAX;

    for (int i = 0; i < limit; ++i)
    {
        TrackedRoadSnow& candidate = g_trackedRoads[i];
        if (candidate.active && candidate.road == road)
        {
            slot = i;
            break;
        }
        if (!candidate.active && freeSlot < 0)
            freeSlot = i;
        if (candidate.active && candidate.lastSeenTick < oldestTick)
        {
            oldestTick = candidate.lastSeenTick;
            oldestSlot = i;
        }
    }

    if (slot >= 0)
    {
        TrackedRoadSnow& tracked = g_trackedRoads[slot];
        tracked.lastSeenTick = now;
        tracked.lastClearCall = clearCall;
        tracked.treatmentStrengthMillion = strengthMillion;
        tracked.treatmentMaterialIndex = materialIndex;
        tracked.treatmentFlags = treatmentFlags;
        if (tracked.begin == state.begin && tracked.length == state.length)
        {
            LeaveCriticalSection(&g_maskDataLock);
            HeapFree(GetProcessHeap(), 0, initial);
            return;
        }

        Event("road-snow tracker storage reset: track=%u road=%p "
              "old_begin=%p old_bytes=%llu new_begin=%p new_bytes=%llu "
              "clear_call=%lld",
              tracked.id, road, tracked.begin,
              (unsigned long long)tracked.length, state.begin,
              (unsigned long long)state.length, (long long)clearCall);
        InterlockedExchange(&tracked.active, 0);
        if (tracked.snapshot)
            HeapFree(GetProcessHeap(), 0, tracked.snapshot);
        tracked.begin = state.begin;
        tracked.length = state.length;
        tracked.snapshot = initial;
        tracked.sum = initialSum;
        tracked.minimum = initialMinimum;
        tracked.maximum = initialMaximum;
        tracked.hash = initialHash;
        tracked.registeredTick = now;
        tracked.lastSampleTick = now;
        InterlockedExchange(&tracked.active, 1);
        RegisterInitialZeroRoadPointsLocked(
            road, state.begin, state.length, initial, now,
            strengthMillion, materialIndex, treatmentFlags);
        LeaveCriticalSection(&g_maskDataLock);
        return;
    }

    slot = freeSlot >= 0 ? freeSlot : oldestSlot;
    if (g_trackedRoads[slot].active)
    {
        Event("road-snow tracker evicted: track=%u road=%p age_ms=%llu",
              g_trackedRoads[slot].id, g_trackedRoads[slot].road,
              (unsigned long long)(now - g_trackedRoads[slot].lastSeenTick));
        ReleaseTrackedRoadSnow(&g_trackedRoads[slot]);
    }

    TrackedRoadSnow& tracked = g_trackedRoads[slot];
    tracked.id = g_nextTrackedRoadId++;
    tracked.road = road;
    tracked.begin = state.begin;
    tracked.length = state.length;
    tracked.snapshot = initial;
    tracked.sum = initialSum;
    tracked.minimum = initialMinimum;
    tracked.maximum = initialMaximum;
    tracked.hash = initialHash;
    tracked.lastClearCall = clearCall;
    tracked.treatmentStrengthMillion = strengthMillion;
    tracked.treatmentMaterialIndex = materialIndex;
    tracked.treatmentFlags = treatmentFlags;
    tracked.registeredTick = now;
    tracked.lastSeenTick = now;
    tracked.lastSampleTick = now;
    InterlockedExchange(&tracked.active, 1);
    RegisterInitialZeroRoadPointsLocked(
        road, state.begin, state.length, initial, now,
        strengthMillion, materialIndex, treatmentFlags);

    Event("road-snow tracker registered: track=%u road=%p begin=%p bytes=%llu "
          "sum=%llu min=%u max=%u avg=%.3f hash=%08X clear_call=%lld "
          "grit_strength=%.3f material_index=%d flags=0x%X",
          tracked.id, tracked.road, tracked.begin,
          (unsigned long long)tracked.length,
          (unsigned long long)tracked.sum, tracked.minimum, tracked.maximum,
          (double)tracked.sum / (double)tracked.length, tracked.hash,
          (long long)clearCall,
          (double)strengthMillion / 1000000.0, materialIndex,
          treatmentFlags);
    LeaveCriticalSection(&g_maskDataLock);
}

static void SampleTrackedRoadSnow(ULONGLONG now)
{
    if (g_nextRoadStateSampleTick && now < g_nextRoadStateSampleTick)
        return;
    g_nextRoadStateSampleTick =
        now + (ULONGLONG)g_cfg.roadStateSampleIntervalMs;

    EnterCriticalSection(&g_maskDataLock);
    int limit = g_cfg.maximumTrackedRoads;
    if (limit > MAX_TRACKED_ROADS) limit = MAX_TRACKED_ROADS;
    int active = 0;
    int changedRoads = 0;
    ULONGLONG expiry = (ULONGLONG)g_cfg.roadStateTrackingSeconds * 1000ULL;
    LONG64 lastVerifiedPlowMaskTick = InterlockedCompareExchange64(
        &g_lastVerifiedPlowMaskTick, 0, 0);
    bool verifiedPlowMaskRecent =
        lastVerifiedPlowMaskTick > 0 &&
        now >= (ULONGLONG)lastVerifiedPlowMaskTick &&
        now - (ULONGLONG)lastVerifiedPlowMaskTick <=
            PLOW_EFFECT_MASK_WINDOW_MS;

    for (int i = 0; i < limit; ++i)
    {
        TrackedRoadSnow& tracked = g_trackedRoads[i];
        if (!tracked.active) continue;

        if (now - tracked.lastSeenTick > expiry)
        {
            Event("road-snow tracker expired: track=%u road=%p age_ms=%llu",
                  tracked.id, tracked.road,
                  (unsigned long long)(now - tracked.lastSeenTick));
            ReleaseTrackedRoadSnow(&tracked);
            continue;
        }

        RoadSnowState state = {};
        const char* reason = NULL;
        if (!ReadRoadSnowState(tracked.road, &state, &reason))
        {
            Event("road-snow tracker stopped: track=%u road=%p reason=%s",
                  tracked.id, tracked.road, reason ? reason : "unknown");
            ReleaseTrackedRoadSnow(&tracked);
            continue;
        }

        if (state.begin != tracked.begin || state.length != tracked.length)
        {
            BYTE* replacement =
                (BYTE*)HeapAlloc(GetProcessHeap(), 0, state.length);
            if (!replacement || !CopyRoadSnowBytes(state, replacement))
            {
                if (replacement) HeapFree(GetProcessHeap(), 0, replacement);
                Event("road-snow tracker stopped: track=%u road=%p reason="
                      "changed storage could not be copied", tracked.id,
                      tracked.road);
                ReleaseTrackedRoadSnow(&tracked);
                continue;
            }

            uint64_t sum = 0;
            unsigned minimum = 0;
            unsigned maximum = 0;
            uint32_t hash = Fnv1a(replacement, state.length, &sum,
                                  &minimum, &maximum);
            Event("road-snow tracker storage changed asynchronously: track=%u "
                  "road=%p old_begin=%p old_bytes=%llu new_begin=%p "
                  "new_bytes=%llu new_sum=%llu new_hash=%08X",
                  tracked.id, tracked.road, tracked.begin,
                  (unsigned long long)tracked.length, state.begin,
                  (unsigned long long)state.length,
                  (unsigned long long)sum, hash);
            InterlockedExchange(&tracked.active, 0);
            if (tracked.snapshot)
                HeapFree(GetProcessHeap(), 0, tracked.snapshot);
            tracked.begin = state.begin;
            tracked.length = state.length;
            tracked.snapshot = replacement;
            tracked.sum = sum;
            tracked.minimum = minimum;
            tracked.maximum = maximum;
            tracked.hash = hash;
            tracked.lastSampleTick = now;
            InterlockedExchange(&tracked.active, 1);
            active++;
            continue;
        }

        SIZE_T changed = 0;
        SIZE_T increased = 0;
        SIZE_T decreased = 0;
        SIZE_T zeroed = 0;
        uint64_t increasedUnits = 0;
        uint64_t decreasedUnits = 0;
        uint64_t newSum = 0;
        unsigned newMinimum = 255;
        unsigned newMaximum = 0;
        uint32_t newHash = 2166136261u;
        bool valid = true;

        __try
        {
            for (SIZE_T j = 0; j < tracked.length; ++j)
            {
                BYTE oldValue = tracked.snapshot[j];
                BYTE newValue = tracked.begin[j];
                newHash ^= newValue;
                newHash *= 16777619u;
                newSum += newValue;
                if (newValue < newMinimum) newMinimum = newValue;
                if (newValue > newMaximum) newMaximum = newValue;
                if (newValue != oldValue)
                {
                    changed++;
                    if (newValue > oldValue)
                    {
                        increased++;
                        increasedUnits += (unsigned)newValue - oldValue;
                    }
                    else
                    {
                        decreased++;
                        decreasedUnits += (unsigned)oldValue - newValue;
                        if (!newValue)
                        {
                            zeroed++;
                            if (g_cfg.plowEffect)
                            {
                                InterlockedIncrement64(
                                    &g_plowEffectDecreaseCandidates);
                                if (verifiedPlowMaskRecent)
                                {
                                    RecordPlowedRoadPoint(
                                        tracked.road, tracked.begin,
                                        tracked.length, j,
                                        tracked.treatmentStrengthMillion,
                                        tracked.treatmentMaterialIndex,
                                        tracked.treatmentFlags);
                                }
                                else
                                {
                                    QueuePendingPlowPointLocked(
                                        tracked.road, tracked.begin,
                                        tracked.length, j, now,
                                        tracked.treatmentStrengthMillion,
                                        tracked.treatmentMaterialIndex,
                                        tracked.treatmentFlags);
                                }
                            }
                        }
                    }
                    tracked.snapshot[j] = newValue;
                }
            }
        }
        __except (FaultFilter(PLUGIN_NAME " tracked road-snow sample",
                              GetExceptionInformation()))
        {
            valid = false;
        }

        if (!valid)
        {
            Event("road-snow tracker stopped: track=%u road=%p reason="
                  "fault scanning tracked bytes", tracked.id, tracked.road);
            ReleaseTrackedRoadSnow(&tracked);
            continue;
        }

        if (changed)
        {
            const char* change = increased && !decreased ? "accumulation" :
                                 decreased && !increased ? "clearing" :
                                                          "mixed";
            long long delta = (long long)newSum - (long long)tracked.sum;
            Event("road-snow state change: track=%u road=%p bytes=%llu "
                  "elapsed_ms=%llu change=%s sum=%llu->%llu delta=%+lld "
                  "changed=%llu increased=%llu(+%llu) decreased=%llu(-%llu) "
                  "zeroed=%llu min=%u->%u max=%u->%u avg=%.3f->%.3f "
                  "hash=%08X->%08X last_clear_call=%lld",
                  tracked.id, tracked.road,
                  (unsigned long long)tracked.length,
                  (unsigned long long)(now - tracked.lastSampleTick), change,
                  (unsigned long long)tracked.sum,
                  (unsigned long long)newSum, delta,
                  (unsigned long long)changed,
                  (unsigned long long)increased,
                  (unsigned long long)increasedUnits,
                  (unsigned long long)decreased,
                  (unsigned long long)decreasedUnits,
                  (unsigned long long)zeroed,
                  tracked.minimum, newMinimum,
                  tracked.maximum, newMaximum,
                  (double)tracked.sum / (double)tracked.length,
                  (double)newSum / (double)tracked.length,
                  tracked.hash, newHash, (long long)tracked.lastClearCall);
            changedRoads++;
        }

        tracked.sum = newSum;
        tracked.minimum = newMinimum;
        tracked.maximum = newMaximum;
        tracked.hash = newHash;
        tracked.lastSampleTick = now;
        active++;
    }

    if (g_cfg.roadStateHeartbeatSeconds > 0 &&
        (!g_nextRoadStateHeartbeatTick || now >= g_nextRoadStateHeartbeatTick))
    {
        Event("road-snow tracker status: active_roads=%d changed_this_sample=%d "
              "sample_interval_ms=%d retention_seconds=%d",
              active, changedRoads, g_cfg.roadStateSampleIntervalMs,
              g_cfg.roadStateTrackingSeconds);
        g_nextRoadStateHeartbeatTick =
            now + (ULONGLONG)g_cfg.roadStateHeartbeatSeconds * 1000ULL;
    }

    uint64_t overlaySum = 0;
    uint64_t overlayBytes = 0;
    unsigned overlayMaximum = 0;
    int overlayRoads = 0;
    for (int i = 0; i < limit; ++i)
    {
        const TrackedRoadSnow& tracked = g_trackedRoads[i];
        if (!tracked.active) continue;
        overlayRoads++;
        overlaySum += tracked.sum;
        overlayBytes += tracked.length;
        if (tracked.maximum > overlayMaximum)
            overlayMaximum = tracked.maximum;
    }
    InterlockedExchange(&g_overlayTrackedRoads, overlayRoads);
    InterlockedExchange64(&g_overlayTrackedBytes, (LONG64)overlayBytes);
    InterlockedExchange64(&g_overlayTrackedSnowSum, (LONG64)overlaySum);
    InterlockedExchange(&g_overlayTrackedSnowMaximum, (LONG)overlayMaximum);

    LeaveCriticalSection(&g_maskDataLock);
}

// ------------------------------------------------ instruction-level writer probe

static int ScalePositiveRoadSnowDelta(int delta)
{
    LONG64 factor = InterlockedCompareExchange64(
        &g_snowAccumulationFactorMillion, 0, 0);
    LONG64 oldRemainder = 0;
    LONG64 newRemainder = 0;
    LONG64 scaled = 0;

    do
    {
        oldRemainder = InterlockedCompareExchange64(
            &g_snowAccumulationRemainder, 0, 0);
        LONG64 numerator = (LONG64)delta * factor + oldRemainder;
        scaled = numerator / 1000000LL;
        newRemainder = numerator % 1000000LL;
        if (scaled > INT_MAX)
        {
            scaled = INT_MAX;
            newRemainder = 0;
        }
    }
    while (InterlockedCompareExchange64(&g_snowAccumulationRemainder,
                                        newRemainder, oldRemainder) !=
           oldRemainder);

    return (int)scaled;
}

static void ResetInternalSnowBurstState(void)
{
    AcquireSRWLockExclusive(&g_internalSnowBurstLock);
    g_internalSnowBurstLastCallTick = 0;
    g_internalSnowBurstForwarded = 0;
    g_internalSnowBurstCapped = 0;
    ReleaseSRWLockExclusive(&g_internalSnowBurstLock);
    InterlockedExchange(&g_overlaySnowBurstForwarded, 0);
    InterlockedExchange(&g_overlaySnowBurstCapped, 0);
}

static void ResetGradualSnowState(void)
{
    LONG pending = InterlockedExchange(&g_gradualSnowPendingUnits, 0);
    if (pending > 0)
        InterlockedExchangeAdd64(&g_gradualSnowCancelledUnits, pending);
    InterlockedExchange64(&g_gradualSnowNextStepTick, 0);
    InterlockedExchange64(&g_gradualSnowLastGameMinuteMilli, 0);
    InterlockedExchange(&g_gradualVisualPendingUnits, 0);
}

static bool SuppressQueuedWeatherSnowMaskRequest(void* world)
{
    if (!world || !ReadablePtr(
            (BYTE*)world + OFF_MASK_DIRTY_REQUEST, sizeof(SHORT)))
        return false;

    volatile SHORT* request = (volatile SHORT*)(
        (BYTE*)world + OFF_MASK_DIRTY_REQUEST);
    SHORT observed = 0;
    __try
    {
        observed = InterlockedCompareExchange16(request, 0, 0);
        if (observed == (SHORT)0x0101)
        {
            if (InterlockedCompareExchange16(
                    request, 0, (SHORT)0x0101) == (SHORT)0x0101)
            {
                InterlockedIncrement64(&g_gradualMaskFlagsSuppressed);
                return true;
            }
        }
    }
    __except (FaultFilter(PLUGIN_NAME " queued snow-mask request",
                          GetExceptionInformation()))
    {
        observed = 0;
    }

    InterlockedIncrement64(&g_gradualMaskFlagsUnexpected);
    InterlockedExchange(&g_globalMaskLastRequestWord,
                        (LONG)(unsigned short)observed);
    return false;
}

static bool ReplayGradualWeatherSnowMaskRequest(void* world)
{
    if (!world || !ReadablePtr(
            (BYTE*)world + OFF_MASK_DIRTY_REQUEST, sizeof(SHORT)))
        return false;

    volatile SHORT* request = (volatile SHORT*)(
        (BYTE*)world + OFF_MASK_DIRTY_REQUEST);
    __try
    {
        SHORT previous = InterlockedCompareExchange16(
            request, (SHORT)0x0101, 0);
        InterlockedExchange(&g_globalMaskLastRequestWord,
                            (LONG)(unsigned short)previous);
        if (previous == 0 || previous == (SHORT)0x0101)
        {
            InterlockedIncrement64(&g_gradualMaskFlagsReplayed);
            return true;
        }
        // A different mask request owns these two bytes. Leave it untouched;
        // the next gradual update may safely retry its own visual refresh.
        InterlockedIncrement64(&g_gradualMaskFlagsUnexpected);
        return false;
    }
    __except (FaultFilter(PLUGIN_NAME " gradual snow-mask request",
                          GetExceptionInformation()))
    {
        InterlockedIncrement64(&g_gradualMaskFlagsUnexpected);
        return false;
    }
}

// Internal road snow remains fine-grained, but a synchronous terrain-mask
// readback for every single unit can block a complete 60-Hz frame. Accumulate
// released units and request one visible redraw only after a configurable
// batch. The last smaller remainder is still flushed when the internal queue
// becomes empty, so visual and simulated snow always finish at the same value.
static bool FlushGradualVisualSnowBatch(void* world, bool allowRemainder)
{
    LONG pending = InterlockedCompareExchange(
        &g_gradualVisualPendingUnits, 0, 0);
    LONG batch = g_cfg.gradualVisualBatchUnits;
    if (batch < 1) batch = 1;
    if (pending <= 0 || (!allowRemainder && pending < batch)) return false;

    LONG units = pending < batch ? pending : batch;
    if (!ReplayGradualWeatherSnowMaskRequest(world)) return false;

    LONG previous = InterlockedCompareExchange(
        &g_gradualVisualPendingUnits, pending - units, pending);
    if (previous != pending)
    {
        // The redraw request is harmless: without publishing a logical delta
        // it only reapplies the current visual state on the next mask pass.
        InterlockedIncrement64(&g_gradualMaskFlagsUnexpected);
        return false;
    }

    SchedulePlowVisualEventSync(world, (int)units);
    InterlockedIncrement64(&g_gradualVisualBatches);
    InterlockedExchangeAdd64(&g_gradualVisualReleasedUnits, units);
    return true;
}

static int QueueGradualSnowUnits(int units)
{
    if (units <= 0) return 0;

    LONG observed = InterlockedCompareExchange(
        &g_gradualSnowPendingUnits, 0, 0);
    LONG accepted = 0;
    for (;;)
    {
        LONG room = MAX_GRADUAL_SNOW_PENDING_UNITS - observed;
        if (room < 0) room = 0;
        accepted = units < room ? units : room;
        LONG wanted = observed + accepted;
        LONG previous = InterlockedCompareExchange(
            &g_gradualSnowPendingUnits, wanted, observed);
        if (previous == observed) break;
        observed = previous;
    }

    if (accepted > 0)
    {
        InterlockedExchangeAdd64(&g_gradualSnowQueuedUnits, accepted);
        if (observed == 0)
            InterlockedExchange64(
                &g_gradualSnowNextStepTick,
                (LONG64)(GetTickCount64() +
                         (ULONGLONG)g_cfg.gradualSnowStepIntervalMs));
    }
    if (accepted < units)
        InterlockedExchangeAdd64(&g_gradualSnowDroppedUnits,
                                 (LONG64)(units - accepted));
    return (int)accepted;
}

// Cancels at most units pending positive snow. A negative limit clears the
// complete queue, which is used for full resets and world changes.
static int CancelGradualSnowUnits(int units)
{
    LONG observed = InterlockedCompareExchange(
        &g_gradualSnowPendingUnits, 0, 0);
    LONG cancelled = 0;
    for (;;)
    {
        cancelled = units < 0 || units > observed ? observed : units;
        LONG wanted = observed - cancelled;
        LONG previous = InterlockedCompareExchange(
            &g_gradualSnowPendingUnits, wanted, observed);
        if (previous == observed)
        {
            if (wanted == 0)
                InterlockedExchange64(&g_gradualSnowNextStepTick, 0);
            break;
        }
        observed = previous;
    }
    if (cancelled > 0)
        InterlockedExchangeAdd64(&g_gradualSnowCancelledUnits, cancelled);
    return (int)cancelled;
}

static bool IsVerifiedWeatherSnowCaller(DWORD returnRva)
{
    return returnRva == RVA_WEATHER_SNOW_ADJUST_RETURN_1 ||
           returnRva == RVA_WEATHER_SNOW_ADJUST_RETURN_2;
}

// One snowfall can call the verified global adjustment function many times in
// a few milliseconds. Scaling every +30 call independently still produced a
// large one-frame jump. This additional quiet-gap burst budget limits the sum
// forwarded by all closely grouped positive calls. Units above the budget are
// deliberately discarded; a new burst begins after the configured quiet gap.
static int LimitPositiveRoadSnowBurst(int scaled)
{
    int limit = g_cfg.maximumSnowAccumulationPerBurst;
    if (scaled <= 0 || limit <= 0)
    {
        InterlockedExchange(&g_overlaySnowBurstForwarded, scaled);
        InterlockedExchange(&g_overlaySnowBurstCapped, 0);
        return scaled;
    }

    ULONGLONG now = GetTickCount64();
    int forwarded = 0;
    int capped = 0;
    int burstForwarded = 0;
    int burstCapped = 0;
    AcquireSRWLockExclusive(&g_internalSnowBurstLock);
    if (!g_internalSnowBurstLastCallTick ||
        now - g_internalSnowBurstLastCallTick >
            (ULONGLONG)g_cfg.snowBurstResetAfterMs)
    {
        g_internalSnowBurstForwarded = 0;
        g_internalSnowBurstCapped = 0;
    }
    g_internalSnowBurstLastCallTick = now;
    int remaining = limit - g_internalSnowBurstForwarded;
    if (remaining < 0) remaining = 0;
    forwarded = scaled < remaining ? scaled : remaining;
    capped = scaled - forwarded;
    g_internalSnowBurstForwarded += forwarded;
    g_internalSnowBurstCapped += capped;
    burstForwarded = g_internalSnowBurstForwarded;
    burstCapped = g_internalSnowBurstCapped;
    ReleaseSRWLockExclusive(&g_internalSnowBurstLock);

    if (capped > 0)
        InterlockedExchangeAdd64(&g_internalSnowBurstCappedUnits, capped);
    InterlockedExchange(&g_overlaySnowBurstForwarded, burstForwarded);
    InterlockedExchange(&g_overlaySnowBurstCapped, burstCapped);
    return forwarded;
}

static int ScaleNaturalRoadSnowReduction(int delta)
{
    if (delta >= 0) return delta;
    LONG64 factor = InterlockedCompareExchange64(
        &g_snowReductionFactorMillion, 0, 0);
    LONG64 magnitude = -(LONG64)delta;
    LONG64 oldRemainder = 0;
    LONG64 newRemainder = 0;
    LONG64 scaled = 0;

    do
    {
        oldRemainder = InterlockedCompareExchange64(
            &g_snowReductionRemainder, 0, 0);
        LONG64 numerator = magnitude * factor + oldRemainder;
        scaled = numerator / 1000000LL;
        newRemainder = numerator % 1000000LL;
        if (scaled > INT_MAX)
        {
            scaled = INT_MAX;
            newRemainder = 0;
        }
    }
    while (InterlockedCompareExchange64(&g_snowReductionRemainder,
                                        newRemainder, oldRemainder) !=
           oldRemainder);

    return -(int)scaled;
}

static bool ResolvePlowEffectByte(PlowEffectPoint& point, BYTE** address)
{
    if (address) *address = NULL;
    if (!point.road || !point.vectorBegin ||
        point.index >= point.vectorLength ||
        point.vectorLength > (SIZE_T)g_cfg.maximumRoadBytes ||
        !ReadablePtr(point.road, OFF_ROAD_SNOW_END + sizeof(void*)))
        return false;

    BYTE* begin = NULL;
    BYTE* end = NULL;
    __try
    {
        BYTE* road = (BYTE*)point.road;
        begin = *(BYTE**)(road + OFF_ROAD_SNOW_BEG);
        end = *(BYTE**)(road + OFF_ROAD_SNOW_END);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (begin != point.vectorBegin || !end || end < begin ||
        (SIZE_T)(end - begin) != point.vectorLength ||
        !ReadablePtr(begin + point.index, 1))
        return false;
    if (address) *address = begin + point.index;
    return true;
}

static void ApplyRoadSnowAdjustWithPlowEffect(void* world, int forwarded)
{
    if (WeatherPersistenceSuspended() || !g_cfg.plowEffect || forwarded <= 0 ||
        !InterlockedCompareExchange(&g_plowEffectLockReady, 0, 0) ||
        world != InterlockedCompareExchangePointer(&g_weatherWorld, NULL, NULL))
    {
        o_RoadSnowAdjust(world, forwarded);
        return;
    }

    LONG64 now = InterlockedCompareExchange64(&g_gameMinuteMilli, 0, 0);
    LONG generation = InterlockedCompareExchange(&g_gameWorldGeneration, 0, 0);
    LONG64 protectionDuration = PlowProtectionDuration();
    LONG64 saltDuration = PlowSaltDuration();
    LONG64 totalDuration = protectionDuration + saltDuration;
    if (now <= 0 || generation <= 0 || totalDuration <= 0)
    {
        o_RoadSnowAdjust(world, forwarded);
        return;
    }

    EnterCriticalSection(&g_plowEffectLock);
    bool sampledAny = false;
    const int limit = g_cfg.maximumTrackedPlowPoints;
    for (int i = 0; i < limit; ++i)
    {
        PlowEffectPoint& point = g_plowEffectPoints[i];
        point.sampledPhase = 0;
        if (!point.active) continue;
        if (point.generation != generation)
        {
            point.active = 0;
            InterlockedIncrement64(&g_plowEffectExpired);
            continue;
        }

        LONG64 age = now - point.clearedGameMinuteMilli;
        if (age < 0 || age >= totalDuration)
        {
            point.active = 0;
            InterlockedIncrement64(&g_plowEffectExpired);
            continue;
        }

        BYTE* address = NULL;
        if (!ResolvePlowEffectByte(point, &address))
        {
            point.active = 0;
            InterlockedIncrement64(&g_plowEffectExpired);
            continue;
        }

        __try
        {
            point.sampledBefore = *address;
            point.sampledClearTime = point.clearedGameMinuteMilli;
            point.sampledPhase = age < protectionDuration ? 1 : 2;
            sampledAny = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            point.active = 0;
            point.sampledPhase = 0;
        }
    }

    if (!sampledAny)
    {
        LeaveCriticalSection(&g_plowEffectLock);
        o_RoadSnowAdjust(world, forwarded);
        return;
    }

    // The verified game function only iterates and clamps road snow bytes. It
    // cannot call the snowplow clearing hook, so retaining this private lock
    // across the call keeps each before/after pair coherent.
    o_RoadSnowAdjust(world, forwarded);

    LONG64 saltFactor = InterlockedCompareExchange64(
        &g_plowSaltFactorMillion, 0, 0);
    for (int i = 0; i < limit; ++i)
    {
        PlowEffectPoint& point = g_plowEffectPoints[i];
        if (!point.active || !point.sampledPhase ||
            point.sampledClearTime != point.clearedGameMinuteMilli)
            continue;

        BYTE* address = NULL;
        if (!ResolvePlowEffectByte(point, &address))
        {
            point.active = 0;
            continue;
        }

        __try
        {
            BYTE after = *address;
            BYTE before = point.sampledBefore;
            if (after > before)
            {
                unsigned added = (unsigned)after - (unsigned)before;
                LONG64 legacyFactor = point.sampledPhase == 1
                    ? 0 : saltFactor;
                LONG64 phaseFactor = BlendGritPhaseFactor(
                    legacyFactor, point.treatmentStrengthMillion);
                LONG64 numerator =
                    (LONG64)added * phaseFactor +
                    point.saltRemainderMillion;
                unsigned wanted =
                    (unsigned)(numerator / GRIT_FACTOR_MILLION);
                point.saltRemainderMillion =
                    numerator % GRIT_FACTOR_MILLION;
                if (wanted > added) wanted = added;
                unsigned prevented = added - wanted;
                if (point.sampledPhase == 1)
                {
                    InterlockedIncrement64(&g_plowEffectProtectionWrites);
                    InterlockedExchangeAdd64(&g_plowEffectProtectionUnits,
                                             (LONG64)prevented);
                }
                else
                {
                    InterlockedIncrement64(&g_plowEffectSaltWrites);
                    InterlockedExchangeAdd64(&g_plowEffectSaltPreventedUnits,
                                             (LONG64)prevented);
                }
                *address = (BYTE)((unsigned)before + wanted);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            point.active = 0;
        }
        point.sampledPhase = 0;
    }
    LeaveCriticalSection(&g_plowEffectLock);
}

// Releases at most one small positive internal step during a regular game
// update. Visible terrain-mask redraws are deliberately batched separately.
// Requiring advancing game time prevents queued snow from accumulating while
// the simulation is paused. Calling the verified original function here keeps
// internal road state, vehicle behaviour and the game's following mask redraw
// on the same update thread.
static void ServiceGradualRoadSnow(void* world, ULONGLONG now)
{
    if (!g_cfg.internalSnowLimiter || !g_cfg.gradualSnowAccumulation ||
        !o_RoadSnowAdjust || !world ||
        world != InterlockedCompareExchangePointer(
            &g_weatherWorld, NULL, NULL))
        return;

    LONG64 gameNow = InterlockedCompareExchange64(
        &g_gameMinuteMilli, 0, 0);
    LONG64 previousGame = InterlockedExchange64(
        &g_gradualSnowLastGameMinuteMilli, gameNow);
    if (gameNow <= 0 || previousGame <= 0 || gameNow <= previousGame)
        return;

    LONG pending = InterlockedCompareExchange(
        &g_gradualSnowPendingUnits, 0, 0);
    if (pending <= 0)
    {
        FlushGradualVisualSnowBatch(world, true);
        return;
    }

    // Release follows the weather (0.3.1, roll semantics 0.3.2): once the
    // captured weather roll is no longer 1 (snowing), the rest of the queue is
    // dropped, so the road snow stops with the snowfall instead of trickling
    // on for the remaining burst. An unreadable or stale snapshot changes
    // nothing (fail open).
    if (g_cfg.releaseFollowsWeather)
    {
        WeatherSnapshot weather;
        if (ReadWeatherSnapshot(&weather) && weather.precipitationState != 1)
        {
            LONG dropped = InterlockedExchange(&g_gradualSnowPendingUnits, 0);
            if (dropped > 0)
            {
                InterlockedExchangeAdd64(&g_gradualSnowCancelledUnits, dropped);
                Event("gradual snow release stopped with the weather: "
                      "precipitation_state=%d (%s), dropped_units=%ld",
                      weather.precipitationState,
                      WeatherRollName(weather.precipitationState),
                      (long)dropped);
            }
            InterlockedExchange64(&g_gradualSnowNextStepTick, 0);
            FlushGradualVisualSnowBatch(world, true);
            return;
        }
    }

    LONG64 due = InterlockedCompareExchange64(
        &g_gradualSnowNextStepTick, 0, 0);
    if (due <= 0)
    {
        InterlockedExchange64(
            &g_gradualSnowNextStepTick,
            (LONG64)(now + (ULONGLONG)g_cfg.gradualSnowStepIntervalMs));
        return;
    }
    if (now < (ULONGLONG)due) return;

    LONG step = g_cfg.gradualSnowStepUnits;
    if (step > pending) step = pending;
    LONG previous = InterlockedCompareExchange(
        &g_gradualSnowPendingUnits, pending - step, pending);
    if (previous != pending) return;

    ApplyRoadSnowAdjustWithPlowEffect(world, (int)step);
    InterlockedExchangeAdd64(&g_gradualSnowReleasedUnits, step);
    InterlockedExchangeAdd(&g_gradualVisualPendingUnits, step);

    LONG remaining = pending - step;
    FlushGradualVisualSnowBatch(world, remaining <= 0);
    InterlockedExchange64(
        &g_gradualSnowNextStepTick,
        remaining > 0
            ? (LONG64)(now +
                       (ULONGLONG)g_cfg.gradualSnowStepIntervalMs)
            : 0);
}

static void h_RoadSnowAdjust(void* world, int delta)
{
    int forwarded = delta;
    int immediate = delta;
    DWORD callerReturnRva = ExecutableCallerRva(_ReturnAddress());
    if (InterlockedCompareExchange(&g_active, 0, 0) &&
        g_cfg.internalSnowLimiter && delta > 0)
    {
        int scaled = ScalePositiveRoadSnowDelta(delta);
        forwarded = LimitPositiveRoadSnowBurst(scaled);
        immediate = forwarded;
        if (g_cfg.gradualSnowAccumulation &&
            world == InterlockedCompareExchangePointer(
                &g_weatherWorld, NULL, NULL) &&
            IsVerifiedWeatherSnowCaller(callerReturnRva))
        {
            forwarded = QueueGradualSnowUnits(forwarded);
            immediate = 0;
            SuppressQueuedWeatherSnowMaskRequest(world);
        }
        InterlockedIncrement64(&g_internalSnowCalls);
        InterlockedExchangeAdd64(&g_internalSnowInputUnits, delta);
        InterlockedExchangeAdd64(&g_internalSnowForwardedUnits, forwarded);
        if (!forwarded) InterlockedIncrement64(&g_internalSnowZeroCalls);
        InterlockedExchange(&g_internalSnowLastInput, delta);
        InterlockedExchange(&g_internalSnowLastForwarded, forwarded);
        InterlockedExchange(&g_internalSnowLastCallerRva,
                            (LONG)callerReturnRva);
    }
    else if (InterlockedCompareExchange(&g_active, 0, 0) &&
             g_cfg.naturalMelting && delta == -30)
    {
        forwarded = ScaleNaturalRoadSnowReduction(delta);
        immediate = forwarded;
        InterlockedIncrement64(&g_naturalMeltCalls);
        InterlockedExchangeAdd64(&g_naturalMeltInputUnits, -(LONG64)delta);
        InterlockedExchangeAdd64(&g_naturalMeltForwardedUnits,
                                 -(LONG64)forwarded);
        if (!forwarded) InterlockedIncrement64(&g_naturalMeltZeroCalls);
        InterlockedExchange(&g_naturalMeltLastInput, delta);
        InterlockedExchange(&g_naturalMeltLastForwarded, forwarded);

        if (g_cfg.gradualSnowAccumulation && forwarded < 0)
        {
            int cancelled = CancelGradualSnowUnits(-forwarded);
            immediate += cancelled;
        }

        // Several verified callers may report the same natural-melt pulse for
        // different road groups. Publish only the part that reaches existing
        // road bytes; cancelling snow that was still queued needs no visual
        // melt because it was never drawn.
        if (immediate < 0 &&
            InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) &&
            world == InterlockedCompareExchangePointer(
                &g_weatherWorld, NULL, NULL) &&
            InterlockedCompareExchange64(
                &g_plowVisualActivePublished, 0, 0) > 0)
            InterlockedExchange64(&g_plowVisualPendingMeltUnits,
                                  -(LONG64)immediate);
    }

    if (g_cfg.gradualSnowAccumulation && delta < 0 && delta != -30)
        CancelGradualSnowUnits(-1);

    // The full -255 reset and every unrecognised negative value pass through
    // bit-for-bit unchanged. Only the verified -30 natural-reduction pulse is
    // scaled, and only while [natural_melting] is enabled.
    if (InterlockedCompareExchange(&g_active, 0, 0))
    {
        InterlockedExchange(&g_overlaySnowDeltaRaw, (LONG)delta);
        InterlockedExchange(&g_overlaySnowDeltaEffective, (LONG)forwarded);
        InterlockedExchange(&g_overlaySnowDeltaValid, 1);
    }
    ApplyRoadSnowAdjustWithPlowEffect(world, immediate);
    if (immediate > 0 || (delta == -30 && immediate < 0))
        SchedulePlowVisualEventSync(world, immediate);
}

static void FlushInternalSnowLimiterStatus(void)
{
    if (!g_cfg.detailedEvents) return;
    if (!g_cfg.internalSnowLimiter) return;
    LONG64 calls = InterlockedCompareExchange64(&g_internalSnowCalls, 0, 0);
    LONG64 gradualReleased = InterlockedCompareExchange64(
        &g_gradualSnowReleasedUnits, 0, 0);
    LONG64 maskSuppressed = InterlockedCompareExchange64(
        &g_gradualMaskFlagsSuppressed, 0, 0);
    LONG64 maskReplayed = InterlockedCompareExchange64(
        &g_gradualMaskFlagsReplayed, 0, 0);
    LONG64 maskUnexpected = InterlockedCompareExchange64(
        &g_gradualMaskFlagsUnexpected, 0, 0);
    LONG64 globalSnowMaskCalls = InterlockedCompareExchange64(
        &g_globalSnowMaskUpdateCalls, 0, 0);
    LONG64 globalInlinePasses = InterlockedCompareExchange64(
        &g_globalMaskInlinePasses, 0, 0);
    LONG64 globalInlineFailures = InterlockedCompareExchange64(
        &g_globalMaskInlineFailures, 0, 0);
    LONG64 visualBatches = InterlockedCompareExchange64(
        &g_gradualVisualBatches, 0, 0);
    LONG64 visualReleased = InterlockedCompareExchange64(
        &g_gradualVisualReleasedUnits, 0, 0);
    if (calls == g_internalSnowLoggedCalls &&
        gradualReleased == g_internalSnowLoggedGradualReleased &&
        maskSuppressed == g_internalSnowLoggedMaskFlagsSuppressed &&
        maskReplayed == g_internalSnowLoggedMaskFlagsReplayed &&
        maskUnexpected == g_internalSnowLoggedMaskFlagsUnexpected &&
        globalSnowMaskCalls == g_internalSnowLoggedGlobalSnowMaskCalls &&
        globalInlinePasses == g_internalSnowLoggedGlobalMaskInlinePasses &&
        globalInlineFailures == g_internalSnowLoggedGlobalMaskInlineFailures &&
        visualBatches == g_internalSnowLoggedVisualBatches &&
        visualReleased == g_internalSnowLoggedVisualReleasedUnits)
        return;

    ULONGLONG now = GetTickCount64();
    if (g_nextInternalSnowStatusTick && now < g_nextInternalSnowStatusTick)
        return;

    LONG64 input = InterlockedCompareExchange64(
        &g_internalSnowInputUnits, 0, 0);
    LONG64 forwarded = InterlockedCompareExchange64(
        &g_internalSnowForwardedUnits, 0, 0);
    LONG64 zeroCalls = InterlockedCompareExchange64(
        &g_internalSnowZeroCalls, 0, 0);
    LONG64 burstCappedUnits = InterlockedCompareExchange64(
        &g_internalSnowBurstCappedUnits, 0, 0);
    LONG gradualPending = InterlockedCompareExchange(
        &g_gradualSnowPendingUnits, 0, 0);
    LONG visualPending = InterlockedCompareExchange(
        &g_gradualVisualPendingUnits, 0, 0);
    LONG64 gradualQueued = InterlockedCompareExchange64(
        &g_gradualSnowQueuedUnits, 0, 0);
    LONG64 gradualCancelled = InterlockedCompareExchange64(
        &g_gradualSnowCancelledUnits, 0, 0);
    LONG64 gradualDropped = InterlockedCompareExchange64(
        &g_gradualSnowDroppedUnits, 0, 0);
    LONG64 deltaCalls = calls - g_internalSnowLoggedCalls;
    LONG64 deltaInput = input - g_internalSnowLoggedInput;
    LONG64 deltaForwarded = forwarded - g_internalSnowLoggedForwarded;
    LONG64 deltaZeroCalls = zeroCalls - g_internalSnowLoggedZeroCalls;
    LONG64 deltaBurstCappedUnits =
        burstCappedUnits - g_internalSnowLoggedBurstCappedUnits;
    LONG callerReturnRva = InterlockedCompareExchange(
        &g_internalSnowLastCallerRva, 0, 0);

    Event("EXPERIMENT internal road-snow accumulation limiter: calls=%lld "
          "input_units=%lld forwarded_units=%lld prevented_units=%lld "
          "zero_forwarded_calls=%lld last_delta=%ld->%ld "
          "burst=%ld/%d burst_capped_units=%lld "
          "gradual[enabled=%d pending=%ld step=%d interval_ms=%d "
          "queued=%lld released=%lld cancelled=%lld dropped=%lld "
          "visual_pending=%ld visual_batch_units=%d visual_batches=%lld "
          "visual_released=%lld] "
          "mask_path[suppressed=%lld replayed=%lld unexpected=%lld "
          "gpu_snow_passes=%lld inline_passes=%lld inline_failures=%lld "
          "last_request_word=0x%04lX] "
          "last_caller_return_rva=%s0x%X multiplier=%.6f totals[calls=%lld "
          "input=%lld forwarded=%lld prevented=%lld burst_capped=%lld]",
          (long long)deltaCalls, (long long)deltaInput,
          (long long)deltaForwarded,
          (long long)(deltaInput - deltaForwarded),
          (long long)deltaZeroCalls,
          InterlockedCompareExchange(&g_internalSnowLastInput, 0, 0),
          InterlockedCompareExchange(&g_internalSnowLastForwarded, 0, 0),
          InterlockedCompareExchange(&g_overlaySnowBurstForwarded, 0, 0),
          g_cfg.maximumSnowAccumulationPerBurst,
          (long long)deltaBurstCappedUnits,
          g_cfg.gradualSnowAccumulation, gradualPending,
          g_cfg.gradualSnowStepUnits, g_cfg.gradualSnowStepIntervalMs,
          (long long)gradualQueued, (long long)gradualReleased,
          (long long)gradualCancelled, (long long)gradualDropped,
          visualPending, g_cfg.gradualVisualBatchUnits,
          (long long)visualBatches, (long long)visualReleased,
          (long long)maskSuppressed, (long long)maskReplayed,
          (long long)maskUnexpected, (long long)globalSnowMaskCalls,
          (long long)globalInlinePasses, (long long)globalInlineFailures,
          InterlockedCompareExchange(&g_globalMaskLastRequestWord, 0, 0) &
              0xFFFF,
          callerReturnRva == (LONG)0xFFFFFFFFu ? "external/" : "",
          (unsigned)callerReturnRva, g_cfg.snowAccumulationMultiplier,
          (long long)calls, (long long)input, (long long)forwarded,
          (long long)(input - forwarded), (long long)burstCappedUnits);

    g_internalSnowLoggedCalls = calls;
    g_internalSnowLoggedInput = input;
    g_internalSnowLoggedForwarded = forwarded;
    g_internalSnowLoggedZeroCalls = zeroCalls;
    g_internalSnowLoggedBurstCappedUnits = burstCappedUnits;
    g_internalSnowLoggedGradualReleased = gradualReleased;
    g_internalSnowLoggedMaskFlagsSuppressed = maskSuppressed;
    g_internalSnowLoggedMaskFlagsReplayed = maskReplayed;
    g_internalSnowLoggedMaskFlagsUnexpected = maskUnexpected;
    g_internalSnowLoggedGlobalSnowMaskCalls = globalSnowMaskCalls;
    g_internalSnowLoggedGlobalMaskInlinePasses = globalInlinePasses;
    g_internalSnowLoggedGlobalMaskInlineFailures = globalInlineFailures;
    g_internalSnowLoggedVisualBatches = visualBatches;
    g_internalSnowLoggedVisualReleasedUnits = visualReleased;
    g_nextInternalSnowStatusTick = now + 5000ULL;
}

static void FlushNaturalMeltingStatus(void)
{
    if (!g_cfg.detailedEvents) return;
    if (!g_cfg.naturalMelting) return;
    LONG64 calls = InterlockedCompareExchange64(&g_naturalMeltCalls, 0, 0);
    if (calls == g_naturalMeltLoggedCalls) return;

    ULONGLONG now = GetTickCount64();
    if (g_nextNaturalMeltStatusTick && now < g_nextNaturalMeltStatusTick)
        return;

    LONG64 input = InterlockedCompareExchange64(
        &g_naturalMeltInputUnits, 0, 0);
    LONG64 forwarded = InterlockedCompareExchange64(
        &g_naturalMeltForwardedUnits, 0, 0);
    LONG64 zeroCalls = InterlockedCompareExchange64(
        &g_naturalMeltZeroCalls, 0, 0);
    LONG64 deltaCalls = calls - g_naturalMeltLoggedCalls;
    LONG64 deltaInput = input - g_naturalMeltLoggedInput;
    LONG64 deltaForwarded = forwarded - g_naturalMeltLoggedForwarded;
    LONG64 deltaZeroCalls = zeroCalls - g_naturalMeltLoggedZeroCalls;

    Event("EXPERIMENT natural road-snow melting: calls=%lld "
          "input_reduction_units=%lld forwarded_reduction_units=%lld "
          "slowed_units=%lld zero_forwarded_calls=%lld last_delta=%ld->%ld "
          "multiplier=%.6f totals[calls=%lld input=%lld forwarded=%lld "
          "slowed=%lld]",
          (long long)deltaCalls, (long long)deltaInput,
          (long long)deltaForwarded,
          (long long)(deltaInput - deltaForwarded),
          (long long)deltaZeroCalls,
          InterlockedCompareExchange(&g_naturalMeltLastInput, 0, 0),
          InterlockedCompareExchange(&g_naturalMeltLastForwarded, 0, 0),
          g_cfg.snowReductionMultiplier,
          (long long)calls, (long long)input, (long long)forwarded,
          (long long)(input - forwarded));

    g_naturalMeltLoggedCalls = calls;
    g_naturalMeltLoggedInput = input;
    g_naturalMeltLoggedForwarded = forwarded;
    g_naturalMeltLoggedZeroCalls = zeroCalls;
    g_nextNaturalMeltStatusTick = now + 5000ULL;
}

static void FlushPlowEffectStatus(void)
{
    if (!g_cfg.detailedEvents) return;
    if (!g_cfg.plowEffect) return;
    LONG64 registered = InterlockedCompareExchange64(
        &g_plowEffectRegistered, 0, 0);
    LONG64 decreaseCandidates = InterlockedCompareExchange64(
        &g_plowEffectDecreaseCandidates, 0, 0);
    LONG64 initialZeroCandidates = InterlockedCompareExchange64(
        &g_plowEffectInitialZeroCandidates, 0, 0);
    LONG64 rejectedNoRecentMask = InterlockedCompareExchange64(
        &g_plowEffectRejectedNoRecentMask, 0, 0);
    LONG64 protectedWrites = InterlockedCompareExchange64(
        &g_plowEffectProtectionWrites, 0, 0);
    LONG64 saltWrites = InterlockedCompareExchange64(
        &g_plowEffectSaltWrites, 0, 0);
    LONG64 gritMatches = InterlockedCompareExchange64(
        &g_gritTreatmentMatches, 0, 0);
    LONG64 gritMaterials = InterlockedCompareExchange64(
        &g_gritTreatmentMaterialEvents, 0, 0);
    LONG64 gritDry = InterlockedCompareExchange64(
        &g_gritTreatmentDryEvents, 0, 0);
    LONG64 gritFallbacks = InterlockedCompareExchange64(
        &g_gritTreatmentFallbackEvents, 0, 0);
    if (registered == g_plowEffectLoggedRegistered &&
        decreaseCandidates == g_plowEffectLoggedDecreaseCandidates &&
        initialZeroCandidates ==
            g_plowEffectLoggedInitialZeroCandidates &&
        rejectedNoRecentMask == g_plowEffectLoggedRejectedNoRecentMask &&
        protectedWrites == g_plowEffectLoggedProtectionWrites &&
        saltWrites == g_plowEffectLoggedSaltWrites &&
        gritMatches == g_gritLoggedTreatmentMatches &&
        gritMaterials == g_gritLoggedTreatmentMaterialEvents &&
        gritDry == g_gritLoggedTreatmentDryEvents &&
        gritFallbacks == g_gritLoggedTreatmentFallbackEvents)
        return;

    ULONGLONG now = GetTickCount64();
    if (g_nextPlowEffectStatusTick && now < g_nextPlowEffectStatusTick)
        return;

    int activePoints = 0;
    if (InterlockedCompareExchange(&g_plowEffectLockReady, 0, 0))
    {
        EnterCriticalSection(&g_plowEffectLock);
        for (int i = 0; i < g_cfg.maximumTrackedPlowPoints; ++i)
            if (g_plowEffectPoints[i].active) activePoints++;
        LeaveCriticalSection(&g_plowEffectLock);
    }

    Event("EXPERIMENT plow after-effect: zero_transition_candidates=%lld "
          "initial_zero_candidates=%lld "
          "registered=%lld refreshed=%lld rejected_without_recent_mask=%lld "
          "active_points=%d protection_writes=%lld protected_units=%lld "
          "salt_writes=%lld salt_prevented_units=%lld expired=%lld "
          "evicted=%lld protection=%.2f game-minutes salt=%.2f game-hours "
          "salt_multiplier=%.6f grit_service=%s grit_matches=%lld "
          "grit_material=%lld grit_dry=%lld grit_fallback=%lld "
          "visible_clear_matches=%lld visible_brushes_matched=%lld "
          "visible_texels_assigned=%lld visible_unmatched_expired=%lld "
          "dry_preserve=%d dry_internal_cleared=%lld "
          "dry_visual_cleared=%lld dry_internal_preserved=%lld "
          "dry_visual_preserved=%lld lower_internal_preserved=%lld "
          "lower_visual_preserved=%lld",
          (long long)decreaseCandidates, (long long)initialZeroCandidates,
          (long long)registered,
          (long long)InterlockedCompareExchange64(&g_plowEffectRefreshed, 0, 0),
          (long long)rejectedNoRecentMask,
          activePoints, (long long)protectedWrites,
          (long long)InterlockedCompareExchange64(
              &g_plowEffectProtectionUnits, 0, 0),
          (long long)saltWrites,
          (long long)InterlockedCompareExchange64(
              &g_plowEffectSaltPreventedUnits, 0, 0),
          (long long)InterlockedCompareExchange64(&g_plowEffectExpired, 0, 0),
          (long long)InterlockedCompareExchange64(&g_plowEffectEvicted, 0, 0),
          g_cfg.plowProtectionMinutes, g_cfg.plowSaltHours,
          g_cfg.plowSaltAccumulationMultiplier,
          g_gritSpreaderApi ? "active" : "legacy",
          (long long)gritMatches, (long long)gritMaterials,
          (long long)gritDry, (long long)gritFallbacks,
          (long long)InterlockedCompareExchange64(
              &g_gritVisibleTreatmentClearMatches, 0, 0),
          (long long)InterlockedCompareExchange64(
              &g_gritVisibleTreatmentMatches, 0, 0),
          (long long)InterlockedCompareExchange64(
              &g_gritVisibleTreatmentTexels, 0, 0),
           (long long)InterlockedCompareExchange64(
               &g_gritVisibleTreatmentFallbacks, 0, 0),
          g_cfg.dryPlowingPreservesTreatment,
          (long long)InterlockedCompareExchange64(
              &g_gritDryInternalPointsCleared, 0, 0),
          (long long)InterlockedCompareExchange64(
              &g_gritDryVisualTexelsCleared, 0, 0),
          (long long)InterlockedCompareExchange64(
              &g_gritDryInternalPointsPreserved, 0, 0),
          (long long)InterlockedCompareExchange64(
              &g_gritDryVisualTexelsPreserved, 0, 0),
          (long long)InterlockedCompareExchange64(
              &g_gritLowerInternalPointsPreserved, 0, 0),
          (long long)InterlockedCompareExchange64(
              &g_gritLowerVisualTexelsPreserved, 0, 0));

    if (gritMatches != g_gritLoggedTreatmentMatches ||
        gritFallbacks != g_gritLoggedTreatmentFallbackEvents)
    {
        Event("grit-spreader status: matched=%lld material=%lld dry=%lld "
             "fallback_legacy=%lld visible_clear_matches=%lld "
             "visible_brushes_matched=%lld visible_texels_assigned=%lld "
             "visible_unmatched_expired=%lld dry_preserve=%d "
             "dry_internal_cleared=%lld dry_visual_cleared=%lld "
             "dry_internal_preserved=%lld dry_visual_preserved=%lld "
             "lower_internal_preserved=%lld lower_visual_preserved=%lld",
             (long long)gritMatches, (long long)gritMaterials,
             (long long)gritDry, (long long)gritFallbacks,
             (long long)InterlockedCompareExchange64(
                 &g_gritVisibleTreatmentClearMatches, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_gritVisibleTreatmentMatches, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_gritVisibleTreatmentTexels, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_gritVisibleTreatmentFallbacks, 0, 0),
             g_cfg.dryPlowingPreservesTreatment,
             (long long)InterlockedCompareExchange64(
                 &g_gritDryInternalPointsCleared, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_gritDryVisualTexelsCleared, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_gritDryInternalPointsPreserved, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_gritDryVisualTexelsPreserved, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_gritLowerInternalPointsPreserved, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_gritLowerVisualTexelsPreserved, 0, 0));
        Event("visible correlation status: clear_samples=%lld "
             "clears_with_preceding=%lld preceding_candidates=%lld "
             "preceding_same_thread=%lld preceding_cross_thread=%lld "
             "identity_ok=%lld identity_bad=%lld position_valid=%lld "
             "brushes_after_clear=%lld after_same_thread=%lld "
             "after_cross_thread=%lld",
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationClearSamples, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationClearsWithCandidates, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationPrecedingCandidates, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationPrecedingSameThread, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationPrecedingCrossThread, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationIdentityMatches, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationIdentityMismatches, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationPositionValid, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationBrushesAfterClear, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationAfterClearSameThread, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleCorrelationAfterClearCrossThread, 0, 0));
        Event("visible track status: created=%lld bound=%lld rebound=%lld "
             "clear_matches=%lld provisional_brushes=%lld "
             "finalized_brushes=%lld corrected_brushes=%lld "
             "unowned_brushes=%lld treated_expired_brushes=%lld",
             (long long)InterlockedCompareExchange64(
                 &g_visibleTracksCreated, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleTracksBound, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleTracksRebound, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleTrackClearMatches, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleTrackProvisionalBrushes, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleTrackFinalizedBrushes, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleTrackCorrectedBrushes, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleTrackUnownedBrushes, 0, 0),
             (long long)InterlockedCompareExchange64(
                 &g_visibleTrackExpiredBrushes, 0, 0));
    }

    g_plowEffectLoggedRegistered = registered;
    g_plowEffectLoggedDecreaseCandidates = decreaseCandidates;
    g_plowEffectLoggedInitialZeroCandidates = initialZeroCandidates;
    g_plowEffectLoggedRejectedNoRecentMask = rejectedNoRecentMask;
    g_plowEffectLoggedProtectionWrites = protectedWrites;
    g_plowEffectLoggedSaltWrites = saltWrites;
    g_gritLoggedTreatmentMatches = gritMatches;
    g_gritLoggedTreatmentMaterialEvents = gritMaterials;
    g_gritLoggedTreatmentDryEvents = gritDry;
    g_gritLoggedTreatmentFallbackEvents = gritFallbacks;
    g_nextPlowEffectStatusTick = now + 5000ULL;
}

// The hook begins after the original function's null-road guard.  It always
// calls the original trampoline exactly once, even if a diagnostic read or
// temporary allocation fails.
static void h_RoadSnowClearBody(void* vehicle, void* road, void* selector)
{
    if (!InterlockedCompareExchange(&g_active, 0, 0))
    {
        o_RoadSnowClearBody(vehicle, road, selector);
        return;
    }

    LONG64 calls = InterlockedIncrement64(&g_snowplowClearCalls);
    NoteSnowplowClearCall(calls, vehicle, road);

    ClearCallContext clearContext = {};
    clearContext.call = calls;
    clearContext.vehicle = vehicle;
    clearContext.road = road;
    clearContext.selector = selector;
    clearContext.treatmentStrengthMillion = GRIT_FACTOR_MILLION;
    clearContext.treatmentMaterialIndex = -1;

    TsmGritRoadTreatment treatment = {};
    LONG treatmentStrengthMillion = GRIT_FACTOR_MILLION;
    if (ReadCurrentGritTreatment(vehicle, road, selector, &treatment,
                                 &treatmentStrengthMillion))
    {
        clearContext.treatmentMatched = 1;
        clearContext.treatmentStrengthMillion = treatmentStrengthMillion;
        clearContext.treatmentMaterialIndex = treatment.materialIndex;
        clearContext.treatmentFlags = treatment.flags;
        clearContext.treatmentSequence = treatment.sequence;
        strncpy_s(clearContext.treatmentResourceName,
                  sizeof(clearContext.treatmentResourceName),
                  treatment.resourceName, _TRUNCATE);
        InterlockedIncrement64(&g_gritTreatmentMatches);
        if (treatmentStrengthMillion == 0 ||
            (treatment.flags & TSM_GRIT_TREATMENT_DRY))
            InterlockedIncrement64(&g_gritTreatmentDryEvents);
        else
            InterlockedIncrement64(&g_gritTreatmentMaterialEvents);
    }
    else if (g_gritSpreaderApi)
    {
        InterlockedIncrement64(&g_gritTreatmentFallbackEvents);
    }
    ApplyTreatmentToPendingVisibleBrushes(clearContext);

    ClearCallContext* previousContext = NULL;
    bool tlsContextActive = false;
    if (g_clearTls != TLS_OUT_OF_INDEXES)
    {
        previousContext = (ClearCallContext*)TlsGetValue(g_clearTls);
        tlsContextActive = TlsSetValue(g_clearTls, &clearContext) != FALSE;
    }

    RoadSnowState before = {};
    const char* reason = NULL;
    bool beforeValid = ReadRoadSnowState(road, &before, &reason);
    BYTE* beforeBytes = NULL;

    if (beforeValid)
    {
        beforeBytes = (BYTE*)HeapAlloc(GetProcessHeap(), 0, before.length);
        if (!beforeBytes)
        {
            reason = "temporary comparison buffer could not be allocated";
            beforeValid = false;
        }
        else
        {
            __try
            {
                memcpy(beforeBytes, before.begin, before.length);
            }
            __except (FaultFilter(PLUGIN_NAME " road snow-byte copy",
                                  GetExceptionInformation()))
            {
                reason = "fault copying snow-vector bytes";
                beforeValid = false;
            }
        }
    }

    if (!beforeValid)
        LogInvalidRoadOnce(reason, vehicle, road, before.length);

    bool nativeClearCompleted = false;
    __try
    {
        o_RoadSnowClearBody(vehicle, road, selector);
        nativeClearCompleted = true;
    }
    __finally
    {
        // Restore nesting even if the original raises an SEH exception. Do
        // not catch/retry it or run successful-clear bookkeeping on failure.
        if (tlsContextActive)
            TlsSetValue(g_clearTls, previousContext);
        if (!nativeClearCompleted && beforeBytes)
            HeapFree(GetProcessHeap(), 0, beforeBytes);
    }

    LogClearWriteContext(clearContext);
    if (beforeValid)
    {
        for (SIZE_T i = 0; i < clearContext.recordCount; ++i)
        {
            RecordPlowedRoadPoint(
                road, before.begin, before.length,
                clearContext.records[i].index,
                clearContext.treatmentStrengthMillion,
                clearContext.treatmentMaterialIndex,
                clearContext.treatmentFlags);
        }
    }
    RegisterTrackedRoadSnow(road, calls,
                            clearContext.treatmentStrengthMillion,
                            clearContext.treatmentMaterialIndex,
                            clearContext.treatmentFlags);

    if (!beforeValid)
    {
        if (beforeBytes) HeapFree(GetProcessHeap(), 0, beforeBytes);
        return;
    }

    RoadSnowState after = {};
    if (!ReadRoadSnowState(road, &after, &reason))
    {
        LogInvalidRoadOnce(reason, vehicle, road, after.length);
        HeapFree(GetProcessHeap(), 0, beforeBytes);
        return;
    }

    if (after.begin != before.begin || after.length != before.length)
    {
        LogInvalidRoadOnce("snow-vector storage changed during the clearing call",
                           vehicle, road, after.length);
        HeapFree(GetProcessHeap(), 0, beforeBytes);
        return;
    }

    SIZE_T changed = 0;
    SIZE_T zeroed = 0;
    SIZE_T decreased = 0;
    SIZE_T increased = 0;
    __try
    {
        for (SIZE_T i = 0; i < after.length; ++i)
        {
            BYTE oldValue = beforeBytes[i];
            BYTE newValue = after.begin[i];
            if (oldValue == newValue) continue;
            changed++;
            if (newValue == 0 && oldValue != 0) zeroed++;
            if (newValue < oldValue) decreased++;
            else increased++;
        }
    }
    __except (FaultFilter(PLUGIN_NAME " road snow-byte comparison",
                          GetExceptionInformation()))
    {
        LogInvalidRoadOnce("fault comparing snow-vector bytes",
                           vehicle, road, after.length);
        HeapFree(GetProcessHeap(), 0, beforeBytes);
        return;
    }

    InterlockedIncrement64(&g_snowplowCompleteSamples);
    long long delta = (long long)after.sum - (long long)before.sum;
    Event("road-snow clearing result: call=%lld vehicle=%p road=%p "
          "selector=%p bytes=%u before_sum=%llu after_sum=%llu delta=%+lld "
          "changed=%u decreased=%u zeroed=%u increased=%u "
          "before[min=%u max=%u avg=%.3f hash=%08X] "
          "after[min=%u max=%u avg=%.3f hash=%08X] "
          "grit_matched=%d grit_resource=%s grit_strength=%.3f "
          "grit_flags=0x%X grit_sequence=%u",
          (long long)calls, vehicle, road, selector, (unsigned)after.length,
          (unsigned long long)before.sum, (unsigned long long)after.sum, delta,
          (unsigned)changed, (unsigned)decreased, (unsigned)zeroed,
          (unsigned)increased,
          before.minimum, before.maximum,
          (double)before.sum / (double)before.length, before.hash,
          after.minimum, after.maximum,
          (double)after.sum / (double)after.length, after.hash,
          clearContext.treatmentMatched,
          clearContext.treatmentResourceName[0]
              ? clearContext.treatmentResourceName : "legacy",
          (double)clearContext.treatmentStrengthMillion / 1000000.0,
          clearContext.treatmentFlags,
          clearContext.treatmentSequence);

    HeapFree(GetProcessHeap(), 0, beforeBytes);
}

// ------------------------------------------------------- instrumentation stub

static void Emit8(BYTE*& p, BYTE value) { *p++ = value; }

static void EmitBytes(BYTE*& p, const BYTE* data, SIZE_T length)
{
    memcpy(p, data, length);
    p += length;
}

static void Emit64(BYTE*& p, uint64_t value)
{
    memcpy(p, &value, sizeof(value));
    p += sizeof(value);
}

static void Emit32(BYTE*& p, uint32_t value)
{
    memcpy(p, &value, sizeof(value));
    p += sizeof(value);
}

static BYTE* BuildRoadSnowWriteStub(DWORD rva, bool indexInRsi,
                                    unsigned site)
{
    BYTE* target = g_exeBase + rva;
    BYTE* stub = AllocNear(target, 256);
    if (!stub) return NULL;

    BYTE* p = stub;

    // Preserve flags, every volatile integer register and every volatile XMM
    // register.  The original function keeps the road in nonvolatile rbx and
    // the current index in nonvolatile rdi/rsi, which the Win64 helper also
    // preserves. Eight pushes keep the already aligned stack aligned.
    const BYTE pushes[] = {
        0x9C,                         // pushfq
        0x50, 0x51, 0x52,             // push rax, rcx, rdx
        0x41, 0x50,                   // push r8
        0x41, 0x51,                   // push r9
        0x41, 0x52,                   // push r10
        0x41, 0x53                    // push r11
    };
    EmitBytes(p, pushes, sizeof(pushes));

    const BYTE subRsp[] = { 0x48, 0x81, 0xEC };
    EmitBytes(p, subRsp, sizeof(subRsp));
    Emit32(p, 0x80);                  // shadow space + xmm0..xmm5

    for (BYTE xmm = 0; xmm < 6; ++xmm)
    {
        Emit8(p, 0xF3); Emit8(p, 0x0F); Emit8(p, 0x7F); // movdqu [rsp+n],xmm
        Emit8(p, (BYTE)(0x44 | (xmm << 3)));
        Emit8(p, 0x24);
        Emit8(p, (BYTE)(0x20 + xmm * 0x10));
    }

    const BYTE movRcxRbx[] = { 0x48, 0x8B, 0xCB };
    const BYTE movRdxRdi[] = { 0x48, 0x8B, 0xD7 };
    const BYTE movRdxRsi[] = { 0x48, 0x8B, 0xD6 };
    EmitBytes(p, movRcxRbx, sizeof(movRcxRbx));
    EmitBytes(p, indexInRsi ? movRdxRsi : movRdxRdi,
              sizeof(movRdxRdi));
    Emit8(p, 0x41); Emit8(p, 0xB8);   // mov r8d, site
    Emit32(p, site);
    Emit8(p, 0x48); Emit8(p, 0xB8);   // mov rax, observation helper
    Emit64(p, (uint64_t)(uintptr_t)&ObserveRoadSnowWrite);
    Emit8(p, 0xFF); Emit8(p, 0xD0);   // call rax

    for (int xmm = 5; xmm >= 0; --xmm)
    {
        Emit8(p, 0xF3); Emit8(p, 0x0F); Emit8(p, 0x6F); // movdqu xmm,[rsp+n]
        Emit8(p, (BYTE)(0x44 | (xmm << 3)));
        Emit8(p, 0x24);
        Emit8(p, (BYTE)(0x20 + xmm * 0x10));
    }

    const BYTE addRsp[] = { 0x48, 0x81, 0xC4 };
    EmitBytes(p, addRsp, sizeof(addRsp));
    Emit32(p, 0x80);

    const BYTE pops[] = {
        0x41, 0x5B,                   // pop r11
        0x41, 0x5A,                   // pop r10
        0x41, 0x59,                   // pop r9
        0x41, 0x58,                   // pop r8
        0x5A, 0x59, 0x58,             // pop rdx, rcx, rax
        0x9D                          // popfq
    };
    EmitBytes(p, pops, sizeof(pops));

    // Replay the exact original 14 bytes, including the zero store and the
    // following TEST that establishes flags for the untouched conditional
    // branch at the resume address.
    const BYTE* original = indexInRsi ? kRoadSnowWriteRsi
                                      : kRoadSnowWriteRdi;
    EmitBytes(p, original, 14);

    // Register- and flag-neutral absolute jump to the first untouched byte.
    Emit8(p, 0xFF); Emit8(p, 0x25);
    Emit32(p, 0);
    Emit64(p, (uint64_t)(uintptr_t)(target + 14));

    FlushInstructionCache(GetCurrentProcess(), stub, (SIZE_T)(p - stub));
    return stub;
}

static BYTE* BuildWeatherCallStub()
{
    BYTE* stub = AllocNear(g_exeBase + RVA_WEATHER_CALL_SITE, 128);
    if (!stub) return NULL;

    BYTE* p = stub;

    // The original caller already has valid Win64 shadow space and stack
    // alignment for its weather call. CaptureWeatherWorld preserves all
    // nonvolatile registers, including the live world pointer in r14.
    const BYTE movRcxR14[] = { 0x49, 0x8B, 0xCE };
    EmitBytes(p, movRcxR14, sizeof(movRcxR14));
    Emit8(p, 0x48); Emit8(p, 0xB8);       // mov rax, capture helper
    Emit64(p, (uint64_t)(uintptr_t)&CaptureWeatherWorld);
    Emit8(p, 0xFF); Emit8(p, 0xD0);       // call rax

    // Recreate the displaced regular weather call. Calling its public entry
    // is intentional: when daynight.dll has detoured that entry, its handler
    // remains in the chain and eventually invokes the original trampoline.
    Emit8(p, 0x33); Emit8(p, 0xD2);       // xor edx,edx
    EmitBytes(p, movRcxR14, sizeof(movRcxR14));
    Emit8(p, 0x48); Emit8(p, 0xB8);       // mov rax, weather tick entry
    Emit64(p, (uint64_t)(uintptr_t)(g_exeBase + RVA_WEATHER_TICK));
    Emit8(p, 0xFF); Emit8(p, 0xD0);       // call rax

    // Finish diagnostics after weather has queued/suppressed all of its +30
    // snow calls. A due gradual release can now set one redraw request without
    // a later weather caller clearing it again in the same tick.
    EmitBytes(p, movRcxR14, sizeof(movRcxR14));
    Emit8(p, 0x48); Emit8(p, 0xB8);       // mov rax, post-weather helper
    Emit64(p, (uint64_t)(uintptr_t)&CompleteWeatherWorldUpdate);
    Emit8(p, 0xFF); Emit8(p, 0xD0);       // call rax

    // Recreate the argument setup for the following light-factor call and
    // resume exactly at that untouched call instruction.
    Emit8(p, 0x33); Emit8(p, 0xD2);       // xor edx,edx
    EmitBytes(p, movRcxR14, sizeof(movRcxR14));
    Emit8(p, 0x48); Emit8(p, 0xB8);       // mov rax, resume address
    Emit64(p, (uint64_t)(uintptr_t)(g_exeBase + RVA_WEATHER_CALL_RESUME));
    Emit8(p, 0xFF); Emit8(p, 0xE0);       // jmp rax

    FlushInstructionCache(GetCurrentProcess(), stub, (SIZE_T)(p - stub));
    return stub;
}

static bool InstallWeatherCaptureProbe()
{
    BYTE* target = g_exeBase + RVA_WEATHER_CALL_SITE;
    if (memcmp(target, kWeatherCallSite, sizeof(kWeatherCallSite)) != 0)
    {
        Warn("weather probe refused: bytes at RVA 0x%X do not match the "
             "verified v1.1.1.9 caller", RVA_WEATHER_CALL_SITE);
        return false;
    }

    BYTE* stub = BuildWeatherCallStub();
    if (!stub)
    {
        Warn("weather probe refused: executable call-site stub could not be "
             "allocated");
        return false;
    }

    // The loader creates a trampoline for every inline hook. This call-site
    // contains a relative CALL, so that generic trampoline is intentionally
    // never executed; the generated stub above recreates the call with an
    // absolute destination and resumes at the first untouched instruction.
    if (!InstallInlineHook(target, stub, &g_weatherCallTrampoline,
                           kWeatherCallSite, sizeof(kWeatherCallSite),
                           PLUGIN_NAME " weather world capture"))
    {
        Warn("weather probe refused by the loader; another plugin may already "
             "use the regular weather caller");
        return false;
    }

    Info("weather world capture installed at caller RVA 0x%X; daynight's "
         "weather-function hook remains in the call chain",
         RVA_WEATHER_CALL_SITE);
    return true;
}

// ------------------------------------------------------- visible snow mask

// This is the engine primitive that actually paints the visible cleared trail.
// The game's road-snow path reaches it through the queue flushed at 0x40EE3B
// with channel=2, radii 0.50/0.75, delta=255, limit=-1 and on=false.  The hook
// batches those final parameters, tracks a few spatially separated points and
// forwards every call byte-for-byte unchanged.
static void h_TextureSetTexel(void* texture, int x, int y, DWORD value)
{
    bool observe = InterlockedCompareExchange(&g_active, 0, 0) &&
        texture == InterlockedCompareExchangePointer(
            &g_terrainMaskTexture, NULL, NULL);
    DWORD callerRva = observe ? EngineCallerRva(_ReturnAddress())
                              : 0xFFFFFFFFu;
    DWORD oldRaw = 0;
    bool oldValueValid = false;

    if (observe)
    {
        __try
        {
            BYTE* object = (BYTE*)texture;
            int width = *(int*)(object + 0x14);
            int height = *(int*)(object + 0x18);
            BYTE* data = *(BYTE**)(object + 0x158);
            unsigned rowPitch = *(unsigned*)(object + 0x160);
            if (data && x >= 0 && y >= 0 && x < width && y < height &&
                rowPitch >= (unsigned)width * sizeof(DWORD))
            {
                oldRaw = *(DWORD*)(data + (SIZE_T)y * rowPitch +
                                    (SIZE_T)x * sizeof(DWORD));
                oldValueValid = true;
            }
        }
        __except (FaultFilter(PLUGIN_NAME " mask SetTexel before-value",
                              GetExceptionInformation()))
        {
            oldValueValid = false;
        }
    }

    // Always preserve the original write exactly once. The SetTexel routine
    // swaps red/blue byte order, but the green byte remains bits 8..15 in both
    // the caller value and the mapped texture word.
    o_TextureSetTexel(texture, x, y, value);
    unsigned newGreen = (value >> 8) & 0xFFu;

    if (observe)
    {
        unsigned oldGreen = (oldRaw >> 8) & 0xFFu;
        RecordDeepMaskWrite(callerRva, oldValueValid, oldGreen, newGreen);
        UpdateExperimentalLimiterShadowTexel(texture, x, y, newGreen);
        UpdatePlowVisualShadowTexel(
            texture, x, y, newGreen,
            g_verifiedPlowEditMaskDepth > 0);
    }
}

static void h_TextureAccessOpen(void* texture)
{
    void* caller = _ReturnAddress();
    bool observe = InterlockedCompareExchange(&g_active, 0, 0) &&
        texture == InterlockedCompareExchangePointer(
            &g_terrainMaskTexture, NULL, NULL);
    TextureStorageState before = {};
    TextureStorageState after = {};
    TextureAccessRawState rawBefore = {};
    TextureAccessRawState rawAfter = {};
    if (observe)
    {
        ReadTextureStorageState(texture, &before);
        ReadTrackedTextureAccessRawState(before, &rawBefore);
    }

    o_TextureAccessOpen(texture);

    if (observe)
    {
        ReadTextureStorageState(texture, &after);
        ReadTrackedTextureAccessRawState(after, &rawAfter);
        RecordTextureAccessCall('O', texture, caller, before, after,
                                rawBefore, rawAfter, -1);
    }
}

static bool h_TextureAccessOpen2(void* texture)
{
    void* caller = _ReturnAddress();
    bool observe = InterlockedCompareExchange(&g_active, 0, 0) &&
        texture == InterlockedCompareExchangePointer(
            &g_terrainMaskTexture, NULL, NULL);
    TextureStorageState before = {};
    TextureStorageState after = {};
    TextureAccessRawState rawBefore = {};
    TextureAccessRawState rawAfter = {};
    if (observe)
    {
        ReadTextureStorageState(texture, &before);
        ReadTrackedTextureAccessRawState(before, &rawBefore);
    }

    bool result = o_TextureAccessOpen2(texture);

    if (observe)
    {
        ReadTextureStorageState(texture, &after);
        ReadTrackedTextureAccessRawState(after, &rawAfter);
        // rawAfter deliberately records the engine-provided values before the
        // optional experiment limits their decrease.
        if (result)
        {
            ApplyExperimentalRecoverageLimiter(after);
            ApplyPlowVisualAfterEffect(after);
        }
        RecordTextureAccessCall('2', texture, caller, before, after,
                                rawBefore, rawAfter, result ? 1 : 0);
    }
    return result;
}

static void h_TextureAccessClose(void* texture)
{
    void* caller = _ReturnAddress();
    bool observe = InterlockedCompareExchange(&g_active, 0, 0) &&
        texture == InterlockedCompareExchangePointer(
            &g_terrainMaskTexture, NULL, NULL);
    TextureStorageState before = {};
    TextureStorageState after = {};
    TextureAccessRawState rawBefore = {};
    TextureAccessRawState rawAfter = {};
    if (observe)
    {
        ReadTextureStorageState(texture, &before);
        ReadTrackedTextureAccessRawState(before, &rawBefore);

        // Open2 exposes the mapped CPU buffer before the engine has finished
        // writing it. Enforce once more immediately before Close uploads that
        // buffer. This prevents a newly covered road from reaching the GPU for
        // one frame before the periodic pass restores its protected texels.
        if (before.valid)
        {
            if (g_cfg.experimentalRecoverageLimiter)
            {
                ApplyExperimentalRecoverageLimiter(before);
                InterlockedIncrement64(&g_limiterEnforcementPasses);
            }
            if (InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) &&
                (InterlockedCompareExchange64(
                    &g_plowVisualActivePublished, 0, 0) > 0 ||
                 WeatherPersistenceVisualPending()))
            {
                ApplyPlowVisualAfterEffect(
                    before, PLOW_VISUAL_APPLY_PRE_UPLOAD);
                InterlockedIncrement64(&g_plowVisualEnforcementPasses);
            }
            // The close observation now records engine data before correction
            // and the exact data handed to the original upload routine.
            ReadTrackedTextureAccessRawState(before, &rawAfter);
        }
    }

    o_TextureAccessClose(texture);

    if (observe)
    {
        ReadTextureStorageState(texture, &after);
        RecordTextureAccessCall('C', texture, caller, before, after,
                                rawBefore, rawAfter, -1);
    }
}

static void h_MaskTextureOpen(void* terrain)
{
    DWORD callerRva = ExecutableCallerRva(_ReturnAddress());
    o_MaskTextureOpen(terrain);
    if (InterlockedCompareExchange(&g_active, 0, 0))
        RecordMaskTextureCall('O', terrain, callerRva);
}

static void h_MaskTextureClose(void* terrain)
{
    DWORD callerRva = ExecutableCallerRva(_ReturnAddress());
    o_MaskTextureClose(terrain);
    if (InterlockedCompareExchange(&g_active, 0, 0))
        RecordMaskTextureCall('C', terrain, callerRva);
}

static void h_EditMask(void* terrain, float* pos, int channel,
                       float innerR, float outerR, int delta,
                       int limit, char on)
{
    DWORD callerRva = ExecutableCallerRva(_ReturnAddress());

    bool plowSignature = channel == 2 &&
                         AbsMaskFloat(innerR - 0.50f) < 0.0001f &&
                         AbsMaskFloat(outerR - 0.75f) < 0.0001f &&
                         delta == 255 && limit == -1 && on == 0;
    bool directFlush = callerRva == RVA_EDIT_MASK_FLUSH_RETURN;

    float x = 0.0f, y = 0.0f, z = 0.0f;
    bool positionValid = false;
    if (channel == 2 && pos && ReadablePtr(pos, sizeof(float) * 3))
    {
        __try
        {
            x = pos[0];
            y = pos[1];
            z = pos[2];
            positionValid = _finite(x) && _finite(y) && _finite(z);
        }
        __except (FaultFilter(PLUGIN_NAME " EditMask position observation",
                              GetExceptionInformation()))
        {
            positionValid = false;
        }
    }

    // The original call is always made exactly once and with the untouched
    // arguments, even if the diagnostic position read above was rejected.
    // A thread-local scope lets the verified deep SetTexel hook identify only
    // texels painted by this exact snowplow brush.
    bool markVisiblePlowTexels = plowSignature && directFlush &&
        InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0);
    LONG previousStrengthMillion =
        g_verifiedPlowEditMaskStrengthMillion;
    int previousTreatmentDeferred =
        g_verifiedPlowEditMaskTreatmentDeferred;
    if (markVisiblePlowTexels)
    {
        if (g_gritSpreaderApi)
            BeginVisibleBrushCapture(positionValid, x, y, z);
        // The material-aware path defers protection changes until the matching
        // vehicle clear is known. The brush itself still removes visible snow.
        // Without the service, the established legacy strength remains 1.00.
        g_verifiedPlowEditMaskTreatmentDeferred =
            g_gritSpreaderApi ? 1 : 0;
        g_verifiedPlowEditMaskStrengthMillion = GRIT_FACTOR_MILLION;
        g_verifiedPlowEditMaskDepth++;
    }
    o_EditMask(terrain, pos, channel, innerR, outerR, delta, limit, on);
    if (markVisiblePlowTexels)
    {
        g_verifiedPlowEditMaskDepth--;
        g_verifiedPlowEditMaskStrengthMillion = previousStrengthMillion;
        g_verifiedPlowEditMaskTreatmentDeferred =
            previousTreatmentDeferred;
        if (g_gritSpreaderApi)
            QueueCapturedVisibleBrush();
    }

    if (!InterlockedCompareExchange(&g_active, 0, 0))
        return;

    if (plowSignature)
    {
        InterlockedIncrement64(&g_plowMaskCalls);
        if (directFlush)
        {
            ULONGLONG now = GetTickCount64();
            InterlockedExchange64(&g_lastVerifiedPlowMaskTick, (LONG64)now);
            ConfirmPendingPlowPoints(now);
        }
    }
    float observedPos[3] = { x, y, z };
    RecordEditMaskObservation(plowSignature, directFlush, positionValid,
                              observedPos, channel, innerR, outerR,
                              delta, limit, on, callerRva);
}

static bool InstallVisibleSnowMaskProbe()
{
    if (!PatchIat(g_exe, DLL_ENGINE, SYM_EDIT_MASK,
                  (void*)h_EditMask, (void**)&o_EditMask,
                  PLUGIN_NAME " visible snow mask"))
    {
        Warn("visible snow-mask probe refused: the C3D_TERRAIN::EditMask "
             "import slot was unavailable; road-call observation can still run");
        return false;
    }

    bool textureOpenActive = PatchIat(
        g_exe, DLL_ENGINE, SYM_MASK_TEXTURE_OPEN,
        (void*)h_MaskTextureOpen, (void**)&o_MaskTextureOpen,
        PLUGIN_NAME " mask texture open");
    bool textureCloseActive = PatchIat(
        g_exe, DLL_ENGINE, SYM_MASK_TEXTURE_CLOSE,
        (void*)h_MaskTextureClose, (void**)&o_MaskTextureClose,
        PLUGIN_NAME " mask texture close");
    if (textureOpenActive && textureCloseActive)
        Info("MaskTextureOpen/Close correlation observer installed; hooks only "
             "record caller RVAs and forward every call unchanged");
    else
        Warn("MaskTextureOpen/Close correlation is incomplete: open=%s, "
             "close=%s; terrain-mask values remain observable",
             textureOpenActive ? "on" : "off",
             textureCloseActive ? "on" : "off");

    Info("visible snow-mask observer installed on C3D_TERRAIN::EditMask; "
         "verified snowplow signature is channel=2 inner=0.50 outer=0.75 "
         "delta=255 limit=-1 on=0; batch=%d ms, samples=%d ms, "
         "tracked_points=%d, pixel_reader=already-mapped-read-only",
         g_cfg.maskBatchIntervalMs, g_cfg.maskSampleIntervalMs,
         g_cfg.maximumTrackedMaskPoints);
    Info("terrain-mask diagnostics never call the engine getter or map a texture; "
         "world changes discard previous sample/road caches");
    return true;
}

static bool InstallDeepMaskWriteProbe()
{
    if (!g_engine || !g_engineSize)
    {
        Warn("deep terrain-mask write observer refused: verified engine image "
             "is unavailable");
        return false;
    }

    BYTE* target = (BYTE*)g_engine + RVA_ENGINE_TEXTURE_SET_TEXEL;
    if (memcmp(target, kEngineTextureSetTexel,
               sizeof(kEngineTextureSetTexel)) != 0)
    {
        Warn("deep terrain-mask write observer refused: bytes at engine "
             "RVA 0x%X do not match WRSR 1.1.1.9",
             RVA_ENGINE_TEXTURE_SET_TEXEL);
        return false;
    }

    if (!InstallInlineHook(target, (void*)h_TextureSetTexel,
                           (void**)&o_TextureSetTexel,
                           kEngineTextureSetTexel,
                           sizeof(kEngineTextureSetTexel),
                           PLUGIN_NAME " terrain mask SetTexel"))
    {
        Warn("deep terrain-mask write observer was refused by the loader; "
             "another plugin may already use the engine SetTexel function");
        return false;
    }

    Info("deep terrain-mask writer installed at C3DDLL64.dll RVA 0x%X; "
         "only the current terrain-mask texture is counted, known EditMask "
         "caller RVA=0x%X; every pixel write remains unchanged",
         RVA_ENGINE_TEXTURE_SET_TEXEL,
         RVA_ENGINE_EDIT_MASK_SET_TEXEL_RETURN);
    return true;
}

static bool InstallTextureAccessProbe()
{
    if (!g_engine || !g_engineSize)
    {
        Warn("texture-access observer refused: verified engine image is "
             "unavailable");
        return false;
    }

    void** vtable = (void**)((BYTE*)g_engine + RVA_ENGINE_TEXTURE_VTABLE);
    SIZE_T tableBytes = 64 * sizeof(void*);
    if (!ReadablePtr(vtable, tableBytes))
    {
        Warn("texture-access observer refused: verified texture vtable at "
             "engine RVA 0x%X is unreadable", RVA_ENGINE_TEXTURE_VTABLE);
        return false;
    }

    void* expectedOpen = (BYTE*)g_engine + RVA_ENGINE_TEXTURE_ACCESS_OPEN;
    void* expectedOpen2 = (BYTE*)g_engine + RVA_ENGINE_TEXTURE_ACCESS_OPEN2;
    void* expectedClose = (BYTE*)g_engine + RVA_ENGINE_TEXTURE_ACCESS_CLOSE;
    if (vtable[TEXTURE_VTABLE_SLOT_ACCESS_OPEN] != expectedOpen ||
        vtable[TEXTURE_VTABLE_SLOT_ACCESS_OPEN2] != expectedOpen2 ||
        vtable[TEXTURE_VTABLE_SLOT_ACCESS_CLOSE] != expectedClose)
    {
        Warn("texture-access observer refused: vtable slots are already "
             "changed or do not match WRSR 1.1.1.9; open=%p expected=%p, "
             "open2=%p expected=%p, close=%p expected=%p",
             vtable[TEXTURE_VTABLE_SLOT_ACCESS_OPEN], expectedOpen,
             vtable[TEXTURE_VTABLE_SLOT_ACCESS_OPEN2], expectedOpen2,
             vtable[TEXTURE_VTABLE_SLOT_ACCESS_CLOSE], expectedClose);
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(vtable, tableBytes, PAGE_READWRITE, &oldProtect))
    {
        Warn("texture-access observer refused: vtable protection could not "
             "be changed (Windows error %lu)", GetLastError());
        return false;
    }

    o_TextureAccessOpen = (t_TextureAccess)expectedOpen;
    o_TextureAccessOpen2 = (t_TextureAccessOpen2)expectedOpen2;
    o_TextureAccessClose = (t_TextureAccess)expectedClose;
    InterlockedExchangePointer(
        (void* volatile*)&vtable[TEXTURE_VTABLE_SLOT_ACCESS_OPEN],
        (void*)h_TextureAccessOpen);
    InterlockedExchangePointer(
        (void* volatile*)&vtable[TEXTURE_VTABLE_SLOT_ACCESS_OPEN2],
        (void*)h_TextureAccessOpen2);
    InterlockedExchangePointer(
        (void* volatile*)&vtable[TEXTURE_VTABLE_SLOT_ACCESS_CLOSE],
        (void*)h_TextureAccessClose);

    DWORD ignored = 0;
    if (!VirtualProtect(vtable, tableBytes, oldProtect, &ignored))
        Warn("texture-access vtable hooks are active, but restoring the "
             "original page protection failed (Windows error %lu)",
             GetLastError());
    FlushInstructionCache(GetCurrentProcess(), vtable, tableBytes);

    Info("texture-access observer installed on verified D3D11 texture vtable "
         "slots 0x80/0x88/0x90; only the current terrain-mask texture is "
         "recorded; tracked raw values bracket Open and Open2, while Close "
         "records the engine buffer and the protected buffer handed to the "
         "original upload routine; every call argument and return value "
         "remains unchanged");
    return true;
}

static bool InstallRoadSnowClearingProbe()
{
    BYTE* target = g_exeBase + RVA_ROAD_SNOW_CLEAR_BODY;
    if (memcmp(target, kRoadSnowClearBody, sizeof(kRoadSnowClearBody)) != 0)
    {
        Warn("road-snow clearing probe refused: bytes at RVA 0x%X do not "
             "match the verified v1.1.1.9 function body; weather sampling "
             "can still run", RVA_ROAD_SNOW_CLEAR_BODY);
        return false;
    }

    struct WriteSite
    {
        DWORD rva;
        const BYTE* expected;
        bool indexInRsi;
        const char* label;
    };
    const WriteSite sites[4] = {
        { RVA_ROAD_SNOW_WRITE_1, kRoadSnowWriteRdi, false,
          PLUGIN_NAME " snow zero write 1" },
        { RVA_ROAD_SNOW_WRITE_2, kRoadSnowWriteRdi, false,
          PLUGIN_NAME " snow zero write 2" },
        { RVA_ROAD_SNOW_WRITE_3, kRoadSnowWriteRsi, true,
          PLUGIN_NAME " snow zero write 3" },
        { RVA_ROAD_SNOW_WRITE_4, kRoadSnowWriteRdi, false,
          PLUGIN_NAME " snow zero write 4" }
    };

    for (unsigned i = 0; i < 4; ++i)
    {
        if (memcmp(g_exeBase + sites[i].rva, sites[i].expected, 14) != 0)
        {
            Warn("exact road-snow write observation refused: bytes at "
                 "RVA 0x%X do not match WRSR 1.1.1.9; no road hook was "
                 "installed", sites[i].rva);
            return false;
        }
    }

    if (g_clearTls == TLS_OUT_OF_INDEXES)
    {
        g_clearTls = TlsAlloc();
        if (g_clearTls == TLS_OUT_OF_INDEXES)
        {
            Warn("road-snow clearing probe refused: a thread-local context "
                 "slot could not be allocated (Windows error %lu)",
                 GetLastError());
            return false;
        }
    }

    BYTE* stubs[4] = {};
    for (unsigned i = 0; i < 4; ++i)
    {
        stubs[i] = BuildRoadSnowWriteStub(sites[i].rva,
                                           sites[i].indexInRsi, i + 1);
        if (!stubs[i])
        {
            Warn("exact road-snow write observation could not allocate stub "
                 "%u; no road hook was installed", i + 1);
            return false;
        }
    }

    if (!InstallInlineHook(target, (void*)h_RoadSnowClearBody,
                           (void**)&o_RoadSnowClearBody,
                           kRoadSnowClearBody, sizeof(kRoadSnowClearBody),
                           PLUGIN_NAME " road snow clearing"))
    {
        Warn("road-snow clearing probe refused by the loader; another plugin "
             "may already use this exact function body. Weather sampling can "
             "still run");
        return false;
    }

    unsigned installedWrites = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        if (!InstallInlineHook(g_exeBase + sites[i].rva, stubs[i],
                               &g_roadWriteTrampolines[i],
                               sites[i].expected, 14, sites[i].label))
        {
            Warn("exact road-snow write site %u could not be installed; "
                 "the body observer remains active, but exact write results "
                 "are incomplete", i + 1);
            break;
        }
        installedWrites++;
    }
    InterlockedExchange(&g_exactRoadWriteSitesActive, (LONG)installedWrites);

    Info("road-snow clearing observer installed at body RVA 0x%X; exact "
         "zero-write sites active=%u/4. Generated stubs only observe the old "
         "byte and then replay the original game instructions; v0.1.18 keeps "
         "these sites for diagnostics only",
         RVA_ROAD_SNOW_CLEAR_BODY, installedWrites);
    Info("road-snow clearing counter armed; if no 'road-snow clearing status' "
         "line appears, the game did not call the clearing function during "
         "the test");
    return true;
}


static bool InstallInternalRoadSnowLimiter()
{
    BYTE* target = g_exeBase + RVA_ROAD_SNOW_ADJUST;
    if (memcmp(target, kRoadSnowAdjust, sizeof(kRoadSnowAdjust)) != 0)
    {
        Warn("internal road-snow adjustment experiment refused: bytes at "
             "RVA 0x%X do not match the verified WRSR 1.1.1.9 function; "
             "no game value was changed", RVA_ROAD_SNOW_ADJUST);
        return false;
    }

    if (!InstallInlineHook(target, (void*)h_RoadSnowAdjust,
                           (void**)&o_RoadSnowAdjust,
                           kRoadSnowAdjust, sizeof(kRoadSnowAdjust),
                           PLUGIN_NAME " internal road snow adjustment"))
    {
        Warn("internal road-snow adjustment experiment refused by the "
             "loader; another plugin may already own RVA 0x%X",
             RVA_ROAD_SNOW_ADJUST);
        return false;
    }

    double activeGlobalMultiplier =
        g_cfg.internalSnowLimiter ? g_cfg.snowAccumulationMultiplier : 1.0;
    double activeReductionMultiplier =
        g_cfg.naturalMelting ? g_cfg.snowReductionMultiplier : 1.0;
    Info("EXPERIMENT internal road-snow adjustment hook installed at verified "
         "RVA 0x%X: positive global deltas use multiplier %.6f; verified "
         "weather snow is released before mask generation=%s(step=%d," 
         "interval=%dms,visual-batch=%d,max-pending=%d); verified natural "
         "-30 reductions "
         "use multiplier %.6f; plow after-effect=%s; "
         "full -255 resets and snowplow clearing remain unchanged",
         RVA_ROAD_SNOW_ADJUST, activeGlobalMultiplier,
         g_cfg.gradualSnowAccumulation ? "on" : "off",
         g_cfg.gradualSnowStepUnits, g_cfg.gradualSnowStepIntervalMs,
         g_cfg.gradualVisualBatchUnits, MAX_GRADUAL_SNOW_PENDING_UNITS,
         activeReductionMultiplier,
         g_cfg.plowEffect ? "on" : "off");
    return true;
}

static void h_GlobalMaskUpdate(void* world)
{
    bool observe = InterlockedCompareExchange(&g_active, 0, 0) && world;
    unsigned requestWord = 0;
    bool snowPass = false;
    if (observe && ReadablePtr(
            (BYTE*)world + OFF_MASK_DIRTY_REQUEST, sizeof(SHORT)))
    {
        __try
        {
            requestWord = *(volatile unsigned short*)(
                (BYTE*)world + OFF_MASK_DIRTY_REQUEST);
            snowPass = (requestWord & 0xFF00u) != 0;
        }
        __except (FaultFilter(PLUGIN_NAME " global mask request flags",
                              GetExceptionInformation()))
        {
            requestWord = 0;
            snowPass = false;
        }
    }

    o_GlobalMaskUpdate(world);

    if (!observe) return;
    InterlockedIncrement64(&g_globalMaskUpdateCalls);
    InterlockedExchange(&g_globalMaskLastRequestWord, (LONG)requestWord);
    if (!snowPass) return;
    InterlockedIncrement64(&g_globalSnowMaskUpdateCalls);

    // The renderer has now submitted the global GPU snowfall update, while its
    // caller has not yet cleared +0x5E5 or presented the frame. Map and correct
    // only the bounded set of verified plow texels at this exact boundary.
    if (!InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) ||
        InterlockedCompareExchange64(
            &g_plowVisualActivePublished, 0, 0) <= 0 ||
        InterlockedCompareExchange(&g_globalMaskUpdateBusy, 1, 0) != 0)
        return;

    void* texture = InterlockedCompareExchangePointer(
        &g_terrainMaskTexture, NULL, NULL);
    bool corrected = texture &&
        ReadablePtr(texture, MIN_TEXTURE_READ_SIZE) &&
        RunExperimentalLimiterEnforcementPass(
            texture, PLOW_VISUAL_APPLY_INLINE_SYNC);
    if (corrected)
    {
        InterlockedIncrement64(&g_globalMaskInlinePasses);
        InterlockedIncrement64(&g_plowVisualInlinePasses);
        InterlockedExchange(&g_plowVisualEventPassesRemaining, 0);
        InterlockedExchange64(&g_plowVisualEventNextTick, 0);
        InterlockedExchange64(&g_plowVisualEventCooldownUntilTick,
                              (LONG64)(GetTickCount64() +
                                       PLOW_VISUAL_EVENT_COOLDOWN_MS));
        InterlockedExchange(&g_plowVisualEventHasSnow, 0);
    }
    else
    {
        InterlockedIncrement64(&g_globalMaskInlineFailures);
    }
    InterlockedExchange(&g_globalMaskUpdateBusy, 0);
}

static bool InstallGlobalMaskUpdateProbe()
{
    BYTE* target = g_exeBase + RVA_GLOBAL_MASK_UPDATE;
    if (memcmp(target, kGlobalMaskUpdate, sizeof(kGlobalMaskUpdate)) != 0)
    {
        Warn("global snow-mask boundary refused: bytes at RVA 0x%X do not "
             "match the verified WRSR 1.1.1.9 function; gradual internal "
             "snow remains active without inline visual correction",
             RVA_GLOBAL_MASK_UPDATE);
        return false;
    }

    if (!InstallInlineHook(target, (void*)h_GlobalMaskUpdate,
                           (void**)&o_GlobalMaskUpdate,
                           kGlobalMaskUpdate, sizeof(kGlobalMaskUpdate),
                           PLUGIN_NAME " global snow mask boundary"))
    {
        Warn("global snow-mask boundary refused by the loader; another plugin "
             "may already own RVA 0x%X", RVA_GLOBAL_MASK_UPDATE);
        return false;
    }

    Info("global snow-mask boundary installed at verified RVA 0x%X: queued "
         "weather redraw flags are replayed per visual batch; protected plow "
         "texels are corrected inline after the GPU command and before frame "
         "presentation", RVA_GLOBAL_MASK_UPDATE);
    return true;
}

// ------------------------------------------------------------------ exports

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion(void)
{
    return TSM_API_VERSION;
}

#include "weather_roads_persistence.h"

// Entry failures must not touch the logging lock before TsmBind.
static int EntryFailure(const TsmHost* host, const char* reason)
{
    if (host && host->structSize >= offsetof(TsmHost,log) + sizeof(host->log) &&
        host->log)
        host->log(PLUGIN_NAME "  ERROR: entry rejected: %s",reason);
    else
    {
        OutputDebugStringA(PLUGIN_NAME "  ERROR: entry rejected: ");
        OutputDebugStringA(reason);
        OutputDebugStringA("\n");
    }
    return 1;
}

static const char* MissingHostRequirement(const TsmHost* host)
{
    if (!host) return "host is null";
    if (host->structSize < offsetof(TsmHost,configString) + sizeof(host->configString))
        return "host table is too short for required configuration callbacks";
    if (!host->baseDir || !host->baseDir[0]) return "host base directory is missing";
    if (!host->log) return "host log callback is missing";
    if (!host->configString) return "host configString callback is missing";
    if (!host->readablePtr) return "host readablePtr callback is missing";
    if (!host->faultFilter) return "host faultFilter callback is missing";
    if (!host->allocNear) return "host allocNear callback is missing";
    if (!host->installInlineHook) return "host installInlineHook callback is missing";
    if (!host->patchIat) return "host patchIat callback is missing";
    return NULL;
}

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host,
                                                    TsmPluginInfo* info)
{
    if (!info) return EntryFailure(host,"plugin information pointer is null");
    const char* missing = MissingHostRequirement(host);
    if (missing) return EntryFailure(host,missing);
    TsmBind(host);
    BeginLogPhase();
    info->name = PLUGIN_NAME;
    info->version = PLUGIN_VERSION;

    if (GetModuleHandleW(L"weather_roads_probe.dll"))
    {
        Warn("weather_roads_probe.dll is already loaded; "
             "weather_roads is disabled to prevent duplicate hooks");
        EndLogPhase("Init","disabled/duplicate-hooks");
        return 1;
    }

    InitializeCriticalSection(&g_maskDataLock);
    InterlockedExchange(&g_maskDataLockReady, 1);
    InitializeCriticalSection(&g_plowEffectLock);
    InterlockedExchange(&g_plowEffectLockReady, 1);
    g_detail = TsmOpenLog(PLUGIN_LOG);
    if (g_detail == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        ReportWindows("logging","detail-open","Detail log could not be created",
                      error,"Main log remains available; check directory access and disk space");
    }

    Info("initialising v%s; road weather, natural melting and the optional "
         "snowplow after-effect use verified runtime hooks; native game/save "
         "files remain unchanged; persistence writes a separate sidecar",
         PLUGIN_VERSION);
    ReadSettings();

    if (!g_cfg.enabled)
    {
        Info("disabled by [general] enabled=0");
        EndLogPhase("Init","disabled/configuration");
        if (g_detail != INVALID_HANDLE_VALUE)
        {
            CloseHandle(g_detail);
            g_detail = INVALID_HANDLE_VALUE;
        }
        return 1;
    }
    if (g_cfg.plowEffect && !g_cfg.snowplowProbe)
    {
        Warn("[snowplow] enabled=1 requires the verified snowplow boundary; "
             "plow after-effect disabled");
        g_cfg.plowEffect = 0;
    }
    if (g_cfg.internalSnowLimiter)
        Info("road-snow accumulation enabled: positive road snow "
             "will use multiplier %.6f and burst maximum %d after %d ms "
             "quiet time; gradual pre-mask release=%s step=%d every %d ms, "
             "visible redraw batch=%d units, release follows weather=%s",
              g_cfg.snowAccumulationMultiplier,
              g_cfg.maximumSnowAccumulationPerBurst,
              g_cfg.snowBurstResetAfterMs,
              g_cfg.gradualSnowAccumulation ? "on" : "off",
              g_cfg.gradualSnowStepUnits,
              g_cfg.gradualSnowStepIntervalMs,
              g_cfg.gradualVisualBatchUnits,
              g_cfg.releaseFollowsWeather ? "on" : "off");
    if (g_cfg.naturalMelting)
        Info("natural road-snow melting enabled: verified reduction -30 "
             "will use multiplier %.6f; visual snow mode=%s levels=%d "
             "shader-range=%.3f curve=%.3f",
             g_cfg.snowReductionMultiplier,
             g_cfg.visualSnowLevels == 0 ? "continuous" : "staged",
              g_cfg.visualSnowLevels, g_cfg.visualShaderRange,
              g_cfg.visualCurve);
    if (g_cfg.experimentalRecoverageLimiter)
        Warn("EXPERIMENT enabled: the base visible-snow layer will limit "
             "decreases in the verified green snow-mask channel to %d units "
             "per real second; it works without a snowplow and leaves mask "
             "increases, including plowing, unchanged; use a test save first",
             g_cfg.maximumGreenDecreasePerSecond);
    if (g_cfg.plowEffect)
        Info("snowplow after-effect enabled: every tracked road point that "
             "reaches zero, "
             "or is already zero on its first verified clearing call, within "
             "%llu ms of the verified visible plow brush receives "
             "%.2f game-minutes in the strong phase followed by %.2f "
             "game-hours at multiplier %.6f; a connected grit-spreader "
             "service scales prevention per material; dry plowing %s prior "
             "protection, and a weaker material never replaces a stronger "
             "active treatment",
              (unsigned long long)PLOW_EFFECT_MASK_WINDOW_MS,
              g_cfg.plowProtectionMinutes, g_cfg.plowSaltHours,
              g_cfg.plowSaltAccumulationMultiplier,
              g_cfg.dryPlowingPreservesTreatment ? "preserves" : "removes");
    if (!g_cfg.weatherProbe && !g_cfg.snowplowProbe &&
        !g_cfg.experimentalRecoverageLimiter &&
        !g_cfg.internalSnowLimiter && !g_cfg.naturalMelting &&
        !g_cfg.plowEffect)
    {
        Warn("weather_probe and snowplow_probe are both disabled; nothing to observe");
        EndLogPhase("Init","disabled/no-features");
        if (g_detail != INVALID_HANDLE_VALUE)
        {
            CloseHandle(g_detail);
            g_detail = INVALID_HANDLE_VALUE;
        }
        return 1;
    }
    InterlockedExchange(&g_initReady,1);
    EndLogPhase("Init","ready");
    return 0;
}

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    if (!InterlockedCompareExchange(&g_initReady,0,0))
        return EntryFailure(H,"Start requires a successful Init");
    BeginLogPhase();
    if (!VerifyExecutable() || !VerifyEngine())
    {
        StartError("Executable/engine verification failed; no hooks installed");
        return 1;
    }

    bool canConsume = H->structSize >= offsetof(TsmHost,consume) + sizeof(H->consume) &&
        H->consume;
    g_gritSpreaderApi = canConsume ? (const TsmGritSpreaderApi*)H->consume(
        TSM_SERVICE_GRIT_SPREADER, TSM_GRIT_SPREADER_VERSION) : NULL;
    if (g_gritSpreaderApi &&
        (g_gritSpreaderApi->structSize < sizeof(TsmGritSpreaderApi) ||
         !g_gritSpreaderApi->currentRoadTreatment))
    {
        Warn("grit-spreader service has an incompatible structure and was "
             "ignored; legacy uniform plow protection remains active");
        g_gritSpreaderApi = NULL;
    }
    if (g_gritSpreaderApi)
        Info("grit-spreader integration active: service=%s version=%u; "
             "each exact road clear uses its material protection strength "
             "with maximum-strength retention; dry plowing %s prior "
             "protection on that segment",
             TSM_SERVICE_GRIT_SPREADER, TSM_GRIT_SPREADER_VERSION,
             g_cfg.dryPlowingPreservesTreatment ? "preserves" : "removes");
    else
        Info("grit-spreader service unavailable: preserving legacy strength "
             "1.000 for every observed snowplow clear");

    InterlockedExchange(&g_active, 1);
    bool weatherHookActive = false;
    bool weatherWorkerActive = false;
    bool roadActive = false;
    bool internalSnowLimiterActive = false;
    bool globalMaskUpdateActive = false;
    bool maskActive = false;
    bool deepMaskActive = false;
    bool textureAccessActive = false;

    // Snow-mask sampling also needs a safe, regular main-thread call site.
    // Therefore the capture hook is installed for either probe, while the
    // separate weather log worker still follows weather_probe alone.
    if (g_cfg.weatherProbe || g_cfg.snowplowProbe ||
        g_cfg.experimentalRecoverageLimiter ||
        g_cfg.internalSnowLimiter || g_cfg.plowEffect)
    {
        weatherHookActive = InstallWeatherCaptureProbe();
        if (weatherHookActive &&
            (g_cfg.weatherProbe ||
             g_cfg.internalSnowLimiter || g_cfg.plowEffect))
        {
            g_stopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
            if (!g_stopEvent)
                Warn("weather worker event could not be created (Windows error %lu)",
                     GetLastError());
            else
            {
                g_worker = CreateThread(NULL, 0, WeatherWorker, NULL, 0, NULL);
                if (!g_worker)
                {
                    Warn("weather worker could not be created (Windows error %lu)",
                         GetLastError());
                    CloseHandle(g_stopEvent);
                    g_stopEvent = NULL;
                }
                else
                {
                    weatherWorkerActive = true;
                    Info("weather observer active; sample interval=%d ms, "
                         "heartbeat=%d s",
                         g_cfg.sampleIntervalMs,
                         g_cfg.weatherHeartbeatSeconds);
                }
            }
        }
    }

    if (g_cfg.snowplowProbe || g_cfg.experimentalRecoverageLimiter)
    {
        if (g_cfg.snowplowProbe && g_cfg.plowEffect && !weatherHookActive)
        {
            Warn("plow after-effect disabled because the verified game-time "
                 "capture hook is unavailable");
            g_cfg.plowEffect = 0;
        }
        maskActive = InstallVisibleSnowMaskProbe();
        textureAccessActive = InstallTextureAccessProbe();
        if (g_cfg.snowplowProbe)
        {
            deepMaskActive = InstallDeepMaskWriteProbe();
            roadActive = InstallRoadSnowClearingProbe();
        }
        if (g_cfg.plowEffect && (!roadActive || !maskActive))
        {
            Warn("plow after-effect disabled because both the tracked "
                 "road-state observer and the verified visible plow-brush "
                 "observer are required; road=%s mask=%s",
                 roadActive ? "on" : "off", maskActive ? "on" : "off");
            g_cfg.plowEffect = 0;
        }
        if (g_cfg.experimentalRecoverageLimiter && !textureAccessActive)
        {
            Warn("base visible-snow limiter disabled because the "
                 "verified texture-access hooks were unavailable");
            g_cfg.experimentalRecoverageLimiter = 0;
        }
        if (maskActive && !weatherHookActive)
            Warn("visible snow-mask call observation is active, but periodic "
                 "batch flushing and pixel sampling are unavailable because "
                 "the verified game-update call site could not be installed");
    }

    if (g_cfg.internalSnowLimiter || g_cfg.naturalMelting || g_cfg.plowEffect)
        internalSnowLimiterActive = InstallInternalRoadSnowLimiter();
    if ((g_cfg.internalSnowLimiter && g_cfg.gradualSnowAccumulation) ||
        g_cfg.plowEffect)
        globalMaskUpdateActive = InstallGlobalMaskUpdateProbe();
    if (g_cfg.naturalMelting && !internalSnowLimiterActive)
    {
        Warn("natural road-snow melting disabled because the verified "
             "internal road-snow adjustment hook is unavailable");
        g_cfg.naturalMelting = 0;
    }
    if (g_cfg.plowEffect && !internalSnowLimiterActive)
    {
        Warn("plow after-effect disabled because the verified internal "
             "road-snow adjustment hook is unavailable");
        g_cfg.plowEffect = 0;
    }

    if (g_cfg.plowEffect && deepMaskActive && textureAccessActive)
    {
        InterlockedExchange(&g_plowVisualEnabled, 1);
        Info("visible plow after-effect enabled: only green-channel "
             "texels written inside the verified snowplow EditMask call, a "
             "%d-texel margin and short links up to %d texels are linked to "
                "the game-time protection and salt phases; when the grit "
                "service is active, each brush joins a bounded spatial "
                "trajectory; v0.2.6 binds that trajectory to one vehicle "
                "across the render/update thread boundary and finalises only "
                "its still-open brush interval with Sand, Gravel or dry; "
               "active-texel event "
               "sync uses one coalesced pass after 16 ms with %llu ms burst "
               "cooldown; each GPU "
               "snow burst is accumulated once; visual snow mode=%s, "
               "shader-range=%.3f, curve=%.3f",
              PLOW_VISUAL_MARGIN_TEXELS,
              PLOW_VISUAL_LINK_MAX_TEXEL_DISTANCE,
              (unsigned long long)PLOW_VISUAL_EVENT_COOLDOWN_MS,
              g_cfg.visualSnowLevels == 0 ? "continuous" : "staged",
              g_cfg.visualShaderRange, g_cfg.visualCurve);
    }
    else
    {
        InterlockedExchange(&g_plowVisualEnabled, 0);
        if (g_cfg.plowEffect)
            Warn("visible plow after-effect unavailable because both the "
                 "verified deep mask writer and texture-access hooks are "
                 "required; the internal per-road effect remains active");
    }

    if (!weatherHookActive && !roadActive && !maskActive && !deepMaskActive &&
        !textureAccessActive && !internalSnowLimiterActive &&
        !globalMaskUpdateActive)
    {
        InterlockedExchange(&g_active, 0);
        if (g_stopEvent) SetEvent(g_stopEvent);
        StartError("No observer could be started; plugin is not kept loaded");
        return 1;
    }

    bool persistenceRequested = weatherHookActive && g_cfg.plowEffect &&
        g_protectionPersistenceEnabled;
    bool persistenceActive = false;
    if (persistenceRequested)
        persistenceActive = WeatherPersistenceInstall();

    if (g_cfg.overlayEnabled)
        StartOverlay();

    Info("Startup components: protection-persistence=%s; overlay=%s; "
         "optional display/diagnostic failures do not disable installed weather hooks",
         persistenceActive ? "active" :
             (persistenceRequested ? "unavailable" : "not-requested"),
         OverlayStatus());

    Info("v%s active: weather-capture=%s, diagnostic-worker=%s, "
          "road-snow-clearing=%s, visible-snow-mask=%s, deep-mask-writer=%s, "
          "texture-access=%s, "
          "global-mask-boundary=%s, "
          "internal-snow-limiter=%s(multiplier=%.6f,burst-max=%d,"
          "burst-gap=%dms,gradual=%s,step=%d/%dms,visual-batch=%d), "
          "natural-melting=%s(multiplier=%.6f), "
          "visual-snow=%s(levels=%d,shader-range=%.3f,curve=%.3f), "
          "plow-after-effect=%s(protection=%.2f game-min,salt=%.2f "
          "game-h,salt-multiplier=%.6f,dry-preserve=%d,"
          "max-strength-retention=1,points=%d), "
          "visible-plow-sync=%s, grit-spreader=%s, "
           "base-visible-snow=%s(rate=%d green units/s,sync=coalesced-16ms), "
         "road-state-tracker=on(interval=%d ms,max=%d,retention=%d s), "
         "detailed log=%s; original game files remain unchanged",
         PLUGIN_VERSION,
         weatherHookActive ? "on" : "off",
         weatherWorkerActive ? "on" : "off",
         roadActive ? "on" : "off",
         maskActive ? "on" : "off",
         deepMaskActive ? "on" : "off",
         textureAccessActive ? "on" : "off",
          globalMaskUpdateActive ? "on/inline" : "off",
           g_cfg.internalSnowLimiter && internalSnowLimiterActive ?
                "on" : "off",
          g_cfg.snowAccumulationMultiplier,
          g_cfg.maximumSnowAccumulationPerBurst,
          g_cfg.snowBurstResetAfterMs,
          g_cfg.gradualSnowAccumulation ? "on" : "off",
          g_cfg.gradualSnowStepUnits,
          g_cfg.gradualSnowStepIntervalMs,
          g_cfg.gradualVisualBatchUnits,
           g_cfg.naturalMelting && internalSnowLimiterActive ?
               "on" : "off",
          g_cfg.snowReductionMultiplier,
          g_cfg.visualSnowLevels == 0 ? "continuous" : "staged",
          g_cfg.visualSnowLevels, g_cfg.visualShaderRange,
          g_cfg.visualCurve,
           g_cfg.plowEffect && internalSnowLimiterActive ?
               "on" : "off",
          g_cfg.plowProtectionMinutes, g_cfg.plowSaltHours,
          g_cfg.plowSaltAccumulationMultiplier,
          g_cfg.dryPlowingPreservesTreatment,
          g_cfg.maximumTrackedPlowPoints,
         InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) ?
             "on" : "off",
         g_gritSpreaderApi ? "material-aware" : "legacy-strength-1.000",
         g_cfg.experimentalRecoverageLimiter ? "EXPERIMENTAL/on" : "off",
         g_cfg.maximumGreenDecreasePerSecond,
         g_cfg.roadStateSampleIntervalMs,
         g_cfg.maximumTrackedRoads,
         g_cfg.roadStateTrackingSeconds,
         PLUGIN_LOG);
    EndLogPhase("Startup","active");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        InterlockedExchange(&g_active, 0);
        if (g_stopEvent) SetEvent(g_stopEvent);
        if (g_overlayStopEvent) SetEvent(g_overlayStopEvent);

        // Successful plugins are retained until process shutdown.  Do not wait
        // for a worker from DllMain; Windows closes the diagnostic handles when
        // the process ends.  Init-failure paths close their log explicitly.
        if (!g_worker && g_detail != INVALID_HANDLE_VALUE)
        {
            CloseHandle(g_detail);
            g_detail = INVALID_HANDLE_VALUE;
        }
    }
    return TRUE;
}
