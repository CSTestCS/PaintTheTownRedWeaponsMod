// Game-side logic: finding the game's classes, turning game weapons into custom ones, spawning,
// firing, projectiles and damage. Everything here runs on Unity's main thread.
#pragma once
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>
#include "../core/weapon.hpp"
#include "il2cpp.hpp"
#include "unity.hpp"

namespace cw {

using il2::Obj;
using il2::Ref;

enum class Holder { None, Player, Enemy, Shop };

/// The game's own classes, resolved by name from _GameHooks.cfg.
class Bridge {
public:
    explicit Bridge(const HooksConfig& cfg) : cfg(cfg) {}
    const HooksConfig& cfg;
    Il2CppClass *player = nullptr, *enemy = nullptr, *weapon = nullptr;
    std::vector<Il2CppClass*> shops, hitParts;
    bool resolved = false;

    void resolve();
    Obj localPlayer();                 // cached, refreshed once a second
    Obj heldWeapon(Obj holder, const char* membersKey);
    bool isDead(Obj enemyObj);
    int isRangedGameWeapon(Obj weaponObj); // 1 yes, 0 no, -1 unknown
    Holder holderOf(Obj weaponObj, Obj* holder = nullptr);
    Obj shopInParent(Obj c);
private:
    Ref player_;
    float nextPlayerSearch_ = 0;
};

struct CustomInstance {
    LoadedWeapon* weapon = nullptr;
    Ref root, component, model;
    std::vector<Ref> hidden;
    int magazine = 0, reserve = 0, burstLeft = 0;
    float reloadDoneAt = -1, nextShotAt = 0, nextBurstShotAt = 0;
    const WeaponDefinition& def() const { return weapon->def; }
    bool reloading() const { return reloadDoneAt > 0; }
};

/// Assets built once per weapon (mesh / texture / materials).
struct WeaponAssets { Ref mesh, projectileMesh, texture; std::map<std::string, Ref> materials; };

class Skinner {
public:
    explicit Skinner(Bridge& b) : game_(b) {}
    CustomInstance* get(Obj weaponComponent);
    bool isCustom(Obj w) { return get(w) != nullptr; }
    CustomInstance* apply(Obj weaponComponent, LoadedWeapon& lw);
    void applyStats(Obj weaponComponent, const WeaponDefinition& def);
    void applyPrice(Obj weaponComponent, Obj shop, int price);
    void enforceGameGunAmmo(CustomInstance& ci);
    void keepOriginalsHidden();
    void cleanup();
    void clearAll() { byRoot_.clear(); assets_.clear(); }
    std::vector<CustomInstance*> all();
    WeaponAssets& assets(LoadedWeapon& lw);
    Obj material(LoadedWeapon& lw, const std::string& key, Obj source, bool textured, Vec4 tint);
private:
    Obj buildModel(Obj root, LoadedWeapon& lw, CustomInstance& ci);
    void gameGunAmmo(Obj w, const WeaponDefinition& def, std::vector<std::string>* done);
    Bridge& game_;
    std::map<int, std::unique_ptr<CustomInstance>> byRoot_;
    std::map<std::string, WeaponAssets> assets_;
    std::set<std::string> statLogged_;
};

struct HitInfo {
    float damage = 0, force = 0;
    Vec3 point, direction;
    std::string damageType;
    Obj attacker = nullptr, weaponComp = nullptr;
};

class Damage {
public:
    explicit Damage(Bridge& b) : game_(b) {}
    bool apply(Obj hitCollider, const HitInfo& hit, bool targetIsPlayer);
private:
    bool tryInvoke(Obj target, Il2CppClass* k, const char* methodsKey, const HitInfo& hit);
    const MethodInfo* choose(Il2CppClass* k, const std::vector<std::string>& names);
    bool buildArgs(const MethodInfo* m, const HitInfo& hit, int& score, std::vector<std::vector<uint8_t>>& storage, std::vector<void*>& args, bool dryRun);
    bool subtractHealth(Obj owner, float damage);
    Bridge& game_;
    std::map<std::pair<Il2CppClass*, std::string>, const MethodInfo*> chosen_;
    std::set<const MethodInfo*> broken_;
};

class Projectiles {
public:
    Projectiles(Bridge& b, Damage& d, Skinner& s) : game_(b), damage_(d), skinner_(s) {}
    void fire(CustomInstance& ci, Vec3 origin, Vec3 aim, Vec3 visualOrigin, bool fromPlayer, Obj owner, float extraSpread, float damageMult);
    void update();
    void clear();
private:
    struct Shot {
        LoadedWeapon* w = nullptr;
        Vec3 pos, vel, visualOffset;
        float born = 0, dieAt = 0, damageMult = 1;
        int bounces = 0, pierce = 0;
        bool fromPlayer = true;
        Ref owner, weaponComp, visual;
        std::set<int> hit;
    };
    struct Effect { Ref go; float born = 0, life = 0, startScale = 0, endScale = 0; bool scales = true; };
    bool trace(Shot& s, Vec3 dir, float dist, Vec3& end, bool& bounced);
    void explode(Shot& s, Vec3 point);
    Obj makeVisual(CustomInstance& ci);
    void updateVisual(Shot& s);
    void tracer(CustomInstance& ci, Vec3 from, Vec3 to);
    void flash(LoadedWeapon& w, Vec3 point, float radius);
    void paint(Obj go, LoadedWeapon& w, Vec4 color);
    HitInfo makeHit(Shot& s, Vec3 point, Vec3 dir, float dmg);
    Bridge& game_;
    Damage& damage_;
    Skinner& skinner_;
    std::vector<std::unique_ptr<Shot>> shots_;
    std::vector<Effect> effects_;
    std::mt19937 rng_{std::random_device{}()};
};

class Combat {
public:
    Combat(Bridge& b, Skinner& s, Projectiles& p);
    void update();
    void clear() { shooters_.clear(); playerWeapon_ = nullptr; }
    void trackEnemy(Obj enemy, CustomInstance* gun);
    CustomInstance* playerWeapon() const { return playerWeapon_; }
private:
    struct Shooter { Ref enemy; CustomInstance* gun = nullptr; Ref gunRoot; float nextVolley = 0, nextShot = 0; int volleyLeft = 0; };
    void updatePlayer();
    void updateEnemies();
    void playerShoot(CustomInstance& ci);
    bool canSee(Vec3 from, Vec3 to, float dist, Obj self);
    Bridge& game_;
    Skinner& skinner_;
    Projectiles& projectiles_;
    std::map<int, Shooter> shooters_;
    CustomInstance* playerWeapon_ = nullptr;
    float nextHeldCheck_ = 0;
    int fireKey_, reloadKey_;
    std::mt19937 rng_{std::random_device{}()};
};

class Director {
public:
    Director(Bridge& b, Skinner& s, Combat& c, std::vector<LoadedWeapon>& weapons) : game_(b), skinner_(s), combat_(c), weapons_(weapons) {}
    void onLevel(const std::string& scene, bool cameFromMenu);
    void update();
    void spawnAllInFront();
    const std::string& scene() const { return scene_; }
private:
    LoadedWeapon* pick(bool (*allowed)(const WeaponDefinition&), float (*chance)(const WeaponDefinition&), int ranged);
    Obj findTemplate(const WeaponDefinition& def, const std::vector<Obj>& sceneWeapons);
    std::vector<Obj> looseWeapons();
    CustomInstance* spawnCopy(LoadedWeapon& lw, Obj templ, Vec3 pos);
    bool findFloor(Vec3 around, Vec3& floor);
    void spawnOnGround(const std::vector<Obj>* anchors);
    void spawnStartingArea();
    void checkForNewFloor();
    void scanEnemies();
    void scanShops();
    void tryEquip(Obj enemy, LoadedWeapon& lw);
    float chance(float c) const;
    Bridge& game_;
    Skinner& skinner_;
    Combat& combat_;
    std::vector<LoadedWeapon>& weapons_;
    std::mt19937 rng_{std::random_device{}()};
    std::string scene_;
    bool startingArea_ = false;
    float setupAt_ = -1, giveUpAt_ = 0, nextScan_ = 0, nextFloorCheck_ = 0;
    std::set<int> seenEnemies_, seenShopWeapons_, seenLoose_;
    std::map<int, int> enemyScans_;
};

void writeReport(const std::string& folder, Bridge& game, Skinner& skinner, const std::string& scene);

/// Windows key state (works whatever input system the game uses).
namespace keys {
int code(const std::string& name);
void poll();
bool down(int vk);
bool pressed(int vk);
bool gameHasFocus();
}

} // namespace cw
