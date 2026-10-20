#include "unity.hpp"
#include <cstring>
#include "../../third_party/stb/stb_image.h"
#include "log.hpp"

using namespace il2;

namespace u {

Il2CppClass *Object, *GameObject, *Component, *Transform, *Renderer, *MeshRenderer, *SkinnedMeshRenderer, *MeshFilter, *Mesh,
    *Material, *Texture2D, *Collider, *Rigidbody, *ScriptableObject;
static Il2CppClass *Shader, *Texture, *Camera, *Resources, *Time, *Cursor, *Physics, *ImageConversion, *SceneManager, *Scene,
    *Vector3C, *Vector2C, *ColorC, *Color32C, *Int32C, *ByteC;
static size_t g_cachedPtrOffset = 0;

// Looks a method up once (first call) and keeps it.
#define M(var, klass, name, argc, ...) static const MethodInfo* var = il2::method(klass, name, argc, {__VA_ARGS__})

static Obj cloneAssetImpl(Obj o) { Obj c = instantiate(o); return c; }
static bool isAssetImpl(Obj o) { return isA(o, Object) && !isA(o, Component) && !isA(o, GameObject); }

bool init() {
    auto core = [](const char* ns, const char* n) { return cls("UnityEngine.CoreModule", ns, n); };
    Object = core("UnityEngine", "Object");
    GameObject = core("UnityEngine", "GameObject");
    Component = core("UnityEngine", "Component");
    Transform = core("UnityEngine", "Transform");
    Renderer = core("UnityEngine", "Renderer");
    MeshRenderer = core("UnityEngine", "MeshRenderer");
    SkinnedMeshRenderer = core("UnityEngine", "SkinnedMeshRenderer");
    MeshFilter = core("UnityEngine", "MeshFilter");
    Mesh = core("UnityEngine", "Mesh");
    Material = core("UnityEngine", "Material");
    Shader = core("UnityEngine", "Shader");
    Texture = core("UnityEngine", "Texture");
    Texture2D = core("UnityEngine", "Texture2D");
    ScriptableObject = core("UnityEngine", "ScriptableObject");
    Camera = core("UnityEngine", "Camera");
    Resources = core("UnityEngine", "Resources");
    Time = core("UnityEngine", "Time");
    Cursor = core("UnityEngine", "Cursor");
    SceneManager = core("UnityEngine.SceneManagement", "SceneManager");
    Scene = core("UnityEngine.SceneManagement", "Scene");
    Vector3C = core("UnityEngine", "Vector3");
    Vector2C = core("UnityEngine", "Vector2");
    ColorC = core("UnityEngine", "Color");
    Color32C = core("UnityEngine", "Color32");
    Physics = cls("UnityEngine.PhysicsModule", "UnityEngine", "Physics");
    Collider = cls("UnityEngine.PhysicsModule", "UnityEngine", "Collider");
    Rigidbody = cls("UnityEngine.PhysicsModule", "UnityEngine", "Rigidbody");
    ImageConversion = cls("UnityEngine.ImageConversionModule", "UnityEngine", "ImageConversion");
    Int32C = il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Int32");
    ByteC = il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Byte");

    if (auto f = findMember(Object, "m_CachedPtr"); f.field) g_cachedPtrOffset = il2cpp_field_get_offset(f.field);
    g_cloneAsset = cloneAssetImpl;
    g_isUnityAsset = isAssetImpl;

    struct { const char* n; Il2CppClass* k; } req[] = {{"Object", Object}, {"GameObject", GameObject}, {"Component", Component},
        {"Transform", Transform}, {"Renderer", Renderer}, {"MeshFilter", MeshFilter}, {"Mesh", Mesh}, {"Material", Material},
        {"Physics", Physics}, {"Vector3", Vector3C}, {"Int32", Int32C}};
    bool ok = true;
    for (auto& r : req) if (!r.k) { cw::logError(std::string("Unity class not found: ") + r.n); ok = false; }
    if (!g_cachedPtrOffset) cw::logWarn("UnityEngine.Object.m_CachedPtr not found; destroyed-object checks are weaker.");
    return ok;
}

// ------------------------------------------------------------------ objects

bool alive(Obj o) {
    if (!o) return false;
    if (!g_cachedPtrOffset) return true;
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(o) + g_cachedPtrOffset) != nullptr;
}

