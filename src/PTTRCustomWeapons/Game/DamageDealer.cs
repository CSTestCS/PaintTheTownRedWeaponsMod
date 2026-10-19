using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Il2CppInterop.Runtime.InteropTypes;
using UnityEngine;
using NV3 = System.Numerics.Vector3;

namespace PTTRCustomWeapons.Game
{
    internal struct HitInfo
    {
        public float Damage;
        public NV3 Point;
        public NV3 Direction;
        public float Force;
        public string DamageType;
        public Component? Attacker;     // the player/enemy who fired
        public Component? WeaponComp;   // the weapon that fired
    }

    /// <summary>
    /// Calls the game's own damage functions. The exact signature is not known in advance, so every method
    /// listed in _GameHooks.cfg is inspected and its parameters are filled by type and name
    /// (damage, hit point, direction, force, attacker, damage type...). The best match is remembered.
    /// </summary>
    internal sealed class DamageDealer
    {
        private readonly GameBridge _game;
        private readonly Dictionary<(Type, string), MethodInfo?> _chosen = new Dictionary<(Type, string), MethodInfo?>();
        private readonly HashSet<MethodInfo> _broken = new HashSet<MethodInfo>();

        public DamageDealer(GameBridge game) { _game = game; }

        /// <summary>Damages whatever was hit. Returns true if a game damage function accepted it.</summary>
        public bool Damage(Component hitCollider, HitInfo hit, bool targetIsPlayer)
        {
            // 1) a body part / hitbox component (lets the game do dismemberment and headshots)
            if (!targetIsPlayer)
            {
                foreach (var partType in _game.HitPartTypes)
                {
                    var part = GameBridge.InParent(hitCollider, partType);
                    if (part != null && TryInvoke(part, partType, "HitPartDamageMethods", hit)) return true;
                }
            }
            // 2) the enemy / player itself
            var ownerType = targetIsPlayer ? _game.PlayerType : _game.EnemyType;
            var owner = GameBridge.InParent(hitCollider, ownerType);
            if (owner == null || ownerType == null) return false;
            var concrete = _game.ConcreteTypeOf(owner, ownerType);
            if (TryInvoke(owner, concrete, targetIsPlayer ? "PlayerDamageMethods" : "EnemyDamageMethods", hit)) return true;
            if (concrete != ownerType && TryInvoke(owner, ownerType, targetIsPlayer ? "PlayerDamageMethods" : "EnemyDamageMethods", hit)) return true;
            // 3) last resort: lower its health directly
            return SubtractHealth(owner, concrete, hit.Damage);
        }

        private bool TryInvoke(Component target, Type type, string methodsKey, HitInfo hit)
        {
            var wrapped = Reflect.Wrap(type, target);
            if (wrapped == null) return false;
            var key = (type, methodsKey);
            if (!_chosen.TryGetValue(key, out var method))
            {
                method = Choose(type, _game.Cfg.List(methodsKey));
                _chosen[key] = method;
                if (method != null)
                    Log.Info($"Damage on {type.Name} will use {method.Name}({string.Join(", ", method.GetParameters().Select(p => Reflect.Describe(p.ParameterType) + " " + p.Name))})");
                else
                    Log.WarnOnce("nodmg" + type.Name + methodsKey, $"No usable damage method found on {type.Name} (tried {methodsKey} in _GameHooks.cfg).");
            }
            if (method == null) return false;
            try
            {
                var args = BuildArgs(method, hit, out _);
                method.Invoke(wrapped, args);
                return true;
            }
            catch (Exception ex)
            {
                var inner = ex is TargetInvocationException tie && tie.InnerException != null ? tie.InnerException : ex;
                Log.WarnOnce("dmgfail" + method, $"Calling {type.Name}.{method.Name} failed: {inner.Message}");
                _broken.Add(method);
                _chosen[key] = Choose(type, _game.Cfg.List(methodsKey)); // pick the next best one next time
                return false;
            }
        }

        private MethodInfo? Choose(Type type, IReadOnlyList<string> names)
        {
            MethodInfo? best = null;
            int bestScore = int.MinValue;
            for (int i = 0; i < names.Count; i++)
            {
                var methods = type.GetMethods(BindingFlags.Public | BindingFlags.Instance)
                    .Where(m => m.Name.Equals(names[i], StringComparison.OrdinalIgnoreCase) && !m.IsGenericMethod && !_broken.Contains(m));
                foreach (var m in methods)
                {
                    BuildArgs(m, default, out int score);
                    score -= i * 3; // earlier names in the list are preferred
                    if (score > bestScore) { best = m; bestScore = score; }
                }
            }
            return bestScore > -50 ? best : null;
        }

        private static bool NameHas(ParameterInfo p, params string[] words)
        {
            var n = (p.Name ?? "").ToLowerInvariant();
            return words.Any(w => n.Contains(w));
        }

        private static bool IsNumber(Type t) => t == typeof(float) || t == typeof(double) || t == typeof(int) || t == typeof(long) || t == typeof(short);

        /// <summary>Fills parameters by type/name; the score says how confident the match is.</summary>
        private object?[] BuildArgs(MethodInfo m, HitInfo hit, out int score)
        {
            var ps = m.GetParameters();
            var args = new object?[ps.Length];
            score = 0;
            bool damageUsed = false;
            bool pointUsed = false;
            var dir = hit.Direction.LengthSquared() > 1e-6f ? NV3.Normalize(hit.Direction) : NV3.UnitZ;

