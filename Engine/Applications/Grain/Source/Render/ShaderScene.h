#pragma once

// The raymarch, the denoiser and the post chain, as one module with named entry points.
namespace Grain::Shaders
{
    constexpr const char* kModule = R"SLANG(
        [[vk::binding(0, 0)]] SamplerState gSamplers[];
        [[vk::binding(1, 0)]] Texture2D    gTextures2D[];

        static const uint SAMPLER_LINEAR_CLAMP = 1;

        float4 SampleLevel0(uint TextureID, float2 UV)
        {
            return gTextures2D[TextureID].SampleLevel(gSamplers[SAMPLER_LINEAR_CLAMP], UV, 0.0);
        }

        struct FEntityGpu
        {
            float3 Position;
            float  Yaw;
            float3 Half;
            uint   Model;
            float3 Tint;
            float  Emissive;
        };

        struct FModelDesc
        {
            uint SizeX;
            uint SizeY;
            uint SizeZ;
            uint Base;
        };

        struct FViewArgs
        {
            FVoxNode* Nodes;
            uint*     Masks;
            uint*     Prefix;
            uint*     Children;
            uint*     Payload;
            uint*     SimGrid;
            uint*     SimCoarse;

            FEntityGpu* Entities;
            FModelDesc* Models;
            uint*       ModelCells;

            float4 CameraPos;
            float4 CameraFwd;
            float4 CameraRight;
            float4 CameraUp;
            float4 SunDir;
            float4 MoonDir;
            float4 SunColor;
            float4 SkyZenith;
            float4 SkyHorizon;
            float4 SkyGround;
            float4 Params;
            float4 Jitter;
            float4 FogParams;
            float4 SimOrigin;
            float4 EntityBounds;

            uint4 IDs;
        };

        FViewArgs View() { return *GetArgs<FViewArgs>(); }

        FScene SceneOf(FViewArgs V)
        {
            FScene S;
            S.Nodes     = V.Nodes;
            S.Masks     = V.Masks;
            S.Prefix    = V.Prefix;
            S.Children  = V.Children;
            S.Payload   = V.Payload;
            S.SimGrid   = V.SimGrid;
            S.SimCoarse = V.SimCoarse;
            S.SimOrigin = V.SimOrigin.xyz;
            S.bSim      = V.IDs.w;
            return S;
        }

        //~ Sky, sun and moon.

        float Hash21S(float2 P)
        {
            P = frac(P * float2(123.34, 456.21));
            P += dot(P, P + 45.32);
            return frac(P.x * P.y);
        }

        float Hash31S(float3 P)
        {
            P = frac(P * 0.1031);
            P += dot(P, P.yzx + 33.33);
            return frac((P.x + P.y) * P.z);
        }

        float ValueNoise2(float2 P)
        {
            const float2 I = floor(P);
            const float2 F = P - I;
            const float2 W = F * F * (3.0 - 2.0 * F);

            const float A = Hash21S(I);
            const float B = Hash21S(I + float2(1.0, 0.0));
            const float C = Hash21S(I + float2(0.0, 1.0));
            const float D = Hash21S(I + float2(1.0, 1.0));

            return lerp(lerp(A, B, W.x), lerp(C, D, W.x), W.y);
        }

        float Fbm2(float2 P, int Octaves)
        {
            float Sum = 0.0;
            float Amplitude = 0.5;
            float Total = 0.0;

            for (int i = 0; i < Octaves; ++i)
            {
                Sum += ValueNoise2(P) * Amplitude;
                Total += Amplitude;
                P = P * 2.07 + 13.7;
                Amplitude *= 0.5;
            }
            return Sum / max(Total, 1e-4);
        }

        float3 StarField(FViewArgs V, float3 Dir)
        {
            if (Dir.y <= 0.0 || V.SkyGround.w <= 0.001)
            {
                return float3(0.0);
            }

            const float3 Grid = Dir * 210.0;
            const float3 Cell = floor(Grid);
            const float Pick = Hash31S(Cell);

            if (Pick < 0.9958)
            {
                return float3(0.0);
            }

            const float3 Offset = Grid - Cell - 0.5;
            const float Falloff = saturate(1.0 - length(Offset) * 2.4);
            const float Twinkle = 0.55 + 0.45 * sin(Pick * 1371.0 + V.CameraRight.w * 2.3);

            const float3 Tint = lerp(float3(0.70, 0.80, 1.00), float3(1.00, 0.88, 0.72), frac(Pick * 57.0));
            return Tint * Falloff * Falloff * Twinkle * V.SkyGround.w * 6.0;
        }

        // A drifting layer read off the direction's intersection with a fixed height.
        float3 ApplyClouds(FViewArgs V, float3 Dir, float3 Sky)
        {
            const float Cover = V.SkyHorizon.w;
            if (Dir.y <= 0.015 || Cover <= 0.001)
            {
                return Sky;
            }

            const float2 Plane = Dir.xz / Dir.y * 1.15 + V.CameraRight.w * float2(0.011, 0.005);

            float Density = Fbm2(Plane, 5);
            Density = saturate((Density - (1.0 - Cover)) * 4.2);
            Density *= smoothstep(0.015, 0.20, Dir.y);

            const float Detail = Fbm2(Plane * 3.1 + 7.3, 3);
            Density *= 0.45 + 0.55 * Detail;

            const float Facing = saturate(dot(Dir, V.SunDir.xyz) * 0.5 + 0.5);
            const float3 Lit = V.SunColor.rgb * V.SunColor.w * 0.55 + V.SkyZenith.rgb * 0.7;
            const float3 Shade = V.SkyZenith.rgb * 0.45 + V.SkyHorizon.rgb * 0.25;

            const float3 CloudColor = lerp(Shade, Lit, Facing * Facing);
            return lerp(Sky, CloudColor, saturate(Density) * 0.92);
        }

        float3 SunRadiance(FViewArgs V)
        {
            return V.SunColor.rgb * V.SunDir.w;
        }

        float3 MoonRadiance(FViewArgs V)
        {
            return float3(0.34, 0.44, 0.72) * V.MoonDir.w;
        }

        float3 SkyRadiance(FViewArgs V, float3 Dir)
        {
            const float Up = Dir.y;

            float3 Sky = lerp(V.SkyHorizon.rgb, V.SkyZenith.rgb, pow(saturate(Up), 0.42));
            Sky = lerp(Sky, V.SkyGround.rgb, smoothstep(0.03, -0.32, Up));

            Sky += StarField(V, Dir);

            const float SunCos = saturate(dot(Dir, V.SunDir.xyz));
            Sky += V.SunColor.rgb * pow(SunCos, 7.0) * 0.34 * V.SunDir.w;
            Sky += V.SunColor.rgb * smoothstep(0.99930, 0.99965, SunCos) * 22.0 * saturate(V.SunDir.w);

            const float MoonCos = saturate(dot(Dir, V.MoonDir.xyz));
            Sky += float3(0.80, 0.86, 1.00) * smoothstep(0.99955, 0.99980, MoonCos) * 5.0 * V.MoonDir.w;
            Sky += float3(0.16, 0.20, 0.34) * pow(MoonCos, 22.0) * V.MoonDir.w;

            return ApplyClouds(V, Dir, Sky);
        }

        //~ Materials.

        float HashVoxel(float3 P)
        {
            const float3 I = floor(P);
            return frac(sin(dot(I, float3(12.9898, 78.233, 37.719))) * 43758.5453);
        }

