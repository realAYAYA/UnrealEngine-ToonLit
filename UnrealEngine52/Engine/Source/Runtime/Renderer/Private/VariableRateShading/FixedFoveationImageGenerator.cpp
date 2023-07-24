// Copyright Epic Games, Inc. All Rights Reserved.

#include "FixedFoveationImageGenerator.h"
#include "SystemTextures.h"
#include "GlobalShader.h"
#include "IXRTrackingSystem.h"
#include "StereoRendering.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneTexturesConfig.h"

/* CVar values used to control generator behavior */

static TAutoConsoleVariable<int> CVarFixedFoveationLevel(
	TEXT("vr.VRS.HMDFixedFoveationLevel"),
	0,
	TEXT("Level of fixed-foveation VRS to apply (when Variable Rate Shading is available)\n")
	TEXT(" 0: Disabled (default);\n")
	TEXT(" 1: Low;\n")
	TEXT(" 2: Medium;\n")
	TEXT(" 3: High;\n")
	TEXT(" 4: High Top;\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarFixedFoveationDynamic(
	TEXT("vr.VRS.HMDFixedFoveationDynamic"),
	0,
	TEXT("Whether fixed-foveation level should adjust based on GPU utilization\n")
	TEXT(" 0: Disabled (default);\n")
	TEXT(" 1: Enabled\n"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarFoveatedPreview(
	TEXT("vr.VRS.HMDFixedFoveationPreview"),
	1,
	TEXT("Include foveated VRS in the VRS debug overlay.")
	TEXT(" 0: Disabled;\n")
	TEXT(" 1: Enabled (default)\n"),
	ECVF_RenderThreadSafe);


/* Image generation parameters, set up by Prepare() */

FVector2f HMDFieldOfView = FVector2f(90.0f, 90.0f);

float FixedFoveationFullRateCutoff = 1.0f;
float FixedFoveationHalfRateCutoff = 1.0f;

float FixedFoveationCenterX = 0.5f;
float FixedFoveationCenterY = 0.5f;

bool bGenerateFixedFoveation = false;


/* Shader parameters and parameter structs */

constexpr int32 kComputeGroupSize = FComputeShaderUtils::kGolden2DGroupSize;
constexpr int32 kMaxCombinedSources = 4;

enum class EVRSGenerationFlags : uint32
{
	None = 0x0,
	StereoRendering = 0x1,
	SideBySideStereo = 0x2,
	HMDFixedFoveation = 0x4,
	HMDEyeTrackedFoveation = 0x8,
};

ENUM_CLASS_FLAGS(EVRSGenerationFlags);


/* The shader itself, which generates the foveated shading rate image */

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

IMPLEMENT_GLOBAL_SHADER(FComputeVariableRateShadingImageGeneration, "/Engine/Private/VariableRateShading/FixedFoveationVariableRateShading.usf", "GenerateShadingRateTexture", SF_Compute);

FRDGTextureRef FFixedFoveationImageGenerator::GetImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSPassType PassType)
{
	return CachedImage;
}

void FFixedFoveationImageGenerator::PrepareImages(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures)
{
	// TODO: Skip some portion of this update if the CVars haven't changed at all

	// VRS level parameters - pretty arbitrary right now, later should depend on device characteristics
	static const float kFixedFoveationFullRateCutoffs[] = { 1.0f, 0.7f, 0.50f, 0.35f, 0.35f };
	static const float kFixedFoveationHalfRateCutofffs[] = { 1.0f, 0.9f, 0.75f, 0.55f, 0.55f };
	static const float kFixedFoveationCenterX[] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
	static const float kFixedFoveationCenterY[] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.42f };

	const int VRSMaxLevel = FMath::Clamp(CVarFixedFoveationLevel.GetValueOnAnyThread(), 0, 4);

	float VRSAmount = 1.0f;

	if (CVarFixedFoveationDynamic.GetValueOnAnyThread() && VRSMaxLevel > 0)
	{
		VRSAmount = GetDynamicVRSAmount();
	}

	bGenerateFixedFoveation = (VRSMaxLevel > 0 && VRSAmount > 0.0f);
	FixedFoveationFullRateCutoff = FMath::Lerp(kFixedFoveationFullRateCutoffs[0], kFixedFoveationFullRateCutoffs[VRSMaxLevel], VRSAmount);
	FixedFoveationHalfRateCutoff = FMath::Lerp(kFixedFoveationHalfRateCutofffs[0], kFixedFoveationHalfRateCutofffs[VRSMaxLevel], VRSAmount);
	FixedFoveationCenterX = FMath::Lerp(kFixedFoveationCenterX[0], kFixedFoveationCenterX[VRSMaxLevel], VRSAmount);
	FixedFoveationCenterY = FMath::Lerp(kFixedFoveationCenterY[0], kFixedFoveationCenterY[VRSMaxLevel], VRSAmount);

	FIntPoint Size = FSceneTexturesConfig::Get().Extent;
	bool bStereoRendering = IStereoRendering::IsStereoEyeView(*ViewFamily.Views[0]) && GEngine->XRSystem.IsValid();

	// Sanity check VRS tile size.
	check(GRHIVariableRateShadingImageTileMinWidth >= 8 && GRHIVariableRateShadingImageTileMinWidth <= 64 && GRHIVariableRateShadingImageTileMinHeight >= 8 && GRHIVariableRateShadingImageTileMaxHeight <= 64);

	FIntPoint TextureSize(Size.X / GRHIVariableRateShadingImageTileMinWidth, Size.Y / GRHIVariableRateShadingImageTileMinHeight);

	// Create texture to hold shading rate image
	FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
		TextureSize,
		GRHIVariableRateShadingImageFormat,
		FClearValueBinding::None,
		TexCreate_Foveation | TexCreate_UAV);

	FRDGTextureRef ShadingRateTexture = GraphBuilder.CreateTexture(Desc, TEXT("FixedFoveationShadingRateTexture"));

	// Setup shader parameters
	FComputeVariableRateShadingImageGeneration::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeVariableRateShadingImageGeneration::FParameters>();
	PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(ShadingRateTexture);

	PassParameters->HMDFieldOfView = HMDFieldOfView;

	PassParameters->FixedFoveationFullRateCutoffSquared = FixedFoveationFullRateCutoff * FixedFoveationFullRateCutoff;
	PassParameters->FixedFoveationHalfRateCutoffSquared = FixedFoveationHalfRateCutoff * FixedFoveationHalfRateCutoff;

	PassParameters->LeftEyeCenterPixelXY = FVector2f(TextureSize.X * FixedFoveationCenterX, TextureSize.Y * FixedFoveationCenterY);
	PassParameters->RightEyeCenterPixelXY = PassParameters->LeftEyeCenterPixelXY;

	// Set up flags - can later allow eye-tracking here since the shader theoretically supports it
	EVRSGenerationFlags GenFlags = EVRSGenerationFlags::HMDFixedFoveation;
	if (bStereoRendering)
	{
		EnumAddFlags(GenFlags, EVRSGenerationFlags::StereoRendering);
		EnumAddFlags(GenFlags, EVRSGenerationFlags::SideBySideStereo); // May need to change this if we add support for mobile multi-view

		// Adjust eyes for side-by-side stereo
		PassParameters->LeftEyeCenterPixelXY.X /= 2;
		PassParameters->RightEyeCenterPixelXY.X = PassParameters->LeftEyeCenterPixelXY.X + TextureSize.X / 2;
	}

	PassParameters->ViewDiagonalSquaredInPixels = FVector2f::DotProduct(PassParameters->LeftEyeCenterPixelXY, PassParameters->LeftEyeCenterPixelXY);
	PassParameters->CombineSourceCount = 0;
	PassParameters->ShadingRateAttachmentGenerationFlags = (uint32)GenFlags;

	TShaderMapRef<FComputeVariableRateShadingImageGeneration> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(TextureSize, FComputeShaderUtils::kGolden2DGroupSize);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("GenerateFixedFoveationVRSImage"),
		PassParameters,
		ERDGPassFlags::Compute,
		[PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
		{
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
		});

	CachedImage = ShadingRateTexture;
}

bool FFixedFoveationImageGenerator::IsEnabledForView(const FSceneView& View) const
{
	// Only enabled if we are in XR
	bool bStereoRendering = IStereoRendering::IsStereoEyeView(View) && GEngine->XRSystem.IsValid();
	return bStereoRendering && CVarFixedFoveationLevel.GetValueOnRenderThread() > 0;
}

FRDGTextureRef FFixedFoveationImageGenerator::GetDebugImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo)
{
	return CachedImage;
}

float FFixedFoveationImageGenerator::GetDynamicVRSAmount()
{
	const float		kUpdateIncrement = 0.1f;
	const int		kNumFramesToAverage = 10;
	const double	kRoundingErrorMargin = 0.5; // Add a small margin (.5 ms) to avoid oscillation
	const double	kDesktopMaxAllowedFrameTime = 12.50; // 80 fps
	const double	kMobileMaxAllowedFrameTime = 16.66; // 60 fps

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