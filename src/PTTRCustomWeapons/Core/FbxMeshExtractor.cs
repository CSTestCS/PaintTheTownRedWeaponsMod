using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;

namespace PTTRCustomWeapons.Core
{
    /// <summary>Plain mesh data in Unity's coordinate system (left-handed, Y up, metres).</summary>
    public sealed class MeshData
    {
        public readonly List<Vector3> Positions = new List<Vector3>();
        public readonly List<Vector3> Normals = new List<Vector3>();
        public readonly List<Vector2> UVs = new List<Vector2>();
        public readonly List<Vector4> Colors = new List<Vector4>();
        public readonly List<int> Indices = new List<int>();
        public bool HasNormals;
        public bool HasUVs;
        public bool HasColors;
        /// <summary>First texture found embedded inside the FBX (png/jpg bytes), if any.</summary>
        public byte[]? EmbeddedTexture;
        public readonly List<string> Warnings = new List<string>();

        public int TriangleCount => Indices.Count / 3;

        public (Vector3 min, Vector3 max) Bounds()
        {
            if (Positions.Count == 0) return (Vector3.Zero, Vector3.Zero);
            var min = new Vector3(float.MaxValue);
            var max = new Vector3(float.MinValue);
            foreach (var p in Positions) { min = Vector3.Min(min, p); max = Vector3.Max(max, p); }
            return (min, max);
        }
    }

    /// <summary>
    /// Turns an FBX document into a single combined static mesh. Every mesh object in the file is
    /// baked with its node transform, converted to Unity axes (same convention as Unity's own importer:
    /// convert to Y-up, then mirror X and flip triangle winding) and scaled to metres.
    /// Skinning, animation and multiple materials are ignored.
    /// </summary>
    public static class FbxMeshExtractor
    {
        public static MeshData Load(string path) => Extract(FbxReader.Load(path));

        public static MeshData Extract(FbxDocument doc)
        {
            var result = new MeshData();
            var objects = doc.Root.Child("Objects") ?? throw new InvalidOperationException("FBX file has no Objects section");

            var geometries = new Dictionary<long, FbxNode>();
            var models = new Dictionary<long, FbxNode>();
            foreach (var n in objects.Children)
            {
                if (n.Name == "Geometry" && n.PropString(2) == "Mesh") geometries[n.PropLong(0)] = n;
                else if (n.Name == "Model") models[n.PropLong(0)] = n;
                else if (n.Name == "Video" && result.EmbeddedTexture == null)
                {
                    if (n.Child("Content")?.Prop(0) is byte[] bytes && bytes.Length > 8) result.EmbeddedTexture = bytes;
                }
            }

            // connections: child -> parents
            var parents = new Dictionary<long, List<long>>();
            var conns = doc.Root.Child("Connections");
            if (conns != null)
            {
                foreach (var c in conns.ChildrenNamed("C"))
                {
                    if (c.PropString(0) != "OO") continue;
                    long child = c.PropLong(1), parent = c.PropLong(2);
                    if (!parents.TryGetValue(child, out var list)) parents[child] = list = new List<long>();
                    list.Add(parent);
                }
            }

            var axis = AxisConversion(doc);
            var globalCache = new Dictionary<long, Matrix4x4>();

            foreach (var kv in geometries)
            {
                var ownerModels = parents.TryGetValue(kv.Key, out var ps) ? ps.Where(models.ContainsKey).ToList() : new List<long>();
                if (ownerModels.Count == 0)
                {
                    AppendGeometry(result, kv.Value, Matrix4x4.Identity, axis);
                    continue;
                }
                foreach (var m in ownerModels)
                {
                    var model = models[m];
                    var geo = GeometricTransform(model);
                    var global = GlobalTransform(m, models, parents, globalCache, 0);
                    AppendGeometry(result, kv.Value, geo * global, axis);
                }
            }

            if (result.Positions.Count == 0) result.Warnings.Add("No mesh geometry found in FBX file.");
            return result;
        }

