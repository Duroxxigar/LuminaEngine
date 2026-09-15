#pragma once

// Shared by both modules, so the traversal cannot drift between the renderer and the compute passes.
namespace Grain::Shaders
{
    constexpr const char* kVoxelCommon = R"SLANG(
        struct FRHIRoot { uint64_t Args; };
        [[vk::push_constant]] FRHIRoot gRHI;
        Ptr<T> GetArgs<T>() { return (T*)gRHI.Args; }

        //~ World layout, mirrored from VoxelTypes.h.

        static const float kVoxelSize = 1.0 / 16.0;
        static const int   kRootX = 5;
        static const int   kRootY = 4;
        static const int   kRootZ = 5;
        static const float kRootSpan = 512.0;

        static const uint FLAG_SOLID   = 1u;
        static const uint FLAG_UNIFORM = 2u;
        static const uint FLAG_PRESENT = 4u;
        static const uint FLAG_LEAF    = 8u;

        static const uint MAT_AIR = 0u, MAT_GRASS = 1u, MAT_DIRT = 2u, MAT_STONE = 3u;
        static const uint MAT_ROCK = 4u, MAT_SAND = 5u, MAT_SNOW = 6u, MAT_WATER = 7u;
        static const uint MAT_WOOD = 8u, MAT_LEAVES = 9u, MAT_GRAVEL = 10u, MAT_CLAY = 11u;
        static const uint MAT_MOSS = 12u, MAT_ORE = 13u, MAT_CRYSTAL = 14u, MAT_LAVA = 15u;
        static const uint MAT_AMETHYST = 16u, MAT_GOLD = 17u, MAT_ICE = 18u, MAT_EMBER = 19u;
        static const uint MAT_OBSIDIAN = 20u, MAT_BONE = 21u, MAT_CLOTH = 22u, MAT_METAL = 23u;
        static const uint MAT_SKIN = 24u, MAT_EYE = 25u, MAT_RUNE = 26u, MAT_HUSK = 27u;
        static const uint MAT_WISP = 28u, MAT_BLOOD = 29u, MAT_SLATE = 30u, MAT_THATCH = 31u;

        static const int kSimSide       = 256;
        static const int kSimCoarseStep = 8;
        static const int kSimCoarseSide = kSimSide / kSimCoarseStep;

        // Bit 0 tells a ray to descend, bit 1 marks water present, bit 2 marks water within one block.
        static const uint COARSE_RENDER = 1u;
        static const uint COARSE_WATER  = 2u;
        static const uint COARSE_ACTIVE = 4u;

        struct FVoxNode
        {
            uint ChildBase;
            uint Palette;
            uint Flags;
            uint PayloadBase;
        };

        //~ A simulated cell carries a solid material and a water volume, so water can be partly full.

        static const uint kMassFull      = 4096u;
        static const uint kMassRender    = 480u;
        static const uint kMassMin       = 48u;
        static const uint kLateralMax    = 1100u;
        static const uint kLateralDead   = 110u;

        uint CellSolid(uint Cell) { return Cell & 0xFFu; }
        uint CellMass(uint Cell)  { return Cell >> 8u; }

        uint PackCell(uint Solid, uint Mass)
        {
            return (Solid & 0xFFu) | (min(Mass, 0xFFFFFFu) << 8u);
        }

        // The traversal reads only these, so a step never copies the whole argument block.
        struct FScene
        {
            FVoxNode* Nodes;
            uint*     Masks;
            uint*     Prefix;
            uint*     Children;
            uint*     Payload;
            uint*     SimGrid;
            uint*     SimCoarse;
            float3    SimOrigin;
            uint      bSim;
        };

        uint LoadMaskWord(uint* Masks, uint NodeIndex, uint Word)
        {
            return Masks[NodeIndex * 16u + Word];
        }

        bool TestSlot(uint* Masks, uint NodeIndex, uint Slot)
        {
            return (LoadMaskWord(Masks, NodeIndex, Slot >> 5u) & (1u << (Slot & 31u))) != 0u;
        }