            for (int i = 0; i < ps.Length; i++)
            {
                var p = ps[i];
                var t = p.ParameterType;
                if (t.IsByRef) { score -= 100; args[i] = null; continue; }

                if (IsNumber(t))
                {
                    double v;
                    if (NameHas(p, "force", "knock", "push", "impulse", "power")) { v = hit.Force; score += 2; }
                    else if (!damageUsed || NameHas(p, "damage", "dmg", "amount", "hp", "value")) { v = hit.Damage; damageUsed = true; score += 10; }
                    else { v = 0; score -= 3; }
                    args[i] = Convert.ChangeType(t == typeof(int) || t == typeof(long) || t == typeof(short) ? Math.Round(v) : v, t);
                }
                else if (t == typeof(UnityEngine.Vector3))
                {
                    if (NameHas(p, "dir", "normal", "velocity", "force", "impulse", "knock", "push"))
                    {
                        var v = NameHas(p, "force", "impulse", "knock", "push", "velocity") ? dir * hit.Force : dir;
                        args[i] = U.V(v); score += 3;
                    }
                    else if (!pointUsed) { args[i] = U.V(hit.Point); pointUsed = true; score += 3; }
                    else { args[i] = U.V(dir * hit.Force); score += 1; }
                }
                else if (t == typeof(bool)) { args[i] = false; score -= 1; }
                else if (t == typeof(string)) { args[i] = hit.DamageType ?? ""; score += 1; }
                else if (t.IsEnum)
                {
                    args[i] = Reflect.TryConvert(hit.DamageType ?? "", t, out var e) ? e : Enum.ToObject(t, 0);
                    score += 1;
                }
                else if (t == typeof(GameObject)) { args[i] = hit.Attacker?.gameObject; score += 1; }
                else if (t == typeof(Transform)) { args[i] = hit.Attacker?.transform; score += 1; }
                else if (typeof(Component).IsAssignableFrom(t))
                {
                    args[i] = ComponentArg(t, hit);
                    score += args[i] != null ? 2 : -1;
                }
                else if (typeof(Il2CppObjectBase).IsAssignableFrom(t) || (t.IsValueType && !t.IsPrimitive))
                {
                    // a damage-info class/struct from the game: create it and fill its fields by name
                    args[i] = MakeInfoObject(t, hit, out int s);
                    score += s;
                }
                else if (p.HasDefaultValue) { args[i] = p.DefaultValue; }
                else { args[i] = t.IsValueType ? Activator.CreateInstance(t) : null; score -= 10; }
            }
            if (!damageUsed && args.All(a => !(a is Il2CppObjectBase))) score -= 20; // e.g. a Hit() with no damage at all
            return args;
        }

        private static object? ComponentArg(Type t, HitInfo hit)
        {
            foreach (var c in new[] { hit.Attacker, hit.WeaponComp })
            {
                if (!U.Alive(c)) continue;
                try
                {
                    var il2 = U.T(t);
                    if (il2 == null) continue;
                    var found = c.GetComponentInParent(il2);
                    if (U.Alive(found)) return Reflect.Wrap(t, found);
                }
                catch { }
            }
            return null;
        }

        private static object? MakeInfoObject(Type t, HitInfo hit, out int score)
        {
            score = -8;
            object? obj;
            try
            {
                obj = t.IsValueType ? Activator.CreateInstance(t) : t.GetConstructor(Type.EmptyTypes)?.Invoke(null);
            }
            catch { return null; }
            if (obj == null) return null;

            bool any = false;
            any |= Reflect.SetFirst(obj, new[] { "damage", "Damage", "amount", "dmg", "*damage*" }, hit.Damage, out _);
            if (any) score = 8;
            Reflect.SetFirst(obj, new[] { "force", "Force", "knockback", "*force*" }, hit.Force, out _);
            Reflect.SetFirst(obj, new[] { "damageType", "type", "DamageType", "*type*" }, hit.DamageType ?? "", out _);
            foreach (var name in new[] { "point", "hitPoint", "position", "pos", "Point" })
            {
                var m = Reflect.FindMember(t, name);
                if (m != null && Reflect.MemberType(m) == typeof(UnityEngine.Vector3)) { Reflect.SetValue(obj, m, U.V(hit.Point)); break; }
            }
            foreach (var name in new[] { "direction", "dir", "hitDirection", "normal", "Direction" })
            {
                var m = Reflect.FindMember(t, name);
                if (m != null && Reflect.MemberType(m) == typeof(UnityEngine.Vector3)) { Reflect.SetValue(obj, m, U.V(hit.Direction)); break; }
            }
            return obj;
        }

        private bool SubtractHealth(Component owner, Type type, float damage)
        {
            var wrapped = Reflect.Wrap(type, owner);
            if (wrapped == null) return false;
            var cfg = _game.Cfg;
            var current = Reflect.GetFirst(wrapped, cfg.List("HealthMembers"), t => t == typeof(float) || t == typeof(int) || t == typeof(double), "number", out var path);
            if (current == null || path == null) return false;
            double next = Convert.ToDouble(current) - damage;

            foreach (var setter in cfg.List("SetHealthMethods"))
            {
                var m = type.GetMethods(BindingFlags.Public | BindingFlags.Instance)
                    .FirstOrDefault(x => x.Name.Equals(setter, StringComparison.OrdinalIgnoreCase) && x.GetParameters().Length == 1 && IsNumber(x.GetParameters()[0].ParameterType));
                if (m == null) continue;
                try
                {
                    m.Invoke(wrapped, new[] { Convert.ChangeType(next, m.GetParameters()[0].ParameterType) });
                    return true;
                }
                catch { }
            }
            if (Reflect.SetFirst(wrapped, new[] { path }, next, out _))
            {
                Log.InfoOnce("hpfallback" + type.Name, $"Damage on {type.Name} falls back to lowering '{path}' directly (no damage method matched).");
                return true;
            }
            return false;
        }
    }
}
