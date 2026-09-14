#include "VoxelPhysics.h"

namespace Grain
{
    namespace
    {
        constexpr float kSkin = 0.004f;

        int32 VoxelFloor(float World)
        {
            return int32(Math::Floor(World / kVoxelSize));
        }
    }

    bool FVoxelPhysics::OverlapsSolid(const FVector3& Center, const FVector3& HalfExtent) const
    {
        const int32 Min[3] =
        {
            VoxelFloor(Center.x - HalfExtent.x),
            VoxelFloor(Center.y - HalfExtent.y),
            VoxelFloor(Center.z - HalfExtent.z),
        };

        const int32 Max[3] =
        {
            VoxelFloor(Center.x + HalfExtent.x),
            VoxelFloor(Center.y + HalfExtent.y),
            VoxelFloor(Center.z + HalfExtent.z),
        };

        return World->OverlapsBox(Min, Max);
    }

    bool FVoxelPhysics::SweepAxis(const FVector3& Center, const FVector3& HalfExtent,
                                  int32 Axis, float Amount, float& OutAllowed) const
    {
        OutAllowed = Amount;

        if (Math::Abs(Amount) < 1e-6f)
        {
            return false;
        }

        // One voxel a step, so a fast body never tunnels through a single layer of ground.
        const int32 Steps = Math::Max(1, int32(Math::Abs(Amount) / (kVoxelSize * 0.9f)) + 1);
        const float Increment = Amount / float(Steps);

        FVector3 Probe = Center;
        float Covered = 0.0f;

        for (int32 Step = 0; Step < Steps; ++Step)
        {
            FVector3 Next = Probe;
            (&Next.x)[Axis] += Increment;

            if (OverlapsSolid(Next, HalfExtent))
            {
                OutAllowed = Covered - (Amount > 0.0f ? kSkin : -kSkin);
                if ((Amount > 0.0f && OutAllowed < 0.0f) || (Amount < 0.0f && OutAllowed > 0.0f))
                {
                    OutAllowed = 0.0f;
                }
                return true;
            }

            Probe = Next;
            Covered += Increment;
        }

        return false;
    }

    void FVoxelPhysics::MoveBody(FBodyState& Body, const FVector3& Delta, float StepHeight) const
    {
        Body.bGrounded = false;
        Body.bHitWall = false;
        Body.bHitCeiling = false;

        //~ Vertical first, so a body is standing on the floor before it tries to walk along it.

        float Allowed = 0.0f;
        if (SweepAxis(Body.Position, Body.HalfExtent, 1, Delta.y, Allowed))
        {
            Body.Position.y += Allowed;
            if (Delta.y <= 0.0f)
            {
                Body.bGrounded = true;
            }
            else
            {
                Body.bHitCeiling = true;
            }
            Body.Velocity.y = 0.0f;
        }
        else
        {
            Body.Position.y += Delta.y;
        }

        //~ Horizontal, with one attempt to step up whatever it ran into.

        for (int32 Axis = 0; Axis < 3; Axis += 2)
        {
            const float Amount = (&Delta.x)[Axis];
            if (Math::Abs(Amount) < 1e-6f)
            {
                continue;
            }

            if (!SweepAxis(Body.Position, Body.HalfExtent, Axis, Amount, Allowed))
            {
                (&Body.Position.x)[Axis] += Amount;
                continue;
            }

            bool bStepped = false;
            if (StepHeight > 0.0f && Body.bGrounded)
            {
                FVector3 Raised = Body.Position;
                Raised.y += StepHeight;

                if (!OverlapsSolid(Raised, Body.HalfExtent))
                {
                    float RaisedAllowed = 0.0f;
                    if (!SweepAxis(Raised, Body.HalfExtent, Axis, Amount, RaisedAllowed))
                    {
                        (&Raised.x)[Axis] += Amount;

                        // Settle back down, or a step up would leave the body hovering.
                        float Drop = 0.0f;
                        SweepAxis(Raised, Body.HalfExtent, 1, -StepHeight, Drop);
                        Raised.y += Drop;

                        Body.Position = Raised;
                        bStepped = true;
                    }
                }
            }

            if (!bStepped)
            {
                (&Body.Position.x)[Axis] += Allowed;
                (&Body.Velocity.x)[Axis] = 0.0f;
                Body.bHitWall = true;
            }
        }

        //~ A body resting exactly on a surface reports no downward hit, so the floor is probed once more.

        if (!Body.bGrounded)
        {
            FVector3 Probe = Body.Position;
            Probe.y -= kVoxelSize * 0.35f;
            Body.bGrounded = OverlapsSolid(Probe, Body.HalfExtent);
        }

        const float Height = Math::Max(Body.HalfExtent.y * 2.0f, 0.01f);
        Body.Submersion = Math::Clamp((kSeaLevel - (Body.Position.y - Body.HalfExtent.y)) / Height,
                                      0.0f, 1.0f);
        Body.bInWater = Body.Submersion > 0.10f;
    }