        float ValueNoise3(float3 P)
        {
            const float3 I = floor(P);
            const float3 F = P - I;
            const float3 W = F * F * (3.0 - 2.0 * F);

            const float A = lerp(lerp(Hash31S(I), Hash31S(I + float3(1, 0, 0)), W.x),
                                 lerp(Hash31S(I + float3(0, 1, 0)), Hash31S(I + float3(1, 1, 0)), W.x), W.y);
            const float B = lerp(lerp(Hash31S(I + float3(0, 0, 1)), Hash31S(I + float3(1, 0, 1)), W.x),
                                 lerp(Hash31S(I + float3(0, 1, 1)), Hash31S(I + float3(1, 1, 1)), W.x), W.y);
            return lerp(A, B, W.z);
        }

        struct FSurfaceMaterial
        {
            float3 Albedo;
            float3 Emissive;
            float  Gloss;
            float  Variation;
        };

        FSurfaceMaterial MaterialOf(uint Id, float3 VoxelPos)
        {
            FSurfaceMaterial M;
            M.Emissive = float3(0.0);
            M.Gloss = 0.0;
            M.Albedo = float3(0.5);
            M.Variation = 0.10;

            switch (Id)
            {
            case MAT_GRASS:  M.Albedo = float3(0.17, 0.29, 0.11); M.Variation = 0.16; break;
            case MAT_DIRT:   M.Albedo = float3(0.25, 0.17, 0.11); M.Variation = 0.20; break;
            case MAT_STONE:  M.Albedo = float3(0.32, 0.32, 0.34); M.Variation = 0.16; break;
            case MAT_ROCK:   M.Albedo = float3(0.37, 0.35, 0.33); M.Variation = 0.20; break;
            case MAT_SAND:   M.Albedo = float3(0.64, 0.54, 0.36); M.Variation = 0.14; break;
            case MAT_SNOW:   M.Albedo = float3(0.84, 0.88, 0.96); M.Gloss = 0.25; M.Variation = 0.05; break;
            case MAT_WATER:  M.Albedo = float3(0.03, 0.10, 0.16); M.Gloss = 1.0; break;
            case MAT_WOOD:   M.Albedo = float3(0.26, 0.16, 0.09); M.Variation = 0.22; break;
            case MAT_LEAVES: M.Albedo = float3(0.14, 0.30, 0.12); M.Variation = 0.24; break;
            case MAT_GRAVEL: M.Albedo = float3(0.36, 0.34, 0.31); M.Variation = 0.24; break;
            case MAT_CLAY:   M.Albedo = float3(0.55, 0.31, 0.20); M.Variation = 0.16; break;
            case MAT_MOSS:   M.Albedo = float3(0.15, 0.28, 0.13); M.Variation = 0.20; break;
            case MAT_SLATE:  M.Albedo = float3(0.20, 0.21, 0.25); M.Gloss = 0.15; M.Variation = 0.14; break;
            case MAT_THATCH: M.Albedo = float3(0.52, 0.41, 0.20); M.Variation = 0.26; break;
            case MAT_ORE:
                M.Albedo = float3(0.40, 0.37, 0.31);
                M.Gloss = 0.45;
                M.Variation = 0.24;
                break;
            case MAT_GOLD:
                M.Albedo = float3(0.82, 0.60, 0.19);
                M.Emissive = float3(0.10, 0.06, 0.01);
                M.Gloss = 0.85;
                M.Variation = 0.10;
                break;
            case MAT_ICE:
                M.Albedo = float3(0.58, 0.74, 0.86);
                M.Gloss = 0.75;
                M.Variation = 0.06;
                break;
            case MAT_OBSIDIAN:
                M.Albedo = float3(0.06, 0.05, 0.08);
                M.Gloss = 0.80;
                M.Variation = 0.08;
                break;
            case MAT_CRYSTAL:
                M.Albedo = float3(0.24, 0.52, 0.68);
                M.Emissive = float3(0.10, 0.72, 1.25);
                M.Gloss = 0.70;
                M.Variation = 0.06;
                break;
            case MAT_AMETHYST:
                M.Albedo = float3(0.42, 0.26, 0.62);
                M.Emissive = float3(0.78, 0.20, 1.15);
                M.Gloss = 0.70;
                M.Variation = 0.06;
                break;
            case MAT_EMBER:
                M.Albedo = float3(0.58, 0.24, 0.08);
                M.Emissive = float3(1.90, 0.62, 0.13);
                M.Gloss = 0.35;
                M.Variation = 0.08;
                break;
            case MAT_LAVA:
                M.Albedo = float3(0.32, 0.10, 0.03);
                M.Emissive = float3(2.30, 0.55, 0.09);
                M.Variation = 0.14;
                break;
            case MAT_RUNE:
                M.Albedo = float3(0.30, 0.32, 0.36);
                M.Emissive = float3(0.24, 0.85, 0.70);
                M.Gloss = 0.30;
                break;
            case MAT_BONE:   M.Albedo = float3(0.76, 0.72, 0.60); M.Variation = 0.12; break;
            case MAT_CLOTH:  M.Albedo = float3(0.34, 0.14, 0.16); M.Variation = 0.12; break;
            case MAT_METAL:
                M.Albedo = float3(0.52, 0.55, 0.60);
                M.Gloss = 0.90;
                M.Variation = 0.06;
                break;
            case MAT_SKIN:   M.Albedo = float3(0.58, 0.39, 0.30); M.Variation = 0.08; break;
            case MAT_HUSK:   M.Albedo = float3(0.26, 0.30, 0.22); M.Variation = 0.16; break;
            case MAT_BLOOD:  M.Albedo = float3(0.32, 0.05, 0.05); M.Gloss = 0.45; break;
            case MAT_EYE:
                M.Albedo = float3(0.60, 0.16, 0.10);
                M.Emissive = float3(1.60, 0.20, 0.06);
                break;
            case MAT_WISP:
                M.Albedo = float3(0.40, 0.62, 0.70);
                M.Emissive = float3(0.30, 1.05, 1.30);
                break;
            default: break;
            }

            // Two scales of variation, so a wide field reads as terrain rather than one flat sheet.
            const float Fine = HashVoxel(VoxelPos);
            const float Coarse = ValueNoise3(VoxelPos * 0.055);
            const float Shift = (Fine - 0.5) * M.Variation + (Coarse - 0.5) * M.Variation * 1.1;

            M.Albedo *= saturate(1.0 + Shift);

            // A slight hue swing keeps a forest from reading as one stamped color.
            M.Albedo.g *= saturate(1.0 + (Coarse - 0.5) * M.Variation * 0.5);
            return M;
        }

        //~ Water volume lookups.

        float WaterMassAt(FViewArgs V, float3 PosVoxels)
        {
            if (V.IDs.w == 0u)
            {
                return 0.0;
            }

            const int3 Local = int3(floor(PosVoxels - V.SimOrigin.xyz));
            if (any(Local < 0) || any(Local >= kSimSide))
            {
                return 0.0;
            }

            const uint Cell = V.SimGrid[uint((Local.y * kSimSide + Local.z) * kSimSide + Local.x)];
            if (CellSolid(Cell) != 0u)
            {
                return -1.0;
            }
            return float(CellMass(Cell)) / float(kMassFull);
        }

        // A column sum, so a deep pool reads darker than a sheet running over rock.
        float WaterColumn(FViewArgs V, float3 PosVoxels)
        {
            float Depth = 0.0;
            for (int i = 0; i < 8; ++i)
            {
                const float Mass = WaterMassAt(V, PosVoxels - float3(0.0, float(i), 0.0));
                if (Mass < 0.0)
                {
                    break;
                }
                Depth += max(Mass, 0.0);
            }
            return Depth;
        }

