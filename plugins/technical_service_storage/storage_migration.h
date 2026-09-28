// v0.1.75: add-only reconciliation at the native building-load call boundary.
// No polling, live UI mutation, raw memcpy of owning Storage objects, or use
// of the plugin's C++ allocator for game-owned memory. See TECHNICAL_NOTES.md.
#pragma once

static const DWORD RVA_MIGRATION_BUILDING_LOAD = 0x001F0BE0;
static const DWORD RVA_MIGRATION_BUILDING_LOAD_CALL = 0x004331D7;
static const DWORD RVA_MIGRATION_STORAGE_PUSH = 0x000CC2F0;
static const DWORD RVA_MIGRATION_VECTOR_DESTROY = 0x000B18F0;
static const DWORD RVA_MIGRATION_STORAGE_CONTROLS = 0x001EBBF0;
static const size_t MIGRATION_TYPE_STORAGES = 0x3A0;

struct MigrationVector { BYTE* begin; BYTE* end; BYTE* capacity; };
static_assert(sizeof(MigrationVector) == 24, "native x64 vector layout");
typedef void (__fastcall *MigrationPushFn)(MigrationVector*, const BYTE*);
typedef void (__fastcall *MigrationDestroyFn)(MigrationVector*);
typedef void (__fastcall *MigrationControlsFn)(BYTE*);
typedef void (__fastcall *MigrationLoadFn)(BYTE*, void*);
static MigrationPushFn g_migrationPush = nullptr;
static MigrationDestroyFn g_migrationDestroy = nullptr;
static MigrationControlsFn g_migrationControls = nullptr;
static MigrationLoadFn g_migrationLoad = nullptr;
static volatile LONG g_migrationFaulted = 0;

static void MigrationWarn(BYTE* building, int index, const char* resource,
                          const char* reason)
{
    Report("WARN", "Storage migration", "rejected",
        "building=%p definition_index=%d resource=%s reason=%s action=retain-existing-storage",
        building, index, resource ? resource : "<unknown>", reason);
}

static bool MigrationReadVector(const void* address, size_t stride,
    size_t maximum, MigrationVector* out, size_t* count)
{
    *out = {};
    *count = 0;
    if (!ReadablePtr(address, sizeof(*out))) return false;
    memcpy(out, address, sizeof(*out));
    uintptr_t b = (uintptr_t)out->begin, e = (uintptr_t)out->end;
    uintptr_t c = (uintptr_t)out->capacity;
    if (!b) return !e && !c;
    if (b % alignof(void*) || e < b || c < e ||
        (e - b) % stride || (c - b) % stride ||
        (e - b) / stride > maximum || (c - b) / stride > maximum * 4)
        return false;
    *count = (e - b) / stride;
    return e == b || ReadablePtr(out->begin, e - b);
}

static bool MigrationResourceName(BYTE* resource, char name[64])
{
    name[0] = 0;
    MigrationVector resources = {};
    size_t count = 0;
    if (!MigrationReadVector(g_exeBase + RVA_RESOURCE_VECTOR,
            RESOURCE_RECORD_SIZE, MAX_RESOURCE_RECORDS, &resources, &count) ||
        count < MIN_RESOURCE_RECORDS)
        return false;
    uintptr_t r = (uintptr_t)resource, b = (uintptr_t)resources.begin;
    return r >= b && r < (uintptr_t)resources.end &&
        (r - b) % RESOURCE_RECORD_SIZE == 0 &&
        SafeReadStr(resource + RESOURCE_NAME, name, 64);
}

// Do not consult the previous world's runtimeValidationState during loading.
static bool MigrationConfigured(const char* name)
{
    if (IsReservedNonSpreaderResource(name)) return false;
    for (size_t i = 0; i < g_spreaderMaterialCount; ++i)
        if (_stricmp(name, g_spreaderMaterials[i].resourceName) == 0) return true;
    return false;
}

