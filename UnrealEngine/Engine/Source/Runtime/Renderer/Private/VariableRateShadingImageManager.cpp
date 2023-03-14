// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariableRateShadingImageManager.h"
#include "StereoRenderTargetManager.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHIDefinitions.h"
#include "RenderTargetPool.h"
#include "SystemTextures.h"
#include "SceneView.h"
#include "IEyeTracker.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"

const int32 kComputeGroupSize = FComputeShaderUtils::kGolden2DGroupSize;

enum class EVRSGenerationFlags : uint32
{
	None = 0x0,
	StereoRendering = 0x1,
	SideBySideStereo = 0x2,
	HMDFixedFoveation = 0x4,
	HMDEyeTrackedFoveation = 0x8,
};

ENUM_CLASS_FLAGS(EVRSGenerationFlags);

const int32 kMaxCombinedSources = 4;

static TAutoConsoleVariable<int> CVarHMDFixedFoveationLevel(
	TEXT("vr.VRS.HMDFixedFoveationLevel"),
	0,
	TEXT("Level of fixed-foveation VRS to apply (when Variable Rate Shading is available)\n")
	TEXT(" 0: Disabled (default);\n")
	TEXT(" 1: Low;\n")
	TEXT(" 2: Medium;\n")
	TEXT(" 3: High;\n")
	TEXT(" 4: High Top;\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHMDFixedFoveationDynamic(
	TEXT("vr.VRS.HMDFixedFoveationDynamic"),
	0,
	TEXT("Whether fixed-foveation level should adjust based on GPU utilization\n")
	TEXT(" 0: Disabled (default);\n")
	TEXT(" 1: Enabled\n"),
	ECVF_RenderThreadSafe);

TGlobalResource<FVariableRateShadingImageManager> GVRSImageManager;

class FComputeVariableRateShadingImageGeneration : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputeVariableRateShadingImageGeneration);
	SHADER_USE_PARAMETER_STRUCT(FComputeVariableRateShadingImageGeneration, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWOutputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_ARRAY(Texture2D<uint>, CombineSourceIn, [kMaxCombinedSources])
		SHADER_PARAMETER(FVector2f, HMDFieldOfView)
		SHADER_PARAMETER(FVector2f, LeftEyeCenterPixelXY)
		SHADER_PARAMETER(FVector2f, RightEyeCenterPixelXY)
		SHADER_PARAMETER(float, ViewDiagonalSquaredInPixels)
		SHADER_PARAMETER(float, FixedFoveationFullRateCutoffSquared)
		SHADER_PARAMETER(float, FixedFoveationHalfRateCutoffSquared)
		SHADER_PARAMETER(uint32, CombineSourceCount)
		SHADER_PARAMETER(uint32, ShadingRateAttachmentGenerationFlags)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVariableRateShading(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Shading rates:
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_1x1"), VRSSR_1x1);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_1x2"), VRSSR_1x2);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_2x1"), VRSSR_2x1);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_2x2"), VRSSR_2x2);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_2x4"), VRSSR_2x4);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_4x2"), VRSSR_4x2);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_4x4"), VRSSR_4x4);

		OutEnvironment.SetDefine(TEXT("MAX_COMBINED_SOURCES_IN"), kMaxCombinedSources);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_TILE_WIDTH"), GRHIVariableRateShadingImageTileMinWidth);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_TILE_HEIGHT"), GRHIVariableRateShadingImageTileMinHeight);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), kComputeGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), kComputeGroupSize);

		OutEnvironment.SetDefine(TEXT("STEREO_RENDERING"), (uint32)EVRSGenerationFlags::StereoRendering);
		OutEnvironment.SetDefine(TEXT("SIDE_BY_SIDE_STEREO"), (uint32)EVRSGenerationFlags::SideBySideStereo);
		OutEnvironment.SetDefine(TEXT("HMD_FIXED_FOVEATION"), (uint32)EVRSGenerationFlags::HMDFixedFoveation);
		OutEnvironment.SetDefine(TEXT("HMD_EYETRACKED_FOVEATION"), (uint32)EVRSGenerationFlags::HMDEyeTrackedFoveation);
	}
};