        // ------------------------------------------------------------ transforms (row-vector convention: v * M)

        private static Matrix4x4 GlobalTransform(long id, Dictionary<long, FbxNode> models, Dictionary<long, List<long>> parents,
            Dictionary<long, Matrix4x4> cache, int depth)
        {
            if (cache.TryGetValue(id, out var cached)) return cached;
            var local = LocalTransform(models[id]);
            var result = local;
            if (depth < 64 && parents.TryGetValue(id, out var ps))
            {
                foreach (var p in ps)
                {
                    if (models.ContainsKey(p))
                    {
                        result = local * GlobalTransform(p, models, parents, cache, depth + 1);
                        break;
                    }
                }
            }
            cache[id] = result;
            return result;
        }

        private static Vector3 P3(Dictionary<string, FbxNode> props, string name, Vector3 def)
        {
            if (!props.TryGetValue(name, out var p) || p.Properties.Count < 7) return def;
            return new Vector3((float)p.PropDouble(4), (float)p.PropDouble(5), (float)p.PropDouble(6));
        }

        private static Matrix4x4 EulerDeg(Vector3 deg, int order)
        {
            const float k = MathF.PI / 180f;
            var rx = Matrix4x4.CreateRotationX(deg.X * k);
            var ry = Matrix4x4.CreateRotationY(deg.Y * k);
            var rz = Matrix4x4.CreateRotationZ(deg.Z * k);
            // FBX rotation order enum: 0 XYZ, 1 XZY, 2 YZX, 3 YXZ, 4 ZXY, 5 ZYX. "XYZ" means X applied first.
            return order switch
            {
                1 => rx * rz * ry,
                2 => ry * rz * rx,
                3 => ry * rx * rz,
                4 => rz * rx * ry,
                5 => rz * ry * rx,
                _ => rx * ry * rz,
            };
        }

        private static Matrix4x4 LocalTransform(FbxNode model)
        {
            var props = model.ReadProperties70();
            int order = props.TryGetValue("RotationOrder", out var ro) ? (int)ro.PropLong(4) : 0;
            var t = P3(props, "Lcl Translation", Vector3.Zero);
            var r = P3(props, "Lcl Rotation", Vector3.Zero);
            var s = P3(props, "Lcl Scaling", Vector3.One);
            var pre = P3(props, "PreRotation", Vector3.Zero);
            var post = P3(props, "PostRotation", Vector3.Zero);
            var rotOffset = P3(props, "RotationOffset", Vector3.Zero);
            var rotPivot = P3(props, "RotationPivot", Vector3.Zero);
            var scaleOffset = P3(props, "ScalingOffset", Vector3.Zero);
            var scalePivot = P3(props, "ScalingPivot", Vector3.Zero);

            Matrix4x4.Invert(EulerDeg(post, 0), out var postInv);
            // FBX: T * Roff * Rp * Rpre * R * Rpost^-1 * Rp^-1 * Soff * Sp * S * Sp^-1 (column vectors)
            // Row-vector equivalent is the reverse product.
            return Matrix4x4.CreateTranslation(-scalePivot)
                   * Matrix4x4.CreateScale(s)
                   * Matrix4x4.CreateTranslation(scalePivot)
                   * Matrix4x4.CreateTranslation(scaleOffset)
                   * Matrix4x4.CreateTranslation(-rotPivot)
                   * postInv
                   * EulerDeg(r, order)
                   * EulerDeg(pre, 0)
                   * Matrix4x4.CreateTranslation(rotPivot)
                   * Matrix4x4.CreateTranslation(rotOffset)
                   * Matrix4x4.CreateTranslation(t);
        }

        private static Matrix4x4 GeometricTransform(FbxNode model)
        {
            var props = model.ReadProperties70();
            var t = P3(props, "GeometricTranslation", Vector3.Zero);
            var r = P3(props, "GeometricRotation", Vector3.Zero);
            var s = P3(props, "GeometricScaling", Vector3.One);
            return Matrix4x4.CreateScale(s) * EulerDeg(r, 0) * Matrix4x4.CreateTranslation(t);
        }

