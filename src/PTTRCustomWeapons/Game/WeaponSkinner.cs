using System;
using System.Collections.Generic;
using System.Linq;
using PTTRCustomWeapons.Core;
using UnityEngine;
using NV3 = System.Numerics.Vector3;

namespace PTTRCustomWeapons.Game
{
    /// <summary>A game weapon object that has been turned into a custom weapon.</summary>
    internal sealed class CustomInstance
    {
        public LoadedWeapon Weapon = null!;
        public GameObject Root = null!;
        public Component WeaponComponent = null!;
        public GameObject? Model;
        public readonly List<Renderer> HiddenRenderers = new List<Renderer>();
        public int Magazine;
        public int Reserve;
        public float ReloadDoneAt = -1f;
        public float NextShotAt;
        public int BurstLeft;
        public float NextBurstShotAt;
        public WeaponDefinition Def => Weapon.Def;
        public bool Reloading => ReloadDoneAt > 0;
    }

    /// <summary>
    /// Turns real game weapons into custom ones: hides the original model, attaches the FBX model,
    /// and writes the custom stats into the game's weapon fields. Because the object stays a genuine
    /// game weapon, the game itself handles picking it up, swinging, throwing, dropping and breaking it.
    /// </summary>
    internal sealed class WeaponSkinner
    {
        private readonly GameBridge _game;
        private readonly Dictionary<int, CustomInstance> _byRoot = new Dictionary<int, CustomInstance>();
        private readonly HashSet<string> _statLogged = new HashSet<string>();

        public WeaponSkinner(GameBridge game) { _game = game; }

        public IEnumerable<CustomInstance> All => _byRoot.Values;

        public CustomInstance? Get(Component? weapon)
        {
            if (!U.Alive(weapon)) return null;
            return _byRoot.TryGetValue(weapon.gameObject.GetInstanceID(), out var ci) && U.Alive(ci.Root) ? ci : null;
        }

        public bool IsCustom(Component weapon) => Get(weapon) != null;

        public void Cleanup()
        {
            foreach (var key in _byRoot.Where(kv => !U.Alive(kv.Value.Root)).Select(kv => kv.Key).ToList())
                _byRoot.Remove(key);
        }

        public CustomInstance? Apply(Component weapon, LoadedWeapon lw)
        {
            if (!U.Alive(weapon)) return null;
            var root = weapon.gameObject;
            int id = root.GetInstanceID();
            if (_byRoot.TryGetValue(id, out var existing) && U.Alive(existing.Model)) UnityEngine.Object.Destroy(existing.Model);

            var ci = new CustomInstance
            {
                Weapon = lw,
                Root = root,
                WeaponComponent = weapon,
                Magazine = lw.Def.InfiniteMagazine ? int.MaxValue : lw.Def.MagazineSize,
                Reserve = lw.Def.ReserveAmmo < 0 ? int.MaxValue : lw.Def.ReserveAmmo,
            };
            try
            {
                ci.Model = BuildModel(root, lw, ci);
            }
            catch (Exception ex)
            {
                Log.Error($"[{lw.Def.Id}] could not attach model: {ex}");
            }
            try { root.name = "CW_" + lw.Def.Name; } catch { }
            ApplyStats(weapon, lw.Def);
            _byRoot[id] = ci;
            return ci;
        }

        // ------------------------------------------------------------------ model

