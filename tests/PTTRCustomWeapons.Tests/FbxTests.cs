using System;
using System.Linq;
using System.Numerics;
using System.Text;
using PTTRCustomWeapons.Core;
using PTTRCustomWeapons.Tools;
using Xunit;

namespace PTTRCustomWeapons.Tests
{
    public class FbxTests
    {
        private static SimpleMesh UnitCube() =>
            new SimpleMesh().Box(new Vector3(0, 0.5f, 0), new Vector3(1, 1, 2), new Vector4(0, 0, 1, 1));

        [Fact]
        public void Binary_cube_roundtrips_with_unity_conversion()
        {
            var bytes = FbxBinaryWriter.BuildMeshFile("Cube", UnitCube());
            var mesh = FbxMeshExtractor.Extract(FbxReader.Load(bytes));

            Assert.Empty(mesh.Warnings);
            Assert.Equal(12, mesh.TriangleCount);
            Assert.True(mesh.HasNormals);
            Assert.True(mesh.HasUVs);
            var (min, max) = mesh.Bounds();
            AssertNear(new Vector3(-0.5f, 0, -1), min);
            AssertNear(new Vector3(0.5f, 1, 1), max);
        }

        [Fact]
        public void Triangles_face_outward_after_mirroring()
        {
            var mesh = FbxMeshExtractor.Extract(FbxReader.Load(FbxBinaryWriter.BuildMeshFile("Cube", UnitCube())));
            var center = new Vector3(0, 0.5f, 0);
            for (int t = 0; t < mesh.Indices.Count; t += 3)
            {
                var a = mesh.Positions[mesh.Indices[t]];
                var b = mesh.Positions[mesh.Indices[t + 1]];
                var c = mesh.Positions[mesh.Indices[t + 2]];
                // Unity's front face is clockwise; with Unity's component formula the
                // outward normal of such a triangle is Cross(b - a, c - a).
                var geometric = Vector3.Cross(b - a, c - a);
                var outward = (a + b + c) / 3 - center;
                Assert.True(Vector3.Dot(geometric, outward) > 0, $"triangle {t / 3} faces inward");
                // stored normals agree with the geometry
                Assert.True(Vector3.Dot(mesh.Normals[mesh.Indices[t]], outward) > 0);
            }
        }

        [Fact]
        public void Node_rotation_is_applied()
        {
            // rotate 90 degrees about X: the 2m long Z axis becomes the Y axis
            var bytes = FbxBinaryWriter.BuildMeshFile("Cube", UnitCube(), new Vector3(90, 0, 0));
            var (min, max) = FbxMeshExtractor.Extract(FbxReader.Load(bytes)).Bounds();
            var size = max - min;
            Assert.Equal(1f, size.X, 3);
            Assert.Equal(2f, size.Y, 3);
            Assert.Equal(1f, size.Z, 3);
        }

        [Fact]
        public void Embedded_texture_is_extracted()
        {
            var png = new byte[] { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 1, 2, 3, 4 };
            var mesh = FbxMeshExtractor.Extract(FbxReader.Load(FbxBinaryWriter.BuildMeshFile("Cube", UnitCube(), null, png)));
            Assert.Equal(png, mesh.EmbeddedTexture);
        }

        [Fact]
        public void Compressed_arrays_are_read()
        {
            // write a node with a zlib-compressed double array by hand
            var values = new double[] { 1.5, -2, 3.25 };
            byte[] raw = new byte[values.Length * 8];
            Buffer.BlockCopy(values, 0, raw, 0, raw.Length);
            byte[] compressed;
            using (var ms = new System.IO.MemoryStream())
            {
                using (var z = new System.IO.Compression.ZLibStream(ms, System.IO.Compression.CompressionLevel.Optimal, true)) z.Write(raw);
                compressed = ms.ToArray();
            }

            var body = new System.IO.MemoryStream();
            var bw = new System.IO.BinaryWriter(body);
            bw.Write(Encoding.ASCII.GetBytes("Kaydara FBX Binary  \0"));
            bw.Write((byte)0x1A); bw.Write((byte)0); bw.Write(7500);
            long start = body.Position;
            bw.Write(0UL); bw.Write(1UL); bw.Write((ulong)(13 + compressed.Length));
            bw.Write((byte)4); bw.Write(Encoding.ASCII.GetBytes("Data"));
            bw.Write((byte)'d'); bw.Write(values.Length); bw.Write(1); bw.Write(compressed.Length); bw.Write(compressed);
            long end = body.Position;
            body.Position = start; bw.Write((ulong)end); body.Position = end;
            bw.Write(new byte[25]);

            var doc = FbxReader.Load(body.ToArray());
            Assert.Equal(7500, doc.Version);
            Assert.Equal(values, doc.Root.Child("Data")!.AsDoubleArray());
        }

        [Fact]
        public void Ascii_fbx_quad_with_indexed_uvs()
        {
            const string ascii = @"; FBX 7.4.0 project file
FBXHeaderExtension:  {
	FBXHeaderVersion: 1003
	FBXVersion: 7400
}
GlobalSettings:  {
	Version: 1000
	Properties70:  {
		P: ""UpAxis"", ""int"", ""Integer"", """",2
		P: ""UpAxisSign"", ""int"", ""Integer"", """",1
		P: ""FrontAxis"", ""int"", ""Integer"", """",1
		P: ""FrontAxisSign"", ""int"", ""Integer"", """",-1
		P: ""CoordAxis"", ""int"", ""Integer"", """",0
		P: ""CoordAxisSign"", ""int"", ""Integer"", """",1
		P: ""UnitScaleFactor"", ""double"", ""Number"", """",100
	}
}
Objects:  {
	Geometry: 140, ""Geometry::Plane"", ""Mesh"" {
		Vertices: *12 {
			a: 0,0,0,1,0,0,
			1,0,3,0,0,3
		}
		PolygonVertexIndex: *4 {
			a: 0,1,2,-4
		}
		LayerElementUV: 0 {
			MappingInformationType: ""ByPolygonVertex""
			ReferenceInformationType: ""IndexToDirect""
			UV: *8 {
				a: 0,0,1,0,1,1,0,1
			}
			UVIndex: *4 {
				a: 0,1,2,3
			}
		}
	}
	Model: 150, ""Model::Plane"", ""Mesh"" {
		Version: 232
		Properties70:  {
			P: ""Lcl Translation"", ""Lcl Translation"", """", ""A"",0,0,1
		}
	}
}
Connections:  {
	C: ""OO"",140,150
	C: ""OO"",150,0
}";
            var mesh = FbxMeshExtractor.Extract(FbxReader.Load(Encoding.UTF8.GetBytes(ascii)));
            Assert.Equal(2, mesh.TriangleCount);
            Assert.False(mesh.HasNormals);
            Assert.True(mesh.HasUVs);
            var (min, max) = mesh.Bounds();
            // Z-up file: z (0..3, +1 translation) becomes Unity Y (1..4); X is mirrored (0..1 -> -1..0)
            AssertNear(new Vector3(-1, 1, 0), min);
            AssertNear(new Vector3(0, 4, 0), max);
            Assert.Contains(new Vector2(1, 1), mesh.UVs);
        }

        [Fact]
        public void Object_names_are_cleaned()
        {
            Assert.Equal("Sword", FbxNode.CleanName("Sword\0\x01Model"));
            Assert.Equal("Sword", FbxNode.CleanName("Model::Sword"));
        }

        private static void AssertNear(Vector3 expected, Vector3 actual, float eps = 1e-4f)
        {
            Assert.True(Vector3.Distance(expected, actual) < eps, $"expected {expected} but got {actual}");
        }
    }
}
