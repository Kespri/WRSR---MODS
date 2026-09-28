#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Pointer-free, little-endian sidecar. Times use the existing game-minute *
// 1000 clock, never wall time. Native save fingerprints bind it to one save.
static const uint32_t WR_PERSIST_VERSION = 1;
static const uint32_t WR_PERSIST_MAX_ROADS = 8192;
static const uint32_t WR_PERSIST_MAX_VISUALS = 1024 * 1024;
static const int64_t WR_PERSIST_MAX_TIME = 60000000000000LL;

#pragma pack(push, 1)
struct WrDiskHeader
{
    char magic[8];
    uint32_t version, headerBytes, roadBytes, visualBytes;
    uint32_t roads, visuals, width, height, registryCount, reserved;
    int64_t savedTime, protectionDuration, saltDuration;
    uint64_t nativeHeaderHash, nativeRoadHash, nativeMaskHash, payloadHash, headerHash;
    float terrain[4]; // origin X/Z, size X/Z
};
struct WrDiskRoad
{
    uint64_t geometryHash;
    uint32_t length, index, geometryPoints, type;
    int32_t strength, material;
    uint32_t flags;
    int64_t clearedAt, remainder;
};
struct WrDiskVisual
{
    uint32_t index;
    int32_t strength;
    int64_t clearedAt;
    uint8_t green, clearGreen, snow, reserved;
};
#pragma pack(pop)

static_assert(sizeof(WrDiskHeader) == 128, "fixed persistence header");
static_assert(sizeof(WrDiskRoad) == 52, "fixed road record");
static_assert(sizeof(WrDiskVisual) == 20, "fixed mask record");

static uint64_t WrHash(const void* bytes, size_t size,
    uint64_t hash = 14695981039346656037ULL)
{
    const uint8_t* data = (const uint8_t*)bytes;
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ data[i]) * 1099511628211ULL;
    return hash;
}

static bool WrHeaderValid(const WrDiskHeader& h)
{
    return memcmp(h.magic, "WRPROT01", 8) == 0 &&
        h.version == WR_PERSIST_VERSION && h.headerBytes == sizeof(h) &&
        h.roadBytes == sizeof(WrDiskRoad) &&
        h.visualBytes == sizeof(WrDiskVisual) &&
        h.roads <= WR_PERSIST_MAX_ROADS &&
        h.visuals <= WR_PERSIST_MAX_VISUALS &&
        h.registryCount <= 1000000 && h.reserved == 0 &&
        h.savedTime > 0 && h.savedTime <= WR_PERSIST_MAX_TIME &&
        h.protectionDuration >= 0 && h.protectionDuration <= 1440000 &&
        h.saltDuration >= 0 && h.saltDuration <= 10080000 &&
        ((!h.visuals && !h.width && !h.height) ||
         (h.width > 0 && h.height > 0 && h.width <= 4096 && h.height <= 4096 &&
          h.visuals <= (uint64_t)h.width * h.height));
}

static uint64_t WrHeaderChecksum(const WrDiskHeader& header)
{
    WrDiskHeader copy = header;
    copy.headerHash = 0;
    return WrHash(&copy, sizeof(copy));
}

static bool WrRoadValid(const WrDiskRoad& r, const WrDiskHeader& h)
{
    return r.geometryHash != 0 && r.length > 0 && r.length <= 16384 &&
        r.index < r.length && r.geometryPoints <= 65536 && r.type <= 4096 &&
        r.strength > 0 && r.strength <= 1000000 &&
        (r.flags & ~3u) == 0 && (r.flags & 1u) == 0 &&
        r.clearedAt > 0 && r.clearedAt <= h.savedTime &&
        r.remainder >= 0 && r.remainder < 1000000;
}

static bool WrVisualValid(const WrDiskVisual& v, const WrDiskHeader& h)
{
    return v.index < (uint64_t)h.width * h.height &&
        v.strength >= 0 && v.strength <= 1000000 &&
        v.clearedAt > 0 && v.clearedAt <= h.savedTime && v.reserved == 0;
}

// Preserve absolute expiry on ordinary reload. If the INI changed, neither
// phase may gain remaining lifetime: longer settings cannot refresh old salt.
static int64_t WrRestoreClearTime(int64_t clearedAt, const WrDiskHeader& h,
    int64_t currentProtection, int64_t currentSalt)
{
    if (clearedAt <= 0 || clearedAt > h.savedTime) return 0;
    int64_t age = h.savedTime - clearedAt;
    int64_t remainingStrong = h.protectionDuration - age;
    int64_t remainingTotal = h.protectionDuration + h.saltDuration - age;
    if (remainingStrong < 0) remainingStrong = 0;
    if (remainingTotal < 0) remainingTotal = 0;
    int64_t minimumStrongAge = currentProtection - remainingStrong;
    int64_t minimumTotalAge = currentProtection + currentSalt - remainingTotal;
    if (age < minimumStrongAge) age = minimumStrongAge;
    if (age < minimumTotalAge) age = minimumTotalAge;
    return age < h.savedTime ? h.savedTime - age : 0;
}
