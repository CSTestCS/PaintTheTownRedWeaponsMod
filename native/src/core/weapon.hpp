// Weapon definitions (.txt files) and the game-integration settings (_GameHooks.cfg).
#pragma once
#include <map>
#include <string>
#include <vector>
#include "fbx.hpp"
#include "math.hpp"

namespace cw {

enum class WeaponKind { Melee, Ranged };
enum class FireMode { Single, Burst, Automatic };
enum class ProjectileVisual { Bullet, Sphere, Cube, Arrow, Model, None };

struct WeaponDefinition {
    std::string id, name, description, modelPath, texturePath, baseWeapon;
    WeaponKind kind = WeaponKind::Melee;
    float damage = 25, areaOfEffect = 0, damageForce = 10, durability = 0, attackSpeed = 0, throwDamage = 0;
    std::string damageType = "Blunt";
    bool areaFalloff = true, unbreakable = false;

    int magazineSize = 12, reserveAmmo = -1, burstCount = 3, pellets = 1, projectileBounces = 0, pierce = 0;
    float reloadTime = 1.5f, fireRate = 6, spread = 1, range = 300, projectileSpeed = 60, projectileDrop = 0,
          bounciness = 0.7f, projectileSize = 0.08f, projectileLifetime = 6, recoil = 0;
    FireMode fireMode = FireMode::Single;
    bool hitscan = false;
    ProjectileVisual projectileVisual = ProjectileVisual::Bullet;
    std::string projectileModelPath;
    Vec4 projectileColor{1.f, 0.85f, 0.25f, 1.f};

    bool appearsAtStartingArea = false, appearsOnGround = true, appearsInShop = true, usedByEnemies = true;
    float groundChance = 0.35f, shopChance = 0.25f, enemyChance = 0.15f, spawnWeight = 1;
    int shopPrice = 100;
    float enemyFireInterval = 1.2f, enemyAccuracy = 4, enemyDamageMultiplier = 0.5f;

    float modelScale = 1;
    Vec3 modelRotation, modelOffset;
    bool autoFit = true;
    Vec4 tint{1, 1, 1, 1};

    std::vector<std::string> warnings;

    bool isRanged() const { return kind == WeaponKind::Ranged; }
    bool infiniteMagazine() const { return magazineSize <= 0; }
};

/// Parses .txt contents. Never throws: problems become entries in warnings.
WeaponDefinition parseWeapon(const std::string& text, const std::string& id);
/// Parses a .txt file and resolves the .fbx / texture / projectile model next to it.
WeaponDefinition parseWeaponFile(const std::string& txtPath);

bool parseBool(const std::string& v, bool& out);
Vec4 parseColor(const std::string& v, bool& ok);

struct LoadedWeapon {
    WeaponDefinition def;
    MeshData mesh;
    MeshData projectileMesh;
    bool hasProjectileMesh = false;
    std::vector<uint8_t> texture; // png/jpg bytes (sidecar file or embedded in the fbx)
};

/// Every .txt with a matching .fbx in the folder (recursively) becomes a weapon.
std::vector<LoadedWeapon> loadWeaponFolder(const std::string& folder, std::vector<std::string>& log);

/// _GameHooks.cfg: "Key = a, b, c" lines. User values override defaults key by key.
class HooksConfig {
public:
    static const char* defaultText();
    static HooksConfig parse(const std::string& text);
    static HooksConfig loadOrCreate(const std::string& folder);
    const std::vector<std::string>& list(const std::string& key) const;
    std::string raw(const std::string& key, const std::string& def = "") const;
    float num(const std::string& key, float def) const;
    bool flag(const std::string& key, bool def) const;
private:
    std::map<std::string, std::vector<std::string>> lists_;
    std::map<std::string, std::string> raw_;
};

} // namespace cw
