// Entry point. Loaded by Windows as version.dll next to PaintTheTownRed.exe.
// A background thread reads the weapon files, waits for the game's IL2CPP runtime, then hooks one
// per-frame game method (PTTRPlayer.Update by default). All game work happens inside that hook,
// on Unity's main thread. There is no decompiling or code generation step.
#include <windows.h>
#include <filesystem>
#include <memory>
#include "../../third_party/minhook/include/MinHook.h"
#include "../core/strutil.hpp"
#include "game.hpp"
#include "log.hpp"

namespace fs = std::filesystem;
using namespace cw;

extern "C" __declspec(dllexport) const char* PTTRCustomWeaponsVersion() { return "PTTR_CUSTOM_WEAPONS_NATIVE 2.0.0"; }

namespace {

std::string g_folder;
std::unique_ptr<HooksConfig> g_cfg;
std::vector<LoadedWeapon> g_weapons;
std::unique_ptr<Bridge> g_bridge;
std::unique_ptr<Skinner> g_skinner;
std::unique_ptr<Damage> g_damage;
std::unique_ptr<Projectiles> g_projectiles;
std::unique_ptr<Combat> g_combat;
std::unique_ptr<Director> g_director;

using TickFn = void (*)(void*, const MethodInfo*);
TickFn g_original = nullptr;
volatile bool g_hooked = false;
bool g_started = false, g_failed = false, g_reportWritten = false;
int g_lastFrame = -1;
int g_spawnKey = 0, g_reloadKey = 0, g_reportKey = 0;
ULONGLONG g_lastTickMs = 0;
std::string g_scene;
int g_playerId = 0;
float g_nextSceneCheck = 0;

void loadWeapons() {
    std::vector<std::string> log;
    g_weapons = loadWeaponFolder(g_folder, log);
    for (auto& l : log) (startsWith(l, "WARN") ? logWarn : logInfo)(l.substr(5));
    logInfo(std::to_string(g_weapons.size()) + " custom weapon(s) loaded from " + g_folder);
}

/// F9: re-read every .txt / .fbx and refresh custom weapons that already exist.
void reloadPacks() {
    std::vector<std::pair<Ref, std::string>> existing;
    for (auto* ci : g_skinner->all()) existing.emplace_back(Ref(ci->component.get()), ci->def().id);
    g_projectiles->clear();
    g_combat->clear();
    g_skinner->clearAll(); // existing pointers into g_weapons die here, before it is replaced
    loadWeapons();
    for (auto& [w, id] : existing)
        for (auto& lw : g_weapons)
            if (iequals(lw.def.id, id) && u::alive(w.get())) g_skinner->apply(w.get(), lw);
    logInfo("Reloaded weapon files.");
}

void tick() {
    if (!g_started) {
        g_started = true;
        if (!u::init()) { g_failed = true; logError("Unity classes missing - the mod is disabled."); return; }
        g_bridge = std::make_unique<Bridge>(*g_cfg);
        g_bridge->resolve();
        g_skinner = std::make_unique<Skinner>(*g_bridge);
        g_damage = std::make_unique<Damage>(*g_bridge);
        g_projectiles = std::make_unique<Projectiles>(*g_bridge, *g_damage, *g_skinner);
        g_combat = std::make_unique<Combat>(*g_bridge, *g_skinner, *g_projectiles);
        g_director = std::make_unique<Director>(*g_bridge, *g_skinner, *g_combat, g_weapons);
        logInfo("Running.");
    }
    if (g_failed) return;
    keys::poll();

    // new level? (scene changed, or a new player object after a menu / loading screen)
    ULONGLONG nowMs = GetTickCount64();
    bool longGap = g_lastTickMs == 0 || nowMs - g_lastTickMs > (ULONGLONG)(g_cfg->num("MenuGapSeconds", 8) * 1000);
    g_lastTickMs = nowMs;
    float now = u::time();
    if (now >= g_nextSceneCheck || longGap) {
        g_nextSceneCheck = now + 0.5f;
        Obj player = g_bridge->localPlayer();
        int pid = player ? u::instanceId(player) : 0;
        std::string scene = u::activeSceneName();
        if (player && (scene != g_scene || pid != g_playerId)) {
            g_scene = scene;
            g_playerId = pid;
            g_director->onLevel(scene, longGap);
            if (!g_reportWritten) { g_reportWritten = true; writeReport(g_folder, *g_bridge, *g_skinner, scene); }
        }
    }

    if (keys::gameHasFocus()) {
        if (keys::pressed(g_spawnKey)) g_director->spawnAllInFront();
        if (keys::pressed(g_reloadKey)) reloadPacks();
        if (keys::pressed(g_reportKey)) writeReport(g_folder, *g_bridge, *g_skinner, g_scene);
    }
    g_director->update();
    g_combat->update();
    g_projectiles->update();
}

void detour(void* self, const MethodInfo* mi) {
    g_original(self, mi);
    int frame = u::Object ? u::frameCount() : -2; // once per frame even if several objects run this method
    if (frame == g_lastFrame && frame != -2) return;
    g_lastFrame = frame;
    try { tick(); }
    catch (const std::exception& e) { logOnce(std::string("tick") + e.what(), std::string("Error: ") + e.what()); }
    catch (...) { logOnce("tick...", "Unknown error in the per-frame update."); }
    il2::endFrame();
}

bool waitFor(const char* what, DWORD timeoutMs, bool (*ready)()) {
    for (DWORD waited = 0; waited < timeoutMs; waited += 100) {
        if (ready()) return true;
        Sleep(100);
    }
    logError(std::string("Timed out waiting for ") + what);
    return false;
}

DWORD WINAPI startup(LPVOID) {
    try {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        fs::path gameDir = fs::path(exe).parent_path();
        g_folder = (gameDir / "CustomWeapons").string();
        fs::create_directories(g_folder);
        logInit(g_folder);
        g_cfg = std::make_unique<HooksConfig>(HooksConfig::loadOrCreate(g_folder));
        g_spawnKey = keys::code(g_cfg->raw("SpawnAllKey", "F8"));
        g_reloadKey = keys::code(g_cfg->raw("ReloadPacksKey", "F9"));
        g_reportKey = keys::code(g_cfg->raw("ReportKey", "F10"));
        loadWeapons();

        if (!waitFor("GameAssembly.dll", 600000, [] { return GetModuleHandleA("GameAssembly.dll") != nullptr; })) return 0;
        auto missing = il2::load(GetModuleHandleA("GameAssembly.dll"));
        for (auto& m : missing) logWarn("IL2CPP function not exported: " + m);
        if (!il2::il2cpp_runtime_invoke || !il2::il2cpp_class_from_name || !il2::il2cpp_get_corlib || !il2::il2cpp_domain_get_assemblies) {
            logError("Essential IL2CPP functions are missing - the mod is disabled.");
            return 0;
        }
        if (!waitFor("the game to start", 600000, [] {
                if (!il2::il2cpp_get_corlib()) return false;
                size_t n = 0;
                auto asms = il2::il2cpp_domain_get_assemblies(il2::il2cpp_domain_get(), &n);
                for (size_t i = 0; i < n; i++)
                    if (startsWith(il2::il2cpp_image_get_name(il2::il2cpp_assembly_get_image(asms[i])), "Assembly-CSharp")) return true;
                return false;
            })) return 0;
        Sleep(1500);

        auto thread = il2::il2cpp_thread_attach(il2::il2cpp_domain_get());
        void* target = nullptr;
        std::string used;
        for (auto& spec : g_cfg->list("TickMethods")) {
            auto dot = spec.rfind('.');
            if (dot == std::string::npos) continue;
            auto k = il2::gameClass(spec.substr(0, dot));
            auto m = k ? il2::method(k, spec.substr(dot + 1).c_str(), 0) : nullptr;
            if (m && *reinterpret_cast<void* const*>(m)) { target = *reinterpret_cast<void* const*>(m); used = spec; break; }
        }
        il2::il2cpp_thread_detach(thread);
        if (!target) { logError("None of the TickMethods in _GameHooks.cfg exist - the mod is disabled."); return 0; }

        if (MH_Initialize() != MH_OK || MH_CreateHook(target, reinterpret_cast<void*>(&detour), reinterpret_cast<void**>(&g_original)) != MH_OK ||
            MH_EnableHook(target) != MH_OK) {
            logError("Could not hook " + used + " - the mod is disabled.");
            return 0;
        }
        g_hooked = true;
        logInfo("Hooked " + used + "; waiting for a level.");
    } catch (const std::exception& e) {
        logError(std::string("Startup failed: ") + e.what());
    }
    return 0;
}

bool isGameProcess() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    fs::path p(exe);
    auto name = lower(p.filename().string());
    if (contains(name, "crashhandler")) return false;
    std::error_code ec;
    return fs::exists(p.parent_path() / "GameAssembly.dll", ec);
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        if (isGameProcess()) {
            if (HANDLE h = CreateThread(nullptr, 0, startup, nullptr, 0, nullptr)) CloseHandle(h);
        }
    }
    return TRUE;
}
