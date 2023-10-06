// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContrastAdaptiveImageGenerator.h"
#include "StereoRenderTargetManager.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHIDefinitions.h"
#include "RenderTargetPool.h"
#include "SystemTextures.h"
#include "SceneRendering.h"
#include "SceneTextures.h"
#include "SceneView.h"
#include "IEyeTracker.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "PostProcess/PostProcessTonemap.h"
#include "DataDrivenShaderPlatformInfo.h"

/**
 * Contrast Adaptive Shading (CAS) is a Tier 2 Variable Rate Shading method which generates a VRS image by examining the contrast from the previous frame.
 * An image is generated which designates lower shading rates for areas of lower contrast in which reductions are unlikely to be noticed.
 * This image is then reprojected and rescaled in accordance with camera movement and dynamic resolution changes before being provided to the manager.
 */


/**
 * CAS Parameters
 */

TAutoConsoleVariable<int32> CVarCASContrastAdaptiveShading(
	TEXT("r.VRS.ContrastAdaptiveShading"),
	0,
	TEXT("Enables using Variable Rate Shading based on the luminance from the previous frame's post process output \n"),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<float> CVarCASEdgeThreshold(
	TEXT("r.VRS.ContrastAdaptiveShading.EdgeThreshold"),
	0.2,
	TEXT(""),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<float> CVarCASConservativeEdgeThreshold(
	TEXT("r.VRS.ContrastAdaptiveShading.ConservativeEdgeThreshold"),
	0.1,
	TEXT(""),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<float> CVarCAS_HDR10CorrectionMultiplier(
	TEXT("r.VRS.ContrastAdaptiveShading.HDR10CorrectionMultiplier"),
	0.55,
	TEXT("Approximation multiplier to account for how perceptual values are spread out in SDR vs HDR10\n"),
	ECVF_RenderThreadSafe);

/**
 * Debug Settings 
 */

TAutoConsoleVariable<int32> CVarCASPreview(
	TEXT("r.VRS.ContrastAdaptiveShading.Preview"),
	1,
	TEXT("Whether to include CAS in VRS preview overlay.")
	TEXT("0 - off, 1 - on (default)"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarCASPreviewPreReprojection(
	TEXT("r.VRS.ContrastAdaptiveShading.PreviewPreReprojection"),
	0,
	TEXT("Sets CAS preview to use the pre-reprojection SRI. Overrides full vs. conservative images.")
	TEXT("0 - off (default), 1 - on"),
	ECVF_RenderThreadSafe);



/**
 * Shaders
 */

 //---------------------------------------------------------------------------------------------
class FCalculateShadingRateImageCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateShadingRateImageCS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateShadingRateImageCS, FGlobalShader);

	class FThreadGroupX : SHADER_PERMUTATION_SPARSE_INT("THREADGROUP_SIZEX", 8, 16);
	class FThreadGroupY : SHADER_PERMUTATION_SPARSE_INT("THREADGROUP_SIZEY", 8, 16);

	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupX, FThreadGroupY>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, LuminanceTexture)
		SHADER_PARAMETER(FVector4f, ViewRect)
		SHADER_PARAMETER(float, EdgeThreshold)
		SHADER_PARAMETER(float, ConservativeEdgeThreshold)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, VariableRateShadingTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
			&& FDataDrivenShaderPlatformInfo::GetSupportsVariableRateShading(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 1);
	}

	static void InitParameters(
		FParameters& Parameters,
		FRDGTextureRef Luminance,
		const FIntRect& ViewRect,
		bool bIsHDR10,
		FRDGTextureUAV* ShadingRateImage)
	{
		Parameters.LuminanceTexture = Luminance;
		Parameters.ViewRect = FVector4f(ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X, ViewRect.Max.Y);
		const float cEdgeThresholdCorrectionValue = bIsHDR10 ? CVarCAS_HDR10CorrectionMultiplier.GetValueOnRenderThread() : 1.0;
		Parameters.EdgeThreshold = cEdgeThresholdCorrectionValue * CVarCASEdgeThreshold.GetValueOnRenderThread();
		Parameters.ConservativeEdgeThreshold = cEdgeThresholdCorrectionValue * CVarCASConservativeEdgeThreshold.GetValueOnRenderThread();
		Parameters.VariableRateShadingTexture = ShadingRateImage;
	}
};
IMPLEMENT_GLOBAL_SHADER(FCalculateShadingRateImageCS, "/Engine/Private/VariableRateShading/VRSShadingRateCalculate.usf", "CalculateShadingRateImage", SF_Compute);

//---------------------------------------------------------------------------------------------
class FRescaleVariableRateShadingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRescaleVariableRateShadingCS);
	SHADER_USE_PARAMETER_STRUCT(FRescaleVariableRateShadingCS, FGlobalShader);

	static constexpr uint32 ThreadGroupSize = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, InputSRITexture)
		SHADER_PARAMETER(FVector2f, SRIViewRectMin)
		SHADER_PARAMETER(FVector2f, SRIViewRectMax)
		SHADER_PARAMETER(FVector2f, TextureDimensions)
		SHADER_PARAMETER(FVector2f, InvTextureDimensions)
		SHADER_PARAMETER(FVector2f, ScaledSRIDimensions)
		SHADER_PARAMETER(FVector2f, ScaledUVOffset)
		SHADER_PARAMETER(float, InvDynamicResolutionScale)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, ScaledSRITexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, ScaledConservativeSRITexture)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);
	}

	static void InitParameters(
		FParameters& Parameters,
		const FMinimalSceneTextures& SceneTextures,
		const FViewInfo& ViewInfo,
		FRDGTextureRef InputSRITexture,
		FVector2f ViewRectMin,
		FVector2f ViewRectMax,
		FVector2f ScaledSRIDimensions,
		FVector2f TextureDimensions,
		FVector2f ScaledUVOffset,
		float DynamicResolutionScale,
		FRDGTextureUAVRef ScaledSRIUAV,
		FRDGTextureUAVRef ScaledConservateSRIUAV)
	{
		Parameters.SceneTextures = SceneTextures.UniformBuffer;
		Parameters.View = ViewInfo.ViewUniformBuffer;
		Parameters.InputSRITexture = InputSRITexture;
		Parameters.SRIViewRectMin = ViewRectMin;
		Parameters.SRIViewRectMax = ViewRectMax;
		Parameters.TextureDimensions = TextureDimensions;
		Parameters.InvTextureDimensions = FVector2f(1.0f / TextureDimensions.X, 1.0f / TextureDimensions.Y);
		Parameters.ScaledSRIDimensions = ScaledSRIDimensions;
		Parameters.ScaledUVOffset = ScaledUVOffset;
		Parameters.InvDynamicResolutionScale = 1.0f / DynamicResolutionScale;
		Parameters.ScaledSRITexture = ScaledSRIUAV;
		Parameters.ScaledConservativeSRITexture = ScaledConservateSRIUAV;
	}
};
IMPLEMENT_GLOBAL_SHADER(FRescaleVariableRateShadingCS, "/Engine/Private/VariableRateShading/VRSShadingRateReproject.usf", "RescaleVariableRateShading", SF_Compute);

