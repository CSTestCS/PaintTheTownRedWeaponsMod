using System;
using System.Collections.Generic;
using System.IO;
using MelonLoader;
using PTTRCustomWeapons.Core;
using PTTRCustomWeapons.Game;

[assembly: MelonInfo(typeof(PTTRCustomWeapons.ModEntry), "PTTR Custom Weapons", "1.0.0", "PaintTheTownRedWeaponsMod")]
[assembly: MelonGame(null, null)] // works with any company/product name the game reports
[assembly: MelonColor(255, 200, 30, 30)]

namespace PTTRCustomWeapons
{
    /// <summary>
    /// Entry point. Drop "Name.fbx" + "Name.txt" (+ optional "Name.png") into the game's CustomWeapons folder.
    /// </summary>
    public sealed class ModEntry : MelonMod
    {
        public static string Folder => Path.Combine(U.GameRoot, "CustomWeapons");

        private List<LoadedWeapon> _weapons = new List<LoadedWeapon>();
        private HooksConfig _cfg = null!;
        private GameBridge _game = null!;
        private WeaponSkinner _skinner = null!;
        private DamageDealer _damage = null!;
        private Projectiles _projectiles = null!;
        private Combat _combat = null!;
        private SpawnDirector _director = null!;
        private readonly Hud _hud = new Hud();
        private int _spawnKey, _reloadPacksKey, _reportKey;
        private bool _showHud;
        private bool _failed;

        public override void OnInitializeMelon()
        {
            try
            {
                Directory.CreateDirectory(Folder);
                Log.Init(LoggerInstance, Folder);
                _cfg = HooksConfig.LoadOrCreate(Folder);
                _game = new GameBridge(_cfg);
                _skinner = new WeaponSkinner(_game);
                _damage = new DamageDealer(_game);
                _projectiles = new Projectiles(_game, _damage);
                _combat = new Combat(_game, _skinner, _projectiles);
                _director = new SpawnDirector(_game, _skinner, _combat, () => _weapons);
                _director.FirstGameplaySetup += () => ReportWriter.Write(Folder, _game, _skinner, _director.SceneName);
                _spawnKey = Win32Input.KeyCode(_cfg.Raw("SpawnAllKey", "F8"));
                _reloadPacksKey = Win32Input.KeyCode(_cfg.Raw("ReloadPacksKey", "F9"));
                _reportKey = Win32Input.KeyCode(_cfg.Raw("ReportKey", "F10"));
                _showHud = _cfg.Bool("ShowHud", true);
                LoadWeapons();
            }
            catch (Exception ex)
            {
                _failed = true;
                LoggerInstance.Error("Custom Weapons failed to start: " + ex);
            }
        }

        private void LoadWeapons()
        {
            _weapons = WeaponPack.LoadFolder(Folder, Log.Info, Log.Warn);
            Log.Info($"{_weapons.Count} custom weapon(s) loaded from {Folder}");
            _hud.Toast($"Custom Weapons: {_weapons.Count} loaded  (F8 spawn all, F9 reload files, F10 report)", 8f);
        }

        public override void OnSceneWasInitialized(int buildIndex, string sceneName)
        {
            if (_failed) return;
            try
            {
                _projectiles.Clear();
                _director.OnSceneLoaded(sceneName);
            }
            catch (Exception ex) { Log.Error("Scene setup failed: " + ex); }
        }

        public override void OnUpdate()
        {
            if (_failed) return;
            try
            {
                Win32Input.Poll();
                if (Win32Input.GameHasFocus())
                {
                    if (Win32Input.Pressed(_spawnKey)) { _director.SpawnAllInFront(); _hud.Toast("Spawned custom weapons"); }
                    if (Win32Input.Pressed(_reloadPacksKey)) ReloadPacks();
                    if (Win32Input.Pressed(_reportKey))
                    {
                        _game.Resolve();
                        ReportWriter.Write(Folder, _game, _skinner, _director.SceneName);
                        _hud.Toast("Wrote CustomWeapons/_GameReport.txt");
                    }
                }
                _director.Update();
                _combat.Update();
                _projectiles.Update();
            }
            catch (Exception ex) { Log.WarnOnce("update" + ex.GetType().Name, "Update error: " + ex); }
        }

        /// <summary>F9: re-read every .txt/.fbx and refresh custom weapons that already exist.</summary>
        private void ReloadPacks()
        {
            var old = _weapons;
            _projectiles.Clear();
            AssetFactory.Clear();
            LoadWeapons();
            foreach (var ci in new List<CustomInstance>(_skinner.All))
            {
                var fresh = _weapons.Find(w => w.Def.Id.Equals(ci.Def.Id, StringComparison.OrdinalIgnoreCase));
                if (fresh != null && U.Alive(ci.WeaponComponent)) _skinner.Apply(ci.WeaponComponent, fresh);
            }
            _hud.Toast($"Reloaded {_weapons.Count} custom weapon(s)");
            GC.KeepAlive(old);
        }

        public override void OnGUI()
        {
            if (_failed || !_showHud) return;
            _hud.Draw(_combat?.PlayerWeapon);
        }
    }
}
