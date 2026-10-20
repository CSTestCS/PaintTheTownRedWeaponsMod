#include "il2cpp.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include "../core/strutil.hpp"
#include "log.hpp"

namespace il2 {

#define X(ret, name, args) ret(*name) args = nullptr;
IL2CPP_API_LIST(X)
#undef X

std::vector<std::string> load(void* mod) {
    std::vector<std::string> missing;
    HMODULE h = static_cast<HMODULE>(mod);
#define X(ret, name, args)                                                     \
    name = reinterpret_cast<ret(*) args>(GetProcAddress(h, #name));            \
    if (!name) missing.push_back(#name);
    IL2CPP_API_LIST(X)
#undef X
    return missing;
}

Obj (*g_cloneAsset)(Obj) = nullptr;
bool (*g_isUnityAsset)(Obj) = nullptr;

// ------------------------------------------------------------------ images and classes

namespace {
const char* kEnginePrefixes[] = {"UnityEngine", "Unity.", "mscorlib", "System", "Mono.", "netstandard", "Microsoft.",
                                 "DOTween", "Newtonsoft", "__Generated", "Il2Cpp"};

std::string imageBaseName(const Il2CppImage* img) {
    std::string n = il2cpp_image_get_name(img) ? il2cpp_image_get_name(img) : "";
    if (cw::endsWithI(n, ".dll")) n = n.substr(0, n.size() - 4);
    return n;
}
}

bool isGameImage(const Il2CppImage* img) {
    if (!img) return false;
    auto n = imageBaseName(img);
    if (cw::startsWith(n, "Assembly-CSharp")) return true;
    for (auto p : kEnginePrefixes) if (cw::startsWith(cw::lower(n), cw::lower(p))) return false;
    return true;
}

const std::vector<const Il2CppImage*>& gameImages() {
    static std::vector<const Il2CppImage*> images;
    static bool done = false;
    if (done) return images;
    size_t n = 0;
    auto asms = il2cpp_domain_get_assemblies(il2cpp_domain_get(), &n);
    for (size_t i = 0; i < n; i++) {
        auto img = il2cpp_assembly_get_image(asms[i]);
        if (isGameImage(img)) images.push_back(img);
    }
    done = !images.empty();
    return images;
}

Il2CppClass* cls(const char* assembly, const char* ns, const char* name) {
    size_t n = 0;
    auto asms = il2cpp_domain_get_assemblies(il2cpp_domain_get(), &n);
    for (size_t i = 0; i < n; i++) {
        auto img = il2cpp_assembly_get_image(asms[i]);
        if (cw::iequals(imageBaseName(img), assembly))
            if (auto k = il2cpp_class_from_name(img, ns, name)) return k;
    }
    return clsAny(ns, name);
}

Il2CppClass* clsAny(const char* ns, const char* name) {
    size_t n = 0;
    auto asms = il2cpp_domain_get_assemblies(il2cpp_domain_get(), &n);
    for (size_t i = 0; i < n; i++)
        if (auto k = il2cpp_class_from_name(il2cpp_assembly_get_image(asms[i]), ns, name)) return k;
    return nullptr;
}

std::vector<Il2CppClass*> allGameClasses() {
    static std::vector<Il2CppClass*> all;
    if (!all.empty()) return all;
    for (auto img : gameImages()) {
        size_t c = il2cpp_image_get_class_count(img);
        for (size_t i = 0; i < c; i++)
            if (auto k = const_cast<Il2CppClass*>(il2cpp_image_get_class(img, i))) all.push_back(k);
    }
    return all;
}

Il2CppClass* gameClass(const std::string& name) {
    static std::map<std::string, Il2CppClass*> cache;
    auto key = cw::lower(name);
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    Il2CppClass* best = nullptr;
    for (auto k : allGameClasses()) {
        if (cw::lower(className(k)) != key && cw::lower(fullName(k)) != key) continue;
        if (!best || (il2cpp_class_get_declaring_type(best) && !il2cpp_class_get_declaring_type(k))) best = k;
    }
    cache[key] = best;
    return best;
}

std::string className(Il2CppClass* k) { return k && il2cpp_class_get_name(k) ? il2cpp_class_get_name(k) : ""; }

std::string fullName(Il2CppClass* k) {
    if (!k) return "";
    std::string ns = il2cpp_class_get_namespace(k) ? il2cpp_class_get_namespace(k) : "";
    return ns.empty() ? className(k) : ns + "." + className(k);
}

std::string typeName(const Il2CppType* t) {
    if (!t) return "";
    char* n = il2cpp_type_get_name(t);
    std::string s = n ? n : "";
    if (n && il2cpp_free) il2cpp_free(n);
    return s;
}

Il2CppClass* classOf(Obj o) { return o ? il2cpp_object_get_class(o) : nullptr; }
bool isSubclass(Il2CppClass* k, Il2CppClass* base) { return k && base && (k == base || il2cpp_class_is_subclass_of(k, base, false)); }
bool isA(Obj o, Il2CppClass* k) { return o && k && il2cpp_class_is_assignable_from(k, classOf(o)); }

// ------------------------------------------------------------------ methods

bool isStatic(const MethodInfo* m) {
    uint32_t iflags = 0;
    return (il2cpp_method_get_flags(m, &iflags) & 0x0010) != 0;
}

const MethodInfo* method(Il2CppClass* k, const char* name, int argc, std::initializer_list<const char*> paramTypes) {
    for (auto c = k; c; c = il2cpp_class_get_parent(c)) {
        void* it = nullptr;
        while (auto m = il2cpp_class_get_methods(c, &it)) {
            if (std::strcmp(il2cpp_method_get_name(m), name) != 0) continue;
            if ((int)il2cpp_method_get_param_count(m) != argc) continue;
            if (il2cpp_method_is_generic(m)) continue;
            bool ok = true;
            uint32_t i = 0;
            for (auto pt : paramTypes) {
                if (typeName(il2cpp_method_get_param(m, i++)) != pt) { ok = false; break; }
            }
            if (ok) return m;
        }
    }
    return nullptr;
}

const MethodInfo* methodI(Il2CppClass* k, const std::string& name, int argc) {
    for (auto c = k; c; c = il2cpp_class_get_parent(c)) {
        void* it = nullptr;
        while (auto m = il2cpp_class_get_methods(c, &it))
            if (cw::iequals(il2cpp_method_get_name(m), name) && (int)il2cpp_method_get_param_count(m) == argc && !il2cpp_method_is_generic(m)) return m;
    }
    return nullptr;
}

static std::string exceptionText(Il2CppException* e) {
    std::string s = className(classOf(e));
    if (auto f = findMember(classOf(e), "_message"); f.field) {
        Il2CppString* msg = nullptr;
        il2cpp_field_get_value(e, f.field, &msg);
        if (msg) s += ": " + toUtf8(msg);
    }
    return s;
}

Obj invoke(const MethodInfo* m, void* self, void** args, bool* threw) {
    if (threw) *threw = false;
    if (!m) { if (threw) *threw = true; return nullptr; }
    Il2CppException* exc = nullptr;
    Obj r = il2cpp_runtime_invoke(m, self, args, &exc);
    if (exc) {
        if (threw) *threw = true;
        auto k = il2cpp_method_get_class(m);
        std::string where = className(k) + "." + il2cpp_method_get_name(m);
        cw::logOnce("exc" + where, "Game threw an exception in " + where + ": " + exceptionText(exc));
        return nullptr;
    }
    return r;
}

Obj newObject(Il2CppClass* k, int argc, void** args, std::initializer_list<const char*> paramTypes) {
    auto ctor = method(k, ".ctor", argc, paramTypes);
    if (!k || !ctor) return nullptr;
    Obj o = il2cpp_object_new(k);
    bool threw = false;
    invoke(ctor, o, args, &threw);
    return threw ? nullptr : o;
}

// ------------------------------------------------------------------ strings, types, arrays

Il2CppString* str(const std::string& s) { return il2cpp_string_new(s.c_str()); }

std::string toUtf8(Il2CppString* s) {
    if (!s) return "";
    int len = il2cpp_string_length(s);
    if (len <= 0) return "";
    auto chars = reinterpret_cast<const wchar_t*>(il2cpp_string_chars(s));
    int n = WideCharToMultiByte(CP_UTF8, 0, chars, len, nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, chars, len, out.data(), n, nullptr, nullptr);
    return out;
}

Obj typeObject(Il2CppClass* k) { return k ? il2cpp_type_get_object(il2cpp_class_get_type(k)) : nullptr; }

Il2CppArray* newArray(Il2CppClass* elem, size_t n) { return elem ? il2cpp_array_new(elem, n) : nullptr; }
uint32_t arrayLen(Il2CppArray* a) { return a ? il2cpp_array_length(a) : 0; }

static std::vector<uint32_t> g_frameHandles;

void keepFrame(Obj o) { if (o) g_frameHandles.push_back(il2cpp_gchandle_new(o, false)); }

void endFrame() {
    for (auto h : g_frameHandles) il2cpp_gchandle_free(h);
    g_frameHandles.clear();
}

std::vector<Obj> arrayObjects(Il2CppArray* a) {
    std::vector<Obj> r;
    uint32_t n = arrayLen(a);
    auto data = static_cast<Obj*>(arrayData(a));
    for (uint32_t i = 0; i < n; i++) if (data[i]) { keepFrame(data[i]); r.push_back(data[i]); }
    return r;
}

// ------------------------------------------------------------------ members

Kind kindOf(const Il2CppType* t) {
    if (!t) return Kind::None;
    switch (il2cpp_type_get_type(t)) {
        case T_BOOLEAN: return Kind::Bool;
        case T_CHAR: case T_I1: case T_U1: case T_I2: case T_U2: case T_I4: case T_U4: case T_I8: case T_U8: case T_I: case T_U: return Kind::Int;
        case T_R4: case T_R8: return Kind::Float;
        case T_STRING: return Kind::String;
        case T_VALUETYPE: {
            auto k = il2cpp_class_from_type(t);
            return k && il2cpp_class_is_enum(k) ? Kind::Enum : Kind::Struct;
        }
        case T_GENERICINST: {
            auto k = il2cpp_class_from_type(t);
            return k && il2cpp_class_is_valuetype(k) ? Kind::Struct : Kind::Object;
        }
        case T_CLASS: case T_OBJECT: case T_SZARRAY: case T_ARRAY: return Kind::Object;
        default: return Kind::None;
    }
}

bool fBool(const Il2CppType* t) { return kindOf(t) == Kind::Bool; }
bool fNumber(const Il2CppType* t) { auto k = kindOf(t); return k == Kind::Int || k == Kind::Float; }
bool fObject(const Il2CppType* t) { return kindOf(t) == Kind::Object; }

namespace {

const char* kWildcardExclusions[] = {"mult", "chance", "sound", "audio", "prefab", "particle", "fx", "icon", "anim", "color", "colour",
                                     "material", "layer", "mask", "type", "ptr", "bone", "path", "scene", "index", "timer", "delay", "sprite", "prefix"};

int intSize(int te) {
    switch (te) {
        case T_I1: case T_U1: case T_BOOLEAN: return 1;
        case T_I2: case T_U2: case T_CHAR: return 2;
        case T_I4: case T_U4: return 4;
        default: return 8;
    }
}

bool isSigned(int te) { return te == T_I1 || te == T_I2 || te == T_I4 || te == T_I8 || te == T_I; }

int64_t readInt(const void* p, int te) {
    switch (intSize(te)) {
        case 1: return isSigned(te) ? *(const int8_t*)p : *(const uint8_t*)p;
        case 2: return isSigned(te) ? *(const int16_t*)p : *(const uint16_t*)p;
        case 4: return isSigned(te) ? *(const int32_t*)p : *(const uint32_t*)p;
        default: return *(const int64_t*)p;
    }
}

void writeInt(std::vector<uint8_t>& raw, int64_t v, int te) {
    int n = intSize(te);
    raw.assign(8, 0);
    std::memcpy(raw.data(), &v, n); // little-endian
    raw.resize(n);
}

const Il2CppType* enumBase(const Il2CppType* t) {
    auto k = il2cpp_class_from_type(t);
    return k ? il2cpp_class_enum_basetype(k) : nullptr;
}

bool readRaw(const Il2CppType* t, const void* p, Value& out) {
    out = Value();
    out.kind = kindOf(t);
    int te = il2cpp_type_get_type(t);
    switch (out.kind) {
        case Kind::Bool: out.b = *(const uint8_t*)p != 0; return true;
        case Kind::Int: out.n = (double)readInt(p, te); return true;
        case Kind::Float: out.n = te == T_R4 ? *(const float*)p : *(const double*)p; return true;
        case Kind::Enum: { auto b = enumBase(t); out.n = (double)readInt(p, b ? il2cpp_type_get_type(b) : T_I4); return true; }
        case Kind::String: out.s = toUtf8(*(Il2CppString* const*)p); return true;
        case Kind::Object: out.o = *(Obj const*)p; return true;
        default: return false;
    }
}

bool isEngineDeclared(Il2CppClass* declaring) { return !isGameImage(il2cpp_class_get_image(declaring)); }

} // namespace

const std::vector<Member>& findMembers(Il2CppClass* k, const std::string& name) {
    static std::map<std::pair<Il2CppClass*, std::string>, std::vector<Member>> cache;
    auto key = std::make_pair(k, name);
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    std::vector<Member> exact, other;
    bool wildcard = !name.empty() && (name.front() == '*' || name.back() == '*');
    std::string needle = name;
    needle.erase(std::remove(needle.begin(), needle.end(), '*'), needle.end());
    needle = cw::lower(needle);

    auto accept = [&](const std::string& memberName, Il2CppClass* declaring) {
        auto lowerName = cw::lower(memberName);
        if (!wildcard) return lowerName == needle;
        if (isEngineDeclared(declaring) || !cw::contains(lowerName, needle)) return false;
        for (auto x : kWildcardExclusions)
            if (needle != x && cw::contains(lowerName, x) && !cw::contains(needle, x)) return false;
        return true;
    };
    auto add = [&](Member m) { (m.name == name ? exact : other).push_back(m); };

    for (auto c = k; c; c = il2cpp_class_get_parent(c)) {
        void* it = nullptr;
        while (auto f = il2cpp_class_get_fields(c, &it)) {
            if (il2cpp_field_get_flags(f) & 0x0010) continue; // static
            std::string n = il2cpp_field_get_name(f);
            if (!accept(n, c)) continue;
            Member m; m.field = f; m.name = n; m.type = il2cpp_field_get_type(f);
            add(m);
        }
        it = nullptr;
        while (auto p = il2cpp_class_get_properties(c, &it)) {
            auto pp = const_cast<PropertyInfo*>(p);
            auto g = il2cpp_property_get_get_method(pp);
            if (!g || il2cpp_method_get_param_count(g) != 0 || isStatic(g)) continue;
            std::string n = il2cpp_property_get_name(pp);
            if (!accept(n, c)) continue;
            Member m; m.getter = g; m.setter = il2cpp_property_get_set_method(pp); m.name = n; m.type = il2cpp_method_get_return_type(g);
            add(m);
        }
    }
    for (auto c = k; c; c = il2cpp_class_get_parent(c)) {
        void* it = nullptr;
        while (auto mi = il2cpp_class_get_methods(c, &it)) {
            if (il2cpp_method_get_param_count(mi) != 0 || il2cpp_method_is_generic(mi) || isStatic(mi)) continue;
            std::string n = il2cpp_method_get_name(mi);
            if (cw::startsWith(n, "get_") || cw::startsWith(n, "set_") || n == ".ctor" || n == ".cctor") continue;
            auto rt = il2cpp_method_get_return_type(mi);
            if (!rt || il2cpp_type_get_type(rt) == T_VOID) continue;
            if (!accept(n, c)) continue;
            Member m; m.getter = mi; m.name = n; m.type = rt;
            add(m);
        }
    }
    exact.insert(exact.end(), other.begin(), other.end());
    return cache[key] = exact;
}

Member findMember(Il2CppClass* k, const std::string& name, bool (*filter)(const Il2CppType*)) {
    if (!k) return {};
    for (auto& m : findMembers(k, name)) if (!filter || filter(m.type)) return m;
    return {};
}

bool getMember(Obj o, const Member& m, Value& out) {
    if (!o || !m) return false;
    if (m.field) {
        alignas(16) uint8_t buf[64] = {};
        auto kind = kindOf(m.type);
        if (kind == Kind::Struct) return false;
        il2cpp_field_get_value(o, m.field, buf);
        return readRaw(m.type, buf, out);
    }
    bool threw = false;
    Obj r = invoke(m.getter, o, nullptr, &threw);
    if (threw) return false;
    auto kind = kindOf(m.type);
    if (kind == Kind::Object || kind == Kind::String) {
        out = Value();
        out.kind = kind;
        if (kind == Kind::Object) out.o = r; else out.s = toUtf8(reinterpret_cast<Il2CppString*>(r));
        return true;
    }
    if (!r || kind == Kind::Struct) return false;
    return readRaw(m.type, il2cpp_object_unbox(r), out);
}

bool enumNamesAndValues(Il2CppClass* k, std::vector<std::pair<std::string, int64_t>>& out) {
    if (!k || !il2cpp_class_is_enum(k)) return false;
    auto base = il2cpp_class_enum_basetype(k);
    int te = base ? il2cpp_type_get_type(base) : T_I4;
    void* it = nullptr;
    while (auto f = il2cpp_class_get_fields(k, &it)) {
        int flags = il2cpp_field_get_flags(f);
        if (!(flags & 0x0010) || !(flags & 0x0040)) continue; // static literal
        alignas(8) uint8_t buf[16] = {};
        il2cpp_field_static_get_value(f, buf);
        out.push_back({il2cpp_field_get_name(f), readInt(buf, te)});
    }
    return true;
}

std::vector<std::string> enumNames(Il2CppClass* k) {
    std::vector<std::pair<std::string, int64_t>> v;
    enumNamesAndValues(k, v);
    std::vector<std::string> r;
    for (auto& p : v) r.push_back(p.first);
    return r;
}

bool enumFromName(Il2CppClass* k, const std::string& name, int64_t& out) {
    std::vector<std::pair<std::string, int64_t>> v;
    if (!enumNamesAndValues(k, v) || name.empty()) return false;
    for (auto& p : v) if (cw::iequals(p.first, name)) { out = p.second; return true; }
    for (auto& p : v) if (cw::icontains(p.first, name) || cw::icontains(name, p.first)) { out = p.second; return true; }
    return false;
}

bool convertTo(const Value& v, const Il2CppType* t, std::vector<uint8_t>& raw, Obj& objOut) {
    raw.clear();
    objOut = nullptr;
    int te = il2cpp_type_get_type(t);
    switch (kindOf(t)) {
        case Kind::Bool:
            if (v.kind != Kind::Bool) return false;
            raw = {(uint8_t)(v.b ? 1 : 0)};
            return true;
        case Kind::Int:
            if (!v.isNumber()) return false;
            writeInt(raw, (int64_t)std::llround(v.n), te);
            return true;
        case Kind::Float:
            if (!v.isNumber()) return false;
            if (te == T_R4) { float f = (float)v.n; raw.resize(4); std::memcpy(raw.data(), &f, 4); }
            else { double d = v.n; raw.resize(8); std::memcpy(raw.data(), &d, 8); }
            return true;
        case Kind::Enum: {
            auto base = enumBase(t);
            int bte = base ? il2cpp_type_get_type(base) : T_I4;
            int64_t value;
            if (v.kind == Kind::String) { if (!enumFromName(il2cpp_class_from_type(t), v.s, value)) return false; }
            else if (v.isNumber()) value = (int64_t)std::llround(v.n);
            else return false;
            writeInt(raw, value, bte);
            return true;
        }
        case Kind::String:
            if (v.kind == Kind::String) objOut = (Obj)str(v.s);
            else if (v.isNumber()) objOut = (Obj)str(std::to_string(v.n));
            else return false;
            return true;
        case Kind::Object:
            if (v.kind != Kind::Object || !v.o) return false;
            if (!il2cpp_class_is_assignable_from(il2cpp_class_from_type(t), classOf(v.o))) return false;
            objOut = v.o;
            return true;
        default:
            return false;
    }
}

bool setMember(Obj o, const Member& m, const Value& v) {
    if (!o || !m) return false;
    std::vector<uint8_t> raw;
    Obj obj = nullptr;
    if (!convertTo(v, m.type, raw, obj)) return false;
    auto kind = kindOf(m.type);
    bool byPointer = kind == Kind::String || kind == Kind::Object;
    alignas(16) uint8_t buf[16] = {};
    if (!byPointer) std::memcpy(buf, raw.data(), std::min(raw.size(), sizeof(buf)));
    if (m.field) {
        il2cpp_field_set_value(o, m.field, byPointer ? (void*)obj : (void*)buf);
        return true;
    }
    if (!m.setter) return false;
    void* args[1] = {byPointer ? (void*)obj : (void*)buf};
    bool threw = false;
    invoke(m.setter, o, args, &threw);
    return !threw;
}

namespace {
/// Follows "a.b.c" up to the object that holds "c". When writing, shared data assets
/// (ScriptableObjects) on the way are copied first so the game's original weapon is not changed.
Obj walkToHolder(Obj target, const std::string& path, bool cloneAssets, std::string& last) {
    auto parts = cw::split(path, '.');
    last = parts.back();
    Obj cur = target;
    for (size_t i = 0; i + 1 < parts.size() && cur; i++) {
        auto m = findMember(classOf(cur), parts[i], fObject);
        if (!m) return nullptr;
        Value v;
        if (!getMember(cur, m, v) || !v.o) return nullptr;
        Obj next = v.o;
        if (cloneAssets && g_isUnityAsset && g_cloneAsset && g_isUnityAsset(next)) {
            if (Obj copy = g_cloneAsset(next)) if (setMember(cur, m, Value::Object(copy))) next = copy;
        }
        cur = next;
    }
    return cur;
}

bool compatible(const Value& v, const Il2CppType* t) {
    std::vector<uint8_t> raw;
    Obj o;
    auto k = kindOf(t);
    if (k == Kind::String) return v.kind == Kind::String;
    if (k == Kind::Object) return v.kind == Kind::Object && v.o && il2cpp_class_is_assignable_from(il2cpp_class_from_type(t), classOf(v.o));
    if (k == Kind::Enum && v.kind == Kind::String) return convertTo(v, t, raw, o);
    if (k == Kind::Bool) return v.kind == Kind::Bool;
    if (k == Kind::Int || k == Kind::Float || k == Kind::Enum) return v.isNumber();
    return false;
}
}

bool getFirst(Obj o, const std::vector<std::string>& paths, bool (*filter)(const Il2CppType*), Value& out, std::string* used) {
    for (auto& path : paths) {
        std::string last;
        Obj holder = walkToHolder(o, path, false, last);
        if (!holder) continue;
        auto m = findMember(classOf(holder), last, filter);
        if (!m) continue;
        if (getMember(holder, m, out)) { if (used) *used = m.name; return true; }
    }
    return false;
}

bool setFirst(Obj o, const std::vector<std::string>& paths, const Value& v, std::string* used) {
    for (auto& path : paths) {
        std::string last;
        Obj holder = walkToHolder(o, path, true, last);
        if (!holder) continue;
        for (auto& m : findMembers(classOf(holder), last)) {
            if (!m.field && !m.setter) continue;
            if (!compatible(v, m.type)) continue;
            if (setMember(holder, m, v)) {
                if (used) *used = (path.find('.') != std::string::npos ? path.substr(0, path.rfind('.') + 1) : "") + m.name;
                return true;
            }
        }
    }
    return false;
}

} // namespace il2
