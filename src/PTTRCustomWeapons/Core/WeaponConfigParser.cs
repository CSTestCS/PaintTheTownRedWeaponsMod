using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Text.RegularExpressions;

namespace PTTRCustomWeapons.Core
{
    /// <summary>
    /// Parses a weapon .txt file. Supported layouts:
    ///   "Key: Value" or "Key = Value" lines (any order, case/spacing insensitive, # or // comments), or
    ///   bare values, one per line, in the order of <see cref="PositionalOrder"/>.
    /// </summary>
    public static class WeaponConfigParser
    {
        /// <summary>Order used when the file contains only values (no "key:" labels).</summary>
        public static readonly string[] PositionalOrder =
        {
            "name", "meleeorranged", "magazinesize", "firetype", "projectilespeed", "projectiledrop",
            "automaticfirerate", "projectilebounce", "projectilevisual", "damage", "damagetype",
            "areaofeffect", "damageforce", "appearsatstartingarea",
        };

        private static readonly Dictionary<string, string> Aliases = new Dictionary<string, string>
        {
            ["name"] = "name", ["weaponname"] = "name", ["displayname"] = "name", ["title"] = "name",
            ["meleeorranged"] = "kind", ["type"] = "kind", ["weapontype"] = "kind", ["kind"] = "kind", ["class"] = "kind", ["category"] = "kind",
            ["magazinesize"] = "magazine", ["magazine"] = "magazine", ["magsize"] = "magazine", ["clipsize"] = "magazine", ["ammo"] = "magazine", ["clip"] = "magazine",
            ["reserveammo"] = "reserve", ["spareammo"] = "reserve", ["totalammo"] = "reserve", ["maxammo"] = "reserve",
            ["reloadtime"] = "reload", ["reload"] = "reload", ["reloadspeed"] = "reload",
            ["firetype"] = "firetype", ["firemode"] = "firetype", ["firingmode"] = "firetype", ["mode"] = "firetype",
            ["projectilespeed"] = "speed", ["bulletspeed"] = "speed", ["velocity"] = "speed", ["muzzlevelocity"] = "speed",
            ["projectiledrop"] = "drop", ["bulletdrop"] = "drop", ["gravity"] = "drop", ["drop"] = "drop",
            ["automaticfirerate"] = "firerate", ["firerate"] = "firerate", ["rateoffire"] = "firerate", ["rof"] = "firerate", ["shotspersecond"] = "firerate",
            ["projectilebounce"] = "bounce", ["projectilebounces"] = "bounce", ["bounce"] = "bounce", ["bounces"] = "bounce", ["ricochet"] = "bounce",
            ["bounciness"] = "bounciness", ["bouncespeed"] = "bounciness",
            ["projectilevisual"] = "visual", ["visual"] = "visual", ["projectilelook"] = "visual", ["projectile"] = "visual",
            ["projectilemodel"] = "projmodel",
            ["projectilesize"] = "projsize", ["bulletsize"] = "projsize",
            ["projectilecolor"] = "projcolor", ["projectilecolour"] = "projcolor", ["bulletcolor"] = "projcolor", ["bulletcolour"] = "projcolor",
            ["projectilelifetime"] = "projlife", ["projectilelife"] = "projlife", ["lifetime"] = "projlife",
            ["pierce"] = "pierce", ["penetration"] = "pierce", ["piercing"] = "pierce",
            ["pellets"] = "pellets", ["projectilespershot"] = "pellets", ["bulletspershot"] = "pellets",
            ["spread"] = "spread", ["accuracy"] = "spread", ["inaccuracy"] = "spread",
            ["burstcount"] = "burst", ["burstsize"] = "burst", ["burst"] = "burst",
            ["hitscan"] = "hitscan", ["instanthit"] = "hitscan",
            ["range"] = "range", ["maxrange"] = "range",
            ["recoil"] = "recoil",
            ["damage"] = "damage", ["dmg"] = "damage", ["power"] = "damage",
            ["damagetype"] = "damagetype", ["element"] = "damagetype",
            ["areaofeffect"] = "aoe", ["aoe"] = "aoe", ["explosionradius"] = "aoe", ["blastradius"] = "aoe", ["splashradius"] = "aoe", ["radius"] = "aoe",
            ["areafalloff"] = "falloff", ["aoefalloff"] = "falloff", ["falloff"] = "falloff",
            ["damageforce"] = "force", ["force"] = "force", ["knockback"] = "force", ["impactforce"] = "force",
            ["durability"] = "durability", ["hits"] = "durability", ["uses"] = "durability",
            ["unbreakable"] = "unbreakable", ["indestructible"] = "unbreakable",
            ["attackspeed"] = "attackspeed", ["swingspeed"] = "attackspeed",
            ["throwdamage"] = "throwdamage", ["throwndamage"] = "throwdamage",
            ["appearsatstartingarea"] = "start", ["startingarea"] = "start", ["appearsatstart"] = "start", ["spawnatstart"] = "start", ["startingweapon"] = "start",
            ["appearsonground"] = "ground", ["ground"] = "ground", ["spawnsonground"] = "ground",
            ["groundchance"] = "groundchance", ["groundspawnchance"] = "groundchance", ["spawnchance"] = "groundchance",
            ["appearsinshop"] = "shop", ["inshop"] = "shop", ["shop"] = "shop", ["sold"] = "shop",
            ["shopchance"] = "shopchance",
            ["shopprice"] = "price", ["price"] = "price", ["cost"] = "price",
            ["usedbyenemies"] = "enemies", ["enemies"] = "enemies", ["enemyuse"] = "enemies", ["enemiescanuse"] = "enemies",
            ["enemychance"] = "enemychance",
            ["spawnweight"] = "weight", ["weight"] = "weight", ["rarity"] = "weight",
            ["enemyfireinterval"] = "enemyinterval", ["enemyfiredelay"] = "enemyinterval",
            ["enemyaccuracy"] = "enemyspread", ["enemyspread"] = "enemyspread",
            ["enemydamagemultiplier"] = "enemydmg", ["enemydamage"] = "enemydmg",
            ["modelscale"] = "scale", ["scale"] = "scale", ["size"] = "scale",
            ["modelrotation"] = "rotation", ["rotation"] = "rotation",
            ["modeloffset"] = "offset", ["offset"] = "offset", ["position"] = "offset",
            ["autofit"] = "autofit", ["autoscale"] = "autofit",
            ["texture"] = "texture", ["image"] = "texture",
            ["tint"] = "tint", ["color"] = "tint", ["colour"] = "tint",
            ["baseweapon"] = "base", ["template"] = "base", ["basedon"] = "base", ["base"] = "base",
            ["description"] = "description", ["desc"] = "description",
        };

