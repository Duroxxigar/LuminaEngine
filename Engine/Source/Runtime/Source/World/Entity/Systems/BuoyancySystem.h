#pragma once

#include "EntitySystem.h"
#include "Core/Object/ObjectMacros.h"
#include "BuoyancySystem.generated.h"

namespace Lumina
{
    REFLECT()
    class RUNTIME_API SBuoyancySystem : public CEntitySystem
    {
        GENERATED_BODY()
    public:

        void Configure() override;
        

        void OnStartup() override;
        void OnUpdate() override;
    };
}
