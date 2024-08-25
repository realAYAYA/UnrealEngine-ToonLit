// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricRenderTarget.cpp
=============================================================================*/

#include "VolumetricRenderTarget.h"
#include "DeferredShadingRenderer.h"
#include "RenderGraphUtils.h"
#include "PixelShaderUtils.h"
#include "ScenePrivate.h"
#include "SceneTextureParameters.h"
#include "SingleLayerWaterRendering.h"
#include "VolumetricCloudRendering.h"
#include "RendererUtils.h"
#include "PostProcess/PostProcessing.h" // IsPostProcessingWithAlphaChannelSupported
#include "EnvironmentComponentsFlags.h"

static TAutoConsoleVariable<int32> CVarVolumetricRenderTarget(
	TEXT("r.VolumetricRenderTarget"), 1,
	TEXT(""),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricRenderTargetUvNoiseSampleAcceptanceWeight(
	TEXT("r.VolumetricRenderTarget.UvNoiseSampleAcceptanceWeight"), 20.0f,
	TEXT("Used when r.VolumetricRenderTarget.UpsamplingMode is in a mode using jitter - this value control the acceptance of noisy cloud samples according to their similarities. A higher value means large differences will be less accepted for blending."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricRenderTargetMode(
	TEXT("r.VolumetricRenderTarget.Mode"), 0,
	TEXT("[0] trace quarter resolution + reconstruct at half resolution + upsample [1] trace half res + reconstruct full res + upsample [2] trace at quarter resolution + reconstruct full resolution (cannot intersect with opaque meshes and forces UpsamplingMode=2 [3] Cinematic mode with tracing done at full reoslution in render target so that clouds can also be applied on translucent.)"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricRenderTargetUpsamplingMode(
	TEXT("r.VolumetricRenderTarget.UpsamplingMode"), 4,
	TEXT("Used in compositing volumetric RT over the scene. [0] bilinear [1] bilinear + jitter [2] nearest + depth test [3] bilinear + jitter + keep closest [4] bilaterial upsampling"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricRenderTargetPreferAsyncCompute(
	TEXT("r.VolumetricRenderTarget.PreferAsyncCompute"), 0,
	TEXT("Whether to prefer using async compute to generate volumetric cloud render targets."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVolumetricRenderTargetReprojectionBoxConstraint(
	TEXT("r.VolumetricRenderTarget.ReprojectionBoxConstraint"), 0,
	TEXT("Whether reprojected data should be constrained to the new incoming cloud data neighborhod value."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarVolumetricRenderTargetMinimumDistanceKmToEnableReprojection(
	TEXT("r.VolumetricRenderTarget.MinimumDistanceKmToEnableReprojection"), 0.0f,
	TEXT("This is the distance in kilometer at which the `cloud surface` must be before we enable reprojection of the previous frame data. One could start with a value of 4km. This helps hide reprojection issues due to imperfect approximation of cloud depth as a single front surface, especially visible when flying through the cloud layer. It is not perfect but will help in lots of cases. The problem when using this method: clouds will look noisier when closer to that distance."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static float GetUvNoiseSampleAcceptanceWeight()
{
	return FMath::Max(0.0f, CVarVolumetricRenderTargetUvNoiseSampleAcceptanceWeight.GetValueOnRenderThread());
}

static bool ShouldPipelineCompileVolumetricRenderTargetShaders(EShaderPlatform ShaderPlatform)
{
	return GetMaxSupportedFeatureLevel(ShaderPlatform) >= ERHIFeatureLevel::SM5;
}

bool ShouldViewRenderVolumetricCloudRenderTarget(const FViewInfo& ViewInfo)
{
	return CVarVolumetricRenderTarget.GetValueOnRenderThread() && ShouldPipelineCompileVolumetricRenderTargetShaders(ViewInfo.GetShaderPlatform())
		&& (ViewInfo.ViewState != nullptr) && !ViewInfo.bIsReflectionCapture;
}

bool IsVolumetricRenderTargetEnabled()
{
	return CVarVolumetricRenderTarget.GetValueOnRenderThread() > 0;
}

bool IsVolumetricRenderTargetAsyncCompute()
{
	// TODO remove that when we remove the pixel shading path in 5.0
	static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricCloud.DisableCompute"));
	const bool bCloudComputePathDisabled = CVar && CVar->GetInt() > 1;

	return GSupportsEfficientAsyncCompute && CVarVolumetricRenderTargetPreferAsyncCompute.GetValueOnRenderThread() > 0 && !bCloudComputePathDisabled;
}

static bool ShouldViewComposeVolumetricRenderTarget(const FViewInfo& ViewInfo)
{
	return ShouldViewRenderVolumetricCloudRenderTarget(ViewInfo);
}

static uint32 GetMainDownsampleFactor(int32 Mode)
{
	switch (Mode)
	{
	case 0:
		return 2; // Reconstruct at half resolution of view
		break;
	case 1:
	case 2:
		return 1; // Reconstruct at full resolution of view
		break;
	case 3:
		return 1; // SKip reconstruct, tracing at full resolution.
		break;
	}
	check(false); // unhandled mode
	return 2;
}

static uint32 GetTraceDownsampleFactor(int32 Mode)
{
	switch (Mode)
	{
	case 0:
		return 2; // Trace at half resolution of the reconstructed buffer (with it being at the half the resolution of the main view)
		break;
	case 1:
		return 2; // Trace at half resolution of the reconstructed buffer (with it being at the same resolution as main view)
		break;
	case 2:
		return 4; // Trace at quarter resolution of the reconstructed buffer (with it being at the same resolution as main view)
		break;
	case 3:
		return 1; // Trace at full resolution
		break;
	}
	check(false); // unhandled mode
	return 2;
}

static void GetTextureSafeUvCoordBound(FRDGTextureRef Texture, FUintVector4& TextureValidCoordRect, FVector4f& TextureValidUvRect)
{
	FIntVector TexSize = Texture->Desc.GetSize();
	TextureValidCoordRect.X = 0;
	TextureValidCoordRect.Y = 0;
	TextureValidCoordRect.Z = TexSize.X - 1;
	TextureValidCoordRect.W = TexSize.Y - 1;
	TextureValidUvRect.X = 0.51f / float(TexSize.X);
	TextureValidUvRect.Y = 0.51f / float(TexSize.Y);
	TextureValidUvRect.Z = (float(TexSize.X) - 0.51f) / float(TexSize.X);
	TextureValidUvRect.W = (float(TexSize.Y) - 0.51f) / float(TexSize.Y);
};

static bool AnyViewRequiresProcessing(TArrayView<FViewInfo> Views)
{
	bool bAnyViewRequiresProcessing = false;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& ViewInfo = Views[ViewIndex];
		bAnyViewRequiresProcessing |= ShouldViewComposeVolumetricRenderTarget(ViewInfo);
	}
	return bAnyViewRequiresProcessing;
}

DECLARE_GPU_STAT(VolCloudReconstruction);
DECLARE_GPU_STAT(VolCloudComposeOverScene);
DECLARE_GPU_STAT(VolCloudComposeUnderSLW);
DECLARE_GPU_STAT(VolCloudComposeForVis);

/*=============================================================================
	UVolumetricCloudComponent implementation.
=============================================================================*/

FVolumetricRenderTargetViewStateData::FVolumetricRenderTargetViewStateData()
	: CurrentRT(1)
	, bFirstTimeUsed(true)
	, bHistoryValid(false)
	, PreViewExposure(1.0f)
	, FullResolution(FIntPoint::ZeroValue)
	, VolumetricReconstructRTResolution(FIntPoint::ZeroValue)
	, VolumetricTracingRTResolution(FIntPoint::ZeroValue)
{
	VolumetricReconstructRTDownsampleFactor = 0;
	VolumetricTracingRTDownsampleFactor = 0;
}

FVolumetricRenderTargetViewStateData::~FVolumetricRenderTargetViewStateData()
{
}

void FVolumetricRenderTargetViewStateData::Initialise(
	FIntPoint& ViewRectResolutionIn,
	int32 InMode,
	int32 InUpsamplingMode,
	bool bCameraCut)
{
	// Update internal settings
	Mode = FMath::Clamp(InMode, 0, 3);
	UpsamplingMode = Mode == 2 || Mode == 3 ? 2 : FMath::Clamp(InUpsamplingMode, 0, 4); // if we are using mode 2 then we cannot intersect with depth and upsampling should be 2 (simple on/off intersection)

	if (bFirstTimeUsed || bCameraCut)
	{
		bFirstTimeUsed = false;
		bHistoryValid = false;
		PreViewExposure = 1.0f;
		FrameId = 0;
		NoiseFrameIndex = 0;
		NoiseFrameIndexModPattern = 0;
		CurrentPixelOffset = FIntPoint::ZeroValue;
	}

	{
		CurrentRT = 1 - CurrentRT;
		const uint32 PreviousRT = 1 - CurrentRT;

		// We always reallocate on a resolution change to adapt to dynamic resolution scaling.
		// TODO allocate once at max resolution and change source and destination coord/uvs/rect.
		if (FullResolution != ViewRectResolutionIn || GetMainDownsampleFactor(Mode) != VolumetricReconstructRTDownsampleFactor || GetTraceDownsampleFactor(Mode) != VolumetricTracingRTDownsampleFactor)
		{
			VolumetricReconstructRTDownsampleFactor = GetMainDownsampleFactor(Mode);
			VolumetricTracingRTDownsampleFactor = GetTraceDownsampleFactor(Mode);

			FullResolution = ViewRectResolutionIn;
			VolumetricReconstructRTResolution = FIntPoint::DivideAndRoundUp(FullResolution, VolumetricReconstructRTDownsampleFactor);							// Half resolution
			VolumetricTracingRTResolution = FIntPoint::DivideAndRoundUp(VolumetricReconstructRTResolution, VolumetricTracingRTDownsampleFactor);	// Half resolution of the volumetric buffer

			// Need a new size so release the low resolution trace buffer
			VolumetricTracingRT.SafeRelease();
			VolumetricSecondaryTracingRT.SafeRelease();
			VolumetricTracingRTDepth.SafeRelease();
		}

		FIntVector CurrentTargetResVec = VolumetricReconstructRT[CurrentRT].IsValid() ? VolumetricReconstructRT[CurrentRT]->GetDesc().GetSize() : FIntVector::ZeroValue;
		FIntPoint CurrentTargetRes = FIntPoint::DivideAndRoundUp(FullResolution, VolumetricReconstructRTDownsampleFactor);
		if (VolumetricReconstructRT[CurrentRT].IsValid() && FIntPoint(CurrentTargetResVec.X, CurrentTargetResVec.Y) != CurrentTargetRes)
		{
			// Resolution does not match so release target we are going to render in
			VolumetricReconstructRT[CurrentRT].SafeRelease();
			VolumetricReconstructRTDepth[CurrentRT].SafeRelease();
		}

		// Regular every frame update
		{
			// Do not mark history as valid if the half resolution buffer is not valid. That means nothing has been rendered last frame.
			// That can happen when cloud is used to render into that buffer
			bHistoryValid = !bCameraCut && VolumetricReconstructRT[PreviousRT].IsValid();

			NoiseFrameIndex += FrameId == 0 ? 1 : 0;
			NoiseFrameIndexModPattern = NoiseFrameIndex % (VolumetricTracingRTDownsampleFactor * VolumetricTracingRTDownsampleFactor);

			FrameId++;
			FrameId = FrameId % (VolumetricTracingRTDownsampleFactor * VolumetricTracingRTDownsampleFactor);

			if (VolumetricTracingRTDownsampleFactor == 2)
			{
				static int32 OrderDithering2x2[4] = { 0, 2, 3, 1 };
				int32 LocalFrameId = OrderDithering2x2[FrameId];
				CurrentPixelOffset = FIntPoint(LocalFrameId % VolumetricTracingRTDownsampleFactor, LocalFrameId / VolumetricTracingRTDownsampleFactor);
			}
			else if (VolumetricTracingRTDownsampleFactor == 4)
			{
				static int32 OrderDithering4x4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
				int32 LocalFrameId = OrderDithering4x4[FrameId];
				CurrentPixelOffset = FIntPoint(LocalFrameId % VolumetricTracingRTDownsampleFactor, LocalFrameId / VolumetricTracingRTDownsampleFactor);
			}
			else
			{
				// Default linear parse
				CurrentPixelOffset = FIntPoint(FrameId % VolumetricTracingRTDownsampleFactor, FrameId / VolumetricTracingRTDownsampleFactor);
			}
		}

		if (Mode == 1 || Mode == 3)
		{
			// No need to jitter in this case. Mode on is tracing half res and then upsample without reconstruction.
			CurrentPixelOffset = FIntPoint::ZeroValue;
		}
	}
}

void FVolumetricRenderTargetViewStateData::Reset()
{
	bFirstTimeUsed = false;
	bHistoryValid = false;
	PreViewExposure = 1.0f;
	FrameId = 0;
	NoiseFrameIndex = 0;
	NoiseFrameIndexModPattern = 0;
	CurrentPixelOffset = FIntPoint::ZeroValue;
	CurrentRT = 0;

	// Release GPU resources
	VolumetricTracingRT.SafeRelease();
	VolumetricSecondaryTracingRT.SafeRelease();
	VolumetricTracingRTDepth.SafeRelease();
	for(uint32 i = 0 ; i < kRenderTargetCount; ++i)
	{
		VolumetricReconstructRT[i].SafeRelease();
		VolumetricReconstructRTDepth[i].SafeRelease();
	}
}

FRDGTextureRef FVolumetricRenderTargetViewStateData::GetOrCreateVolumetricTracingRT(FRDGBuilder& GraphBuilder)
{
	check(FullResolution != FIntPoint::ZeroValue); // check that initialization has been done at least once

	if (!VolumetricTracingRT.IsValid())
	{
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			VolumetricTracingRTResolution, PF_FloatRGBA, FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)),
			TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false);
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, VolumetricTracingRT, TEXT("VolumetricRenderTarget.Tracing"));
	}

	return GraphBuilder.RegisterExternalTexture(VolumetricTracingRT);
}

FRDGTextureRef FVolumetricRenderTargetViewStateData::GetOrCreateVolumetricSecondaryTracingRT(FRDGBuilder& GraphBuilder)
{
	check(FullResolution != FIntPoint::ZeroValue); // check that initialization has been done at least once

	if (!VolumetricSecondaryTracingRT.IsValid())
	{
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			VolumetricTracingRTResolution, PF_FloatRGBA, FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)),
			TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false);
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, VolumetricSecondaryTracingRT, TEXT("VolumetricRenderTarget.SecondaryTracing"));
	}

	return GraphBuilder.RegisterExternalTexture(VolumetricSecondaryTracingRT);
}

FRDGTextureRef FVolumetricRenderTargetViewStateData::GetOrCreateVolumetricTracingRTDepth(FRDGBuilder& GraphBuilder)
{
	check(FullResolution != FIntPoint::ZeroValue); // check that initialization has been done at least once

	if (!VolumetricTracingRTDepth.IsValid())
	{
		EPixelFormat DepthDataFormat = Mode == 0 ? PF_FloatRGBA : PF_G16R16F; // Mode 0 supports MinAndMax depth tracing when the compute path is used so always allocate a 4-components texture in this case.

		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			VolumetricTracingRTResolution, DepthDataFormat, FClearValueBinding(FLinearColor(63000.0f, 63000.0f, 63000.0f, 63000.0f)),
			TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false);
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, VolumetricTracingRTDepth, TEXT("VolumetricRenderTarget.TracingDepth"));
	}

	return GraphBuilder.RegisterExternalTexture(VolumetricTracingRTDepth);
}

FRDGTextureRef FVolumetricRenderTargetViewStateData::GetDstVolumetricReconstructRT(FRDGBuilder& GraphBuilder)
{
	return VolumetricReconstructRT[CurrentRT].IsValid() ? GraphBuilder.RegisterExternalTexture(VolumetricReconstructRT[CurrentRT]) : nullptr;
}

FRDGTextureRef FVolumetricRenderTargetViewStateData::GetOrCreateDstVolumetricReconstructRT(FRDGBuilder& GraphBuilder)
{
	check(VolumetricReconstructRTResolution != FIntPoint::ZeroValue); // check that initialization has been done at least once

	if (!VolumetricReconstructRT[CurrentRT].IsValid())
	{
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			VolumetricReconstructRTResolution, PF_FloatRGBA, FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)),
			TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable, false);
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, VolumetricReconstructRT[CurrentRT], TEXT("VolumetricRenderTarget.Reconstruct"));
	}

	return GraphBuilder.RegisterExternalTexture(VolumetricReconstructRT[CurrentRT]);
}


