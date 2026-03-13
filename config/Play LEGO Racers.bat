@echo off
title LEGO Racers - Windows 11 Edition
cd /d "%~dp0"

echo.
echo  ============================================
echo    LEGO Racers - Windows 11 Compatible
echo  ============================================
echo.
echo  Launching LEGO Racers...
echo  (Fullscreen - press ESC to exit)
echo.
echo  Controls:
echo    Keyboard:   WASD = Steer/Accel/Brake
echo                Enter = Fire weapon
echo    Xbox Pad:   Left Stick / D-Pad = Steer
echo                RT = Accelerate, LT = Brake
echo                A = Fire weapon
echo.

REM Start Xbox controller bridge in background (auto-detects controller)
start "" /min "LegoController.exe"

REM Small delay to let controller bridge initialize
timeout /t 1 >nul

REM Launch game - no flags needed
start "" "LEGORacers.exe"

REM Wait for game to exit, then clean up controller bridge
:wait_loop
timeout /t 2 >nul
tasklist /FI "IMAGENAME eq LEGORacers.exe" 2>nul | find /I "LEGORacers.exe" >nul
if %errorlevel%==0 goto wait_loop

REM Game exited - close controller bridge
taskkill /IM "LegoController.exe" /F >nul 2>&1
