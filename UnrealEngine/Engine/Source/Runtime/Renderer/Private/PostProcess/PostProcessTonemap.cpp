// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTonemap.cpp: Post processing tone mapping implementation.
=============================================================================*/

#include "PostProcess/PostProcessTonemap.h"
#include "EngineGlobals.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessCombineLUTs.h"
#include "PostProcess/PostProcessMobile.h"
#include "PostProcess/PostProcessing.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
#include "Rendering/Texture2DResource.h"
#include "Math/Halton.h"
#include "SystemTextures.h"
#include "HDRHelper.h"

bool SupportsFilmGrain(EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
}

namespace
{
TAutoConsoleVariable<float> CVarTonemapperSharpen(
	TEXT("r.Tonemapper.Sharpen"),
	0,
	TEXT("Sharpening in the tonemapper (not for mobile), actual implementation is work in progress, clamped at 10\n")
	TEXT("   0: off(default)\n")
	TEXT(" 0.5: half strength\n")
	TEXT("   1: full strength"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTonemapperGamma(
	TEXT("r.TonemapperGamma"),
	0.0f,
	TEXT("0: Default behavior\n")
	TEXT("#: Use fixed gamma # instead of sRGB or Rec709 transform"),
	ECVF_Scalability | ECVF_RenderThreadSafe);	

TAutoConsoleVariable<float> CVarGamma(
	TEXT("r.Gamma"),
	1.0f,
	TEXT("Gamma on output"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarFilmGrainSequenceLength(
	TEXT("r.FilmGrain.SequenceLength"), 97,
	TEXT("Length of the random sequence for film grain (preferably a prime number, default=97)."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarFilmGrainCacheTextureConstants(
	TEXT("r.FilmGrain.CacheTextureConstants"), 1,
	TEXT("Wether the constants related to the film grain should be cached."),
	ECVF_RenderThreadSafe);

const int32 GTonemapComputeTileSizeX = 8;
const int32 GTonemapComputeTileSizeY = 8;

namespace TonemapperPermutation
{
// Shared permutation dimensions between deferred and mobile renderer.
class FTonemapperBloomDim          : SHADER_PERMUTATION_BOOL("USE_BLOOM");
class FTonemapperGammaOnlyDim      : SHADER_PERMUTATION_BOOL("USE_GAMMA_ONLY");
class FTonemapperLocalExposureDim : SHADER_PERMUTATION_BOOL("USE_LOCAL_EXPOSURE");
class FTonemapperVignetteDim       : SHADER_PERMUTATION_BOOL("USE_VIGNETTE");
class FTonemapperSharpenDim        : SHADER_PERMUTATION_BOOL("USE_SHARPEN");
class FTonemapperFilmGrainDim      : SHADER_PERMUTATION_BOOL("USE_FILM_GRAIN");
class FTonemapperMsaaDim           : SHADER_PERMUTATION_BOOL("METAL_MSAA_HDR_DECODE");
class FTonemapperEyeAdaptationDim  : SHADER_PERMUTATION_BOOL("EYEADAPTATION_EXPOSURE_FIX");

using FCommonDomain = TShaderPermutationDomain<
	FTonemapperBloomDim,
	FTonemapperGammaOnlyDim,
	FTonemapperLocalExposureDim,
	FTonemapperVignetteDim,
	FTonemapperSharpenDim,
	FTonemapperFilmGrainDim,
	FTonemapperMsaaDim>;

bool ShouldCompileCommonPermutation(const FGlobalShaderPermutationParameters& Parameters, const FCommonDomain& PermutationVector)
{
	// MSAA pre-resolve step only used on iOS atm
	if (PermutationVector.Get<FTonemapperMsaaDim>() && !IsMetalMobilePlatform(Parameters.Platform))
	{
		return false;
	}

	// If GammaOnly, don't compile any other dimmension == true.
	if (PermutationVector.Get<FTonemapperGammaOnlyDim>())
	{
		return !PermutationVector.Get<FTonemapperBloomDim>() &&
			!PermutationVector.Get<FTonemapperLocalExposureDim>() &&
			!PermutationVector.Get<FTonemapperVignetteDim>() &&
			!PermutationVector.Get<FTonemapperSharpenDim>() &&
			!PermutationVector.Get<FTonemapperFilmGrainDim>() &&
			!PermutationVector.Get<FTonemapperMsaaDim>();
	}
	return true;
}

// Common conversion of engine settings into.
FCommonDomain BuildCommonPermutationDomain(const FViewInfo& View, bool bGammaOnly, bool bLocalExposure, bool bMetalMSAAHDRDecode)
{
	const FSceneViewFamily* Family = View.Family;

	FCommonDomain PermutationVector;

	// Gamma
	if (bGammaOnly ||
		(Family->EngineShowFlags.Tonemapper == 0) ||
		(Family->EngineShowFlags.PostProcessing == 0))
	{
		PermutationVector.Set<FTonemapperGammaOnlyDim>(true);
		return PermutationVector;
	}

	const FPostProcessSettings& Settings = View.FinalPostProcessSettings;
	PermutationVector.Set<FTonemapperVignetteDim>(Settings.VignetteIntensity > 0.0f);
	PermutationVector.Set<FTonemapperBloomDim>(Settings.BloomIntensity > 0.0);
	PermutationVector.Set<FTonemapperLocalExposureDim>(bLocalExposure);
	PermutationVector.Set<FTonemapperFilmGrainDim>(View.FilmGrainTexture != nullptr);
	PermutationVector.Set<FTonemapperSharpenDim>(CVarTonemapperSharpen.GetValueOnRenderThread() > 0.0f);	
	PermutationVector.Set<FTonemapperMsaaDim>(bMetalMSAAHDRDecode);
	return PermutationVector;
}

// Desktop renderer permutation dimensions.
class FTonemapperColorFringeDim       : SHADER_PERMUTATION_BOOL("USE_COLOR_FRINGE");
class FTonemapperGrainQuantizationDim : SHADER_PERMUTATION_BOOL("USE_GRAIN_QUANTIZATION");
class FTonemapperOutputDeviceDim      : SHADER_PERMUTATION_ENUM_CLASS("DIM_OUTPUT_DEVICE", EDisplayOutputFormat);

using FDesktopDomain = TShaderPermutationDomain<
	FCommonDomain,
	FTonemapperColorFringeDim,
	FTonemapperGrainQuantizationDim,
	FTonemapperOutputDeviceDim>;

FDesktopDomain RemapPermutation(FDesktopDomain PermutationVector, ERHIFeatureLevel::Type FeatureLevel)
{
	FCommonDomain CommonPermutationVector = PermutationVector.Get<FCommonDomain>();

	// No remapping if gamma only.
	if (CommonPermutationVector.Get<FTonemapperGammaOnlyDim>())
	{
		return PermutationVector;
	}

	// Grain jitter or intensity looks bad anyway.
	bool bFallbackToSlowest = false;
	bFallbackToSlowest = bFallbackToSlowest || CommonPermutationVector.Get<FTonemapperFilmGrainDim>();

	if (bFallbackToSlowest)
	{
		CommonPermutationVector.Set<FTonemapperFilmGrainDim>(true);
		CommonPermutationVector.Set<FTonemapperSharpenDim>(true);

		PermutationVector.Set<FTonemapperColorFringeDim>(true);
	}

	// You most likely need Bloom anyway.
	CommonPermutationVector.Set<FTonemapperBloomDim>(true);

	// Mobile supports only sRGB and LinearNoToneCurve output
	if (FeatureLevel <= ERHIFeatureLevel::ES3_1 &&
		PermutationVector.Get<FTonemapperOutputDeviceDim>() != EDisplayOutputFormat::HDR_LinearNoToneCurve)
	{
		PermutationVector.Set<FTonemapperOutputDeviceDim>(EDisplayOutputFormat::SDR_sRGB);
	}

	// Disable grain quantization for LinearNoToneCurve and LinearWithToneCurve output device
	if (PermutationVector.Get<FTonemapperOutputDeviceDim>() == EDisplayOutputFormat::HDR_LinearNoToneCurve || PermutationVector.Get<FTonemapperOutputDeviceDim>() == EDisplayOutputFormat::HDR_LinearWithToneCurve)
		PermutationVector.Set<FTonemapperGrainQuantizationDim>(false);
	else
		PermutationVector.Set<FTonemapperGrainQuantizationDim>(true);

	if (FeatureLevel < ERHIFeatureLevel::SM5)
	{
		CommonPermutationVector.Set<FTonemapperFilmGrainDim>(false);
	}

	PermutationVector.Set<FCommonDomain>(CommonPermutationVector);

	return PermutationVector;
}

bool ShouldCompileDesktopPermutation(const FGlobalShaderPermutationParameters& Parameters, FDesktopDomain PermutationVector)
{
	auto CommonPermutationVector = PermutationVector.Get<FCommonDomain>();

	if (RemapPermutation(PermutationVector, GetMaxSupportedFeatureLevel(Parameters.Platform)) != PermutationVector)
	{
		return false;
	}

	if (!ShouldCompileCommonPermutation(Parameters, CommonPermutationVector))
	{
		return false;
	}

	if (CommonPermutationVector.Get<FTonemapperGammaOnlyDim>())
	{
		return !PermutationVector.Get<FTonemapperColorFringeDim>() &&
			!PermutationVector.Get<FTonemapperGrainQuantizationDim>();
	}
	return true;
}

} // namespace TonemapperPermutation
} // namespace

FTonemapperOutputDeviceParameters GetTonemapperOutputDeviceParameters(const FSceneViewFamily& Family)
{
	static TConsoleVariableData<float>* CVarOutputGamma = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.TonemapperGamma"));

	EDisplayOutputFormat OutputDeviceValue;

	if (Family.SceneCaptureSource == SCS_FinalColorHDR)
	{
		OutputDeviceValue = EDisplayOutputFormat::HDR_LinearNoToneCurve;
	}
	else if (Family.SceneCaptureSource == SCS_FinalToneCurveHDR)
	{
		OutputDeviceValue = EDisplayOutputFormat::HDR_LinearWithToneCurve;
	}
	else if (Family.bIsHDR)
	{
		OutputDeviceValue = EDisplayOutputFormat::HDR_ACES_1000nit_ST2084;
	}
	else
	{
		OutputDeviceValue = Family.RenderTarget->GetDisplayOutputFormat();
	}

	float Gamma = CVarOutputGamma->GetValueOnRenderThread();

    // In case gamma is unspecified, fall back to 2.2 which is the most common case
	if ((PLATFORM_APPLE || OutputDeviceValue == EDisplayOutputFormat::SDR_ExplicitGammaMapping) && Gamma == 0.0f)
	{
		Gamma = 2.2f;
	}

	// Enforce user-controlled ramp over sRGB or Rec709
	if (Gamma > 0.0f && (OutputDeviceValue == EDisplayOutputFormat::SDR_sRGB || OutputDeviceValue == EDisplayOutputFormat::SDR_Rec709))
	{
		OutputDeviceValue = EDisplayOutputFormat::SDR_ExplicitGammaMapping;
	}

	FVector3f InvDisplayGammaValue;
	InvDisplayGammaValue.X = 1.0f / Family.RenderTarget->GetDisplayGamma();
	InvDisplayGammaValue.Y = 2.2f / Family.RenderTarget->GetDisplayGamma();
	InvDisplayGammaValue.Z = 1.0f / FMath::Max(Gamma, 1.0f);

	FTonemapperOutputDeviceParameters Parameters;
	Parameters.InverseGamma = InvDisplayGammaValue;
	Parameters.OutputDevice = static_cast<uint32>(OutputDeviceValue);
	Parameters.OutputGamut = static_cast<uint32>(Family.RenderTarget->GetDisplayColorGamut());
	Parameters.OutputMaxLuminance = HDRGetDisplayMaximumLuminance();
	return Parameters;
}

BEGIN_SHADER_PARAMETER_STRUCT(FFilmGrainParameters, )
	SHADER_PARAMETER(FVector3f, GrainRandomFull)
	SHADER_PARAMETER(float, FilmGrainIntensityShadows)
	SHADER_PARAMETER(float, FilmGrainIntensityMidtones)
	SHADER_PARAMETER(float, FilmGrainIntensityHighlights)
	SHADER_PARAMETER(float, FilmGrainShadowsMax)
	SHADER_PARAMETER(float, FilmGrainHighlightsMin)
	SHADER_PARAMETER(float, FilmGrainHighlightsMax)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FilmGrainTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, FilmGrainSampler)
	SHADER_PARAMETER(FScreenTransform, ScreenPosToFilmGrainTextureUV)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, FilmGrainTextureConstants)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTonemapParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FFilmGrainParameters, FilmGrain)
	SHADER_PARAMETER_STRUCT_INCLUDE(FTonemapperOutputDeviceParameters, OutputDevice)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Color)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
	SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorTexture)

