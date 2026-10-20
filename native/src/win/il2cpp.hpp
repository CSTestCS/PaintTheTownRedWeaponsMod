// The IL2CPP runtime's C API (exported by GameAssembly.dll), resolved at runtime, plus helpers.
// Lets the mod find the game's classes, call methods and read fields by name - no generated code.
#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

struct Il2CppObject { void* klass; void* monitor; };
struct Il2CppClass;
struct Il2CppType;
struct Il2CppImage;
struct Il2CppAssembly;
struct Il2CppDomain;
struct Il2CppThread;
struct FieldInfo;
struct PropertyInfo;
struct MethodInfo;
struct Il2CppString;
struct Il2CppArray;
typedef Il2CppObject Il2CppException;

namespace il2 {

// Il2CppTypeEnum values used here
enum TypeEnum : int {
    T_VOID = 0x01, T_BOOLEAN = 0x02, T_CHAR = 0x03, T_I1 = 0x04, T_U1 = 0x05, T_I2 = 0x06, T_U2 = 0x07, T_I4 = 0x08,
    T_U4 = 0x09, T_I8 = 0x0a, T_U8 = 0x0b, T_R4 = 0x0c, T_R8 = 0x0d, T_STRING = 0x0e, T_PTR = 0x0f, T_VALUETYPE = 0x11,
    T_CLASS = 0x12, T_ARRAY = 0x14, T_GENERICINST = 0x15, T_I = 0x18, T_U = 0x19, T_OBJECT = 0x1c, T_SZARRAY = 0x1d,
};

#define IL2CPP_API_LIST(X)                                                                                          \
    X(Il2CppDomain*, il2cpp_domain_get, ())                                                                         \
    X(const Il2CppAssembly**, il2cpp_domain_get_assemblies, (const Il2CppDomain*, size_t*))                         \
    X(const Il2CppImage*, il2cpp_assembly_get_image, (const Il2CppAssembly*))                                       \
    X(const char*, il2cpp_image_get_name, (const Il2CppImage*))                                                     \
    X(size_t, il2cpp_image_get_class_count, (const Il2CppImage*))                                                   \
    X(const Il2CppClass*, il2cpp_image_get_class, (const Il2CppImage*, size_t))                                     \
    X(const Il2CppImage*, il2cpp_get_corlib, ())                                                                    \
    X(Il2CppClass*, il2cpp_class_from_name, (const Il2CppImage*, const char*, const char*))                         \
    X(const char*, il2cpp_class_get_name, (Il2CppClass*))                                                           \
    X(const char*, il2cpp_class_get_namespace, (Il2CppClass*))                                                      \
    X(Il2CppClass*, il2cpp_class_get_parent, (Il2CppClass*))                                                        \
    X(Il2CppClass*, il2cpp_class_get_declaring_type, (Il2CppClass*))                                                \
    X(const Il2CppImage*, il2cpp_class_get_image, (Il2CppClass*))                                                   \
    X(const MethodInfo*, il2cpp_class_get_methods, (Il2CppClass*, void**))                                          \
    X(FieldInfo*, il2cpp_class_get_fields, (Il2CppClass*, void**))                                                  \
    X(const PropertyInfo*, il2cpp_class_get_properties, (Il2CppClass*, void**))                                     \
    X(const Il2CppType*, il2cpp_class_get_type, (Il2CppClass*))                                                     \
    X(Il2CppClass*, il2cpp_class_from_type, (const Il2CppType*))                                                    \
    X(bool, il2cpp_class_is_enum, (const Il2CppClass*))                                                             \
    X(bool, il2cpp_class_is_valuetype, (const Il2CppClass*))                                                        \
    X(bool, il2cpp_class_is_assignable_from, (Il2CppClass*, Il2CppClass*))                                          \
    X(bool, il2cpp_class_is_subclass_of, (Il2CppClass*, Il2CppClass*, bool))                                        \
    X(int32_t, il2cpp_class_instance_size, (Il2CppClass*))                                                          \
    X(int32_t, il2cpp_class_value_size, (Il2CppClass*, uint32_t*))                                                  \
    X(const Il2CppType*, il2cpp_class_enum_basetype, (Il2CppClass*))                                                \
    X(int, il2cpp_type_get_type, (const Il2CppType*))                                                               \
    X(char*, il2cpp_type_get_name, (const Il2CppType*))                                                             \
    X(Il2CppObject*, il2cpp_type_get_object, (const Il2CppType*))                                                   \
    X(bool, il2cpp_type_is_byref, (const Il2CppType*))                                                              \
    X(const char*, il2cpp_method_get_name, (const MethodInfo*))                                                     \
    X(uint32_t, il2cpp_method_get_param_count, (const MethodInfo*))                                                 \
    X(const Il2CppType*, il2cpp_method_get_param, (const MethodInfo*, uint32_t))                                    \
    X(const char*, il2cpp_method_get_param_name, (const MethodInfo*, uint32_t))                                     \
    X(const Il2CppType*, il2cpp_method_get_return_type, (const MethodInfo*))                                        \
    X(uint32_t, il2cpp_method_get_flags, (const MethodInfo*, uint32_t*))                                            \
    X(bool, il2cpp_method_is_generic, (const MethodInfo*))                                                          \
    X(Il2CppClass*, il2cpp_method_get_class, (const MethodInfo*))                                                   \
    X(const char*, il2cpp_field_get_name, (FieldInfo*))                                                             \
    X(const Il2CppType*, il2cpp_field_get_type, (FieldInfo*))                                                       \
    X(size_t, il2cpp_field_get_offset, (FieldInfo*))                                                                \
    X(int, il2cpp_field_get_flags, (FieldInfo*))                                                                    \
    X(Il2CppClass*, il2cpp_field_get_parent, (FieldInfo*))                                                          \
    X(void, il2cpp_field_get_value, (Il2CppObject*, FieldInfo*, void*))                                             \
    X(void, il2cpp_field_set_value, (Il2CppObject*, FieldInfo*, void*))                                             \
    X(void, il2cpp_field_static_get_value, (FieldInfo*, void*))                                                     \
    X(const char*, il2cpp_property_get_name, (PropertyInfo*))                                                       \
    X(const MethodInfo*, il2cpp_property_get_get_method, (PropertyInfo*))                                           \
    X(const MethodInfo*, il2cpp_property_get_set_method, (PropertyInfo*))                                           \
    X(Il2CppObject*, il2cpp_runtime_invoke, (const MethodInfo*, void*, void**, Il2CppException**))                  \
    X(void, il2cpp_runtime_object_init, (Il2CppObject*))                                                            \
    X(Il2CppObject*, il2cpp_object_new, (const Il2CppClass*))                                                       \
    X(Il2CppClass*, il2cpp_object_get_class, (Il2CppObject*))                                                       \
    X(void*, il2cpp_object_unbox, (Il2CppObject*))                                                                  \
    X(Il2CppString*, il2cpp_string_new, (const char*))                                                              \
    X(int32_t, il2cpp_string_length, (Il2CppString*))                                                               \
    X(uint16_t*, il2cpp_string_chars, (Il2CppString*))                                                              \
    X(Il2CppArray*, il2cpp_array_new, (Il2CppClass*, uintptr_t))                                                    \
    X(uint32_t, il2cpp_array_length, (Il2CppArray*))                                                                \
    X(uint32_t, il2cpp_gchandle_new, (Il2CppObject*, bool))                                                         \
    X(Il2CppObject*, il2cpp_gchandle_get_target, (uint32_t))                                                        \
    X(void, il2cpp_gchandle_free, (uint32_t))                                                                       \
    X(Il2CppThread*, il2cpp_thread_attach, (Il2CppDomain*))                                                         \
    X(void, il2cpp_thread_detach, (Il2CppThread*))                                                                  \
    X(void*, il2cpp_resolve_icall, (const char*))                                                                   \
    X(Il2CppObject*, il2cpp_value_box, (Il2CppClass*, void*))                                                       \
    X(void, il2cpp_free, (void*))

#define X(ret, name, args) extern ret(*name) args;
IL2CPP_API_LIST(X)
#undef X

/// Resolves every function above from GameAssembly.dll. Returns the names that are missing.
std::vector<std::string> load(void* gameAssembly);

// ------------------------------------------------------------------ helpers

using Obj = Il2CppObject*;

/// Images (assemblies) that belong to the game rather than Unity/.NET.
const std::vector<const Il2CppImage*>& gameImages();
bool isGameImage(const Il2CppImage* img);
/// Class by namespace + name from a specific assembly ("UnityEngine.CoreModule").
Il2CppClass* cls(const char* assembly, const char* ns, const char* name);
/// Class by namespace + name from any assembly.
Il2CppClass* clsAny(const char* ns, const char* name);
/// Game class by simple name (namespace ignored, case-insensitive).
Il2CppClass* gameClass(const std::string& name);
std::vector<Il2CppClass*> allGameClasses();

std::string className(Il2CppClass* k);
std::string fullName(Il2CppClass* k);
std::string typeName(const Il2CppType* t);
Il2CppClass* classOf(Obj o);
bool isA(Obj o, Il2CppClass* k);
bool isSubclass(Il2CppClass* k, Il2CppClass* base);

/// Method by name and parameter count (searches base classes, skips generic methods).
/// paramTypes, if given, must match the parameters' type names (e.g. "System.Type", "UnityEngine.Vector3").
const MethodInfo* method(Il2CppClass* k, const char* name, int argc, std::initializer_list<const char*> paramTypes = {});
const MethodInfo* methodI(Il2CppClass* k, const std::string& name, int argc); // case-insensitive
bool isStatic(const MethodInfo* m);

/// Calls a method. Managed exceptions are caught and logged once; returns nullptr then.
Obj invoke(const MethodInfo* m, void* self, void** args = nullptr, bool* threw = nullptr);
template <class T> T unbox(Obj o, T def = T()) { return o ? *static_cast<T*>(il2cpp_object_unbox(o)) : def; }

/// Creates an object and runs a constructor with the given argument count / types.
Obj newObject(Il2CppClass* k, int argc, void** args, std::initializer_list<const char*> paramTypes = {});

Il2CppString* str(const std::string& s);
std::string toUtf8(Il2CppString* s);
Obj typeObject(Il2CppClass* k);

/// Array helpers (element data starts 32 bytes into the array object on 64-bit).
Il2CppArray* newArray(Il2CppClass* elem, size_t n);
inline void* arrayData(Il2CppArray* a) { return reinterpret_cast<char*>(a) + 32; }
uint32_t arrayLen(Il2CppArray* a);
std::vector<Obj> arrayObjects(Il2CppArray* a);

/// Keeps an object alive until endFrame(): for objects only referenced from C++ heap memory
/// (vectors), which the garbage collector does not scan.
void keepFrame(Obj o);
void endFrame();

/// Strong GC handle so objects stored between frames are not collected.
class Ref {
public:
    Ref() = default;
    explicit Ref(Obj o) : h_(o ? il2cpp_gchandle_new(o, false) : 0) {}
    Ref(const Ref& o) : h_(o.get() ? il2cpp_gchandle_new(o.get(), false) : 0) {}
    Ref& operator=(const Ref& o) { if (this != &o) { reset(); h_ = o.get() ? il2cpp_gchandle_new(o.get(), false) : 0; } return *this; }
    Ref(Ref&& o) noexcept : h_(o.h_) { o.h_ = 0; }
    Ref& operator=(Ref&& o) noexcept { if (this != &o) { reset(); h_ = o.h_; o.h_ = 0; } return *this; }
    ~Ref() { reset(); }
    Obj get() const { return h_ ? il2cpp_gchandle_get_target(h_) : nullptr; }
    void reset() { if (h_) il2cpp_gchandle_free(h_); h_ = 0; }
    explicit operator bool() const { return get() != nullptr; }
private:
    uint32_t h_ = 0;
};

// ------------------------------------------------------------------ name-based member access (game classes)

/// A field, property getter, or parameterless method found by name.
struct Member {
    FieldInfo* field = nullptr;
    const MethodInfo* getter = nullptr;
    const MethodInfo* setter = nullptr;
    std::string name;
    const Il2CppType* type = nullptr;
    explicit operator bool() const { return field || getter; }
};

enum class Kind { None, Bool, Int, Float, String, Enum, Struct, Object };
Kind kindOf(const Il2CppType* t);

/// A loosely typed value read from / written to game members.
struct Value {
    Kind kind = Kind::None;
    bool b = false;
    double n = 0;
    std::string s;
    Obj o = nullptr;
    static Value Bool(bool v) { Value x; x.kind = Kind::Bool; x.b = v; return x; }
    static Value Num(double v) { Value x; x.kind = Kind::Float; x.n = v; return x; }
    static Value Str(const std::string& v) { Value x; x.kind = Kind::String; x.s = v; return x; }
    static Value Object(Obj v) { Value x; x.kind = Kind::Object; x.o = v; return x; }
    bool isNumber() const { return kind == Kind::Int || kind == Kind::Float || kind == Kind::Enum; }
};

/// Set by the Unity layer: copy a data asset (ScriptableObject) / tell whether an object is one.
extern Obj (*g_cloneAsset)(Obj);
extern bool (*g_isUnityAsset)(Obj);

/// Every member matching a name, best match first (exact case, fields/properties before methods).
const std::vector<Member>& findMembers(Il2CppClass* k, const std::string& name);
/// filter receives the member's type; "*word*" names match members containing the word.
Member findMember(Il2CppClass* k, const std::string& name, bool (*filter)(const Il2CppType*) = nullptr);
bool getMember(Obj o, const Member& m, Value& out);
bool setMember(Obj o, const Member& m, const Value& v);
/// First readable path from the list ("a.b" walks into sub-objects).
bool getFirst(Obj o, const std::vector<std::string>& paths, bool (*filter)(const Il2CppType*), Value& out, std::string* used = nullptr);
/// Writes into the first compatible path. Shared assets on the way are copied first.
bool setFirst(Obj o, const std::vector<std::string>& paths, const Value& v, std::string* used = nullptr);
bool convertTo(const Value& v, const Il2CppType* t, std::vector<uint8_t>& raw, Obj& objOut);
bool enumFromName(Il2CppClass* enumClass, const std::string& name, int64_t& out);
std::vector<std::string> enumNames(Il2CppClass* enumClass);

// type filters
bool fBool(const Il2CppType* t);
bool fNumber(const Il2CppType* t);
bool fObject(const Il2CppType* t);

} // namespace il2
