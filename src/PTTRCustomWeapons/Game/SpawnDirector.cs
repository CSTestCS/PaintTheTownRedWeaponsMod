using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using PTTRCustomWeapons.Core;
using UnityEngine;
using NV3 = System.Numerics.Vector3;

namespace PTTRCustomWeapons.Game
{
    /// <summary>
    /// Decides where custom weapons appear: lying on the ground, in shops, in enemies' hands and in the
    /// starting area. It works by copying/reskinning the game's own weapons, so the game treats them as
    /// ordinary weapons (pick up, throw, drop, buy, break).
    /// </summary>
    internal sealed class SpawnDirector
    {
        private readonly GameBridge _game;
        private readonly WeaponSkinner _skinner;
        private readonly Combat _combat;
        private readonly Func<IReadOnlyList<LoadedWeapon>> _weapons;
        private readonly System.Random _rng = new System.Random();

        private string _scene = "";
        private bool _gameplayScene;
        private bool _lastWasMenu = true;
        private bool _isStartingArea;
        private float _setupAt = -1f;
        private float _setupGiveUpAt;
        private float _nextScan;
        private readonly HashSet<int> _seenEnemies = new HashSet<int>();
        private readonly Dictionary<int, int> _enemyScans = new Dictionary<int, int>();
        private readonly HashSet<int> _seenLoose = new HashSet<int>();
        private float _nextFloorCheck;
        private readonly HashSet<int> _seenShopWeapons = new HashSet<int>();

        public string SceneName => _scene;
        public bool InGameplay => _gameplayScene;
        public event Action? FirstGameplaySetup;
        private bool _firstSetupDone;

        public SpawnDirector(GameBridge game, WeaponSkinner skinner, Combat combat, Func<IReadOnlyList<LoadedWeapon>> weapons)
        {
            _game = game;
            _skinner = skinner;
            _combat = combat;
            _weapons = weapons;
        }

        private float Chance(float c) => Math.Clamp(c * _game.Cfg.Float("ChanceMultiplier", 1f), 0f, 1f);

        private static bool Matches(string pattern, string text)
        {
            if (string.IsNullOrWhiteSpace(pattern)) return false;
            try { return Regex.IsMatch(text, pattern); } catch { return false; }
        }

        public void OnSceneLoaded(string name)
        {
            _scene = name;
            _seenEnemies.Clear();
            _enemyScans.Clear();
            _seenShopWeapons.Clear();
            _seenLoose.Clear();
            _combat.Clear();
            _skinner.Cleanup();

            bool menu = Matches(_game.Cfg.Raw("MenuScenes"), name);
            var gameplayPattern = _game.Cfg.Raw("GameplayScenes");
            _gameplayScene = !menu && (string.IsNullOrWhiteSpace(gameplayPattern) || Matches(gameplayPattern, name));
            Log.Info($"Scene loaded: \"{name}\" ({(menu ? "menu" : _gameplayScene ? "gameplay" : "ignored")})");

            if (menu) { _lastWasMenu = true; _setupAt = -1f; return; }
            if (!_gameplayScene) { _setupAt = -1f; return; }

            _isStartingArea = Matches(_game.Cfg.Raw("StartingAreaScenes"), name) || _lastWasMenu;
            _lastWasMenu = false;
            _setupAt = Time.time + Math.Max(0f, _game.Cfg.Float("SceneSetupDelay", 2.5f));
            _setupGiveUpAt = _setupAt + 30f;
        }

        public void Update()
        {
            if (!_gameplayScene) return;
            float now = Time.time;

            if (_setupAt > 0 && now >= _setupAt)
            {
                _game.Resolve();
                if (_game.Player == null && now < _setupGiveUpAt)
                {
                    _setupAt = now + 1f; // level not ready yet
                }
                else
                {
                    _setupAt = -1f;
                    if (!_firstSetupDone) { _firstSetupDone = true; FirstGameplaySetup?.Invoke(); }
                    SafeRun("ground spawns", SpawnOnGround);
                    if (_isStartingArea) SafeRun("starting area", SpawnStartingArea);
                }
            }

            if (now >= _nextScan && _setupAt < 0 && _game.Resolved)
            {
                _nextScan = now + Math.Max(0.2f, _game.Cfg.Float("ScanInterval", 1f));
                _skinner.Cleanup();
                _skinner.KeepOriginalsHidden();
                SafeRun("enemy scan", ScanEnemies);
                SafeRun("shop scan", ScanShops);
            }

            // Beneath can build a new floor without loading a new scene: many new loose weapons = new floor.
            if (now >= _nextFloorCheck && _setupAt < 0 && _game.Resolved)
            {
                _nextFloorCheck = now + 4f;
                SafeRun("new floor check", CheckForNewFloor);
            }
        }

