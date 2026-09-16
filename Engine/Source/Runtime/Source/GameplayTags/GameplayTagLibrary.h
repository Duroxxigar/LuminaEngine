#pragma once

#include "Containers/String.h"
#include "Containers/Vector.h"
#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "World/ECS/Entity.h"
#include "GameplayTagLibrary.generated.h"

namespace Lumina
{
    class CWorld;

    /** Tag queries against an entity's tag component, which is the tag registry's business. */
    REFLECT()
    class RUNTIME_API CGameplayTagLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        /** The registered tag ids the entity carries, empty when it has no tag component. */
        FUNCTION()
        static void GetTags(CWorld* World, ECS::FEntity Entity, TVector<uint32>& Out);

        //~ The process-global registry, where an id interns a dotted name and all of its ancestors.

        /** Interns the name and its ancestors and returns its id. Zero for an empty name. */
        FUNCTION()
        static uint32 RequestTag(const FString& Name);

        /** The dotted name behind an id, empty when the id names nothing. */
        FUNCTION()
        static FString GetTagName(uint32 TagId);

        /** Hierarchical, so A matches B when A is B or a descendant of it. */
        FUNCTION()
        static bool Matches(uint32 A, uint32 B);

        FUNCTION()
        static bool MatchesExact(uint32 A, uint32 B);

        /** The immediate parent id, or zero at the root. */
        FUNCTION()
        static uint32 GetParent(uint32 TagId);

        FUNCTION()
        static bool IsValidTag(uint32 TagId);

        //~ Per entity, stored on the tag component that Add creates.

        FUNCTION()
        static void AddTag(CWorld* World, ECS::FEntity Entity, uint32 TagId);

        FUNCTION()
        static void RemoveTag(CWorld* World, ECS::FEntity Entity, uint32 TagId);

        /** Hierarchical, so an entity tagged with a leaf answers true for a query on its parent. */
        FUNCTION()
        static bool HasTag(CWorld* World, ECS::FEntity Entity, uint32 TagId);

        FUNCTION()
        static bool HasTagExact(CWorld* World, ECS::FEntity Entity, uint32 TagId);

        FUNCTION()
        static void ClearTags(CWorld* World, ECS::FEntity Entity);
    };
}