std::string name(Obj o) {
    M(m, Object, "get_name", 0);
    return alive(o) ? toUtf8(reinterpret_cast<Il2CppString*>(invoke(m, o))) : "";
}

void setName(Obj o, const std::string& n) {
    M(m, Object, "set_name", 1);
    if (!alive(o)) return;
    void* a[] = {str(n)};
    invoke(m, o, a);
}

int instanceId(Obj o) {
    M(m, Object, "GetInstanceID", 0);
    return o ? unbox<int>(invoke(m, o)) : 0;
}

Obj instantiate(Obj original) {
    M(m, Object, "Instantiate", 1, "UnityEngine.Object");
    if (!alive(original)) return nullptr;
    void* a[] = {original};
    return invoke(m, nullptr, a);
}

void destroy(Obj o) {
    M(m, Object, "Destroy", 1, "UnityEngine.Object");
    if (!alive(o)) return;
    void* a[] = {o};
    invoke(m, nullptr, a);
}

void keepLoaded(Obj asset) {
    M(m, Object, "set_hideFlags", 1);
    if (!asset) return;
    int flags = 32; // DontUnloadUnusedAsset
    void* a[] = {&flags};
    invoke(m, asset, a);
}

std::vector<Obj> findObjectsOfType(Il2CppClass* k) {
    M(m, Object, "FindObjectsOfType", 1, "System.Type");
    if (!k) return {};
    void* a[] = {typeObject(k)};
    auto arr = reinterpret_cast<Il2CppArray*>(invoke(m, nullptr, a));
    std::vector<Obj> r;
    for (auto o : arrayObjects(arr)) if (alive(o)) r.push_back(o);
    return r;
}

std::vector<Obj> findAllIncludingAssets(Il2CppClass* k) {
    M(m, Resources, "FindObjectsOfTypeAll", 1, "System.Type");
    if (!k) return {};
    void* a[] = {typeObject(k)};
    auto arr = reinterpret_cast<Il2CppArray*>(invoke(m, nullptr, a));
    std::vector<Obj> r;
    for (auto o : arrayObjects(arr)) if (alive(o)) r.push_back(o);
    return r;
}

// ------------------------------------------------------------------ game objects / components

Obj gameObject(Obj c) {
    M(m, Component, "get_gameObject", 0);
    if (!alive(c)) return nullptr;
    if (isA(c, GameObject)) return c;
    return invoke(m, c);
}

Obj transform(Obj o) {
    M(mg, GameObject, "get_transform", 0);
    M(mc, Component, "get_transform", 0);
    if (!alive(o)) return nullptr;
    return invoke(isA(o, GameObject) ? mg : mc, o);
}

Obj getComponent(Obj o, Il2CppClass* k) {
    M(mg, GameObject, "GetComponent", 1, "System.Type");
    M(mc, Component, "GetComponent", 1, "System.Type");
    if (!alive(o) || !k) return nullptr;
    void* a[] = {typeObject(k)};
    Obj r = invoke(isA(o, GameObject) ? mg : mc, o, a);
    return alive(r) ? r : nullptr;
}

Obj componentInParent(Obj o, Il2CppClass* k) {
    M(mc, Component, "GetComponentInParent", 1, "System.Type");
    M(mg, GameObject, "GetComponentInParent", 1, "System.Type");
    if (!alive(o) || !k) return nullptr;
    void* a[] = {typeObject(k)};
    Obj r = invoke(isA(o, GameObject) ? mg : mc, o, a);
    return alive(r) ? r : nullptr;
}

std::vector<Obj> componentsInChildren(Obj o, Il2CppClass* k, bool includeInactive) {
    M(mg, GameObject, "GetComponentsInChildren", 2, "System.Type", "System.Boolean");
    M(mc, Component, "GetComponentsInChildren", 2, "System.Type", "System.Boolean");
    if (!alive(o) || !k) return {};
    void* a[] = {typeObject(k), &includeInactive};
    auto arr = reinterpret_cast<Il2CppArray*>(invoke(isA(o, GameObject) ? mg : mc, o, a));
    std::vector<Obj> r;
    for (auto x : arrayObjects(arr)) if (alive(x)) r.push_back(x);
    return r;
}

