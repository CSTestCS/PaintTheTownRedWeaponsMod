#!/usr/bin/env bash
# Builds, tests and packages everything into dist/:
#   dist/PTTR-CustomWeapons-Installer.exe   one-click installer
#   dist/PTTR-CustomWeapons-DropIn.zip      unzip into the game folder instead of running the installer
#   dist/version.dll                        the mod itself (goes next to PaintTheTownRed.exe)
# Needs: MinGW-w64 (x86_64-w64-mingw32-g++-posix), g++, make, the .NET 8 SDK and python3.
set -euo pipefail
cd "$(dirname "$0")"

echo "== core tests"
make -C native test

echo "== native mod (version.dll)"
make -C native

echo "== installer"
dotnet build src/Installer -c Release --nologo -v quiet

echo "== package"
mkdir -p dist
cp build/native/version.dll dist/
cp build/installer/PTTR-CustomWeapons-Installer.exe dist/
python3 - <<'PY'
import zipfile, glob, os
out = "dist/PTTR-CustomWeapons-DropIn.zip"
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    z.write("build/native/version.dll", "version.dll")
    for f in sorted(glob.glob("CustomWeapons/*")):
        if not os.path.basename(f).startswith("_"):
            z.write(f, "CustomWeapons/" + os.path.basename(f))
    z.writestr("CUSTOM-WEAPONS-INSTALL.txt",
        "Extract everything in this zip into the Paint the Town Red game folder\r\n"
        "(Steam > right-click the game > Manage > Browse local files), so that\r\n"
        "version.dll sits next to PaintTheTownRed.exe. Then start the game as usual.\r\n"
        "If a MelonLoader folder is there from an older version of this mod, delete it\r\n"
        "(it also uses version.dll).\r\n"
        "Add weapons in the CustomWeapons folder - see CustomWeapons/README.txt.\r\n")
print("wrote", out)
PY
ls -la dist
