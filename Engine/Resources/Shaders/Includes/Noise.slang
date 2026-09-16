#pragma once

// Procedural noise shared by fog, sky, particles, water and the cloud bake.

// Includes nothing on purpose, so a bake-time pass can pull it in without the scene bindings.

// Multiply-frac hash. Offset decorrelates fields that would otherwise sample the same lattice.
float Hash31(float3 P, float3 Offset)
{
    P = frac(P * 0.3183099 + Offset);
    P *= 17.0;
    return frac(P.x * P.y * P.z * (P.x + P.y + P.z));
}

float Hash31(float3 P)
{
    return Hash31(P, float3(0.1, 0.1, 0.1));
}

float3 Hash33(float3 P)
{
    return float3(Hash31(P + 17.0), Hash31(P + 53.0), Hash31(P + 91.0));
}

// Sine-based hash, better distributed than Hash31 and correspondingly more expensive.
float Hash31Sin(float3 P)
{
    return frac(sin(dot(P, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

float Hash21Sin(float2 P)
{
    return frac(sin(dot(P, float2(127.1, 311.7))) * 43758.5453);
}

// Trilinear value noise in [0,1], smoothstep-faded so cell boundaries carry no gradient break.
float ValueNoise3D(float3 P, float3 HashOffset)
{
    float3 I = floor(P);
    float3 F = P - I;
    F = F * F * (3.0 - 2.0 * F);

    float N000 = Hash31(I + float3(0.0, 0.0, 0.0), HashOffset);
    float N100 = Hash31(I + float3(1.0, 0.0, 0.0), HashOffset);
    float N010 = Hash31(I + float3(0.0, 1.0, 0.0), HashOffset);
    float N110 = Hash31(I + float3(1.0, 1.0, 0.0), HashOffset);
    float N001 = Hash31(I + float3(0.0, 0.0, 1.0), HashOffset);
    float N101 = Hash31(I + float3(1.0, 0.0, 1.0), HashOffset);
    float N011 = Hash31(I + float3(0.0, 1.0, 1.0), HashOffset);
    float N111 = Hash31(I + float3(1.0, 1.0, 1.0), HashOffset);

    float NX00 = lerp(N000, N100, F.x);
    float NX10 = lerp(N010, N110, F.x);
    float NX01 = lerp(N001, N101, F.x);
    float NX11 = lerp(N011, N111, F.x);
    float NXY0 = lerp(NX00, NX10, F.y);
    float NXY1 = lerp(NX01, NX11, F.y);
    return lerp(NXY0, NXY1, F.z);
}

float ValueNoise3D(float3 P)
{
    return ValueNoise3D(P, float3(0.1, 0.1, 0.1));
}

// Bilinear value noise in [0,1], on the sine hash rather than the multiply-frac one.
float ValueNoise2D(float2 P)
{
    float2 I = floor(P);
    float2 F = P - I;
    F = F * F * (3.0 - 2.0 * F);

    float A = Hash21Sin(I);
    float B = Hash21Sin(I + float2(1.0, 0.0));
    float C = Hash21Sin(I + float2(0.0, 1.0));
    float D = Hash21Sin(I + float2(1.0, 1.0));
    return lerp(lerp(A, B, F.x), lerp(C, D, F.x), F.y);
}

// Raw fractal sum with the first octave at amplitude 1; see FBM3DNormalized for a [0,1] result.
float FBM3D(float3 P, int Octaves, float Gain, float Lacunarity, float3 HashOffset)
{
    float Sum = 0.0;
    float Amp = 1.0;
    for (int i = 0; i < Octaves; ++i)
    {
        Sum += Amp * ValueNoise3D(P, HashOffset);
        Amp *= Gain;
        P   *= Lacunarity;
    }
    return Sum;
}

float FBM3D(float3 P, int Octaves, float Gain, float Lacunarity)
{
    return FBM3D(P, Octaves, Gain, Lacunarity, float3(0.1, 0.1, 0.1));
}

float FBM2D(float2 P, int Octaves, float Gain, float Lacunarity)
{
    float Sum = 0.0;
    float Amp = 1.0;
    for (int i = 0; i < Octaves; ++i)
    {
        Sum += Amp * ValueNoise2D(P);
        Amp *= Gain;
        P   *= Lacunarity;
    }
    return Sum;
}

// Normalized to [0,1] in one pass, so raising the octave count adds detail without moving the mean.
float FBM3DNormalized(float3 P, int Octaves, float Gain, float Lacunarity, float3 HashOffset)
{
    float Sum   = 0.0;
    float Amp   = 1.0;
    float Total = 0.0;
    for (int i = 0; i < Octaves; ++i)
    {
        Sum   += Amp * ValueNoise3D(P, HashOffset);
        Total += Amp;
        Amp   *= Gain;
        P     *= Lacunarity;
    }
    return Sum / max(Total, 1e-4);
}
