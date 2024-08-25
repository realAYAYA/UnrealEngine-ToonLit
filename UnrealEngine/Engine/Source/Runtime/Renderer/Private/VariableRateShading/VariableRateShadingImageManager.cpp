// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariableRateShadingImageManager.h"
#include "FoveatedImageGenerator.h"
#include "ContrastAdaptiveImageGenerator.h"
#include "StereoRenderTargetManager.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphUtils.h"
#include "RHIDefinitions.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderTargetPool.h"
#include "SceneRendering.h"
#include "SystemTextures.h"
#include "SceneRendering.h"
#include "SceneView.h"
#include "IEyeTracker.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "PostProcess/PostProcessTonemap.h"


TGlobalResource<FVariableRateShadingImageManager> GVRSImageManager;

DEFINE_LOG_CATEGORY(LogVRS);

/**
 * Basic CVars
 */

void CVarVRSPreviewCallback(IConsoleVariable* Var)
{
	const int32 RequestedPreview = Var->GetInt();
	if (RequestedPreview < 0 || RequestedPreview > 4)
	{
		UE_LOG(LogVRS, Warning, TEXT("Selected invalid preview mode, disabling preview"));
	}
}

TAutoConsoleVariable<int32> CVarVRSPreview(
	TEXT("r.VRS.Preview"),
	0,
	TEXT("Show a debug visualization of the VRS shading rate image texture. Conservative and software images are only available via Contrast Adaptive Shading.")
	TEXT("0 - off, 1 - full (hardware), 2 - conservative (hardware), 3 - full (software), 4 - conservative (software)"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSPreviewCallback),
	ECVF_RenderThreadSafe);

void CVarVRSDebugForceRateCallback(IConsoleVariable* Var)
{
	const int32 RequestedDebugForceRate = Var->GetInt();
	const int32 NumberOfAvailableRates = FVariableRateShadingImageManager::GetNumberOfSupportedRates();
	if (RequestedDebugForceRate >= NumberOfAvailableRates) 
	{
		UE_LOG(LogVRS, Warning, TEXT("Selected forced shading rate exceeds maximum available, defaulting to %s"), GRHISupportsLargerVariableRateShadingSizes ? TEXT("4x4") : TEXT("2x2"));
	}
}

