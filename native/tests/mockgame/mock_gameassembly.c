// A fake GameAssembly.dll: just enough of the IL2CPP API for the mod to start up, find
// PTTRPlayer.Update and hook it. Used to test the native mod under Wine without the real game.
#include <windows.h>
#include <stdint.h>
#include <string.h>

typedef void (*UpdateFn)(void*, const void*);
typedef struct { void* methodPointer; const char* name; } MockMethod;
typedef struct { const char* name; const char* ns; MockMethod* methods; int methodCount; } MockClass;

static volatile LONG g_updateCalls = 0;
__declspec(noinline) void PTTRPlayer_Update(void* self, const void* mi) { (void)self; (void)mi; InterlockedIncrement(&g_updateCalls); }

static MockMethod g_methods[] = {{(void*)PTTRPlayer_Update, "Update"}};
static MockClass g_player = {"PTTRPlayer", "", g_methods, 1};
static int g_initialized = 0;
static int g_image = 1, g_assembly = 2, g_domain = 3, g_corlib = 4;
static const void* g_assemblies[] = {&g_assembly};

__declspec(dllexport) void mock_init(void) { g_initialized = 1; }          // what il2cpp_init would do
__declspec(dllexport) UpdateFn mock_update(void) { return PTTRPlayer_Update; }
__declspec(dllexport) LONG mock_update_calls(void) { return g_updateCalls; }

__declspec(dllexport) void* il2cpp_get_corlib(void) { return g_initialized ? &g_corlib : NULL; }
__declspec(dllexport) void* il2cpp_domain_get(void) { return &g_domain; }
__declspec(dllexport) const void** il2cpp_domain_get_assemblies(void* d, size_t* n) { (void)d; *n = 1; return g_assemblies; }
__declspec(dllexport) void* il2cpp_assembly_get_image(void* a) { (void)a; return &g_image; }
__declspec(dllexport) const char* il2cpp_image_get_name(void* i) { (void)i; return "Assembly-CSharp.dll"; }
__declspec(dllexport) size_t il2cpp_image_get_class_count(void* i) { (void)i; return 1; }
__declspec(dllexport) void* il2cpp_image_get_class(void* i, size_t n) { (void)i; return n == 0 ? &g_player : NULL; }
__declspec(dllexport) void* il2cpp_class_from_name(void* i, const char* ns, const char* n) { (void)i; (void)ns; (void)n; return NULL; }
__declspec(dllexport) const char* il2cpp_class_get_name(MockClass* k) { return k->name; }
__declspec(dllexport) const char* il2cpp_class_get_namespace(MockClass* k) { return k->ns; }
__declspec(dllexport) void* il2cpp_class_get_parent(void* k) { (void)k; return NULL; }
__declspec(dllexport) void* il2cpp_class_get_declaring_type(void* k) { (void)k; return NULL; }
__declspec(dllexport) const MockMethod* il2cpp_class_get_methods(MockClass* k, void** iter) {
    intptr_t i = (intptr_t)*iter;
    if (i >= k->methodCount) return NULL;
    *iter = (void*)(i + 1);
    return &k->methods[i];
}
__declspec(dllexport) const char* il2cpp_method_get_name(const MockMethod* m) { return m->name; }
__declspec(dllexport) uint32_t il2cpp_method_get_param_count(const void* m) { (void)m; return 0; }
__declspec(dllexport) int il2cpp_method_is_generic(const void* m) { (void)m; return 0; }
__declspec(dllexport) void* il2cpp_thread_attach(void* d) { (void)d; return &g_domain; }
__declspec(dllexport) void il2cpp_thread_detach(void* t) { (void)t; }
__declspec(dllexport) void* il2cpp_runtime_invoke(void* m, void* o, void** a, void** e) { (void)m; (void)o; (void)a; if (e) *e = NULL; return NULL; }