FRDGTextureRef FVolumetricRenderTargetViewStateData::GetOrCreateDstVolumetricReconstructRTDepth(FRDGBuilder& GraphBuilder)
{
	check(VolumetricReconstructRTResolution != FIntPoint::ZeroValue); // check that initialization has been done at least once

	if (!VolumetricReconstructRTDepth[CurrentRT].IsValid())
	{
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			VolumetricReconstructRTResolution, PF_G16R16F, FClearValueBinding(FLinearColor(63000.0f, 63000.0f, 63000.0f, 63000.0f)),
			TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable, false);
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, VolumetricReconstructRTDepth[CurrentRT], TEXT("VolumetricRenderTarget.ReconstructDepth"));
	}

	return GraphBuilder.RegisterExternalTexture(VolumetricReconstructRTDepth[CurrentRT]);
}

TRefCountPtr<IPooledRenderTarget> FVolumetricRenderTargetViewStateData::GetDstVolumetricReconstructRT()
{
	return VolumetricReconstructRT[CurrentRT];
}
TRefCountPtr<IPooledRenderTarget> FVolumetricRenderTargetViewStateData::GetDstVolumetricReconstructRTDepth()
{
	return VolumetricReconstructRTDepth[CurrentRT];
}

FRDGTextureRef FVolumetricRenderTargetViewStateData::GetOrCreateSrcVolumetricReconstructRT(FRDGBuilder& GraphBuilder)
{
	check(VolumetricReconstructRTResolution != FIntPoint::ZeroValue); // check that initialization has been done at least once

	check(VolumetricReconstructRT[1u - CurrentRT].IsValid());
	return GraphBuilder.RegisterExternalTexture(VolumetricReconstructRT[1u - CurrentRT]);
}