        private static void SafeRun(string what, Action a)
        {
            try { a(); }
            catch (Exception ex) { Log.WarnOnce("run" + what, $"{what} failed: {ex}"); }
        }

        // ------------------------------------------------------------------ choosing

        private LoadedWeapon? Pick(Func<WeaponDefinition, bool> allowed, Func<WeaponDefinition, float> chance, bool? ranged = null)
        {
            var pool = _weapons().Where(w => allowed(w.Def) && w.Def.SpawnWeight > 0).ToList();
            if (ranged.HasValue)
            {
                var same = pool.Where(w => w.Def.IsRanged == ranged.Value).ToList();
                if (same.Count > 0) pool = same;
            }
            // each weapon rolls its own chance; winners are picked by weight
            var winners = pool.Where(w => _rng.NextDouble() < Chance(chance(w.Def))).ToList();
            if (winners.Count == 0) return null;
            float total = winners.Sum(w => w.Def.SpawnWeight);
            float roll = (float)_rng.NextDouble() * total;
            foreach (var w in winners)
            {
                roll -= w.Def.SpawnWeight;
                if (roll <= 0) return w;
            }
            return winners[^1];
        }

        /// <summary>A game weapon to copy for this custom weapon: Base Weapon name, else same melee/ranged kind.</summary>
        public Component? FindTemplate(WeaponDefinition def, List<Component>? sceneWeapons = null)
        {
            sceneWeapons ??= LooseWeaponsInScene();
            var candidates = sceneWeapons.Where(w => !_skinner.IsCustom(w)).ToList();
            var assets = GameBridge.FindAllIncludingAssets(_game.WeaponType).Where(w => !_skinner.IsCustom(w) && !w.gameObject.name.StartsWith("CW_")).ToList();

            if (!string.IsNullOrWhiteSpace(def.BaseWeapon))
            {
                var byName = candidates.Concat(assets).FirstOrDefault(w => w.gameObject.name.IndexOf(def.BaseWeapon, StringComparison.OrdinalIgnoreCase) >= 0
                                                                          || (_game.ConcreteType(w)?.Name ?? "").Equals(def.BaseWeapon, StringComparison.OrdinalIgnoreCase));
                if (byName != null) return byName;
                Log.WarnOnce("base" + def.Id, $"[{def.Id}] Base Weapon \"{def.BaseWeapon}\" not found; using a similar weapon instead.");
            }
            foreach (var pool in new[] { candidates, assets })
            {
                var same = pool.Where(w => _game.IsRangedGameWeapon(w) == def.IsRanged).ToList();
                if (same.Count > 0) return same[_rng.Next(same.Count)];
            }
            if (candidates.Count > 0) return candidates[_rng.Next(candidates.Count)];
            return assets.Count > 0 ? assets[_rng.Next(assets.Count)] : null;
        }

        /// <summary>Weapons in the level that nobody holds and that are not shop items.</summary>
        private List<Component> LooseWeaponsInScene()
        {
            var list = new List<Component>();
            foreach (var w in GameBridge.FindAll(_game.WeaponType))
            {
                if (!w.gameObject.activeInHierarchy) continue;
                if (_game.HolderOf(w, out _) == Holder.None) list.Add(w);
            }
            return list;
        }

        /// <summary>Copies a game weapon and turns the copy into <paramref name="lw"/>.</summary>
        public CustomInstance? SpawnCopy(LoadedWeapon lw, Component template, NV3 position)
        {
            var copyObj = UnityEngine.Object.Instantiate(template.gameObject);
            var copy = copyObj?.TryCast<GameObject>();
            if (copy == null) return null;
            copy.transform.SetParent(null, true);
            copy.transform.position = U.V(position);
            copy.transform.rotation = U.Q(System.Numerics.Quaternion.CreateFromAxisAngle(NV3.UnitY, (float)(_rng.NextDouble() * Math.PI * 2)));
            if (!copy.activeSelf) copy.SetActive(true);

            Component? weapon = null;
            var il2 = U.T(_game.WeaponType!);
            if (il2 != null) weapon = copy.GetComponent(il2);
            if (!U.Alive(weapon)) { UnityEngine.Object.Destroy(copy); return null; }
            return _skinner.Apply(weapon, lw);
        }

        private static bool FindFloor(NV3 around, out NV3 floor)
        {
            var from = around + NV3.UnitY * 1.5f;
            if (Physics.Raycast(U.V(from), U.V(0, -1, 0), out RaycastHit hit, 6f, ~0, QueryTriggerInteraction.Ignore))
            {
                floor = U.N(hit.point) + NV3.UnitY * 0.15f;
                return true;
            }
            floor = around;
            return false;
        }

        // ------------------------------------------------------------------ ground / start

