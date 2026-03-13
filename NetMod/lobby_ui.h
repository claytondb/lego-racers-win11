#pragma once

#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX

#include <windows.h>
#include <cstdint>

struct LobbyConfig {
    bool isHost;
    char remoteIP[64];
    uint16_t portNum;
    bool confirmed;
};

// Show a Win32 dialog box asking user to host or join
// Returns when user clicks OK or cancels
LobbyConfig ShowLobbyDialog(HINSTANCE hInst);
