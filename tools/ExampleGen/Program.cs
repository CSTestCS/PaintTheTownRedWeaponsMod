using System;
using System.IO;
using System.IO.Compression;
using System.Numerics;
using PTTRCustomWeapons.Tools;

// Builds the example weapons shipped in CustomWeapons/: low-poly models made of boxes and
// cylinders, textured from a small colour-palette image (each part samples one palette cell).

var outDir = args.Length > 0 ? args[0] : Path.Combine(FindRepoRoot(), "CustomWeapons");
Directory.CreateDirectory(outDir);

// 4x4 palette, 16x16 pixels per cell
var palette = new (byte r, byte g, byte b)[]
{
    (30, 30, 34),   (70, 70, 78),   (150, 150, 160), (225, 225, 235), // 0-3 greys
    (200, 30, 40),  (255, 120, 20), (255, 210, 40),  (60, 200, 90),   // 4-7 red orange yellow green
    (40, 160, 255), (140, 60, 230), (255, 80, 200),  (120, 75, 40),   // 8-11 blue purple pink brown
    (180, 130, 70), (20, 90, 40),   (0, 255, 240),   (255, 255, 255), // 12-15 tan darkgreen cyan white
};
var png = PaletteTexture(palette, 16);

Vector4 Cell(int i)
{
    float x = i % 4, y = 3 - i / 4; // v grows upward in the image
    const float pad = 0.3f;         // sample the middle of the cell to avoid colour bleeding
    return new Vector4((x + pad) / 4f, (y + pad) / 4f, (x + 1 - pad) / 4f, (y + 1 - pad) / 4f);
}

// ---- Example Blaster (ranged, automatic, bouncing energy balls) -----------------------
var blaster = new SimpleMesh()
    .Box(new Vector3(0, 0.05f, 0.05f), new Vector3(0.07f, 0.09f, 0.32f), Cell(1))        // body
    .Box(new Vector3(0, 0.105f, 0.02f), new Vector3(0.03f, 0.025f, 0.12f), Cell(0))      // sight rail
    .Box(new Vector3(0, -0.06f, -0.06f), new Vector3(0.05f, 0.14f, 0.06f), Cell(11))     // grip
    .Box(new Vector3(0, 0.0f, 0.05f), new Vector3(0.075f, 0.02f, 0.2f), Cell(14))        // glowing strip
    .Box(new Vector3(0, -0.025f, 0.08f), new Vector3(0.04f, 0.05f, 0.05f), Cell(9))      // energy cell
    .CylinderZ(new Vector3(0, 0.06f, 0.29f), 0.022f, 0.18f, 10, Cell(2))                 // barrel
    .CylinderZ(new Vector3(0, 0.06f, 0.385f), 0.03f, 0.02f, 10, Cell(14));               // muzzle ring
Write("ExampleBlaster", blaster, embedTexture: false);
File.WriteAllBytes(Path.Combine(outDir, "ExampleBlaster.png"), png);

// ---- Example Greatsword (melee) ---------------------------------------------------------
var sword = new SimpleMesh()
    .Box(new Vector3(0, 0.62f, 0), new Vector3(0.09f, 0.95f, 0.018f), Cell(3))           // blade
    .Box(new Vector3(0, 0.62f, 0), new Vector3(0.02f, 0.9f, 0.022f), Cell(2))            // fuller
    .Box(new Vector3(0, 1.12f, 0), new Vector3(0.05f, 0.06f, 0.016f), Cell(3))           // tip
    .Box(new Vector3(0, 0.13f, 0), new Vector3(0.26f, 0.035f, 0.05f), Cell(6))           // guard
    .Box(new Vector3(0, -0.02f, 0), new Vector3(0.035f, 0.26f, 0.035f), Cell(4))         // handle wrap
    .Box(new Vector3(0, -0.17f, 0), new Vector3(0.06f, 0.05f, 0.06f), Cell(6));          // pommel
// this one carries its texture inside the .fbx, to show embedded textures work too
Write("ExampleGreatsword", sword, embedTexture: true);

