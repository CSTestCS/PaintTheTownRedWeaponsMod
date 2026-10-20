using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Principal;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace PTTRCustomWeapons.Installer
{
    /// <summary>
    /// Installs the Custom Weapons mod into the Steam copy of Paint the Town Red:
    ///   1. finds the game through Steam (or asks for the folder / takes one dragged onto the exe)
    ///   2. puts the mod (a small version.dll) next to the game's exe - no mod loader needed
    ///   3. copies the example weapons into CustomWeapons\
    /// An older MelonLoader-based install is removed. Run with "uninstall" to remove the mod.
    /// </summary>
    internal static class Program
    {
        private const string SteamAppId = "337320";
        private const string DefaultFolderName = "Paint the Town Red";
        private const string Marker = "PTTR_CUSTOM_WEAPONS_NATIVE";
        private const string OldModFile = "PTTRCustomWeapons.dll";

        private static int Main(string[] args)
        {
            Console.Title = "Paint the Town Red - Custom Weapons installer";
            Header();
            try
            {
                bool uninstall = args.Any(a => a.Trim('-', '/').Equals("uninstall", StringComparison.OrdinalIgnoreCase));
                bool quiet = args.Any(a => a.Trim('-', '/').Equals("quiet", StringComparison.OrdinalIgnoreCase));
                var explicitDir = args.FirstOrDefault(a => Directory.Exists(a.Trim('"')))?.Trim('"');

                var gameDir = explicitDir != null && IsGameDir(explicitDir) ? explicitDir : FindGame();
                if (gameDir == null) gameDir = AskForGame();
                if (gameDir == null) return Fail("Paint the Town Red was not found.", quiet);
                Info($"Game folder: {gameDir}");

                if (IsGameRunning())
                {
                    Warn("Paint the Town Red is running. Close the game, then press Enter.");
                    if (!quiet) Console.ReadLine();
                }

                if (!uninstall && !quiet)
                {
                    Console.WriteLine();
                    Console.WriteLine("  [1] Install / update the mod   (default - just press Enter)");
                    Console.WriteLine("  [2] Uninstall the mod (keeps your CustomWeapons folder)");
                    Console.Write("> ");
                    var choice = (Console.ReadLine() ?? "").Trim();
                    if (choice == "2") return Uninstall(gameDir, quiet);
                }
                else if (uninstall) return Uninstall(gameDir, quiet);

                return Install(gameDir, quiet);
            }
            catch (UnauthorizedAccessException)
            {
                if (!IsAdmin() && RelaunchAsAdmin(args)) return 0;
                return Fail("Windows refused access to the game folder. Right-click the installer and choose 'Run as administrator'.", false);
            }
            catch (IOException ex) when (ex.HResult == unchecked((int)0x80070020))
            {
                return Fail("A game file is in use. Close Paint the Town Red and run the installer again.", false);
            }
            catch (Exception ex)
            {
                return Fail(ex.ToString(), false);
            }
        }

        // ------------------------------------------------------------------ install

        private static bool IsOurDll(string path)
        {
            if (!File.Exists(path)) return false;
            var bytes = File.ReadAllBytes(path);
            var marker = System.Text.Encoding.ASCII.GetBytes(Marker);
            for (int i = 0; i + marker.Length <= bytes.Length; i++)
            {
                int k = 0;
                while (k < marker.Length && bytes[i + k] == marker[k]) k++;
                if (k == marker.Length) return true;
            }
            return false;
        }

        private static bool Ask(string question, bool quiet)
        {
            if (quiet) return true;
            Console.Write(question + " [Y/n] ");
            var a = (Console.ReadLine() ?? "").Trim();
            return a.Length == 0 || a.StartsWith("y", StringComparison.OrdinalIgnoreCase);
        }

        private static int Install(string gameDir, bool quiet)
        {
            var dll = Path.Combine(gameDir, "version.dll");
            var melon = Path.Combine(gameDir, "MelonLoader");

            // 1. an earlier install that used MelonLoader, or another tool's version.dll
            if (Directory.Exists(melon))
            {
                Warn("MelonLoader is installed. This version of the mod does not need it, and both use version.dll.");
                if (!Ask("Remove MelonLoader now? (Other MelonLoader mods will stop working)", quiet))
                    return Fail("Installation cancelled - MelonLoader was left in place.", quiet);
                RemoveMelonLoader(gameDir);
                Ok("Removed MelonLoader.");
            }
            else if (File.Exists(dll) && !IsOurDll(dll))
            {
                Warn("The game folder already has a version.dll from another tool.");
                if (!Ask("Replace it? (it is kept as version.dll.bak)", quiet))
                    return Fail("Installation cancelled.", quiet);
                File.Copy(dll, dll + ".bak", true);
            }

            // 2. the mod
            WriteResource("payload/version.dll", dll, overwrite: true);
            Ok("Mod installed: version.dll");

            // 3. weapon folder + examples (never overwrites the player's own files)
            var weapons = Path.Combine(gameDir, "CustomWeapons");
            Directory.CreateDirectory(weapons);
            int added = 0;
            foreach (var res in Assembly.GetExecutingAssembly().GetManifestResourceNames().Where(r => r.StartsWith("payload/CustomWeapons/")))
            {
                var name = res.Substring("payload/CustomWeapons/".Length);
                bool alwaysUpdate = name.Equals("TEMPLATE.txt", StringComparison.OrdinalIgnoreCase) || name.Equals("README.txt", StringComparison.OrdinalIgnoreCase);
                if (WriteResource(res, Path.Combine(weapons, name), overwrite: alwaysUpdate)) added++;
            }
            Ok($"Weapon folder ready: {weapons} ({added} file(s) added)");

            Console.WriteLine();
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("  All done! Start the game from Steam as usual.");
            Console.ResetColor();
            Console.WriteLine();
            Console.WriteLine("  * Add weapons: put  MyWeapon.fbx  +  MyWeapon.txt  (+ MyWeapon.png) into:");
            Console.WriteLine("      " + weapons);
            Console.WriteLine("    Copy TEMPLATE.txt to start a new .txt - every setting is explained inside.");
            Console.WriteLine("  * In a level: F8 = spawn all custom weapons in front of you, F9 = reload weapon files,");
            Console.WriteLine("    F10 = write a report about the game's classes (for troubleshooting).");
            Console.WriteLine("  * What happened is logged in CustomWeapons\\_CustomWeapons.log");
            Console.WriteLine();

            if (!quiet && Ask("Open the CustomWeapons folder now?", false))
                try { Process.Start("explorer.exe", "\"" + weapons + "\""); } catch { }
            return 0;
        }

        private static void RemoveMelonLoader(string gameDir)
        {
            var proxy = Path.Combine(gameDir, "version.dll");
            if (File.Exists(proxy) && !IsOurDll(proxy)) File.Delete(proxy);
            var ml = Path.Combine(gameDir, "MelonLoader");
            if (Directory.Exists(ml)) Directory.Delete(ml, true);
            var oldMod = Path.Combine(gameDir, "Mods", OldModFile);
            if (File.Exists(oldMod)) File.Delete(oldMod);
            foreach (var d in new[] { "Mods", "Plugins", "UserLibs", "UserData" })
            {
                var p = Path.Combine(gameDir, d);
                if (Directory.Exists(p) && !Directory.EnumerateFileSystemEntries(p).Any()) Directory.Delete(p);
            }
        }

        private static bool WriteResource(string resource, string path, bool overwrite)
        {
            if (!overwrite && File.Exists(path)) return false;
            using var s = Assembly.GetExecutingAssembly().GetManifestResourceStream(resource)
                          ?? throw new InvalidOperationException("Installer is missing " + resource);
            using var f = File.Create(path);
            s.CopyTo(f);
            return true;
        }

        // ------------------------------------------------------------------ uninstall

        private static int Uninstall(string gameDir, bool quiet)
        {
            var dll = Path.Combine(gameDir, "version.dll");
            if (IsOurDll(dll))
            {
                File.Delete(dll);
                Ok("Removed version.dll");
                var bak = dll + ".bak";
                if (File.Exists(bak)) { File.Move(bak, dll); Ok("Restored the previous version.dll"); }
            }
            else Info("The mod was not installed.");
            if (Directory.Exists(Path.Combine(gameDir, "MelonLoader")) && Ask("Also remove MelonLoader (from an older version of this mod)?", quiet))
            {
                RemoveMelonLoader(gameDir);
                Ok("Removed MelonLoader.");
            }
            Info("Your CustomWeapons folder was kept: " + Path.Combine(gameDir, "CustomWeapons"));
            if (!quiet) { Console.WriteLine("Press Enter to close."); Console.ReadLine(); }
            return 0;
        }

        // ------------------------------------------------------------------ finding the game

        private static bool IsGameDir(string dir) =>
            Directory.Exists(dir) && File.Exists(Path.Combine(dir, "GameAssembly.dll")) && Directory.GetFiles(dir, "*.exe").Length > 0;

        private static string? FindGame()
        {
            foreach (var lib in SteamLibraries())
            {
                var steamapps = Path.Combine(lib, "steamapps");
                var manifest = Path.Combine(steamapps, $"appmanifest_{SteamAppId}.acf");
                var folder = DefaultFolderName;
                if (File.Exists(manifest))
                {
                    var m = Regex.Match(File.ReadAllText(manifest), "\"installdir\"\\s+\"([^\"]+)\"", RegexOptions.IgnoreCase);
                    if (m.Success) folder = m.Groups[1].Value;
                }
                var dir = Path.Combine(steamapps, "common", folder);
                if (IsGameDir(dir)) return dir;
            }
            return null;
        }

        private static IEnumerable<string> SteamLibraries()
        {
            var roots = new List<string>();
            void AddRoot(string? p) { if (!string.IsNullOrWhiteSpace(p) && Directory.Exists(p) && !roots.Contains(p!, StringComparer.OrdinalIgnoreCase)) roots.Add(p!.Replace('/', '\\')); }
            try { AddRoot(Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) as string); } catch { }
            try { AddRoot(Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath", null) as string); } catch { }
            try { AddRoot(Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam", "InstallPath", null) as string); } catch { }
            AddRoot(@"C:\Program Files (x86)\Steam");
            AddRoot(@"C:\Program Files\Steam");

            var libs = new List<string>(roots);
            foreach (var root in roots)
            {
                var vdf = Path.Combine(root, "steamapps", "libraryfolders.vdf");
                if (!File.Exists(vdf)) continue;
                foreach (Match m in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s+\"([^\"]+)\"", RegexOptions.IgnoreCase))
                {
                    var p = m.Groups[1].Value.Replace(@"\\", @"\");
                    if (Directory.Exists(p) && !libs.Contains(p, StringComparer.OrdinalIgnoreCase)) libs.Add(p);
                }
            }
            // other drives' default library folders
            foreach (var drive in DriveInfo.GetDrives().Where(d => d.DriveType == DriveType.Fixed))
                foreach (var name in new[] { "SteamLibrary", "Steam", @"Games\Steam", @"Games\SteamLibrary" })
                {
                    var p = Path.Combine(drive.Name, name);
                    if (Directory.Exists(p) && !libs.Contains(p, StringComparer.OrdinalIgnoreCase)) libs.Add(p);
                }
            return libs;
        }

        private static string? AskForGame()
        {
            Warn("Could not find Paint the Town Red automatically.");
            Console.WriteLine("In Steam: right-click the game > Manage > Browse local files, copy that folder's path");
            Console.WriteLine("and paste it here (or drag the folder onto this window), then press Enter:");
            for (int tries = 0; tries < 5; tries++)
            {
                Console.Write("> ");
                var p = (Console.ReadLine() ?? "").Trim().Trim('"');
                if (p.Length == 0) return null;
                if (IsGameDir(p)) return p;
                Warn("That folder does not contain the game (GameAssembly.dll not found). Try again:");
            }
            return null;
        }

        private static bool IsGameRunning()
        {
            try { return Process.GetProcesses().Any(p => p.ProcessName.IndexOf("PaintTheTownRed", StringComparison.OrdinalIgnoreCase) >= 0); }
            catch { return false; }
        }

        private static bool IsAdmin()
        {
            try { return new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator); }
            catch { return false; }
        }

        private static bool RelaunchAsAdmin(string[] args)
        {
            try
            {
                var exe = Process.GetCurrentProcess().MainModule!.FileName;
                Process.Start(new ProcessStartInfo(exe, string.Join(" ", args.Select(a => "\"" + a + "\""))) { Verb = "runas", UseShellExecute = true });
                return true;
            }
            catch { return false; }
        }

        // ------------------------------------------------------------------ console helpers

        private static void Header()
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine("==============================================================");
            Console.WriteLine("   PAINT THE TOWN RED  -  CUSTOM WEAPONS  -  installer v2.0");
            Console.WriteLine("==============================================================");
            Console.ResetColor();
        }

        private static void Info(string s) => Console.WriteLine("  " + s);
        private static void Ok(string s) { Console.ForegroundColor = ConsoleColor.Green; Console.WriteLine("  OK  " + s); Console.ResetColor(); }
        private static void Warn(string s) { Console.ForegroundColor = ConsoleColor.Yellow; Console.WriteLine("  !   " + s); Console.ResetColor(); }

        private static int Fail(string s, bool quiet)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine("  ERROR: " + s);
            Console.ResetColor();
            if (!quiet) { Console.WriteLine("Press Enter to close."); Console.ReadLine(); }
            return 1;
        }
    }
}