int GVRSDebugForceRate = -1;
FAutoConsoleVariableRef CVarVRSDebugForceRate(
	TEXT("r.VRS.DebugForceRate"),
	GVRSDebugForceRate,
	TEXT("-1: None, 0: Force 1x1, 1: Force 1x2, 2: Force 2x1, 3: Force 2x2, 4: Force 2x4, 5: Force 4x2, 6: Force 4x4"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSDebugForceRateCallback),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarVRSSoftwareImage(
	TEXT("r.VRS.EnableSoftware"),
	0,
	TEXT("Generate 2x2 tile size software shading rate images when possible for use with nanite CS. Works even when r.VRS.Enable = 0 or Tier 2 VRS is unsupported by the hardware.")
	TEXT("0: Off, 1: On"),
	ECVF_RenderThreadSafe);


/**
 * Pass Settings
 */

static void CVarVRSImagePassTypeCallback(IConsoleVariable* Var)
{
	const int32 RequestedImageType = Var->GetInt();
	if (RequestedImageType < 0 || RequestedImageType >= FVariableRateShadingImageManager::EVRSPassType::Num)
	{
		UE_LOG(LogVRS, Warning, TEXT("Selected invalid image type, disabling VRS for pass"));
	}
}

TAutoConsoleVariable<int32> CVarVRSBasePass(
	TEXT("r.VRS.BasePass"),
	2,
	TEXT("Enables Variable Rate Shading for the base pass\n")
	TEXT("0: Disabled")
	TEXT("1: Full")
	TEXT("2: Conservative (default)"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSImagePassTypeCallback),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> CVarVRSTranslucency(
	TEXT("r.VRS.Translucency"),
	1,
	TEXT("Enable VRS with translucency rendering.\n")
	TEXT("0: Disabled")
	TEXT("1: Full (default)")
	TEXT("2: Conservative"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSImagePassTypeCallback),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> CVarVRSNaniteEmitGBuffer(
	TEXT("r.VRS.NaniteEmitGBuffer"),
	2,
	TEXT("Enable VRS with Nanite EmitGBuffer rendering.\n")
	TEXT("0: Disabled")
	TEXT("1: Full")
	TEXT("2: Conservative (default)"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSImagePassTypeCallback),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> CVarVRS_SSAO(
	TEXT("r.VRS.SSAO"),
	0,
	TEXT("Enable VRS with SSAO rendering.\n")
	TEXT("0: Disabled")
	TEXT("1: Full")
	TEXT("2: Conservative (default)"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSImagePassTypeCallback),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> CVarVRS_SSR(
	TEXT("r.VRS.SSR"),
	2,
	TEXT("Enable VRS with SSR (PS) rendering.\n")
	TEXT("0: Disabled")
	TEXT("1: Full")
	TEXT("2: Conservative (default)"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSImagePassTypeCallback),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> CVarVRSReflectionEnvironmentSky(
	TEXT("r.VRS.ReflectionEnvironmentSky"),
	2,
	TEXT("Enable VRS with ReflectionEnvironmentAndSky (PS) rendering.\n")
	TEXT("0: Disabled")
	TEXT("1: Full")
	TEXT("2: Conservative (default)"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSImagePassTypeCallback),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> CVarVRSLightFunctions(
	TEXT("r.VRS.LightFunctions"),
	1,
	TEXT("Enables Variable Rate Shading for light functions\n")
	TEXT("0: Disabled")
	TEXT("1: Full (default)")
	TEXT("2: Conservative"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSImagePassTypeCallback),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> CVarVRSDecals(
	TEXT("r.VRS.Decals"),
	2,
	TEXT("Enables Variable Rate Shading for decals\n")
	TEXT("0: Disabled")
	TEXT("1: Full")
	TEXT("2: Conservative (default)"),
	FConsoleVariableDelegate::CreateStatic(&CVarVRSImagePassTypeCallback),
	ECVF_RenderThreadSafe);

/**
 * Shaders
 */

namespace VRSHelpers
{
	constexpr int32 kCombineGroupSize = FComputeShaderUtils::kGolden2DGroupSize;
	constexpr uint32 kShadingRateDimensionBits = 2;
}

class FCombineShadingRateTexturesCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCombineShadingRateTexturesCS);
	SHADER_USE_PARAMETER_STRUCT(FCombineShadingRateTexturesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWOutputTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, SourceTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, SourceTexture1)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVariableRateShading(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_XY"), VRSHelpers::kCombineGroupSize);
		OutEnvironment.SetDefine(TEXT("SHADING_RATE_DIMENSION_BITS"), VRSHelpers::kShadingRateDimensionBits);
	}

};

IMPLEMENT_GLOBAL_SHADER(FCombineShadingRateTexturesCS, "/Engine/Private/VariableRateShading/VRSShadingRateCombine.usf", "CombineShadingRateTextures", SF_Compute);\

class FDebugVariableRateShadingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDebugVariableRateShadingCS);
	SHADER_USE_PARAMETER_STRUCT(FDebugVariableRateShadingCS, FGlobalShader);

	static constexpr uint32 ThreadGroupSize = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, VariableRateShadingTextureIn)
		SHADER_PARAMETER(FVector4f, ViewRect)
		SHADER_PARAMETER(float, DynamicResolutionScale)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, SceneColorOut)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 1);
	}

	static void InitParameters(
		FParameters& Parameters,
		FRDGTextureRef VariableRateShadingTexture,
		const FIntRect& ViewRect,
		float DynamicResolutionScale,
		FRDGTextureUAVRef SceneColorUAV)
	{
		Parameters.VariableRateShadingTextureIn = VariableRateShadingTexture;
		Parameters.ViewRect = FVector4f(ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X, ViewRect.Max.Y);
		Parameters.DynamicResolutionScale = DynamicResolutionScale;
		Parameters.SceneColorOut = SceneColorUAV;
	}
};

IMPLEMENT_GLOBAL_SHADER(FDebugVariableRateShadingCS, "/Engine/Private/VariableRateShading/VRSShadingRatePreview.usf", "PreviewVariableRateShadingTextureCS", SF_Compute);

//---------------------------------------------------------------------------------------------
using FDebugVariableRateShadingVS = FScreenPassVS;

