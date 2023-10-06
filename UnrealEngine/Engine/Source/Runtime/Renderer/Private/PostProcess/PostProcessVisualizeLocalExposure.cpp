// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcess/PostProcessVisualizeLocalExposure.h"
#include "PostProcess/PostProcessTonemap.h"
#include "PostProcess/PostProcessLocalExposure.h"
#include "UnrealEngine.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneRendering.h"

TAutoConsoleVariable<int> CVarLocalExposureVisualizeDebugMode(
	TEXT("r.LocalExposure.VisualizeDebugMode"),
	0,
	TEXT("When enabling Show->Visualize->Local Exposure is enabled, this flag controls which mode to use.\n")
	TEXT("    0: Local Exposure\n")
	TEXT("    1: Base Luminance\n")
	TEXT("    2: Detail Luminance\n")
	TEXT("    3: Valid Bilateral Grid Lookup\n"),
	ECVF_RenderThreadSafe);

class FVisualizeLocalExposurePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVisualizeLocalExposurePS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeLocalExposurePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
		SHADER_PARAMETER_STRUCT(FLocalExposureParameters, LocalExposure)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HDRSceneColorTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, LumBilateralGrid)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredLogLum)

		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)

		SHADER_PARAMETER(uint32, DebugMode)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeLocalExposurePS, "/Engine/Private/PostProcessVisualizeLocalExposure.usf", "MainPS", SF_Pixel);

FScreenPassTexture AddVisualizeLocalExposurePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FVisualizeLocalExposureInputs& Inputs)
{
	check(Inputs.SceneColor.IsValid());
	check(Inputs.HDRSceneColor.IsValid());
	check(Inputs.LumBilateralGridTexture);
	check(Inputs.BlurredLumTexture);
	check(Inputs.EyeAdaptationBuffer);
	check(Inputs.EyeAdaptationParameters);

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;

	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, Inputs.SceneColor, View.GetOverwriteLoadAction(), TEXT("VisualizeLocalExposure"));
	}

	const FScreenPassTextureViewport InputViewport(Inputs.SceneColor);
	const FScreenPassTextureViewport OutputViewport(Output);

	FRHISamplerState* BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const FPostProcessSettings& Settings = View.FinalPostProcessSettings;

	auto PassParameters = GraphBuilder.AllocParameters<FVisualizeLocalExposurePS::FParameters>();
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->EyeAdaptation = *Inputs.EyeAdaptationParameters;
	PassParameters->LocalExposure = *Inputs.LocalExposureParameters;
	PassParameters->Input = GetScreenPassTextureViewportParameters(InputViewport);
	PassParameters->Output = GetScreenPassTextureViewportParameters(OutputViewport);
	PassParameters->HDRSceneColorTexture = Inputs.HDRSceneColor.Texture;
	PassParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(Inputs.EyeAdaptationBuffer);
	PassParameters->LumBilateralGrid = Inputs.LumBilateralGridTexture;
	PassParameters->BlurredLogLum = Inputs.BlurredLumTexture;
	PassParameters->TextureSampler = BilinearClampSampler;
	PassParameters->DebugMode = CVarLocalExposureVisualizeDebugMode.GetValueOnRenderThread();

	TShaderMapRef<FVisualizeLocalExposurePS> PixelShader(View.ShaderMap);

	RDG_EVENT_SCOPE(GraphBuilder, "VisualizeLocalExposure");

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Visualizer"), View, OutputViewport, InputViewport, PixelShader, PassParameters);

	return MoveTemp(Output);
}