	// Parameters to apply to the scene color.
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, SceneColorApplyParamaters)

	// Bloom texture
	SHADER_PARAMETER(FScreenTransform, ColorToBloom)
	SHADER_PARAMETER(FVector2f, BloomUVViewportBilinearMin)
	SHADER_PARAMETER(FVector2f, BloomUVViewportBilinearMax)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BloomTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, BloomSampler)

	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, LumBilateralGrid)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredLogLum)
	SHADER_PARAMETER_SAMPLER(SamplerState, LumBilateralGridSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, BlurredLogLumSampler)

	// SM5 and above use Texture2D for EyeAdaptationTexture
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EyeAdaptationTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorGradingLUT)
	SHADER_PARAMETER_TEXTURE(Texture2D, BloomDirtMaskTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, ColorSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, ColorGradingLUTSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, BloomDirtMaskSampler)
	SHADER_PARAMETER(FVector4f, ColorScale0)
	SHADER_PARAMETER(FVector4f, BloomDirtMaskTint)
	SHADER_PARAMETER(FVector4f, ChromaticAberrationParams)
	SHADER_PARAMETER(FVector4f, TonemapperParams)
	SHADER_PARAMETER(FVector4f, LensPrincipalPointOffsetScale)
	SHADER_PARAMETER(FVector4f, LensPrincipalPointOffsetScaleInverse)
	SHADER_PARAMETER(float, DefaultEyeExposure)
	SHADER_PARAMETER(float, EditorNITLevel)
	SHADER_PARAMETER(uint32, bOutputInHDR)
	// ES3_1 uses EyeAdaptationBuffer
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, EyeAdaptationBuffer)
END_SHADER_PARAMETER_STRUCT()

class FFilmGrainReduceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFilmGrainReduceCS);
	SHADER_USE_PARAMETER_STRUCT(FFilmGrainReduceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, FilmGrainTextureSize)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FilmGrainTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
	END_SHADER_PARAMETER_STRUCT()
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsFilmGrain(Parameters.Platform);
	}
};

class FFilmGrainPackConstantsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFilmGrainPackConstantsCS);
	SHADER_USE_PARAMETER_STRUCT(FFilmGrainPackConstantsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, OriginalFilmGrainTextureSize)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReducedFilmGrainTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, FilmGrainConstantsOutput)
	END_SHADER_PARAMETER_STRUCT()
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsFilmGrain(Parameters.Platform);
	}
};

class FTonemapVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTonemapVS);

	// FDrawRectangleParameters is filled by DrawScreenPass.
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FTonemapVS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<TonemapperPermutation::FTonemapperEyeAdaptationDim>;
	using FParameters = FTonemapParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FTonemapPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTonemapPS);
	SHADER_USE_PARAMETER_STRUCT(FTonemapPS, FGlobalShader);

	using FPermutationDomain = TonemapperPermutation::FDesktopDomain;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTonemapParameters, Tonemap)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1))
		{
			return false;
		}
		return TonemapperPermutation::ShouldCompileDesktopPermutation(Parameters, FPermutationDomain(Parameters.PermutationId));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		const int UseVolumeLut = PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(Parameters.Platform) ? 1 : 0;
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeLut);

		OutEnvironment.SetDefine(TEXT("SUPPORTS_SCENE_COLOR_APPLY_PARAMETERS"), FTonemapInputs::SupportsSceneColorApplyParametersBuffer(Parameters.Platform) ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("SUPPORTS_FILM_GRAIN"), SupportsFilmGrain(Parameters.Platform) ? 1 : 0);
	}
};

class FTonemapCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTonemapCS);
	SHADER_USE_PARAMETER_STRUCT(FTonemapCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<
		TonemapperPermutation::FDesktopDomain,
		TonemapperPermutation::FTonemapperEyeAdaptationDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTonemapParameters, Tonemap)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWOutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		return TonemapperPermutation::ShouldCompileDesktopPermutation(Parameters, PermutationVector.Get<TonemapperPermutation::FDesktopDomain>());
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GTonemapComputeTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GTonemapComputeTileSizeY);

		const int UseVolumeLut = PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(Parameters.Platform) ? 1 : 0;
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeLut);

		OutEnvironment.SetDefine(TEXT("SUPPORTS_SCENE_COLOR_APPLY_PARAMETERS"), FTonemapInputs::SupportsSceneColorApplyParametersBuffer(Parameters.Platform) ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("SUPPORTS_FILM_GRAIN"), SupportsFilmGrain(Parameters.Platform) ? 1 : 0);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFilmGrainReduceCS,        "/Engine/Private/PostProcessing/FilmGrainReduce.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FFilmGrainPackConstantsCS, "/Engine/Private/PostProcessing/FilmGrainPackConstants.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTonemapVS, "/Engine/Private/PostProcessTonemap.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FTonemapPS, "/Engine/Private/PostProcessTonemap.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FTonemapCS, "/Engine/Private/PostProcessTonemap.usf", "MainCS", SF_Compute);

bool FTonemapInputs::SupportsSceneColorApplyParametersBuffer(EShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsFFTBloom(Platform);
}

static
FRDGBufferRef BuildFilmGrainConstants(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef FilmGrainTexture)
{
	RDG_EVENT_SCOPE(GraphBuilder, "FilmGrain BuildTextureConstants");

	FRDGTextureRef ReducedFilmGrainTexture = FilmGrainTexture;

	for (int32 PassId = 0; ReducedFilmGrainTexture->Desc.Extent.X > 1 && ReducedFilmGrainTexture->Desc.Extent.Y > 1; PassId++)
	{
		FRDGTextureRef NewReducedFilmGrainTexture;
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				FIntPoint::DivideAndRoundUp(ReducedFilmGrainTexture->Desc.Extent, 8),
				PF_A32B32G32R32F,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV);

			NewReducedFilmGrainTexture = GraphBuilder.CreateTexture(Desc, TEXT("FilmGrain.Reduce"));
		}

		FFilmGrainReduceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmGrainReduceCS::FParameters>();
		PassParameters->FilmGrainTextureSize = ReducedFilmGrainTexture->Desc.Extent;
		PassParameters->FilmGrainTexture = ReducedFilmGrainTexture;
		PassParameters->Output = GraphBuilder.CreateUAV(NewReducedFilmGrainTexture);

		TShaderMapRef<FFilmGrainReduceCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FilmGrain Reduce %dx%d -> %dx%d",
				ReducedFilmGrainTexture->Desc.Extent.X, ReducedFilmGrainTexture->Desc.Extent.Y,
				NewReducedFilmGrainTexture->Desc.Extent.X, NewReducedFilmGrainTexture->Desc.Extent.Y),
			ComputeShader,
			PassParameters,
			FIntVector(NewReducedFilmGrainTexture->Desc.Extent.X, NewReducedFilmGrainTexture->Desc.Extent.Y, 1));

		ReducedFilmGrainTexture = NewReducedFilmGrainTexture;
	}

	FRDGBufferRef FilmGrainConstantsBuffer;
	{
		FilmGrainConstantsBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FLinearColor), /* NumElements = */ 1),
			TEXT("FilmGrain.TextureConstants"));

		FFilmGrainPackConstantsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilmGrainPackConstantsCS::FParameters>();
		PassParameters->OriginalFilmGrainTextureSize = FilmGrainTexture->Desc.Extent;
		PassParameters->ReducedFilmGrainTexture = ReducedFilmGrainTexture;
		PassParameters->FilmGrainConstantsOutput = GraphBuilder.CreateUAV(FilmGrainConstantsBuffer);

		TShaderMapRef<FFilmGrainPackConstantsCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FilmGrain PackConstants"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	return FilmGrainConstantsBuffer;
}

