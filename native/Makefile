# Builds the native mod (a version.dll proxy) with MinGW-w64, and the core unit tests for the host.
#   make            -> ../build/native/version.dll
#   make test       -> builds and runs the unit tests (any OS with g++)
CXX_WIN ?= x86_64-w64-mingw32-g++-posix
CC_WIN  ?= x86_64-w64-mingw32-gcc-posix
CXX     ?= g++
OUT     := ../build/native
CXXFLAGS_COMMON := -std=c++17 -O2 -Wall -Wno-unused-function -Wno-missing-field-initializers -include cstring

CORE := src/core/fbx.cpp src/core/weapon.cpp src/core/hooks_config.cpp src/core/stb_impl.cpp
WIN  := src/win/main.cpp src/win/il2cpp.cpp src/win/unity.cpp src/win/game.cpp src/win/play.cpp src/win/report.cpp src/win/log.cpp src/win/version_proxy.cpp
MH   := third_party/minhook/src/buffer.c third_party/minhook/src/hook.c third_party/minhook/src/trampoline.c third_party/minhook/src/hde/hde64.c

all: $(OUT)/version.dll

$(OUT)/minhook/%.o: third_party/minhook/src/%.c
	@mkdir -p $(dir $@)
	$(CC_WIN) -O2 -c $< -o $@

MH_OBJ := $(patsubst third_party/minhook/src/%.c,$(OUT)/minhook/%.o,$(MH))

$(OUT)/version.dll: $(CORE) $(WIN) $(MH_OBJ) src/win/version.def $(wildcard src/*/*.hpp) src/core/hooks_default.inc
	@mkdir -p $(OUT)
	$(CXX_WIN) $(CXXFLAGS_COMMON) -shared -o $@ $(CORE) $(WIN) $(MH_OBJ) src/win/version.def \
		-static -static-libgcc -static-libstdc++ -s -Wl,--enable-stdcall-fixup -lkernel32 -luser32

$(OUT)/tests: $(CORE) tests/tests.cpp $(wildcard src/core/*.hpp) src/core/hooks_default.inc
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS_COMMON) -o $@ tests/tests.cpp $(CORE)

test: $(OUT)/tests
	$(OUT)/tests ..

clean:
	rm -rf $(OUT)

.PHONY: all test clean
