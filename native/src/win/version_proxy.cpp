// Makes this DLL a drop-in "version.dll": Windows loads it from the game folder when the game starts,
// and every real version.dll function is passed straight through to the system copy.
#include <windows.h>
#include <cstdint>

namespace {
HMODULE realVersion() {
    static HMODULE h = [] {
        wchar_t path[MAX_PATH];
        GetSystemDirectoryW(path, MAX_PATH);
        lstrcatW(path, L"\\version.dll");
        return LoadLibraryW(path);
    }();
    return h;
}
}

// Every version.dll function takes at most 8 integer/pointer arguments and returns an integer,
// so one forwarding shape fits all of them on x64.
#define FORWARD(name)                                                                                       \
    extern "C" uintptr_t proxy_##name(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e,     \
                                      uintptr_t f, uintptr_t g, uintptr_t h) {                              \
        using Fn = uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t); \
        static Fn fn = reinterpret_cast<Fn>(GetProcAddress(realVersion(), #name));                          \
        if (!fn) { SetLastError(ERROR_PROC_NOT_FOUND); return 0; }                                          \
        return fn(a, b, c, d, e, f, g, h);                                                                  \
    }

FORWARD(GetFileVersionInfoA)
FORWARD(GetFileVersionInfoByHandle)
FORWARD(GetFileVersionInfoExA)
FORWARD(GetFileVersionInfoExW)
FORWARD(GetFileVersionInfoSizeA)
FORWARD(GetFileVersionInfoSizeExA)
FORWARD(GetFileVersionInfoSizeExW)
FORWARD(GetFileVersionInfoSizeW)
FORWARD(GetFileVersionInfoW)
FORWARD(VerFindFileA)
FORWARD(VerFindFileW)
FORWARD(VerInstallFileA)
FORWARD(VerInstallFileW)
FORWARD(VerLanguageNameA)
FORWARD(VerLanguageNameW)
FORWARD(VerQueryValueA)
FORWARD(VerQueryValueW)