//---------------------------------------------------------------------------------------------
class FDebugVariableRateShadingPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDebugVariableRateShadingPS);
	SHADER_USE_PARAMETER_STRUCT(FDebugVariableRateShadingPS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 0);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, VariableRateShadingTextureIn)
		RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		static void InitParameters(
			FParameters& Parameters,
			FRDGTextureRef VariableRateShadingTexture,
			FRDGTextureRef OutputSceneColor)
	{
		Parameters.VariableRateShadingTextureIn = VariableRateShadingTexture;
		Parameters.RenderTargets[0] = FRenderTargetBinding(OutputSceneColor, ERenderTargetLoadAction::ELoad);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDebugVariableRateShadingPS, "/Engine/Private/VariableRateShading/VRSShadingRatePreview.usf", "PreviewVariableRateShadingTexturePS", SF_Pixel);


/**
 * Public functions
 */

FVariableRateShadingImageManager::FVariableRateShadingImageManager()
	: FRenderResource()
{
	InternalGenerators.Add(MakeUnique<FFoveatedImageGenerator>());
	InternalGenerators.Add(MakeUnique<FContrastAdaptiveImageGenerator>());

	FWriteScopeLock GeneratorsLock(GeneratorsMutex);
	for (const TUniquePtr<IVariableRateShadingImageGenerator>& Generator : InternalGenerators)
	{
		ImageGenerators.Add(Generator.Get());
	}
}

FVariableRateShadingImageManager::~FVariableRateShadingImageManager() {}

void FVariableRateShadingImageManager::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (IsHardwareVRSSupported())
	{
		UE_LOG(LogVRS, Log, TEXT("Current RHI supports Variable Rate Shading"));
	}
	else
	{
		UE_LOG(LogVRS, Log, TEXT("Current RHI does not support Variable Rate Shading"));
	}
}

void FVariableRateShadingImageManager::ReleaseRHI()
{
	GRenderTargetPool.FreeUnusedResources();
}

bool FVariableRateShadingImageManager::IsHardwareVRSSupported()
{
	return GRHISupportsAttachmentVariableRateShading && FDataDrivenShaderPlatformInfo::GetSupportsVariableRateShading(GMaxRHIShaderPlatform);
}

bool FVariableRateShadingImageManager::IsSoftwareVRSSupported()
{
	return IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM6);
}

bool FVariableRateShadingImageManager::IsHardwareVRSEnabled()
{
	// Currently corresponds to r.VRS.Enable and r.VRS.EnableImage
	return GRHIVariableRateShadingEnabled && GRHIAttachmentVariableRateShadingEnabled;
}

bool FVariableRateShadingImageManager::IsSoftwareVRSEnabled()
{
	return CVarVRSSoftwareImage.GetValueOnRenderThread() > 0;
}

bool FVariableRateShadingImageManager::IsVRSEnabledForFrame()
{
	return bHardwareVRSEnabledForFrame || bSoftwareVRSEnabledForFrame;
}

bool FVariableRateShadingImageManager::IsHardwareVRSEnabledForFrame()
{
	return bHardwareVRSEnabledForFrame;
}

bool FVariableRateShadingImageManager::IsSoftwareVRSEnabledForFrame()
{
	return bSoftwareVRSEnabledForFrame;
}

static EDisplayOutputFormat GetDisplayOutputFormat(const FViewInfo& View)
{
	FTonemapperOutputDeviceParameters Parameters = GetTonemapperOutputDeviceParameters(*View.Family);
	return (EDisplayOutputFormat)Parameters.OutputDevice;
}

bool FVariableRateShadingImageManager::IsVRSCompatibleWithOutputType(const EDisplayOutputFormat& OutputFormat)
{
	// VRS texture generation is currently only compatible with SDR and HDR10
	return OutputFormat == EDisplayOutputFormat::SDR_sRGB
		|| OutputFormat == EDisplayOutputFormat::HDR_ACES_1000nit_ST2084
		|| OutputFormat == EDisplayOutputFormat::HDR_ACES_2000nit_ST2084;
}