        private static string Normalize(string key)
        {
            var chars = key.ToLowerInvariant().Where(char.IsLetterOrDigit).ToArray();
            return new string(chars);
        }

        public static WeaponDefinition ParseFile(string txtPath)
        {
            var def = Parse(File.ReadAllText(txtPath), Path.GetFileNameWithoutExtension(txtPath));
            var dir = Path.GetDirectoryName(txtPath) ?? ".";
            ResolveFiles(def, dir);
            return def;
        }

        public static WeaponDefinition Parse(string text, string id)
        {
            var def = new WeaponDefinition { Id = id, Name = id };
            var pairs = new List<(string key, string value, int line)>();
            var bare = new List<(string value, int line)>();

            var lines = text.Replace("\r\n", "\n").Split('\n');
            for (int i = 0; i < lines.Length; i++)
            {
                var line = StripComment(lines[i]).Trim();
                if (line.Length == 0) continue;
                if (line.StartsWith("+")) line = line.Substring(1).Trim();
                int sep = FindSeparator(line);
                if (sep > 0)
                    pairs.Add((line.Substring(0, sep).Trim(), line.Substring(sep + 1).Trim(), i + 1));
                else
                    bare.Add((line, i + 1));
            }

            // A file with no labels at all: values in the documented order.
            if (pairs.Count == 0 && bare.Count > 0)
            {
                for (int i = 0; i < bare.Count && i < PositionalOrder.Length; i++)
                    pairs.Add((PositionalOrder[i], bare[i].value, bare[i].line));
                if (bare.Count > PositionalOrder.Length)
                    def.Warnings.Add($"{bare.Count - PositionalOrder.Length} extra unlabeled line(s) ignored.");
            }
            else
            {
                foreach (var b in bare) def.Warnings.Add($"Line {b.line}: no ':' or '=' found, ignored: \"{b.value}\"");
            }

            // Fire type first so explicit Pellets/Spread lines can override its presets.
            foreach (var p in pairs.OrderBy(p => Canonical(p.key) == "firetype" ? 0 : 1))
            {
                var canon = Canonical(p.key);
                if (canon == null)
                {
                    def.Warnings.Add($"Line {p.line}: unknown setting \"{p.key}\" ignored.");
                    continue;
                }
                try { Apply(def, canon, p.value); }
                catch (FormatException ex) { def.Warnings.Add($"Line {p.line}: {p.key}: {ex.Message}"); }
            }

            if (def.Name.Length == 0) def.Name = id;
            if (def.FireRate <= 0) def.FireRate = 1f;
            if (def.Pellets < 1) def.Pellets = 1;
            if (def.BurstCount < 1) def.BurstCount = 1;
            if (def.ProjectileSpeed <= 0 && def.IsRanged) def.Hitscan = true;
            return def;
        }