// All owning members used by the verified native copy constructor/destructor.
static const size_t MIGRATION_VECTOR_OFFSETS[] = { 0, 0x18, 0x30, 0x50, 0x68, 0xC8 };
static const size_t MIGRATION_VECTOR_STRIDES[] = { 16, 16, 4, 12, 12, 188 };

static bool MigrationStorageValid(BYTE* storage, const char** reason = nullptr)
{
    const char* ignored = nullptr;
    if (!reason) reason = &ignored;
    *reason = "unreadable-storage-object";
    if (!ReadablePtr(storage, STORAGE_SIZE)) return false;
    static const char* vectorErrors[] = {
        "invalid-resource-slot-vector", "invalid-per-resource-control-vector",
        "invalid-selection-bitset-vector", "invalid-owner-vector-0x50",
        "invalid-owner-vector-0x68", "invalid-owner-vector-0xC8"
    };
    for (size_t i = 0; i < _countof(MIGRATION_VECTOR_OFFSETS); ++i)
    {
        MigrationVector view = {};
        size_t count = 0;
        *reason = vectorErrors[i];
        if (!MigrationReadVector(storage + MIGRATION_VECTOR_OFFSETS[i],
                MIGRATION_VECTOR_STRIDES[i], 4096, &view, &count)) return false;
        if (i == 0)
        {
            *reason = "resource-slot-count-exceeds-64";
            if (count > MAX_SLOTS_PER_STORAGE) return false;
            for (size_t j = 0; j < count; ++j)
            {
                char name[64] = {};
                *reason = "slot-resource-outside-live-resource-table";
                if (!MigrationResourceName(*(BYTE**)(view.begin + j * SLOT_SIZE), name))
                    return false;
                float amount = *(float*)(view.begin + j * SLOT_SIZE + 8);
                *reason = "non-finite-resource-stock";
                if (!_finite(amount)) return false;
            }
        }
    }
    // +0x30 is vector<bool>'s backing words, +0x48 its bit count.
    MigrationVector bits = {};
    size_t words = 0;
    *reason = "selection-bit-count-exceeds-backing-words";
    if (!MigrationReadVector(storage + 0x30, 4, 4096, &bits, &words) ||
        *(size_t*)(storage + 0x48) > words * 32) return false;
    // +0xC0 owns a further nested object, used by specialized storage modes.
    // Those are outside the single-resource grit/fuel contract, not guessed.
    *reason = "unsupported-specialized-storage-owner-0xC0";
    return *(BYTE**)(storage + 0xC0) == nullptr;
}

struct MigrationCandidate
{
    BYTE* definition;
    int index;
    char name[64];
};
struct MigrationPlan
{
    MigrationVector original;
    size_t originalCount;
    MigrationCandidate add[64];
    size_t count;
};

