using System;
using System.Collections.Generic;
using System.Linq;
using PTTRCustomWeapons.Core;
using UnityEngine;
using Object = UnityEngine.Object;

namespace PTTRCustomWeapons.Game
{
    internal enum Holder { None, Player, Enemy, Shop }

    /// <summary>Everything the mod knows about the game's own classes, resolved by name from _GameHooks.cfg.</summary>
    internal sealed class GameBridge
    {
        public readonly HooksConfig Cfg;
        public Type? PlayerType, EnemyType, WeaponType;
        public readonly List<Type> ShopTypes = new List<Type>();
        public readonly List<Type> HitPartTypes = new List<Type>();
        public bool Resolved;

        private Component? _player;
        private float _nextPlayerSearch;

        public GameBridge(HooksConfig cfg) { Cfg = cfg; }

        private static bool IsComponentType(Type t) => typeof(Component).IsAssignableFrom(t);

        public void Resolve()
        {
            if (Resolved) return;
            PlayerType = Reflect.FindFirstType(Cfg.List("PlayerTypes"), IsComponentType);
            EnemyType = Reflect.FindFirstType(Cfg.List("EnemyTypes"), IsComponentType);

            foreach (var name in Cfg.List("WeaponTypes"))
            {
                if (name.Equals("auto", StringComparison.OrdinalIgnoreCase))
                {
                    if (PlayerType == null) continue;
                    foreach (var memberName in Cfg.List("PlayerWeaponMembers"))
                    {
                        var m = Reflect.FindMember(PlayerType, memberName, t => IsComponentType(t) && t != typeof(Component) && !typeof(Transform).IsAssignableFrom(t), "weapon");
                        if (m != null) { WeaponType = Reflect.MemberType(m); break; }
                    }
                }
                else WeaponType = Reflect.FindType(name);
                if (WeaponType != null && IsComponentType(WeaponType)) break;
                WeaponType = null;
            }

            foreach (var n in Cfg.List("ShopTypes"))
            {
                var t = Reflect.FindType(n);
                if (t != null && IsComponentType(t) && !ShopTypes.Contains(t)) ShopTypes.Add(t);
            }
            foreach (var n in Cfg.List("HitPartTypes"))
            {
                var t = Reflect.FindType(n);
                if (t != null && IsComponentType(t) && !HitPartTypes.Contains(t)) HitPartTypes.Add(t);
            }

            Log.Info($"Game classes: player={PlayerType?.FullName ?? "NOT FOUND"}, enemy={EnemyType?.FullName ?? "NOT FOUND"}, " +
                     $"weapon={WeaponType?.FullName ?? "NOT FOUND"}, shop=[{string.Join(", ", ShopTypes.Select(t => t.Name))}], " +
                     $"hit parts=[{string.Join(", ", HitPartTypes.Select(t => t.Name))}]");
            if (PlayerType == null || WeaponType == null)
                Log.Warn("Some game classes were not found. Check CustomWeapons/_GameReport.txt and edit CustomWeapons/_GameHooks.cfg.");
            Resolved = true;
        }

        // ------------------------------------------------------------------ finding objects

        public static List<Component> FindAll(Type? gameType)
        {
            var list = new List<Component>();
            if (gameType == null) return list;
            var il2 = U.T(gameType);
            if (il2 == null) return list;
            try
            {
                var arr = Object.FindObjectsOfType(il2);
                if (arr == null) return list;
                for (int i = 0; i < arr.Length; i++)
                {
                    var o = arr[i];
                    if (!U.Alive(o)) continue;
                    var c = o.TryCast<Component>();
                    if (c != null) list.Add(c);
                }
            }
            catch (Exception ex) { Log.WarnOnce("findall" + gameType.Name, $"Could not list {gameType.Name} objects: {ex.Message}"); }
            return list;
        }

        /// <summary>Every loaded object of the type, including prefabs and inactive ones.</summary>
        public static List<Component> FindAllIncludingAssets(Type? gameType)
        {
            var list = new List<Component>();
            if (gameType == null) return list;
            var il2 = U.T(gameType);
            if (il2 == null) return list;
            try
            {
                var arr = Resources.FindObjectsOfTypeAll(il2);
                for (int i = 0; i < arr.Length; i++)
                {
                    var c = arr[i]?.TryCast<Component>();
                    if (c != null && U.Alive(c)) list.Add(c);
                }
            }
            catch (Exception ex) { Log.WarnOnce("findassets" + gameType.Name, $"Could not list {gameType.Name} assets: {ex.Message}"); }
            return list;
        }

