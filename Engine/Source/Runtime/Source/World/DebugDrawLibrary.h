#pragma once

#include "Containers/String.h"
#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "Core/Math/Quat/Quat.h"
#include "Core/Math/Vector/VectorTypes.h"
#include "DebugDrawLibrary.generated.h"

namespace Lumina
{
    class CWorld;

    /** Transient debug geometry for a world. A Duration at or below zero draws for one frame. */
    REFLECT()
    class RUNTIME_API CDebugDrawLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        FUNCTION()
        static void DrawLine(CWorld* World, FVector3 Start, FVector3 End, FVector4 Color,
            float Thickness = 1.0f, float Duration = -1.0f);

        FUNCTION()
        static void DrawSphere(CWorld* World, FVector3 Center, float Radius, FVector4 Color,
            float Thickness = 1.0f, float Duration = -1.0f);

        FUNCTION()
        static void DrawBox(CWorld* World, FVector3 Center, FVector3 HalfExtents, FQuat Rotation,
            FVector4 Color, float Thickness = 1.0f, float Duration = -1.0f);

        /** One screen-space line, stacked from the top left of the world viewport for this frame only. */
        FUNCTION()
        static void DrawText(CWorld* World, const FString& Text, FVector4 Color);
    };
}