        // Two loads and one popcount, where summing the words below the slot cost about eight.
        uint RankOfSlot(uint* Masks, uint* Prefix, uint NodeIndex, uint Slot)
        {
            const uint Word = Slot >> 5u;
            const uint Packed = Prefix[NodeIndex * 8u + (Word >> 1u)];
            const uint Below = (Word & 1u) != 0u ? (Packed >> 16u) : (Packed & 0xFFFFu);

            return Below + countbits(LoadMaskWord(Masks, NodeIndex, Word) & ((1u << (Slot & 31u)) - 1u));
        }

        uint LeafMaterial(uint* Payload, FVoxNode Node, uint Slot)
        {
            if ((Node.Flags & FLAG_UNIFORM) != 0u)
            {
                return Node.Palette & 0xFFu;
            }

            const uint Word = Payload[Node.PayloadBase + (Slot >> 4u)];
            const uint Index = (Word >> ((Slot & 15u) * 2u)) & 3u;
            return (Node.Palette >> (Index * 8u)) & 0xFFu;
        }

        // A collapsed interior node has no per voxel mask, so digging stops where subdivision stops.
        bool FindLeaf(FVoxNode* Nodes, uint* Masks, uint* Prefix, uint* Children, int3 Voxel,
                      out uint OutNode, out int3 OutBase)
        {
            OutNode = 0u;
            OutBase = int3(0);

            const int3 Root = Voxel >> 9;
            if (any(Root < 0) || Root.x >= kRootX || Root.y >= kRootY || Root.z >= kRootZ)
            {
                return false;
            }

            uint NodeIndex = uint((Root.y * kRootZ + Root.z) * kRootX + Root.x);
            FVoxNode Node = Nodes[NodeIndex];

            if ((Node.Flags & FLAG_PRESENT) == 0u)
            {
                return false;
            }

            int3 NodeMin = Root << 9;
            int NodeSize = 512;

            for (int Level = 0; Level < 3; ++Level)
            {
                if ((Node.Flags & FLAG_LEAF) != 0u)
                {
                    OutNode = NodeIndex;
                    OutBase = NodeMin;
                    return true;
                }

                if ((Node.Flags & FLAG_SOLID) != 0u)
                {
                    return false;
                }

                const int ChildSize = NodeSize >> 3;
                const int3 Local = (Voxel - NodeMin) / ChildSize;
                const uint Slot = uint((Local.y * 8 + Local.z) * 8 + Local.x);

                if (!TestSlot(Masks, NodeIndex, Slot))
                {
                    return false;
                }

                NodeIndex = Children[Node.ChildBase + RankOfSlot(Masks, Prefix, NodeIndex, Slot)];
                Node = Nodes[NodeIndex];
                NodeMin += Local * ChildSize;
                NodeSize = ChildSize;
            }

            return false;
        }

        struct FCell
        {
            bool   bSolid;
            uint   Material;
            float3 Min;
            float  Size;
        };

        bool InsideSim(FScene S, float3 P)
        {
            if (S.bSim == 0u)
            {
                return false;
            }
            const float3 Local = P - S.SimOrigin;
            return all(Local >= 0.0) && all(Local < float(kSimSide));
        }

        // The dense volume owns its box outright, so a cell that moved there never disagrees with the tree.
        FCell ResolveSim(FScene S, float3 P)
        {
            FCell Out;
            Out.bSolid = false;
            Out.Material = MAT_AIR;

            const int3 Local = int3(floor(P - S.SimOrigin));
            const int3 Block = Local >> 3;

            const uint BlockIndex = uint((Block.y * kSimCoarseSide + Block.z) * kSimCoarseSide + Block.x);
            if ((S.SimCoarse[BlockIndex] & COARSE_RENDER) == 0u)
            {
                Out.Min = S.SimOrigin + float3(Block << 3);
                Out.Size = float(kSimCoarseStep);
                return Out;
            }

            const uint Index = uint((Local.y * kSimSide + Local.z) * kSimSide + Local.x);
            const uint Cell = S.SimGrid[Index];
            const uint Solid = CellSolid(Cell);
            const uint Mass = CellMass(Cell);

            Out.Min = S.SimOrigin + float3(Local);
            Out.Size = 1.0;

            if (Solid != 0u)
            {
                Out.bSolid = true;
                Out.Material = Solid;
            }
            else if (Mass >= kMassRender)
            {
                Out.bSolid = true;
                Out.Material = MAT_WATER;
            }

            return Out;
        }