        // The surface follows the volume gradient, which is what makes a blocky grid read as flowing.
        float3 WaterSurfaceNormal(FViewArgs V, float3 PosVoxels, out float Slope)
        {
            Slope = 0.0;

            if (V.IDs.w == 0u)
            {
                return float3(0.0, 1.0, 0.0);
            }

            const float Here = max(WaterMassAt(V, PosVoxels), 0.0);
            const float Right = max(WaterMassAt(V, PosVoxels + float3(1.0, 0.0, 0.0)), 0.0);
            const float Left  = max(WaterMassAt(V, PosVoxels - float3(1.0, 0.0, 0.0)), 0.0);
            const float Front = max(WaterMassAt(V, PosVoxels + float3(0.0, 0.0, 1.0)), 0.0);
            const float Back  = max(WaterMassAt(V, PosVoxels - float3(0.0, 0.0, 1.0)), 0.0);

            if (Here <= 0.0)
            {
                return float3(0.0, 1.0, 0.0);
            }

            const float2 Gradient = float2(Right - Left, Front - Back) * 0.5;
            Slope = length(Gradient);

            return normalize(float3(-Gradient.x, 0.62, -Gradient.y));
        }

        //~ Sampling.

        float3 CosineDirection(float3 Normal, float2 Random)
        {
            const float Angle = Random.x * 6.2831853;
            const float Radius = sqrt(Random.y);
            const float Height = sqrt(max(0.0, 1.0 - Random.y));

            float3 Tangent = abs(Normal.y) < 0.95 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
            Tangent = normalize(cross(Tangent, Normal));
            const float3 Bitangent = cross(Normal, Tangent);

            return normalize(Tangent * (cos(Angle) * Radius) + Bitangent * (sin(Angle) * Radius) + Normal * Height);
        }

        float2 PixelRotation(uint2 Pixel)
        {
            uint Seed = Pixel.x * 1973u + Pixel.y * 9277u + 26699u;
            Seed = (Seed ^ 61u) ^ (Seed >> 16u);
            Seed *= 9u;
            Seed = Seed ^ (Seed >> 4u);
            Seed *= 0x27d4eb2du;
            Seed = Seed ^ (Seed >> 15u);

            const uint A = Seed;
            const uint B = Seed * 1664525u + 1013904223u;
            return float2(float(A & 0xFFFFFFu) / 16777216.0, float(B & 0xFFFFFFu) / 16777216.0);
        }

        // R2 over the frame index with a per pixel rotation, since white noise per frame never stratifies.
        float2 Random2(uint2 Pixel, uint Frame)
        {
            const float2 R2 = float2(0.7548776662, 0.5698402909) * float(Frame + 1u);
            return frac(R2 + PixelRotation(Pixel));
        }

        //~ Entities are small voxel grids in their own rotated box, marched after the terrain.

        struct FEntityHit
        {
            bool   bHit;
            float  T;
            uint   Material;
            float3 Normal;
            float3 Position;
            float3 Tint;
            float  Emissive;
        };

        float3 RotateY(float3 V, float CosA, float SinA)
        {
            return float3(V.x * CosA - V.z * SinA, V.y, V.x * SinA + V.z * CosA);
        }

        uint ModelCellAt(FViewArgs V, FModelDesc Desc, int3 Cell)
        {
            const uint Index = uint((Cell.y * int(Desc.SizeZ) + Cell.z) * int(Desc.SizeX) + Cell.x);
            const uint Word = V.ModelCells[Desc.Base + (Index >> 2u)];
            return (Word >> ((Index & 3u) * 8u)) & 0xFFu;
        }

        bool TraceEntity(FViewArgs V, uint Index, float3 Origin, float3 Dir, float MaxT,
                         bool bAnyHit, inout FEntityHit Out)
        {
            const FEntityGpu E = V.Entities[Index];

            const float CosA = cos(-E.Yaw);
            const float SinA = sin(-E.Yaw);

            const float3 LocalOrigin = RotateY(Origin - E.Position, CosA, SinA);
            const float3 LocalDir = RotateY(Dir, CosA, SinA);
            const float3 InvDir = SafeInverse(LocalDir);

            const float3 A = (-E.Half - LocalOrigin) * InvDir;
            const float3 B = ( E.Half - LocalOrigin) * InvDir;
            const float3 Near = min(A, B);
            const float3 Far = max(A, B);

            const float TEnter = max(max(Near.x, Near.y), max(Near.z, 0.0));
            const float TExit = min(Far.x, min(Far.y, Far.z));

            if (TExit <= TEnter || TEnter >= MaxT)
            {
                return false;
            }

            const FModelDesc Desc = V.Models[E.Model];
            const float3 Size = float3(float(Desc.SizeX), float(Desc.SizeY), float(Desc.SizeZ));
            const float3 CellSize = (E.Half * 2.0) / max(Size, float3(1.0));

            const float3 Entry = LocalOrigin + LocalDir * (TEnter + 1e-4);
            int3 Cell = int3(floor((Entry + E.Half) / CellSize));
            Cell = clamp(Cell, int3(0), int3(Size) - 1);

            const int3 Stride = int3(LocalDir.x > 0.0 ? 1 : -1,
                                     LocalDir.y > 0.0 ? 1 : -1,
                                     LocalDir.z > 0.0 ? 1 : -1);

            const float3 Advance = abs(CellSize * InvDir);
            const float3 Plane = (float3(Cell) + max(float3(Stride), float3(0.0))) * CellSize - E.Half;

            float3 Next = (Plane - LocalOrigin) * InvDir;

            float T = TEnter;
            int Axis = Near.x > Near.y ? (Near.x > Near.z ? 0 : 2) : (Near.y > Near.z ? 1 : 2);

            for (int Step = 0; Step < 96; ++Step)
            {
                const uint Material = ModelCellAt(V, Desc, Cell);

                if (Material != MAT_AIR)
                {
                    if (bAnyHit)
                    {
                        return true;
                    }

                    float3 LocalNormal = float3(0.0, 0.0, 0.0);
                    LocalNormal[Axis] = float(-Stride[Axis]);

                    Out.bHit = true;
                    Out.T = T;
                    Out.Material = Material;
                    Out.Normal = RotateY(LocalNormal, CosA, -SinA);
                    Out.Position = Origin + Dir * T;
                    Out.Tint = E.Tint;
                    Out.Emissive = E.Emissive;
                    return true;
                }

                Axis = Next.x < Next.y ? (Next.x < Next.z ? 0 : 2) : (Next.y < Next.z ? 1 : 2);

                T = Next[Axis];
                if (T > TExit || T >= MaxT)
                {
                    return false;
                }

                Cell[Axis] += Stride[Axis];
                if (Cell[Axis] < 0 || Cell[Axis] >= int(Size[Axis]))
                {
                    return false;
                }

                Next[Axis] += Advance[Axis];
            }

            return false;
        }

        bool MissesEntityBounds(FViewArgs V, float3 Origin, float3 Dir, float MaxT)
        {
            if (V.IDs.x == 0u)
            {
                return true;
            }

            const float3 Center = V.EntityBounds.xyz - Origin;
            const float Radius = V.EntityBounds.w;

            const float Along = dot(Center, Dir);
            if (Along < -Radius || Along - Radius > MaxT)
            {
                return true;
            }

            return dot(Center, Center) - Along * Along > Radius * Radius;
        }