        private static string? Canonical(string key)
        {
            var n = Normalize(key);
            return Aliases.TryGetValue(n, out var c) ? c : null;
        }

        private static readonly Regex HexColorRx = new Regex("^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?(?![0-9a-zA-Z])", RegexOptions.Compiled);

        /// <summary>Removes "# comment" and "// comment" but keeps colours like #FF8800.</summary>
        private static string StripComment(string line)
        {
            for (int i = line.IndexOf('#'); i >= 0; i = line.IndexOf('#', i + 1))
            {
                if (HexColorRx.IsMatch(line.Substring(i))) continue;
                line = line.Substring(0, i);
                break;
            }
            int slashes = line.IndexOf("//", StringComparison.Ordinal);
            if (slashes >= 0) line = line.Substring(0, slashes);
            return line;
        }

        private static int FindSeparator(string line)
        {
            int colon = line.IndexOf(':');
            int eq = line.IndexOf('=');
            if (colon < 0) return eq;
            if (eq < 0) return colon;
            return Math.Min(colon, eq);
        }

        private static void Apply(WeaponDefinition d, string key, string v)
        {
            switch (key)
            {
                case "name": d.Name = v.Trim('"'); break;
                case "description": d.Description = v; break;
                case "kind": d.Kind = ParseKind(v); break;
                case "magazine":
                    if (IsInfinite(v)) d.MagazineSize = 0; else d.MagazineSize = Math.Max(0, (int)Num(v)); break;
                case "reserve":
                    d.ReserveAmmo = IsInfinite(v) ? -1 : Math.Max(0, (int)Num(v)); break;
                case "reload": d.ReloadTime = Math.Max(0, Num(v)); break;
                case "firetype": ApplyFireType(d, v); break;
                case "speed":
                    if (IsHitscanWord(v)) { d.Hitscan = true; d.ProjectileSpeed = 0; }
                    else { d.ProjectileSpeed = Math.Max(0, Num(v)); d.Hitscan = d.ProjectileSpeed <= 0; }
                    break;
                case "drop": d.ProjectileDrop = ParseDrop(v); break;
                case "firerate": d.FireRate = Math.Max(0.01f, Num(v)); break;
                case "bounce":
                {
                    var b = HasDigit(v) ? null : TryBool(v);
                    if (b.HasValue) d.ProjectileBounces = b.Value ? 3 : 0;
                    else d.ProjectileBounces = Math.Max(0, (int)Num(v));
                    break;
                }
                case "bounciness": d.Bounciness = Clamp01(Percent(v)); break;
                case "visual": ApplyVisual(d, v); break;
                case "projmodel": d.ProjectileModelPath = v.Trim('"'); d.ProjectileVisual = ProjectileVisual.Model; break;
                case "projsize": d.ProjectileSize = Math.Max(0.005f, Num(v)); break;
                case "projcolor": d.ProjectileColor = ParseColor(v); break;
                case "projlife": d.ProjectileLifetime = Math.Max(0.05f, Num(v)); break;
                case "pierce": d.Pierce = Math.Max(0, (int)Num(v)); break;
                case "pellets": d.Pellets = Math.Max(1, (int)Num(v)); break;
                case "spread": d.Spread = Math.Max(0, Num(v)); break;
                case "burst": d.BurstCount = Math.Max(1, (int)Num(v)); break;
                case "hitscan": d.Hitscan = Bool(v); break;
                case "range": d.Range = Math.Max(1, Num(v)); break;
                case "recoil": d.Recoil = Num(v); break;
                case "damage": d.Damage = Num(v); break;
                case "damagetype": d.DamageType = v.Trim(); if (Normalize(v).Contains("explos") && d.AreaOfEffect <= 0) d.AreaOfEffect = 3f; break;
                case "aoe":
                {
                    var b = HasDigit(v) ? null : TryBool(v);
                    if (b.HasValue) d.AreaOfEffect = b.Value ? 3f : 0f;
                    else d.AreaOfEffect = Math.Max(0, Num(v));
                    break;
                }
                case "falloff": d.AreaFalloff = Bool(v); break;
                case "force": d.DamageForce = Num(v); break;
                case "durability":
                    if (IsInfinite(v)) { d.Unbreakable = true; d.Durability = 0; } else d.Durability = Math.Max(0, Num(v)); break;
                case "unbreakable": d.Unbreakable = Bool(v); break;
                case "attackspeed": d.AttackSpeed = Math.Max(0, Num(v)); break;
                case "throwdamage": d.ThrowDamage = Math.Max(0, Num(v)); break;
                case "start": d.AppearsAtStartingArea = Bool(v); break;
                case "ground": d.AppearsOnGround = Bool(v); break;
                case "groundchance": d.GroundChance = Clamp01(Percent(v)); break;
                case "shop": d.AppearsInShop = Bool(v); break;
                case "shopchance": d.ShopChance = Clamp01(Percent(v)); break;
                case "price": d.ShopPrice = Math.Max(0, (int)Num(v)); break;
                case "enemies": d.UsedByEnemies = Bool(v); break;
                case "enemychance": d.EnemyChance = Clamp01(Percent(v)); break;
                case "weight": d.SpawnWeight = Math.Max(0, ParseWeight(v)); break;
                case "enemyinterval": d.EnemyFireInterval = Math.Max(0.05f, Num(v)); break;
                case "enemyspread": d.EnemyAccuracy = Math.Max(0, Num(v)); break;
                case "enemydmg": d.EnemyDamageMultiplier = Math.Max(0, v.Contains('%') ? Num(v) / 100f : Num(v)); break;
                case "scale": d.ModelScale = Math.Max(0.001f, Num(v)); break;
                case "rotation": d.ModelRotation = Vec3(v); break;
                case "offset": d.ModelOffset = Vec3(v); break;
                case "autofit": d.AutoFit = Bool(v); break;
                case "texture": d.TexturePath = v.Trim('"'); break;
                case "tint": d.Tint = ParseColor(v); break;
                case "base": d.BaseWeapon = v.Trim('"'); break;
            }
        }