Obj addComponent(Obj go, Il2CppClass* k) {
    M(m, GameObject, "AddComponent", 1, "System.Type");
    if (!alive(go) || !k) return nullptr;
    void* a[] = {typeObject(k)};
    return invoke(m, go, a);
}

Obj newGameObject(const std::string& n) {
    void* a[] = {str(n)};
    return newObject(GameObject, 1, a, {"System.String"});
}

Obj createPrimitive(int type) {
    M(m, GameObject, "CreatePrimitive", 1, "UnityEngine.PrimitiveType");
    void* a[] = {&type};
    return invoke(m, nullptr, a);
}

void setActive(Obj go, bool v) { M(m, GameObject, "SetActive", 1); if (!alive(go)) return; void* a[] = {&v}; invoke(m, go, a); }
bool activeSelf(Obj go) { M(m, GameObject, "get_activeSelf", 0); return alive(go) && unbox<bool>(invoke(m, go)); }
bool activeInHierarchy(Obj go) { M(m, GameObject, "get_activeInHierarchy", 0); return alive(go) && unbox<bool>(invoke(m, go)); }
int layer(Obj go) { M(m, GameObject, "get_layer", 0); return alive(go) ? unbox<int>(invoke(m, go)) : 0; }
void setLayer(Obj go, int l) { M(m, GameObject, "set_layer", 1); if (!alive(go)) return; void* a[] = {&l}; invoke(m, go, a); }

// ------------------------------------------------------------------ transforms

#define GET_V3(fn, prop) Vec3 fn(Obj t) { M(m, Transform, prop, 0); return alive(t) ? unbox<Vec3>(invoke(m, t)) : Vec3(); }
#define SET_V3(fn, prop) void fn(Obj t, Vec3 v) { M(m, Transform, prop, 1); if (!alive(t)) return; void* a[] = {&v}; invoke(m, t, a); }
GET_V3(position, "get_position")
SET_V3(setPosition, "set_position")
SET_V3(setLocalPosition, "set_localPosition")
SET_V3(setLocalScale, "set_localScale")
GET_V3(lossyScale, "get_lossyScale")
GET_V3(forward, "get_forward")
GET_V3(right, "get_right")
GET_V3(up, "get_up")

Quat rotation(Obj t) { M(m, Transform, "get_rotation", 0); return alive(t) ? unbox<Quat>(invoke(m, t)) : Quat(); }
void setRotation(Obj t, Quat q) { M(m, Transform, "set_rotation", 1); if (!alive(t)) return; void* a[] = {&q}; invoke(m, t, a); }
void setLocalRotation(Obj t, Quat q) { M(m, Transform, "set_localRotation", 1); if (!alive(t)) return; void* a[] = {&q}; invoke(m, t, a); }
Obj parent(Obj t) { M(m, Transform, "get_parent", 0); return alive(t) ? invoke(m, t) : nullptr; }

void setParent(Obj t, Obj p, bool stays) {
    M(m, Transform, "SetParent", 2, "UnityEngine.Transform", "System.Boolean");
    if (!alive(t)) return;
    void* a[] = {p, &stays};
    invoke(m, t, a);
}

Vec3 transformPoint(Obj t, Vec3 v) {
    M(m, Transform, "TransformPoint", 1, "UnityEngine.Vector3");
    if (!alive(t)) return v;
    void* a[] = {&v};
    return unbox<Vec3>(invoke(m, t, a), v);
}

Vec3 inverseTransformPoint(Obj t, Vec3 v) {
    M(m, Transform, "InverseTransformPoint", 1, "UnityEngine.Vector3");
    if (!alive(t)) return v;
    void* a[] = {&v};
    return unbox<Vec3>(invoke(m, t, a), v);
}

// ------------------------------------------------------------------ rendering

