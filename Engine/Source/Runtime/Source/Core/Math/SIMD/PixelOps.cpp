#include "PixelOps.h"

#include "SIMDConfig.h"

namespace Lumina::SIMD
{
    namespace
    {
        // A wide step reads a full 16 bytes but consumes fewer, so the guard keeps it inside the source.
        constexpr size_t kRGBA8PixelsPerStep  = 4;
        constexpr size_t kRGBA8GuardPixels    = 6;
        constexpr size_t kRGBA16PixelsPerStep = 2;
        constexpr size_t kRGBA16GuardPixels   = 3;

        void ExpandTail8(const uint8* Src, uint8* Dst, size_t From, size_t PixelCount)
        {
            for (size_t i = From; i < PixelCount; ++i)
            {
                Dst[i * 4 + 0] = Src[i * 3 + 0];
                Dst[i * 4 + 1] = Src[i * 3 + 1];
                Dst[i * 4 + 2] = Src[i * 3 + 2];
                Dst[i * 4 + 3] = 0xFFu;
            }
        }

        void ExpandTail16(const uint16* Src, uint16* Dst, size_t From, size_t PixelCount)
        {
            for (size_t i = From; i < PixelCount; ++i)
            {
                Dst[i * 4 + 0] = Src[i * 3 + 0];
                Dst[i * 4 + 1] = Src[i * 3 + 1];
                Dst[i * 4 + 2] = Src[i * 3 + 2];
                Dst[i * 4 + 3] = 0xFFFFu;
            }
        }
    }

    void ExpandRGBToRGBA8(const uint8* Src, uint8* Dst, size_t PixelCount)
    {
        // 0x80 zeroes that destination byte, leaving the alpha lane for the blend below to fill.
        const __m128i Shuffle = _mm_setr_epi8(0, 1, 2, (char)0x80,
                                              3, 4, 5, (char)0x80,
                                              6, 7, 8, (char)0x80,
                                              9, 10, 11, (char)0x80);
        const __m128i Alpha = _mm_setr_epi8(0, 0, 0, (char)0xFF,
                                            0, 0, 0, (char)0xFF,
                                            0, 0, 0, (char)0xFF,
                                            0, 0, 0, (char)0xFF);

        size_t i = 0;
        for (; i + kRGBA8GuardPixels <= PixelCount; i += kRGBA8PixelsPerStep)
        {
            const __m128i Triples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Src + i * 3));
            const __m128i Quads   = _mm_or_si128(_mm_shuffle_epi8(Triples, Shuffle), Alpha);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(Dst + i * 4), Quads);
        }

        ExpandTail8(Src, Dst, i, PixelCount);
    }

    void ExpandRGBToRGBA16(const uint16* Src, uint16* Dst, size_t PixelCount)
    {
        const __m128i Shuffle = _mm_setr_epi8(0, 1, 2, 3, 4, 5, (char)0x80, (char)0x80,
                                              6, 7, 8, 9, 10, 11, (char)0x80, (char)0x80);
        const __m128i Alpha = _mm_setr_epi8(0, 0, 0, 0, 0, 0, (char)0xFF, (char)0xFF,
                                            0, 0, 0, 0, 0, 0, (char)0xFF, (char)0xFF);

        size_t i = 0;
        for (; i + kRGBA16GuardPixels <= PixelCount; i += kRGBA16PixelsPerStep)
        {
            const __m128i Triples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Src + i * 3));
            const __m128i Quads   = _mm_or_si128(_mm_shuffle_epi8(Triples, Shuffle), Alpha);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(Dst + i * 4), Quads);
        }

        ExpandTail16(Src, Dst, i, PixelCount);
    }
}
