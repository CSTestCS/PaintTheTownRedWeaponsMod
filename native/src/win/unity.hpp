// The Unity engine functions the mod uses, called through IL2CPP by name.
// Every call must happen on Unity's main thread (the mod only calls them from its per-frame hook).
#pragma once
#include <string>
#include <vector>
#include "../core/fbx.hpp"
#include "../core/math.hpp"
#include "il2cpp.hpp"

namespace u {

using il2::Obj;
using cw::Quat;
using cw::Vec3;
using cw::Vec4;

struct Bounds { Vec3 center, extents; };
struct Hit { Vec3 point, normal; float distance = 0; Obj collider = nullptr; };

/// Looks up the Unity classes; returns false (and logs) if essential ones are missing.
bool init();

// classes (valid after init)
extern Il2CppClass *Object, *GameObject, *Component, *Transform, *Renderer, *MeshRenderer, *SkinnedMeshRenderer,
    *MeshFilter, *Mesh, *Material, *Texture2D, *Collider, *Rigidbody, *ScriptableObject;

// objects
bool alive(Obj o);
std::string name(Obj o);
void setName(Obj o, const std::string& n);
int instanceId(Obj o);
Obj instantiate(Obj original);
void destroy(Obj o);
void keepLoaded(Obj asset); // HideFlags.DontUnloadUnusedAsset
std::vector<Obj> findObjectsOfType(Il2CppClass* k);
std::vector<Obj> findAllIncludingAssets(Il2CppClass* k);

// game objects / components
Obj gameObject(Obj component);
Obj transform(Obj componentOrGameObject);
Obj getComponent(Obj componentOrGameObject, Il2CppClass* k);
Obj componentInParent(Obj component, Il2CppClass* k);
std::vector<Obj> componentsInChildren(Obj componentOrGameObject, Il2CppClass* k, bool includeInactive);
Obj addComponent(Obj go, Il2CppClass* k);
Obj newGameObject(const std::string& name);
Obj createPrimitive(int type); // 0 sphere, 1 capsule, 2 cylinder, 3 cube
void setActive(Obj go, bool v);
bool activeSelf(Obj go);
bool activeInHierarchy(Obj go);
int layer(Obj go);
void setLayer(Obj go, int l);

// transforms
Vec3 position(Obj t);
void setPosition(Obj t, Vec3 v);
Quat rotation(Obj t);
void setRotation(Obj t, Quat q);
void setLocalPosition(Obj t, Vec3 v);
void setLocalRotation(Obj t, Quat q);
void setLocalScale(Obj t, Vec3 v);
Vec3 lossyScale(Obj t);
Vec3 forward(Obj t);
Vec3 right(Obj t);
Vec3 up(Obj t);
Obj parent(Obj t);
void setParent(Obj t, Obj parent, bool worldPositionStays);
Vec3 transformPoint(Obj t, Vec3 v);
Vec3 inverseTransformPoint(Obj t, Vec3 v);

// rendering
bool rendererEnabled(Obj r);
void setRendererEnabled(Obj r, bool v);
Bounds rendererBounds(Obj r);
Obj sharedMaterial(Obj r);
void setSharedMaterial(Obj r, Obj m);
Obj sharedMesh(Obj meshFilter);
void setSharedMesh(Obj meshFilter, Obj mesh);
bool meshBounds(Obj mesh, Bounds& out);
Obj buildMesh(const cw::MeshData& data, const std::string& name);
Obj loadTexture(const std::vector<uint8_t>& bytes, const std::string& name);
Obj whiteTexture();
/// Copy of source (keeps the game's shader) with texture and tint applied; falls back to Standard.
Obj makeMaterial(Obj source, Obj texture, Vec4 tint, const std::string& name);
std::string shaderName(Obj material);

// misc
Obj mainCamera();
float time();
float deltaTime();
float timeScale();
int frameCount();
bool cursorFree();
std::string activeSceneName();

// physics
bool raycast(Vec3 origin, Vec3 dir, float maxDistance, Hit& out);
std::vector<Obj> overlapSphere(Vec3 pos, float radius);
Obj attachedRigidbody(Obj collider);
bool isKinematic(Obj rb);
void addForce(Obj rb, Vec3 force);              // impulse
void addExplosionForce(Obj rb, float force, Vec3 pos, float radius, float upwards);

} // namespace u