        FEntityHit MarchEntities(FViewArgs V, float3 Origin, float3 Dir, float MaxT)
        {
            FEntityHit Out;
            Out.bHit = false;
            Out.T = MaxT;
            Out.Material = MAT_AIR;
            Out.Normal = float3(0.0, 1.0, 0.0);
            Out.Position = Origin;
            Out.Tint = float3(1.0);
            Out.Emissive = 0.0;

            if (MissesEntityBounds(V, Origin, Dir, MaxT))
            {
                return Out;
            }

            for (uint i = 0; i < V.IDs.x; ++i)
            {
                TraceEntity(V, i, Origin, Dir, Out.T, false, Out);
            }

            return Out;
        }

        bool EntitiesOcclude(FViewArgs V, float3 Origin, float3 Dir, float MaxT)
        {
            FEntityHit Ignored;
            Ignored.bHit = false;
            Ignored.T = MaxT;
            Ignored.Material = MAT_AIR;
            Ignored.Normal = float3(0.0, 1.0, 0.0);
            Ignored.Position = Origin;
            Ignored.Tint = float3(1.0);
            Ignored.Emissive = 0.0;

            if (MissesEntityBounds(V, Origin, Dir, MaxT))
            {
                return false;
            }

            for (uint i = 0; i < V.IDs.x; ++i)
            {
                if (TraceEntity(V, i, Origin, Dir, MaxT, true, Ignored))
                {
                    return true;
                }
            }

            return false;
        }

        //~ Shading.

        // Past the stopping cell, since a fixed offset lands back inside the surface at grazing angles.
        float3 SurfaceOrigin(FHit Hit, float Footprint)
        {
            return Hit.Position + Hit.Normal * max(0.16, Footprint * 2.2);
        }

        // One short probe upward, which is what stops a secondary hit underground collecting sky light.
        float SkyVisibility(FScene S, float3 Point, float3 Normal)
        {
            const float3 Up = normalize(Normal + float3(0.0, 2.2, 0.0));
            return MarchOccluded(S, Point + Normal * 0.14, Up, 64.0, 18, 0.075) ? 0.0 : 1.0;
        }

        // A secondary hit never traces its own bounce, so reflections and the light cache stay cheap.
        float3 ShadeSecondary(FScene S, FViewArgs V, FHit Hit)
        {
            const FSurfaceMaterial M = MaterialOf(Hit.Material, Hit.Position);
            const float3 Point = SurfaceOrigin(Hit, Hit.T * 0.045);

            float3 Lit = M.Emissive;

            const float NdotL = saturate(dot(Hit.Normal, V.SunDir.xyz));
            if (NdotL > 0.0 && V.SunDir.w > 0.001
                && !MarchOccluded(S, Point, V.SunDir.xyz, 900.0, 40, 0.045))
            {
                Lit += M.Albedo * SunRadiance(V) * NdotL;
            }

            const float NdotM = saturate(dot(Hit.Normal, V.MoonDir.xyz));
            if (NdotM > 0.0 && V.MoonDir.w > 0.001
                && !MarchOccluded(S, Point, V.MoonDir.xyz, 600.0, 28, 0.060))
            {
                Lit += M.Albedo * MoonRadiance(V) * NdotM;
            }

            // Gated on a real probe, because an unconditional ambient is what lit sealed rock before.
            const float Sky = SkyVisibility(S, Hit.Position, Hit.Normal);
            return Lit + M.Albedo * SkyRadiance(V, Hit.Normal) * 0.22 * Sky;
        }

        // Returns irradiance rather than radiance, so albedo can be divided out before denoising.
        float3 GatherBounce(FScene S, FViewArgs V, FHit Hit, float Footprint, uint2 Pixel, uint Frame)
        {
            const float3 Dir = CosineDirection(Hit.Normal, Random2(Pixel, Frame));
            const FHit Bounce = March(S, SurfaceOrigin(Hit, Footprint), Dir, 4000.0, 56, 0.050);

            float3 Radiance = float3(0.0);
            if (Bounce.bHit)
            {
                Radiance = ShadeSecondary(S, V, Bounce);
            }
            else if (!Bounce.bExhausted)
            {
                Radiance = SkyRadiance(V, Dir);
            }

            // One ray onto a crystal returns far above the mean and smears into a blob for dozens of frames.
            const float Luma = dot(Radiance, float3(0.2126, 0.7152, 0.0722));
            const float Ceiling = 6.0;
            return Luma > Ceiling ? Radiance * (Ceiling / Luma) : Radiance;
        }

        // The direct term carries no Monte Carlo noise, so it bypasses the denoiser entirely.
        float3 ShadeDirect(FScene S, FViewArgs V, FHit Hit, float3 RayDir, FSurfaceMaterial M,
                           float Footprint)
        {
            const float3 Point = SurfaceOrigin(Hit, Footprint);

            float3 Color = M.Emissive;

            const float NdotL = saturate(dot(Hit.Normal, V.SunDir.xyz));
            if (NdotL > 0.0 && V.SunDir.w > 0.001
                && !MarchOccluded(S, Point, V.SunDir.xyz, 3000.0, 128, 0.012)
                && !EntitiesOcclude(V, Point, V.SunDir.xyz, 240.0))
            {
                Color += M.Albedo * SunRadiance(V) * NdotL;

                if (M.Gloss > 0.0)
                {
                    const float3 Halfway = normalize(V.SunDir.xyz - RayDir);
                    const float Spec = pow(saturate(dot(Hit.Normal, Halfway)), 42.0);
                    Color += SunRadiance(V) * Spec * M.Gloss * 0.65;
                }
            }

            const float NdotM = saturate(dot(Hit.Normal, V.MoonDir.xyz));
            if (NdotM > 0.0 && V.MoonDir.w > 0.001
                && !MarchOccluded(S, Point, V.MoonDir.xyz, 1600.0, 72, 0.020))
            {
                Color += M.Albedo * MoonRadiance(V) * NdotM;
            }

            return Color;
        }

        //~ Axis aligned faces pack into three bits, which is all the a trous edge stop needs.

        float EncodeNormal(float3 N)
        {
            if (N.x > 0.5) { return 0.0; }
            if (N.x < -0.5) { return 1.0; }
            if (N.y > 0.5) { return 2.0; }
            if (N.y < -0.5) { return 3.0; }
            if (N.z > 0.5) { return 4.0; }
            return 5.0;
        }

        struct FFullscreenOut
        {
            float4 Position : SV_Position;
            float2 UV       : TEXCOORD0;
        };

        [shader("vertex")]
        FFullscreenOut FullscreenVS(uint VertexID : SV_VertexID)
        {
            FFullscreenOut Output;
            Output.UV = float2((VertexID << 1) & 2, VertexID & 2);
            Output.Position = float4(Output.UV * 2.0 - 1.0, 0.5, 1.0);
            return Output;
        }

        float3 RayThrough(FViewArgs V, float2 UV)
        {
            const float2 Ndc = UV * 2.0 - 1.0;
            return normalize(V.CameraFwd.xyz
                + V.CameraRight.xyz * (Ndc.x * V.CameraPos.w * V.CameraFwd.w)
                - V.CameraUp.xyz * (Ndc.y * V.CameraPos.w));
        }