FRDGTextureRef FVolumetricRenderTargetViewStateData::GetOrCreateSrcVolumetricReconstructRTDepth(FRDGBuilder& GraphBuilder)
{
	check(VolumetricReconstructRTResolution != FIntPoint::ZeroValue); // check that initialization has been done at least once

	check(VolumetricReconstructRT[1u - CurrentRT].IsValid());
	return GraphBuilder.RegisterExternalTexture(VolumetricReconstructRTDepth[1u - CurrentRT]);
}

FUintVector4 FVolumetricRenderTargetViewStateData::GetTracingCoordToZbufferCoordScaleBias() const
{
	if (Mode == 2 || Mode == 3)
	{
		// In this case, the source depth buffer full resolution depth buffer is the full resolution scene one
		const uint32 CombinedDownsampleFactor = VolumetricReconstructRTDownsampleFactor * VolumetricTracingRTDownsampleFactor;
		return FUintVector4(CombinedDownsampleFactor, CombinedDownsampleFactor,																// Scale is the combined downsample factor
			CurrentPixelOffset.X * VolumetricReconstructRTDownsampleFactor, CurrentPixelOffset.Y * VolumetricReconstructRTDownsampleFactor);// Each sample will then sample from full res according to reconstructed RT offset times its downsample factor
	}

	// Otherwise, a half resolution depth buffer is used
	const uint32 SourceDepthBufferRTDownsampleFactor = 2;
	const uint32 CombinedDownsampleFactor = VolumetricReconstructRTDownsampleFactor * VolumetricTracingRTDownsampleFactor / SourceDepthBufferRTDownsampleFactor;
	return FUintVector4( CombinedDownsampleFactor, CombinedDownsampleFactor,									// Scale is the combined downsample factor
		CurrentPixelOffset.X * VolumetricReconstructRTDownsampleFactor / VolumetricReconstructRTDownsampleFactor,	// Each sample will then sample from full res according to reconstructed RT offset times its downsample factor
		CurrentPixelOffset.Y * VolumetricReconstructRTDownsampleFactor / VolumetricReconstructRTDownsampleFactor);
}

FUintVector4 FVolumetricRenderTargetViewStateData::GetTracingCoordToFullResPixelCoordScaleBias() const
{
	// In this case, the source depth buffer full resolution depth buffer is the full resolution scene one
	const uint32 CombinedDownsampleFactor = VolumetricReconstructRTDownsampleFactor * VolumetricTracingRTDownsampleFactor;
	return FUintVector4(CombinedDownsampleFactor, CombinedDownsampleFactor,																// Scale is the combined downsample factor
		CurrentPixelOffset.X * VolumetricReconstructRTDownsampleFactor, CurrentPixelOffset.Y * VolumetricReconstructRTDownsampleFactor);// Each sample will then sample from full res according to reconstructed RT offset times its downsample factor
}