        public Component? Player
        {
            get
            {
                if (U.Alive(_player)) return _player;
                if (Time.time < _nextPlayerSearch) return null;
                _nextPlayerSearch = Time.time + 1f;
                _player = FindAll(PlayerType).FirstOrDefault();
                return _player;
            }
        }

        public static Component? InParent(Component c, Type? gameType)
        {
            if (gameType == null || !U.Alive(c)) return null;
            var il2 = U.T(gameType);
            if (il2 == null) return null;
            try
            {
                var found = c.GetComponentInParent(il2);
                return U.Alive(found) ? found : null;
            }
            catch { return null; }
        }

        public Component? ShopInParent(Component c)
        {
            foreach (var t in ShopTypes)
            {
                var s = InParent(c, t);
                if (s != null) return s;
            }
            return null;
        }

        public Holder HolderOf(Component weapon, out Component? holder)
        {
            holder = InParent(weapon, PlayerType);
            if (holder != null) return Holder.Player;
            holder = InParent(weapon, EnemyType);
            if (holder != null) return Holder.Enemy;
            holder = ShopInParent(weapon);
            if (holder != null) return Holder.Shop;
            return Holder.None;
        }

        // ------------------------------------------------------------------ members

        /// <summary>The weapon component held by a player/enemy, or null.</summary>
        public Component? HeldWeapon(Component holder, Type? holderType, string membersKey)
        {
            if (holderType == null || WeaponType == null || !U.Alive(holder)) return null;
            var wrapped = Reflect.Wrap(holderType, holder);
            if (wrapped == null) return null;
            var weaponType = WeaponType;
            var value = Reflect.GetFirst(wrapped, Cfg.List(membersKey), t => weaponType.IsAssignableFrom(t) || t.IsAssignableFrom(weaponType), "heldweapon", out _);
            if (value is Il2CppInterop.Runtime.InteropTypes.Il2CppObjectBase obj)
            {
                var c = obj.TryCast<Component>();
                if (U.Alive(c)) return c;
            }
            return null;
        }

        public bool IsDead(Component enemy)
        {
            if (EnemyType == null) return false;
            var wrapped = Reflect.Wrap(EnemyType, enemy);
            if (wrapped == null) return true;
            var v = Reflect.GetFirst(wrapped, Cfg.List("EnemyDeadMembers"), t => t == typeof(bool), "bool", out _);
            return v is bool b && b;
        }

        /// <summary>Best guess whether a game weapon is a gun: a bool member, or words in its class name.</summary>
        public bool? IsRangedGameWeapon(Component weapon)
        {
            if (WeaponType == null) return null;
            var concrete = ConcreteType(weapon);
            var wrapped = Reflect.Wrap(concrete ?? WeaponType, weapon);
            if (wrapped != null)
            {
                var v = Reflect.GetFirst(wrapped, Cfg.List("WeaponRangedMembers"), t => t == typeof(bool), "bool", out _);
                if (v is bool b) return b;
            }
            var typeName = (concrete ?? WeaponType).Name;
            foreach (var word in Cfg.List("RangedClassWords"))
                if (typeName.IndexOf(word, StringComparison.OrdinalIgnoreCase) >= 0) return true;
            return concrete != null && concrete != WeaponType ? false : (bool?)null;
        }

        /// <summary>The most derived game class of a component (e.g. Gun when the field type is Weapon).</summary>
        public Type? ConcreteType(Component c) => WeaponType == null ? null : ConcreteTypeOf(c, WeaponType);

        public Type ConcreteTypeOf(Component c, Type fallback)
        {
            try
            {
                var cls = Il2CppInterop.Runtime.IL2CPP.il2cpp_object_get_class(c.Pointer);
                var il2Name = System.Runtime.InteropServices.Marshal.PtrToStringAnsi(Il2CppInterop.Runtime.IL2CPP.il2cpp_class_get_name(cls));
                var found = il2Name != null ? Reflect.FindType(il2Name) : null;
                return found != null && fallback.IsAssignableFrom(found) ? found : fallback;
            }
            catch { return fallback; }
        }
    }
}