bool FVariableRateShadingImageManager::IsVRSCompatibleWithView(const FViewInfo& ViewInfo)
{
	// TODO: Investigate if it's worthwhile getting scene captures working. Things that we'll need to take care of
	// is to associate shading rate texture image with main scene, and scene capture.  But what if there is
	// more than 1 scene capture?  Is there a unique identifier that connects two frames of scene capture.
	return !ViewInfo.bIsSceneCapture
		&& ViewInfo.Family->bRealtimeUpdate
		&& IsVRSCompatibleWithOutputType(GetDisplayOutputFormat(ViewInfo));
}

FIntPoint FVariableRateShadingImageManager::GetSRITileSize(bool bSoftwareVRS)
{
	return bSoftwareVRS ? FIntPoint(2, 2) : FIntPoint(GRHIVariableRateShadingImageTileMinWidth, GRHIVariableRateShadingImageTileMinHeight);
}

FRDGTextureDesc FVariableRateShadingImageManager::GetSRIDesc(const FSceneViewFamily& ViewFamily, bool bSoftwareVRS)
{
	check(!ViewFamily.Views.IsEmpty())
	check(ViewFamily.Views[0]->bIsViewInfo);

	const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(ViewFamily.Views[0]);
	const FIntRect FamilyViewRect = ViewInfo->GetFamilyViewRect(); // May vary from the size of the scene textures if using constrained aspect ratios
	const FIntPoint SRISize = FMath::DivideAndRoundUp(FamilyViewRect.Size(), GetSRITileSize(bSoftwareVRS));

	return FRDGTextureDesc::Create2D(
		SRISize,
		bSoftwareVRS ? PF_R8_UINT : GRHIVariableRateShadingImageFormat,
		FClearValueBinding::None,
		TexCreate_Foveation | TexCreate_UAV | TexCreate_ShaderResource | TexCreate_DisableDCC);
}

int32 FVariableRateShadingImageManager::GetNumberOfSupportedRates()
{
	// We will always support the 4 rates 1x1, 1x2, 2x1, and 2x2.
	// If the RHI supports larger rates, we can also use 2x4, 4x2, and 4x4, for 7 total.
	static const int32 NumBaseRates = 4;
	static const int32 NumExpandedRates = 7;

	return GRHISupportsLargerVariableRateShadingSizes ? NumExpandedRates : NumBaseRates;
}

FRDGTextureRef FVariableRateShadingImageManager::GetVariableRateShadingImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSPassType PassType, bool bRequestSoftwareImage)
{
	// If the view doesn't support VRS or this pass is disabled, bail immediately
	if (!IsVRSEnabledForFrame())
	{
		return nullptr;
	}

	EVRSImageType ImageType = GetImageTypeFromPassType(PassType);
	if (ImageType == EVRSImageType::Disabled || !IsVRSCompatibleWithView(ViewInfo))
	{
		return nullptr;
	}

	// Use debug rate if provided, otherwise bail if no generators available
	if (VRSForceRateForFrame >= 0)
	{
		return GetForceRateImage(GraphBuilder, *ViewInfo.Family, VRSForceRateForFrame, GetImageTypeFromPassType(PassType), bRequestSoftwareImage);
	}

	if (ActiveGenerators.IsEmpty())
	{
		return nullptr;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "GetVariableRateShadingImage");
	SCOPED_NAMED_EVENT(GetVariableRateShadingImage, FColor::Yellow);

	// Collate all internal sources
	TArray<FRDGTextureRef> InternalVRSSources;
	for (IVariableRateShadingImageGenerator* const Generator : ActiveGenerators)
	{
		const bool bGetSoftwareImage = bSoftwareVRSEnabledForFrame && bRequestSoftwareImage;

		FRDGTextureRef Image = nullptr;
		if (Generator && Generator->IsSupportedByView(ViewInfo))
		{
			Image = Generator->GetImage(GraphBuilder, ViewInfo, ImageType, bGetSoftwareImage);
		}

		if (Image)
		{
			InternalVRSSources.Add(Image);
		}
	}

	// If we have more than one internal source, combine the first available two
	// If we have exactly one, the combiner will just return that
	if (InternalVRSSources.Num())
	{
		return CombineShadingRateImages(GraphBuilder, *ViewInfo.Family, InternalVRSSources);
	}

	// Default to nullptr if no sources are available
	else
	{
		return nullptr;
	}
}