bool rendererEnabled(Obj r) { M(m, Renderer, "get_enabled", 0); return alive(r) && unbox<bool>(invoke(m, r)); }
void setRendererEnabled(Obj r, bool v) { M(m, Renderer, "set_enabled", 1); if (!alive(r)) return; void* a[] = {&v}; invoke(m, r, a); }
Bounds rendererBounds(Obj r) { M(m, Renderer, "get_bounds", 0); return alive(r) ? unbox<Bounds>(invoke(m, r)) : Bounds(); }
Obj sharedMaterial(Obj r) { M(m, Renderer, "get_sharedMaterial", 0); return alive(r) ? invoke(m, r) : nullptr; }
void setSharedMaterial(Obj r, Obj mat) { M(m, Renderer, "set_sharedMaterial", 1); if (!alive(r)) return; void* a[] = {mat}; invoke(m, r, a); }
Obj sharedMesh(Obj mf) { M(m, MeshFilter, "get_sharedMesh", 0); return alive(mf) ? invoke(m, mf) : nullptr; }
void setSharedMesh(Obj mf, Obj mesh) { M(m, MeshFilter, "set_sharedMesh", 1); if (!alive(mf)) return; void* a[] = {mesh}; invoke(m, mf, a); }

bool meshBounds(Obj mesh, Bounds& out) {
    M(m, Mesh, "get_bounds", 0);
    if (!alive(mesh)) return false;
    Obj r = invoke(m, mesh);
    if (!r) return false;
    out = unbox<Bounds>(r);
    return true;
}

template <class T>
static Il2CppArray* makeArray(Il2CppClass* elem, const std::vector<T>& v) {
    auto a = newArray(elem, v.size());
    if (a && !v.empty()) std::memcpy(arrayData(a), v.data(), v.size() * sizeof(T));
    return a;
}

Obj buildMesh(const cw::MeshData& d, const std::string& nm) {
    M(setIndexFormat, Mesh, "set_indexFormat", 1);
    M(setVertices, Mesh, "set_vertices", 1, "UnityEngine.Vector3[]");
    M(setNormals, Mesh, "set_normals", 1, "UnityEngine.Vector3[]");
    M(setUv, Mesh, "set_uv", 1, "UnityEngine.Vector2[]");
    M(setColors, Mesh, "set_colors", 1, "UnityEngine.Color[]");
    M(setTriangles, Mesh, "set_triangles", 1, "System.Int32[]");
    M(recalcNormals, Mesh, "RecalculateNormals", 0);
    M(recalcBounds, Mesh, "RecalculateBounds", 0);
    Obj mesh = newObject(Mesh, 0, nullptr);
    if (!mesh) { cw::logError("Could not create a Unity Mesh"); return nullptr; }
    setName(mesh, nm);
    keepLoaded(mesh);
    if (d.positions.size() > 65000) { int fmt = 1; void* a[] = {&fmt}; invoke(setIndexFormat, mesh, a); }
    { void* a[] = {makeArray(Vector3C, d.positions)}; invoke(setVertices, mesh, a); }
    { void* a[] = {makeArray(Vector3C, d.normals)}; invoke(setNormals, mesh, a); }
    { void* a[] = {makeArray(Vector2C, d.uvs)}; invoke(setUv, mesh, a); }
    if (ColorC && setColors) { void* a[] = {makeArray(ColorC, d.colors)}; invoke(setColors, mesh, a); }
    { void* a[] = {makeArray(Int32C, d.indices)}; invoke(setTriangles, mesh, a); }
    if (!d.hasNormals) invoke(recalcNormals, mesh);
    invoke(recalcBounds, mesh);
    return mesh;
}

Obj whiteTexture() { M(m, Texture2D, "get_whiteTexture", 0); return invoke(m, nullptr); }

