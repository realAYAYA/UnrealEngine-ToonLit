// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLocalExposure.cpp: Post processing local exposure implementation.
=============================================================================*/

#include "PostProcess/PostProcessLocalExposure.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "PostProcess/PostProcessWeightedSampleSum.h"
#include "ShaderCompilerCore.h"

namespace
{

class FSetupLogLuminanceCS : public FGlobalShader
{
public:
	// Changing these numbers requires LocalExposure.usf to be recompiled
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	DECLARE_GLOBAL_SHADER(FSetupLogLuminanceCS);
	SHADER_USE_PARAMETER_STRUCT(FSetupLogLuminanceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutputFloat)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSetupLogLuminanceCS, "/Engine/Private/PostProcessLocalExposure.usf", "SetupLogLuminanceCS", SF_Compute);

class FApplyLocalExposureCS : public FGlobalShader
{
public:
	// Changing these numbers requires LocalExposure.usf to be recompiled
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	DECLARE_GLOBAL_SHADER(FApplyLocalExposureCS);
	SHADER_USE_PARAMETER_STRUCT(FApplyLocalExposureCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputFloat4)

		SHADER_PARAMETER_STRUCT(FEyeAdaptationParameters, EyeAdaptation)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EyeAdaptationTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, LumBilateralGrid)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredLogLum)

		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}
};

IMPLEMENT_GLOBAL_SHADER(FApplyLocalExposureCS, "/Engine/Private/PostProcessLocalExposure.usf", "ApplyLocalExposureCS", SF_Compute);

} //! namespace

FRDGTextureRef AddLocalExposureBlurredLogLuminancePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FScreenPassTexture InputTexture)
{
	check(InputTexture.IsValid());

	RDG_EVENT_SCOPE(GraphBuilder, "LocalExposure - Blurred Luminance");

	FRDGTextureRef GaussianLumSetupTexture;

	// Copy log luminance to temporary texture
	{
		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(
			InputTexture.ViewRect.Size(),
			PF_R16F,
			FClearValueBinding::None,
			TexCreate_UAV | TexCreate_ShaderResource);

		GaussianLumSetupTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("GaussianLumSetupTexture"));

		auto* PassParameters = GraphBuilder.AllocParameters<FSetupLogLuminanceCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->EyeAdaptation = EyeAdaptationParameters;
		PassParameters->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(InputTexture));
		PassParameters->InputTexture = InputTexture.Texture;
		PassParameters->OutputFloat = GraphBuilder.CreateUAV(GaussianLumSetupTexture);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SetupLogLuminance %dx%d", GaussianLumSetupTexture->Desc.Extent.X, GaussianLumSetupTexture->Desc.Extent.Y),
			ERDGPassFlags::Compute,
			View.ShaderMap->GetShader<FSetupLogLuminanceCS>(),
			PassParameters,
			FComputeShaderUtils::GetGroupCount(GaussianLumSetupTexture->Desc.Extent, FIntPoint(FSetupLogLuminanceCS::ThreadGroupSizeX, FSetupLogLuminanceCS::ThreadGroupSizeY)));
	}

	FRDGTextureRef GaussianTexture;

	{
		FGaussianBlurInputs GaussianBlurInputs;
		GaussianBlurInputs.NameX = TEXT("LocalExposureGaussianX");
		GaussianBlurInputs.NameY = TEXT("LocalExposureGaussianY");
		GaussianBlurInputs.Filter = FScreenPassTexture(GaussianLumSetupTexture, InputTexture.ViewRect);
		GaussianBlurInputs.TintColor = FLinearColor::White;
		GaussianBlurInputs.CrossCenterWeight = FVector2f::ZeroVector;
		GaussianBlurInputs.KernelSizePercent = View.FinalPostProcessSettings.LocalExposureBlurredLuminanceKernelSizePercent;
		GaussianBlurInputs.UseMirrorAddressMode = true;

		GaussianTexture = AddGaussianBlurPass(GraphBuilder, View, GaussianBlurInputs).Texture;
	}

	return GaussianTexture;
}

void AddApplyLocalExposurePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FRDGTextureRef EyeAdaptationTexture,
	FRDGTextureRef LocalExposureTexture,
	FRDGTextureRef BlurredLogLuminanceTexture,
	FScreenPassTexture Input,
	FScreenPassTexture Output,
	ERDGPassFlags PassFlags)
{
	check(Input.IsValid() && Output.IsValid());
	check(PassFlags == ERDGPassFlags::Compute || PassFlags == ERDGPassFlags::AsyncCompute);

	RDG_EVENT_SCOPE(GraphBuilder, "LocalExposure - Apply");

	auto* PassParameters = GraphBuilder.AllocParameters<FApplyLocalExposureCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->Input = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Input));
	PassParameters->Output = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Output));

	PassParameters->InputTexture = Input.Texture;
	PassParameters->OutputFloat4 = GraphBuilder.CreateUAV(Output.Texture);

	PassParameters->EyeAdaptation = EyeAdaptationParameters;
	PassParameters->EyeAdaptationTexture = EyeAdaptationTexture;
	PassParameters->LumBilateralGrid = LocalExposureTexture;
	PassParameters->BlurredLogLum = BlurredLogLuminanceTexture;
	PassParameters->TextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ApplyLocalExposure %dx%d", Output.ViewRect.Width(), Output.ViewRect.Height()),
		PassFlags,
		View.ShaderMap->GetShader<FApplyLocalExposureCS>(),
		PassParameters,
		FComputeShaderUtils::GetGroupCount(Output.ViewRect.Size(), FIntPoint(FApplyLocalExposureCS::ThreadGroupSizeX, FApplyLocalExposureCS::ThreadGroupSizeY)));
}