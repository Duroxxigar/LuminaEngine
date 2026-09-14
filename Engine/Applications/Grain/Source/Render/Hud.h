#pragma once

#include "Containers/String.h"
#include "Containers/Vector.h"
#include "Renderer/RHI.h"
#include "World/VoxelTypes.h"

namespace Grain
{
    class FGame;
    class FCamera;

    struct FHudVertex
    {
        FVector4 Color { 1.0f, 1.0f, 1.0f, 1.0f };
        FVector2 Position { 0.0f, 0.0f };
        FVector2 Local { 0.0f, 0.0f };
        uint32   Glyph = 0xFFFFFFFFu;
        uint32   Pad[3] = { 0, 0, 0 };
    };

    static_assert(sizeof(FHudVertex) == 48, "The Slang mirror expects a packed 48 byte vertex.");

    inline constexpr uint32 kMaxHudVertices = 32768;

    // An immediate mode quad list, rebuilt every frame and drawn in one pass over the swapchain.
    class FHud
    {
    public:

        bool Initialize(EFormat InSwapchainFormat);
        void Shutdown();

        void Build(const FGame& Game, const FUIntVector2& Extent, float RealTime);
        void Draw(RHI::FCmdListH CL, RHI::FTextureH Target, const FUIntVector2& Extent);

        void SetVisible(bool bValue) { bVisible = bValue; }
        NODISCARD bool IsVisible() const { return bVisible; }

    private:

        void Quad(float X, float Y, float Width, float Height, const FVector4& Color);
        void Frame(float X, float Y, float Width, float Height, float Thickness, const FVector4& Color);
        void Bar(float X, float Y, float Width, float Height, float Fill,
                 const FVector4& Color, const FVector4& Back);

        void Glyph(char Character, float X, float Y, float Scale, const FVector4& Color);
        void Text(const char* Value, float X, float Y, float Scale, const FVector4& Color);
        void TextCentered(const char* Value, float CenterX, float Y, float Scale, const FVector4& Color);

        NODISCARD static float TextWidth(const char* Value, float Scale);

        RHI::FPipelineH     Pipeline;
        RHI::FGPUAllocation VertexBuffer;
        RHI::FGPUAllocation FontBuffer;

        TVector<FHudVertex> Vertices;

        EFormat SwapchainFormat = EFormat::UNKNOWN;
        bool    bVisible = true;
    };
}