        // ------------------------------------------------------------------ value helpers

        private static readonly Regex NumberRx = new Regex(@"[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?", RegexOptions.Compiled);

        public static float Num(string v)
        {
            var m = NumberRx.Match(v.Replace(',', '.'));
            if (!m.Success) throw new FormatException($"expected a number but got \"{v}\"");
            return float.Parse(m.Value, NumberStyles.Float, CultureInfo.InvariantCulture);
        }

        /// <summary>"25%" -> 0.25, "0.25" -> 0.25, "25" -> 0.25 (values above 1 are treated as percentages).</summary>
        public static float Percent(string v)
        {
            float n = Num(v);
            if (v.Contains('%') || n > 1f) n /= 100f;
            return n;
        }

        private static float Clamp01(float f) => f < 0 ? 0 : f > 1 ? 1 : f;

        private static bool IsInfinite(string v)
        {
            var n = Normalize(v);
            return n is "infinite" or "infinity" or "unlimited" or "inf" or "none" or "endless" or "never";
        }

        private static bool IsHitscanWord(string v)
        {
            var n = Normalize(v);
            return n is "hitscan" or "instant" or "infinite" or "laser" or "instanthit";
        }

        public static bool? TryBool(string v)
        {
            switch (Normalize(v))
            {
                case "yes": case "y": case "true": case "t": case "1": case "on": case "enabled": case "enable": case "always": return true;
                case "no": case "n": case "false": case "f": case "0": case "off": case "disabled": case "disable": case "never": case "none": return false;
                default: return null;
            }
        }

        private static bool Bool(string v) => TryBool(v) ?? throw new FormatException($"expected yes/no but got \"{v}\"");