Obj loadTexture(const std::vector<uint8_t>& bytes, const std::string& nm) {
    if (bytes.empty() || !Texture2D) return nullptr;
    M(loadImage, ImageConversion, "LoadImage", 3);
    M(setPixels32, Texture2D, "SetPixels32", 1, "UnityEngine.Color32[]");
    M(apply, Texture2D, "Apply", 0);
    auto bytesArr = makeArray(ByteC, bytes);

    // 1) ImageConversion.LoadImage, 2) the engine function directly, 3) decode here and upload pixels
    {
        int w = 2, h = 2;
        void* ca[] = {&w, &h};
        Obj tex = newObject(Texture2D, 2, ca, {"System.Int32", "System.Int32"});
        if (tex) {
            bool ok = false;
            if (loadImage) {
                bool nonReadable = false;
                void* a[] = {tex, bytesArr, &nonReadable};
                bool threw = false;
                Obj r = invoke(loadImage, nullptr, a, &threw);
                ok = !threw && unbox<bool>(r);
            } else if (auto fn = reinterpret_cast<bool (*)(Obj, Il2CppArray*, bool)>(il2cpp_resolve_icall(
                           "UnityEngine.ImageConversion::LoadImage(UnityEngine.Texture2D,System.Byte[],System.Boolean)"))) {
                ok = fn(tex, bytesArr, false);
            }
            if (ok) { setName(tex, nm); keepLoaded(tex); return tex; }
            destroy(tex);
        }
    }
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &comp, 4);
    if (!px || !setPixels32 || !Color32C) { if (px) stbi_image_free(px); cw::logWarn("Texture '" + nm + "' could not be decoded."); return nullptr; }
    // Unity textures start at the bottom row
    std::vector<uint8_t> flipped((size_t)w * h * 4);
    for (int y = 0; y < h; y++) std::memcpy(&flipped[(size_t)y * w * 4], px + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
    stbi_image_free(px);
    void* ca[] = {&w, &h};
    Obj tex = newObject(Texture2D, 2, ca, {"System.Int32", "System.Int32"});
    if (!tex) return nullptr;
    auto colors = newArray(Color32C, (size_t)w * h);
    std::memcpy(arrayData(colors), flipped.data(), flipped.size());
    void* a[] = {colors};
    invoke(setPixels32, tex, a);
    invoke(apply, tex);
    setName(tex, nm);
    keepLoaded(tex);
    return tex;
}

std::string shaderName(Obj mat) {
    M(m, Material, "get_shader", 0);
    return alive(mat) ? name(invoke(m, mat)) : "";
}

Obj makeMaterial(Obj source, Obj texture, Vec4 tint, const std::string& nm) {
    M(find, Shader, "Find", 1, "System.String");
    M(setMainTex, Material, "set_mainTexture", 1);
    M(setColor, Material, "set_color", 1);
    M(hasProp, Material, "HasProperty", 1, "System.String");
    M(setTex, Material, "SetTexture", 2, "System.String", "UnityEngine.Texture");
    M(setCol, Material, "SetColor", 2, "System.String", "UnityEngine.Color");
    Obj mat = nullptr;
    if (alive(source)) { void* a[] = {source}; mat = newObject(Material, 1, a, {"UnityEngine.Material"}); }
    if (!mat) {
        for (auto sn : {"Standard", "Universal Render Pipeline/Lit", "Unlit/Texture"}) {
            void* a[] = {str(sn)};
            Obj sh = invoke(find, nullptr, a);
            if (alive(sh)) { void* b[] = {sh}; mat = newObject(Material, 1, b, {"UnityEngine.Shader"}); if (mat) break; }
        }
    }
    if (!mat) return nullptr;
    setName(mat, nm);
    keepLoaded(mat);
    Obj tex = texture ? texture : whiteTexture();
    if (tex) { void* a[] = {tex}; invoke(setMainTex, mat, a); }
    { void* a[] = {&tint}; invoke(setColor, mat, a); }
    auto has = [&](const char* p) { void* a[] = {str(p)}; return unbox<bool>(invoke(hasProp, mat, a)); };
    for (auto p : {"_MainTex", "_BaseMap", "_BaseColorMap", "_Albedo", "_AlbedoTex", "_Texture", "_Diffuse"})
        if (tex && has(p)) { void* a[] = {str(p), tex}; invoke(setTex, mat, a); }
    for (auto p : {"_Color", "_BaseColor", "_Tint", "_TintColor", "_MainColor"})
        if (has(p)) { void* a[] = {str(p), &tint}; invoke(setCol, mat, a); }
    return mat;
}

// ------------------------------------------------------------------ misc

Obj mainCamera() { M(m, Camera, "get_main", 0); Obj c = invoke(m, nullptr); return alive(c) ? c : nullptr; }
float time() { M(m, Time, "get_time", 0); return unbox<float>(invoke(m, nullptr)); }
float deltaTime() { M(m, Time, "get_deltaTime", 0); return unbox<float>(invoke(m, nullptr)); }
float timeScale() { M(m, Time, "get_timeScale", 0); return unbox<float>(invoke(m, nullptr), 1.f); }
int frameCount() { M(m, Time, "get_frameCount", 0); return unbox<int>(invoke(m, nullptr)); }