        float3 ShadeWater(FScene S, FViewArgs V, FHit Hit, float3 Dir)
        {
            const float Time = V.CameraRight.w;
            const float2 W = Hit.Position.xz;

            // Ripple detail fades with distance, or a flat plane aliases into a moire grid.
            const float Detail = saturate(1.0 - Hit.T / 900.0);

            float2 Ripple = float2(0.0);
            Ripple += float2(sin(W.x * 0.21 + Time * 1.9), cos(W.y * 0.19 - Time * 1.7)) * 0.055;
            Ripple += float2(sin(W.y * 0.47 - Time * 2.6), cos(W.x * 0.51 + Time * 2.3)) * 0.030 * Detail;
            Ripple += float2(sin(dot(W, float2(0.77, 0.63)) + Time * 3.4)) * 0.018 * Detail * Detail;

            float Slope = 0.0;
            const float3 Surface = WaterSurfaceNormal(V, Hit.Position, Slope);

            const float3 Normal = normalize(Surface + float3(Ripple.x, 0.0, Ripple.y));
            const float3 Reflected = reflect(Dir, Normal);

            const FHit Mirror = March(S, Hit.Position + Normal * 0.08, Reflected, 700.0, 64, 0.024);
            const float3 Reflection = Mirror.bHit
                ? ShadeSecondary(S, V, Mirror)
                : (Mirror.bExhausted ? V.SkyHorizon.rgb * 0.5 : SkyRadiance(V, Reflected));

            const FHit Floor = March(S, Hit.Position + Dir * 0.05, Dir, 170.0, 44, 0.024);

            // Beer absorption over the path through the body, so shallows stay clear and pools go deep.
            const float Through = Floor.bHit ? Floor.T * kVoxelSize : 6.0;
            const float3 Absorb = exp(-float3(0.42, 0.13, 0.09) * Through * 3.0);

            const float3 Below = Floor.bHit
                ? ShadeSecondary(S, V, Floor) * Absorb
                : float3(0.008, 0.030, 0.040);

            const float Fresnel = 0.03 + 0.97 * pow(saturate(1.0 + dot(Dir, Normal)), 5.0);
            float3 Color = lerp(Below, Reflection, Fresnel);

            // In scattering, which is what keeps a deep pool from reading as black glass.
            const float Column = WaterColumn(V, Hit.Position);
            const float3 Tint = float3(0.025, 0.19, 0.23) * (V.SunDir.w * 0.7 + 0.3);
            Color += Tint * saturate(0.25 + Column * 0.30) * (1.0 - Fresnel);

            // Sun glint, which is most of what makes a water plane read as a surface at all.
            const float3 Halfway = normalize(V.SunDir.xyz - Dir);
            Color += SunRadiance(V) * pow(saturate(dot(Normal, Halfway)), 220.0) * 2.6;

            // Whitewater only where the surface actually breaks, or the whole run reads as snow.
            const float Thin = 1.0 - saturate(Column * 1.4);
            const float Break = saturate((Slope - 0.22) * 2.4);
            const float Foam = Break * 0.30 + Break * Thin * 0.34;

            return lerp(Color, float3(0.88, 0.94, 1.00) * (0.5 + SunRadiance(V) * 0.7), saturate(Foam));
        }

        //~ Primary pass. Direct radiance and albedo at full resolution, with the hit distance in alpha.

        struct FRaymarchOut
        {
            float4 Direct : SV_Target0;
            float4 Albedo : SV_Target1;
        };

        [shader("fragment")]
        FRaymarchOut RaymarchPS(FFullscreenOut Input)
        {
            const FViewArgs V = View();
            const FScene S = SceneOf(V);

            const uint Frame = uint(V.Params.z);
            const uint2 Pixel = uint2(Input.UV * V.Params.xy);

            const float3 Dir = RayThrough(V, Input.UV + V.Jitter.xy);
            const float3 Origin = V.CameraPos.xyz;

            float3 EntityTint = float3(1.0);
            float EntityEmissive = 0.0;

            const FHit Terrain = March(S, Origin, Dir, 4000.0, 256, V.Params.w);
            const FHit Hit = ResolveEntity(V, Terrain, Origin, Dir, EntityTint, EntityEmissive);

            FRaymarchOut Out;
            Out.Direct = float4(0.0, 0.0, 0.0, 7.0);
            Out.Albedo = float4(0.0, 0.0, 0.0, 1e5);

            if (!Hit.bHit)
            {
                Out.Direct.rgb = Hit.bExhausted ? V.SkyHorizon.rgb : SkyRadiance(V, Dir);
                return Out;
            }

            Out.Albedo.w = Hit.T;
            Out.Direct.w = EncodeNormal(Hit.Normal);

            if (V.IDs.z != 0u)
            {
                if (V.IDs.z == 1u)
                {
                    const float H = float(Hit.Material) * 0.0625;
                    Out.Direct.rgb = saturate(abs(frac(H + float3(0.0, 0.667, 0.333)) * 6.0 - 3.0) - 1.0);
                }
                else
                {
                    Out.Direct.rgb = abs(Hit.Normal);
                }
                return Out;
            }

            if (Hit.Material == MAT_WATER)
            {
                // Water resolves to one radiance with no separable albedo, so it skips the denoiser.
                Out.Direct.rgb = ShadeWater(S, V, Hit, Dir);
                return Out;
            }

            FSurfaceMaterial M = MaterialOf(Hit.Material, Hit.Position);
            M.Albedo *= EntityTint;
            M.Emissive *= 1.0 + EntityEmissive * 2.0;

            Out.Direct.rgb = ShadeDirect(S, V, Hit, Dir, M, Hit.T * V.Params.w);
            Out.Albedo.rgb = M.Albedo;
            return Out;
        }

        // Shared by the primary pass and the bounce pass, so a figure shades the same way in both.
        FHit ResolveEntity(FViewArgs V, FHit Hit, float3 Origin, float3 Dir,
                           out float3 OutTint, out float OutEmissive)
        {
            OutTint = float3(1.0);
            OutEmissive = 0.0;

            const FEntityHit Entity = MarchEntities(V, Origin, Dir, Hit.bHit ? Hit.T : 4000.0);
            if (!Entity.bHit)
            {
                return Hit;
            }

            OutTint = Entity.Tint;
            OutEmissive = Entity.Emissive;

            FHit Out;
            Out.bHit = true;
            Out.bExhausted = false;
            Out.T = Entity.T;
            Out.Material = Entity.Material;
            Out.Normal = Entity.Normal;
            Out.Position = Entity.Position;
            return Out;
        }

        //~ Bounce pass. Half resolution, since the result is denoised and low frequency anyway.

        struct FGiOut
        {
            float4 Irradiance : SV_Target0;
            float4 Geometry   : SV_Target1;
        };

        [shader("fragment")]
        FGiOut GiPS(FFullscreenOut Input)
        {
            const FViewArgs V = View();
            const FScene S = SceneOf(V);

            const uint Frame = uint(V.Params.z);
            const uint2 Pixel = uint2(Input.UV * V.Params.xy);

            const float3 Dir = RayThrough(V, Input.UV);

            float3 EntityTint = float3(1.0);
            float EntityEmissive = 0.0;

            const FHit Terrain = March(S, V.CameraPos.xyz, Dir, 4000.0, 192, V.Params.w);
            const FHit Hit = ResolveEntity(V, Terrain, V.CameraPos.xyz, Dir, EntityTint, EntityEmissive);

            FGiOut Out;
            Out.Irradiance = float4(0.0, 0.0, 0.0, 1e5);
            Out.Geometry = float4(1e5, 7.0, 0.0, 0.0);

            if (!Hit.bHit || Hit.Material == MAT_WATER)
            {
                return Out;
            }

            Out.Irradiance.w = Hit.T;
            Out.Geometry = float4(Hit.T, EncodeNormal(Hit.Normal), 0.0, 0.0);
            Out.Irradiance.rgb = GatherBounce(S, V, Hit, Hit.T * V.Params.w, Pixel, Frame) * V.CameraUp.w;
            return Out;
        }

