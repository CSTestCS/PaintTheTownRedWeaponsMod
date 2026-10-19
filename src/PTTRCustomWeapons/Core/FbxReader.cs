using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Text;

namespace PTTRCustomWeapons.Core
{
    /// <summary>Reads binary (6.1 - 7.7) and ASCII (7.x) FBX files into an <see cref="FbxDocument"/>.</summary>
    public static class FbxReader
    {
        private static readonly byte[] BinaryMagic = Encoding.ASCII.GetBytes("Kaydara FBX Binary");

        public static FbxDocument Load(string path) => Load(File.ReadAllBytes(path));

        public static FbxDocument Load(byte[] data)
        {
            if (data.Length >= BinaryMagic.Length && StartsWith(data, BinaryMagic))
                return ReadBinary(data);
            return ReadAscii(Encoding.UTF8.GetString(data));
        }

        private static bool StartsWith(byte[] data, byte[] prefix)
        {
            for (int i = 0; i < prefix.Length; i++) if (data[i] != prefix[i]) return false;
            return true;
        }

        // ---------------------------------------------------------------- binary

        private static FbxDocument ReadBinary(byte[] data)
        {
            var doc = new FbxDocument { IsBinary = true };
            using var ms = new MemoryStream(data, false);
            using var br = new BinaryReader(ms);
            ms.Position = 23;
            doc.Version = br.ReadInt32();
            bool wide = doc.Version >= 7500;
            while (ms.Position < ms.Length)
            {
                var node = ReadBinaryNode(br, wide);
                if (node == null) break;
                doc.Root.Children.Add(node);
            }
            return doc;
        }

        private static FbxNode? ReadBinaryNode(BinaryReader br, bool wide)
        {
            long endOffset = wide ? (long)br.ReadUInt64() : br.ReadUInt32();
            long numProps = wide ? (long)br.ReadUInt64() : br.ReadUInt32();
            long propListLen = wide ? (long)br.ReadUInt64() : br.ReadUInt32();
            int nameLen = br.ReadByte();
            if (endOffset == 0 && numProps == 0 && propListLen == 0 && nameLen == 0) return null; // null record

            var node = new FbxNode { Name = Encoding.ASCII.GetString(br.ReadBytes(nameLen)) };
            for (long i = 0; i < numProps; i++) node.Properties.Add(ReadBinaryProperty(br));

            var stream = br.BaseStream;
            while (stream.Position < endOffset)
            {
                var child = ReadBinaryNode(br, wide);
                if (child == null) break;
                node.Children.Add(child);
            }
            stream.Position = endOffset;
            return node;
        }

        private static object ReadBinaryProperty(BinaryReader br)
        {
            char type = (char)br.ReadByte();
            switch (type)
            {
                case 'Y': return (long)br.ReadInt16();
                case 'C': return br.ReadByte() != 0;
                case 'I': return (long)br.ReadInt32();
                case 'F': return (double)br.ReadSingle();
                case 'D': return br.ReadDouble();
                case 'L': return br.ReadInt64();
                case 'S':
                {
                    int len = br.ReadInt32();
                    return Encoding.UTF8.GetString(br.ReadBytes(len));
                }
                case 'R':
                {
                    int len = br.ReadInt32();
                    return br.ReadBytes(len);
                }
                case 'f': return ReadArray(br, 4, (b, n) => { var a = new float[n]; Buffer.BlockCopy(b, 0, a, 0, n * 4); return a; });
                case 'd': return ReadArray(br, 8, (b, n) => { var a = new double[n]; Buffer.BlockCopy(b, 0, a, 0, n * 8); return a; });
                case 'l': return ReadArray(br, 8, (b, n) => { var a = new long[n]; Buffer.BlockCopy(b, 0, a, 0, n * 8); return a; });
                case 'i': return ReadArray(br, 4, (b, n) => { var a = new int[n]; Buffer.BlockCopy(b, 0, a, 0, n * 4); return a; });
                case 'b': return ReadArray(br, 1, (b, n) => { var a = new bool[n]; for (int i = 0; i < n; i++) a[i] = b[i] != 0; return a; });
                default:
                    throw new InvalidDataException($"Unknown FBX property type '{type}' at offset {br.BaseStream.Position - 1}");
            }
        }

        private static object ReadArray(BinaryReader br, int elemSize, Func<byte[], int, object> convert)
        {
            int length = br.ReadInt32();
            int encoding = br.ReadInt32();
            int compressedLength = br.ReadInt32();
            byte[] raw = br.ReadBytes(compressedLength);
            byte[] bytes;
            if (encoding == 1)
            {
                bytes = new byte[length * elemSize];
                using var input = new MemoryStream(raw);
                using var z = new ZLibStream(input, CompressionMode.Decompress);
                int read = 0;
                while (read < bytes.Length)
                {
                    int r = z.Read(bytes, read, bytes.Length - read);
                    if (r <= 0) break;
                    read += r;
                }
            }
            else
            {
                bytes = raw;
            }
            return convert(bytes, length);
        }

