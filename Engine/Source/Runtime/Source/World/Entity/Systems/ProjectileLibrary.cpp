#include "RuntimePCH.h"

#include "ProjectileLibrary.h"

#include "World/ECS/Registry.h"
#include "World/Entity/Components/LifetimeComponent.h"
#include "World/Entity/Components/ProjectileComponent.h"
#include "World/World.h"

namespace Lumina
{
    ECS::FEntity CProjectileLibrary::SpawnProjectile(CWorld* World, FVector3 Position, FVector3 Velocity,
        float Damage, float Lifetime, ECS::FEntity Instigator)
    {
        if (World == nullptr)
        {
            return ECS::NullEntity;
        }

        FTransform SpawnTransform;
        SpawnTransform.SetLocation(Position);
        const ECS::FEntity Entity = World->ConstructEntity("Projectile", SpawnTransform);

        ECS::FRegistry& Registry = ECS::GetWorldRegistry(*World);
        SProjectileComponent& Projectile = Registry.Emplace<SProjectileComponent>(Entity);
        Projectile.Velocity = Velocity;
        Projectile.Damage = Damage;
        Projectile.Instigator = Instigator;

        if (Lifetime > 0.0f)
        {
            Registry.Emplace<SLifetimeComponent>(Entity).Lifetime = Lifetime;
        }
        return Entity;
    }
}