IMPLEMENT_GLOBAL_SHADER(FComputeVariableRateShadingImageGeneration, "/Engine/Private/VariableRateShading.usf", "GenerateShadingRateTexture", SF_Compute);

/**
 * Struct containing parameters used to build a VRS image.
 */
struct FVRSImageGenerationParameters
{
	FIntPoint Size = FIntPoint(0, 0);

	FVector2f HMDFieldOfView = FVector2f(90.0f, 90.0f);
	FVector2f HMDEyeTrackedFoveationOrigin = FVector2f(0.0f, 0.0f);

	float HMDFixedFoveationFullRateCutoff = 1.0f;
	float HMDFixedFoveationHalfRateCutoff = 1.0f;
	float HMDEyeTrackedFoveationFullRateCutoff = 1.0f;
	float HMDEyeTrackedFoveationHalfRateCutoff = 1.0f;

	float HMDFixedFoveationCenterX = 0.5f;
	float HMDFixedFoveationCenterY = 0.5f;

	bool bGenerateFixedFoveation = false;
	bool bGenerateEyeTrackedFoveation = false;
	bool bInstancedStereo = false;
};

uint64 FVariableRateShadingImageManager::CalculateVRSImageHash(const FVRSImageGenerationParameters& GenerationParamsIn, EVRSGenerationFlags GenFlags) const
{
	uint64 Hash = GetTypeHash(GenerationParamsIn.HMDFieldOfView);
	Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.Size));
	Hash = HashCombine(Hash, GetTypeHash(GenFlags));

	// Currently the only view flag that differentiates is stereo, but down the road there might be others.
	if (EnumHasAllFlags(GenFlags, EVRSGenerationFlags::StereoRendering))
	{
		// Only hash these values for stereo rendering.
		Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.HMDFixedFoveationFullRateCutoff));
		Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.HMDFixedFoveationHalfRateCutoff));
		Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.HMDEyeTrackedFoveationOrigin));
		Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.HMDEyeTrackedFoveationFullRateCutoff));
		Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.HMDEyeTrackedFoveationHalfRateCutoff));
		Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.bInstancedStereo));
		Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.HMDFixedFoveationCenterX));
		Hash = HashCombine(Hash, GetTypeHash(GenerationParamsIn.HMDFixedFoveationCenterY));
	}

	return Hash;
}

FVariableRateShadingImageManager::FVariableRateShadingImageManager()
	: FRenderResource()
	, LastFrameTick(0)
{
}

FVariableRateShadingImageManager::~FVariableRateShadingImageManager()
{}

void FVariableRateShadingImageManager::ReleaseDynamicRHI()
{
	for (auto& Image : ActiveVRSImages)
	{
		Image.Value.Target.SafeRelease();
	}

	ActiveVRSImages.Empty();

	GRenderTargetPool.FreeUnusedResources();
}

