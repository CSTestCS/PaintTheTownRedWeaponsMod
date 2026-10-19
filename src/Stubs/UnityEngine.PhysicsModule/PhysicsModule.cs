// Reference-only stand-in. Bodies are never executed.
using System;
using Il2CppInterop.Runtime.InteropTypes.Arrays;

namespace UnityEngine
{
    public enum ForceMode { Force = 0, Impulse = 1, VelocityChange = 2, Acceleration = 5 }
    public enum QueryTriggerInteraction { UseGlobal = 0, Ignore = 1, Collide = 2 }

    public struct RaycastHit
    {
        public Vector3 point => throw null;
        public Vector3 normal => throw null;
        public float distance => throw null;
        public Collider collider => throw null;
    }

    public class Collider : Component
    {
        public Collider(IntPtr pointer) : base(pointer) { }
        public bool enabled { get => throw null; set => throw null; }
        public Rigidbody attachedRigidbody => throw null;
    }

    public class Rigidbody : Component
    {
        public Rigidbody(IntPtr pointer) : base(pointer) { }
        public bool isKinematic { get => throw null; set => throw null; }
        public void AddForce(Vector3 force, ForceMode mode) => throw null;
        public void AddExplosionForce(float explosionForce, Vector3 explosionPosition, float explosionRadius, float upwardsModifier, ForceMode mode) => throw null;
    }

    public class Physics
    {
        public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit hitInfo, float maxDistance, int layerMask, QueryTriggerInteraction queryTriggerInteraction) => throw null;
        public static Il2CppReferenceArray<Collider> OverlapSphere(Vector3 position, float radius, int layerMask, QueryTriggerInteraction queryTriggerInteraction) => throw null;
    }
}
