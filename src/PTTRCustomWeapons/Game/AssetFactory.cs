using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Il2CppInterop.Runtime;
using Il2CppInterop.Runtime.InteropTypes.Arrays;
using PTTRCustomWeapons.Core;
using UnityEngine;
using UnityEngine.Rendering;
using NV4 = System.Numerics.Vector4;

namespace PTTRCustomWeapons.Game
{
    /// <summary>Creates (and caches) Unity meshes, textures and materials for custom weapons.</summary>
    internal static class AssetFactory
    {
        private const HideFlags Keep = HideFlags.DontUnloadUnusedAsset;
        private static readonly Dictionary<MeshData, Mesh> Meshes = new Dictionary<MeshData, Mesh>();
        private static readonly Dictionary<byte[], Texture2D> Textures = new Dictionary<byte[], Texture2D>();
        private static readonly Dictionary<string, Material> Materials = new Dictionary<string, Material>();

        public static void Clear()
        {
            foreach (var m in Meshes.Values) if (U.Alive(m)) UnityEngine.Object.Destroy(m);
            foreach (var t in Textures.Values) if (U.Alive(t)) UnityEngine.Object.Destroy(t);
            foreach (var m in Materials.Values) if (U.Alive(m)) UnityEngine.Object.Destroy(m);
            Meshes.Clear(); Textures.Clear(); Materials.Clear();
        }

        public static Mesh? GetMesh(MeshData data, string name)
        {
            if (Meshes.TryGetValue(data, out var cached) && U.Alive(cached)) return cached;
            try
            {
                var mesh = new Mesh();
                mesh.name = name;
                mesh.hideFlags = Keep;
                int n = data.Positions.Count;
                if (n > 65000) mesh.indexFormat = IndexFormat.UInt32;

                var verts = new Il2CppStructArray<UnityEngine.Vector3>(n);
                var norms = new Il2CppStructArray<UnityEngine.Vector3>(n);
                var uvs = new Il2CppStructArray<UnityEngine.Vector2>(n);
                var colors = new Il2CppStructArray<UnityEngine.Color>(n);
                for (int i = 0; i < n; i++)
                {
                    verts[i] = U.V(data.Positions[i]);
                    norms[i] = U.V(data.Normals[i]);
                    var uv = data.UVs[i];
                    uvs[i] = new UnityEngine.Vector2 { x = uv.X, y = uv.Y };
                    // always set colours: shaders that use vertex colour would otherwise draw black
                    colors[i] = U.C(i < data.Colors.Count ? data.Colors[i] : System.Numerics.Vector4.One);
                }
                var tris = new Il2CppStructArray<int>(data.Indices.Count);
                for (int i = 0; i < data.Indices.Count; i++) tris[i] = data.Indices[i];

                mesh.vertices = verts;
                mesh.normals = norms;
                mesh.uv = uvs;
                mesh.triangles = tris;
                try { mesh.colors = colors; } catch (Exception ex) { Log.WarnOnce("meshcolors", "Vertex colours not supported: " + ex.Message); }
                if (!data.HasNormals) mesh.RecalculateNormals();
                mesh.RecalculateBounds();
                Meshes[data] = mesh;
                return mesh;
            }
            catch (Exception ex)
            {
                Log.Error($"Could not build mesh '{name}': {ex}");
                return null;
            }
        }

        public static Texture2D? GetTexture(byte[]? bytes, string name)
        {
            if (bytes == null) return null;
            if (Textures.TryGetValue(bytes, out var cached) && U.Alive(cached)) return cached;
            try
            {
                var tex = new Texture2D(2, 2);
                tex.name = name;
                tex.hideFlags = Keep;
                if (!LoadImage(tex, bytes))
                {
                    Log.Warn($"Texture for '{name}' could not be decoded (use PNG or JPG).");
                    UnityEngine.Object.Destroy(tex);
                    return null;
                }
                Textures[bytes] = tex;
                return tex;
            }
            catch (Exception ex)
            {
                Log.Warn($"Texture for '{name}' failed: {ex.Message}");
                return null;
            }
        }

        private delegate bool LoadImageICall(IntPtr tex, IntPtr data, bool markNonReadable);
        private static LoadImageICall? _loadImageICall;

        private static bool LoadImage(Texture2D tex, byte[] bytes)
        {
            var arr = new Il2CppStructArray<byte>(bytes.Length);
            for (int i = 0; i < bytes.Length; i++) arr[i] = bytes[i];
            try { return LoadImageManaged(tex, arr); }
            catch (Exception ex) when (ex is MissingMethodException || ex is TypeLoadException || ex is System.IO.FileNotFoundException)
            {
                // The game's build may have stripped ImageConversion; call the engine function directly.
                _loadImageICall ??= IL2CPP.ResolveICall<LoadImageICall>("UnityEngine.ImageConversion::LoadImage(UnityEngine.Texture2D,System.Byte[],System.Boolean)");
                return _loadImageICall(tex.Pointer, arr.Pointer, false);
            }
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        private static bool LoadImageManaged(Texture2D tex, Il2CppStructArray<byte> arr) => ImageConversion.LoadImage(tex, arr, false);

        private static readonly string[] TextureProps = { "_MainTex", "_BaseMap", "_BaseColorMap", "_Albedo", "_AlbedoTex", "_Texture", "_Diffuse" };
        private static readonly string[] ColorProps = { "_Color", "_BaseColor", "_Tint", "_TintColor", "_MainColor" };

        /// <summary>
        /// Copies the game's own material (so lighting/outline style match) and swaps in the custom texture.
        /// Without a texture, the model is drawn with a plain tint colour.
        /// </summary>
        public static Material? GetMaterial(string key, Material? source, Texture2D? texture, NV4 tint)
        {
            var cacheKey = key + "|" + (U.Alive(source) ? source.GetInstanceID().ToString() : "none");
            if (Materials.TryGetValue(cacheKey, out var cached) && U.Alive(cached)) return cached;
            try
            {
                Material mat;
                if (U.Alive(source)) mat = new Material(source);
                else
                {
                    var shader = Shader.Find("Standard") ?? Shader.Find("Universal Render Pipeline/Lit") ?? Shader.Find("Unlit/Texture");
                    if (!U.Alive(shader)) return null;
                    mat = new Material(shader);
                }
                mat.name = "CW_" + key;
                mat.hideFlags = Keep;
                var tex = texture ?? Texture2D.whiteTexture;
                try { mat.mainTexture = tex; } catch { }
                foreach (var p in TextureProps) try { if (mat.HasProperty(p)) mat.SetTexture(p, tex); } catch { }
                var color = U.C(tint);
                try { mat.color = color; } catch { }
                foreach (var p in ColorProps) try { if (mat.HasProperty(p)) mat.SetColor(p, color); } catch { }
                Materials[cacheKey] = mat;
                return mat;
            }
            catch (Exception ex)
            {
                Log.Warn($"Material for '{key}' failed: {ex.Message}");
                return null;
            }
        }

        /// <summary>Adds a child object that renders <paramref name="mesh"/> with <paramref name="material"/>.</summary>
        public static GameObject? AddModel(Transform parent, string name, Mesh mesh, Material? material)
        {
            var go = new GameObject(name);
            go.transform.SetParent(parent, false);
            go.layer = parent.gameObject.layer;
            var mf = go.AddComponent(U.T<MeshFilter>()).Cast<MeshFilter>();
            mf.sharedMesh = mesh;
            var mr = go.AddComponent(U.T<MeshRenderer>()).Cast<MeshRenderer>();
            if (material != null) mr.sharedMaterial = material;
            return go;
        }
    }
}