void FVariableRateShadingImageManager::PrepareImageBasedVRS(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures)
{
	bHardwareVRSEnabledForFrame = IsHardwareVRSSupported() && IsHardwareVRSEnabled();
	bSoftwareVRSEnabledForFrame = IsSoftwareVRSSupported() && CVarVRSSoftwareImage.GetValueOnRenderThread() != 0;
	if (!IsVRSEnabledForFrame())
	{
		return;
	}

	// If no generators or forced rate are active, bail
	{
		FReadScopeLock GeneratorsLock(GeneratorsMutex);
		ActiveGenerators = ImageGenerators.FilterByPredicate([](IVariableRateShadingImageGenerator* const InGenerator) { return InGenerator && InGenerator->IsEnabled(); });
	}

	VRSForceRateForFrame = CVarVRSDebugForceRate->GetInt();

	if (ActiveGenerators.IsEmpty() && VRSForceRateForFrame < 0)
	{
		return;
	}
	
	// If no views support VRS, bail
	bool bIsAnyViewVRSCompatible = false;
	for (const FSceneView* View : ViewFamily.Views)
	{
		check(View->bIsViewInfo);
		auto ViewInfo = static_cast<const FViewInfo*>(View);
		if (IsVRSCompatibleWithView(*ViewInfo))
		{
			bIsAnyViewVRSCompatible = true;
			break;
		}
	}

	if (!bIsAnyViewVRSCompatible)
	{
		return;
	}

	// Also bail if we're given a ViewFamily with no valid RenderTarget
	if (ViewFamily.RenderTarget == nullptr)
	{
		ensureMsgf(0, TEXT("VRS Image Manager does not support ViewFamilies with no valid RenderTarget"));
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "PrepareImageBasedVRS");
	SCOPED_NAMED_EVENT(PrepareImageBasedVRS, FColor::Red);

	VRSForceRateForFrame = CVarVRSDebugForceRate->GetInt();

	// Invoke active image generators
	for (IVariableRateShadingImageGenerator* const Generator : ActiveGenerators)
	{
		if (Generator && Generator->IsSupportedByView(*ViewFamily.Views[0]))
		{
			Generator->PrepareImages(GraphBuilder, ViewFamily, SceneTextures, bHardwareVRSEnabledForFrame, bSoftwareVRSEnabledForFrame);
		}
	}
}

bool FVariableRateShadingImageManager::IsTypeEnabledForView(const FSceneView& View, FVariableRateShadingImageManager::EVRSSourceType Type)
{
	for (IVariableRateShadingImageGenerator* const Generator : ActiveGenerators)
	{
		if (Generator && EnumHasAnyFlags(Type, Generator->GetType()) && Generator->IsSupportedByView(View))
		{
			return true;
		}
	}
	return false;
}