/*=============================================================================
	FSceneRenderer implementation.
=============================================================================*/

void InitVolumetricRenderTargetForViews(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& ViewInfo = Views[ViewIndex];
		if (!ShouldViewRenderVolumetricCloudRenderTarget(ViewInfo))
		{
			continue;
		}
		FVolumetricRenderTargetViewStateData& VolumetricCloudRT = ViewInfo.ViewState->VolumetricCloudRenderTarget;

		// Determine if we are initializing or we should reset the persistent state
		const bool bCameraCut = ViewInfo.bCameraCut || ViewInfo.bForceCameraVisibilityReset;

		FIntPoint ViewRect = ViewInfo.ViewRect.Size();
		VolumetricCloudRT.Initialise(	// TODO this is going to reallocate a buffer each time dynamic resolution scaling is applied 
			ViewRect,
			CVarVolumetricRenderTargetMode.GetValueOnRenderThread(),
			CVarVolumetricRenderTargetUpsamplingMode.GetValueOnAnyThread(),
			bCameraCut);

		FViewUniformShaderParameters ViewVolumetricCloudRTParameters = *ViewInfo.CachedViewUniformShaderParameters;
		{
			const FIntPoint& VolumetricReconstructResolution = VolumetricCloudRT.GetCurrentVolumetricReconstructRTResolution();
			const FIntPoint& VolumetricTracingResolution = VolumetricCloudRT.GetCurrentVolumetricTracingRTResolution();
			const FIntPoint& CurrentPixelOffset = VolumetricCloudRT.GetCurrentTracingPixelOffset();
			const uint32 VolumetricReconstructRTDownSample = VolumetricCloudRT.GetVolumetricReconstructRTDownsampleFactor();
			const uint32 VolumetricTracingRTDownSample = VolumetricCloudRT.GetVolumetricTracingRTDownsampleFactor();

			// We jitter and reconstruct the volumetric view before TAA so we do not want any of its jitter.
			// We do use TAA remove bilinear artifact at up sampling time.
			FViewMatrices ViewMatrices = ViewInfo.ViewMatrices;
			ViewMatrices.HackRemoveTemporalAAProjectionJitter();

			float DownSampleFactor = float(VolumetricReconstructRTDownSample * VolumetricTracingRTDownSample);

			// Offset to the correct half resolution pixel
			FVector2D CenterCoord = FVector2D(VolumetricReconstructRTDownSample / 2.0f);
			FVector2D TargetCoord = FVector2D(CurrentPixelOffset) + FVector2D(0.5f, 0.5f);
			FVector2D OffsetCoord = (TargetCoord - CenterCoord) * (FVector2D(-2.0f, 2.0f) / FVector2D(VolumetricReconstructResolution));
			ViewMatrices.HackAddTemporalAAProjectionJitter(OffsetCoord);

			ViewInfo.SetupViewRectUniformBufferParameters(
				ViewVolumetricCloudRTParameters,
				VolumetricTracingResolution,
				FIntRect(0, 0, VolumetricTracingResolution.X, VolumetricTracingResolution.Y),
				ViewMatrices,
				ViewInfo.PrevViewInfo.ViewMatrices // This could also be changed if needed
			);
		}
		ViewInfo.VolumetricRenderTargetViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(ViewVolumetricCloudRTParameters, UniformBuffer_SingleFrame);
	}
}

void ResetVolumetricRenderTargetForViews(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& ViewInfo = Views[ViewIndex];
		if (ViewInfo.ViewState)
		{
			FVolumetricRenderTargetViewStateData& VolumetricCloudRT = ViewInfo.ViewState->VolumetricCloudRenderTarget;
			VolumetricCloudRT.Reset();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

class FReconstructVolumetricRenderTargetPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReconstructVolumetricRenderTargetPS);
	SHADER_USE_PARAMETER_STRUCT(FReconstructVolumetricRenderTargetPS, FGlobalShader);

	class FHistoryAvailable : SHADER_PERMUTATION_BOOL("PERMUTATION_HISTORY_AVAILABLE");
	class FReprojectionBoxConstraint : SHADER_PERMUTATION_BOOL("PERMUTATION_REPROJECTION_BOX_CONSTRAINT");
	class FCloudMinAndMaxDepth : SHADER_PERMUTATION_BOOL("CLOUD_MIN_AND_MAX_DEPTH");
	using FPermutationDomain = TShaderPermutationDomain<FHistoryAvailable, FReprojectionBoxConstraint, FCloudMinAndMaxDepth>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TracingVolumetricTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SecondaryTracingVolumetricTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TracingVolumetricDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PreviousFrameVolumetricTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PreviousFrameVolumetricDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HalfResDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
		SHADER_PARAMETER(FVector4f, DstVolumetricTextureSizeAndInvSize)
		SHADER_PARAMETER(FVector4f, PreviousVolumetricTextureSizeAndInvSize)
		SHADER_PARAMETER(FIntPoint, CurrentTracingPixelOffset)
		SHADER_PARAMETER(FIntPoint, ViewViewRectMin)
		SHADER_PARAMETER(int32, DownSampleFactor)
		SHADER_PARAMETER(int32, VolumetricRenderTargetMode)
		SHADER_PARAMETER(FUintVector4, TracingVolumetricTextureValidCoordRect)
		SHADER_PARAMETER(FVector4f, TracingVolumetricTextureValidUvRect)
		SHADER_PARAMETER(FUintVector4, PreviousFrameVolumetricTextureValidCoordRect)
		SHADER_PARAMETER(FVector4f, PreviousFrameVolumetricTextureValidUvRect)
		SHADER_PARAMETER(float, MinimumDistanceKmToEnableReprojection)
		SHADER_PARAMETER(float, HistoryPreExposureCorrection)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldPipelineCompileVolumetricRenderTargetShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_RECONSTRUCT_VOLUMETRICRT"), TEXT("1"));
	}
};

IMPLEMENT_GLOBAL_SHADER(FReconstructVolumetricRenderTargetPS, "/Engine/Private/VolumetricRenderTarget.usf", "ReconstructVolumetricRenderTargetPS", SF_Pixel);

//////////////////////////////////////////////////////////////////////////