        private GameObject? BuildModel(GameObject root, LoadedWeapon lw, CustomInstance ci)
        {
            var def = lw.Def;
            var mesh = AssetFactory.GetMesh(lw.Mesh, "CW_" + def.Id);
            if (mesh == null) return null;

            // Measure and hide the original visuals (everything except models we added).
            Material? sourceMaterial = null;
            NV3 origMin = new NV3(float.MaxValue), origMax = new NV3(float.MinValue);
            bool haveOrig = false;
            var rootT = root.transform;
            foreach (var r in Renderers(root))
            {
                if (r.gameObject.name.StartsWith("CW_Model")) { UnityEngine.Object.Destroy(r.gameObject); continue; }
                if (!r.enabled) continue;
                if (sourceMaterial == null)
                {
                    try
                    {
                        sourceMaterial = r.sharedMaterial;
                        Log.InfoOnce("shader" + def.Id, $"[{def.Id}] copying material of '{r.gameObject.name}' (shader: {(U.Alive(sourceMaterial) ? sourceMaterial.shader?.name : "none")})");
                    }
                    catch { }
                }
                try
                {
                    foreach (var corner in LocalCorners(r))
                    {
                        var local = U.N(rootT.InverseTransformPoint(U.V(corner)));
                        origMin = NV3.Min(origMin, local);
                        origMax = NV3.Max(origMax, local);
                        haveOrig = true;
                    }
                }
                catch { }
                r.enabled = false;
                ci.HiddenRenderers.Add(r);
            }

            var texture = AssetFactory.GetTexture(lw.TextureBytes, "CW_" + def.Id);
            var material = AssetFactory.GetMaterial(def.Id, sourceMaterial, texture, def.Tint);
            var model = AssetFactory.AddModel(rootT, "CW_Model_" + def.Id, mesh, material);
            if (model == null) return null;

            // Place the model where the original one was: same centre, longest axes lined up, same length.
            var (min, max) = lw.Mesh.Bounds();
            var size = max - min;
            var center = (min + max) * 0.5f;
            var rotation = System.Numerics.Quaternion.Identity;
            float scale = def.ModelScale;
            var targetCenter = NV3.Zero;
            if (def.AutoFit && haveOrig)
            {
                var origSize = origMax - origMin;
                targetCenter = (origMin + origMax) * 0.5f;
                rotation = AlignLongestAxis(size, origSize);
                float ours = MathF.Max(size.X, MathF.Max(size.Y, size.Z));
                float theirs = MathF.Max(origSize.X, MathF.Max(origSize.Y, origSize.Z));
                if (ours > 1e-5f && theirs > 1e-5f) scale *= theirs / ours;
            }
            else
            {
                // root may be scaled: undo it so ModelScale is in metres
                var ls = U.N(rootT.lossyScale);
                float s = MathF.Max(1e-4f, MathF.Abs(ls.X));
                scale /= s;
            }
            rotation = Ballistics.EulerDegrees(def.ModelRotation) * rotation;
            var t = model.transform;
            t.localScale = U.V(scale, scale, scale);
            t.localRotation = U.Q(rotation);
            t.localPosition = U.V(targetCenter - NV3.Transform(center * scale, rotation) + def.ModelOffset);
            return model;
        }

        /// <summary>World-space corners of a renderer's mesh box (tight: uses the mesh's own local bounds when possible).</summary>
        private static IEnumerable<NV3> LocalCorners(Renderer r)
        {
            NV3 c, e;
            bool local = false;
            try
            {
                var mf = r.gameObject.GetComponent(U.T<MeshFilter>())?.TryCast<MeshFilter>();
                var mesh = U.Alive(mf) ? mf.sharedMesh : null;
                if (U.Alive(mesh))
                {
                    var b = mesh.bounds;
                    c = U.N(b.center); e = U.N(b.extents);
                    local = true;
                }
                else
                {
                    var b = r.bounds;
                    c = U.N(b.center); e = U.N(b.extents);
                }
            }
            catch
            {
                var b = r.bounds;
                c = U.N(b.center); e = U.N(b.extents);
            }
            var t = r.transform;
            for (int i = 0; i < 8; i++)
            {
                var corner = c + new NV3((i & 1) == 0 ? -e.X : e.X, (i & 2) == 0 ? -e.Y : e.Y, (i & 4) == 0 ? -e.Z : e.Z);
                yield return local ? U.N(t.TransformPoint(U.V(corner))) : corner;
            }
        }

        /// <summary>Some games re-enable renderers (highlights, pickup effects); hide the originals again.</summary>
        public void KeepOriginalsHidden()
        {
            foreach (var ci in _byRoot.Values)
            {
                if (!U.Alive(ci.Root)) continue;
                foreach (var r in ci.HiddenRenderers)
                    try { if (U.Alive(r) && r.enabled) r.enabled = false; } catch { }
            }
        }

        private static IEnumerable<Renderer> Renderers(GameObject root)
        {
            var list = new List<Renderer>();
            foreach (var t in new[] { U.T<MeshRenderer>(), U.T<SkinnedMeshRenderer>() })
            {
                try
                {
                    var arr = root.GetComponentsInChildren(t, true);
                    for (int i = 0; i < arr.Length; i++)
                    {
                        var r = arr[i]?.TryCast<Renderer>();
                        if (r != null) list.Add(r);
                    }
                }
                catch { }
            }
            return list;
        }

        /// <summary>90-degree rotation that maps our model's longest axis onto the original model's longest axis.</summary>
        private static System.Numerics.Quaternion AlignLongestAxis(NV3 ours, NV3 theirs)
        {
            int Longest(NV3 v) => v.X >= v.Y && v.X >= v.Z ? 0 : v.Y >= v.Z ? 1 : 2;
            int a = Longest(ours), b = Longest(theirs);
            if (a == b) return System.Numerics.Quaternion.Identity;
            const float h = MathF.PI / 2;
            // rotation taking axis a to axis b
            return (a, b) switch
            {
                (0, 1) => System.Numerics.Quaternion.CreateFromAxisAngle(NV3.UnitZ, h),
                (1, 0) => System.Numerics.Quaternion.CreateFromAxisAngle(NV3.UnitZ, -h),
                (0, 2) => System.Numerics.Quaternion.CreateFromAxisAngle(NV3.UnitY, -h),
                (2, 0) => System.Numerics.Quaternion.CreateFromAxisAngle(NV3.UnitY, h),
                (1, 2) => System.Numerics.Quaternion.CreateFromAxisAngle(NV3.UnitX, h),
                _ => System.Numerics.Quaternion.CreateFromAxisAngle(NV3.UnitX, -h),
            };
        }

