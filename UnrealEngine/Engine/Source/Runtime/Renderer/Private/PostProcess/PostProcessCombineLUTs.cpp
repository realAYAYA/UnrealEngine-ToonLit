// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcess/PostProcessCombineLUTs.h"
#include "PostProcess/PostProcessTonemap.h"
#include "ScenePrivate.h"
#include "VolumeRendering.h"
#include "HDRHelper.h"

namespace
{
TAutoConsoleVariable<float> CVarColorMin(
	TEXT("r.Color.Min"),
	0.0f,
	TEXT("Allows to define where the value 0 in the color channels is mapped to after color grading.\n")
	TEXT("The value should be around 0, positive: a gray scale is added to the darks, negative: more dark values become black, Default: 0"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarColorMid(
	TEXT("r.Color.Mid"),
	0.5f,
	TEXT("Allows to define where the value 0.5 in the color channels is mapped to after color grading (This is similar to a gamma correction).\n")
	TEXT("Value should be around 0.5, smaller values darken the mid tones, larger values brighten the mid tones, Default: 0.5"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarColorMax(
	TEXT("r.Color.Max"),
	1.0f,
	TEXT("Allows to define where the value 1.0 in the color channels is mapped to after color grading.\n")
	TEXT("Value should be around 1, smaller values darken the highlights, larger values move more colors towards white, Default: 1"),
	ECVF_RenderThreadSafe);

int32 GLUTSize = 32;
FAutoConsoleVariableRef CVarLUTSize(
	TEXT("r.LUT.Size"),
	GLUTSize,
	TEXT("Size of film LUT"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarColorGrading(
	TEXT("r.Color.Grading"), 1,
	TEXT("Controls whether post process settings's color grading settings should be applied."),
	ECVF_RenderThreadSafe);

// Including the neutral one at index 0
const uint32 GMaxLUTBlendCount = 5;

struct FColorTransform
{
	FColorTransform()
	{
		Reset();
	}

	float MinValue;
	float MidValue;
	float MaxValue;

	void Reset()
	{
		MinValue = 0.0f;
		MidValue = 0.5f;
		MaxValue = 1.0f;
	}
};
} //! namespace

// false:use 256x16 texture / true:use volume texture (faster, requires geometry shader)
// USE_VOLUME_LUT: needs to be the same for C++ and HLSL.
// Safe to use at pipeline and run time.
bool PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(EShaderPlatform Platform)
{
	// This is used to know if the target shader platform does not support required volume texture features we need for sure (read, render to).
	return RHIVolumeTextureRenderingSupportGuaranteed(Platform) && (RHISupportsGeometryShaders(Platform) || RHISupportsVertexShaderLayer(Platform));
}

FColorRemapParameters GetColorRemapParameters()
{
	FColorTransform ColorTransform;
	ColorTransform.MinValue = FMath::Clamp(CVarColorMin.GetValueOnRenderThread(), -10.0f, 10.0f);
	ColorTransform.MidValue = FMath::Clamp(CVarColorMid.GetValueOnRenderThread(), -10.0f, 10.0f);
	ColorTransform.MaxValue = FMath::Clamp(CVarColorMax.GetValueOnRenderThread(), -10.0f, 10.0f);

	// x is the input value, y the output value
	// RGB = a, b, c where y = a * x*x + b * x + c
	float c = ColorTransform.MinValue;
	float b = 4 * ColorTransform.MidValue - 3 * ColorTransform.MinValue - ColorTransform.MaxValue;
	float a = ColorTransform.MaxValue - ColorTransform.MinValue - b;

	FColorRemapParameters Parameters;
	Parameters.MappingPolynomial = FVector3f(a, b, c);
	return Parameters;
}

BEGIN_SHADER_PARAMETER_STRUCT(FACESTonemapShaderParameters, )
	SHADER_PARAMETER(FVector4f, ACESMinMaxData) // xy = min ACES/luminance, zw = max ACES/luminance
	SHADER_PARAMETER(FVector4f, ACESMidData) // x = mid ACES, y = mid luminance, z = mid slope
	SHADER_PARAMETER(FVector4f, ACESCoefsLow_0) // coeflow 0-3
	SHADER_PARAMETER(FVector4f, ACESCoefsHigh_0) // coefhigh 0-3
	SHADER_PARAMETER(float, ACESCoefsLow_4)
	SHADER_PARAMETER(float, ACESCoefsHigh_4)
	SHADER_PARAMETER(float, ACESSceneColorMultiplier)
END_SHADER_PARAMETER_STRUCT()


BEGIN_SHADER_PARAMETER_STRUCT(FCombineLUTParameters, )
	SHADER_PARAMETER_TEXTURE_ARRAY(Texture2D, Textures, [GMaxLUTBlendCount])
	SHADER_PARAMETER_SAMPLER_ARRAY(SamplerState, Samplers, [GMaxLUTBlendCount])
	SHADER_PARAMETER_SCALAR_ARRAY(float, LUTWeights, [GMaxLUTBlendCount])
	SHADER_PARAMETER_STRUCT_REF(FWorkingColorSpaceShaderParameters, WorkingColorSpace)
	SHADER_PARAMETER_STRUCT_INCLUDE(FACESTonemapShaderParameters, ACESTonemapParameters)
	SHADER_PARAMETER(FVector4f, OverlayColor)
	SHADER_PARAMETER(FVector3f, ColorScale)
	SHADER_PARAMETER(FVector4f, ColorSaturation)
	SHADER_PARAMETER(FVector4f, ColorContrast)
	SHADER_PARAMETER(FVector4f, ColorGamma)
	SHADER_PARAMETER(FVector4f, ColorGain)
	SHADER_PARAMETER(FVector4f, ColorOffset)
	SHADER_PARAMETER(FVector4f, ColorSaturationShadows)
	SHADER_PARAMETER(FVector4f, ColorContrastShadows)
	SHADER_PARAMETER(FVector4f, ColorGammaShadows)
	SHADER_PARAMETER(FVector4f, ColorGainShadows)
	SHADER_PARAMETER(FVector4f, ColorOffsetShadows)
	SHADER_PARAMETER(FVector4f, ColorSaturationMidtones)
	SHADER_PARAMETER(FVector4f, ColorContrastMidtones)
	SHADER_PARAMETER(FVector4f, ColorGammaMidtones)
	SHADER_PARAMETER(FVector4f, ColorGainMidtones)
	SHADER_PARAMETER(FVector4f, ColorOffsetMidtones)
	SHADER_PARAMETER(FVector4f, ColorSaturationHighlights)
	SHADER_PARAMETER(FVector4f, ColorContrastHighlights)
	SHADER_PARAMETER(FVector4f, ColorGammaHighlights)
	SHADER_PARAMETER(FVector4f, ColorGainHighlights)
	SHADER_PARAMETER(FVector4f, ColorOffsetHighlights)
	SHADER_PARAMETER(float, WhiteTemp)
	SHADER_PARAMETER(float, WhiteTint)
	SHADER_PARAMETER(float, ColorCorrectionShadowsMax)
	SHADER_PARAMETER(float, ColorCorrectionHighlightsMin)
	SHADER_PARAMETER(float, ColorCorrectionHighlightsMax)
	SHADER_PARAMETER(float, BlueCorrection)
	SHADER_PARAMETER(float, ExpandGamut)
	SHADER_PARAMETER(float, ToneCurveAmount)
	SHADER_PARAMETER(float, FilmSlope)
	SHADER_PARAMETER(float, FilmToe)
	SHADER_PARAMETER(float, FilmShoulder)
	SHADER_PARAMETER(float, FilmBlackClip)
	SHADER_PARAMETER(float, FilmWhiteClip)
	SHADER_PARAMETER(uint32, bUseMobileTonemapper)
	SHADER_PARAMETER(uint32, bIsTemperatureWhiteBalance)
	SHADER_PARAMETER_STRUCT_INCLUDE(FColorRemapParameters, ColorRemap)
	SHADER_PARAMETER_STRUCT_INCLUDE(FTonemapperOutputDeviceParameters, OutputDevice)
END_SHADER_PARAMETER_STRUCT()

void GetCombineLUTParameters(
	FCombineLUTParameters& Parameters,
	const FViewInfo& View,
	const FTexture* const* Textures,
	const float* Weights,
	uint32 BlendCount)
{
	check(Textures);
	check(Weights);

	static const FPostProcessSettings DefaultSettings;

	const FSceneViewFamily& ViewFamily = *(View.Family);
	const FPostProcessSettings& Settings = (
		ViewFamily.EngineShowFlags.ColorGrading && CVarColorGrading.GetValueOnRenderThread() != 0)
		? View.FinalPostProcessSettings
		: DefaultSettings;

	for (uint32 BlendIndex = 0; BlendIndex < BlendCount; ++BlendIndex)
	{
		// Neutral texture occupies the first slot and doesn't actually need to be set.
		if (BlendIndex != 0)
		{
			check(Textures[BlendIndex]);

			// Don't use texture asset sampler as it might have anisotropic filtering enabled
			Parameters.Textures[BlendIndex] = Textures[BlendIndex]->TextureRHI;
			Parameters.Samplers[BlendIndex] = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
		}

		GET_SCALAR_ARRAY_ELEMENT(Parameters.LUTWeights, BlendIndex) = Weights[BlendIndex];
	}

	Parameters.WorkingColorSpace = GDefaultWorkingColorSpaceUniformBuffer.GetUniformBufferRef();
	GetACESTonemapParameters(Parameters.ACESTonemapParameters);

	Parameters.ColorScale = FVector4f(View.ColorScale);
	Parameters.OverlayColor = View.OverlayColor;
	Parameters.ColorRemap = GetColorRemapParameters();

	// White balance
	Parameters.bIsTemperatureWhiteBalance = Settings.TemperatureType == ETemperatureMethod::TEMP_WhiteBalance;
	Parameters.WhiteTemp = Settings.WhiteTemp;
	Parameters.WhiteTint = Settings.WhiteTint;

	// Color grade
	Parameters.ColorSaturation = (FVector4f)Settings.ColorSaturation;
	Parameters.ColorContrast = (FVector4f)Settings.ColorContrast;
	Parameters.ColorGamma = (FVector4f)Settings.ColorGamma;
	Parameters.ColorGain = (FVector4f)Settings.ColorGain;
	Parameters.ColorOffset = (FVector4f)Settings.ColorOffset;

	Parameters.ColorSaturationShadows = (FVector4f)Settings.ColorSaturationShadows;
	Parameters.ColorContrastShadows = (FVector4f)Settings.ColorContrastShadows;
	Parameters.ColorGammaShadows = (FVector4f)Settings.ColorGammaShadows;
	Parameters.ColorGainShadows = (FVector4f)Settings.ColorGainShadows;
	Parameters.ColorOffsetShadows = (FVector4f)Settings.ColorOffsetShadows;

	Parameters.ColorSaturationMidtones = (FVector4f)Settings.ColorSaturationMidtones;
	Parameters.ColorContrastMidtones = (FVector4f)Settings.ColorContrastMidtones;
	Parameters.ColorGammaMidtones = (FVector4f)Settings.ColorGammaMidtones;
	Parameters.ColorGainMidtones = (FVector4f)Settings.ColorGainMidtones;
	Parameters.ColorOffsetMidtones = (FVector4f)Settings.ColorOffsetMidtones;

	Parameters.ColorSaturationHighlights = (FVector4f)Settings.ColorSaturationHighlights;
	Parameters.ColorContrastHighlights = (FVector4f)Settings.ColorContrastHighlights;
	Parameters.ColorGammaHighlights = (FVector4f)Settings.ColorGammaHighlights;
	Parameters.ColorGainHighlights = (FVector4f)Settings.ColorGainHighlights;
	Parameters.ColorOffsetHighlights = (FVector4f)Settings.ColorOffsetHighlights;

	Parameters.ColorCorrectionShadowsMax = Settings.ColorCorrectionShadowsMax;
	Parameters.ColorCorrectionHighlightsMin = Settings.ColorCorrectionHighlightsMin;
	Parameters.ColorCorrectionHighlightsMax = Settings.ColorCorrectionHighlightsMax;

	Parameters.BlueCorrection = Settings.BlueCorrection;
	Parameters.ExpandGamut = Settings.ExpandGamut;
	Parameters.ToneCurveAmount = Settings.ToneCurveAmount;

	Parameters.FilmSlope = Settings.FilmSlope;
	Parameters.FilmToe = Settings.FilmToe;
	Parameters.FilmShoulder = Settings.FilmShoulder;
	Parameters.FilmBlackClip = Settings.FilmBlackClip;
	Parameters.FilmWhiteClip = Settings.FilmWhiteClip;

	Parameters.OutputDevice = GetTonemapperOutputDeviceParameters(ViewFamily);
}

class FLUTBlenderShader : public FGlobalShader
{
public:
	static const int32 GroupSize = 8;

	class FBlendCount : SHADER_PERMUTATION_RANGE_INT("BLENDCOUNT", 1, 5);
	class FSkipTemperature : SHADER_PERMUTATION_BOOL("SKIP_TEMPERATURE");
	using FPermutationDomain = TShaderPermutationDomain<FBlendCount, FSkipTemperature>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GroupSize);

		const int UseVolumeLUT = PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(Parameters.Platform) ? 1 : 0;
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeLUT);
	}

	FLUTBlenderShader() = default;
	FLUTBlenderShader(const CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

class FLUTBlenderPS : public FLUTBlenderShader
{
public:
	DECLARE_GLOBAL_SHADER(FLUTBlenderPS);
	SHADER_USE_PARAMETER_STRUCT(FLUTBlenderPS, FLUTBlenderShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCombineLUTParameters, CombineLUT)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLUTBlenderPS, "/Engine/Private/PostProcessCombineLUTs.usf", "MainPS", SF_Pixel);

class FLUTBlenderCS : public FLUTBlenderShader
{
public:
	DECLARE_GLOBAL_SHADER(FLUTBlenderCS);
	SHADER_USE_PARAMETER_STRUCT(FLUTBlenderCS, FLUTBlenderShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCombineLUTParameters, CombineLUT)
		SHADER_PARAMETER(FVector2f, OutputExtentInverse)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWOutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLUTBlenderCS, "/Engine/Private/PostProcessCombineLUTs.usf", "MainCS", SF_Compute);

uint32 GenerateFinalTable(const FFinalPostProcessSettings& Settings, const FTexture* OutTextures[], float OutWeights[], uint32 MaxCount)
{
	// Find the n strongest contributors, drop small contributors.
	uint32 LocalCount = 1;

	// Add the neutral one (done in the shader) as it should be the first and always there.
	OutTextures[0] = nullptr;
	OutWeights[0] = 0.0f;

	// Neutral index is the entry with no LUT texture assigned.
	for (int32 Index = 0; Index < Settings.ContributingLUTs.Num(); ++Index)
	{
		if (Settings.ContributingLUTs[Index].LUTTexture == nullptr)
		{
			OutWeights[0] = Settings.ContributingLUTs[Index].Weight;
			break;
		}
	}

	float OutWeightsSum = OutWeights[0];
	for (; LocalCount < MaxCount; ++LocalCount)
	{
		int32 BestIndex = INDEX_NONE;

		// Find the one with the strongest weight, add until full.
		for (int32 InputIndex = 0; InputIndex < Settings.ContributingLUTs.Num(); ++InputIndex)
		{
			bool bAlreadyInArray = false;

			{
				UTexture* LUTTexture = Settings.ContributingLUTs[InputIndex].LUTTexture;
				FTexture* Internal = LUTTexture ? LUTTexture->GetResource() : nullptr;
				for (uint32 OutputIndex = 0; OutputIndex < LocalCount; ++OutputIndex)
				{
					if (Internal == OutTextures[OutputIndex])
					{
						bAlreadyInArray = true;
						break;
					}
				}
			}

			if (bAlreadyInArray)
			{
				// We already have this one.
				continue;
			}

			// Take the first or better entry.
			if (BestIndex == INDEX_NONE || Settings.ContributingLUTs[BestIndex].Weight <= Settings.ContributingLUTs[InputIndex].Weight)
			{
				BestIndex = InputIndex;
			}
		}

		if (BestIndex == INDEX_NONE)
		{
			// No more elements to process.
			break;
		}

		const float WeightThreshold = 1.0f / 512.0f;

		const float BestWeight = Settings.ContributingLUTs[BestIndex].Weight;

		if (BestWeight < WeightThreshold)
		{
			// Drop small contributor 
			break;
		}

		UTexture* BestLUTTexture = Settings.ContributingLUTs[BestIndex].LUTTexture;
		FTexture* BestInternal = BestLUTTexture ? BestLUTTexture->GetResource() : nullptr;

		OutTextures[LocalCount] = BestInternal;
		OutWeights[LocalCount] = BestWeight;
		OutWeightsSum += BestWeight;
	}

	// Normalize the weights.
	if (OutWeightsSum > 0.001f)
	{
		const float OutWeightsSumInverse = 1.0f / OutWeightsSum;

		for (uint32 Index = 0; Index < LocalCount; ++Index)
		{
			OutWeights[Index] *= OutWeightsSumInverse;
		}
	}
	else
	{
		// Just the neutral texture at full weight.
		OutWeights[0] = 1.0f;
		LocalCount = 1;
	}

	return LocalCount;
}

FRDGTextureRef AddCombineLUTPass(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	const FSceneViewFamily& ViewFamily = *(View.Family);
	const FTexture* LocalTextures[GMaxLUTBlendCount];
	float LocalWeights[GMaxLUTBlendCount];
	uint32 LocalCount = 1;

	// Default to no LUTs.
	LocalTextures[0] = nullptr;
	LocalWeights[0] = 1.0f;

	if (ViewFamily.EngineShowFlags.ColorGrading)
	{
		LocalCount = GenerateFinalTable(View.FinalPostProcessSettings, LocalTextures, LocalWeights, GMaxLUTBlendCount);
	}

	const bool bUseComputePass = View.bUseComputePasses;

	const bool bUseVolumeTextureLUT = PipelineVolumeTextureLUTSupportGuaranteedAtRuntime(View.GetShaderPlatform());

	const bool bUseFloatOutput = ViewFamily.SceneCaptureSource == SCS_FinalColorHDR || ViewFamily.SceneCaptureSource == SCS_FinalToneCurveHDR;

	// Attempt to register the persistent view LUT texture.
	FRDGTextureRef OutputTexture = TryRegisterExternalTexture(GraphBuilder,
		View.GetTonemappingLUT(GraphBuilder.RHICmdList, GLUTSize, bUseVolumeTextureLUT, bUseComputePass, bUseFloatOutput));

	View.SetValidTonemappingLUT();

	// View doesn't support a persistent LUT, so create a temporary one.
	if (!OutputTexture)
	{
		OutputTexture = GraphBuilder.CreateTexture(
			Translate(FSceneViewState::CreateLUTRenderTarget(GLUTSize, bUseVolumeTextureLUT, bUseComputePass, bUseFloatOutput)),
			TEXT("CombineLUT"));
	}

	check(OutputTexture);

	// For a 3D texture, the viewport is 16x16 (per slice); for a 2D texture, it's unwrapped to 256x16.
	const FIntPoint OutputViewSize(bUseVolumeTextureLUT ? GLUTSize : GLUTSize * GLUTSize, GLUTSize);

	FLUTBlenderShader::FPermutationDomain PermutationVector;
	PermutationVector.Set<FLUTBlenderShader::FBlendCount>(LocalCount);

	const float DefaultTemperature = 6500;
	const float DefaultTint = 0;

	if (bUseComputePass)
	{
		FLUTBlenderCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLUTBlenderCS::FParameters>();
		GetCombineLUTParameters(PassParameters->CombineLUT, View, LocalTextures, LocalWeights, LocalCount);
		PassParameters->OutputExtentInverse = FVector2f(1.0f, 1.0f) / FVector2f(OutputViewSize);
		PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(OutputTexture);

		const bool ShouldSkipTemperature = FMath::IsNearlyEqual(PassParameters->CombineLUT.WhiteTemp, DefaultTemperature) && FMath::IsNearlyEqual(PassParameters->CombineLUT.WhiteTint, DefaultTint);

		PermutationVector.Set<FLUTBlenderShader::FSkipTemperature>(ShouldSkipTemperature);
		TShaderMapRef<FLUTBlenderCS> ComputeShader(View.ShaderMap, PermutationVector);

		const uint32 GroupSizeXY = FMath::DivideAndRoundUp(OutputViewSize.X, FLUTBlenderCS::GroupSize);
		const uint32 GroupSizeZ = bUseVolumeTextureLUT ? GroupSizeXY : 1;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CombineLUTs (CS)"),
			ComputeShader,
			PassParameters,
			FIntVector(GroupSizeXY, GroupSizeXY, GroupSizeZ));
	}
	else
	{
		FLUTBlenderPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLUTBlenderPS::FParameters>();
		GetCombineLUTParameters(PassParameters->CombineLUT, View, LocalTextures, LocalWeights, LocalCount);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);

		const bool ShouldSkipTemperature = FMath::IsNearlyEqual(PassParameters->CombineLUT.WhiteTemp, DefaultTemperature) && FMath::IsNearlyEqual(PassParameters->CombineLUT.WhiteTint, DefaultTint);

		PermutationVector.Set<FLUTBlenderShader::FSkipTemperature>(ShouldSkipTemperature);
		TShaderMapRef<FLUTBlenderPS> PixelShader(View.ShaderMap, PermutationVector);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("CombineLUTS (PS)"),
			PassParameters,
			ERDGPassFlags::Raster,
			[&View, PixelShader, PassParameters, bUseVolumeTextureLUT] (FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			if (bUseVolumeTextureLUT)
			{
				const FVolumeBounds VolumeBounds(GLUTSize);

				TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
				TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);

				GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				VertexShader->SetParameters(RHICmdList, VolumeBounds, FIntVector(VolumeBounds.MaxX - VolumeBounds.MinX));

				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

				RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
			}
			else
			{
				TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);

				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

				DrawRectangle(
					RHICmdList,
					0, 0,										// XY
					GLUTSize * GLUTSize, GLUTSize,				// SizeXY
					0, 0,										// UV
					GLUTSize * GLUTSize, GLUTSize,				// SizeUV
					FIntPoint(GLUTSize * GLUTSize, GLUTSize),	// TargetSize
					FIntPoint(GLUTSize * GLUTSize, GLUTSize),	// TextureSize
					VertexShader,
					EDRF_UseTriangleOptimization);
			}
		});
	}

	return OutputTexture;
}