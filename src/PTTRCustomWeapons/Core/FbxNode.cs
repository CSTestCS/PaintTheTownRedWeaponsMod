using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace PTTRCustomWeapons.Core
{
    /// <summary>One node of an FBX document tree (same shape for binary and ASCII files).</summary>
    public sealed class FbxNode
    {
        public string Name = "";
        public readonly List<object> Properties = new List<object>();
        public readonly List<FbxNode> Children = new List<FbxNode>();

        public FbxNode? Child(string name) => Children.FirstOrDefault(c => c.Name == name);
        public IEnumerable<FbxNode> ChildrenNamed(string name) => Children.Where(c => c.Name == name);

        public object? Prop(int i) => i < Properties.Count ? Properties[i] : null;

        public string PropString(int i) => Properties.Count > i ? Properties[i]?.ToString() ?? "" : "";

        public long PropLong(int i)
        {
            var p = Prop(i);
            return p switch
            {
                long l => l,
                int n => n,
                short s => s,
                double d => (long)d,
                float f => (long)f,
                bool b => b ? 1 : 0,
                string str when long.TryParse(str, NumberStyles.Integer, CultureInfo.InvariantCulture, out var v) => v,
                _ => 0
            };
        }

        public double PropDouble(int i)
        {
            var p = Prop(i);
            return p switch
            {
                double d => d,
                float f => f,
                long l => l,
                int n => n,
                short s => s,
                bool b => b ? 1 : 0,
                string str when double.TryParse(str, NumberStyles.Float, CultureInfo.InvariantCulture, out var v) => v,
                _ => 0
            };
        }

        /// <summary>Returns the first property as a double array (works for typed arrays and ASCII lists).</summary>
        public double[] AsDoubleArray()
        {
            if (Properties.Count == 1)
            {
                switch (Properties[0])
                {
                    case double[] d: return d;
                    case float[] f: return Array.ConvertAll(f, x => (double)x);
                    case int[] n: return Array.ConvertAll(n, x => (double)x);
                    case long[] l: return Array.ConvertAll(l, x => (double)x);
                }
            }
            var res = new double[Properties.Count];
            for (int i = 0; i < res.Length; i++) res[i] = PropDouble(i);
            return res;
        }

        public int[] AsIntArray()
        {
            if (Properties.Count == 1)
            {
                switch (Properties[0])
                {
                    case int[] n: return n;
                    case long[] l: return Array.ConvertAll(l, x => (int)x);
                    case double[] d: return Array.ConvertAll(d, x => (int)x);
                    case float[] f: return Array.ConvertAll(f, x => (int)x);
                }
            }
            var res = new int[Properties.Count];
            for (int i = 0; i < res.Length; i++) res[i] = (int)PropLong(i);
            return res;
        }

        /// <summary>
        /// Object names are stored as "Name\0\x01Class" in binary files and "Class::Name" in ASCII files.
        /// This returns just the name part.
        /// </summary>
        public static string CleanName(string raw)
        {
            int sep = raw.IndexOf("\0\x01", StringComparison.Ordinal);
            if (sep >= 0) return raw.Substring(0, sep);
            int colons = raw.IndexOf("::", StringComparison.Ordinal);
            if (colons >= 0) return raw.Substring(colons + 2);
            return raw;
        }

        /// <summary>Reads a Properties70 block into name -> values.</summary>
        public Dictionary<string, FbxNode> ReadProperties70()
        {
            var dict = new Dictionary<string, FbxNode>(StringComparer.Ordinal);
            var p70 = Child("Properties70");
            if (p70 == null) return dict;
            foreach (var p in p70.ChildrenNamed("P"))
            {
                var key = p.PropString(0);
                if (key.Length > 0) dict[key] = p;
            }
            return dict;
        }
    }

    public sealed class FbxDocument
    {
        public readonly FbxNode Root = new FbxNode { Name = "<root>" };
        public int Version;
        public bool IsBinary;
    }
}
