PAINT THE TOWN RED - CUSTOM WEAPONS
===================================

ADDING A WEAPON
  1. Put your model here:          MyWeapon.fbx
  2. Add a settings file next to it with the SAME name:   MyWeapon.txt
     (copy TEMPLATE.txt - every setting is explained inside it)
  3. Optional texture, same name:  MyWeapon.png  (or .jpg)
     If there is no image, a texture embedded in the .fbx is used, otherwise
     the model is drawn in its "Tint" colour / vertex colours.
  4. Play. Custom weapons show up lying around in levels (Beneath included),
     in shops and in enemies' hands, depending on the chances in the .txt.

  Sub-folders are fine. Files starting with "_" and TEMPLATE.txt are ignored.

A MINIMAL .txt
  Name: Laser Pistol
  Melee or Ranged: Ranged
  Fire type: Single
  Projectile speed: 0        (0 = hitscan)
  Damage: 30

KEYS (in a level)
  F8   spawn every custom weapon in front of you (great for testing)
  F9   reload all .txt / .fbx files after editing them
  F10  write _GameReport.txt (game class names, for troubleshooting)
  R    reload a custom gun
  Keys can be changed in _GameHooks.cfg. Edits to _GameHooks.cfg need a game restart.

MODEL TIPS
  * Export from Blender with File > Export > FBX, default settings are fine.
    Apply modifiers. Keep it low-poly (a few thousand triangles at most).
  * The model is automatically sized and turned to match the game weapon it
    replaces ("Auto fit"). If it is held the wrong way, set
    Model rotation: 0, 90, 0  (try 90/180/-90 on each axis) and press F9.
  * "Base weapon" picks which game weapon to copy (hold pose, swing, hitbox,
    sounds). Names are listed in _GameReport.txt.

FILES THE MOD WRITES HERE
  _GameHooks.cfg      how the mod talks to the game (class/member names).
                      Only needs changing if a game update breaks something.
  _GameReport.txt     what the mod found inside the game.
  _CustomWeapons.log  what happened (loaded weapons, errors). Check this first
                      if a weapon does not show up.
