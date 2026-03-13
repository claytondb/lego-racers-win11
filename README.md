# LEGO Racers Win11

Makes **LEGO Racers (1999)** run on Windows 10/11 at full resolution with Xbox controller support and an experimental online multiplayer mod.

## What's included

| Component | Description |
|-----------|-------------|
| `LegoController/` | C# tool — maps Xbox controller to keyboard, manages the game window |
| `NetMod/` | C++ DINPUT.DLL proxy — online multiplayer mod (press **F10** in-game to activate) |
| `Server/` | Python asyncio relay server for online play (TCP 3031 + UDP 3030) |
| `config/` | dgVoodoo2 config, dxwrapper config, and launch batch file |
| `installer/` | Inno Setup script to build `LEGORacers_Win11_Setup.exe` |

## Requirements

To build and run you'll need:

- LEGO Racers 1999 (the original NoCD/NoDRM version — not included)
- [dgVoodoo2](http://dege.freeweb.hu/dgVoodoo2/) — DirectDraw / Direct3D Immediate Mode wrapper
- [dxwrapper](https://github.com/elishacloud/dxwrapper) — DirectSound fix
- [Inno Setup 6](https://jrsoftware.org/isinfo.php) — to build the installer
- .NET 10 SDK — to build LegoController
- Visual Studio 2022 (MSVC x86) — to build DINPUT.DLL
- Python 3.10+ — to run the relay server

## Running the game

Use the pre-built installer (`LEGORacers_Win11_Setup.exe`) if you have it, or:

1. Place the game files alongside the configs from `config/`
2. Build `LegoController.exe` with `dotnet publish`
3. Build `DINPUT.DLL` with MSVC x86 (see `NetMod/` README)
4. Launch via `Play LEGO Racers.bat`

## Online multiplayer (experimental)

The multiplayer mod is a DINPUT.DLL proxy that reads/writes game memory to sync two players over UDP.

- **Press F10** during gameplay to open the lobby dialog
- One player hosts (opens UDP port 27777), the other connects by IP
- For internet play, run `Server/server.py` on a VPS as a relay

Memory addresses are for the **1999 NoDRM version** of `LEGORacers.exe` (438,272 bytes). See `NetMod/memory_scanner.h` for the full pointer chain.

## Xbox controller mapping

LegoController maps the Xbox controller to the game's keyboard controls:

| Controller | Game action |
|-----------|-------------|
| Left stick / D-pad | Steer |
| Right trigger | Accelerate |
| Left trigger | Brake/reverse |
| A | Select / fire weapon |
| Start | Pause |

## Build notes

Build DINPUT.DLL (32-bit, must match game architecture):
```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
cd NetMod
cl /O2 dinput_proxy.cpp memory_scanner.cpp netcode.cpp game_hook.cpp lobby_ui.cpp ^
  /link /DLL /OUT:DINPUT.DLL /MACHINE:X86 ^
  /EXPORT:DirectInputCreateA /EXPORT:DirectInputCreateW ^
  /EXPORT:DirectInputCreateEx /EXPORT:DirectInput8Create ^
  ws2_32.lib winmm.lib user32.lib kernel32.lib ole32.lib
```

## Credits

- [dgVoodoo2](http://dege.freeweb.hu/dgVoodoo2/) by Dege
- [dxwrapper](https://github.com/elishacloud/dxwrapper) by Elisha
- [LROnline](https://github.com/grappigegovert/LROnline) — memory addresses reference
- [Lego-Racers-Mod](https://github.com/Laupetin/Lego-Racers-Mod) — DINPUT.DLL injection approach
