#!/usr/bin/env bash
# Builds, tests and packages everything into dist/:
#   dist/PTTR-CustomWeapons-Installer.exe   one-click installer (MelonLoader bundled)
#   dist/PTTR-CustomWeapons-DropIn.zip      unzip into the game folder instead of running the installer
#   dist/PTTRCustomWeapons.dll              the mod alone (for people who already have MelonLoader)
# Needs the .NET 8 SDK and python3. Works on Linux, macOS and Windows (Git Bash).
set -euo pipefail
cd "$(dirname "$0")"

ML_VERSION="0.7.1"
ML_ZIP="build/MelonLoader.x64.zip"
ML_URL="https://github.com/LavaGang/MelonLoader/releases/download/v${ML_VERSION}/MelonLoader.x64.zip"

echo "== tests"
dotnet test tests/PTTRCustomWeapons.Tests -c Release --nologo -v quiet

echo "== mod"
dotnet build src/PTTRCustomWeapons -c Release --nologo -v quiet

echo "== MelonLoader ${ML_VERSION}"
mkdir -p build
if [ ! -f "$ML_ZIP" ]; then curl -fsSL -o "$ML_ZIP" "$ML_URL"; fi

echo "== installer"
dotnet build src/Installer -c Release --nologo -v quiet -p:MelonLoaderZip="$PWD/$ML_ZIP"

echo "== package"
mkdir -p dist
cp build/installer/PTTR-CustomWeapons-Installer.exe dist/
cp build/mod/PTTRCustomWeapons.dll dist/
python3 - "$ML_ZIP" <<'EOF'
import sys, zipfile, os, glob
ml = sys.argv[1]
out = "dist/PTTR-CustomWeapons-DropIn.zip"
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    with zipfile.ZipFile(ml) as src:
        for item in src.infolist():
            z.writestr(item, src.read(item.filename))
    z.write("build/mod/PTTRCustomWeapons.dll", "Mods/PTTRCustomWeapons.dll")
    for f in sorted(glob.glob("CustomWeapons/*")):
        if not os.path.basename(f).startswith("_"):
            z.write(f, "CustomWeapons/" + os.path.basename(f))
    z.writestr("CUSTOM-WEAPONS-INSTALL.txt",
        "Extract everything in this zip into the Paint the Town Red game folder\n"
        "(Steam > right-click the game > Manage > Browse local files), so that\n"
        "version.dll sits next to PaintTheTownRed.exe. Then start the game.\n"
        "The first start takes a few minutes while MelonLoader sets itself up.\n"
        "Add weapons in the CustomWeapons folder - see CustomWeapons/README.txt.\n")
print("wrote", out)
EOF
ls -la dist
