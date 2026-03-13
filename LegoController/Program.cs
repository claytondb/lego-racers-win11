// LEGO Racers Xbox Controller Bridge + Window Manager
// Maps Xbox Series controller (via XInput) to WASD+Enter keyboard input
// Only injects keys when LEGO Racers window is in the foreground
// Also manages the game window: continuously enforces 1280x720 windowed size

using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Diagnostics;
using System.Text;

// ─── Win32 / XInput P/Invoke ────────────────────────────────────────────────

static class Win32
{
    [DllImport("xinput1_4.dll")]
    public static extern int XInputGetState(int dwUserIndex, out XINPUT_STATE pState);

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll")]
    public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

    // Window management
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern long GetWindowLong(IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern long SetWindowLong(IntPtr hWnd, int nIndex, long dwNewLong);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter,
        int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int nIndex);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    public const int SM_CXSCREEN = 0;
    public const int SM_CYSCREEN = 1;

    // GWL_STYLE
    public const int GWL_STYLE = -16;

    // Window styles
    public const long WS_POPUP          = 0x80000000L;
    public const long WS_VISIBLE        = 0x10000000L;
    public const long WS_MAXIMIZE       = 0x01000000L;
    public const long WS_CAPTION        = 0x00C00000L; // title bar
    public const long WS_SYSMENU        = 0x00080000L; // system menu
    public const long WS_THICKFRAME     = 0x00040000L; // resize handles
    public const long WS_MINIMIZEBOX    = 0x00020000L;
    public const long WS_MAXIMIZEBOX    = 0x00010000L;
    public const long WS_CLIPSIBLINGS   = 0x04000000L;
    public const long WS_CLIPCHILDREN   = 0x02000000L;
    public const long WS_OVERLAPPEDWINDOW = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME
                                          | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    // SetWindowPos flags
    public const uint SWP_FRAMECHANGED  = 0x0020;
    public const uint SWP_SHOWWINDOW    = 0x0040;
    public const uint SWP_NOZORDER      = 0x0004;
    public const uint SWP_NOACTIVATE    = 0x0010;
    public const uint SWP_NOMOVE        = 0x0002;
    public const uint SWP_NOSIZE        = 0x0001;

