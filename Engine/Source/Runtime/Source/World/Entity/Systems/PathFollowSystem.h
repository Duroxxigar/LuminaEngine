#pragma once

#include "EntitySystem.h"
#include "Core/Object/ObjectMacros.h"
#include "PathFollowSystem.generated.h"

namespace Lumina
{
    // Ticks every SPathFollowComponent: refreshes the cached path (Nav::FindPath), advances the corner cursor,
    // and writes a move direction into the paired controller. In PrePhysics so input lands the same step.
    REFLECT()
    class RUNTIME_API SPathFollowSystem : public CEntitySystem
    {
        GENERATED_BODY()
    public:

        void Configure() override;

        void OnUpdate() override;
    };
}