FScreenPassTexture AddTonemapPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FTonemapInputs& Inputs)
{
	if (!Inputs.bGammaOnly)
	{
		check(Inputs.ColorGradingTexture);
	}
	check(Inputs.SceneColor.IsValid());

	const FSceneViewFamily& ViewFamily = *(View.Family);
	const FPostProcessSettings& PostProcessSettings = View.FinalPostProcessSettings;

	const bool bIsEyeAdaptationResource = (View.GetFeatureLevel() >= ERHIFeatureLevel::SM5) ? Inputs.EyeAdaptationTexture != nullptr : Inputs.EyeAdaptationBuffer != nullptr;
	const bool bEyeAdaptation = ViewFamily.EngineShowFlags.EyeAdaptation && bIsEyeAdaptationResource;

	const FScreenPassTextureViewport SceneColorViewport(Inputs.SceneColor);

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;

	if (!Output.IsValid())
	{
		FRDGTextureDesc OutputDesc = Inputs.SceneColor.Texture->Desc;
		OutputDesc.Reset();
		OutputDesc.Flags |= View.bUseComputePasses ? TexCreate_UAV : TexCreate_RenderTargetable;
		OutputDesc.Flags |= GFastVRamConfig.Tonemap;
		// RGB is the color in LDR, A is the luminance for PostprocessAA
		OutputDesc.ClearValue = FClearValueBinding(FLinearColor(0, 0, 0, 0));

		const FTonemapperOutputDeviceParameters OutputDeviceParameters = GetTonemapperOutputDeviceParameters(*View.Family);
		const EDisplayOutputFormat OutputDevice = static_cast<EDisplayOutputFormat>(OutputDeviceParameters.OutputDevice);

		if (OutputDevice == EDisplayOutputFormat::HDR_LinearEXR)
		{
			OutputDesc.Format = PF_A32B32G32R32F;
		}
		else if (OutputDevice == EDisplayOutputFormat::HDR_LinearNoToneCurve || OutputDevice == EDisplayOutputFormat::HDR_LinearWithToneCurve)
		{
			OutputDesc.Format = PF_FloatRGBA;
		}
		else if (Inputs.bOutputInHDR)
		{
			OutputDesc.Format = GRHIHDRDisplayOutputFormat;
		}
		else if (View.Family->RenderTarget && View.Family->RenderTarget->GetRenderTargetTexture())
		{
			// Render into a pixel format that do not loose bit depth precision for the view family.
			OutputDesc.Format = View.Family->RenderTarget->GetRenderTargetTexture()->GetFormat();
		}
		else if (IsPostProcessingWithAlphaChannelSupported())
		{
			// Make sure there is no loss for a 10bit bit-depth using the 10bit of mantissa of halfs
			OutputDesc.Format = PF_FloatRGBA;
		}
		else 
		{
			// Make sure there is no loss for a 10bit bit-depth
			OutputDesc.Format = PF_A2B10G10R10;
		}

		Output = FScreenPassRenderTarget(
			GraphBuilder.CreateTexture(OutputDesc, TEXT("Tonemap")),
			Inputs.SceneColor.ViewRect,
			ERenderTargetLoadAction::EClear);
	}

	const FScreenPassTextureViewport OutputViewport(Output);

	FRHISamplerState* BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	FRHISamplerState* PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const float DefaultEyeExposure = bEyeAdaptation ? 0.0f : GetEyeAdaptationFixedExposure(View);

	const float SharpenDiv6 = FMath::Clamp(CVarTonemapperSharpen.GetValueOnRenderThread(), 0.0f, 10.0f) / 6.0f;

	FVector4f ChromaticAberrationParams;

	{
		// for scene color fringe
		// from percent to fraction
		float Offset = 0.0f;
		float StartOffset = 0.0f;
		float Multiplier = 1.0f;

		if (PostProcessSettings.ChromaticAberrationStartOffset < 1.0f - KINDA_SMALL_NUMBER)
		{
			Offset = PostProcessSettings.SceneFringeIntensity * 0.01f;
			StartOffset = PostProcessSettings.ChromaticAberrationStartOffset;
			Multiplier = 1.0f / (1.0f - StartOffset);
		}

		// Wavelength of primaries in nm
		const float PrimaryR = 611.3f;
		const float PrimaryG = 549.1f;
		const float PrimaryB = 464.3f;

		// Simple lens chromatic aberration is roughly linear in wavelength
		float ScaleR = 0.007f * (PrimaryR - PrimaryB);
		float ScaleG = 0.007f * (PrimaryG - PrimaryB);
		ChromaticAberrationParams = FVector4f(Offset * ScaleR * Multiplier, Offset * ScaleG * Multiplier, StartOffset, 0.f);
	}

	float EditorNITLevel = 160.0f;

	#if WITH_EDITOR
	{
		static auto CVarHDRNITLevel = IConsoleManager::Get().FindConsoleVariable(TEXT("Editor.HDRNITLevel"));
		if (CVarHDRNITLevel)
		{
			EditorNITLevel = CVarHDRNITLevel->GetFloat();
		}
	}
	#endif

	FTonemapParameters CommonParameters;
	CommonParameters.View = View.ViewUniformBuffer;

	{
		uint8 FrameIndexMod8 = 0;
		if (View.ViewState)
		{
			FrameIndexMod8 = View.ViewState->GetFrameIndex(8);
		}
		GrainRandomFromFrame(&CommonParameters.FilmGrain.GrainRandomFull, FrameIndexMod8);
	}

	if (View.FilmGrainTexture)
	{
		check(SupportsFilmGrain(View.GetShaderPlatform()));

		FRHITexture* FilmGrainTextureRHI = View.FilmGrainTexture->GetTexture2DRHI();
		FRDGTextureRef FilmGrainTexture = RegisterExternalTexture(GraphBuilder, FilmGrainTextureRHI, TEXT("FilmGrain.OriginalTexture"));


		FRDGBufferRef FilmGrainConstantsBuffer;
		if (View.ViewState && View.ViewState->FilmGrainCache.TextureRHI == FilmGrainTextureRHI && CVarFilmGrainCacheTextureConstants.GetValueOnRenderThread() != 0)
		{
			FilmGrainConstantsBuffer = GraphBuilder.RegisterExternalBuffer(View.ViewState->FilmGrainCache.ConstantsBuffer, TEXT("FilmGrain.TextureConstants"));
		}
		else
		{
			FilmGrainConstantsBuffer = BuildFilmGrainConstants(GraphBuilder, View, FilmGrainTexture);

			if (View.ViewState)
			{
				View.ViewState->FilmGrainCache.Texture = View.FinalPostProcessSettings.FilmGrainTexture;
				View.ViewState->FilmGrainCache.TextureRHI = FilmGrainTextureRHI;
				GraphBuilder.QueueBufferExtraction(FilmGrainConstantsBuffer, &View.ViewState->FilmGrainCache.ConstantsBuffer);
			}
		}

		// (FilmGrainTexture * FilmGrainDecodeMultiply + FilmGrainDecodeAdd)
		CommonParameters.FilmGrain.FilmGrainIntensityShadows    = View.FinalPostProcessSettings.FilmGrainIntensity * View.FinalPostProcessSettings.FilmGrainIntensityShadows;
		CommonParameters.FilmGrain.FilmGrainIntensityMidtones   = View.FinalPostProcessSettings.FilmGrainIntensity * View.FinalPostProcessSettings.FilmGrainIntensityMidtones;
		CommonParameters.FilmGrain.FilmGrainIntensityHighlights = View.FinalPostProcessSettings.FilmGrainIntensity * View.FinalPostProcessSettings.FilmGrainIntensityHighlights;
		CommonParameters.FilmGrain.FilmGrainShadowsMax = View.FinalPostProcessSettings.FilmGrainShadowsMax;
		CommonParameters.FilmGrain.FilmGrainHighlightsMin = View.FinalPostProcessSettings.FilmGrainHighlightsMin;
		CommonParameters.FilmGrain.FilmGrainHighlightsMax = View.FinalPostProcessSettings.FilmGrainHighlightsMax;

		CommonParameters.FilmGrain.FilmGrainTexture = FilmGrainTexture;
		CommonParameters.FilmGrain.FilmGrainSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap>::GetRHI();

		int32 RandomSequenceLength = CVarFilmGrainSequenceLength.GetValueOnRenderThread();
		int32 RandomSequenceIndex = (View.ViewState ? View.ViewState->FrameIndex : 0) % RandomSequenceLength;

		FVector2f RandomGrainTextureUVOffset;
		RandomGrainTextureUVOffset.X = Halton(RandomSequenceIndex + 1, 2);
		RandomGrainTextureUVOffset.Y = Halton(RandomSequenceIndex + 1, 3);

		FVector2f TextureSize(View.FilmGrainTexture->GetSizeX(), View.FilmGrainTexture->GetSizeY());
		FVector2f OutputSizeF(1920.0f, 1920.0f * float(OutputViewport.Rect.Height()) / float(OutputViewport.Rect.Width()));

		CommonParameters.FilmGrain.ScreenPosToFilmGrainTextureUV = 
			FScreenTransform::ScreenPosToViewportUV *
			((OutputSizeF / TextureSize) * (1.0f / View.FinalPostProcessSettings.FilmGrainTexelSize)) + RandomGrainTextureUVOffset;

		CommonParameters.FilmGrain.FilmGrainTextureConstants = GraphBuilder.CreateSRV(FilmGrainConstantsBuffer);
	}
	else
	{
		// Release the film grain cache
		if (View.ViewState)
		{
			View.ViewState->FilmGrainCache.SafeRelease();
		}

		CommonParameters.FilmGrain.FilmGrainIntensityShadows = 0.0f;
		CommonParameters.FilmGrain.FilmGrainIntensityMidtones = 0.0f;
		CommonParameters.FilmGrain.FilmGrainIntensityHighlights = 0.0f;
		CommonParameters.FilmGrain.FilmGrainShadowsMax = 0.09f;
		CommonParameters.FilmGrain.FilmGrainHighlightsMin = 0.5f;
		CommonParameters.FilmGrain.FilmGrainHighlightsMax = 1.0f;
		CommonParameters.FilmGrain.FilmGrainTexture = GSystemTextures.GetWhiteDummy(GraphBuilder);
		CommonParameters.FilmGrain.FilmGrainSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap>::GetRHI();
		CommonParameters.FilmGrain.ScreenPosToFilmGrainTextureUV = FScreenTransform::ScreenPosToViewportUV;

		// Set a dummy texture constants
		if (SupportsFilmGrain(View.GetShaderPlatform()))
		{
			FRDGBufferRef DummyFilmGrainTextureConstants = GSystemTextures.GetDefaultStructuredBuffer(
				GraphBuilder, sizeof(FVector4f), FVector4f(FLinearColor::White));

			CommonParameters.FilmGrain.FilmGrainTextureConstants = GraphBuilder.CreateSRV(DummyFilmGrainTextureConstants);
		}
		else
		{
			CommonParameters.FilmGrain.FilmGrainTextureConstants = nullptr;
		}
	}

	CommonParameters.OutputDevice = GetTonemapperOutputDeviceParameters(ViewFamily);
	CommonParameters.Color = GetScreenPassTextureViewportParameters(SceneColorViewport);
	CommonParameters.Output = GetScreenPassTextureViewportParameters(OutputViewport);
	CommonParameters.ColorTexture = Inputs.SceneColor.Texture;
	CommonParameters.LumBilateralGrid = Inputs.LocalExposureTexture;
	CommonParameters.BlurredLogLum = Inputs.BlurredLogLuminanceTexture;
	CommonParameters.LumBilateralGridSampler = BilinearClampSampler;
	CommonParameters.BlurredLogLumSampler = BilinearClampSampler;
	CommonParameters.EyeAdaptationTexture = Inputs.EyeAdaptationTexture;
	CommonParameters.EyeAdaptation = *Inputs.EyeAdaptationParameters;
	CommonParameters.ColorGradingLUT = Inputs.ColorGradingTexture;
	CommonParameters.ColorSampler = BilinearClampSampler;
	CommonParameters.ColorGradingLUTSampler = BilinearClampSampler;
	CommonParameters.ColorScale0 = PostProcessSettings.SceneColorTint;
	CommonParameters.ChromaticAberrationParams = ChromaticAberrationParams;
	CommonParameters.TonemapperParams = FVector4f(PostProcessSettings.VignetteIntensity, SharpenDiv6, 0.0f, 0.0f);
	CommonParameters.DefaultEyeExposure = DefaultEyeExposure;
	CommonParameters.EditorNITLevel = EditorNITLevel;
	CommonParameters.bOutputInHDR = ViewFamily.bIsHDR;
	CommonParameters.LensPrincipalPointOffsetScale = View.LensPrincipalPointOffsetScale;

	// TODO: PostProcessSettings.BloomDirtMask->GetResource() is not thread safe
	if (PostProcessSettings.BloomDirtMask && PostProcessSettings.BloomDirtMask->GetResource() && PostProcessSettings.BloomDirtMaskIntensity > 0 && PostProcessSettings.BloomIntensity > 0.0)
	{
		CommonParameters.BloomDirtMaskTint = PostProcessSettings.BloomDirtMaskTint * PostProcessSettings.BloomDirtMaskIntensity / PostProcessSettings.BloomIntensity;
		CommonParameters.BloomDirtMaskTexture = PostProcessSettings.BloomDirtMask->GetResource()->TextureRHI;
		CommonParameters.BloomDirtMaskSampler = BilinearClampSampler;
	}
	else
	{
		CommonParameters.BloomDirtMaskTint = FLinearColor::Black;
		CommonParameters.BloomDirtMaskTexture = GBlackTexture->TextureRHI;
		CommonParameters.BloomDirtMaskSampler = BilinearClampSampler;
	}

	if (!FTonemapInputs::SupportsSceneColorApplyParametersBuffer(View.GetShaderPlatform()))
	{
		check(Inputs.SceneColorApplyParamaters == nullptr);
	}
	else if (Inputs.SceneColorApplyParamaters)
	{
		CommonParameters.SceneColorApplyParamaters = GraphBuilder.CreateSRV(Inputs.SceneColorApplyParamaters);
	}
	else
	{
		FRDGBufferRef ApplyParametersBuffer = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f), FVector4f(1.0f, 1.0f, 1.0f, 1.0f));
		CommonParameters.SceneColorApplyParamaters = GraphBuilder.CreateSRV(ApplyParametersBuffer);
	}

	// Bloom parameters
	{
		if (Inputs.Bloom.Texture)
		{
			const FScreenPassTextureViewport BloomViewport(Inputs.Bloom);
			CommonParameters.ColorToBloom = FScreenTransform::ChangeTextureUVCoordinateFromTo(SceneColorViewport, BloomViewport);
			CommonParameters.BloomUVViewportBilinearMin = GetScreenPassTextureViewportParameters(BloomViewport).UVViewportBilinearMin;
			CommonParameters.BloomUVViewportBilinearMax = GetScreenPassTextureViewportParameters(BloomViewport).UVViewportBilinearMax;
			CommonParameters.BloomTexture = Inputs.Bloom.Texture;
			CommonParameters.BloomSampler = BilinearClampSampler;
		}
		else
		{
			CommonParameters.ColorToBloom = FScreenTransform::Identity;
			CommonParameters.BloomUVViewportBilinearMin = FVector2f::ZeroVector;
			CommonParameters.BloomUVViewportBilinearMax = FVector2f::UnitVector;
			CommonParameters.BloomTexture = GSystemTextures.GetBlackDummy(GraphBuilder);
			CommonParameters.BloomSampler = BilinearClampSampler;
		}
	}

	// forward transformation from shader:
	//return LensPrincipalPointOffsetScale.xy + UV * LensPrincipalPointOffsetScale.zw;

	// reverse transformation from shader:
	//return UV*(1.0f/LensPrincipalPointOffsetScale.zw) - LensPrincipalPointOffsetScale.xy/LensPrincipalPointOffsetScale.zw;

	CommonParameters.LensPrincipalPointOffsetScaleInverse.X = -View.LensPrincipalPointOffsetScale.X / View.LensPrincipalPointOffsetScale.Z;
	CommonParameters.LensPrincipalPointOffsetScaleInverse.Y = -View.LensPrincipalPointOffsetScale.Y / View.LensPrincipalPointOffsetScale.W;
	CommonParameters.LensPrincipalPointOffsetScaleInverse.Z = 1.0f / View.LensPrincipalPointOffsetScale.Z;
	CommonParameters.LensPrincipalPointOffsetScaleInverse.W = 1.0f / View.LensPrincipalPointOffsetScale.W;
	
	FRDGBufferSRVRef EyeAdaptationBufferSRV = Inputs.EyeAdaptationBuffer != nullptr ? GraphBuilder.CreateSRV(Inputs.EyeAdaptationBuffer, PF_A32B32G32R32F) : nullptr;

	CommonParameters.EyeAdaptationBuffer = EyeAdaptationBufferSRV;

	// Generate permutation vector for the desktop tonemapper.
	TonemapperPermutation::FDesktopDomain DesktopPermutationVector;

	{
		TonemapperPermutation::FCommonDomain CommonDomain = TonemapperPermutation::BuildCommonPermutationDomain(View, Inputs.bGammaOnly, Inputs.LocalExposureTexture != nullptr, Inputs.bMetalMSAAHDRDecode);
		DesktopPermutationVector.Set<TonemapperPermutation::FCommonDomain>(CommonDomain);

		if (!CommonDomain.Get<TonemapperPermutation::FTonemapperGammaOnlyDim>())
		{
			// Grain Quantization
			{
				static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Tonemapper.GrainQuantization"));
				const int32 Value = CVar->GetValueOnRenderThread();
				DesktopPermutationVector.Set<TonemapperPermutation::FTonemapperGrainQuantizationDim>(Value > 0);
			}

			DesktopPermutationVector.Set<TonemapperPermutation::FTonemapperColorFringeDim>(PostProcessSettings.SceneFringeIntensity > 0.01f);
		}

		DesktopPermutationVector.Set<TonemapperPermutation::FTonemapperOutputDeviceDim>(EDisplayOutputFormat(CommonParameters.OutputDevice.OutputDevice));

		DesktopPermutationVector = TonemapperPermutation::RemapPermutation(DesktopPermutationVector, View.GetFeatureLevel());
	}

	// Override output might not support UAVs.
	const bool bComputePass = (Output.Texture->Desc.Flags & TexCreate_UAV) == TexCreate_UAV ? View.bUseComputePasses : false;

	if (bComputePass)
	{
		FTonemapCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTonemapCS::FParameters>();
		PassParameters->Tonemap = CommonParameters;
		PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(Output.Texture);

		FTonemapCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<TonemapperPermutation::FDesktopDomain>(DesktopPermutationVector);
		PermutationVector.Set<TonemapperPermutation::FTonemapperEyeAdaptationDim>(bEyeAdaptation);

		TShaderMapRef<FTonemapCS> ComputeShader(View.ShaderMap, PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Tonemap %dx%d (CS GammaOnly=%d)", OutputViewport.Rect.Width(), OutputViewport.Rect.Height(), Inputs.bGammaOnly),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(OutputViewport.Rect.Size(), FIntPoint(GTonemapComputeTileSizeX, GTonemapComputeTileSizeY)));
	}
	else
	{
		FTonemapPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTonemapPS::FParameters>();
		PassParameters->Tonemap = CommonParameters;
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		FTonemapVS::FPermutationDomain VertexPermutationVector;
		VertexPermutationVector.Set<TonemapperPermutation::FTonemapperEyeAdaptationDim>(bEyeAdaptation);

		TShaderMapRef<FTonemapVS> VertexShader(View.ShaderMap, VertexPermutationVector);
		TShaderMapRef<FTonemapPS> PixelShader(View.ShaderMap, DesktopPermutationVector);

		// If this is a stereo view, there's a good chance we need alpha out of the tonemapper
		// @todo: Remove this once Oculus fix the bug in their runtime that requires alpha here.
		const bool bIsStereo = IStereoRendering::IsStereoEyeView(View);
		const bool bFormatNeedsAlphaWrite = Output.Texture->Desc.Format == PF_R9G9B9EXP5;
		FRHIBlendState* BlendState = (Inputs.bWriteAlphaChannel || bIsStereo || bFormatNeedsAlphaWrite) ? FScreenPassPipelineState::FDefaultBlendState::GetRHI() : TStaticBlendStateWriteMask<CW_RGB>::GetRHI();
		FRHIDepthStencilState* DepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();

		EScreenPassDrawFlags DrawFlags = EScreenPassDrawFlags::AllowHMDHiddenAreaMask;

		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("Tonemap %dx%d (PS GammaOnly=%d)", OutputViewport.Rect.Width(), OutputViewport.Rect.Height(), Inputs.bGammaOnly),
			View,
			OutputViewport,
			SceneColorViewport,
			FScreenPassPipelineState(VertexShader, PixelShader, BlendState, DepthStencilState),
			PassParameters,
			DrawFlags,
			[VertexShader, PixelShader, PassParameters](FRHICommandList& RHICmdList)
		{
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->Tonemap);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
		});
	}

	return MoveTemp(Output);
}

