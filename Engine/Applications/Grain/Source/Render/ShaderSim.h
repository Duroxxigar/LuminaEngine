#pragma once

// A separate module, since a bindless texture array in the interface makes compute pipeline creation fail.
namespace Grain::Shaders
{
    constexpr const char* kSimModule = R"SLANG(
        struct FSimArgs
        {
            uint*  Grid;
            uint*  Coarse;
            uint4  Source;
            uint4  Control;
        };

        uint SimIndex(int3 P)
        {
            return uint((P.y * kSimSide + P.z) * kSimSide + P.x);
        }

        uint CoarseIndex(int3 Block)
        {
            return uint((Block.y * kSimCoarseSide + Block.z) * kSimCoarseSide + Block.x);
        }

        bool BlockIdle(uint* Coarse, int3 Voxel)
        {
            const int3 Block = clamp(Voxel >> 3, int3(0), int3(kSimCoarseSide - 1));
            return (Coarse[CoarseIndex(Block)] & COARSE_ACTIVE) == 0u;
        }

        // Mass moves between a thread's own pair, so the exchange is exact without atomics or a copy.
        [shader("compute")]
        [numthreads(4, 4, 4)]
        void SimFlowCS(uint3 Thread : SV_DispatchThreadID)
        {
            const FSimArgs Args = *GetArgs<FSimArgs>();

            const uint Axis = Args.Control.z;
            const uint Parity = Args.Control.w;

            int3 A = int3(Thread);
            int3 Step = int3(0, 0, 0);

            if (Axis == 0u)      { A.y = A.y * 2 + int(Parity); Step = int3(0, 1, 0); }
            else if (Axis == 1u) { A.x = A.x * 2 + int(Parity); Step = int3(1, 0, 0); }
            else                 { A.z = A.z * 2 + int(Parity); Step = int3(0, 0, 1); }

            const int3 B = A + Step;
            if (any(A < 0) || any(B >= kSimSide))
            {
                return;
            }

            const bool bSpringPass = Axis == 0u && Args.Control.y != 0u;

            // Skipping dry pairs is what keeps an idle volume off the frame, since most blocks never move.
            if (!bSpringPass && BlockIdle(Args.Coarse, A) && BlockIdle(Args.Coarse, B))
            {
                return;
            }

            const uint IndexA = SimIndex(A);
            const uint IndexB = SimIndex(B);

            uint CellA = Args.Grid[IndexA];
            uint CellB = Args.Grid[IndexB];

            const uint SolidA = CellSolid(CellA);
            const uint SolidB = CellSolid(CellB);

            uint MassA = CellMass(CellA);
            uint MassB = CellMass(CellB);

            if (Axis == 0u)
            {
                // B sits above A, so everything B holds falls into whatever room A has left.
                if (SolidA == 0u && SolidB == 0u && MassB > 0u && MassA < kMassFull)
                {
                    const uint Flow = min(MassB, kMassFull - MassA);
                    MassA += Flow;
                    MassB -= Flow;
                }
            }
            else if (SolidA == 0u && SolidB == 0u)
            {
                const int Difference = int(MassA) - int(MassB);

                // A deadband, or a pool spreads into an infinitely thin film across the whole floor.
                if (abs(Difference) > int(kLateralDead))
                {
                    int Flow = Difference / 2;
                    Flow = clamp(Flow, -int(kLateralMax), int(kLateralMax));

                    MassA = uint(int(MassA) - Flow);
                    MassB = uint(int(MassB) + Flow);
                }
            }

            if (bSpringPass)
            {
                // The spring and the drain both ride the vertical pass, which runs once per tick.
                const float3 Spring = float3(Args.Source.xyz);
                const float Radius = float(Args.Control.y);

                if (SolidA == 0u && distance(float3(A), Spring) < Radius)
                {
                    MassA = kMassFull;
                }
                if (SolidB == 0u && distance(float3(B), Spring) < Radius)
                {
                    MassB = kMassFull;
                }

                if (any(A < 8) || any(A >= kSimSide - 8) || A.y < 10) { MassA = 0u; }
                if (any(B < 8) || any(B >= kSimSide - 8) || B.y < 10) { MassB = 0u; }
            }

            if (MassA < kMassMin) { MassA = 0u; }
            if (MassB < kMassMin) { MassB = 0u; }

            Args.Grid[IndexA] = PackCell(SolidA, MassA);
            Args.Grid[IndexB] = PackCell(SolidB, MassB);
        }