bool cursorFree() {
    M(vis, Cursor, "get_visible", 0);
    M(lock, Cursor, "get_lockState", 0);
    if (!vis || !lock) return false;
    return unbox<bool>(invoke(vis, nullptr)) && unbox<int>(invoke(lock, nullptr)) == 0;
}

std::string activeSceneName() {
    M(getActive, SceneManager, "GetActiveScene", 0);
    M(getName, Scene, "get_name", 0);
    Obj boxed = invoke(getActive, nullptr); // a boxed Scene struct
    if (!boxed || !getName) return "";
    return toUtf8(reinterpret_cast<Il2CppString*>(invoke(getName, boxed)));
}

// ------------------------------------------------------------------ physics

namespace {
#pragma pack(push, 4)
struct RawHit { Vec3 point, normal; uint32_t faceId; float distance; float uv[2]; int32_t collider; };
#pragma pack(pop)
static_assert(sizeof(RawHit) == 44, "RaycastHit layout");

Obj colliderFromHit(RawHit& h) {
    M(find, Object, "FindObjectFromInstanceID", 1, "System.Int32");
    static auto icall = reinterpret_cast<Obj (*)(int)>(il2cpp_resolve_icall("UnityEngine.Object::FindObjectFromInstanceID(System.Int32)"));
    if (find) { void* a[] = {&h.collider}; return invoke(find, nullptr, a); }
    if (icall) return icall(h.collider);
    static Il2CppClass* hitClass = cls("UnityEngine.PhysicsModule", "UnityEngine", "RaycastHit");
    M(getCollider, hitClass, "get_collider", 0);
    Obj boxed = il2cpp_value_box(hitClass, &h);
    return invoke(getCollider, boxed);
}
}

bool raycast(Vec3 o, Vec3 d, float maxDistance, Hit& out) {
    M(m, Physics, "Raycast", 6, "UnityEngine.Vector3", "UnityEngine.Vector3");
    if (!m) { cw::logOnce("noraycast", "Physics.Raycast not found; custom projectiles cannot hit anything."); return false; }
    RawHit h{};
    int mask = ~0, qti = 1; // all layers, ignore triggers
    void* a[] = {&o, &d, &h, &maxDistance, &mask, &qti};
    if (!unbox<bool>(invoke(m, nullptr, a))) return false;
    out.point = h.point;
    out.normal = h.normal;
    out.distance = h.distance;
    out.collider = colliderFromHit(h);
    return true;
}

std::vector<Obj> overlapSphere(Vec3 pos, float radius) {
    M(m, Physics, "OverlapSphere", 4, "UnityEngine.Vector3", "System.Single", "System.Int32", "UnityEngine.QueryTriggerInteraction");
    int mask = ~0, qti = 1;
    void* a[] = {&pos, &radius, &mask, &qti};
    return arrayObjects(reinterpret_cast<Il2CppArray*>(invoke(m, nullptr, a)));
}

Obj attachedRigidbody(Obj col) {
    M(m, Collider, "get_attachedRigidbody", 0);
    if (!alive(col) || !isA(col, Collider)) return nullptr;
    Obj r = invoke(m, col);
    return alive(r) ? r : nullptr;
}

bool isKinematic(Obj rb) { M(m, Rigidbody, "get_isKinematic", 0); return !alive(rb) || unbox<bool>(invoke(m, rb)); }

void addForce(Obj rb, Vec3 f) {
    M(m, Rigidbody, "AddForce", 2, "UnityEngine.Vector3", "UnityEngine.ForceMode");
    if (!alive(rb)) return;
    int mode = 1; // Impulse
    void* a[] = {&f, &mode};
    invoke(m, rb, a);
}

void addExplosionForce(Obj rb, float force, Vec3 pos, float radius, float upwards) {
    M(m, Rigidbody, "AddExplosionForce", 5);
    if (!alive(rb)) return;
    int mode = 1;
    void* a[] = {&force, &pos, &radius, &upwards, &mode};
    invoke(m, rb, a);
}

} // namespace u
