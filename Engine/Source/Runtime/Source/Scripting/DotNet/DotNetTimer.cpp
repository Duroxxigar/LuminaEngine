#include "Platform/GenericPlatform.h"
#include "World/ECS/Registry.h"
#include "Containers/HashTable.h"
#include "Containers/Vector.h"
#include "Core/Object/ObjectHandleTyped.h"
#include "Memory/SmartPtr.h"
#include "World/World.h"
#include "World/Subsystems/TimerManager.h"
#include "Scripting/DotNet/DotNetExport.h"
#include "Scripting/DotNet/ManagedContextRegistry.h"
#include "Scripting/ScriptCallback.h"

// The returned id is generational, so a stale one safely reports inactive after recycling.

using namespace Lumina;
using namespace Lumina::DotNet;

namespace
{
    struct FManagedTimer;

    using FTimerRegistry = TManagedContextRegistry<FManagedTimer>;

    // Tracked so a generation unload stops the timer itself, not just the callback it can no longer run.
    struct FManagedTimer
    {
        FScriptCallbackOwner   Callback;
        TWeakObjectPtr<CWorld> World;
        FTimerHandle           Handle;

        FManagedTimer(FScriptCallback InCallback, CWorld* InWorld)
            : Callback(InCallback)
            , World(InWorld)
        {
            FTimerRegistry::Add(this);
        }

        ~FManagedTimer() { FTimerRegistry::Remove(this); }

        LE_NO_COPYMOVE(FManagedTimer);
    };

    // Shared, since a looping timer moves its callback out and back on every fire.
    FTimerManager::FTimerCallback MakeCallback(const TSharedPtr<FManagedTimer>& Timer)
    {
        return [Timer]() { Timer->Callback.Invoke(0); };
    }

    uint32 SetManagedTimer(uint64 World, ECS::FEntity Owner, bool bHasOwner, float Rate, int32 bLoop,
        float FirstDelay, FScriptCallback Callback)
    {
        CWorld* W = AsWorld(World);
        if (W == nullptr || !Callback.IsBound())
        {
            Scripting::ReleaseScriptCallback(Callback);
            return ToId(ECS::NullEntity);
        }

        TSharedPtr<FManagedTimer> Timer = MakeShared<FManagedTimer>(Callback, W);

        const FTimerHandle Handle = bHasOwner
            ? W->GetTimerManager().SetTimerForEntity(Owner, Rate, MakeCallback(Timer), bLoop != 0, FirstDelay)
            : W->GetTimerManager().SetTimer(Rate, MakeCallback(Timer), bLoop != 0, FirstDelay);

        if (Handle.Handle == ECS::NullEntity)
        {
            return ToId(ECS::NullEntity);
        }

        Timer->Handle = Handle;
        return ToId(Handle.Handle);
    }
}

LUMINA_DOTNET_EXPORT(uint32, Timer_Set)(uint64 World, float Rate, int32 bLoop, float FirstDelay, uint64 Callback)
{
    return SetManagedTimer(World, ECS::NullEntity, false, Rate, bLoop, FirstDelay, FScriptCallback{ Callback });
}

// As the plain setter, but owned by an entity so the timer clears when that entity is destroyed.
LUMINA_DOTNET_EXPORT(uint32, Timer_SetForEntity)(uint64 World, uint32 Owner, float Rate, int32 bLoop, float FirstDelay, uint64 Callback)
{
    return SetManagedTimer(World, AsEntity(Owner), true, Rate, bLoop, FirstDelay, FScriptCallback{ Callback });
}

// Clears every managed timer before its generation unloads, so a looper does not tick on into the next one.
LUMINA_DOTNET_EXPORT(void, Timer_ClearAllManaged)()
{
    FTimerRegistry::ForEachSnapshot([](FManagedTimer* Timer)
    {
        if (CWorld* W = Timer->World.Get())
        {
            W->GetTimerManager().ClearTimer(Timer->Handle);
        }
    });
}

LUMINA_DOTNET_EXPORT(void, Timer_Clear)(uint64 World, uint32 Timer)
{
    CWorld* W = AsWorld(World);
    if (W != nullptr)
    {
        FTimerHandle Handle{ AsEntity(Timer) };
        W->GetTimerManager().ClearTimer(Handle);
    }
}

LUMINA_DOTNET_EXPORT(int32, Timer_IsActive)(uint64 World, uint32 Timer)
{
    CWorld* W = AsWorld(World);
    return (W != nullptr && W->GetTimerManager().IsTimerActive(FTimerHandle{ AsEntity(Timer) })) ? 1 : 0;
}

LUMINA_DOTNET_EXPORT(float, Timer_GetRemaining)(uint64 World, uint32 Timer)
{
    CWorld* W = AsWorld(World);
    return W != nullptr ? W->GetTimerManager().GetTimerRemaining(FTimerHandle{ AsEntity(Timer) }) : 0.0f;
}

LUMINA_DOTNET_EXPORT(float, Timer_GetElapsed)(uint64 World, uint32 Timer)
{
    CWorld* W = AsWorld(World);
    return W != nullptr ? W->GetTimerManager().GetTimerElapsed(FTimerHandle{ AsEntity(Timer) }) : 0.0f;
}

LUMINA_DOTNET_EXPORT(void, Timer_SetPaused)(uint64 World, uint32 Timer, int32 bPaused)
{
    CWorld* W = AsWorld(World);
    if (W != nullptr)
    {
        W->GetTimerManager().SetTimerPaused(FTimerHandle{ AsEntity(Timer) }, bPaused != 0);
    }
}