/**
 * Helper Functions and Structures
 */

namespace ESRITextureType
{
	enum Type
	{
		None = 0,
		ScaledSRIForRender,
		ScaledConservativeSRIForRender,
		ConstructedSRI,
		Num
	};
	bool IsInBounds(int32 TypeAsInt)
	{
		return TypeAsInt >= 0 && TypeAsInt < ESRITextureType::Num;
	}
	bool IsInBounds(ESRITextureType::Type TextureType)
	{
		return IsInBounds(static_cast<int32>(TextureType));
	}
	bool IsValidShadingRateTexture(int32 TextureType)
	{
		return IsInBounds(TextureType) && TextureType != ESRITextureType::None && TextureType != ESRITextureType::ConstructedSRI;
	}
	bool IsValidShadingRateTexture(ESRITextureType::Type TextureType)
	{
		return IsValidShadingRateTexture(static_cast<int32>(TextureType));
	}
	ESRITextureType::Type GetTextureType(FVariableRateShadingImageManager::EVRSImageType ImageType)
	{
		return static_cast<ESRITextureType::Type>(ImageType);
	}
};

namespace ESRIPreviewType
{
	enum Type
	{
		Off,
		Projected,
		ProjectedConservative,
		BeforeReprojection,
		Num
	};
	static const TCHAR* Names[] = {
		TEXT("Off"),
		TEXT("Projected"),
		TEXT("ProjectedConservative"),
		TEXT("BeforeReprojection"),
	};
	static const TCHAR* GetName(Type PreviewType)
	{
		if (PreviewType >= Off && PreviewType < Num)
		{
			return Names[static_cast<int32>(PreviewType)];
		}

		return TEXT("Invalid Type");
	}
};