static bool MigrationMakePlan(BYTE* building, MigrationPlan* plan)
{
    *plan = {};
    if (ReadTechnicalType(building) != BUILDING_GARBAGE_OFFICE) return false;
    if (!ReadablePtr(building + SAND_BUILDING_CONSTRUCTION_PROGRESS, sizeof(float)) ||
        !ReadablePtr(building + SAND_BUILDING_GOING_AWAY, 1))
    {
        MigrationWarn(building, -1, nullptr, "unreadable-building-lifecycle");
        return false;
    }
    if (SandLifecycleConstructionClass(
            *(float*)(building + SAND_BUILDING_CONSTRUCTION_PROGRESS)) != 2 ||
        *(BYTE*)(building + SAND_BUILDING_GOING_AWAY))
    {
        Report("INFO", "Storage migration", "deferred",
            "building=%p reason=construction-or-demolition-or-collapse no-lifetime-or-priority-change=1 retry=next-savegame-load",
            building);
        return false;
    }
    MigrationVector definitions = {};
    size_t definitionCount = 0;
    BYTE* type = *(BYTE**)(building + B_TYPEDESC);
    if (!MigrationReadVector(type + MIGRATION_TYPE_STORAGES, STORAGE_SIZE,
            MAX_STORAGES, &definitions, &definitionCount) ||
        !MigrationReadVector(building + B_STORAGE_BEGIN, STORAGE_SIZE,
            MAX_STORAGES, &plan->original, &plan->originalCount))
    {
        MigrationWarn(building, -1, nullptr, "invalid-native-storage-vector");
        return false;
    }
    // Preflight every existing owner BEFORE a native allocation/copy occurs.
    for (size_t j = 0; j < plan->originalCount; ++j)
    {
        const char* detail = nullptr;
        if (!MigrationStorageValid(plan->original.begin + j * STORAGE_SIZE, &detail))
        {
            char reason[192];
            sprintf_s(reason, "existing-storage-index=%llu:%s", (unsigned long long)j, detail);
            MigrationWarn(building, -1, nullptr, reason);
            return false;
        }
    }
    MigrationCandidate candidates[64] = {};
    size_t candidateCount = 0;
    for (size_t i = 0; i < definitionCount; ++i)
    {
        BYTE* definition = definitions.begin + i * STORAGE_SIZE;
        MigrationVector slots = {};
        size_t slotCount = 0;
        if (!MigrationReadVector(definition, SLOT_SIZE, MAX_SLOTS_PER_STORAGE,
                &slots, &slotCount))
        {
            MigrationWarn(building, (int)i, nullptr, "invalid-definition-slots");
            continue;
        }
        if (!slotCount) continue;
        char name[64] = {};
        if (!MigrationResourceName(*(BYTE**)slots.begin, name))
        {
            MigrationWarn(building, (int)i, nullptr, "definition-resource-outside-live-resource-table");
            continue;
        }
        if (!MigrationConfigured(name)) continue;
        int transport = *(int*)(definition + STORAGE_CLASS);
        float capacity = *(float*)(definition + STORAGE_CAPACITY);
        // Parser 0x116ECE..0x116F13 marks IMPORT_SPECIAL +0x84=1;
        // EXPORT uses +0x85. Normal imports retain purpose +0x88=-1.
        if (slotCount != 1 || definition[0x84] != 1 || definition[0x85] != 0 ||
            *(int*)(definition + 0x88) != -1 ||
            (transport != RESOURCE_TRANSPORT_COVERED &&
             transport != RESOURCE_TRANSPORT_OPEN && transport != RESOURCE_TRANSPORT_GRAVEL) ||
            !_finite(capacity) || capacity <= 0)
        {
            char reason[224];
            sprintf_s(reason, "requires-single-resource-import:slots=%llu import=%u export=%u purpose=%d class=%d capacity=%.6g",
                (unsigned long long)slotCount, definition[0x84], definition[0x85],
                *(int*)(definition + 0x88), transport, capacity);
            MigrationWarn(building, (int)i, name, reason);
            continue;
        }
        const char* detail = nullptr;
        if (!MigrationStorageValid(definition, &detail))
        {
            MigrationWarn(building, (int)i, name, detail);
            continue;
        }
        MigrationCandidate& candidate = candidates[candidateCount++];
        candidate.definition = definition;
        candidate.index = (int)i;
        strcpy_s(candidate.name, name);
    }
    for (size_t i = 0; i < candidateCount; ++i)
    {
        const MigrationCandidate& candidate = candidates[i];
        bool duplicate = false;
        for (size_t j = 0; j < candidateCount; ++j)
            if (i != j && _stricmp(candidate.name, candidates[j].name) == 0)
                duplicate = true;
        if (duplicate)
        {
            MigrationWarn(building, candidate.index, candidate.name,
                "ambiguous-duplicate-resource-in-building-definition");
            continue;
        }
        bool present = false;
        for (size_t j = 0; j < plan->originalCount; ++j)
        {
            BYTE* existing = plan->original.begin + j * STORAGE_SIZE;
            BYTE* slots = *(BYTE**)existing;
            size_t n = ((uintptr_t)*(BYTE**)(existing + 8) - (uintptr_t)slots) / SLOT_SIZE;
            for (size_t k = 0; k < n; ++k)
            {
                char name[64] = {};
                MigrationResourceName(*(BYTE**)(slots + k * SLOT_SIZE), name);
                if (_stricmp(name, candidate.name) != 0) continue;
                present = true;
                if (n != 1 || *(int*)(existing + STORAGE_CLASS) !=
                        *(int*)(candidate.definition + STORAGE_CLASS) ||
                    *(float*)(existing + STORAGE_CAPACITY) !=
                        *(float*)(candidate.definition + STORAGE_CAPACITY) ||
                    existing[0x84] != 1 || existing[0x85] != 0)
                    MigrationWarn(building, candidate.index, name,
                        "already-present-with-different-layout-or-capacity:no-conversion-or-resize");
            }
        }
        if (!present) plan->add[plan->count++] = candidate;
    }
    if (plan->originalCount + plan->count > MAX_STORAGES)
    {
        MigrationWarn(building, -1, nullptr, "combined-storage-count-exceeds-64:no-partial-migration");
        plan->count = 0;
        return false;
    }
    return plan->count != 0;
}