        private void CheckForNewFloor()
        {
            var loose = LooseWeaponsInScene();
            var fresh = loose.Where(w => !_seenLoose.Contains(w.GetInstanceID()) && !_skinner.IsCustom(w)).ToList();
            int threshold = (int)_game.Cfg.Float("NewFloorWeaponCount", 3);
            if (fresh.Count >= threshold && threshold > 0)
            {
                Log.Info($"{fresh.Count} new weapons appeared - treating it as a new floor.");
                SpawnOnGround(fresh);
            }
            else foreach (var w in fresh) _seenLoose.Add(w.GetInstanceID()); // dropped/thrown weapons etc.
        }

        private void SpawnOnGround() => SpawnOnGround(null);

        private void SpawnOnGround(List<Component>? anchors)
        {
            if (_game.WeaponType == null) return;
            var all = LooseWeaponsInScene();
            foreach (var w in all) _seenLoose.Add(w.GetInstanceID());
            var loose = anchors ?? all;
            if (loose.Count == 0)
            {
                Log.Info("No weapons lying around in this scene; nothing to place custom weapons next to.");
                return;
            }
            int max = (int)_game.Cfg.Float("MaxGroundSpawnsPerScene", 6);
            int spawned = 0;
            foreach (var lw in _weapons().OrderBy(_ => _rng.Next()))
            {
                if (spawned >= max) break;
                var def = lw.Def;
                if (!def.AppearsOnGround || _rng.NextDouble() >= Chance(def.GroundChance)) continue;
                var template = FindTemplate(def, loose);
                if (template == null) continue;
                // put it next to a random existing weapon so it lands somewhere reachable
                var anchor = U.N(loose[_rng.Next(loose.Count)].transform.position);
                var offset = new NV3((float)(_rng.NextDouble() * 2 - 1), 0, (float)(_rng.NextDouble() * 2 - 1)) * 1.2f;
                FindFloor(anchor + offset, out var pos);
                var ci = SpawnCopy(lw, template, pos);
                if (ci != null)
                {
                    _seenLoose.Add(ci.WeaponComponent.GetInstanceID());
                    spawned++;
                    Log.Info($"[{def.Id}] placed on the ground at {pos}");
                }
            }
        }

        private void SpawnStartingArea()
        {
            var starters = _weapons().Where(w => w.Def.AppearsAtStartingArea).ToList();
            if (starters.Count == 0) return;
            var player = _game.Player;
            if (player == null) return;
            var cam = Camera.main;
            var basePos = U.N(player.transform.position);
            var fwd = U.Alive(cam) ? U.N(cam.transform.forward) : NV3.UnitZ;
            fwd.Y = 0;
            fwd = fwd.LengthSquared() > 0.01f ? NV3.Normalize(fwd) : NV3.UnitZ;
            var right = NV3.Cross(NV3.UnitY, fwd);
            var loose = LooseWeaponsInScene();
            for (int i = 0; i < starters.Count; i++)
            {
                var lw = starters[i];
                var template = FindTemplate(lw.Def, loose);
                if (template == null) { Log.Warn($"[{lw.Def.Id}] no game weapon available to copy for the starting area."); continue; }
                var spot = basePos + fwd * 2.2f + right * ((i - (starters.Count - 1) * 0.5f) * 0.9f);
                FindFloor(spot, out var pos);
                if (SpawnCopy(lw, template, pos) != null) Log.Info($"[{lw.Def.Id}] placed in the starting area");
            }
        }

        /// <summary>Debug hotkey: every custom weapon in front of the player.</summary>
        public void SpawnAllInFront()
        {
            _game.Resolve();
            var player = _game.Player;
            var cam = Camera.main;
            if (player == null || !U.Alive(cam)) { Log.Warn("Spawn hotkey: player not found (are you in a level?)."); return; }
            var all = _weapons();
            var camPos = U.N(cam.transform.position);
            var fwd = U.N(cam.transform.forward);
            fwd.Y = 0;
            fwd = fwd.LengthSquared() > 0.01f ? NV3.Normalize(fwd) : NV3.UnitZ;
            var right = NV3.Cross(NV3.UnitY, fwd);
            var loose = LooseWeaponsInScene();
            int ok = 0;
            for (int i = 0; i < all.Count; i++)
            {
                var template = FindTemplate(all[i].Def, loose);
                if (template == null) continue;
                var spot = camPos + fwd * 2f + right * ((i - (all.Count - 1) * 0.5f) * 0.8f);
                FindFloor(spot, out var pos);
                if (SpawnCopy(all[i], template, pos) != null) ok++;
            }
            Log.Info($"Spawned {ok}/{all.Count} custom weapons in front of the player.");
            if (ok == 0 && all.Count > 0) Log.Warn("Nothing could be spawned: no game weapon was found to copy. See _GameReport.txt.");
        }

        // ------------------------------------------------------------------ enemies / shops

