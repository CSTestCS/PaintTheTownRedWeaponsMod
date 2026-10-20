# Paint the Town Red – Custom Weapons

A mod for the **Steam version of Paint the Town Red** that adds a `CustomWeapons` folder to the game.
Drop in a raw `.fbx` model with a `.txt` file of the same name, and it becomes a new weapon. Custom weapons can turn up:

- **on the ground** in levels, including Beneath (and on new Beneath floors),
- **in shops**, at the price you set,
- **in enemies' hands**; enemies holding a custom gun shoot it at you,
- **in the starting area**, if `Appears at starting area?: yes`.

## Install (players)

**Option A – installer (recommended).** Download **`dist/PTTR-CustomWeapons-Installer.exe`** and run it.
It finds the game through Steam and puts the mod (a small `version.dll`) next to the game's exe, along with three example weapons.
Run it again to update or uninstall.

**Option B – drop-in.** Extract **`dist/PTTR-CustomWeapons-DropIn.zip`** into the game folder
(Steam → right-click the game → *Manage* → *Browse local files*), so that `version.dll` sits next to `PaintTheTownRed.exe`.

Then start the game as usual. There is no mod loader, no first-launch setup, and no extra background process.
The mod adds well under a second to startup.

*Upgrading from the first (MelonLoader) version:* the installer removes MelonLoader for you after asking.
For the drop-in, delete the `MelonLoader` folder yourself.

## Adding a weapon

```
Paint the Town Red/
  CustomWeapons/
    LaserPistol.fbx     <- your model
    LaserPistol.txt     <- its settings (copy TEMPLATE.txt)
    LaserPistol.png     <- optional texture (or embed it in the .fbx)
```

`LaserPistol.txt`:

```
Name: Laser Pistol
Melee or Ranged: Ranged
Magazine size: 10
Fire type: Single
Projectile speed: 0            # 0 = instant hit (hitscan)
Projectile drop: 0
Automatic fire rate: 4
Projectile bounce: 0
Projectile visual: red bullet
Damage: 35
Damage type: Fire
Area of effect: 0
Damage force: 8
Appears at starting area?: yes
```

The format is `Setting: value` (or `=`), in any order, ignoring case and spaces. `#` starts a comment.
A file with only values, one per line, in the order of the list below, also works.
[`CustomWeapons/TEMPLATE.txt`](CustomWeapons/TEMPLATE.txt) documents every setting.

### All settings

| Setting | Meaning | Default |
|---|---|---|
| **Name** | Display name | file name |
| **Melee or Ranged** | `Melee` / `Ranged` | Melee |
| **Magazine size** | Shots per reload, `0`/`infinite` = never reload | 12 |
| **Fire type** | `Single`, `Burst`, `Automatic`, `Shotgun`, `Hitscan`, `Beam` | Single |
| **Projectile speed** | m/s, `0`/`hitscan` = instant | 60 |
| **Projectile drop** | Gravity multiplier (0 = straight, 1 = normal) | 0 |
| **Automatic fire rate** | Shots per second | 6 |
| **Projectile bounce** | Bounces off walls | 0 |
| **Projectile visual** | `Bullet`, `Sphere`, `Cube`, `Arrow`, `None`, `Model`, or `Something.fbx`. A colour word works too (`red sphere`) | Bullet |
| **Damage** | Per hit / per projectile | 25 |
| **Damage type** | `Blunt`, `Sharp`, `Fire`, `Explosive`, … passed to the game; `Explosive` adds a blast | Blunt |
| **Area of effect** | Blast radius (m) on impact | 0 |
| **Damage force** | Push on bodies, ragdolls and props | 10 |
| **Appears at starting area?** | Always placed in the starting area | no |
| Reserve ammo, Reload time | Spare ammo (`infinite`), seconds | infinite, 1.5 |
| Burst count, Pellets, Spread | Burst size, projectiles per shot, inaccuracy (°) | 3, 1, 1 |
| Bounciness, Pierce, Range | Speed kept per bounce, enemies passed through, hitscan range | 70%, 0, 300 |
| Projectile color / size / lifetime / model | Look of the projectile | yellow, 0.08 m, 6 s |
| Durability, Unbreakable, Attack speed, Throw damage | Written into the game's own weapon | game value |
| Appears on ground / in shop, Used by enemies | Where it may appear | yes |
| Ground chance, Shop chance, Enemy chance | Spawn chances (`25%` or `0.25`) | 35%, 25%, 15% |
| Shop price, Rarity | Price; `common`…`legendary` or a weight | 100, common |
| Enemy fire interval / accuracy / damage multiplier | How enemies use custom guns | 1.2 s, 4°, 0.5 |
| Base weapon | Name of the game weapon to copy (hold pose, swing, sounds) | auto |
| Auto fit, Model scale / rotation / offset | Model placement in the hand | yes, 1, 0, 0 |
| Texture, Tint | Another image file, colour multiplier | Name.png, white |

