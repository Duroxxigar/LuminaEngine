#pragma once

#include "VoxelTypes.h"

namespace Grain
{
    // Everything the shaders need about the time of day, evaluated once per frame on the CPU.
    struct FSkyState
    {
        FVector3 SunDir { 0.45f, 0.78f, -0.44f };
        FVector3 MoonDir { -0.45f, -0.78f, 0.44f };
        FVector3 SunColor { 1.00f, 0.95f, 0.86f };
        FVector3 Zenith { 0.09f, 0.20f, 0.46f };
        FVector3 Horizon { 0.47f, 0.60f, 0.78f };
        FVector3 Ground { 0.10f, 0.10f, 0.10f };

        float SunIntensity  = 3.0f;
        float MoonIntensity = 0.0f;
        float StarIntensity = 0.0f;
        float CloudCover    = 0.42f;
        float Haze          = 1.0f;
        float FogDensity    = 0.022f;
        float FogFalloff    = 0.030f;
        float Exposure      = 1.05f;
        float GiScale       = 1.0f;
        float DayFactor     = 1.0f;
    };

    inline FVector3 SkyLerp(const FVector3& A, const FVector3& B, float T)
    {
        return { Math::Lerp(A.x, B.x, T), Math::Lerp(A.y, B.y, T), Math::Lerp(A.z, B.z, T) };
    }

    // Phase runs 0 at dawn through 0.5 at dusk, so noon sits at 0.25 and midnight at 0.75.
    inline FSkyState EvaluateSky(float Phase)
    {
        FSkyState Out;

        const float Angle = (Phase - 0.25f) * 6.2831853f;
        const float Elevation = Math::Cos(Angle);

        // A tilted arc, so the sun does not simply pass straight overhead.
        const FVector3 Sun
        {
            Math::Sin(Angle) * 0.86f,
            Elevation,
            Math::Sin(Angle) * 0.51f - 0.28f,
        };

        Out.SunDir = Math::Normalize(Sun);
        Out.MoonDir = { -Out.SunDir.x, -Out.SunDir.y, -Out.SunDir.z };

        const float Height = Out.SunDir.y;
        const float Day = Math::Clamp((Height + 0.10f) * 4.2f, 0.0f, 1.0f);
        const float Golden = Math::Clamp(1.0f - Math::Abs(Height) * 4.0f, 0.0f, 1.0f);
        const float Night = 1.0f - Day;

        Out.DayFactor = Day;

        const FVector3 Noon { 1.00f, 0.97f, 0.90f };
        const FVector3 Dusk { 1.00f, 0.52f, 0.22f };
        Out.SunColor = SkyLerp(Noon, Dusk, Golden);
        Out.SunIntensity = Day * Day * 2.6f;

        Out.MoonIntensity = Night * Night * 0.40f;
        Out.StarIntensity = Math::Clamp((Night - 0.35f) * 2.0f, 0.0f, 1.0f);

        const FVector3 ZenithDay { 0.065f, 0.165f, 0.44f };
        const FVector3 ZenithNight { 0.010f, 0.016f, 0.045f };
        const FVector3 HorizonDay { 0.54f, 0.66f, 0.82f };
        const FVector3 HorizonNight { 0.030f, 0.042f, 0.085f };
        const FVector3 HorizonGolden { 0.86f, 0.43f, 0.22f };

        Out.Zenith = SkyLerp(ZenithNight, ZenithDay, Day);
        Out.Horizon = SkyLerp(HorizonNight, HorizonDay, Day);
        Out.Horizon = SkyLerp(Out.Horizon, HorizonGolden, Golden * Day * 0.85f);

        Out.Ground = SkyLerp(FVector3{ 0.012f, 0.014f, 0.020f }, FVector3{ 0.10f, 0.11f, 0.12f }, Day);

        Out.CloudCover = 0.50f + Golden * 0.10f;
        Out.Haze = 1.0f;

        // Mist pools at dawn and burns off by noon, which is the cheapest hour of the day to read.
        Out.FogDensity = Math::Lerp(0.0075f, 0.0016f, Day) + Golden * 0.0040f;
        Out.FogFalloff = 0.021f;

        // A darker sky is not a darker image, so exposure opens up once the sun is down.
        Out.Exposure = Math::Lerp(1.30f, 0.95f, Day);
        Out.GiScale = Math::Lerp(1.35f, 1.15f, Day);

        return Out;
    }
}
