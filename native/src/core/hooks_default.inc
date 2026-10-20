R"CFG(# ---------------------------------------------------------------------------
# Paint the Town Red - Custom Weapons: game integration settings
# ---------------------------------------------------------------------------
# The mod finds the game's classes by name at runtime (no decompiling step).
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
# On the player: bool that is true for the local player (online play has several players)
PlayerLocalMembers  = IsLocalPlayer, isLocalPlayer, isLocal, IsLocal, isMine, IsMine
# Per-frame method the mod runs alongside (Class.Method). The first one found is used.
TickMethods         = PTTRPlayer.Update, PTTRPlayer.LateUpdate, PTTRPlayer.FixedUpdate
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
# Seconds without any level running (menus, loading) after which the next level counts as
# "coming from the menu" - used to detect the starting area
MenuGapSeconds      = 8
# Scenes treated as the Beneath starting area (also: the first level after the menu).
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
)CFG"