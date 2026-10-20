#include "weapon.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "strutil.hpp"

namespace fs = std::filesystem;

namespace cw {

namespace {

struct BadValue : std::runtime_error { using std::runtime_error::runtime_error; };

const std::vector<std::string> kPositional = {
    "name", "meleeorranged", "magazinesize", "firetype", "projectilespeed", "projectiledrop", "automaticfirerate",
    "projectilebounce", "projectilevisual", "damage", "damagetype", "areaofeffect", "damageforce", "appearsatstartingarea"};

const std::map<std::string, std::string>& aliases() {
    static const std::map<std::string, std::string> a = {
        {"name", "name"}, {"weaponname", "name"}, {"displayname", "name"}, {"title", "name"},
        {"meleeorranged", "kind"}, {"type", "kind"}, {"weapontype", "kind"}, {"kind", "kind"}, {"class", "kind"}, {"category", "kind"},
        {"magazinesize", "magazine"}, {"magazine", "magazine"}, {"magsize", "magazine"}, {"clipsize", "magazine"}, {"ammo", "magazine"}, {"clip", "magazine"},
        {"reserveammo", "reserve"}, {"spareammo", "reserve"}, {"totalammo", "reserve"}, {"maxammo", "reserve"},
        {"reloadtime", "reload"}, {"reload", "reload"}, {"reloadspeed", "reload"},
        {"firetype", "firetype"}, {"firemode", "firetype"}, {"firingmode", "firetype"}, {"mode", "firetype"},
        {"projectilespeed", "speed"}, {"bulletspeed", "speed"}, {"velocity", "speed"}, {"muzzlevelocity", "speed"},
        {"projectiledrop", "drop"}, {"bulletdrop", "drop"}, {"gravity", "drop"}, {"drop", "drop"},
        {"automaticfirerate", "firerate"}, {"firerate", "firerate"}, {"rateoffire", "firerate"}, {"rof", "firerate"}, {"shotspersecond", "firerate"},
        {"projectilebounce", "bounce"}, {"projectilebounces", "bounce"}, {"bounce", "bounce"}, {"bounces", "bounce"}, {"ricochet", "bounce"},
        {"bounciness", "bounciness"}, {"bouncespeed", "bounciness"},
        {"projectilevisual", "visual"}, {"visual", "visual"}, {"projectilelook", "visual"}, {"projectile", "visual"},
        {"projectilemodel", "projmodel"},
        {"projectilesize", "projsize"}, {"bulletsize", "projsize"},
        {"projectilecolor", "projcolor"}, {"projectilecolour", "projcolor"}, {"bulletcolor", "projcolor"}, {"bulletcolour", "projcolor"},
        {"projectilelifetime", "projlife"}, {"projectilelife", "projlife"}, {"lifetime", "projlife"},
        {"pierce", "pierce"}, {"penetration", "pierce"}, {"piercing", "pierce"},
        {"pellets", "pellets"}, {"projectilespershot", "pellets"}, {"bulletspershot", "pellets"},
        {"spread", "spread"}, {"accuracy", "spread"}, {"inaccuracy", "spread"},
        {"burstcount", "burst"}, {"burstsize", "burst"}, {"burst", "burst"},
        {"hitscan", "hitscan"}, {"instanthit", "hitscan"},
        {"range", "range"}, {"maxrange", "range"}, {"recoil", "recoil"},
        {"damage", "damage"}, {"dmg", "damage"}, {"power", "damage"},
        {"damagetype", "damagetype"}, {"element", "damagetype"},
        {"areaofeffect", "aoe"}, {"aoe", "aoe"}, {"explosionradius", "aoe"}, {"blastradius", "aoe"}, {"splashradius", "aoe"}, {"radius", "aoe"},
        {"areafalloff", "falloff"}, {"aoefalloff", "falloff"}, {"falloff", "falloff"},
        {"damageforce", "force"}, {"force", "force"}, {"knockback", "force"}, {"impactforce", "force"},
        {"durability", "durability"}, {"hits", "durability"}, {"uses", "durability"},
        {"unbreakable", "unbreakable"}, {"indestructible", "unbreakable"},
        {"attackspeed", "attackspeed"}, {"swingspeed", "attackspeed"},
        {"throwdamage", "throwdamage"}, {"throwndamage", "throwdamage"},
        {"appearsatstartingarea", "start"}, {"startingarea", "start"}, {"appearsatstart", "start"}, {"spawnatstart", "start"}, {"startingweapon", "start"},
        {"appearsonground", "ground"}, {"ground", "ground"}, {"spawnsonground", "ground"},
        {"groundchance", "groundchance"}, {"groundspawnchance", "groundchance"}, {"spawnchance", "groundchance"},
        {"appearsinshop", "shop"}, {"inshop", "shop"}, {"shop", "shop"}, {"sold", "shop"},
        {"shopchance", "shopchance"}, {"shopprice", "price"}, {"price", "price"}, {"cost", "price"},
        {"usedbyenemies", "enemies"}, {"enemies", "enemies"}, {"enemyuse", "enemies"}, {"enemiescanuse", "enemies"},
        {"enemychance", "enemychance"}, {"spawnweight", "weight"}, {"weight", "weight"}, {"rarity", "weight"},
        {"enemyfireinterval", "enemyinterval"}, {"enemyfiredelay", "enemyinterval"},
        {"enemyaccuracy", "enemyspread"}, {"enemyspread", "enemyspread"},
        {"enemydamagemultiplier", "enemydmg"}, {"enemydamage", "enemydmg"},
        {"modelscale", "scale"}, {"scale", "scale"}, {"size", "scale"},
        {"modelrotation", "rotation"}, {"rotation", "rotation"},
        {"modeloffset", "offset"}, {"offset", "offset"}, {"position", "offset"},
        {"autofit", "autofit"}, {"autoscale", "autofit"},
        {"texture", "texture"}, {"image", "texture"},
        {"tint", "tint"}, {"color", "tint"}, {"colour", "tint"},
        {"baseweapon", "base"}, {"template", "base"}, {"basedon", "base"}, {"base", "base"},
        {"description", "description"}, {"desc", "description"},
    };
    return a;
}

const std::map<std::string, Vec4>& namedColors() {
    static const std::map<std::string, Vec4> c = {
        {"white", {1, 1, 1, 1}}, {"black", {0, 0, 0, 1}}, {"red", {1, 0.1f, 0.1f, 1}}, {"green", {0.2f, 1, 0.2f, 1}},
        {"blue", {0.2f, 0.4f, 1, 1}}, {"yellow", {1, 0.9f, 0.2f, 1}}, {"orange", {1, 0.55f, 0.1f, 1}}, {"purple", {0.6f, 0.2f, 1, 1}},
        {"pink", {1, 0.4f, 0.8f, 1}}, {"cyan", {0.2f, 1, 1, 1}}, {"gold", {1, 0.8f, 0.2f, 1}}, {"grey", {0.5f, 0.5f, 0.5f, 1}},
        {"gray", {0.5f, 0.5f, 0.5f, 1}}, {"silver", {0.8f, 0.8f, 0.85f, 1}}, {"brown", {0.45f, 0.3f, 0.15f, 1}},
    };
    return c;
}

bool isHex(char c) { return std::isxdigit((unsigned char)c) != 0; }

bool hexColorAt(const std::string& s, size_t i) {
    if (s[i] != '#') return false;
    size_t n = 0;
    while (i + 1 + n < s.size() && isHex(s[i + 1 + n])) n++;
    if (n != 6 && n != 8) return false;
    size_t after = i + 1 + n;
    return after >= s.size() || !std::isalnum((unsigned char)s[after]);
}

/// removes "# comment" / "// comment" but keeps colours like #FF8800
std::string stripComment(std::string line) {
    for (size_t i = line.find('#'); i != std::string::npos; i = line.find('#', i + 1)) {
        if (hexColorAt(line, i)) continue;
        line = line.substr(0, i);
        break;
    }
    auto sl = line.find("//");
    if (sl != std::string::npos) line = line.substr(0, sl);
    return line;
}

float num(const std::string& v) {
    std::string t = v;
    std::replace(t.begin(), t.end(), ',', '.');
    auto n = numbers(t);
    if (n.empty()) throw BadValue("expected a number but got \"" + v + "\"");
    return n[0];
}

float percent(const std::string& v) {
    float n = num(v);
    if (contains(v, "%") || n > 1.f) n /= 100.f;
    return n;
}

float clamp01(float f) { return f < 0 ? 0 : f > 1 ? 1 : f; }
bool hasDigit(const std::string& v) { return std::any_of(v.begin(), v.end(), [](char c) { return std::isdigit((unsigned char)c); }); }

bool isInfinite(const std::string& v) {
    auto n = normalizeKey(v);
    return n == "infinite" || n == "infinity" || n == "unlimited" || n == "inf" || n == "none" || n == "endless" || n == "never";
}

bool boolValue(const std::string& v) {
    bool b;
    if (!parseBool(v, b)) throw BadValue("expected yes/no but got \"" + v + "\"");
    return b;
}

/// a yes/no word, but only when the value has no digits ("1" is a count, not "yes")
bool wordBool(const std::string& v, bool& out) { return !hasDigit(v) && parseBool(v, out); }

WeaponKind parseKind(const std::string& v) {
    auto n = normalizeKey(v);
    for (auto w : {"range", "gun", "bow", "shoot", "projectile", "pistol", "rifle", "launcher", "firearm"})
        if (contains(n, w)) return WeaponKind::Ranged;
    for (auto w : {"melee", "blade", "blunt", "sword", "club", "hand"})
        if (contains(n, w)) return WeaponKind::Melee;
    throw BadValue("expected Melee or Ranged but got \"" + v + "\"");
}

void applyFireType(WeaponDefinition& d, const std::string& v) {
    auto n = normalizeKey(v);
    bool instant = contains(n, "hitscan") || contains(n, "laser") || contains(n, "instant");
    if (instant) { d.hitscan = true; d.projectileSpeed = 0; }
    if (contains(n, "beam")) { d.hitscan = true; d.projectileSpeed = 0; d.fireMode = FireMode::Automatic; return; }
    if (contains(n, "shotgun") || contains(n, "scatter")) {
        d.fireMode = FireMode::Single;
        if (d.pellets <= 1) d.pellets = 8;
        if (d.spread <= 1) d.spread = 7;
        return;
    }
    if (contains(n, "burst")) { d.fireMode = FireMode::Burst; return; }
    if (contains(n, "auto") && !contains(n, "semi")) { d.fireMode = FireMode::Automatic; return; }
    for (auto w : {"semi", "single", "bolt", "pump", "manual"})
        if (contains(n, w)) { d.fireMode = FireMode::Single; return; }
    if (instant) { d.fireMode = FireMode::Single; return; }
    throw BadValue("expected Single, Burst, Automatic, Shotgun or Hitscan but got \"" + v + "\"");
}

float parseDrop(const std::string& v) {
    bool b;
    if (wordBool(v, b)) return b ? 1.f : 0.f;
    auto n = normalizeKey(v);
    if (n == "low") return 0.3f;
    if (n == "medium") return 1.f;
    if (n == "high") return 2.f;
    return std::max(0.f, num(v));
}

float parseWeight(const std::string& v) {
    auto n = normalizeKey(v);
    if (n == "common") return 1;
    if (n == "uncommon") return 0.6f;
    if (n == "rare") return 0.3f;
    if (n == "epic") return 0.15f;
    if (n == "legendary") return 0.05f;
    return num(v);
}

Vec3 vec3(const std::string& v) {
    auto n = numbers(v);
    if (n.size() == 1) return Vec3(n[0]);
    if (n.size() < 3) throw BadValue("expected three numbers like 0, 90, 0 but got \"" + v + "\"");
    return {n[0], n[1], n[2]};
}

Vec4 colorValue(const std::string& v) {
    bool ok;
    Vec4 c = parseColor(v, ok);
    if (!ok) throw BadValue("expected a colour like #FF8800, red, or 1,0.5,0 but got \"" + v + "\"");
    return c;
}

void applyVisual(WeaponDefinition& d, const std::string& v) {
    auto t = trimQuotes(v);
    if (endsWithI(t, ".fbx")) { d.projectileVisual = ProjectileVisual::Model; d.projectileModelPath = t; return; }
    auto n = normalizeKey(v);
    auto has = [&](std::initializer_list<const char*> ws) { for (auto w : ws) if (contains(n, w)) return true; return false; };
    if (has({"none", "invisible"})) d.projectileVisual = ProjectileVisual::None;
    else if (has({"sphere", "ball", "orb", "plasma", "grenade"})) d.projectileVisual = ProjectileVisual::Sphere;
    else if (has({"cube", "box", "block"})) d.projectileVisual = ProjectileVisual::Cube;
    else if (has({"arrow", "bolt", "spear", "dart"})) d.projectileVisual = ProjectileVisual::Arrow;
    else if (has({"model", "weapon", "self"})) d.projectileVisual = ProjectileVisual::Model;
    else if (has({"bullet", "tracer", "pellet", "shell"})) d.projectileVisual = ProjectileVisual::Bullet;
    else if (has({"laser", "beam"})) { d.projectileVisual = ProjectileVisual::Bullet; d.projectileColor = {1, 0.1f, 0.1f, 1}; }
    else throw BadValue("expected Bullet, Sphere, Cube, Arrow, Model, None or a .fbx file but got \"" + v + "\"");
    std::string word;
    for (char c : v + " ") {
        if (c == ' ' || c == ',') {
            auto it = namedColors().find(normalizeKey(word));
            if (it != namedColors().end()) d.projectileColor = it->second;
            word.clear();
        } else word += c;
    }
}

void apply(WeaponDefinition& d, const std::string& key, const std::string& v) {
    bool b;
    if (key == "name") d.name = trimQuotes(v);
    else if (key == "description") d.description = v;
    else if (key == "kind") d.kind = parseKind(v);
    else if (key == "magazine") d.magazineSize = isInfinite(v) ? 0 : std::max(0, (int)num(v));
    else if (key == "reserve") d.reserveAmmo = isInfinite(v) ? -1 : std::max(0, (int)num(v));
    else if (key == "reload") d.reloadTime = std::max(0.f, num(v));
    else if (key == "firetype") applyFireType(d, v);
    else if (key == "speed") {
        auto n = normalizeKey(v);
        if (n == "hitscan" || n == "instant" || n == "infinite" || n == "laser" || n == "instanthit") { d.hitscan = true; d.projectileSpeed = 0; }
        else { d.projectileSpeed = std::max(0.f, num(v)); d.hitscan = d.projectileSpeed <= 0; }
    }
    else if (key == "drop") d.projectileDrop = parseDrop(v);
    else if (key == "firerate") d.fireRate = std::max(0.01f, num(v));
    else if (key == "bounce") d.projectileBounces = wordBool(v, b) ? (b ? 3 : 0) : std::max(0, (int)num(v));
    else if (key == "bounciness") d.bounciness = clamp01(percent(v));
    else if (key == "visual") applyVisual(d, v);
    else if (key == "projmodel") { d.projectileModelPath = trimQuotes(v); d.projectileVisual = ProjectileVisual::Model; }
    else if (key == "projsize") d.projectileSize = std::max(0.005f, num(v));
    else if (key == "projcolor") d.projectileColor = colorValue(v);
    else if (key == "projlife") d.projectileLifetime = std::max(0.05f, num(v));
    else if (key == "pierce") d.pierce = std::max(0, (int)num(v));
    else if (key == "pellets") d.pellets = std::max(1, (int)num(v));
    else if (key == "spread") d.spread = std::max(0.f, num(v));
    else if (key == "burst") d.burstCount = std::max(1, (int)num(v));
    else if (key == "hitscan") d.hitscan = boolValue(v);
    else if (key == "range") d.range = std::max(1.f, num(v));
    else if (key == "recoil") d.recoil = num(v);
    else if (key == "damage") d.damage = num(v);
    else if (key == "damagetype") { d.damageType = trim(v); if (contains(normalizeKey(v), "explos") && d.areaOfEffect <= 0) d.areaOfEffect = 3; }
    else if (key == "aoe") d.areaOfEffect = wordBool(v, b) ? (b ? 3.f : 0.f) : std::max(0.f, num(v));
    else if (key == "falloff") d.areaFalloff = boolValue(v);
    else if (key == "force") d.damageForce = num(v);
    else if (key == "durability") { if (isInfinite(v)) { d.unbreakable = true; d.durability = 0; } else d.durability = std::max(0.f, num(v)); }
    else if (key == "unbreakable") d.unbreakable = boolValue(v);
    else if (key == "attackspeed") d.attackSpeed = std::max(0.f, num(v));
    else if (key == "throwdamage") d.throwDamage = std::max(0.f, num(v));
    else if (key == "start") d.appearsAtStartingArea = boolValue(v);
    else if (key == "ground") d.appearsOnGround = boolValue(v);
    else if (key == "groundchance") d.groundChance = clamp01(percent(v));
    else if (key == "shop") d.appearsInShop = boolValue(v);
    else if (key == "shopchance") d.shopChance = clamp01(percent(v));
    else if (key == "price") d.shopPrice = std::max(0, (int)num(v));
    else if (key == "enemies") d.usedByEnemies = boolValue(v);
    else if (key == "enemychance") d.enemyChance = clamp01(percent(v));
    else if (key == "weight") d.spawnWeight = std::max(0.f, parseWeight(v));
    else if (key == "enemyinterval") d.enemyFireInterval = std::max(0.05f, num(v));
    else if (key == "enemyspread") d.enemyAccuracy = std::max(0.f, num(v));
    else if (key == "enemydmg") d.enemyDamageMultiplier = std::max(0.f, contains(v, "%") ? num(v) / 100.f : num(v));
    else if (key == "scale") d.modelScale = std::max(0.001f, num(v));
    else if (key == "rotation") d.modelRotation = vec3(v);
    else if (key == "offset") d.modelOffset = vec3(v);
    else if (key == "autofit") d.autoFit = boolValue(v);
    else if (key == "texture") d.texturePath = trimQuotes(v);
    else if (key == "tint") d.tint = colorValue(v);
    else if (key == "base") d.baseWeapon = trimQuotes(v);
}

std::string readText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

bool parseBool(const std::string& v, bool& out) {
    auto n = normalizeKey(v);
    for (auto w : {"yes", "y", "true", "t", "1", "on", "enabled", "enable", "always"}) if (n == w) { out = true; return true; }
    for (auto w : {"no", "n", "false", "f", "0", "off", "disabled", "disable", "never", "none"}) if (n == w) { out = false; return true; }
    return false;
}

Vec4 parseColor(const std::string& v, bool& ok) {
    ok = true;
    auto t = trimQuotes(v);
    if (!t.empty() && t[0] == '#' && (t.size() == 7 || t.size() == 9) && hexColorAt(t, 0)) {
        auto h = [&](int i) { return std::stoi(t.substr(i, 2), nullptr, 16) / 255.f; };
        return {h(1), h(3), h(5), t.size() == 9 ? h(7) : 1.f};
    }
    auto it = namedColors().find(normalizeKey(t));
    if (it != namedColors().end()) return it->second;
    auto n = numbers(t);
    if (n.size() >= 3) {
        bool bytes = n[0] > 1 || n[1] > 1 || n[2] > 1;
        float s = bytes ? 1.f / 255.f : 1.f;
        float a = n.size() > 3 ? n[3] * (n[3] > 1 ? 1.f / 255.f : 1.f) : 1.f;
        return {n[0] * s, n[1] * s, n[2] * s, a};
    }
    ok = false;
    return {1, 1, 1, 1};
}

WeaponDefinition parseWeapon(const std::string& text, const std::string& id) {
    WeaponDefinition d;
    d.id = id;
    d.name = id;
    struct Pair { std::string key, value; int line; };
    std::vector<Pair> pairs;
    std::vector<Pair> bare;
    auto lines = split(text, '\n');
    for (size_t i = 0; i < lines.size(); i++) {
        auto line = trim(stripComment(lines[i]));
        if (line.empty()) continue;
        if (line[0] == '+') line = trim(line.substr(1));
        auto colon = line.find(':'), eq = line.find('=');
        size_t sep = std::min(colon, eq);
        if (sep != std::string::npos && sep > 0) pairs.push_back({trim(line.substr(0, sep)), trim(line.substr(sep + 1)), (int)i + 1});
        else bare.push_back({"", line, (int)i + 1});
    }
    if (pairs.empty() && !bare.empty()) {
        for (size_t i = 0; i < bare.size() && i < kPositional.size(); i++) pairs.push_back({kPositional[i], bare[i].value, bare[i].line});
        if (bare.size() > kPositional.size()) d.warnings.push_back(std::to_string(bare.size() - kPositional.size()) + " extra unlabeled line(s) ignored.");
    } else {
        for (auto& b : bare) d.warnings.push_back("Line " + std::to_string(b.line) + ": no ':' or '=' found, ignored: \"" + b.value + "\"");
    }
    auto canon = [](const std::string& k) -> std::string {
        auto it = aliases().find(normalizeKey(k));
        return it == aliases().end() ? "" : it->second;
    };
    // fire type first so explicit Pellets / Spread lines win over its presets
    std::stable_sort(pairs.begin(), pairs.end(), [&](const Pair& a, const Pair& b) { return (canon(a.key) == "firetype") > (canon(b.key) == "firetype"); });
    for (auto& p : pairs) {
        auto c = canon(p.key);
        if (c.empty()) { d.warnings.push_back("Line " + std::to_string(p.line) + ": unknown setting \"" + p.key + "\" ignored."); continue; }
        try { apply(d, c, p.value); }
        catch (const BadValue& e) { d.warnings.push_back("Line " + std::to_string(p.line) + ": " + p.key + ": " + e.what()); }
    }
    if (d.name.empty()) d.name = id;
    if (d.fireRate <= 0) d.fireRate = 1;
    if (d.pellets < 1) d.pellets = 1;
    if (d.burstCount < 1) d.burstCount = 1;
    if (d.projectileSpeed <= 0 && d.isRanged()) d.hitscan = true;
    return d;
}

WeaponDefinition parseWeaponFile(const std::string& txtPath) {
    fs::path p(txtPath);
    auto d = parseWeapon(readText(txtPath), p.stem().string());
    auto dir = p.parent_path();
    auto find = [&](std::initializer_list<const char*> exts) -> std::string {
        for (auto e : exts) { auto c = dir / (d.id + e); if (fs::exists(c)) return c.string(); }
        return "";
    };
    d.modelPath = find({".fbx", ".FBX", ".Fbx"});
    if (d.modelPath.empty()) d.modelPath = (dir / (d.id + ".fbx")).string();
    if (!d.texturePath.empty()) {
        fs::path t = fs::path(d.texturePath).is_absolute() ? fs::path(d.texturePath) : dir / d.texturePath;
        if (fs::exists(t)) d.texturePath = t.string();
        else { d.warnings.push_back("Texture \"" + d.texturePath + "\" not found."); d.texturePath.clear(); }
    } else d.texturePath = find({".png", ".PNG", ".jpg", ".JPG", ".jpeg"});
    if (!d.projectileModelPath.empty()) {
        fs::path t = fs::path(d.projectileModelPath).is_absolute() ? fs::path(d.projectileModelPath) : dir / d.projectileModelPath;
        if (fs::exists(t)) d.projectileModelPath = t.string();
        else { d.warnings.push_back("Projectile model \"" + d.projectileModelPath + "\" not found, using the weapon model."); d.projectileModelPath.clear(); }
    }
    return d;
}

std::vector<LoadedWeapon> loadWeaponFolder(const std::string& folder, std::vector<std::string>& log) {
    std::vector<LoadedWeapon> out;
    std::error_code ec;
    if (!fs::is_directory(folder, ec)) return out;
    std::vector<fs::path> txts;
    for (auto& e : fs::recursive_directory_iterator(folder, ec)) {
        if (!e.is_regular_file()) continue;
        auto p = e.path();
        if (lower(p.extension().string()) != ".txt") continue;
        auto fn = p.filename().string();
        if (fn[0] == '_' || iequals(fn, "README.txt") || startsWith(lower(fn), "template")) continue;
        txts.push_back(p);
    }
    std::sort(txts.begin(), txts.end(), [](const fs::path& a, const fs::path& b) { return lower(a.string()) < lower(b.string()); });
    std::vector<std::string> ids;
    for (auto& txt : txts) {
        auto id = txt.stem().string();
        try {
            LoadedWeapon lw;
            lw.def = parseWeaponFile(txt.string());
            if (!fs::exists(lw.def.modelPath)) { log.push_back("WARN [" + id + "] skipped: no model file \"" + id + ".fbx\" next to it."); continue; }
            if (std::find_if(ids.begin(), ids.end(), [&](const std::string& x) { return iequals(x, id); }) != ids.end()) {
                log.push_back("WARN [" + id + "] skipped: another weapon already uses this file name."); continue;
            }
            for (auto& w : lw.def.warnings) log.push_back("WARN [" + id + "] " + w);
            lw.mesh = fbxLoadMesh(lw.def.modelPath);
            for (auto& w : lw.mesh.warnings) log.push_back("WARN [" + id + "] " + w);
            if (lw.mesh.triangleCount() == 0) { log.push_back("WARN [" + id + "] skipped: the FBX contains no triangles."); continue; }
            if (!lw.def.texturePath.empty()) lw.texture = readFileBytes(lw.def.texturePath);
            else lw.texture = lw.mesh.embeddedTexture;
            if (!lw.def.projectileModelPath.empty()) {
                try { lw.projectileMesh = fbxLoadMesh(lw.def.projectileModelPath); lw.hasProjectileMesh = true; }
                catch (const std::exception& e) { log.push_back("WARN [" + id + "] projectile model failed to load: " + e.what()); }
            }
            log.push_back("INFO [" + id + "] loaded \"" + lw.def.name + "\" (" + (lw.def.isRanged() ? "Ranged" : "Melee") + ", " +
                          std::to_string(lw.mesh.triangleCount()) + " triangles" + (lw.texture.empty() ? "" : ", textured") + ")");
            ids.push_back(id);
            out.push_back(std::move(lw));
        } catch (const std::exception& e) {
            log.push_back("WARN [" + id + "] failed to load: " + e.what());
        }
    }
    return out;
}

} // namespace cw