        /// <summary>Converts file axes to FBX's Y-up right-handed frame, then scales to metres.</summary>
        private static Matrix4x4 AxisConversion(FbxDocument doc)
        {
            var gs = doc.Root.Child("GlobalSettings")?.ReadProperties70() ?? new Dictionary<string, FbxNode>();
            int Get(string n, int def) => gs.TryGetValue(n, out var p) && p.Properties.Count > 4 ? (int)p.PropLong(4) : def;
            double unit = gs.TryGetValue("UnitScaleFactor", out var u) && u.Properties.Count > 4 ? u.PropDouble(4) : 1.0;
            if (unit <= 0) unit = 1.0;

            int upAxis = Get("UpAxis", 1), upSign = Get("UpAxisSign", 1);
            int frontAxis = Get("FrontAxis", 2), frontSign = Get("FrontAxisSign", 1);
            int coordAxis = Get("CoordAxis", 0), coordSign = Get("CoordAxisSign", 1);

            Vector3 Axis(int a, int sign)
            {
                var v = a switch { 0 => Vector3.UnitX, 1 => Vector3.UnitY, _ => Vector3.UnitZ };
                return sign < 0 ? -v : v;
            }
            var right = Axis(coordAxis, coordSign);
            var up = Axis(upAxis, upSign);
            var front = Axis(frontAxis, frontSign);
            if (upAxis == frontAxis || upAxis == coordAxis || frontAxis == coordAxis)
            {
                right = Vector3.UnitX; up = Vector3.UnitY; front = Vector3.UnitZ; // broken header, assume default
            }

            // new.x = dot(v, right), new.y = dot(v, up), new.z = dot(v, front)  (row-vector matrix, columns = axes)
            var m = new Matrix4x4(
                right.X, up.X, front.X, 0,
                right.Y, up.Y, front.Y, 0,
                right.Z, up.Z, front.Z, 0,
                0, 0, 0, 1);
            float metres = (float)(unit / 100.0);
            return m * Matrix4x4.CreateScale(metres);
        }

        // ------------------------------------------------------------ geometry