        // Rebuilt every tick, because a block that empties has to stop blocking the ray's skip.
        [shader("compute")]
        [numthreads(4, 4, 4)]
        void SimCoarseCS(uint3 Thread : SV_DispatchThreadID)
        {
            const FSimArgs Args = *GetArgs<FSimArgs>();

            if (any(Thread >= uint(kSimCoarseSide)))
            {
                return;
            }

            const int3 Block = int3(Thread) * kSimCoarseStep;
            const uint Both = COARSE_RENDER | COARSE_WATER;
            uint Flags = 0u;

            for (int i = 0; i < kSimCoarseStep * kSimCoarseStep * kSimCoarseStep && Flags != Both; ++i)
            {
                const int3 Offset = int3(i & 7, (i >> 6) & 7, (i >> 3) & 7);
                const uint Cell = Args.Grid[SimIndex(Block + Offset)];
                const uint Mass = CellMass(Cell);

                if (CellSolid(Cell) != 0u || Mass >= kMassRender)
                {
                    Flags |= COARSE_RENDER;
                }
                if (Mass > 0u)
                {
                    Flags |= COARSE_WATER;
                }
            }

            Args.Coarse[CoarseIndex(int3(Thread))] = Flags;
        }

        // A dry block beside a wet one still has to run, or a pool never crosses a block edge.
        [shader("compute")]
        [numthreads(4, 4, 4)]
        void SimDilateCS(uint3 Thread : SV_DispatchThreadID)
        {
            const FSimArgs Args = *GetArgs<FSimArgs>();

            if (any(Thread >= uint(kSimCoarseSide)))
            {
                return;
            }

            const uint Index = CoarseIndex(int3(Thread));
            uint Flags = Args.Coarse[Index] & (COARSE_RENDER | COARSE_WATER);

            if ((Flags & COARSE_WATER) != 0u)
            {
                Args.Coarse[Index] = Flags | COARSE_ACTIVE;
                return;
            }

            // Only bit 1 is read here and only bit 2 is written, so the neighbor reads cannot tear.
            for (int Face = 0; Face < 6; ++Face)
            {
                int3 Neighbor = int3(Thread);
                Neighbor[Face >> 1] += (Face & 1) != 0 ? 1 : -1;

                if (any(Neighbor < 0) || any(Neighbor >= kSimCoarseSide))
                {
                    continue;
                }

                if ((Args.Coarse[CoarseIndex(Neighbor)] & COARSE_WATER) != 0u)
                {
                    Flags |= COARSE_ACTIVE;
                    break;
                }
            }

            Args.Coarse[Index] = Flags;
        }

        struct FDestroyArgs
        {
            FVoxNode* Nodes;
            uint*     Masks;
            uint*     Prefix;
            uint*     Children;
            uint*     SimGrid;
            uint*     SimCoarse;
            float*    Pick;

            // Seven pointers leave the next float4 at offset 56, which straddles a 16 byte boundary.
            uint64_t Pad0;

            float4 Origin;
            float4 Direction;
            float4 SimOrigin;
            float4 Params;
        };

        int3 DestroyCellBase(float3 HitVoxels, int HalfCells)
        {
            return int3(floor(HitVoxels / 8.0)) - HalfCells;
        }

