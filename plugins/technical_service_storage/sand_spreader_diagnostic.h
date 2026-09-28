// Grit runtime: fuel-proportional vehicle tanks, native home-refill requests,
// material priorities and savegame persistence.
//
// Runtime skill 35 identifies snowplows. Verified clear calls publish the
// material used to weather_roads; dry clears preserve previous protection when
// configured there. Fuel is read, never changed. A refill request at 20 percent
// preserves the remaining grit and delegates the journey to the native AI.
// Depot inventory is charged only for confirmed fills, never for loading a
// saved tank. Lifecycle observation prevents demolished/collapsed depots from
// transferring saved settings to their replacement.
// v0.1.76 beta removes discovery scans, manual route experiments and fixed
// per-road depot consumption. Detailed runtime logs use [general] debug=1.

#ifndef TECHNICAL_SERVICE_SAND_SPREADER_DIAGNOSTIC_H
#define TECHNICAL_SERVICE_SAND_SPREADER_DIAGNOSTIC_H

static const size_t SAND_DIAG_MAX_TRACKED_VEHICLES = 1024;   // table size; [sand_diagnostic] max_vehicles (default 512) may only lower it
static const size_t SAND_DIAG_MAX_SKILLS = 3;
static const size_t SAND_DIAG_RECENT_CLEAR_KEYS = 16;
static const size_t SAND_DIAG_MAX_PANEL_SAMPLE_STATES = 64;
static const size_t SPREADER_MAX_DEPOT_PRIORITY_STATES = 512;
static const size_t SPREADER_MAX_PERSISTED_PRIORITY_STATES = 256;
static const size_t SPREADER_MAX_PERSISTED_TANK_STATES = 512;
static const size_t SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES = 256;
static const size_t SAND_BUILDING_LIFECYCLE_SNAPSHOT_BYTES = 0x0F00;
static const size_t SAND_BUILDING_LIFECYCLE_CHUNK_BYTES = 0x0100;
static const size_t SAND_BUILDING_LIFECYCLE_CHUNKS =
    SAND_BUILDING_LIFECYCLE_SNAPSHOT_BYTES /
    SAND_BUILDING_LIFECYCLE_CHUNK_BYTES;

// Verified WRSR 1.1.1.9 vehicle skill layout. The game uses the same vector in
// its native skill lookup at RVA 0x3E2320. Each record is 0x10 bytes and starts
// with a VEHICLESKILL id; VEHICLESKILL_SNOWPLOW is 35.
static const size_t SAND_VEHICLE_CURRENT_BUILDING = 0x04F0;
static const size_t SAND_VEHICLE_HOME_BUILDING = 0x04F8;
static const size_t SAND_VEHICLE_TARGET_BUILDING = 0x0528;
static const size_t SAND_VEHICLE_FUEL_BUILDING = 0x05E8;
static const size_t SAND_VEHICLE_CURRENT_FUEL = 0x05F0;
static const size_t SAND_VEHICLE_CURRENT_SPEED_KMH = 0x0D24;
static const size_t SAND_VEHICLE_TYPE_DESCRIPTION = 0x1708;
static const size_t SAND_VEHICLE_RETURN_ROUTE_STATE = 0x07A0;
static const size_t SAND_VEHICLE_ROUTE_BEGIN = 0x06A0;
static const size_t SAND_VEHICLE_ROUTE_END = 0x06A8;
static const size_t SAND_VEHICLE_ROUTE_LINK_BEGIN = 0x06B8;
static const size_t SAND_VEHICLE_ROUTE_LINK_END = 0x06C0;
static const size_t SAND_VEHICLE_ROUTE_INDEX = 0x0700;
// These fields are deliberately diagnostic-only. Their changes help identify
// vanilla work reassignment while a return lock is active, but v0.1.19 never
// writes them because their ownership and lifetime are not yet fully verified.
static const size_t SAND_VEHICLE_ASSIGNMENT_BEGIN = 0x0680;
static const size_t SAND_VEHICLE_ASSIGNMENT_END = 0x0688;
static const size_t SAND_VEHICLE_ASSIGNMENT_INDEX = 0x0698;
static const size_t SAND_VEHICLE_CURRENT_TARGET = 0x0C80;
static const size_t SAND_TYPE_SKILL_BEGIN = 0x85E0;
static const size_t SAND_TYPE_SKILL_END = 0x85E8;
// Verified from the WRSR 1.1.1.9 vehicle-information renderer. The native UI
// reads +0x867C for localization ID 1950 (Empty weight) and +0x8678 for ID 1951
// (Engine power). Engine power is already stored in kW and empty weight is
// already stored in metric tonnes.
static const size_t SAND_TYPE_ENGINE_POWER_KW = 0x8678;
static const size_t SAND_TYPE_EMPTY_WEIGHT_T = 0x867C;
static const size_t SAND_TYPE_FUEL_CAPACITY = 0x8684;
static const size_t SAND_SKILL_RECORD_SIZE = 0x10;
static const int SAND_SKILL_SNOWPLOW = 35;
static const size_t SAND_GAME_BUILDING_BEGIN = 0x11B08;
static const size_t SAND_GAME_BUILDING_END = 0x11B10;

// Building lifecycle fields already used by the bundled accumulator and cities
// plugins on the same verified game build. C3D_NODE::GetPosition is a trivial
// read of node+0x80/+0x84/+0x88, making the world position available without
// calling engine code from the background registry observer.
static const size_t SAND_BUILDING_CONSTRUCTION_PROGRESS = 0x0604;
static const size_t SAND_BUILDING_NODE = 0x0320;
static const size_t SAND_BUILDING_NODE_POSITION = 0x0080;
static const size_t SAND_BUILDING_WORLD_POSITION =
    SAND_BUILDING_NODE + SAND_BUILDING_NODE_POSITION;
static const size_t SAND_BUILDING_GOING_AWAY = 0x0EA8;
static const char* const SAND_C3D_NODE_GET_POSITION =
    "?GetPosition@C3D_NODE@@QEAA?AVC3DVECTOR3@@XZ";
static const BYTE EXPECT_SAND_C3D_NODE_GET_POSITION[] = {
    0x8B,0x81,0x80,0x00,0x00,0x00,
    0x89,0x02,
    0x8B,0x81,0x84,0x00,0x00,0x00,
    0x89,0x42,0x04,
    0x8B,0x81,0x88,0x00,0x00,0x00,
    0x89,0x42,0x08
};

// WRSR 1.1.1.9 global world/registry access. The simulation game object is a
// static object, not a pointer. The LEA at RVA_SAND_STATIC_GAME_LEA is verified
// before the background observer reads its building vector. The separate world
// pointer changes on every load and is used only as a generation token.
static const DWORD RVA_SAND_STATIC_GAME = 0x009D4F10;
static const DWORD RVA_SAND_GLOBAL_WORLD_POINTER = 0x009941F0;
static const DWORD RVA_SAND_STATIC_GAME_LEA = 0x0043970A;

static const DWORD RVA_SAND_SKILL_CHECK = 0x003E2320;
static const DWORD RVA_SAND_CLEAR_ENTRY = 0x006BC210;
static const DWORD RVA_SAND_CLEAR_CALL_1 = 0x0069CD08;
static const DWORD RVA_SAND_CLEAR_CALL_2 = 0x0069D0A1;
static const DWORD RVA_SAND_NATURAL_HOME_CALL = 0x0069D0E7;
static const DWORD RVA_SAND_PLAN_ROUTE_HOME = 0x006BC5A0;
static const DWORD RVA_SAND_CLEAN_ROUTE_LINKS = 0x006A6720;
static const DWORD RVA_SAND_VANILLA_REFUEL_ORDER = 0x006B4DE0;
static const DWORD RVA_SAND_TASK_FUEL_ELIGIBILITY = 0x006B3AB0;
static const DWORD RVA_SAND_TASK_FUEL_MODE = 0x009D54C0;
static const DWORD RVA_SAND_NATIVE_LOW_FUEL_COMPARE = 0x006CFB22;
static const DWORD RVA_SAND_NATIVE_FUEL_BUILDING_WRITE = 0x006CFB4E;
static const DWORD RVA_SAND_NATIVE_FUEL_CAPACITY_LOAD = 0x006CFB12;
// After loading the skill-35 float at exe+0x6A2775, vanilla folds it into its
// target speed and compares that value directly with vehicle+0xD24 here. This
// proves that +0xD24 and the snowplow skill parameter use the same km/h scale.
static const DWORD RVA_SAND_NATIVE_SPEED_COMPARE = 0x006A2951;

static const BYTE EXPECT_SAND_SKILL_CHECK[] = {
    0x45,0x8B,0xD0,0x48,0x85,0xD2,0x74,0x4B,
    0x48,0x8B,0x82,0xE8,0x85,0x00,0x00
};
static const BYTE EXPECT_SAND_CLEAR_ENTRY[] = {
    0x48,0x85,0xD2,0x0F,0x84,0x80,0x03,0x00,0x00
};
static const BYTE EXPECT_SAND_CLEAR_CALL_1[] = {
    0xE8,0x03,0xF5,0x01,0x00
};
static const BYTE EXPECT_SAND_CLEAR_CALL_2[] = {
    0xE8,0x6A,0xF1,0x01,0x00
};
static const BYTE EXPECT_SAND_NATURAL_HOME_CALL[] = {
    0xE8,0xB4,0xF4,0x01,0x00
};
static const BYTE EXPECT_SAND_PLAN_ROUTE_HOME[] = {
    0x48,0x8B,0xC4,0x55,0x56,0x57,0x41,0x54,
    0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,
    0xEC,0x80,0x00,0x00,0x00
};
static const BYTE EXPECT_SAND_CLEAN_ROUTE_LINKS[] = {
    0x40,0x53,0x41,0x57,0x48,0x83,0xEC,0x28,
    0x48,0x8B,0x81,0x08,0x17,0x00,0x00,0x44,
    0x0F,0xB6,0xFA,0x48,0x8B,0xD9
};
static const BYTE EXPECT_SAND_VANILLA_REFUEL_ORDER[] = {
    0x48,0x89,0x5C,0x24,0x10,0x56,0x48,0x83,
    0xEC,0x30,0x4C,0x8B,0x81,0x08,0x17,0x00,
    0x00
};
// Complete instructions through mov rax,[rcx+0x1708]. The stock loader's
// generic inline trampoline cannot relocate the leading RIP-relative compare
// or its short conditional jump, so v0.1.61 installs a dedicated trampoline
// which reproduces both control-flow paths before continuing at +0x10.
static const BYTE EXPECT_SAND_TASK_FUEL_ELIGIBILITY[] = {
    0x83,0x3D,0x09,0x1A,0x32,0x00,0x02,
    0x75,0x50,
    0x48,0x8B,0x81,0x08,0x17,0x00,0x00
};
static const size_t SAND_TASK_FUEL_ELIGIBILITY_STOLEN =
    sizeof(EXPECT_SAND_TASK_FUEL_ELIGIBILITY);
// Vanilla compares its calculated reserve with vehicle+0x5F0 and, after the
// candidate-building check, stores that building in vehicle+0x5E8. v0.1.62
// does not patch these instructions; verifying them makes the field ownership
// and the adopted state transition fail closed on an unsupported game build.
static const BYTE EXPECT_SAND_NATIVE_LOW_FUEL_COMPARE[] = {
    0x41,0x0F,0x2F,0x87,0xF0,0x05,0x00,0x00,
    0x76,0x29
};
static const BYTE EXPECT_SAND_NATIVE_FUEL_BUILDING_WRITE[] = {
    0x49,0x89,0x9F,0xE8,0x05,0x00,0x00
};
static const BYTE EXPECT_SAND_NATIVE_FUEL_CAPACITY_LOAD[] = {
    0xF3,0x0F,0x10,0x80,0x84,0x86,0x00,0x00
};
static const BYTE EXPECT_SAND_NATIVE_SPEED_COMPARE[] = {
    0x45,0x0F,0x2F,0xAF,0x24,0x0D,0x00,0x00
};

struct SandSkillView
{
    int valid;
    int snowplow;
    BYTE* typeDescription;
    size_t count;
    int ids[SAND_DIAG_MAX_SKILLS];
    DWORD parameterRaw[SAND_DIAG_MAX_SKILLS];
};

struct SpreaderStorageObservation
{
    int found;
    int materialIndex;
    float protectionStrength;
    size_t storageIndex;
    size_t slotIndex;
    BYTE* storage;
    BYTE* slot;
    void* resource;
    int resourceTextId;
    char resourceName[64];
    float amount;
    float capacity;
    int transportClass;
};

struct SpreaderDepotPriorityState
{
    BYTE* building;
    DWORD presentMask;
    DWORD knownMask;
    int priorities[SPREADER_MAX_MATERIALS]; // 0=OFF, otherwise 1..N
    ULONGLONG lastSeenTick;
    ULONGLONG lastClickTick[SPREADER_MAX_MATERIALS];
};

#pragma pack(push, 1)
struct SpreaderPriorityDiskHeader
{
    char magic[8];
    DWORD version;
    DWORD headerSize;
    DWORD entrySize;
    DWORD count;
    ULONGLONG payloadHash;
};

struct SpreaderPriorityDiskMaterial
{
    char resourceName[64];
    LONG priority;
};

struct SpreaderPriorityDiskEntry
{
    LONG positionMillimetres[3];
    DWORD materialCount;
    SpreaderPriorityDiskMaterial materials[SPREADER_MAX_MATERIALS];
};

struct SpreaderTankDiskHeader
{
    char magic[8];
    DWORD version;
    DWORD headerSize;
    DWORD entrySize;
    DWORD count;
    ULONGLONG payloadHash;
};

struct SpreaderTankDiskEntry
{
    LONG homePositionMillimetres[3];
    DWORD vehicleSlotIndex;
    LONG enginePowerMilliKw;
    LONG emptyWeightMilliKg;
    LONG savedCapacityMilliKg;
    LONG remainingMilliKg;
    DWORD flags;
    LONG returnReason;
    char resourceName[64];
};
#pragma pack(pop)

static const DWORD SPREADER_TANK_FLAG_DRY = 0x00000001u;
static const DWORD SPREADER_TANK_FLAG_THRESHOLD_PENDING = 0x00000002u;
static const DWORD SPREADER_TANK_FLAG_THRESHOLD_CROSSED = 0x00000004u;
static const DWORD SPREADER_TANK_FLAG_REFILL_RETURN = 0x00000008u;
static const DWORD SPREADER_TANK_KNOWN_FLAGS =
    SPREADER_TANK_FLAG_DRY |
    SPREADER_TANK_FLAG_THRESHOLD_PENDING |
    SPREADER_TANK_FLAG_THRESHOLD_CROSSED |
    SPREADER_TANK_FLAG_REFILL_RETURN;

struct SpreaderPersistedPriorityState
{
    int valid;
    BYTE* claimedBuilding;
    SpreaderPriorityDiskEntry disk;
};

struct SpreaderPersistedTankState
{
    int valid;
    void* claimedVehicle;
    SpreaderTankDiskEntry disk;
};

struct SandBuildingLifecycleState
{
    BYTE* building;
    BYTE* typeDescription;
    LONG generation;
    ULONGLONG firstSeenScan;
    ULONGLONG lastSeenScan;
    size_t firstRegistryIndex;
    size_t lastRegistryIndex;
    int initialTechnicalType;
    int lastTechnicalType;
    int active;
    int terminalObserved;
    int positionValid;
    float position[3];
    float constructionProgress;
    int goingAway;
    ULONGLONG siteFingerprint;
    ULONGLONG normalizedChunkHashes[
        SAND_BUILDING_LIFECYCLE_CHUNKS];
};

struct SandRecentClearKey
{
    void* road;
    void* selector;
    ULONGLONG tick;
    TsmGritRoadTreatment treatment;
};

enum SandReturnReason
{
    SAND_RETURN_NONE = 0,
    SAND_RETURN_MANUAL_HOME = 1,
    SAND_RETURN_REFILL = 2
};

struct SandTrackedVehicle
{
    void* vehicle;
    BYTE* typeDescription;
    int initialized;
    int skillValid;
    int snowplow;
    int skillIds[SAND_DIAG_MAX_SKILLS];
    DWORD skillParameters[SAND_DIAG_MAX_SKILLS];
    size_t skillCount;
    void* currentBuilding;
    void* homeBuilding;
    int seenInTechnicalPanel;
    int identityProbed;
    ULONGLONG lastSeenTick;
    ULONGLONG lastClearLogTick;
    LONG64 clearCalls;
    int shadowTankInitialized;
    int shadowTankEmptyLogged;
    float enginePowerKw;
    float emptyWeightKg;
    float shadowTankCapacityKg;
    float shadowTankRemainingKg;
    int shadowTankDryPlowing;
    int shadowTankMaterialIndex;
    float shadowTankProtectionStrength;
    int shadowTankResourceTextId;
    char shadowTankResourceName[64];
    LONG64 shadowTankUseEvents;
    float shadowTankPendingConsumedKg;
    int shadowTankReturnThresholdPending;
    int shadowTankReturnThresholdCrossed;
    int shadowTankRefillCycleActive;
    int nativeFuelBaselineValid;
    float nativeFuelLastAmount;
    float nativeFuelCapacity;
    float nativeLastSpeedKmh;
    float nativePlowSpeedKmh;
    ULONGLONG nativeLastQualifiedClearTick;
    ULONGLONG nativeLastConsumptionSampleTick;
    ULONGLONG nativeLastConsumptionLogTick;
    void* nativeLastQualifiedClearRoad;
    LONG64 nativeFuelConsumptionSamples;
    int returnLockActive;
    int returnStartPending;
    int returnRouteSeen;
    int returnReason;
    DWORD returnRouteState;
    ULONGLONG returnRequestedTick;
    ULONGLONG returnNextDispatchTick;
    int returnRetryRequiresProgress;
    int returnBlindRetryAvailable;
    int returnRetryRouteIndex;
    size_t returnRetryRouteCount;
    size_t returnRetryRouteLinkCount;
    void* returnRetryAssignmentBegin;
    void* returnRetryAssignmentEnd;
    int returnRetryAssignmentIndex;
    void* returnRetryTarget;
    ULONGLONG returnTailObservedTick;
    ULONGLONG returnNextTailRecoveryTick;
    LONG64 returnDispatches;
    void** returnPlannedRouteBegin;
    void** returnPlannedRouteEnd;
    void** returnPlannedLinkBegin;
    void** returnPlannedLinkEnd;
    int returnLastRouteIndex;
    size_t returnLastRouteCount;
    size_t returnLastRouteLinkCount;
    void* returnLastAssignmentBegin;
    void* returnLastAssignmentEnd;
    int returnLastAssignmentIndex;
    void* returnLastTarget;
    void* returnFinishRoad;
    int returnFinishRoadClearPending;
    LONG64 returnFinishRoadClears;
    LONG64 returnBlockedClearCalls;
    ULONGLONG returnLastViolationLogTick;
    LONG64 returnFuelEligibilityBlocks;
    ULONGLONG returnLastFuelEligibilityLogTick;
    LONG64 returnFuelBuildingLatchWrites;
    ULONGLONG returnLastFuelBuildingLatchLogTick;
    int returnArrivalPending;
    ULONGLONG returnArrivalDetectedTick;
    int returnArrivalFinalizeAttempts;
    SandRecentClearKey recentClearKeys[SAND_DIAG_RECENT_CLEAR_KEYS];
    size_t nextRecentClearKey;
    size_t recentClearKeyCount;
};

struct SandTrackUpdate
{
    int firstSeen;
    int skillChanged;
    int buildingChanged;
    int seenInTechnicalPanel;
    void* previousCurrentBuilding;
    void* previousHomeBuilding;
    void* currentBuilding;
    void* homeBuilding;
    int previousInsideHome;
    int insideHome;
    LONG64 clearCalls;
};

enum SandShadowTankStatus
{
    SAND_SHADOW_TANK_DISABLED = 0,
    SAND_SHADOW_TANK_CONSUMED,
    SAND_SHADOW_TANK_DUPLICATE,
    SAND_SHADOW_TANK_NOT_SNOWPLOW,
    SAND_SHADOW_TANK_UNINITIALIZED,
    SAND_SHADOW_TANK_EMPTY,
    SAND_SHADOW_TANK_INACTIVE_WORK
};

struct SandShadowTankUseResult
{
    SandShadowTankStatus status;
    int materialIndex;
    float protectionStrength;
    int resourceTextId;
    char resourceName[64];
    float capacityKg;
    float requestedKg;
    float consumedKg;
    float beforeKg;
    float afterKg;
    int becameEmpty;
    int firstEmptyReport;
    int returnThresholdPending;
    int activePlowing;
    float currentSpeedKmh;
    float plowSpeedKmh;
};

enum SandConsumeStatus
{
    SAND_CONSUME_DISABLED = 0,
    SAND_CONSUME_OK,
    SAND_CONSUME_DUPLICATE,
    SAND_CONSUME_NOT_SNOWPLOW,
    SAND_CONSUME_NO_HOME,
    SAND_CONSUME_WRONG_HOME_TYPE,
    SAND_CONSUME_NO_STORAGE,
    SAND_CONSUME_EMPTY,
    SAND_CONSUME_INVALID_STORAGE,
    SAND_CONSUME_WRITE_RACE
};

struct SandConsumeResult
{
    SandConsumeStatus status;
    size_t storageIndex;
    size_t slotIndex;
    float requested;
    float consumed;
    float before;
    float after;
};

typedef void (__fastcall *SandRoadSnowClearFn)(void* vehicle,
                                                void* road,
                                                void* selector);
typedef bool (__fastcall *SandPlanRouteHomeFn)(void* vehicle);
typedef void (__fastcall *SandCleanRouteLinksFn)(void* vehicle,
                                                  bool removeBothSides);
typedef void (__fastcall *SandVanillaRefuelOrderFn)(void* vehicle,
                                                     bool broadSearch);
typedef bool (__fastcall *SandTaskFuelEligibilityFn)(void* vehicle);

static SandTrackedVehicle
    g_sandTrackedVehicles[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
static SRWLOCK g_sandTrackerLock = SRWLOCK_INIT;
static SpreaderDepotPriorityState
    g_spreaderDepotPriorities[SPREADER_MAX_DEPOT_PRIORITY_STATES] = {};
static SRWLOCK g_spreaderDepotPriorityLock = SRWLOCK_INIT;
static SpreaderPersistedPriorityState g_spreaderPersistedPriorities[
    SPREADER_MAX_PERSISTED_PRIORITY_STATES] = {};
static SpreaderPersistedTankState g_spreaderPersistedTanks[
    SPREADER_MAX_PERSISTED_TANK_STATES] = {};
static SRWLOCK g_spreaderTankPersistenceLock = SRWLOCK_INIT;
static char g_spreaderPriorityWorldFolder[MAX_PATH] = {};
static char g_spreaderTankWorldFolder[MAX_PATH] = {};
static volatile LONG g_spreaderPriorityPersistenceActive = 0;
static volatile LONG g_spreaderPriorityPersistenceLoadSeen = 0;
static volatile LONG64 g_spreaderPriorityPersistenceLoads = 0;
static volatile LONG64 g_spreaderPriorityPersistenceSaves = 0;
static volatile LONG64 g_spreaderPriorityPersistenceRestores = 0;
static volatile LONG64 g_spreaderPriorityPersistenceDeletes = 0;
static volatile LONG64 g_spreaderPriorityLifetimeResets = 0;
static volatile LONG g_spreaderTankPersistenceLoadSeen = 0;
static volatile LONG64 g_spreaderTankPersistenceLoads = 0;
static volatile LONG64 g_spreaderTankPersistenceSaves = 0;
static volatile LONG64 g_spreaderTankPersistenceRestores = 0;
static volatile LONG64 g_spreaderTankPersistenceFallbacks = 0;
static SandBuildingLifecycleState g_sandBuildingLifecycleStates[
    SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES] = {};
static SandBuildingLifecycleState g_sandPreviousWorldLifecycleStates[
    SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES] = {};
static SandRoadSnowClearFn g_sandRoadSnowClearEntry = nullptr;
static SandPlanRouteHomeFn g_sandPlanRouteHome = nullptr;
static SandCleanRouteLinksFn g_sandCleanRouteLinks = nullptr;
static SandVanillaRefuelOrderFn g_sandVanillaRefuelOrder = nullptr;
static SandTaskFuelEligibilityFn
    g_sandTaskFuelEligibilityOriginal = nullptr;
static void* volatile g_sandGameContext = nullptr;
static volatile LONG64 g_sandClearCalls = 0;
static volatile LONG64 g_sandUniqueClearCalls = 0;
static volatile LONG64 g_sandDuplicateClearCalls = 0;
static volatile LONG64 g_sandConsumptionEvents = 0;
static volatile LONG64 g_sandEmptyEvents = 0;
static volatile LONG64 g_sandDepotRefillDebitEvents = 0;
static volatile LONG64 g_sandDepotRefillPartialEvents = 0;
static volatile LONG64 g_sandDepotRefillDebitFailures = 0;
static volatile LONG64 g_sandShadowTankLoadEvents = 0;
static volatile LONG64 g_sandShadowTankUseEvents = 0;
static volatile LONG64 g_sandShadowTankEmptyEvents = 0;
static volatile LONG64 g_sandReturnRequests = 0;
static volatile LONG64 g_sandReturnReassignments = 0;
static volatile LONG64 g_sandReturnArrivals = 0;
static volatile LONG64 g_sandReturnFinishRoadClears = 0;
static volatile LONG64 g_sandReturnReactivationRequests = 0;
static volatile LONG64 g_sandReturnBlockedClears = 0;
static volatile LONG64 g_sandReturnArrivalCleanups = 0;
static volatile LONG64 g_sandReturnTailRecoveries = 0;
static volatile LONG64 g_sandTaskFuelEligibilityBlocks = 0;
static volatile LONG64 g_sandFuelBuildingLatchWrites = 0;
static volatile LONG64 g_sandFuelBuildingLatchReassertions = 0;
static volatile LONG64 g_sandNaturalBoundaryCalls = 0;
static volatile LONG64 g_sandNaturalBoundaryPlans = 0;
static volatile LONG64 g_sandNaturalBoundaryFailures = 0;
static volatile LONG64 g_sandNativeFuelConsumptionSamples = 0;
static volatile LONG64 g_sandNativeFuelConsumptionDebits = 0;
static volatile LONG64 g_sandNativeFuelConsumptionThresholds = 0;
static volatile LONG g_sandClearHooksActive = 0;
static volatile LONG g_sandNaturalBoundaryHookActive = 0;
static volatile LONG g_sandTaskFuelEligibilityHookActive = 0;
static volatile LONG g_sandNativeFuelBuildingPathVerified = 0;
static volatile LONG g_sandNativeFuelConsumptionPathVerified = 0;
static volatile LONG g_sandSkillLayoutVerified = 0;
static volatile LONG g_sandClearFaultLogged = 0;
static volatile LONG g_sandReturnDispatchInProgress = 0;
static volatile LONG g_sandReturnObserverStarted = 0;
static volatile LONG g_sandReturnObserverFaultLogged = 0;
static volatile LONG g_sandGlobalRegistryVerified = 0;
static volatile LONG g_sandGlobalRegistryFaultLogged = 0;
static volatile LONG g_sandWorldGeneration = 0;
static volatile LONG g_sandWorldBaselineComplete = 0;
static volatile LONG g_sandWorldBaselineCandidateBuildingCount = -1;
static volatile LONG g_sandWorldBaselineStableScans = 0;
static volatile LONG64 g_sandGlobalRegistryScans = 0;
static volatile LONG g_sandBuildingLifecycleLayoutVerified = 0;
static volatile LONG g_sandBuildingLifecycleCapacityWarningLogged = 0;
static volatile LONG g_sandBuildingLifecycleSnapshotRequested = 0;
static volatile LONG g_sandBuildingLifecycleSnapshotKeyDown = 0;
static volatile LONG64 g_sandBuildingLifecycleAppearances = 0;
static volatile LONG64 g_sandBuildingLifecycleStateChanges = 0;
static volatile LONG64 g_sandBuildingLifecycleTerminalEvents = 0;
static volatile LONG64 g_sandBuildingLifecycleDisappearances = 0;
static volatile LONG64 g_sandBuildingLifecycleManualSnapshots = 0;
static volatile LONG g_spreaderMaterialValidationGeneration = 0;
static void* volatile g_sandObservedWorld = nullptr;
static HANDLE g_sandReturnObserverThread = nullptr;
static volatile LONG g_sandClearThreadId = 0;
static volatile LONG g_sandArrivalTimerQueued = 0;
static volatile LONG g_sandReturnRetryTimerQueued = 0;
static volatile LONG g_sandArrivalThreadWarningLogged = 0;
static HWND g_sandArrivalTimerWindow = nullptr;
static const UINT_PTR SAND_ARRIVAL_TIMER_ID = 0x54535350u;
static const UINT_PTR SAND_RETURN_RETRY_TIMER_ID = 0x54535351u;
static const ULONGLONG SAND_RETURN_TAIL_HOLD_MS = 500;
static const ULONGLONG SAND_RETURN_STALLED_RETRY_MS = 1000;
static const ULONGLONG SAND_FUEL_CONSUMPTION_SAMPLE_MS = 100;
static const float SAND_MINIMUM_PLOW_SPEED_KMH = 0.01f;
static const int SAND_NATURAL_BOUNDARY_RETURN_ENABLED = 1;
static const int SAND_RETURN_ACTION_DIRECT_ONCE = 3;
struct SandPanelSampleState
{
    BYTE* building;
    ULONGLONG lastTick;
};
static SandPanelSampleState
    g_sandPanelSampleStates[SAND_DIAG_MAX_PANEL_SAMPLE_STATES] = {};
// Vanilla keeps Technical Services vehicles working after the visible fuel
// gauge reaches 0.00 and represents that reserve as a finite negative value in
// vehicle+0x5F0. Non-negative validation therefore ends a valid plough session
// too early. This helper rejects NaN/infinity while deliberately retaining the
// signed value; the sampler applies the capacity-relative plausibility bound.
static bool SandIsFiniteSignedNativeFuel(float value)
{
    return value == value && value > -FLT_MAX && value < FLT_MAX;
}

static int ClampSandSetting(const char* key, int value,
                            int minimum, int maximum)
{
    if (value < minimum)
    {
        Report("WARN", INI_NAME, "sand-diagnostic-config",
            "%s=%d is below the safe minimum and was clamped to %d",
            key, value, minimum);
        return minimum;
    }
    if (value > maximum)
    {
        Report("WARN", INI_NAME, "sand-diagnostic-config",
            "%s=%d exceeds the safe maximum and was clamped to %d",
            key, value, maximum);
        return maximum;
    }
    return value;
}

static void LoadSandDiagnosticConfig()
{
    g_sandDiagnosticEnabled =
        ConfigInt("sand_diagnostic", "enabled") ? 1 : 0;
    // Removed fixed-per-road debit must not be silently reactivated by an old INI.
    if (ConfigInt("sand_diagnostic", "consumption_enabled"))
        Report("WARN", INI_NAME, "removed-legacy-consumption",
            "consumption_enabled is obsolete and ignored; only confirmed depot refills debit inventory");
    g_sandFuelConsumptionFactorPercent = ClampSandSetting(
        "grit_fuel_consumption_factor_percent",
        ConfigInt("sand_diagnostic", "grit_fuel_consumption_factor_percent"),
        1, 1000);
    g_sandDuplicateWindowMs = ClampSandSetting(
        "duplicate_window_ms",
        ConfigInt("sand_diagnostic", "duplicate_window_ms"),
        0, 60000);
    g_sandSampleIntervalMs = ClampSandSetting(
        "sample_interval_ms",
        ConfigInt("sand_diagnostic", "sample_interval_ms"),
        100, 60000);
    g_sandClearLogIntervalMs = ClampSandSetting(
        "clear_log_interval_ms",
        ConfigInt("sand_diagnostic", "clear_log_interval_ms"),
        0, 60000);
    g_sandShadowTankEnabled =
        ConfigInt("sand_diagnostic", "shadow_tank_enabled") ? 1 : 0;
    g_sandAutomaticReturnEnabled =
        ConfigInt("sand_diagnostic", "automatic_return_enabled") ? 1 : 0;
    g_sandReturnLockEnabled =
        ConfigInt("sand_diagnostic", "return_lock_enabled") ? 1 : 0;
    g_sandReturnRetryIntervalMs = ClampSandSetting(
        "return_retry_interval_ms",
        ConfigInt("sand_diagnostic", "return_retry_interval_ms"),
        250, 10000);
    g_sandReturnArrivalSettleMs = ClampSandSetting(
        "return_arrival_settle_ms",
        ConfigInt("sand_diagnostic", "return_arrival_settle_ms"),
        10, 2000);
    // 0.3.3: the reserve threshold is configured in whole percent of the tank
    // capacity; the 0.3.2 key in basis points is still honoured when the percent
    // key is absent. Internally the value stays in basis points.
    if (ConfigKeyPresent("sand_diagnostic", "return_threshold_percent") ||
        !ConfigKeyPresent("sand_diagnostic", "return_threshold_basis_points"))
    {
        g_sandReturnThresholdBasisPoints = 100 * ClampSandSetting(
            "return_threshold_percent",
            ConfigInt("sand_diagnostic", "return_threshold_percent"),
            0, 100);
    }
    else
    {
        g_sandReturnThresholdBasisPoints = ClampSandSetting(
            "return_threshold_basis_points",
            ConfigInt("sand_diagnostic", "return_threshold_basis_points"),
            0, 10000);
        Report("WARN", INI_NAME, "sand-diagnostic-config",
            "return_threshold_basis_points=%d is the 0.3.2 key; use return_threshold_percent=%d",
            g_sandReturnThresholdBasisPoints, g_sandReturnThresholdBasisPoints / 100);
    }
    g_sandTankWeightPercent = ClampSandSetting(
        "tank_weight_percent",
        ConfigInt("sand_diagnostic", "tank_weight_percent"),
        0, 50);
    g_sandTankPowerKgPerKw = ClampSandSetting(
        "tank_power_kg_per_kw",
        ConfigInt("sand_diagnostic", "tank_power_kg_per_kw"),
        0, 20);
    g_sandTankCapacityMultiplierPercent = ClampSandSetting(
        "tank_capacity_multiplier_percent",
        ConfigInt("sand_diagnostic", "tank_capacity_multiplier_percent"),
        10, 500);
    g_sandTankCapacityStepKg = ClampSandSetting(
        "tank_capacity_step_kg",
        ConfigInt("sand_diagnostic", "tank_capacity_step_kg"),
        1, 1000);
    g_sandTankMinimumCapacityKg = ClampSandSetting(
        "tank_minimum_capacity_kg",
        ConfigInt("sand_diagnostic", "tank_minimum_capacity_kg"),
        1, 10000);
    g_sandTankMaximumCapacityKg = ClampSandSetting(
        "tank_maximum_capacity_kg",
        ConfigInt("sand_diagnostic", "tank_maximum_capacity_kg"),
        1, 20000);
    g_sandVehicleTankDisplayEnabled =
        ConfigInt("sand_diagnostic", "vehicle_tank_display_enabled")
        ? 1 : 0;
    g_sandVehicleTankDisplayOffsetX = ClampSandSetting(
        "vehicle_tank_display_offset_x",
        ConfigInt("sand_diagnostic", "vehicle_tank_display_offset_x"),
        -1000, 1000);
    g_sandVehicleTankDisplayOffsetY = ClampSandSetting(
        "vehicle_tank_display_offset_y",
        ConfigInt("sand_diagnostic", "vehicle_tank_display_offset_y"),
        -1000, 1000);
    g_sandGlobalInitializationIntervalMs = ClampSandSetting(
        "global_initialization_interval_ms",
        ConfigInt("sand_diagnostic", "global_initialization_interval_ms"),
        250, 10000);
    g_sandBuildingLifecycleDiagnosticEnabled =
        ConfigInt("tank_probe", "building_lifecycle") ? 1 : 0;
    if (g_sandTankMaximumCapacityKg < g_sandTankMinimumCapacityKg)
    {
        Report("WARN", INI_NAME, "sand-diagnostic-config",
            "tank_maximum_capacity_kg=%d is below tank_minimum_capacity_kg=%d and was raised to the minimum",
            g_sandTankMaximumCapacityKg,
            g_sandTankMinimumCapacityKg);
        g_sandTankMaximumCapacityKg = g_sandTankMinimumCapacityKg;
    }
    g_sandMaxVehicles = ClampSandSetting(
        "max_vehicles",
        ConfigInt("sand_diagnostic", "max_vehicles"),
        1, (int)SAND_DIAG_MAX_TRACKED_VEHICLES);

}

static bool SandReadSkillView(BYTE* vehicle, SandSkillView* out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!InterlockedCompareExchange(&g_sandSkillLayoutVerified, 0, 0))
        return false;
    if (!vehicle ||
        !ReadablePtr(vehicle + SAND_VEHICLE_TYPE_DESCRIPTION, sizeof(void*)))
        return false;

    BYTE* typeDescription =
        *(BYTE**)(vehicle + SAND_VEHICLE_TYPE_DESCRIPTION);
    if (!typeDescription ||
        !ReadablePtr(typeDescription + SAND_TYPE_SKILL_BEGIN, 16))
        return false;

    BYTE* begin = *(BYTE**)(typeDescription + SAND_TYPE_SKILL_BEGIN);
    BYTE* end = *(BYTE**)(typeDescription + SAND_TYPE_SKILL_END);
    if ((!begin && end) || (begin && !end) ||
        (begin && end && end < begin))
        return false;

    size_t bytes = begin ? (size_t)(end - begin) : 0;
    if ((bytes % SAND_SKILL_RECORD_SIZE) != 0) return false;
    size_t count = bytes / SAND_SKILL_RECORD_SIZE;
    if (count > SAND_DIAG_MAX_SKILLS ||
        (bytes && !ReadablePtr(begin, bytes)))
        return false;

    out->valid = 1;
    out->typeDescription = typeDescription;
    out->count = count;
    for (size_t i = 0; i < count; ++i)
    {
        BYTE* record = begin + i * SAND_SKILL_RECORD_SIZE;
        out->ids[i] = *(int*)(record + 0x00);
        memcpy(&out->parameterRaw[i], record + 0x04, sizeof(DWORD));
        if (out->ids[i] == SAND_SKILL_SNOWPLOW)
            out->snowplow = 1;
    }
    return true;
}

static void SandFormatSkills(const SandSkillView& skills,
                             char* out, size_t outSize)
{
    if (!out || outSize == 0) return;
    out[0] = 0;
    if (!skills.valid)
    {
        strcpy_s(out, outSize, "unreadable");
        return;
    }
    if (skills.count == 0)
    {
        strcpy_s(out, outSize, "none");
        return;
    }

    size_t used = 0;
    for (size_t i = 0; i < skills.count && used + 1 < outSize; ++i)
    {
        float parameterFloat = 0.0f;
        memcpy(&parameterFloat, &skills.parameterRaw[i], sizeof(float));
        int written = _snprintf_s(
            out + used, outSize - used, _TRUNCATE,
            "%s%d(raw=0x%08lX,float=%.6g)",
            i ? "," : "", skills.ids[i],
            (unsigned long)skills.parameterRaw[i], parameterFloat);
        if (written < 0) break;
        used += (size_t)written;
    }
}



static bool SandTakePanelSample(BYTE* building, ULONGLONG now,
                                bool force, bool* firstSample)
{
    if (firstSample) *firstSample = false;
    SandPanelSampleState* selected = nullptr;
    SandPanelSampleState* empty = nullptr;
    SandPanelSampleState* oldest = &g_sandPanelSampleStates[0];
    for (size_t i = 0; i < SAND_DIAG_MAX_PANEL_SAMPLE_STATES; ++i)
    {
        SandPanelSampleState* item = &g_sandPanelSampleStates[i];
        if (item->building == building) { selected = item; break; }
        if (!item->building && !empty) empty = item;
        if (item->lastTick < oldest->lastTick) oldest = item;
    }
    if (!selected)
    {
        selected = empty ? empty : oldest;
        selected->building = building;
        selected->lastTick = 0;
        if (firstSample) *firstSample = true;
    }
    if (!force && selected->lastTick && now >= selected->lastTick &&
        now - selected->lastTick < (ULONGLONG)g_sandSampleIntervalMs)
        return false;
    selected->lastTick = now;
    return true;
}


















struct SpreaderResourceVectorView
{
    BYTE* begin;
    BYTE* end;
    BYTE* capacityEnd;
};

// The resources plugin and the base game both publish resources through this
// live std::vector. Validation is deliberately delayed until a loaded world has
// buildings, because mod resources are appended while the world data is being
// prepared. The catalogue is revalidated for every world generation.
static bool ValidateSpreaderMaterialCatalogRuntime(LONG generation)
{
    if (generation <= 0 || !g_exeBase || !g_spreaderMaterialCount)
        return false;
    if (InterlockedCompareExchange(
            &g_spreaderMaterialValidationGeneration, 0, 0) == generation)
        return true;

    SpreaderResourceVectorView view = {};
    BYTE* vectorAddress = g_exeBase + RVA_RESOURCE_VECTOR;
    if (!ReadablePtr(vectorAddress, sizeof(view))) return false;
    memcpy(&view, vectorAddress, sizeof(view));

    uintptr_t beginAddress = (uintptr_t)view.begin;
    uintptr_t endAddress = (uintptr_t)view.end;
    uintptr_t capacityAddress = (uintptr_t)view.capacityEnd;
    if (!beginAddress || endAddress < beginAddress ||
        (capacityAddress && capacityAddress < endAddress))
        return false;

    size_t bytes = (size_t)(endAddress - beginAddress);
    if (!bytes || (bytes % RESOURCE_RECORD_SIZE) != 0)
        return false;
    size_t resourceCount = bytes / RESOURCE_RECORD_SIZE;
    if (resourceCount < MIN_RESOURCE_RECORDS ||
        resourceCount > MAX_RESOURCE_RECORDS ||
        !ReadablePtr(view.begin, bytes))
        return false;

    size_t resolvedCount = 0;
    for (size_t materialIndex = 0;
         materialIndex < g_spreaderMaterialCount; ++materialIndex)
    {
        SpreaderMaterialDefinition* material =
            &g_spreaderMaterials[materialIndex];
        int resolvedIndex = -1;
        for (size_t resourceIndex = 0;
             resourceIndex < resourceCount; ++resourceIndex)
        {
            char liveName[64] = {};
            BYTE* record = view.begin +
                           resourceIndex * RESOURCE_RECORD_SIZE;
            if (!SafeReadStr(record + RESOURCE_NAME,
                             liveName, sizeof(liveName)))
                continue;
            if (_stricmp(liveName, material->resourceName) == 0)
            {
                resolvedIndex = (int)resourceIndex;
                break;
            }
        }

        material->runtimeResourceIndex = resolvedIndex;
        if (resolvedIndex >= 0)
        {
            InterlockedExchange(
                (volatile LONG*)&material->runtimeValidationState, 1);
            ++resolvedCount;
            Report("INFO", INI_NAME, "grit-material-runtime",
                "generation=%ld order=%llu resource=%s protection_strength=%.3f resource_index=%d validation=resolved enabled=1",
                generation,
                (unsigned long long)(materialIndex + 1),
                material->resourceName,
                material->protectionStrength,
                resolvedIndex);
        }
        else
        {
            InterlockedExchange(
                (volatile LONG*)&material->runtimeValidationState, 2);
            Report("WARN", INI_NAME, "grit-material-runtime",
                "generation=%ld order=%llu resource=%s protection_strength=%.3f validation=not-found enabled=0; use the exact internal name from resources.ini or the base-game resource table",
                generation,
                (unsigned long long)(materialIndex + 1),
                material->resourceName,
                material->protectionStrength);
        }
    }

    InterlockedExchange(&g_spreaderMaterialValidationGeneration,
                        generation);
    Report(resolvedCount ? "INFO" : "WARN", INI_NAME,
        "grit-material-runtime",
        "generation=%ld live_resources=%llu configured=%llu resolved=%llu disabled=%llu validation=complete",
        generation,
        (unsigned long long)resourceCount,
        (unsigned long long)g_spreaderMaterialCount,
        (unsigned long long)resolvedCount,
        (unsigned long long)(g_spreaderMaterialCount - resolvedCount));
    return true;
}

static int SpreaderRuntimeResourceTextId(
    const SpreaderMaterialDefinition* material)
{
    if (!material || material->runtimeResourceIndex < 0 || !g_exeBase)
        return 0;
    SpreaderResourceVectorView view = {};
    BYTE* vectorAddress = g_exeBase + RVA_RESOURCE_VECTOR;
    if (!ReadablePtr(vectorAddress, sizeof(view))) return 0;
    memcpy(&view, vectorAddress, sizeof(view));
    if (!view.begin || !view.end || view.end < view.begin) return 0;
    size_t bytes = (size_t)(view.end - view.begin);
    if (!bytes || (bytes % RESOURCE_RECORD_SIZE) != 0) return 0;
    size_t count = bytes / RESOURCE_RECORD_SIZE;
    if ((size_t)material->runtimeResourceIndex >= count) return 0;
    BYTE* record = view.begin +
        (size_t)material->runtimeResourceIndex * RESOURCE_RECORD_SIZE;
    if (!ReadablePtr(record + RESOURCE_TEXT_ID, sizeof(int))) return 0;
    return *(int*)(record + RESOURCE_TEXT_ID);
}

static const char SPREADER_PRIORITY_FILE_NAME[] =
    "tesmioloader.technical_service_storage.priorities.bin";
static const char SPREADER_PRIORITY_FILE_MAGIC[8] = {
    'T','S','M','G','R','I','T','1'
};
static const DWORD SPREADER_PRIORITY_FILE_VERSION = 1;
static const char SPREADER_TANK_FILE_NAME[] =
    "tesmioloader.technical_service_storage.tanks.bin";
static const char SPREADER_TANK_FILE_MAGIC[8] = {
    'T','S','M','T','A','N','K','1'
};
static const DWORD SPREADER_TANK_FILE_VERSION = 1;
static const DWORD RVA_SPREADER_WORLD_SAVE = 0x00007C20;
static const DWORD RVA_SPREADER_WORLD_SAVE_CALL = 0x0042CDAE;
static const char SPREADER_CREATE_MANAGED_TEXTURE_SYMBOL[] =
    "?CreateManagedTexture@C3D_MIDDLEPOINT@@QEAAPEAVC3DAPI_TEXTURE@@PEBD@Z";

typedef void* (__fastcall *SpreaderCreateManagedTextureFn)(
    void* middlepoint, const char* path);
typedef void (*SpreaderWorldSaveFn)(void* self, const char* folder);

static SpreaderCreateManagedTextureFn
    g_spreaderOriginalCreateManagedTexture = nullptr;
static SpreaderWorldSaveFn g_spreaderOriginalWorldSave = nullptr;

static int SpreaderCountMaterialBits(DWORD mask);
static void SpreaderLoadTankSidecar(const char* folder);
static void SpreaderSaveTankSidecar(const char* folder);

static ULONGLONG SpreaderPersistenceHash(
    const void* bytes, size_t size)
{
    const BYTE* data = (const BYTE*)bytes;
    ULONGLONG hash = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool SpreaderPositionMillimetres(
    BYTE* building, LONG out[3])
{
    if (out) memset(out, 0, sizeof(LONG) * 3);
    if (!building || !out ||
        !InterlockedCompareExchange(
            &g_sandBuildingLifecycleLayoutVerified, 0, 0) ||
        !ReadablePtr(building + SAND_BUILDING_WORLD_POSITION,
                     sizeof(float) * 3))
        return false;

    float position[3] = {};
    memcpy(position,
           building + SAND_BUILDING_WORLD_POSITION,
           sizeof(position));
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!_finite((double)position[axis]) ||
            position[axis] < -2000000.0f ||
            position[axis] > 2000000.0f)
            return false;
        double scaled = (double)position[axis] * 1000.0;
        out[axis] = (LONG)(scaled >= 0.0 ?
            scaled + 0.5 : scaled - 0.5);
    }
    return true;
}

static bool SpreaderPositionsMatch(
    const LONG left[3], const LONG right[3])
{
    if (!left || !right) return false;
    const LONG toleranceMillimetres = 50;
    for (int axis = 0; axis < 3; ++axis)
    {
        LONGLONG delta = (LONGLONG)left[axis] - right[axis];
        if (delta < -toleranceMillimetres ||
            delta > toleranceMillimetres)
            return false;
    }
    return true;
}

// The native world loader/saver supplies paths such as
// `save\12345 - World Name`. Those paths are relative to the game's
// `media_soviet` virtual-filesystem root, not to the process working directory
// (which can be the TesmioLoader build folder). Resolve them explicitly before
// using ordinary Win32 file APIs. Absolute paths are retained unchanged.
static bool SpreaderResolveWorldFolder(
    const char* folder, char* out, size_t outSize)
{
    if (!folder || !folder[0] || !out || outSize < 2) return false;

    bool driveAbsolute = folder[0] && folder[1] == ':';
    bool uncAbsolute =
        (folder[0] == '\\' && folder[1] == '\\') ||
        (folder[0] == '/' && folder[1] == '/');
    if (driveAbsolute || uncAbsolute)
    {
        int written = _snprintf_s(
            out, outSize, _TRUNCATE, "%s", folder);
        return written > 0 && (size_t)written < outSize;
    }

    char executable[MAX_PATH * 2] = {};
    DWORD length = GetModuleFileNameA(
        nullptr, executable, (DWORD)sizeof(executable));
    if (!length || length >= sizeof(executable)) return false;
    char* slash = strrchr(executable, '\\');
    char* forwardSlash = strrchr(executable, '/');
    if (!slash || (forwardSlash && forwardSlash > slash))
        slash = forwardSlash;
    if (!slash) return false;
    *slash = 0;

    bool alreadyMediaRelative =
        _strnicmp(folder, "media_soviet\\", 13) == 0 ||
        _strnicmp(folder, "media_soviet/", 13) == 0;
    int written = _snprintf_s(
        out, outSize, _TRUNCATE,
        alreadyMediaRelative ? "%s\\%s" : "%s\\media_soviet\\%s",
        executable, folder);
    return written > 0 && (size_t)written < outSize;
}

static bool SpreaderPriorityPath(
    const char* folder, const char* suffix,
    char* out, size_t outSize)
{
    if (!folder || !folder[0] || !out || outSize < 2) return false;
    int written = _snprintf_s(
        out, outSize, _TRUNCATE, "%s/%s%s",
        folder, SPREADER_PRIORITY_FILE_NAME,
        suffix ? suffix : "");
    return written > 0 && (size_t)written < outSize;
}

static bool SpreaderTankPath(
    const char* folder, const char* suffix,
    char* out, size_t outSize)
{
    if (!folder || !folder[0] || !out || outSize < 2) return false;
    int written = _snprintf_s(
        out, outSize, _TRUNCATE, "%s/%s%s",
        folder, SPREADER_TANK_FILE_NAME,
        suffix ? suffix : "");
    return written > 0 && (size_t)written < outSize;
}

static bool SpreaderReadExact(
    HANDLE file, void* buffer, DWORD bytes)
{
    BYTE* cursor = (BYTE*)buffer;
    DWORD remaining = bytes;
    while (remaining)
    {
        DWORD got = 0;
        if (!ReadFile(file, cursor, remaining, &got, nullptr) || !got)
            return false;
        cursor += got;
        remaining -= got;
    }
    return true;
}

static bool SpreaderWriteExact(
    HANDLE file, const void* buffer, DWORD bytes)
{
    const BYTE* cursor = (const BYTE*)buffer;
    DWORD remaining = bytes;
    while (remaining)
    {
        DWORD put = 0;
        if (!WriteFile(file, cursor, remaining, &put, nullptr) || !put)
            return false;
        cursor += put;
        remaining -= put;
    }
    return true;
}

static void SpreaderClearLoadedPriorityRecordsLocked()
{
    memset(g_spreaderPersistedPriorities, 0,
           sizeof(g_spreaderPersistedPriorities));
}

static void SpreaderLoadPrioritySidecar(const char* folder)
{
    if (!g_depotMaterialPriorityPersistenceEnabled ||
        !folder || !folder[0])
        return;

    AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
    SpreaderClearLoadedPriorityRecordsLocked();
    memset(g_spreaderDepotPriorities, 0,
           sizeof(g_spreaderDepotPriorities));
    g_spreaderPriorityWorldFolder[0] = 0;
    ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);
    InterlockedExchange(&g_spreaderPriorityPersistenceLoadSeen, 0);

    char resolvedFolder[MAX_PATH * 2] = {};
    char path[MAX_PATH * 2] = {};
    if (!SpreaderResolveWorldFolder(
            folder, resolvedFolder, sizeof(resolvedFolder)) ||
        !SpreaderPriorityPath(
            resolvedFolder, "", path, sizeof(path)))
    {
        InterlockedExchange(&g_spreaderPriorityPersistenceLoadSeen, 1);
        Report("WARN", "Priority persistence", "load-path",
            "The native world folder could not be resolved to the media_soviet save directory; this save uses default depot priorities (native_folder=%s)",
            folder);
        return;
    }

    AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
    strncpy_s(g_spreaderPriorityWorldFolder,
              sizeof(g_spreaderPriorityWorldFolder),
              resolvedFolder, _TRUNCATE);
    ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);

    HANDLE file = CreateFileA(
        path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        InterlockedExchange(&g_spreaderPriorityPersistenceLoadSeen, 1);
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        {
            Report("INFO", "Priority persistence", "load",
                "native_folder=%s resolved_folder=%s file=%s status=no-sidecar restored_entries=0 defaults_for_new_world=1",
                folder, resolvedFolder, SPREADER_PRIORITY_FILE_NAME);
        }
        else
        {
            ReportWindows("WARN", "Priority persistence", "load-open",
                "The save-specific priority sidecar could not be opened; defaults are used",
                error, "Verify access to the savegame folder");
        }
        return;
    }

    SpreaderPriorityDiskHeader header = {};
    bool valid = SpreaderReadExact(
        file, &header, (DWORD)sizeof(header));
    if (valid)
    {
        valid = memcmp(header.magic,
                       SPREADER_PRIORITY_FILE_MAGIC, 8) == 0 &&
            header.version == SPREADER_PRIORITY_FILE_VERSION &&
            header.headerSize == sizeof(header) &&
            header.entrySize == sizeof(SpreaderPriorityDiskEntry) &&
            header.count <= SPREADER_MAX_PERSISTED_PRIORITY_STATES;
    }

    size_t payloadBytes = valid ?
        (size_t)header.count * sizeof(SpreaderPriorityDiskEntry) : 0;
    SpreaderPriorityDiskEntry* entries = nullptr;
    if (valid && payloadBytes)
    {
        entries = (SpreaderPriorityDiskEntry*)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, payloadBytes);
        valid = entries && SpreaderReadExact(
            file, entries, (DWORD)payloadBytes) &&
            SpreaderPersistenceHash(entries, payloadBytes) ==
                header.payloadHash;
    }
    CloseHandle(file);

    size_t accepted = 0;
    if (valid)
    {
        AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
        for (DWORD i = 0; i < header.count; ++i)
        {
            SpreaderPriorityDiskEntry& source = entries[i];
            if (source.materialCount > SPREADER_MAX_MATERIALS)
                continue;
            bool entryValid = true;
            for (DWORD material = 0;
                 material < source.materialCount; ++material)
            {
                if (!memchr(source.materials[material].resourceName,
                            0,
                            sizeof(source.materials[material].resourceName)) ||
                    source.materials[material].priority < 0 ||
                    source.materials[material].priority >
                        (LONG)SPREADER_MAX_MATERIALS)
                {
                    entryValid = false;
                    break;
                }
            }
            if (!entryValid) continue;
            SpreaderPersistedPriorityState& target =
                g_spreaderPersistedPriorities[accepted++];
            target.valid = 1;
            target.claimedBuilding = nullptr;
            target.disk = source;
        }
        ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);
    }
    if (entries) HeapFree(GetProcessHeap(), 0, entries);
    InterlockedExchange(&g_spreaderPriorityPersistenceLoadSeen, 1);
    InterlockedIncrement64(&g_spreaderPriorityPersistenceLoads);

    Report(valid ? "INFO" : "WARN",
        "Priority persistence", "load",
        "native_folder=%s resolved_folder=%s file=%s status=%s file_entries=%lu accepted_entries=%llu restore_match=position_within_50mm_plus_resource_name defaults_when_unmatched=1",
        folder, resolvedFolder, SPREADER_PRIORITY_FILE_NAME,
        valid ? "loaded" : "invalid-sidecar-defaults-used",
        valid ? header.count : 0,
        (unsigned long long)accepted);
}

static bool SpreaderTankDiskEntryPlausible(
    const SpreaderTankDiskEntry& entry)
{
    if (entry.vehicleSlotIndex >= 4096 ||
        entry.enginePowerMilliKw <= 0 ||
        entry.enginePowerMilliKw > 5000000 ||
        entry.emptyWeightMilliKg <= 0 ||
        entry.emptyWeightMilliKg > 200000000 ||
        entry.savedCapacityMilliKg <= 0 ||
        entry.savedCapacityMilliKg > 20000000 ||
        entry.remainingMilliKg < 0 ||
        entry.remainingMilliKg > entry.savedCapacityMilliKg ||
        (entry.flags & ~SPREADER_TANK_KNOWN_FLAGS) != 0 ||
        !memchr(entry.resourceName, 0, sizeof(entry.resourceName)))
        return false;

    bool dry = (entry.flags & SPREADER_TANK_FLAG_DRY) != 0;
    bool refillReturn =
        (entry.flags & SPREADER_TANK_FLAG_REFILL_RETURN) != 0;
    if (entry.remainingMilliKg > 0 &&
        (dry || !entry.resourceName[0]))
        return false;
    if (refillReturn != (entry.returnReason == SAND_RETURN_REFILL))
        return false;
    if (!refillReturn && entry.returnReason != SAND_RETURN_NONE)
        return false;
    return true;
}

static void SpreaderLoadTankSidecar(const char* folder)
{
    if (!g_sandTankPersistenceEnabled || !folder || !folder[0])
        return;

    AcquireSRWLockExclusive(&g_spreaderTankPersistenceLock);
    memset(g_spreaderPersistedTanks, 0,
           sizeof(g_spreaderPersistedTanks));
    g_spreaderTankWorldFolder[0] = 0;
    ReleaseSRWLockExclusive(&g_spreaderTankPersistenceLock);
    InterlockedExchange(&g_spreaderTankPersistenceLoadSeen, 0);

    char resolvedFolder[MAX_PATH * 2] = {};
    char path[MAX_PATH * 2] = {};
    if (!SpreaderResolveWorldFolder(
            folder, resolvedFolder, sizeof(resolvedFolder)) ||
        !SpreaderTankPath(resolvedFolder, "", path, sizeof(path)))
    {
        InterlockedExchange(&g_spreaderTankPersistenceLoadSeen, 1);
        Report("WARN", "Tank persistence", "load-path",
            "The native world folder could not be resolved; this save uses safe first-observation tank reconstruction (native_folder=%s)",
            folder);
        return;
    }

    AcquireSRWLockExclusive(&g_spreaderTankPersistenceLock);
    strncpy_s(g_spreaderTankWorldFolder,
              sizeof(g_spreaderTankWorldFolder),
              resolvedFolder, _TRUNCATE);
    ReleaseSRWLockExclusive(&g_spreaderTankPersistenceLock);

    HANDLE file = CreateFileA(
        path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        InterlockedExchange(&g_spreaderTankPersistenceLoadSeen, 1);
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        {
            Report("INFO", "Tank persistence", "load",
                "native_folder=%s resolved_folder=%s file=%s status=no-sidecar restored_entries=0 fallback=first-observation-reconstruction",
                folder, resolvedFolder, SPREADER_TANK_FILE_NAME);
        }
        else
        {
            ReportWindows("WARN", "Tank persistence", "load-open",
                "The save-specific tank sidecar could not be opened; safe first-observation reconstruction remains active",
                error, "Verify access to the savegame folder");
        }
        return;
    }

    SpreaderTankDiskHeader header = {};
    bool valid = SpreaderReadExact(
        file, &header, (DWORD)sizeof(header));
    if (valid)
    {
        valid = memcmp(header.magic, SPREADER_TANK_FILE_MAGIC, 8) == 0 &&
            header.version == SPREADER_TANK_FILE_VERSION &&
            header.headerSize == sizeof(header) &&
            header.entrySize == sizeof(SpreaderTankDiskEntry) &&
            header.count <= SPREADER_MAX_PERSISTED_TANK_STATES;
    }

    size_t payloadBytes = valid ?
        (size_t)header.count * sizeof(SpreaderTankDiskEntry) : 0;
    SpreaderTankDiskEntry* entries = nullptr;
    if (valid && payloadBytes)
    {
        entries = (SpreaderTankDiskEntry*)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, payloadBytes);
        valid = entries && SpreaderReadExact(
            file, entries, (DWORD)payloadBytes) &&
            SpreaderPersistenceHash(entries, payloadBytes) ==
                header.payloadHash;
    }
    CloseHandle(file);

    size_t accepted = 0;
    if (valid)
    {
        AcquireSRWLockExclusive(&g_spreaderTankPersistenceLock);
        for (DWORD i = 0; i < header.count; ++i)
        {
            if (!SpreaderTankDiskEntryPlausible(entries[i]))
                continue;
            SpreaderPersistedTankState& target =
                g_spreaderPersistedTanks[accepted++];
            target.valid = 1;
            target.claimedVehicle = nullptr;
            target.disk = entries[i];
        }
        ReleaseSRWLockExclusive(&g_spreaderTankPersistenceLock);
    }
    if (entries) HeapFree(GetProcessHeap(), 0, entries);
    InterlockedExchange(&g_spreaderTankPersistenceLoadSeen, 1);
    InterlockedIncrement64(&g_spreaderTankPersistenceLoads);

    Report(valid ? "INFO" : "WARN", "Tank persistence", "load",
        "native_folder=%s resolved_folder=%s file=%s status=%s file_entries=%lu accepted_entries=%llu identity=home-position-within-50mm-plus-vehicle-slot-plus-power-weight fallback_on_mismatch=first-observation-reconstruction depot_debit_on_restore=0",
        folder, resolvedFolder, SPREADER_TANK_FILE_NAME,
        valid ? "loaded" : "invalid-sidecar-fallback-used",
        valid ? header.count : 0,
        (unsigned long long)accepted);
}

static bool SpreaderApplyPersistedPrioritiesLocked(
    SpreaderDepotPriorityState* state, DWORD presentMask)
{
    if (!state || !state->building ||
        !g_depotMaterialPriorityPersistenceEnabled ||
        !InterlockedCompareExchange(
            &g_spreaderPriorityPersistenceLoadSeen, 0, 0))
        return false;

    LONG position[3] = {};
    if (!SpreaderPositionMillimetres(state->building, position))
        return false;

    SpreaderPersistedPriorityState* record = nullptr;
    for (size_t i = 0;
         i < SPREADER_MAX_PERSISTED_PRIORITY_STATES; ++i)
    {
        SpreaderPersistedPriorityState& candidate =
            g_spreaderPersistedPriorities[i];
        if (!candidate.valid ||
            (candidate.claimedBuilding &&
             candidate.claimedBuilding != state->building) ||
            !SpreaderPositionsMatch(
                candidate.disk.positionMillimetres, position))
            continue;
        record = &candidate;
        break;
    }
    if (!record) return false;

    memset(state->priorities, 0, sizeof(state->priorities));
    const int availableCount = SpreaderCountMaterialBits(presentMask);
    bool used[SPREADER_MAX_MATERIALS + 1] = {};
    DWORD restoredMask = 0;
    for (DWORD diskIndex = 0;
         diskIndex < record->disk.materialCount; ++diskIndex)
    {
        SpreaderPriorityDiskMaterial& saved =
            record->disk.materials[diskIndex];
        int materialIndex = FindSpreaderMaterialIndex(saved.resourceName);
        if (materialIndex < 0) continue;
        DWORD bit = (DWORD)(1u << (unsigned)materialIndex);
        if (!(presentMask & bit)) continue;
        int priority = (int)saved.priority;
        if (priority > availableCount ||
            (priority > 0 && used[priority]))
            continue;
        state->priorities[materialIndex] = priority;
        restoredMask |= bit;
        if (priority > 0) used[priority] = true;
    }

    for (size_t materialIndex = 0;
         materialIndex < g_spreaderMaterialCount; ++materialIndex)
    {
        DWORD bit = (DWORD)(1u << (unsigned)materialIndex);
        if (!(presentMask & bit) || (restoredMask & bit)) continue;
        for (int priority = 1;
             priority <= availableCount; ++priority)
        {
            if (used[priority]) continue;
            state->priorities[materialIndex] = priority;
            used[priority] = true;
            break;
        }
    }

    record->claimedBuilding = state->building;
    InterlockedIncrement64(&g_spreaderPriorityPersistenceRestores);
    Report("INFO", "Priority persistence", "restore",
        "building=%p position_mm=[%ld,%ld,%ld] saved_materials=%lu present_materials=%d restored_mask=0x%08lX unmatched_materials_use_defaults=1",
        state->building, position[0], position[1], position[2],
        record->disk.materialCount, availableCount,
        (unsigned long)restoredMask);
    return true;
}

static void SpreaderDeletePriorityLifetime(
    BYTE* building, const LONG knownPosition[3],
    const char* reason)
{
    if (!building) return;
    LONG position[3] = {};
    bool positionValid = knownPosition != nullptr;
    if (positionValid)
        memcpy(position, knownPosition, sizeof(position));
    else
        positionValid = SpreaderPositionMillimetres(building, position);

    size_t runtimeRemoved = 0;
    size_t persistedRemoved = 0;
    AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
    for (size_t i = 0; i < SPREADER_MAX_DEPOT_PRIORITY_STATES; ++i)
    {
        if (g_spreaderDepotPriorities[i].building == building)
        {
            memset(&g_spreaderDepotPriorities[i], 0,
                   sizeof(g_spreaderDepotPriorities[i]));
            ++runtimeRemoved;
        }
    }
    for (size_t i = 0;
         i < SPREADER_MAX_PERSISTED_PRIORITY_STATES; ++i)
    {
        SpreaderPersistedPriorityState& candidate =
            g_spreaderPersistedPriorities[i];
        if (!candidate.valid) continue;
        if (candidate.claimedBuilding != building &&
            (!positionValid || !SpreaderPositionsMatch(
                candidate.disk.positionMillimetres, position)))
            continue;
        memset(&candidate, 0, sizeof(candidate));
        ++persistedRemoved;
    }
    ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);
    InterlockedIncrement64(&g_spreaderPriorityPersistenceDeletes);
    Report("INFO", "Priority persistence", "lifetime-delete",
        "building=%p reason=%s position_valid=%d position_mm=[%ld,%ld,%ld] runtime_states_removed=%llu loaded_save_records_removed=%llu disk_update=next-game-save collapse_and_demolition_equal=1",
        building, reason ? reason : "terminal",
        positionValid ? 1 : 0,
        position[0], position[1], position[2],
        (unsigned long long)runtimeRemoved,
        (unsigned long long)persistedRemoved);
}

static int SpreaderCountMaterialBits(DWORD mask)
{
    int count = 0;
    while (mask)
    {
        count += (int)(mask & 1u);
        mask >>= 1;
    }
    return count;
}

static void SpreaderInitializeDefaultPrioritiesLocked(
    SpreaderDepotPriorityState* state,
    BYTE* building, DWORD presentMask)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->building = building;
    int priority = 1;
    for (size_t i = 0; i < g_spreaderMaterialCount; ++i)
    {
        DWORD bit = (DWORD)(1u << (unsigned)i);
        if (presentMask & bit)
            state->priorities[i] = priority++;
    }
    state->presentMask = presentMask;
    state->knownMask = presentMask;
    state->lastSeenTick = GetTickCount64();
}

static SpreaderDepotPriorityState*
SpreaderFindOrCreateDepotPriorityLocked(BYTE* building, DWORD presentMask)
{
    if (!building) return nullptr;

    SpreaderDepotPriorityState* selected = nullptr;
    SpreaderDepotPriorityState* empty = nullptr;
    SpreaderDepotPriorityState* oldest = &g_spreaderDepotPriorities[0];
    for (size_t i = 0; i < SPREADER_MAX_DEPOT_PRIORITY_STATES; ++i)
    {
        SpreaderDepotPriorityState* state =
            &g_spreaderDepotPriorities[i];
        if (state->building == building)
        {
            selected = state;
            break;
        }
        if (!state->building && !empty) empty = state;
        if (state->lastSeenTick < oldest->lastSeenTick) oldest = state;
    }

    if (!selected)
    {
        selected = empty ? empty : oldest;
        SpreaderInitializeDefaultPrioritiesLocked(
            selected, building, presentMask);
        SpreaderApplyPersistedPrioritiesLocked(selected, presentMask);
    }
    else if (selected->presentMask != presentMask)
    {
        const int availableCount =
            SpreaderCountMaterialBits(presentMask);
        bool used[SPREADER_MAX_MATERIALS + 1] = {};
        DWORD previousKnown = selected->knownMask;
        bool needsAssignment[SPREADER_MAX_MATERIALS] = {};

        for (size_t i = 0; i < g_spreaderMaterialCount; ++i)
        {
            DWORD bit = (DWORD)(1u << (unsigned)i);
            if (!(presentMask & bit))
            {
                selected->priorities[i] = 0;
                continue;
            }

            int priority = selected->priorities[i];
            bool newlyPresent = (previousKnown & bit) == 0;
            if (newlyPresent || priority > availableCount ||
                (priority > 0 && used[priority]))
            {
                // A newly added material is enabled automatically. An
                // existing material whose number became invalid after the
                // depot layout changed stays enabled and receives a free one.
                needsAssignment[i] = newlyPresent || priority > 0;
                selected->priorities[i] = 0;
            }
            else if (priority > 0)
            {
                used[priority] = true;
            }
        }

        for (size_t i = 0; i < g_spreaderMaterialCount; ++i)
        {
            if (!needsAssignment[i]) continue;
            for (int priority = 1;
                 priority <= availableCount; ++priority)
            {
                if (used[priority]) continue;
                selected->priorities[i] = priority;
                used[priority] = true;
                break;
            }
        }
        selected->knownMask = presentMask;
    }

    selected->presentMask = presentMask;
    selected->lastSeenTick = GetTickCount64();
    return selected;
}

static void SpreaderResetDepotPriorityDefaults(
    BYTE* building, DWORD presentMask,
    const char* reason)
{
    if (!building) return;
    AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
    SpreaderDepotPriorityState* selected = nullptr;
    SpreaderDepotPriorityState* empty = nullptr;
    SpreaderDepotPriorityState* oldest = &g_spreaderDepotPriorities[0];
    for (size_t i = 0; i < SPREADER_MAX_DEPOT_PRIORITY_STATES; ++i)
    {
        SpreaderDepotPriorityState* state =
            &g_spreaderDepotPriorities[i];
        if (state->building == building)
        {
            selected = state;
            break;
        }
        if (!state->building && !empty) empty = state;
        if (state->lastSeenTick < oldest->lastSeenTick) oldest = state;
    }
    if (!selected) selected = empty ? empty : oldest;
    SpreaderInitializeDefaultPrioritiesLocked(
        selected, building, presentMask);
    ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);
    InterlockedIncrement64(&g_spreaderPriorityLifetimeResets);
    Report("INFO", "Priority persistence", "lifetime-defaults",
        "building=%p reason=%s present_mask=0x%08lX available_priorities=%d source=grit_materials-order persisted_restore_suppressed=1",
        building, reason ? reason : "new-lifetime",
        (unsigned long)presentMask,
        SpreaderCountMaterialBits(presentMask));
}

static void SpreaderGetDepotPrioritySnapshot(
    BYTE* building, DWORD presentMask,
    int priorities[SPREADER_MAX_MATERIALS], int* availableCountOut)
{
    if (priorities)
        memset(priorities, 0,
               sizeof(int) * SPREADER_MAX_MATERIALS);
    if (availableCountOut)
        *availableCountOut = SpreaderCountMaterialBits(presentMask);
    if (!building || !priorities) return;

    AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
    SpreaderDepotPriorityState* state =
        SpreaderFindOrCreateDepotPriorityLocked(building, presentMask);
    if (state)
        memcpy(priorities, state->priorities,
               sizeof(state->priorities));
    ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);
}

static bool SpreaderCycleDepotPriority(
    BYTE* building, DWORD presentMask, int materialIndex,
    int* previousOut, int* currentOut, int* swappedMaterialOut,
    int* availableCountOut)
{
    if (previousOut) *previousOut = 0;
    if (currentOut) *currentOut = 0;
    if (swappedMaterialOut) *swappedMaterialOut = -1;
    int availableCount = SpreaderCountMaterialBits(presentMask);
    if (availableCountOut) *availableCountOut = availableCount;
    if (!building || materialIndex < 0 ||
        (size_t)materialIndex >= g_spreaderMaterialCount ||
        availableCount < 1 ||
        !(presentMask & (DWORD)(1u << (unsigned)materialIndex)))
        return false;

    bool changed = false;
    AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
    SpreaderDepotPriorityState* state =
        SpreaderFindOrCreateDepotPriorityLocked(building, presentMask);
    if (state)
    {
        ULONGLONG now = GetTickCount64();
        ULONGLONG last = state->lastClickTick[materialIndex];
        if (!last || now < last || now - last >= 200)
        {
            int previous = state->priorities[materialIndex];
            // QoL order: an OFF material enters at the lowest priority and
            // moves deliberately toward priority 1. Priority 1 therefore is
            // not displaced merely while the player is clicking toward OFF.
            // Examples: OFF -> 2 -> 1 -> OFF, or OFF -> 3 -> 2 -> 1 -> OFF.
            int next = previous == 0 ? availableCount :
                       (previous > 1 ? previous - 1 : 0);
            int swapped = -1;
            if (next > 0)
            {
                for (size_t i = 0; i < g_spreaderMaterialCount; ++i)
                {
                    if ((int)i != materialIndex &&
                        state->priorities[i] == next)
                    {
                        swapped = (int)i;
                        state->priorities[i] = previous;
                        break;
                    }
                }
            }
            state->priorities[materialIndex] = next;
            state->lastClickTick[materialIndex] = now;
            if (previousOut) *previousOut = previous;
            if (currentOut) *currentOut = next;
            if (swappedMaterialOut) *swappedMaterialOut = swapped;
            changed = true;
        }
    }
    ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);
    return changed;
}

static void SpreaderSavePrioritySidecar(const char* folder)
{
    if (!InterlockedCompareExchange(
            &g_spreaderPriorityPersistenceActive, 0, 0) ||
        !g_depotMaterialPriorityPersistenceEnabled ||
        !folder || !folder[0])
        return;

    const size_t allocationBytes =
        SPREADER_MAX_PERSISTED_PRIORITY_STATES *
        sizeof(SpreaderPriorityDiskEntry);
    SpreaderPriorityDiskEntry* entries =
        (SpreaderPriorityDiskEntry*)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, allocationBytes);
    if (!entries)
    {
        Report("WARN", "Priority persistence", "save-memory",
            "The bounded priority snapshot could not be allocated; the game save continues without updating the sidecar");
        return;
    }

    size_t count = 0;
    size_t skippedTerminal = 0;
    AcquireSRWLockShared(&g_spreaderDepotPriorityLock);
    for (size_t stateIndex = 0;
         stateIndex < SPREADER_MAX_DEPOT_PRIORITY_STATES &&
         count < SPREADER_MAX_PERSISTED_PRIORITY_STATES;
         ++stateIndex)
    {
        const SpreaderDepotPriorityState& state =
            g_spreaderDepotPriorities[stateIndex];
        BYTE* building = state.building;
        if (!building || !state.presentMask) continue;

        bool validBuilding = false;
        bool terminal = false;
        __try
        {
            validBuilding =
                ReadTechnicalType(building) == BUILDING_GARBAGE_OFFICE &&
                ReadablePtr(
                    building + SAND_BUILDING_GOING_AWAY,
                    sizeof(BYTE));
            terminal = validBuilding &&
                *(BYTE*)(building + SAND_BUILDING_GOING_AWAY) != 0;
        }
        __except(FaultFilter(
            "priority sidecar building snapshot",
            GetExceptionInformation()))
        {
            validBuilding = false;
        }
        if (!validBuilding || terminal)
        {
            if (terminal) ++skippedTerminal;
            continue;
        }

        SpreaderPriorityDiskEntry& entry = entries[count];
        if (!SpreaderPositionMillimetres(
                building, entry.positionMillimetres))
            continue;

        for (size_t materialIndex = 0;
             materialIndex < g_spreaderMaterialCount; ++materialIndex)
        {
            DWORD bit = (DWORD)(1u << (unsigned)materialIndex);
            if (!(state.presentMask & bit)) continue;
            SpreaderPriorityDiskMaterial& material =
                entry.materials[entry.materialCount++];
            strncpy_s(material.resourceName,
                      sizeof(material.resourceName),
                      g_spreaderMaterials[materialIndex].resourceName,
                      _TRUNCATE);
            material.priority = state.priorities[materialIndex];
        }
        if (entry.materialCount) ++count;
    }
    ReleaseSRWLockShared(&g_spreaderDepotPriorityLock);

    char resolvedFolder[MAX_PATH * 2] = {};
    char path[MAX_PATH * 2] = {};
    char temporaryPath[MAX_PATH * 2] = {};
    if (!SpreaderResolveWorldFolder(
            folder, resolvedFolder, sizeof(resolvedFolder)) ||
        !SpreaderPriorityPath(
            resolvedFolder, "", path, sizeof(path)) ||
        !SpreaderPriorityPath(
            resolvedFolder, ".tmp", temporaryPath,
            sizeof(temporaryPath)))
    {
        HeapFree(GetProcessHeap(), 0, entries);
        Report("WARN", "Priority persistence", "save-path",
            "The native destination folder could not be resolved to the media_soviet save directory; the game save continues without updating the priority sidecar (native_folder=%s)",
            folder);
        return;
    }

    SpreaderPriorityDiskHeader header = {};
    memcpy(header.magic, SPREADER_PRIORITY_FILE_MAGIC, 8);
    header.version = SPREADER_PRIORITY_FILE_VERSION;
    header.headerSize = (DWORD)sizeof(header);
    header.entrySize = (DWORD)sizeof(SpreaderPriorityDiskEntry);
    header.count = (DWORD)count;
    size_t payloadBytes = count * sizeof(SpreaderPriorityDiskEntry);
    header.payloadHash = SpreaderPersistenceHash(entries, payloadBytes);

    HANDLE file = CreateFileA(
        temporaryPath, GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool saved = file != INVALID_HANDLE_VALUE;
    DWORD error = saved ? ERROR_SUCCESS : GetLastError();
    if (saved)
    {
        saved = SpreaderWriteExact(
            file, &header, (DWORD)sizeof(header)) &&
            (!payloadBytes || SpreaderWriteExact(
                file, entries, (DWORD)payloadBytes)) &&
            FlushFileBuffers(file) != 0;
        if (!saved) error = GetLastError();
        CloseHandle(file);
    }
    if (saved)
    {
        saved = MoveFileExA(
            temporaryPath, path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!saved) error = GetLastError();
    }
    if (!saved) DeleteFileA(temporaryPath);
    HeapFree(GetProcessHeap(), 0, entries);

    if (!saved)
    {
        ReportWindows("WARN", "Priority persistence", "save",
            "The atomic save-specific priority sidecar update failed; the game save itself is unaffected",
            error, "Verify write access and free space in the savegame folder");
        return;
    }

    AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
    strncpy_s(g_spreaderPriorityWorldFolder,
              sizeof(g_spreaderPriorityWorldFolder),
              resolvedFolder, _TRUNCATE);
    ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);
    InterlockedIncrement64(&g_spreaderPriorityPersistenceSaves);
    Report("INFO", "Priority persistence", "save",
        "native_folder=%s resolved_folder=%s file=%s entries=%llu terminal_entries_skipped=%llu bytes=%llu atomic_replace=1 save_specific=1",
        folder, resolvedFolder, SPREADER_PRIORITY_FILE_NAME,
        (unsigned long long)count,
        (unsigned long long)skippedTerminal,
        (unsigned long long)(sizeof(header) + payloadBytes));
}

static const char* SpreaderWorldFolderFromResourceMap(
    const char* path, char* out, size_t outSize)
{
    if (!path || !out || outSize < 2) return nullptr;
    size_t length = strlen(path);
    static const char tail[] = "/resourcemap.dds";
    const size_t tailLength = sizeof(tail) - 1;
    if (length <= tailLength) return nullptr;
    const char* at = path + length - tailLength;
    if (_stricmp(at, tail) != 0 &&
        _stricmp(at, "\\resourcemap.dds") != 0)
        return nullptr;
    size_t keep = (size_t)(at - path);
    if (!keep || keep >= outSize) return nullptr;
    memcpy(out, path, keep);
    out[keep] = 0;
    return out;
}

static void* __fastcall SpreaderHookCreateManagedTexture(
    void* middlepoint, const char* path)
{
    void* result = g_spreaderOriginalCreateManagedTexture ?
        g_spreaderOriginalCreateManagedTexture(
            middlepoint, path) : nullptr;
    if (InterlockedCompareExchange(
            &g_spreaderPriorityPersistenceActive, 0, 0))
    {
        char folder[MAX_PATH] = {};
        if (SpreaderWorldFolderFromResourceMap(
                path, folder, sizeof(folder)))
        {
            if (g_depotMaterialPriorityPersistenceEnabled)
            {
                __try
                {
                    SpreaderLoadPrioritySidecar(folder);
                }
                __except(FaultFilter(
                    "priority sidecar world-load hook",
                    GetExceptionInformation()))
                {
                    Report("WARN", "Priority persistence", "load-fault",
                        "The guarded priority sidecar load faulted; unmatched depots use catalogue defaults");
                }
            }
            if (g_sandTankPersistenceEnabled)
            {
                __try
                {
                    SpreaderLoadTankSidecar(folder);
                }
                __except(FaultFilter(
                    "tank sidecar world-load hook",
                    GetExceptionInformation()))
                {
                    Report("WARN", "Tank persistence", "load-fault",
                        "The guarded tank sidecar load faulted; vehicles use safe first-observation reconstruction");
                }
            }
        }
    }
    return result;
}

static void SpreaderHookWorldSave(
    void* self, const char* folder)
{
    if (g_spreaderOriginalWorldSave)
        g_spreaderOriginalWorldSave(self, folder);
    if (!InterlockedCompareExchange(
            &g_spreaderPriorityPersistenceActive, 0, 0))
        return;
    if (g_depotMaterialPriorityPersistenceEnabled)
    {
        __try
        {
            SpreaderSavePrioritySidecar(folder);
        }
        __except(FaultFilter(
            "priority sidecar world-save hook",
            GetExceptionInformation()))
        {
            Report("WARN", "Priority persistence", "save-fault",
                "The guarded priority sidecar save faulted; the game save itself is unaffected");
        }
    }
    if (g_sandTankPersistenceEnabled)
    {
        __try
        {
            SpreaderSaveTankSidecar(folder);
        }
        __except(FaultFilter(
            "tank sidecar world-save hook",
            GetExceptionInformation()))
        {
            Report("WARN", "Tank persistence", "save-fault",
                "The guarded tank sidecar save faulted; the game save itself is unaffected");
        }
    }
}

static bool SpreaderPatchWorldSaveCall()
{
    BYTE* site = g_exeBase + RVA_SPREADER_WORLD_SAVE_CALL;
    if (!ReadablePtr(site - 3, 8) ||
        site[-3] != 0x49 || site[-2] != 0x8B ||
        site[-1] != 0xD7 || site[0] != 0xE8)
    {
        Report("WARN", "Priority persistence", "save-hook",
            "The verified world-save call no longer has mov rdx,r15 followed by call; persistence is disabled safely");
        return false;
    }

    int relative = 0;
    memcpy(&relative, site + 1, sizeof(relative));
    BYTE* previousTarget = site + 5 + relative;
    if (!ReadablePtr(previousTarget, 1) ||
        !ReadablePtr(g_exeBase + RVA_SPREADER_WORLD_SAVE, 16))
    {
        Report("WARN", "Priority persistence", "save-hook",
            "The current world-save target or verified native saver is unreadable; persistence is disabled safely");
        return false;
    }

    BYTE* cave = AllocNear(site, 16);
    if (!cave) return false;
    cave[0] = 0xFF;
    cave[1] = 0x25;
    memset(cave + 2, 0, 4);
    void* detour = (void*)&SpreaderHookWorldSave;
    memcpy(cave + 6, &detour, sizeof(detour));
    INT64 displacement64 = cave - (site + 5);
    int displacement = (int)displacement64;
    if (displacement64 != displacement) return false;

    if (!FlushCodeChecked(cave,14,"save-hook-stub")) return false;
    DWORD protection = 0;
    if (!MakeCodeWritable(site,5,&protection,"save-hook")) return false;
    // Publish the original target before making the new branch reachable.
    g_spreaderOriginalWorldSave = (SpreaderWorldSaveFn)previousTarget;
    memcpy(site + 1, &displacement, sizeof(displacement));
    FinishCodePatch(site,5,protection,"save-hook");
    Report("INFO", "Priority persistence", "save-hook",
        "active=1 call=exe+0x%X previous_target=%p native_target=exe+0x%X chained_existing_hook=%d folder_argument=rdx atomic_sidecar=1",
        RVA_SPREADER_WORLD_SAVE_CALL, previousTarget,
        RVA_SPREADER_WORLD_SAVE,
        previousTarget != g_exeBase + RVA_SPREADER_WORLD_SAVE ? 1 : 0);
    return true;
}

static bool SpreaderInstallPriorityPersistenceHooks()
{
    if (!g_depotMaterialPriorityPersistenceEnabled &&
        !g_sandTankPersistenceEnabled)
        return false;
    if (!InterlockedCompareExchange(
            &g_sandBuildingLifecycleLayoutVerified, 0, 0))
    {
        Report("WARN", "Persistence", "startup",
            "The verified building position/lifecycle layout is unavailable; priority persistence remains session-only and tank persistence falls back to first-observation reconstruction");
        return false;
    }
    if (!SpreaderPatchWorldSaveCall()) return false;

    void* original = nullptr;
    if (!H->patchIat(
            H->exeModule, "C3DDLL64.dll",
            SPREADER_CREATE_MANAGED_TEXTURE_SYMBOL,
            (void*)&SpreaderHookCreateManagedTexture,
            &original,
            "technical service priority world-folder load"))
    {
        Report("WARN", "Priority persistence", "load-hook",
            "The resource-map world-folder import could not be chained; the installed save hook remains a pass-through and persistence stays disabled");
        return false;
    }
    g_spreaderOriginalCreateManagedTexture =
        (SpreaderCreateManagedTextureFn)original;
    InterlockedExchange(&g_spreaderPriorityPersistenceActive, 1);
    Report("INFO", "Persistence", "startup",
        "active=1 priority_file=%s priority_format_version=%lu priority_max_entries=%llu tank_file=%s tank_format_version=%lu tank_max_entries=%llu tank_identity=home-position-within-50mm-plus-vehicle-slot-plus-power-weight save_hook=verified-world-folder-call load_hook=resourcemap-path-IAT collapse_delete=going-away-rising-edge rebuild_defaults=1 pointer_reuse_new_lifetime=1",
        SPREADER_PRIORITY_FILE_NAME,
        SPREADER_PRIORITY_FILE_VERSION,
        (unsigned long long)SPREADER_MAX_PERSISTED_PRIORITY_STATES,
        SPREADER_TANK_FILE_NAME,
        SPREADER_TANK_FILE_VERSION,
        (unsigned long long)SPREADER_MAX_PERSISTED_TANK_STATES);
    return true;
}

static DWORD SpreaderObservePresentMaterialMask(BYTE* building)
{
    DWORD presentMask = 0;
    if (!building || !ReadablePtr(building + B_STORAGE_BEGIN, 16))
        return presentMask;
    BYTE* begin = *(BYTE**)(building + B_STORAGE_BEGIN);
    BYTE* end = *(BYTE**)(building + B_STORAGE_END);
    if (!begin || !end || end < begin) return presentMask;
    size_t bytes = (size_t)(end - begin);
    if ((bytes % STORAGE_SIZE) != 0) return presentMask;
    size_t count = bytes / STORAGE_SIZE;
    if (count > MAX_STORAGES || (bytes && !ReadablePtr(begin, bytes)))
        return presentMask;

    for (size_t storageIndex = 0; storageIndex < count; ++storageIndex)
    {
        BYTE* storage = begin + storageIndex * STORAGE_SIZE;
        BYTE* slots = nullptr;
        size_t slotCount = 0;
        if (!GetStorageSlots(storage, &slots, &slotCount)) continue;
        for (size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
        {
            void* resource = *(void**)(
                slots + slotIndex * SLOT_SIZE + SLOT_RESOURCE);
            char name[64];
            ResourceName(resource, name, sizeof(name));
            int materialIndex = FindSpreaderMaterialIndex(name);
            if (materialIndex >= 0)
                presentMask |=
                    (DWORD)(1u << (unsigned)materialIndex);
        }
    }
    return presentMask;
}

// Selects the stocked material with the lowest enabled priority in this
// depot. Disabled materials are ignored. If all enabled materials are empty,
// the highest-priority matching empty slot is returned so a zero-load tank can
// retain its material identity in the vehicle window. If every material is
// OFF, no storage is returned and the snowplow continues without grit.
static SpreaderStorageObservation SpreaderObserveStorage(BYTE* building)
{
    SpreaderStorageObservation result = {};
    if (!building || !ReadablePtr(building + B_STORAGE_BEGIN, 16))
        return result;
    BYTE* begin = *(BYTE**)(building + B_STORAGE_BEGIN);
    BYTE* end = *(BYTE**)(building + B_STORAGE_END);
    if (!begin || !end || end < begin) return result;

    size_t bytes = (size_t)(end - begin);
    if ((bytes % STORAGE_SIZE) != 0) return result;
    size_t count = bytes / STORAGE_SIZE;
    if (count > MAX_STORAGES || (bytes && !ReadablePtr(begin, bytes)))
        return result;

    SpreaderStorageObservation
        candidates[SPREADER_MAX_MATERIALS] = {};
    DWORD presentMask = 0;
    for (size_t storageIndex = 0; storageIndex < count; ++storageIndex)
    {
        BYTE* storage = begin + storageIndex * STORAGE_SIZE;
        BYTE* slots = nullptr;
        size_t slotCount = 0;
        if (!GetStorageSlots(storage, &slots, &slotCount)) continue;
        for (size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
        {
            BYTE* slot = slots + slotIndex * SLOT_SIZE;
            void* resource = *(void**)(slot + SLOT_RESOURCE);
            char name[64];
            ResourceName(resource, name, sizeof(name));
            int materialIndex = FindSpreaderMaterialIndex(name);
            const SpreaderMaterialDefinition* material =
                SpreaderMaterialAt(materialIndex);
            if (!material) continue;
            float amount = *(float*)(slot + SLOT_AMOUNT);
            float capacity = *(float*)(storage + STORAGE_CAPACITY);
            if (!IsFiniteNonNegative(amount) ||
                !IsFiniteNonNegative(capacity))
                continue;

            presentMask |=
                (DWORD)(1u << (unsigned)materialIndex);
            SpreaderStorageObservation candidate = {};
            candidate.found = 1;
            candidate.materialIndex = materialIndex;
            candidate.protectionStrength =
                material->protectionStrength;
            candidate.storageIndex = storageIndex;
            candidate.slotIndex = slotIndex;
            candidate.storage = storage;
            candidate.slot = slot;
            candidate.resource = resource;
            if (resource &&
                ReadablePtr((BYTE*)resource + RESOURCE_TEXT_ID,
                            sizeof(int)))
                candidate.resourceTextId =
                    *(int*)((BYTE*)resource + RESOURCE_TEXT_ID);
            strncpy_s(candidate.resourceName,
                      sizeof(candidate.resourceName),
                      name, _TRUNCATE);
            candidate.amount = amount;
            candidate.capacity = capacity;
            candidate.transportClass =
                *(int*)(storage + STORAGE_CLASS);

            SpreaderStorageObservation* retained =
                &candidates[materialIndex];
            if (!retained->found ||
                (candidate.amount > retained->amount))
                *retained = candidate;
        }
    }

    int priorities[SPREADER_MAX_MATERIALS] = {};
    int availableCount = 0;
    SpreaderGetDepotPrioritySnapshot(
        building, presentMask, priorities, &availableCount);
    (void)availableCount;
    int bestStockedPriority = INT_MAX;
    int bestEmptyPriority = INT_MAX;
    SpreaderStorageObservation emptyFallback = {};
    for (size_t i = 0; i < g_spreaderMaterialCount; ++i)
    {
        if (!candidates[i].found || priorities[i] <= 0) continue;
        if (candidates[i].amount > 0.000001f &&
            priorities[i] < bestStockedPriority)
        {
            bestStockedPriority = priorities[i];
            result = candidates[i];
        }
        if (priorities[i] < bestEmptyPriority)
        {
            bestEmptyPriority = priorities[i];
            emptyFallback = candidates[i];
        }
    }
    return result.found ? result : emptyFallback;
}

static const char* SandConsumeStatusName(SandConsumeStatus status)
{
    switch (status)
    {
        case SAND_CONSUME_DISABLED:        return "disabled";
        case SAND_CONSUME_OK:              return "consumed";
        case SAND_CONSUME_DUPLICATE:       return "duplicate";
        case SAND_CONSUME_NOT_SNOWPLOW:    return "not-snowplow";
        case SAND_CONSUME_NO_HOME:         return "no-home-building";
        case SAND_CONSUME_WRONG_HOME_TYPE: return "home-not-technical-service";
        case SAND_CONSUME_NO_STORAGE:      return "material-storage-not-found";
        case SAND_CONSUME_EMPTY:           return "material-empty";
        case SAND_CONSUME_INVALID_STORAGE: return "material-storage-not-writable";
        case SAND_CONSUME_WRITE_RACE:      return "material-write-race";
        default:                           return "unknown";
    }
}

static bool SandWritableMemory(BYTE* address, size_t bytes)
{
    if (!address || bytes == 0 || !ReadablePtr(address, bytes))
        return false;

    MEMORY_BASIC_INFORMATION mbi = {};
    if (!VirtualQuery(address, &mbi, sizeof(mbi)) ||
        mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) ||
        (mbi.Protect & PAGE_NOACCESS))
        return false;

    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if ((uintptr_t)address > regionEnd ||
        bytes > regionEnd - (uintptr_t)address)
        return false;

    DWORD protection = mbi.Protect & 0xFF;
    return protection == PAGE_READWRITE ||
           protection == PAGE_WRITECOPY ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

static bool SandWritableFloat(BYTE* address)
{
    if (!address || ((size_t)address & (sizeof(LONG) - 1)) != 0)
        return false;
    return SandWritableMemory(address, sizeof(float));
}

static SandConsumeResult SpreaderConsumeObservedStorage(
    const SpreaderStorageObservation& material,
    float requestedKg)
{
    SandConsumeResult result = {};
    result.status = SAND_CONSUME_NO_STORAGE;
    result.requested = requestedKg / 1000.0f;
    if (!material.found)
        return result;
    result.storageIndex = material.storageIndex;
    result.slotIndex = material.slotIndex;

    BYTE* amountAddress = material.slot + SLOT_AMOUNT;
    if (!SandWritableFloat(amountAddress) ||
        !IsFiniteNonNegative(result.requested) || result.requested <= 0.0f)
    {
        result.status = SAND_CONSUME_INVALID_STORAGE;
        return result;
    }

    volatile LONG* amountBits = (volatile LONG*)amountAddress;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        LONG beforeBits = InterlockedCompareExchange(amountBits, 0, 0);
        float before = 0.0f;
        memcpy(&before, &beforeBits, sizeof(before));
        result.before = before;
        result.after = before;
        if (!IsFiniteNonNegative(before))
        {
            result.status = SAND_CONSUME_INVALID_STORAGE;
            return result;
        }
        if (before <= 0.000001f)
        {
            result.status = SAND_CONSUME_EMPTY;
            return result;
        }

        float consumed = before < result.requested ? before : result.requested;
        float after = before - consumed;
        if (after < 0.000001f) after = 0.0f;
        LONG afterBits = 0;
        memcpy(&afterBits, &after, sizeof(afterBits));
        if (InterlockedCompareExchange(amountBits, afterBits, beforeBits) ==
            beforeBits)
        {
            result.status = SAND_CONSUME_OK;
            result.consumed = consumed;
            result.after = after;
            return result;
        }
    }

    result.status = SAND_CONSUME_WRITE_RACE;
    return result;
}


static SandTrackedVehicle* SandFindTrackedVehicleLocked(void* vehicle,
                                                         bool create)
{
    SandTrackedVehicle* freeSlot = nullptr;
    SandTrackedVehicle* oldest = nullptr;
    for (size_t i = 0; i < (size_t)g_sandMaxVehicles; ++i)
    {
        SandTrackedVehicle* item = &g_sandTrackedVehicles[i];
        if (item->initialized && item->vehicle == vehicle) return item;
        if (!item->initialized && !freeSlot) freeSlot = item;
        if (item->initialized &&
            (!oldest || item->lastSeenTick < oldest->lastSeenTick))
            oldest = item;
    }
    if (!create) return nullptr;
    SandTrackedVehicle* item = freeSlot ? freeSlot : oldest;
    if (!item) return nullptr;
    memset(item, 0, sizeof(*item));
    item->vehicle = vehicle;
    item->initialized = 1;
    return item;
}

static bool SandIsDuplicateClearLocked(SandTrackedVehicle* item,
                                       void* road, void* selector,
                                       ULONGLONG now)
{
    if (!item || !road || !selector || g_sandDuplicateWindowMs <= 0)
        return false;

    for (size_t i = 0; i < item->recentClearKeyCount; ++i)
    {
        SandRecentClearKey* key = &item->recentClearKeys[i];
        if (key->road != road || key->selector != selector) continue;
        bool duplicate = now >= key->tick &&
            now - key->tick <= (ULONGLONG)g_sandDuplicateWindowMs;
        key->tick = now;
        return duplicate;
    }

    size_t slot = item->nextRecentClearKey;
    item->recentClearKeys[slot].road = road;
    item->recentClearKeys[slot].selector = selector;
    item->recentClearKeys[slot].tick = now;
    item->nextRecentClearKey =
        (slot + 1) % SAND_DIAG_RECENT_CLEAR_KEYS;
    if (item->recentClearKeyCount < SAND_DIAG_RECENT_CLEAR_KEYS)
        item->recentClearKeyCount++;
    return false;
}

static bool SandBuildRoadTreatment(
    void* vehicle, void* road, void* selector,
    const SandSkillView& skills, bool duplicate,
    const SandShadowTankUseResult& shadowTank,
    TsmGritRoadTreatment* out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!vehicle || !road || !selector ||
        !g_sandShadowTankEnabled || !skills.valid || !skills.snowplow)
        return false;

    if (duplicate)
    {
        bool found = false;
        AcquireSRWLockShared(&g_sandTrackerLock);
        SandTrackedVehicle* item =
            SandFindTrackedVehicleLocked(vehicle, false);
        if (item)
        {
            for (size_t i = 0; i < item->recentClearKeyCount; ++i)
            {
                const SandRecentClearKey& key = item->recentClearKeys[i];
                if (key.road == road && key.selector == selector &&
                    key.treatment.structSize ==
                        sizeof(TsmGritRoadTreatment))
                {
                    *out = key.treatment;
                    found = true;
                    break;
                }
            }
        }
        ReleaseSRWLockShared(&g_sandTrackerLock);
        if (!found) return false;
        out->flags |= TSM_GRIT_TREATMENT_DUPLICATE;
        out->vehicle = vehicle;
        out->road = road;
        out->selector = selector;
        return true;
    }

    out->structSize = sizeof(*out);
    out->vehicle = vehicle;
    out->road = road;
    out->selector = selector;
    out->materialIndex = shadowTank.materialIndex;
    out->resourceTextId = shadowTank.resourceTextId;
    out->consumedKg = shadowTank.consumedKg;
    strncpy_s(out->resourceName, sizeof(out->resourceName),
              shadowTank.resourceName, _TRUNCATE);

    bool materialApplied =
        shadowTank.status == SAND_SHADOW_TANK_CONSUMED &&
        (shadowTank.afterKg > 0.000001f ||
         shadowTank.consumedKg > 0.000001f);
    if (materialApplied)
    {
        float strength = shadowTank.protectionStrength;
        if (!(strength == strength) || strength < 0.0f) strength = 0.0f;
        if (strength > 1.0f) strength = 1.0f;
        out->protectionStrength = strength;
    }
    else
    {
        out->flags |= TSM_GRIT_TREATMENT_DRY;
        out->protectionStrength = 0.0f;
        out->consumedKg = 0.0f;
    }

    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item)
    {
        for (size_t i = 0; i < item->recentClearKeyCount; ++i)
        {
            SandRecentClearKey& key = item->recentClearKeys[i];
            if (key.road == road && key.selector == selector)
            {
                key.treatment = *out;
                break;
            }
        }
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);
    return true;
}

static void SandUpdateTrackedVehicle(void* vehicle,
                                     const SandSkillView& skills,
                                     void* currentBuilding,
                                     void* homeBuilding,
                                     bool seenInPanel,
                                     SandTrackUpdate* update)
{
    if (update) memset(update, 0, sizeof(*update));
    if (!vehicle) return;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item = SandFindTrackedVehicleLocked(vehicle, true);
    if (item)
    {
        bool firstSeen = item->lastSeenTick == 0;
        bool skillChanged = !firstSeen &&
            (item->skillValid != skills.valid ||
             item->snowplow != skills.snowplow ||
             item->skillCount != skills.count ||
             memcmp(item->skillIds, skills.ids, sizeof(item->skillIds)) != 0 ||
             memcmp(item->skillParameters, skills.parameterRaw,
                    sizeof(item->skillParameters)) != 0);
        bool buildingChanged = !firstSeen &&
            (item->currentBuilding != currentBuilding ||
             item->homeBuilding != homeBuilding);
        void* previousCurrentBuilding = item->currentBuilding;
        void* previousHomeBuilding = item->homeBuilding;
        int previousInsideHome =
            previousCurrentBuilding &&
            previousCurrentBuilding == previousHomeBuilding ? 1 : 0;
        int insideHome = currentBuilding &&
            currentBuilding == homeBuilding ? 1 : 0;

        item->typeDescription = skills.typeDescription;
        item->skillValid = skills.valid;
        item->snowplow = skills.snowplow;
        item->skillCount = skills.count;
        memcpy(item->skillIds, skills.ids, sizeof(item->skillIds));
        memcpy(item->skillParameters, skills.parameterRaw,
               sizeof(item->skillParameters));
        item->currentBuilding = currentBuilding;
        item->homeBuilding = homeBuilding;
        if (seenInPanel) item->seenInTechnicalPanel = 1;
        item->lastSeenTick = GetTickCount64();
        if (update)
        {
            update->firstSeen = firstSeen ? 1 : 0;
            update->skillChanged = skillChanged ? 1 : 0;
            update->buildingChanged = buildingChanged ? 1 : 0;
            update->seenInTechnicalPanel = item->seenInTechnicalPanel;
            update->previousCurrentBuilding = previousCurrentBuilding;
            update->previousHomeBuilding = previousHomeBuilding;
            update->currentBuilding = currentBuilding;
            update->homeBuilding = homeBuilding;
            update->previousInsideHome = previousInsideHome;
            update->insideHome = insideHome;
            update->clearCalls = item->clearCalls;
        }
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);
}

static const char* SandShadowTankStatusName(SandShadowTankStatus status)
{
    switch (status)
    {
        case SAND_SHADOW_TANK_DISABLED:      return "disabled";
        case SAND_SHADOW_TANK_CONSUMED:      return "consumed";
        case SAND_SHADOW_TANK_DUPLICATE:     return "duplicate";
        case SAND_SHADOW_TANK_NOT_SNOWPLOW:  return "not-snowplow";
        case SAND_SHADOW_TANK_UNINITIALIZED: return "uninitialized";
        case SAND_SHADOW_TANK_EMPTY:         return "empty";
        case SAND_SHADOW_TANK_INACTIVE_WORK: return "inactive-work";
        default:                             return "unknown";
    }
}

static bool SandCalculateShadowTankCapacity(
    BYTE* typeDescription,
    float* powerKwOut,
    float* emptyWeightKgOut,
    float* capacityKgOut)
{
    if (powerKwOut) *powerKwOut = 0.0f;
    if (emptyWeightKgOut) *emptyWeightKgOut = 0.0f;
    if (capacityKgOut) *capacityKgOut = 0.0f;
    if (!typeDescription ||
        !ReadablePtr(typeDescription + SAND_TYPE_ENGINE_POWER_KW,
                     sizeof(float)) ||
        !ReadablePtr(typeDescription + SAND_TYPE_EMPTY_WEIGHT_T,
                     sizeof(float)))
        return false;

    float powerKw =
        *(float*)(typeDescription + SAND_TYPE_ENGINE_POWER_KW);
    float emptyWeightT =
        *(float*)(typeDescription + SAND_TYPE_EMPTY_WEIGHT_T);
    float emptyWeightKg = emptyWeightT * 1000.0f;
    if (!IsFiniteNonNegative(powerKw) || powerKw <= 0.0f ||
        !IsFiniteNonNegative(emptyWeightKg) || emptyWeightKg <= 0.0f ||
        powerKw > 5000.0f || emptyWeightKg > 200000.0f)
        return false;

    float baseCapacityKg =
        emptyWeightKg * ((float)g_sandTankWeightPercent / 100.0f) +
        powerKw * (float)g_sandTankPowerKgPerKw;
    float rawCapacityKg = baseCapacityKg *
        ((float)g_sandTankCapacityMultiplierPercent / 100.0f);
    if (!IsFiniteNonNegative(rawCapacityKg)) return false;

    int wholeKg = (int)(rawCapacityKg + 0.5f);
    int stepKg = g_sandTankCapacityStepKg > 0
               ? g_sandTankCapacityStepKg : 1;
    int roundedKg =
        ((wholeKg + stepKg / 2) / stepKg) * stepKg;
    if (roundedKg < g_sandTankMinimumCapacityKg)
        roundedKg = g_sandTankMinimumCapacityKg;
    if (roundedKg > g_sandTankMaximumCapacityKg)
        roundedKg = g_sandTankMaximumCapacityKg;

    if (powerKwOut) *powerKwOut = powerKw;
    if (emptyWeightKgOut) *emptyWeightKgOut = emptyWeightKg;
    if (capacityKgOut) *capacityKgOut = (float)roundedKg;
    return true;
}

static LONG SpreaderTankRoundMilli(float value)
{
    if (!(value == value) || value <= 0.0f) return 0;
    double scaled = (double)value * 1000.0;
    if (scaled > 2147483647.0) return 2147483647L;
    return (LONG)(scaled + 0.5);
}

static void SandArmReturnLock(void* vehicle, int reason,
                              bool startPending,
                              void* finishRoad,
                              const char* source);

static bool SpreaderTryRestorePersistedTank(
    BYTE* building, size_t vehicleSlotIndex,
    BYTE* vehicle, const SandSkillView& skills,
    void* currentBuilding, void* homeBuilding,
    bool firstObservation)
{
    if (!g_sandTankPersistenceEnabled ||
        !building || !vehicle || homeBuilding != building ||
        vehicleSlotIndex >= 4096 ||
        !skills.valid || !skills.snowplow ||
        !InterlockedCompareExchange(
            &g_spreaderTankPersistenceLoadSeen, 0, 0))
        return false;

    // A road-clear callback can discover a vehicle before the periodic
    // building scan knows its depot slot. In that case the tracker exists but
    // its tank is deliberately still uninitialised. Permit the later indexed
    // scan to restore it; once a tank is live, never attempt a second restore.
    if (!firstObservation)
    {
        bool alreadyInitialized = false;
        AcquireSRWLockShared(&g_sandTrackerLock);
        SandTrackedVehicle* tracked =
            SandFindTrackedVehicleLocked(vehicle, false);
        alreadyInitialized = tracked && tracked->shadowTankInitialized;
        ReleaseSRWLockShared(&g_sandTrackerLock);
        if (alreadyInitialized) return false;
    }

    LONG position[3] = {};
    float powerKw = 0.0f;
    float emptyWeightKg = 0.0f;
    float currentCapacityKg = 0.0f;
    if (!SpreaderPositionMillimetres(building, position) ||
        !SandCalculateShadowTankCapacity(
            skills.typeDescription, &powerKw,
            &emptyWeightKg, &currentCapacityKg))
        return false;

    LONG powerMilli = SpreaderTankRoundMilli(powerKw);
    LONG weightMilli = SpreaderTankRoundMilli(emptyWeightKg);
    SpreaderTankDiskEntry saved = {};
    bool foundPositionAndSlot = false;
    bool matched = false;
    size_t matchedIndex = 0;

    AcquireSRWLockExclusive(&g_spreaderTankPersistenceLock);
    for (size_t i = 0;
         i < SPREADER_MAX_PERSISTED_TANK_STATES; ++i)
    {
        SpreaderPersistedTankState& candidate =
            g_spreaderPersistedTanks[i];
        if (!candidate.valid || candidate.claimedVehicle ||
            candidate.disk.vehicleSlotIndex != vehicleSlotIndex ||
            !SpreaderPositionsMatch(
                candidate.disk.homePositionMillimetres, position))
            continue;
        foundPositionAndSlot = true;
        if (candidate.disk.enginePowerMilliKw != powerMilli ||
            candidate.disk.emptyWeightMilliKg != weightMilli)
        {
            candidate.valid = 0;
            break;
        }
        saved = candidate.disk;
        candidate.claimedVehicle = vehicle;
        matched = true;
        matchedIndex = i;
        break;
    }
    ReleaseSRWLockExclusive(&g_spreaderTankPersistenceLock);

    if (!matched)
    {
        InterlockedIncrement64(&g_spreaderTankPersistenceFallbacks);
        Report(foundPositionAndSlot ? "WARN" : "INFO",
            "Tank persistence", "restore-fallback",
            "vehicle=%p home_building=%p vehicle_slot=%llu position_mm=[%ld,%ld,%ld] power_kw=%.3f empty_weight_kg=%.3f reason=%s action=first-observation-reconstruction depot_debit=0",
            vehicle, homeBuilding,
            (unsigned long long)vehicleSlotIndex,
            position[0], position[1], position[2],
            powerKw, emptyWeightKg,
            foundPositionAndSlot ?
                "vehicle-signature-mismatch" : "no-saved-record");
        return false;
    }

    int materialIndex = -1;
    float protectionStrength = 0.0f;
    int resourceTextId = 0;
    if (saved.resourceName[0])
    {
        materialIndex = FindSpreaderMaterialIndex(saved.resourceName);
        const SpreaderMaterialDefinition* material =
            SpreaderMaterialAt(materialIndex);
        if (material && material->runtimeValidationState == 0)
        {
            ValidateSpreaderMaterialCatalogRuntime(
                InterlockedCompareExchange(
                    &g_sandWorldGeneration, 0, 0));
            materialIndex = FindSpreaderMaterialIndex(saved.resourceName);
            material = SpreaderMaterialAt(materialIndex);
        }
        if (!material || material->runtimeValidationState != 1)
        {
            AcquireSRWLockExclusive(&g_spreaderTankPersistenceLock);
            g_spreaderPersistedTanks[matchedIndex].valid = 0;
            g_spreaderPersistedTanks[matchedIndex].claimedVehicle = nullptr;
            ReleaseSRWLockExclusive(&g_spreaderTankPersistenceLock);
            InterlockedIncrement64(&g_spreaderTankPersistenceFallbacks);
            Report("WARN", "Tank persistence", "restore-fallback",
                "vehicle=%p home_building=%p vehicle_slot=%llu saved_material=%s reason=material-not-configured-or-not-live action=first-observation-reconstruction depot_debit=0",
                vehicle, homeBuilding,
                (unsigned long long)vehicleSlotIndex,
                saved.resourceName);
            return false;
        }
        protectionStrength = material->protectionStrength;
        resourceTextId = SpreaderRuntimeResourceTextId(material);
    }

    float savedRemainingKg =
        (float)saved.remainingMilliKg / 1000.0f;
    float savedCapacityKg =
        (float)saved.savedCapacityMilliKg / 1000.0f;
    float remainingKg = savedRemainingKg;
    if (remainingKg > currentCapacityKg)
        remainingKg = currentCapacityKg;
    if (remainingKg < 0.000001f) remainingKg = 0.0f;
    bool insideHome = currentBuilding &&
        currentBuilding == homeBuilding;
    bool refillReturn =
        (saved.flags & SPREADER_TANK_FLAG_REFILL_RETURN) != 0 &&
        !insideHome;

    float nativeFuel = 0.0f;
    bool nativeFuelReadable =
        ReadablePtr(vehicle + SAND_VEHICLE_CURRENT_FUEL,
                    sizeof(float));
    if (nativeFuelReadable)
    {
        nativeFuel = *(float*)(vehicle + SAND_VEHICLE_CURRENT_FUEL);
        nativeFuelReadable = SandIsFiniteSignedNativeFuel(nativeFuel);
    }
    ULONGLONG now = GetTickCount64();
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item)
    {
        item->shadowTankInitialized = 1;
        item->shadowTankEmptyLogged = 0;
        item->enginePowerKw = powerKw;
        item->emptyWeightKg = emptyWeightKg;
        item->shadowTankCapacityKg = currentCapacityKg;
        item->shadowTankRemainingKg = remainingKg;
        item->shadowTankDryPlowing =
            remainingKg <= 0.000001f ? 1 : 0;
        item->shadowTankMaterialIndex = materialIndex;
        item->shadowTankProtectionStrength = protectionStrength;
        item->shadowTankResourceTextId = resourceTextId;
        strncpy_s(item->shadowTankResourceName,
                  sizeof(item->shadowTankResourceName),
                  saved.resourceName, _TRUNCATE);
        item->shadowTankUseEvents = 0;
        item->shadowTankPendingConsumedKg = 0.0f;
        item->shadowTankReturnThresholdPending = refillReturn ? 0 :
            ((saved.flags & SPREADER_TANK_FLAG_THRESHOLD_PENDING) ? 1 : 0);
        item->shadowTankReturnThresholdCrossed = insideHome ? 0 :
            ((saved.flags & SPREADER_TANK_FLAG_THRESHOLD_CROSSED) ? 1 : 0);
        if (refillReturn)
            item->shadowTankReturnThresholdCrossed = 1;
        item->shadowTankRefillCycleActive = 0;
        item->nativeFuelBaselineValid =
            !insideHome && nativeFuelReadable ? 1 : 0;
        item->nativeFuelLastAmount =
            !insideHome && nativeFuelReadable ? nativeFuel : 0.0f;
        item->nativeFuelCapacity = 0.0f;
        item->nativeLastSpeedKmh = 0.0f;
        item->nativePlowSpeedKmh = 0.0f;
        item->nativeLastQualifiedClearTick = 0;
        item->nativeLastConsumptionSampleTick = now;
        item->nativeLastConsumptionLogTick = 0;
        item->nativeLastQualifiedClearRoad = nullptr;
        item->nativeFuelConsumptionSamples = 0;
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    if (!item)
    {
        AcquireSRWLockExclusive(&g_spreaderTankPersistenceLock);
        SpreaderPersistedTankState& record =
            g_spreaderPersistedTanks[matchedIndex];
        if (record.valid && record.claimedVehicle == vehicle)
            record.claimedVehicle = nullptr;
        ReleaseSRWLockExclusive(&g_spreaderTankPersistenceLock);
        return false;
    }
    if (refillReturn)
    {
        SandArmReturnLock(
            vehicle, SAND_RETURN_REFILL, true, nullptr,
            "savegame-tank-restore");
    }
    InterlockedIncrement64(&g_spreaderTankPersistenceRestores);
    Report("INFO", "Tank persistence", "restore",
        "vehicle=%p home_building=%p vehicle_slot=%llu position_mm=[%ld,%ld,%ld] power_kw=%.3f empty_weight_kg=%.3f saved_capacity_kg=%.3f current_capacity_kg=%.3f saved_remaining_kg=%.3f restored_remaining_kg=%.3f material_index=%d material=%s protection_strength=%.3f dry=%d inside_home=%d refill_return_restored=%d native_fuel_baseline=%.6f native_fuel_baseline_valid=%d depot_debit=0 capacity_policy=clamp-saved-kg-to-current-capacity",
        vehicle, homeBuilding,
        (unsigned long long)vehicleSlotIndex,
        position[0], position[1], position[2],
        powerKw, emptyWeightKg,
        savedCapacityKg, currentCapacityKg,
        savedRemainingKg, remainingKg,
        materialIndex,
        saved.resourceName[0] ? saved.resourceName : "none",
        protectionStrength,
        remainingKg <= 0.000001f ? 1 : 0,
        insideHome ? 1 : 0,
        refillReturn ? 1 : 0,
        nativeFuel, nativeFuelReadable ? 1 : 0);
    return true;
}

static bool SpreaderCopyLoadedTankRecord(
    const LONG position[3], DWORD vehicleSlotIndex,
    LONG powerMilli, LONG weightMilli,
    SpreaderTankDiskEntry* out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!position || !out) return false;
    bool found = false;
    AcquireSRWLockShared(&g_spreaderTankPersistenceLock);
    for (size_t i = 0;
         i < SPREADER_MAX_PERSISTED_TANK_STATES; ++i)
    {
        const SpreaderPersistedTankState& candidate =
            g_spreaderPersistedTanks[i];
        if (!candidate.valid ||
            candidate.disk.vehicleSlotIndex != vehicleSlotIndex ||
            candidate.disk.enginePowerMilliKw != powerMilli ||
            candidate.disk.emptyWeightMilliKg != weightMilli ||
            !SpreaderPositionsMatch(
                candidate.disk.homePositionMillimetres, position))
            continue;
        *out = candidate.disk;
        found = true;
        break;
    }
    ReleaseSRWLockShared(&g_spreaderTankPersistenceLock);
    return found;
}

static void SpreaderSaveTankSidecar(const char* folder)
{
    if (!InterlockedCompareExchange(
            &g_spreaderPriorityPersistenceActive, 0, 0) ||
        !g_sandTankPersistenceEnabled || !folder || !folder[0])
        return;

    const size_t allocationBytes =
        SPREADER_MAX_PERSISTED_TANK_STATES *
        sizeof(SpreaderTankDiskEntry);
    SpreaderTankDiskEntry* entries =
        (SpreaderTankDiskEntry*)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, allocationBytes);
    if (!entries)
    {
        Report("WARN", "Tank persistence", "save-memory",
            "The bounded tank snapshot could not be allocated; the game save continues without updating the tank sidecar");
        return;
    }

    size_t count = 0;
    size_t liveSnapshots = 0;
    size_t carriedLoadedRecords = 0;
    size_t uninitializedSkipped = 0;
    size_t terminalSkipped = 0;
    size_t buildingCount = 0;
    bool registryValid = false;
    BYTE* staticGame = g_exeBase + RVA_SAND_STATIC_GAME;
    void* world = nullptr;
    __try
    {
        world = *(void**)(g_exeBase + RVA_SAND_GLOBAL_WORLD_POINTER);
        if (world && ReadablePtr(
                staticGame + SAND_GAME_BUILDING_BEGIN, 16))
        {
            BYTE** begin = *(BYTE***)(
                staticGame + SAND_GAME_BUILDING_BEGIN);
            BYTE** end = *(BYTE***)(
                staticGame + SAND_GAME_BUILDING_END);
            if ((!begin && !end) ||
                (begin && end && end >= begin))
            {
                buildingCount = begin ? (size_t)(end - begin) : 0;
                registryValid = buildingCount <= 500000 &&
                    (!buildingCount || ReadablePtr(
                        begin, buildingCount * sizeof(void*)));
                if (registryValid)
                {
                    for (size_t buildingIndex = 0;
                         buildingIndex < buildingCount &&
                         count < SPREADER_MAX_PERSISTED_TANK_STATES;
                         ++buildingIndex)
                    {
                        BYTE* building = begin[buildingIndex];
                        if (!building ||
                            ReadTechnicalType(building) !=
                                BUILDING_GARBAGE_OFFICE ||
                            !ReadablePtr(
                                building + SAND_BUILDING_GOING_AWAY,
                                sizeof(BYTE)))
                            continue;
                        if (*(BYTE*)(building +
                                SAND_BUILDING_GOING_AWAY) != 0)
                        {
                            ++terminalSkipped;
                            continue;
                        }
                        LONG position[3] = {};
                        if (!SpreaderPositionMillimetres(
                                building, position) ||
                            !ReadablePtr(
                                building + B_VEHICLE_BEGIN, 16))
                            continue;
                        void** vehicleBegin = *(void***)(
                            building + B_VEHICLE_BEGIN);
                        void** vehicleEnd = *(void***)(
                            building + B_VEHICLE_END);
                        if ((!vehicleBegin && vehicleEnd) ||
                            (vehicleBegin && !vehicleEnd) ||
                            (vehicleBegin && vehicleEnd &&
                             vehicleEnd < vehicleBegin))
                            continue;
                        size_t vehicleCount = vehicleBegin ?
                            (size_t)(vehicleEnd - vehicleBegin) : 0;
                        if (vehicleCount > 4096 ||
                            (vehicleCount && !ReadablePtr(
                                vehicleBegin,
                                vehicleCount * sizeof(void*))))
                            continue;

                        for (size_t vehicleIndex = 0;
                             vehicleIndex < vehicleCount &&
                             count < SPREADER_MAX_PERSISTED_TANK_STATES;
                             ++vehicleIndex)
                        {
                            BYTE* vehicle =
                                (BYTE*)vehicleBegin[vehicleIndex];
                            if (!vehicle) continue;
                            SandSkillView skills = {};
                            SandReadSkillView(vehicle, &skills);
                            if (!skills.valid || !skills.snowplow)
                                continue;
                            float powerKw = 0.0f;
                            float emptyWeightKg = 0.0f;
                            float calculatedCapacityKg = 0.0f;
                            if (!SandCalculateShadowTankCapacity(
                                    skills.typeDescription,
                                    &powerKw, &emptyWeightKg,
                                    &calculatedCapacityKg))
                                continue;
                            LONG powerMilli =
                                SpreaderTankRoundMilli(powerKw);
                            LONG weightMilli =
                                SpreaderTankRoundMilli(emptyWeightKg);

                            SandTrackedVehicle snapshot = {};
                            bool liveTank = false;
                            AcquireSRWLockShared(&g_sandTrackerLock);
                            SandTrackedVehicle* tracked =
                                SandFindTrackedVehicleLocked(
                                    vehicle, false);
                            if (tracked &&
                                tracked->shadowTankInitialized &&
                                tracked->homeBuilding == building)
                            {
                                snapshot = *tracked;
                                liveTank = true;
                            }
                            ReleaseSRWLockShared(&g_sandTrackerLock);

                            SpreaderTankDiskEntry entry = {};
                            if (!liveTank)
                            {
                                if (SpreaderCopyLoadedTankRecord(
                                        position,
                                        (DWORD)vehicleIndex,
                                        powerMilli, weightMilli,
                                        &entry))
                                {
                                    entries[count++] = entry;
                                    ++carriedLoadedRecords;
                                }
                                else
                                    ++uninitializedSkipped;
                                continue;
                            }

                            float capacityKg =
                                snapshot.shadowTankCapacityKg;
                            float remainingKg =
                                snapshot.shadowTankRemainingKg;
                            if (!IsFiniteNonNegative(capacityKg) ||
                                capacityKg <= 0.0f)
                                capacityKg = calculatedCapacityKg;
                            if (!IsFiniteNonNegative(remainingKg))
                                remainingKg = 0.0f;
                            if (remainingKg > capacityKg)
                                remainingKg = capacityKg;
                            memcpy(entry.homePositionMillimetres,
                                   position, sizeof(position));
                            entry.vehicleSlotIndex =
                                (DWORD)vehicleIndex;
                            entry.enginePowerMilliKw = powerMilli;
                            entry.emptyWeightMilliKg = weightMilli;
                            entry.savedCapacityMilliKg =
                                SpreaderTankRoundMilli(capacityKg);
                            entry.remainingMilliKg =
                                SpreaderTankRoundMilli(remainingKg);
                            if (entry.remainingMilliKg == 0)
                                entry.flags |=
                                    SPREADER_TANK_FLAG_DRY;
                            if (snapshot.shadowTankReturnThresholdPending)
                                entry.flags |=
                                    SPREADER_TANK_FLAG_THRESHOLD_PENDING;
                            if (snapshot.shadowTankReturnThresholdCrossed)
                                entry.flags |=
                                    SPREADER_TANK_FLAG_THRESHOLD_CROSSED;
                            if (snapshot.returnLockActive &&
                                snapshot.returnReason ==
                                    SAND_RETURN_REFILL)
                            {
                                entry.flags |=
                                    SPREADER_TANK_FLAG_REFILL_RETURN;
                                entry.returnReason =
                                    SAND_RETURN_REFILL;
                            }
                            strncpy_s(entry.resourceName,
                                      sizeof(entry.resourceName),
                                      snapshot.shadowTankResourceName,
                                      _TRUNCATE);
                            if (!SpreaderTankDiskEntryPlausible(entry))
                            {
                                ++uninitializedSkipped;
                                continue;
                            }
                            entries[count++] = entry;
                            ++liveSnapshots;
                        }
                    }
                }
            }
        }
    }
    __except(FaultFilter(
        "tank sidecar registry snapshot",
        GetExceptionInformation()))
    {
        registryValid = false;
    }

    if (!registryValid || !world || buildingCount == 0)
    {
        HeapFree(GetProcessHeap(), 0, entries);
        Report("WARN", "Tank persistence", "save-registry",
            "The live world/building registry was unavailable or transiently empty; the game save continues and the previous tank sidecar is retained (world=%p buildings=%llu)",
            world, (unsigned long long)buildingCount);
        return;
    }

    char resolvedFolder[MAX_PATH * 2] = {};
    char path[MAX_PATH * 2] = {};
    char temporaryPath[MAX_PATH * 2] = {};
    if (!SpreaderResolveWorldFolder(
            folder, resolvedFolder, sizeof(resolvedFolder)) ||
        !SpreaderTankPath(
            resolvedFolder, "", path, sizeof(path)) ||
        !SpreaderTankPath(
            resolvedFolder, ".tmp", temporaryPath,
            sizeof(temporaryPath)))
    {
        HeapFree(GetProcessHeap(), 0, entries);
        Report("WARN", "Tank persistence", "save-path",
            "The native destination folder could not be resolved; the game save continues without updating the tank sidecar (native_folder=%s)",
            folder);
        return;
    }

    SpreaderTankDiskHeader header = {};
    memcpy(header.magic, SPREADER_TANK_FILE_MAGIC, 8);
    header.version = SPREADER_TANK_FILE_VERSION;
    header.headerSize = (DWORD)sizeof(header);
    header.entrySize = (DWORD)sizeof(SpreaderTankDiskEntry);
    header.count = (DWORD)count;
    size_t payloadBytes = count * sizeof(SpreaderTankDiskEntry);
    header.payloadHash =
        SpreaderPersistenceHash(entries, payloadBytes);

    HANDLE file = CreateFileA(
        temporaryPath, GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool saved = file != INVALID_HANDLE_VALUE;
    DWORD error = saved ? ERROR_SUCCESS : GetLastError();
    if (saved)
    {
        saved = SpreaderWriteExact(
            file, &header, (DWORD)sizeof(header)) &&
            (!payloadBytes || SpreaderWriteExact(
                file, entries, (DWORD)payloadBytes)) &&
            FlushFileBuffers(file) != 0;
        if (!saved) error = GetLastError();
        CloseHandle(file);
    }
    if (saved)
    {
        saved = MoveFileExA(
            temporaryPath, path,
            MOVEFILE_REPLACE_EXISTING |
            MOVEFILE_WRITE_THROUGH) != 0;
        if (!saved) error = GetLastError();
    }
    if (!saved) DeleteFileA(temporaryPath);
    HeapFree(GetProcessHeap(), 0, entries);

    if (!saved)
    {
        ReportWindows("WARN", "Tank persistence", "save",
            "The atomic save-specific tank sidecar update failed; the game save itself is unaffected",
            error,
            "Verify write access and free space in the savegame folder");
        return;
    }

    AcquireSRWLockExclusive(&g_spreaderTankPersistenceLock);
    strncpy_s(g_spreaderTankWorldFolder,
              sizeof(g_spreaderTankWorldFolder),
              resolvedFolder, _TRUNCATE);
    ReleaseSRWLockExclusive(&g_spreaderTankPersistenceLock);
    InterlockedIncrement64(&g_spreaderTankPersistenceSaves);
    Report("INFO", "Tank persistence", "save",
        "native_folder=%s resolved_folder=%s file=%s entries=%llu live_snapshots=%llu carried_loaded_records=%llu uninitialized_skipped=%llu terminal_buildings_skipped=%llu bytes=%llu atomic_replace=1 save_specific=1 identity=home-position-within-50mm-plus-vehicle-slot-plus-power-weight",
        folder, resolvedFolder, SPREADER_TANK_FILE_NAME,
        (unsigned long long)count,
        (unsigned long long)liveSnapshots,
        (unsigned long long)carriedLoadedRecords,
        (unsigned long long)uninitializedSkipped,
        (unsigned long long)terminalSkipped,
        (unsigned long long)(sizeof(header) + payloadBytes));
}

// A Technical Services snowplow receives its active work records before it
// leaves the depot. This is a stronger plough-session signal than its current
// speed: vanilla allows a lightly covered road to be cleared faster than the
// nominal skill-35 clearing speed. The caller already verified skill 35, so
// this helper only validates the live assignment vector and current index.
static bool SandVehicleHasActiveWorkAssignment(void* vehicle)
{
    BYTE* bytes = (BYTE*)vehicle;
    if (!bytes ||
        !ReadablePtr(bytes + SAND_VEHICLE_ASSIGNMENT_BEGIN,
                     sizeof(void*)) ||
        !ReadablePtr(bytes + SAND_VEHICLE_ASSIGNMENT_END,
                     sizeof(void*)) ||
        !ReadablePtr(bytes + SAND_VEHICLE_ASSIGNMENT_INDEX,
                     sizeof(int)))
        return false;

    void* beginPointer =
        *(void**)(bytes + SAND_VEHICLE_ASSIGNMENT_BEGIN);
    void* endPointer =
        *(void**)(bytes + SAND_VEHICLE_ASSIGNMENT_END);
    int index =
        *(int*)(bytes + SAND_VEHICLE_ASSIGNMENT_INDEX);
    if (!beginPointer || !endPointer) return false;
    uintptr_t begin = (uintptr_t)beginPointer;
    uintptr_t end = (uintptr_t)endPointer;
    if (end <= begin || ((end - begin) % 0x10) != 0)
        return false;
    size_t count = (size_t)((end - begin) / 0x10);
    return count > 0 && count <= 4096 &&
        index >= 0 && (size_t)index < count;
}

static void SandObserveShadowTankState(
    const char* source,
    void* vehicle,
    const SandSkillView& skills,
    void* homeBuilding,
    const SandTrackUpdate& update,
    bool reconstructFirstSeenOutsideHome = false)
{
    if (!g_sandShadowTankEnabled || !vehicle ||
        !skills.valid || !skills.snowplow)
        return;

    bool arrivedHome = update.insideHome &&
        !update.firstSeen && !update.previousInsideHome;
    bool firstSeenAtHome = update.firstSeen && update.insideHome;
    int tankAlreadyInitialized = 0;
    float previousCapacityKg = 0.0f;
    float previousRemainingKg = 0.0f;
    int previousMaterialIndex = -1;
    float previousProtectionStrength = 0.0f;
    int previousResourceTextId = 0;
    int previousRefillCycleActive = 0;
    char previousResourceName[64] = {};
    AcquireSRWLockShared(&g_sandTrackerLock);
    SandTrackedVehicle* existingTank =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (existingTank)
    {
        tankAlreadyInitialized = existingTank->shadowTankInitialized;
        previousCapacityKg = existingTank->shadowTankCapacityKg;
        previousRemainingKg = existingTank->shadowTankRemainingKg;
        previousMaterialIndex = existingTank->shadowTankMaterialIndex;
        previousProtectionStrength =
            existingTank->shadowTankProtectionStrength;
        previousResourceTextId =
            existingTank->shadowTankResourceTextId;
        previousRefillCycleActive =
            existingTank->shadowTankRefillCycleActive;
        strncpy_s(previousResourceName, sizeof(previousResourceName),
                  existingTank->shadowTankResourceName, _TRUNCATE);
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);
    bool reconstructOutsideHome = reconstructFirstSeenOutsideHome &&
        !update.insideHome && !tankAlreadyInitialized;
    bool loadTank = firstSeenAtHome || arrivedHome ||
        reconstructOutsideHome;
    if (loadTank)
    {
        float powerKw = 0.0f;
        float emptyWeightKg = 0.0f;
        float capacityKg = 0.0f;
        if (!SandCalculateShadowTankCapacity(
                skills.typeDescription, &powerKw,
                &emptyWeightKg, &capacityKg))
        {
            Report("WARN", "Tank diagnostic", "shadow-tank-capacity",
                "source=%s vehicle=%p type=%p native power/weight fields are invalid; the plugin-owned grit tank was not initialized",
                source ? source : "unknown", vehicle,
                skills.typeDescription);
            return;
        }

        SpreaderStorageObservation material = {};
        if (homeBuilding &&
            ReadTechnicalType((BYTE*)homeBuilding) ==
                BUILDING_GARBAGE_OFFICE)
            material = SpreaderObserveStorage((BYTE*)homeBuilding);
        float depotAvailableKg = material.found
            ? material.amount * 1000.0f : 0.0f;
        if (!IsFiniteNonNegative(depotAvailableKg))
            depotAvailableKg = 0.0f;

        // A first observation after loading/creating a world reconstructs the
        // plugin-owned tank because it is not part of the savegame. Charging
        // that reconstruction would remove another tank from the depot on
        // every reload. Only a later, observed arrival (including the native
        // fuel-refill fallback) is a real refill transaction.
        bool worldBaselineComplete = InterlockedCompareExchange(
            &g_sandWorldBaselineComplete, 0, 0) != 0;
        bool newlyAssignedAtHome = firstSeenAtHome && worldBaselineComplete;
        bool baselineInitialization = reconstructOutsideHome ||
            (firstSeenAtHome && !worldBaselineComplete);
        bool refillObservation = arrivedHome || newlyAssignedAtHome;
        bool duplicateRefillObservation = refillObservation &&
            previousRefillCycleActive;
        bool refillTransaction = refillObservation &&
            !duplicateRefillObservation;
        bool sameMaterial = tankAlreadyInitialized && material.found &&
            previousMaterialIndex == material.materialIndex;
        float clampedPreviousKg = previousRemainingKg;
        if (!IsFiniteNonNegative(clampedPreviousKg)) clampedPreviousKg = 0.0f;
        if (clampedPreviousKg > capacityKg) clampedPreviousKg = capacityKg;

        float loadedKg = clampedPreviousKg;
        float requestedDepotKg = 0.0f;
        float debitedDepotKg = 0.0f;
        float discardedPreviousKg = 0.0f;
        SandConsumeResult depotDebit = {};
        depotDebit.status = SAND_CONSUME_DISABLED;

        int finalMaterialIndex = previousMaterialIndex;
        float finalProtectionStrength = previousProtectionStrength;
        int finalResourceTextId = previousResourceTextId;
        char finalResourceName[64] = {};
        strncpy_s(finalResourceName, sizeof(finalResourceName),
                  previousResourceName, _TRUNCATE);
        if (!tankAlreadyInitialized && material.found)
        {
            finalMaterialIndex = material.materialIndex;
            finalProtectionStrength = material.protectionStrength;
            finalResourceTextId = material.resourceTextId;
            strncpy_s(finalResourceName, sizeof(finalResourceName),
                      material.resourceName, _TRUNCATE);
        }

        if (baselineInitialization)
        {
            loadedKg = depotAvailableKg < capacityKg
                     ? depotAvailableKg : capacityKg;
            if (loadedKg < 0.000001f) loadedKg = 0.0f;
            finalMaterialIndex = material.found ? material.materialIndex : -1;
            finalProtectionStrength = material.found
                ? material.protectionStrength : 0.0f;
            finalResourceTextId = material.found
                ? material.resourceTextId : 0;
            strncpy_s(finalResourceName, sizeof(finalResourceName),
                      material.found ? material.resourceName : "",
                      _TRUNCATE);
        }
        else if (refillTransaction && material.found &&
                 depotAvailableKg > 0.000001f &&
                 clampedPreviousKg + 0.000001f < capacityKg)
        {
            float retainedKg = sameMaterial ? clampedPreviousKg : 0.0f;
            requestedDepotKg = capacityKg - retainedKg;
            depotDebit = SpreaderConsumeObservedStorage(
                material, requestedDepotKg);
            if (depotDebit.status == SAND_CONSUME_OK)
            {
                debitedDepotKg = depotDebit.consumed * 1000.0f;
                loadedKg = retainedKg + debitedDepotKg;
                if (loadedKg > capacityKg) loadedKg = capacityKg;
                if (!sameMaterial)
                    discardedPreviousKg = clampedPreviousKg;
                finalMaterialIndex = material.materialIndex;
                finalProtectionStrength = material.protectionStrength;
                finalResourceTextId = material.resourceTextId;
                strncpy_s(finalResourceName, sizeof(finalResourceName),
                          material.resourceName, _TRUNCATE);
                InterlockedIncrement64(&g_sandDepotRefillDebitEvents);
                if (debitedDepotKg + 0.000001f < requestedDepotKg)
                    InterlockedIncrement64(
                        &g_sandDepotRefillPartialEvents);
            }
            else if (depotDebit.status == SAND_CONSUME_EMPTY ||
                     depotDebit.status == SAND_CONSUME_WRITE_RACE ||
                     depotDebit.status == SAND_CONSUME_INVALID_STORAGE)
            {
                InterlockedIncrement64(&g_sandDepotRefillDebitFailures);
                Report("WARN", "Tank diagnostic", "depot-refill-debit-refused",
                    "source=%s vehicle=%p home_building=%p material_index=%d material=%s requested_kg=%.3f observed_available_kg=%.3f reason=%s previous_tank_preserved=1",
                    source ? source : "unknown", vehicle, homeBuilding,
                    material.materialIndex,
                    material.resourceName[0] ?
                        material.resourceName : "none",
                    requestedDepotKg, depotAvailableKg,
                    SandConsumeStatusName(depotDebit.status));
            }
        }

        if (loadedKg < 0.000001f) loadedKg = 0.0f;

        AcquireSRWLockExclusive(&g_sandTrackerLock);
        SandTrackedVehicle* item =
            SandFindTrackedVehicleLocked(vehicle, true);
        if (item)
        {
            item->shadowTankInitialized = 1;
            item->shadowTankEmptyLogged = 0;
            item->enginePowerKw = powerKw;
            item->emptyWeightKg = emptyWeightKg;
            item->shadowTankCapacityKg = capacityKg;
            item->shadowTankRemainingKg = loadedKg;
            item->shadowTankDryPlowing =
                loadedKg <= 0.000001f ? 1 : 0;
            item->shadowTankMaterialIndex =
                finalMaterialIndex;
            item->shadowTankProtectionStrength =
                finalProtectionStrength;
            item->shadowTankResourceTextId =
                finalResourceTextId;
            strncpy_s(
                item->shadowTankResourceName,
                sizeof(item->shadowTankResourceName),
                finalResourceName,
                _TRUNCATE);
            item->shadowTankUseEvents = 0;
            item->shadowTankPendingConsumedKg = 0.0f;
            item->shadowTankReturnThresholdPending = 0;
            item->shadowTankReturnThresholdCrossed = 0;
            item->shadowTankRefillCycleActive = baselineInitialization ? 0 :
                (refillTransaction ? 1 : previousRefillCycleActive);
            item->nativeFuelBaselineValid = 0;
            item->nativeFuelLastAmount = 0.0f;
            item->nativeFuelCapacity = 0.0f;
            item->nativeLastSpeedKmh = 0.0f;
            item->nativePlowSpeedKmh = 0.0f;
            item->nativeLastQualifiedClearTick = 0;
            item->nativeLastConsumptionSampleTick = 0;
            item->nativeLastConsumptionLogTick = 0;
            item->nativeLastQualifiedClearRoad = nullptr;
            item->nativeFuelConsumptionSamples = 0;
        }
        ReleaseSRWLockExclusive(&g_sandTrackerLock);

        if (item)
        {
            float formulaBaseKg =
                emptyWeightKg *
                    ((float)g_sandTankWeightPercent / 100.0f) +
                powerKw * (float)g_sandTankPowerKgPerKw;
            float formulaAdjustedKg = formulaBaseKg *
                ((float)g_sandTankCapacityMultiplierPercent / 100.0f);
            InterlockedIncrement64(&g_sandShadowTankLoadEvents);
            Report("INFO", "Tank diagnostic", "shadow-tank-loaded",
                "source=%s reason=%s vehicle=%p home_building=%p power_kw=%.3f empty_weight_kg=%.3f formula_base_kg=%.3f capacity_multiplier_percent=%d formula_adjusted_kg=%.3f capacity_kg=%.3f previous_capacity_kg=%.3f previous_loaded_kg=%.3f depot_grit_available_before_kg=%.3f requested_depot_kg=%.3f debited_depot_kg=%.3f loaded_kg=%.3f partial=%d discarded_previous_kg=%.3f dry_plowing=%d material_index=%d protection_strength=%.3f resource_text_id=%d resource_name=%s storage=%s baseline_reconstruction=%d refill_observation=%d refill_transaction=%d duplicate_refill_observation=%d same_material=%d depot_debit_status=%s tank_memory=plugin-owned depot_storage_mutated=%d",
                source ? source : "unknown",
                reconstructOutsideHome ?
                    "world-load-reconstruction-outside-home" :
                newlyAssignedAtHome ? "newly-assigned-at-home" :
                firstSeenAtHome ? "world-load-reconstruction-in-home" :
                                  "arrived-home",
                vehicle, homeBuilding,
                powerKw, emptyWeightKg,
                formulaBaseKg,
                g_sandTankCapacityMultiplierPercent,
                formulaAdjustedKg,
                capacityKg, previousCapacityKg, clampedPreviousKg,
                depotAvailableKg, requestedDepotKg, debitedDepotKg,
                loadedKg,
                loadedKg + 0.000001f < capacityKg ? 1 : 0,
                discardedPreviousKg,
                loadedKg <= 0.000001f ? 1 : 0,
                finalMaterialIndex,
                finalProtectionStrength,
                finalResourceTextId,
                finalResourceName[0] ? finalResourceName : "none",
                finalMaterialIndex >= 0 ?
                    "configured-material" : "not-found",
                baselineInitialization ? 1 : 0,
                refillObservation ? 1 : 0,
                refillTransaction ? 1 : 0,
                duplicateRefillObservation ? 1 : 0,
                sameMaterial ? 1 : 0,
                SandConsumeStatusName(depotDebit.status),
                depotDebit.status == SAND_CONSUME_OK ? 1 : 0);
        }
    }

    if (update.previousInsideHome && !update.insideHome)
    {
        int initialized = 0;
        int activeAssignment =
            SandVehicleHasActiveWorkAssignment(vehicle) ? 1 : 0;
        float capacityKg = 0.0f;
        float remainingKg = 0.0f;
        float currentFuel = 0.0f;
        bool currentFuelReadable = ReadablePtr(
            (BYTE*)vehicle + SAND_VEHICLE_CURRENT_FUEL,
            sizeof(float));
        if (currentFuelReadable)
        {
            currentFuel = *(float*)((BYTE*)vehicle +
                SAND_VEHICLE_CURRENT_FUEL);
            currentFuelReadable =
                SandIsFiniteSignedNativeFuel(currentFuel);
        }
        ULONGLONG departureTick = GetTickCount64();
        AcquireSRWLockShared(&g_sandTrackerLock);
        SandTrackedVehicle* item =
            SandFindTrackedVehicleLocked(vehicle, false);
        if (item)
        {
            initialized = item->shadowTankInitialized;
            capacityKg = item->shadowTankCapacityKg;
            remainingKg = item->shadowTankRemainingKg;
        }
        ReleaseSRWLockShared(&g_sandTrackerLock);
        AcquireSRWLockExclusive(&g_sandTrackerLock);
        SandTrackedVehicle* departingItem =
            SandFindTrackedVehicleLocked(vehicle, false);
        if (departingItem)
        {
            // Technical Services starts ploughing immediately after departure
            // when an active assignment is present. Start with the current
            // native fuel as the baseline; no depot fuel can be charged
            // retroactively. A later confirmed clear also starts the session
            // if assignment publication happened just after this transition.
            departingItem->nativeFuelBaselineValid =
                currentFuelReadable ? 1 : 0;
            departingItem->nativeFuelLastAmount =
                currentFuelReadable ? currentFuel : 0.0f;
            departingItem->nativeLastQualifiedClearTick =
                activeAssignment ? departureTick : 0;
            departingItem->nativeLastConsumptionSampleTick =
                departureTick;
            departingItem->nativeLastQualifiedClearRoad = nullptr;
            departingItem->shadowTankPendingConsumedKg = 0.0f;
            departingItem->shadowTankRefillCycleActive = 0;
        }
        ReleaseSRWLockExclusive(&g_sandTrackerLock);
        Report("INFO", "Tank diagnostic", "shadow-tank-departure",
            "source=%s vehicle=%p home_building=%p initialized=%d capacity_kg=%.3f remaining_kg=%.3f active_assignment=%d plow_session_started=%d native_fuel_baseline=%.6f native_fuel_baseline_valid=%d read_only=1",
            source ? source : "unknown", vehicle, homeBuilding,
            initialized, capacityKg, remainingKg,
            activeAssignment,
            activeAssignment && currentFuelReadable ? 1 : 0,
            currentFuel,
            currentFuelReadable ? 1 : 0);
    }
}

static SandShadowTankUseResult SandUseShadowTank(
    void* vehicle,
    const SandSkillView& skills,
    bool duplicate,
    bool activePlowing,
    float currentSpeedKmh,
    float plowSpeedKmh)
{
    SandShadowTankUseResult result = {};
    result.materialIndex = -1;
    result.activePlowing = activePlowing ? 1 : 0;
    result.currentSpeedKmh = currentSpeedKmh;
    result.plowSpeedKmh = plowSpeedKmh;
    if (!g_sandShadowTankEnabled)
    {
        result.status = SAND_SHADOW_TANK_DISABLED;
        return result;
    }
    if (duplicate)
    {
        result.status = SAND_SHADOW_TANK_DUPLICATE;
        return result;
    }
    if (!skills.valid || !skills.snowplow)
    {
        result.status = SAND_SHADOW_TANK_NOT_SNOWPLOW;
        return result;
    }
    if (!activePlowing)
    {
        result.status = SAND_SHADOW_TANK_INACTIVE_WORK;
        AcquireSRWLockShared(&g_sandTrackerLock);
        SandTrackedVehicle* inactiveItem =
            SandFindTrackedVehicleLocked(vehicle, false);
        if (inactiveItem && inactiveItem->shadowTankInitialized)
        {
            result.materialIndex = inactiveItem->shadowTankMaterialIndex;
            result.protectionStrength =
                inactiveItem->shadowTankProtectionStrength;
            result.resourceTextId = inactiveItem->shadowTankResourceTextId;
            strncpy_s(result.resourceName,
                      sizeof(result.resourceName),
                      inactiveItem->shadowTankResourceName, _TRUNCATE);
            result.capacityKg = inactiveItem->shadowTankCapacityKg;
            result.beforeKg = inactiveItem->shadowTankRemainingKg;
            result.afterKg = result.beforeKg;
            result.returnThresholdPending =
                inactiveItem->shadowTankReturnThresholdPending;
        }
        ReleaseSRWLockShared(&g_sandTrackerLock);
        return result;
    }

    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (!item || !item->shadowTankInitialized)
    {
        result.status = SAND_SHADOW_TANK_UNINITIALIZED;
    }
    else
    {
        // One unique active clear proves that a native-fuel fallback refill has
        // entered a new work cycle. A later physical home arrival is therefore
        // a new fill, while an immediate second arrival observation before any
        // work cannot debit another priority slot.
        item->shadowTankRefillCycleActive = 0;
        result.materialIndex = item->shadowTankMaterialIndex;
        result.protectionStrength =
            item->shadowTankProtectionStrength;
        result.resourceTextId = item->shadowTankResourceTextId;
        strncpy_s(result.resourceName,
                  sizeof(result.resourceName),
                  item->shadowTankResourceName, _TRUNCATE);
        result.capacityKg = item->shadowTankCapacityKg;
        result.consumedKg = item->shadowTankPendingConsumedKg;
        result.requestedKg = result.consumedKg;
        result.afterKg = item->shadowTankRemainingKg;
        result.beforeKg = result.afterKg + result.consumedKg;
        result.returnThresholdPending =
            item->shadowTankReturnThresholdPending;
        item->shadowTankPendingConsumedKg = 0.0f;
        if (result.afterKg <= 0.000001f &&
            result.consumedKg <= 0.000001f)
        {
            result.status = SAND_SHADOW_TANK_EMPTY;
            if (!item->shadowTankEmptyLogged)
            {
                item->shadowTankEmptyLogged = 1;
                result.firstEmptyReport = 1;
            }
        }
        else
        {
            result.status = SAND_SHADOW_TANK_CONSUMED;
            if (result.afterKg <= 0.000001f)
            {
                result.becameEmpty = 1;
                if (!item->shadowTankEmptyLogged)
                {
                    item->shadowTankEmptyLogged = 1;
                    result.firstEmptyReport = 1;
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    if (result.firstEmptyReport)
        InterlockedIncrement64(&g_sandShadowTankEmptyEvents);
    return result;
}

// Read-only bridge for the vehicle-information renderer. The resource metadata
// belongs to the tank fill event, so the UI describes what this individual
// vehicle actually carries instead of inferring it from whichever depot window
// happens to be open. Returning false keeps non-snowplows and vehicles without
// a constructed tank entirely unchanged.
static bool SandGetShadowTankDisplayDetails(
    void* vehicle,
    float* remainingKgOut,
    float* capacityKgOut,
    int* dryPlowingOut,
    int* returningToDepotOut,
    int* resourceTextIdOut,
    char* resourceNameOut,
    size_t resourceNameSize)
{
    if (remainingKgOut) *remainingKgOut = 0.0f;
    if (capacityKgOut) *capacityKgOut = 0.0f;
    if (dryPlowingOut) *dryPlowingOut = 0;
    if (returningToDepotOut) *returningToDepotOut = 0;
    if (resourceTextIdOut) *resourceTextIdOut = 0;
    if (resourceNameOut && resourceNameSize > 0)
        resourceNameOut[0] = 0;
    if (!g_sandShadowTankEnabled || !vehicle) return false;

    bool available = false;
    AcquireSRWLockShared(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->snowplow && item->shadowTankInitialized &&
        IsFiniteNonNegative(item->shadowTankRemainingKg) &&
        IsFiniteNonNegative(item->shadowTankCapacityKg) &&
        item->shadowTankCapacityKg > 0.0f)
    {
        if (remainingKgOut)
            *remainingKgOut = item->shadowTankRemainingKg;
        if (capacityKgOut)
            *capacityKgOut = item->shadowTankCapacityKg;
        if (dryPlowingOut)
            *dryPlowingOut = item->shadowTankDryPlowing ? 1 : 0;
        if (returningToDepotOut)
            *returningToDepotOut =
                item->returnLockActive &&
                item->returnReason == SAND_RETURN_REFILL ? 1 : 0;
        if (resourceTextIdOut)
            *resourceTextIdOut = item->shadowTankResourceTextId;
        if (resourceNameOut && resourceNameSize > 0)
            strncpy_s(resourceNameOut, resourceNameSize,
                      item->shadowTankResourceName, _TRUNCATE);
        available = true;
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);
    return available;
}

static bool SandGetShadowTankDisplay(void* vehicle,
                                     float* remainingKgOut,
                                     float* capacityKgOut)
{
    return SandGetShadowTankDisplayDetails(
        vehicle, remainingKgOut, capacityKgOut,
        nullptr, nullptr, nullptr, nullptr, 0);
}

static void SandReleaseCompletedReturnLockLocked(
    SandTrackedVehicle* item);
static const char* SandReturnReasonName(int reason);

// Some Technical Services vehicles begin native refuelling without exposing a
// stable currentBuilding == homeBuilding sample. The native fuel increase is
// still authoritative: it can only occur while the return lock has latched the
// vehicle's own home depot as its refuelling destination. Use that already
// observed read-only transition as a second arrival/refill confirmation, then
// load the plugin-owned grit tank from the same depot selection as the normal
// physical-arrival path. No native fuel, route or building field is changed.
static void SandCompleteReturnFromNativeFuelIncrease(
    void* vehicle,
    void* currentBuilding,
    void* homeBuilding,
    void* fuelBuilding,
    float nativeFuelBefore,
    float nativeFuelAfter,
    float nativeFuelCapacity)
{
    if (!vehicle || !homeBuilding) return;

    SandSkillView skills = {};
    SandReadSkillView((BYTE*)vehicle, &skills);
    if (!skills.valid || !skills.snowplow) return;

    SandTrackUpdate syntheticArrival = {};
    syntheticArrival.previousCurrentBuilding = currentBuilding;
    syntheticArrival.previousHomeBuilding = homeBuilding;
    syntheticArrival.currentBuilding = homeBuilding;
    syntheticArrival.homeBuilding = homeBuilding;
    syntheticArrival.previousInsideHome = 0;
    syntheticArrival.insideHome = 1;
    SandObserveShadowTankState(
        "native-fuel-refill-observer", vehicle, skills,
        homeBuilding, syntheticArrival);

    float loadedKg = 0.0f;
    float capacityKg = 0.0f;
    int dryPlowing = 0;
    int resourceTextId = 0;
    char resourceName[64] = {};
    bool tankAvailable = SandGetShadowTankDisplayDetails(
        vehicle, &loadedKg, &capacityKg, &dryPlowing,
        nullptr, &resourceTextId, resourceName, sizeof(resourceName));

    int lockReleased = 0;
    int reason = 0;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (tankAvailable && item && item->returnLockActive &&
        item->homeBuilding == homeBuilding)
    {
        reason = item->returnReason;
        SandReleaseCompletedReturnLockLocked(item);
        lockReleased = 1;
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    Report("INFO", "Tank diagnostic", "native-fuel-refill-detected",
        "vehicle=%p current_building=%p home_building=%p fuel_building=%p native_fuel_before=%.6f native_fuel_after=%.6f native_fuel_increase=%.6f native_fuel_capacity=%.3f tank_available=%d loaded_kg=%.3f capacity_kg=%.3f dry_plowing=%d material_text_id=%d material=%s reason=%s lock_released=%d detection=native-positive-fuel-delta game_memory_mutation=0",
        vehicle, currentBuilding, homeBuilding, fuelBuilding,
        nativeFuelBefore, nativeFuelAfter,
        nativeFuelAfter - nativeFuelBefore, nativeFuelCapacity,
        tankAvailable ? 1 : 0, loadedKg, capacityKg, dryPlowing,
        resourceTextId, resourceName[0] ? resourceName : "none",
        SandReturnReasonName(reason), lockReleased);
}


static void SandResolveShadowTankReturnThreshold(
    void* vehicle)
{
    if (!vehicle) return;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->shadowTankInitialized)
    {
        // The reserve threshold is only a one-shot return request. The real
        // plugin-owned amount continues to be consumed down to zero while
        // vanilla finishes clearing and builds its route home.
        item->shadowTankReturnThresholdPending = 0;
        item->shadowTankDryPlowing =
            item->shadowTankRemainingKg <= 0.000001f ? 1 : 0;
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);
}

static const char* SandReturnReasonName(int reason)
{
    switch (reason)
    {
        case SAND_RETURN_MANUAL_HOME: return "manual-home";
        case SAND_RETURN_REFILL:      return "empty-tank-refill";
        default:                      return "unknown";
    }
}

static bool SandReadReturnRouteState(void* vehicle, DWORD* stateOut)
{
    if (stateOut) *stateOut = 0;
    BYTE* bytes = (BYTE*)vehicle;
    if (!bytes ||
        !ReadablePtr(bytes + SAND_VEHICLE_RETURN_ROUTE_STATE,
                     sizeof(DWORD)))
        return false;
    DWORD state = *(DWORD*)(bytes + SAND_VEHICLE_RETURN_ROUTE_STATE);
    if (stateOut) *stateOut = state;
    return true;
}

struct SandRouteVectorState
{
    bool readable;
    size_t count;
    size_t linkCount;
    int index;
    void** begin;
    void** end;
    void** linkBegin;
    void** linkEnd;
};

struct SandAssignmentSnapshot
{
    bool readable;
    void* begin;
    void* end;
    int index;
    void* target;
};

static SandAssignmentSnapshot SandReadAssignmentSnapshot(void* vehicle)
{
    SandAssignmentSnapshot snapshot = {};
    BYTE* bytes = (BYTE*)vehicle;
    if (!bytes ||
        !ReadablePtr(bytes + SAND_VEHICLE_ASSIGNMENT_BEGIN, sizeof(void*)) ||
        !ReadablePtr(bytes + SAND_VEHICLE_ASSIGNMENT_END, sizeof(void*)) ||
        !ReadablePtr(bytes + SAND_VEHICLE_ASSIGNMENT_INDEX, sizeof(int)) ||
        !ReadablePtr(bytes + SAND_VEHICLE_CURRENT_TARGET, sizeof(void*)))
        return snapshot;

    snapshot.begin = *(void**)(bytes + SAND_VEHICLE_ASSIGNMENT_BEGIN);
    snapshot.end = *(void**)(bytes + SAND_VEHICLE_ASSIGNMENT_END);
    snapshot.index = *(int*)(bytes + SAND_VEHICLE_ASSIGNMENT_INDEX);
    snapshot.target = *(void**)(bytes + SAND_VEHICLE_CURRENT_TARGET);
    snapshot.readable = true;
    return snapshot;
}

static bool SandAssignmentHasActiveSnowWork(
    const SandAssignmentSnapshot& assignment)
{
    if (!assignment.readable || !assignment.begin || !assignment.end)
        return false;
    uintptr_t begin = (uintptr_t)assignment.begin;
    uintptr_t end = (uintptr_t)assignment.end;
    if (end <= begin || ((end - begin) % 0x10) != 0)
        return false;
    size_t count = (size_t)((end - begin) / 0x10);
    return count > 0 && count <= 4096 &&
        assignment.index >= 0 &&
        (size_t)assignment.index < count;
}

static float SandSnowplowClearingSpeedKmh(
    const SandSkillView& skills)
{
    for (size_t i = 0; i < skills.count; ++i)
    {
        if (skills.ids[i] != SAND_SKILL_SNOWPLOW) continue;
        float speed = 0.0f;
        memcpy(&speed, &skills.parameterRaw[i], sizeof(speed));
        if (IsFiniteNonNegative(speed) && speed > 0.0f &&
            speed <= 250.0f)
            return speed;
    }
    return 0.0f;
}

// Departure with an active Technical Services assignment normally starts the
// plough session. A confirmed native clear is the authoritative fallback and
// keep-alive when assignment publication races departure. Current speed is
// retained only for diagnostics; it is not a plough on/off indicator because
// vanilla clears lightly covered roads above the nominal skill-35 speed.
static bool SandMarkQualifiedClearActivity(
    void* vehicle,
    const SandSkillView& skills,
    void* road,
    ULONGLONG now,
    float* currentSpeedKmhOut,
    float* plowSpeedKmhOut)
{
    if (currentSpeedKmhOut) *currentSpeedKmhOut = 0.0f;
    if (plowSpeedKmhOut) *plowSpeedKmhOut = 0.0f;
    if (!vehicle || !skills.valid || !skills.snowplow ||
        !InterlockedCompareExchange(
            &g_sandNativeFuelConsumptionPathVerified, 0, 0))
        return false;

    BYTE* bytes = (BYTE*)vehicle;
    float speedKmh = 0.0f;
    float currentFuel = 0.0f;
    void* currentBuilding = nullptr;
    void* homeBuilding = nullptr;
    void* fuelBuilding = nullptr;
    if (!ReadablePtr(bytes + SAND_VEHICLE_CURRENT_BUILDING, 16) ||
        !ReadablePtr(bytes + SAND_VEHICLE_FUEL_BUILDING, sizeof(void*)) ||
        !ReadablePtr(bytes + SAND_VEHICLE_CURRENT_FUEL, sizeof(float)) ||
        !ReadablePtr(bytes + SAND_VEHICLE_CURRENT_SPEED_KMH,
                     sizeof(float)))
        return false;

    currentBuilding =
        *(void**)(bytes + SAND_VEHICLE_CURRENT_BUILDING);
    homeBuilding =
        *(void**)(bytes + SAND_VEHICLE_HOME_BUILDING);
    fuelBuilding =
        *(void**)(bytes + SAND_VEHICLE_FUEL_BUILDING);
    speedKmh =
        *(float*)(bytes + SAND_VEHICLE_CURRENT_SPEED_KMH);
    currentFuel =
        *(float*)(bytes + SAND_VEHICLE_CURRENT_FUEL);
    if (speedKmh < 0.0f) speedKmh = -speedKmh;
    float plowSpeedKmh = SandSnowplowClearingSpeedKmh(skills);
    if (currentSpeedKmhOut) *currentSpeedKmhOut = speedKmh;
    if (plowSpeedKmhOut) *plowSpeedKmhOut = plowSpeedKmh;

    SandAssignmentSnapshot assignment =
        SandReadAssignmentSnapshot(vehicle);
    bool qualified =
        homeBuilding && !currentBuilding &&
        SandAssignmentHasActiveSnowWork(assignment) &&
        SandIsFiniteSignedNativeFuel(currentFuel) &&
        plowSpeedKmh > 0.0f;

    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    bool refillReturnActive = item && item->returnLockActive &&
        item->returnReason == SAND_RETURN_REFILL;
    if (!item ||
        (item->returnLockActive && !refillReturnActive) ||
        (fuelBuilding &&
         (!refillReturnActive || fuelBuilding != homeBuilding)))
    {
        qualified = false;
    }
    if (item)
    {
        item->nativeLastSpeedKmh = speedKmh;
        item->nativePlowSpeedKmh = plowSpeedKmh;
        bool previousSessionInactive =
            !item->nativeLastQualifiedClearTick;
        if (qualified)
        {
            item->nativeLastQualifiedClearTick = now;
            item->nativeLastQualifiedClearRoad = road;
            if (!item->nativeFuelBaselineValid ||
                previousSessionInactive)
            {
                item->nativeFuelLastAmount = currentFuel;
                item->nativeFuelBaselineValid = 1;
                item->nativeLastConsumptionSampleTick = now;
            }
        }
        else
        {
            // A structurally rejected clear belongs to a manual return, depot
            // or inactive assignment. A refill return remains qualified while
            // the native game is still issuing real snow-clear calls.
            item->nativeLastQualifiedClearTick = 0;
            item->nativeLastQualifiedClearRoad = nullptr;
            item->nativeFuelLastAmount = currentFuel;
            item->nativeFuelBaselineValid = 1;
            item->nativeLastConsumptionSampleTick = now;
        }
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);
    return qualified;
}

static void SandSampleNativeFuelConsumption(
    void* vehicle, ULONGLONG now)
{
    if (!vehicle ||
        !InterlockedCompareExchange(
            &g_sandNativeFuelConsumptionPathVerified, 0, 0))
        return;

    BYTE* bytes = (BYTE*)vehicle;
    float currentFuel = 0.0f;
    float currentSpeedKmh = 0.0f;
    float nativeFuelCapacity = 0.0f;
    void* currentBuilding = nullptr;
    void* homeBuilding = nullptr;
    void* fuelBuilding = nullptr;
    __try
    {
        if (!ReadablePtr(bytes + SAND_VEHICLE_CURRENT_BUILDING, 16) ||
            !ReadablePtr(bytes + SAND_VEHICLE_FUEL_BUILDING,
                         sizeof(void*)) ||
            !ReadablePtr(bytes + SAND_VEHICLE_CURRENT_FUEL,
                         sizeof(float)) ||
            !ReadablePtr(bytes + SAND_VEHICLE_CURRENT_SPEED_KMH,
                         sizeof(float)) ||
            !ReadablePtr(bytes + SAND_VEHICLE_TYPE_DESCRIPTION,
                         sizeof(void*)))
            return;
        currentBuilding =
            *(void**)(bytes + SAND_VEHICLE_CURRENT_BUILDING);
        homeBuilding =
            *(void**)(bytes + SAND_VEHICLE_HOME_BUILDING);
        fuelBuilding =
            *(void**)(bytes + SAND_VEHICLE_FUEL_BUILDING);
        currentFuel =
            *(float*)(bytes + SAND_VEHICLE_CURRENT_FUEL);
        currentSpeedKmh =
            *(float*)(bytes + SAND_VEHICLE_CURRENT_SPEED_KMH);
        if (currentSpeedKmh < 0.0f)
            currentSpeedKmh = -currentSpeedKmh;
        BYTE* typeDescription =
            *(BYTE**)(bytes + SAND_VEHICLE_TYPE_DESCRIPTION);
        if (!typeDescription ||
            !ReadablePtr(typeDescription + SAND_TYPE_FUEL_CAPACITY,
                         sizeof(float)))
            return;
        nativeFuelCapacity =
            *(float*)(typeDescription + SAND_TYPE_FUEL_CAPACITY);
    }
    __except(FaultFilter("native grit fuel-proportional sampler",
                         GetExceptionInformation()))
    {
        return;
    }

    if (!SandIsFiniteSignedNativeFuel(currentFuel) ||
        !IsFiniteNonNegative(currentSpeedKmh) ||
        !IsFiniteNonNegative(nativeFuelCapacity) ||
        nativeFuelCapacity <= 0.000001f ||
        nativeFuelCapacity > 100000.0f)
        return;

    // Fail closed on corrupted fuel data without rejecting vanilla's observed
    // negative reserve. A generous capacity-relative window covers the game's
    // approximately -50 percent fallback range and a possible full refill jump.
    if (currentFuel < -nativeFuelCapacity * 1.25f ||
        currentFuel > nativeFuelCapacity * 1.25f)
        return;

    SandAssignmentSnapshot assignment =
        SandReadAssignmentSnapshot(vehicle);
    bool activeAssignment =
        SandAssignmentHasActiveSnowWork(assignment);
    bool debitApplied = false;
    bool thresholdTriggered = false;
    bool nativeRefillDetected = false;
    bool shouldLog = false;
    float fuelDelta = 0.0f;
    float nativeFuelBefore = currentFuel;
    float debitKg = 0.0f;
    float beforeKg = 0.0f;
    float afterKg = 0.0f;
    float capacityKg = 0.0f;
    float plowSpeedKmh = 0.0f;

    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->snowplow && item->shadowTankInitialized)
    {
        if (item->nativeLastConsumptionSampleTick &&
            now - item->nativeLastConsumptionSampleTick <
                SAND_FUEL_CONSUMPTION_SAMPLE_MS)
        {
            ReleaseSRWLockExclusive(&g_sandTrackerLock);
            return;
        }
        item->nativeLastConsumptionSampleTick = now;
        item->nativeLastSpeedKmh = currentSpeedKmh;
        item->nativeFuelCapacity = nativeFuelCapacity;
        plowSpeedKmh = item->nativePlowSpeedKmh;
        bool plowSessionActive =
            item->nativeLastQualifiedClearTick != 0;
        bool refillReturnActive = item->returnLockActive &&
            item->returnReason == SAND_RETURN_REFILL;
        bool returnStateAllowsWork =
            !item->returnLockActive || refillReturnActive;
        bool fuelStateAllowsWork =
            !fuelBuilding ||
            (refillReturnActive && fuelBuilding == homeBuilding);
        bool structurallyEligible =
            plowSessionActive && activeAssignment &&
            homeBuilding && !currentBuilding && fuelStateAllowsWork &&
            returnStateAllowsWork &&
            !item->shadowTankReturnThresholdPending &&
            item->shadowTankRemainingKg > 0.000001f;
        // Speed is used only to suppress stationary engine consumption. There
        // is deliberately no upper limit: snow depth controls vanilla's
        // clearing speed and a fast confirmed plough remains valid work.
        bool moving =
            currentSpeedKmh >= SAND_MINIMUM_PLOW_SPEED_KMH;
        bool eligible = structurallyEligible && moving;

        if (plowSessionActive && !structurallyEligible)
        {
            // Assignment completion, depot/manual-return state or an empty
            // tank ends fuel-proportional debit. A refill return does not end
            // the work session while native clear calls keep confirming that
            // the plough is still working. Merely stopping never ends it.
            item->nativeLastQualifiedClearTick = 0;
            item->nativeLastQualifiedClearRoad = nullptr;
        }

        if (!item->nativeFuelBaselineValid)
        {
            item->nativeFuelBaselineValid = 1;
            item->nativeFuelLastAmount = currentFuel;
        }
        else
        {
            nativeFuelBefore = item->nativeFuelLastAmount;
            fuelDelta = nativeFuelBefore - currentFuel;
            float refuelIncrease = currentFuel - nativeFuelBefore;
            float refuelEpsilon = nativeFuelCapacity * 0.00001f;
            if (refuelEpsilon < 0.000001f)
                refuelEpsilon = 0.000001f;
            nativeRefillDetected =
                item->returnLockActive &&
                item->homeBuilding == homeBuilding &&
                item->returnFuelBuildingLatchWrites > 0 &&
                refuelIncrease > refuelEpsilon &&
                currentFuel <= nativeFuelCapacity * 1.25f;
            item->nativeFuelLastAmount = currentFuel;
            if (eligible && fuelDelta > 0.000001f &&
                fuelDelta <= nativeFuelCapacity * 1.25f)
            {
                capacityKg = item->shadowTankCapacityKg;
                beforeKg = item->shadowTankRemainingKg;
                debitKg = capacityKg *
                    (fuelDelta / nativeFuelCapacity) *
                    ((float)g_sandFuelConsumptionFactorPercent / 100.0f);
                if (IsFiniteNonNegative(debitKg) &&
                    debitKg > 0.000001f)
                {
                    if (debitKg > beforeKg) debitKg = beforeKg;
                    afterKg = beforeKg - debitKg;
                    // The native fuel reserve may be negative, but grit never
                    // is: preserve the visible and simulation invariant at 0.
                    if (afterKg < 0.000001f) afterKg = 0.0f;
                    item->shadowTankPendingConsumedKg += debitKg;
                    item->shadowTankRemainingKg = afterKg;
                    item->shadowTankUseEvents++;
                    item->nativeFuelConsumptionSamples++;
                    debitApplied = true;
                    float thresholdKg = capacityKg *
                        ((float)g_sandReturnThresholdBasisPoints /
                         10000.0f);
                    if (!item->shadowTankReturnThresholdCrossed &&
                        afterKg <= thresholdKg + 0.000001f)
                    {
                        // The reserve is a one-shot internal return trigger.
                        // It never changes the visible or actual tank amount.
                        item->shadowTankReturnThresholdCrossed = 1;
                        item->shadowTankReturnThresholdPending = 1;
                        thresholdTriggered = true;
                    }
                    if (afterKg <= 0.000001f)
                        item->shadowTankDryPlowing = 1;
                    shouldLog = thresholdTriggered ||
                        !item->nativeLastConsumptionLogTick ||
                        now - item->nativeLastConsumptionLogTick >= 1000;
                    if (shouldLog)
                        item->nativeLastConsumptionLogTick = now;
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    if (nativeRefillDetected)
    {
        SandCompleteReturnFromNativeFuelIncrease(
            vehicle, currentBuilding, homeBuilding, fuelBuilding,
            nativeFuelBefore, currentFuel, nativeFuelCapacity);
    }
    if (!debitApplied) return;
    InterlockedIncrement64(&g_sandShadowTankUseEvents);
    InterlockedIncrement64(&g_sandNativeFuelConsumptionSamples);
    InterlockedIncrement64(&g_sandNativeFuelConsumptionDebits);
    if (thresholdTriggered)
        InterlockedIncrement64(
            &g_sandNativeFuelConsumptionThresholds);
    if (shouldLog)
    {
        Report("INFO", "Tank diagnostic",
            thresholdTriggered ?
                "native-fuel-grit-threshold" :
                "native-fuel-grit-consumption",
            "vehicle=%p native_fuel_before=%.6f native_fuel_after=%.6f native_fuel_delta=%.6f native_fuel_capacity=%.3f factor_percent=%d grit_before_kg=%.3f grit_debit_kg=%.3f grit_after_kg=%.3f grit_capacity_kg=%.3f current_speed_kmh=%.3f plow_speed_kmh=%.3f speed_role=stationary-suppression-only upper_speed_limit=none active_assignment=1 plow_session=active signed_native_fuel_supported=1 return_threshold_triggered=%d visible_amount_zero=%d reserve_amount_preserved=1 real_fuel_mutation=0",
            vehicle, currentFuel + fuelDelta, currentFuel,
            fuelDelta, nativeFuelCapacity,
            g_sandFuelConsumptionFactorPercent,
            beforeKg, debitKg, afterKg, capacityKg,
            currentSpeedKmh, plowSpeedKmh,
            thresholdTriggered ? 1 : 0,
            afterKg <= 0.000001f ? 1 : 0);
    }
}

static void SandSampleNativeFuelConsumptionForTrackedVehicles(
    ULONGLONG now)
{
    void* vehicles[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
    size_t count = 0;
    AcquireSRWLockShared(&g_sandTrackerLock);
    for (size_t i = 0;
         i < SAND_DIAG_MAX_TRACKED_VEHICLES &&
         count < SAND_DIAG_MAX_TRACKED_VEHICLES; ++i)
    {
        const SandTrackedVehicle& item = g_sandTrackedVehicles[i];
        if (item.vehicle && item.snowplow &&
            item.shadowTankInitialized)
            vehicles[count++] = item.vehicle;
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);
    for (size_t i = 0; i < count; ++i)
        SandSampleNativeFuelConsumption(vehicles[i], now);
}

static bool SandReadPointerVector(BYTE* vehicle, size_t beginOffset,
                                  size_t endOffset, void*** beginOut,
                                  void*** endOut, size_t* countOut)
{
    if (beginOut) *beginOut = nullptr;
    if (endOut) *endOut = nullptr;
    if (countOut) *countOut = 0;
    if (!vehicle || !ReadablePtr(vehicle + beginOffset, sizeof(void*)) ||
        !ReadablePtr(vehicle + endOffset, sizeof(void*)))
        return false;

    void** begin = *(void***)(vehicle + beginOffset);
    void** end = *(void***)(vehicle + endOffset);

    if ((!begin && end) || (begin && !end))
        return false;
    size_t count = 0;
    if (begin)
    {
        uintptr_t beginAddress = (uintptr_t)begin;
        uintptr_t endAddress = (uintptr_t)end;
        if (endAddress < beginAddress ||
            ((endAddress - beginAddress) % sizeof(void*)) != 0)
            return false;
        count = (size_t)((endAddress - beginAddress) / sizeof(void*));
        if (count > 100000 ||
            (count && !ReadablePtr(begin, count * sizeof(void*))))
            return false;
    }

    if (beginOut) *beginOut = begin;
    if (endOut) *endOut = end;
    if (countOut) *countOut = count;
    return true;
}

static SandRouteVectorState SandReadRouteVectorState(void* vehicle)
{
    SandRouteVectorState state = {};
    state.index = -1;
    BYTE* bytes = (BYTE*)vehicle;
    if (!bytes ||
        !ReadablePtr(bytes + SAND_VEHICLE_ROUTE_INDEX, sizeof(int)))
        return state;

    state.index = *(int*)(bytes + SAND_VEHICLE_ROUTE_INDEX);
    bool mainReadable = SandReadPointerVector(
        bytes, SAND_VEHICLE_ROUTE_BEGIN, SAND_VEHICLE_ROUTE_END,
        &state.begin, &state.end, &state.count);
    bool linksReadable = SandReadPointerVector(
        bytes, SAND_VEHICLE_ROUTE_LINK_BEGIN,
        SAND_VEHICLE_ROUTE_LINK_END,
        &state.linkBegin, &state.linkEnd, &state.linkCount);
    // Road routes normally contain N nodes and N-1 link records. Some short
    // transitional routes contain N/N. Any other relationship is unsafe.
    if (!mainReadable || !linksReadable ||
        state.linkCount > state.count ||
        state.count - state.linkCount > 1)
        return state;

    state.readable = true;
    return state;
}

enum SandRouteTrimStatus
{
    SAND_ROUTE_TRIM_INVALID = 0,
    SAND_ROUTE_TRIM_ALREADY_AT_TAIL,
    SAND_ROUTE_TRIM_OK,
    SAND_ROUTE_TRIM_HELPER_UNAVAILABLE,
    SAND_ROUTE_TRIM_FIELDS_UNWRITABLE,
    SAND_ROUTE_TRIM_VERIFY_FAILED
};

static const char* SandRouteTrimStatusName(int status)
{
    switch (status)
    {
        case SAND_ROUTE_TRIM_ALREADY_AT_TAIL: return "already-at-current-tail";
        case SAND_ROUTE_TRIM_OK: return "pending-tail-discarded";
        case SAND_ROUTE_TRIM_HELPER_UNAVAILABLE: return "cleanup-helper-unavailable";
        case SAND_ROUTE_TRIM_FIELDS_UNWRITABLE: return "route-end-fields-unwritable";
        case SAND_ROUTE_TRIM_VERIFY_FAILED: return "post-write-verification-failed";
        default: return "invalid-route-state";
    }
}

struct SandRouteTrimResult
{
    int status;
    size_t discarded;
    SandRouteVectorState before;
    SandRouteVectorState after;
};

static SandRouteTrimResult SandTrimPendingRouteAtCurrentNode(void* vehicle)
{
    SandRouteTrimResult result = {};
    result.before = SandReadRouteVectorState(vehicle);
    if (!result.before.readable || result.before.index < 0 ||
        (size_t)result.before.index >= result.before.count)
    {
        result.status = SAND_ROUTE_TRIM_INVALID;
        return result;
    }

    size_t keepCount = (size_t)result.before.index + 1;
    size_t originalGap = result.before.count - result.before.linkCount;
    size_t keepLinkCount = keepCount >= originalGap
        ? keepCount - originalGap : 0;
    if (keepCount >= result.before.count)
    {
        result.status = SAND_ROUTE_TRIM_ALREADY_AT_TAIL;
        result.after = result.before;
        return result;
    }
    if (!g_sandCleanRouteLinks)
    {
        result.status = SAND_ROUTE_TRIM_HELPER_UNAVAILABLE;
        return result;
    }

    BYTE* bytes = (BYTE*)vehicle;
    if (!SandWritableMemory(bytes + SAND_VEHICLE_ROUTE_END,
                            sizeof(void*)) ||
        !SandWritableMemory(bytes + SAND_VEHICLE_ROUTE_LINK_END,
                            sizeof(void*)))
    {
        result.status = SAND_ROUTE_TRIM_FIELDS_UNWRITABLE;
        return result;
    }

    // This is the same cleanup call the game makes before replacing a vehicle
    // route. It unregisters the old road/link relationships. The current node
    // remains as the pathfinder origin; only nodes after it are hidden from the
    // two synchronized route vectors.
    g_sandCleanRouteLinks(vehicle, true);
    InterlockedExchangePointer(
        (PVOID volatile*)(bytes + SAND_VEHICLE_ROUTE_END),
        result.before.begin + keepCount);
    InterlockedExchangePointer(
        (PVOID volatile*)(bytes + SAND_VEHICLE_ROUTE_LINK_END),
        result.before.linkBegin + keepLinkCount);

    result.discarded = result.before.count - keepCount;
    result.after = SandReadRouteVectorState(vehicle);
    if (!result.after.readable || result.after.index != result.before.index ||
        result.after.count != keepCount ||
        result.after.linkCount != keepLinkCount)
    {
        result.status = SAND_ROUTE_TRIM_VERIFY_FAILED;
        return result;
    }
    result.status = SAND_ROUTE_TRIM_OK;
    return result;
}

struct SandArrivalRouteCleanupResult
{
    bool success;
    bool nativeCleanupCalled;
    SandRouteVectorState before;
    SandRouteVectorState after;
};

// Caller holds g_sandTrackerLock exclusively. Keep every field which controls
// route assertion in one reset so the empty-route fast path and the native
// game-window cleanup cannot diverge again.
static void SandReleaseCompletedReturnLockLocked(SandTrackedVehicle* item)
{
    if (!item) return;
    item->returnLockActive = 0;
    item->returnStartPending = 0;
    item->returnRouteSeen = 0;
    item->returnNextDispatchTick = 0;
    item->returnRetryRequiresProgress = 0;
    item->returnBlindRetryAvailable = 0;
    item->returnRetryRouteIndex = -1;
    item->returnRetryRouteCount = 0;
    item->returnRetryRouteLinkCount = 0;
    item->returnRetryAssignmentBegin = nullptr;
    item->returnRetryAssignmentEnd = nullptr;
    item->returnRetryAssignmentIndex = -1;
    item->returnRetryTarget = nullptr;
    item->returnTailObservedTick = 0;
    item->returnNextTailRecoveryTick = 0;
    item->returnPlannedRouteBegin = nullptr;
    item->returnPlannedRouteEnd = nullptr;
    item->returnPlannedLinkBegin = nullptr;
    item->returnPlannedLinkEnd = nullptr;
    item->returnLastRouteIndex = -1;
    item->returnLastRouteCount = 0;
    item->returnLastRouteLinkCount = 0;
    item->returnFinishRoad = nullptr;
    item->returnFinishRoadClearPending = 0;
    item->returnFinishRoadClears = 0;
    item->returnFuelBuildingLatchWrites = 0;
    item->returnLastFuelBuildingLatchLogTick = 0;
    item->returnArrivalPending = 0;
    item->returnArrivalDetectedTick = 0;
}

// The verified native route-reset paths call the relationship cleanup and then
// set both active route-vector ends back to their begins. Do the same only after
// the vehicle has remained inside its own depot for the configured settlement
// period and only from the game window thread. The worker observer merely
// queues this operation and never calls native route helpers itself.
static SandArrivalRouteCleanupResult SandCleanupCompletedHomeRoute(
    void* vehicle)
{
    SandArrivalRouteCleanupResult result = {};
    result.before = SandReadRouteVectorState(vehicle);
    if (!result.before.readable)
        return result;

    if (result.before.count == 0 && result.before.linkCount == 0)
    {
        result.after = result.before;
        result.success = true;
        return result;
    }
    if (!g_sandCleanRouteLinks)
        return result;

    BYTE* bytes = (BYTE*)vehicle;
    if (!SandWritableMemory(bytes + SAND_VEHICLE_ROUTE_END,
                            sizeof(void*)) ||
        !SandWritableMemory(bytes + SAND_VEHICLE_ROUTE_LINK_END,
                            sizeof(void*)))
        return result;

    g_sandCleanRouteLinks(vehicle, true);
    result.nativeCleanupCalled = true;
    InterlockedExchangePointer(
        (PVOID volatile*)(bytes + SAND_VEHICLE_ROUTE_END),
        result.before.begin);
    InterlockedExchangePointer(
        (PVOID volatile*)(bytes + SAND_VEHICLE_ROUTE_LINK_END),
        result.before.linkBegin);

    result.after = SandReadRouteVectorState(vehicle);
    result.success = result.after.readable &&
                     result.after.count == 0 &&
                     result.after.linkCount == 0;
    return result;
}

static BOOL CALLBACK SandFindOwnTopLevelWindowProc(
    HWND window, LPARAM parameter)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId() ||
        GetWindow(window, GW_OWNER) != nullptr ||
        !IsWindowVisible(window))
        return TRUE;
    HWND* output = (HWND*)parameter;
    if (output) *output = window;
    return FALSE;
}

static HWND SandFindOwnTopLevelWindow()
{
    HWND window = nullptr;
    EnumWindows(&SandFindOwnTopLevelWindowProc, (LPARAM)&window);
    return window;
}

static void SandQueueReturnArrivalFinalization();
static void SandQueueReturnRetry();
static bool SandPrepareReturnAction(void* vehicle,
                                    void* currentBuilding,
                                    void* homeBuilding,
                                    ULONGLONG now,
                                    int* actionKindOut,
                                    int* reasonOut,
                                    DWORD* stateOut);
static void SandExecuteReturnAction(void* vehicle,
                                    int actionKind, int reason,
                                    DWORD previousState);

static void CALLBACK SandReturnArrivalTimerProc(
    HWND window, UINT, UINT_PTR timerId, DWORD)
{
    KillTimer(window, timerId);
    InterlockedExchange(&g_sandArrivalTimerQueued, 0);
    if (!g_runtimeActive) return;

    DWORD currentThread = GetCurrentThreadId();
    DWORD clearThread = (DWORD)InterlockedCompareExchange(
        &g_sandClearThreadId, 0, 0);
    DWORD uiThread = (DWORD)InterlockedCompareExchange(
        &g_uiThreadId, 0, 0);
    if (clearThread && currentThread != clearThread &&
        (!uiThread || currentThread != uiThread) &&
        InterlockedCompareExchange(
            &g_sandArrivalThreadWarningLogged, 1, 0) == 0)
    {
        Report("WARN", "Tank diagnostic", "return-arrival-thread",
            "window_thread=%lu clear_thread=%lu ui_thread=%lu; the timer still runs serially on the game's owning window thread, but the observed clear callback used another thread",
            (unsigned long)currentThread,
            (unsigned long)clearThread,
            (unsigned long)uiThread);
    }

    void* vehicles[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
    size_t count = 0;
    ULONGLONG now = GetTickCount64();
    bool reschedule = false;
    AcquireSRWLockShared(&g_sandTrackerLock);
    for (size_t i = 0;
         i < SAND_DIAG_MAX_TRACKED_VEHICLES &&
         count < SAND_DIAG_MAX_TRACKED_VEHICLES; ++i)
    {
        const SandTrackedVehicle& item = g_sandTrackedVehicles[i];
        if (!item.vehicle || !item.returnLockActive ||
            !item.returnArrivalPending)
            continue;
        if (now - item.returnArrivalDetectedTick <
            (ULONGLONG)g_sandReturnArrivalSettleMs)
        {
            reschedule = true;
            continue;
        }
        vehicles[count++] = item.vehicle;
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);

    for (size_t i = 0; i < count; ++i)
    {
        BYTE* vehicle = (BYTE*)vehicles[i];
        __try
        {
            if (!ReadablePtr(
                    vehicle + SAND_VEHICLE_CURRENT_BUILDING,
                    sizeof(void*)) ||
                !ReadablePtr(
                    vehicle + SAND_VEHICLE_HOME_BUILDING,
                    sizeof(void*)))
            {
                reschedule = true;
                continue;
            }
            void* currentBuilding =
                *(void**)(vehicle + SAND_VEHICLE_CURRENT_BUILDING);
            void* homeBuilding =
                *(void**)(vehicle + SAND_VEHICLE_HOME_BUILDING);
            if (!currentBuilding || !homeBuilding ||
                currentBuilding != homeBuilding)
            {
                int reason = SAND_RETURN_NONE;
                AcquireSRWLockExclusive(&g_sandTrackerLock);
                SandTrackedVehicle* item =
                    SandFindTrackedVehicleLocked(vehicle, false);
                if (item && item->returnLockActive &&
                    item->returnArrivalPending)
                {
                    reason = item->returnReason;
                    item->returnArrivalPending = 0;
                    item->returnArrivalDetectedTick = 0;
                    item->returnRouteSeen = 0;
                    item->returnStartPending = 1;
                    item->returnNextDispatchTick = now;
                }
                ReleaseSRWLockExclusive(&g_sandTrackerLock);
                Report("WARN", "Tank diagnostic",
                    "return-lock-arrival-interrupted",
                    "vehicle=%p home_building=%p current_building=%p reason=%s left_before_route_cleanup=1 lock_retained=1 reassert_home_pending=1",
                    vehicle, homeBuilding, currentBuilding,
                    SandReturnReasonName(reason));
                continue;
            }

            SandAssignmentSnapshot assignmentBefore =
                SandReadAssignmentSnapshot(vehicle);
            SandArrivalRouteCleanupResult cleanup =
                SandCleanupCompletedHomeRoute(vehicle);
            SandAssignmentSnapshot assignmentAfter =
                SandReadAssignmentSnapshot(vehicle);
            int reason = SAND_RETURN_NONE;
            int attempts = 0;
            bool released = false;
            AcquireSRWLockExclusive(&g_sandTrackerLock);
            SandTrackedVehicle* item =
                SandFindTrackedVehicleLocked(vehicle, false);
            if (item && item->returnLockActive &&
                item->returnArrivalPending)
            {
                reason = item->returnReason;
                attempts = ++item->returnArrivalFinalizeAttempts;
                if (cleanup.success)
                {
                    SandReleaseCompletedReturnLockLocked(item);
                    released = true;
                }
            }
            ReleaseSRWLockExclusive(&g_sandTrackerLock);

            if (released)
            {
                InterlockedIncrement64(&g_sandReturnArrivals);
                InterlockedIncrement64(&g_sandReturnArrivalCleanups);
                Report("INFO", "Tank diagnostic", "return-lock-arrived",
                    "vehicle=%p home_building=%p reason=%s settle_ms=%d finalize_attempts=%d game_window_thread=%lu route_cleanup=%s route_before_readable=%d route_index_before=%d route_count_before=%llu route_link_count_before=%llu route_after_readable=%d route_index_after=%d route_count_after=%llu route_link_count_after=%llu assignment_before_readable=%d assignment_before_index=%d assignment_before_target=%p assignment_after_readable=%d assignment_after_index=%d assignment_after_target=%p lock_released=1 stale_home_route_retained=0",
                    vehicle, homeBuilding,
                    SandReturnReasonName(reason),
                    g_sandReturnArrivalSettleMs, attempts,
                    (unsigned long)currentThread,
                    cleanup.nativeCleanupCalled ?
                        "native-relationships-and-route-vectors" :
                        "route-already-empty",
                    cleanup.before.readable ? 1 : 0,
                    cleanup.before.index,
                    (unsigned long long)cleanup.before.count,
                    (unsigned long long)cleanup.before.linkCount,
                    cleanup.after.readable ? 1 : 0,
                    cleanup.after.index,
                    (unsigned long long)cleanup.after.count,
                    (unsigned long long)cleanup.after.linkCount,
                    assignmentBefore.readable ? 1 : 0,
                    assignmentBefore.index,
                    assignmentBefore.target,
                    assignmentAfter.readable ? 1 : 0,
                    assignmentAfter.index,
                    assignmentAfter.target);
            }
            else
            {
                reschedule = true;
                if (attempts == 1 || attempts % 10 == 0)
                {
                    Report("WARN", "Tank diagnostic",
                        "return-lock-arrival-cleanup-pending",
                        "vehicle=%p home_building=%p reason=%s finalize_attempts=%d route_before_readable=%d route_index_before=%d route_count_before=%llu route_link_count_before=%llu native_cleanup_called=%d route_after_readable=%d route_count_after=%llu route_link_count_after=%llu lock_released=0 retry_pending=1",
                        vehicle, homeBuilding,
                        SandReturnReasonName(reason), attempts,
                        cleanup.before.readable ? 1 : 0,
                        cleanup.before.index,
                        (unsigned long long)cleanup.before.count,
                        (unsigned long long)cleanup.before.linkCount,
                        cleanup.nativeCleanupCalled ? 1 : 0,
                        cleanup.after.readable ? 1 : 0,
                        (unsigned long long)cleanup.after.count,
                        (unsigned long long)cleanup.after.linkCount);
                }
            }
        }
        __except(FaultFilter(
            "sand spreader settled arrival route cleanup",
            GetExceptionInformation()))
        {
            reschedule = true;
        }
    }

    if (reschedule)
        SandQueueReturnArrivalFinalization();
}

static void SandQueueReturnArrivalFinalization()
{
    if (!g_runtimeActive ||
        InterlockedCompareExchange(
            &g_sandArrivalTimerQueued, 1, 0) != 0)
        return;

    HWND window = g_sandArrivalTimerWindow;
    if (!window || !IsWindow(window))
    {
        window = SandFindOwnTopLevelWindow();
        g_sandArrivalTimerWindow = window;
    }
    if (!window || !SetTimer(
            window, SAND_ARRIVAL_TIMER_ID,
            (UINT)g_sandReturnArrivalSettleMs,
            &SandReturnArrivalTimerProc))
    {
        DWORD error = GetLastError();
        InterlockedExchange(&g_sandArrivalTimerQueued, 0);
        ReportWindows("WARN", "Tank diagnostic",
            "return-arrival-timer",
            "The settled-arrival route cleanup could not be queued on the game window thread",
            error,
            "The return lock stays active and the observer will retry; no native helper is called from the worker thread");
    }
}

// Keep every active return under a lightweight owning-window watch. Besides
// progress-gated retries, this detects a stable route tail outside the depot
// even when no later snow-clear callback occurs. The worker thread still
// performs reads only and never invokes a native route helper.
static void CALLBACK SandReturnRetryTimerProc(
    HWND window, UINT, UINT_PTR timerId, DWORD)
{
    KillTimer(window, timerId);
    InterlockedExchange(&g_sandReturnRetryTimerQueued, 0);
    if (!g_runtimeActive) return;

    if (InterlockedCompareExchange(
            &g_sandReturnDispatchInProgress, 1, 0) != 0)
    {
        SandQueueReturnRetry();
        return;
    }

    void* vehicles[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
    size_t count = 0;
    ULONGLONG now = GetTickCount64();
    AcquireSRWLockShared(&g_sandTrackerLock);
    for (size_t i = 0;
         i < SAND_DIAG_MAX_TRACKED_VEHICLES &&
         count < SAND_DIAG_MAX_TRACKED_VEHICLES; ++i)
    {
        const SandTrackedVehicle& item = g_sandTrackedVehicles[i];
        if (!item.vehicle || !item.returnLockActive ||
            item.returnArrivalPending)
            continue;
        vehicles[count++] = item.vehicle;
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);

    for (size_t i = 0; i < count; ++i)
    {
        BYTE* vehicle = (BYTE*)vehicles[i];
        __try
        {
            if (!ReadablePtr(
                    vehicle + SAND_VEHICLE_CURRENT_BUILDING,
                    sizeof(void*)) ||
                !ReadablePtr(
                    vehicle + SAND_VEHICLE_HOME_BUILDING,
                    sizeof(void*)))
                continue;
            void* currentBuilding =
                *(void**)(vehicle + SAND_VEHICLE_CURRENT_BUILDING);
            void* homeBuilding =
                *(void**)(vehicle + SAND_VEHICLE_HOME_BUILDING);
            int actionKind = 0;
            int reason = SAND_RETURN_NONE;
            DWORD previousState = 0;
            if (SandPrepareReturnAction(
                    vehicle, currentBuilding, homeBuilding, now,
                    &actionKind, &reason, &previousState))
            {
                Report("INFO", "Tank diagnostic",
                    "return-timer-dispatch",
                    "vehicle=%p current_building=%p home_building=%p reason=%s action=%s window_thread=%lu configured_retry_ms=%d callback_independent=1",
                    vehicle, currentBuilding, homeBuilding,
                    SandReturnReasonName(reason),
                    actionKind == 2 ? "reassert-home" : "plan-home",
                    (unsigned long)GetCurrentThreadId(),
                    g_sandReturnRetryIntervalMs);
                SandExecuteReturnAction(
                    vehicle, actionKind, reason, previousState);
            }
        }
        __except(FaultFilter(
            "sand spreader game-window return retry",
            GetExceptionInformation()))
        {
            Report("WARN", "Tank diagnostic",
                "return-timer-dispatch-fault",
                "vehicle=%p game-window retry faulted; return lock retained and retry remains pending",
                vehicle);
        }
    }

    bool watchPending = false;
    AcquireSRWLockShared(&g_sandTrackerLock);
    for (size_t i = 0; i < SAND_DIAG_MAX_TRACKED_VEHICLES; ++i)
    {
        const SandTrackedVehicle& item = g_sandTrackedVehicles[i];
        if (item.vehicle && item.returnLockActive &&
            !item.returnArrivalPending)
        {
            watchPending = true;
            break;
        }
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);
    InterlockedExchange(&g_sandReturnDispatchInProgress, 0);

    if (watchPending)
        SandQueueReturnRetry();
}

static void SandQueueReturnRetry()
{
    if (!g_runtimeActive ||
        InterlockedCompareExchange(
            &g_sandReturnRetryTimerQueued, 1, 0) != 0)
        return;

    HWND window = g_sandArrivalTimerWindow;
    if (!window || !IsWindow(window))
    {
        window = SandFindOwnTopLevelWindow();
        g_sandArrivalTimerWindow = window;
    }
    if (!window || !SetTimer(
            window, SAND_RETURN_RETRY_TIMER_ID,
            (UINT)g_sandReturnRetryIntervalMs,
            &SandReturnRetryTimerProc))
    {
        DWORD error = GetLastError();
        InterlockedExchange(&g_sandReturnRetryTimerQueued, 0);
        ReportWindows("WARN", "Tank diagnostic",
            "return-retry-timer",
            "A failed home-route plan could not queue its retry on the game window thread",
            error,
            "The return lock remains active and a later verified game callback will retry");
    }
}

static bool SandHasActiveReturnLock()
{
    bool active = false;
    AcquireSRWLockShared(&g_sandTrackerLock);
    for (size_t i = 0; i < SAND_DIAG_MAX_TRACKED_VEHICLES; ++i)
    {
        if (g_sandTrackedVehicles[i].vehicle &&
            g_sandTrackedVehicles[i].returnLockActive)
        {
            active = true;
            break;
        }
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);
    return active;
}

static bool SandVehicleReturnLockActive(void* vehicle)
{
    if (!vehicle) return false;
    bool active = false;
    AcquireSRWLockShared(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    active = item && item->returnLockActive;
    ReleaseSRWLockShared(&g_sandTrackerLock);
    return active;
}

static bool SandVehicleRefillReturnActive(void* vehicle)
{
    if (!vehicle) return false;
    bool active = false;
    AcquireSRWLockShared(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    active = item && item->returnLockActive &&
        item->returnReason == SAND_RETURN_REFILL;
    ReleaseSRWLockShared(&g_sandTrackerLock);
    return active;
}

// Adopt vanilla's persistent low-fuel destination state without altering the
// real fuel value. This function is called only from the two verified
// snow-clear call sites, therefore the write happens on the owning simulation
// thread. The normal snowplow update clears +0x5E8 during some target changes
// before it reaches those calls; reasserting the home pointer here leaves it
// present for the later route-completion branch at exe+0x69D34D/+0x69DC4E.
static bool SandLatchNativeFuelBuildingToHome(void* vehicle,
                                              const char* source)
{
    if (!vehicle || !g_sandAutomaticReturnEnabled ||
        !g_sandReturnLockEnabled)
        return false;

    int reason = SAND_RETURN_NONE;
    bool active = false;
    AcquireSRWLockShared(&g_sandTrackerLock);
    SandTrackedVehicle* tracked =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (tracked && tracked->returnLockActive)
    {
        active = true;
        reason = tracked->returnReason;
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);
    if (!active) return false;

    DWORD currentThread = GetCurrentThreadId();
    LONG clearThread = InterlockedCompareExchange(
        &g_sandClearThreadId, 0, 0);
    if (clearThread && currentThread != (DWORD)clearThread)
    {
        Report("WARN", "Tank diagnostic",
            "native-refuel-destination-thread-rejected",
            "source=%s vehicle=%p current_thread=%lu clear_thread=%ld write_skipped=1 return_lock_retained=1",
            source ? source : "unknown", vehicle,
            (unsigned long)currentThread, clearThread);
        return false;
    }

    BYTE* bytes = (BYTE*)vehicle;
    const size_t readableBytes =
        SAND_VEHICLE_FUEL_BUILDING + sizeof(void*) -
        SAND_VEHICLE_CURRENT_BUILDING;
    if (!ReadablePtr(bytes + SAND_VEHICLE_CURRENT_BUILDING,
                     readableBytes))
        return false;

    void* currentBuilding = nullptr;
    void* homeBuilding = nullptr;
    void* targetBuilding = nullptr;
    void* fuelBuildingBefore = nullptr;
    void* fuelBuildingAfter = nullptr;
    bool changed = false;
    __try
    {
        currentBuilding =
            *(void**)(bytes + SAND_VEHICLE_CURRENT_BUILDING);
        homeBuilding =
            *(void**)(bytes + SAND_VEHICLE_HOME_BUILDING);
        targetBuilding =
            *(void**)(bytes + SAND_VEHICLE_TARGET_BUILDING);
        if (!homeBuilding || currentBuilding == homeBuilding)
            return false;

        PVOID volatile* slot = (PVOID volatile*)(
            bytes + SAND_VEHICLE_FUEL_BUILDING);
        fuelBuildingBefore = InterlockedCompareExchangePointer(
            slot, nullptr, nullptr);
        if (fuelBuildingBefore != homeBuilding)
        {
            InterlockedExchangePointer(slot, homeBuilding);
            changed = true;
        }
        fuelBuildingAfter = InterlockedCompareExchangePointer(
            slot, nullptr, nullptr);
    }
    __except(FaultFilter("native grit refuel destination latch",
                         GetExceptionInformation()))
    {
        Report("WARN", "Tank diagnostic",
            "native-refuel-destination-fault",
            "source=%s vehicle=%p home_building=%p write_faulted=1 return_lock_retained=1",
            source ? source : "unknown", vehicle, homeBuilding);
        return false;
    }

    if (fuelBuildingAfter != homeBuilding)
    {
        Report("WARN", "Tank diagnostic",
            "native-refuel-destination-verify-failed",
            "source=%s vehicle=%p home_building=%p fuel_building_before=%p fuel_building_after=%p return_lock_retained=1",
            source ? source : "unknown", vehicle, homeBuilding,
            fuelBuildingBefore, fuelBuildingAfter);
        return false;
    }
    if (!changed) return true;

    ULONGLONG now = GetTickCount64();
    LONG64 vehicleWrites = 0;
    bool shouldLog = false;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->returnLockActive)
    {
        vehicleWrites = ++item->returnFuelBuildingLatchWrites;
        shouldLog = vehicleWrites == 1 ||
            now - item->returnLastFuelBuildingLatchLogTick >= 1000;
        if (shouldLog)
            item->returnLastFuelBuildingLatchLogTick = now;
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    LONG64 totalWrites =
        InterlockedIncrement64(&g_sandFuelBuildingLatchWrites);
    LONG64 totalReassertions =
        vehicleWrites > 1 ?
            InterlockedIncrement64(
                &g_sandFuelBuildingLatchReassertions) :
            InterlockedCompareExchange64(
                &g_sandFuelBuildingLatchReassertions, 0, 0);
    if (shouldLog)
    {
        SandRouteVectorState route = SandReadRouteVectorState(vehicle);
        SandAssignmentSnapshot assignment =
            SandReadAssignmentSnapshot(vehicle);
        Report("INFO", "Tank diagnostic",
            "native-refuel-destination-latched",
            "source=%s vehicle=%p reason=%s home_building=%p current_building=%p target_building=%p fuel_building_before=%p fuel_building_after=%p action=%s vehicle_writes=%lld total_writes=%lld total_reassertions=%lld game_thread=%lu real_fuel_unchanged=1 route_unchanged=1 target_unchanged=1 route_readable=%d route_index=%d route_count=%llu assignment_readable=%d assignment_index=%d assignment_target=%p",
            source ? source : "unknown", vehicle,
            SandReturnReasonName(reason), homeBuilding,
            currentBuilding, targetBuilding, fuelBuildingBefore,
            fuelBuildingAfter,
            vehicleWrites > 1 ? "reassert-home" : "set-home",
            (long long)vehicleWrites, (long long)totalWrites,
            (long long)totalReassertions,
            (unsigned long)currentThread,
            route.readable ? 1 : 0, route.index,
            (unsigned long long)route.count,
            assignment.readable ? 1 : 0, assignment.index,
            assignment.target);
    }
    return true;
}

// Retained as reverse-engineering history from v0.1.61. This generic predicate
// is not installed in v0.1.62 because an active Technical Services work chain
// never reaches it. This is the same boolean gate vanilla uses to stop
// assigning work to a
// vehicle whose real fuel is too low. Returning false here does not alter the
// current route. It only prevents another Technical Services task from being
// accepted, after which vanilla's own service lifecycle sends the vehicle to
// its home depot. All vehicles without a plugin refill lock execute the exact
// original predicate through the dedicated relocated trampoline.

static bool SandTakeFinishRoadClearPermission(
    void* vehicle,
    void* road,
    LONG64* vehicleFinishClearsOut,
    LONG64* totalFinishClearsOut,
    int* reasonOut)
{
    if (vehicleFinishClearsOut) *vehicleFinishClearsOut = 0;
    if (totalFinishClearsOut) *totalFinishClearsOut = 0;
    if (reasonOut) *reasonOut = SAND_RETURN_NONE;
    if (!vehicle || !road) return false;

    bool allowed = false;
    LONG64 vehicleFinishClears = 0;
    int reason = SAND_RETURN_NONE;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->returnLockActive)
    {
        reason = item->returnReason;
        if (item->returnFinishRoadClearPending &&
            item->returnFinishRoad == road)
        {
            // Exactly one dry native clear may finish the road object whose
            // preceding clear drained the tank. The permission is consumed
            // before forwarding so re-entrant or later calls stay blocked.
            item->returnFinishRoadClearPending = 0;
            vehicleFinishClears = ++item->returnFinishRoadClears;
            allowed = true;
        }
        else if (item->returnFinishRoadClearPending)
        {
            // The next call belongs to another road, so the original road is
            // no longer being finished and the one-shot permission expires.
            item->returnFinishRoadClearPending = 0;
        }
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    if (!allowed) return false;
    LONG64 totalFinishClears =
        InterlockedIncrement64(&g_sandReturnFinishRoadClears);
    if (vehicleFinishClearsOut)
        *vehicleFinishClearsOut = vehicleFinishClears;
    if (totalFinishClearsOut)
        *totalFinishClearsOut = totalFinishClears;
    if (reasonOut) *reasonOut = reason;
    return true;
}

static void SandMarkBlockedClearDuringReturn(void* vehicle,
                                             void* road,
                                             void* selector)
{
    if (!vehicle) return;
    ULONGLONG now = GetTickCount64();
    LONG64 vehicleBlockedCalls = 0;
    bool shouldLog = false;
    int reason = SAND_RETURN_NONE;
    void* finishRoad = nullptr;
    LONG64 finishRoadClears = 0;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->returnLockActive)
    {
        reason = item->returnReason;
        finishRoad = item->returnFinishRoad;
        finishRoadClears = item->returnFinishRoadClears;
        item->returnFinishRoadClearPending = 0;
        vehicleBlockedCalls = ++item->returnBlockedClearCalls;
        // The vehicle may cross snowy roads while finishing its current native
        // job route. This callback changes neither that route nor its tank.
        shouldLog = vehicleBlockedCalls == 1 ||
            now - item->returnLastViolationLogTick >= 1000;
        if (shouldLog) item->returnLastViolationLogTick = now;
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);
    if (!vehicleBlockedCalls) return;

    LONG64 totalBlocked =
        InterlockedIncrement64(&g_sandReturnBlockedClears);
    if (!shouldLog) return;

    SandRouteVectorState route = SandReadRouteVectorState(vehicle);
    SandAssignmentSnapshot assignment =
        SandReadAssignmentSnapshot(vehicle);
    Report("INFO", "Tank diagnostic", "return-lock-clear-blocked",
        "vehicle=%p road=%p selector=%p reason=%s finish_road=%p finish_road_clears=%lld vehicle_blocked_calls=%lld total_blocked_calls=%lld route_readable=%d route_index=%d route_count=%llu route_link_count=%llu assignment_readable=%d assignment_begin=%p assignment_end=%p assignment_index=%d current_target=%p original_snow_clear_skipped=1 tank_and_treatment_unchanged=1 native_refuel_destination_maintained=1 live_route_preserved=1 plugin_route_dispatch=0",
        vehicle, road, selector, SandReturnReasonName(reason),
        finishRoad, (long long)finishRoadClears,
        (long long)vehicleBlockedCalls, (long long)totalBlocked,
        route.readable ? 1 : 0, route.index,
        (unsigned long long)route.count,
        (unsigned long long)route.linkCount,
        assignment.readable ? 1 : 0,
        assignment.begin, assignment.end,
        assignment.index, assignment.target);
}

static void SandArmReturnLock(void* vehicle, int reason,
                              bool startPending,
                              void* finishRoad,
                              const char* source)
{
    if (!g_sandReturnLockEnabled || !vehicle) return;
    ULONGLONG now = GetTickCount64();
    DWORD routeState = 0;
    bool routeReadable = SandReadReturnRouteState(vehicle, &routeState);
    SandRouteVectorState route = SandReadRouteVectorState(vehicle);
    SandAssignmentSnapshot assignment =
        SandReadAssignmentSnapshot(vehicle);
    bool newlyArmed = false;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, true);
    if (item)
    {
        newlyArmed = !item->returnLockActive;
        item->returnLockActive = 1;
        item->returnStartPending = startPending ? 1 : 0;
        item->returnRouteSeen = 0;
        item->returnReason = reason;
        item->returnRouteState = routeState;
        item->returnRequestedTick = now;
        item->returnNextDispatchTick = now;
        item->returnRetryRequiresProgress = 0;
        item->returnBlindRetryAvailable = 1;
        item->returnRetryRouteIndex = -1;
        item->returnRetryRouteCount = 0;
        item->returnRetryRouteLinkCount = 0;
        item->returnRetryAssignmentBegin = nullptr;
        item->returnRetryAssignmentEnd = nullptr;
        item->returnRetryAssignmentIndex = -1;
        item->returnRetryTarget = nullptr;
        item->returnTailObservedTick = 0;
        item->returnNextTailRecoveryTick = 0;
        item->returnDispatches = 0;
        item->returnPlannedRouteBegin = nullptr;
        item->returnPlannedRouteEnd = nullptr;
        item->returnPlannedLinkBegin = nullptr;
        item->returnPlannedLinkEnd = nullptr;
        item->returnLastRouteIndex = route.readable ? route.index : -1;
        item->returnLastRouteCount = route.readable ? route.count : 0;
        item->returnLastRouteLinkCount =
            route.readable ? route.linkCount : 0;
        item->returnLastAssignmentBegin = assignment.begin;
        item->returnLastAssignmentEnd = assignment.end;
        item->returnLastAssignmentIndex = assignment.index;
        item->returnLastTarget = assignment.target;
        item->returnFinishRoad = finishRoad;
        item->returnFinishRoadClearPending =
            reason == SAND_RETURN_REFILL && finishRoad ? 1 : 0;
        item->returnFinishRoadClears = 0;
        item->returnBlockedClearCalls = 0;
        item->returnLastViolationLogTick = 0;
        item->returnFuelEligibilityBlocks = 0;
        item->returnLastFuelEligibilityLogTick = 0;
        item->returnFuelBuildingLatchWrites = 0;
        item->returnLastFuelBuildingLatchLogTick = 0;
        item->returnArrivalPending = 0;
        item->returnArrivalDetectedTick = 0;
        item->returnArrivalFinalizeAttempts = 0;
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);
    if (!item) return;
    if (newlyArmed) InterlockedIncrement64(&g_sandReturnRequests);
    Report("INFO", "Tank diagnostic", "return-lock-armed",
        "source=%s vehicle=%p reason=%s native_refuel_destination_pending=%d fuel_building_latch=home-depot direct_route_dispatch=0 vanilla_route_owner=1 finish_same_road_once=%d finish_road=%p finish_road_clear_pending=%d route_readable=%d route_index=%d route_count=%llu route_link_count=%llu route_state_readable=%d route_state=0x%08X assignment_readable=%d assignment_begin=%p assignment_end=%p assignment_index=%d current_target=%p plugin_route_retry=0 route_state_diagnostic_only=1",
        source ? source : "unknown", vehicle,
        SandReturnReasonName(reason), startPending ? 1 : 0,
        reason == SAND_RETURN_REFILL && finishRoad ? 1 : 0,
        finishRoad,
        reason == SAND_RETURN_REFILL && finishRoad ? 1 : 0,
        route.readable ? 1 : 0,
        route.index, (unsigned long long)route.count,
        (unsigned long long)route.linkCount,
        routeReadable ? 1 : 0, routeState,
        assignment.readable ? 1 : 0,
        assignment.begin, assignment.end,
        assignment.index, assignment.target);
}

// A vehicle that became empty while every permitted material was empty or OFF
// must not depend on another snow-clear callback to notice a later restock or
// priority change. The global Technical Services observer calls this read-only
// detector once per configured registry scan. It only arms plugin state here;
// the next verified game-thread clear can establish the native refuelling
// destination latch.
static bool SandTryArmPeriodicDryTankReturn(void* vehicle,
                                            void* currentBuilding,
                                            void* homeBuilding)
{
    if (!vehicle || !homeBuilding || currentBuilding == homeBuilding ||
        !g_sandAutomaticReturnEnabled || !g_sandReturnLockEnabled)
        return false;

    bool eligible = false;
    float remainingKg = 0.0f;
    float capacityKg = 0.0f;
    int previousMaterialIndex = -1;
    char previousMaterialName[64] = {};
    AcquireSRWLockShared(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->shadowTankInitialized &&
        item->shadowTankDryPlowing && !item->returnLockActive)
    {
        remainingKg = item->shadowTankRemainingKg;
        capacityKg = item->shadowTankCapacityKg;
        previousMaterialIndex = item->shadowTankMaterialIndex;
        strncpy_s(previousMaterialName, sizeof(previousMaterialName),
                  item->shadowTankResourceName, _TRUNCATE);
        float thresholdKg = capacityKg *
            ((float)g_sandReturnThresholdBasisPoints / 10000.0f);
        eligible = remainingKg <= thresholdKg + 0.000001f;
    }
    ReleaseSRWLockShared(&g_sandTrackerLock);
    if (!eligible) return false;

    SpreaderStorageObservation depotMaterial =
        SpreaderObserveStorage((BYTE*)homeBuilding);
    if (!depotMaterial.found ||
        !IsFiniteNonNegative(depotMaterial.amount) ||
        depotMaterial.amount <= 0.000001f ||
        SandVehicleReturnLockActive(vehicle))
        return false;

    LONG64 reactivationRequests =
        InterlockedIncrement64(&g_sandReturnReactivationRequests);
    SandArmReturnLock(vehicle, SAND_RETURN_REFILL, true, nullptr,
                      "periodic-dry-tank-material-became-available");
    Report("INFO", "Tank diagnostic",
        "periodic-dry-tank-refill-became-available",
        "vehicle=%p current_building=%p home_building=%p previous_tank_material_index=%d previous_tank_material=%s remaining_kg=%.3f capacity_kg=%.3f depot_material_index=%d depot_material=%s depot_material_available_kg=%.3f reactivation_requests=%lld return_requested=1 native_refuel_destination_pending=1 direct_route_dispatch=0 vanilla_route_owner=1 finish_same_road_once=0 observer_native_route_calls=0",
        vehicle, currentBuilding, homeBuilding,
        previousMaterialIndex,
        previousMaterialName[0] ? previousMaterialName : "none",
        remainingKg, capacityKg,
        depotMaterial.materialIndex,
        depotMaterial.resourceName[0] ?
            depotMaterial.resourceName : "none",
        depotMaterial.amount * 1000.0f,
        (long long)reactivationRequests);
    return true;
}

static bool SandPrepareReturnAction(void* vehicle,
                                    void* currentBuilding,
                                    void* homeBuilding,
                                    ULONGLONG now,
                                    int* actionKindOut,
                                    int* reasonOut,
                                    DWORD* stateOut)
{
    if (actionKindOut) *actionKindOut = 0;
    if (reasonOut) *reasonOut = SAND_RETURN_NONE;
    if (stateOut) *stateOut = 0;
    DWORD routeState = 0;
    SandReadReturnRouteState(vehicle, &routeState);
    SandRouteVectorState route = SandReadRouteVectorState(vehicle);
    SandAssignmentSnapshot assignment =
        SandReadAssignmentSnapshot(vehicle);
    bool arrived = currentBuilding && homeBuilding &&
                   currentBuilding == homeBuilding;
    // v0.1.57 never mutates an outside vehicle's route from a panel, timer or
    // background observation. The active lock simply waits for the verified
    // vanilla job-boundary call. Arrival processing below remains unchanged.
    if (SAND_NATURAL_BOUNDARY_RETURN_ENABLED && !arrived)
        return false;
    bool active = false;
    bool arrivalDetected = false;
    bool arrivalPending = false;
    bool arrivalCompletedWithEmptyRoute = false;
    bool arrivalInterrupted = false;
    bool workAssignmentChanged = false;
    bool routeRestartedAfterTail = false;
    int tailRecoveryMode = 0;
    int previousRouteIndex = -1;
    size_t previousRouteCount = 0;
    size_t previousRouteLinkCount = 0;
    void* previousAssignmentBegin = nullptr;
    void* previousAssignmentEnd = nullptr;
    int previousAssignmentIndex = -1;
    void* previousAssignmentTarget = nullptr;
    int actionKind = 0;
    int reason = SAND_RETURN_NONE;
    bool liveRouteAtTail = route.readable && route.count > 0 &&
        route.index >= 0 && (size_t)route.index < route.count &&
        (size_t)(route.index + 2) >= route.count;

    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->returnLockActive)
    {
        active = true;
        reason = item->returnReason;
        item->returnRouteState = routeState;
        if (arrived)
        {
            // The vanilla fuel state machine owns its route and target fields.
            // SandObserveShadowTankState has already performed the synchronous
            // grit refill before this observer reaches us, so only our clear
            // lock is released here. Do not clean or shorten a native fuel
            // route while the game is completing its own depot transaction.
            arrivalDetected = !item->returnArrivalPending;
            SandReleaseCompletedReturnLockLocked(item);
            arrivalCompletedWithEmptyRoute = true;
        }
        else
        {
            if (item->returnArrivalPending)
            {
                item->returnArrivalPending = 0;
                item->returnArrivalDetectedTick = 0;
                item->returnRouteSeen = 0;
                item->returnStartPending = 1;
                item->returnNextDispatchTick = now;
                item->returnRetryRequiresProgress = 0;
                item->returnBlindRetryAvailable = 1;
                item->returnTailObservedTick = 0;
                item->returnNextTailRecoveryTick =
                    now + SAND_RETURN_STALLED_RETRY_MS;
                arrivalInterrupted = true;
            }
            if (item->returnRouteSeen)
            {
                // The native planner expands a valid home route in segments.
                // Its vector end (and sometimes its allocation) therefore
                // changes during normal driving. Only a changed work
                // assignment is evidence that vanilla supplied another snow
                // target and that the home route must be reasserted.
                // The native planner itself toggles the assignment index while
                // advancing a valid route. Treat only a changed assignment
                // identity/target as a replacement; index-only changes are
                // diagnostic and the strong route-tail restart remains the
                // independent fallback for a newly installed work route.
                bool assignmentChanged = assignment.readable &&
                    (assignment.begin !=
                        item->returnLastAssignmentBegin ||
                     assignment.end !=
                        item->returnLastAssignmentEnd ||
                     assignment.target != item->returnLastTarget);
                bool previousRouteAtTail =
                    item->returnLastRouteCount > 0 &&
                    item->returnLastRouteIndex >= 0 &&
                    (size_t)(item->returnLastRouteIndex + 2) >=
                        item->returnLastRouteCount;
                bool strongIndexRestart = route.readable &&
                    route.count > 0 && route.index >= 0 &&
                    item->returnLastRouteIndex >= 0 &&
                    route.index + 3 < item->returnLastRouteIndex;
                bool routeRestarted =
                    previousRouteAtTail && strongIndexRestart;
                if (assignmentChanged || routeRestarted)
                {
                    previousRouteIndex = item->returnLastRouteIndex;
                    previousRouteCount = item->returnLastRouteCount;
                    previousRouteLinkCount =
                        item->returnLastRouteLinkCount;
                    previousAssignmentBegin =
                        item->returnLastAssignmentBegin;
                    previousAssignmentEnd =
                        item->returnLastAssignmentEnd;
                    previousAssignmentIndex =
                        item->returnLastAssignmentIndex;
                    previousAssignmentTarget = item->returnLastTarget;
                    workAssignmentChanged = assignmentChanged;
                    routeRestartedAfterTail = routeRestarted;
                    item->returnRouteSeen = 0;
                    item->returnStartPending = 1;
                    item->returnNextDispatchTick = now;
                    item->returnRetryRequiresProgress = 0;
                    item->returnBlindRetryAvailable = 1;
                    item->returnTailObservedTick = 0;
                    item->returnNextTailRecoveryTick =
                        now + SAND_RETURN_STALLED_RETRY_MS;
                }
                else if (liveRouteAtTail)
                {
                    if (!item->returnTailObservedTick)
                        item->returnTailObservedTick = now;
                    if (now - item->returnTailObservedTick >=
                            SAND_RETURN_TAIL_HOLD_MS &&
                        now >= item->returnNextTailRecoveryTick)
                    {
                        previousRouteIndex = item->returnLastRouteIndex;
                        previousRouteCount = item->returnLastRouteCount;
                        previousRouteLinkCount =
                            item->returnLastRouteLinkCount;
                        item->returnRouteSeen = 0;
                        item->returnStartPending = 1;
                        item->returnNextDispatchTick = now;
                        item->returnRetryRequiresProgress = 0;
                        item->returnBlindRetryAvailable = 1;
                        item->returnTailObservedTick = 0;
                        item->returnNextTailRecoveryTick =
                            now + SAND_RETURN_STALLED_RETRY_MS;
                        tailRecoveryMode = 1;
                    }
                }
                else
                {
                    item->returnTailObservedTick = 0;
                }
            }
            bool stalledTailRetryDue =
                item->returnRetryRequiresProgress && liveRouteAtTail &&
                now >= item->returnNextTailRecoveryTick;
            bool retryProgressed =
                !item->returnRetryRequiresProgress ||
                (route.readable &&
                    (route.index != item->returnRetryRouteIndex ||
                     route.count != item->returnRetryRouteCount ||
                     route.linkCount !=
                        item->returnRetryRouteLinkCount)) ||
                (assignment.readable &&
                    (assignment.begin !=
                        item->returnRetryAssignmentBegin ||
                     assignment.end !=
                        item->returnRetryAssignmentEnd ||
                     assignment.index !=
                        item->returnRetryAssignmentIndex ||
                     assignment.target != item->returnRetryTarget)) ||
                stalledTailRetryDue;
            if (route.readable)
            {
                item->returnLastRouteIndex = route.index;
                item->returnLastRouteCount = route.count;
                item->returnLastRouteLinkCount = route.linkCount;
            }
            if (assignment.readable)
            {
                item->returnLastAssignmentBegin = assignment.begin;
                item->returnLastAssignmentEnd = assignment.end;
                item->returnLastAssignmentIndex = assignment.index;
                item->returnLastTarget = assignment.target;
            }
            if (!item->returnRouteSeen &&
                (item->returnStartPending ||
                 now >= item->returnNextDispatchTick) &&
                retryProgressed &&
                route.readable && route.index >= 0 &&
                (size_t)route.index < route.count)
            {
                actionKind = item->returnDispatches > 0 ? 2 : 1;
                item->returnStartPending = 0;
                // Execution schedules another attempt after a failure. A true
                // result is watched until arrival and is reasserted only if
                // vanilla installs a changed work assignment.
                item->returnNextDispatchTick = 0;
                item->returnRetryRequiresProgress = 0;
                if (stalledTailRetryDue && !tailRecoveryMode)
                {
                    item->returnNextTailRecoveryTick =
                        now + SAND_RETURN_STALLED_RETRY_MS;
                    tailRecoveryMode = 2;
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    if (arrivalDetected)
    {
        Report("INFO", "Tank diagnostic", "return-lock-arrival-detected",
            "vehicle=%p home_building=%p reason=%s route_state=0x%08X route_readable=%d route_index=%d route_count=%llu route_link_count=%llu assignment_readable=%d assignment_index=%d current_target=%p settle_ms=%d lock_released=%d game_window_cleanup_pending=%d native_fuel_state_machine_owner=1 plugin_route_mutation=0 worker_native_route_calls=0",
            vehicle, homeBuilding, SandReturnReasonName(reason), routeState,
            route.readable ? 1 : 0, route.index,
            (unsigned long long)route.count,
            (unsigned long long)route.linkCount,
            assignment.readable ? 1 : 0,
            assignment.index, assignment.target,
            arrivalCompletedWithEmptyRoute ? 0 :
                g_sandReturnArrivalSettleMs,
            arrivalCompletedWithEmptyRoute ? 1 : 0,
            arrivalCompletedWithEmptyRoute ? 0 : 1);
    }
    if (arrivalCompletedWithEmptyRoute)
    {
        InterlockedIncrement64(&g_sandReturnArrivals);
        Report("INFO", "Tank diagnostic", "return-lock-arrived",
            "vehicle=%p home_building=%p reason=%s settle_ms=0 finalize_attempts=0 completion_thread=%lu route_cleanup=owned-by-vanilla-fuel-state-no-plugin-call route_before_readable=%d route_index_before=%d route_count_before=%llu route_link_count_before=%llu route_after_readable=%d route_index_after=%d route_count_after=%llu route_link_count_after=%llu assignment_before_readable=%d assignment_before_index=%d assignment_before_target=%p assignment_after_readable=%d assignment_after_index=%d assignment_after_target=%p lock_released=1 plugin_route_mutation=0 native_fuel_state_retained=1 fast_forward_safe=1",
            vehicle, homeBuilding, SandReturnReasonName(reason),
            (unsigned long)GetCurrentThreadId(),
            route.readable ? 1 : 0, route.index,
            (unsigned long long)route.count,
            (unsigned long long)route.linkCount,
            route.readable ? 1 : 0, route.index,
            (unsigned long long)route.count,
            (unsigned long long)route.linkCount,
            assignment.readable ? 1 : 0, assignment.index,
            assignment.target,
            assignment.readable ? 1 : 0, assignment.index,
            assignment.target);
    }
    if (arrivalPending)
        SandQueueReturnArrivalFinalization();
    if (arrivalInterrupted)
    {
        Report("WARN", "Tank diagnostic",
            "return-lock-arrival-interrupted",
            "vehicle=%p home_building=%p current_building=%p reason=%s left_before_route_cleanup=1 lock_retained=1 reassert_home_pending=1",
            vehicle, homeBuilding, currentBuilding,
            SandReturnReasonName(reason));
    }
    if (workAssignmentChanged)
    {
        Report("INFO", "Tank diagnostic",
            "return-lock-work-assignment-changed",
            "vehicle=%p home_building=%p reason=%s route_readable=%d route_index=%d route_count=%llu route_link_count=%llu previous_assignment_begin=%p previous_assignment_end=%p previous_assignment_index=%d previous_target=%p assignment_readable=%d assignment_begin=%p assignment_end=%p assignment_index=%d current_target=%p natural_route_extension_ignored=1 reassert_home_pending=1 clear_calls_remain_blocked=1",
            vehicle, homeBuilding, SandReturnReasonName(reason),
            route.readable ? 1 : 0, route.index,
            (unsigned long long)route.count,
            (unsigned long long)route.linkCount,
            previousAssignmentBegin, previousAssignmentEnd,
            previousAssignmentIndex, previousAssignmentTarget,
            assignment.readable ? 1 : 0,
            assignment.begin, assignment.end,
            assignment.index, assignment.target);
    }
    if (routeRestartedAfterTail)
    {
        Report("INFO", "Tank diagnostic",
            "return-lock-route-restarted-after-tail",
            "vehicle=%p home_building=%p reason=%s previous_route_index=%d previous_route_count=%llu previous_route_link_count=%llu route_readable=%d route_index=%d route_count=%llu route_link_count=%llu assignment_readable=%d assignment_begin=%p assignment_end=%p assignment_index=%d current_target=%p watched_home_tail_completed=1 work_route_takeover_detected=1 reassert_home_pending=1 clear_calls_remain_blocked=1",
            vehicle, homeBuilding, SandReturnReasonName(reason),
            previousRouteIndex,
            (unsigned long long)previousRouteCount,
            (unsigned long long)previousRouteLinkCount,
            route.readable ? 1 : 0, route.index,
            (unsigned long long)route.count,
            (unsigned long long)route.linkCount,
            assignment.readable ? 1 : 0,
            assignment.begin, assignment.end,
            assignment.index, assignment.target);
    }
    if (tailRecoveryMode && actionKind)
    {
        LONG64 totalTailRecoveries =
            InterlockedIncrement64(&g_sandReturnTailRecoveries);
        Report("INFO", "Tank diagnostic",
            "return-lock-tail-watchdog",
            "vehicle=%p home_building=%p reason=%s mode=%s route_readable=%d route_index=%d route_count=%llu route_link_count=%llu previous_route_index=%d previous_route_count=%llu previous_route_link_count=%llu stable_tail_hold_ms=%llu stalled_retry_ms=%llu total_tail_recoveries=%lld outside_home=1 reassert_home_pending=1 clear_calls_remain_blocked=1",
            vehicle, homeBuilding, SandReturnReasonName(reason),
            tailRecoveryMode == 1 ? "planned-route-tail" :
                                    "failed-planner-stalled-tail",
            route.readable ? 1 : 0, route.index,
            (unsigned long long)route.count,
            (unsigned long long)route.linkCount,
            previousRouteIndex,
            (unsigned long long)previousRouteCount,
            (unsigned long long)previousRouteLinkCount,
            (unsigned long long)SAND_RETURN_TAIL_HOLD_MS,
            (unsigned long long)SAND_RETURN_STALLED_RETRY_MS,
            (long long)totalTailRecoveries);
    }
    if (!active || arrived || actionKind == 0)
        return false;
    if (actionKindOut) *actionKindOut = actionKind;
    if (reasonOut) *reasonOut = reason;
    if (stateOut) *stateOut = routeState;
    return true;
}

static void SandLogStateTransition(const char* source, void* vehicle,
                                   const SandTrackUpdate& update);

static void SandExecuteReturnAction(void* vehicle,
                                    int actionKind, int reason,
                                    DWORD previousState)
{
    // v0.1.62 only records the first successful game-thread hand-off here.
    // SandDispatchReturnFromClearHook has already latched +0x5E8 to the home
    // depot using the same persistent field as vanilla's real-low-fuel path.
    // Route, target and real fuel remain owned by the Technical Services AI.
    if (actionKind == SAND_RETURN_ACTION_DIRECT_ONCE)
    {
        LONG64 dispatches = 0;
        AcquireSRWLockExclusive(&g_sandTrackerLock);
        SandTrackedVehicle* item =
            SandFindTrackedVehicleLocked(vehicle, false);
        if (item && item->returnLockActive)
        {
            dispatches = ++item->returnDispatches;
            item->returnStartPending = 0;
            item->returnRouteSeen = 0;
            item->returnNextDispatchTick = 0;
            item->returnRetryRequiresProgress = 0;
            item->returnBlindRetryAvailable = 0;
            item->returnTailObservedTick = 0;
            item->returnNextTailRecoveryTick = 0;
        }
        ReleaseSRWLockExclusive(&g_sandTrackerLock);

        void* homeBuilding = nullptr;
        void* fuelBuilding = nullptr;
        BYTE* bytes = (BYTE*)vehicle;
        if (vehicle && ReadablePtr(
                bytes + SAND_VEHICLE_HOME_BUILDING,
                SAND_VEHICLE_FUEL_BUILDING + sizeof(void*) -
                    SAND_VEHICLE_HOME_BUILDING))
        {
            homeBuilding =
                *(void**)(bytes + SAND_VEHICLE_HOME_BUILDING);
            fuelBuilding =
                *(void**)(bytes + SAND_VEHICLE_FUEL_BUILDING);
        }
        Report("INFO", "Tank diagnostic",
            "native-refuel-destination-armed",
            "vehicle=%p reason=%s previous_route_state=0x%08X vehicle_dispatches=%lld home_building=%p fuel_building=%p home_latched=%d native_refuel_order_called=0 native_route_helper_called=0 real_fuel_unchanged=1 route_unchanged=1 target_unchanged=1 vanilla_route_owner=1 native_clears_continue_during_refill_return=1",
            vehicle, SandReturnReasonName(reason), previousState,
            (long long)dispatches, homeBuilding, fuelBuilding,
            homeBuilding && fuelBuilding == homeBuilding ? 1 : 0);
        return;
    }


    const bool directOnce =
        actionKind == SAND_RETURN_ACTION_DIRECT_ONCE;
    if (SAND_NATURAL_BOUNDARY_RETURN_ENABLED && !directOnce)
    {
        Report("WARN", "Tank diagnostic",
            "legacy-route-dispatch-blocked",
            "vehicle=%p reason=%s action_kind=%d natural_job_boundary_mode=1 live_route_trimmed=0 native_home_planner_called=0",
            vehicle, SandReturnReasonName(reason), actionKind);
        return;
    }
    if (!vehicle || !g_sandPlanRouteHome || !g_sandCleanRouteLinks)
    {
        Report("WARN", "Tank diagnostic", "return-lock-dispatch",
            "vehicle=%p reason=%s action=%s could not run because a verified native route helper is unavailable",
            vehicle, SandReturnReasonName(reason),
            "finish-job-and-plan-home");
        return;
    }

    SandRouteTrimResult trim =
        SandTrimPendingRouteAtCurrentNode(vehicle);
    bool trimReady = trim.status == SAND_ROUTE_TRIM_OK ||
                     trim.status == SAND_ROUTE_TRIM_ALREADY_AT_TAIL;

    // The planner now sees the current node as the end of the active route, as
    // it would after vanilla completed a clearing job. A true result appends a
    // normal road route home and releases the current snow target without
    // storing or teleporting the vehicle.
    bool planned = trimReady && g_sandPlanRouteHome(vehicle);

    SandRouteVectorState after = SandReadRouteVectorState(vehicle);
    SandAssignmentSnapshot assignmentAfter =
        SandReadAssignmentSnapshot(vehicle);
    DWORD routeState = 0;
    bool routeReadable = SandReadReturnRouteState(vehicle, &routeState);
    LONG64 dispatches = 0;
    bool retryScheduled = false;
    bool returnWatchRequired = false;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* item =
        SandFindTrackedVehicleLocked(vehicle, false);
    if (item && item->returnLockActive)
    {
        returnWatchRequired = true;
        item->returnRouteState = routeState;
        item->returnRouteSeen = planned ? 1 : 0;
        dispatches = ++item->returnDispatches;
        if (planned)
        {
            item->returnNextDispatchTick = 0;
            item->returnRetryRequiresProgress = 0;
            item->returnBlindRetryAvailable = 0;
            item->returnTailObservedTick = 0;
            item->returnNextTailRecoveryTick = 0;
            item->returnPlannedRouteBegin = after.begin;
            item->returnPlannedRouteEnd = after.end;
            item->returnPlannedLinkBegin = after.linkBegin;
            item->returnPlannedLinkEnd = after.linkEnd;
            item->returnLastRouteIndex =
                after.readable ? after.index : -1;
            item->returnLastRouteCount =
                after.readable ? after.count : 0;
            item->returnLastRouteLinkCount =
                after.readable ? after.linkCount : 0;
            // Store the post-planner state. The planner itself releases the
            // former snow target, which must not be mistaken for a later
            // replacement assignment.
            if (assignmentAfter.readable)
            {
                item->returnLastAssignmentBegin = assignmentAfter.begin;
                item->returnLastAssignmentEnd = assignmentAfter.end;
                item->returnLastAssignmentIndex = assignmentAfter.index;
                item->returnLastTarget = assignmentAfter.target;
            }
        }
        else if (directOnce)
        {
            // v0.1.58 deliberately does not retry a failed immediate plan.
            // The persistent lock remains active and the verified vanilla
            // job-boundary call is the only fallback, avoiding route fights.
            item->returnStartPending = 0;
            item->returnRouteSeen = 0;
            item->returnNextDispatchTick = 0;
            item->returnRetryRequiresProgress = 0;
            item->returnBlindRetryAvailable = 0;
            item->returnTailObservedTick = 0;
            item->returnNextTailRecoveryTick = 0;
        }
        else
        {
            item->returnPlannedRouteBegin = nullptr;
            item->returnPlannedRouteEnd = nullptr;
            item->returnPlannedLinkBegin = nullptr;
            item->returnPlannedLinkEnd = nullptr;
            item->returnNextDispatchTick =
                GetTickCount64() +
                (ULONGLONG)g_sandReturnRetryIntervalMs;
            item->returnTailObservedTick = 0;
            item->returnNextTailRecoveryTick =
                GetTickCount64() + SAND_RETURN_STALLED_RETRY_MS;
            if (item->returnBlindRetryAvailable)
            {
                // One short blind retry covers the normal transient planner
                // hand-off. Further calls wait for observed route/assignment
                // progress instead of repeatedly trimming and replanning the
                // same unchanged tail every 250 ms.
                item->returnBlindRetryAvailable = 0;
                item->returnRetryRequiresProgress = 0;
            }
            else
            {
                item->returnRetryRequiresProgress = 1;
                item->returnRetryRouteIndex =
                    after.readable ? after.index : -1;
                item->returnRetryRouteCount =
                    after.readable ? after.count : 0;
                item->returnRetryRouteLinkCount =
                    after.readable ? after.linkCount : 0;
                item->returnRetryAssignmentBegin =
                    assignmentAfter.readable ? assignmentAfter.begin : nullptr;
                item->returnRetryAssignmentEnd =
                    assignmentAfter.readable ? assignmentAfter.end : nullptr;
                item->returnRetryAssignmentIndex =
                    assignmentAfter.readable ? assignmentAfter.index : -1;
                item->returnRetryTarget =
                    assignmentAfter.readable ? assignmentAfter.target : nullptr;
            }
            retryScheduled = true;
        }
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);
    if (actionKind == 2)
        InterlockedIncrement64(&g_sandReturnReassignments);

    Report("INFO", "Tank diagnostic", "return-lock-dispatch",
        "vehicle=%p reason=%s action=%s previous_route_state=0x%08X route_state_readable=%d route_state_after=0x%08X route_state_diagnostic_only=1 trim_status=%s discarded_pending_nodes=%llu route_vector_before_readable=%d route_index_before=%d route_count_before=%llu route_link_count_before=%llu route_vector_after_readable=%d route_index_after=%d route_count_after=%llu route_link_count_after=%llu assignment_after_readable=%d assignment_after_begin=%p assignment_after_end=%p assignment_after_index=%d assignment_after_target=%p vehicle_dispatches=%lld native_route_home_planned=%d retry_scheduled=%d retry_interval_ms=%d persistent_assignment_watch=1 natural_route_extension_ignored=1 clear_calls_blocked_until_arrival=1 teleport_path=0",
        vehicle, SandReturnReasonName(reason),
        actionKind == 2
            ? "retry-finish-job-and-plan-home"
            : (directOnce ? "single-direct-home" :
                            "finish-job-and-plan-home"),
        previousState, routeReadable ? 1 : 0, routeState,
        SandRouteTrimStatusName(trim.status),
        (unsigned long long)trim.discarded,
        trim.before.readable ? 1 : 0, trim.before.index,
        (unsigned long long)trim.before.count,
        (unsigned long long)trim.before.linkCount,
        after.readable ? 1 : 0, after.index,
        (unsigned long long)after.count,
        (unsigned long long)after.linkCount,
        assignmentAfter.readable ? 1 : 0,
        assignmentAfter.begin, assignmentAfter.end,
        assignmentAfter.index, assignmentAfter.target,
        (long long)dispatches, planned ? 1 : 0,
        retryScheduled ? 1 : 0,
        g_sandReturnRetryIntervalMs);
    if (returnWatchRequired && !directOnce)
        SandQueueReturnRetry();
}

// Maintains vanilla's persistent refuelling-destination latch on the verified
// game thread. The snowplow AI still owns every route operation. Repeating the
// inexpensive pointer check matters because native target transitions may
// clear +0x5E8 before the route-completion branch consumes it.
static void SandDispatchReturnFromClearHook(void* vehicle,
                                            const char* source)
{
    if (!vehicle || !g_sandAutomaticReturnEnabled ||
        !g_sandReturnLockEnabled ||
        !SandVehicleReturnLockActive(vehicle))
        return;
    bool homeLatched =
        SandLatchNativeFuelBuildingToHome(vehicle, source);
    if (InterlockedCompareExchange(
            &g_sandReturnDispatchInProgress, 1, 0) != 0)
        return;

    __try
    {
        int reason = SAND_RETURN_NONE;
        DWORD previousState = 0;
        bool dispatch = false;
        AcquireSRWLockExclusive(&g_sandTrackerLock);
        SandTrackedVehicle* item =
            SandFindTrackedVehicleLocked(vehicle, false);
        if (item && item->returnLockActive &&
            item->returnStartPending && item->returnDispatches == 0)
        {
            item->returnStartPending = 0;
            reason = item->returnReason;
            previousState = item->returnRouteState;
            dispatch = true;
        }
        ReleaseSRWLockExclusive(&g_sandTrackerLock);

        if (dispatch)
        {
            Report("INFO", "Tank diagnostic", "return-hook-dispatch",
                "source=%s vehicle=%p reason=%s action=latch-native-refuel-destination home_latched=%d game_thread=1 dispatch_once=1 route_mutation=0 target_mutation=0 real_fuel_mutation=0 vanilla_route_owner=1",
                source ? source : "unknown", vehicle,
                SandReturnReasonName(reason), homeLatched ? 1 : 0);
            SandExecuteReturnAction(
                vehicle, SAND_RETURN_ACTION_DIRECT_ONCE,
                reason, previousState);
        }
    }
    __except(FaultFilter("sand spreader native refuel destination arm",
                         GetExceptionInformation()))
    {
        Report("WARN", "Tank diagnostic", "return-hook-dispatch-fault",
            "source=%s vehicle=%p native refuel destination arm faulted; the return lock remains active and subsequent clearing calls remain blocked",
            source ? source : "unknown", vehicle);
    }

    InterlockedExchange(&g_sandReturnDispatchInProgress, 0);
}

static bool SandLifecycleFiniteFloat(float value)
{
    return value == value && value > -10000000.0f &&
        value < 10000000.0f;
}

static ULONGLONG SandLifecycleHashBytes(
    ULONGLONG hash, const void* data, size_t bytes)
{
    const BYTE* cursor = (const BYTE*)data;
    for (size_t i = 0; i < bytes; ++i)
    {
        hash ^= (ULONGLONG)cursor[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool SandLifecycleLooksLikePointer(ULONGLONG value)
{
    // User-mode pointers observed in this 64-bit game occupy canonical low
    // addresses above 4 GiB. Normalizing them lets the diagnostic compare
    // object chunks across a save/load even though every allocation moves.
    return value >= 0x0000000100000000ULL &&
        value < 0x0000800000000000ULL;
}

static bool SandCaptureLifecycleHashes(
    BYTE* building,
    ULONGLONG hashes[SAND_BUILDING_LIFECYCLE_CHUNKS])
{
    if (!building || !hashes ||
        !ReadablePtr(building, SAND_BUILDING_LIFECYCLE_SNAPSHOT_BYTES))
        return false;

    BYTE normalized[SAND_BUILDING_LIFECYCLE_CHUNK_BYTES] = {};
    __try
    {
        for (size_t chunk = 0;
             chunk < SAND_BUILDING_LIFECYCLE_CHUNKS; ++chunk)
        {
            memcpy(normalized,
                   building + chunk * SAND_BUILDING_LIFECYCLE_CHUNK_BYTES,
                   sizeof(normalized));
            for (size_t offset = 0;
                 offset + sizeof(ULONGLONG) <= sizeof(normalized);
                 offset += sizeof(ULONGLONG))
            {
                ULONGLONG value = 0;
                memcpy(&value, normalized + offset, sizeof(value));
                if (SandLifecycleLooksLikePointer(value))
                    memset(normalized + offset, 0, sizeof(value));
            }
            hashes[chunk] = SandLifecycleHashBytes(
                1469598103934665603ULL,
                normalized, sizeof(normalized));
        }
        return true;
    }
    __except(FaultFilter(
        "Technical Services lifecycle fingerprint",
        GetExceptionInformation()))
    {
        memset(hashes, 0,
               sizeof(ULONGLONG) *
                   SAND_BUILDING_LIFECYCLE_CHUNKS);
        return false;
    }
}

static void SandFormatLifecycleHashes(
    const ULONGLONG hashes[SAND_BUILDING_LIFECYCLE_CHUNKS],
    char* out, size_t outSize)
{
    if (!out || !outSize) return;
    out[0] = 0;
    size_t used = 0;
    for (size_t i = 0;
         i < SAND_BUILDING_LIFECYCLE_CHUNKS && used + 1 < outSize; ++i)
    {
        int written = _snprintf_s(
            out + used, outSize - used, _TRUNCATE,
            "%s%03llX=%016llX",
            i ? "," : "",
            (unsigned long long)
                (i * SAND_BUILDING_LIFECYCLE_CHUNK_BYTES),
            (unsigned long long)hashes[i]);
        if (written < 0) break;
        used += (size_t)written;
    }
}

static bool SandReadLifecyclePosition(BYTE* building, float out[3])
{
    if (!out) return false;
    out[0] = out[1] = out[2] = 0.0f;
    if (!InterlockedCompareExchange(
            &g_sandBuildingLifecycleLayoutVerified, 0, 0) ||
        !building ||
        !ReadablePtr(building + SAND_BUILDING_WORLD_POSITION,
                     sizeof(float) * 3))
        return false;
    __try
    {
        memcpy(out, building + SAND_BUILDING_WORLD_POSITION,
               sizeof(float) * 3);
        return SandLifecycleFiniteFloat(out[0]) &&
            SandLifecycleFiniteFloat(out[1]) &&
            SandLifecycleFiniteFloat(out[2]);
    }
    __except(FaultFilter(
        "Technical Services lifecycle position",
        GetExceptionInformation()))
    {
        out[0] = out[1] = out[2] = 0.0f;
        return false;
    }
}

static ULONGLONG SandLifecycleSiteFingerprint(
    int technicalType, int positionValid, const float position[3])
{
    ULONGLONG hash = 1469598103934665603ULL;
    hash = SandLifecycleHashBytes(
        hash, &technicalType, sizeof(technicalType));
    hash = SandLifecycleHashBytes(
        hash, &positionValid, sizeof(positionValid));
    if (positionValid && position)
    {
        int millimetres[3] = {};
        for (int i = 0; i < 3; ++i)
        {
            float scaled = position[i] * 1000.0f;
            millimetres[i] = (int)(scaled >= 0.0f
                ? scaled + 0.5f : scaled - 0.5f);
        }
        hash = SandLifecycleHashBytes(
            hash, millimetres, sizeof(millimetres));
    }
    return hash;
}

static SandBuildingLifecycleState* SandFindLifecycleState(
    BYTE* building)
{
    if (!building) return nullptr;
    for (size_t i = 0;
         i < SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES; ++i)
    {
        if (g_sandBuildingLifecycleStates[i].building == building)
            return &g_sandBuildingLifecycleStates[i];
    }
    return nullptr;
}

static SandBuildingLifecycleState* SandAllocateLifecycleState()
{
    for (size_t i = 0;
         i < SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES; ++i)
    {
        if (!g_sandBuildingLifecycleStates[i].building)
            return &g_sandBuildingLifecycleStates[i];
    }
    if (InterlockedCompareExchange(
            &g_sandBuildingLifecycleCapacityWarningLogged, 1, 0) == 0)
    {
        Report("WARN", "Building lifecycle diagnostic", "capacity",
            "The bounded lifecycle tracker reached %llu depot instances; later instances remain fully functional but are not fingerprinted",
            (unsigned long long)
                SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES);
    }
    return nullptr;
}

static SandBuildingLifecycleState* SandFindLifecyclePredecessor(
    LONG generation, BYTE* currentBuilding,
    int positionValid, const float position[3],
    bool* crossGeneration)
{
    if (crossGeneration) *crossGeneration = false;
    if (!positionValid || !position) return nullptr;
    const float maximumDistanceSquared = 0.25f * 0.25f;
    SandBuildingLifecycleState* best = nullptr;
    float bestDistanceSquared = maximumDistanceSquared;
    for (size_t i = 0;
         i < SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES; ++i)
    {
        SandBuildingLifecycleState* state =
            &g_sandBuildingLifecycleStates[i];
        if (!state->building || state->building == currentBuilding ||
            state->generation != generation || state->active ||
            !state->positionValid ||
            state->initialTechnicalType != BUILDING_GARBAGE_OFFICE)
            continue;
        float dx = state->position[0] - position[0];
        float dy = state->position[1] - position[1];
        float dz = state->position[2] - position[2];
        float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared <= bestDistanceSquared)
        {
            best = state;
            bestDistanceSquared = distanceSquared;
        }
    }
    if (best) return best;

    for (size_t i = 0;
         i < SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES; ++i)
    {
        SandBuildingLifecycleState* state =
            &g_sandPreviousWorldLifecycleStates[i];
        if (!state->building || !state->positionValid ||
            state->initialTechnicalType != BUILDING_GARBAGE_OFFICE)
            continue;
        float dx = state->position[0] - position[0];
        float dy = state->position[1] - position[1];
        float dz = state->position[2] - position[2];
        float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared <= bestDistanceSquared)
        {
            best = state;
            bestDistanceSquared = distanceSquared;
        }
    }
    if (best && crossGeneration) *crossGeneration = true;
    return best;
}

static size_t SandCountMatchingLifecycleChunks(
    const SandBuildingLifecycleState& a,
    const SandBuildingLifecycleState& b)
{
    size_t matching = 0;
    for (size_t i = 0;
         i < SAND_BUILDING_LIFECYCLE_CHUNKS; ++i)
    {
        if (a.normalizedChunkHashes[i] &&
            a.normalizedChunkHashes[i] ==
                b.normalizedChunkHashes[i])
            ++matching;
    }
    return matching;
}

static int SandLifecycleConstructionClass(float progress)
{
    if (!SandLifecycleFiniteFloat(progress)) return -1;
    if (progress >= 0.9999f) return 2;
    if (progress > 0.0001f) return 1;
    return 0;
}

static void SandLogLifecycleSnapshot(
    const char* reason, const SandBuildingLifecycleState& state,
    BYTE* sameSitePredecessor)
{
    char hashes[512] = {};
    SandFormatLifecycleHashes(
        state.normalizedChunkHashes, hashes, sizeof(hashes));
    Report("INFO", "Building lifecycle diagnostic", "snapshot",
        "reason=%s generation=%ld first_scan=%llu last_scan=%llu building=%p type_description=%p registry_index_first=%llu registry_index_current=%llu initial_type=%d current_type=%d construction_progress=%.6f construction_class=%d going_away=%d position_valid=%d position=[%.3f,%.3f,%.3f] site_fingerprint=0x%016llX same_site_predecessor=%p active=%d terminal_observed=%d normalized_chunks=[%s] pointer_normalization=canonical-user-addresses-zeroed game_memory_write=0 priority_persistence_active=%d priority_inheritance_allowed=0",
        reason ? reason : "unknown", state.generation,
        (unsigned long long)state.firstSeenScan,
        (unsigned long long)state.lastSeenScan,
        state.building, state.typeDescription,
        (unsigned long long)state.firstRegistryIndex,
        (unsigned long long)state.lastRegistryIndex,
        state.initialTechnicalType, state.lastTechnicalType,
        state.constructionProgress,
        SandLifecycleConstructionClass(state.constructionProgress),
        state.goingAway, state.positionValid,
        state.position[0], state.position[1], state.position[2],
        (unsigned long long)state.siteFingerprint,
        sameSitePredecessor, state.active,
        state.terminalObserved, hashes,
        InterlockedCompareExchange(
            &g_spreaderPriorityPersistenceActive, 0, 0) ? 1 : 0);
}

static void SandObserveBuildingLifecycle(
    BYTE* building, size_t registryIndex, int technicalType,
    ULONGLONG scan, bool manualSnapshot)
{
    if (!g_sandBuildingLifecycleDiagnosticEnabled || !building) return;

    float position[3] = {};
    int positionValid = SandReadLifecyclePosition(
        building, position) ? 1 : 0;
    float constructionProgress = -1.0f;
    int goingAway = -1;
    BYTE* typeDescription = nullptr;
    __try
    {
        if (ReadablePtr(building + B_TYPEDESC, sizeof(void*)))
            typeDescription = *(BYTE**)(building + B_TYPEDESC);
        if (ReadablePtr(
                building + SAND_BUILDING_CONSTRUCTION_PROGRESS,
                sizeof(float)))
        {
            constructionProgress = *(float*)(
                building + SAND_BUILDING_CONSTRUCTION_PROGRESS);
            if (!SandLifecycleFiniteFloat(constructionProgress))
                constructionProgress = -1.0f;
        }
        if (ReadablePtr(building + SAND_BUILDING_GOING_AWAY,
                        sizeof(BYTE)))
            goingAway = *(BYTE*)(building + SAND_BUILDING_GOING_AWAY);
    }
    __except(FaultFilter(
        "Technical Services lifecycle fields",
        GetExceptionInformation()))
    {
        return;
    }

    LONG generation = InterlockedCompareExchange(
        &g_sandWorldGeneration, 0, 0);
    SandBuildingLifecycleState* state =
        SandFindLifecycleState(building);
    int previousConstructionClass = state ?
        SandLifecycleConstructionClass(state->constructionProgress) : -1;
    int observedConstructionClass =
        SandLifecycleConstructionClass(constructionProgress);
    bool implicitConstructionRestart = state && state->active &&
        !state->terminalObserved && goingAway == 0 &&
        previousConstructionClass == 2 &&
        observedConstructionClass >= 0 &&
        observedConstructionClass < 2;
    if (implicitConstructionRestart)
    {
        LONG previousPositionMillimetres[3] = {};
        bool previousPositionValid = state->positionValid != 0;
        if (previousPositionValid)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                double scaled = (double)state->position[axis] * 1000.0;
                previousPositionMillimetres[axis] = (LONG)(
                    scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
            }
        }
        SpreaderDeletePriorityLifetime(
            building,
            previousPositionValid ? previousPositionMillimetres : nullptr,
            "construction-restart-without-observed-going-away");
        Report("INFO", "Building lifecycle diagnostic",
            "implicit-terminal-and-new-lifetime",
            "building=%p previous_construction_class=%d current_construction_class=%d going_away=%d reason=completed-building-returned-to-construction-between-scans priority_action=old-lifetime-deleted new_lifetime_defaults=1",
            building, previousConstructionClass,
            observedConstructionClass, goingAway);
    }
    bool reusedPointerLifetime = state &&
        (implicitConstructionRestart || state->terminalObserved) &&
        (!state->active ||
         implicitConstructionRestart ||
         (state->goingAway > 0 && goingAway == 0));
    bool postCollapseConstructionSite = reusedPointerLifetime &&
        state->active && goingAway == 0 &&
        (implicitConstructionRestart || state->goingAway > 0);
    if (reusedPointerLifetime)
    {
        ULONGLONG previousFirstScan = state->firstSeenScan;
        ULONGLONG previousLastScan = state->lastSeenScan;
        ULONGLONG previousSiteFingerprint = state->siteFingerprint;
        Report("INFO", "Building lifecycle diagnostic",
            "pointer-reuse-new-lifetime",
            "building=%p previous_first_scan=%llu previous_last_scan=%llu previous_site_fingerprint=0x%016llX new_position=[%.3f,%.3f,%.3f] post_collapse_construction_site=%d previous_terminal=1 action=new-lifetime-defaults",
            building,
            (unsigned long long)previousFirstScan,
            (unsigned long long)previousLastScan,
            (unsigned long long)previousSiteFingerprint,
            position[0], position[1], position[2],
            postCollapseConstructionSite ? 1 : 0);
        memset(state, 0, sizeof(*state));
    }
    if (!state || reusedPointerLifetime)
    {
        if (!state)
        {
            state = SandAllocateLifecycleState();
            if (!state) return;
        }
        memset(state, 0, sizeof(*state));
        state->building = building;
        state->typeDescription = typeDescription;
        state->generation = generation;
        state->firstSeenScan = scan;
        state->lastSeenScan = scan;
        state->firstRegistryIndex = registryIndex;
        state->lastRegistryIndex = registryIndex;
        state->initialTechnicalType = technicalType;
        state->lastTechnicalType = technicalType;
        state->active = 1;
        state->positionValid = positionValid;
        memcpy(state->position, position, sizeof(position));
        state->constructionProgress = constructionProgress;
        state->goingAway = goingAway;
        state->siteFingerprint = SandLifecycleSiteFingerprint(
            BUILDING_GARBAGE_OFFICE, positionValid, position);
        SandCaptureLifecycleHashes(
            building, state->normalizedChunkHashes);

        bool predecessorFromPreviousWorld = false;
        SandBuildingLifecycleState* predecessor =
            SandFindLifecyclePredecessor(
                generation, building, positionValid, position,
                &predecessorFromPreviousWorld);
        size_t matchingPredecessorChunks = predecessor ?
            SandCountMatchingLifecycleChunks(*state, *predecessor) : 0;
        InterlockedIncrement64(
            &g_sandBuildingLifecycleAppearances);
        SandLogLifecycleSnapshot(
            reusedPointerLifetime ?
                (postCollapseConstructionSite ?
                    "post-collapse-construction-site-new-lifetime" :
                    "appeared-new-lifetime-reused-pointer") :
            predecessor ?
                (predecessorFromPreviousWorld ?
                    "appeared-after-world-load-at-known-site" :
                    "appeared-new-instance-at-old-site") :
                "appeared",
            *state, predecessor ? predecessor->building : nullptr);
        if (predecessor)
        {
            Report("INFO", "Building lifecycle diagnostic",
                predecessorFromPreviousWorld ?
                    "world-load-identity-candidate" :
                    "new-instance-default-policy",
                "predecessor=%p new_building=%p site_fingerprint=0x%016llX predecessor_generation=%ld current_generation=%ld predecessor_terminal=%d cross_world_generation=%d matching_normalized_chunks=%llu/%d priority_inheritance_allowed=%d future_action=%s sidecar_write=next-game-save",
                predecessor->building, building,
                (unsigned long long)state->siteFingerprint,
                predecessor->generation, generation,
                predecessor->terminalObserved,
                predecessorFromPreviousWorld ? 1 : 0,
                (unsigned long long)matchingPredecessorChunks,
                SAND_BUILDING_LIFECYCLE_CHUNKS,
                predecessorFromPreviousWorld ? 1 : 0,
                predecessorFromPreviousWorld ?
                    "diagnose-normal-load-identity-only" :
                    "initialize-from-grit_materials-defaults");
        }
        DWORD presentMask =
            SpreaderObservePresentMaterialMask(building);
        if (reusedPointerLifetime ||
            (predecessor && !predecessorFromPreviousWorld))
        {
            SpreaderResetDepotPriorityDefaults(
                building, presentMask,
                postCollapseConstructionSite ?
                    "post-collapse-construction-site" :
                    "new-building-lifetime");
        }
        else if (presentMask)
        {
            // Materialize every loaded depot's runtime state during the
            // registry scan. This restores its save-specific priorities even
            // when the player never opens the building window, and guarantees
            // that the next game save retains untouched depots as well.
            int priorities[SPREADER_MAX_MATERIALS] = {};
            SpreaderGetDepotPrioritySnapshot(
                building, presentMask, priorities, nullptr);
        }
    }
    else
    {
        int previousType = state->lastTechnicalType;
        int previousGoingAway = state->goingAway;
        int previousObservedConstructionClass =
            SandLifecycleConstructionClass(
                state->constructionProgress);
        int constructionClass =
            SandLifecycleConstructionClass(constructionProgress);

        state->typeDescription = typeDescription;
        state->lastSeenScan = scan;
        state->lastRegistryIndex = registryIndex;
        state->lastTechnicalType = technicalType;
        state->active = 1;
        state->positionValid = positionValid;
        memcpy(state->position, position, sizeof(position));
        state->constructionProgress = constructionProgress;
        state->goingAway = goingAway;
        state->siteFingerprint = SandLifecycleSiteFingerprint(
            BUILDING_GARBAGE_OFFICE, positionValid, position);

        bool terminalNow =
            technicalType != BUILDING_GARBAGE_OFFICE || goingAway > 0;
        bool stateChanged = previousType != technicalType ||
            previousGoingAway != goingAway ||
            previousObservedConstructionClass != constructionClass;
        if (stateChanged || manualSnapshot ||
            (terminalNow && !state->terminalObserved))
        {
            SandCaptureLifecycleHashes(
                building, state->normalizedChunkHashes);
            if (terminalNow && !state->terminalObserved)
            {
                state->terminalObserved = 1;
                InterlockedIncrement64(
                    &g_sandBuildingLifecycleTerminalEvents);
                SandLogLifecycleSnapshot(
                    goingAway > 0 ? "terminal-going-away" :
                                    "terminal-type-transition",
                    *state, nullptr);
                LONG positionMillimetres[3] = {};
                bool positionMillimetresValid =
                    SpreaderPositionMillimetres(
                        building, positionMillimetres);
                SpreaderDeletePriorityLifetime(
                    building,
                    positionMillimetresValid ?
                        positionMillimetres : nullptr,
                    goingAway > 0 ?
                        "native-going-away" :
                        "technical-type-transition");
                Report("INFO", "Building lifecycle diagnostic",
                    "terminal-policy",
                    "building=%p going_away=%d current_type=%d priority_action=deleted-memory sidecar_action=delete-on-next-game-save old_instance_resources_owned_by_game=1 collapse_and_demolition_equal=1 rebuild_defaults=1",
                    building, goingAway, technicalType);
            }
            else if (manualSnapshot)
            {
                InterlockedIncrement64(
                    &g_sandBuildingLifecycleManualSnapshots);
                SandLogLifecycleSnapshot(
                    "manual-CTRL-F8", *state, nullptr);
            }
            else
            {
                InterlockedIncrement64(
                    &g_sandBuildingLifecycleStateChanges);
                SandLogLifecycleSnapshot(
                    "state-change", *state, nullptr);
            }
        }
    }
}

static void SandFinalizeMissingBuildingLifecycles(
    ULONGLONG scan, bool manualSnapshot)
{
    if (!g_sandBuildingLifecycleDiagnosticEnabled) return;
    size_t active = 0;
    size_t disappeared = 0;
    for (size_t i = 0;
         i < SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES; ++i)
    {
        SandBuildingLifecycleState* state =
            &g_sandBuildingLifecycleStates[i];
        if (!state->building || !state->active) continue;
        if (state->lastSeenScan == scan)
        {
            ++active;
            continue;
        }

        bool wasTerminal = state->terminalObserved != 0;
        state->active = 0;
        state->terminalObserved = 1;
        ++disappeared;
        InterlockedIncrement64(
            &g_sandBuildingLifecycleDisappearances);
        SandLogLifecycleSnapshot(
            "disappeared-from-global-registry", *state, nullptr);
        if (!wasTerminal)
        {
            LONG positionMillimetres[3] = {};
            bool positionMillimetresValid =
                state->positionValid != 0;
            if (positionMillimetresValid)
            {
                for (int axis = 0; axis < 3; ++axis)
                {
                    double scaled =
                        (double)state->position[axis] * 1000.0;
                    positionMillimetres[axis] = (LONG)(
                        scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
                }
            }
            SpreaderDeletePriorityLifetime(
                state->building,
                positionMillimetresValid ?
                    positionMillimetres : nullptr,
                "registry-disappearance");
        }
        Report("INFO", "Building lifecycle diagnostic",
            "terminal-policy",
            "building=%p going_away_last=%d current_type_last=%d priority_action=%s disappearance_terminal=1 collapse_and_demolition_equal=1 rebuild_defaults=1 sidecar_action=delete-on-next-game-save",
            state->building, state->goingAway,
            state->lastTechnicalType,
            wasTerminal ? "already-deleted-on-going-away" :
                          "deleted-on-disappearance");
    }
    if (manualSnapshot || disappeared)
    {
        Report("INFO", "Building lifecycle diagnostic", "scan-summary",
            "scan=%llu active=%llu disappeared_this_scan=%llu appearances_total=%lld state_changes_total=%lld terminal_events_total=%lld disappearances_total=%lld manual_snapshots_total=%lld game_memory_write=0 sidecar_write=next-game-save-only",
            (unsigned long long)scan,
            (unsigned long long)active,
            (unsigned long long)disappeared,
            (long long)InterlockedCompareExchange64(
                &g_sandBuildingLifecycleAppearances, 0, 0),
            (long long)InterlockedCompareExchange64(
                &g_sandBuildingLifecycleStateChanges, 0, 0),
            (long long)InterlockedCompareExchange64(
                &g_sandBuildingLifecycleTerminalEvents, 0, 0),
            (long long)InterlockedCompareExchange64(
                &g_sandBuildingLifecycleDisappearances, 0, 0),
            (long long)InterlockedCompareExchange64(
                &g_sandBuildingLifecycleManualSnapshots, 0, 0));
    }
}

static void SandPollBuildingLifecycleSnapshotHotkey()
{
    if (!g_debug || !g_sandBuildingLifecycleDiagnosticEnabled) return;
    bool down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
        (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    LONG previous = InterlockedExchange(
        &g_sandBuildingLifecycleSnapshotKeyDown, down ? 1 : 0);
    if (down && !previous)
    {
        InterlockedExchange(
            &g_sandBuildingLifecycleSnapshotRequested, 1);
        Report("INFO", "Building lifecycle diagnostic",
            "manual-snapshot-request",
            "marker=CTRL_F8 next_global_registry_scan=1 game_memory_write=0 sidecar_write=next-game-save-only");
    }
}

static bool SandDiagnosticVerifyBuildingLifecycleLayout()
{
    if (!g_sandBuildingLifecycleDiagnosticEnabled) return true;
    if (!g_engine)
    {
        Report("WARN", "Building lifecycle diagnostic", "layout",
            "C3DDLL64.dll is unavailable; lifecycle diagnostics are disabled");
        g_sandBuildingLifecycleDiagnosticEnabled = 0;
        return false;
    }
    FARPROC exported = GetProcAddress(
        g_engine, SAND_C3D_NODE_GET_POSITION);
    if (!exported ||
        !ReadablePtr((const void*)exported,
                     sizeof(EXPECT_SAND_C3D_NODE_GET_POSITION)) ||
        memcmp((const void*)exported,
               EXPECT_SAND_C3D_NODE_GET_POSITION,
               sizeof(EXPECT_SAND_C3D_NODE_GET_POSITION)) != 0)
    {
        Report("WARN", "Building lifecycle diagnostic", "layout",
            "The native C3D_NODE::GetPosition export no longer matches; lifecycle diagnostics are disabled before reading building position fields");
        g_sandBuildingLifecycleDiagnosticEnabled = 0;
        return false;
    }
    InterlockedExchange(
        &g_sandBuildingLifecycleLayoutVerified, 1);
    Report("INFO", "Building lifecycle diagnostic", "layout",
        "active=1 technical_services_type=%d construction_progress=building+0x%llX going_away=building+0x%llX node=building+0x%llX native_position=node+0x%llX resolved_position=building+0x%llX position_export=%s registry_membership=verified-global-vector normalized_snapshot_bytes=0x%llX chunk_bytes=0x%llX manual_snapshot=CTRL_F8 game_memory_read_only=1 sidecar_persistence_supported=1",
        BUILDING_GARBAGE_OFFICE,
        (unsigned long long)SAND_BUILDING_CONSTRUCTION_PROGRESS,
        (unsigned long long)SAND_BUILDING_GOING_AWAY,
        (unsigned long long)SAND_BUILDING_NODE,
        (unsigned long long)SAND_BUILDING_NODE_POSITION,
        (unsigned long long)SAND_BUILDING_WORLD_POSITION,
        SAND_C3D_NODE_GET_POSITION,
        (unsigned long long)SAND_BUILDING_LIFECYCLE_SNAPSHOT_BYTES,
        (unsigned long long)SAND_BUILDING_LIFECYCLE_CHUNK_BYTES);
    return true;
}

static bool SandDiagnosticVerifyGlobalRegistry()
{
    BYTE* lea = g_exeBase + RVA_SAND_STATIC_GAME_LEA;
    BYTE* staticGame = g_exeBase + RVA_SAND_STATIC_GAME;
    BYTE* worldPointer = g_exeBase + RVA_SAND_GLOBAL_WORLD_POINTER;
    if (!ReadablePtr(lea, 7) ||
        lea[0] != 0x48 || lea[1] != 0x8D || lea[2] != 0x0D)
    {
        Report("WARN", "SOVIET64.exe", "global-building-registry",
            "The verified game-object LEA at exe+0x%X does not match; window-independent tank initialization is disabled",
            RVA_SAND_STATIC_GAME_LEA);
        return false;
    }

    LONG displacement = 0;
    memcpy(&displacement, lea + 3, sizeof(displacement));
    BYTE* resolved = lea + 7 + displacement;
    if (resolved != staticGame ||
        !ReadablePtr(staticGame + SAND_GAME_BUILDING_BEGIN, 16) ||
        !ReadablePtr(worldPointer, sizeof(void*)))
    {
        Report("WARN", "SOVIET64.exe", "global-building-registry",
            "The verified game-object access resolved to %p instead of exe+0x%X, or its registry fields are unreadable; window-independent tank initialization is disabled",
            resolved, RVA_SAND_STATIC_GAME);
        return false;
    }

    InterlockedExchangePointer(
        (PVOID volatile*)&g_sandGameContext, staticGame);
    InterlockedExchange(&g_sandGlobalRegistryVerified, 1);
    SandDiagnosticVerifyBuildingLifecycleLayout();
    Report("INFO", "Tank diagnostic", "global-building-registry",
        "active=1 static_game=exe+0x%X world_generation_pointer=exe+0x%X building_vector=game+0x%llX..+0x%llX scan_interval_ms=%d max_tracked_vehicles=%d observer_reads=1 refill_storage_write_scope=confirmed-refill-only",
        RVA_SAND_STATIC_GAME, RVA_SAND_GLOBAL_WORLD_POINTER,
        (unsigned long long)SAND_GAME_BUILDING_BEGIN,
        (unsigned long long)SAND_GAME_BUILDING_END,
        g_sandGlobalInitializationIntervalMs,
        g_sandMaxVehicles);
    return true;
}

static void SandResetTrackedVehiclesForWorld(
    void* previousWorld, void* currentWorld)
{
    // Returning to the main menu destroys the world's owning UI worker. The
    // next loaded world may render panels on a different thread, as observed
    // in v0.1.71. Release the old binding at every verified world transition;
    // the first Technical Services panel in the new world binds it again.
    LONG previousUiThread = InterlockedExchange(&g_uiThreadId, 0);
    InterlockedExchange(&g_uiThreadWarningLogged, 0);

    size_t lifecycleRecordsDiscarded = 0;
    size_t lifecycleActiveDiscarded = 0;
    for (size_t i = 0;
         i < SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES; ++i)
    {
        if (!g_sandBuildingLifecycleStates[i].building) continue;
        ++lifecycleRecordsDiscarded;
        if (g_sandBuildingLifecycleStates[i].active)
            ++lifecycleActiveDiscarded;
    }
    bool retainedPreviousWorldSnapshot = false;
    if (lifecycleRecordsDiscarded)
    {
        memcpy(g_sandPreviousWorldLifecycleStates,
               g_sandBuildingLifecycleStates,
               sizeof(g_sandPreviousWorldLifecycleStates));
        for (size_t i = 0;
             i < SAND_DIAG_MAX_BUILDING_LIFECYCLE_STATES; ++i)
        {
            g_sandPreviousWorldLifecycleStates[i].active = 0;
        }
        retainedPreviousWorldSnapshot = true;
    }
    if (g_sandBuildingLifecycleDiagnosticEnabled &&
        lifecycleRecordsDiscarded)
    {
        Report("INFO", "Building lifecycle diagnostic", "world-reset",
            "previous_world=%p current_world=%p tracked_instances=%llu active_instances=%llu lifecycle_records_cleared=1 previous_world_snapshot_retained=%d reason=world-generation-change-not-demolition priority_delete_inferred=0 loaded_sidecar_records_retained_until-save-folder-load=1",
            previousWorld, currentWorld,
            (unsigned long long)lifecycleRecordsDiscarded,
            (unsigned long long)lifecycleActiveDiscarded,
            retainedPreviousWorldSnapshot ? 1 : 0);
    }
    memset(g_sandBuildingLifecycleStates, 0,
           sizeof(g_sandBuildingLifecycleStates));
    InterlockedExchange(
        &g_sandBuildingLifecycleCapacityWarningLogged, 0);
    InterlockedExchange(
        &g_sandBuildingLifecycleSnapshotRequested, 0);

    HWND arrivalWindow = g_sandArrivalTimerWindow;
    if (arrivalWindow && IsWindow(arrivalWindow))
    {
        KillTimer(arrivalWindow, SAND_ARRIVAL_TIMER_ID);
        KillTimer(arrivalWindow, SAND_RETURN_RETRY_TIMER_ID);
    }
    InterlockedExchange(&g_sandArrivalTimerQueued, 0);
    InterlockedExchange(&g_sandReturnRetryTimerQueued, 0);

    AcquireSRWLockExclusive(&g_sandTrackerLock);
    memset(g_sandTrackedVehicles, 0, sizeof(g_sandTrackedVehicles));
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    AcquireSRWLockExclusive(&g_spreaderDepotPriorityLock);
    memset(g_spreaderDepotPriorities, 0,
           sizeof(g_spreaderDepotPriorities));
    ReleaseSRWLockExclusive(&g_spreaderDepotPriorityLock);

    for (size_t i = 0; i < g_spreaderMaterialCount; ++i)
    {
        g_spreaderMaterials[i].runtimeResourceIndex = -1;
        InterlockedExchange(
            (volatile LONG*)&g_spreaderMaterials[i].runtimeValidationState,
            0);
    }
    InterlockedExchange(&g_spreaderMaterialValidationGeneration, 0);
    InterlockedExchange(&g_sandWorldBaselineComplete, 0);
    InterlockedExchange(
        &g_sandWorldBaselineCandidateBuildingCount, -1);
    InterlockedExchange(&g_sandWorldBaselineStableScans, 0);

    LONG generation = InterlockedIncrement(&g_sandWorldGeneration);
    Report("INFO", "Tank diagnostic", "world-generation",
        "generation=%ld previous_world=%p current_world=%p tracked_vehicle_tanks_reset=1 material_catalogue_validation_reset=1 depot_priorities_reset=1 refill_debit_baseline_reset=1 lifecycle_records_reset=1 previous_ui_thread=%lu ui_thread_binding_reset=1 next_world_panel_rebind=1 previous_world_lifecycle_snapshot_retained=%d lifecycle_world_change_is_not_terminal_event=1 reason=%s",
        generation, previousWorld, currentWorld,
        (unsigned long)(DWORD)previousUiThread,
        retainedPreviousWorldSnapshot ? 1 : 0,
        currentWorld ? "world-loaded-or-created" : "world-unloaded");
}

static void SandScanGlobalTechnicalServices()
{
    if (!InterlockedCompareExchange(
            &g_sandGlobalRegistryVerified, 0, 0))
        return;

    BYTE* staticGame = g_exeBase + RVA_SAND_STATIC_GAME;
    void* world = nullptr;
    __try
    {
        world = *(void**)(g_exeBase + RVA_SAND_GLOBAL_WORLD_POINTER);
    }
    __except(FaultFilter("sand spreader world generation pointer",
                         GetExceptionInformation()))
    {
        return;
    }

    void* previousWorld = InterlockedCompareExchangePointer(
        (PVOID volatile*)&g_sandObservedWorld, nullptr, nullptr);
    bool worldChanged = previousWorld != world;
    if (worldChanged)
    {
        previousWorld = InterlockedExchangePointer(
            (PVOID volatile*)&g_sandObservedWorld, world);
        SandResetTrackedVehiclesForWorld(previousWorld, world);
    }
    if (!world) return;

    size_t buildingCount = 0;
    size_t technicalServiceCount = 0;
    size_t assignedVehicleCount = 0;
    size_t snowplowCount = 0;
    size_t initializedTankCount = 0;
    ULONGLONG scanOrdinal = (ULONGLONG)
        InterlockedCompareExchange64(
            &g_sandGlobalRegistryScans, 0, 0) + 1ULL;
    bool manualLifecycleSnapshot =
        InterlockedExchange(
            &g_sandBuildingLifecycleSnapshotRequested, 0) != 0;
    __try
    {
        if (!ReadablePtr(staticGame + SAND_GAME_BUILDING_BEGIN, 16))
            return;
        BYTE** begin = *(BYTE***)(staticGame + SAND_GAME_BUILDING_BEGIN);
        BYTE** end = *(BYTE***)(staticGame + SAND_GAME_BUILDING_END);
        if ((!begin && end) || (begin && !end) ||
            (begin && end && end < begin))
            return;

        buildingCount = begin ? (size_t)(end - begin) : 0;
        if (buildingCount > 500000 ||
            (buildingCount &&
             !ReadablePtr(begin, buildingCount * sizeof(void*))))
            return;

        if (buildingCount)
        {
            ValidateSpreaderMaterialCatalogRuntime(
                InterlockedCompareExchange(
                    &g_sandWorldGeneration, 0, 0));
        }

        for (size_t buildingIndex = 0;
             buildingIndex < buildingCount; ++buildingIndex)
        {
            BYTE* building = begin[buildingIndex];
            if (!building) continue;
            int technicalType = ReadTechnicalType(building);
            SandBuildingLifecycleState* knownLifecycle =
                SandFindLifecycleState(building);
            if (technicalType == BUILDING_GARBAGE_OFFICE ||
                knownLifecycle)
            {
                SandObserveBuildingLifecycle(
                    building, buildingIndex, technicalType,
                    scanOrdinal, manualLifecycleSnapshot);
            }
            if (technicalType != BUILDING_GARBAGE_OFFICE)
                continue;
            ++technicalServiceCount;

            if (!ReadablePtr(building + B_VEHICLE_BEGIN, 16))
                continue;
            void** vehicleBegin =
                *(void***)(building + B_VEHICLE_BEGIN);
            void** vehicleEnd =
                *(void***)(building + B_VEHICLE_END);
            if ((!vehicleBegin && vehicleEnd) ||
                (vehicleBegin && !vehicleEnd) ||
                (vehicleBegin && vehicleEnd && vehicleEnd < vehicleBegin))
                continue;

            size_t vehicleCount = vehicleBegin
                ? (size_t)(vehicleEnd - vehicleBegin) : 0;
            if (vehicleCount > 4096 ||
                (vehicleCount &&
                 !ReadablePtr(vehicleBegin,
                              vehicleCount * sizeof(void*))))
                continue;
            assignedVehicleCount += vehicleCount;

            for (size_t vehicleIndex = 0;
                 vehicleIndex < vehicleCount; ++vehicleIndex)
            {
                BYTE* vehicle = (BYTE*)vehicleBegin[vehicleIndex];
                if (!vehicle ||
                    !ReadablePtr(
                        vehicle + SAND_VEHICLE_CURRENT_BUILDING, 16))
                    continue;

                SandSkillView skills = {};
                SandReadSkillView(vehicle, &skills);
                if (!skills.valid || !skills.snowplow) continue;
                ++snowplowCount;

                void* currentBuilding =
                    *(void**)(vehicle + SAND_VEHICLE_CURRENT_BUILDING);
                void* homeBuilding =
                    *(void**)(vehicle + SAND_VEHICLE_HOME_BUILDING);
                float previousRemainingKg = 0.0f;
                float previousCapacityKg = 0.0f;
                bool tankWasInitialized = SandGetShadowTankDisplay(
                    vehicle, &previousRemainingKg,
                    &previousCapacityKg);
                SandTrackUpdate update = {};
                SandUpdateTrackedVehicle(
                    vehicle, skills, currentBuilding, homeBuilding,
                    false, &update);
                bool restoredTank = SpreaderTryRestorePersistedTank(
                    building, vehicleIndex, vehicle, skills,
                    currentBuilding, homeBuilding,
                    update.firstSeen != 0);
                if (!restoredTank)
                {
                    SandObserveShadowTankState(
                        "global-building-registry", vehicle, skills,
                        homeBuilding, update, true);
                }
                SandTryArmPeriodicDryTankReturn(
                    vehicle, currentBuilding, homeBuilding);

                float remainingKg = 0.0f;
                float capacityKg = 0.0f;
                if (!tankWasInitialized && SandGetShadowTankDisplay(
                        vehicle, &remainingKg, &capacityKg))
                    ++initializedTankCount;
            }
        }

        InterlockedExchangePointer(
            (PVOID volatile*)&g_sandGameContext, staticGame);
        LONG64 scan = InterlockedIncrement64(
            &g_sandGlobalRegistryScans);
        SandFinalizeMissingBuildingLifecycles(
            (ULONGLONG)scan, manualLifecycleSnapshot);
        LONG baselineWasComplete = InterlockedCompareExchange(
            &g_sandWorldBaselineComplete, 0, 0);
        LONG stableScans = InterlockedCompareExchange(
            &g_sandWorldBaselineStableScans, 0, 0);
        LONG baselineCompletedThisScan = 0;
        if (!baselineWasComplete)
        {
            LONG candidate = InterlockedCompareExchange(
                &g_sandWorldBaselineCandidateBuildingCount, 0, 0);
            if (candidate == (LONG)buildingCount)
            {
                stableScans = InterlockedIncrement(
                    &g_sandWorldBaselineStableScans);
            }
            else
            {
                InterlockedExchange(
                    &g_sandWorldBaselineCandidateBuildingCount,
                    (LONG)buildingCount);
                InterlockedExchange(&g_sandWorldBaselineStableScans, 1);
                stableScans = 1;
            }

            bool populatedRegistryStable = buildingCount > 0 &&
                stableScans >= 2;
            if (populatedRegistryStable)
            {
                baselineWasComplete = InterlockedExchange(
                    &g_sandWorldBaselineComplete, 1);
                baselineCompletedThisScan =
                    baselineWasComplete ? 0 : 1;
            }
        }
        LONG baselineComplete = InterlockedCompareExchange(
            &g_sandWorldBaselineComplete, 0, 0);
        if (worldChanged || initializedTankCount ||
            baselineCompletedThisScan)
        {
            Report("INFO", "Tank diagnostic", "global-initialization-scan",
                "scan=%lld generation=%ld world=%p buildings=%llu technical_services=%llu assigned_vehicles=%llu snowplows=%llu newly_initialized_tanks=%llu panel_required=0 world_baseline_complete=%ld baseline_was_complete=%ld baseline_completed_this_scan=%ld baseline_stable_scans=%ld populated_required_scans=2 empty_registry_never_completes_baseline=1 refill_storage_write_scope=confirmed-refill-only",
                (long long)scan,
                InterlockedCompareExchange(&g_sandWorldGeneration, 0, 0),
                world,
                (unsigned long long)buildingCount,
                (unsigned long long)technicalServiceCount,
                (unsigned long long)assignedVehicleCount,
                (unsigned long long)snowplowCount,
                (unsigned long long)initializedTankCount,
                baselineComplete,
                baselineWasComplete,
                baselineCompletedThisScan,
                stableScans);
        }
    }
    __except(FaultFilter("sand spreader global building registry scan",
                         GetExceptionInformation()))
    {
        if (InterlockedCompareExchange(
                &g_sandGlobalRegistryFaultLogged, 1, 0) == 0)
        {
            Report("WARN", "Tank diagnostic",
                "global-initialization-scan-fault",
                "A guarded registry read faulted during a concurrent world/building change; the periodic observer will retry without writing game memory");
        }
    }
}

static DWORD WINAPI SandReturnArrivalObserverThread(void*)
{
    const DWORD pollMs = 50;
    ULONGLONG nextRegistryScan = 0;
    while (g_runtimeActive)
    {
        Sleep(pollMs);
        if (!g_runtimeActive) break;
        if (!g_sandDiagnosticEnabled || !g_sandShadowTankEnabled)
            continue;

        AcquireSRWLockShared(&g_storageLoadLock);
        __try
        {
        SandPollBuildingLifecycleSnapshotHotkey();
        ULONGLONG now = GetTickCount64();
        if (now >= nextRegistryScan)
        {
            nextRegistryScan = now +
                (ULONGLONG)g_sandGlobalInitializationIntervalMs;
            SandScanGlobalTechnicalServices();
        }

        // Sampling is read-only with respect to native vehicle state. Only
        // the plugin-owned shadow tank is debited, in proportion to the fuel
        // delta observed while a confirmed plough-work window is eligible.
        SandSampleNativeFuelConsumptionForTrackedVehicles(now);

        if (!g_sandAutomaticReturnEnabled || !g_sandReturnLockEnabled)
            continue;

        void* vehicles[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
        size_t count = 0;
        AcquireSRWLockShared(&g_sandTrackerLock);
        for (size_t i = 0;
             i < SAND_DIAG_MAX_TRACKED_VEHICLES &&
             count < SAND_DIAG_MAX_TRACKED_VEHICLES; ++i)
        {
            const SandTrackedVehicle& item = g_sandTrackedVehicles[i];
            if (item.vehicle && item.returnLockActive)
                vehicles[count++] = item.vehicle;
        }
        ReleaseSRWLockShared(&g_sandTrackerLock);

        for (size_t i = 0; i < count; ++i)
        {
            BYTE* vehicle = (BYTE*)vehicles[i];
            __try
            {
                if (!ReadablePtr(
                        vehicle + SAND_VEHICLE_CURRENT_BUILDING,
                        sizeof(void*)) ||
                    !ReadablePtr(
                        vehicle + SAND_VEHICLE_HOME_BUILDING,
                        sizeof(void*)))
                    continue;

                void* currentBuilding =
                    *(void**)(vehicle + SAND_VEHICLE_CURRENT_BUILDING);
                void* homeBuilding =
                    *(void**)(vehicle + SAND_VEHICLE_HOME_BUILDING);
                if (!currentBuilding || !homeBuilding ||
                    currentBuilding != homeBuilding)
                    continue;

                SandSkillView skills = {};
                SandReadSkillView(vehicle, &skills);
                SandTrackUpdate update = {};
                SandUpdateTrackedVehicle(
                    vehicle, skills, currentBuilding, homeBuilding,
                    false, &update);
                SandObserveShadowTankState(
                    "return-arrival-observer", vehicle, skills,
                    homeBuilding, update);
                SandLogStateTransition(
                    "return-arrival-observer", vehicle, update);

                int ignoredAction = 0;
                int ignoredReason = SAND_RETURN_NONE;
                DWORD ignoredState = 0;
                SandPrepareReturnAction(
                    vehicle, currentBuilding, homeBuilding,
                    GetTickCount64(), &ignoredAction, &ignoredReason,
                    &ignoredState);
            }
            __except(FaultFilter(
                "sand spreader return arrival observer",
                GetExceptionInformation()))
            {
                if (InterlockedCompareExchange(
                        &g_sandReturnObserverFaultLogged, 1, 0) == 0)
                {
                    Report("WARN", "Tank diagnostic",
                        "return-arrival-observer-fault",
                        "A guarded vehicle arrival read faulted; the observer continues and never invokes native route helpers from its worker thread");
                }
            }
        }
        }
        __finally
        {
            ReleaseSRWLockShared(&g_storageLoadLock);
        }
    }
    return 0;
}

static bool SandDiagnosticStartReturnObserver()
{
    if (!g_sandDiagnosticEnabled || !g_sandShadowTankEnabled)
        return false;
    if (InterlockedCompareExchange(
            &g_sandReturnObserverStarted, 1, 0) != 0)
        return g_sandReturnObserverThread != nullptr;

    g_sandReturnObserverThread = CreateThread(
        nullptr, 0, SandReturnArrivalObserverThread,
        nullptr, 0, nullptr);
    if (!g_sandReturnObserverThread)
    {
        DWORD error = GetLastError();
        InterlockedExchange(&g_sandReturnObserverStarted, 0);
        ReportWindows("WARN", "Tank diagnostic",
            "return-arrival-observer",
            "The window-independent depot-arrival observer could not be started",
            error, "The return lock will still block clearing, but arrival/refill requires the Technical Services panel to be open");
        return false;
    }

    Report("INFO", "Tank diagnostic", "return-arrival-observer",
        "active=1 window_independent=1 poll_ms=50 global_initialization_interval_ms=%d global_registry=%d native_route_calls_from_worker=0 arrival_checks=tracked_return_locks_only empty_route_release=immediate-read-only arrival_settle_ms=%d nonempty_route_cleanup_thread=game-window",
        g_sandGlobalInitializationIntervalMs,
        InterlockedCompareExchange(
            &g_sandGlobalRegistryVerified, 0, 0),
        g_sandReturnArrivalSettleMs);
    return true;
}

static void SandLogStateTransition(const char* source, void* vehicle,
                                   const SandTrackUpdate& update)
{
    if (!update.buildingChanged) return;
    const char* eventName = "BUILDING_CHANGED";
    if (update.previousInsideHome && !update.insideHome)
        eventName = "DEPARTED_HOME";
    else if (!update.previousInsideHome && update.insideHome)
        eventName = "ARRIVED_HOME";
    Report("INFO", "Tank diagnostic", "vehicle-state-transition",
        "source=%s event=%s vehicle=%p previous_current=%p current=%p previous_home=%p home=%p previous_inside_home=%d inside_home=%d",
        source ? source : "unknown", eventName, vehicle,
        update.previousCurrentBuilding, update.currentBuilding,
        update.previousHomeBuilding, update.homeBuilding,
        update.previousInsideHome, update.insideHome);
}

static void SandDiagnosticObserve(void* game, BYTE* building)
{
    if (!g_sandDiagnosticEnabled || !building) return;
    InterlockedExchangePointer(
        (PVOID volatile*)&g_sandGameContext, game);
    ULONGLONG now = GetTickCount64();
    bool firstBuildingSample = false;
    bool fullSample = SandTakePanelSample(
        building, now, false, &firstBuildingSample);
    // During an armed return the panel observer also performs lightweight
    // state checks between normal one-second summaries. This catches both the
    // next completed route segment and very short depot-arrival transitions.
    if (!fullSample && !SandHasActiveReturnLock())
        return;

    if (!ReadablePtr(building + B_VEHICLE_BEGIN, 16))
    {
        RuntimeWarning("Grit diagnostic", "vehicle-vector",
            "Technical Services vehicle vector fields are unreadable");
        return;
    }
    void** begin = *(void***)(building + B_VEHICLE_BEGIN);
    void** end = *(void***)(building + B_VEHICLE_END);
    if ((!begin && end) || (begin && !end) ||
        (begin && end && end < begin))
    {
        RuntimeWarning("Grit diagnostic", "vehicle-vector",
            "Technical Services vehicle vector pointers are invalid");
        return;
    }

    size_t bytes = begin ? (size_t)((BYTE*)end - (BYTE*)begin) : 0;
    if ((bytes % sizeof(void*)) != 0)
    {
        RuntimeWarning("Grit diagnostic", "vehicle-vector",
            "Technical Services vehicle vector is not pointer-aligned");
        return;
    }
    size_t count = bytes / sizeof(void*);
    if (count > (size_t)g_sandMaxVehicles ||
        (bytes && !ReadablePtr(begin, bytes)))
    {
        RuntimeWarning("Grit diagnostic", "vehicle-vector",
            "Technical Services vehicle count %llu exceeds max_vehicles=%d or the vector is unreadable",
            (unsigned long long)count, g_sandMaxVehicles);
        return;
    }

    SpreaderStorageObservation material = {};
    if (fullSample) material = SpreaderObserveStorage(building);
    size_t snowplows = 0;
    size_t unreadableSkills = 0;
    void* returnVehicles[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
    int returnActionKinds[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
    int returnReasons[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
    DWORD returnPreviousStates[SAND_DIAG_MAX_TRACKED_VEHICLES] = {};
    size_t returnActionCount = 0;
    for (size_t index = 0; index < count; ++index)
    {
        BYTE* vehicle = (BYTE*)begin[index];
        if (!vehicle ||
            !ReadablePtr(vehicle + SAND_VEHICLE_HOME_BUILDING, sizeof(void*)))
        {
            Report("WARN", "Grit diagnostic", "vehicle-pointer",
                "building=%p index=%llu vehicle=%p is unreadable",
                building, (unsigned long long)index, vehicle);
            continue;
        }

        SandSkillView skills = {};
        SandReadSkillView(vehicle, &skills);
        if (skills.snowplow) ++snowplows;
        if (!skills.valid) ++unreadableSkills;
        void* currentBuilding =
            *(void**)(vehicle + SAND_VEHICLE_CURRENT_BUILDING);
        void* homeBuilding =
            *(void**)(vehicle + SAND_VEHICLE_HOME_BUILDING);
        SandTrackUpdate update = {};
        SandUpdateTrackedVehicle(vehicle, skills,
                                 currentBuilding, homeBuilding,
                                 true, &update);
        bool restoredTank = SpreaderTryRestorePersistedTank(
            building, index, vehicle, skills,
            currentBuilding, homeBuilding,
            update.firstSeen != 0);
        if (!restoredTank)
        {
            SandObserveShadowTankState(
                "technical-panel", vehicle, skills,
                homeBuilding, update);
        }
        SandLogStateTransition("technical-panel", vehicle, update);
        int returnActionKind = 0;
        int returnReason = SAND_RETURN_NONE;
        DWORD returnPreviousState = 0;
        if (skills.snowplow &&
            returnActionCount < SAND_DIAG_MAX_TRACKED_VEHICLES &&
            SandPrepareReturnAction(
                vehicle, currentBuilding, homeBuilding, now,
                &returnActionKind, &returnReason,
                &returnPreviousState))
        {
            returnVehicles[returnActionCount] = vehicle;
            returnActionKinds[returnActionCount] = returnActionKind;
            returnReasons[returnActionCount] = returnReason;
            returnPreviousStates[returnActionCount] = returnPreviousState;
            ++returnActionCount;
        }
        if (g_debug && fullSample && (update.firstSeen || update.skillChanged ||
            update.buildingChanged || firstBuildingSample)
           )
        {
            char skillText[256];
            SandFormatSkills(skills, skillText, sizeof(skillText));
            Report("INFO", "Grit diagnostic", "vehicle-skill",
                "building=%p index=%llu vehicle=%p type=%p skills=[%s] snowplow=%d current_building=%p home_building=%p inside_home=%d clear_calls=%lld",
                building, (unsigned long long)index, vehicle,
                skills.typeDescription, skillText, skills.snowplow,
                currentBuilding, homeBuilding,
                currentBuilding && currentBuilding == homeBuilding ? 1 : 0,
                (long long)update.clearCalls);
        }
    }
    for (size_t i = 0; i < returnActionCount; ++i)
    {
        SandExecuteReturnAction(
            returnVehicles[i], returnActionKinds[i],
            returnReasons[i], returnPreviousStates[i]);
    }
    if (!fullSample || !g_debug) return;

    LONG64 clearCalls = InterlockedCompareExchange64(&g_sandClearCalls, 0, 0);
    LONG64 uniqueClearCalls =
        InterlockedCompareExchange64(&g_sandUniqueClearCalls, 0, 0);
    LONG64 duplicateClearCalls =
        InterlockedCompareExchange64(&g_sandDuplicateClearCalls, 0, 0);
    LONG64 consumptionEvents =
        InterlockedCompareExchange64(&g_sandConsumptionEvents, 0, 0);
    LONG64 emptyEvents =
        InterlockedCompareExchange64(&g_sandEmptyEvents, 0, 0);
    LONG64 depotRefillDebitEvents =
        InterlockedCompareExchange64(
            &g_sandDepotRefillDebitEvents, 0, 0);
    LONG64 depotRefillPartialEvents =
        InterlockedCompareExchange64(
            &g_sandDepotRefillPartialEvents, 0, 0);
    LONG64 depotRefillDebitFailures =
        InterlockedCompareExchange64(
            &g_sandDepotRefillDebitFailures, 0, 0);
    LONG64 shadowTankLoadEvents =
        InterlockedCompareExchange64(
            &g_sandShadowTankLoadEvents, 0, 0);
    LONG64 shadowTankUseEvents =
        InterlockedCompareExchange64(
            &g_sandShadowTankUseEvents, 0, 0);
    LONG64 shadowTankEmptyEvents =
        InterlockedCompareExchange64(
            &g_sandShadowTankEmptyEvents, 0, 0);
    LONG64 returnRequests =
        InterlockedCompareExchange64(&g_sandReturnRequests, 0, 0);
    LONG64 returnReassignments =
        InterlockedCompareExchange64(&g_sandReturnReassignments, 0, 0);
    LONG64 returnArrivals =
        InterlockedCompareExchange64(&g_sandReturnArrivals, 0, 0);
    LONG64 returnFinishRoadClears =
        InterlockedCompareExchange64(
            &g_sandReturnFinishRoadClears, 0, 0);
    LONG64 returnReactivationRequests =
        InterlockedCompareExchange64(
            &g_sandReturnReactivationRequests, 0, 0);
    LONG64 returnBlockedClears =
        InterlockedCompareExchange64(&g_sandReturnBlockedClears, 0, 0);
    if (material.found)
    {
        Report("INFO", "Grit diagnostic", "sample",
            "building=%p vehicles=%llu snowplows=%llu unreadable_skills=%llu material_index=%d material=%s protection_strength=%.3f storage=%llu slot=%llu amount=%.6f/%.6f class=%s(%d) confirmed_clear_calls=%lld unique_clear_calls=%lld duplicate_clear_calls=%lld shadow_tank_loads=%lld shadow_tank_uses=%lld shadow_tank_empty_events=%lld return_requests=%lld return_reassertions=%lld return_arrivals=%lld return_finish_road_clears=%lld return_reactivation_requests=%lld return_blocked_clears=%lld depot_refill_debits=%lld depot_refill_partial=%lld depot_refill_debit_failures=%lld legacy_clear_consumption_events=%lld empty_events=%lld",
            building, (unsigned long long)count,
            (unsigned long long)snowplows,
            (unsigned long long)unreadableSkills,
            material.materialIndex,
            material.resourceName[0] ? material.resourceName : "none",
            material.protectionStrength,
            (unsigned long long)material.storageIndex,
            (unsigned long long)material.slotIndex,
            material.amount, material.capacity,
            TransportClassName(material.transportClass),
            material.transportClass,
            (long long)clearCalls, (long long)uniqueClearCalls,
            (long long)duplicateClearCalls,
            (long long)shadowTankLoadEvents,
            (long long)shadowTankUseEvents,
            (long long)shadowTankEmptyEvents,
            (long long)returnRequests,
            (long long)returnReassignments,
            (long long)returnArrivals,
            (long long)returnFinishRoadClears,
            (long long)returnReactivationRequests,
            (long long)returnBlockedClears,
            (long long)depotRefillDebitEvents,
            (long long)depotRefillPartialEvents,
            (long long)depotRefillDebitFailures,
            (long long)consumptionEvents,
            (long long)emptyEvents);
    }
    else
    {
        Report("INFO", "Grit diagnostic", "sample",
            "building=%p vehicles=%llu snowplows=%llu unreadable_skills=%llu configured_material=NOT_FOUND confirmed_clear_calls=%lld unique_clear_calls=%lld duplicate_clear_calls=%lld shadow_tank_loads=%lld shadow_tank_uses=%lld shadow_tank_empty_events=%lld return_requests=%lld return_reassertions=%lld return_arrivals=%lld return_finish_road_clears=%lld return_reactivation_requests=%lld return_blocked_clears=%lld depot_refill_debits=%lld depot_refill_partial=%lld depot_refill_debit_failures=%lld legacy_clear_consumption_events=%lld empty_events=%lld",
            building, (unsigned long long)count,
            (unsigned long long)snowplows,
            (unsigned long long)unreadableSkills,
            (long long)clearCalls, (long long)uniqueClearCalls,
            (long long)duplicateClearCalls,
            (long long)shadowTankLoadEvents,
            (long long)shadowTankUseEvents,
            (long long)shadowTankEmptyEvents,
            (long long)returnRequests,
            (long long)returnReassignments,
            (long long)returnArrivals,
            (long long)returnFinishRoadClears,
            (long long)returnReactivationRequests,
            (long long)returnBlockedClears,
            (long long)depotRefillDebitEvents,
            (long long)depotRefillPartialEvents,
            (long long)depotRefillDebitFailures,
            (long long)consumptionEvents,
            (long long)emptyEvents);
    }
}

static void SandRecordClearEvent(int callSite, void* vehicle,
                                 void* road, void* selector)
{
    LONG64 totalCalls = InterlockedIncrement64(&g_sandClearCalls);
    BYTE* vehicleBytes = (BYTE*)vehicle;
    SandSkillView skills = {};
    SandReadSkillView(vehicleBytes, &skills);
    void* currentBuilding = nullptr;
    void* homeBuilding = nullptr;
    if (vehicleBytes &&
        ReadablePtr(vehicleBytes + SAND_VEHICLE_HOME_BUILDING, sizeof(void*)))
    {
        currentBuilding =
            *(void**)(vehicleBytes + SAND_VEHICLE_CURRENT_BUILDING);
        homeBuilding =
            *(void**)(vehicleBytes + SAND_VEHICLE_HOME_BUILDING);
    }

    SandTrackUpdate update = {};
    SandUpdateTrackedVehicle(vehicle, skills,
                             currentBuilding, homeBuilding,
                             false, &update);
    SandObserveShadowTankState(
        "clear-observer", vehicle, skills,
        homeBuilding, update);
    SandLogStateTransition("clear-observer", vehicle, update);
    ULONGLONG now = GetTickCount64();
    bool shouldLog = false;
    bool duplicate = false;
    LONG64 vehicleCalls = 0;
    AcquireSRWLockExclusive(&g_sandTrackerLock);
    SandTrackedVehicle* tracked = SandFindTrackedVehicleLocked(vehicle, true);
    if (tracked)
    {
        vehicleCalls = ++tracked->clearCalls;
        duplicate = SandIsDuplicateClearLocked(
            tracked, road, selector, now);
        shouldLog = vehicleCalls == 1 ||
            g_sandClearLogIntervalMs == 0 ||
            now - tracked->lastClearLogTick >=
                (ULONGLONG)g_sandClearLogIntervalMs;
        if (shouldLog) tracked->lastClearLogTick = now;
    }
    ReleaseSRWLockExclusive(&g_sandTrackerLock);

    float currentSpeedKmh = 0.0f;
    float plowSpeedKmh = 0.0f;
    bool activePlowing = SandMarkQualifiedClearActivity(
        vehicle, skills, road, now,
        &currentSpeedKmh, &plowSpeedKmh);
    SandShadowTankUseResult shadowTank =
        SandUseShadowTank(vehicle, skills, duplicate,
                          activePlowing,
                          currentSpeedKmh,
                          plowSpeedKmh);
    TsmGritRoadTreatment roadTreatment = {};
    bool roadTreatmentReady = SandBuildRoadTreatment(
        vehicle, road, selector, skills, duplicate,
        shadowTank, &roadTreatment);
    if (roadTreatmentReady)
        SpreaderPublishRoadTreatment(&roadTreatment);
    if (shadowTank.status == SAND_SHADOW_TANK_CONSUMED)
    {
        Report("INFO", "Tank diagnostic", "shadow-tank-use",
            "vehicle=%p road=%p selector=%p material_index=%d material=%s protection_strength=%.3f requested_kg=%.3f consumed_kg=%.3f tank_before_kg=%.3f tank_after_kg=%.3f capacity_kg=%.3f became_empty=%d active_plowing=%d current_speed_kmh=%.3f plow_speed_kmh=%.3f consumption_basis=native-fuel-delta real_fuel_mutation=0 depot_unchanged=1",
            vehicle, road, selector,
            shadowTank.materialIndex,
            shadowTank.resourceName[0] ?
                shadowTank.resourceName : "none",
            shadowTank.protectionStrength,
            shadowTank.requestedKg,
            shadowTank.consumedKg,
            shadowTank.beforeKg,
            shadowTank.afterKg,
            shadowTank.capacityKg,
            shadowTank.becameEmpty,
            shadowTank.activePlowing,
            shadowTank.currentSpeedKmh,
            shadowTank.plowSpeedKmh);
    }
    float returnThresholdKg = shadowTank.capacityKg *
        ((float)g_sandReturnThresholdBasisPoints / 10000.0f);
    bool returnThresholdReached =
        shadowTank.returnThresholdPending != 0;
    if (returnThresholdReached)
    {
        SpreaderStorageObservation depotMaterial = {};
        if (homeBuilding &&
            ReadTechnicalType((BYTE*)homeBuilding) ==
                BUILDING_GARBAGE_OFFICE)
            depotMaterial =
                SpreaderObserveStorage((BYTE*)homeBuilding);
        bool refillAvailable = depotMaterial.found &&
            IsFiniteNonNegative(depotMaterial.amount) &&
            depotMaterial.amount > 0.000001f;
        bool returnQueued = g_sandAutomaticReturnEnabled &&
            g_sandReturnLockEnabled && refillAvailable;
        if (returnQueued)
        {
            SandArmReturnLock(
                vehicle, SAND_RETURN_REFILL, true, nullptr,
                "shadow-tank-return-threshold");
        }
        SandResolveShadowTankReturnThreshold(vehicle);
        Report("INFO", "Tank diagnostic", "shadow-tank-return-threshold",
        "vehicle=%p home_building=%p capacity_kg=%.3f remaining_kg=%.3f threshold_basis_points=%d threshold_percent=%.2f threshold_kg=%.6f became_empty=%d depot_material_found=%d depot_material_index=%d depot_material=%s depot_material_available_kg=%.3f automatic_return_enabled=%d return_lock_enabled=%d return_requested=%d native_refuel_destination_latch=home-depot direct_route_dispatch=0 vanilla_route_owner=1 reserve_amount_preserved=1 continue_with_grit=%d dry_plowing=%d depot_unchanged=1 finish_same_road_once=0 native_clear_forwarding_continues=1 consumption_basis=native-fuel-delta",
            vehicle, homeBuilding,
            shadowTank.capacityKg,
            shadowTank.afterKg,
            g_sandReturnThresholdBasisPoints,
            (double)g_sandReturnThresholdBasisPoints / 100.0,
            returnThresholdKg,
            shadowTank.becameEmpty,
            depotMaterial.found ? 1 : 0,
            depotMaterial.found ? depotMaterial.materialIndex : -1,
            depotMaterial.found && depotMaterial.resourceName[0]
                ? depotMaterial.resourceName : "none",
            depotMaterial.found ?
                depotMaterial.amount * 1000.0f : 0.0f,
            g_sandAutomaticReturnEnabled,
            g_sandReturnLockEnabled,
            returnQueued ? 1 : 0,
            shadowTank.afterKg > 0.000001f ? 1 : 0,
            shadowTank.afterKg <= 0.000001f ? 1 : 0);
    }
    else if (shadowTank.status == SAND_SHADOW_TANK_EMPTY &&
             !duplicate &&
             g_sandAutomaticReturnEnabled &&
             g_sandReturnLockEnabled &&
             !SandVehicleReturnLockActive(vehicle))
    {
        // The tank may have become empty while every permitted depot material
        // was empty or OFF. Re-evaluate the live per-depot priorities and
        // amounts on each later unique dry clear so enabling or restocking a
        // material does not require another impossible tank transition.
        SpreaderStorageObservation depotMaterial = {};
        if (homeBuilding &&
            ReadTechnicalType((BYTE*)homeBuilding) ==
                BUILDING_GARBAGE_OFFICE)
        {
            depotMaterial =
                SpreaderObserveStorage((BYTE*)homeBuilding);
        }
        bool refillBecameAvailable = depotMaterial.found &&
            IsFiniteNonNegative(depotMaterial.amount) &&
            depotMaterial.amount > 0.000001f;
        if (refillBecameAvailable)
        {
            LONG64 reactivationRequests =
                InterlockedIncrement64(
                    &g_sandReturnReactivationRequests);
            SandArmReturnLock(
                vehicle, SAND_RETURN_REFILL, true, nullptr,
                "dry-tank-material-became-available");
            Report("INFO", "Tank diagnostic",
                "dry-tank-refill-became-available",
                "vehicle=%p home_building=%p road=%p selector=%p previous_tank_material_index=%d previous_tank_material=%s remaining_kg=%.3f capacity_kg=%.3f depot_material_index=%d depot_material=%s depot_material_available_kg=%.3f reactivation_requests=%lld return_requested=1 native_refuel_destination_latch=home-depot direct_route_dispatch=0 vanilla_route_owner=1 current_clear_dry=1 finish_same_road_once=0 native_clear_forwarding_continues=1 depot_unchanged=1",
                vehicle, homeBuilding, road, selector,
                shadowTank.materialIndex,
                shadowTank.resourceName[0] ?
                    shadowTank.resourceName : "none",
                shadowTank.afterKg,
                shadowTank.capacityKg,
                depotMaterial.materialIndex,
                depotMaterial.resourceName[0] ?
                    depotMaterial.resourceName : "none",
                depotMaterial.amount * 1000.0f,
                (long long)reactivationRequests);
        }
    }
    else if (shadowTank.status == SAND_SHADOW_TANK_UNINITIALIZED && shouldLog)
    {
        Report("WARN", "Tank diagnostic", "shadow-tank-uninitialized",
            "vehicle=%p home_building=%p reached a confirmed clearing call before it was observed inside its home depot; no virtual grit was consumed",
            vehicle, homeBuilding);
    }

    if (duplicate)
        InterlockedIncrement64(&g_sandDuplicateClearCalls);
    else
        InterlockedIncrement64(&g_sandUniqueClearCalls);

    if (!shouldLog || !g_debug) return;
    char skillText[256];
    SandFormatSkills(skills, skillText, sizeof(skillText));
    Report("INFO", "Grit diagnostic", "confirmed-clear-event",
        "site=%d total_calls=%lld vehicle_calls=%lld vehicle=%p road=%p selector=%p type=%p skills=[%s] snowplow=%d seen_in_technical_panel=%d current_building=%p home_building=%p duplicate=%d active_plowing=%d current_speed_kmh=%.3f plow_speed_kmh=%.3f speed_role=diagnostic-only upper_speed_limit=none shadow_tank_action=%s shadow_tank_material_index=%d shadow_tank_material=%s shadow_tank_protection_strength=%.3f shadow_tank_before_kg=%.3f shadow_tank_after_kg=%.3f shadow_tank_capacity_kg=%.3f road_treatment_ready=%d road_treatment_dry=%d road_treatment_strength=%.3f consumption_basis=native-fuel-delta real_fuel_mutation=0",
        callSite, (long long)totalCalls, (long long)vehicleCalls,
        vehicle, road, selector, skills.typeDescription,
        skillText, skills.snowplow, update.seenInTechnicalPanel,
        currentBuilding, homeBuilding, duplicate ? 1 : 0,
        activePlowing ? 1 : 0,
        currentSpeedKmh, plowSpeedKmh,
        SandShadowTankStatusName(shadowTank.status),
        shadowTank.materialIndex,
        shadowTank.resourceName[0] ?
            shadowTank.resourceName : "none",
        shadowTank.protectionStrength,
        shadowTank.beforeKg, shadowTank.afterKg,
        shadowTank.capacityKg,
        roadTreatmentReady ? 1 : 0,
        roadTreatmentReady &&
            (roadTreatment.flags & TSM_GRIT_TREATMENT_DRY) ? 1 : 0,
        roadTreatmentReady ? roadTreatment.protectionStrength : -1.0f);
}

static void SandCallRoadSnowClear(int callSite, void* vehicle,
                                  void* road, void* selector)
{
    InterlockedCompareExchange(
        &g_sandClearThreadId, (LONG)GetCurrentThreadId(), 0);
    // Refill return and plough activity are deliberately independent. While
    // vanilla is still issuing confirmed clear calls, forward every one and
    // keep consuming the remaining grit down to zero; later calls publish dry
    // treatment. Manual-home return retains the established clear suppression.
    bool returnLockedBeforeCall =
        SandVehicleReturnLockActive(vehicle);
    bool refillReturnBeforeCall =
        SandVehicleRefillReturnActive(vehicle);
    LONG64 vehicleFinishClears = 0;
    LONG64 totalFinishClears = 0;
    int finishReason = SAND_RETURN_NONE;
    bool finishRoadClearAllowed = returnLockedBeforeCall &&
        !refillReturnBeforeCall &&
        SandTakeFinishRoadClearPermission(
            vehicle, road,
            &vehicleFinishClears,
            &totalFinishClears,
            &finishReason);
    SpreaderClearPublishedRoadTreatment();
    if (returnLockedBeforeCall && !refillReturnBeforeCall &&
        !finishRoadClearAllowed)
    {
        // A return-requested vehicle is finishing its current native work
        // route without treating further roads. The verified vanilla job
        // boundary will send it home; no route helper is called here.
        SandMarkBlockedClearDuringReturn(vehicle, road, selector);
        // A return armed by the periodic dry-tank observer first reaches a
        // safe game thread here. Consume its one dispatch opportunity now.
        SandDispatchReturnFromClearHook(vehicle, "blocked-clear-start");
        return;
    }
    if (finishRoadClearAllowed)
    {
        // This is only the second native half of the road object which crossed
        // the reserve threshold. Forward it so no visible strip is skipped,
        // but do not run SandRecordClearEvent: that would subtract another
        // configured use step and re-arm an already active return. Publish an
        // explicit dry treatment so weather_roads preserves any existing
        // stronger protection according to its configured dry-plough policy.
        TsmGritRoadTreatment dryTreatment = {};
        dryTreatment.structSize = sizeof(dryTreatment);
        dryTreatment.flags = TSM_GRIT_TREATMENT_DRY;
        dryTreatment.vehicle = vehicle;
        dryTreatment.road = road;
        dryTreatment.selector = selector;
        dryTreatment.materialIndex = -1;
        dryTreatment.protectionStrength = 0.0f;
        dryTreatment.consumedKg = 0.0f;
        SpreaderPublishRoadTreatment(&dryTreatment);
        // This second half of the threshold road returns before the common
        // post-clear block below, so establish the native destination latch
        // here as well.
        SandDispatchReturnFromClearHook(
            vehicle, "finish-threshold-road");
        __try
        {
            if (g_sandRoadSnowClearEntry)
                g_sandRoadSnowClearEntry(vehicle, road, selector);
        }
        __finally
        {
            SpreaderClearPublishedRoadTreatment();
        }
        Report("INFO", "Tank diagnostic",
            "return-lock-finish-road-clear",
            "vehicle=%p road=%p selector=%p reason=%s vehicle_finish_road_clears=%lld total_finish_road_clears=%lld dry_clear=1 one_shot_permission_consumed=1 original_snow_clear_forwarded=1 shadow_tank_consumed=0 return_lock_rearmed=0 further_clears_blocked_until_arrival=1",
            vehicle, road, selector,
            SandReturnReasonName(finishReason),
            (long long)vehicleFinishClears,
            (long long)totalFinishClears);
        return;
    }
    __try
    {
        SandRecordClearEvent(callSite, vehicle, road, selector);
    }
    __except(FaultFilter("grit spreader confirmed clearing observer",
                         GetExceptionInformation()))
    {
        if (InterlockedCompareExchange(&g_sandClearFaultLogged, 1, 0) == 0)
        {
            Report("WARN", "Grit spreader", "clear-observer-fault",
                "The clearing observer or guarded grit update faulted; the original game function is still called exactly once");
        }
    }
    __try
    {
        if (g_sandRoadSnowClearEntry)
            g_sandRoadSnowClearEntry(vehicle, road, selector);
    }
    __finally
    {
        SpreaderClearPublishedRoadTreatment();
    }

    if (finishRoadClearAllowed)
    {
        Report("INFO", "Tank diagnostic",
            "return-lock-finish-road-clear",
            "vehicle=%p road=%p selector=%p reason=%s vehicle_finish_road_clears=%lld total_finish_road_clears=%lld dry_clear=1 one_shot_permission_consumed=1 original_snow_clear_forwarded=1 further_clears_blocked_until_arrival=1",
            vehicle, road, selector,
            SandReturnReasonName(finishReason),
            (long long)vehicleFinishClears,
            (long long)totalFinishClears);
    }

    // If this clear crossed the configured reserve threshold, latch the home
    // depot into vanilla's persistent refuelling-destination field now. No
    // route helper is called; the later native route-completion logic consumes
    // that field itself.
    if (SandVehicleReturnLockActive(vehicle))
        SandDispatchReturnFromClearHook(
            vehicle, "post-threshold-clear");
}

static void __fastcall SandRoadSnowClearObserved1(void* vehicle,
                                                  void* road,
                                                  void* selector)
{
    SandCallRoadSnowClear(1, vehicle, road, selector);
}
static void __fastcall SandRoadSnowClearObserved2(void* vehicle,
                                                  void* road,
                                                  void* selector)
{
    SandCallRoadSnowClear(2, vehicle, road, selector);
}

// This call site belongs to vanilla's own route-completion branch. It is not
// an injected dispatch: the original game would call the same helper with the
// same vehicle at this exact point. The bridge only records when an empty-tank
// return request reaches that safe boundary and forwards the call unchanged.

static BYTE* SandBuildNearJumpStub(BYTE* anchor, void* handler,
                                   const char* label)
{
    if (!H || !H->allocNear || !anchor || !handler) return nullptr;
    BYTE* stub = H->allocNear(anchor, 16);
    if (!stub)
    {
        Report("ERROR", "Grit diagnostic", "clear-call-stub",
            "%s could not allocate executable memory near call site %p",
            label, anchor);
        return nullptr;
    }
    BYTE code[16];
    memset(code, 0xCC, sizeof(code));
    code[0] = 0x48;
    code[1] = 0xB8;                    // mov rax, imm64
    memcpy(code + 2, &handler, sizeof(handler));
    code[10] = 0xFF;
    code[11] = 0xE0;                   // jmp rax
    memcpy(stub, code, sizeof(code));
    if (!FlushCodeChecked(stub, sizeof(code), label)) return nullptr;
    return stub;
}

static bool SandPatchRelativeCall(BYTE* callSite, const BYTE expected[5],
                                  BYTE* stub, const char* label)
{
    if (!callSite || !expected || !stub ||
        !ReadablePtr(callSite, 5) || memcmp(callSite, expected, 5) != 0)
    {
        Report("ERROR", "Grit diagnostic", "clear-call-patch",
            "%s refused because the verified call bytes do not match", label);
        return false;
    }
    INT64 distance = (INT64)(uintptr_t)stub -
                     (INT64)(uintptr_t)(callSite + 5);
    if (distance < INT_MIN || distance > INT_MAX)
    {
        Report("ERROR", "Grit diagnostic", "clear-call-patch",
            "%s refused because its generated stub is outside rel32 range",
            label);
        return false;
    }
    BYTE replacement[5] = { 0xE8,0,0,0,0 };
    int relative = (int)distance;
    memcpy(replacement + 1, &relative, sizeof(relative));
    DWORD oldProtect = 0;
    if (!MakeCodeWritable(callSite, sizeof(replacement), &oldProtect, label)) return false;
    memcpy(callSite, replacement, sizeof(replacement));
    FinishCodePatch(callSite, sizeof(replacement), oldProtect, label);
    return true;
}

static bool SandVerifyNativeFuelBuildingPath()
{
    BYTE* compareSite =
        g_exeBase + RVA_SAND_NATIVE_LOW_FUEL_COMPARE;
    BYTE* writeSite =
        g_exeBase + RVA_SAND_NATIVE_FUEL_BUILDING_WRITE;
    if (!ReadablePtr(compareSite,
                     sizeof(EXPECT_SAND_NATIVE_LOW_FUEL_COMPARE)) ||
        memcmp(compareSite, EXPECT_SAND_NATIVE_LOW_FUEL_COMPARE,
               sizeof(EXPECT_SAND_NATIVE_LOW_FUEL_COMPARE)) != 0 ||
        !ReadablePtr(writeSite,
                     sizeof(EXPECT_SAND_NATIVE_FUEL_BUILDING_WRITE)) ||
        memcmp(writeSite, EXPECT_SAND_NATIVE_FUEL_BUILDING_WRITE,
               sizeof(EXPECT_SAND_NATIVE_FUEL_BUILDING_WRITE)) != 0)
    {
        Report("ERROR", "SOVIET64.exe",
            "native-refuel-destination-path",
            "The verified real-low-fuel comparison at exe+0x%X or its vehicle+0x%llX destination write at exe+0x%X does not match; automatic grit return is disabled",
            RVA_SAND_NATIVE_LOW_FUEL_COMPARE,
            (unsigned long long)SAND_VEHICLE_FUEL_BUILDING,
            RVA_SAND_NATIVE_FUEL_BUILDING_WRITE);
        return false;
    }
    InterlockedExchange(
        &g_sandNativeFuelBuildingPathVerified, 1);
    Report("INFO", "Tank diagnostic",
        "native-refuel-destination-path",
        "verified=1 native_low_fuel_compare=exe+0x%X native_destination_write=exe+0x%X vehicle_fuel_building=+0x%llX patch_installed=0 adopted_state=home-depot-pointer game_route_owner=1",
        RVA_SAND_NATIVE_LOW_FUEL_COMPARE,
        RVA_SAND_NATIVE_FUEL_BUILDING_WRITE,
        (unsigned long long)SAND_VEHICLE_FUEL_BUILDING);
    return true;
}

static bool SandVerifyNativeFuelConsumptionPath()
{
    BYTE* capacityLoad =
        g_exeBase + RVA_SAND_NATIVE_FUEL_CAPACITY_LOAD;
    BYTE* speedCompare =
        g_exeBase + RVA_SAND_NATIVE_SPEED_COMPARE;
    if (!ReadablePtr(capacityLoad,
                     sizeof(EXPECT_SAND_NATIVE_FUEL_CAPACITY_LOAD)) ||
        memcmp(capacityLoad, EXPECT_SAND_NATIVE_FUEL_CAPACITY_LOAD,
               sizeof(EXPECT_SAND_NATIVE_FUEL_CAPACITY_LOAD)) != 0 ||
        !ReadablePtr(speedCompare,
                     sizeof(EXPECT_SAND_NATIVE_SPEED_COMPARE)) ||
        memcmp(speedCompare, EXPECT_SAND_NATIVE_SPEED_COMPARE,
               sizeof(EXPECT_SAND_NATIVE_SPEED_COMPARE)) != 0)
    {
        Report("ERROR", "SOVIET64.exe",
            "native-fuel-grit-consumption-path",
            "The verified native fuel-capacity load at exe+0x%X or skill-35/current-speed comparison at exe+0x%X does not match; fuel-proportional grit consumption is disabled without falling back to per-road subtraction",
            RVA_SAND_NATIVE_FUEL_CAPACITY_LOAD,
            RVA_SAND_NATIVE_SPEED_COMPARE);
        return false;
    }
    InterlockedExchange(
        &g_sandNativeFuelConsumptionPathVerified, 1);
    Report("INFO", "Tank diagnostic",
        "native-fuel-grit-consumption-path",
        "verified=1 vehicle_current_fuel=+0x%llX type_fuel_capacity=+0x%llX vehicle_current_speed_kmh=+0x%llX native_capacity_load=exe+0x%X skill35_speed_compare=exe+0x%X fuel_consumption_factor_percent=%d speed_role=stationary-suppression-only upper_speed_limit=none real_fuel_read_only=1 real_fuel_mutation=0 per_road_subtraction=0",
        (unsigned long long)SAND_VEHICLE_CURRENT_FUEL,
        (unsigned long long)SAND_TYPE_FUEL_CAPACITY,
        (unsigned long long)SAND_VEHICLE_CURRENT_SPEED_KMH,
        RVA_SAND_NATIVE_FUEL_CAPACITY_LOAD,
        RVA_SAND_NATIVE_SPEED_COMPARE,
        g_sandFuelConsumptionFactorPercent);
    return true;
}

// The loader's generic inline hook intentionally copies prologue bytes without
// relocating them. This predicate starts with both a RIP-relative compare and
// a short branch, so install a small purpose-built trampoline instead. It
// reproduces the displaced instructions with absolute addresses and preserves
// vanilla's two original continuation targets exactly.

static bool SandDiagnosticInstallClearHooks()
{
    if (!g_sandDiagnosticEnabled) return false;
    BYTE* skillCheck = g_exeBase + RVA_SAND_SKILL_CHECK;
    BYTE* clearEntry = g_exeBase + RVA_SAND_CLEAR_ENTRY;
    BYTE* call1 = g_exeBase + RVA_SAND_CLEAR_CALL_1;
    BYTE* call2 = g_exeBase + RVA_SAND_CLEAR_CALL_2;

    if (!ReadablePtr(skillCheck, sizeof(EXPECT_SAND_SKILL_CHECK)) ||
        memcmp(skillCheck, EXPECT_SAND_SKILL_CHECK,
               sizeof(EXPECT_SAND_SKILL_CHECK)) != 0)
    {
        Report("ERROR", "SOVIET64.exe", "snowplow-skill-layout",
            "The verified VEHICLESKILL lookup at exe+0x%X does not match; skill-based classification is disabled",
            RVA_SAND_SKILL_CHECK);
        return false;
    }
    InterlockedExchange(&g_sandSkillLayoutVerified, 1);
    SandDiagnosticVerifyGlobalRegistry();
    if (!ReadablePtr(clearEntry, sizeof(EXPECT_SAND_CLEAR_ENTRY)) ||
        memcmp(clearEntry, EXPECT_SAND_CLEAR_ENTRY,
               sizeof(EXPECT_SAND_CLEAR_ENTRY)) != 0)
    {
        Report("ERROR", "SOVIET64.exe", "clear-entry",
            "The verified road-snow clear entry at exe+0x%X does not match",
            RVA_SAND_CLEAR_ENTRY);
        return false;
    }
    if (!ReadablePtr(call1, 5) ||
        memcmp(call1, EXPECT_SAND_CLEAR_CALL_1, 5) != 0 ||
        !ReadablePtr(call2, 5) ||
        memcmp(call2, EXPECT_SAND_CLEAR_CALL_2, 5) != 0)
    {
        Report("ERROR", "SOVIET64.exe", "clear-call-sites",
            "A verified road-snow clear call does not match; no related call was patched");
        return false;
    }
    bool nativeFuelBuildingReady =
        SandVerifyNativeFuelBuildingPath();
    bool nativeFuelConsumptionReady =
        SandVerifyNativeFuelConsumptionPath();
    if (!nativeFuelBuildingReady)
    {
        g_sandAutomaticReturnEnabled = 0;
        g_sandReturnLockEnabled = 0;
    }

    BYTE* stub1 = SandBuildNearJumpStub(
        call1, (void*)&SandRoadSnowClearObserved1, "clear call site 1");
    BYTE* stub2 = SandBuildNearJumpStub(
        call2, (void*)&SandRoadSnowClearObserved2, "clear call site 2");
    if (!stub1 || !stub2) return false;
    g_sandRoadSnowClearEntry =
        (SandRoadSnowClearFn)(g_exeBase + RVA_SAND_CLEAR_ENTRY);

    bool first = SandPatchRelativeCall(call1, EXPECT_SAND_CLEAR_CALL_1,
                                       stub1, "clear call site 1");
    bool second = SandPatchRelativeCall(call2, EXPECT_SAND_CLEAR_CALL_2,
                                        stub2, "clear call site 2");
    LONG active = (first ? 1 : 0) + (second ? 1 : 0);
    InterlockedExchange(&g_sandClearHooksActive, active);
    InterlockedExchange(&g_sandNaturalBoundaryHookActive, 0);
    if (!first || !second)
    {
        Report("WARN", "Grit diagnostic", "clear-call-sites",
            "Only %ld/2 confirmed clear calls are active; automatic return requires both calls",
            active);
        g_sandAutomaticReturnEnabled = 0;
        g_sandReturnLockEnabled = 0;
        return false;
    }
    Report("INFO", "Grit diagnostic", "clear-call-sites",
        "Both confirmed road-snow clear call sites are active at exe+0x%X and exe+0x%X; forwarding continues through the public entry at exe+0x%X and remains compatible with weather_roads",
        RVA_SAND_CLEAR_CALL_1, RVA_SAND_CLEAR_CALL_2,
        RVA_SAND_CLEAR_ENTRY);
    Report("INFO", "Grit diagnostic", "native-job-boundary",
        "active=0 call=exe+0x%X original_target=exe+0x%X patch_installed=0 vanilla_call_untouched=1 native_fuel_state_machine_owner=1",
        RVA_SAND_NATURAL_HOME_CALL,
        RVA_SAND_PLAN_ROUTE_HOME);
    Report("INFO", "Grit diagnostic", "return-to-depot-path",
        "native_fuel_building_path_ready=%d native_fuel_consumption_path_ready=%d automatic_return=%d return_threshold_basis_points=%d return_lock=%d strategy=vanilla-native-refuel-destination-latch native_low_fuel_compare=exe+0x%X native_destination_write=exe+0x%X native_boundary_hook=%d native_boundary_call=exe+0x%X native_boundary_untouched=1 direct_route_dispatch=0 live_route_trim=0 plugin_route_planner=0 plugin_route_retry=0 plugin_timer_dispatch=0 real_fuel_mutation=0 vehicle_target_mutation=0 vehicle_fuel_building_mutation=home-only-on-verified-game-thread block_snow_clear_until_arrival=1 finish_same_road_once=1 finish_same_road_consumption=0 hidden_return_reserve_displayed_as_zero=1 periodic_dry_tank_reactivation=global-registry-scan arrival_settle_ms=0 arrival_route_cleanup=0 worker_native_route_calls=0 instant_storage_routine_used=0 vehicle_target_building=+0x%llX vehicle_fuel_building=+0x%llX",
        nativeFuelBuildingReady ? 1 : 0,
        nativeFuelConsumptionReady ? 1 : 0,
        g_sandAutomaticReturnEnabled,
        g_sandReturnThresholdBasisPoints,
        g_sandReturnLockEnabled,
        RVA_SAND_NATIVE_LOW_FUEL_COMPARE,
        RVA_SAND_NATIVE_FUEL_BUILDING_WRITE,
        InterlockedCompareExchange(
            &g_sandNaturalBoundaryHookActive, 0, 0),
        RVA_SAND_NATURAL_HOME_CALL,
        (unsigned long long)SAND_VEHICLE_TARGET_BUILDING,
        (unsigned long long)SAND_VEHICLE_FUEL_BUILDING);
    return true;
}

#endif // TECHNICAL_SERVICE_SAND_SPREADER_DIAGNOSTIC_H
