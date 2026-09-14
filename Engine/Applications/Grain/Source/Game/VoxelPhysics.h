#pragma once

#include "World/VoxelWorld.h"

namespace Grain
{
    struct FVoxelRayHit
    {
        FVector3 Position { 0.0f, 0.0f, 0.0f };
        FVector3 Normal { 0.0f, 1.0f, 0.0f };
        float    Distance = 0.0f;
        uint8    Material = 0;
        bool     bHit = false;
    };

    struct FBodyState
    {
        FVector3 Position { 0.0f, 0.0f, 0.0f };
        FVector3 Velocity { 0.0f, 0.0f, 0.0f };
        FVector3 HalfExtent { 0.30f, 0.90f, 0.30f };

        // How much of the box sits below the water line, which is what buoyancy balances against.
        float Submersion = 0.0f;

        bool bGrounded = false;
        bool bInWater = false;
        bool bHitWall = false;
        bool bHitCeiling = false;
    };

    // Axis separated sweeps against the built tree, which is what a blocky world actually wants.
    class FVoxelPhysics
    {
    public:

        explicit FVoxelPhysics(const FVoxelWorld& InWorld) : World(&InWorld) {}

        NODISCARD bool OverlapsSolid(const FVector3& Center, const FVector3& HalfExtent) const;

        // Moves a box by Delta, stopping against the first solid voxel on each axis in turn.
        void MoveBody(FBodyState& Body, const FVector3& Delta, float StepHeight) const;

        NODISCARD FVoxelRayHit Raycast(const FVector3& Origin, const FVector3& Direction, float MaxDistance) const;

        // Drops a probe until it finds standing room, so a spawn never starts inside rock.
        NODISCARD bool FindGround(float WorldX, float WorldZ, float StartY, float& OutY) const;

        NODISCARD static bool IsUnderwater(const FVector3& Position)
        {
            return Position.y < kSeaLevel;
        }

    private:

        NODISCARD bool SweepAxis(const FVector3& Center, const FVector3& HalfExtent,
                                 int32 Axis, float Amount, float& OutAllowed) const;

        const FVoxelWorld* World = nullptr;
    };
}
