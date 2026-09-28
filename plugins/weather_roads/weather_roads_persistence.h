#pragma once
#include "weather_roads_persistence_format.h"

static const char WR_SIDE_FILE[] = "tesmioloader.weather_roads.protection.bin";
// Last successful native FILE close, after header/road writes, before optional
// autosave ZIP packing. The earlier 42CDAE call is only a partial save stage.
static const DWORD WR_SAVE_CLOSE_CALL = 0x42EE32;
static const char WR_TEXTURE_SYMBOL[] =
    "?CreateManagedTexture@C3D_MIDDLEPOINT@@QEAAPEAVC3DAPI_TEXTURE@@PEBD@Z";
typedef int (*WrCloseFn)(void*);
typedef void* (*WrTextureFn)(void*, const char*);
static WrCloseFn g_wrOriginalClose;
static WrTextureFn g_wrOriginalTexture;
static volatile LONG g_wrPersistenceActive, g_wrResetRequested, g_wrSuspended;
static volatile LONG g_wrVisualPending, g_wrRestorePending;
static SRWLOCK g_wrPersistenceLock = SRWLOCK_INIT;
static WrDiskHeader g_wrLoadedHeader;
static BYTE* g_wrLoadedPayload;
static bool g_wrRoadsPending;
static LONG g_wrLoadedGeneration;
static ULONGLONG g_wrNextRestoreTick;
static size_t g_wrPreviousRoadCount;
static unsigned g_wrStableRoadScans;

struct WrLiveRoad
{
    WrDiskRoad key;
    void* road;
    BYTE* begin;
};

static bool WrReadGameTimeUnsafe(void* world, LONG64* out)
{
    if (!world || !out || !ReadablePtr(world, OFF_DAYTIME + sizeof(float)))
        return false;
    int day = *(int*)((BYTE*)world + OFF_DAY);
    int year = *(int*)((BYTE*)world + OFF_YEAR);
    float time = *(float*)((BYTE*)world + OFF_DAYTIME);
    if (day < -1 || day > 366 || year < 0 || year > 100000 ||
        !_finite(time) || time < 0 || time > 60.1f) return false;
    *out = (LONG64)((((double)year * 367 + day) * 1440 + time * 24.0) * 1000 + 0.5);
    return *out > 0 && *out <= WR_PERSIST_MAX_TIME;
}