        private static void AppendGeometry(MeshData mesh, FbxNode geo, Matrix4x4 nodeToFile, Matrix4x4 axis)
        {
            var vertsNode = geo.Child("Vertices");
            var polyNode = geo.Child("PolygonVertexIndex");
            if (vertsNode == null || polyNode == null) return;

            var verts = vertsNode.AsDoubleArray();
            var poly = polyNode.AsIntArray();
            var full = nodeToFile * axis;
            Matrix4x4.Invert(full, out var inv);
            var normalMatrix = Matrix4x4.Transpose(inv);

            var normalLayer = ReadLayer(geo, "LayerElementNormal", "Normals", "NormalsIndex", 3);
            var uvLayer = ReadLayer(geo, "LayerElementUV", "UV", "UVIndex", 2);
            var colorLayer = ReadLayer(geo, "LayerElementColor", "Colors", "ColorIndex", 4);
            if (normalLayer != null) mesh.HasNormals = true;
            if (uvLayer != null) mesh.HasUVs = true;
            if (colorLayer != null) mesh.HasColors = true;

            int controlPoints = verts.Length / 3;
            var polygon = new List<int>(8);          // control point indices
            var polygonVertexIds = new List<int>(8); // polygon-vertex running index
            int polygonIndex = 0;

            for (int pv = 0; pv < poly.Length; pv++)
            {
                int idx = poly[pv];
                bool end = idx < 0;
                if (end) idx = ~idx;
                polygon.Add(idx);
                polygonVertexIds.Add(pv);
                if (!end) continue;

                if (polygon.Count >= 3)
                {
                    int baseIndex = mesh.Positions.Count;
                    for (int k = 0; k < polygon.Count; k++)
                    {
                        int cp = polygon[k];
                        if (cp < 0 || cp >= controlPoints)
                        {
                            mesh.Warnings.Add($"Invalid vertex index {cp}");
                            cp = 0;
                        }
                        var p = new Vector3((float)verts[cp * 3], (float)verts[cp * 3 + 1], (float)verts[cp * 3 + 2]);
                        p = Vector3.Transform(p, full);
                        p.X = -p.X; // mirror to Unity's left-handed space
                        mesh.Positions.Add(p);

                        if (normalLayer != null)
                        {
                            var nv = normalLayer.Get(cp, polygonVertexIds[k], polygonIndex);
                            var n = Vector3.TransformNormal(new Vector3(nv[0], nv[1], nv[2]), normalMatrix);
                            n.X = -n.X;
                            mesh.Normals.Add(n.LengthSquared() > 1e-12f ? Vector3.Normalize(n) : Vector3.UnitY);
                        }
                        else mesh.Normals.Add(Vector3.UnitY);

                        if (uvLayer != null)
                        {
                            var uv = uvLayer.Get(cp, polygonVertexIds[k], polygonIndex);
                            mesh.UVs.Add(new Vector2(uv[0], uv[1]));
                        }
                        else mesh.UVs.Add(Vector2.Zero);

                        if (colorLayer != null)
                        {
                            var c = colorLayer.Get(cp, polygonVertexIds[k], polygonIndex);
                            mesh.Colors.Add(new Vector4(c[0], c[1], c[2], c[3]));
                        }
                        else mesh.Colors.Add(Vector4.One);
                    }
                    // fan triangulation; winding flipped because of the X mirror
                    for (int k = 1; k + 1 < polygon.Count; k++)
                    {
                        mesh.Indices.Add(baseIndex);
                        mesh.Indices.Add(baseIndex + k + 1);
                        mesh.Indices.Add(baseIndex + k);
                    }
                }
                polygon.Clear();
                polygonVertexIds.Clear();
                polygonIndex++;
            }
        }

        private sealed class Layer
        {
            public string Mapping = "ByPolygonVertex";
            public bool Indexed;
            public double[] Data = Array.Empty<double>();
            public int[] Index = Array.Empty<int>();
            public int Stride;

            public float[] Get(int controlPoint, int polygonVertex, int polygon)
            {
                int i = Mapping switch
                {
                    "ByVertice" or "ByVertex" or "ByControlPoint" => controlPoint,
                    "ByPolygon" => polygon,
                    "AllSame" => 0,
                    _ => polygonVertex,
                };
                if (Indexed) i = i >= 0 && i < Index.Length ? Index[i] : 0;
                var res = new float[Stride];
                int b = i * Stride;
                if (i < 0 || b + Stride > Data.Length) return res;
                for (int k = 0; k < Stride; k++) res[k] = (float)Data[b + k];
                return res;
            }
        }

        private static Layer? ReadLayer(FbxNode geo, string elementName, string dataName, string indexName, int stride)
        {
            var el = geo.Child(elementName);
            var dataNode = el?.Child(dataName);
            if (el == null || dataNode == null) return null;
            var layer = new Layer
            {
                Stride = stride,
                Data = dataNode.AsDoubleArray(),
                Mapping = el.Child("MappingInformationType")?.PropString(0) ?? "ByPolygonVertex",
            };
            var reference = el.Child("ReferenceInformationType")?.PropString(0) ?? "Direct";
            var indexNode = el.Child(indexName) ?? (indexName == "NormalsIndex" ? el.Child("NormalIndex") : null);
            if ((reference == "IndexToDirect" || reference == "Index") && indexNode != null)
            {
                layer.Indexed = true;
                layer.Index = indexNode.AsIntArray();
            }
            return layer.Data.Length >= stride ? layer : null;
        }
    }
}
