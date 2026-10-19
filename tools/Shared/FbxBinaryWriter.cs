using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Text;

namespace PTTRCustomWeapons.Tools
{
    /// <summary>Minimal binary FBX 7.4 writer for static meshes (used for tests and example models).</summary>
    public sealed class FbxOut
    {
        public string Name;
        public List<object> Props = new List<object>();
        public List<FbxOut> Children = new List<FbxOut>();

        public FbxOut(string name, params object[] props) { Name = name; Props.AddRange(props); }
        public FbxOut Add(FbxOut child) { Children.Add(child); return this; }
        public FbxOut Add(string name, params object[] props) { Children.Add(new FbxOut(name, props)); return this; }
    }

    public static class FbxBinaryWriter
    {
        public static byte[] Write(IEnumerable<FbxOut> topLevel, int version = 7400)
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);
            bw.Write(Encoding.ASCII.GetBytes("Kaydara FBX Binary  \0"));
            bw.Write((byte)0x1A);
            bw.Write((byte)0x00);
            bw.Write(version);
            foreach (var n in topLevel) WriteNode(bw, n);
            bw.Write(new byte[13]); // top-level null record

            // footer
            bw.Write(new byte[] { 0xfa, 0xbc, 0xab, 0x09, 0xd0, 0xc8, 0xd4, 0x66, 0xb1, 0x76, 0xfb, 0x83, 0x1c, 0xf7, 0x26, 0x7e });
            bw.Write(new byte[4]);
            int pad = (int)((16 - (ms.Position % 16)) % 16);
            if (pad == 0) pad = 16;
            bw.Write(new byte[pad]);
            bw.Write(version);
            bw.Write(new byte[120]);
            bw.Write(new byte[] { 0xf8, 0x5a, 0x8c, 0x6a, 0xde, 0xf5, 0xd9, 0x7e, 0xec, 0xe9, 0x0c, 0xe3, 0x75, 0x8f, 0x29, 0x0b });
            bw.Flush();
            return ms.ToArray();
        }

        private static void WriteNode(BinaryWriter bw, FbxOut node)
        {
            var s = bw.BaseStream;
            long start = s.Position;
            bw.Write(0u); bw.Write((uint)node.Props.Count); bw.Write(0u);
            var name = Encoding.ASCII.GetBytes(node.Name);
            bw.Write((byte)name.Length);
            bw.Write(name);
            long propStart = s.Position;
            foreach (var p in node.Props) WriteProp(bw, p);
            long propLen = s.Position - propStart;
            if (node.Children.Count > 0)
            {
                foreach (var c in node.Children) WriteNode(bw, c);
                bw.Write(new byte[13]);
            }
            long end = s.Position;
            s.Position = start;
            bw.Write((uint)end);
            bw.Write((uint)node.Props.Count);
            bw.Write((uint)propLen);
            s.Position = end;
        }

        private static void WriteProp(BinaryWriter bw, object p)
        {
            switch (p)
            {
                case bool b: bw.Write((byte)'C'); bw.Write((byte)(b ? 1 : 0)); break;
                case short sh: bw.Write((byte)'Y'); bw.Write(sh); break;
                case int i: bw.Write((byte)'I'); bw.Write(i); break;
                case long l: bw.Write((byte)'L'); bw.Write(l); break;
                case float f: bw.Write((byte)'F'); bw.Write(f); break;
                case double d: bw.Write((byte)'D'); bw.Write(d); break;
                case string str:
                {
                    var bytes = Encoding.UTF8.GetBytes(str);
                    bw.Write((byte)'S'); bw.Write(bytes.Length); bw.Write(bytes); break;
                }
                case byte[] raw: bw.Write((byte)'R'); bw.Write(raw.Length); bw.Write(raw); break;
                case double[] da:
                    bw.Write((byte)'d'); bw.Write(da.Length); bw.Write(0); bw.Write(da.Length * 8);
                    foreach (var x in da) bw.Write(x);
                    break;
                case int[] ia:
                    bw.Write((byte)'i'); bw.Write(ia.Length); bw.Write(0); bw.Write(ia.Length * 4);
                    foreach (var x in ia) bw.Write(x);
                    break;
                default: throw new ArgumentException("Unsupported FBX property " + p.GetType());
            }
        }

        /// <summary>FBX object name: "Name\0\x01Class".</summary>
        public static string ObjName(string name, string cls) => name + "\0\x01" + cls;

        public static FbxOut P(string name, string type, string label, string flags, params object[] values)
        {
            var n = new FbxOut("P", name, type, label, flags);
            n.Props.AddRange(values);
            return n;
        }

        /// <summary>Builds a whole FBX file for one mesh (Y-up, metres).</summary>
        public static byte[] BuildMeshFile(string name, SimpleMesh mesh, Vector3? rotationDeg = null, byte[]? embeddedPng = null)
        {
            long geoId = 1000, modelId = 2000, matId = 3000, texId = 4000, vidId = 5000;

            var header = new FbxOut("FBXHeaderExtension")
                .Add("FBXHeaderVersion", 1003)
                .Add("FBXVersion", 7400)
                .Add("Creator", "PTTRCustomWeapons ExampleGen");

            var gs = new FbxOut("GlobalSettings").Add("Version", 1000);
            var gsp = new FbxOut("Properties70");
            gsp.Add(P("UpAxis", "int", "Integer", "", 1));
            gsp.Add(P("UpAxisSign", "int", "Integer", "", 1));
            gsp.Add(P("FrontAxis", "int", "Integer", "", 2));
            gsp.Add(P("FrontAxisSign", "int", "Integer", "", 1));
            gsp.Add(P("CoordAxis", "int", "Integer", "", 0));
            gsp.Add(P("CoordAxisSign", "int", "Integer", "", 1));
            gsp.Add(P("OriginalUpAxis", "int", "Integer", "", 1));
            gsp.Add(P("OriginalUpAxisSign", "int", "Integer", "", 1));
            gsp.Add(P("UnitScaleFactor", "double", "Number", "", 100.0));
            gsp.Add(P("OriginalUnitScaleFactor", "double", "Number", "", 100.0));
            gs.Add(gsp);

            var defs = new FbxOut("Definitions").Add("Version", 100).Add("Count", embeddedPng != null ? 5 : 3);
            defs.Add(new FbxOut("ObjectType", "GlobalSettings").Add("Count", 1));
            defs.Add(new FbxOut("ObjectType", "Model").Add("Count", 1));
            defs.Add(new FbxOut("ObjectType", "Geometry").Add("Count", 1));
            if (embeddedPng != null)
            {
                defs.Add(new FbxOut("ObjectType", "Material").Add("Count", 1));
                defs.Add(new FbxOut("ObjectType", "Texture").Add("Count", 1));
                defs.Add(new FbxOut("ObjectType", "Video").Add("Count", 1));
            }

            // geometry (polygons are triangles; last index of each is negated: ~i)
            var verts = new double[mesh.Positions.Count * 3];
            for (int i = 0; i < mesh.Positions.Count; i++)
            {
                verts[i * 3] = mesh.Positions[i].X; verts[i * 3 + 1] = mesh.Positions[i].Y; verts[i * 3 + 2] = mesh.Positions[i].Z;
            }
            var poly = new int[mesh.Triangles.Count];
            var normals = new double[mesh.Triangles.Count * 3];
            var uvs = new double[mesh.Triangles.Count * 2];
            var uvIndex = new int[mesh.Triangles.Count];
            for (int i = 0; i < mesh.Triangles.Count; i++)
            {
                int v = mesh.Triangles[i];
                poly[i] = (i % 3 == 2) ? ~v : v;
                var n = mesh.Normals[v];
                normals[i * 3] = n.X; normals[i * 3 + 1] = n.Y; normals[i * 3 + 2] = n.Z;
                uvs[i * 2] = mesh.UVs[v].X; uvs[i * 2 + 1] = mesh.UVs[v].Y;
                uvIndex[i] = i;
            }

            var geo = new FbxOut("Geometry", geoId, ObjName(name, "Geometry"), "Mesh")
                .Add(new FbxOut("Properties70"))
                .Add("GeometryVersion", 124)
                .Add("Vertices", verts)
                .Add("PolygonVertexIndex", poly);
            geo.Add(new FbxOut("LayerElementNormal", 0)
                .Add("Version", 102).Add("Name", "")
                .Add("MappingInformationType", "ByPolygonVertex")
                .Add("ReferenceInformationType", "Direct")
                .Add("Normals", normals));
            geo.Add(new FbxOut("LayerElementUV", 0)
                .Add("Version", 101).Add("Name", "UVMap")
                .Add("MappingInformationType", "ByPolygonVertex")
                .Add("ReferenceInformationType", "IndexToDirect")
                .Add("UV", uvs).Add("UVIndex", uvIndex));
            geo.Add(new FbxOut("Layer", 0).Add("Version", 100)
                .Add(new FbxOut("LayerElement").Add("Type", "LayerElementNormal").Add("TypedIndex", 0))
                .Add(new FbxOut("LayerElement").Add("Type", "LayerElementUV").Add("TypedIndex", 0)));

            var mp = new FbxOut("Properties70");
            var rot = rotationDeg ?? Vector3.Zero;
            mp.Add(P("Lcl Rotation", "Lcl Rotation", "", "A", (double)rot.X, (double)rot.Y, (double)rot.Z));
            mp.Add(P("Lcl Scaling", "Lcl Scaling", "", "A", 1.0, 1.0, 1.0));
            var model = new FbxOut("Model", modelId, ObjName(name, "Model"), "Mesh")
                .Add("Version", 232).Add(mp).Add("Shading", true).Add("Culling", "CullingOff");

            var objects = new FbxOut("Objects").Add(geo).Add(model);
            var conns = new FbxOut("Connections")
                .Add("C", "OO", geoId, modelId)
                .Add("C", "OO", modelId, 0L);

            if (embeddedPng != null)
            {
                var mat = new FbxOut("Material", matId, ObjName(name + "_Mat", "Material"), "")
                    .Add("Version", 102).Add("ShadingModel", "phong").Add("MultiLayer", 0)
                    .Add(new FbxOut("Properties70"));
                var video = new FbxOut("Video", vidId, ObjName(name + "_Tex", "Video"), "Clip")
                    .Add("Type", "Clip").Add("RelativeFilename", name + ".png").Add("Content", embeddedPng);
                var tex = new FbxOut("Texture", texId, ObjName(name + "_Tex", "Texture"), "")
                    .Add("Type", "TextureVideoClip").Add("Version", 202)
                    .Add("TextureName", ObjName(name + "_Tex", "Texture"))
                    .Add("RelativeFilename", name + ".png");
                objects.Add(mat).Add(video).Add(tex);
                conns.Add("C", "OO", matId, modelId)
                     .Add("C", "OO", vidId, texId)
                     .Add(new FbxOut("C", "OP", texId, matId, "DiffuseColor"));
            }

            return Write(new[] { header, gs, defs, objects, conns });
        }
    }

    /// <summary>Tiny mesh builder for boxes / cylinders with flat normals (Y-up, right-handed, metres).</summary>
    public sealed class SimpleMesh
    {
        public readonly List<Vector3> Positions = new List<Vector3>();
        public readonly List<Vector3> Normals = new List<Vector3>();
        public readonly List<Vector2> UVs = new List<Vector2>();
        public readonly List<int> Triangles = new List<int>();

        private void Quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector2 uv0, Vector2 uv1)
        {
            var n = Vector3.Normalize(Vector3.Cross(b - a, c - a)); // counter-clockwise = front (right-handed)
            int i = Positions.Count;
            Positions.AddRange(new[] { a, b, c, d });
            Normals.AddRange(new[] { n, n, n, n });
            UVs.AddRange(new[] { uv0, new Vector2(uv1.X, uv0.Y), uv1, new Vector2(uv0.X, uv1.Y) });
            Triangles.AddRange(new[] { i, i + 1, i + 2, i, i + 2, i + 3 });
        }

        /// <summary>Axis-aligned box; uvRect = (u0, v0, u1, v1) region of the texture.</summary>
        public SimpleMesh Box(Vector3 center, Vector3 size, Vector4 uvRect)
        {
            var h = size / 2;
            Vector3 P(float x, float y, float z) => center + new Vector3(x * h.X, y * h.Y, z * h.Z);
            var u0 = new Vector2(uvRect.X, uvRect.Y);
            var u1 = new Vector2(uvRect.Z, uvRect.W);
            Quad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), u0, u1);     // +Z
            Quad(P(1, -1, -1), P(-1, -1, -1), P(-1, 1, -1), P(1, 1, -1), u0, u1); // -Z
            Quad(P(1, -1, 1), P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), u0, u1);     // +X
            Quad(P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), u0, u1); // -X
            Quad(P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), P(-1, 1, -1), u0, u1);     // +Y
            Quad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), u0, u1); // -Y
            return this;
        }

        /// <summary>Cylinder along +Z.</summary>
        public SimpleMesh CylinderZ(Vector3 center, float radius, float length, int segments, Vector4 uvRect)
        {
            var u0 = new Vector2(uvRect.X, uvRect.Y);
            var u1 = new Vector2(uvRect.Z, uvRect.W);
            float hz = length / 2;
            for (int s = 0; s < segments; s++)
            {
                float a0 = MathF.PI * 2 * s / segments, a1 = MathF.PI * 2 * (s + 1) / segments;
                var p0 = new Vector3(MathF.Cos(a0) * radius, MathF.Sin(a0) * radius, 0);
                var p1 = new Vector3(MathF.Cos(a1) * radius, MathF.Sin(a1) * radius, 0);
                Quad(center + p0 + new Vector3(0, 0, -hz), center + p1 + new Vector3(0, 0, -hz),
                     center + p1 + new Vector3(0, 0, hz), center + p0 + new Vector3(0, 0, hz), u0, u1);
                // caps
                AddTri(center + new Vector3(0, 0, hz), center + p0 + new Vector3(0, 0, hz), center + p1 + new Vector3(0, 0, hz), u0);
                AddTri(center + new Vector3(0, 0, -hz), center + p1 + new Vector3(0, 0, -hz), center + p0 + new Vector3(0, 0, -hz), u0);
            }
            return this;
        }

        private void AddTri(Vector3 a, Vector3 b, Vector3 c, Vector2 uv)
        {
            var n = Vector3.Normalize(Vector3.Cross(b - a, c - a));
            int i = Positions.Count;
            Positions.AddRange(new[] { a, b, c });
            Normals.AddRange(new[] { n, n, n });
            UVs.AddRange(new[] { uv, uv, uv });
            Triangles.AddRange(new[] { i, i + 1, i + 2 });
        }
    }
}
