// Bridge (game classes), Skinner (custom models/stats on game weapons), Damage, key input.
#include "game.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include "../core/ballistics.hpp"
#include "../core/strutil.hpp"
#include "log.hpp"

namespace cw {

using namespace il2;

// ====================================================================== Bridge

static bool isComponentClass(Il2CppClass* k) { return k && u::Component && isSubclass(k, u::Component); }

void Bridge::resolve() {
    if (resolved) return;
    for (auto& n : cfg.list("PlayerTypes")) if (auto k = gameClass(n); isComponentClass(k)) { player = k; break; }
    for (auto& n : cfg.list("EnemyTypes")) if (auto k = gameClass(n); isComponentClass(k)) { enemy = k; break; }
    for (auto& n : cfg.list("WeaponTypes")) {
        if (iequals(n, "auto")) {
            if (!player) continue;
            for (auto& mn : cfg.list("PlayerWeaponMembers")) {
                for (auto& m : findMembers(player, mn)) {
                    auto k = il2cpp_class_from_type(m.type);
                    if (kindOf(m.type) == Kind::Object && isComponentClass(k) && k != u::Component && !isSubclass(k, u::Transform)) { weapon = k; break; }
                }
                if (weapon) break;
            }
        } else weapon = gameClass(n);
        if (isComponentClass(weapon)) break;
        weapon = nullptr;
    }
    for (auto& n : cfg.list("ShopTypes")) if (auto k = gameClass(n); isComponentClass(k) && std::find(shops.begin(), shops.end(), k) == shops.end()) shops.push_back(k);
    for (auto& n : cfg.list("HitPartTypes")) if (auto k = gameClass(n); isComponentClass(k) && std::find(hitParts.begin(), hitParts.end(), k) == hitParts.end()) hitParts.push_back(k);

    std::string shopNames, partNames;
    for (auto k : shops) shopNames += className(k) + " ";
    for (auto k : hitParts) partNames += className(k) + " ";
    logInfo("Game classes: player=" + (player ? fullName(player) : "NOT FOUND") + ", enemy=" + (enemy ? fullName(enemy) : "NOT FOUND") +
            ", weapon=" + (weapon ? fullName(weapon) : "NOT FOUND") + ", shop=[" + shopNames + "], hit parts=[" + partNames + "]");
    if (!player || !weapon) logWarn("Some game classes were not found. Check _GameReport.txt and edit _GameHooks.cfg.");
    resolved = true;
}

Obj Bridge::localPlayer() {
    if (Obj p = player_.get(); u::alive(p)) return p;
    float now = u::time();
    if (now < nextPlayerSearch_) return nullptr;
    nextPlayerSearch_ = now + 1.f;
    player_.reset();
    auto all = u::findObjectsOfType(player);
    for (auto p : all) {
        Value v;
        if (all.size() > 1 && getFirst(p, cfg.list("PlayerLocalMembers"), fBool, v) && !v.b) continue; // a remote player
        player_ = Ref(p);
        return p;
    }
    return nullptr;
}

Obj Bridge::heldWeapon(Obj holder, const char* key) {
    if (!weapon || !u::alive(holder)) return nullptr;
    static Il2CppClass* weaponForFilter = nullptr;
    weaponForFilter = weapon;
    auto filter = +[](const Il2CppType* t) {
        if (kindOf(t) != Kind::Object) return false;
        auto k = il2cpp_class_from_type(t);
        return k && (isSubclass(k, weaponForFilter) || isSubclass(weaponForFilter, k)) && k != u::Component;
    };
    Value v;
    if (!getFirst(holder, cfg.list(key), filter, v) || !u::alive(v.o) || !isA(v.o, weapon)) return nullptr;
    return v.o;
}

bool Bridge::isDead(Obj e) {
    Value v;
    return getFirst(e, cfg.list("EnemyDeadMembers"), fBool, v) && v.b;
}

int Bridge::isRangedGameWeapon(Obj w) {
    if (!weapon || !w) return -1;
    Value v;
    if (getFirst(w, cfg.list("WeaponRangedMembers"), fBool, v)) return v.b ? 1 : 0;
    auto cn = className(classOf(w));
    for (auto& word : cfg.list("RangedClassWords")) if (icontains(cn, word)) return 1;
    return classOf(w) != weapon ? 0 : -1;
}

Obj Bridge::shopInParent(Obj c) {
    for (auto k : shops) if (Obj s = u::componentInParent(c, k)) return s;
    return nullptr;
}

Holder Bridge::holderOf(Obj w, Obj* holder) {
    Obj h = nullptr;
    Holder r = Holder::None;
    if (player && (h = u::componentInParent(w, player))) r = Holder::Player;
    else if (enemy && (h = u::componentInParent(w, enemy))) r = Holder::Enemy;
    else if ((h = shopInParent(w))) r = Holder::Shop;
    if (holder) *holder = h;
    return r;
}

// ====================================================================== Skinner

CustomInstance* Skinner::get(Obj w) {
    if (!u::alive(w)) return nullptr;
    auto it = byRoot_.find(u::instanceId(u::gameObject(w)));
    if (it == byRoot_.end() || !u::alive(it->second->root.get())) return nullptr;
    return it->second.get();
}

std::vector<CustomInstance*> Skinner::all() {
    std::vector<CustomInstance*> r;
    for (auto& kv : byRoot_) if (u::alive(kv.second->root.get())) r.push_back(kv.second.get());
    return r;
}

void Skinner::cleanup() {
    for (auto it = byRoot_.begin(); it != byRoot_.end();)
        it = u::alive(it->second->root.get()) ? std::next(it) : byRoot_.erase(it);
}

void Skinner::keepOriginalsHidden() {
    for (auto& kv : byRoot_)
        for (auto& r : kv.second->hidden)
            if (Obj o = r.get(); u::alive(o) && u::rendererEnabled(o)) u::setRendererEnabled(o, false);
}

WeaponAssets& Skinner::assets(LoadedWeapon& lw) {
    auto& a = assets_[lw.def.id];
    if (!u::alive(a.mesh.get())) a.mesh = Ref(u::buildMesh(lw.mesh, "CW_" + lw.def.id));
    if (!u::alive(a.texture.get()) && !lw.texture.empty()) a.texture = Ref(u::loadTexture(lw.texture, "CW_" + lw.def.id));
    if (lw.hasProjectileMesh && !u::alive(a.projectileMesh.get())) a.projectileMesh = Ref(u::buildMesh(lw.projectileMesh, "CW_Proj_" + lw.def.id));
    return a;
}

Obj Skinner::material(LoadedWeapon& lw, const std::string& key, Obj source, bool textured, Vec4 tint) {
    auto& a = assets(lw);
    std::string k = key + "|" + std::to_string(source ? u::instanceId(source) : 0);
    if (Obj m = a.materials[k].get(); u::alive(m)) return m;
    Obj m = u::makeMaterial(source, textured ? a.texture.get() : nullptr, tint, "CW_" + lw.def.id + "_" + key);
    a.materials[k] = Ref(m);
    return m;
}

static Quat alignLongestAxis(Vec3 ours, Vec3 theirs) {
    auto longest = [](Vec3 v) { return v.x >= v.y && v.x >= v.z ? 0 : v.y >= v.z ? 1 : 2; };
    int a = longest(ours), b = longest(theirs);
    if (a == b) return {};
    const float h = kPi / 2;
    if (a == 0 && b == 1) return Quat::axisAngle({0, 0, 1}, h);
    if (a == 1 && b == 0) return Quat::axisAngle({0, 0, 1}, -h);
    if (a == 0 && b == 2) return Quat::axisAngle({0, 1, 0}, -h);
    if (a == 2 && b == 0) return Quat::axisAngle({0, 1, 0}, h);
    if (a == 1 && b == 2) return Quat::axisAngle({1, 0, 0}, h);
    return Quat::axisAngle({1, 0, 0}, -h);
}

Obj Skinner::buildModel(Obj root, LoadedWeapon& lw, CustomInstance& ci) {
    auto& def = lw.def;
    auto& a = assets(lw);
    Obj mesh = a.mesh.get();
    if (!mesh) return nullptr;
    Obj rootT = u::transform(root);

    // measure and hide the original visuals
    Obj sourceMat = nullptr;
    Vec3 omin(1e30f), omax(-1e30f);
    bool haveOrig = false;
    std::vector<Obj> renderers = u::componentsInChildren(root, u::MeshRenderer, true);
    for (auto r : u::componentsInChildren(root, u::SkinnedMeshRenderer, true)) renderers.push_back(r);
    for (auto r : renderers) {
        Obj go = u::gameObject(r);
        if (startsWith(u::name(go), "CW_Model")) { u::destroy(go); continue; }
        if (!u::rendererEnabled(r)) continue;
        if (!sourceMat) {
            sourceMat = u::sharedMaterial(r);
            logOnce("shader" + def.id, "[" + def.id + "] copying material of '" + u::name(go) + "' (shader: " + u::shaderName(sourceMat) + ")", false);
        }
        u::Bounds b;
        bool local = false;
        if (Obj mf = u::getComponent(go, u::MeshFilter)) local = u::meshBounds(u::sharedMesh(mf), b);
        if (!local) b = u::rendererBounds(r);
        Obj rt = u::transform(r);
        for (int i = 0; i < 8; i++) {
            Vec3 c = b.center + Vec3((i & 1) ? b.extents.x : -b.extents.x, (i & 2) ? b.extents.y : -b.extents.y, (i & 4) ? b.extents.z : -b.extents.z);
            Vec3 world = local ? u::transformPoint(rt, c) : c;
            Vec3 l = u::inverseTransformPoint(rootT, world);
            omin = vmin(omin, l);
            omax = vmax(omax, l);
            haveOrig = true;
        }
        u::setRendererEnabled(r, false);
        ci.hidden.emplace_back(r);
    }

    Obj mat = material(lw, "model", sourceMat, true, def.tint);
    Obj model = u::newGameObject("CW_Model_" + def.id);
    if (!model) return nullptr;
    Obj mt = u::transform(model);
    u::setParent(mt, rootT, false);
    u::setLayer(model, u::layer(root));
    Obj mf = u::addComponent(model, u::MeshFilter);
    u::setSharedMesh(mf, mesh);
    Obj mr = u::addComponent(model, u::MeshRenderer);
    if (mat) u::setSharedMaterial(mr, mat);

    // same centre, longest axes lined up, same length as the original model
    Vec3 mn, mx;
    lw.mesh.bounds(mn, mx);
    Vec3 size = mx - mn, center = (mn + mx) * 0.5f;
    Quat rot;
    float scale = def.modelScale;
    Vec3 target;
    if (def.autoFit && haveOrig) {
        Vec3 osize = omax - omin;
        target = (omin + omax) * 0.5f;
        rot = alignLongestAxis(size, osize);
        float ours = std::max(size.x, std::max(size.y, size.z)), theirs = std::max(osize.x, std::max(osize.y, osize.z));
        if (ours > 1e-5f && theirs > 1e-5f) scale *= theirs / ours;
    } else {
        float s = std::max(1e-4f, std::fabs(u::lossyScale(rootT).x));
        scale /= s;
    }
    rot = ballistics::eulerDeg(def.modelRotation) * rot;
    u::setLocalScale(mt, Vec3(scale));
    u::setLocalRotation(mt, rot);
    u::setLocalPosition(mt, target - rot.rotate(center * scale) + def.modelOffset);
    return model;
}

CustomInstance* Skinner::apply(Obj w, LoadedWeapon& lw) {
    if (!u::alive(w)) return nullptr;
    Obj root = u::gameObject(w);
    int id = u::instanceId(root);
    if (auto it = byRoot_.find(id); it != byRoot_.end()) u::destroy(it->second->model.get());
    auto ci = std::make_unique<CustomInstance>();
    ci->weapon = &lw;
    ci->root = Ref(root);
    ci->component = Ref(w);
    ci->magazine = lw.def.infiniteMagazine() ? 1 << 30 : lw.def.magazineSize;
    ci->reserve = lw.def.reserveAmmo < 0 ? 1 << 30 : lw.def.reserveAmmo;
    ci->model = Ref(buildModel(root, lw, *ci));
    u::setName(root, "CW_" + lw.def.name);
    applyStats(w, lw.def);
    auto* raw = ci.get();
    byRoot_[id] = std::move(ci);
    return raw;
}

void Skinner::gameGunAmmo(Obj w, const WeaponDefinition& def, std::vector<std::string>* done) {
    auto mode = lower(trim(game_.cfg.raw("GameGunAmmo", "empty")));
    if (mode == "leave") return;
    double value = mode == "sync" ? (def.infiniteMagazine() ? 999 : def.magazineSize) : 0;
    std::string used;
    if (mode == "sync" && setFirst(w, game_.cfg.list("Stat.MaxAmmo"), Value::Num(value), &used) && done) done->push_back("MaxAmmo->" + used);
    if (setFirst(w, game_.cfg.list("Stat.Ammo"), Value::Num(value), &used) && done) done->push_back("Ammo->" + used);
}

void Skinner::applyStats(Obj w, const WeaponDefinition& def) {
    std::vector<std::string> done;
    auto set = [&](const std::string& stat, const Value& v) {
        std::string used;
        if (setFirst(w, game_.cfg.list("Stat." + stat), v, &used)) done.push_back(stat + "->" + used);
    };
    set("Damage", Value::Num(def.damage));
    set("Force", Value::Num(def.damageForce));
    set("DamageType", Value::Str(def.damageType));
    if (def.durability > 0) { set("Durability", Value::Num(def.durability)); set("CurrentDurability", Value::Num(def.durability)); }
    if (def.unbreakable) set("Unbreakable", Value::Bool(true));
    if (def.attackSpeed > 0) set("AttackSpeed", Value::Num(def.attackSpeed));
    if (def.throwDamage > 0) set("ThrowDamage", Value::Num(def.throwDamage));
    if (def.isRanged()) gameGunAmmo(w, def, &done);
    set("Name", Value::Str(def.name));
    auto key = def.id + className(classOf(w));
    if (statLogged_.insert(key).second) {
        std::string list;
        for (auto& d : done) list += d + " ";
        logInfo("[" + def.id + "] stats written onto " + className(classOf(w)) + ": " + (list.empty() ? "none matched (edit Stat.* in _GameHooks.cfg)" : list));
    }
}

void Skinner::enforceGameGunAmmo(CustomInstance& ci) {
    if (!ci.def().isRanged() || !iequals(trim(game_.cfg.raw("GameGunAmmo", "empty")), "empty")) return;
    Obj w = ci.component.get();
    Value v;
    if (getFirst(w, game_.cfg.list("Stat.Ammo"), fNumber, v) && v.n != 0) gameGunAmmo(w, ci.def(), nullptr);
}

void Skinner::applyPrice(Obj w, Obj shop, int price) {
    bool ok = shop && setFirst(shop, game_.cfg.list("ShopPriceMembers"), Value::Num(price));
    if (!ok) ok = setFirst(w, game_.cfg.list("Stat.Price"), Value::Num(price));
    if (!ok) logOnce("price", "Could not find a price field for shop items (see ShopPriceMembers in _GameHooks.cfg).");
}

// ====================================================================== Damage

bool Damage::apply(Obj col, const HitInfo& hit, bool targetIsPlayer) {
    if (!targetIsPlayer)
        for (auto k : game_.hitParts)
            if (Obj part = u::componentInParent(col, k); part && tryInvoke(part, classOf(part), "HitPartDamageMethods", hit)) return true;
    Il2CppClass* ownerClass = targetIsPlayer ? game_.player : game_.enemy;
    Obj owner = u::componentInParent(col, ownerClass);
    if (!owner) return false;
    const char* key = targetIsPlayer ? "PlayerDamageMethods" : "EnemyDamageMethods";
    if (tryInvoke(owner, classOf(owner), key, hit)) return true;
    return subtractHealth(owner, hit.damage);
}

static bool isNumberType(int te) {
    return te == T_I1 || te == T_U1 || te == T_I2 || te == T_U2 || te == T_I4 || te == T_U4 || te == T_I8 || te == T_U8 || te == T_R4 || te == T_R8;
}

static bool nameHas(const std::string& n, std::initializer_list<const char*> words) {
    auto l = lower(n);
    for (auto w : words) if (contains(l, w)) return true;
    return false;
}

static void putNumber(std::vector<uint8_t>& raw, int te, double v) {
    raw.assign(8, 0);
    switch (te) {
        case T_R4: { float f = (float)v; std::memcpy(raw.data(), &f, 4); break; }
        case T_R8: std::memcpy(raw.data(), &v, 8); break;
        default: { int64_t i = (int64_t)std::llround(v); std::memcpy(raw.data(), &i, 8); break; }
    }
}

/// Fills a damage-info object/struct from the game by field names.
static bool fillInfo(Obj obj, Il2CppClass* k, const HitInfo& hit, uint8_t* structData) {
    auto setField = [&](std::initializer_list<const char*> names, auto writer) {
        for (auto n : names)
            for (auto& m : findMembers(k, n))
                if (m.field && writer(m)) return true;
        return false;
    };
    auto writeValue = [&](const Member& m, const Value& v) {
        if (obj) return setMember(obj, m, v);
        std::vector<uint8_t> raw;
        Obj o;
        if (!convertTo(v, m.type, raw, o) || o) return false;
        std::memcpy(structData + il2cpp_field_get_offset(m.field) - sizeof(Il2CppObject), raw.data(), raw.size());
        return true;
    };
    auto writeVec = [&](const Member& m, Vec3 v) {
        if (typeName(m.type) != "UnityEngine.Vector3") return false;
        if (obj) il2cpp_field_set_value(obj, m.field, &v);
        else std::memcpy(structData + il2cpp_field_get_offset(m.field) - sizeof(Il2CppObject), &v, sizeof(v));
        return true;
    };
    bool any = setField({"damage", "Damage", "amount", "dmg", "*damage*"}, [&](const Member& m) { return writeValue(m, Value::Num(hit.damage)); });
    setField({"force", "Force", "knockback", "*force*"}, [&](const Member& m) { return writeValue(m, Value::Num(hit.force)); });
    setField({"damageType", "type", "DamageType", "*type*"}, [&](const Member& m) { return writeValue(m, Value::Str(hit.damageType)); });
    setField({"point", "hitPoint", "position", "pos", "Point"}, [&](const Member& m) { return writeVec(m, hit.point); });
    setField({"direction", "dir", "hitDirection", "normal", "Direction"}, [&](const Member& m) { return writeVec(m, hit.direction); });
    return any;
}

bool Damage::buildArgs(const MethodInfo* m, const HitInfo& hit, int& score, std::vector<std::vector<uint8_t>>& storage,
                       std::vector<void*>& args, bool dryRun) {
    uint32_t n = il2cpp_method_get_param_count(m);
    storage.assign(n, {});
    args.assign(n, nullptr);
    score = 0;
    bool damageUsed = false, pointUsed = false, hasInfo = false;
    Vec3 dir = hit.direction.lengthSq() > 1e-6f ? normalize(hit.direction) : Vec3(0, 0, 1);
    for (uint32_t i = 0; i < n; i++) {
        auto t = il2cpp_method_get_param(m, i);
        std::string pn = il2cpp_method_get_param_name(m, i) ? il2cpp_method_get_param_name(m, i) : "";
        int te = il2cpp_type_get_type(t);
        auto& raw = storage[i];
        if (il2cpp_type_is_byref(t)) { score -= 100; continue; }
        auto kind = kindOf(t);
        auto tn = typeName(t);
        if (isNumberType(te)) {
            double v;
            if (nameHas(pn, {"force", "knock", "push", "impulse", "power"})) { v = hit.force; score += 2; }
            else if (!damageUsed || nameHas(pn, {"damage", "dmg", "amount", "hp", "value"})) { v = hit.damage; damageUsed = true; score += 10; }
            else { v = 0; score -= 3; }
            putNumber(raw, te, v);
            args[i] = raw.data();
        } else if (tn == "UnityEngine.Vector3") {
            Vec3 v;
            if (nameHas(pn, {"dir", "normal", "velocity", "force", "impulse", "knock", "push"})) {
                v = nameHas(pn, {"force", "impulse", "knock", "push", "velocity"}) ? dir * hit.force : dir;
                score += 3;
            } else if (!pointUsed) { v = hit.point; pointUsed = true; score += 3; }
            else { v = dir * hit.force; score += 1; }
            raw.resize(sizeof(Vec3));
            std::memcpy(raw.data(), &v, sizeof(v));
            args[i] = raw.data();
        } else if (kind == Kind::Bool) {
            raw.assign(8, 0);
            args[i] = raw.data();
            score -= 1;
        } else if (kind == Kind::String) {
            if (!dryRun) { Obj sv = (Obj)str(hit.damageType); keepFrame(sv); args[i] = sv; }
            score += 1;
        } else if (kind == Kind::Enum) {
            int64_t ev = 0;
            enumFromName(il2cpp_class_from_type(t), hit.damageType, ev);
            raw.assign(8, 0);
            std::memcpy(raw.data(), &ev, 8);
            args[i] = raw.data();
            score += 1;
        } else if (kind == Kind::Object) {
            auto pk = il2cpp_class_from_type(t);
            Obj chosen = nullptr;
            if (pk == u::GameObject) chosen = hit.attacker ? u::gameObject(hit.attacker) : nullptr;
            else if (pk == u::Transform) chosen = hit.attacker ? u::transform(hit.attacker) : nullptr;
            else if (isSubclass(pk, u::Component)) {
                for (Obj c : {hit.attacker, hit.weaponComp}) {
                    if (!u::alive(c)) continue;
                    if (isA(c, pk)) { chosen = c; break; }
                    if (Obj f = u::componentInParent(c, pk)) { chosen = f; break; }
                }
                score += chosen ? 2 : -1;
            } else if (!isSubclass(pk, u::Object)) {
                // a damage-info class from the game: create and fill it
                if (findMember(pk, "*damage*") || findMember(pk, "amount")) {
                    score += 8; hasInfo = true;
                    if (!dryRun) { chosen = il2cpp_object_new(pk); keepFrame(chosen); il2cpp_runtime_object_init(chosen); fillInfo(chosen, pk, hit, nullptr); }
                } else score -= 8;
            } else score -= 5;
            args[i] = chosen;
        } else if (kind == Kind::Struct) {
            auto pk = il2cpp_class_from_type(t);
            uint32_t align = 0;
            int size = il2cpp_class_value_size(pk, &align);
            raw.assign(std::max(size, 8), 0);
            if (findMember(pk, "*damage*") || findMember(pk, "amount")) { score += 8; hasInfo = true; if (!dryRun) fillInfo(nullptr, pk, hit, raw.data()); }
            else score -= 8;
            args[i] = raw.data();
        } else {
            score -= 10;
        }
    }
    if (!damageUsed && !hasInfo) score -= 20;
    return true;
}

const MethodInfo* Damage::choose(Il2CppClass* k, const std::vector<std::string>& names) {
    const MethodInfo* best = nullptr;
    int bestScore = -1000000;
    for (size_t i = 0; i < names.size(); i++) {
        for (auto c = k; c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            while (auto m = il2cpp_class_get_methods(c, &it)) {
                if (!iequals(il2cpp_method_get_name(m), names[i]) || il2cpp_method_is_generic(m) || isStatic(m) || broken_.count(m)) continue;
                int score;
                std::vector<std::vector<uint8_t>> st;
                std::vector<void*> args;
                buildArgs(m, HitInfo(), score, st, args, true);
                score -= (int)i * 3;
                if (score > bestScore) { best = m; bestScore = score; }
            }
        }
    }
    return bestScore > -50 ? best : nullptr;
}

bool Damage::tryInvoke(Obj target, Il2CppClass* k, const char* key, const HitInfo& hit) {
    auto ck = std::make_pair(k, std::string(key));
    auto it = chosen_.find(ck);
    if (it == chosen_.end()) {
        auto m = choose(k, game_.cfg.list(key));
        it = chosen_.emplace(ck, m).first;
        if (m) {
            std::string sig;
            for (uint32_t i = 0; i < il2cpp_method_get_param_count(m); i++)
                sig += (i ? ", " : "") + typeName(il2cpp_method_get_param(m, i)) + " " + il2cpp_method_get_param_name(m, i);
            logInfo("Damage on " + className(k) + " will use " + il2cpp_method_get_name(m) + "(" + sig + ")");
        } else logOnce("nodmg" + className(k) + key, "No usable damage method on " + className(k) + " (tried " + key + " in _GameHooks.cfg).");
    }
    auto m = it->second;
    if (!m) return false;
    int score;
    std::vector<std::vector<uint8_t>> storage;
    std::vector<void*> args;
    buildArgs(m, hit, score, storage, args, false);
    bool threw = false;
    invoke(m, target, args.empty() ? nullptr : args.data(), &threw);
    if (!threw) return true;
    broken_.insert(m);
    chosen_.erase(it); // choose another one next time
    return false;
}

bool Damage::subtractHealth(Obj owner, float dmg) {
    Value v;
    std::string path;
    if (!getFirst(owner, game_.cfg.list("HealthMembers"), fNumber, v, &path)) return false;
    double next = v.n - dmg;
    for (auto& setter : game_.cfg.list("SetHealthMethods")) {
        if (auto m = methodI(classOf(owner), setter, 1)) {
            auto t = il2cpp_method_get_param(m, 0);
            if (!isNumberType(il2cpp_type_get_type(t))) continue;
            std::vector<uint8_t> raw;
            putNumber(raw, il2cpp_type_get_type(t), next);
            void* a[] = {raw.data()};
            bool threw = false;
            invoke(m, owner, a, &threw);
            if (!threw) return true;
        }
    }
    if (setFirst(owner, {path}, Value::Num(next))) {
        logOnce("hp" + className(classOf(owner)), "Damage on " + className(classOf(owner)) + " falls back to lowering '" + path + "' directly.", false);
        return true;
    }
    return false;
}

// ====================================================================== keys

namespace keys {
namespace {
std::map<int, bool> g_now, g_prev;
bool raw(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
}

void poll() {
    g_prev = g_now;
    for (auto& kv : g_now) kv.second = raw(kv.first);
}

bool down(int vk) {
    if (vk <= 0) return false;
    auto it = g_now.find(vk);
    if (it == g_now.end()) it = g_now.emplace(vk, raw(vk)).first;
    return it->second;
}

bool pressed(int vk) {
    bool d = down(vk);
    auto it = g_prev.find(vk);
    return d && (it == g_prev.end() || !it->second);
}

bool gameHasFocus() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

int code(const std::string& name) {
    auto n = normalizeKey(name);
    if (n.empty() || n == "none") return 0;
    if (n == "mouse0" || n == "lmb" || n == "leftmouse" || n == "mouseleft") return VK_LBUTTON;
    if (n == "mouse1" || n == "rmb" || n == "rightmouse" || n == "mouseright") return VK_RBUTTON;
    if (n == "mouse2" || n == "mmb" || n == "middlemouse") return VK_MBUTTON;
    if (n == "mouse3") return VK_XBUTTON1;
    if (n == "mouse4") return VK_XBUTTON2;
    if (n == "space") return VK_SPACE;
    if (n == "tab") return VK_TAB;
    if (n == "enter" || n == "return") return VK_RETURN;
    if (n == "leftshift" || n == "shift") return VK_LSHIFT;
    if (n == "leftcontrol" || n == "ctrl" || n == "control") return VK_LCONTROL;
    if (n == "leftalt" || n == "alt") return VK_LMENU;
    if (n.size() == 1 && std::isalnum((unsigned char)n[0])) return std::toupper((unsigned char)n[0]);
    if (startsWith(n, "alpha") && n.size() == 6 && std::isdigit((unsigned char)n[5])) return n[5];
    if (n[0] == 'f' && n.size() <= 3) { int f = std::atoi(n.c_str() + 1); if (f >= 1 && f <= 24) return VK_F1 + f - 1; }
    if (startsWith(n, "0x")) return (int)std::strtol(n.c_str() + 2, nullptr, 16);
    return 0;
}
} // namespace keys

} // namespace cw