static bool MigrationCopyMatches(BYTE* source, BYTE* copy)
{
    // Native copy invalidates its +0xA4 cache. Preserve that value too for
    // existing objects after the final push (which can itself reallocate).
    memcpy(copy + 0xA4, source + 0xA4, 4);
    // +0x86/+0x87 are padding, not initialized by the native copy constructor.
    if (memcmp(source + 0x80, copy + 0x80, 6) ||
        memcmp(source + 0x88, copy + 0x88, 0x38) ||
        *(size_t*)(source + 0x48) != *(size_t*)(copy + 0x48)) return false;
    for (size_t i = 0; i < _countof(MIGRATION_VECTOR_OFFSETS); ++i)
    {
        size_t offset = MIGRATION_VECTOR_OFFSETS[i];
        MigrationVector a = {}, b = {};
        size_t na = 0, nb = 0;
        if (!MigrationReadVector(source + offset, MIGRATION_VECTOR_STRIDES[i], 4096, &a, &na) ||
            !MigrationReadVector(copy + offset, MIGRATION_VECTOR_STRIDES[i], 4096, &b, &nb) ||
            na != nb || (a.begin && a.begin == b.begin)) return false;
        if (na && memcmp(a.begin, b.begin, na * MIGRATION_VECTOR_STRIDES[i])) return false;
    }
    BYTE* a = *(BYTE**)(source + 0xC0), *b = *(BYTE**)(copy + 0xC0);
    return (!a && !b) || (a && b && a != b && ReadablePtr(b, 0x90) && !memcmp(a, b, 0x90));
}

static bool MigrationPrepareEmpty(BYTE* storage, BYTE* definition)
{
    // Type slots hold capacity, not stock. Never grant that amount as cargo.
    if (!MigrationCopyMatches(definition, storage)) return false;
    BYTE* slot = *(BYTE**)storage;
    memset(slot + 8, 0, 8); // actual stock and transient per-slot quantity
    g_migrationControls(storage); // native per-resource controls + bit mask
    MigrationVector controls = {};
    size_t count = 0;
    if (!MigrationReadVector(storage + 0x18, SLOT_SIZE, MAX_SLOTS_PER_STORAGE,
            &controls, &count) || count != 1) return false;
    if (*(BYTE**)controls.begin != *(BYTE**)slot) return false;
    memset(controls.begin + 8, 0, 8);
    MigrationVector bits = {};
    if (!MigrationReadVector(storage + 0x30, 4, 4096, &bits, &count)) return false;
    if (count) memset(bits.begin, 0, count * 4);
    *(float*)(storage + 0xA4) = -1.0f; // native cache invalidation
    return MigrationStorageValid(storage);
}