FRDGTextureRef FVariableRateShadingImageManager::GetVariableRateShadingImage(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const TArray<TRefCountPtr<IPooledRenderTarget>>* ExternalVRSSources, EVRSType VRSTypesToExclude)
{
	// If the RHI doesn't support VRS, we should always bail immediately.
	if (!GRHISupportsAttachmentVariableRateShading || !GRHIVariableRateShadingEnabled || !GRHIAttachmentVariableRateShadingEnabled || !FDataDrivenShaderPlatformInfo::GetSupportsVariableRateShading(GMaxRHIShaderPlatform))
	{
		return nullptr;
	}

	// Also bail if we're given a ViewFamily with no valid RenderTarget
	if (ViewFamily.RenderTarget == nullptr)
	{
		ensureMsgf(0, TEXT("VRS Image Manager does not support ViewFamilies with no valid RenderTarget"));
		return nullptr;
	}

	// Always want to make sure we tick every frame, even if we're not going to be generating any VRS images.
	Tick();

	if (EnumHasAllFlags(VRSTypesToExclude, EVRSType::All))
	{
		return nullptr;
	}

	FVRSImageGenerationParameters VRSImageParams;

	const bool bIsStereo = IStereoRendering::IsStereoEyeView(*ViewFamily.Views[0]) && GEngine->XRSystem.IsValid();
	
	VRSImageParams.bInstancedStereo |= ViewFamily.Views[0]->IsInstancedStereoPass();
	VRSImageParams.Size = FIntPoint(ViewFamily.RenderTarget->GetSizeXY());

	UpdateFixedFoveationParameters(VRSImageParams);
	UpdateEyeTrackedFoveationParameters(VRSImageParams, ViewFamily);

	EVRSGenerationFlags GenFlags = EVRSGenerationFlags::None;

	// Setup generation flags for XR foveation VRS generation.
	if (bIsStereo && !EnumHasAnyFlags(VRSTypesToExclude, EVRSType::XRFoveation) && !EnumHasAnyFlags(VRSTypesToExclude, EVRSType::EyeTrackedFoveation))
	{
		EnumAddFlags(GenFlags, EVRSGenerationFlags::StereoRendering);

		if (!EnumHasAnyFlags(VRSTypesToExclude, EVRSType::FixedFoveation) && VRSImageParams.bGenerateFixedFoveation)
		{
			EnumAddFlags(GenFlags, EVRSGenerationFlags::HMDFixedFoveation);
		}

		if (!EnumHasAllFlags(VRSTypesToExclude, EVRSType::EyeTrackedFoveation) && VRSImageParams.bGenerateEyeTrackedFoveation)
		{
			EnumAddFlags(GenFlags, EVRSGenerationFlags::HMDEyeTrackedFoveation);
		}

		if (VRSImageParams.bInstancedStereo)
		{
			EnumAddFlags(GenFlags, EVRSGenerationFlags::SideBySideStereo);
		}
	}

	// @todo: Other VRS generation flags here.

	if (GenFlags == EVRSGenerationFlags::None)
	{
		if (ExternalVRSSources == nullptr || ExternalVRSSources->Num() == 0)
		{
			// Nothing to generate.
			return nullptr;
		}
		else
		{
			// If there's one external VRS image, just return that since we're not building anything here.
			if (ExternalVRSSources->Num() == 1)
			{
				const FIntVector& ExtSize = (*ExternalVRSSources)[0]->GetDesc().GetSize();
				check(ExtSize.X == VRSImageParams.Size.X / GRHIVariableRateShadingImageTileMinWidth && ExtSize.Y == VRSImageParams.Size.Y / GRHIVariableRateShadingImageTileMinHeight);
				return GraphBuilder.RegisterExternalTexture((*ExternalVRSSources)[0]);
			}

			// If there is more than one external image, we'll generate a final one by combining, so fall through.
		}
	}

	IHeadMountedDisplay* HMDDevice = (GEngine->XRSystem == nullptr) ? nullptr : GEngine->XRSystem->GetHMDDevice();
	if (HMDDevice != nullptr)
	{
		HMDDevice->GetFieldOfView(VRSImageParams.HMDFieldOfView.X, VRSImageParams.HMDFieldOfView.Y);
	}

	const uint64 Key = CalculateVRSImageHash(VRSImageParams, GenFlags);
	FActiveTarget* ActiveTarget = ActiveVRSImages.Find(Key);
	if (ActiveTarget == nullptr)
	{
		// Render it.
		return GraphBuilder.RegisterExternalTexture(RenderShadingRateImage(GraphBuilder, Key, VRSImageParams, GenFlags));
	}

	ActiveTarget->LastUsedFrame = GFrameNumber;

	return GraphBuilder.RegisterExternalTexture(ActiveTarget->Target);
}