void FVariableRateShadingImageManager::DrawDebugPreview(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, FRDGTextureRef OutputSceneColor)
{
	if (!IsVRSEnabledForFrame() || !OutputSceneColor)
	{
		return;
	}

	uint32 ImageTypeAsInt = CVarVRSPreview.GetValueOnRenderThread();
	EVRSImageType PreviewImageType = EVRSImageType::Disabled;
	bool bUseSoftwareImage = false;
	
	switch (ImageTypeAsInt)
	{
		// Full hardware
		case 1: 
			PreviewImageType = EVRSImageType::Full;
			break;

		// Conservative hardware
		case 2:
			PreviewImageType = EVRSImageType::Conservative;
			break;

		// Full software
		case 3:
			PreviewImageType = EVRSImageType::Full;
			bUseSoftwareImage = true;
			break;

		// Conservative software
		case 4:
			PreviewImageType = EVRSImageType::Conservative;
			bUseSoftwareImage = true;
			break;

		default:
			return;
	}

	if ((bUseSoftwareImage && !IsSoftwareVRSEnabledForFrame()) || (!bUseSoftwareImage && !IsHardwareVRSEnabledForFrame()))
	{
		return;
	}

	for (const FSceneView* View : ViewFamily.Views)
	{
		check(View->bIsViewInfo);
		auto ViewInfo = static_cast<const FViewInfo*>(View);
		if (IsVRSCompatibleWithView(*ViewInfo))
		{
			FRDGTextureRef PreviewTexture;
			
			// Use debug rate if provided
			if (VRSForceRateForFrame >= 0)
			{
				PreviewTexture = GetForceRateImage(GraphBuilder, ViewFamily, VRSForceRateForFrame, PreviewImageType, bUseSoftwareImage);
			}

			// Otherwise collate debug images
			else
			{
				TArray<FRDGTextureRef> InternalVRSSources;

				for (IVariableRateShadingImageGenerator* const Generator : ActiveGenerators)
				{
					FRDGTextureRef Image = nullptr;
					if (Generator && Generator->IsSupportedByView(*View))
					{
						Image = Generator->GetDebugImage(GraphBuilder, *ViewInfo, PreviewImageType, bUseSoftwareImage);
					}

					if (Image)
					{
						InternalVRSSources.Add(Image);
					}
				}

				PreviewTexture = CombineShadingRateImages(GraphBuilder, ViewFamily, InternalVRSSources);

				// Generate a dummy 1x1 image if we have no VRS sources
				if (!PreviewTexture)
				{
					PreviewTexture = GetForceRateImage(GraphBuilder, ViewFamily, VRSSR_1x1, EVRSImageType::Full, bUseSoftwareImage);
				}
			}

			// If we have an active debug image, render it as a preview overlay
			auto& RHICmdList = GraphBuilder.RHICmdList;

			SCOPED_DRAW_EVENT(RHICmdList, VRSDebugPreview);

			FIntRect SrcViewRect = ViewInfo->ViewRect;

			const FIntRect& DestViewRect = ViewInfo->UnscaledViewRect;

			TShaderMapRef<FDebugVariableRateShadingVS> VertexShader(ViewInfo->ShaderMap);
			TShaderMapRef<FDebugVariableRateShadingPS> PixelShader(ViewInfo->ShaderMap);

			auto* PassParameters = GraphBuilder.AllocParameters<FDebugVariableRateShadingPS::FParameters>();

			FDebugVariableRateShadingPS::InitParameters(
				*PassParameters,
				PreviewTexture,
				OutputSceneColor);

			FRHIBlendState* BlendState =
				TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSource1Alpha>::GetRHI();
			FRHIDepthStencilState* DepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();

			EScreenPassDrawFlags DrawFlags = EScreenPassDrawFlags::AllowHMDHiddenAreaMask;

			FIntRect ScaledSrcRect = FIntRect::DivideAndRoundUp(SrcViewRect, FVariableRateShadingImageManager::GetSRITileSize(bUseSoftwareImage));

			const FScreenPassTextureViewport InputViewport = FScreenPassTextureViewport(PreviewTexture, ScaledSrcRect);
			const FScreenPassTextureViewport OutputViewport(OutputSceneColor, DestViewRect);

			AddDrawScreenPass(
				GraphBuilder,
				RDG_EVENT_NAME("Display VRS Debug Preview"),
				*ViewInfo,
				OutputViewport,
				InputViewport,
				VertexShader,
				PixelShader,
				BlendState,
				DepthStencilState,
				PassParameters,
				DrawFlags);
		}
	}

}

void FVariableRateShadingImageManager::RegisterExternalImageGenerator(IVariableRateShadingImageGenerator* ExternalGenerator)
{
	if (ExternalGenerator == nullptr)
	{
		UE_LOG(LogVRS, Warning, TEXT("Trying to register a null VRS generator. Generator will be ignored."));
		return;
	}
	FWriteScopeLock GeneratorsLock(GeneratorsMutex);
	ImageGenerators.Add(ExternalGenerator);
}

void FVariableRateShadingImageManager::UnregisterExternalImageGenerator(IVariableRateShadingImageGenerator* ExternalGenerator)
{
	if (ExternalGenerator == nullptr)
	{
		UE_LOG(LogVRS, Warning, TEXT("Trying to unregister a null VRS generator. Generator will be ignored."));
		return;
	}
	FWriteScopeLock GeneratorsLock(GeneratorsMutex);
	ImageGenerators.Remove(ExternalGenerator);
}

/**
 * Private functions
 */