static bool MigrationBuildReplacement(const MigrationPlan& plan, MigrationVector* staged)
{
    // Exceptions leave the original untouched; the caller disposes staged.
    for (size_t i = 0; i < plan.originalCount; ++i)
        g_migrationPush(staged, plan.original.begin + i * STORAGE_SIZE);
    for (size_t i = 0; i < plan.count; ++i)
    {
        g_migrationPush(staged, plan.add[i].definition);
        if (!MigrationPrepareEmpty(staged->end - STORAGE_SIZE, plan.add[i].definition))
            return false;
    }
    MigrationVector checked = {};
    size_t count = 0;
    if (!MigrationReadVector(staged, STORAGE_SIZE, MAX_STORAGES, &checked, &count) ||
        count != plan.originalCount + plan.count) return false;
    for (size_t i = 0; i < plan.originalCount; ++i)
        if (!MigrationCopyMatches(plan.original.begin + i * STORAGE_SIZE,
                staged->begin + i * STORAGE_SIZE)) return false;
    for (size_t i = 0; i < plan.count; ++i)
    {
        BYTE* added = staged->begin + (plan.originalCount + i) * STORAGE_SIZE;
        if (*(float*)(*(BYTE**)added + 8) != 0.0f) return false;
    }
    return true;
}

// Keep C++ exception handling separate from the outer SEH function (MSVC).
static bool MigrationTryBuild(const MigrationPlan& plan, MigrationVector* staged)
{
    try { return MigrationBuildReplacement(plan, staged); }
    catch (...) { return false; }
}

static void MigrationAfterLoad(BYTE* building)
{
    MigrationPlan plan = {};
    MigrationVector staged = {};
    bool committed = false;
    __try
    {
        if (!MigrationMakePlan(building, &plan)) return;
        if (!MigrationTryBuild(plan, &staged))
        {
            MigrationWarn(building, -1, nullptr, "native-copy-or-empty-initialization-failed:transaction-not-committed");
            g_migrationDestroy(&staged);
            return;
        }
        // Swap only vector ownership; individual Storage owners were cloned
        // by the game. The observer is excluded for the whole native load.
        memcpy(building + B_STORAGE_BEGIN, &staged, sizeof(staged));
        staged = plan.original;
        committed = true;
        g_migrationDestroy(&staged);
        for (size_t i = 0; i < plan.count; ++i)
            Report("INFO", "Storage migration", "added",
                "building=%p definition_index=%d storage_index=%llu resource=%s capacity=%.3f stock=0 old_stock_and_fuel=preserved priority=existing-policy persistence=native-save",
                building, plan.add[i].index,
                (unsigned long long)(plan.originalCount + i), plan.add[i].name,
                *(float*)(plan.add[i].definition + STORAGE_CAPACITY));
    }
    __except(FaultFilter("storage migration", GetExceptionInformation()))
    {
        // Do not traverse partially faulting ownership again. Disable only
        // migration, retaining the native load and the rest of the plugin.
        InterlockedExchange(&g_migrationFaulted, 1);
        Report("WARN", "Storage migration", "fault",
            "building=%p committed=%d migration-disabled-for-session=1 reason=native-memory-fault further-cleanup-skipped=1",
            building, committed ? 1 : 0);
    }
}

static void __fastcall MigrationHookBuildingLoad(BYTE* building, void* stream)
{
    AcquireSRWLockExclusive(&g_storageLoadLock);
    __try
    {
        // The original load is never swallowed, retried or replaced.
        g_migrationLoad(building, stream);
        if (g_runtimeActive && !InterlockedCompareExchange(&g_migrationFaulted, 0, 0))
            MigrationAfterLoad(building);
    }
    __finally { ReleaseSRWLockExclusive(&g_storageLoadLock); }
}

static bool MigrationVerify(DWORD rva, const BYTE* expected, size_t count, const char* label)
{
    if (rva <= g_exeSize && count <= g_exeSize - rva &&
        ReadablePtr(g_exeBase + rva, count) &&
        !memcmp(g_exeBase + rva, expected, count)) return true;
    Report("WARN", "Storage migration", "signature-rejected",
        "helper=%s rva=0x%X reason=unreadable-or-changed-native-code migration-disabled=1", label, rva);
    return false;
}

