// A fake PaintTheTownRed.exe: imports version.dll (like the real game), loads GameAssembly.dll
// later (like UnityPlayer does), "initializes" it, then calls PTTRPlayer.Update every frame.
#include <windows.h>
#include <stdio.h>

typedef void (*UpdateFn)(void*, const void*);

int main(void) {
    // goes through the version.dll proxy
    DWORD dummy;
    DWORD size = GetFileVersionInfoSizeW(L"C:\\windows\\system32\\kernel32.dll", &dummy);
    printf("GetFileVersionInfoSizeW via proxy: %lu\n", size);

    Sleep(300);
    HMODULE ga = LoadLibraryA("GameAssembly.dll");
    if (!ga) { printf("no GameAssembly.dll\n"); return 2; }
    Sleep(300);
    ((void (*)(void))GetProcAddress(ga, "mock_init"))();
    UpdateFn update = ((UpdateFn (*)(void))GetProcAddress(ga, "mock_update"))();
    LONG (*calls)(void) = (LONG (*)(void))GetProcAddress(ga, "mock_update_calls");
    for (int frame = 0; frame < 400; frame++) { update(NULL, NULL); Sleep(10); }
    printf("Update ran %ld times\n", calls());
    return 0;
}
