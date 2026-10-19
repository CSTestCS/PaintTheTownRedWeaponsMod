using System;
using System.Numerics;
using PTTRCustomWeapons.Core;
using Xunit;

namespace PTTRCustomWeapons.Tests
{
    public class ConfigTests
    {
        [Fact]
        public void Labeled_file_parses_every_requested_field()
        {
            var d = WeaponConfigParser.Parse(@"
# a rocket launcher
Name: Boom Stick
Melee or Ranged: Ranged
Magazine size: 4
Fire type: Semi-automatic
Projectile speed: 35
Projectile drop: 0.5
Automatic fire rate: 2
Projectile bounce: 1
Projectile visual: Sphere
Damage: 120
Damage type: Explosive
Area of effect: 4.5
Damage force: 80
Appears at starting area?: no
", "BoomStick");

            Assert.Empty(d.Warnings);
            Assert.Equal("Boom Stick", d.Name);
            Assert.Equal(WeaponKind.Ranged, d.Kind);
            Assert.Equal(4, d.MagazineSize);
            Assert.Equal(FireMode.Single, d.FireMode);
            Assert.Equal(35f, d.ProjectileSpeed);
            Assert.Equal(0.5f, d.ProjectileDrop);
            Assert.Equal(2f, d.FireRate);
            Assert.Equal(1, d.ProjectileBounces);
            Assert.Equal(ProjectileVisual.Sphere, d.ProjectileVisual);
            Assert.Equal(120f, d.Damage);
            Assert.Equal("Explosive", d.DamageType);
            Assert.Equal(4.5f, d.AreaOfEffect);
            Assert.Equal(80f, d.DamageForce);
            Assert.False(d.AppearsAtStartingArea);
            Assert.False(d.Hitscan);
        }

        [Fact]
        public void Unlabeled_file_uses_documented_order()
        {
            var d = WeaponConfigParser.Parse(string.Join("\n",
                "Plasma Rifle", "Ranged", "30", "Automatic", "80", "0", "10", "0", "red sphere",
                "15", "Fire", "0", "5", "yes"), "Plasma");
            Assert.Equal("Plasma Rifle", d.Name);
            Assert.Equal(WeaponKind.Ranged, d.Kind);
            Assert.Equal(30, d.MagazineSize);
            Assert.Equal(FireMode.Automatic, d.FireMode);
            Assert.Equal(10f, d.FireRate);
            Assert.Equal(ProjectileVisual.Sphere, d.ProjectileVisual);
            Assert.Equal(new Vector4(1, 0.1f, 0.1f, 1), d.ProjectileColor);
            Assert.Equal("Fire", d.DamageType);
            Assert.True(d.AppearsAtStartingArea);
        }

        [Fact]
        public void Shotgun_preset_and_overrides()
        {
            var d = WeaponConfigParser.Parse("Pellets = 12\nType = Ranged\nFire Type = Shotgun", "S");
            Assert.Equal(12, d.Pellets); // explicit value wins over preset regardless of line order
            Assert.Equal(FireMode.Single, d.FireMode);
            Assert.True(d.Spread > 1);
        }

        [Theory]
        [InlineData("Projectile speed: hitscan")]
        [InlineData("Projectile speed: 0")]
        [InlineData("Fire type: laser")]
        public void Hitscan_variants(string line)
        {
            var d = WeaponConfigParser.Parse("Type: Ranged\n" + line, "H");
            Assert.True(d.Hitscan);
        }

        [Fact]
        public void Melee_defaults_and_extras()
        {
            var d = WeaponConfigParser.Parse(@"Name = Big Bone
Type = Melee
Damage = 40
Durability = infinite
Shop Price = 250
Enemy Chance = 30%
Ground Chance = 0.5
Model Rotation = 0, 90, 0
Tint = #FF8800
Rarity = rare
Base Weapon = Bat", "Bone");
            Assert.Empty(d.Warnings);
            Assert.Equal(WeaponKind.Melee, d.Kind);
            Assert.True(d.Unbreakable);
            Assert.Equal(250, d.ShopPrice);
            Assert.Equal(0.3f, d.EnemyChance, 3);
            Assert.Equal(0.5f, d.GroundChance, 3);
            Assert.Equal(new Vector3(0, 90, 0), d.ModelRotation);
            Assert.Equal(1f, d.Tint.X, 3);
            Assert.Equal(0x88 / 255f, d.Tint.Y, 3);
            Assert.Equal(0.3f, d.SpawnWeight, 3);
            Assert.Equal("Bat", d.BaseWeapon);
        }

        [Fact]
        public void Unknown_keys_and_bad_values_become_warnings_not_errors()
        {
            var d = WeaponConfigParser.Parse("Name: X\nFlavour: spicy\nDamage: lots\nType: Ranged", "X");
            Assert.Equal(2, d.Warnings.Count);
            Assert.Equal(WeaponKind.Ranged, d.Kind);
        }

        [Fact]
        public void Shipped_template_and_examples_parse_without_warnings()
        {
            var dir = System.IO.Path.Combine(RepoRoot(), "CustomWeapons");
            foreach (var file in System.IO.Directory.GetFiles(dir, "*.txt"))
            {
                if (System.IO.Path.GetFileName(file) == "README.txt") continue;
                var d = WeaponConfigParser.Parse(System.IO.File.ReadAllText(file), System.IO.Path.GetFileNameWithoutExtension(file));
                Assert.True(d.Warnings.Count == 0, System.IO.Path.GetFileName(file) + ": " + string.Join("; ", d.Warnings));
            }
            var t = WeaponConfigParser.Parse(System.IO.File.ReadAllText(System.IO.Path.Combine(dir, "TEMPLATE.txt")), "T");
            Assert.Equal(new Vector4(1f, 0xD9 / 255f, 0x40 / 255f, 1f), t.ProjectileColor);
            Assert.Equal("My Weapon", t.Name);
            Assert.Equal(-1, t.ReserveAmmo);
        }

        [Fact]
        public void Example_weapons_load_from_folder()
        {
            var dir = System.IO.Path.Combine(RepoRoot(), "CustomWeapons");
            var warnings = new System.Collections.Generic.List<string>();
            var loaded = WeaponPack.LoadFolder(dir, _ => { }, warnings.Add);
            Assert.Empty(warnings);
            Assert.Equal(3, loaded.Count);
            var rocket = loaded.Find(w => w.Def.Id == "ExampleRocketLauncher")!;
            Assert.Equal(ProjectileVisual.Model, rocket.Def.ProjectileVisual);
            Assert.NotNull(rocket.ProjectileMesh);
            Assert.NotNull(rocket.TextureBytes);
            var sword = loaded.Find(w => w.Def.Id == "ExampleGreatsword")!;
            Assert.NotNull(sword.TextureBytes); // embedded in the fbx
            Assert.Equal(WeaponKind.Melee, sword.Def.Kind);
        }

        internal static string RepoRoot()
        {
            var dir = System.AppContext.BaseDirectory;
            while (dir != null && !System.IO.Directory.Exists(System.IO.Path.Combine(dir, "CustomWeapons"))) dir = System.IO.Path.GetDirectoryName(dir);
            return dir ?? throw new System.Exception("repo root not found");
        }

        [Fact]
        public void Hooks_config_user_values_override_defaults()
        {
            var cfg = HooksConfig.Parse(HooksConfig.DefaultText);
            Assert.Contains("PTTRPlayer", cfg.List("PlayerTypes"));
            Assert.Equal(2.5f, cfg.Float("SceneSetupDelay", 0));
            Assert.Equal("", cfg.Raw("GameplayScenes"));
            Assert.Contains("|", cfg.Raw("MenuScenes"));
        }
    }

    public class BallisticsTests
    {
        [Fact]
        public void Drop_matches_closed_form()
        {
            var v = new Vector3(10, 0, 0);
            var pos = Vector3.Zero;
            for (int i = 0; i < 100; i++) pos += Ballistics.Step(ref v, 1f, 0.01f);
            Assert.Equal(10f, pos.X, 3);
            Assert.Equal(-0.5f * 9.81f, pos.Y, 3);
        }

        [Fact]
        public void Bounce_reflects_and_damps()
        {
            var v = Ballistics.Bounce(new Vector3(1, -1, 0), Vector3.UnitY, 0.5f);
            Assert.Equal(new Vector3(0.5f, 0.5f, 0), v);
        }

        [Fact]
        public void Spread_stays_inside_cone()
        {
            var rng = new Random(1);
            var fwd = Vector3.Normalize(new Vector3(1, 2, 3));
            for (int i = 0; i < 500; i++)
            {
                var d = Ballistics.Spread(fwd, 10f, rng);
                var angle = MathF.Acos(Math.Clamp(Vector3.Dot(d, fwd), -1, 1)) * 180 / MathF.PI;
                Assert.True(angle <= 10.01f);
            }
        }

        [Fact]
        public void Look_rotation_points_forward_axis_along_direction()
        {
            foreach (var dir in new[] { new Vector3(1, 0, 0), new Vector3(0, 0, -1), Vector3.Normalize(new Vector3(1, 1, 1)), Vector3.Normalize(new Vector3(-2, -1, 0.5f)) })
            {
                var q = Ballistics.LookRotation(dir);
                var f = Vector3.Transform(Vector3.UnitZ, q);
                Assert.True(Vector3.Distance(dir, f) < 1e-4f, $"{dir} -> {f}");
            }
        }

        [Fact]
        public void Area_damage_falloff()
        {
            Assert.Equal(100f, Ballistics.AreaDamage(100, 4, 0, true));
            Assert.Equal(25f, Ballistics.AreaDamage(100, 4, 4, true));
            Assert.Equal(0f, Ballistics.AreaDamage(100, 4, 5, true));
            Assert.Equal(100f, Ballistics.AreaDamage(100, 4, 3.9f, false));
        }
    }
}
