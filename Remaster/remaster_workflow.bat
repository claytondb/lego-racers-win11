@echo off
setlocal EnableDelayedExpansion

REM ============================================================================
REM  LEGO Racers (1999) Remaster Workflow
REM  Run from: lego-racers-win11\Remaster\
REM
REM  STEP 1: Extract textures    -> Run this script with no args (or "extract")
REM  STEP 2: Upscale textures    -> Run this script with "upscale" (needs Real-ESRGAN)
REM  STEP 3: Rebuild JAM         -> Run this script with "repack"
REM  STEP 4: All steps at once   -> Run this script with "all"
REM ============================================================================

set GAME_DIR=%USERPROFILE%\OneDrive\Documents\dc-lego-racers\LegoRacersPortable
set WORKSPACE=%GAME_DIR%\remaster_workspace
set PYTHON=python

REM Check Python
%PYTHON% --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found. Install Python from https://python.org
    pause
    exit /b 1
)

REM Check Pillow
%PYTHON% -c "from PIL import Image" >nul 2>&1
if errorlevel 1 (
    echo Installing Pillow...
    %PYTHON% -m pip install Pillow
)

REM Parse argument
set STEP=%1
if "%STEP%"=="" set STEP=menu

if "%STEP%"=="menu" goto :menu
if "%STEP%"=="extract" goto :extract
if "%STEP%"=="upscale" goto :upscale
if "%STEP%"=="repack" goto :repack
if "%STEP%"=="all" goto :all
goto :menu

:menu
echo.
echo  LEGO Racers Remaster Workflow
echo  ===============================
echo  [1] Extract textures from LEGO.JAM (first time setup)
echo  [2] Upscale textures with Real-ESRGAN
echo  [3] Repack upscaled textures into LEGO_REMASTERED.JAM
echo  [4] Run all steps (extract + upscale + repack)
echo  [Q] Quit
echo.
set /p CHOICE="Choose: "
if "%CHOICE%"=="1" goto :extract
if "%CHOICE%"=="2" goto :upscale
if "%CHOICE%"=="3" goto :repack
if "%CHOICE%"=="4" goto :all
if /i "%CHOICE%"=="Q" exit /b 0
goto :menu

:extract
echo.
echo [1/3] Extracting textures from LEGO.JAM...
echo       Output: %WORKSPACE%\original_textures\
echo.
%PYTHON% "%~dp0extract_assets.py" --jam "%GAME_DIR%\LEGO.JAM" --out "%WORKSPACE%\original_textures"
if errorlevel 1 goto :error
echo.
echo Extraction complete!
echo Next: Run step 2 (upscale) to AI-upscale the PNG textures.
if "%STEP%"=="extract" pause
goto :done_extract

:upscale
echo.
echo [2/3] Upscaling textures with Real-ESRGAN...
echo       Input:  %WORKSPACE%\original_textures\
echo       Output: %WORKSPACE%\upscaled_textures\
echo.

REM Find Real-ESRGAN
set ESRGAN=
if exist "C:\tools\realesrgan\realesrgan-ncnn-vulkan.exe" (
    set ESRGAN=C:\tools\realesrgan\realesrgan-ncnn-vulkan.exe
)
if exist "%USERPROFILE%\Downloads\realesrgan-ncnn-vulkan\realesrgan-ncnn-vulkan.exe" (
    set ESRGAN=%USERPROFILE%\Downloads\realesrgan-ncnn-vulkan\realesrgan-ncnn-vulkan.exe
)

if "%ESRGAN%"=="" (
    echo.
    echo  Real-ESRGAN not found!
    echo.
    echo  1. Download from: https://github.com/xinntao/Real-ESRGAN/releases
    echo     File: realesrgan-ncnn-vulkan-*-windows.zip
    echo.
    echo  2. Extract to one of:
    echo     C:\tools\realesrgan\
    echo     %USERPROFILE%\Downloads\realesrgan-ncnn-vulkan\
    echo.
    echo  3. Then run this script again.
    echo.
    pause
    exit /b 1
)

echo Using Real-ESRGAN: %ESRGAN%
%PYTHON% "%~dp0upscale_textures.py" --input "%WORKSPACE%\original_textures" --output "%WORKSPACE%\upscaled_textures" --esrgan "%ESRGAN%" --model realesr-animevideov3
if errorlevel 1 goto :error
if "%STEP%"=="upscale" pause
goto :done_upscale

:repack
echo.
echo [3/3] Repacking into LEGO_REMASTERED.JAM...
echo       Input:  %WORKSPACE%\upscaled_textures\
echo       Output: %GAME_DIR%\LEGO_REMASTERED.JAM
echo.
%PYTHON% "%~dp0repack_remastered.py" --jam "%GAME_DIR%\LEGO.JAM" --upscaled "%WORKSPACE%\upscaled_textures" --out "%GAME_DIR%\LEGO_REMASTERED.JAM"
if errorlevel 1 goto :error
echo.
echo ============================================================
echo  DONE! LEGO_REMASTERED.JAM is ready.
echo.
echo  To use remastered textures:
echo  Launch the game via LegoController.exe and press [R].
echo ============================================================
if "%STEP%"=="repack" pause
goto :done_repack

:all
call :extract
call :upscale
call :repack
goto :end

:done_extract
:done_upscale
:done_repack
:end
exit /b 0

:error
echo.
echo ERROR: Step failed. Check output above.
pause
exit /b 1