        //~ Denoiser.

        struct FDenoiseArgs
        {
            uint4  IDs;
            uint4  Extra;
            float4 Params;
            float4 CameraPos;
            float4 CameraFwd;
            float4 CameraRight;
            float4 CameraUp;
            float4 PrevPos;
            float4 PrevFwd;
            float4 PrevRight;
            float4 PrevUp;
            float4 SunDir;
            float4 FogParams;
            float4 SkyHorizon;
            float4 SkyZenith;
            float4 Misc;
        };

        float Luminance(float3 C) { return dot(C, float3(0.2126, 0.7152, 0.0722)); }

        float3 DenoiseRay(FDenoiseArgs Args, float2 UV)
        {
            const float2 Ndc = UV * 2.0 - 1.0;
            return normalize(Args.CameraFwd.xyz
                + Args.CameraRight.xyz * (Ndc.x * Args.CameraPos.w * Args.CameraFwd.w)
                - Args.CameraUp.xyz * (Ndc.y * Args.CameraPos.w));
        }

        // Projects a world point into the previous frame, which is the whole basis of reprojection here.
        bool PreviousUV(FDenoiseArgs Args, float3 World, out float2 OutUV, out float OutDistance)
        {
            const float3 Rel = World - Args.PrevPos.xyz;
            const float Z = dot(Rel, Args.PrevFwd.xyz);

            OutUV = float2(0.0);
            OutDistance = length(Rel);

            if (Z <= 0.05)
            {
                return false;
            }

            OutUV = float2(
                dot(Rel, Args.PrevRight.xyz) / (Z * Args.CameraPos.w * Args.CameraFwd.w),
                -dot(Rel, Args.PrevUp.xyz) / (Z * Args.CameraPos.w)) * 0.5 + 0.5;

            return all(OutUV > 0.001) && all(OutUV < 0.999);
        }

        struct FTemporalOut
        {
            float4 Color  : SV_Target0;
            float4 Moment : SV_Target1;
        };

        // The sample count drives the blend rate, since a fixed rate caps the count however long a pixel holds.
        [shader("fragment")]
        FTemporalOut TemporalPS(FFullscreenOut Input)
        {
            const FDenoiseArgs Args = *GetArgs<FDenoiseArgs>();

            const float4 Raw = SampleLevel0(Args.IDs.x, Input.UV);
            const float3 Current = Raw.rgb;
            const float Distance = Raw.w;

            FTemporalOut Out;

            float History = 0.0;
            float3 Blended = Current;
            float2 Moments = float2(Luminance(Current), Luminance(Current) * Luminance(Current));

            if (Args.Extra.x != 0u && Distance < 9e4)
            {
                const float3 World = Args.CameraPos.xyz + DenoiseRay(Args, Input.UV) * Distance;

                float2 PrevUV;
                float Expected;
                if (PreviousUV(Args, World, PrevUV, Expected))
                {
                    const float4 PrevMoment = SampleLevel0(Args.IDs.z, PrevUV);

                    if (abs(PrevMoment.b - Expected) < max(Expected * 0.035, 1.2))
                    {
                        const float4 PrevColor = SampleLevel0(Args.IDs.y, PrevUV);

                        History = min(PrevMoment.a + 1.0, Args.Params.z);
                        const float Alpha = max(1.0 / (History + 1.0), Args.Params.w);

                        Blended = lerp(PrevColor.rgb, Current, Alpha);
                        Moments = lerp(PrevMoment.rg, Moments, Alpha);
                    }
                }
            }

            const float Variance = max(Moments.y - Moments.x * Moments.x, 0.0);

            Out.Color = float4(Blended, Variance);
            Out.Moment = float4(Moments, Distance, History);
            return Out;
        }

        // Edge stopping on distance, face and luminance, so the blur crosses noise but not geometry.
        [shader("fragment")]
        float4 AtrousPS(FFullscreenOut Input) : SV_Target
        {
            const FDenoiseArgs Args = *GetArgs<FDenoiseArgs>();

            const float2 Texel = 1.0 / Args.Params.xy;
            const float Step = Args.Params.z;

            const float4 Center = SampleLevel0(Args.IDs.x, Input.UV);
            const float2 CenterGeo = SampleLevel0(Args.IDs.y, Input.UV).xy;

            if (CenterGeo.x > 9e4)
            {
                return Center;
            }

            const float CenterLuma = Luminance(Center.rgb);
            const float Sigma = sqrt(max(Center.w, 0.0)) + 1e-3;

            const float Kernel[3] = { 0.375, 0.25, 0.0625 };

            float3 Sum = Center.rgb * Kernel[0] * Kernel[0];
            float SumVariance = Center.w * Kernel[0] * Kernel[0] * Kernel[0] * Kernel[0];
            float Weight = Kernel[0] * Kernel[0];

            for (int Y = -2; Y <= 2; ++Y)
            {
                for (int X = -2; X <= 2; ++X)
                {
                    if (X == 0 && Y == 0)
                    {
                        continue;
                    }

                    const float2 UV = Input.UV + float2(X, Y) * Texel * Step;
                    if (any(UV < 0.0) || any(UV > 1.0))
                    {
                        continue;
                    }

                    const float4 Tap = SampleLevel0(Args.IDs.x, UV);
                    const float2 TapGeo = SampleLevel0(Args.IDs.y, UV).xy;

                    if (TapGeo.x > 9e4)
                    {
                        continue;
                    }

                    const float DepthWeight = exp(-abs(TapGeo.x - CenterGeo.x)
                        / (Args.Params.w * max(CenterGeo.x, 1.0) + 1e-3));
                    const float NormalWeight = TapGeo.y == CenterGeo.y ? 1.0 : 0.08;
                    const float LumaWeight = exp(-abs(Luminance(Tap.rgb) - CenterLuma) / (4.0 * Sigma));

                    const float Spatial = Kernel[abs(X)] * Kernel[abs(Y)];
                    const float W = Spatial * DepthWeight * NormalWeight * LumaWeight;

                    Sum += Tap.rgb * W;
                    SumVariance += Tap.w * W * W;
                    Weight += W;
                }
            }

            const float Inverse = 1.0 / max(Weight, 1e-4);
            return float4(Sum * Inverse, SumVariance * Inverse * Inverse);
        }

        //~ Compose. Albedo goes back on, the half resolution bounce is upsampled, then atmosphere.

        // Four half resolution taps weighted by how close their hit distance is to this pixel's.
        float3 UpsampleIndirect(uint IndirectID, uint GeoID, float2 UV, float2 HalfTexel, float CenterT)
        {
            float3 Sum = float3(0.0);
            float Weight = 0.0;

            for (int Y = 0; Y < 2; ++Y)
            {
                for (int X = 0; X < 2; ++X)
                {
                    const float2 Tap = UV + (float2(X, Y) - 0.5) * HalfTexel;
                    const float TapT = SampleLevel0(GeoID, Tap).x;

                    if (TapT > 9e4)
                    {
                        continue;
                    }

                    const float W = exp(-abs(TapT - CenterT) / max(CenterT * 0.04, 0.5));
                    Sum += SampleLevel0(IndirectID, Tap).rgb * W;
                    Weight += W;
                }
            }

            return Weight > 1e-4 ? Sum / Weight : SampleLevel0(IndirectID, UV).rgb;
        }