void ReconstructVolumetricRenderTarget(
	FRDGBuilder& GraphBuilder,
	TArrayView<FViewInfo> Views,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef HalfResolutionDepthCheckerboardMinMaxTexture,
	bool bWaitFinishFence)
{
	if (!AnyViewRequiresProcessing(Views))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "VolCloudReconstruction");
	RDG_GPU_STAT_SCOPE(GraphBuilder, VolCloudReconstruction);
	SCOPED_NAMED_EVENT(VolCloudReconstruction, FColor::Emerald);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& ViewInfo = Views[ViewIndex];
		if (!ShouldViewComposeVolumetricRenderTarget(ViewInfo))
		{
			continue;
		}

		FVolumetricRenderTargetViewStateData& VolumetricCloudRT = ViewInfo.ViewState->VolumetricCloudRenderTarget;

		if (VolumetricCloudRT.GetMode() == 1 || VolumetricCloudRT.GetMode() == 3)
		{
			// In this case, we trace at half resolution using checker boarded min max depth.
			// We will then directly up sample on screen from half resolution to full resolution. 
			// No reconstruction needed.
			continue;
		}

		FRDGTextureRef DstVolumetric = VolumetricCloudRT.GetOrCreateDstVolumetricReconstructRT(GraphBuilder);
		FRDGTextureRef DstVolumetricDepth = VolumetricCloudRT.GetOrCreateDstVolumetricReconstructRTDepth(GraphBuilder);
		FRDGTextureRef SrcTracingVolumetric = VolumetricCloudRT.GetOrCreateVolumetricTracingRT(GraphBuilder);
		FRDGTextureRef SrcSecondaryTracingVolumetric = VolumetricCloudRT.GetOrCreateVolumetricSecondaryTracingRT(GraphBuilder);
		FRDGTextureRef SrcTracingVolumetricDepth = VolumetricCloudRT.GetOrCreateVolumetricTracingRTDepth(GraphBuilder);
		FRDGTextureRef PreviousFrameVolumetricTexture = VolumetricCloudRT.GetHistoryValid() ? VolumetricCloudRT.GetOrCreateSrcVolumetricReconstructRT(GraphBuilder) : SystemTextures.Black;
		FRDGTextureRef PreviousFrameVolumetricDepthTexture = VolumetricCloudRT.GetHistoryValid() ? VolumetricCloudRT.GetOrCreateSrcVolumetricReconstructRTDepth(GraphBuilder) : SystemTextures.Black;

		const uint32 TracingVolumetricCloudRTDownSample = VolumetricCloudRT.GetVolumetricTracingRTDownsampleFactor();

		FReconstructVolumetricRenderTargetPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FReconstructVolumetricRenderTargetPS::FHistoryAvailable>(VolumetricCloudRT.GetHistoryValid());
		PermutationVector.Set<FReconstructVolumetricRenderTargetPS::FReprojectionBoxConstraint>(CVarVolumetricRenderTargetReprojectionBoxConstraint.GetValueOnAnyThread() > 0);
		PermutationVector.Set<FReconstructVolumetricRenderTargetPS::FCloudMinAndMaxDepth>(ShouldVolumetricCloudTraceWithMinMaxDepth(ViewInfo));
		TShaderMapRef<FReconstructVolumetricRenderTargetPS> PixelShader(ViewInfo.ShaderMap, PermutationVector);

		FReconstructVolumetricRenderTargetPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReconstructVolumetricRenderTargetPS::FParameters>();
		PassParameters->ViewUniformBuffer = ViewInfo.VolumetricRenderTargetViewUniformBuffer; // Using a special uniform buffer because the view has some special resolution and no split screen offset.
		PassParameters->RenderTargets[0] = FRenderTargetBinding(DstVolumetric, ERenderTargetLoadAction::ENoAction);
		PassParameters->RenderTargets[1] = FRenderTargetBinding(DstVolumetricDepth, ERenderTargetLoadAction::ENoAction);
		PassParameters->TracingVolumetricTexture = SrcTracingVolumetric;
		PassParameters->SecondaryTracingVolumetricTexture = SrcSecondaryTracingVolumetric;
		PassParameters->TracingVolumetricDepthTexture = SrcTracingVolumetricDepth;
		PassParameters->PreviousFrameVolumetricTexture = PreviousFrameVolumetricTexture;
		PassParameters->PreviousFrameVolumetricDepthTexture = PreviousFrameVolumetricDepthTexture;
		PassParameters->LinearTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		PassParameters->CurrentTracingPixelOffset = VolumetricCloudRT.GetCurrentTracingPixelOffset();
		PassParameters->ViewViewRectMin = ViewInfo.ViewRect.Min / GetMainDownsampleFactor(VolumetricCloudRT.GetMode());// because we use the special VolumetricRenderTargetViewUniformBuffer, we have to specify View.RectMin separately.
		PassParameters->DownSampleFactor = TracingVolumetricCloudRTDownSample;
		PassParameters->VolumetricRenderTargetMode = VolumetricCloudRT.GetMode();
		PassParameters->HalfResDepthTexture = (VolumetricCloudRT.GetMode() == 0 || VolumetricCloudRT.GetMode() == 3) ? HalfResolutionDepthCheckerboardMinMaxTexture : SceneDepthTexture;
		PassParameters->MinimumDistanceKmToEnableReprojection = FMath::Max(0.0f, CVarVolumetricRenderTargetMinimumDistanceKmToEnableReprojection.GetValueOnRenderThread());
		PassParameters->HistoryPreExposureCorrection = ViewInfo.PreExposure / VolumetricCloudRT.GetPrevViewExposure();

		const bool bVisualizeConservativeDensity = ShouldViewVisualizeVolumetricCloudConservativeDensity(ViewInfo, ViewInfo.Family->EngineShowFlags);
		PassParameters->HalfResDepthTexture = bVisualizeConservativeDensity ?
			((bool)ERHIZBuffer::IsInverted ? SystemTextures.Black : SystemTextures.White) :
			((VolumetricCloudRT.GetMode() == 0 || VolumetricCloudRT.GetMode() == 3) ?
				HalfResolutionDepthCheckerboardMinMaxTexture :
				SceneDepthTexture);

		GetTextureSafeUvCoordBound(SrcTracingVolumetric, PassParameters->TracingVolumetricTextureValidCoordRect, PassParameters->TracingVolumetricTextureValidUvRect);
		GetTextureSafeUvCoordBound(PreviousFrameVolumetricTexture, PassParameters->PreviousFrameVolumetricTextureValidCoordRect, PassParameters->PreviousFrameVolumetricTextureValidUvRect);

		FIntVector DstVolumetricSize = DstVolumetric->Desc.GetSize();
		FVector2D DstVolumetricTextureSize = FVector2D(float(DstVolumetricSize.X), float(DstVolumetricSize.Y));
		FVector2D PreviousVolumetricTextureSize = FVector2D(float(PreviousFrameVolumetricTexture->Desc.GetSize().X), float(PreviousFrameVolumetricTexture->Desc.GetSize().Y));
		PassParameters->DstVolumetricTextureSizeAndInvSize = FVector4f(DstVolumetricTextureSize.X, DstVolumetricTextureSize.Y, 1.0f / DstVolumetricTextureSize.X, 1.0f / DstVolumetricTextureSize.Y);
		PassParameters->PreviousVolumetricTextureSizeAndInvSize = FVector4f(PreviousVolumetricTextureSize.X, PreviousVolumetricTextureSize.Y, 1.0f / PreviousVolumetricTextureSize.X, 1.0f / PreviousVolumetricTextureSize.Y);

		FPixelShaderUtils::AddFullscreenPass<FReconstructVolumetricRenderTargetPS>(
			GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("VolumetricReconstruct"), PixelShader, PassParameters,
			FIntRect(0, 0, DstVolumetricSize.X, DstVolumetricSize.Y));

		VolumetricCloudRT.PostRenderUpdate(ViewInfo.PreExposure);
	}

}

//////////////////////////////////////////////////////////////////////////

class FComposeVolumetricRTOverScenePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FComposeVolumetricRTOverScenePS);
	SHADER_USE_PARAMETER_STRUCT(FComposeVolumetricRTOverScenePS, FGlobalShader);

	class FUpsamplingMode : SHADER_PERMUTATION_RANGE_INT("PERMUTATION_UPSAMPLINGMODE", 0, 5);
	class FRenderUnderWaterBuffer : SHADER_PERMUTATION_BOOL("PERMUTATION_RENDER_UNDERWATER_BUFFER");	// Render into the water scene color buffer (used when rendering from water system)
	class FRenderCameraComposeWithWater : SHADER_PERMUTATION_BOOL("PERMUTATION_COMPOSE_WITH_WATER");	// When water us used and the camera is under water, use that permutation (to handle camera intersection with water and double cloud composition)
	class FMSAASampleCount : SHADER_PERMUTATION_SPARSE_INT("MSAA_SAMPLE_COUNT", 1, 2, 4, 8);
	using FPermutationDomain = TShaderPermutationDomain<FUpsamplingMode, FRenderUnderWaterBuffer, FRenderCameraComposeWithWater, FMSAASampleCount>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VolumetricTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VolumetricDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, WaterLinearDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DMS<float>, MSAADepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearTextureSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, WaterLinearDepthSampler)
		RENDER_TARGET_BINDING_SLOTS()
		SHADER_PARAMETER(uint32, ForwardShadingEnable)
		SHADER_PARAMETER(float, UvOffsetSampleAcceptanceWeight)
		SHADER_PARAMETER(FVector4f, VolumetricTextureSizeAndInvSize)
		SHADER_PARAMETER(FVector2f, FullResolutionToVolumetricBufferResolutionScale)
		SHADER_PARAMETER(FVector2f, FullResolutionToWaterBufferScale)
		SHADER_PARAMETER(FVector4f, SceneWithoutSingleLayerWaterViewRect)
		SHADER_PARAMETER(FUintVector4, VolumetricTextureValidCoordRect)
		SHADER_PARAMETER(FVector4f, VolumetricTextureValidUvRect)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if ((!IsForwardShadingEnabled(Parameters.Platform) || !RHISupportsMSAA(Parameters.Platform)) && PermutationVector.Get<FMSAASampleCount>() > 1)
		{
			// We only compile the MSAA support when Forward shading is enabled because MSAA can only be used in this case.
			return false;
		}

		return ShouldPipelineCompileVolumetricRenderTargetShaders(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_COMPOSE_VOLUMETRICRT"), TEXT("1"));
	}
};

IMPLEMENT_GLOBAL_SHADER(FComposeVolumetricRTOverScenePS, "/Engine/Private/VolumetricRenderTarget.usf", "ComposeVolumetricRTOverScenePS", SF_Pixel);

//////////////////////////////////////////////////////////////////////////

static FVector2f GetCompositionFullResolutionToVolumetricBufferResolutionScale(uint32 VRTMode)
{
	if (VRTMode == 1)
	{
		return FVector2f(0.5f, 2.0f);
	}
	return FVector2f(1.0f / float(GetMainDownsampleFactor(VRTMode)), float(GetMainDownsampleFactor(VRTMode)));
}

static void GetCompositionCloudTextures(uint32 VRTMode, FVolumetricRenderTargetViewStateData& VolumetricCloudRT, FRDGBuilder& GraphBuilder, FRDGTextureRef& VolumetricTexture, FRDGTextureRef& VolumetricDepthTexture)
{
	if (VRTMode == 1 || VRTMode == 3)
	{
		// In this case, we trace at half resolution using checker boarded min max depth.
		// We will then directly up sample on screen from half resolution to full resolution. 
		// No reconstruction needed.
		VolumetricTexture = VolumetricCloudRT.GetOrCreateVolumetricTracingRT(GraphBuilder);
		VolumetricDepthTexture = VolumetricCloudRT.GetOrCreateVolumetricTracingRTDepth(GraphBuilder);
	}
	else
	{
		VolumetricTexture = VolumetricCloudRT.GetOrCreateDstVolumetricReconstructRT(GraphBuilder);
		VolumetricDepthTexture = VolumetricCloudRT.GetOrCreateDstVolumetricReconstructRTDepth(GraphBuilder);
	}
}

static int32 GetCompositionUpsamplingMode(uint32 VRTMode, int32 UpsamplingMode)
{
	return UpsamplingMode == 3 && (VRTMode == 1 || VRTMode == 2 || VRTMode == 3) ? 2 : UpsamplingMode;
}

