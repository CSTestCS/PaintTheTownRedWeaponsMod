// Reference-only stand-in. Member names/signatures mirror what Il2CppInterop generates for Unity.
// Bodies are never executed.
using System;
using Il2CppInterop.Runtime.InteropTypes.Arrays;

namespace UnityEngine
{
    public struct Vector2 { public float x; public float y; }
    public struct Vector3 { public float x; public float y; public float z; }
    public struct Vector4 { public float x; public float y; public float z; public float w; }
    public struct Quaternion { public float x; public float y; public float z; public float w; }
    public struct Color { public float r; public float g; public float b; public float a; }
    public struct Rect { public float m_XMin; public float m_YMin; public float m_Width; public float m_Height; }

    public struct Bounds
    {
        public Vector3 center => throw null;
        public Vector3 size => throw null;
        public Vector3 extents => throw null;
    }

    public enum PrimitiveType { Sphere = 0, Capsule = 1, Cylinder = 2, Cube = 3, Plane = 4, Quad = 5 }
    [Flags] public enum HideFlags { None = 0, HideInHierarchy = 1, HideInInspector = 2, DontSaveInEditor = 4, NotEditable = 8, DontSaveInBuild = 16, DontUnloadUnusedAsset = 32, DontSave = 52, HideAndDontSave = 61 }
    public enum CursorLockMode { None = 0, Locked = 1, Confined = 2 }

    public class Object : Il2CppSystem.Object
    {
        public Object(IntPtr pointer) : base(pointer) { }
        public string name { get => throw null; set => throw null; }
        public HideFlags hideFlags { get => throw null; set => throw null; }
        public int GetInstanceID() => throw null;
        public static Object Instantiate(Object original) => throw null;
        public static void Destroy(Object obj) => throw null;
        public static void DontDestroyOnLoad(Object target) => throw null;
        public static Il2CppReferenceArray<Object> FindObjectsOfType(Il2CppSystem.Type type) => throw null;
        public static bool operator ==(Object x, Object y) => throw null;
        public static bool operator !=(Object x, Object y) => throw null;
        public static implicit operator bool(Object exists) => throw null;
    }

    public class Component : Object
    {
        public Component(IntPtr pointer) : base(pointer) { }
        public Transform transform => throw null;
        public GameObject gameObject => throw null;
        public Component GetComponent(Il2CppSystem.Type type) => throw null;
        public Component GetComponentInParent(Il2CppSystem.Type t) => throw null;
        public Il2CppReferenceArray<Component> GetComponentsInChildren(Il2CppSystem.Type t, bool includeInactive) => throw null;
    }

    public class Behaviour : Component
    {
        public Behaviour(IntPtr pointer) : base(pointer) { }
        public bool enabled { get => throw null; set => throw null; }
    }

    public class MonoBehaviour : Behaviour
    {
        public MonoBehaviour(IntPtr pointer) : base(pointer) { }
    }

    public class ScriptableObject : Object
    {
        public ScriptableObject(IntPtr pointer) : base(pointer) { }
    }

    public sealed class GameObject : Object
    {
        public GameObject(IntPtr pointer) : base(pointer) { }
        public GameObject(string name) : base(IntPtr.Zero) { }
        public Transform transform => throw null;
        public int layer { get => throw null; set => throw null; }
        public bool activeSelf => throw null;
        public bool activeInHierarchy => throw null;
        public void SetActive(bool value) => throw null;
        public Component AddComponent(Il2CppSystem.Type componentType) => throw null;
        public Component GetComponent(Il2CppSystem.Type type) => throw null;
        public Component GetComponentInParent(Il2CppSystem.Type type) => throw null;
        public Il2CppReferenceArray<Component> GetComponentsInChildren(Il2CppSystem.Type type, bool includeInactive) => throw null;
        public static GameObject CreatePrimitive(PrimitiveType type) => throw null;
    }

