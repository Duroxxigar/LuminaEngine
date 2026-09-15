#pragma once

#include "World/Camera.h"
#include "World/Sky.h"
#include "World/VoxelSim.h"
#include "World/VoxelWorld.h"

#include "Containers/Function.h"
#include "Renderer/RHI.h"
#include "Renderer/RHITexture.h"
#include "Renderer/RHIUtils.h"

namespace Grain
{
    inline constexpr int32 kBloomLevels = 5;
    inline constexpr int32 kAtrousPasses = 4;
    inline constexpr int32 kMaxDestroyPerFrame = 8;
    inline constexpr int32 kMaxRenderEntities = 96;

    struct FViewArgs
    {
        RHI::GPUPtr Nodes     = 0;
        RHI::GPUPtr Masks     = 0;
        RHI::GPUPtr Prefix    = 0;
        RHI::GPUPtr Children  = 0;
        RHI::GPUPtr Payload   = 0;
        RHI::GPUPtr SimGrid   = 0;
        RHI::GPUPtr SimCoarse = 0;

        RHI::GPUPtr Entities   = 0;
        RHI::GPUPtr Models     = 0;
        RHI::GPUPtr ModelCells = 0;

        FVector4 CameraPos;
        FVector4 CameraFwd;
        FVector4 CameraRight;
        FVector4 CameraUp;
        FVector4 SunDir;
        FVector4 MoonDir;
        FVector4 SunColor;
        FVector4 SkyZenith;
        FVector4 SkyHorizon;
        FVector4 SkyGround;
        FVector4 Params;
        FVector4 Jitter;
        FVector4 FogParams;
        FVector4 SimOrigin;
        FVector4 EntityBounds;

        uint32 EntityCount = 0;
        uint32 Unused0     = 0;
        uint32 DebugMode   = 0;
        uint32 bSim        = 0;
    };

    static_assert(sizeof(FViewArgs) == 336, "The Slang mirror expects a packed 336 byte block.");

    // Shared by the temporal, a trous, compose and antialiasing passes, since they need the same basis.
    struct FDenoiseArgs
    {
        uint32 IDs[4]   = { 0, 0, 0, 0 };
        uint32 Extra[4] = { 0, 0, 0, 0 };

        FVector4 Params;
        FVector4 CameraPos;
        FVector4 CameraFwd;
        FVector4 CameraRight;
        FVector4 CameraUp;
        FVector4 PrevPos;
        FVector4 PrevFwd;
        FVector4 PrevRight;
        FVector4 PrevUp;
        FVector4 SunDir;
        FVector4 FogParams;
        FVector4 SkyHorizon;
        FVector4 SkyZenith;
        FVector4 Misc;
    };

    static_assert(sizeof(FDenoiseArgs) == 256, "The Slang mirror expects a packed 256 byte block.");

    struct FDestroyArgs
    {
        RHI::GPUPtr Nodes     = 0;
        RHI::GPUPtr Masks     = 0;
        RHI::GPUPtr Prefix    = 0;
        RHI::GPUPtr Children  = 0;
        RHI::GPUPtr SimGrid   = 0;
        RHI::GPUPtr SimCoarse = 0;
        RHI::GPUPtr Pick      = 0;

        // Seven pointers leave the next float4 at offset 56, which straddles a 16 byte boundary.
        RHI::GPUPtr Pad0      = 0;

        FVector4 Origin;
        FVector4 Direction;
        FVector4 SimOrigin;
        FVector4 Params;
    };

    static_assert(sizeof(FDestroyArgs) == 128, "The Slang mirror expects a packed 128 byte block.");

    // An aimed dig resolves its center on the GPU, while an explosion already knows where it landed.
    struct FDestroyRequest
    {
        FVector3 Center { 0.0f, 0.0f, 0.0f };
        FVector3 Origin { 0.0f, 0.0f, 0.0f };
        FVector3 Direction { 0.0f, 0.0f, 1.0f };
        float    Radius = 1.0f;
        float    Reach = 90.0f;
        bool     bExplicit = false;
    };

    // Mirrored by FEntityGpu in the scene module.
    struct FEntityGpu
    {
        FVector3 Position { 0.0f, 0.0f, 0.0f };
        float    Yaw = 0.0f;
        FVector3 Half { 0.0f, 0.0f, 0.0f };
        uint32   Model = 0;
        FVector3 Tint { 1.0f, 1.0f, 1.0f };
        float    Emissive = 0.0f;
    };

    static_assert(sizeof(FEntityGpu) == 48, "The Slang mirror expects a packed 48 byte block.");

    struct FFrameTint
    {
        FVector3 Color { 1.0f, 1.0f, 1.0f };
        float    Saturation = 1.16f;
        float    Contrast = 1.10f;
        float    ExposureScale = 1.0f;
    };

    class FRenderer
    {
    public:

        bool Initialize(EFormat InSwapchainFormat);
        void Shutdown();

        void EnsureTargets(const FUIntVector2& Extent);
        void SetDebugMode(uint32 Mode) { DebugMode = Mode; }

        void SetTemporal(bool bEnabled) { bTemporal = bEnabled; }
        NODISCARD bool IsTemporalEnabled() const { return bTemporal; }

        void SetFilter(bool bEnabled) { bFilter = bEnabled; }
        NODISCARD bool IsFilterEnabled() const { return bFilter; }

        void SetAntialiasing(bool bEnabled) { bAntialias = bEnabled; }
        NODISCARD bool IsAntialiasingEnabled() const { return bAntialias; }

        void SetSky(const FSkyState& InSky) { Sky = InSky; }

        // Uploaded once, since a model library never changes after the game starts.
        bool UploadModels(TSpan<const uint32> Descs, TSpan<const uint32> Cells);
        void SetEntities(TSpan<const FEntityGpu> InEntities);

