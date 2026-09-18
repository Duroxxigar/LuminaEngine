#include "ArrayOps.h"

#include "SIMDConfig.h"
#include "VFloat8.h"

#if defined(_MSC_VER)
    #include <intrin.h>
#else
    #include <cpuid.h>
#endif

namespace Lumina::SIMD
{
    namespace
    {
        bool DetectFMA()
        {
            int Info[4] = {};
        #if defined(_MSC_VER)
            __cpuid(Info, 1);
        #else
            __cpuid(1, Info[0], Info[1], Info[2], Info[3]);
        #endif
            return (Info[2] & (1 << 12)) != 0;
        }

    #if defined(_MSC_VER)
        #define LUMINA_FMA_TARGET
    #else
        #define LUMINA_FMA_TARGET __attribute__((target("fma,avx")))
    #endif

        void LerpTail(float* Out, const float* A, const float* B, int32 From, int32 Count, float Alpha)
        {
            const float OneMinus = 1.0f - Alpha;
            for (int32 i = From; i < Count; ++i)
            {
                Out[i] = A[i] * OneMinus + B[i] * Alpha;
            }
        }

        void LerpBase(float* Out, const float* A, const float* B, int32 Count, float Alpha)
        {
            const VFloat8 VAlpha    = VFloat8::Broadcast(Alpha);
            const VFloat8 VOneMinus = VFloat8::Broadcast(1.0f - Alpha);

            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                const VFloat8 Va = VFloat8::Load(A + i);
                const VFloat8 Vb = VFloat8::Load(B + i);
                MulAdd(Va, VOneMinus, Vb * VAlpha).Store(Out + i);
            }

            LerpTail(Out, A, B, i, Count, Alpha);
        }

        LUMINA_FMA_TARGET
        void LerpFused(float* Out, const float* A, const float* B, int32 Count, float Alpha)
        {
            const __m256 VAlpha    = _mm256_set1_ps(Alpha);
            const __m256 VOneMinus = _mm256_set1_ps(1.0f - Alpha);

            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                const __m256 Va = _mm256_loadu_ps(A + i);
                const __m256 Vb = _mm256_loadu_ps(B + i);
                _mm256_storeu_ps(Out + i, _mm256_fmadd_ps(Va, VOneMinus, _mm256_mul_ps(Vb, VAlpha)));
            }

