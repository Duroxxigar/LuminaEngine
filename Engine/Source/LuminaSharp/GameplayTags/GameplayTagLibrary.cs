using LuminaSharp;

namespace Lumina;

// Handwritten ergonomics over the reflected CGameplayTagLibrary, interning a dotted name at the call.
public unsafe partial class CGameplayTagLibrary
{
    public static void AddTag(CWorld World, Entity Entity, string Tag)
        => AddTag(World, Entity, RequestTag(Tag));

    public static void RemoveTag(CWorld World, Entity Entity, string Tag)
        => RemoveTag(World, Entity, RequestTag(Tag));

    public static bool HasTag(CWorld World, Entity Entity, string Tag)
        => HasTag(World, Entity, RequestTag(Tag));

    public static bool HasTagExact(CWorld World, Entity Entity, string Tag)
        => HasTagExact(World, Entity, RequestTag(Tag));
}