    // ShowWindow
    public const int SW_RESTORE = 9;
    public const int SW_HIDE    = 0;

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left, Top, Right, Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct XINPUT_STATE
    {
        public uint dwPacketNumber;
        public XINPUT_GAMEPAD Gamepad;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct XINPUT_GAMEPAD
    {
        public ushort wButtons;
        public byte bLeftTrigger;
        public byte bRightTrigger;
        public short sThumbLX;
        public short sThumbLY;
        public short sThumbRX;
        public short sThumbRY;
    }

    public const ushort GAMEPAD_DPAD_UP    = 0x0001;
    public const ushort GAMEPAD_DPAD_DOWN  = 0x0002;
    public const ushort GAMEPAD_DPAD_LEFT  = 0x0004;
    public const ushort GAMEPAD_DPAD_RIGHT = 0x0008;
    public const ushort GAMEPAD_START      = 0x0010;
    public const ushort GAMEPAD_A          = 0x1000;
    public const ushort GAMEPAD_B          = 0x2000;

    public const short STICK_DEADZONE      = 12000;
    public const byte  TRIGGER_THRESHOLD   = 50;

    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT
    {
        public uint type;
        public KEYBDINPUT ki;
        public long padding;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT
    {
        public ushort wVk;
        public ushort wScan;
        public uint   dwFlags;
        public uint   time;
        public IntPtr dwExtraInfo;
    }

    public const uint INPUT_KEYBOARD     = 1;
    public const uint KEYEVENTF_SCANCODE = 0x0008;
    public const uint KEYEVENTF_KEYUP    = 0x0002;
}

static class DI
{
    public const ushort W     = 0x11;
    public const ushort S     = 0x1F;
    public const ushort A     = 0x1E;
    public const ushort D     = 0x20;
    public const ushort ENTER = 0x1C;
    public const ushort ESC   = 0x01;
    public const ushort SPACE = 0x39;
}

static class KeySender
{
    private static readonly bool[] _pressed = new bool[256];

    public static void Press(ushort scanCode)
    {
        if (scanCode >= _pressed.Length || _pressed[scanCode]) return;
        _pressed[scanCode] = true;
        Send(scanCode, false);
    }

    public static void Release(ushort scanCode)
    {
        if (scanCode >= _pressed.Length || !_pressed[scanCode]) return;
        _pressed[scanCode] = false;
        Send(scanCode, true);
    }

    public static void ReleaseAll()
    {
        for (ushort sc = 0; sc < _pressed.Length; sc++)
            if (_pressed[sc]) Release(sc);
    }

    private static void Send(ushort scanCode, bool keyUp)
    {
        var input = new Win32.INPUT
        {
            type = Win32.INPUT_KEYBOARD,
            ki = new Win32.KEYBDINPUT
            {
                wVk   = 0,
                wScan = scanCode,
                dwFlags = Win32.KEYEVENTF_SCANCODE | (keyUp ? Win32.KEYEVENTF_KEYUP : 0)
            }
        };
        Win32.SendInput(1, new[] { input }, Marshal.SizeOf<Win32.INPUT>());
    }
}

class WindowManager
{
    private const int TARGET_W   = 1280;
    private const int TARGET_H   = 720;
    private const int CHECK_MS   = 500;   // how often to check the window size
    private const int SETTLE_MS  = 5000;  // extra wait after window appears

    // Desired style: overlapped window with title bar, no popup flag
    private static long DesiredStyle =>
        (Win32.WS_OVERLAPPEDWINDOW | Win32.WS_VISIBLE |
         Win32.WS_CLIPSIBLINGS | Win32.WS_CLIPCHILDREN) &
        ~(Win32.WS_POPUP | Win32.WS_MAXIMIZE);


    // Hides any top-level window from the same process that is NOT our main window.
    // These are raw DirectDraw surfaces that dgVoodoo leaks to the screen.
    private static void HideGhostWindows(Process gameProcess, IntPtr mainWnd)
    {
        Win32.EnumWindows((hWnd, lParam) =>
        {
            if (hWnd == mainWnd) return true;
            if (!Win32.IsWindowVisible(hWnd)) return true;
            Win32.GetWindowThreadProcessId(hWnd, out uint pid);
            if ((int)pid != gameProcess.Id) return true;
            Win32.GetWindowRect(hWnd, out Win32.RECT r);
            int w = r.Right - r.Left;
            int h = r.Bottom - r.Top;
            var sb = new StringBuilder(256);
            Win32.GetWindowText(hWnd, sb, 256);
            Console.WriteLine($"[Window] Hiding ghost window hwnd=0x{hWnd:X} title=\"{sb}\" size={w}x{h}");
            Win32.SetWindowPos(hWnd, IntPtr.Zero, -10000, -10000, 1, 1,
                Win32.SWP_NOZORDER | Win32.SWP_NOACTIVATE);
            Win32.ShowWindow(hWnd, Win32.SW_HIDE);
            return true;
        }, IntPtr.Zero);
    }
    public static void Run()
    {
        try
        {
            // ── 1. Wait for LEGORacers.exe to start ─────────────────────────
            Process gameProcess = null;
            int waitMs = 0;
            while (gameProcess == null && waitMs < 60000)
            {
                var procs = Process.GetProcessesByName("LEGORacers");
                if (procs.Length > 0) gameProcess = procs[0];
                Thread.Sleep(500);
                waitMs += 500;
            }
            if (gameProcess == null)
            {
                Console.WriteLine("[Window] Game process not found after 60s.");
                return;
            }

            Console.WriteLine("[Window] Game process found. Waiting for window...");

            // ── 2. Wait for "LEGO Racers" window to appear (up to 30s) ──────
            IntPtr gameWnd = IntPtr.Zero;
            waitMs = 0;
            while (waitMs < 30000)
            {
                Thread.Sleep(1000);
                waitMs += 1000;
                if (gameProcess.HasExited) return;

                IntPtr found = IntPtr.Zero;
                Win32.EnumWindows((hWnd, lParam) =>
                {
                    if (!Win32.IsWindowVisible(hWnd)) return true;
                    Win32.GetWindowThreadProcessId(hWnd, out uint pid);
                    if ((int)pid != gameProcess.Id) return true;
                    var sb = new StringBuilder(256);
                    Win32.GetWindowText(hWnd, sb, 256);
                    if (sb.ToString() == "LEGO Racers")
                    {
                        found = hWnd;
                        return false;
                    }
                    return true;
                }, IntPtr.Zero);

                if (found != IntPtr.Zero)
                {
                    gameWnd = found;
                    break;
                }
            }

            if (gameWnd == IntPtr.Zero)
            {
                Console.WriteLine("[Window] Could not find LEGO Racers window.");
                return;
            }

            Console.WriteLine($"[Window] Window found (hwnd=0x{gameWnd:X}). Waiting {SETTLE_MS/1000}s for full load...");
            Thread.Sleep(SETTLE_MS);

            HideGhostWindows(gameProcess, gameWnd);

            // ── 3. Calculate target position (centered on screen) ────────────
            int screenW = Win32.GetSystemMetrics(Win32.SM_CXSCREEN);
            int screenH = Win32.GetSystemMetrics(Win32.SM_CYSCREEN);
            int targetX = (screenW - TARGET_W) / 2;
            int targetY = (screenH - TARGET_H) / 2;

            Console.WriteLine($"[Window] Screen: {screenW}x{screenH}, target: {TARGET_W}x{TARGET_H} at ({targetX},{targetY})");
            Console.WriteLine("[Window] Starting continuous windowed-mode enforcement...");

            long desiredStyle = DesiredStyle;
            int enforceCount  = 0;
            int ghostCheckTick = 0;

            // ── 4. Continuous enforcement loop ───────────────────────────────
            while (!gameProcess.HasExited)
            {
                if (gameProcess.HasExited) break;

                long curStyle = Win32.GetWindowLong(gameWnd, Win32.GWL_STYLE);
                Win32.GetWindowRect(gameWnd, out Win32.RECT rect);
                int curW = rect.Right - rect.Left;
                int curH = rect.Bottom - rect.Top;

                bool styleWrong = (curStyle != desiredStyle);
                bool sizeWrong  = (curW != TARGET_W || curH != TARGET_H);

                if (styleWrong || sizeWrong)
                {
                    enforceCount++;

                    // Re-apply style if needed
                    if (styleWrong)
                        Win32.SetWindowLong(gameWnd, Win32.GWL_STYLE, desiredStyle);

                    // Re-apply position and size
                    Win32.SetWindowPos(gameWnd, IntPtr.Zero,
                        targetX, targetY, TARGET_W, TARGET_H,
                        Win32.SWP_FRAMECHANGED | Win32.SWP_SHOWWINDOW | Win32.SWP_NOZORDER | Win32.SWP_NOACTIVATE);

                    Console.WriteLine($"[Window] Enforce #{enforceCount}: style={curStyle:X8}->{desiredStyle:X8}, " +
                                      $"size={curW}x{curH}->{TARGET_W}x{TARGET_H}");
                }

                Thread.Sleep(CHECK_MS);
                ghostCheckTick++;
                if (ghostCheckTick % 1 == 0)  // every tick (~500ms)
                    HideGhostWindows(gameProcess, gameWnd);
            }

            Console.WriteLine("[Window] Game exited. Window manager stopping.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[Window] Error: {ex.Message}\n{ex.StackTrace}");
        }
    }
}

// ─── Remaster Manager ───────────────────────────────────────────────────────

static class RemasterManager
{
    // LEGO.JAM lives next to the game EXE. Swap between original and remastered.
    public static string GameDir => AppContext.BaseDirectory;
    public static string OriginalJam    => System.IO.Path.Combine(GameDir, "LEGO.JAM");
    public static string RemasteredJam  => System.IO.Path.Combine(GameDir, "LEGO_REMASTERED.JAM");
    public static string BackupJam      => System.IO.Path.Combine(GameDir, "LEGO_ORIGINAL.JAM");
    public static string StatePath      => System.IO.Path.Combine(GameDir, ".remaster_state");

    public static bool IsRemasteredAvailable()
        => System.IO.File.Exists(RemasteredJam);

    public static bool IsRemasteredActive()
    {
        if (!System.IO.File.Exists(StatePath)) return false;
        return System.IO.File.ReadAllText(StatePath).Trim() == "remastered";
    }

    public static bool ActivateRemastered()
    {
        try
        {
            if (!IsRemasteredAvailable())
            {
                Console.WriteLine("[Remaster] LEGO_REMASTERED.JAM not found. Run the remaster pipeline first.");
                return false;
            }
            if (!System.IO.File.Exists(BackupJam) && System.IO.File.Exists(OriginalJam))
                System.IO.File.Copy(OriginalJam, BackupJam);
            System.IO.File.Copy(RemasteredJam, OriginalJam, overwrite: true);
            System.IO.File.WriteAllText(StatePath, "remastered");
            Console.WriteLine("[Remaster] Remastered textures ACTIVE.");
            return true;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[Remaster] Failed to activate: {ex.Message}");
            return false;
        }
    }

    public static void RestoreOriginal()
    {
        try
        {
            if (System.IO.File.Exists(BackupJam))
            {
                System.IO.File.Copy(BackupJam, OriginalJam, overwrite: true);
                Console.WriteLine("[Remaster] Original textures restored.");
            }
            System.IO.File.WriteAllText(StatePath, "original");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[Remaster] Failed to restore: {ex.Message}");
        }
    }

    public static void ShowMenu()
    {
        Console.WriteLine();
        Console.WriteLine("╔══════════════════════════════════════╗");
        Console.WriteLine("║        LEGO Racers Launcher          ║");
        Console.WriteLine("╠══════════════════════════════════════╣");

        bool remasterAvail  = IsRemasteredAvailable();
        bool remasterActive = IsRemasteredActive();

        if (!remasterAvail)
        {
            Console.WriteLine("║  Textures: Original                  ║");
            Console.WriteLine("║  (Remastered pack not installed)     ║");
        }
        else if (remasterActive)
        {
            Console.WriteLine("║  Textures: ★ REMASTERED ★            ║");
        }
        else
        {
            Console.WriteLine("║  Textures: Original                  ║");
        }

        Console.WriteLine("╠══════════════════════════════════════╣");
        Console.WriteLine("║  [Enter]  Launch game                ║");

        if (remasterAvail)
        {
            if (remasterActive)
                Console.WriteLine("║  [O]      Switch to Original         ║");
            else
                Console.WriteLine("║  [R]      Switch to Remastered       ║");
        }

        Console.WriteLine("║  [Esc]    Exit                       ║");
        Console.WriteLine("╚══════════════════════════════════════╝");
        Console.Write("> ");

        while (true)
        {
            var key = Console.ReadKey(intercept: true);
            if (key.Key == ConsoleKey.Enter)
            {
                Console.WriteLine();
                return;  // proceed to launch
            }
            if (key.Key == ConsoleKey.Escape)
            {
                Console.WriteLine("\nExiting.");
                System.Environment.Exit(0);
            }
            if (remasterAvail && (key.KeyChar == 'r' || key.KeyChar == 'R') && !remasterActive)
            {
                Console.WriteLine();
                ActivateRemastered();
                ShowMenu();
                return;
            }
            if (remasterAvail && (key.KeyChar == 'o' || key.KeyChar == 'O') && remasterActive)
            {
                Console.WriteLine();
                RestoreOriginal();
                ShowMenu();
                return;
            }
        }
    }
}

class Program
{
    static void Main()
    {
        Console.Title = "LEGO Racers Launcher";
        Console.WriteLine("LEGO Racers Win11 Compatibility Layer");

        // Show launcher menu (texture toggle + launch)
        RemasterManager.ShowMenu();

        // Launch the game executable from the same folder as this launcher
        string gameDir = AppContext.BaseDirectory;
        string gameExe = System.IO.Path.Combine(gameDir, "LEGORacers.exe");

        if (!System.IO.File.Exists(gameExe))
        {
            Console.WriteLine($"\nERROR: Could not find LEGORacers.exe at:\n  {gameExe}");
            Console.WriteLine("Make sure LegoController.exe is in the same folder as LEGORacers.exe.");
            Console.WriteLine("\nPress any key to exit.");
            Console.ReadKey(true);
            return;
        }

        Console.WriteLine("\nLaunching LEGO Racers...");
        Console.WriteLine("Xbox controller active. Press F10 in-game for online multiplayer.\n");

        var gameProc = new Process();
        gameProc.StartInfo.FileName         = gameExe;
        gameProc.StartInfo.WorkingDirectory = gameDir;
        gameProc.StartInfo.UseShellExecute  = true;
        gameProc.Start();

        // Start window manager in background
        // Window manager disabled - game runs fullscreen, dgVoodoo handles presentation

        bool wasConnected   = false;
        bool wasGameActive  = false;

        while (!gameProc.HasExited)
        {
            Thread.Sleep(8); // ~120Hz polling

            var result = Win32.XInputGetState(0, out var state);
            bool controllerConnected = (result == 0);

            if (controllerConnected != wasConnected)
            {
                wasConnected = controllerConnected;
                Console.WriteLine(controllerConnected
                    ? "[Controller] Xbox controller connected!"
                    : "[Controller] Xbox controller disconnected.");
                if (!controllerConnected) KeySender.ReleaseAll();
            }

            if (!controllerConnected) continue;

            // Check if LEGO Racers is the foreground window
            IntPtr fgWnd = Win32.GetForegroundWindow();
            Win32.GetWindowThreadProcessId(fgWnd, out uint fgPid);

            bool gameActive = false;
            foreach (var proc in Process.GetProcessesByName("LEGORacers"))
            {
                if ((uint)proc.Id == fgPid) { gameActive = true; break; }
            }

            if (gameActive != wasGameActive)
            {
                wasGameActive = gameActive;
                if (!gameActive)
                {
                    KeySender.ReleaseAll();
                    Console.WriteLine("[Input] Game not focused - keys released.");
                }
                else
                {
                    Console.WriteLine("[Input] Game focused - controller active!");
                }
            }

            if (!gameActive) continue;

            var gp = state.Gamepad;

            bool steerLeft  = gp.sThumbLX < -Win32.STICK_DEADZONE
                           || (gp.wButtons & Win32.GAMEPAD_DPAD_LEFT) != 0;
            bool steerRight = gp.sThumbLX >  Win32.STICK_DEADZONE
                           || (gp.wButtons & Win32.GAMEPAD_DPAD_RIGHT) != 0;
            bool accel      = gp.bRightTrigger > Win32.TRIGGER_THRESHOLD
                           || gp.sThumbLY >  Win32.STICK_DEADZONE
                           || (gp.wButtons & Win32.GAMEPAD_DPAD_UP) != 0;
            bool brake      = gp.bLeftTrigger > Win32.TRIGGER_THRESHOLD
                           || gp.sThumbLY < -Win32.STICK_DEADZONE
                           || (gp.wButtons & Win32.GAMEPAD_DPAD_DOWN) != 0;

            bool shootWeapon = (gp.wButtons & Win32.GAMEPAD_A)     != 0;
            bool pauseGame   = (gp.wButtons & Win32.GAMEPAD_START) != 0;
            bool spaceAction = (gp.wButtons & Win32.GAMEPAD_B)     != 0;

            if (steerLeft  && steerRight) { steerLeft = false; steerRight = false; }
            if (accel      && brake)      { brake = false; }

            if (steerLeft)   KeySender.Press(DI.A);     else KeySender.Release(DI.A);
            if (steerRight)  KeySender.Press(DI.D);     else KeySender.Release(DI.D);
            if (accel)       KeySender.Press(DI.W);     else KeySender.Release(DI.W);
            if (brake)       KeySender.Press(DI.S);     else KeySender.Release(DI.S);
            if (shootWeapon) KeySender.Press(DI.ENTER); else KeySender.Release(DI.ENTER);
            if (pauseGame)   KeySender.Press(DI.ESC);   else KeySender.Release(DI.ESC);
            if (spaceAction) KeySender.Press(DI.SPACE);  else KeySender.Release(DI.SPACE);
        }
    }
}