    FVoxelRayHit FVoxelPhysics::Raycast(const FVector3& Origin, const FVector3& Direction,
                                        float MaxDistance) const
    {
        FVoxelRayHit Out;

        const float Length = Math::Sqrt(Direction.x * Direction.x + Direction.y * Direction.y
                                      + Direction.z * Direction.z);
        if (Length < 1e-6f)
        {
            return Out;
        }

        const FVector3 Dir { Direction.x / Length, Direction.y / Length, Direction.z / Length };

        int32 Voxel[3] =
        {
            VoxelFloor(Origin.x), VoxelFloor(Origin.y), VoxelFloor(Origin.z),
        };

        int32 Stride[3] = { 0, 0, 0 };
        float Next[3] = { 0.0f, 0.0f, 0.0f };
        float Advance[3] = { 0.0f, 0.0f, 0.0f };

        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            const float Component = (&Dir.x)[Axis];
            const float Start = (&Origin.x)[Axis] / kVoxelSize;

            if (Math::Abs(Component) < 1e-8f)
            {
                Stride[Axis] = 0;
                Next[Axis] = 1e30f;
                Advance[Axis] = 1e30f;
                continue;
            }

            Stride[Axis] = Component > 0.0f ? 1 : -1;
            Advance[Axis] = kVoxelSize / Math::Abs(Component);

            const float Boundary = Component > 0.0f
                ? float(Voxel[Axis] + 1) - Start
                : Start - float(Voxel[Axis]);

            Next[Axis] = Boundary * kVoxelSize / Math::Abs(Component);
        }

        float Distance = 0.0f;
        int32 LastAxis = 1;

        for (int32 Step = 0; Step < 4096 && Distance <= MaxDistance; ++Step)
        {
            if (World->IsSolidVoxel(Voxel[0], Voxel[1], Voxel[2]))
            {
                Out.bHit = true;
                Out.Distance = Distance;
                Out.Position = { Origin.x + Dir.x * Distance,
                                 Origin.y + Dir.y * Distance,
                                 Origin.z + Dir.z * Distance };
                Out.Material = World->MaterialAtVoxel(Voxel[0], Voxel[1], Voxel[2]);
                Out.Normal = { 0.0f, 0.0f, 0.0f };
                (&Out.Normal.x)[LastAxis] = float(-Stride[LastAxis]);
                return Out;
            }

            LastAxis = Next[0] < Next[1] ? (Next[0] < Next[2] ? 0 : 2) : (Next[1] < Next[2] ? 1 : 2);

            Distance = Next[LastAxis];
            Voxel[LastAxis] += Stride[LastAxis];
            Next[LastAxis] += Advance[LastAxis];
        }

        Out.Distance = MaxDistance;
        Out.Position = { Origin.x + Dir.x * MaxDistance,
                         Origin.y + Dir.y * MaxDistance,
                         Origin.z + Dir.z * MaxDistance };
        return Out;
    }

    bool FVoxelPhysics::FindGround(float WorldX, float WorldZ, float StartY, float& OutY) const
    {
        const int32 X = VoxelFloor(WorldX);
        const int32 Z = VoxelFloor(WorldZ);

        int32 Y = Math::Clamp(VoxelFloor(StartY), 1, kWorldVoxelsY - 2);

        // Climb out first, or a probe that starts buried reports the roof of its own pocket.
        while (Y < kWorldVoxelsY - 2 && World->IsSolidVoxel(X, Y, Z))
        {
            ++Y;
        }

        while (Y > 1)
        {
            if (World->IsSolidVoxel(X, Y - 1, Z))
            {
                OutY = float(Y) * kVoxelSize;
                return true;
            }
            --Y;
        }

        return false;
    }
}
