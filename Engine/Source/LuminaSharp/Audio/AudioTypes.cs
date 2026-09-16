namespace Lumina;

// Handwritten ergonomics over the reflected FAudioHandle.
public partial struct FAudioHandle
{
    // False for a sound that never started, whether from missing data, no device, or the voice cap.
    public bool IsValid => Generation != 0;
}

// Handwritten ergonomics over the reflected FAudioPlayParams.
public partial struct FAudioPlayParams
{
    // Reads the engine's own defaults rather than restating them, so the two cannot drift.
    public static FAudioPlayParams Default() => CAudioLibrary.DefaultPlayParams();
}
