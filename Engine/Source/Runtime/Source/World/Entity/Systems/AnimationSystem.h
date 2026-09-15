#pragma once
#include "EntitySystem.h"
#include "Core/Object/ObjectMacros.h"
#include "AnimationSystem.generated.h"

namespace Lumina
{
    REFLECT()
    class SAnimationSystem : public CEntitySystem
    {
        GENERATED_BODY()
    public:

        void Configure() override;

    public:

        // Union of both passes' access: writes the skeletal pose + (root motion) transforms + Lua VM
        // (anim notifies); reads the simple-anim / graph / blackboard components. Defined in the .cpp.

        void OnUpdate() override;
    };
}
