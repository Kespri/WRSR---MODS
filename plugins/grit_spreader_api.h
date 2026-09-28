// grit_spreader_api.h - POD-only service shared by technical_service_storage
// and weather_roads. No allocation or CRT ownership crosses the DLL boundary.

#ifndef TESMIO_GRIT_SPREADER_API_H
#define TESMIO_GRIT_SPREADER_API_H

#include <stddef.h>

// TesmioLoader stores service names in a 32-byte field including the NUL.
// Keep this shared ID short so provide() and consume() compare the same name.
#define TSM_SERVICE_GRIT_SPREADER "tss.grit_spreader"
#define TSM_GRIT_SPREADER_VERSION 1u
#define TSM_GRIT_RESOURCE_NAME_CAPACITY 64

#ifdef __cplusplus
static_assert(sizeof(TSM_SERVICE_GRIT_SPREADER) <= 32,
              "TesmioLoader service names must fit in 31 characters plus NUL");
#endif

#define TSM_GRIT_TREATMENT_DRY       0x00000001u
#define TSM_GRIT_TREATMENT_DUPLICATE 0x00000002u

typedef struct TsmGritRoadTreatment
{
    unsigned structSize;
    unsigned sequence;
    unsigned flags;
    unsigned reserved;
    void* vehicle;
    void* road;
    void* selector;
    float protectionStrength;
    float consumedKg;
    int materialIndex;
    int resourceTextId;
    char resourceName[TSM_GRIT_RESOURCE_NAME_CAPACITY];
} TsmGritRoadTreatment;

typedef struct TsmGritSpreaderApi
{
    unsigned structSize;

    // This is intentionally a synchronous current-call query. The provider
    // publishes immediately before invoking the game's verified road-clear
    // entry and removes the value when that call returns. A consumer therefore
    // cannot accidentally reuse stale vehicle material on a later road.
    int (*currentRoadTreatment)(
        void* vehicle,
        void* road,
        void* selector,
        TsmGritRoadTreatment* out,
        unsigned outSize);
} TsmGritSpreaderApi;

#endif // TESMIO_GRIT_SPREADER_API_H
