#pragma once

#include "Memory/Memory.h"
#include "Platform/GenericPlatform.h"
#include "Object/ObjectMacros.h"
#include "UpdateStage.generated.h"

namespace Lumina
{
    REFLECT()
    enum class EUpdateStage : uint8
    {
        FrameStart,
        PrePhysics,
        DuringPhysics,
        PostPhysics,
        FrameEnd,
        Paused,
        Max,
    };
    
    constexpr const char* GUpdateStageNames[] = 
    {
        "FrameStart",
        "PrePhysics",
        "DuringPhysics",
        "PostPhysics",
        "FrameEnd",
        "Paused"
    };

    // Lower value = higher priority; systems sort ascending so Highest ticks first.
    // Disabled drops the system from the stage entirely.
    REFLECT()
    enum class EUpdatePriority : uint8
    {
        Highest     = 0,
        High        = 64,
        Medium      = 128,
        Low         = 192,
        Disabled    = 255,
        Default     = Medium,
    };

    // The stages one system ticks in, as a priority per stage. CEntitySystem::Configure fills it.
    struct FUpdatePriorityList
    {
        FUpdatePriorityList()
        {
            Reset();
        }

        void Reset()
        {
            Memory::Memset(Priorities, (uint8)EUpdatePriority::Disabled, sizeof(Priorities));
        }

        bool IsStageEnabled(EUpdateStage Stage) const
        {
            return Priorities[(uint8)Stage] != (uint8)EUpdatePriority::Disabled;
        }

        uint8 GetPriorityForStage(EUpdateStage Stage) const
        {
            return Priorities[(uint8)Stage];
        }

        void SetStagePriority(EUpdateStage Stage, EUpdatePriority Priority)
        {
            Priorities[(uint8)Stage] = (uint8)Priority;
        }

    private:

        uint8           Priorities[(uint8)EUpdateStage::Max];
    };
}