        private void ScanEnemies()
        {
            if (_game.EnemyType == null) return;
            foreach (var enemy in GameBridge.FindAll(_game.EnemyType))
            {
                var held = _game.HeldWeapon(enemy, _game.EnemyType, "EnemyWeaponMembers");
                int id = enemy.GetInstanceID();
                // weapons dropped by dying enemies must not look like a freshly generated floor
                if (held != null) _seenLoose.Add(held.GetInstanceID());
                // enemies may be handed their weapon a moment after spawning: wait for it (up to 3 scans)
                _enemyScans.TryGetValue(id, out var scans);
                _enemyScans[id] = scans + 1;
                bool ready = held != null || scans >= 2;
                if (ready && _seenEnemies.Add(id) && !_game.IsDead(enemy))
                {
                    bool? ranged = held != null ? _game.IsRangedGameWeapon(held) : null;
                    var lw = Pick(d => d.UsedByEnemies, d => d.EnemyChance, ranged);
                    if (lw != null)
                    {
                        if (held != null && !_skinner.IsCustom(held))
                        {
                            _skinner.Apply(held, lw);
                            Log.Info($"[{lw.Def.Id}] given to enemy {enemy.gameObject.name}");
                        }
                        else if (held == null) TryEquip(enemy, lw);
                        held = _game.HeldWeapon(enemy, _game.EnemyType, "EnemyWeaponMembers");
                    }
                }
                var ci = _skinner.Get(held);
                if (ci != null && ci.Def.IsRanged) _combat.TrackEnemy(enemy, ci);
            }
        }

        private void TryEquip(Component enemy, LoadedWeapon lw)
        {
            var template = FindTemplate(lw.Def);
            if (template == null || _game.EnemyType == null || _game.WeaponType == null) return;
            var ci = SpawnCopy(lw, template, U.N(enemy.transform.position) + NV3.UnitY);
            if (ci == null) return;
            var wrappedEnemy = Reflect.Wrap(_game.ConcreteTypeOf(enemy, _game.EnemyType), enemy);
            var weaponType = _game.ConcreteType(ci.WeaponComponent) ?? _game.WeaponType;
            var wrappedWeapon = Reflect.Wrap(weaponType, ci.WeaponComponent);
            if (wrappedEnemy == null || wrappedWeapon == null) { UnityEngine.Object.Destroy(ci.Root); return; }

            foreach (var name in _game.Cfg.List("EnemyEquipMethods"))
            {
                foreach (var m in wrappedEnemy.GetType().GetMethods().Where(x => x.Name.Equals(name, StringComparison.OrdinalIgnoreCase)))
                {
                    var ps = m.GetParameters();
                    if (ps.Length == 0 || !ps[0].ParameterType.IsAssignableFrom(wrappedWeapon.GetType())) continue;
                    var args = new object?[ps.Length];
                    args[0] = wrappedWeapon;
                    for (int i = 1; i < ps.Length; i++)
                        args[i] = ps[i].HasDefaultValue ? ps[i].DefaultValue : ps[i].ParameterType.IsValueType ? Activator.CreateInstance(ps[i].ParameterType) : null;
                    try
                    {
                        m.Invoke(wrappedEnemy, args);
                        Log.Info($"[{lw.Def.Id}] equipped on enemy via {m.Name}");
                        return;
                    }
                    catch { }
                }
            }
            Log.InfoOnce("noequip", "Enemies without a weapon can't be armed (no EnemyEquipMethods matched); only armed enemies get custom weapons.");
            UnityEngine.Object.Destroy(ci.Root);
        }

        private void ScanShops()
        {
            if (_game.ShopTypes.Count == 0 || _game.WeaponType == null) return;
            var il2Weapon = U.T(_game.WeaponType);
            if (il2Weapon == null) return;
            foreach (var shopType in _game.ShopTypes)
            {
                foreach (var shop in GameBridge.FindAll(shopType))
                {
                    Il2CppInterop.Runtime.InteropTypes.Arrays.Il2CppReferenceArray<Component> items;
                    try { items = shop.GetComponentsInChildren(il2Weapon, true); } catch { continue; }
                    for (int i = 0; i < items.Length; i++)
                    {
                        var w = items[i];
                        if (!U.Alive(w) || !_seenShopWeapons.Add(w.GetInstanceID()) || _skinner.IsCustom(w)) continue;
                        var lw = Pick(d => d.AppearsInShop, d => d.ShopChance, _game.IsRangedGameWeapon(w));
                        if (lw == null) continue;
                        _skinner.Apply(w, lw);
                        _skinner.ApplyPrice(w, shop, lw.Def.ShopPrice);
                        Log.Info($"[{lw.Def.Id}] put up for sale in {shopType.Name} for {lw.Def.ShopPrice}");
                    }
                }
            }
        }
    }
}