        // One thread marches for the aim point, so the crater center never has to round trip to the CPU.
        [shader("compute")]
        [numthreads(1, 1, 1)]
        void PickCS()
        {
            const FDestroyArgs Args = *GetArgs<FDestroyArgs>();

            const float3 Origin = Args.Origin.xyz;
            const float3 Dir = Args.Direction.xyz;

            float3 InvDir;
            InvDir.x = abs(Dir.x) < 1e-7 ? 1e30 : 1.0 / Dir.x;
            InvDir.y = abs(Dir.y) < 1e-7 ? 1e30 : 1.0 / Dir.y;
            InvDir.z = abs(Dir.z) < 1e-7 ? 1e30 : 1.0 / Dir.z;

            float T = 0.001;
            const float Reach = Args.Params.y;

            Args.Pick[3] = 0.0;

            for (int Step = 0; Step < 512; ++Step)
            {
                if (T >= Reach)
                {
                    return;
                }

                const float3 P = Origin + Dir * T;

                const int3 SimLocal = int3(floor(P - Args.SimOrigin.xyz));
                if (Args.Params.z != 0.0 && all(SimLocal >= 0) && all(SimLocal < kSimSide))
                {
                    const uint Index = uint((SimLocal.y * kSimSide + SimLocal.z) * kSimSide + SimLocal.x);
                    const uint Cell = Args.SimGrid[Index];
                    if (CellSolid(Cell) != 0u || CellMass(Cell) >= kMassRender)
                    {
                        Args.Pick[0] = P.x;
                        Args.Pick[1] = P.y;
                        Args.Pick[2] = P.z;
                        Args.Pick[3] = 1.0;
                        return;
                    }

                    const float3 CellMin = Args.SimOrigin.xyz + float3(SimLocal);
                    const float3 Planes = CellMin + step(float3(0.0), Dir);
                    const float3 Exit = (Planes - Origin) * InvDir;
                    T = max(min(Exit.x, min(Exit.y, Exit.z)), T + 1e-3) + 1e-4;
                    continue;
                }

                uint LeafNode;
                int3 LeafBase;
                const int3 Voxel = int3(floor(P));

                if (FindLeaf(Args.Nodes, Args.Masks, Args.Prefix, Args.Children, Voxel, LeafNode, LeafBase))
                {
                    const int3 Local = Voxel - LeafBase;
                    const uint Slot = uint((Local.y * 8 + Local.z) * 8 + Local.x);

                    if (TestSlot(Args.Masks, LeafNode, Slot))
                    {
                        Args.Pick[0] = P.x;
                        Args.Pick[1] = P.y;
                        Args.Pick[2] = P.z;
                        Args.Pick[3] = 1.0;
                        return;
                    }
                }

                const float3 CellMin = float3(Voxel);
                const float3 Planes = CellMin + step(float3(0.0), Dir);
                const float3 Exit = (Planes - Origin) * InvDir;
                T = max(min(Exit.x, min(Exit.y, Exit.z)), T + 1e-3) + 1e-4;
            }
        }

        // One thread per leaf cell in the crater box, so no two threads ever write the same node.
        [shader("compute")]
        [numthreads(4, 4, 4)]
        void DestroyCS(uint3 Thread : SV_DispatchThreadID)
        {
            const FDestroyArgs Args = *GetArgs<FDestroyArgs>();

            const bool bExplicit = Args.Origin.w != 0.0;

            if (!bExplicit && Args.Pick[3] == 0.0)
            {
                return;
            }

            const float3 Center = bExplicit
                ? Args.Origin.xyz
                : float3(Args.Pick[0], Args.Pick[1], Args.Pick[2]);

            const float Radius = Args.Params.x;

            const int3 CellBase = DestroyCellBase(Center, int(Args.Params.w)) + int3(Thread);
            const int3 VoxelBase = CellBase * 8;

            //~ The dense volume owns its box, so a crater there is cleared directly.

            const int3 SimLocal = VoxelBase - int3(Args.SimOrigin.xyz);
            if (Args.Params.z != 0.0 && all(SimLocal >= 0) && all(SimLocal + 8 <= kSimSide))
            {
                for (int i = 0; i < 512; ++i)
                {
                    const int3 Offset = int3(i & 7, (i >> 6) & 7, (i >> 3) & 7);
                    const float3 P = float3(VoxelBase + Offset) + 0.5;

                    if (distance(P, Center) < Radius)
                    {
                        const int3 L = SimLocal + Offset;
                        Args.SimGrid[uint((L.y * kSimSide + L.z) * kSimSide + L.x)] = 0u;
                    }
                }
                return;
            }

            uint LeafNode;
            int3 LeafBase;
            if (!FindLeaf(Args.Nodes, Args.Masks, Args.Prefix, Args.Children, VoxelBase + 4, LeafNode, LeafBase))
            {
                return;
            }

            uint Clear[16];
            for (int w = 0; w < 16; ++w)
            {
                Clear[w] = 0u;
            }

            bool bAny = false;

            for (int i = 0; i < 512; ++i)
            {
                const int3 Offset = int3(i & 7, (i >> 6) & 7, (i >> 3) & 7);
                const float3 P = float3(LeafBase + Offset) + 0.5;

                if (distance(P, Center) < Radius)
                {
                    const uint Slot = uint((Offset.y * 8 + Offset.z) * 8 + Offset.x);
                    Clear[Slot >> 5u] |= 1u << (Slot & 31u);
                    bAny = true;
                }
            }

            if (!bAny)
            {
                return;
            }

            // Clearing bits never allocates, which is the whole reason destruction can run in place.
            for (int w = 0; w < 16; ++w)
            {
                if (Clear[w] != 0u)
                {
                    Args.Masks[LeafNode * 16u + uint(w)] &= ~Clear[w];
                }
            }

            // A solid leaf ignores its mask, so it has to stop being solid before the hole shows.
            Args.Nodes[LeafNode].Flags &= ~FLAG_SOLID;
        }
    )SLANG";
}
