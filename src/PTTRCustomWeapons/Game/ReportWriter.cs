using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using UnityEngine;

namespace PTTRCustomWeapons.Game
{
    /// <summary>
    /// Writes _GameReport.txt: the game classes the mod found, plus every game class whose name looks
    /// weapon/enemy/shop related with its members. This is what you need to fix _GameHooks.cfg after a
    /// game update, without decompiling anything.
    /// </summary>
    internal static class ReportWriter
    {
        private static readonly Regex Interesting = new Regex(
            "weapon|gun|melee|blade|bow|ammo|bullet|projectile|enemy|npc|player|shop|store|vendor|merchant|price|damage|health|hit|limb|body|pickup|item|loot|spawn|beneath|floor|dungeon|level|room",
            RegexOptions.IgnoreCase | RegexOptions.Compiled);

        public static void Write(string folder, GameBridge game, WeaponSkinner skinner, string scene)
        {
            var sb = new StringBuilder();
            sb.AppendLine("Paint the Town Red - Custom Weapons: game report");
            sb.AppendLine($"Written {DateTime.Now}. Current scene: \"{scene}\"");
            sb.AppendLine("Use the class and member names below to adjust _GameHooks.cfg if something is not detected.");
            sb.AppendLine();
            sb.AppendLine("== Resolved");
            sb.AppendLine($"Player class : {game.PlayerType?.FullName ?? "NOT FOUND"}");
            sb.AppendLine($"Enemy class  : {game.EnemyType?.FullName ?? "NOT FOUND"}");
            sb.AppendLine($"Weapon class : {game.WeaponType?.FullName ?? "NOT FOUND"}");
            sb.AppendLine($"Shop classes : {string.Join(", ", game.ShopTypes.Select(t => t.FullName))}");
            sb.AppendLine($"Hit parts    : {string.Join(", ", game.HitPartTypes.Select(t => t.FullName))}");
            try
            {
                sb.AppendLine($"Players in scene: {GameBridge.FindAll(game.PlayerType).Count}, enemies: {GameBridge.FindAll(game.EnemyType).Count}, weapons: {GameBridge.FindAll(game.WeaponType).Count}");
                var names = GameBridge.FindAllIncludingAssets(game.WeaponType).Select(w => w.gameObject.name).Distinct().OrderBy(n => n).ToList();
                sb.AppendLine($"Known weapon objects ({names.Count}) - usable as 'Base Weapon':");
                foreach (var n in names) sb.AppendLine("    " + n);
            }
            catch (Exception ex) { sb.AppendLine("(could not count objects: " + ex.Message + ")"); }
            sb.AppendLine($"Custom weapons alive: {skinner.All.Count()}");
            sb.AppendLine();
            sb.AppendLine("== Game assemblies");
            foreach (var a in Reflect.GameAssemblies) sb.AppendLine("  " + a.GetName().Name);
            sb.AppendLine();
            sb.AppendLine("== Interesting game classes");

            foreach (var t in Reflect.AllGameTypes().Where(t => Interesting.IsMatch(t.Name)).OrderBy(t => t.FullName))
            {
                sb.AppendLine();
                sb.AppendLine($"class {t.FullName} : {t.BaseType?.Name}");
                const BindingFlags f = BindingFlags.Public | BindingFlags.Instance | BindingFlags.DeclaredOnly;
                try
                {
                    foreach (var p in t.GetProperties(f).Where(p => p.GetIndexParameters().Length == 0))
                        sb.AppendLine($"    {Reflect.Describe(p.PropertyType)} {p.Name}{(p.CanWrite ? "" : " (read-only)")}");
                    foreach (var m in t.GetMethods(f).Where(m => !m.IsSpecialName))
                        sb.AppendLine($"    {Reflect.Describe(m.ReturnType)} {m.Name}({string.Join(", ", m.GetParameters().Select(p => Reflect.Describe(p.ParameterType) + " " + p.Name))})");
                    foreach (var e in t.IsEnum ? Enum.GetNames(t) : Array.Empty<string>()) sb.AppendLine("    = " + e);
                }
                catch (Exception ex) { sb.AppendLine("    (error: " + ex.Message + ")"); }
            }

            try
            {
                File.WriteAllText(Path.Combine(folder, "_GameReport.txt"), sb.ToString());
                Log.Info("Wrote CustomWeapons/_GameReport.txt");
            }
            catch (Exception ex) { Log.Warn("Could not write _GameReport.txt: " + ex.Message); }
        }
    }

    internal sealed class Hud
    {
        private string _toast = "";
        private float _toastUntil;

        private float _pendingSeconds;

        /// <summary>Shows a message in the top-left corner (the timer starts on the next frame drawn).</summary>
        public void Toast(string text, float seconds = 4f)
        {
            _toast = text;
            _pendingSeconds = seconds;
        }

        private static Rect R(float x, float y, float w, float h) => new Rect { m_XMin = x, m_YMin = y, m_Width = w, m_Height = h };

        public void Draw(CustomInstance? held)
        {
            try
            {
                if (_pendingSeconds > 0) { _toastUntil = Time.time + _pendingSeconds; _pendingSeconds = 0; }
                int sw = Screen.width, sh = Screen.height;
                if (Time.time < _toastUntil) Shadowed(R(20, 20, 900, 30), _toast);
                if (held == null || !held.Def.IsRanged) return;
                var def = held.Def;
                string ammo = def.InfiniteMagazine ? "inf" :
                    held.Reloading ? "reloading..." :
                    $"{held.Magazine} / {(held.Reserve == int.MaxValue ? "inf" : held.Reserve.ToString())}";
                Shadowed(R(sw - 320, sh - 70, 300, 25), def.Name);
                Shadowed(R(sw - 320, sh - 45, 300, 25), ammo);
            }
            catch (Exception ex) { Log.WarnOnce("hud", "HUD disabled: " + ex.Message); }
        }

        private static void Shadowed(Rect r, string text)
        {
            var white = new Color { r = 1, g = 1, b = 1, a = 1 };
            var black = new Color { r = 0, g = 0, b = 0, a = 0.8f };
            GUI.color = black;
            GUI.Label(R(r.m_XMin + 1, r.m_YMin + 1, r.m_Width, r.m_Height), text);
            GUI.color = white;
            GUI.Label(r, text);
        }
    }
}