        // A miss reports the empty cell it landed in, which is what lets the ray skip whole subtrees.
        FCell Resolve(FScene S, float3 P, float MinCell)
        {
            if (InsideSim(S, P))
            {
                return ResolveSim(S, P);
            }

            FCell Out;
            Out.bSolid = false;
            Out.Material = MAT_AIR;
            Out.Min = float3(0.0);
            Out.Size = kRootSpan;

            const int3 Root = int3(floor(P / kRootSpan));
            if (Root.x < 0 || Root.y < 0 || Root.z < 0 || Root.x >= kRootX || Root.y >= kRootY || Root.z >= kRootZ)
            {
                Out.Min = float3(Root) * kRootSpan;
                return Out;
            }

            uint NodeIndex = uint((Root.y * kRootZ + Root.z) * kRootX + Root.x);
            FVoxNode Node = S.Nodes[NodeIndex];

            float3 NodeMin = float3(Root) * kRootSpan;
            float NodeSize = kRootSpan;

            if ((Node.Flags & FLAG_PRESENT) == 0u)
            {
                Out.Min = NodeMin;
                Out.Size = NodeSize;
                return Out;
            }

            for (int Level = 0; Level < 3; ++Level)
            {
                if ((Node.Flags & FLAG_SOLID) != 0u)
                {
                    Out.bSolid = true;
                    Out.Material = Node.Palette & 0xFFu;
                    Out.Min = NodeMin;
                    Out.Size = NodeSize;
                    return Out;
                }

                const float ChildSize = NodeSize * 0.125;
                int3 Local = int3(floor((P - NodeMin) / ChildSize));
                Local = clamp(Local, int3(0), int3(7));

                const uint Slot = uint((Local.y * 8 + Local.z) * 8 + Local.x);
                const float3 ChildMin = NodeMin + float3(Local) * ChildSize;

                if (!TestSlot(S.Masks, NodeIndex, Slot))
                {
                    Out.Min = ChildMin;
                    Out.Size = ChildSize;
                    return Out;
                }

                if (S.bSim != 0u && ChildSize > float(kSimCoarseStep))
                {
                    const float3 SimMax = S.SimOrigin + float(kSimSide);
                    if (all(ChildMin < SimMax) && all(ChildMin + ChildSize > S.SimOrigin))
                    {
                        // Descend rather than trust a coarse cell that straddles the simulated box.
                        MinCell = 0.0;
                    }
                }

                // Stopping below a pixel's footprint removes the aliasing one ray per voxel would give.
                const bool bStop = ChildSize <= MinCell;

                if ((Node.Flags & FLAG_LEAF) != 0u)
                {
                    Out.bSolid = true;
                    Out.Material = bStop ? (Node.Palette & 0xFFu) : LeafMaterial(S.Payload, Node, Slot);
                    Out.Min = ChildMin;
                    Out.Size = ChildSize;
                    return Out;
                }

                const uint ChildIndex = S.Children[Node.ChildBase + RankOfSlot(S.Masks, S.Prefix, NodeIndex, Slot)];

                if (bStop)
                {
                    Out.bSolid = true;
                    Out.Material = S.Nodes[ChildIndex].Palette & 0xFFu;
                    Out.Min = ChildMin;
                    Out.Size = ChildSize;
                    return Out;
                }

                NodeIndex = ChildIndex;
                Node = S.Nodes[NodeIndex];
                NodeMin = ChildMin;
                NodeSize = ChildSize;
            }

            Out.Min = NodeMin;
            Out.Size = NodeSize;
            return Out;
        }

        float3 SafeInverse(float3 D)
        {
            float3 R;
            R.x = abs(D.x) < 1e-7 ? 1e30 : 1.0 / D.x;
            R.y = abs(D.y) < 1e-7 ? 1e30 : 1.0 / D.y;
            R.z = abs(D.z) < 1e-7 ? 1e30 : 1.0 / D.z;
            return R;
        }

