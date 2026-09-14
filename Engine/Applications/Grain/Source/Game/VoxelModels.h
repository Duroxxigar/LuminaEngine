#pragma once

#include "Containers/Vector.h"
#include "World/VoxelTypes.h"

namespace Grain
{
    enum class EModel : uint32
    {
        Delver = 0,
        Husk,
        Wisp,
        Golem,
        Shard,
        Chest,
        Beacon,
        Bolt,
        Debris,
        Count,
    };

    inline constexpr uint32 kMaxModels = 16;
    inline constexpr uint32 kMaxEntities = 96;

    // One cell of a model is one world voxel, so a 28 cell figure stands 1.75 m tall.
    struct FVoxelModel
    {
        int32          Size[3] = { 0, 0, 0 };
        TVector<uint8> Cells;

        void Reset(int32 X, int32 Y, int32 Z);
        void Box(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1, EMaterial Material);
        void Ball(float CX, float CY, float CZ, float Radius, EMaterial Material);
        void Set(int32 X, int32 Y, int32 Z, EMaterial Material);

        NODISCARD int32 Count() const { return Size[0] * Size[1] * Size[2]; }
    };

    // Mirrored by FModelDesc in the scene module.
    struct FModelDescGpu
    {
        uint32 SizeX = 0;
        uint32 SizeY = 0;
        uint32 SizeZ = 0;
        uint32 Base  = 0;
    };

    class FModelLibrary
    {
    public:

        void Build();

        NODISCARD const FVoxelModel& Get(EModel Model) const { return Models[uint32(Model)]; }
        NODISCARD const TVector<FModelDescGpu>& Descs() const { return GpuDescs; }
        NODISCARD const TVector<uint32>& PackedCells() const { return GpuCells; }

        NODISCARD FVector3 HalfExtentOf(EModel Model, float Scale) const
        {
            const FVoxelModel& M = Models[uint32(Model)];
            return { float(M.Size[0]) * 0.5f * Scale * kVoxelSize,
                     float(M.Size[1]) * 0.5f * Scale * kVoxelSize,
                     float(M.Size[2]) * 0.5f * Scale * kVoxelSize };
        }

    private:

        void Pack();

        FVoxelModel            Models[uint32(EModel::Count)];
        TVector<FModelDescGpu> GpuDescs;
        TVector<uint32>        GpuCells;
    };
}
