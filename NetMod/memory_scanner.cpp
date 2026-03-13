#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX
#include <windows.h>
#include <cstring>
#include <cstdio>
#include "memory_scanner.h"

extern void Log(const char* fmt, ...);

// ─── Safe memory read helpers ─────────────────────────────────────────────────
// All reads use SEH to catch access violations

static bool SafeReadDWORD(DWORD addr, DWORD* out) {
    __try {
        *out = *(DWORD*)addr;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeReadFloat(DWORD addr, float* out) {
    __try {
        *out = *(float*)addr;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeWriteFloat(DWORD addr, float val) {
    __try {
        *(float*)addr = val;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeWriteDWORD(DWORD addr, DWORD val) {
    __try {
        *(DWORD*)addr = val;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ─── Pointer chain resolver ───────────────────────────────────────────────────
// Mirrors C# MemoryManager.CalculatePointer:
//   ptr = *(DWORD*)baseAddr
//   for i in 0..n-2:  ptr = *(DWORD*)(ptr + offsets[i])
//   return ptr + offsets[n-1]   ← last offset just added, not dereffed
static DWORD FollowChain(DWORD baseAddr, const DWORD* offsets, int count) {
    DWORD ptr = 0;
    if (!SafeReadDWORD(baseAddr, &ptr) || ptr == 0) {
        Log("[MemScan] FollowChain: failed to read base 0x%X", baseAddr);
        return 0;
    }
    for (int i = 0; i < count - 1; i++) {
        DWORD next = 0;
        if (!SafeReadDWORD(ptr + offsets[i], &next) || next == 0) {
            Log("[MemScan] FollowChain: failed step %d (addr=0x%X + off=0x%X)", i, ptr, offsets[i]);
            return 0;
        }
        ptr = next;
    }
    return ptr + offsets[count - 1];
}

// ─── Public API ───────────────────────────────────────────────────────────────

bool MemScanner_Init() {
    Log("[MemScan] Init: DRIVER_BASEADDRESS=0x%X, INRACE_BASEADDRESS=0x%X",
        DRIVER_BASEADDRESS, INRACE_BASEADDRESS);

    // Quick sanity: check that DRIVER_BASEADDRESS contains a non-zero DWORD
    // (It's a global variable that's populated at runtime when a race starts)
    DWORD baseVal = 0;
    if (SafeReadDWORD(DRIVER_BASEADDRESS, &baseVal)) {
        Log("[MemScan] DRIVER_BASEADDRESS value = 0x%X (0 = game not in race yet)", baseVal);
    } else {
        Log("[MemScan] DRIVER_BASEADDRESS not readable - wrong binary version?");
    }
    return true;
}

bool MemScanner_IsInRace() {
    DWORD inRacePtr = 0;
    if (!SafeReadDWORD(INRACE_BASEADDRESS, &inRacePtr) || inRacePtr == 0)
        return false;
    // If we can read the driver count offset, we're in a race
    DWORD driverCount = 0;
    return SafeReadDWORD(inRacePtr + INRACE_DRIVERCOUNT_OFF, &driverCount) && driverCount > 0;
}

DWORD MemScanner_GetDriverBase(int driverIndex) {
    DWORD driverArrayAddr = FollowChain(DRIVER_BASEADDRESS, DRIVER_CHAIN_OFFSETS, DRIVER_CHAIN_LEN);
    if (driverArrayAddr == 0) {
        Log("[MemScan] GetDriverBase(%d): pointer chain failed", driverIndex);
        return 0;
    }
    DWORD driverBase = 0;
    if (!SafeReadDWORD(driverArrayAddr + driverIndex * 4, &driverBase)) {
        Log("[MemScan] GetDriverBase(%d): ReadDWORD(0x%X) failed", driverIndex,
            driverArrayAddr + driverIndex * 4);
        return 0;
    }
    return driverBase;
}

bool MemScanner_ReadState(int driverIndex, PlayerState* out) {
    if (!out) return false;
    DWORD base = MemScanner_GetDriverBase(driverIndex);
    if (base == 0) return false;

    __try {
        out->posX = *(float*)(base + DRIVER_OFF_POS_X);
        out->posY = *(float*)(base + DRIVER_OFF_POS_Y);
        out->posZ = *(float*)(base + DRIVER_OFF_POS_Z);
        out->spdX = *(float*)(base + DRIVER_OFF_SPD_X);
        out->spdY = *(float*)(base + DRIVER_OFF_SPD_Y);
        out->spdZ = *(float*)(base + DRIVER_OFF_SPD_Z);
        out->fwdX = *(float*)(base + DRIVER_OFF_FWD_X);
        out->fwdY = *(float*)(base + DRIVER_OFF_FWD_Y);
        out->fwdZ = *(float*)(base + DRIVER_OFF_FWD_Z);
        out->upX  = *(float*)(base + DRIVER_OFF_UP_X);
        out->upY  = *(float*)(base + DRIVER_OFF_UP_Y);
        out->upZ  = *(float*)(base + DRIVER_OFF_UP_Z);
        out->brick       = *(int32_t*)(base + DRIVER_OFF_BRICK);
        out->whiteBricks = *(int32_t*)(base + DRIVER_OFF_WHITE);
        out->flags = MemScanner_IsInRace() ? 0x01 : 0x00;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[MemScan] ReadState(%d) AV at base=0x%X", driverIndex, base);
        return false;
    }
}

bool MemScanner_WriteState(int driverIndex, const PlayerState* in) {
    if (!in) return false;
    DWORD base = MemScanner_GetDriverBase(driverIndex);
    if (base == 0) return false;

    __try {
        *(float*)(base + DRIVER_OFF_POS_X) = in->posX;
        *(float*)(base + DRIVER_OFF_POS_Y) = in->posY;
        *(float*)(base + DRIVER_OFF_POS_Z) = in->posZ;
        *(float*)(base + DRIVER_OFF_SPD_X) = in->spdX;
        *(float*)(base + DRIVER_OFF_SPD_Y) = in->spdY;
        *(float*)(base + DRIVER_OFF_SPD_Z) = in->spdZ;
        *(float*)(base + DRIVER_OFF_FWD_X) = in->fwdX;
        *(float*)(base + DRIVER_OFF_FWD_Y) = in->fwdY;
        *(float*)(base + DRIVER_OFF_FWD_Z) = in->fwdZ;
        *(float*)(base + DRIVER_OFF_UP_X)  = in->upX;
        *(float*)(base + DRIVER_OFF_UP_Y)  = in->upY;
        *(float*)(base + DRIVER_OFF_UP_Z)  = in->upZ;
        *(int32_t*)(base + DRIVER_OFF_BRICK)  = in->brick;
        *(int32_t*)(base + DRIVER_OFF_WHITE)  = in->whiteBricks;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[MemScan] WriteState(%d) AV at base=0x%X", driverIndex, base);
        return false;
    }
}