FRDGTextureRef FVariableRateShadingImageManager::CombineShadingRateImages(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, TArray<FRDGTextureRef> Sources)
{
	// If we have more than one source, combine the first available two
	// TODO: Support combining more textures
	if (Sources.Num() < 1)
	{
		return nullptr;
	}
	else if (Sources.Num() == 1)
	{
		return Sources[0];
	}
	else
	{
		SCOPED_NAMED_EVENT(CombineShadingRateImages, FColor::Green);

		// Create texture to hold shading rate image
		FRDGTextureRef CombinedShadingRateTexture = GraphBuilder.CreateTexture(Sources[0]->Desc, TEXT("CombinedShadingRateTexture"));

		FCombineShadingRateTexturesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCombineShadingRateTexturesCS::FParameters>();
		PassParameters->SourceTexture0 = Sources[0];
		PassParameters->SourceTexture1 = Sources[1];
		PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(CombinedShadingRateTexture);

		TShaderMapRef<FCombineShadingRateTexturesCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CombineShadingRateImages"),
			ERDGPassFlags::AsyncCompute | ERDGPassFlags::NeverCull,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FSceneTexturesConfig::Get().Extent, FIntPoint(VRSHelpers::kCombineGroupSize, VRSHelpers::kCombineGroupSize)));

		return CombinedShadingRateTexture;
	}

}

FRDGTextureRef FVariableRateShadingImageManager::GetForceRateImage(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, int RateIndex /* = 0*/, EVRSImageType ImageType /* = EVRSImageType::Full*/, bool bGetSoftwareImage)
{
	static const TArray<uint32> ValidShadingRates = { VRSSR_1x1, VRSSR_1x2, VRSSR_2x1, VRSSR_2x2, VRSSR_2x4, VRSSR_4x2, VRSSR_4x4 };

	const int32 NumberOfAvailableRates = GetNumberOfSupportedRates();

	if (RateIndex >= NumberOfAvailableRates)
	{
		RateIndex = NumberOfAvailableRates - 1; // Default to maximum shading rate if value exceeds valid rates
	}

	if (ImageType == EVRSImageType::Disabled)
	{
		RateIndex = 0; // Force to minimum shading rate if VRS is disabled for this pass
	}

	FRDGTextureRef ForceShadingRateTexture = GraphBuilder.CreateTexture(GetSRIDesc(ViewFamily, bGetSoftwareImage), TEXT("ForceShadingRateTexture"));
	FRDGTextureUAVRef ForceShadingRateUAV = GraphBuilder.CreateUAV(ForceShadingRateTexture);
	AddClearUAVPass(GraphBuilder, ForceShadingRateUAV, ValidShadingRates[RateIndex]);

	return ForceShadingRateTexture;
}

FVariableRateShadingImageManager::EVRSImageType FVariableRateShadingImageManager::GetImageTypeFromPassType(EVRSPassType PassType)
{
	static struct FStaticPassToImageCVarData
	{
		TAutoConsoleVariable<int32>* CVarByPassType[EVRSPassType::Num] = {};
		FStaticPassToImageCVarData()
		{
			CVarByPassType[EVRSPassType::BasePass] = &CVarVRSBasePass;
			CVarByPassType[EVRSPassType::TranslucencyAll] = &CVarVRSTranslucency;
			CVarByPassType[EVRSPassType::NaniteEmitGBufferPass] = &CVarVRSNaniteEmitGBuffer;
			CVarByPassType[EVRSPassType::SSAO] = &CVarVRS_SSAO;
			CVarByPassType[EVRSPassType::SSR] = &CVarVRS_SSR;
			CVarByPassType[EVRSPassType::ReflectionEnvironmentAndSky] = &CVarVRSReflectionEnvironmentSky;
			CVarByPassType[EVRSPassType::LightFunctions] = &CVarVRSLightFunctions;
			CVarByPassType[EVRSPassType::Decals] = &CVarVRSDecals;
		}
	} StaticData;

	uint32 ImageTypeAsInt = StaticData.CVarByPassType[PassType]->GetValueOnRenderThread();
	if (ImageTypeAsInt >= 0 && ImageTypeAsInt <= EVRSImageType::Conservative)
	{
		return static_cast<EVRSImageType>(ImageTypeAsInt);
	}
	else
	{
		return EVRSImageType::Disabled;
	}
}