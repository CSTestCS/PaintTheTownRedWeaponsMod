# Paint the Town Red – Custom Weapons

A mod for the **Steam version of Paint the Town Red** that adds a `CustomWeapons` folder to the game.
Drop in a raw `.fbx` model with a `.txt` file of the same name, and it becomes a new weapon. Custom weapons can turn up:

- **on the ground** in levels, including Beneath (and on new Beneath floors),
- **in shops**, at the price you set,
- **in enemies' hands**; enemies holding a custom gun shoot it at you,
- **in the starting area**, if `Appears at starting area?: yes`.

## Install (players)

**Option A – installer (recommended).** Download **`dist/PTTR-CustomWeapons-Installer.exe`** and run it.
It finds the game through Steam, installs [MelonLoader](https://github.com/LavaGang/MelonLoader) (the mod loader, bundled inside),
the mod, and three example weapons. Run it again to update or uninstall.

**Option B – drop-in.** Extract **`dist/PTTR-CustomWeapons-DropIn.zip`** into the game folder
(Steam → right-click the game → *Manage* → *Browse local files*) so that `version.dll` sits next to `PaintTheTownRed.exe`.

Then start the game. **The first launch takes a few minutes** while MelonLoader sets itself up. It needs internet access once.
Already using MelonLoader 0.7+? Just put `dist/PTTRCustomWeapons.dll` into the game's `Mods` folder.

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

Paint the Town Red is an IL2CPP Unity game, so the mod runs on MelonLoader 0.7 and uses the game's own classes
(`PTTRPlayer`, `Enemy`, the weapon class returned by `PTTRPlayer.CurrentWeapon`) through reflection.

- **Models.** Unity can't load `.fbx` files at runtime, so the mod has its own FBX reader. It reads binary 6.1–7.7 and ASCII 7.x files, including node transforms, n-gons, normals, UVs, vertex colours, embedded textures, and the axis/unit conversion Unity's importer does. It is tested against real Blender exports.
- **Weapons.** A custom weapon is a copy of a real game weapon. The original model is hidden, yours is attached and auto-fitted, and your stats are written into the game's weapon fields. The game itself handles pickup, swinging, throwing, dropping, breaking, buying and enemy use. Melee weapons fight entirely through the game.
- **Ranged.** Custom guns use the mod's own projectile system: raycast-swept projectiles with drop, bounce, pierce, spread, pellets, bursts, area damage with falloff and physics force, plus magazine and reload. Hits call the game's own damage functions.
- **Game integration** (class names, member names, scene names, keys) lives in `CustomWeapons/_GameHooks.cfg`. After a game update, look at `_GameReport.txt` and fix names there; no recompiling needed.

## Status / known limitations

- **This has not been tested inside the game yet.** It was built and unit-tested without access to Paint the Town Red.
  The FBX loading, settings parsing, ballistics and the installer are tested. Class names come from a public PTTR mod (`PTTRPlayer`, `Enemy`, `CurrentWeapon`).
  The rest of the game integration (shop and damage method names, the Beneath scene names) is best-effort and configurable. If something doesn't happen in game, check `CustomWeapons/_CustomWeapons.log` and `_GameReport.txt`, then adjust `_GameHooks.cfg`.
- A custom gun copies a game gun. By default the copy's own ammo is set to 0 so only the custom projectiles fire (`GameGunAmmo` in `_GameHooks.cfg`).
- Area of effect, bounce and other projectile settings apply to ranged weapons. Melee hits are the game's own.
- Custom weapons carried to a new Beneath floor may revert to the original model if the game re-creates the weapon object.
- Sounds and animations come from the copied game weapon.
- MelonLoader needs internet on first launch to download Unity dependencies.

## Building from source

Requires the .NET 8 SDK (and python3 for packaging).

```
./build.sh                                   # tests + mod + installer + dist/ packages
dotnet test tests/PTTRCustomWeapons.Tests    # tests only
dotnet run --project tools/ExampleGen -- CustomWeapons   # regenerate the example models
```

No game files are needed to build. `src/Stubs` contains small reference-only stand-ins for the assemblies MelonLoader generates from the game. They are never shipped.
GitHub Actions builds every push and publishes a release for `v*` tags.

| Path | What |
|---|---|
| `src/PTTRCustomWeapons/Core` | Engine-independent code: FBX reader, settings parser, ballistics (unit-tested) |
| `src/PTTRCustomWeapons/Game` | Unity/game side: spawning, model swap, combat, projectiles, damage, report |
| `src/Installer` | Windows installer (.NET Framework 4.8, built into Windows 10/11) |
| `src/Stubs` | Compile-time stand-ins for MelonLoader's generated Unity assemblies |
| `tests` | xUnit tests, including Blender-exported FBX fixtures |
| `tools/ExampleGen` | Generates the example models |
| `CustomWeapons` | Example weapons, template and readme copied into the game |
