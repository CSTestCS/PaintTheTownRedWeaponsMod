using System;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Text.Json;
using PTTRCustomWeapons.Core;
using Xunit;

namespace PTTRCustomWeapons.Tests
{
    /// <summary>
    /// Files exported by Blender 4.0 (rotated/scaled cube + a 7-sided cylinder parented to a rotated,
    /// scaled empty) with different export settings. Expected positions were taken from Blender's own
    /// world-space vertices converted the way Unity imports Blender files: (-x, z, -y).
    /// </summary>
    public class BlenderFixtureTests
    {
        [Theory]
        [InlineData("blender_default.fbx")]
        [InlineData("blender_zup.fbx")]       // Forward Y / Up Z export axes
        [InlineData("blender_scale_all.fbx")] // "FBX All" scale option (UnitScaleFactor 100)
        public void Blender_exports_match_blender_world_positions(string file)
        {
            var dir = Path.Combine(AppContext.BaseDirectory, "Fixtures");
            var json = JsonDocument.Parse(File.ReadAllText(Path.Combine(dir, "blender_expected.json"))).RootElement;
            var expected = json.GetProperty("expected").EnumerateArray()
                .Select(a => new Vector3(a[0].GetSingle(), a[1].GetSingle(), a[2].GetSingle())).ToList();

            var mesh = FbxMeshExtractor.Load(Path.Combine(dir, file));
            Assert.Equal(36, mesh.TriangleCount);
            Assert.True(mesh.HasUVs);
            foreach (var e in expected)
                Assert.True(mesh.Positions.Min(p => Vector3.Distance(p, e)) < 1e-3f, $"missing vertex {e}");
            foreach (var p in mesh.Positions)
                Assert.True(expected.Min(e => Vector3.Distance(p, e)) < 1e-3f, $"unexpected vertex {p}");
            // Unity front faces: Cross(b - a, c - a) agrees with the exported normals
            for (int t = 0; t < mesh.Indices.Count; t += 3)
            {
                var a = mesh.Positions[mesh.Indices[t]];
                var b = mesh.Positions[mesh.Indices[t + 1]];
                var c = mesh.Positions[mesh.Indices[t + 2]];
                Assert.True(Vector3.Dot(Vector3.Cross(b - a, c - a), mesh.Normals[mesh.Indices[t]]) > 0);
            }
        }
    }
}
