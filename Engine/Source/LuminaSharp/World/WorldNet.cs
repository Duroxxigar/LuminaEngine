namespace Lumina;

// Handwritten role shorthands over the reflected CWorld net accessors.
public unsafe partial class CWorld
{
    /// True on the authority, so a listen or dedicated server.
    public bool IsServer => IsNetServer();

    /// True on a connected client.
    public bool IsClient => GetNetMode() == ENetMode.Client;

    /// True when the world is not networked at all.
    public bool IsStandalone => GetNetMode() == ENetMode.Standalone;

    /// True when running as either a client or a server.
    public bool IsNetworked => GetNetMode() != ENetMode.Standalone;
}
