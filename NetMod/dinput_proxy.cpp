//
// dinput_proxy.cpp — LEGO Racers NetMod
// DINPUT.DLL placed in the game directory is loaded instead of the system one.
// We forward all DirectInput calls, and inject our networking on a background thread.
//
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <objbase.h>`n#include <cstring>

#include "memory_scanner.h"
#include "netcode.h"
#include "game_hook.h"
#include "lobby_ui.h"

// ─── Logging ──────────────────────────────────────────────────────────────────
static FILE* g_logFile = nullptr;

void InitLog() {
    if (!g_logFile) {
        fopen_s(&g_logFile, "C:\\Temp\\LegoNetMod.log", "a");
        if (g_logFile) {
            fprintf(g_logFile, "\n=== LegoNetMod started (%s %s) ===\n",
                    __DATE__, __TIME__);
            fflush(g_logFile);
        }
    }
}

void Log(const char* fmt, ...) {
    InitLog();
    if (!g_logFile) return;
    // Timestamp
    fprintf(g_logFile, "[%06u] ", (unsigned)GetTickCount() % 1000000);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
}

// ─── Real DINPUT.DLL ─────────────────────────────────────────────────────────
static HMODULE g_realDinput = nullptr;

typedef HRESULT (WINAPI* FnDICA)(HINSTANCE,DWORD,LPVOID*,IUnknown*);
typedef HRESULT (WINAPI* FnDICW)(HINSTANCE,DWORD,LPVOID*,IUnknown*);
typedef HRESULT (WINAPI* FnDICEx)(HINSTANCE,DWORD,REFIID,LPVOID*,IUnknown*);
typedef HRESULT (WINAPI* FnDI8C)(HINSTANCE,DWORD,REFIID,LPVOID*,IUnknown*);
typedef HRESULT (WINAPI* FnCULN)(void);
typedef HRESULT (WINAPI* FnGCO)(REFCLSID,REFIID,LPVOID*);
typedef HRESULT (WINAPI* FnRS)(void);
typedef HRESULT (WINAPI* FnUS)(void);

static FnDICA  p_DirectInputCreateA   = nullptr;
static FnDICW  p_DirectInputCreateW   = nullptr;
static FnDICEx p_DirectInputCreateEx  = nullptr;
static FnDI8C  p_DirectInput8Create   = nullptr;
static FnCULN  p_DllCanUnloadNow      = nullptr;
static FnGCO   p_DllGetClassObject    = nullptr;
static FnRS    p_DllRegisterServer    = nullptr;
static FnUS    p_DllUnregisterServer  = nullptr;

static bool LoadRealDinput() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    wchar_t path[MAX_PATH];
    swprintf_s(path, MAX_PATH, L"%s\\dinput.dll", sysDir);
    g_realDinput = LoadLibraryW(path);
    if (!g_realDinput) { Log("[Proxy] Cannot load %S (%d)", path, GetLastError()); return false; }
    Log("[Proxy] Loaded real dinput.dll from %S", path);
#define PROC(sym, var) var = (decltype(var))GetProcAddress(g_realDinput, #sym)
    PROC(DirectInputCreateA,  p_DirectInputCreateA);
    PROC(DirectInputCreateW,  p_DirectInputCreateW);
    PROC(DirectInputCreateEx, p_DirectInputCreateEx);
    PROC(DirectInput8Create,  p_DirectInput8Create);
    PROC(DllCanUnloadNow,     p_DllCanUnloadNow);
    PROC(DllGetClassObject,   p_DllGetClassObject);
    PROC(DllRegisterServer,   p_DllRegisterServer);
    PROC(DllUnregisterServer, p_DllUnregisterServer);
#undef PROC
    return true;
}

// ─── Forwarded exports ────────────────────────────────────────────────────────
extern "C" {

HRESULT WINAPI DirectInputCreateA(HINSTANCE h,DWORD v,LPVOID* p,IUnknown* u)
{ return p_DirectInputCreateA ? p_DirectInputCreateA(h,v,p,u) : E_FAIL; }

HRESULT WINAPI DirectInputCreateW(HINSTANCE h,DWORD v,LPVOID* p,IUnknown* u)
{ return p_DirectInputCreateW ? p_DirectInputCreateW(h,v,p,u) : E_FAIL; }

HRESULT WINAPI DirectInputCreateEx(HINSTANCE h,DWORD v,REFIID r,LPVOID* p,IUnknown* u)
{ return p_DirectInputCreateEx ? p_DirectInputCreateEx(h,v,r,p,u) : E_FAIL; }

HRESULT WINAPI DirectInput8Create(HINSTANCE h,DWORD v,REFIID r,LPVOID* p,IUnknown* u)
{ return p_DirectInput8Create ? p_DirectInput8Create(h,v,r,p,u) : E_FAIL; }

HRESULT WINAPI DllCanUnloadNow(void)
{ return p_DllCanUnloadNow ? p_DllCanUnloadNow() : S_FALSE; }

HRESULT WINAPI DllGetClassObject(REFCLSID c,REFIID r,LPVOID* p)
{ return p_DllGetClassObject ? p_DllGetClassObject(c,r,p) : E_FAIL; }

HRESULT WINAPI DllRegisterServer(void)
{ return p_DllRegisterServer ? p_DllRegisterServer() : E_FAIL; }

HRESULT WINAPI DllUnregisterServer(void)
{ return p_DllUnregisterServer ? p_DllUnregisterServer() : E_FAIL; }

} // extern "C"

// ─── Mod thread ───────────────────────────────────────────────────────────────
bool g_isHost = false;
static volatile bool g_shutdown = false;

static DWORD WINAPI ModThread(LPVOID)
{
    Log("[Mod] NetMod loaded. Press F10 during gameplay to open the multiplayer lobby.");

    // Wait for game to fully initialize before monitoring for hotkey
    Sleep(3000);

    bool f10WasDown  = false;
    bool netStarted  = false;

    while (!g_shutdown) {
        Sleep(100);

        // F10 toggles the lobby dialog
        bool f10Now = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
        bool f10Pressed = f10Now && !f10WasDown;
        f10WasDown = f10Now;

        if (!f10Pressed || netStarted) continue;

        Log("[Mod] F10 pressed - showing lobby dialog");

        LobbyConfig cfg = ShowLobbyDialog(nullptr);
        if (!cfg.confirmed) {
            Log("[Mod] Lobby cancelled");
            continue;
        }

        g_isHost = cfg.isHost;
        Log("[Mod] Lobby: isHost=%d ip=%s port=%d", cfg.isHost, cfg.remoteIP, cfg.portNum);

        MemScanner_Init();

        g_netSession = new NetSession();
        bool ok = cfg.isHost
            ? g_netSession->StartHost(cfg.portNum)
            : g_netSession->Connect(cfg.remoteIP, cfg.portNum);

        if (!ok) {
            Log("[Mod] Network setup failed");
            delete g_netSession; g_netSession = nullptr;
            continue;
        }

        netStarted = true;

        if (!InstallGameLoopHook()) {
            Log("[Mod] Game hook failed - will poll instead");
            while (!g_shutdown) {
                OnGameTick();
                Sleep(16);
            }
        }
    }

    Log("[Mod] Shutdown");
    UninstallGameLoopHook();
    if (g_netSession) { g_netSession->Stop(); delete g_netSession; g_netSession = nullptr; }
    return 0;
}
BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        InitLog();
        Log("[DllMain] PROCESS_ATTACH");
        DisableThreadLibraryCalls(hMod);
        if (!LoadRealDinput()) return FALSE;
        CreateThread(nullptr, 0, ModThread, nullptr, 0, nullptr);
        Log("[DllMain] Mod thread created");
        break;

    case DLL_PROCESS_DETACH:
        Log("[DllMain] PROCESS_DETACH");
        g_shutdown = true;
        if (g_realDinput) { FreeLibrary(g_realDinput); g_realDinput = nullptr; }
        if (g_logFile)    { fclose(g_logFile); g_logFile = nullptr; }
        break;
    }
    return TRUE;
}