### In-game keys

| Key | Action |
|---|---|
| **F8** | Spawn every custom weapon in front of you |
| **F9** | Reload all weapon files (edit, then press F9 to see changes) |
| **F10** | Write `CustomWeapons/_GameReport.txt` |
| **R** | Reload a custom gun |

## How it works

Paint the Town Red is an IL2CPP Unity game. The mod is a native `version.dll`, which Windows loads from the game folder.
Every real `version.dll` function is passed through to the system copy.

- **Startup.** A background thread reads the weapon files, waits for the game's IL2CPP runtime, then hooks one per-frame game method (`PTTRPlayer.Update`). All game work happens inside that hook, on Unity's main thread.
- **Game access.** Classes, methods and fields are found by name through the IL2CPP runtime's own C API. Nothing is decompiled or generated, so there is no MelonLoader-style first-launch step.
- **Models.** The mod has its own FBX reader, because Unity can't load `.fbx` at runtime. It reads binary 6.1–7.7 and ASCII 7.x files, including node transforms, n-gons, normals, UVs, vertex colours and embedded textures, and applies the same axis/unit conversion as Unity's importer. It is tested against real Blender exports.
- **Weapons.** A custom weapon is a copy of a real game weapon. The original model is hidden, yours is attached and auto-fitted, and your stats are written into the game's weapon fields. The game handles pickup, swinging, throwing, dropping, breaking, buying and enemy use.
- **Ranged.** Custom guns use the mod's own projectiles: drop, bounce, pierce, spread, pellets, bursts, area damage with falloff, physics force, magazine and reload. Hits call the game's own damage functions.
- **Game integration** (class names, member names, scene names, keys) lives in `CustomWeapons/_GameHooks.cfg`. After a game update, look at `_GameReport.txt` and fix names there.

## Status / known limitations

- **Not tested inside the real game yet.** This was built without access to Paint the Town Red.
  - Tested: the FBX reader, settings parser, ballistics and installer. Under Wine with a mock game, the mod loads as `version.dll`, waits for the runtime, hooks `PTTRPlayer.Update` and runs every frame without disturbing the game.
  - Not tested: the Unity-side work (spawning, models, firing, damage). Class names come from a public PTTR mod (`PTTRPlayer`, `Enemy`, `CurrentWeapon`). Shop and damage method names and Beneath scene names are best guesses, and all of them can be changed in `_GameHooks.cfg`.
  - If something doesn't happen, check `CustomWeapons/_CustomWeapons.log` and `_GameReport.txt`.
- The mod only runs while a level is loaded (its per-frame hook is the player's update).
- A custom gun copies a game gun. By default the copy's own ammo is set to 0 so only the custom projectiles fire (`GameGunAmmo` in `_GameHooks.cfg`).
- There is no on-screen ammo counter yet; reloads happen automatically or with **R**.
- Area of effect, bounce and other projectile settings apply to ranged weapons. Melee hits are the game's own.
- Custom weapons carried to a new Beneath floor may revert to the original model if the game re-creates the weapon object.
- Sounds and animations come from the copied game weapon.

## Building from source

Requires MinGW-w64, g++, make, the .NET 8 SDK (for the installer) and python3.
On Ubuntu: `sudo apt install g++-mingw-w64-x86-64 dotnet-sdk-8.0`.

```
./build.sh                  # core tests + version.dll + installer + dist/ packages
make -C native test         # core unit tests only (any OS with g++)
dotnet run --project tools/ExampleGen -- CustomWeapons   # regenerate the example models
```

No game files are needed to build. GitHub Actions builds every push and publishes a release for `v*` tags.

| Path | What |
|---|---|
| `native/src/core` | Portable code: FBX reader, settings parser, ballistics (unit-tested) |
| `native/src/win` | The mod: `version.dll` proxy, IL2CPP access, Unity calls, spawning, combat, projectiles, damage, report |
| `native/tests` | Unit tests with Blender-exported fixtures, plus a mock game for testing under Wine |
| `native/third_party` | MinHook (function hooking, BSD-2) and stb_image (zlib/PNG/JPG, public domain) |
| `src/Installer` | Windows installer (.NET Framework 4.8, built into Windows 10/11) |
| `tools/ExampleGen` | Generates the example models |
| `CustomWeapons` | Example weapons, template and readme copied into the game |