            LerpTail(Out, A, B, i, Count, Alpha);
        }

        void LerpVarTail(float* Out, const float* A, const float* B, const float* Alphas, int32 From, int32 Count)
        {
            for (int32 i = From; i < Count; ++i)
            {
                Out[i] = A[i] * (1.0f - Alphas[i]) + B[i] * Alphas[i];
            }
        }

        void LerpVarBase(float* Out, const float* A, const float* B, const float* Alphas, int32 Count)
        {
            const VFloat8 One = VFloat8::Broadcast(1.0f);

            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                const VFloat8 Va = VFloat8::Load(A + i);
                const VFloat8 Vb = VFloat8::Load(B + i);
                const VFloat8 Vt = VFloat8::Load(Alphas + i);
                MulAdd(Va, One - Vt, Vb * Vt).Store(Out + i);
            }

            LerpVarTail(Out, A, B, Alphas, i, Count);
        }

        LUMINA_FMA_TARGET
        void LerpVarFused(float* Out, const float* A, const float* B, const float* Alphas, int32 Count)
        {
            const __m256 One = _mm256_set1_ps(1.0f);

            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                const __m256 Va = _mm256_loadu_ps(A + i);
                const __m256 Vb = _mm256_loadu_ps(B + i);
                const __m256 Vt = _mm256_loadu_ps(Alphas + i);
                _mm256_storeu_ps(Out + i, _mm256_fmadd_ps(Va, _mm256_sub_ps(One, Vt), _mm256_mul_ps(Vb, Vt)));
            }

            LerpVarTail(Out, A, B, Alphas, i, Count);
        }

        void AddScaledTail(float* Out, const float* A, const float* B, float S, int32 From, int32 Count)
        {
            for (int32 i = From; i < Count; ++i)
            {
                Out[i] = A[i] + B[i] * S;
            }
        }

        void AddScaledBase(float* Out, const float* A, const float* B, float S, int32 Count)
        {
            const VFloat8 Vs = VFloat8::Broadcast(S);

            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                MulAdd(VFloat8::Load(B + i), Vs, VFloat8::Load(A + i)).Store(Out + i);
            }

            AddScaledTail(Out, A, B, S, i, Count);
        }

        LUMINA_FMA_TARGET
        void AddScaledFused(float* Out, const float* A, const float* B, float S, int32 Count)
        {
            const __m256 Vs = _mm256_set1_ps(S);

            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                _mm256_storeu_ps(Out + i, _mm256_fmadd_ps(_mm256_loadu_ps(B + i), Vs, _mm256_loadu_ps(A + i)));
            }

            AddScaledTail(Out, A, B, S, i, Count);
        }

        void MulAddTail(float* Out, const float* Base, const float* Dir, const float* Scale, int32 From, int32 Count)
        {
            for (int32 i = From; i < Count; ++i)
            {
                Out[i] = Base[i] + Dir[i] * Scale[i];
            }
        }

        void MulAddBase(float* Out, const float* Base, const float* Dir, const float* Scale, int32 Count)
        {
            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                MulAdd(VFloat8::Load(Dir + i), VFloat8::Load(Scale + i), VFloat8::Load(Base + i)).Store(Out + i);
            }

            MulAddTail(Out, Base, Dir, Scale, i, Count);
        }

        LUMINA_FMA_TARGET
        void MulAddFused(float* Out, const float* Base, const float* Dir, const float* Scale, int32 Count)
        {
            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                _mm256_storeu_ps(Out + i, _mm256_fmadd_ps(_mm256_loadu_ps(Dir + i),
                                                          _mm256_loadu_ps(Scale + i),
                                                          _mm256_loadu_ps(Base + i)));
            }

            MulAddTail(Out, Base, Dir, Scale, i, Count);
        }

        void MulLerpOneTail(float* Out, const float* A, const float* B, float S, int32 From, int32 Count)
        {
            for (int32 i = From; i < Count; ++i)
            {
                Out[i] = A[i] * (1.0f + S * (B[i] - 1.0f));
            }
        }

        void MulLerpOneBase(float* Out, const float* A, const float* B, float S, int32 Count)
        {
            const VFloat8 Vs  = VFloat8::Broadcast(S);
            const VFloat8 One = VFloat8::Broadcast(1.0f);

            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                const VFloat8 Factor = MulAdd(VFloat8::Load(B + i) - One, Vs, One);
                (VFloat8::Load(A + i) * Factor).Store(Out + i);
            }

            MulLerpOneTail(Out, A, B, S, i, Count);
        }

        LUMINA_FMA_TARGET
        void MulLerpOneFused(float* Out, const float* A, const float* B, float S, int32 Count)
        {
            const __m256 Vs  = _mm256_set1_ps(S);
            const __m256 One = _mm256_set1_ps(1.0f);

            int32 i = 0;
            for (; i + 8 <= Count; i += 8)
            {
                const __m256 Factor = _mm256_fmadd_ps(_mm256_sub_ps(_mm256_loadu_ps(B + i), One), Vs, One);
                _mm256_storeu_ps(Out + i, _mm256_mul_ps(_mm256_loadu_ps(A + i), Factor));
            }

            MulLerpOneTail(Out, A, B, S, i, Count);
        }
    }

    bool HasFMA()
    {
        static const bool bSupported = DetectFMA();
        return bSupported;
    }

    void LerpArray(float* Out, const float* A, const float* B, int32 Count, float Alpha)
    {
        HasFMA() ? LerpFused(Out, A, B, Count, Alpha) : LerpBase(Out, A, B, Count, Alpha);
    }

    void LerpArrayVarAlpha(float* Out, const float* A, const float* B, const float* Alphas, int32 Count)
    {
        HasFMA() ? LerpVarFused(Out, A, B, Alphas, Count) : LerpVarBase(Out, A, B, Alphas, Count);
    }

    void AddScaledArray(float* Out, const float* A, const float* B, float S, int32 Count)
    {
        HasFMA() ? AddScaledFused(Out, A, B, S, Count) : AddScaledBase(Out, A, B, S, Count);
    }

    void MulAddArray(float* Out, const float* Base, const float* Dir, const float* Scale, int32 Count)
    {
        HasFMA() ? MulAddFused(Out, Base, Dir, Scale, Count) : MulAddBase(Out, Base, Dir, Scale, Count);
    }

    void MulLerpOneArray(float* Out, const float* A, const float* B, float S, int32 Count)
    {
        HasFMA() ? MulLerpOneFused(Out, A, B, S, Count) : MulLerpOneBase(Out, A, B, S, Count);
    }
}