// ---- Example Rocket Launcher (ranged, explosive, drop) ---------------------------------
var launcher = new SimpleMesh()
    .CylinderZ(new Vector3(0, 0.09f, 0.1f), 0.065f, 0.95f, 14, Cell(13))                 // tube
    .CylinderZ(new Vector3(0, 0.09f, 0.58f), 0.075f, 0.05f, 14, Cell(0))                 // front ring
    .CylinderZ(new Vector3(0, 0.09f, -0.38f), 0.08f, 0.06f, 14, Cell(0))                 // back ring
    .Box(new Vector3(0, -0.03f, 0.0f), new Vector3(0.045f, 0.13f, 0.05f), Cell(1))       // grip
    .Box(new Vector3(0, -0.03f, 0.22f), new Vector3(0.04f, 0.12f, 0.045f), Cell(1))      // front grip
    .Box(new Vector3(0.08f, 0.13f, 0.12f), new Vector3(0.03f, 0.05f, 0.12f), Cell(5))    // sight
    .Box(new Vector3(0, 0.165f, 0.1f), new Vector3(0.02f, 0.02f, 0.5f), Cell(4));        // stripe
Write("ExampleRocketLauncher", launcher, embedTexture: false);
File.WriteAllBytes(Path.Combine(outDir, "ExampleRocketLauncher.png"), png);

// the rocket the launcher fires
var rocket = new SimpleMesh()
    .CylinderZ(new Vector3(0, 0, 0), 0.04f, 0.3f, 10, Cell(2))
    .CylinderZ(new Vector3(0, 0, 0.17f), 0.025f, 0.05f, 8, Cell(4))
    .Box(new Vector3(0, 0, -0.13f), new Vector3(0.14f, 0.01f, 0.06f), Cell(4))
    .Box(new Vector3(0, 0, -0.13f), new Vector3(0.01f, 0.14f, 0.06f), Cell(4));
Write("ExampleRocket", rocket, embedTexture: true);

Console.WriteLine("Examples written to " + outDir);

void Write(string name, SimpleMesh mesh, bool embedTexture)
{
    var path = Path.Combine(outDir, name + ".fbx");
    File.WriteAllBytes(path, FbxBinaryWriter.BuildMeshFile(name, mesh, null, embedTexture ? png : null));
    Console.WriteLine($"  {name}.fbx  ({mesh.Triangles.Count / 3} triangles)");
}

static string FindRepoRoot()
{
    var dir = AppContext.BaseDirectory;
    while (dir != null && !File.Exists(Path.Combine(dir, "README.md"))) dir = Path.GetDirectoryName(dir);
    return dir ?? Directory.GetCurrentDirectory();
}

static byte[] PaletteTexture((byte r, byte g, byte b)[] colors, int cell)
{
    int size = cell * 4;
    var raw = new byte[size * (size * 4 + 1)];
    for (int y = 0; y < size; y++)
    {
        int row = y * (size * 4 + 1);
        raw[row] = 0; // filter: none
        for (int x = 0; x < size; x++)
        {
            var c = colors[(y / cell) * 4 + x / cell];
            int o = row + 1 + x * 4;
            raw[o] = c.r; raw[o + 1] = c.g; raw[o + 2] = c.b; raw[o + 3] = 255;
        }
    }
    using var ms = new MemoryStream();
    ms.Write(new byte[] { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A });
    var ihdr = new byte[13];
    BigEndian(ihdr, 0, size); BigEndian(ihdr, 4, size);
    ihdr[8] = 8; ihdr[9] = 6; // 8-bit RGBA
    Chunk(ms, "IHDR", ihdr);
    using (var z = new MemoryStream())
    {
        using (var zs = new ZLibStream(z, CompressionLevel.Optimal, true)) zs.Write(raw);
        Chunk(ms, "IDAT", z.ToArray());
    }
    Chunk(ms, "IEND", Array.Empty<byte>());
    return ms.ToArray();
}

static void BigEndian(byte[] b, int o, int v) { b[o] = (byte)(v >> 24); b[o + 1] = (byte)(v >> 16); b[o + 2] = (byte)(v >> 8); b[o + 3] = (byte)v; }

static void Chunk(Stream s, string type, byte[] data)
{
    var len = new byte[4]; BigEndian(len, 0, data.Length);
    s.Write(len);
    var td = new byte[4 + data.Length];
    for (int i = 0; i < 4; i++) td[i] = (byte)type[i];
    Buffer.BlockCopy(data, 0, td, 4, data.Length);
    s.Write(td);
    var crc = new byte[4]; BigEndian(crc, 0, (int)Crc32(td));
    s.Write(crc);
}

static uint Crc32(byte[] data)
{
    uint c = 0xFFFFFFFF;
    foreach (var b in data)
    {
        c ^= b;
        for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320 ^ (c >> 1) : c >> 1;
    }
    return c ^ 0xFFFFFFFF;
}
