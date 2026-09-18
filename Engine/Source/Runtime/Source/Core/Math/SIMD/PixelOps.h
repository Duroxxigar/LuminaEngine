#pragma once

#include "Platform/GenericPlatform.h"

namespace Lumina::SIMD
{
    // Widens PixelCount RGB triples to RGBA quads with an opaque alpha. Src and Dst must not overlap.
    RUNTIME_API void ExpandRGBToRGBA8(const uint8* Src, uint8* Dst, size_t PixelCount);

    // The 16-bit unorm form of ExpandRGBToRGBA8, with 0xFFFF alpha.
    RUNTIME_API void ExpandRGBToRGBA16(const uint16* Src, uint16* Dst, size_t PixelCount);
}
