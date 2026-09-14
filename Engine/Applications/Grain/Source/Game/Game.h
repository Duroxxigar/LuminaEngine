#pragma once

#include "VoxelModels.h"
#include "VoxelPhysics.h"

#include "Containers/String.h"
#include "Containers/Vector.h"
#include "World/Sky.h"

namespace Grain
{
    class FRenderer;

    enum class EEntityKind : uint8
    {
        None = 0,
        Husk,
        Wisp,
        Golem,
        Shard,
        Chest,
        Beacon,
        Bolt,
        Debris,
        Spark,
    };

    struct FEntity
    {
        FBodyState  Body;
        FVector3    Tint { 1.0f, 1.0f, 1.0f };
        EEntityKind Kind = EEntityKind::None;
        EModel      Model = EModel::Debris;

        float Yaw = 0.0f;
        float Scale = 1.0f;
        float Emissive = 0.0f;
        float Health = 1.0f;
        float MaxHealth = 1.0f;
        float Cooldown = 0.0f;
        float Age = 0.0f;
        float Lifetime = 0.0f;
        float HurtFlash = 0.0f;
        float Bob = 0.0f;

        uint32 Owner = 0;
        bool   bActive = false;
        bool   bGravity = true;
        bool   bLit = false;
    };

    struct FPlayerState
    {
        FBodyState Body;

        float Yaw = 0.0f;
        float Pitch = -0.12f;

        float Health = 100.0f;
        float MaxHealth = 100.0f;
        float Stamina = 100.0f;
        float MaxStamina = 100.0f;
        float Focus = 60.0f;
        float MaxFocus = 60.0f;

        int32 Level = 1;
        int32 Experience = 0;
        int32 Shards = 0;
        int32 Kills = 0;

        float SwingTimer = 0.0f;
        float AttackCooldown = 0.0f;
        float CastCooldown = 0.0f;
        float HurtTimer = 0.0f;
        float Recovery = 0.0f;
        float DeathTimer = 0.0f;

        bool bSprinting = false;
        bool bAlive = true;
    };

    struct FFloatingText
    {
        FVector3 World { 0.0f, 0.0f, 0.0f };
        FVector3 Color { 1.0f, 1.0f, 1.0f };
        FString  Text;
        float    Age = 0.0f;
        float    Life = 1.4f;
    };

    struct FGameInput
    {
        FVector2 Move { 0.0f, 0.0f };
        float    LookX = 0.0f;
        float    LookY = 0.0f;

        bool bJump = false;
        bool bSprint = false;
        bool bAttack = false;
        bool bCast = false;
        bool bInteract = false;
    };

    struct FBanner
    {
        FString Text;
        float   Age = 0.0f;
        float   Life = 0.0f;
    };

    // One objective of the run, tracked by count so the HUD can show progress without special cases.
    struct FObjective
    {
        FString Text;
        int32   Progress = 0;
        int32   Target = 1;
        bool    bComplete = false;
    };

    class FGame
    {
    public:

        void Initialize(FVoxelWorld& InWorld, uint32 Seed);
        void SetTimeOfDay(float Phase) { TimeOfDay = Phase; Sky = EvaluateSky(Phase); }
        void SetDayLength(float Seconds) { DayLength = Math::Max(Seconds, 1.0f); }
        void Update(float Delta, const FGameInput& Input, FRenderer& Renderer);

        NODISCARD const FPlayerState& GetPlayer() const { return Player; }
        NODISCARD const TVector<FEntity>& GetEntities() const { return Entities; }
        NODISCARD const TVector<FFloatingText>& GetFloatingText() const { return Floaters; }
        NODISCARD const TVector<FObjective>& GetObjectives() const { return Objectives; }
        NODISCARD const FBanner& GetBanner() const { return Banner; }
        NODISCARD const FModelLibrary& GetModels() const { return Models; }

        NODISCARD float GetTimeOfDay() const { return TimeOfDay; }
        NODISCARD const FSkyState& GetSky() const { return Sky; }
        NODISCARD int32 GetExperienceForNext() const { return 60 + Player.Level * Player.Level * 40; }
        NODISCARD int32 GetBeaconsLit() const { return BeaconsLit; }
        NODISCARD bool IsComplete() const { return bComplete; }

        // Where the third person boom lands after it is pulled in past anything solid.
        NODISCARD FVector3 GetCameraPosition() const { return CameraPosition; }
        NODISCARD bool IsFirstPerson() const { return bFirstPerson; }
        NODISCARD FVector3 GetEyePosition() const;
        NODISCARD FVector3 GetLookDirection() const;

        NODISCARD const char* GetPrompt() const { return Prompt; }
        NODISCARD int32 CountOf(EEntityKind Kind) const;

    private:

        void SpawnPlayer();
        void PlaceBeacons();

        FEntity* Allocate();
        FEntity* AllocateScrap(EEntityKind Kind, int32 Budget);
        void SpawnEnemy(EEntityKind Kind, const FVector3& Position);
        void SpawnShard(const FVector3& Position, int32 Count);
        void SpawnDebris(const FVector3& Position, const FVector3& Impulse, uint8 Material, int32 Count);
        void SpawnSpark(const FVector3& Position, const FVector3& Color, int32 Count);

        void UpdatePlayer(float Delta, const FGameInput& Input, FRenderer& Renderer);
        void UpdateEntities(float Delta, FRenderer& Renderer);
        void UpdateCamera(float Delta);
        void UpdateSpawning(float Delta);

        void Swing(FRenderer& Renderer);
        void Cast();
        void Explode(const FVector3& Center, float Radius, float Damage, FRenderer& Renderer);
        void DamagePlayer(float Amount);
        void DamageEntity(FEntity& Target, float Amount, const FVector3& From);
        void GrantExperience(int32 Amount);

        void Say(const char* Text, float Life = 3.2f);
        void Floater(const FVector3& World, const FString& Text, const FVector3& Color);
        void RefreshObjectives();

        NODISCARD float RandomUnit();
        NODISCARD FVector3 RandomHorizontal();

        FVoxelWorld*  World = nullptr;
        FModelLibrary Models;

        FPlayerState          Player;
        TVector<FEntity>      Entities;
        TVector<FFloatingText> Floaters;
        TVector<FObjective>   Objectives;
        FBanner               Banner;
        FSkyState             Sky;

        FVector3 CameraPosition { 0.0f, 0.0f, 0.0f };
        float    CameraBoom = 4.2f;
        bool     bFirstPerson = false;

        float TimeOfDay = 0.24f;
        float DayLength = 480.0f;
        float SpawnTimer = 3.0f;
        float ShardTimer = 0.0f;
        uint32 RandomState = 1u;

        int32 BeaconsLit = 0;
        int32 TotalBeacons = 0;
        bool  bComplete = false;

        const char* Prompt = "";
    };
}
