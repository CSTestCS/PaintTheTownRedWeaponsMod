using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using Il2CppInterop.Runtime;
using Il2CppInterop.Runtime.InteropTypes;
using MelonLoader;
using NV3 = System.Numerics.Vector3;
using NV4 = System.Numerics.Vector4;
using NQ = System.Numerics.Quaternion;

namespace PTTRCustomWeapons.Game
{
    internal static class Log
    {
        private static MelonLogger.Instance? _logger;
        private static string? _file;
        private static readonly HashSet<string> Once = new HashSet<string>();

        public static void Init(MelonLogger.Instance logger, string folder)
        {
            _logger = logger;
            _file = Path.Combine(folder, "_CustomWeapons.log");
            try { File.WriteAllText(_file, $"Custom Weapons log - {DateTime.Now}\n"); } catch { _file = null; }
        }

        private static void ToFile(string level, string msg)
        {
            if (_file == null) return;
            try { File.AppendAllText(_file, $"[{DateTime.Now:HH:mm:ss}] {level} {msg}\n"); } catch { }
        }

        public static void Info(string msg) { _logger?.Msg(msg); ToFile("INFO ", msg); }
        public static void Warn(string msg) { _logger?.Warning(msg); ToFile("WARN ", msg); }
        public static void Error(string msg) { _logger?.Error(msg); ToFile("ERROR", msg); }

        /// <summary>Logs a warning only the first time this key is seen (avoids per-frame spam).</summary>
        public static void WarnOnce(string key, string msg)
        {
            lock (Once) { if (!Once.Add(key)) return; }
            Warn(msg);
        }

        public static void InfoOnce(string key, string msg)
        {
            lock (Once) { if (!Once.Add(key)) return; }
            Info(msg);
        }
    }

    /// <summary>Conversions between System.Numerics (used by the core code) and Unity types.</summary>
    internal static class U
    {
        public static UnityEngine.Vector3 V(NV3 v) => new UnityEngine.Vector3 { x = v.X, y = v.Y, z = v.Z };
        public static UnityEngine.Vector3 V(float x, float y, float z) => new UnityEngine.Vector3 { x = x, y = y, z = z };
        public static NV3 N(UnityEngine.Vector3 v) => new NV3(v.x, v.y, v.z);
        public static UnityEngine.Quaternion Q(NQ q) => new UnityEngine.Quaternion { x = q.X, y = q.Y, z = q.Z, w = q.W };
        public static NQ N(UnityEngine.Quaternion q) => new NQ(q.x, q.y, q.z, q.w);
        public static UnityEngine.Color C(NV4 c) => new UnityEngine.Color { r = c.X, g = c.Y, b = c.Z, a = c.W };

        /// <summary>True when the wrapper exists and the Unity object has not been destroyed.</summary>
        public static bool Alive(UnityEngine.Object? o)
        {
            if (ReferenceEquals(o, null)) return false;
            try { return !o.WasCollected && o.Pointer != IntPtr.Zero && (bool)o; }
            catch { return false; }
        }

        public static Il2CppSystem.Type T<T>() where T : Il2CppObjectBase => Il2CppType.Of<T>();

        public static Il2CppSystem.Type? T(Type managed)
        {
            try { return Il2CppType.From(managed, false); } catch { return null; }
        }

        public static string GameRoot =>
            Path.GetDirectoryName(Environment.ProcessPath ?? Process.GetCurrentProcess().MainModule?.FileName ?? ".") ?? ".";
    }

    /// <summary>Keyboard / mouse state read straight from Windows, so it works with any Unity input system.</summary>
    internal static class Win32Input
    {
        [DllImport("user32.dll")] private static extern short GetAsyncKeyState(int vKey);
        [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        private static readonly uint Pid = (uint)Environment.ProcessId;
        private static readonly Dictionary<int, bool> Prev = new Dictionary<int, bool>();
        private static readonly Dictionary<int, bool> Now = new Dictionary<int, bool>();

        public static bool GameHasFocus()
        {
            try
            {
                GetWindowThreadProcessId(GetForegroundWindow(), out var pid);
                return pid == Pid;
            }
            catch { return true; }
        }

        /// <summary>Call once per frame before using Down/Pressed.</summary>
        public static void Poll()
        {
            Prev.Clear();
            foreach (var kv in Now) Prev[kv.Key] = kv.Value;
            var keys = new List<int>(Now.Keys);
            foreach (var k in keys) Now[k] = Raw(k);
        }

        private static bool Raw(int vk)
        {
            try { return (GetAsyncKeyState(vk) & 0x8000) != 0; } catch { return false; }
        }

        public static bool Down(int vk)
        {
            if (vk <= 0) return false;
            if (!Now.ContainsKey(vk)) Now[vk] = Raw(vk);
            return Now[vk];
        }

        public static bool Pressed(int vk)
        {
            bool down = Down(vk);
            Prev.TryGetValue(vk, out var before);
            return down && !before;
        }

        public static int KeyCode(string name)
        {
            if (string.IsNullOrWhiteSpace(name)) return 0;
            var n = name.Trim().ToUpperInvariant().Replace(" ", "");
            switch (n)
            {
                case "MOUSE0": case "LMB": case "LEFTMOUSE": case "MOUSELEFT": return 0x01;
                case "MOUSE1": case "RMB": case "RIGHTMOUSE": case "MOUSERIGHT": return 0x02;
                case "MOUSE2": case "MMB": case "MIDDLEMOUSE": return 0x04;
                case "MOUSE3": return 0x05;
                case "MOUSE4": return 0x06;
                case "SPACE": return 0x20;
                case "TAB": return 0x09;
                case "ENTER": case "RETURN": return 0x0D;
                case "LEFTSHIFT": case "SHIFT": return 0xA0;
                case "LEFTCONTROL": case "CTRL": case "CONTROL": return 0xA2;
                case "LEFTALT": case "ALT": return 0xA4;
                case "BACKQUOTE": case "TILDE": return 0xC0;
                case "NONE": return 0;
            }
            if (n.Length == 1 && n[0] >= 'A' && n[0] <= 'Z') return n[0];
            if (n.Length == 1 && n[0] >= '0' && n[0] <= '9') return n[0];
            if (n.StartsWith("ALPHA") && n.Length == 6 && char.IsDigit(n[5])) return n[5];
            if (n.StartsWith("F") && int.TryParse(n.Substring(1), out var f) && f >= 1 && f <= 24) return 0x70 + f - 1;
            if (n.StartsWith("0X") && int.TryParse(n.Substring(2), System.Globalization.NumberStyles.HexNumber, null, out var hex)) return hex;
            return 0;
        }
    }
}
