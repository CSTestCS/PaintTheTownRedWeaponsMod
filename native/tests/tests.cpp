// Unit tests for the engine-independent core. Build and run on any OS: see native/Makefile ("make test").
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include "../src/core/ballistics.hpp"
#include "../src/core/fbx.hpp"
#include "../src/core/strutil.hpp"
#include "../src/core/weapon.hpp"

using namespace cw;
namespace fs = std::filesystem;

static int g_failed = 0, g_checks = 0;
static std::string g_test;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failed++; std::printf("  FAIL %s:%d [%s] %s\n", __FILE__, __LINE__, g_test.c_str(), #cond); } } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

static std::string repo;   // repository root
static std::vector<std::pair<std::string, std::function<void()>>> tests;
#define TEST(name) static void name(); static bool reg_##name = (tests.push_back({#name, name}), true); static void name()

static std::string readAll(const fs::path& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

static std::vector<Vec3> blenderExpected() {
    // tiny parser for {"expected": [[x,y,z], ...]}
    auto t = readAll(fs::path(repo) / "native/tests/fixtures/blender_expected.json");
    auto start = t.find("[[");
    auto end = t.find("]]", start);
    auto nums = numbers(t.substr(start, end - start));
    std::vector<Vec3> r;
    for (size_t i = 0; i + 2 < nums.size(); i += 3) r.push_back({nums[i], nums[i + 1], nums[i + 2]});
    return r;
}

TEST(blender_exports_match_world_positions) {
    auto expected = blenderExpected();
    CHECK(expected.size() == 22); // 8 cube + 14 cylinder vertices
    for (auto f : {"blender_default.fbx", "blender_zup.fbx", "blender_scale_all.fbx"}) {
        auto mesh = fbxLoadMesh((fs::path(repo) / "native/tests/fixtures" / f).string());
        CHECK(mesh.triangleCount() == 36);
        CHECK(mesh.hasUVs);
        for (auto& e : expected) {
            float best = 1e9f;
            for (auto& p : mesh.positions) best = std::min(best, distance(p, e));
            CHECK(best < 1e-3f);
        }
        for (auto& p : mesh.positions) {
            float best = 1e9f;
            for (auto& e : expected) best = std::min(best, distance(p, e));
            CHECK(best < 1e-3f);
        }
        for (size_t t = 0; t < mesh.indices.size(); t += 3) {
            auto a = mesh.positions[mesh.indices[t]], b = mesh.positions[mesh.indices[t + 1]], c = mesh.positions[mesh.indices[t + 2]];
            CHECK(dot(cross(b - a, c - a), mesh.normals[mesh.indices[t]]) > 0);
        }
    }
}

TEST(ascii_quad_z_up) {
    const char* ascii = R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {
	FBXVersion: 7400
}
GlobalSettings:  {
	Properties70:  {
		P: "UpAxis", "int", "Integer", "",2
		P: "UpAxisSign", "int", "Integer", "",1
		P: "FrontAxis", "int", "Integer", "",1
		P: "FrontAxisSign", "int", "Integer", "",-1
		P: "CoordAxis", "int", "Integer", "",0
		P: "CoordAxisSign", "int", "Integer", "",1
		P: "UnitScaleFactor", "double", "Number", "",100
	}
}
Objects:  {
	Geometry: 140, "Geometry::Plane", "Mesh" {
		Vertices: *12 {
			a: 0,0,0,1,0,0,
			1,0,3,0,0,3
		}
		PolygonVertexIndex: *4 {
			a: 0,1,2,-4
		}
		LayerElementUV: 0 {
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "IndexToDirect"
			UV: *8 {
				a: 0,0,1,0,1,1,0,1
			}
			UVIndex: *4 {
				a: 0,1,2,3
			}
		}
	}
	Model: 150, "Model::Plane", "Mesh" {
		Properties70:  {
			P: "Lcl Translation", "Lcl Translation", "", "A",0,0,1
		}
	}
}
Connections:  {
	C: "OO",140,150
	C: "OO",150,0
})";
    std::vector<uint8_t> bytes(ascii, ascii + std::strlen(ascii));
    auto mesh = fbxExtractMesh(fbxParse(bytes));
    CHECK(mesh.triangleCount() == 2);
    CHECK(!mesh.hasNormals);
    Vec3 mn, mx;
    mesh.bounds(mn, mx);
    NEAR(mn.x, -1, 1e-4); NEAR(mn.y, 1, 1e-4); NEAR(mx.x, 0, 1e-4); NEAR(mx.y, 4, 1e-4);
}

TEST(example_weapons_load) {
    std::vector<std::string> log;
    auto ws = loadWeaponFolder((fs::path(repo) / "CustomWeapons").string(), log);
    for (auto& l : log) if (startsWith(l, "WARN")) std::printf("  log: %s\n", l.c_str());
    CHECK(ws.size() == 3);
    for (auto& w : ws) {
        CHECK(w.def.warnings.empty());
        CHECK(!w.texture.empty());
        if (w.def.id == "ExampleRocketLauncher") { CHECK(w.hasProjectileMesh); CHECK(w.def.projectileVisual == ProjectileVisual::Model); }
        if (w.def.id == "ExampleGreatsword") CHECK(w.def.kind == WeaponKind::Melee);
        if (w.def.id == "ExampleBlaster") { CHECK(w.def.fireMode == FireMode::Automatic); CHECK(w.def.appearsAtStartingArea); }
    }
}

TEST(template_parses_cleanly) {
    auto d = parseWeapon(readAll(fs::path(repo) / "CustomWeapons/TEMPLATE.txt"), "T");
    for (auto& w : d.warnings) std::printf("  warn: %s\n", w.c_str());
    CHECK(d.warnings.empty());
    CHECK(d.name == "My Weapon");
    CHECK(d.reserveAmmo == -1);
    NEAR(d.projectileColor.y, 0xD9 / 255.0, 1e-4);
}

TEST(labeled_file_all_requested_fields) {
    auto d = parseWeapon("# rocket\nName: Boom Stick\nMelee or Ranged: Ranged\nMagazine size: 4\nFire type: Semi-automatic\n"
                         "Projectile speed: 35\nProjectile drop: 0.5\nAutomatic fire rate: 2\nProjectile bounce: 1\n"
                         "Projectile visual: Sphere\nDamage: 120\nDamage type: Explosive\nArea of effect: 4.5\nDamage force: 80\n"
                         "Appears at starting area?: no\n", "B");
    CHECK(d.warnings.empty());
    CHECK(d.name == "Boom Stick");
    CHECK(d.kind == WeaponKind::Ranged);
    CHECK(d.magazineSize == 4);
    CHECK(d.fireMode == FireMode::Single);
    NEAR(d.projectileSpeed, 35, 1e-6); NEAR(d.projectileDrop, 0.5, 1e-6); NEAR(d.fireRate, 2, 1e-6);
    CHECK(d.projectileBounces == 1);
    CHECK(d.projectileVisual == ProjectileVisual::Sphere);
    NEAR(d.damage, 120, 1e-6); NEAR(d.areaOfEffect, 4.5, 1e-6); NEAR(d.damageForce, 80, 1e-6);
    CHECK(d.damageType == "Explosive");
    CHECK(!d.appearsAtStartingArea);
    CHECK(!d.hitscan);
}

TEST(unlabeled_file_in_order) {
    auto d = parseWeapon("Plasma Rifle\nRanged\n30\nAutomatic\n80\n0\n10\n0\nred sphere\n15\nFire\n0\n5\nyes", "P");
    CHECK(d.name == "Plasma Rifle");
    CHECK(d.magazineSize == 30);
    CHECK(d.fireMode == FireMode::Automatic);
    CHECK(d.projectileVisual == ProjectileVisual::Sphere);
    NEAR(d.projectileColor.y, 0.1, 1e-6);
    CHECK(d.appearsAtStartingArea);
}

TEST(presets_hitscan_and_warnings) {
    auto s = parseWeapon("Pellets = 12\nType = Ranged\nFire Type = Shotgun", "S");
    CHECK(s.pellets == 12);
    CHECK(s.spread > 1);
    for (auto line : {"Projectile speed: hitscan", "Projectile speed: 0", "Fire type: laser"})
        CHECK(parseWeapon(std::string("Type: Ranged\n") + line, "H").hitscan);
    auto w = parseWeapon("Name: X\nFlavour: spicy\nDamage: lots\nType: Ranged", "X");
    CHECK(w.warnings.size() == 2);
    CHECK(w.kind == WeaponKind::Ranged);
    auto m = parseWeapon("Durability = infinite\nEnemy Chance = 30%\nGround Chance = 0.5\nModel Rotation = 0, 90, 0\nTint = #FF8800\nRarity = rare", "M");
    CHECK(m.warnings.empty());
    CHECK(m.unbreakable);
    NEAR(m.enemyChance, 0.3, 1e-5); NEAR(m.groundChance, 0.5, 1e-5); NEAR(m.modelRotation.y, 90, 1e-6);
    NEAR(m.tint.y, 0x88 / 255.0, 1e-4); NEAR(m.spawnWeight, 0.3, 1e-5);
}

TEST(hooks_config_defaults) {
    auto c = HooksConfig::parse(HooksConfig::defaultText());
    bool found = false;
    for (auto& s : c.list("PlayerTypes")) found |= s == "PTTRPlayer";
    CHECK(found);
    NEAR(c.num("SceneSetupDelay", 0), 2.5, 1e-6);
    CHECK(c.raw("GameplayScenes") == "");
    CHECK(contains(c.raw("MenuScenes"), "|"));
    CHECK(!c.list("TickMethods").empty());
}

TEST(ballistics_math) {
    Vec3 v{10, 0, 0}, pos;
    for (int i = 0; i < 100; i++) pos += ballistics::step(v, 1, 0.01f);
    NEAR(pos.x, 10, 1e-3); NEAR(pos.y, -0.5 * 9.81, 1e-3);
    auto b = ballistics::bounce({1, -1, 0}, {0, 1, 0}, 0.5f);
    NEAR(b.x, 0.5, 1e-6); NEAR(b.y, 0.5, 1e-6);
    std::mt19937 rng(1);
    Vec3 fwd = normalize({1, 2, 3});
    for (int i = 0; i < 500; i++) CHECK(std::acos(std::clamp(dot(ballistics::spread(fwd, 10, rng), fwd), -1.f, 1.f)) / kDeg <= 10.01f);
    for (Vec3 d : {Vec3(1, 0, 0), Vec3(0, 0, -1), normalize({1, 1, 1}), normalize({-2, -1, 0.5f})})
        CHECK(distance(ballistics::lookRotation(d).rotate({0, 0, 1}), d) < 1e-4f);
    NEAR(ballistics::areaDamage(100, 4, 4, true), 25, 1e-4);
    NEAR(ballistics::areaDamage(100, 4, 5, true), 0, 1e-6);
}

int main(int argc, char** argv) {
    repo = argc > 1 ? argv[1] : ".";
    for (auto& [name, fn] : tests) {
        g_test = name;
        int before = g_failed;
        try { fn(); } catch (const std::exception& e) { g_failed++; std::printf("  EXCEPTION in %s: %s\n", name.c_str(), e.what()); }
        std::printf("%s %s\n", g_failed == before ? "PASS" : "FAIL", name.c_str());
    }
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
