namespace LuminaSharp;

// A service owned by one world, and the place world scoped gameplay data belongs.
public abstract class WorldSubsystem : Lumina.CWorldSubsystem
{
    // Valid from OnInitialize onwards, read from the native object that owns the value for both languages.
    public Lumina.CWorld World => GetWorld();

    public EntityRegistry Registry => World.Registry;

    // ShouldCreate, OnInitialize, OnWorldReady, OnUpdate and OnTeardown are inherited [ScriptEvent] virtuals.
}
