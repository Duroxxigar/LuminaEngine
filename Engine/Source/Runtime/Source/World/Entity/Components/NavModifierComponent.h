#pragma once

#include "AI/Navigation/NavTypes.h"
#include "Core/Object/ObjectMacros.h"
#include "NavModifierComponent.generated.h"

namespace Lumina
{
    // Stamps a nav area onto the walkable surface inside an oriented box that follows the entity transform.
    REFLECT(Component, Category = "AI")
    struct RUNTIME_API SNavModifierComponent
    {
        GENERATED_BODY()

        /** Area id stamped onto covered surface. Null carves the surface away instead. */
        PROPERTY(Editable, Category = "NavModifier")
        ENavArea Area = ENavArea::Null;

        /** Half-size of the volume before the entity scale is applied. */
        PROPERTY(Editable, Category = "NavModifier")
        FVector3 Extents = FVector3(2.0f);

        /** Local-space offset of the volume center from the entity origin. */
        PROPERTY(Editable, Category = "NavModifier")
        FVector3 Offset = FVector3(0.0f);

        /** When false the volume is ignored by bakes. */
        PROPERTY(Editable, Category = "NavModifier")
        bool bEnabled = true;
    };
}
