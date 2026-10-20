#!/usr/bin/env bash
# Runs the native mod inside a fake game under Wine: checks that version.dll is loaded, forwards
# version.dll calls, waits for the (mock) IL2CPP runtime, hooks PTTRPlayer.Update and runs per frame.
# Needs: wine64, MinGW-w64, and a built ../../../build/native/version.dll (make -C native).
set -euo pipefail
cd "$(dirname "$0")"
OUT=../../../build/mockgame
rm -rf "$OUT" && mkdir -p "$OUT"
x86_64-w64-mingw32-gcc -O1 -shared -o "$OUT/GameAssembly.dll" mock_gameassembly.c
x86_64-w64-mingw32-gcc -O1 -o "$OUT/PaintTheTownRed.exe" mock_game.c -lversion
cp ../../../build/native/version.dll "$OUT/"
cp -r ../../../CustomWeapons "$OUT/"
rm -f "$OUT/CustomWeapons/_"*
export WINEDEBUG=-all WINEDLLOVERRIDES="version=n,b" PATH="/usr/lib/wine:$PATH"
(cd "$OUT" && wine64 PaintTheTownRed.exe)
LOG="$OUT/CustomWeapons/_CustomWeapons.log"
grep -q "3 custom weapon(s) loaded" "$LOG"
grep -q "Hooked PTTRPlayer.Update" "$LOG"
grep -q "Unity classes missing" "$LOG"   # expected: the mock has no Unity engine
echo "mock game test passed"
