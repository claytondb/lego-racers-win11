; LEGO Racers Windows 11 Edition - Inno Setup Script
; Personal use installer for David Clayton

#define MyAppName "LEGO Racers"
#define MyAppVersion "1.3"
#define MyAppPublisher "LEGO Media (personal use)"
#define SourceDir "C:\Users\clayt\OneDrive\Documents\dc-lego-racers\LegoRacersPortable"
#define OutputDir "C:\Users\clayt\OneDrive\Documents\dc-lego-racers"

[Setup]
AppId={{B4E2F1A3-7C91-4D5E-8F62-3A9D0C1E7B45}
AppName={#MyAppName} - Windows 11 Edition
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\LEGO Racers Win11
DefaultGroupName=LEGO Racers
AllowNoIcons=no
OutputDir={#OutputDir}
OutputBaseFilename=LEGORacers_Win11_Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#SourceDir}\Racers.ico
UninstallDisplayIcon={app}\Racers.ico
UninstallDisplayName=LEGO Racers - Windows 11 Edition
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=no
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; === Main Executables and DLLs ===
Source: "{#SourceDir}\LEGORacers.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LEGORacers.icd"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\GolDP.dll"; DestDir: "{app}"; Flags: ignoreversion

; === dgVoodoo2 DirectX wrapper (1080p upscaling + AA) ===
Source: "{#SourceDir}\DDraw.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\D3DImm.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\dgVoodoo.conf"; DestDir: "{app}"; Flags: ignoreversion

; === dxwrapper (DirectSound fix) ===
Source: "{#SourceDir}\dsound.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\dxwrapper.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\dxwrapper.ini"; DestDir: "{app}"; Flags: ignoreversion

; === Intro Videos (re-encoded for Windows 11 compatibility) ===
Source: "{#SourceDir}\HVSCmp.avi"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\introcmp.avi"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\lmicmp.avi"; DestDir: "{app}"; Flags: ignoreversion

; === Xbox Controller Bridge (WASD + XInput helper + Window Manager) ===
Source: "{#SourceDir}\LegoController.exe"; DestDir: "{app}"; Flags: ignoreversion

; === Online Multiplayer Mod (DINPUT.DLL proxy — loads automatically) ===
Source: "{#SourceDir}\DINPUT.DLL"; DestDir: "{app}"; Flags: ignoreversion

; === Standalone Network Client (alternative to DINPUT.DLL for relay server use) ===
Source: "{#SourceDir}\LRNetClient.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; === Main Game Data ===
Source: "{#SourceDir}\LEGO.JAM"; DestDir: "{app}"; Flags: ignoreversion

; === Track/Asset Files ===
Source: "{#SourceDir}\1st.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\2nd.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\3rd.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\alien.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\amazon.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\armor.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\bonus.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\builder.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\cannon.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Circuit1.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Circuit2.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Circuit3.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Circuit4.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Circuit5.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Circuit6.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\credits.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\docks.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\forest.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\GameIntr.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\gencar.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\gold.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\hook.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\HvsIntro.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\ice.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\island.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\knight.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Lose.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\luck.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\magma.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\meteor.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\moat.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\parrot.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\pyrmid.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\reward.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\rocket.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\royal.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\RRacer.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\RRCar.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\skull.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\sphinx.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\stars.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\start.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\test.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\theme.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\tomb.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\ufo.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\VVCar.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\whip.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\win.tun"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\witch.tun"; DestDir: "{app}"; Flags: ignoreversion

; === Icon, Launcher, Manual ===
Source: "{#SourceDir}\Racers.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Play LEGO Racers.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Manual.pdf"; DestDir: "{app}"; Flags: ignoreversion

; === Online Multiplayer Relay Server (run separately for internet play) ===
Source: "C:\Temp\LegoServer\server.py"; DestDir: "{app}\Server"; Flags: ignoreversion
Source: "C:\Temp\LegoServer\README.md"; DestDir: "{app}\Server"; Flags: ignoreversion skipifsourcedoesntexist

; === Fresh save files with WASD bindings ===
Source: "{#SourceDir}\Save\0\LEGORac1"; DestDir: "{app}\Save\0"; Flags: ignoreversion onlyifdoesntexist
Source: "{#SourceDir}\Save\1\LEGORac1"; DestDir: "{app}\Save\1"; Flags: ignoreversion onlyifdoesntexist

[Dirs]
Name: "{app}\Save\0"
Name: "{app}\Save\1"
Name: "{app}\Save\2"
Name: "{app}\Save\3"
Name: "{app}\Save\4"
Name: "{app}\Save\5"
Name: "{app}\Save\6"
Name: "{app}\Save\7"
Name: "{app}\Save\8"
Name: "{app}\Save\9"
Name: "{app}\Save\10"
Name: "{app}\Save\11"
Name: "{app}\Save\12"
Name: "{app}\Save\13"
Name: "{app}\Save\14"
Name: "{app}\Save\15"
Name: "{app}\Server"

[Icons]
Name: "{group}\Play LEGO Racers"; Filename: "{app}\Play LEGO Racers.bat"; WorkingDir: "{app}"; IconFilename: "{app}\Racers.ico"; Comment: "Play LEGO Racers (Windows 11 Edition)"
Name: "{group}\Game Manual (PDF)"; Filename: "{app}\Manual.pdf"; Comment: "View the LEGO Racers game manual"
Name: "{group}\Uninstall LEGO Racers"; Filename: "{uninstallexe}"
Name: "{autodesktop}\LEGO Racers"; Filename: "{app}\Play LEGO Racers.bat"; WorkingDir: "{app}"; IconFilename: "{app}\Racers.ico"; Tasks: desktopicon; Comment: "Play LEGO Racers (Windows 11 Edition)"

[Run]
Filename: "{app}\Play LEGO Racers.bat"; Description: "{cm:LaunchProgram,LEGO Racers}"; Flags: nowait postinstall skipifsilent shellexec; WorkingDir: "{app}"

[Code]
procedure InitializeWizard();
begin
  WizardForm.WelcomeLabel2.Caption :=
    'This will install LEGO Racers (Windows 11 Edition) on your computer.' + #13#10 + #13#10 +
    'Windows 11 fixes included:' + #13#10 +
    '  - Intro movies skipped (instant launch)' + #13#10 +
    '  - Windowed mode (borderless, alt-tab friendly)' + #13#10 +
    '  - DirectSound compatibility via dxwrapper' + #13#10 +
    '  - 1080p rendering + 4x AA via dgVoodoo2' + #13#10 +
    '  - WASD keyboard controls (patched in save file)' + #13#10 +
    '  - Xbox controller support (XInput bridge)' + #13#10 +
    '  - Online multiplayer (DINPUT.DLL network mod)' + #13#10 + #13#10 +
    'ONLINE MULTIPLAYER:' + #13#10 +
    '  Both players launch LEGO Racers — a lobby dialog' + #13#10 +
    '  appears. Host selects "Host", client selects "Join"' + #13#10 +
    '  and enters the host''s IP. UDP port 27777.' + #13#10 + #13#10 +
    'Click Next to continue.';
end;