        private static WeaponKind ParseKind(string v)
        {
            var n = Normalize(v);
            if (n.Contains("range") || n.Contains("gun") || n.Contains("bow") || n.Contains("shoot") || n.Contains("projectile")
                || n.Contains("pistol") || n.Contains("rifle") || n.Contains("launcher") || n.Contains("firearm"))
                return WeaponKind.Ranged;
            if (n.Contains("melee") || n.Contains("blade") || n.Contains("blunt") || n.Contains("sword") || n.Contains("club") || n.Contains("hand"))
                return WeaponKind.Melee;
            throw new FormatException($"expected Melee or Ranged but got \"{v}\"");
        }

        private static void ApplyFireType(WeaponDefinition d, string v)
        {
            var n = Normalize(v);
            if (n.Contains("hitscan") || n.Contains("laser") || n.Contains("instant")) { d.Hitscan = true; d.ProjectileSpeed = 0; }
            if (n.Contains("beam")) { d.Hitscan = true; d.ProjectileSpeed = 0; d.FireMode = FireMode.Automatic; return; }
            if (n.Contains("shotgun") || n.Contains("scatter"))
            {
                d.FireMode = FireMode.Single;
                if (d.Pellets <= 1) d.Pellets = 8;
                if (d.Spread <= 1) d.Spread = 7f;
                return;
            }
            if (n.Contains("burst")) { d.FireMode = FireMode.Burst; return; }
            if (n.Contains("auto") && !n.Contains("semi")) { d.FireMode = FireMode.Automatic; return; }
            if (n.Contains("semi") || n.Contains("single") || n.Contains("bolt") || n.Contains("pump") || n.Contains("manual") || n.Contains("hitscan") || n.Contains("laser") || n.Contains("instant"))
            { d.FireMode = FireMode.Single; return; }
            throw new FormatException($"expected Single, Burst, Automatic, Shotgun or Hitscan but got \"{v}\"");
        }

        private static bool HasDigit(string v) => v.Any(char.IsDigit);

        private static float ParseDrop(string v)
        {
            var b = HasDigit(v) ? null : TryBool(v);
            if (b.HasValue) return b.Value ? 1f : 0f;
            var n = Normalize(v);
            if (n == "low") return 0.3f;
            if (n == "medium") return 1f;
            if (n == "high") return 2f;
            return Math.Max(0, Num(v));
        }

        private static float ParseWeight(string v)
        {
            switch (Normalize(v))
            {
                case "common": return 1f;
                case "uncommon": return 0.6f;
                case "rare": return 0.3f;
                case "epic": return 0.15f;
                case "legendary": return 0.05f;
            }
            return Num(v);
        }

        private static void ApplyVisual(WeaponDefinition d, string v)
        {
            var trimmed = v.Trim().Trim('"');
            if (trimmed.EndsWith(".fbx", StringComparison.OrdinalIgnoreCase))
            {
                d.ProjectileVisual = ProjectileVisual.Model;
                d.ProjectileModelPath = trimmed;
                return;
            }
            var n = Normalize(v);
            if (n.Contains("none") || n.Contains("invisible")) d.ProjectileVisual = ProjectileVisual.None;
            else if (n.Contains("sphere") || n.Contains("ball") || n.Contains("orb") || n.Contains("plasma") || n.Contains("grenade")) d.ProjectileVisual = ProjectileVisual.Sphere;
            else if (n.Contains("cube") || n.Contains("box") || n.Contains("block")) d.ProjectileVisual = ProjectileVisual.Cube;
            else if (n.Contains("arrow") || n.Contains("bolt") || n.Contains("spear") || n.Contains("dart")) d.ProjectileVisual = ProjectileVisual.Arrow;
            else if (n.Contains("model") || n.Contains("weapon") || n.Contains("self")) d.ProjectileVisual = ProjectileVisual.Model;
            else if (n.Contains("bullet") || n.Contains("tracer") || n.Contains("pellet") || n.Contains("shell")) d.ProjectileVisual = ProjectileVisual.Bullet;
            else if (n.Contains("laser") || n.Contains("beam")) { d.ProjectileVisual = ProjectileVisual.Bullet; d.ProjectileColor = new Vector4(1f, 0.1f, 0.1f, 1f); }
            else throw new FormatException($"expected Bullet, Sphere, Cube, Arrow, Model, None or a .fbx file but got \"{v}\"");

            // "red sphere" style values also set the colour
            foreach (var word in v.Split(new[] { ' ', ',' }, StringSplitOptions.RemoveEmptyEntries))
                if (NamedColors.TryGetValue(Normalize(word), out var c)) d.ProjectileColor = c;
        }