void ComposeVolumetricRenderTargetOverScene(
	FRDGBuilder& GraphBuilder,
	TArrayView<FViewInfo> Views,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef SceneDepthTexture,
	bool bComposeWithWater,
	const FSceneWithoutWaterTextures& WaterPassData,
	const FMinimalSceneTextures& SceneTextures)
{
	if (!AnyViewRequiresProcessing(Views))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "VolCloudComposeOverScene");
	RDG_GPU_STAT_SCOPE(GraphBuilder, VolCloudComposeOverScene);
	SCOPED_NAMED_EVENT(VolCloudComposeOverScene, FColor::Emerald);

	FRHIBlendState* PreMultipliedColorTransmittanceBlend;
	const bool bSupportsAlpha = IsPostProcessingWithAlphaChannelSupported();
	if (bSupportsAlpha)
	{
		// When alpha channel is enabled, we always write transmittance to impact other alpha holdout values from sky or fog for instance.
		// We will run a second pass later accumulating the cloud contribution to hold out
		PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
	}
	else
	{
		PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& ViewInfo = Views[ViewIndex];
		if (!ShouldViewRenderVolumetricCloudRenderTarget(ViewInfo))
		{
			continue;
		}
		if (ViewInfo.CachedViewUniformShaderParameters->RenderingReflectionCaptureMask == 0 && !IsVolumetricCloudRenderedInMain(ViewInfo.CachedViewUniformShaderParameters->EnvironmentComponentsFlags))
		{
			continue;
		}

		FVolumetricRenderTargetViewStateData& VolumetricCloudRT = ViewInfo.ViewState->VolumetricCloudRenderTarget;

		// When reconstructed and back buffer resolution matches, force using a pixel perfect upsampling.
		const uint32 VRTMode = VolumetricCloudRT.GetMode();
		int32 UpsamplingMode = GetCompositionUpsamplingMode(VRTMode, VolumetricCloudRT.GetUpsamplingMode());

		FRDGTextureRef VolumetricTexture = nullptr;
		FRDGTextureRef VolumetricDepthTexture = nullptr;
		GetCompositionCloudTextures(VRTMode, VolumetricCloudRT, GraphBuilder, VolumetricTexture, VolumetricDepthTexture);

		// We only support MSAA up to 8 sample and in forward
		check(SceneDepthTexture->Desc.NumSamples <= 8);
		// We only support MSAA in forward, not in deferred.
		const bool bForwardShading = IsForwardShadingEnabled(ViewInfo.GetShaderPlatform());
		check(bForwardShading || (!bForwardShading && SceneDepthTexture->Desc.NumSamples==1));

		FComposeVolumetricRTOverScenePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FUpsamplingMode>(UpsamplingMode);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FRenderUnderWaterBuffer>(0);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FRenderCameraComposeWithWater>((bComposeWithWater && ViewInfo.IsUnderwater()) ? 1 : 0);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FMSAASampleCount>(SceneDepthTexture->Desc.NumSamples);
		TShaderMapRef<FComposeVolumetricRTOverScenePS> PixelShader(ViewInfo.ShaderMap, PermutationVector);

		FComposeVolumetricRTOverScenePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComposeVolumetricRTOverScenePS::FParameters>();
		PassParameters->ViewUniformBuffer = ViewInfo.ViewUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->MSAADepthTexture = SceneDepthTexture;
		PassParameters->VolumetricTexture = VolumetricTexture;
		PassParameters->VolumetricDepthTexture = VolumetricDepthTexture;
		PassParameters->LinearTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		PassParameters->UvOffsetSampleAcceptanceWeight = GetUvNoiseSampleAcceptanceWeight();
		PassParameters->FullResolutionToVolumetricBufferResolutionScale = GetCompositionFullResolutionToVolumetricBufferResolutionScale(VRTMode);
		PassParameters->SceneTextures = SceneTextures.UniformBuffer;
		PassParameters->ForwardShadingEnable = bForwardShading ? 1 : 0;
		GetTextureSafeUvCoordBound(PassParameters->VolumetricTexture, PassParameters->VolumetricTextureValidCoordRect, PassParameters->VolumetricTextureValidUvRect);

		PassParameters->WaterLinearDepthTexture = WaterPassData.DepthTexture;
		PassParameters->WaterLinearDepthSampler = TStaticSamplerState<SF_Point>::GetRHI();
		if (bComposeWithWater)
		{
			const FSceneWithoutWaterTextures::FView& WaterPassViewData = WaterPassData.Views[ViewIndex];
			PassParameters->FullResolutionToWaterBufferScale = FVector2f(1.0f / WaterPassData.RefractionDownsampleFactor, WaterPassData.RefractionDownsampleFactor);
			PassParameters->SceneWithoutSingleLayerWaterViewRect = FVector4f(WaterPassViewData.ViewRect.Min.X, WaterPassViewData.ViewRect.Min.Y,
				WaterPassViewData.ViewRect.Max.X, WaterPassViewData.ViewRect.Max.Y);
		}

		FVector2D VolumetricTextureSize = FVector2D(float(VolumetricTexture->Desc.GetSize().X), float(VolumetricTexture->Desc.GetSize().Y));
		PassParameters->VolumetricTextureSizeAndInvSize = FVector4f(VolumetricTextureSize.X, VolumetricTextureSize.Y, 1.0f / VolumetricTextureSize.X, 1.0f / VolumetricTextureSize.Y);

		FPixelShaderUtils::AddFullscreenPass<FComposeVolumetricRTOverScenePS>(
			GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("VolumetricComposeOverScene"), PixelShader, PassParameters, ViewInfo.ViewRect,
			PreMultipliedColorTransmittanceBlend);
	}
}

//////////////////////////////////////////////////////////////////////////

void ComposeVolumetricRenderTargetOverSceneUnderWater(
	FRDGBuilder& GraphBuilder,
	TArrayView<FViewInfo> Views,
	const FSceneWithoutWaterTextures& WaterPassData,
	const FMinimalSceneTextures& SceneTextures)
{
	if (!AnyViewRequiresProcessing(Views))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "VolCloudComposeUnderSLW");
	RDG_GPU_STAT_SCOPE(GraphBuilder, VolCloudComposeUnderSLW);
	SCOPED_NAMED_EVENT(VolCloudComposeUnderSLW, FColor::Emerald);

	FRHIBlendState* PreMultipliedColorTransmittanceBlend = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& ViewInfo = Views[ViewIndex];
		if (!ShouldViewRenderVolumetricCloudRenderTarget(ViewInfo) || !ViewInfo.ShouldRenderView())
		{
			continue;
		}

		const FSceneWithoutWaterTextures::FView& WaterPassViewData = WaterPassData.Views[ViewIndex];
		FVolumetricRenderTargetViewStateData& VolumetricCloudRT = ViewInfo.ViewState->VolumetricCloudRenderTarget;

		// When reconstructed and back buffer resolution matches, force using a pixel perfect upsampling.
		const uint32 VRTMode = VolumetricCloudRT.GetMode();
		int32 UpsamplingMode = GetCompositionUpsamplingMode(VRTMode, VolumetricCloudRT.GetUpsamplingMode());

		FRDGTextureRef VolumetricTexture = nullptr;
		FRDGTextureRef VolumetricDepthTexture = nullptr;
		GetCompositionCloudTextures(VRTMode, VolumetricCloudRT, GraphBuilder, VolumetricTexture, VolumetricDepthTexture);

		FComposeVolumetricRTOverScenePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FUpsamplingMode>(UpsamplingMode);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FRenderUnderWaterBuffer>(1);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FRenderCameraComposeWithWater>(0);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FMSAASampleCount>(1);
		TShaderMapRef<FComposeVolumetricRTOverScenePS> PixelShader(ViewInfo.ShaderMap, PermutationVector);

		FComposeVolumetricRTOverScenePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComposeVolumetricRTOverScenePS::FParameters>();
		PassParameters->ViewUniformBuffer = ViewInfo.ViewUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(WaterPassData.ColorTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->VolumetricTexture = VolumetricTexture;
		PassParameters->VolumetricDepthTexture = VolumetricDepthTexture;
		PassParameters->WaterLinearDepthTexture = WaterPassData.DepthTexture;
		PassParameters->LinearTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		PassParameters->WaterLinearDepthSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->UvOffsetSampleAcceptanceWeight = GetUvNoiseSampleAcceptanceWeight();
		PassParameters->FullResolutionToVolumetricBufferResolutionScale = GetCompositionFullResolutionToVolumetricBufferResolutionScale(VRTMode);
		PassParameters->FullResolutionToWaterBufferScale = FVector2f(1.0f / WaterPassData.RefractionDownsampleFactor, WaterPassData.RefractionDownsampleFactor);
		PassParameters->SceneWithoutSingleLayerWaterViewRect = FVector4f(WaterPassViewData.ViewRect.Min.X, WaterPassViewData.ViewRect.Min.Y,
																		WaterPassViewData.ViewRect.Max.X, WaterPassViewData.ViewRect.Max.Y);
		PassParameters->SceneTextures = SceneTextures.UniformBuffer;
		GetTextureSafeUvCoordBound(PassParameters->VolumetricTexture, PassParameters->VolumetricTextureValidCoordRect, PassParameters->VolumetricTextureValidUvRect);

		FVector2D VolumetricTextureSize = FVector2D(float(VolumetricTexture->Desc.GetSize().X), float(VolumetricTexture->Desc.GetSize().Y));
		PassParameters->VolumetricTextureSizeAndInvSize = FVector4f(VolumetricTextureSize.X, VolumetricTextureSize.Y, 1.0f / VolumetricTextureSize.X, 1.0f / VolumetricTextureSize.Y);

		FPixelShaderUtils::AddFullscreenPass<FComposeVolumetricRTOverScenePS>(
			GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("SLW::VolumetricComposeOverScene"), PixelShader, PassParameters, WaterPassViewData.ViewRect,
			PreMultipliedColorTransmittanceBlend);
	}
}