        // ---------------------------------------------------------------- ascii

        private enum Tok { Name, Value, Comma, Open, Close, NewLine, Star, End }

        private sealed class AsciiLexer
        {
            private readonly string _s;
            private int _i;
            public Tok Kind;
            public string Text = "";
            public object? Value;

            public AsciiLexer(string s) { _s = s; Next(); }

            public void Next()
            {
                while (_i < _s.Length)
                {
                    char c = _s[_i];
                    if (c == ';') { while (_i < _s.Length && _s[_i] != '\n') _i++; continue; }
                    if (c == '\n') { _i++; Kind = Tok.NewLine; return; }
                    if (char.IsWhiteSpace(c)) { _i++; continue; }
                    break;
                }
                if (_i >= _s.Length) { Kind = Tok.End; return; }
                char ch = _s[_i];
                switch (ch)
                {
                    case ',': _i++; Kind = Tok.Comma; return;
                    case '{': _i++; Kind = Tok.Open; return;
                    case '}': _i++; Kind = Tok.Close; return;
                    case '*': _i++; ReadNumberOrWord(); Kind = Tok.Star; return;
                    case '"':
                    {
                        int start = ++_i;
                        while (_i < _s.Length && _s[_i] != '"') _i++;
                        Text = _s.Substring(start, _i - start);
                        Value = Text;
                        _i++;
                        Kind = Tok.Value;
                        return;
                    }
                }
                ReadNumberOrWord();
                if (_i < _s.Length && _s[_i] == ':')
                {
                    _i++;
                    Kind = Tok.Name;
                    return;
                }
                Kind = Tok.Value;
                if (long.TryParse(Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var l)) Value = l;
                else if (double.TryParse(Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var d)) Value = d;
                else Value = Text; // bare words such as T, Y, W
            }

            private void ReadNumberOrWord()
            {
                int start = _i;
                while (_i < _s.Length)
                {
                    char c = _s[_i];
                    if (char.IsWhiteSpace(c) || c == ',' || c == '{' || c == '}' || c == ':' || c == '"' || c == ';') break;
                    _i++;
                }
                Text = _s.Substring(start, _i - start);
            }
        }

        private static FbxDocument ReadAscii(string text)
        {
            var doc = new FbxDocument { IsBinary = false, Version = 7400 };
            var lex = new AsciiLexer(text);
            ParseAsciiChildren(lex, doc.Root, topLevel: true);
            var header = doc.Root.Child("FBXHeaderExtension")?.Child("FBXVersion");
            if (header != null) doc.Version = (int)header.PropLong(0);
            return doc;
        }

        private static void ParseAsciiChildren(AsciiLexer lex, FbxNode parent, bool topLevel)
        {
            while (true)
            {
                switch (lex.Kind)
                {
                    case Tok.End:
                        if (!topLevel) throw new InvalidDataException("Unexpected end of ASCII FBX file");
                        return;
                    case Tok.Close:
                        if (topLevel) throw new InvalidDataException("Unbalanced '}' in ASCII FBX file");
                        lex.Next();
                        return;
                    case Tok.Name:
                        parent.Children.Add(ParseAsciiNode(lex));
                        break;
                    default:
                        lex.Next(); // stray tokens / blank lines
                        break;
                }
            }
        }

        private static FbxNode ParseAsciiNode(AsciiLexer lex)
        {
            var node = new FbxNode { Name = lex.Text };
            lex.Next();
            bool isArray = false;
            // properties
            while (true)
            {
                if (lex.Kind == Tok.Value) { node.Properties.Add(lex.Value!); lex.Next(); }
                else if (lex.Kind == Tok.Star) { isArray = true; lex.Next(); }
                if (lex.Kind == Tok.Comma)
                {
                    lex.Next();
                    while (lex.Kind == Tok.NewLine) lex.Next(); // values may continue on the next line
                    continue;
                }
                break;
            }
            if (lex.Kind == Tok.Open)
            {
                lex.Next();
                ParseAsciiChildren(lex, node, topLevel: false);
            }
            else if (lex.Kind == Tok.NewLine)
            {
                lex.Next();
            }

            // "Vertices: *24 { a: 1,2,3 }" -> flatten the "a" child into a typed array property.
            if (isArray)
            {
                var a = node.Child("a");
                node.Children.Clear();
                node.Properties.Clear();
                if (a != null)
                {
                    bool allInts = true;
                    foreach (var p in a.Properties) if (!(p is long)) { allInts = false; break; }
                    if (allInts)
                    {
                        var arr = new long[a.Properties.Count];
                        for (int i = 0; i < arr.Length; i++) arr[i] = (long)a.Properties[i];
                        node.Properties.Add(arr);
                    }
                    else
                    {
                        node.Properties.Add(a.AsDoubleArray());
                    }
                }
            }
            return node;
        }
    }
}
