using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace PTTRCustomWeapons.Core
{
    /// <summary>
    /// _GameHooks.cfg: tells the mod which of the game's classes and members to use.
    /// Every list is tried in order. Names are case-insensitive. "a.b" walks into a sub-object.
    /// "*word*" matches any member containing "word". This lets the mod be adapted to game
    /// updates by editing a text file instead of recompiling.
    /// </summary>
    public sealed class HooksConfig
    {
        public const string FileName = "_GameHooks.cfg";

        public const string DefaultText = @"# ---------------------------------------------------------------------------
# Paint the Town Red - Custom Weapons: game integration settings
# ---------------------------------------------------------------------------
# The game is IL2CPP, so the mod finds the game's classes by name at runtime.
# If a game update renames something, press F10 in game (or look at
# _GameReport.txt, written automatically) and update the names below.
# Lists are tried in order. Names are case-insensitive.
#   a.b    = walk into a sub-object (b on the object stored in a)
#   *word* = any member whose name contains 'word'
# Delete this file to get the defaults back.
# ---------------------------------------------------------------------------

# --- Game classes ----------------------------------------------------------
PlayerTypes        = PTTRPlayer, Player, PlayerController, FPSPlayer
EnemyTypes         = Enemy, EnemyAI, NPC, Character
# 'auto' = use the type returned by the player's current-weapon member
WeaponTypes        = auto, Weapon, MeleeWeapon, BaseWeapon, Item, Pickup
ShopTypes          = Shop, ShopItem, ShopSlot, ShopStand, ShopPedestal, Store, Vendor, Merchant, ShopKeeper

# --- Who is holding what ---------------------------------------------------
PlayerWeaponMembers = CurrentWeapon, currentWeapon, weapon, Weapon, heldWeapon, equippedWeapon, *weapon*
EnemyWeaponMembers  = CurrentWeapon, currentWeapon, weapon, Weapon, heldWeapon, equippedWeapon, *weapon*
EnemyEquipMethods   = EquipWeapon, PickUpWeapon, PickupWeapon, SetWeapon, GiveWeapon, Equip, GrabWeapon, PickUp
EnemyDeadMembers    = IsDead, isDead, dead, Dead, *dead*
# Member on a weapon that says it is a gun (bool) - or the class name contains one of RangedClassWords
WeaponRangedMembers = isRanged, IsRanged, ranged, isGun, IsGun, gun, isFirearm, *ranged*, *gun*
RangedClassWords    = Gun, Ranged, Firearm, Bow, Crossbow, Pistol, Rifle, Shotgun, Launcher

# --- Damage ----------------------------------------------------------------
# Methods called on the hit enemy / player. The mod fills in arguments by type
# and name (damage, position, direction, force, attacker, damage type...).
EnemyDamageMethods  = TakeDamage, ApplyDamage, Damage, ReceiveDamage, GetHit, Hit, OnHit, DoDamage, Hurt
PlayerDamageMethods = TakeDamage, ApplyDamage, Damage, ReceiveDamage, GetHit, Hit, OnHit, DoDamage, Hurt
# Body-part / hitbox classes on the hit collider are tried first (lets the game dismember)
HitPartTypes        = BodyPart, Limb, Hitbox, HitBox, Hurtbox, DamageReceiver, Damageable, Bodypart
HitPartDamageMethods = TakeDamage, ApplyDamage, Damage, ReceiveDamage, GetHit, Hit, OnHit, DoDamage
# Last resort if no damage method works: subtract from one of these members
HealthMembers       = health, Health, hp, HP, currentHealth, *health*
SetHealthMethods    = SetHealth

# --- Weapon stats written onto game weapons --------------------------------
Stat.Damage         = damage, Damage, baseDamage, hitDamage, *damage*
Stat.DamageType     = damageType, DamageType, *damagetype*
Stat.Force          = force, Force, hitForce, impactForce, knockback, knockBack, pushForce, *force*
Stat.Durability     = durability, Durability, maxDurability, *durability*
Stat.CurrentDurability = currentDurability, durabilityLeft, *currentdurability*
Stat.Unbreakable    = unbreakable, Unbreakable, isUnbreakable, indestructible, *unbreakable*
Stat.AttackSpeed    = attackSpeed, swingSpeed, AttackSpeed, *attackspeed*
Stat.ThrowDamage    = throwDamage, thrownDamage, *throwdamage*
Stat.Ammo           = ammo, Ammo, currentAmmo, bullets, *ammo*
Stat.MaxAmmo        = maxAmmo, magazineSize, clipSize, *maxammo*
Stat.Name           = weaponName, displayName, itemName, WeaponName, *name*
Stat.Price          = price, Price, cost, Cost, *price*, *cost*

# A custom gun is a copy of a game gun, which would also fire its own bullets.
#   empty = set the game gun's ammo to 0 so only the custom projectiles fire (default)
#   sync  = give the game gun the custom magazine size (both fire)
#   leave = do not touch the game gun's ammo
GameGunAmmo         = empty

# --- Shop ------------------------------------------------------------------
ShopPriceMembers    = price, Price, cost, Cost, *price*, *cost*

# --- Scenes ----------------------------------------------------------------
# Regular expressions matched against the scene name (see the log / report).
MenuScenes          = (?i)menu|title|splash|intro|loading|credits|boot
# Leave empty to allow every non-menu scene; e.g. (?i)beneath|rogue to limit to Beneath
GameplayScenes      =
# Scenes treated as the Beneath starting area. Empty = the first gameplay scene after a menu.
StartingAreaScenes  = (?i)start|hub|lobby|camp|tavern|spawn
# Seconds to wait after a scene loads before placing weapons (levels may be generated late)
SceneSetupDelay     = 2.5
# How often (seconds) to look for new enemies / shop items
ScanInterval        = 1.0
# Enemies with custom guns only shoot when the player is closer than this (metres)
EnemyMaxRange       = 35
# Multiply every weapon's spawn chances
ChanceMultiplier    = 1.0
# If this many new weapons appear at once (without a scene load), treat it as a new floor
# and place custom weapons again. 0 = off
NewFloorWeaponCount = 3
# Max extra custom weapons added to the ground per scene
MaxGroundSpawnsPerScene = 6

# --- Controls (Windows virtual-key names: Mouse0, Mouse1, R, F8, ...) -------
FireKey             = Mouse0
ReloadKey           = R
SpawnAllKey         = F8
ReloadPacksKey      = F9
ReportKey           = F10
ShowHud             = true
";

        private readonly Dictionary<string, List<string>> _values = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, string> _raw = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        public static HooksConfig LoadOrCreate(string folder)
        {
            var path = Path.Combine(folder, FileName);
            var cfg = Parse(DefaultText);
            if (!File.Exists(path))
            {
                try { File.WriteAllText(path, DefaultText); } catch { /* read-only install, defaults still work */ }
                return cfg;
            }
            // user values override defaults key by key, so new keys added in updates keep working
            cfg.Merge(Parse(File.ReadAllText(path)));
            return cfg;
        }

        public static HooksConfig Parse(string text)
        {
            var cfg = new HooksConfig();
            foreach (var rawLine in text.Replace("\r\n", "\n").Split('\n'))
            {
                var line = rawLine.Trim();
                if (line.Length == 0 || line.StartsWith("#") || line.StartsWith("//")) continue;
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                var key = line.Substring(0, eq).Trim();
                var value = line.Substring(eq + 1).Trim();
                cfg._raw[key] = value;
                cfg._values[key] = value.Split(',').Select(s => s.Trim()).Where(s => s.Length > 0).ToList();
            }
            return cfg;
        }

        private void Merge(HooksConfig other)
        {
            foreach (var kv in other._values) _values[kv.Key] = kv.Value;
            foreach (var kv in other._raw) _raw[kv.Key] = kv.Value;
        }

        public IReadOnlyList<string> List(string key) =>
            _values.TryGetValue(key, out var l) ? l : (IReadOnlyList<string>)Array.Empty<string>();

        public string Raw(string key, string def = "") => _raw.TryGetValue(key, out var v) ? v : def;

        public float Float(string key, float def)
        {
            var r = Raw(key);
            return float.TryParse(r, NumberStyles.Float, CultureInfo.InvariantCulture, out var f) ? f : def;
        }

        public bool Bool(string key, bool def) => WeaponConfigParser.TryBool(Raw(key)) ?? def;
    }
}
