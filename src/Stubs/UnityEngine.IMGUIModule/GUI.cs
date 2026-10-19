// Reference-only stand-in. Bodies are never executed.
namespace UnityEngine
{
    public class GUI
    {
        public static Color color { get => throw null; set => throw null; }
        public static void Label(Rect position, string text) => throw null;
    }
}
