using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace PTTRCustomWeapons.Core
{
    public sealed class LoadedWeapon
    {
        public WeaponDefinition Def = new WeaponDefinition();
        public MeshData Mesh = new MeshData();
        public MeshData? ProjectileMesh;
        public byte[]? TextureBytes;
    }

    /// <summary>Scans the CustomWeapons folder: every .txt with a matching .fbx becomes a weapon.</summary>
    public static class WeaponPack
    {
        public static List<LoadedWeapon> LoadFolder(string folder, Action<string> info, Action<string> warn)
        {
            var result = new List<LoadedWeapon>();
            if (!Directory.Exists(folder)) return result;

            var txts = Directory.GetFiles(folder, "*.txt", SearchOption.AllDirectories)
                .Where(p => !Path.GetFileName(p).StartsWith("_"))                       // _GameReport.txt etc.
                .Where(p => !Path.GetFileName(p).Equals("README.txt", StringComparison.OrdinalIgnoreCase))
                .Where(p => !Path.GetFileName(p).StartsWith("TEMPLATE", StringComparison.OrdinalIgnoreCase))
                .OrderBy(p => p, StringComparer.OrdinalIgnoreCase);

            var ids = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var txt in txts)
            {
                var id = Path.GetFileNameWithoutExtension(txt);
                try
                {
                    var def = WeaponConfigParser.ParseFile(txt);
                    if (!File.Exists(def.ModelPath))
                    {
                        warn($"[{id}] skipped: no model file \"{id}.fbx\" next to \"{Path.GetFileName(txt)}\".");
                        continue;
                    }
                    if (!ids.Add(id))
                    {
                        warn($"[{id}] skipped: another weapon already uses this file name.");
                        continue;
                    }
                    foreach (var w in def.Warnings) warn($"[{id}] {w}");

                    var loaded = new LoadedWeapon { Def = def, Mesh = FbxMeshExtractor.Load(def.ModelPath) };
                    foreach (var w in loaded.Mesh.Warnings) warn($"[{id}] {w}");
                    if (loaded.Mesh.TriangleCount == 0)
                    {
                        warn($"[{id}] skipped: the FBX contains no triangles.");
                        continue;
                    }

                    if (def.TexturePath != null) loaded.TextureBytes = File.ReadAllBytes(def.TexturePath);
                    else loaded.TextureBytes = loaded.Mesh.EmbeddedTexture;

                    if (def.ProjectileModelPath != null)
                    {
                        try { loaded.ProjectileMesh = FbxMeshExtractor.Load(def.ProjectileModelPath); }
                        catch (Exception ex) { warn($"[{id}] projectile model failed to load: {ex.Message}"); }
                    }

                    info($"[{id}] loaded \"{def.Name}\" ({def.Kind}, {loaded.Mesh.TriangleCount} triangles{(loaded.TextureBytes != null ? ", textured" : "")})");
                    result.Add(loaded);
                }
                catch (Exception ex)
                {
                    warn($"[{id}] failed to load: {ex.Message}");
                }
            }
            return result;
        }
    }
}
