using System;
using System.Collections.Generic;
using PTTRCustomWeapons.Core;
using UnityEngine;
using NV3 = System.Numerics.Vector3;

namespace PTTRCustomWeapons.Game
{
    /// <summary>
    /// The mod's own projectile simulation (the game has no notion of drop, bounce or area damage for
    /// custom guns). Projectiles are moved with raycasts every frame, so they never tunnel through walls.
    /// </summary>
    internal sealed class Projectiles
    {
        private sealed class Shot
        {
            public LoadedWeapon W = null!;
            public NV3 Pos, Vel, VisualOffset;
            public float Born, DieAt;
            public int Bounces, Pierce;
            public bool FromPlayer;
            public Component? Owner, WeaponComp;
            public float DamageMult = 1f;
            public GameObject? Visual;
            public readonly HashSet<int> Hit = new HashSet<int>();
        }

        private sealed class Effect
        {
            public GameObject Go = null!;
            public float Born, Life, StartScale, EndScale;
            public bool Scales = true;
        }

        private const int AllLayers = ~0;
        private readonly GameBridge _game;
        private readonly DamageDealer _damage;
        private readonly List<Shot> _shots = new List<Shot>();
        private readonly List<Effect> _effects = new List<Effect>();
        private readonly Random _rng = new Random();
        public const int MaxShots = 400;

        public Projectiles(GameBridge game, DamageDealer damage) { _game = game; _damage = damage; }

        public void Clear()
        {
            foreach (var s in _shots) if (U.Alive(s.Visual)) UnityEngine.Object.Destroy(s.Visual);
            foreach (var e in _effects) if (U.Alive(e.Go)) UnityEngine.Object.Destroy(e.Go);
            _shots.Clear();
            _effects.Clear();
        }

        /// <summary>Fires one trigger pull (all pellets) from <paramref name="origin"/>.</summary>
        public void Fire(CustomInstance ci, NV3 origin, NV3 aim, NV3 visualOrigin, bool fromPlayer, Component? owner, float extraSpread, float damageMult)
        {
            var def = ci.Def;
            for (int i = 0; i < def.Pellets; i++)
            {
                var dir = Ballistics.Spread(aim, def.Spread + extraSpread, _rng);
                if (def.Hitscan) Hitscan(ci, origin, dir, visualOrigin, fromPlayer, owner, damageMult);
                else Launch(ci, origin, dir, visualOrigin, fromPlayer, owner, damageMult);
            }
        }

        private void Launch(CustomInstance ci, NV3 origin, NV3 dir, NV3 visualOrigin, bool fromPlayer, Component? owner, float damageMult)
        {
            if (_shots.Count >= MaxShots) Kill(_shots[0]);
            var def = ci.Def;
            var s = new Shot
            {
                W = ci.Weapon,
                Pos = origin,
                Vel = dir * def.ProjectileSpeed,
                VisualOffset = visualOrigin - origin,
                Born = Time.time,
                DieAt = Time.time + def.ProjectileLifetime,
                Bounces = def.ProjectileBounces,
                Pierce = def.Pierce,
                FromPlayer = fromPlayer,
                Owner = owner,
                WeaponComp = ci.WeaponComponent,
                DamageMult = damageMult,
            };
            s.Visual = MakeVisual(ci, def.ProjectileSize);
            UpdateVisual(s);
            _shots.Add(s);
        }

        private void Hitscan(CustomInstance ci, NV3 origin, NV3 dir, NV3 visualOrigin, bool fromPlayer, Component? owner, float damageMult)
        {
            var s = new Shot
            {
                W = ci.Weapon, Pos = origin, Vel = dir, FromPlayer = fromPlayer, Owner = owner, WeaponComp = ci.WeaponComponent,
                Pierce = ci.Def.Pierce, Bounces = 0, DamageMult = damageMult,
            };
            Trace(s, dir, ci.Def.Range, out var end, out _);
            if (ci.Def.ProjectileVisual != ProjectileVisual.None) Tracer(ci, visualOrigin, end);
        }

