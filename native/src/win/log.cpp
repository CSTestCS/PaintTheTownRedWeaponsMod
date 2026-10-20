#include "log.hpp"
#include <windows.h>
#include <cstdio>
#include <mutex>
#include <set>

namespace cw {
namespace {
std::mutex g_mu;
FILE* g_file = nullptr;
std::set<std::string> g_once;

void write(const char* level, const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_file) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::fprintf(g_file, "[%02d:%02d:%02d] %s %s\n", t.wHour, t.wMinute, t.wSecond, level, msg.c_str());
    std::fflush(g_file);
}
}

void logInit(const std::string& folder) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_file) return;
    g_file = std::fopen((folder + "\\_CustomWeapons.log").c_str(), "wb");
    if (g_file) std::fprintf(g_file, "Paint the Town Red - Custom Weapons (native) log\n");
}
void logInfo(const std::string& m) { write("INFO ", m); }
void logWarn(const std::string& m) { write("WARN ", m); }
void logError(const std::string& m) { write("ERROR", m); }
void logOnce(const std::string& key, const std::string& m, bool warn) {
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_once.insert(key).second) return;
    }
    if (warn) logWarn(m); else logInfo(m);
}
}