        void SetTint(const FFrameTint& InTint) { Tint = InTint; }
        void SetOverlay(uint32 SampledSlot) { OverlaySlot = SampledSlot; }

        // Queued rather than issued, because destruction has to land before the frame's raymarch.
        void QueueDestroy(const FDestroyRequest& Request);

        void Render(RHI::FCmdListH CL, RHI::FTextureH SwapImage, const FUIntVector2& Extent,
                    const FVoxelWorld& World, const FVoxelSim& Sim, const FCamera& Camera,
                    float RealTime, float DeltaTime);

        // The overlay draws itself, so the capture takes a callback rather than a texture.
        bool CaptureToFile(const FUIntVector2& Extent, const char* Path,
                           const TFunction<void(RHI::FCmdListH, RHI::FTextureH)>& DrawOverlay = {});

        void EnableGpuTimers();
        void ReportGpuTimers() const;

    private:

        bool CreatePipelines();
        void ReleaseTargets();

        void FillViewBasis(FDenoiseArgs& Args, const FUIntVector2& Extent, const FCamera& Camera) const;
        void FillViewArgs(FViewArgs& Args, const FUIntVector2& Extent, const FVoxelWorld& World,
                          const FVoxelSim& Sim, const FCamera& Camera, float RealTime) const;

        void StepSim(RHI::FCmdListH CL, const FVoxelSim& Sim, float DeltaTime);
        void RunDestroy(RHI::FCmdListH CL, const FVoxelWorld& World, const FVoxelSim& Sim);

        void DrawScene(RHI::FCmdListH CL, const FUIntVector2& Extent, const FVoxelWorld& World,
                       const FVoxelSim& Sim, const FCamera& Camera, float RealTime);
        void DrawIndirect(RHI::FCmdListH CL, const FVoxelWorld& World, const FVoxelSim& Sim,
                          const FCamera& Camera, float RealTime);
        void Accumulate(RHI::FCmdListH CL, const FCamera& Camera);
        void FilterIndirect(RHI::FCmdListH CL);
        void Compose(RHI::FCmdListH CL, const FUIntVector2& Extent, const FCamera& Camera);
        void Resolve(RHI::FCmdListH CL, const FUIntVector2& Extent, const FCamera& Camera);
        void Adapt(RHI::FCmdListH CL, float DeltaTime);

        void DrawBloom(RHI::FCmdListH CL);
        void DrawComposite(RHI::FCmdListH CL, RHI::FTextureH SwapImage, const FUIntVector2& Extent);

        NODISCARD RHI::FManagedTexture& SceneHistory() { return History[WriteIndex]; }

        EFormat SwapchainFormat = EFormat::UNKNOWN;

        RHI::FPipelineH RaymarchPipeline;
        RHI::FPipelineH GiPipeline;
        RHI::FPipelineH TemporalPipeline;
        RHI::FPipelineH AtrousPipeline;
        RHI::FPipelineH ComposePipeline;
        RHI::FPipelineH TaaPipeline;
        RHI::FPipelineH ExposurePipeline;
        RHI::FPipelineH DownsamplePipeline;
        RHI::FPipelineH UpsamplePipeline;
        RHI::FPipelineH CompositePipeline;
        RHI::FPipelineH SimStepPipeline;
        RHI::FPipelineH SimCoarsePipeline;
        RHI::FPipelineH SimDilatePipeline;
        RHI::FPipelineH PickPipeline;
        RHI::FPipelineH DestroyPipeline;

        void Mark(RHI::FCmdListH CL, uint32 Slot);

        RHI::FQueryPoolH TimerPool;
        bool             bTimers = false;

        RHI::FGPUAllocation PickBuffer;
        RHI::FGPUAllocation ModelDescBuffer;
        RHI::FGPUAllocation ModelCellBuffer;
        RHI::FGPUAllocation EntityBuffer;
        uint32              EntityCount = 0;
        FVector4            EntityBounds;

        RHI::FManagedTexture RawDirect;
        RHI::FManagedTexture RawAlbedo;
        RHI::FManagedTexture GiRaw;
        RHI::FManagedTexture GiGeo;
        RHI::FManagedTexture Accum[2];
        RHI::FManagedTexture Moment[2];
        RHI::FManagedTexture Scratch[2];
        RHI::FManagedTexture SceneTarget;
        RHI::FManagedTexture History[2];
        RHI::FManagedTexture Exposure[2];
        RHI::Utils::FMipChain BloomChain;

        FDestroyRequest PendingDestroy[kMaxDestroyPerFrame];
        int32           PendingDestroyCount = 0;

        FSkyState  Sky;
        FFrameTint Tint;

        FUIntVector2 TargetExtent { 0, 0 };
        FUIntVector2 HalfExtent { 0, 0 };
        uint32       FrameIndex = 0;
        uint32       DebugMode = 0;
        uint32       OverlaySlot = 0;
        bool         bTemporal = true;
        bool         bFilter = true;
        bool         bAntialias = true;
        int32        WriteIndex = 0;
        int32        FilterOutput = 0;
        bool         bHasHistory = false;
        bool         bHasSceneHistory = false;
        bool         bHasExposure = false;

        // A dig only invalidates the pixels it moved, so the history shortens rather than resets.
        uint32       SettleFrames = 0;

        float        SimAccumulator = 0.0f;
        FVector2     Jitter { 0.0f, 0.0f };

        FVector3 PrevPosition { 0.0f, 0.0f, 0.0f };
        FVector3 PrevForward { 0.0f, 0.0f, 1.0f };
        FVector3 PrevRight { 1.0f, 0.0f, 0.0f };
        FVector3 PrevUp { 0.0f, 1.0f, 0.0f };
    };
}
