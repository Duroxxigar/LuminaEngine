#pragma once

#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "Core/Math/Vector/VectorTypes.h"
#include "World/ECS/Entity.h"
#include "ProjectileLibrary.generated.h"

namespace Lumina
{
    class CWorld;

    /** Spawning for the projectile system, which is gameplay rather than anything the world itself owns. */
    REFLECT()
    class RUNTIME_API CProjectileLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        /** A Lifetime above zero despawns the projectile through the engine lifetime system. */
        FUNCTION()
        static ECS::FEntity SpawnProjectile(CWorld* World, FVector3 Position, FVector3 Velocity, float Damage,
            float Lifetime, ECS::FEntity Instigator);
    };
}