static bool InstallStorageMigrationHook()
{
    if (!g_storageMigrationEnabled)
    {
        Info("Storage migration disabled by [storage_migration] enabled=0");
        return false;
    }
    static const BYTE load[] = { 0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xAC,0x24,0x50,0xAB,0xFF,0xFF };
    static const BYTE push[] = { 0x40,0x57,0x48,0x83,0xEC,0x30,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF };
    static const BYTE copy[] = { 0x48,0x8B,0xC4,0x48,0x89,0x48,0x08,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57 };
    static const BYTE controls[] = { 0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0x51,0x08 };
    static const BYTE destroy[] = { 0x48,0x89,0x5C,0x24,0x10,0x56,0x48,0x83,0xEC,0x20,0x48,0x8B,0x19,0x48,0x8B,0xF1 };
    static const BYTE call[] = { 0x48,0x8B,0xD6,0x4A,0x8B,0x0C,0xC1,0xE8,0x04,0xDA,0xDB,0xFF };
    if (!MigrationVerify(RVA_MIGRATION_BUILDING_LOAD_CALL - 7, call, sizeof(call), "load call + arguments") ||
        !MigrationVerify(RVA_MIGRATION_BUILDING_LOAD, load, sizeof(load), "loader") ||
        !MigrationVerify(RVA_MIGRATION_STORAGE_PUSH, push, sizeof(push), "push") ||
        !MigrationVerify(0x10C500, copy, sizeof(copy), "deep copy") ||
        !MigrationVerify(RVA_MIGRATION_VECTOR_DESTROY, destroy, sizeof(destroy), "vector destructor") ||
        !MigrationVerify(RVA_MIGRATION_STORAGE_CONTROLS, controls, sizeof(controls), "controls"))
    {
        MigrationWarn(nullptr, -1, nullptr, "unsupported-native-signature-or-conflicting-load-hook:migration-disabled");
        return false;
    }
    BYTE* site = g_exeBase + RVA_MIGRATION_BUILDING_LOAD_CALL;
    BYTE* cave = AllocNear(site, 16);
    if (!cave)
    {
        MigrationWarn(nullptr, -1, nullptr, "cannot-allocate-load-bridge");
        return false;
    }
    cave[0] = 0xFF; cave[1] = 0x25;
    memset(cave + 2, 0, 4);
    void* hook = (void*)&MigrationHookBuildingLoad;
    memcpy(cave + 6, &hook, sizeof(hook));
    INT64 distance = (INT64)(uintptr_t)cave - (INT64)(uintptr_t)(site + 5);
    int relative = (int)distance;
    DWORD oldProtection = 0;
    if (distance != relative || !FlushCodeChecked(cave,14,"storage-migration-stub") ||
        !MakeCodeWritable(site,5,&oldProtection,"storage-migration-patch"))
    {
        MigrationWarn(nullptr, -1, nullptr, "load-bridge-out-of-range-or-code-protection-refused");
        VirtualFree(cave, 0, MEM_RELEASE);
        return false;
    }
    g_migrationLoad = (MigrationLoadFn)(g_exeBase + RVA_MIGRATION_BUILDING_LOAD);
    g_migrationPush = (MigrationPushFn)(g_exeBase + RVA_MIGRATION_STORAGE_PUSH);
    g_migrationDestroy = (MigrationDestroyFn)(g_exeBase + RVA_MIGRATION_VECTOR_DESTROY);
    g_migrationControls = (MigrationControlsFn)(g_exeBase + RVA_MIGRATION_STORAGE_CONTROLS);
    memcpy(site + 1, &relative, 4);
    FinishCodePatch(site,5,oldProtection,"storage-migration-patch");
    Report("INFO", "Storage migration", "load-hook",
        "active=1 call=exe+0x%X after-native-load=1 type=49 add-only=1 empty-new-stock=1 worker-exclusion=1 periodic-mutations=0",
        RVA_MIGRATION_BUILDING_LOAD_CALL);
    return true;
}
