#include "RuntimePCH.h"

#include "DebugDrawLibrary.h"

#include "World/World.h"

namespace Lumina
{
    void CDebugDrawLibrary::DrawLine(CWorld* World, FVector3 Start, FVector3 End, FVector4 Color,
        float Thickness, float Duration)
    {
        if (World != nullptr)
        {
            World->DrawLine(Start, End, Color, Thickness, true, Duration);
        }
    }

    void CDebugDrawLibrary::DrawSphere(CWorld* World, FVector3 Center, float Radius, FVector4 Color,
        float Thickness, float Duration)
    {
        if (World != nullptr)
        {
            World->GetDebugInterface()->DrawSphere(Center, Radius, Color, TOptional<float>(Thickness),
                TOptional<bool>(true), TOptional<float>(Duration));
        }
    }

    void CDebugDrawLibrary::DrawBox(CWorld* World, FVector3 Center, FVector3 HalfExtents, FQuat Rotation,
        FVector4 Color, float Thickness, float Duration)
    {
        if (World != nullptr)
        {
            World->GetDebugInterface()->DrawBox(Center, HalfExtents, Rotation, Color, TOptional<float>(Thickness),
                TOptional<bool>(true), TOptional<float>(Duration));
        }
    }

    void CDebugDrawLibrary::DrawText(CWorld* World, const FString& Text, FVector4 Color)
    {
        if (World != nullptr)
        {
            World->DrawDebugText(Text, Color);
        }
    }
}
