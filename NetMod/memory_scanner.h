#pragma once
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX
#include <windows.h>
#include <cstdint>

// ─── LEGO Racers 1999 NoDRM — Verified addresses (from LROnline project) ──────
// Source: https://github.com/grappigegovert/LROnline
//
// Pointer chain to get driver base:
//   1. Read DWORD at DRIVER_BASEADDRESS              → ptr0
//   2. Read DWORD at ptr0 + CHAIN[0] (0x98)         → ptr1
//   3. Read DWORD at ptr1 + CHAIN[1] (0x4F0)        → ptr2
//   4. Read DWORD at ptr2 + CHAIN[2] (0x188)        → ptr3
//   5. addr = ptr3 + CHAIN[3] (0x548)  (no final deref)
//   6. driverBase(i) = DWORD at (addr + i * 4)

#define DRIVER_BASEADDRESS      0x004C530CUL
#define INRACE_BASEADDRESS      0x004C4914UL
#define INRACE_DRIVERCOUNT_OFF  0x598

static const DWORD DRIVER_CHAIN_OFFSETS[] = { 0x98, 0x4F0, 0x188, 0x548 };
#define DRIVER_CHAIN_LEN 4

// Per-driver offsets (relative to each driver object's base pointer)
#define DRIVER_OFF_POS_X    0x518u  // float
#define DRIVER_OFF_POS_Y    0x51Cu  // float
#define DRIVER_OFF_POS_Z    0x520u  // float
#define DRIVER_OFF_SPD_X    0x3F0u  // float (velocity X)
#define DRIVER_OFF_SPD_Y    0x3F4u  // float (velocity Y)
#define DRIVER_OFF_SPD_Z    0x3F8u  // float (velocity Z)
#define DRIVER_OFF_FWD_X    0x4F4u  // float (forward vector X)
#define DRIVER_OFF_FWD_Y    0x4F8u  // float (forward vector Y)
#define DRIVER_OFF_FWD_Z    0x4FCu  // float (forward vector Z)
#define DRIVER_OFF_UP_X     0x50Cu  // float (up vector X)
#define DRIVER_OFF_UP_Y     0x510u  // float (up vector Y)
#define DRIVER_OFF_UP_Z     0x514u  // float (up vector Z)
#define DRIVER_OFF_BRICK    0xCCCu  // int  (brick type 0-4)
#define DRIVER_OFF_WHITE    0xD58u  // int  (white bricks 0-3)

// ─── Player state sent/received over network ──────────────────────────────────
#pragma pack(push, 1)
struct PlayerState {
    float posX, posY, posZ;     // World position
    float spdX, spdY, spdZ;     // Velocity vector
    float fwdX, fwdY, fwdZ;     // Forward direction
    float upX,  upY,  upZ;      // Up direction
    int32_t brick;              // Powerup type (0=none,1=red,2=blue,3=green,4=yellow)
    int32_t whiteBricks;        // White brick charge level (0-3)
    uint8_t  flags;             // bit0=inRace, bit1=finished
    uint8_t  _pad[3];
};
#pragma pack(pop)

// ─── API ──────────────────────────────────────────────────────────────────────

// Initialize (should be called once in-game, after loading)
bool MemScanner_Init();

// True if in an active race (InRace base pointer is valid and non-null)
bool MemScanner_IsInRace();

// Resolve driver base pointer for driverIndex (0 = local player, 1 = remote/player2)
// Returns 0 on failure
DWORD MemScanner_GetDriverBase(int driverIndex);

// Read/Write full player state for driver driverIndex
bool MemScanner_ReadState(int driverIndex, PlayerState* out);
bool MemScanner_WriteState(int driverIndex, const PlayerState* in);