static const TCHAR* ShadingRateTextureName = TEXT("ShadingRateTexture");
static const TCHAR* ScaledShadingRateTextureName = TEXT("ScaledShadingRateTexture");
static const TCHAR* ScaledConservativeShadingRateTextureName = TEXT("ConservativeScaledShadingRateTexture");

struct RENDERER_API FVRSTextures
{
	// Returns an FVRSTextures created immutable instance from the builder blackboard. Asserts if none was created.
	static const FVRSTextures& Get(FRDGBuilder& GraphBuilder)
	{
		const FVRSTextures* VRSTextures = GraphBuilder.Blackboard.Get<FVRSTextures>();
			checkf(VRSTextures, TEXT("FVRSTextures was unexpectedly not initialized."));
		return *VRSTextures;
	}
	static const bool IsInitialized(FRDGBuilder& GraphBuilder)
	{
		const FVRSTextures* VRSTextures = GraphBuilder.Blackboard.Get<FVRSTextures>();
		return VRSTextures != nullptr;
	}
	void Create(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily)
	{
		FRDGTextureDesc ConstructedSRIDesc = CreateSRIDesc(ViewFamily, false);
		ConstructedSRI = GraphBuilder.CreateTexture(ConstructedSRIDesc, ShadingRateTextureName);
		FRDGTextureDesc ScaledSRIDesc = CreateSRIDesc(ViewFamily, true);
		ScaledSRI = GraphBuilder.CreateTexture(ScaledSRIDesc, ScaledShadingRateTextureName);
		ScaledConservativeSRI = GraphBuilder.CreateTexture(ScaledSRIDesc, ScaledConservativeShadingRateTextureName);
	}
	FRDGTextureRef ConstructedSRI;
	FRDGTextureRef ScaledSRI;
	FRDGTextureRef ScaledConservativeSRI;
private:
	static FRDGTextureDesc CreateSRIDesc(const FSceneViewFamily& ViewFamily, bool bIsForDynResScaled)
	{
		if (bIsForDynResScaled)
		{
			// Use SceneTextures
			return FVariableRateShadingImageManager::GetSRIDesc();
		}
		else
		{
			// Get initial size based on luminance texture from previous frame
			check(ViewFamily.Views[0]->bIsViewInfo);
			const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(ViewFamily.Views[0]);
			const FIntPoint ViewTargetExtents = ViewInfo->PrevViewInfo.LuminanceHistory->GetDesc().Extent;

			const FIntPoint SRIDimensions = FMath::DivideAndRoundUp(ViewTargetExtents, FVariableRateShadingImageManager::GetSRITileSize());
			return FRDGTextureDesc::Create2D(
				SRIDimensions,
				GRHIVariableRateShadingImageFormat,
				EClearBinding::ENoneBound,
				ETextureCreateFlags::DisableDCC |
				ETextureCreateFlags::ShaderResource |
				ETextureCreateFlags::UAV);
		}
	}
};
RDG_REGISTER_BLACKBOARD_STRUCT(FVRSTextures);

static EDisplayOutputFormat GetDisplayOutputFormat(const FSceneView& View)
{
	FTonemapperOutputDeviceParameters Parameters = GetTonemapperOutputDeviceParameters(*View.Family);
	return (EDisplayOutputFormat)Parameters.OutputDevice;
}

static bool IsHDR10(const EDisplayOutputFormat& OutputFormat)
{
	return OutputFormat == EDisplayOutputFormat::HDR_ACES_1000nit_ST2084 ||
		OutputFormat == EDisplayOutputFormat::HDR_ACES_2000nit_ST2084;
}

static bool IsContrastAdaptiveShadingEnabled()
{
	return GRHISupportsAttachmentVariableRateShading && GRHIAttachmentVariableRateShadingEnabled && (CVarCASContrastAdaptiveShading.GetValueOnRenderThread() != 0);
}

static FIntRect GetPostProcessOutputRect(const FViewInfo& ViewInfo)
{
	// If TAA/TSR is enabled, upscaling is done at the start of post-processing so the final output will match UnscaledViewRect. Otherwise use the dynamically rescale view rect since
	// the secondary upscale will happen after post processing
	return ViewInfo.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale ? ViewInfo.UnscaledViewRect.Scale(ViewInfo.Family->SecondaryViewFraction) : ViewInfo.ViewRect;
}

bool AddCreateShadingRateImagePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View)
{
	//------------------------------------------------------------------------------------------------
	// Do some sanity checks for early out
	if (!IsContrastAdaptiveShadingEnabled() || !FVariableRateShadingImageManager::IsVRSCompatibleWithView(View) || !View.PrevViewInfo.LuminanceHistory)
	{
		// Shading Rate Image unsupported
		return false;
	}
	FRDGTextureRef Luminance = GraphBuilder.RegisterExternalTexture(View.PrevViewInfo.LuminanceHistory);
	const FVRSTextures& VRSTextures = FVRSTextures::Get(GraphBuilder);
	// Complete early out sanity checks
	//------------------------------------------------------------------------------------------------

	{
		FCalculateShadingRateImageCS::FPermutationDomain PermutationVector;

		const FIntPoint TileSize = FVariableRateShadingImageManager::GetSRITileSize();
		PermutationVector.Set<FCalculateShadingRateImageCS::FThreadGroupX>(TileSize.X);
		PermutationVector.Set<FCalculateShadingRateImageCS::FThreadGroupY>(TileSize.Y);

		TShaderMapRef<FCalculateShadingRateImageCS> ComputeShader(View.ShaderMap, PermutationVector);
		auto* PassParameters = GraphBuilder.AllocParameters<FCalculateShadingRateImageCS::FParameters>();
		EDisplayOutputFormat OutputDisplayFormat = GetDisplayOutputFormat(View);
		FIntRect PostProcessRect = View.UnscaledViewRect.Scale(View.Family->SecondaryViewFraction);
		FCalculateShadingRateImageCS::InitParameters(
			*PassParameters,
			Luminance,
			PostProcessRect,
			IsHDR10(OutputDisplayFormat),
			GraphBuilder.CreateUAV(VRSTextures.ConstructedSRI));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CreateShadingRateImage"),
			ERDGPassFlags::AsyncCompute | ERDGPassFlags::NeverCull,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(PostProcessRect.Size(), TileSize));
	}

	return true;
}

void AddPrepareImageBasedVRSPass(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FSceneViewFamily& ViewFamily)
{
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, ScaleVariableRateShadingTexture);

	const FVRSTextures& VRSTextures = FVRSTextures::Get(GraphBuilder);
	FRDGTextureRef VariableRateShadingImage = VRSTextures.ConstructedSRI;

	FIntPoint TileSize = FVariableRateShadingImageManager::GetSRITileSize();

	FIntPoint TextureSize = VRSTextures.ScaledSRI->Desc.Extent;
	FVector2f TextureDimensions(TextureSize.X, TextureSize.Y);

	for (int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ViewIndex++)
	{
		const FSceneView* View = ViewFamily.Views[ViewIndex];
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, ViewFamily.Views.Num() > 1, "View%d", ViewIndex);
		check(View->bIsViewInfo);
		const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(View);
		if (View->bCameraCut || !FVariableRateShadingImageManager::IsVRSCompatibleWithView(*ViewInfo) || !ViewInfo->PrevViewInfo.LuminanceHistory)
		{
			break;
		}
		FIntPoint SrcBufferSize = FSceneTexturesConfig::Get().Extent;

		TShaderMapRef<FRescaleVariableRateShadingCS> RescaleVariableRateShadingCS(ViewInfo->ShaderMap);

		int32 ViewportWidth = ViewInfo->ViewRect.Width();
		int32 ViewportHeight = ViewInfo->ViewRect.Height();

		int32 ScaledTilesWide = FMath::DivideAndRoundUp(ViewportWidth, TileSize.X);
		int32 ScaledTilesHigh = FMath::DivideAndRoundUp(ViewportHeight, TileSize.Y);
		FVector2f ScaledSRIDimensions(ScaledTilesWide, ScaledTilesHigh);

		FIntRect PostProcessRect = GetPostProcessOutputRect(*ViewInfo);

		FVector2f SRIViewRectMin(
			static_cast<float>(FMath::DivideAndRoundDown(PostProcessRect.Min.X, TileSize.X)),
			static_cast<float>(FMath::DivideAndRoundDown(PostProcessRect.Min.Y, TileSize.Y)));

		FVector2f SRIViewRectMax(
			static_cast<float>(FMath::DivideAndRoundUp(PostProcessRect.Max.X, TileSize.X)),
			static_cast<float>(FMath::DivideAndRoundUp(PostProcessRect.Max.Y, TileSize.Y)));

		FVector2f UVOffset(
			(float)ViewInfo->ViewRect.Min.X / (float)SrcBufferSize.X,
			(float)ViewInfo->ViewRect.Min.Y / (float)SrcBufferSize.Y);

		float DynamicResolutionScale = (float)ViewportWidth / (float)PostProcessRect.Width();

		FRescaleVariableRateShadingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRescaleVariableRateShadingCS::FParameters>();

		FRescaleVariableRateShadingCS::InitParameters(
			*PassParameters,
			SceneTextures,
			*ViewInfo,
			VariableRateShadingImage,
			SRIViewRectMin,
			SRIViewRectMax,
			ScaledSRIDimensions,
			TextureDimensions,
			UVOffset,
			DynamicResolutionScale,
			GraphBuilder.CreateUAV(VRSTextures.ScaledSRI),
			GraphBuilder.CreateUAV(VRSTextures.ScaledConservativeSRI));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ReprojectShadingRateImage"),
			ERDGPassFlags::AsyncCompute | ERDGPassFlags::NeverCull,
			RescaleVariableRateShadingCS,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FIntPoint(ScaledTilesWide, ScaledTilesHigh), FRescaleVariableRateShadingCS::ThreadGroupSize));
	}
}


