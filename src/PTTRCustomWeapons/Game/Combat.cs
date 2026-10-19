using System;
using System.Collections.Generic;
using System.Linq;
using PTTRCustomWeapons.Core;
using UnityEngine;
using NV3 = System.Numerics.Vector3;

namespace PTTRCustomWeapons.Game
{
    /// <summary>Firing logic for custom ranged weapons held by the player or by enemies.</summary>
    internal sealed class Combat
    {
        private sealed class Shooter
        {
            public Component Enemy = null!;
            public CustomInstance Gun = null!;
            public float NextVolley;
            public int VolleyLeft;
            public float NextShot;
        }

        private readonly GameBridge _game;
        private readonly WeaponSkinner _skinner;
        private readonly Projectiles _projectiles;
        private readonly Dictionary<int, Shooter> _shooters = new Dictionary<int, Shooter>();
        private readonly System.Random _rng = new System.Random();
        private readonly int _fireKey, _reloadKey;
        private float _nextHeldCheck;

        /// <summary>The custom weapon the player is holding right now (for the HUD).</summary>
        public CustomInstance? PlayerWeapon { get; private set; }

        public Combat(GameBridge game, WeaponSkinner skinner, Projectiles projectiles)
        {
            _game = game;
            _skinner = skinner;
            _projectiles = projectiles;
            _fireKey = Win32Input.KeyCode(game.Cfg.Raw("FireKey", "Mouse0"));
            _reloadKey = Win32Input.KeyCode(game.Cfg.Raw("ReloadKey", "R"));
        }

        public void Clear() { _shooters.Clear(); PlayerWeapon = null; }

        public void Update()
        {
            UpdatePlayer();
            UpdateEnemies();
        }

        // ------------------------------------------------------------------ player

        private void UpdatePlayer()
        {
            if (Time.time >= _nextHeldCheck || (PlayerWeapon != null && !U.Alive(PlayerWeapon.Root)))
            {
                _nextHeldCheck = Time.time + 0.1f;
                var player = _game.Player;
                var held = player != null ? _game.HeldWeapon(player, _game.PlayerType, "PlayerWeaponMembers") : null;
                PlayerWeapon = _skinner.Get(held);
                if (PlayerWeapon != null) _skinner.EnforceGameGunAmmo(PlayerWeapon);
            }
            var ci = PlayerWeapon;
            if (ci == null || !ci.Def.IsRanged) return;
            var def = ci.Def;
            float now = Time.time;

            if (ci.Reloading && now >= ci.ReloadDoneAt) FinishReload(ci);

            bool canAct = Time.timeScale > 0f && Win32Input.GameHasFocus() && !CursorFree();
            if (!canAct) return;

            if (Win32Input.Pressed(_reloadKey) && !ci.Reloading && !def.InfiniteMagazine && ci.Magazine < def.MagazineSize && ci.Reserve > 0)
                StartReload(ci);

            // burst in progress
            if (ci.BurstLeft > 0 && now >= ci.NextBurstShotAt)
            {
                ci.BurstLeft--;
                ci.NextBurstShotAt = now + BurstGap(def);
                PlayerShoot(ci);
            }

            bool pressed = Win32Input.Pressed(_fireKey);
            bool down = Win32Input.Down(_fireKey);
            if (now < ci.NextShotAt) return;
            switch (def.FireMode)
            {
                case FireMode.Single:
                    if (pressed) { ci.NextShotAt = now + 1f / def.FireRate; PlayerShoot(ci); }
                    break;
                case FireMode.Automatic:
                    if (down) { ci.NextShotAt = now + 1f / def.FireRate; PlayerShoot(ci); }
                    break;
                case FireMode.Burst:
                    if (pressed && ci.BurstLeft == 0)
                    {
                        ci.NextShotAt = now + 1f / def.FireRate;
                        ci.BurstLeft = def.BurstCount - 1;
                        ci.NextBurstShotAt = now + BurstGap(def);
                        PlayerShoot(ci);
                    }
                    break;
            }
        }

        private static float BurstGap(WeaponDefinition def) => Math.Clamp(1f / (def.FireRate * Math.Max(1, def.BurstCount)), 0.04f, 0.15f);

        private static bool CursorFree()
        {
            // A visible, unlocked cursor means a menu, the shop UI or the pause screen is open.
            try { return Cursor.visible && Cursor.lockState == CursorLockMode.None; } catch { return false; }
        }

        private void PlayerShoot(CustomInstance ci)
        {
            var def = ci.Def;
            if (ci.Reloading) { ci.BurstLeft = 0; return; }
            if (!def.InfiniteMagazine)
            {
                if (ci.Magazine <= 0)
                {
                    ci.BurstLeft = 0;
                    if (ci.Reserve > 0) StartReload(ci);
                    return;
                }
                ci.Magazine--;
            }

            var cam = Camera.main;
            if (!U.Alive(cam)) return;
            var ct = cam.transform;
            var camPos = U.N(ct.position);
            var fwd = U.N(ct.forward);
            var right = U.N(ct.right);
            var up = U.N(ct.up);
            var origin = camPos + fwd * 0.3f;
            var visual = U.Alive(ci.Root) ? U.N(ci.Root.transform.position) + fwd * 0.3f : camPos + fwd * 0.8f + right * 0.15f - up * 0.15f;
            _projectiles.Fire(ci, origin, fwd, visual, fromPlayer: true, owner: _game.Player, extraSpread: 0f, damageMult: 1f);

            if (!def.InfiniteMagazine && ci.Magazine <= 0 && ci.Reserve > 0) StartReload(ci);
        }