        public void Update()
        {
            float dt = Time.deltaTime;
            float now = Time.time;
            for (int i = _shots.Count - 1; i >= 0; i--)
            {
                var s = _shots[i];
                if (now >= s.DieAt) { Kill(s); continue; }
                if (dt <= 0) continue;
                var move = Ballistics.Step(ref s.Vel, s.W.Def.ProjectileDrop, dt);
                float len = move.Length();
                if (len > 1e-6f)
                {
                    bool alive = Trace(s, move / len, len, out var end, out var bounced);
                    if (!alive) { Kill(s); continue; }
                    if (!bounced) s.Pos = end;
                }
                UpdateVisual(s);
            }
            for (int i = _effects.Count - 1; i >= 0; i--)
            {
                var e = _effects[i];
                float t = (now - e.Born) / e.Life;
                if (t >= 1f || !U.Alive(e.Go))
                {
                    if (U.Alive(e.Go)) UnityEngine.Object.Destroy(e.Go);
                    _effects.RemoveAt(i);
                    continue;
                }
                if (!e.Scales) continue;
                float sc = e.StartScale + (e.EndScale - e.StartScale) * t;
                e.Go.transform.localScale = U.V(sc, sc, sc);
            }
        }

        /// <summary>
        /// Moves along a ray, handling hits. Returns false if the shot is used up.
        /// <paramref name="end"/> is where it stopped; <paramref name="bounced"/> means Pos/Vel were already updated.
        /// </summary>
        private bool Trace(Shot s, NV3 dir, float distance, out NV3 end, out bool bounced)
        {
            var origin = s.Pos;
            float remaining = distance;
            bounced = false;
            for (int guard = 0; guard < 12 && remaining > 0.0001f; guard++)
            {
                if (!Physics.Raycast(U.V(origin), U.V(dir), out RaycastHit hit, remaining, AllLayers, QueryTriggerInteraction.Ignore))
                {
                    end = origin + dir * remaining;
                    return true;
                }
                var point = U.N(hit.point);
                var col = hit.collider;
                float step = Math.Max(hit.distance, 0f) + 0.02f;

                if (!U.Alive(col) || BelongsTo(col, s.Owner))
                {
                    origin = point + dir * 0.02f;
                    remaining -= step;
                    continue;
                }

                var enemy = GameBridge.InParent(col, _game.EnemyType);
                var player = enemy == null ? GameBridge.InParent(col, _game.PlayerType) : null;
                bool isTarget = s.FromPlayer ? enemy != null : player != null;
                bool isFriend = s.FromPlayer ? player != null : enemy != null;

                if (isFriend)
                {
                    origin = point + dir * 0.02f;
                    remaining -= step;
                    continue;
                }

                if (isTarget)
                {
                    var target = (enemy ?? player)!;
                    int id = target.GetInstanceID();
                    if (s.Hit.Add(id))
                    {
                        var def = s.W.Def;
                        var info = MakeHit(s, point, dir, def.Damage * s.DamageMult);
                        _damage.Damage(col, info, targetIsPlayer: !s.FromPlayer);
                        Push(col, dir, def.DamageForce, point);
                    }
                    if (s.Pierce > 0)
                    {
                        s.Pierce--;
                        origin = point + dir * 0.05f;
                        remaining -= step;
                        continue;
                    }
                    Explode(s, point);
                    end = point;
                    return false;
                }

                // world geometry or props
                Push(col, dir, s.W.Def.DamageForce * 0.5f, point);
                if (s.Bounces > 0 && s.Vel.LengthSquared() > 0.01f && !s.W.Def.Hitscan)
                {
                    s.Bounces--;
                    var normal = U.N(hit.normal);
                    s.Vel = Ballistics.Bounce(s.Vel, normal, s.W.Def.Bounciness);
                    s.Pos = point + normal * 0.02f;
                    bounced = true;
                    end = s.Pos;
                    if (s.Vel.Length() < 1.5f) { Explode(s, point); return false; }
                    return true;
                }
                Explode(s, point);
                end = point;
                return false;
            }
            end = origin;
            return true;
        }

        private static HitInfo MakeHit(Shot s, NV3 point, NV3 dir, float damage) => new HitInfo
        {
            Damage = damage,
            Point = point,
            Direction = dir,
            Force = s.W.Def.DamageForce,
            DamageType = s.W.Def.DamageType,
            Attacker = s.Owner,
            WeaponComp = s.WeaponComp,
        };

