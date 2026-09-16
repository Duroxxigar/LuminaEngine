using System;
using Lumina;

namespace LuminaSharp;

/// A scoped rigid-body creation batch. Every body created while it is open is inserted into the broadphase in one pass when it is disposed, instead of one insert per body. Bodies do not exist until the scope closes, so their body id, velocity and mass are only valid after it. Game thread only, and it nests.
public readonly struct FPhysicsBatchScope : IDisposable
{
    private readonly CWorld World;

    public FPhysicsBatchScope(CWorld World)
    {
        this.World = World;
        CPhysicsLibrary.BeginBodyBatch(World);
    }

    public void Dispose()
    {
        if (World != null)
        {
            CPhysicsLibrary.EndBodyBatch(World);
        }
    }
}