TRefCountPtr<IPooledRenderTarget> FVariableRateShadingImageManager::RenderShadingRateImage(FRDGBuilder& GraphBuilder, uint64 Key, const FVRSImageGenerationParameters& VRSImageGenParamsIn, EVRSGenerationFlags GenFlags)
{
	// Sanity check VRS tile size.
	check(GRHIVariableRateShadingImageTileMinWidth >= 8 && GRHIVariableRateShadingImageTileMinWidth <= 64 && GRHIVariableRateShadingImageTileMinHeight >= 8 && GRHIVariableRateShadingImageTileMaxHeight <= 64);

	FIntPoint AttachmentSize(VRSImageGenParamsIn.Size.X / GRHIVariableRateShadingImageTileMinWidth, VRSImageGenParamsIn.Size.Y / GRHIVariableRateShadingImageTileMinHeight);

	FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
		AttachmentSize,
		GRHIVariableRateShadingImageFormat,
		FClearValueBinding::None,
		TexCreate_Foveation,
		TexCreate_UAV,
		false);

	TRefCountPtr<IPooledRenderTarget> Attachment;

	GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, Attachment, TEXT("VariableRateShadingAttachment"));

	if (!Attachment.IsValid())
	{
		return Attachment;
	}

	FRDGTextureRef RDGAttachment = GraphBuilder.RegisterExternalTexture(Attachment);

	FComputeVariableRateShadingImageGeneration::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeVariableRateShadingImageGeneration::FParameters>();
	PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(RDGAttachment);

	PassParameters->HMDFieldOfView = VRSImageGenParamsIn.HMDFieldOfView;

	PassParameters->FixedFoveationFullRateCutoffSquared = VRSImageGenParamsIn.HMDFixedFoveationFullRateCutoff * VRSImageGenParamsIn.HMDFixedFoveationFullRateCutoff;
	PassParameters->FixedFoveationHalfRateCutoffSquared = VRSImageGenParamsIn.HMDFixedFoveationHalfRateCutoff * VRSImageGenParamsIn.HMDFixedFoveationHalfRateCutoff;

	PassParameters->LeftEyeCenterPixelXY = FVector2f(AttachmentSize.X * VRSImageGenParamsIn.HMDFixedFoveationCenterX, AttachmentSize.Y * VRSImageGenParamsIn.HMDFixedFoveationCenterY);
	PassParameters->RightEyeCenterPixelXY = PassParameters->LeftEyeCenterPixelXY;

	// If instanced (side-by-side) stereo, there's two "center" points, so adjust both eyes
	if (VRSImageGenParamsIn.bInstancedStereo)
	{
		PassParameters->LeftEyeCenterPixelXY.X /= 2;
		PassParameters->RightEyeCenterPixelXY.X = PassParameters->LeftEyeCenterPixelXY.X + AttachmentSize.X / 2;
	}

	PassParameters->ViewDiagonalSquaredInPixels = FVector2f::DotProduct(PassParameters->LeftEyeCenterPixelXY, PassParameters->LeftEyeCenterPixelXY);
	PassParameters->CombineSourceCount = 0;
	PassParameters->ShadingRateAttachmentGenerationFlags = (uint32)GenFlags;

	// @todo: When we have other VRS sources to combine in.
	for (uint32 i = 0; i < kMaxCombinedSources; ++i)
	{
		PassParameters->CombineSourceIn[i] = GraphBuilder.RegisterExternalTexture(GSystemTextures.WhiteDummy, TEXT("CombineSourceDummy"));
	}

	TShaderMapRef<FComputeVariableRateShadingImageGeneration> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(AttachmentSize, FComputeShaderUtils::kGolden2DGroupSize);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("GenerateVariableRateShadingImage"),
		PassParameters,
		ERDGPassFlags::Compute,
		[PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
	{
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
	});

	ActiveVRSImages.Add(Key, FActiveTarget(Attachment));

	return Attachment;
}

