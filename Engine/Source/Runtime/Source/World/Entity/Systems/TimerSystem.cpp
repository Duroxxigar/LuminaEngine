#include "RuntimePCH.h"
#include "TimerSystem.h"
#include "World/Subsystems/TimerManager.h"

namespace Lumina
{
    void STimerSystem::Configure()
    {
        RequireUpdate(EUpdateStage::FrameStart, EUpdatePriority::Highest);
    }

    void STimerSystem::OnUpdate()
    {
        const FSystemContext& Context = GetContext();

        LUMINA_PROFILE_SCOPE();
        Context.GetRegistry().Ctx().Get<FTimerManager>().Tick((float)Context.GetDeltaTime());
    }
}