        // The analytic height fog integral, so a valley fills while a ridge above it stays clear.
        float FogAlong(FDenoiseArgs Args, float3 Origin, float3 Dir, float Meters)
        {
            const float Density = Args.FogParams.x;
            const float Falloff = Args.FogParams.y;
            const float Ground = Args.FogParams.z;

            const float Base = Density * exp(-Falloff * max(Origin.y * kVoxelSize - Ground, -40.0));

            const float Rise = Falloff * Dir.y;
            const float Integral = abs(Rise) < 1e-4
                ? Meters
                : (1.0 - exp(-Rise * Meters)) / Rise;

            return 1.0 - exp(-Base * max(Integral, 0.0));
        }

        float3 FogColor(FDenoiseArgs Args, float3 Dir)
        {
            const float Facing = pow(saturate(dot(Dir, Args.SunDir.xyz)), 6.0);
            const float3 Ahead = lerp(Args.SkyHorizon.rgb, Args.SkyZenith.rgb, saturate(Dir.y * 1.4));
            return lerp(Ahead, Args.SkyHorizon.rgb * 1.35 + float3(0.06, 0.04, 0.01), Facing);
        }

        [shader("fragment")]
        float4 ComposePS(FFullscreenOut Input) : SV_Target
        {
            const FDenoiseArgs Args = *GetArgs<FDenoiseArgs>();

            const float4 Direct = SampleLevel0(Args.IDs.x, Input.UV);
            const float4 Albedo = SampleLevel0(Args.IDs.y, Input.UV);
            const float Distance = Albedo.w;

            float3 Color = Direct.rgb;

            if (Distance < 9e4)
            {
                const float2 HalfTexel = 2.0 / Args.Params.xy;
                const float3 Indirect = UpsampleIndirect(Args.IDs.z, Args.IDs.w, Input.UV, HalfTexel, Distance);
                Color += Albedo.rgb * Indirect;
            }

            const float3 Dir = DenoiseRay(Args, Input.UV);
            const float Meters = min(Distance, 8000.0) * kVoxelSize;
            const float Fog = FogAlong(Args, Args.CameraPos.xyz, Dir, Meters) * Args.FogParams.w;

            Color = lerp(Color, FogColor(Args, Dir), saturate(Fog));
            return float4(Color, Distance);
        }

        //~ Temporal antialiasing over the composed image, which is what resolves the primary ray jitter.

        float3 RgbToYCoCg(float3 C)
        {
            return float3(0.25 * C.r + 0.5 * C.g + 0.25 * C.b,
                          0.5 * C.r - 0.5 * C.b,
                          -0.25 * C.r + 0.5 * C.g - 0.25 * C.b);
        }

        float3 YCoCgToRgb(float3 C)
        {
            return float3(C.x + C.y - C.z, C.x + C.z, C.x - C.y - C.z);
        }

        // Bilinear history compounds a blur every frame, so the history is rebuilt with Catmull-Rom.
        float3 SampleHistory(uint TextureID, float2 UV, float2 Resolution)
        {
            const float2 Position = UV * Resolution;
            const float2 Center = floor(Position - 0.5) + 0.5;
            const float2 F = Position - Center;

            const float2 W0 = F * (-0.5 + F * (1.0 - 0.5 * F));
            const float2 W1 = 1.0 + F * F * (-2.5 + 1.5 * F);
            const float2 W2 = F * (0.5 + F * (2.0 - 1.5 * F));
            const float2 W3 = F * F * (-0.5 + 0.5 * F);

            const float2 W12 = W1 + W2;
            const float2 Offset12 = W2 / max(W12, 1e-5);

            const float2 Texel = 1.0 / Resolution;
            const float2 UV0 = (Center - 1.0) * Texel;
            const float2 UV3 = (Center + 2.0) * Texel;
            const float2 UV12 = (Center + Offset12) * Texel;

            float3 Sum = float3(0.0);
            Sum += SampleLevel0(TextureID, float2(UV0.x, UV0.y)).rgb * (W0.x * W0.y);
            Sum += SampleLevel0(TextureID, float2(UV12.x, UV0.y)).rgb * (W12.x * W0.y);
            Sum += SampleLevel0(TextureID, float2(UV3.x, UV0.y)).rgb * (W3.x * W0.y);

            Sum += SampleLevel0(TextureID, float2(UV0.x, UV12.y)).rgb * (W0.x * W12.y);
            Sum += SampleLevel0(TextureID, float2(UV12.x, UV12.y)).rgb * (W12.x * W12.y);
            Sum += SampleLevel0(TextureID, float2(UV3.x, UV12.y)).rgb * (W3.x * W12.y);

            Sum += SampleLevel0(TextureID, float2(UV0.x, UV3.y)).rgb * (W0.x * W3.y);
            Sum += SampleLevel0(TextureID, float2(UV12.x, UV3.y)).rgb * (W12.x * W3.y);
            Sum += SampleLevel0(TextureID, float2(UV3.x, UV3.y)).rgb * (W3.x * W3.y);

            return max(Sum, 0.0);
        }

        [shader("fragment")]
        float4 TaaPS(FFullscreenOut Input) : SV_Target
        {
            const FDenoiseArgs Args = *GetArgs<FDenoiseArgs>();

            const float2 Resolution = Args.Params.xy;
            const float2 Texel = 1.0 / Resolution;

            const float4 Center = SampleLevel0(Args.IDs.x, Input.UV);
            const float Distance = Center.w;

            if (Args.Extra.x == 0u)
            {
                return float4(Center.rgb, Distance);
            }

            float3 Mean = float3(0.0);
            float3 MeanSquare = float3(0.0);
            float3 Lowest = float3(1e9);
            float3 Highest = float3(-1e9);

            for (int Y = -1; Y <= 1; ++Y)
            {
                for (int X = -1; X <= 1; ++X)
                {
                    const float3 Tap = RgbToYCoCg(SampleLevel0(Args.IDs.x, Input.UV + float2(X, Y) * Texel).rgb);
                    Mean += Tap;
                    MeanSquare += Tap * Tap;
                    Lowest = min(Lowest, Tap);
                    Highest = max(Highest, Tap);
                }
            }

            Mean *= 1.0 / 9.0;
            MeanSquare *= 1.0 / 9.0;

            const float3 Sigma = sqrt(max(MeanSquare - Mean * Mean, 0.0));
            const float3 MinBound = max(Mean - Sigma * 1.85, Lowest);
            const float3 MaxBound = min(Mean + Sigma * 1.85, Highest);

            // Sky carries no hit distance, so it reprojects off the direction alone.
            const float3 Dir = DenoiseRay(Args, Input.UV);
            const float3 World = Args.CameraPos.xyz + Dir * min(Distance, 4000.0);

            float2 PrevUV;
            float Expected;
            if (!PreviousUV(Args, World, PrevUV, Expected))
            {
                return float4(Center.rgb, Distance);
            }

            const float3 History = RgbToYCoCg(SampleHistory(Args.IDs.y, PrevUV, Resolution));
            const float3 Clipped = clamp(History, MinBound, MaxBound);

            // A pixel dragged far across the screen carries more reprojection error, so it trusts less.
            const float2 Motion = (PrevUV - Input.UV) * Resolution;
            const float Speed = saturate(length(Motion) * 0.12);
            const float Alpha = lerp(Args.Misc.x, 0.35, Speed);

            const float3 Blended = lerp(Clipped, RgbToYCoCg(Center.rgb), Alpha);
            return float4(max(YCoCgToRgb(Blended), 0.0), Distance);
        }