    public class Transform : Component
    {
        public Transform(IntPtr pointer) : base(pointer) { }
        public Vector3 position { get => throw null; set => throw null; }
        public Vector3 localPosition { get => throw null; set => throw null; }
        public Quaternion rotation { get => throw null; set => throw null; }
        public Quaternion localRotation { get => throw null; set => throw null; }
        public Vector3 localScale { get => throw null; set => throw null; }
        public Vector3 lossyScale => throw null;
        public Vector3 forward { get => throw null; set => throw null; }
        public Vector3 up { get => throw null; set => throw null; }
        public Vector3 right { get => throw null; set => throw null; }
        public Transform parent { get => throw null; set => throw null; }
        public Transform root => throw null;
        public int childCount => throw null;
        public Transform GetChild(int index) => throw null;
        public void SetParent(Transform parent, bool worldPositionStays) => throw null;
        public Vector3 TransformPoint(Vector3 position) => throw null;
        public Vector3 InverseTransformPoint(Vector3 position) => throw null;
    }

    public class Renderer : Component
    {
        public Renderer(IntPtr pointer) : base(pointer) { }
        public bool enabled { get => throw null; set => throw null; }
        public Bounds bounds => throw null;
        public Material sharedMaterial { get => throw null; set => throw null; }
    }

    public class MeshRenderer : Renderer { public MeshRenderer(IntPtr pointer) : base(pointer) { } }
    public class SkinnedMeshRenderer : Renderer { public SkinnedMeshRenderer(IntPtr pointer) : base(pointer) { } }

    public sealed class MeshFilter : Component
    {
        public MeshFilter(IntPtr pointer) : base(pointer) { }
        public Mesh sharedMesh { get => throw null; set => throw null; }
    }

    public sealed class Mesh : Object
    {
        public Mesh(IntPtr pointer) : base(pointer) { }
        public Mesh() : base(IntPtr.Zero) { }
        public UnityEngine.Rendering.IndexFormat indexFormat { get => throw null; set => throw null; }
        public Il2CppStructArray<Vector3> vertices { get => throw null; set => throw null; }
        public Il2CppStructArray<Vector3> normals { get => throw null; set => throw null; }
        public Il2CppStructArray<Vector2> uv { get => throw null; set => throw null; }
        public Il2CppStructArray<int> triangles { get => throw null; set => throw null; }
        public Il2CppStructArray<Color> colors { get => throw null; set => throw null; }
        public Bounds bounds { get => throw null; set => throw null; }
        public void RecalculateNormals() => throw null;
        public void RecalculateBounds() => throw null;
    }

    public class Shader : Object
    {
        public Shader(IntPtr pointer) : base(pointer) { }
        public static Shader Find(string name) => throw null;
    }

    public class Material : Object
    {
        public Material(IntPtr pointer) : base(pointer) { }
        public Material(Material source) : base(IntPtr.Zero) { }
        public Material(Shader shader) : base(IntPtr.Zero) { }
        public Shader shader { get => throw null; set => throw null; }
        public Color color { get => throw null; set => throw null; }
        public Texture mainTexture { get => throw null; set => throw null; }
        public bool HasProperty(string name) => throw null;
        public void SetColor(string name, Color value) => throw null;
        public void SetTexture(string name, Texture value) => throw null;
    }

    public class Texture : Object { public Texture(IntPtr pointer) : base(pointer) { } }

    public sealed class Texture2D : Texture
    {
        public Texture2D(IntPtr pointer) : base(pointer) { }
        public Texture2D(int width, int height) : base(IntPtr.Zero) { }
        public static Texture2D whiteTexture => throw null;
        public void Apply() => throw null;
    }

    public sealed class Camera : Behaviour
    {
        public Camera(IntPtr pointer) : base(pointer) { }
        public static Camera main => throw null;
    }

    public sealed class Resources
    {
        public static Il2CppReferenceArray<Object> FindObjectsOfTypeAll(Il2CppSystem.Type type) => throw null;
    }

    public class Time
    {
        public static float deltaTime => throw null;
        public static float time => throw null;
        public static float timeScale { get => throw null; set => throw null; }
    }

    public class Cursor
    {
        public static bool visible { get => throw null; set => throw null; }
        public static CursorLockMode lockState { get => throw null; set => throw null; }
    }

    public sealed class Screen
    {
        public static int width => throw null;
        public static int height => throw null;
    }
}

namespace UnityEngine.Rendering
{
    public enum IndexFormat { UInt16 = 0, UInt32 = 1 }
}