        float ExitDistance(float3 Origin, float3 Dir, float3 InvDir, float3 CellMin, float CellSize)
        {
            const float3 Planes = CellMin + step(float3(0.0), Dir) * CellSize;
            const float3 T = (Planes - Origin) * InvDir;
            return min(T.x, min(T.y, T.z));
        }

        float3 EntryNormal(float3 Origin, float3 Dir, float3 InvDir, float3 CellMin, float CellSize)
        {
            const float3 Planes = CellMin + step(Dir, float3(0.0)) * CellSize;
            const float3 T = (Planes - Origin) * InvDir;

            if (T.x > T.y && T.x > T.z)
            {
                return float3(Dir.x > 0.0 ? -1.0 : 1.0, 0.0, 0.0);
            }
            if (T.y > T.z)
            {
                return float3(0.0, Dir.y > 0.0 ? -1.0 : 1.0, 0.0);
            }
            return float3(0.0, 0.0, Dir.z > 0.0 ? -1.0 : 1.0);
        }

        bool ClipToWorld(float3 Origin, float3 InvDir, out float TMin, out float TMax)
        {
            const float3 BoxMax = float3(kRootX, kRootY, kRootZ) * kRootSpan;

            const float3 A = (float3(0.0) - Origin) * InvDir;
            const float3 B = (BoxMax - Origin) * InvDir;
            const float3 Near = min(A, B);
            const float3 Far  = max(A, B);

            TMin = max(max(Near.x, Near.y), max(Near.z, 0.0));
            TMax = min(Far.x, min(Far.y, Far.z));
            return TMax > TMin;
        }

        struct FHit
        {
            bool   bHit;
            // Spending the whole step budget is neither a hit nor a view of open sky.
            bool   bExhausted;
            float  T;
            uint   Material;
            float3 Normal;
            float3 Position;
        };

        FHit March(FScene S, float3 Origin, float3 Dir, float MaxDistance, int MaxSteps, float PixelScale)
        {
            FHit Out;
            Out.bHit = false;
            Out.bExhausted = false;
            Out.T = MaxDistance;
            Out.Material = MAT_AIR;
            Out.Normal = float3(0.0, 1.0, 0.0);
            Out.Position = Origin;

            const float3 InvDir = SafeInverse(Dir);

            float TMin;
            float TMax;
            if (!ClipToWorld(Origin, InvDir, TMin, TMax))
            {
                return Out;
            }

            float T = max(TMin, 0.0) + 1e-4;
            TMax = min(TMax, MaxDistance);

            for (int Step = 0; Step < MaxSteps; ++Step)
            {
                if (T >= TMax)
                {
                    return Out;
                }

                const float3 P = Origin + Dir * T;
                const FCell Cell = Resolve(S, P, T * PixelScale);

                if (Cell.bSolid)
                {
                    Out.bHit = true;
                    Out.T = T;
                    Out.Material = Cell.Material;
                    Out.Normal = EntryNormal(Origin, Dir, InvDir, Cell.Min, Cell.Size);
                    Out.Position = P;
                    return Out;
                }

                const float Next = ExitDistance(Origin, Dir, InvDir, Cell.Min, Cell.Size);
                T = max(Next, T + 1e-3) + 1e-4;
            }

            Out.bExhausted = true;
            return Out;
        }

        // A ray that dies inside deep rock counts as blocked, or the caves fill with daylight.
        bool MarchOccluded(FScene S, float3 Origin, float3 Dir, float MaxDistance, int MaxSteps, float PixelScale)
        {
            const float3 InvDir = SafeInverse(Dir);

            float TMin;
            float TMax;
            if (!ClipToWorld(Origin, InvDir, TMin, TMax))
            {
                return false;
            }

            float T = max(TMin, 0.0) + 1e-4;
            TMax = min(TMax, MaxDistance);

            for (int Step = 0; Step < MaxSteps; ++Step)
            {
                if (T >= TMax)
                {
                    return false;
                }

                const FCell Cell = Resolve(S, Origin + Dir * T, T * PixelScale);
                if (Cell.bSolid && Cell.Material != MAT_WATER)
                {
                    return true;
                }

                const float Next = ExitDistance(Origin, Dir, InvDir, Cell.Min, Cell.Size);
                T = max(Next, T + 1e-3) + 1e-4;
            }

            return true;
        }
    )SLANG";
}