        //~ Eye adaptation, resolved into a single pixel so the composite never waits on a readback.

        [shader("fragment")]
        float4 ExposurePS(FFullscreenOut Input) : SV_Target
        {
            const FDenoiseArgs Args = *GetArgs<FDenoiseArgs>();

            // A sparse grid, since one pixel of output can afford to walk the whole frame.
            const int kSteps = 24;
            float Sum = 0.0;
            float Count = 0.0;

            for (int Y = 0; Y < kSteps; ++Y)
            {
                for (int X = 0; X < kSteps; ++X)
                {
                    const float2 UV = (float2(X, Y) + 0.5) / float(kSteps);

                    // Metering the sky as heavily as the ground is what leaves the ground underexposed.
                    const float2 Centered = UV * 2.0 - 1.0;
                    const float Center = exp(-dot(Centered, Centered) * 0.55);
                    const float Weight = Center * lerp(0.30, 1.0, saturate(Centered.y * 0.5 + 0.5));

                    const float Luma = Luminance(SampleLevel0(Args.IDs.x, UV).rgb);
                    Sum += log2(max(Luma, 1e-4)) * Weight;
                    Count += Weight;
                }
            }

            const float Average = exp2(Sum / max(Count, 1e-4));
            const float Previous = Args.Extra.x != 0u ? SampleLevel0(Args.IDs.y, float2(0.5, 0.5)).x : Average;

            // Opening up takes longer than closing down, which is how an eye actually behaves.
            const float Rate = Average > Previous ? 2.6 : 0.9;
            const float Blend = 1.0 - exp(-max(Args.Misc.y, 0.0) * Rate);

            return float4(lerp(Previous, Average, Blend), 0.0, 0.0, 1.0);
        }

        //~ Bloom.

        struct FBloomArgs
        {
            uint   SourceID;
            uint   bFirstPass;
            float2 SourceTexelSize;
            float  Threshold;
            float  Radius;
            float  Intensity;
            float  Pad0;
        };

        [shader("fragment")]
        float4 DownsamplePS(FFullscreenOut Input) : SV_Target
        {
            const FBloomArgs Args = *GetArgs<FBloomArgs>();
            const float2 Texel = Args.SourceTexelSize;

            float3 Sum = float3(0.0);
            Sum += SampleLevel0(Args.SourceID, Input.UV).rgb * 4.0;
            Sum += SampleLevel0(Args.SourceID, Input.UV + float2(-Texel.x, -Texel.y)).rgb;
            Sum += SampleLevel0(Args.SourceID, Input.UV + float2( Texel.x, -Texel.y)).rgb;
            Sum += SampleLevel0(Args.SourceID, Input.UV + float2(-Texel.x,  Texel.y)).rgb;
            Sum += SampleLevel0(Args.SourceID, Input.UV + float2( Texel.x,  Texel.y)).rgb;
            Sum /= 8.0;

            if (Args.bFirstPass != 0u)
            {
                const float Luma = dot(Sum, float3(0.2126, 0.7152, 0.0722));
                Sum *= max(Luma - Args.Threshold, 0.0) / max(Luma, 0.0001);
            }

            return float4(Sum, 1.0);
        }

        [shader("fragment")]
        float4 UpsamplePS(FFullscreenOut Input) : SV_Target
        {
            const FBloomArgs Args = *GetArgs<FBloomArgs>();
            const float2 Texel = Args.SourceTexelSize * Args.Radius;

            float3 Sum = float3(0.0);
            Sum += SampleLevel0(Args.SourceID, Input.UV).rgb * 4.0;
            Sum += SampleLevel0(Args.SourceID, Input.UV + float2(-Texel.x, 0.0)).rgb * 2.0;
            Sum += SampleLevel0(Args.SourceID, Input.UV + float2( Texel.x, 0.0)).rgb * 2.0;
            Sum += SampleLevel0(Args.SourceID, Input.UV + float2(0.0, -Texel.y)).rgb * 2.0;
            Sum += SampleLevel0(Args.SourceID, Input.UV + float2(0.0,  Texel.y)).rgb * 2.0;
            Sum /= 12.0;

            return float4(Sum * Args.Intensity, 1.0);
        }

        //~ Tonemap and present.

        struct FCompositeArgs
        {
            uint   SceneID;
            uint   BloomID;
            uint   OverlayID;
            uint   ExposureID;
            uint   Flags;
            float  BloomIntensity;
            float2 Resolution;
            float  ExposureBias;
            float  Vignette;
            float  Saturation;
            float  Contrast;
            float4 Tint;
        };

        static const float3x3 kAcesIn = float3x3(
            0.59719, 0.35458, 0.04823,
            0.07600, 0.90834, 0.01566,
            0.02840, 0.13383, 0.83777);

        static const float3x3 kAcesOut = float3x3(
             1.60475, -0.53108, -0.07367,
            -0.10208,  1.10813, -0.00605,
            -0.00327, -0.07276,  1.07602);

        float3 RrtAndOdtFit(float3 V)
        {
            const float3 A = V * (V + 0.0245786) - 0.000090537;
            const float3 B = V * (0.983729 * V + 0.4329510) + 0.238081;
            return A / B;
        }

        // The fitted curve, which holds saturation far better than the single term approximation.
        float3 Tonemap(float3 Color)
        {
            Color = mul(kAcesIn, Color);
            Color = RrtAndOdtFit(Color);
            return saturate(mul(kAcesOut, Color));
        }

        float3 LinearToSrgb(float3 C)
        {
            C = saturate(C);
            const float3 Low = C * 12.92;
            const float3 High = 1.055 * pow(C, 1.0 / 2.4) - 0.055;
            return lerp(Low, High, step(0.0031308, C));
        }

        [shader("fragment")]
        float4 CompositePS(FFullscreenOut Input) : SV_Target
        {
            const FCompositeArgs Args = *GetArgs<FCompositeArgs>();

            float3 Color = SampleLevel0(Args.SceneID, Input.UV).rgb;
            Color += SampleLevel0(Args.BloomID, Input.UV).rgb * Args.BloomIntensity;

            const float Average = SampleLevel0(Args.ExposureID, float2(0.5, 0.5)).x;
            const float Exposure = clamp(0.23 / max(Average, 1e-4), 0.10, 30.0) * Args.ExposureBias;

            Color *= Exposure * Args.Tint.rgb;

            // A scene wide tint rides the grade, so damage and low health can wash the frame.
            const float Luma = dot(Color, float3(0.2126, 0.7152, 0.0722));
            Color = max(lerp(float3(Luma), Color, Args.Saturation), 0.0);
            Color = max(lerp(float3(0.18), Color, Args.Contrast), 0.0);

            Color = Tonemap(Color);

            const float2 Centered = Input.UV * 2.0 - 1.0;
            Color *= saturate(1.0 - dot(Centered, Centered) * 0.20 * Args.Vignette);

            Color = LinearToSrgb(Color);

            if (Args.Flags != 0u)
            {
                const float4 Overlay = SampleLevel0(Args.OverlayID, Input.UV);
                Color = lerp(Color, Overlay.rgb, saturate(Overlay.a));
            }

            return float4(Color, 1.0);
        }
    )SLANG";
}
