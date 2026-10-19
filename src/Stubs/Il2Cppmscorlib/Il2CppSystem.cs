using System;
using Il2CppInterop.Runtime.InteropTypes;
[assembly: System.Reflection.AssemblyVersion("4.0.0.0")]

namespace Il2CppSystem
{
    public class Object : Il2CppObjectBase
    {
        public Object(IntPtr pointer) : base(pointer) { }
    }

    public class Type : Object
    {
        public Type(IntPtr pointer) : base(pointer) { }
        public virtual string FullName => throw null;
    }
}