static bool WrReadGameTime(void* world, LONG64* out)
{
    __try { return WrReadGameTimeUnsafe(world, out); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool WrReadTerrainUnsafe(float out[4], void** texture)
{
    void* game = *(void**)(g_exeBase + RVA_GLOBAL_WORLD_PTR);
    if (!game || !ReadablePtr(game, OFF_TERRAIN + sizeof(void*))) return false;
    BYTE* terrain = *(BYTE**)((BYTE*)game + OFF_TERRAIN);
    if (!terrain || !ReadablePtr(terrain, MIN_TERRAIN_READ_SIZE)) return false;
    out[0] = *(float*)(terrain + OFF_TERRAIN_ORIGIN_X);
    out[1] = *(float*)(terrain + OFF_TERRAIN_ORIGIN_Z);
    out[2] = *(float*)(terrain + OFF_TERRAIN_SIZE_X);
    out[3] = *(float*)(terrain + OFF_TERRAIN_SIZE_Z);
    for (int i = 0; i < 4; ++i)
        if (!_finite(out[i]) || fabs(out[i]) > 2000000.0f) return false;
    if (out[2] <= 0 || out[3] <= 0) return false;
    if (texture) *texture = *(void**)(terrain + OFF_TERRAIN_MASK_TEXTURE);
    return true;
}

static bool WrReadTerrain(float out[4], void** texture)
{
    __try { return WrReadTerrainUnsafe(out, texture); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool WrFolder(const char* folder, char* out, size_t size)
{
    if (!folder || !folder[0]) return false;
    if (folder[1] == ':' || (folder[0] == '\\' && folder[1] == '\\') ||
        (folder[0] == '/' && folder[1] == '/'))
        return _snprintf_s(out, size, _TRUNCATE, "%s", folder) > 0;
    char executable[MAX_PATH * 2] = {};
    DWORD n = GetModuleFileNameA(NULL, executable, sizeof(executable));
    if (!n || n >= sizeof(executable)) return false;
    char* slash = strrchr(executable, '\\');
    if (!slash) return false;
    *slash = 0;
    bool media = _strnicmp(folder, "media_soviet\\", 13) == 0 ||
        _strnicmp(folder, "media_soviet/", 13) == 0;
    return _snprintf_s(out, size, _TRUNCATE,
        media ? "%s\\%s" : "%s\\media_soviet\\%s", executable, folder) > 0;
}

static bool WrPath(const char* folder, const char* name, char* out, size_t size)
{
    return _snprintf_s(out, size, _TRUNCATE, "%s\\%s", folder, name) > 0;
}

static bool WrRead(HANDLE file, void* buffer, DWORD bytes)
{
    DWORD read = 0;
    return !bytes || (ReadFile(file, buffer, bytes, &read, NULL) && read == bytes);
}
static bool WrWrite(HANDLE file, const void* buffer, DWORD bytes)
{
    DWORD written = 0;
    return !bytes || (WriteFile(file, buffer, bytes, &written, NULL) && written == bytes);
}

static bool WrHashFile(const char* folder, const char* name, uint64_t* hash)
{
    char path[MAX_PATH * 2];
    if (!WrPath(folder, name, path, sizeof(path))) return false;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    BYTE bytes[16384];
    DWORD read = 0;
    bool ok = true;
    uint64_t result = WrHash(NULL, 0);
    do
    {
        if (!ReadFile(file, bytes, sizeof(bytes), &read, NULL)) { ok = false; break; }
        result = WrHash(bytes, read, result);
    } while (read);
    CloseHandle(file);
    if (ok) *hash = result;
    return ok;
}

static bool WrNativeHashes(const char* folder, WrDiskHeader* h)
{
    return WrHashFile(folder, "header.bin", &h->nativeHeaderHash) &&
        WrHashFile(folder, "road.bin", &h->nativeRoadHash) &&
        WrHashFile(folder, "mask.dds", &h->nativeMaskHash);
}

static bool WrHashPosition(const BYTE* position, uint64_t* hash)
{
    if (!ReadablePtr(position, 12)) return false;
    int32_t fixed[3];
    for (int i = 0; i < 3; ++i)
    {
        float v = *(const float*)(position + 4 * i);
        if (!_finite(v) || fabs(v) > 2000000.0f) return false;
        double scaled = (double)v * 1000.0;
        fixed[i] = (int32_t)(scaled < 0 ? scaled - 0.5 : scaled + 0.5);
    }
    *hash = WrHash(fixed, sizeof(fixed), *hash);
    return true;
}

// Verified against exe+0x56AE30: road+8/+10 is a 24-byte point vector,
// its first 12 bytes are xyz, and endpoint nodes at +20/+28 store xyz at +4.
// exe+0x42AF60 traverses weather+F7A0 -> manager+2B0/+2B8 and reads type+120.
static bool WrRoadIdentity(void* road, WrLiveRoad* out)
{
    if (!road || !out || !ReadablePtr(road, 0x124)) return false;
    BYTE* r = (BYTE*)road;
    BYTE* begin = *(BYTE**)(r + OFF_ROAD_SNOW_BEG);
    BYTE* end = *(BYTE**)(r + OFF_ROAD_SNOW_END);
    BYTE* positions = *(BYTE**)(r + 8);
    BYTE* positionsEnd = *(BYTE**)(r + 0x10);
    if (!begin || !end || end <= begin || (size_t)(end - begin) > 16384 ||
        !ReadablePtr(begin, (size_t)(end - begin))) return false;
    if ((!positions != !positionsEnd) || positionsEnd < positions) return false;
    size_t pointBytes = positions ? (size_t)(positionsEnd - positions) : 0;
    if (pointBytes % 24 || pointBytes / 24 > 65536 ||
        (pointBytes && !ReadablePtr(positions, pointBytes))) return false;
    WrDiskRoad key = {};
    key.length = (uint32_t)(end - begin);
    key.geometryPoints = (uint32_t)(pointBytes / 24);
    key.type = *(uint32_t*)(r + 0x120);
    if (key.type > 4096) return false;
    uint64_t hash = WrHash(&key.length, sizeof(key.length));
    hash = WrHash(&key.type, sizeof(key.type), hash);
    hash = WrHash(&key.geometryPoints, sizeof(key.geometryPoints), hash);
    for (int endpoint = 0; endpoint < 2; ++endpoint)
    {
        BYTE* node = *(BYTE**)(r + 0x20 + 8 * endpoint);
        if (!node || !WrHashPosition(node + 4, &hash)) return false;
    }
    for (size_t p = 0; p < pointBytes; p += 24)
        if (!WrHashPosition(positions + p, &hash)) return false;
    key.geometryHash = hash;
    out->key = key; out->road = road; out->begin = begin;
    return true;
}

static WrLiveRoad* WrRoadMap(void* world, size_t* count, size_t* registryCount)
{
    *count = 0; *registryCount = 0;
    void** begin = NULL;
    void** end = NULL;
    __try
    {
        if (!world || !ReadablePtr((BYTE*)world + 0xF7A0, sizeof(void*))) return NULL;
        BYTE* manager = *(BYTE**)((BYTE*)world + 0xF7A0);
        if (!manager || !ReadablePtr(manager + 0x2B0, 16)) return NULL;
        begin = *(void***)(manager + 0x2B0);
        end = *(void***)(manager + 0x2B8);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return NULL; }
    if ((!begin != !end) || end < begin) return NULL;
    size_t size = begin ? (size_t)(end - begin) : 0;
    if (size > 1000000 || (size && !ReadablePtr(begin, size * sizeof(void*)))) return NULL;
    WrLiveRoad* map = (WrLiveRoad*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        (size ? size : 1) * sizeof(WrLiveRoad));
    if (!map) return NULL;
    *registryCount = size;
    for (size_t i = 0; i < size; ++i)
    {
        bool valid = false;
        __try { valid = WrRoadIdentity(begin[i], &map[*count]); }
        __except(EXCEPTION_EXECUTE_HANDLER) { valid = false; }
        if (valid) ++*count;
    }
    return map;
}

static int WrCompareKey(const WrDiskRoad& a, const WrDiskRoad& b)
{
    if (a.geometryHash != b.geometryHash) return a.geometryHash < b.geometryHash ? -1 : 1;
    if (a.length != b.length) return a.length < b.length ? -1 : 1;
    if (a.type != b.type) return a.type < b.type ? -1 : 1;
    if (a.geometryPoints != b.geometryPoints) return a.geometryPoints < b.geometryPoints ? -1 : 1;
    return 0;
}
static int __cdecl WrCompareGeometry(const void* a, const void* b)
{ return WrCompareKey(((const WrLiveRoad*)a)->key, ((const WrLiveRoad*)b)->key); }
static int __cdecl WrComparePointers(const void* a, const void* b)
{
    uintptr_t x = (uintptr_t)((const WrLiveRoad*)a)->road;
    uintptr_t y = (uintptr_t)((const WrLiveRoad*)b)->road;
    return x == y ? 0 : (x < y ? -1 : 1);
}
static const WrLiveRoad* WrFindGeometry(const WrLiveRoad* map, size_t count, const WrDiskRoad& key)
{
    size_t lo = 0, hi = count;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2;
        if (WrCompareKey(map[mid].key, key) < 0) lo = mid + 1; else hi = mid; }
    if (lo == count || WrCompareKey(map[lo].key, key)) return NULL;
    // Coincident/ambiguous geometry must never receive another road's state.
    if (lo + 1 < count && !WrCompareKey(map[lo + 1].key, key)) return NULL;
    return &map[lo];
}

static void WrClearLoadedLocked()
{
    if (g_wrLoadedPayload) HeapFree(GetProcessHeap(), 0, g_wrLoadedPayload);
    g_wrLoadedPayload = NULL;
    memset(&g_wrLoadedHeader, 0, sizeof(g_wrLoadedHeader));
    g_wrRoadsPending = false;
    g_wrLoadedGeneration = 0;
    g_wrNextRestoreTick = 0;
    g_wrPreviousRoadCount = SIZE_MAX;
    g_wrStableRoadScans = 0;
    InterlockedExchange(&g_wrVisualPending, 0);
    InterlockedExchange(&g_wrRestorePending, 0);
}

static bool WeatherPersistenceConsumeReset()
{ return InterlockedExchange(&g_wrResetRequested, 0) != 0; }
static bool WeatherPersistenceSuspended()
{ return InterlockedCompareExchange(&g_wrSuspended, 0, 0) != 0; }
static bool WeatherPersistenceVisualPending()
{ return InterlockedCompareExchange(&g_wrVisualPending, 0, 0) != 0; }

static void WrLoad(const char* nativeFolder)
{
    InterlockedExchange(&g_wrSuspended, 1);
    InterlockedExchange(&g_wrResetRequested, 1);
    AcquireSRWLockExclusive(&g_wrPersistenceLock);
    WrClearLoadedLocked();
    ReleaseSRWLockExclusive(&g_wrPersistenceLock);
    char folder[MAX_PATH * 2], path[MAX_PATH * 2];
    if (!WrFolder(nativeFolder, folder, sizeof(folder)) ||
        !WrPath(folder, WR_SIDE_FILE, path, sizeof(path)))
    { Warn("protection persistence load: invalid save folder"); return; }
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            Info("protection persistence load: folder=%s status=no-sidecar", folder);
        else Warn("protection persistence load: folder=%s error=%lu", folder, error);
        return;
    }
    WrDiskHeader h = {};
    LARGE_INTEGER fileBytes = {};
    bool valid = WrRead(file, &h, sizeof(h)) && WrHeaderValid(h) &&
        h.headerHash == WrHeaderChecksum(h) &&
        GetFileSizeEx(file, &fileBytes);
    size_t bytes = valid ? h.roads * sizeof(WrDiskRoad) + h.visuals * sizeof(WrDiskVisual) : 0;
    valid = valid && fileBytes.QuadPart == (LONGLONG)(sizeof(h) + bytes);
    BYTE* payload = valid ? (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes ? bytes : 1) : NULL;
    valid = valid && payload && WrRead(file, payload, (DWORD)bytes) &&
        WrHash(payload, bytes) == h.payloadHash;
    CloseHandle(file);
    WrDiskHeader native = {};
    const char* reason = valid ? "record-validation" : "sidecar-format-or-checksum";
    if (valid)
    {
        if (!WrNativeHashes(folder, &native)) { reason = "native-files-unreadable"; valid = false; }
        else if (native.nativeHeaderHash != h.nativeHeaderHash) { reason = "native-header-fingerprint"; valid = false; }
        else if (native.nativeRoadHash != h.nativeRoadHash) { reason = "native-road-fingerprint"; valid = false; }
        else if (native.nativeMaskHash != h.nativeMaskHash) { reason = "native-mask-fingerprint"; valid = false; }
    }
    if (valid)
    {
        const WrDiskRoad* roads = (const WrDiskRoad*)payload;
        const WrDiskVisual* visuals = (const WrDiskVisual*)(payload + h.roads * sizeof(WrDiskRoad));
        for (uint32_t i = 0; valid && i < h.roads; ++i) valid = WrRoadValid(roads[i], h);
        for (uint32_t i = 0; valid && i < h.visuals; ++i)
            valid = WrVisualValid(visuals[i], h) && (i == 0 || visuals[i - 1].index < visuals[i].index);
        for (int i = 0; i < 4; ++i)
            if (!_finite(h.terrain[i]) || fabs(h.terrain[i]) > 2000000.0f) valid = false;
    }
    if (!valid)
    {
        if (payload) HeapFree(GetProcessHeap(), 0, payload);
        Warn("protection persistence load: folder=%s status=rejected-invalid-or-stale-sidecar reason=%s native_save_unchanged=1", folder, reason);
        return;
    }
    AcquireSRWLockExclusive(&g_wrPersistenceLock);
    g_wrLoadedHeader = h; g_wrLoadedPayload = payload;
    g_wrRoadsPending = h.roads != 0;
    InterlockedExchange(&g_wrVisualPending, h.visuals &&
        InterlockedCompareExchange(&g_plowVisualEnabled, 0, 0) ? 1 : 0);
    InterlockedExchange(&g_wrRestorePending, 1);
    ReleaseSRWLockExclusive(&g_wrPersistenceLock);
    Info("protection persistence load: folder=%s roads=%u visual_texels=%u saved_game_minute_milli=%lld native_save_fingerprints=matched restore=deferred-until-world-ready",
        folder, h.roads, h.visuals, (long long)h.savedTime);
}

static bool WrRestoreRoad(const WrLiveRoad& live, const WrDiskRoad& saved, LONG64 cleared, LONG generation)
{
    uint64_t hash = ((uint64_t)(uintptr_t)live.road >> 4) ^
        ((uint64_t)(uintptr_t)live.begin >> 5) ^
        ((uint64_t)saved.index * 11400714819323198485ull) ^
        ((uint64_t)(unsigned long)generation * 0x9E3779B1u);
    int first = (int)(hash % (uint64_t)(g_cfg.maximumTrackedPlowPoints / PLOW_EFFECT_BUCKET_SIZE)) * PLOW_EFFECT_BUCKET_SIZE;
    int freeSlot = -1;
    LONG64 now = InterlockedCompareExchange64(&g_gameMinuteMilli, 0, 0);
    EnterCriticalSection(&g_plowEffectLock);
    for (int probe = 0; probe < g_cfg.maximumTrackedPlowPoints; ++probe)
    {
        int i = (first + probe) % g_cfg.maximumTrackedPlowPoints;
        PlowEffectPoint& p = g_plowEffectPoints[i];
        if (p.generation == 0)
        {
            if (freeSlot < 0) freeSlot = i;
            break;
        }
        bool active = p.active && p.generation == generation &&
            now < p.clearedGameMinuteMilli + PlowProtectionDuration() + PlowSaltDuration();
        if (!active && freeSlot < 0) freeSlot = i;
        if (active && p.road == live.road && p.vectorBegin == live.begin &&
            p.vectorLength == saved.length && p.index == saved.index)
        {
            // A brush may already have reached this point after loading. Only
            // recover the old treatment if it is stronger; never refresh it.
            if (p.treatmentStrengthMillion >= saved.strength)
            { LeaveCriticalSection(&g_plowEffectLock); return true; }
            freeSlot = i;
            break;
        }
    }
    if (freeSlot >= 0)
    {
        PlowEffectPoint& p = g_plowEffectPoints[freeSlot];
        memset(&p, 0, sizeof(p));
        p.active = 1; p.generation = generation; p.road = live.road;
        p.vectorBegin = live.begin; p.vectorLength = saved.length; p.index = saved.index;
        p.clearedGameMinuteMilli = cleared; p.saltRemainderMillion = saved.remainder;
        p.treatmentStrengthMillion = saved.strength;
        // A material's catalogue index is process-local; strength owns the effect.
        p.treatmentMaterialIndex = -1; p.treatmentFlags = saved.flags;
    }
    LeaveCriticalSection(&g_plowEffectLock);
    return freeSlot >= 0;
}

// Caller owns the existing visual mutation lock. Loading only recreates shadow
// state here; it never paints the terrain or resets the saved visible snow.
static void WeatherPersistenceRestoreVisualLocked(const TextureStorageState& storage)
{
    if (!WeatherPersistenceVisualPending() || WeatherPersistenceSuspended()) return;
    AcquireSRWLockExclusive(&g_wrPersistenceLock);
    if (!g_wrLoadedPayload || !g_wrLoadedGeneration ||
        g_wrLoadedGeneration != g_plowVisualGeneration)
    { ReleaseSRWLockExclusive(&g_wrPersistenceLock); return; }
    const WrDiskHeader& h = g_wrLoadedHeader;
    float terrain[4];
    bool match = WrReadTerrain(terrain, NULL) &&
        h.width == (uint32_t)storage.width && h.height == (uint32_t)storage.height;
    for (int i = 0; match && i < 4; ++i) match = fabs(terrain[i] - h.terrain[i]) <= 0.01f;
    unsigned restored = 0;
    if (match)
    {
        const WrDiskVisual* records = (const WrDiskVisual*)(g_wrLoadedPayload + h.roads * sizeof(WrDiskRoad));
        for (uint32_t i = 0; i < h.visuals; ++i)
        {
            const WrDiskVisual& v = records[i];
            LONG64 cleared = WrRestoreClearTime(v.clearedAt, h, PlowProtectionDuration(), PlowSaltDuration());
            if (!cleared || v.index >= g_plowVisualCapacity ||
                g_plowVisualClearMinuteMilli[v.index] != 0 ||
                g_plowVisualActiveCount >= g_plowVisualActiveCapacity) continue;
            g_plowVisualActiveIndices[g_plowVisualActiveCount++] = v.index;
            g_plowVisualClearMinuteMilli[v.index] = cleared;
            g_plowVisualStrengthMillion[v.index] = v.strength;
            g_plowVisualGreenShadow[v.index] = v.green;
            g_plowVisualClearGreen[v.index] = v.clearGreen;
            g_plowVisualSnowAmount[v.index] = v.snow;
            g_plowVisualSnowBurstGeneration[v.index] = 0;
            g_plowVisualSnowBurstAppliedUnits[v.index] = 0;
            ++restored;
        }
        InterlockedExchange64(&g_plowVisualActivePublished, (LONG64)g_plowVisualActiveCount);
    }
    InterlockedExchange(&g_wrVisualPending, 0);
    ReleaseSRWLockExclusive(&g_wrPersistenceLock);
    if (match) Info("protection persistence restore-visual: restored=%u texture=%dx%d terrain_match=1 lifetime_refresh=0 native_mask_write=0", restored, storage.width, storage.height);
    else Warn("protection persistence restore-visual: skipped terrain/texture mismatch native_mask_unchanged=1");
}

static void WeatherPersistenceTick(void* world)
{
    if (!InterlockedCompareExchange(&g_wrPersistenceActive, 0, 0)) return;
    if (!WeatherPersistenceSuspended() &&
        !InterlockedCompareExchange(&g_wrRestorePending, 0, 0)) return;
    LONG64 now = 0;
    if (!WrReadGameTime(world, &now)) return;
    LONG generation = InterlockedCompareExchange(&g_gameWorldGeneration, 0, 0);
    AcquireSRWLockExclusive(&g_wrPersistenceLock);
    if (g_wrLoadedPayload && now < g_wrLoadedHeader.savedTime)
    { ReleaseSRWLockExclusive(&g_wrPersistenceLock); return; }
    InterlockedExchange(&g_wrSuspended, 0);
    if (g_wrLoadedPayload && !g_wrLoadedGeneration) g_wrLoadedGeneration = generation;
    if (g_wrLoadedPayload && g_wrLoadedGeneration != generation) WrClearLoadedLocked();
    bool roadsPending = g_wrRoadsPending;
    ULONGLONG tick = GetTickCount64();
    if (roadsPending && tick >= g_wrNextRestoreTick)
    {
        g_wrNextRestoreTick = tick + 500;
        size_t count = 0, registryCount = 0;
        WrLiveRoad* map = WrRoadMap(world, &count, &registryCount);
        if (map)
        {
            qsort(map, count, sizeof(WrLiveRoad), WrCompareGeometry);
            g_wrStableRoadScans = registryCount == g_wrPreviousRoadCount ? g_wrStableRoadScans + 1 : 1;
            g_wrPreviousRoadCount = registryCount;
            const WrDiskRoad* records = (const WrDiskRoad*)g_wrLoadedPayload;
            unsigned matches = 0;
            for (uint32_t i = 0; i < g_wrLoadedHeader.roads; ++i)
                if (WrFindGeometry(map, count, records[i])) ++matches;
            // Restore immediately when all identities exist; otherwise allow
            // native loading to populate the registry before rejecting entries.
            if (matches == g_wrLoadedHeader.roads || g_wrStableRoadScans >= 4)
            {
                unsigned restored = 0, expired = 0, missing = 0, capacity = 0;
                for (uint32_t i = 0; i < g_wrLoadedHeader.roads; ++i)
                {
                    const WrDiskRoad& r = records[i];
                    LONG64 cleared = WrRestoreClearTime(r.clearedAt, g_wrLoadedHeader, PlowProtectionDuration(), PlowSaltDuration());
                    if (!cleared || now >= cleared + PlowProtectionDuration() + PlowSaltDuration()) { ++expired; continue; }
                    const WrLiveRoad* live = WrFindGeometry(map, count, r);
                    if (!live) { ++missing; continue; }
                    if (WrRestoreRoad(*live, r, cleared, generation)) ++restored; else ++capacity;
                }
                g_wrRoadsPending = false;
                Info("protection persistence restore-road: restored=%u expired=%u unmatched_or_ambiguous=%u capacity_skipped=%u live_roads=%llu lifetime_refresh=0 native_snow_write=0",
                    restored, expired, missing, capacity, (unsigned long long)registryCount);
            }
        }
        if (map) HeapFree(GetProcessHeap(), 0, map);
    }
    ReleaseSRWLockExclusive(&g_wrPersistenceLock);
    if (WeatherPersistenceVisualPending())
    {
        float terrain[4]; void* texture = NULL;
        if (WrReadTerrain(terrain, &texture) && texture)
        {
            InterlockedExchangePointer(&g_terrainMaskTexture, texture);
            TextureStorageState storage = {};
            if (ReadTextureStorageState(texture, &storage) && AcquirePlowVisualLockBounded())
            { PreparePlowVisualShadow(storage); InterlockedExchange(&g_plowVisualBusy, 0); }
        }
    }
    AcquireSRWLockExclusive(&g_wrPersistenceLock);
    if (!g_wrRoadsPending && !WeatherPersistenceVisualPending()) WrClearLoadedLocked();
    ReleaseSRWLockExclusive(&g_wrPersistenceLock);
}

static int __cdecl WrCompareVisual(const void* a, const void* b)
{
    uint32_t x = ((const WrDiskVisual*)a)->index, y = ((const WrDiskVisual*)b)->index;
    return x == y ? 0 : (x < y ? -1 : 1);
}

static void WrSave(const char* nativeFolder)
{
    char folder[MAX_PATH * 2], path[MAX_PATH * 2], temporary[MAX_PATH * 2];
    void* world = InterlockedCompareExchangePointer(&g_weatherWorld, NULL, NULL);
    WrDiskHeader h = {};
    if (!WrFolder(nativeFolder, folder, sizeof(folder)) ||
        !WrPath(folder, WR_SIDE_FILE, path, sizeof(path)) ||
        _snprintf_s(temporary, sizeof(temporary), _TRUNCATE, "%s.tmp", path) <= 0 ||
        !WrReadGameTime(world, &h.savedTime) || !WrReadTerrain(h.terrain, NULL) ||
        WeatherPersistenceSuspended())
    { Warn("protection persistence save: world/path not ready; sidecar not overwritten"); return; }
    WeatherPersistenceTick(world);
    AcquireSRWLockShared(&g_wrPersistenceLock);
    bool pending = g_wrRoadsPending;
    ReleaseSRWLockShared(&g_wrPersistenceLock);
    if (pending) { Warn("protection persistence save: road restore still pending; sidecar not overwritten"); return; }
    size_t count = 0, registryCount = 0;
    WrLiveRoad* map = WrRoadMap(world, &count, &registryCount);
    if (!map) { Warn("protection persistence save: native road registry unavailable"); return; }
    qsort(map, count, sizeof(WrLiveRoad), WrComparePointers);
    size_t maximumBytes = WR_PERSIST_MAX_ROADS * sizeof(WrDiskRoad) + WR_PERSIST_MAX_VISUALS * sizeof(WrDiskVisual);
    BYTE* payload = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, maximumBytes);
    if (!payload) { HeapFree(GetProcessHeap(), 0, map); return; }
    WrDiskRoad* roads = (WrDiskRoad*)payload;
    LONG generation = InterlockedCompareExchange(&g_gameWorldGeneration, 0, 0);
    EnterCriticalSection(&g_plowEffectLock);
    for (int i = 0; i < g_cfg.maximumTrackedPlowPoints; ++i)
    {
        const PlowEffectPoint& p = g_plowEffectPoints[i];
        if (!p.active || p.generation != generation || p.clearedGameMinuteMilli > h.savedTime ||
            h.savedTime >= p.clearedGameMinuteMilli + PlowProtectionDuration() + PlowSaltDuration()) continue;
        WrLiveRoad pointerKey = {}; pointerKey.road = p.road;
        const WrLiveRoad* live = (const WrLiveRoad*)bsearch(&pointerKey, map, count, sizeof(WrLiveRoad), WrComparePointers);
        if (!live || live->begin != p.vectorBegin || live->key.length != p.vectorLength || p.index >= p.vectorLength) continue;
        WrDiskRoad r = live->key;
        r.index = (uint32_t)p.index; r.strength = p.treatmentStrengthMillion;
        r.material = -1; r.flags = p.treatmentFlags;
        r.clearedAt = p.clearedGameMinuteMilli; r.remainder = p.saltRemainderMillion;
        if (WrRoadValid(r, h) && h.roads < WR_PERSIST_MAX_ROADS) roads[h.roads++] = r;
    }
    LeaveCriticalSection(&g_plowEffectLock);
    HeapFree(GetProcessHeap(), 0, map);
    WrDiskVisual* visuals = (WrDiskVisual*)(payload + h.roads * sizeof(WrDiskRoad));
    bool visualLocked = false;
    for (int attempt = 0; attempt < 100 && !visualLocked; ++attempt)
    { visualLocked = AcquirePlowVisualLockBounded(); if (!visualLocked) Sleep(1); }
    if (!visualLocked)
    { HeapFree(GetProcessHeap(), 0, payload); Warn("protection persistence save: visual snapshot busy; no partial sidecar written"); return; }
    AcquireSRWLockShared(&g_wrPersistenceLock);
    if (WeatherPersistenceVisualPending() && g_wrLoadedPayload)
    {
        const WrDiskHeader& old = g_wrLoadedHeader;
        const WrDiskVisual* loaded = (const WrDiskVisual*)(g_wrLoadedPayload + old.roads * sizeof(WrDiskRoad));
        h.width = old.width; h.height = old.height;
        for (uint32_t i = 0; i < old.visuals; ++i)
        {
            WrDiskVisual v = loaded[i];
            v.clearedAt = WrRestoreClearTime(v.clearedAt, old, PlowProtectionDuration(), PlowSaltDuration());
            if (v.clearedAt > 0 && v.clearedAt <= h.savedTime) visuals[h.visuals++] = v;
        }
    }
    else if (g_plowVisualGeneration == generation)
    {
        h.width = (uint32_t)g_plowVisualWidth; h.height = (uint32_t)g_plowVisualHeight;
        for (size_t i = 0; i < g_plowVisualActiveCount && h.visuals < WR_PERSIST_MAX_VISUALS; ++i)
        {
            DWORD index = g_plowVisualActiveIndices[i];
            if (index >= g_plowVisualCapacity) continue;
            WrDiskVisual v = {};
            v.index = index; v.strength = g_plowVisualStrengthMillion[index];
            v.clearedAt = g_plowVisualClearMinuteMilli[index];
            v.green = g_plowVisualGreenShadow[index]; v.clearGreen = g_plowVisualClearGreen[index];
            v.snow = g_plowVisualSnowAmount[index];
            if (WrVisualValid(v, h)) visuals[h.visuals++] = v;
        }
    }
    ReleaseSRWLockShared(&g_wrPersistenceLock);
    InterlockedExchange(&g_plowVisualBusy, 0);
    qsort(visuals, h.visuals, sizeof(WrDiskVisual), WrCompareVisual);
    uint32_t unique = 0;
    for (uint32_t i = 0; i < h.visuals; ++i)
        if (!unique || visuals[i].index != visuals[unique - 1].index) visuals[unique++] = visuals[i];
    h.visuals = unique;
    if (!h.visuals) h.width = h.height = 0;
    memcpy(h.magic, "WRPROT01", 8); h.version = WR_PERSIST_VERSION;
    h.headerBytes = sizeof(h); h.roadBytes = sizeof(WrDiskRoad); h.visualBytes = sizeof(WrDiskVisual);
    h.registryCount = (uint32_t)registryCount;
    h.protectionDuration = PlowProtectionDuration(); h.saltDuration = PlowSaltDuration();
    size_t bytes = h.roads * sizeof(WrDiskRoad) + h.visuals * sizeof(WrDiskVisual);
    h.payloadHash = WrHash(payload, bytes);
    if (!WrHeaderValid(h) || !WrNativeHashes(folder, &h))
    { HeapFree(GetProcessHeap(), 0, payload); Warn("protection persistence save: native save files unavailable; no sidecar replacement"); return; }
    h.headerHash = WrHeaderChecksum(h);
    HANDLE file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    bool ok = file != INVALID_HANDLE_VALUE;
    if (ok)
    {
        ok = WrWrite(file, &h, sizeof(h)) && WrWrite(file, payload, (DWORD)bytes) && FlushFileBuffers(file);
        CloseHandle(file);
    }
    if (ok) ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    DWORD error = ok ? 0 : GetLastError();
    if (!ok) DeleteFileA(temporary);
    HeapFree(GetProcessHeap(), 0, payload);
    if (ok) Info("protection persistence save: folder=%s roads=%u visual_texels=%u saved_game_minute_milli=%lld bytes=%llu atomic_replace=1 native_save_unchanged=1 stage=after-final-native-close",
        folder, h.roads, h.visuals, (long long)h.savedTime, (unsigned long long)(sizeof(h) + bytes));
    else Warn("protection persistence save: atomic sidecar write failed error=%lu native_save_unchanged=1", error);
}

static int WrSaveCloseHook(void* file, const char* folder)
{
    // Forward the native close exactly once and preserve its result. No timer,
    // background polling or second native save is involved in finalisation.
    int result = g_wrOriginalClose ? g_wrOriginalClose(file) : EOF;
    if (!InterlockedCompareExchange(&g_wrPersistenceActive, 0, 0)) return result;
    if (result != 0)
    { Warn("protection persistence save: final native close failed; sidecar not overwritten"); return result; }
    __try { WrSave(folder); }
    __except(FaultFilter("weather protection sidecar save", GetExceptionInformation()))
    { Warn("protection persistence save: guarded fault; native save unaffected"); }
    return result;
}

static void* WrTextureHook(void* self, const char* path)
{
    if (InterlockedCompareExchange(&g_wrPersistenceActive, 0, 0) && path)
    {
        size_t length = strlen(path);
        const size_t tail = sizeof("/resourcemap.dds") - 1;
        if (length > tail && (_stricmp(path + length - tail, "/resourcemap.dds") == 0 ||
            _stricmp(path + length - tail, "\\resourcemap.dds") == 0))
        {
            char folder[MAX_PATH * 2] = {};
            size_t keep = length - tail;
            if (keep < sizeof(folder))
            {
                memcpy(folder, path, keep);
                __try { WrLoad(folder); }
                __except(FaultFilter("weather protection sidecar load", GetExceptionInformation()))
                { Warn("protection persistence load: guarded fault; native save unaffected"); }
            }
        }
    }
    return g_wrOriginalTexture ? g_wrOriginalTexture(self, path) : NULL;
}

static bool WeatherPersistenceInstall()
{
    // These untouched snippets prove registry, geometry stride, xyz and both
    // endpoint layouts before any persistence identity reads are enabled.
    static const BYTE registry[] = {0x49,0x81,0xC0,0xB0,0x02,0,0};
    static const BYTE points[] = {0x49,0x8B,0x50,0x08,0x4B,0x8D,0x04,0x5B,0xF2,0x0F,0x10,0x04,0xC2};
    static const BYTE endpoint[] = {0x49,0x8B,0x40,0x20,0xF2,0x0F,0x11,0x45,0xF0,0xF2,0x0F,0x10,0x40,0x04};
    if (memcmp(g_exeBase + 0x42AF6F, registry, sizeof(registry)) ||
        memcmp(g_exeBase + 0x56AEAD, points, sizeof(points)) ||
        memcmp(g_exeBase + 0x56AE64, endpoint, sizeof(endpoint)))
    { Warn("protection persistence unavailable: native road geometry signature mismatch"); return false; }
    // r15 is the current save folder (reloaded at 42D92A and unchanged until
    // 42EE55); r14 is the final FILE*. The ZIP branch is strictly after this.
    static const BYTE folder[] = {0x4C,0x8D,0xBF,0xF8,0x09,0,0};
    static const BYTE close[] = {0x49,0x8B,0xCE,0xFF,0x15,0xE0,0xF1,0x43,0x00,
        0x80,0xBF,0x36,0x4D,0x01,0x00,0x00};
    BYTE* site = g_exeBase + WR_SAVE_CLOSE_CALL;
    if (memcmp(g_exeBase + 0x42D92A, folder, sizeof(folder)) ||
        memcmp(site - 3, close, sizeof(close)))
    { Warn("protection persistence unavailable: final-save-close signature mismatch"); return false; }
    int relative = 0; memcpy(&relative, site + 2, 4);
    void** slot = (void**)(site + 6 + relative);
    if (!ReadablePtr(slot, sizeof(void*)) || !*slot || !ReadablePtr(*slot, 1)) return false;
    BYTE* cave = AllocNear(site, 32);
    if (!cave) return false;
    // Leaf bridge: keep rcx=FILE*, add rdx=folder, tail-jump to the wrapper.
    cave[0] = 0x49; cave[1] = 0x8B; cave[2] = 0xD7;
    cave[3] = 0xFF; cave[4] = 0x25; memset(cave + 5, 0, 4);
    void* target = (void*)&WrSaveCloseHook; memcpy(cave + 9, &target, 8);
    FlushInstructionCache(GetCurrentProcess(), cave, 17);
    INT64 delta = cave - (site + 5);
    if ((int)delta != delta) return false;
    DWORD protection = 0;
    if (!VirtualProtect(site, 6, PAGE_EXECUTE_READWRITE, &protection)) return false;
    g_wrOriginalClose = (WrCloseFn)*slot;
    site[0] = 0xE8;
    relative = (int)delta; memcpy(site + 1, &relative, 4);
    site[5] = 0x90;
    DWORD ignored = 0; VirtualProtect(site, 6, protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, 6);
    void* original = NULL;
    if (!PatchIat(g_exe, "C3DDLL64.dll", WR_TEXTURE_SYMBOL, (void*)&WrTextureHook,
        &original, "weather protection save-folder load"))
    { Warn("protection persistence unavailable: load hook refused; final-close wrapper remains pass-through"); return false; }
    g_wrOriginalTexture = (WrTextureFn)original;
    InterlockedExchange(&g_wrPersistenceActive, 1);
    Info("protection persistence active: file=%s format=%u road_identity=verified-geometry-and-byte-index visual_identity=terrain-and-texel native_save_fingerprints=header-road-mask save_stage=final-native-close save_close_rva=0x%X early_save_call_untouched=1 lifetime_refresh=0", WR_SIDE_FILE, WR_PERSIST_VERSION, WR_SAVE_CLOSE_CALL);
    return true;
}
