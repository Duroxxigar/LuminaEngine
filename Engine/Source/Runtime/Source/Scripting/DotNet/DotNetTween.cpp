#include "Platform/GenericPlatform.h"
#include "Containers/HashTable.h"
#include "Containers/Vector.h"
#include "Core/Math/Math.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "Memory/SmartPtr.h"
#include "Scripting/DotNet/DotNetExport.h"
#include "Scripting/ScriptCallback.h"
#include "World/ECS/Registry.h"
#include "World/Subsystems/TweenManager.h"
#include "World/World.h"

using namespace Lumina;
using namespace Lumina::DotNet;

namespace
{
    // Shared with whatever step captured it, so the managed handle frees when the tween is destroyed.
    using FTweenCallback = TSharedPtr<FScriptCallbackOwner>;

    FTweenCallback MakeCallback(uint64 Token)
    {
        return MakeShared<FScriptCallbackOwner>(FScriptCallback{ Token });
    }

    FTweenManager* ManagerOf(uint64 World)
    {
        CWorld* W = AsWorld(World);
        return W != nullptr ? &W->GetTweenManager() : nullptr;
    }

    // Rebuilt per call, since the builder is just a manager pointer plus a generational id.
    bool ResolveTween(uint64 World, uint32 Id, FTween& Out)
    {
        FTweenManager* Manager = ManagerOf(World);
        if (Manager == nullptr)
        {
            return false;
        }

        FTweenHandle Handle;
        Handle.Handle = ECS::FEntity::FromPacked(Id);
        Out = FTween(Manager, Handle);
        return true;
    }
}

LUMINA_DOTNET_EXPORT(uint32, Tween_Create)(uint64 World, uint32 OwnerEntity, int32 bHasOwner)
{
    FTweenManager* Manager = ManagerOf(World);
    if (Manager == nullptr)
    {
        return ToId(ECS::NullEntity);
    }

    const FTween Tween = (bHasOwner != 0)
        ? Manager->CreateForEntity(ECS::FEntity::FromPacked(OwnerEntity))
        : Manager->Create();

    return ToId(Tween.GetHandle().Handle);
}

LUMINA_DOTNET_EXPORT(void, Tween_MoveTo)(uint64 World, uint32 Id, uint32 Entity, FVector3 Target, float Duration)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.MoveTo(ECS::FEntity::FromPacked(Entity), Target, Duration);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_RotateTo)(uint64 World, uint32 Id, uint32 Entity, FQuat Target, float Duration)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.RotateTo(ECS::FEntity::FromPacked(Entity), Target, Duration);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_ScaleTo)(uint64 World, uint32 Id, uint32 Entity, FVector3 Target, float Duration)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.ScaleTo(ECS::FEntity::FromPacked(Entity), Target, Duration);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_ValueTo)(uint64 World, uint32 Id, float From, float To, float Duration, uint64 Callback)
{
    FTween Tween;
    if (!ResolveTween(World, Id, Tween))
    {
        Scripting::ReleaseScriptCallback(FScriptCallback{ Callback });
        return;
    }

    FTweenCallback Owner = MakeCallback(Callback);
    Tween.To(From, To, Duration, [Owner](const float& Value)
    {
        Owner->Invoke(Scripting::PackFloatPayload(Value));
    });
}

LUMINA_DOTNET_EXPORT(void, Tween_Interval)(uint64 World, uint32 Id, float Duration)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.Interval(Duration);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_Call)(uint64 World, uint32 Id, uint64 Callback)
{
    FTween Tween;
    if (!ResolveTween(World, Id, Tween))
    {
        Scripting::ReleaseScriptCallback(FScriptCallback{ Callback });
        return;
    }

    FTweenCallback Owner = MakeCallback(Callback);
    Tween.Call([Owner] { Owner->Invoke(0); });
}

LUMINA_DOTNET_EXPORT(void, Tween_OnFinished)(uint64 World, uint32 Id, uint64 Callback)
{
    FTween Tween;
    if (!ResolveTween(World, Id, Tween))
    {
        Scripting::ReleaseScriptCallback(FScriptCallback{ Callback });
        return;
    }

    FTweenCallback Owner = MakeCallback(Callback);
    Tween.OnFinished([Owner] { Owner->Invoke(0); });
}

LUMINA_DOTNET_EXPORT(void, Tween_Trans)(uint64 World, uint32 Id, int32 Transition)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.Trans((EEaseTransition)Transition);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_Ease)(uint64 World, uint32 Id, int32 Ease)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.Ease((EEaseType)Ease);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_Delay)(uint64 World, uint32 Id, float Seconds)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.Delay(Seconds);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_Parallel)(uint64 World, uint32 Id)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.Parallel();
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_SetLoops)(uint64 World, uint32 Id, int32 Count)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.SetLoops(Count);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_SetSpeedScale)(uint64 World, uint32 Id, float Scale)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.SetSpeedScale(Scale);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_SetPaused)(uint64 World, uint32 Id, int32 bPaused)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.SetPaused(bPaused != 0);
    }
}

LUMINA_DOTNET_EXPORT(void, Tween_Kill)(uint64 World, uint32 Id)
{
    FTween Tween;
    if (ResolveTween(World, Id, Tween))
    {
        Tween.Kill();
    }
}

LUMINA_DOTNET_EXPORT(int32, Tween_IsRunning)(uint64 World, uint32 Id)
{
    FTween Tween;
    return (ResolveTween(World, Id, Tween) && Tween.IsRunning()) ? 1 : 0;
}
