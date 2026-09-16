#pragma once

#include "World/ECS/Registry.h"

#define USE_IMGUI_API
#include <imgui.h>
#include "Core/Math/Math.h"

#include "UI/Properties/PropertyTable.h"
#include "World/Entity/Components/BlockoutComponent.h"
#include "WorldEditorMode.h"

namespace Lumina
{
    class CWorld;
    struct SCameraComponent;

    /** Blockout modeling mode, dragging grid-snapped primitives onto whatever surface is under the cursor. */
    class FModelingEditMode final : public IWorldEditorMode
    {
    public:
        const char* GetDisplayName() const override { return "Modeling"; }
        const char* GetIcon() const override { return LE_ICON_SHAPE_PLUS; }
        const char* GetTooltip() const override
        {
            return "Blockout modeling. Drag primitives onto the world, snapped to a grid, and tune them live.";
        }

        void OnEnter(CWorld* World) override;
        void OnExit(CWorld* World) override;

        void Tick(CWorld* World, const SCameraComponent& Camera, bool bViewportHovered, ImVec2 ViewportScreenOrigin, ImVec2 ViewportSize) override;
        void DrawOverlay(CWorld* World, ImVec2 ViewportScreenOrigin, ImVec2 ViewportSize, const SCameraComponent& Camera) override;
        void DrawToolbar(CWorld* World, float ButtonSize) override;

        /** Only while a shape is armed, so the gizmo stays usable for nudging what was just placed. */
        bool ConsumesViewportInput() const override { return bPlacementArmed; }

    private:

        /** Ground, terrain and existing blockout surfaces, whichever the ray reaches first. */
        bool TraceSurface(CWorld* World, const FVector3& RayOrigin, const FVector3& RayDir, FVector3& OutHit) const;

        /** Footprint the release would build, from the drag rectangle or the template when it is a click. */
        void GetPendingFootprint(FVector3& OutCenter, float& OutSizeX, float& OutSizeZ) const;

        void CreateShape(CWorld* World, const FVector3& Center, float SizeX, float SizeZ);

        /** Points the property table at the selected blockout when there is one, else at the template. */
        void RebindShapeSettings(CWorld* World);

        void DrawShapePalette();
        void DrawPlacementSettings();

        float SnapAxis(float Value) const;

        /** Parameters the next placed shape is built from, and the edit target when nothing is selected. */
        SBlockoutComponent Template;

        bool bPlacementArmed  = true;
        bool bShowPanel       = true;
        bool bSnapEnabled     = true;
        bool bCreateCollision = true;

        float GridSize = 0.5f;

        bool     bHoverValid = false;
        FVector3 HoverPoint  = FVector3(0.0f);

        bool     bDragging   = false;
        FVector3 DragAnchor  = FVector3(0.0f);
        FVector3 DragCurrent = FVector3(0.0f);

        FPropertyTable ShapeSettings;
        void*          BoundShapeData = nullptr;
        bool           bPropertyTransactionOpen = false;
    };
}