TRefCountPtr<IPooledRenderTarget> FVariableRateShadingImageManager::GetMobileVariableRateShadingImage(const FSceneViewFamily& ViewFamily)
{
	if (!(IStereoRendering::IsStereoEyeView(*ViewFamily.Views[0]) && GEngine->XRSystem.IsValid()))
	{
		return TRefCountPtr<IPooledRenderTarget>();
	}

	FIntPoint Size(ViewFamily.RenderTarget->GetSizeXY());

	const bool bStereo = GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled();
	IStereoRenderTargetManager* const StereoRenderTargetManager = bStereo ? GEngine->StereoRenderingDevice->GetRenderTargetManager() : nullptr;

	FTexture2DRHIRef Texture;
	FIntPoint TextureSize(0, 0);

	// Allocate variable resolution texture for VR foveation if supported
	if (StereoRenderTargetManager && StereoRenderTargetManager->NeedReAllocateShadingRateTexture(MobileHMDFixedFoveationOverrideImage))
	{
		bool bAllocatedShadingRateTexture = StereoRenderTargetManager->AllocateShadingRateTexture(0, Size.X, Size.Y, GRHIVariableRateShadingImageFormat, 0, TexCreate_None, TexCreate_None, Texture, TextureSize);
		if (bAllocatedShadingRateTexture)
		{
			MobileHMDFixedFoveationOverrideImage = CreateRenderTarget(Texture, TEXT("ShadingRate"));
		}
	}

	return MobileHMDFixedFoveationOverrideImage;
}

void FVariableRateShadingImageManager::Tick()
{
	static const uint64 RecycleTargetAfterNFrames = 12;

	uint64 FrameNumber = GFrameNumber;

	if (LastFrameTick == GFrameNumber)
	{
		return;
	}

	TArray<uint64> RemoveKeys;

	for (auto& ActiveTarget : ActiveVRSImages)
	{
		if ((GFrameNumber - ActiveTarget.Value.LastUsedFrame) > RecycleTargetAfterNFrames)
		{
			ActiveTarget.Value.Target.SafeRelease();
			RemoveKeys.Add(ActiveTarget.Key);
		}
	}

	for (uint64 Key : RemoveKeys)
	{
		ActiveVRSImages.Remove(Key);
	}

	LastFrameTick = FrameNumber;
}

void FVariableRateShadingImageManager::UpdateFixedFoveationParameters(FVRSImageGenerationParameters& VRSImageGenParamsInOut)
{

	// VRS level parameters - pretty arbitrary right now, later should depend on device characteristics
	static const float kFixedFoveationFullRateCutoffs[] = { 1.0f, 0.7f, 0.50f, 0.35f, 0.35f };
	static const float kFixedFoveationHalfRateCutofffs[] = { 1.0f, 0.9f, 0.75f, 0.55f, 0.55f };
	static const float kFixedFoveationCenterX[] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
	static const float kFixedFoveationCenterY[] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.42f };


	const int VRSMaxLevel = FMath::Clamp(CVarHMDFixedFoveationLevel.GetValueOnAnyThread(), 0, 4);
	float VRSAmount = 1.0f;
	
	if (CVarHMDFixedFoveationDynamic.GetValueOnAnyThread() && VRSMaxLevel > 0)
	{
		VRSAmount = GetDynamicVRSAmount();
	}
	
	VRSImageGenParamsInOut.bGenerateFixedFoveation = (VRSMaxLevel > 0 && VRSAmount > 0.0f);
	VRSImageGenParamsInOut.HMDFixedFoveationFullRateCutoff = FMath::Lerp(kFixedFoveationFullRateCutoffs[0], kFixedFoveationFullRateCutoffs[VRSMaxLevel], VRSAmount);
	VRSImageGenParamsInOut.HMDFixedFoveationHalfRateCutoff = FMath::Lerp(kFixedFoveationHalfRateCutofffs[0], kFixedFoveationHalfRateCutofffs[VRSMaxLevel], VRSAmount);
	VRSImageGenParamsInOut.HMDFixedFoveationCenterX = FMath::Lerp(kFixedFoveationCenterX[0], kFixedFoveationCenterX[VRSMaxLevel], VRSAmount);
	VRSImageGenParamsInOut.HMDFixedFoveationCenterY = FMath::Lerp(kFixedFoveationCenterY[0], kFixedFoveationCenterY[VRSMaxLevel], VRSAmount);
}

