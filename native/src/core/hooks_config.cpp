#include <filesystem>
#include <fstream>
#include <sstream>
#include "strutil.hpp"
#include "weapon.hpp"

namespace cw {

const char* HooksConfig::defaultText() {
    static const char* text =
#include "hooks_default.inc"
        ;
    return text;
}

HooksConfig HooksConfig::parse(const std::string& text) {
    HooksConfig c;
    for (auto& rawLine : split(text, '\n')) {
        auto line = trim(rawLine);
        if (line.empty() || line[0] == '#' || startsWith(line, "//")) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        auto key = lower(trim(line.substr(0, eq)));
        auto value = trim(line.substr(eq + 1));
        c.raw_[key] = value;
        std::vector<std::string> items;
        for (auto& s : split(value, ',')) { auto t = trim(s); if (!t.empty()) items.push_back(t); }
        c.lists_[key] = items;
    }
    return c;
}

HooksConfig HooksConfig::loadOrCreate(const std::string& folder) {
    auto path = std::filesystem::path(folder) / "_GameHooks.cfg";
    auto cfg = parse(defaultText());
    if (!std::filesystem::exists(path)) {
        std::ofstream(path, std::ios::binary) << defaultText();
        return cfg;
    }
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    auto user = parse(ss.str());
    for (auto& kv : user.lists_) cfg.lists_[kv.first] = kv.second;
    for (auto& kv : user.raw_) cfg.raw_[kv.first] = kv.second;
    return cfg;
}

const std::vector<std::string>& HooksConfig::list(const std::string& key) const {
    static const std::vector<std::string> empty;
    auto it = lists_.find(lower(key));
    return it == lists_.end() ? empty : it->second;
}

std::string HooksConfig::raw(const std::string& key, const std::string& def) const {
    auto it = raw_.find(lower(key));
    return it == raw_.end() ? def : it->second;
}

float HooksConfig::num(const std::string& key, float def) const {
    auto n = numbers(raw(key));
    return n.empty() ? def : n[0];
}

bool HooksConfig::flag(const std::string& key, bool def) const {
    bool b;
    return parseBool(raw(key), b) ? b : def;
}

} // namespace cw
