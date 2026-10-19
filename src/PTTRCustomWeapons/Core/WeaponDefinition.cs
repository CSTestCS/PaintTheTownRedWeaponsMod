using System.Collections.Generic;
using System.Numerics;

namespace PTTRCustomWeapons.Core
{
    public enum WeaponKind { Melee, Ranged }
    public enum FireMode { Single, Burst, Automatic }
    public enum ProjectileVisual { Bullet, Sphere, Cube, Arrow, Model, None }

    /// <summary>Everything a weapon .txt file can describe. Defaults are used for anything left out.</summary>
    public sealed class WeaponDefinition
    {
        // identity / files
        public string Id = "";              // file name without extension (unique)
        public string Name = "";
        public string Description = "";
        public string ModelPath = "";
        public string? TexturePath;
        public string BaseWeapon = "";      // optional: name of a game weapon to use as the template

        // core
        public WeaponKind Kind = WeaponKind.Melee;
        public float Damage = 25f;
        public string DamageType = "Blunt";
        public float AreaOfEffect = 0f;     // radius in metres, 0 = none
        public bool AreaFalloff = true;
        public float DamageForce = 10f;
        public float Durability = 0f;       // 0 = keep the template's value
        public bool Unbreakable;
        public float AttackSpeed = 0f;      // 0 = keep template; otherwise multiplier
        public float ThrowDamage = 0f;      // 0 = keep template

        // ranged
        public int MagazineSize = 12;       // 0 = never needs reloading
        public int ReserveAmmo = -1;        // -1 = infinite spare ammo
        public float ReloadTime = 1.5f;
        public FireMode FireMode = FireMode.Single;
        public float FireRate = 6f;         // shots per second
        public int BurstCount = 3;
        public int Pellets = 1;
        public float Spread = 1f;           // degrees
        public bool Hitscan;
        public float Range = 300f;          // metres, for hitscan and enemy targeting
        public float ProjectileSpeed = 60f; // m/s
        public float ProjectileDrop = 0f;   // multiple of normal gravity
        public int ProjectileBounces = 0;
        public float Bounciness = 0.7f;     // fraction of speed kept per bounce
        public int Pierce = 0;              // extra targets a projectile passes through
        public ProjectileVisual ProjectileVisual = ProjectileVisual.Bullet;
        public string? ProjectileModelPath;
        public float ProjectileSize = 0.08f;
        public Vector4 ProjectileColor = new Vector4(1f, 0.85f, 0.25f, 1f);
        public float ProjectileLifetime = 6f;
        public float Recoil = 0f;           // reserved (camera kick) - stored, not yet applied

        // spawning
        public bool AppearsAtStartingArea;
        public bool AppearsOnGround = true;
        public float GroundChance = 0.35f;
        public bool AppearsInShop = true;
        public float ShopChance = 0.25f;
        public int ShopPrice = 100;
        public bool UsedByEnemies = true;
        public float EnemyChance = 0.15f;
        public float SpawnWeight = 1f;
        public float EnemyFireInterval = 1.2f;
        public float EnemyAccuracy = 4f;     // degrees of spread for enemy shots
        public float EnemyDamageMultiplier = 0.5f;

        // model placement
        public float ModelScale = 1f;
        public Vector3 ModelRotation = Vector3.Zero;
        public Vector3 ModelOffset = Vector3.Zero;
        public bool AutoFit = true;
        public Vector4 Tint = Vector4.One;

        public readonly List<string> Warnings = new List<string>();

        public bool IsRanged => Kind == WeaponKind.Ranged;
        public bool InfiniteMagazine => MagazineSize <= 0;

        public override string ToString() => $"{Name} ({Kind})";
    }
}
