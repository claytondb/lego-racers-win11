#pragma once
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

bool InstallGameLoopHook();
void UninstallGameLoopHook();
void OnGameTick();
