// Projectiles, firing (player and enemies) and where custom weapons appear.
#include <algorithm>
#include <cmath>
#include <regex>
#include "../core/ballistics.hpp"
#include "../core/strutil.hpp"
#include "game.hpp"
#include "log.hpp"

namespace cw {

using namespace il2;

static bool belongsTo(Obj col, Obj owner) {
    if (!u::alive(owner)) return false;
    Obj ot = u::transform(owner);
    Obj t = u::transform(col);
    for (int i = 0; i < 64 && u::alive(t); i++) {
        if (t == ot) return true;
        t = u::parent(t);
    }
    return false;
}

static void push(Obj col, Vec3 dir, float force) {
    if (force <= 0) return;
    Obj rb = u::attachedRigidbody(col);
    if (rb && !u::isKinematic(rb)) u::addForce(rb, dir * force);
}

// ====================================================================== Projectiles

void Projectiles::clear() {
    for (auto& s : shots_) u::destroy(s->visual.get());
    for (auto& e : effects_) u::destroy(e.go.get());
    shots_.clear();
    effects_.clear();
}

HitInfo Projectiles::makeHit(Shot& s, Vec3 point, Vec3 dir, float dmg) {
    HitInfo h;
    h.damage = dmg;
    h.point = point;
    h.direction = dir;
    h.force = s.w->def.damageForce;
    h.damageType = s.w->def.damageType;
    h.attacker = s.owner.get();
    h.weaponComp = s.weaponComp.get();
    return h;
}

void Projectiles::fire(CustomInstance& ci, Vec3 origin, Vec3 aim, Vec3 visualOrigin, bool fromPlayer, Obj owner, float extraSpread, float damageMult) {
    auto& def = ci.def();
    for (int i = 0; i < def.pellets; i++) {
        Vec3 dir = ballistics::spread(aim, def.spread + extraSpread, rng_);
        auto s = std::make_unique<Shot>();
        s->w = ci.weapon;
        s->pos = origin;
        s->fromPlayer = fromPlayer;
        s->owner = Ref(owner);
        s->weaponComp = Ref(ci.component.get());
        s->pierce = def.pierce;
        s->damageMult = damageMult;
        if (def.hitscan) {
            s->vel = dir;
            Vec3 end;
            bool bounced;
            trace(*s, dir, def.range, end, bounced);
            if (def.projectileVisual != ProjectileVisual::None) tracer(ci, visualOrigin, end);
            continue;
        }
        if (shots_.size() >= 400) { u::destroy(shots_.front()->visual.get()); shots_.erase(shots_.begin()); }
        s->vel = dir * def.projectileSpeed;
        s->visualOffset = visualOrigin - origin;
        s->born = u::time();
        s->dieAt = s->born + def.projectileLifetime;
        s->bounces = def.projectileBounces;
        s->visual = Ref(makeVisual(ci));
        updateVisual(*s);
        shots_.push_back(std::move(s));
    }
}

void Projectiles::update() {
    float dt = u::deltaTime(), now = u::time();
    for (size_t i = shots_.size(); i-- > 0;) {
        auto& s = *shots_[i];
        bool dead = now >= s.dieAt;
        if (!dead && dt > 0) {
            Vec3 move = ballistics::step(s.vel, s.w->def.projectileDrop, dt);
            float len = move.length();
            if (len > 1e-6f) {
                Vec3 end;
                bool bounced = false;
                if (!trace(s, move / len, len, end, bounced)) dead = true;
                else if (!bounced) s.pos = end;
            }
        }
        if (dead) { u::destroy(s.visual.get()); shots_.erase(shots_.begin() + i); continue; }
        updateVisual(s);
    }
    for (size_t i = effects_.size(); i-- > 0;) {
        auto& e = effects_[i];
        Obj go = e.go.get();
        float t = (now - e.born) / e.life;
        if (t >= 1 || !u::alive(go)) { u::destroy(go); effects_.erase(effects_.begin() + i); continue; }
        if (e.scales) u::setLocalScale(u::transform(go), Vec3(e.startScale + (e.endScale - e.startScale) * t));
    }
}

bool Projectiles::trace(Shot& s, Vec3 dir, float distance, Vec3& end, bool& bounced) {
    Vec3 origin = s.pos;
    float remaining = distance;
    bounced = false;
    auto& def = s.w->def;
    for (int guard = 0; guard < 12 && remaining > 0.0001f; guard++) {
        u::Hit hit;
        if (!u::raycast(origin, dir, remaining, hit)) { end = origin + dir * remaining; return true; }
        float step = std::max(hit.distance, 0.f) + 0.02f;
        Obj col = hit.collider;
        if (!u::alive(col) || belongsTo(col, s.owner.get())) { origin = hit.point + dir * 0.02f; remaining -= step; continue; }

        Obj enemy = u::componentInParent(col, game_.enemy);
        Obj player = enemy ? nullptr : u::componentInParent(col, game_.player);
        bool isTarget = s.fromPlayer ? enemy != nullptr : player != nullptr;
        bool isFriend = s.fromPlayer ? player != nullptr : enemy != nullptr;
        if (isFriend) { origin = hit.point + dir * 0.02f; remaining -= step; continue; }

        if (isTarget) {
            Obj target = enemy ? enemy : player;
            if (s.hit.insert(u::instanceId(target)).second) {
                damage_.apply(col, makeHit(s, hit.point, dir, def.damage * s.damageMult), !s.fromPlayer);
                push(col, dir, def.damageForce);
            }
            if (s.pierce > 0) { s.pierce--; origin = hit.point + dir * 0.05f; remaining -= step; continue; }
            explode(s, hit.point);
            end = hit.point;
            return false;
        }

        push(col, dir, def.damageForce * 0.5f);
        if (s.bounces > 0 && !def.hitscan && s.vel.lengthSq() > 0.01f) {
            s.bounces--;
            s.vel = ballistics::bounce(s.vel, hit.normal, def.bounciness);
            s.pos = hit.point + normalize(hit.normal) * 0.02f;
            bounced = true;
            end = s.pos;
            if (s.vel.length() < 1.5f) { explode(s, hit.point); return false; }
            return true;
        }
        explode(s, hit.point);
        end = hit.point;
        return false;
    }
    end = origin;
    return true;
}

void Projectiles::explode(Shot& s, Vec3 point) {
    auto& def = s.w->def;
    if (def.areaOfEffect <= 0) return;
    float r = def.areaOfEffect;
    flash(*s.w, point, r);
    std::set<int> damaged = s.hit;
    for (Obj col : u::overlapSphere(point, r)) {
        if (!u::alive(col)) continue;
        if (Obj rb = u::attachedRigidbody(col); rb && !u::isKinematic(rb)) u::addExplosionForce(rb, def.damageForce * 2, point, r, 0.4f);
        Obj target = u::componentInParent(col, s.fromPlayer ? game_.enemy : game_.player);
        if (!target || !damaged.insert(u::instanceId(target)).second) continue;
        Vec3 tp = u::position(u::transform(target));
        float dist = distance(tp, point);
        float dmg = ballistics::areaDamage(def.damage * s.damageMult, r, std::min(dist, r), def.areaFalloff);
        if (dmg <= 0) continue;
        Vec3 dir = dist > 0.01f ? (tp - point) / dist : Vec3(0, 1, 0);
        damage_.apply(col, makeHit(s, point, dir, dmg), !s.fromPlayer);
    }
}

void Projectiles::updateVisual(Shot& s) {
    Obj v = s.visual.get();
    if (!u::alive(v)) return;
    float k = std::max(0.f, 1.f - (u::time() - s.born) / 0.12f); // starts at the gun, catches up with the real shot
    Obj t = u::transform(v);
    u::setPosition(t, s.pos + s.visualOffset * k);
    if (s.vel.lengthSq() > 1e-6f) u::setRotation(t, ballistics::lookRotation(s.vel));
}

static void removeCollider(Obj go) {
    if (Obj c = u::getComponent(go, u::Collider)) u::destroy(c);
}

void Projectiles::paint(Obj go, LoadedWeapon& w, Vec4 color) {
    Obj r = u::getComponent(go, u::MeshRenderer);
    if (!r) return;
    if (Obj m = skinner_.material(w, "proj", u::sharedMaterial(r), false, color)) u::setSharedMaterial(r, m);
}

Obj Projectiles::makeVisual(CustomInstance& ci) {
    auto& def = ci.def();
    float size = def.projectileSize;
    switch (def.projectileVisual) {
        case ProjectileVisual::None: return nullptr;
        case ProjectileVisual::Model: {
            auto& a = skinner_.assets(*ci.weapon);
            bool own = ci.weapon->hasProjectileMesh && u::alive(a.projectileMesh.get());
            Obj mesh = own ? a.projectileMesh.get() : a.mesh.get();
            const MeshData& data = own ? ci.weapon->projectileMesh : ci.weapon->mesh;
            if (!mesh) return nullptr;
            Obj holder = u::newGameObject("CW_Projectile_" + def.id);
            Obj model = u::newGameObject("CW_ProjModel");
            u::setParent(u::transform(model), u::transform(holder), false);
            u::setSharedMesh(u::addComponent(model, u::MeshFilter), mesh);
            Obj mr = u::addComponent(model, u::MeshRenderer);
            Obj src = ci.model.get() ? u::sharedMaterial(u::getComponent(ci.model.get(), u::MeshRenderer)) : nullptr;
            if (Obj m = skinner_.material(*ci.weapon, "projmodel", src, true, def.tint)) u::setSharedMaterial(mr, m);
            Vec3 mn, mx;
            data.bounds(mn, mx);
            Vec3 sz = mx - mn;
            float longest = std::max(sz.x, std::max(sz.y, sz.z));
            float sc = longest > 1e-5f ? size / longest : 1.f;
            u::setLocalScale(u::transform(model), Vec3(sc));
            u::setLocalPosition(u::transform(model), (mn + mx) * (-0.5f * sc));
            return holder;
        }
        default: {
            Obj go = u::createPrimitive(def.projectileVisual == ProjectileVisual::Sphere ? 0 : 3);
            if (!go) return nullptr;
            u::setName(go, "CW_Projectile_" + def.id);
            removeCollider(go);
            Vec3 scale = def.projectileVisual == ProjectileVisual::Bullet ? Vec3(size, size, size * 5)
                       : def.projectileVisual == ProjectileVisual::Arrow  ? Vec3(size * 0.3f, size * 0.3f, size * 10)
                                                                          : Vec3(size);
            u::setLocalScale(u::transform(go), scale);
            paint(go, *ci.weapon, def.projectileColor);
            return go;
        }
    }
}

void Projectiles::tracer(CustomInstance& ci, Vec3 from, Vec3 to) {
    float len = distance(from, to);
    if (len < 0.05f) return;
    Obj go = u::createPrimitive(3);
    if (!go) return;
    removeCollider(go);
    float w = std::max(0.01f, ci.def().projectileSize * 0.4f);
    Obj t = u::transform(go);
    u::setPosition(t, (from + to) * 0.5f);
    u::setRotation(t, ballistics::lookRotation(to - from));
    u::setLocalScale(t, Vec3(w, w, len));
    paint(go, *ci.weapon, ci.def().projectileColor);
    effects_.push_back({Ref(go), u::time(), 0.07f, 0, 0, false});
}

void Projectiles::flash(LoadedWeapon& w, Vec3 point, float radius) {
    Obj go = u::createPrimitive(0);
    if (!go) return;
    removeCollider(go);
    u::setPosition(u::transform(go), point);
    paint(go, w, w.def.projectileColor);
    effects_.push_back({Ref(go), u::time(), 0.25f, radius * 0.3f, radius * 2.f, true});
}

// ====================================================================== Combat

Combat::Combat(Bridge& b, Skinner& s, Projectiles& p) : game_(b), skinner_(s), projectiles_(p) {
    fireKey_ = keys::code(b.cfg.raw("FireKey", "Mouse0"));
    reloadKey_ = keys::code(b.cfg.raw("ReloadKey", "R"));
}

void Combat::update() {
    updatePlayer();
    updateEnemies();
}

static void finishReload(CustomInstance& ci) {
    ci.reloadDoneAt = -1;
    int need = ci.def().magazineSize - ci.magazine;
    if (need <= 0) return;
    bool inf = ci.reserve >= (1 << 30);
    int take = inf ? need : std::min(need, ci.reserve);
    ci.magazine += take;
    if (!inf) ci.reserve -= take;
}

static void startReload(CustomInstance& ci) {
    ci.burstLeft = 0;
    if (ci.def().reloadTime <= 0) { finishReload(ci); return; }
    ci.reloadDoneAt = u::time() + ci.def().reloadTime;
}

static float burstGap(const WeaponDefinition& d) { return std::clamp(1.f / (d.fireRate * std::max(1, d.burstCount)), 0.04f, 0.15f); }

void Combat::updatePlayer() {
    Obj player = game_.localPlayer();
    playerWeapon_ = player ? skinner_.get(game_.heldWeapon(player, "PlayerWeaponMembers")) : nullptr;
    auto* ci = playerWeapon_;
    if (!ci || !ci->def().isRanged()) return;
    float now = u::time();
    if (now >= nextHeldCheck_) { nextHeldCheck_ = now + 0.25f; skinner_.enforceGameGunAmmo(*ci); }
    auto& def = ci->def();
    if (ci->reloading() && now >= ci->reloadDoneAt) finishReload(*ci);
    if (u::timeScale() <= 0 || !keys::gameHasFocus() || u::cursorFree()) return;

    if (keys::pressed(reloadKey_) && !ci->reloading() && !def.infiniteMagazine() && ci->magazine < def.magazineSize && ci->reserve > 0) startReload(*ci);
    if (ci->burstLeft > 0 && now >= ci->nextBurstShotAt) { ci->burstLeft--; ci->nextBurstShotAt = now + burstGap(def); playerShoot(*ci); }
    if (now < ci->nextShotAt) return;
    bool pressed = keys::pressed(fireKey_), down = keys::down(fireKey_);
    switch (def.fireMode) {
        case FireMode::Single:
            if (pressed) { ci->nextShotAt = now + 1.f / def.fireRate; playerShoot(*ci); }
            break;
        case FireMode::Automatic:
            if (down) { ci->nextShotAt = now + 1.f / def.fireRate; playerShoot(*ci); }
            break;
        case FireMode::Burst:
            if (pressed && ci->burstLeft == 0) {
                ci->nextShotAt = now + 1.f / def.fireRate;
                ci->burstLeft = def.burstCount - 1;
                ci->nextBurstShotAt = now + burstGap(def);
                playerShoot(*ci);
            }
            break;
    }
}

void Combat::playerShoot(CustomInstance& ci) {
    auto& def = ci.def();
    if (ci.reloading()) { ci.burstLeft = 0; return; }
    if (!def.infiniteMagazine()) {
        if (ci.magazine <= 0) { ci.burstLeft = 0; if (ci.reserve > 0) startReload(ci); return; }
        ci.magazine--;
    }
    Obj cam = u::mainCamera();
    if (!cam) return;
    Obj ct = u::transform(cam);
    Vec3 camPos = u::position(ct), fwd = u::forward(ct);
    Vec3 origin = camPos + fwd * 0.3f;
    Obj root = ci.root.get();
    Vec3 visual = u::alive(root) ? u::position(u::transform(root)) + fwd * 0.3f : camPos + fwd * 0.8f;
    projectiles_.fire(ci, origin, fwd, visual, true, game_.localPlayer(), 0, 1);
    if (!def.infiniteMagazine() && ci.magazine <= 0 && ci.reserve > 0) startReload(ci);
}

void Combat::trackEnemy(Obj enemy, CustomInstance* gun) {
    if (!gun || !gun->def().isRanged()) return;
    int id = u::instanceId(enemy);
    auto it = shooters_.find(id);
    if (it != shooters_.end() && it->second.gunRoot.get() == gun->component.get()) return;
    Shooter s;
    s.enemy = Ref(enemy);
    s.gunRoot = Ref(gun->component.get());
    std::uniform_real_distribution<float> u01(0, 1);
    s.nextVolley = u::time() + 1.f + u01(rng_);
    shooters_[id] = std::move(s);
}

bool Combat::canSee(Vec3 from, Vec3 to, float dist, Obj self) {
    Vec3 dir = (to - from) / std::max(dist, 0.001f);
    Vec3 origin = from;
    float remaining = dist;
    int selfId = u::instanceId(self);
    for (int i = 0; i < 6 && remaining > 0.3f; i++) {
        u::Hit hit;
        if (!u::raycast(origin, dir, remaining, hit) || !u::alive(hit.collider)) return true;
        if (u::componentInParent(hit.collider, game_.player)) return true;
        Obj e = u::componentInParent(hit.collider, game_.enemy);
        if (e && u::instanceId(e) == selfId) { origin = hit.point + dir * 0.05f; remaining -= hit.distance + 0.05f; continue; }
        return false;
    }
    return true;
}

void Combat::updateEnemies() {
    if (shooters_.empty() || u::timeScale() <= 0) return;
    Obj cam = u::mainCamera();
    if (!game_.localPlayer() || !cam) return;
    Vec3 target = u::position(u::transform(cam)) - Vec3(0, 0.35f, 0);
    float now = u::time();
    float maxRange = game_.cfg.num("EnemyMaxRange", 35);
    std::uniform_real_distribution<float> u01(0, 1);
    for (auto it = shooters_.begin(); it != shooters_.end();) {
        auto& s = it->second;
        Obj enemy = s.enemy.get();
        CustomInstance* gun = skinner_.get(s.gunRoot.get());
        if (!u::alive(enemy) || !gun) { it = shooters_.erase(it); continue; }
        auto& def = gun->def();
        Vec3 muzzle = u::position(u::transform(gun->root.get())) + Vec3(0, 0.05f, 0);
        if (s.volleyLeft > 0) {
            if (now >= s.nextShot) {
                s.volleyLeft--;
                s.nextShot = now + 1.f / def.fireRate;
                Vec3 aim = target - muzzle;
                if (!def.hitscan && def.projectileDrop > 0 && def.projectileSpeed > 1) {
                    float t = aim.length() / def.projectileSpeed;
                    aim.y += 0.5f * ballistics::kGravity * def.projectileDrop * t * t;
                }
                projectiles_.fire(*gun, muzzle, normalize(aim), muzzle, false, enemy, def.enemyAccuracy, def.enemyDamageMultiplier);
            }
            ++it;
            continue;
        }
        if (now < s.nextVolley) { ++it; continue; }
        s.nextVolley = now + 0.4f;
        Obj holder = u::componentInParent(gun->component.get(), game_.enemy);
        if (!holder || u::instanceId(holder) != it->first || game_.isDead(enemy)) { it = shooters_.erase(it); continue; }
        float dist = distance(muzzle, target);
        if (dist > std::min(maxRange, def.range) || !canSee(muzzle, target, dist, enemy)) { ++it; continue; }
        s.volleyLeft = def.fireMode == FireMode::Automatic ? 3 + (int)(u01(rng_) * 3) : def.fireMode == FireMode::Burst ? def.burstCount : 1;
        s.nextShot = now;
        s.nextVolley = now + def.enemyFireInterval * (0.8f + 0.5f * u01(rng_));
        ++it;
    }
}

// ====================================================================== Director

static bool matches(const std::string& pattern, const std::string& text) {
    static std::map<std::string, std::unique_ptr<std::regex>> cache; // compiled once per pattern
    auto p = trim(pattern);
    if (p.empty()) return false;
    auto it = cache.find(p);
    if (it == cache.end()) {
        std::unique_ptr<std::regex> rx;
        auto flags = std::regex::ECMAScript;
        auto body = p;
        if (startsWith(body, "(?i)")) { body = body.substr(4); flags |= std::regex::icase; }
        try { rx = std::make_unique<std::regex>(body, flags); }
        catch (...) { logOnce("regex" + p, "Invalid pattern in _GameHooks.cfg: " + p); }
        it = cache.emplace(p, std::move(rx)).first;
    }
    return it->second && std::regex_search(text, *it->second);
}

float Director::chance(float c) const { return std::clamp(c * game_.cfg.num("ChanceMultiplier", 1), 0.f, 1.f); }

void Director::onLevel(const std::string& scene, bool cameFromMenu) {
    scene_ = scene;
    seenEnemies_.clear(); enemyScans_.clear(); seenShopWeapons_.clear(); seenLoose_.clear();
    combat_.clear();
    skinner_.cleanup();
    startingArea_ = matches(game_.cfg.raw("StartingAreaScenes"), scene) || cameFromMenu;
    setupAt_ = u::time() + std::max(0.f, game_.cfg.num("SceneSetupDelay", 2.5f));
    giveUpAt_ = setupAt_ + 30;
    logInfo("Level started: scene \"" + scene + "\"" + (startingArea_ ? " (starting area)" : ""));
}

void Director::update() {
    float now = u::time();
    bool menu = matches(game_.cfg.raw("MenuScenes"), scene_);
    auto gp = game_.cfg.raw("GameplayScenes");
    if (menu || (!trim(gp).empty() && !matches(gp, scene_))) return;

    if (setupAt_ > 0 && now >= setupAt_) {
        setupAt_ = -1;
        spawnOnGround(nullptr);
        if (startingArea_) spawnStartingArea();
    }
    if (setupAt_ < 0 && now >= nextScan_) {
        nextScan_ = now + std::max(0.2f, game_.cfg.num("ScanInterval", 1));
        skinner_.cleanup();
        skinner_.keepOriginalsHidden();
        scanEnemies();
        scanShops();
    }
    if (setupAt_ < 0 && now >= nextFloorCheck_) {
        nextFloorCheck_ = now + 4;
        checkForNewFloor();
    }
}

LoadedWeapon* Director::pick(bool (*allowed)(const WeaponDefinition&), float (*chanceOf)(const WeaponDefinition&), int ranged) {
    std::vector<LoadedWeapon*> pool, same;
    for (auto& w : weapons_) if (allowed(w.def) && w.def.spawnWeight > 0) pool.push_back(&w);
    if (ranged >= 0) {
        for (auto* w : pool) if (w->def.isRanged() == (ranged == 1)) same.push_back(w);
        if (!same.empty()) pool = same;
    }
    std::uniform_real_distribution<float> u01(0, 1);
    std::vector<LoadedWeapon*> winners;
    float total = 0;
    for (auto* w : pool) if (u01(rng_) < chance(chanceOf(w->def))) { winners.push_back(w); total += w->def.spawnWeight; }
    if (winners.empty()) return nullptr;
    float roll = u01(rng_) * total;
    for (auto* w : winners) { roll -= w->def.spawnWeight; if (roll <= 0) return w; }
    return winners.back();
}

std::vector<Obj> Director::looseWeapons() {
    std::vector<Obj> r;
    for (Obj w : u::findObjectsOfType(game_.weapon))
        if (u::activeInHierarchy(u::gameObject(w)) && game_.holderOf(w) == Holder::None) r.push_back(w);
    return r;
}

Obj Director::findTemplate(const WeaponDefinition& def, const std::vector<Obj>& scene) {
    std::vector<Obj> cands, assets;
    for (Obj w : scene) if (!skinner_.isCustom(w)) cands.push_back(w);
    for (Obj w : u::findAllIncludingAssets(game_.weapon))
        if (!skinner_.isCustom(w) && !startsWith(u::name(u::gameObject(w)), "CW_")) assets.push_back(w);
    if (!trim(def.baseWeapon).empty()) {
        for (auto* list : {&cands, &assets})
            for (Obj w : *list)
                if (icontains(u::name(u::gameObject(w)), def.baseWeapon) || iequals(className(classOf(w)), def.baseWeapon)) return w;
        logOnce("base" + def.id, "[" + def.id + "] Base Weapon \"" + def.baseWeapon + "\" not found; using a similar weapon.");
    }
    for (auto* list : {&cands, &assets}) {
        std::vector<Obj> same;
        for (Obj w : *list) if (game_.isRangedGameWeapon(w) == (def.isRanged() ? 1 : 0)) same.push_back(w);
        if (!same.empty()) return same[rng_() % same.size()];
    }
    if (!cands.empty()) return cands[rng_() % cands.size()];
    return assets.empty() ? nullptr : assets[rng_() % assets.size()];
}

CustomInstance* Director::spawnCopy(LoadedWeapon& lw, Obj templ, Vec3 pos) {
    Obj copy = u::instantiate(u::gameObject(templ));
    if (!copy) return nullptr;
    copy = u::gameObject(copy);
    Obj t = u::transform(copy);
    u::setParent(t, nullptr, true);
    u::setPosition(t, pos);
    std::uniform_real_distribution<float> ang(0, 2 * kPi);
    u::setRotation(t, Quat::axisAngle({0, 1, 0}, ang(rng_)));
    if (!u::activeSelf(copy)) u::setActive(copy, true);
    Obj w = u::getComponent(copy, game_.weapon);
    if (!w) { u::destroy(copy); return nullptr; }
    return skinner_.apply(w, lw);
}

bool Director::findFloor(Vec3 around, Vec3& floor) {
    u::Hit hit;
    if (u::raycast(around + Vec3(0, 1.5f, 0), {0, -1, 0}, 6, hit)) { floor = hit.point + Vec3(0, 0.15f, 0); return true; }
    floor = around;
    return false;
}

void Director::spawnOnGround(const std::vector<Obj>* anchors) {
    if (!game_.weapon) return;
    auto all = looseWeapons();
    for (Obj w : all) seenLoose_.insert(u::instanceId(w));
    const auto& loose = anchors ? *anchors : all;
    if (loose.empty()) { logInfo("No weapons lying around here; nothing to place custom weapons next to."); return; }
    int max = (int)game_.cfg.num("MaxGroundSpawnsPerScene", 6), spawned = 0;
    std::vector<LoadedWeapon*> order;
    for (auto& w : weapons_) order.push_back(&w);
    std::shuffle(order.begin(), order.end(), rng_);
    std::uniform_real_distribution<float> u01(0, 1), off(-1.2f, 1.2f);
    for (auto* lw : order) {
        if (spawned >= max) break;
        if (!lw->def.appearsOnGround || u01(rng_) >= chance(lw->def.groundChance)) continue;
        Obj templ = findTemplate(lw->def, all);
        if (!templ) continue;
        Vec3 anchor = u::position(u::transform(loose[rng_() % loose.size()]));
        Vec3 pos;
        findFloor(anchor + Vec3(off(rng_), 0, off(rng_)), pos);
        if (auto* ci = spawnCopy(*lw, templ, pos)) {
            seenLoose_.insert(u::instanceId(ci->component.get()));
            spawned++;
            logInfo("[" + lw->def.id + "] placed on the ground");
        }
    }
}

void Director::spawnStartingArea() {
    std::vector<LoadedWeapon*> starters;
    for (auto& w : weapons_) if (w.def.appearsAtStartingArea) starters.push_back(&w);
    Obj player = game_.localPlayer();
    if (starters.empty() || !player) return;
    Obj cam = u::mainCamera();
    Vec3 base = u::position(u::transform(player));
    Vec3 fwd = cam ? u::forward(u::transform(cam)) : Vec3(0, 0, 1);
    fwd.y = 0;
    fwd = fwd.lengthSq() > 0.01f ? normalize(fwd) : Vec3(0, 0, 1);
    Vec3 right = cross({0, 1, 0}, fwd);
    auto loose = looseWeapons();
    for (size_t i = 0; i < starters.size(); i++) {
        Obj templ = findTemplate(starters[i]->def, loose);
        if (!templ) { logWarn("[" + starters[i]->def.id + "] no game weapon to copy for the starting area."); continue; }
        Vec3 pos;
        findFloor(base + fwd * 2.2f + right * (((float)i - (starters.size() - 1) * 0.5f) * 0.9f), pos);
        if (spawnCopy(*starters[i], templ, pos)) logInfo("[" + starters[i]->def.id + "] placed in the starting area");
    }
}

void Director::spawnAllInFront() {
    Obj player = game_.localPlayer();
    Obj cam = u::mainCamera();
    if (!player || !cam) { logWarn("Spawn key: player not found (are you in a level?)."); return; }
    Obj ct = u::transform(cam);
    Vec3 camPos = u::position(ct), fwd = u::forward(ct);
    fwd.y = 0;
    fwd = fwd.lengthSq() > 0.01f ? normalize(fwd) : Vec3(0, 0, 1);
    Vec3 right = cross({0, 1, 0}, fwd);
    auto loose = looseWeapons();
    int ok = 0;
    for (size_t i = 0; i < weapons_.size(); i++) {
        Obj templ = findTemplate(weapons_[i].def, loose);
        if (!templ) continue;
        Vec3 pos;
        findFloor(camPos + fwd * 2 + right * (((float)i - (weapons_.size() - 1) * 0.5f) * 0.8f), pos);
        if (spawnCopy(weapons_[i], templ, pos)) ok++;
    }
    logInfo("Spawned " + std::to_string(ok) + "/" + std::to_string(weapons_.size()) + " custom weapons in front of the player.");
    if (ok == 0 && !weapons_.empty()) logWarn("Nothing could be spawned: no game weapon was found to copy. See _GameReport.txt.");
}

void Director::checkForNewFloor() {
    auto loose = looseWeapons();
    std::vector<Obj> fresh;
    for (Obj w : loose) if (!seenLoose_.count(u::instanceId(w)) && !skinner_.isCustom(w)) fresh.push_back(w);
    int threshold = (int)game_.cfg.num("NewFloorWeaponCount", 3);
    if (threshold > 0 && (int)fresh.size() >= threshold) {
        logInfo(std::to_string(fresh.size()) + " new weapons appeared - treating it as a new floor.");
        spawnOnGround(&fresh);
    } else for (Obj w : fresh) seenLoose_.insert(u::instanceId(w));
}

void Director::scanEnemies() {
    if (!game_.enemy) return;
    for (Obj enemy : u::findObjectsOfType(game_.enemy)) {
        Obj held = game_.heldWeapon(enemy, "EnemyWeaponMembers");
        int id = u::instanceId(enemy);
        if (held) seenLoose_.insert(u::instanceId(held)); // dropped weapons are not a "new floor"
        int scans = enemyScans_[id]++;
        bool ready = held || scans >= 2; // enemies may get their weapon a moment after spawning
        if (ready && seenEnemies_.insert(id).second && !game_.isDead(enemy)) {
            int ranged = held ? game_.isRangedGameWeapon(held) : -1;
            auto* lw = pick([](const WeaponDefinition& d) { return d.usedByEnemies; }, [](const WeaponDefinition& d) { return d.enemyChance; }, ranged);
            if (lw) {
                if (held && !skinner_.isCustom(held)) { skinner_.apply(held, *lw); logInfo("[" + lw->def.id + "] given to an enemy"); }
                else if (!held) tryEquip(enemy, *lw);
                held = game_.heldWeapon(enemy, "EnemyWeaponMembers");
            }
        }
        if (auto* ci = skinner_.get(held); ci && ci->def().isRanged()) combat_.trackEnemy(enemy, ci);
    }
}

void Director::tryEquip(Obj enemy, LoadedWeapon& lw) {
    Obj templ = findTemplate(lw.def, looseWeapons());
    if (!templ) return;
    auto* ci = spawnCopy(lw, templ, u::position(u::transform(enemy)) + Vec3(0, 1, 0));
    if (!ci) return;
    Obj w = ci->component.get();
    for (auto& name : game_.cfg.list("EnemyEquipMethods")) {
        for (auto c = classOf(enemy); c; c = il2cpp_class_get_parent(c)) {
            void* it = nullptr;
            while (auto m = il2cpp_class_get_methods(c, &it)) {
                if (!iequals(il2cpp_method_get_name(m), name) || il2cpp_method_is_generic(m) || isStatic(m)) continue;
                uint32_t n = il2cpp_method_get_param_count(m);
                if (n == 0 || kindOf(il2cpp_method_get_param(m, 0)) != Kind::Object) continue;
                if (!il2cpp_class_is_assignable_from(il2cpp_class_from_type(il2cpp_method_get_param(m, 0)), classOf(w))) continue;
                std::vector<std::vector<uint8_t>> st(n, std::vector<uint8_t>(64, 0));
                std::vector<void*> args(n);
                args[0] = w;
                for (uint32_t i = 1; i < n; i++) args[i] = kindOf(il2cpp_method_get_param(m, i)) == Kind::Object ? nullptr : st[i].data();
                bool threw = false;
                invoke(m, enemy, args.data(), &threw);
                if (!threw) { logInfo("[" + lw.def.id + "] equipped on an enemy via " + il2cpp_method_get_name(m)); return; }
            }
        }
    }
    logOnce("noequip", "Unarmed enemies can't be armed (no EnemyEquipMethods matched); only armed enemies get custom weapons.", false);
    u::destroy(ci->root.get());
}

void Director::scanShops() {
    if (game_.shops.empty() || !game_.weapon) return;
    for (auto k : game_.shops)
        for (Obj shop : u::findObjectsOfType(k))
            for (Obj w : u::componentsInChildren(shop, game_.weapon, true)) {
                if (!seenShopWeapons_.insert(u::instanceId(w)).second || skinner_.isCustom(w)) continue;
                auto* lw = pick([](const WeaponDefinition& d) { return d.appearsInShop; }, [](const WeaponDefinition& d) { return d.shopChance; },
                                game_.isRangedGameWeapon(w));
                if (!lw) continue;
                skinner_.apply(w, *lw);
                skinner_.applyPrice(w, shop, lw->def.shopPrice);
                logInfo("[" + lw->def.id + "] put up for sale in " + className(k) + " for " + std::to_string(lw->def.shopPrice));
            }
}

} // namespace cw
