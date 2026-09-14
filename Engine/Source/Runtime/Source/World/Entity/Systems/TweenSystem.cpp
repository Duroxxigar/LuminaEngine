#include "RuntimePCH.h"
#include "TweenSystem.h"

#include "World/Subsystems/TweenManager.h"

namespace Lumina
{
    void STweenSystem::Configure()
    {
        RequireUpdate(EUpdateStage::FrameStart, EUpdatePriority::Highest);
    }

    void STweenSystem::OnUpdate()
    {
        const FSystemContext& Context = GetContext();

        LUMINA_PROFILE_SCOPE();

        ECS::FRegistry& Registry = Context.GetRegistry();

        FTweenManager& Tweens = Registry.Ctx().Get<FTweenManager>();
        Tweens.SetWorldRegistry(&Registry);

        Tweens.Tick((float)Context.GetDeltaTime());
    }
}
