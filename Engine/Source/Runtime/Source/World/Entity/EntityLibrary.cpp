#include "RuntimePCH.h"

#include "EntityLibrary.h"

#include "World/Entity/Components/RelationshipComponent.h"
#include "World/World.h"

namespace Lumina
{
    namespace
    {
        const FRelationshipComponent* RelationshipOf(CWorld* World, ECS::FEntity Entity)
        {
            return World != nullptr ? World->TryGetComponent<FRelationshipComponent>(Entity) : nullptr;
        }
    }

    ECS::FEntity CEntityLibrary::GetFirstChild(CWorld* World, ECS::FEntity Entity)
    {
        const FRelationshipComponent* Relationship = RelationshipOf(World, Entity);
        return Relationship != nullptr ? Relationship->First : ECS::NullEntity;
    }

    ECS::FEntity CEntityLibrary::GetNextSibling(CWorld* World, ECS::FEntity Entity)
    {
        const FRelationshipComponent* Relationship = RelationshipOf(World, Entity);
        return Relationship != nullptr ? Relationship->Next : ECS::NullEntity;
    }

    void CEntityLibrary::GetAncestorChain(CWorld* World, ECS::FEntity Entity, TVector<ECS::FEntity>& Out)
    {
        if (World == nullptr)
        {
            return;
        }

        for (ECS::FEntity Current = Entity; Current != ECS::NullEntity; )
        {
            Out.push_back(Current);
            const FRelationshipComponent* Relationship = RelationshipOf(World, Current);
            Current = Relationship != nullptr ? Relationship->Parent : ECS::NullEntity;
        }
    }

    void CEntityLibrary::GetSubtree(CWorld* World, ECS::FEntity Entity, TVector<ECS::FEntity>& Out)
    {
        if (World == nullptr)
        {
            return;
        }

        TVector<ECS::FEntity> Stack;
        Stack.push_back(Entity);
        while (!Stack.empty())
        {
            const ECS::FEntity Node = Stack.back();
            Stack.pop_back();
            Out.push_back(Node);

            const FRelationshipComponent* Relationship = RelationshipOf(World, Node);
            for (ECS::FEntity Child = Relationship != nullptr ? Relationship->First : ECS::NullEntity;
                 Child != ECS::NullEntity; )
            {
                Stack.push_back(Child);
                const FRelationshipComponent* ChildRelationship = RelationshipOf(World, Child);
                Child = ChildRelationship != nullptr ? ChildRelationship->Next : ECS::NullEntity;
            }
        }
    }
}
