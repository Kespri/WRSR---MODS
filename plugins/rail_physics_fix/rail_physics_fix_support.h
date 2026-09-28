// RailPhysics reliability helpers. GPL-3.0. No physics/configuration changes.
#pragma once
#include <stdint.h>

static HANDLE g_rpLog = INVALID_HANDLE_VALUE;
static SRWLOCK g_rpLogLock = SRWLOCK_INIT;
static bool g_patchMaintenanceFailed;

static void RpMessageV(const char* level, const char* area, const char* rule,
                       const char* format, va_list args)
{
    char message[1800], line[2048];
    _vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    const char* body = message;
    if (!strncmp(body, "railphysics ", 12)) body += 12;
    else if (!strncmp(body, "rail_physics_fix ", 17)) body += 17;
    while (*body == ' ') ++body;
    SYSTEMTIME now; GetLocalTime(&now);
    int n = _snprintf_s(line, sizeof(line), _TRUNCATE,
        "%04u-%02u-%02u %02u:%02u:%02u.%03u [%s] [%s] [%s] %s\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, level, area, rule, body);
    if (n < 0) n = (int)strlen(line);
    AcquireSRWLockExclusive(&g_rpLogLock);
    if (H && H->log) H->log("rail_physics_fix  [%s] [%s] [%s] %s", level, area, rule, body);
    if (g_rpLog != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        const BOOL ok = WriteFile(g_rpLog, line, (DWORD)n, &written, NULL);
        if (!ok || written != (DWORD)n)
        {
            const DWORD error = ok ? ERROR_WRITE_FAULT : GetLastError();
            CloseHandle(g_rpLog); g_rpLog = INVALID_HANDLE_VALUE;
            if (H && H->log) H->log("rail_physics_fix [ERROR] [log] [RP901] log write failed (%lu); loader log remains available", error);
        }
    }
    ReleaseSRWLockExclusive(&g_rpLogLock);
}
static void RpMessage(const char* level, const char* area, const char* rule, const char* format, ...)
{
    va_list args; va_start(args, format); RpMessageV(level, area, rule, format, args); va_end(args);
}
static void RpInfo(const char* format, ...)
{
    va_list args; va_start(args, format); RpMessageV("INFO", "general", "RP000", format, args); va_end(args);
}
static void RpOpenLog()
{
    g_rpLog = TsmOpenLog("tesmioloader.rail_physics_fix.log");
    if (g_rpLog == INVALID_HANDLE_VALUE)
        RpMessage("WARN", "log", "RP900", "cannot open plugin log (%lu); using loader log", GetLastError());
}

static int RpStartRejected()
{
    AcquireSRWLockExclusive(&g_rpLogLock);
    if (g_rpLog != INVALID_HANDLE_VALUE) CloseHandle(g_rpLog);
    g_rpLog = INVALID_HANDLE_VALUE;
    ReleaseSRWLockExclusive(&g_rpLogLock);
    return 1;
}

// Runtime warnings: first occurrence immediately, then at most once per 30 s
// for each rule. They are never disabled by the diagnostic INI switches.
enum RpWarning { RP_VEHICLE, RP_CONSIST, RP_NUMERIC, RP_SCAN, RP_SCAN_ALLOC, RP_ROUTE, RP_WARNING_COUNT };
struct RpWarningState { DWORD last; unsigned suppressed; bool seen; };
static RpWarningState g_rpWarnings[RP_WARNING_COUNT];
static SRWLOCK g_rpWarningLock = SRWLOCK_INIT;
static void RpWarn(RpWarning rule, const char* context)
{
    DWORD now = GetTickCount();
    AcquireSRWLockExclusive(&g_rpWarningLock);
    RpWarningState& s = g_rpWarnings[rule];
    bool emit = !s.seen || (DWORD)(now-s.last) >= 30000;
    unsigned suppressed = s.suppressed;
    if (emit) { s.seen = true; s.last = now; s.suppressed = 0; }
    else if (s.suppressed != UINT_MAX) ++s.suppressed;
    ReleaseSRWLockExclusive(&g_rpWarningLock);
    if (emit)
    {
        char id[16]; sprintf_s(id, "RP%03u", 100u+(unsigned)rule);
        RpMessage("WARN", "runtime", id, "%s; suppressed repeats=%u", context, suppressed);
    }
}

// Fault injection exists only in the offline test executable, never in the DLL.
#ifdef RAIL_PHYSICS_TESTING
static int g_testAllocFailAt = -1, g_testAllocCalls, g_testLiveAllocations;
static const char* g_testProtectFailAction;
static const char* g_testFlushFailAction;
static bool RpAllocationFails() { return ++g_testAllocCalls == g_testAllocFailAt; }
#else
static bool RpAllocationFails() { return false; }
#endif
static void* RpMalloc(size_t size)
{
    void* p = RpAllocationFails() ? NULL : malloc(size);
    if (!p) RpWarn(RP_SCAN_ALLOC, "malloc failed; allocation not published");
#ifdef RAIL_PHYSICS_TESTING
    if (p) ++g_testLiveAllocations;
#endif
    return p;
}
static void* RpCalloc(size_t count, size_t size)
{
    if (!size || count > SIZE_MAX/size)
    { RpWarn(RP_SCAN_ALLOC, "calloc size overflow/zero"); return NULL; }
    void* p = RpAllocationFails() ? NULL : calloc(count, size);
    if (!p) RpWarn(RP_SCAN_ALLOC, "calloc failed; allocation not published");
#ifdef RAIL_PHYSICS_TESTING
    if (p) ++g_testLiveAllocations;
#endif
    return p;
}
static void* RpRealloc(void* old, size_t size)
{
    if (!size) { RpWarn(RP_SCAN_ALLOC, "zero-size realloc refused"); return NULL; }
    void* p = RpAllocationFails() ? NULL : realloc(old, size);
    if (!p) RpWarn(RP_SCAN_ALLOC, "realloc failed; previous allocation retained");
#ifdef RAIL_PHYSICS_TESTING
    if (p && !old) ++g_testLiveAllocations;
#endif
    return p;
}
static void RpFree(void* p)
{
#ifdef RAIL_PHYSICS_TESTING
    if (p) --g_testLiveAllocations;
#endif
    free(p);
}
template<class T> static bool RpResize(T*& current, size_t count)
{
    if (!count || count > SIZE_MAX/sizeof(T)) return false;
    T* next = (T*)RpRealloc(current, count*sizeof(T));
    if (!next) return false;
    current = next;
    return true;
}

static bool RpProtect(void* address, size_t size, DWORD flags, DWORD* previous, const char* action)
{
    BOOL ok;
#ifdef RAIL_PHYSICS_TESTING
    if (g_testProtectFailAction && !strcmp(action, g_testProtectFailAction))
    { SetLastError(ERROR_ACCESS_DENIED); ok = FALSE; }
    else
#endif
        ok = VirtualProtect(address, size, flags, previous);
    if (!ok) RpMessage("ERROR", "patch", "RP301", "%s: VirtualProtect(%p, %zu) failed, Win32=%lu", action, address, size, GetLastError());
    return ok != FALSE;
}
static bool RpFlush(void* address, size_t size, const char* action)
{
    BOOL ok;
#ifdef RAIL_PHYSICS_TESTING
    if (g_testFlushFailAction && !strcmp(action, g_testFlushFailAction))
    { SetLastError(ERROR_INVALID_FUNCTION); ok = FALSE; }
    else
#endif
        ok = FlushInstructionCache(GetCurrentProcess(), address, size);
    if (!ok) RpMessage("ERROR", "patch", "RP302", "%s: FlushInstructionCache(%p, %zu) failed, Win32=%lu", action, address, size, GetLastError());
    return ok != FALSE;
}
