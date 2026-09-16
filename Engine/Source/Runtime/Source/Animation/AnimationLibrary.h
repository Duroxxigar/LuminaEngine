#pragma once

#include "Containers/Name.h"
#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "World/ECS/Entity.h"
#include "AnimationLibrary.generated.h"

namespace Lumina
{
    class CAnimation;
    class CAnimationMontage;
    class CWorld;

    /** Playback for both animation backends. Every call but Play is a no-op when the component is absent. */
    REFLECT()
    class RUNTIME_API CAnimationLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        //~ The simple single clip backend, SSimpleAnimationComponent.

        /** Adds the component if the entity has none, which is why this is the one call that cannot no-op. */
        FUNCTION()
        static void Play(CWorld* World, ECS::FEntity Entity, CAnimation* Clip, bool bLoop = false,
            float Speed = 1.0f);

        FUNCTION()
        static void Stop(CWorld* World, ECS::FEntity Entity);

        FUNCTION()
        static void Pause(CWorld* World, ECS::FEntity Entity);

        FUNCTION()
        static void Resume(CWorld* World, ECS::FEntity Entity);

        FUNCTION()
        static bool IsPlaying(CWorld* World, ECS::FEntity Entity);

        FUNCTION()
        static bool IsFinished(CWorld* World, ECS::FEntity Entity);

        FUNCTION()
        static void SetSpeed(CWorld* World, ECS::FEntity Entity, float Speed);

        FUNCTION()
        static void SetTime(CWorld* World, ECS::FEntity Entity, float Time);

        FUNCTION()
        static float GetTime(CWorld* World, ECS::FEntity Entity);

        //~ Graph parameters, which live as fields on the graph's own parameter struct.

        FUNCTION()
        static void SetFloat(CWorld* World, ECS::FEntity Entity, const FName& Name, float Value);

        FUNCTION()
        static float GetFloat(CWorld* World, ECS::FEntity Entity, const FName& Name, float Default = 0.0f);

        FUNCTION()
        static void SetBool(CWorld* World, ECS::FEntity Entity, const FName& Name, bool bValue);

        FUNCTION()
        static bool GetBool(CWorld* World, ECS::FEntity Entity, const FName& Name, bool bDefault = false);

        FUNCTION()
        static bool HasParameter(CWorld* World, ECS::FEntity Entity, const FName& Name);

        //~ Montages. The graph must contain a matching slot node for anything to show.

        /** Zero when it did not start. An empty section starts at the montage's first one. */
        FUNCTION()
        static uint32 PlayMontage(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage,
            float PlayRate = 1.0f, const FName& Section = FName());

        /** A null montage stops every one playing on the entity. */
        FUNCTION()
        static void StopMontage(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage,
            float BlendOutTime = -1.0f);

        FUNCTION()
        static bool JumpToMontageSection(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage,
            const FName& Section);

        FUNCTION()
        static bool SetNextMontageSection(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage,
            const FName& Section);

        /** A null montage asks whether any montage is playing at all. */
        FUNCTION()
        static bool IsMontagePlaying(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage);

        FUNCTION()
        static float GetMontagePosition(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage);

        FUNCTION()
        static float GetMontageWeight(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage);

        FUNCTION()
        static void SetMontagePlayRate(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage,
            float PlayRate);

        FUNCTION()
        static FName GetMontageSection(CWorld* World, ECS::FEntity Entity, CAnimationMontage* Montage);
    };
}