        private static bool BelongsTo(Component col, Component? owner)
        {
            if (!U.Alive(owner)) return false;
            try
            {
                var t = col.transform;
                var ownerT = owner.transform;
                for (int i = 0; i < 64 && U.Alive(t); i++)
                {
                    if (t == ownerT) return true;
                    t = t.parent;
                }
            }
            catch { }
            return false;
        }

        private static void Push(Component col, NV3 dir, float force, NV3 point)
        {
            if (force <= 0) return;
            try
            {
                var c = col.TryCast<Collider>();
                var rb = c?.attachedRigidbody;
                if (U.Alive(rb) && !rb.isKinematic) rb.AddForce(U.V(dir * force), ForceMode.Impulse);
            }
            catch { }
        }

        private void Explode(Shot s, NV3 point)
        {
            var def = s.W.Def;
            if (def.AreaOfEffect <= 0) return;
            float r = def.AreaOfEffect;
            Flash(s.W, point, r);
            try
            {
                var cols = Physics.OverlapSphere(U.V(point), r, AllLayers, QueryTriggerInteraction.Ignore);
                var damaged = new HashSet<int>(s.Hit);
                for (int i = 0; i < cols.Length; i++)
                {
                    var col = cols[i];
                    if (!U.Alive(col)) continue;
                    try
                    {
                        var rb = col.attachedRigidbody;
                        if (U.Alive(rb) && !rb.isKinematic)
                            rb.AddExplosionForce(def.DamageForce * 2f, U.V(point), r, 0.4f, ForceMode.Impulse);
                    }
                    catch { }

                    var target = s.FromPlayer ? GameBridge.InParent(col, _game.EnemyType) : GameBridge.InParent(col, _game.PlayerType);
                    if (target == null || !damaged.Add(target.GetInstanceID())) continue;
                    var tp = U.N(target.transform.position);
                    float dist = NV3.Distance(tp, point);
                    float dmg = Ballistics.AreaDamage(def.Damage * s.DamageMult, r, Math.Min(dist, r), def.AreaFalloff);
                    if (dmg <= 0) continue;
                    var dir = dist > 0.01f ? (tp - point) / dist : NV3.UnitY;
                    _damage.Damage(col, MakeHit(s, point, dir, dmg), targetIsPlayer: !s.FromPlayer);
                }
            }
            catch (Exception ex) { Log.WarnOnce("explode", "Explosion failed: " + ex.Message); }
        }

        private void Kill(Shot s)
        {
            if (U.Alive(s.Visual)) UnityEngine.Object.Destroy(s.Visual);
            _shots.Remove(s);
        }

        // ------------------------------------------------------------------ visuals

        private static void UpdateVisual(Shot s)
        {
            if (!U.Alive(s.Visual)) return;
            // the shot starts at the camera; the visual starts at the gun and catches up quickly
            float age = Time.time - s.Born;
            float k = Math.Max(0f, 1f - age / 0.12f);
            var t = s.Visual.transform;
            t.position = U.V(s.Pos + s.VisualOffset * k);
            if (s.Vel.LengthSquared() > 1e-6f) t.rotation = U.Q(Ballistics.LookRotation(s.Vel));
        }