        private static void StartReload(CustomInstance ci)
        {
            ci.BurstLeft = 0;
            if (ci.Def.ReloadTime <= 0) { FinishReload(ci); return; }
            ci.ReloadDoneAt = Time.time + ci.Def.ReloadTime;
        }

        private static void FinishReload(CustomInstance ci)
        {
            ci.ReloadDoneAt = -1f;
            int need = ci.Def.MagazineSize - ci.Magazine;
            if (need <= 0) return;
            int take = ci.Reserve == int.MaxValue ? need : Math.Min(need, ci.Reserve);
            ci.Magazine += take;
            if (ci.Reserve != int.MaxValue) ci.Reserve -= take;
        }

        // ------------------------------------------------------------------ enemies

        /// <summary>Called by the spawn director's periodic scan with every enemy currently holding a custom gun.</summary>
        public void TrackEnemy(Component enemy, CustomInstance gun)
        {
            if (!gun.Def.IsRanged) return;
            int id = enemy.GetInstanceID();
            if (_shooters.TryGetValue(id, out var s) && s.Gun == gun) return;
            _shooters[id] = new Shooter { Enemy = enemy, Gun = gun, NextVolley = Time.time + 1f + (float)_rng.NextDouble() };
        }

        private void UpdateEnemies()
        {
            if (_shooters.Count == 0) return;
            if (Time.timeScale <= 0f) return;
            var player = _game.Player;
            var cam = Camera.main;
            if (player == null || !U.Alive(cam)) return;
            var target = U.N(cam.transform.position) - NV3.UnitY * 0.35f;
            float now = Time.time;
            float maxRange = _game.Cfg.Float("EnemyMaxRange", 35f);

            foreach (var kv in _shooters.ToList())
            {
                var s = kv.Value;
                if (!U.Alive(s.Enemy) || !U.Alive(s.Gun.Root)) { _shooters.Remove(kv.Key); continue; }

                if (s.VolleyLeft > 0)
                {
                    if (now >= s.NextShot) { s.VolleyLeft--; s.NextShot = now + 1f / s.Gun.Def.FireRate; EnemyShoot(s, target); }
                    continue;
                }
                if (now < s.NextVolley) continue;
                s.NextVolley = now + 0.4f;

                // still holding it and alive?
                var holder = GameBridge.InParent(s.Gun.WeaponComponent, _game.EnemyType);
                if (holder == null || holder.GetInstanceID() != kv.Key || _game.IsDead(s.Enemy)) { _shooters.Remove(kv.Key); continue; }

                var muzzle = U.N(s.Gun.Root.transform.position) + NV3.UnitY * 0.05f;
                float dist = NV3.Distance(muzzle, target);
                if (dist > Math.Min(maxRange, s.Gun.Def.Range)) continue;
                if (!CanSee(muzzle, target, dist, s.Enemy)) continue;

                var def = s.Gun.Def;
                s.VolleyLeft = def.FireMode == FireMode.Automatic ? 3 + _rng.Next(3) : def.FireMode == FireMode.Burst ? def.BurstCount : 1;
                s.NextShot = now;
                s.NextVolley = now + def.EnemyFireInterval * (0.8f + 0.5f * (float)_rng.NextDouble());
            }
        }

        private void EnemyShoot(Shooter s, NV3 target)
        {
            if (!U.Alive(s.Gun.Root)) return;
            var muzzle = U.N(s.Gun.Root.transform.position) + NV3.UnitY * 0.05f;
            var aim = target - muzzle;
            var def = s.Gun.Def;
            // lead nothing, but aim a little higher for dropping projectiles
            if (!def.Hitscan && def.ProjectileDrop > 0 && def.ProjectileSpeed > 1f)
            {
                float t = aim.Length() / def.ProjectileSpeed;
                aim.Y += 0.5f * Ballistics.Gravity * def.ProjectileDrop * t * t;
            }
            _projectiles.Fire(s.Gun, muzzle, NV3.Normalize(aim), muzzle, fromPlayer: false, owner: s.Enemy,
                extraSpread: def.EnemyAccuracy, damageMult: def.EnemyDamageMultiplier);
        }

        private bool CanSee(NV3 from, NV3 to, float dist, Component self)
        {
            var dir = (to - from) / Math.Max(dist, 0.001f);
            var origin = from;
            float remaining = dist;
            for (int i = 0; i < 6 && remaining > 0.3f; i++)
            {
                if (!Physics.Raycast(U.V(origin), U.V(dir), out RaycastHit hit, remaining, ~0, QueryTriggerInteraction.Ignore)) return true;
                var col = hit.collider;
                if (!U.Alive(col)) return true;
                if (GameBridge.InParent(col, _game.PlayerType) != null) return true;
                var e = GameBridge.InParent(col, _game.EnemyType);
                if (e != null && e.GetInstanceID() == self.GetInstanceID())
                {
                    origin = U.N(hit.point) + dir * 0.05f;
                    remaining -= hit.distance + 0.05f;
                    continue;
                }
                return false;
            }
            return true;
        }
    }
}
