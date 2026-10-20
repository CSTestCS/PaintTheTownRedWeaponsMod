// String helpers shared by the parsers.
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace cw {

inline std::string lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

inline std::string trimQuotes(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"') t = t.substr(1, t.size() - 2);
    return t;
}

/// lower-case letters and digits only: "Melee or Ranged?" -> "meleeorranged"
inline std::string normalizeKey(const std::string& s) {
    std::string r;
    for (char c : s) if (std::isalnum((unsigned char)c)) r += (char)std::tolower((unsigned char)c);
    return r;
}

inline std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> r;
    size_t st = 0;
    for (size_t i = 0; i <= s.size(); i++)
        if (i == s.size() || s[i] == sep) { r.push_back(s.substr(st, i - st)); st = i + 1; }
    return r;
}

inline bool contains(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
inline bool iequals(const std::string& a, const std::string& b) { return lower(a) == lower(b); }
inline bool icontains(const std::string& h, const std::string& n) { return contains(lower(h), lower(n)); }
inline bool startsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }
inline bool endsWithI(const std::string& s, const std::string& p) { return s.size() >= p.size() && lower(s.substr(s.size() - p.size())) == lower(p); }

/// All numbers in a string ("0, 90, -1.5e2" -> {0, 90, -150}). Commas between digits count as separators.
inline std::vector<float> numbers(const std::string& s) {
    std::vector<float> r;
    size_t i = 0;
    while (i < s.size()) {
        char c = s[i];
        bool start = std::isdigit((unsigned char)c) || ((c == '-' || c == '+' || c == '.') && i + 1 < s.size() &&
                     (std::isdigit((unsigned char)s[i + 1]) || (s[i + 1] == '.' && i + 2 < s.size() && std::isdigit((unsigned char)s[i + 2]))));
        if (!start) { i++; continue; }
        size_t j = i;
        if (s[j] == '-' || s[j] == '+') j++;
        while (j < s.size() && std::isdigit((unsigned char)s[j])) j++;
        if (j < s.size() && s[j] == '.') { j++; while (j < s.size() && std::isdigit((unsigned char)s[j])) j++; }
        if (j < s.size() && (s[j] == 'e' || s[j] == 'E')) {
            size_t k = j + 1;
            if (k < s.size() && (s[k] == '-' || s[k] == '+')) k++;
            if (k < s.size() && std::isdigit((unsigned char)s[k])) { j = k; while (j < s.size() && std::isdigit((unsigned char)s[j])) j++; }
        }
        r.push_back(std::strtof(s.substr(i, j - i).c_str(), nullptr));
        i = j;
    }
    return r;
}

} // namespace cw