        // ------------------------------------------------------------------ stats

        public void ApplyStats(Component weapon, WeaponDefinition def)
        {
            if (_game.WeaponType == null) return;
            var type = _game.ConcreteType(weapon) ?? _game.WeaponType;
            var w = Reflect.Wrap(type, weapon);
            if (w == null) return;
            var cfg = _game.Cfg;
            var done = new List<string>();

            void Set(string stat, object value)
            {
                if (Reflect.SetFirst(w, cfg.List("Stat." + stat), value, out var used)) done.Add($"{stat}->{used}");
            }

            Set("Damage", def.Damage);
            Set("Force", def.DamageForce);
            Set("DamageType", def.DamageType);
            if (def.Durability > 0) { Set("Durability", def.Durability); Set("CurrentDurability", def.Durability); }
            if (def.Unbreakable) Set("Unbreakable", true);
            if (def.AttackSpeed > 0) Set("AttackSpeed", def.AttackSpeed);
            if (def.ThrowDamage > 0) Set("ThrowDamage", def.ThrowDamage);
            if (def.IsRanged) ApplyGameGunAmmo(w, def, done);
            Set("Name", def.Name);

            if (_statLogged.Add(def.Id + type.Name))
                Log.Info($"[{def.Id}] stats written onto {type.Name}: {(done.Count > 0 ? string.Join(", ", done) : "none matched (edit Stat.* in _GameHooks.cfg)")}");
        }

        /// <summary>
        /// The copied game gun would also fire its own bullets. GameGunAmmo in _GameHooks.cfg decides:
        /// empty = set its ammo to 0 (only the custom projectiles fire), sync = give it the custom magazine, leave = untouched.
        /// </summary>
        private void ApplyGameGunAmmo(object wrapped, WeaponDefinition def, List<string>? done)
        {
            var mode = _game.Cfg.Raw("GameGunAmmo", "empty").Trim().ToLowerInvariant();
            if (mode == "leave") return;
            int value = mode == "sync" ? (def.InfiniteMagazine ? 999 : def.MagazineSize) : 0;
            if (mode == "sync" && Reflect.SetFirst(wrapped, _game.Cfg.List("Stat.MaxAmmo"), value, out var u1)) done?.Add("MaxAmmo->" + u1);
            if (Reflect.SetFirst(wrapped, _game.Cfg.List("Stat.Ammo"), value, out var u2)) done?.Add("Ammo->" + u2);
        }

        /// <summary>Re-applies the GameGunAmmo rule to a held custom gun (the game may refill it).</summary>
        public void EnforceGameGunAmmo(CustomInstance ci)
        {
            if (!ci.Def.IsRanged || _game.WeaponType == null || !U.Alive(ci.WeaponComponent)) return;
            if (!_game.Cfg.Raw("GameGunAmmo", "empty").Trim().Equals("empty", StringComparison.OrdinalIgnoreCase)) return;
            var w = Reflect.Wrap(_game.ConcreteType(ci.WeaponComponent) ?? _game.WeaponType, ci.WeaponComponent);
            if (w == null) return;
            var current = Reflect.GetFirst(w, _game.Cfg.List("Stat.Ammo"), t => t == typeof(int) || t == typeof(float), "ammo", out _);
            if (current != null && Convert.ToDouble(current) != 0) ApplyGameGunAmmo(w, ci.Def, null);
        }

        /// <summary>Shop price: written onto the shop component that displays this weapon, then onto the weapon.</summary>
        public void ApplyPrice(Component weapon, Component? shop, int price)
        {
            bool ok = false;
            if (shop != null)
            {
                var st = _game.ShopTypes.FirstOrDefault(t => U.T(t) != null && GameBridge.InParent(weapon, t) != null) ?? _game.ShopTypes.FirstOrDefault();
                var wrapped = st != null ? Reflect.Wrap(st, shop) : null;
                if (wrapped != null) ok = Reflect.SetFirst(wrapped, _game.Cfg.List("ShopPriceMembers"), price, out _);
            }
            if (!ok && _game.WeaponType != null)
            {
                var w = Reflect.Wrap(_game.ConcreteType(weapon) ?? _game.WeaponType, weapon);
                if (w != null) ok = Reflect.SetFirst(w, _game.Cfg.List("Stat.Price"), price, out _);
            }
            if (!ok) Log.WarnOnce("price", "Could not find a price field to set for shop items (see ShopPriceMembers in _GameHooks.cfg).");
        }
    }
}
