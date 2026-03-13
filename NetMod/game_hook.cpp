#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include "game_hook.h"
#include "memory_scanner.h"
#include "netcode.h"

#pragma comment(lib, "winmm.lib")

extern void Log(const char* fmt, ...);
extern NetSession* g_netSession;
extern bool g_isHost;

// ─── timeSetEvent hook ────────────────────────────────────────────────────────
typedef MMRESULT (WINAPI* timeSetEvent_t)(UINT, UINT, LPTIMECALLBACK, DWORD_PTR, UINT);
static timeSetEvent_t g_origTimeSetEvent = nullptr;
static bool g_hookInstalled = false;

// Original callback pointer registered by the game
static LPTIMECALLBACK g_gameCallback = nullptr;
static DWORD_PTR      g_gameUser     = 0;

// Our wrapper — called on game tick
static void CALLBACK TimerHook(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
    // Forward to original game callback FIRST (keep physics deterministic)
    if (g_gameCallback)
        g_gameCallback(uTimerID, uMsg, g_gameUser, dw1, dw2);

    // Then do our network I/O
    OnGameTick();
}

// Hooked timeSetEvent — intercepts the game's timer setup
MMRESULT WINAPI Hooked_timeSetEvent(
    UINT uDelay, UINT uResolution,
    LPTIMECALLBACK lpTimeProc, DWORD_PTR dwUser, UINT fuEvent)
{
    Log("[Hook] timeSetEvent called: delay=%u res=%u flags=%u", uDelay, uResolution, fuEvent);

    // Save the game's callback and user data
    g_gameCallback = lpTimeProc;
    g_gameUser     = dwUser;

    // Replace the callback with our wrapper
    MMRESULT r = g_origTimeSetEvent(uDelay, uResolution, TimerHook, dwUser, fuEvent);
    Log("[Hook] timeSetEvent -> id=%u", r);
    return r;
}

// 5-byte JMP patch trampoline
static BYTE  g_originalBytes[5] = {};
static void* g_hookTarget = nullptr;

bool InstallGameLoopHook() {
    // Get the address of timeSetEvent in WINMM.DLL
    HMODULE winmm = GetModuleHandleA("winmm.dll");
    if (!winmm) winmm = LoadLibraryA("winmm.dll");
    if (!winmm) {
        Log("[Hook] winmm.dll not found!");
        return false;
    }

    g_origTimeSetEvent = (timeSetEvent_t)GetProcAddress(winmm, "timeSetEvent");
    if (!g_origTimeSetEvent) {
        Log("[Hook] timeSetEvent not found in winmm.dll");
        return false;
    }
    Log("[Hook] timeSetEvent @ 0x%X", (DWORD)g_origTimeSetEvent);

    // Make the first 5 bytes writable
    DWORD oldProt;
    if (!VirtualProtect(g_origTimeSetEvent, 5, PAGE_EXECUTE_READWRITE, &oldProt)) {
        Log("[Hook] VirtualProtect failed: %d", GetLastError());
        return false;
    }

    // Save original bytes
    memcpy(g_originalBytes, g_origTimeSetEvent, 5);

    // Write JMP rel32 to our hook
    BYTE* target = (BYTE*)g_origTimeSetEvent;
    DWORD rel = (DWORD)Hooked_timeSetEvent - (DWORD)g_origTimeSetEvent - 5;
    target[0] = 0xE9;
    memcpy(target + 1, &rel, 4);

    // Restore protection
    VirtualProtect(g_origTimeSetEvent, 5, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), g_origTimeSetEvent, 5);

    g_hookInstalled = true;
    Log("[Hook] timeSetEvent hook installed (JMP 0x%X)", (DWORD)Hooked_timeSetEvent);
    return true;
}

void UninstallGameLoopHook() {
    if (!g_hookInstalled || !g_origTimeSetEvent) return;

    DWORD oldProt;
    VirtualProtect(g_origTimeSetEvent, 5, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(g_origTimeSetEvent, g_originalBytes, 5);
    VirtualProtect(g_origTimeSetEvent, 5, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), g_origTimeSetEvent, 5);

    g_hookInstalled = false;
    Log("[Hook] timeSetEvent hook removed");
}

// ─── Per-tick network logic ───────────────────────────────────────────────────
static uint32_t g_tickCount = 0;
static const uint32_t LOG_EVERY_N_TICKS = 600; // log every ~10s at 60fps

void OnGameTick() {
    if (!g_netSession || !g_netSession->IsConnected()) return;

    g_tickCount++;

    // Send our state every tick
    if (MemScanner_IsInRace()) {
        PlayerState myState = {};
        if (MemScanner_ReadState(0, &myState)) {
            uint8_t slot = g_isHost ? 0 : 1;
            g_netSession->SendState(myState, slot);

            if (g_tickCount % LOG_EVERY_N_TICKS == 0) {
                Log("[Tick] Sent: pos=(%.1f,%.1f,%.1f) brick=%d ping=%ums",
                    myState.posX, myState.posY, myState.posZ,
                    myState.brick, g_netSession->GetPing());
            }
        }

        // Receive and apply remote state (driver slot 1 = second player)
        PlayerState remoteState = {};
        if (g_netSession->RecvState(&remoteState)) {
            MemScanner_WriteState(1, &remoteState);
        }
    }
}