void FVariableRateShadingImageManager::UpdateEyeTrackedFoveationParameters(FVRSImageGenerationParameters& VRSImageGenParamsInOut, const FSceneViewFamily& ViewFamily)
{
	VRSImageGenParamsInOut.bGenerateEyeTrackedFoveation = false;
	VRSImageGenParamsInOut.HMDEyeTrackedFoveationFullRateCutoff = 1.0f;
	VRSImageGenParamsInOut.HMDEyeTrackedFoveationHalfRateCutoff = 1.0f;

	auto EyeTracker = GEngine->EyeTrackingDevice;
	if (!EyeTracker.IsValid())
	{
		return;
	}

	FEyeTrackerGazeData GazeData;
	if (!EyeTracker->GetEyeTrackerGazeData(GazeData))
	{
		return;
	}

	// @todo:
}

float FVariableRateShadingImageManager::GetDynamicVRSAmount()
{
	const float		kUpdateIncrement		= 0.1f;
	const int		kNumFramesToAverage		= 10; 
	const double	kRoundingErrorMargin	= 0.5; // Add a small margin (.5 ms) to avoid oscillation
	const double	kDesktopMaxAllowedFrameTime	= 12.50; // 80 fps
	const double	kMobileMaxAllowedFrameTime	= 16.66; // 60 fps

	if (GFrameNumber != DynamicVRSData.LastUpdateFrame)
	{
		DynamicVRSData.LastUpdateFrame = GFrameNumber;

		const double GPUFrameTime = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		
		// Update sum for rolling average
		if (DynamicVRSData.NumFramesStored < kNumFramesToAverage)
		{
			DynamicVRSData.SumBusyTime += GPUFrameTime;
			DynamicVRSData.NumFramesStored++;
		}
		else if (DynamicVRSData.NumFramesStored == kNumFramesToAverage)
		{
			DynamicVRSData.SumBusyTime -= DynamicVRSData.SumBusyTime / kNumFramesToAverage;
			DynamicVRSData.SumBusyTime += GPUFrameTime;
		}

		// If rolling average has sufficient samples, check if update is necessary
		if (DynamicVRSData.NumFramesStored == kNumFramesToAverage)
		{
			const double AverageBusyTime = DynamicVRSData.SumBusyTime / kNumFramesToAverage;
			const double TargetBusyTime = IsMobilePlatform(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]) ? kMobileMaxAllowedFrameTime : kDesktopMaxAllowedFrameTime;
			
			if (AverageBusyTime > TargetBusyTime + kRoundingErrorMargin && DynamicVRSData.VRSAmount < 1.0f)
			{
				DynamicVRSData.VRSAmount = FMath::Clamp(DynamicVRSData.VRSAmount + kUpdateIncrement, 0.0f, 1.0f);
				DynamicVRSData.SumBusyTime = 0.0;
				DynamicVRSData.NumFramesStored = 0;
			}
			else if (AverageBusyTime < TargetBusyTime - kRoundingErrorMargin && DynamicVRSData.VRSAmount > 0.0f)
			{
				DynamicVRSData.VRSAmount = FMath::Clamp(DynamicVRSData.VRSAmount - kUpdateIncrement, 0.0f, 1.0f);
				DynamicVRSData.SumBusyTime = 0.0;
				DynamicVRSData.NumFramesStored = 0;
			}
		}
	}

	return DynamicVRSData.VRSAmount;
}
