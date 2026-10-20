// _GameReport.txt: what the mod found inside the game, for fixing _GameHooks.cfg after updates.
#include <cstdio>
#include <regex>
#include <set>
#include "../core/strutil.hpp"
#include "game.hpp"
#include "log.hpp"

namespace cw {

using namespace il2;

void writeReport(const std::string& folder, Bridge& game, Skinner& skinner, const std::string& scene) {
    std::string out;
    auto line = [&](const std::string& s) { out += s + "\r\n"; };
    line("Paint the Town Red - Custom Weapons: game report");
    line("Current scene: \"" + scene + "\"");
    line("Use the class and member names below to adjust _GameHooks.cfg if something is not detected.");
    line("");
    line("== Resolved");
    line("Player class : " + (game.player ? fullName(game.player) : "NOT FOUND"));
    line("Enemy class  : " + (game.enemy ? fullName(game.enemy) : "NOT FOUND"));
    line("Weapon class : " + (game.weapon ? fullName(game.weapon) : "NOT FOUND"));
    std::string shops, parts;
    for (auto k : game.shops) shops += fullName(k) + " ";
    for (auto k : game.hitParts) parts += fullName(k) + " ";
    line("Shop classes : " + shops);
    line("Hit parts    : " + parts);
    line("In scene: players " + std::to_string(u::findObjectsOfType(game.player).size()) + ", enemies " +
         std::to_string(u::findObjectsOfType(game.enemy).size()) + ", weapons " + std::to_string(u::findObjectsOfType(game.weapon).size()));
    std::set<std::string> names;
    for (Obj w : u::findAllIncludingAssets(game.weapon)) names.insert(u::name(u::gameObject(w)));
    line("Known weapon objects (" + std::to_string(names.size()) + ") - usable as 'Base Weapon':");
    for (auto& n : names) line("    " + n);
    line("Custom weapons alive: " + std::to_string(skinner.all().size()));
    line("");
    line("== Game assemblies");
    for (auto img : gameImages()) line(std::string("  ") + il2cpp_image_get_name(img));
    line("");
    line("== Interesting game classes");
    std::regex interesting("weapon|gun|melee|blade|bow|ammo|bullet|projectile|enemy|npc|player|shop|store|vendor|merchant|price|damage|health|hit|limb|body|pickup|item|loot|spawn|beneath|floor|dungeon|level|room",
                           std::regex::icase);
    for (auto k : allGameClasses()) {
        auto cn = className(k);
        if (!std::regex_search(cn, interesting)) continue;
        auto parent = il2cpp_class_get_parent(k);
        line("");
        line("class " + fullName(k) + " : " + className(parent));
        void* it = nullptr;
        while (auto f = il2cpp_class_get_fields(k, &it)) {
            bool st = il2cpp_field_get_flags(f) & 0x10;
            line(std::string("    ") + (st ? "static " : "") + typeName(il2cpp_field_get_type(f)) + " " + il2cpp_field_get_name(f));
        }
        it = nullptr;
        while (auto m = il2cpp_class_get_methods(k, &it)) {
            std::string sig;
            for (uint32_t i = 0; i < il2cpp_method_get_param_count(m); i++)
                sig += (i ? ", " : "") + typeName(il2cpp_method_get_param(m, i)) + " " + il2cpp_method_get_param_name(m, i);
            line("    " + typeName(il2cpp_method_get_return_type(m)) + " " + il2cpp_method_get_name(m) + "(" + sig + ")");
        }
        if (il2cpp_class_is_enum(k)) for (auto& e : enumNames(k)) line("    = " + e);
    }
    auto path = folder + "\\_GameReport.txt";
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(out.data(), 1, out.size(), f);
        std::fclose(f);
        logInfo("Wrote _GameReport.txt");
    } else logWarn("Could not write _GameReport.txt");
}

} // namespace cw
