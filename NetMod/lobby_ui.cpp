#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include "lobby_ui.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

extern void Log(const char* fmt, ...);

// Dialog IDs
#define IDC_RADIO_HOST 1001
#define IDC_RADIO_JOIN 1002
#define IDC_EDIT_IP 1003
#define IDC_EDIT_PORT 1004
#define IDC_BTN_OK 1005
#define IDC_BTN_CANCEL 1006

static LobbyConfig g_dialogResult;

INT_PTR CALLBACK LobbyDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            // Set up dialog controls
            SendDlgItemMessage(hDlg, IDC_RADIO_HOST, BM_SETCHECK, BST_CHECKED, 0);
            SendDlgItemMessage(hDlg, IDC_RADIO_JOIN, BM_SETCHECK, BST_UNCHECKED, 0);
            
            // Disable IP field initially (host mode doesn't need it)
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_IP), FALSE);
            
            // Set default port
            SetDlgItemTextA(hDlg, IDC_EDIT_PORT, "27777");
            
            return TRUE;
        }
        
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            
            if (wmId == IDC_RADIO_HOST) {
                // Host mode selected
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_IP), FALSE);
                SetDlgItemTextA(hDlg, IDC_EDIT_IP, "");
                return TRUE;
            }
            
            if (wmId == IDC_RADIO_JOIN) {
                // Join mode selected
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_IP), TRUE);
                SetDlgItemTextA(hDlg, IDC_EDIT_IP, "127.0.0.1");
                return TRUE;
            }
            
            if (wmId == IDC_BTN_OK) {
                // Get selected mode
                if (SendDlgItemMessage(hDlg, IDC_RADIO_HOST, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    g_dialogResult.isHost = true;
                } else {
                    g_dialogResult.isHost = false;
                }
                
                // Get IP (only if joining)
                if (!g_dialogResult.isHost) {
                    GetDlgItemTextA(hDlg, IDC_EDIT_IP, g_dialogResult.remoteIP, sizeof(g_dialogResult.remoteIP) - 1);
                } else {
                    strcpy_s(g_dialogResult.remoteIP, sizeof(g_dialogResult.remoteIP), "");
                }
                
                // Get port
                char portStr[16];
                GetDlgItemTextA(hDlg, IDC_EDIT_PORT, portStr, sizeof(portStr) - 1);
                g_dialogResult.portNum = (uint16_t)atoi(portStr);
                if (g_dialogResult.portNum == 0) {
                    g_dialogResult.portNum = 27777;
                }
                
                g_dialogResult.confirmed = true;
                EndDialog(hDlg, IDOK);
                return TRUE;
            }
            
            if (wmId == IDC_BTN_CANCEL) {
                g_dialogResult.confirmed = false;
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            
            break;
        }
        
        case WM_CLOSE: {
            g_dialogResult.confirmed = false;
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
    }
    
    return FALSE;
}

LobbyConfig ShowLobbyDialog(HINSTANCE hInst) {
    Log("[LobbyUI] Showing lobby dialog");
    
    memset(&g_dialogResult, 0, sizeof(g_dialogResult));
    
    // Create a simple dialog programmatically
    // Since we don't have a .rc file, we'll use CreateWindowEx to build the dialog
    
    // Create the dialog window
    HWND hDlg = CreateDialogParamA(
        hInst,
        MAKEINTRESOURCEA(101), // Resource ID (we'll create it manually)
        nullptr,
        LobbyDialogProc,
        0);
    
    // If CreateDialogParam fails, create window manually
    if (!hDlg) {
        Log("[LobbyUI] Creating dialog window manually");
        
        // Register dialog class if needed
        WNDCLASSA wc = {};
        wc.lpfnWndProc = DefDlgProcA;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "LobbyDialog";
        RegisterClassA(&wc);
        
        // Create the dialog window
        hDlg = CreateWindowExA(
            WS_EX_DLGMODALFRAME,
            "LobbyDialog",
            "LEGO Racers - Network Setup",
            WS_POPUP | WS_CAPTION | WS_SYSMENU,
            100, 100, 400, 300,
            nullptr, nullptr, hInst, nullptr);
        
        if (!hDlg) {
            Log("[LobbyUI] Failed to create dialog window");
            g_dialogResult.confirmed = false;
            return g_dialogResult;
        }
        
        // Create radio buttons
        CreateWindowExA(0, "BUTTON", "Host Game",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            20, 20, 150, 25,
            hDlg, (HMENU)(intptr_t)IDC_RADIO_HOST, hInst, nullptr);
        
        CreateWindowExA(0, "BUTTON", "Join Game",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            20, 55, 150, 25,
            hDlg, (HMENU)(intptr_t)IDC_RADIO_JOIN, hInst, nullptr);
        
        // Create IP label and input
        CreateWindowExA(0, "STATIC", "Remote IP:",
            WS_CHILD | WS_VISIBLE,
            20, 95, 100, 20,
            hDlg, nullptr, hInst, nullptr);
        
        CreateWindowExA(0, "EDIT", "127.0.0.1",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            130, 95, 200, 20,
            hDlg, (HMENU)(intptr_t)IDC_EDIT_IP, hInst, nullptr);
        
        // Create port label and input
        CreateWindowExA(0, "STATIC", "Port:",
            WS_CHILD | WS_VISIBLE,
            20, 130, 100, 20,
            hDlg, nullptr, hInst, nullptr);
        
        CreateWindowExA(0, "EDIT", "27777",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            130, 130, 200, 20,
            hDlg, (HMENU)(intptr_t)IDC_EDIT_PORT, hInst, nullptr);
        
        // Create buttons
        CreateWindowExA(0, "BUTTON", "OK",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            130, 190, 100, 30,
            hDlg, (HMENU)(intptr_t)IDC_BTN_OK, hInst, nullptr);
        
        CreateWindowExA(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            250, 190, 100, 30,
            hDlg, (HMENU)(intptr_t)IDC_BTN_CANCEL, hInst, nullptr);
        
        // Initialize dialog state
        g_dialogResult.isHost = true;
        g_dialogResult.portNum = 27777;
        strcpy_s(g_dialogResult.remoteIP, sizeof(g_dialogResult.remoteIP), "127.0.0.1");
        g_dialogResult.confirmed = false;
    }
    
    // Show dialog
    ShowWindow(hDlg, SW_SHOW);
    SetForegroundWindow(hDlg);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Check if dialog was closed
        if (!IsWindow(hDlg)) {
            break;
        }
    }
    
    // Destroy dialog
    if (IsWindow(hDlg)) {
        DestroyWindow(hDlg);
    }
    
    Log("[LobbyUI] Dialog result: confirmed=%d, isHost=%d", 
        g_dialogResult.confirmed, g_dialogResult.isHost);
    
    return g_dialogResult;
}



