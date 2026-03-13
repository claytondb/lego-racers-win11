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

REM LegoController is now the launcher — it shows the menu, swaps textures, then starts the game
start "" "LegoController.exe"
