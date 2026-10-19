using System;
using System.Numerics;

namespace PTTRCustomWeapons.Core
{
    /// <summary>Engine-independent projectile maths (unit tested).</summary>
    public static class Ballistics
    {
        public const float Gravity = 9.81f;

        /// <summary>Advances velocity by gravity; returns the displacement for this step.</summary>
        public static Vector3 Step(ref Vector3 velocity, float dropMultiplier, float dt)
        {
            var v0 = velocity;
            velocity.Y -= Gravity * dropMultiplier * dt;
            return (v0 + velocity) * 0.5f * dt; // trapezoid: exact for constant acceleration
        }

        /// <summary>Reflects velocity off a surface and scales by bounciness.</summary>
        public static Vector3 Bounce(Vector3 velocity, Vector3 normal, float bounciness)
        {
            if (normal.LengthSquared() < 1e-8f) return -velocity * bounciness;
            normal = Vector3.Normalize(normal);
            return Vector3.Reflect(velocity, normal) * bounciness;
        }

        /// <summary>Random direction inside a cone of <paramref name="spreadDegrees"/> around <paramref name="forward"/>.</summary>
        public static Vector3 Spread(Vector3 forward, float spreadDegrees, Random rng)
        {
            forward = Vector3.Normalize(forward);
            if (spreadDegrees <= 0.0001f) return forward;
            var helper = MathF.Abs(forward.Y) < 0.99f ? Vector3.UnitY : Vector3.UnitX;
            var right = Vector3.Normalize(Vector3.Cross(helper, forward));
            var up = Vector3.Cross(forward, right);
            // uniform over the cone's cap
            float maxAngle = spreadDegrees * MathF.PI / 180f;
            float cosMax = MathF.Cos(maxAngle);
            float cosT = 1f - (float)rng.NextDouble() * (1f - cosMax);
            float sinT = MathF.Sqrt(MathF.Max(0f, 1f - cosT * cosT));
            float phi = (float)rng.NextDouble() * MathF.PI * 2f;
            return Vector3.Normalize(forward * cosT + (right * MathF.Cos(phi) + up * MathF.Sin(phi)) * sinT);
        }

        /// <summary>Damage after distance falloff inside an explosion radius.</summary>
        public static float AreaDamage(float damage, float radius, float distance, bool falloff)
        {
            if (radius <= 0 || distance > radius) return 0f;
            if (!falloff) return damage;
            float t = 1f - distance / radius;
            return damage * (0.25f + 0.75f * t); // at least 25% at the edge
        }

        /// <summary>Unity quaternion (x,y,z,w) that rotates +Z to look along <paramref name="dir"/>.</summary>
        public static Quaternion LookRotation(Vector3 dir)
        {
            if (dir.LengthSquared() < 1e-8f) return Quaternion.Identity;
            dir = Vector3.Normalize(dir);
            float yaw = MathF.Atan2(dir.X, dir.Z);
            float pitch = -MathF.Asin(Math.Clamp(dir.Y, -1f, 1f));
            return EulerYXZ(pitch, yaw, 0f);
        }

        /// <summary>Unity's Quaternion.Euler (applied Z, then X, then Y) for angles in radians.</summary>
        public static Quaternion EulerYXZ(float x, float y, float z)
        {
            var qx = Quaternion.CreateFromAxisAngle(Vector3.UnitX, x);
            var qy = Quaternion.CreateFromAxisAngle(Vector3.UnitY, y);
            var qz = Quaternion.CreateFromAxisAngle(Vector3.UnitZ, z);
            return qy * qx * qz;
        }

        public static Quaternion EulerDegrees(Vector3 deg)
        {
            const float k = MathF.PI / 180f;
            return EulerYXZ(deg.X * k, deg.Y * k, deg.Z * k);
        }
    }
}
