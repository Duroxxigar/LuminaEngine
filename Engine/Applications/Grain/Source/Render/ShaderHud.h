#pragma once

// The overlay draws straight onto the swapchain after tonemapping, so its colors are display values.
namespace Grain::Shaders
{
    constexpr const char* kHudModule = R"SLANG(
        struct FRHIRoot { uint64_t Args; };
        [[vk::push_constant]] FRHIRoot gRHI;
        Ptr<T> GetArgs<T>() { return (T*)gRHI.Args; }

        struct FHudVertex
        {
            float4 Color;
            float2 Position;
            float2 Local;
            uint   Glyph;
            uint   Pad0;
            uint   Pad1;
            uint   Pad2;
        };

        struct FHudArgs
        {
            FHudVertex* Vertices;
            uint*       Font;
            float2      Resolution;
            float2      Pad;
        };

        static const uint kSolidGlyph = 0xFFFFFFFFu;
        static const uint kGlyphWidth = 5u;
        static const uint kGlyphHeight = 7u;

        struct FHudOut
        {
            float4 Position : SV_Position;
            float4 Color    : COLOR0;
            float2 Local    : TEXCOORD0;
            nointerpolation uint Glyph : TEXCOORD1;
        };

        [shader("vertex")]
        FHudOut HudVS(uint VertexID : SV_VertexID)
        {
            const FHudArgs Args = *GetArgs<FHudArgs>();
            const FHudVertex Vertex = Args.Vertices[VertexID];

            FHudOut Output;
            Output.Position = float4(Vertex.Position / Args.Resolution * 2.0 - 1.0, 0.0, 1.0);
            Output.Color = Vertex.Color;
            Output.Local = Vertex.Local;
            Output.Glyph = Vertex.Glyph;
            return Output;
        }

        [shader("fragment")]
        float4 HudPS(FHudOut Input) : SV_Target
        {
            if (Input.Glyph == kSolidGlyph)
            {
                return Input.Color;
            }

            const FHudArgs Args = *GetArgs<FHudArgs>();

            const uint Column = min(uint(Input.Local.x * float(kGlyphWidth)), kGlyphWidth - 1u);
            const uint Row = min(uint(Input.Local.y * float(kGlyphHeight)), kGlyphHeight - 1u);

            // Five bytes to a glyph, one column per byte, so a lookup is one load and two shifts.
            const uint Index = Input.Glyph * kGlyphWidth + Column;
            const uint Bits = (Args.Font[Index >> 2u] >> ((Index & 3u) * 8u)) & 0xFFu;

            if (((Bits >> Row) & 1u) == 0u)
            {
                discard;
            }

            return Input.Color;
        }
    )SLANG";
}