        private static readonly Dictionary<string, Vector4> NamedColors = new Dictionary<string, Vector4>
        {
            ["white"] = new Vector4(1, 1, 1, 1), ["black"] = new Vector4(0, 0, 0, 1), ["red"] = new Vector4(1, 0.1f, 0.1f, 1),
            ["green"] = new Vector4(0.2f, 1, 0.2f, 1), ["blue"] = new Vector4(0.2f, 0.4f, 1, 1), ["yellow"] = new Vector4(1, 0.9f, 0.2f, 1),
            ["orange"] = new Vector4(1, 0.55f, 0.1f, 1), ["purple"] = new Vector4(0.6f, 0.2f, 1, 1), ["pink"] = new Vector4(1, 0.4f, 0.8f, 1),
            ["cyan"] = new Vector4(0.2f, 1, 1, 1), ["gold"] = new Vector4(1, 0.8f, 0.2f, 1), ["grey"] = new Vector4(0.5f, 0.5f, 0.5f, 1),
            ["gray"] = new Vector4(0.5f, 0.5f, 0.5f, 1), ["silver"] = new Vector4(0.8f, 0.8f, 0.85f, 1), ["brown"] = new Vector4(0.45f, 0.3f, 0.15f, 1),
        };

        public static Vector4 ParseColor(string v)
        {
            var t = v.Trim().Trim('"');
            if (t.StartsWith("#") && (t.Length == 7 || t.Length == 9))
            {
                float H(int i) => int.Parse(t.Substring(i, 2), NumberStyles.HexNumber) / 255f;
                return new Vector4(H(1), H(3), H(5), t.Length == 9 ? H(7) : 1f);
            }
            if (NamedColors.TryGetValue(Normalize(t), out var named)) return named;
            var nums = NumberRx.Matches(t).Select(m => float.Parse(m.Value, CultureInfo.InvariantCulture)).ToArray();
            if (nums.Length >= 3)
            {
                bool bytes = nums.Take(3).Any(x => x > 1f);
                float s = bytes ? 1f / 255f : 1f;
                return new Vector4(nums[0] * s, nums[1] * s, nums[2] * s, nums.Length > 3 ? nums[3] * (nums[3] > 1 ? 1f / 255f : 1f) : 1f);
            }
            throw new FormatException($"expected a colour like #FF8800, red, or 1,0.5,0 but got \"{v}\"");
        }

        private static Vector3 Vec3(string v)
        {
            var nums = NumberRx.Matches(v).Select(m => float.Parse(m.Value, CultureInfo.InvariantCulture)).ToArray();
            if (nums.Length == 1) return new Vector3(nums[0]);
            if (nums.Length < 3) throw new FormatException($"expected three numbers like 0, 90, 0 but got \"{v}\"");
            return new Vector3(nums[0], nums[1], nums[2]);
        }

        private static void ResolveFiles(WeaponDefinition d, string dir)
        {
            string? Find(string baseName, params string[] exts)
            {
                foreach (var e in exts)
                {
                    var p = Path.Combine(dir, baseName + e);
                    if (File.Exists(p)) return p;
                }
                return null;
            }

            d.ModelPath = Find(d.Id, ".fbx", ".FBX", ".Fbx") ?? Path.Combine(dir, d.Id + ".fbx");

            if (!string.IsNullOrEmpty(d.TexturePath))
            {
                var p = Path.IsPathRooted(d.TexturePath) ? d.TexturePath : Path.Combine(dir, d.TexturePath);
                if (File.Exists(p)) d.TexturePath = p;
                else { d.Warnings.Add($"Texture \"{d.TexturePath}\" not found."); d.TexturePath = null; }
            }
            else d.TexturePath = Find(d.Id, ".png", ".PNG", ".jpg", ".JPG", ".jpeg");

            if (!string.IsNullOrEmpty(d.ProjectileModelPath))
            {
                var p = Path.IsPathRooted(d.ProjectileModelPath) ? d.ProjectileModelPath : Path.Combine(dir, d.ProjectileModelPath);
                if (File.Exists(p)) d.ProjectileModelPath = p;
                else
                {
                    d.Warnings.Add($"Projectile model \"{d.ProjectileModelPath}\" not found, using the weapon model.");
                    d.ProjectileModelPath = null;
                }
            }
        }
    }
}