//////////////////////////////////////////////////////////////////////////

void ComposeVolumetricRenderTargetOverSceneForVisualization(
	FRDGBuilder& GraphBuilder,
	TArrayView<FViewInfo> Views,
	FRDGTextureRef SceneColorTexture,
	const FMinimalSceneTextures& SceneTextures)
{
	if (!AnyViewRequiresProcessing(Views))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "VolCloudComposeForVis");
	RDG_GPU_STAT_SCOPE(GraphBuilder, VolCloudComposeForVis);
	SCOPED_NAMED_EVENT(VolCloudComposeForVis, FColor::Emerald);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& ViewInfo = Views[ViewIndex];
		if (!ShouldViewRenderVolumetricCloudRenderTarget(ViewInfo))
		{
			continue;
		}
		FVolumetricRenderTargetViewStateData& VolumetricCloudRT = ViewInfo.ViewState->VolumetricCloudRenderTarget;

		// When reconstructed and back buffer resolution matches, force using a pixel perfect upsampling.
		const uint32 VRTMode = VolumetricCloudRT.GetMode();
		int32 UpsamplingMode = GetCompositionUpsamplingMode(VRTMode, VolumetricCloudRT.GetUpsamplingMode());

		FRDGTextureRef VolumetricTexture = nullptr;
		FRDGTextureRef VolumetricDepthTexture = nullptr;
		GetCompositionCloudTextures(VRTMode, VolumetricCloudRT, GraphBuilder, VolumetricTexture, VolumetricDepthTexture);

		FComposeVolumetricRTOverScenePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FUpsamplingMode>(0);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FRenderUnderWaterBuffer>(0);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FRenderCameraComposeWithWater>(0);
		PermutationVector.Set<FComposeVolumetricRTOverScenePS::FMSAASampleCount>(1);
		TShaderMapRef<FComposeVolumetricRTOverScenePS> PixelShader(ViewInfo.ShaderMap, PermutationVector);

		FComposeVolumetricRTOverScenePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComposeVolumetricRTOverScenePS::FParameters>();
		PassParameters->ViewUniformBuffer = ViewInfo.ViewUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->VolumetricTexture = VolumetricTexture;
		PassParameters->VolumetricDepthTexture = VolumetricDepthTexture;
		PassParameters->LinearTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		PassParameters->UvOffsetSampleAcceptanceWeight = GetUvNoiseSampleAcceptanceWeight();
		PassParameters->FullResolutionToVolumetricBufferResolutionScale = GetCompositionFullResolutionToVolumetricBufferResolutionScale(VRTMode);
		PassParameters->SceneTextures = SceneTextures.UniformBuffer;
		GetTextureSafeUvCoordBound(PassParameters->VolumetricTexture, PassParameters->VolumetricTextureValidCoordRect, PassParameters->VolumetricTextureValidUvRect);

		PassParameters->WaterLinearDepthTexture = GSystemTextures.GetBlackDummy(GraphBuilder);

		FVector2D VolumetricTextureSize = FVector2D(float(VolumetricTexture->Desc.GetSize().X), float(VolumetricTexture->Desc.GetSize().Y));
		PassParameters->VolumetricTextureSizeAndInvSize = FVector4f(VolumetricTextureSize.X, VolumetricTextureSize.Y, 1.0f / VolumetricTextureSize.X, 1.0f / VolumetricTextureSize.Y);

		FPixelShaderUtils::AddFullscreenPass<FComposeVolumetricRTOverScenePS>(
			GraphBuilder, ViewInfo.ShaderMap, RDG_EVENT_NAME("VolumetricComposeOverSceneForVisualization"), PixelShader, PassParameters, ViewInfo.ViewRect);
	}
}

//////////////////////////////////////////////////////////////////////////



FTemporalRenderTargetState::FTemporalRenderTargetState()
	: CurrentRT(1)
	, FrameId(0)
	, bFirstTimeUsed(true)
	, bHistoryValid(false)
	, Resolution(FIntPoint::ZeroValue)
	, Format(PF_MAX)
{ 
}

FTemporalRenderTargetState::~FTemporalRenderTargetState()
{
}

void FTemporalRenderTargetState::Initialise(const FIntPoint& ResolutionIn, EPixelFormat FormatIn)
{
	// Update internal settings

	if (bFirstTimeUsed)
	{
		bFirstTimeUsed = false;
		bHistoryValid = false;
		FrameId = 0;
	}

	CurrentRT = 1 - CurrentRT;
	const uint32 PreviousRT = 1 - CurrentRT;

	FIntVector ResolutionVector = FIntVector(ResolutionIn.X, ResolutionIn.Y, 0);
	for (int32 i = 0; i < kRenderTargetCount; ++i)
	{
		if (RenderTargets[i].IsValid() && (RenderTargets[i]->GetDesc().GetSize() != ResolutionVector || Format != FormatIn))
		{
			// Resolution does not match so release target we are going to render in, keep the previous one at a different resolution.
			RenderTargets[i].SafeRelease();
		}
	}
	Resolution = ResolutionIn;
	Format = FormatIn;

	// Regular every frame update
	bHistoryValid = RenderTargets[PreviousRT].IsValid();
}

FRDGTextureRef FTemporalRenderTargetState::GetOrCreateCurrentRT(FRDGBuilder& GraphBuilder)
{
	check(Resolution.X > 0 && Resolution.Y > 0);

	if (RenderTargets[CurrentRT].IsValid())
	{
		return GraphBuilder.RegisterExternalTexture(RenderTargets[CurrentRT]);
	}

	FRDGTextureRef RDGTexture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(Resolution, Format, FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)), 
			TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable), TEXT("VolumetricRenderTarget.GeneralTemporalTexture"));
	return RDGTexture;
}
void FTemporalRenderTargetState::ExtractCurrentRT(FRDGBuilder& GraphBuilder, FRDGTextureRef RDGTexture)
{
	check(Resolution.X > 0 && Resolution.Y > 0);

	RenderTargets[CurrentRT] = GraphBuilder.ConvertToExternalTexture(RDGTexture);
}

FRDGTextureRef FTemporalRenderTargetState::GetOrCreatePreviousRT(FRDGBuilder& GraphBuilder)
{
	check(Resolution.X > 0 && Resolution.Y > 0);
	const uint32 PreviousRT = 1u - CurrentRT;
	check(RenderTargets[PreviousRT].IsValid());

	return GraphBuilder.RegisterExternalTexture(RenderTargets[PreviousRT]);
}

void FTemporalRenderTargetState::Reset()
{
	bFirstTimeUsed = false;
	bHistoryValid = false;
	FrameId = 0;
	for (int32 i = 0; i < kRenderTargetCount; ++i)
	{
		RenderTargets[i].SafeRelease();
	}
	Resolution = FIntPoint::ZeroValue;
	Format = PF_MAX;
}

