#pragma once

#include "AI/Navigation/NavTypes.h"
#include "Core/Object/ObjectMacros.h"
#include "NavLinkComponent.generated.h"

namespace Lumina
{
    // An off-mesh connection, so a jump across a gap, a ladder, or a teleporter becomes a routable shortcut.
    REFLECT(Component, Category = "AI")
    struct RUNTIME_API SNavLinkComponent
    {
        GENERATED_BODY()

        /** Local-space entry point. Must land within Radius of walkable surface or the bake drops the link. */
        PROPERTY(Editable, Category = "NavLink")
        FVector3 Start = FVector3(0.0f);

        /** Local-space exit point. */
        PROPERTY(Editable, Category = "NavLink")
        FVector3 End = FVector3(0.0f, 0.0f, 4.0f);

        /** Snap radius used to attach each endpoint to a polygon. */
        PROPERTY(Editable, Category = "NavLink", ClampMin = 0.01f)
        float Radius = 0.6f;

        /** When false the link is one-way, from Start to End. */
        PROPERTY(Editable, Category = "NavLink")
        bool bBidirectional = true;

        /** Area id of the link polygon, which drives its traversal cost. */
        PROPERTY(Editable, Category = "NavLink")
        ENavArea Area = ENavArea::Ground;

        /** Poly flag a query filter must include for an agent to take this link; excluding it routes around. */
        PROPERTY(Editable, Category = "NavLink")
        ENavPolyFlag Flag = ENavPolyFlag::Jump;

        /** When false the link is ignored by bakes. */
        PROPERTY(Editable, Category = "NavLink")
        bool bEnabled = true;
    };
}