        private GameObject? MakeVisual(CustomInstance ci, float size)
        {
            var def = ci.Def;
            try
            {
                switch (def.ProjectileVisual)
                {
                    case ProjectileVisual.None:
                        return null;
                    case ProjectileVisual.Model:
                    {
                        var data = ci.Weapon.ProjectileMesh ?? ci.Weapon.Mesh;
                        var mesh = AssetFactory.GetMesh(data, "CW_Proj_" + def.Id);
                        if (mesh == null) return null;
                        var tex = AssetFactory.GetTexture(ci.Weapon.TextureBytes ?? data.EmbeddedTexture, "CW_" + def.Id);
                        var mat = AssetFactory.GetMaterial(def.Id, SourceMaterial(ci), tex, def.Tint);
                        var holder = new GameObject("CW_Projectile_" + def.Id);
                        var model = AssetFactory.AddModel(holder.transform, "CW_ProjModel", mesh, mat);
                        var (min, max) = data.Bounds();
                        var sz = max - min;
                        float longest = MathF.Max(sz.X, MathF.Max(sz.Y, sz.Z));
                        float sc = longest > 1e-5f ? size / longest : 1f;
                        if (model != null)
                        {
                            model.transform.localScale = U.V(sc, sc, sc);
                            model.transform.localPosition = U.V(-(min + max) * 0.5f * sc);
                        }
                        return holder;
                    }
                    default:
                    {
                        var prim = def.ProjectileVisual == ProjectileVisual.Sphere ? PrimitiveType.Sphere : PrimitiveType.Cube;
                        var go = GameObject.CreatePrimitive(prim);
                        go.name = "CW_Projectile_" + def.Id;
                        RemoveCollider(go);
                        var scale = def.ProjectileVisual switch
                        {
                            ProjectileVisual.Bullet => U.V(size, size, size * 5f),
                            ProjectileVisual.Arrow => U.V(size * 0.3f, size * 0.3f, size * 10f),
                            _ => U.V(size, size, size),
                        };
                        go.transform.localScale = scale;
                        Paint(go, ci, def.ProjectileColor);
                        return go;
                    }
                }
            }
            catch (Exception ex)
            {
                Log.WarnOnce("visual" + def.Id, $"[{def.Id}] projectile visual failed: {ex.Message}");
                return null;
            }
        }

        private static Material? SourceMaterial(CustomInstance ci)
        {
            try
            {
                var r = ci.Model?.GetComponent(U.T<MeshRenderer>())?.TryCast<Renderer>();
                return U.Alive(r) ? r.sharedMaterial : null;
            }
            catch { return null; }
        }

        private static void RemoveCollider(GameObject go)
        {
            try
            {
                var c = go.GetComponent(U.T<Collider>());
                if (U.Alive(c)) UnityEngine.Object.Destroy(c);
            }
            catch { }
        }

        private static void Paint(GameObject go, CustomInstance ci, System.Numerics.Vector4 color)
        {
            try
            {
                var r = go.GetComponent(U.T<MeshRenderer>())?.TryCast<Renderer>();
                if (!U.Alive(r)) return;
                var mat = AssetFactory.GetMaterial("proj_" + ci.Def.Id, SourceMaterial(ci) ?? r.sharedMaterial, null, color);
                if (mat != null) r.sharedMaterial = mat;
            }
            catch { }
        }

        private void Tracer(CustomInstance ci, NV3 from, NV3 to)
        {
            try
            {
                var len = NV3.Distance(from, to);
                if (len < 0.05f) return;
                var go = GameObject.CreatePrimitive(PrimitiveType.Cube);
                go.name = "CW_Tracer";
                RemoveCollider(go);
                float w = Math.Max(0.01f, ci.Def.ProjectileSize * 0.4f);
                go.transform.position = U.V((from + to) * 0.5f);
                go.transform.rotation = U.Q(Ballistics.LookRotation(to - from));
                go.transform.localScale = U.V(w, w, len);
                Paint(go, ci, ci.Def.ProjectileColor);
                _effects.Add(new Effect { Go = go, Born = Time.time, Life = 0.07f, Scales = false });
            }
            catch { }
        }

        private void Flash(LoadedWeapon w, NV3 point, float radius)
        {
            try
            {
                var go = GameObject.CreatePrimitive(PrimitiveType.Sphere);
                go.name = "CW_Explosion";
                RemoveCollider(go);
                go.transform.position = U.V(point);
                var r = go.GetComponent(U.T<MeshRenderer>())?.TryCast<Renderer>();
                if (U.Alive(r))
                {
                    var mat = AssetFactory.GetMaterial("boom_" + w.Def.Id, r.sharedMaterial, null, w.Def.ProjectileColor);
                    if (mat != null) r.sharedMaterial = mat;
                }
                _effects.Add(new Effect { Go = go, Born = Time.time, Life = 0.25f, StartScale = radius * 0.3f, EndScale = radius * 2f });
            }
            catch { }
        }
    }
}