/**
 * Interface Functions
 */

FRDGTextureRef FContrastAdaptiveImageGenerator::GetImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSImageType ImageType)
{
	if (FVRSTextures::IsInitialized(GraphBuilder))
	{
		ESRITextureType::Type TextureType = ESRITextureType::GetTextureType(ImageType);
		if (TextureType != ESRITextureType::None)
		{
			const FVRSTextures& VRSTextures = FVRSTextures::Get(GraphBuilder);
			return (TextureType == ESRITextureType::ScaledSRIForRender) ? VRSTextures.ScaledSRI : VRSTextures.ScaledConservativeSRI;
		}
	}

	return nullptr;
}

void FContrastAdaptiveImageGenerator::PrepareImages(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ContrastAdaptiveShading");
	bool bAreAllViewsVRSCompatible = true;
	for (const FSceneView* View : ViewFamily.Views)
	{
		check(View->bIsViewInfo);
		const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(View);
		if (View->bCameraCut || !FVariableRateShadingImageManager::IsVRSCompatibleWithView(*ViewInfo) || !ViewInfo->PrevViewInfo.LuminanceHistory)
		{
			bAreAllViewsVRSCompatible = false;
			break;
		}
	}
	bool bPrepareImageBasedVRS = IsContrastAdaptiveShadingEnabled() && bAreAllViewsVRSCompatible;
	if (!bPrepareImageBasedVRS)
	{
		return;
	}

	FVRSTextures& VRSTextures = GraphBuilder.Blackboard.Create<FVRSTextures>();
	VRSTextures.Create(GraphBuilder, ViewFamily);

	for (int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ViewIndex++)
	{
		check(ViewFamily.Views[ViewIndex]->bIsViewInfo);
		const FViewInfo& View = *(FViewInfo*)ViewFamily.Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, ViewFamily.Views.Num() > 1, "View%d", ViewIndex);
		AddCreateShadingRateImagePass(GraphBuilder, View);
	}
	AddPrepareImageBasedVRSPass(GraphBuilder, SceneTextures, ViewFamily);
}

bool FContrastAdaptiveImageGenerator::IsEnabledForView(const FSceneView& View) const
{
	EDisplayOutputFormat DisplayOutputFormat = GetDisplayOutputFormat(View);
	bool bCompatibleWithOutputType = (DisplayOutputFormat == EDisplayOutputFormat::SDR_sRGB) || IsHDR10(DisplayOutputFormat);

	return IsContrastAdaptiveShadingEnabled() && !View.bIsSceneCapture && bCompatibleWithOutputType;
}

FRDGTextureRef FContrastAdaptiveImageGenerator::GetDebugImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSImageType ImageType)
{
	if (!CVarCASPreview.GetValueOnRenderThread() || !FVRSTextures::IsInitialized(GraphBuilder))
	{
		return nullptr;
	}

	ESRIPreviewType::Type PreviewType = static_cast<ESRIPreviewType::Type>(ImageType);
	if (CVarCASPreviewPreReprojection.GetValueOnRenderThread() && PreviewType != ESRIPreviewType::Off)
	{
		PreviewType = ESRIPreviewType::BeforeReprojection;
	}

	const FVRSTextures& VRSTextures = FVRSTextures::Get(GraphBuilder);
	switch (PreviewType)
	{
	case ESRIPreviewType::Projected:
		return VRSTextures.ScaledSRI;
		break;
	case ESRIPreviewType::ProjectedConservative:
		return VRSTextures.ScaledConservativeSRI;
		break;
	case ESRIPreviewType::BeforeReprojection:
		return VRSTextures.ConstructedSRI;
		break;
	}

	return nullptr;
}

