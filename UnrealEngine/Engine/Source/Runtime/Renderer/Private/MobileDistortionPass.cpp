// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MobileDistortionPass.cpp - Mobile specific rendering of primtives with refraction
=============================================================================*/

#include "MobileDistortionPass.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "TranslucentRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"
#include "DistortionRendering.h"

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FMobileDistortionPassUniformParameters, RENDERER_API)
	SHADER_PARAMETER_STRUCT(FMobileSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER(FVector4f, DistortionParams)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FMobileDistortionPassUniformParameters, "MobileDistortionPass", SceneTextures);

bool IsMobileDistortionActive(const FViewInfo& View)
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DisableDistortion"));
	int32 DisableDistortion = CVar->GetInt();

	// Distortion on mobile requires SceneDepth information in SceneColor.A channel
	const EMobileHDRMode HDRMode = GetMobileHDRMode();
	const bool bVisiblePrims = View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].HasAnyDraw();

	return
		HDRMode == EMobileHDRMode::EnabledFloat16 &&
		View.Family->EngineShowFlags.Translucency &&
		bVisiblePrims &&
		FSceneRenderer::GetRefractionQuality(*View.Family) > 0 &&
		DisableDistortion == 0;
}

BEGIN_SHADER_PARAMETER_STRUCT(FMobileDistortionPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileDistortionPassUniformParameters, Pass)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

TRDGUniformBufferRef<FMobileDistortionPassUniformParameters> CreateMobileDistortionPassUniformBuffer(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	auto* Parameters = GraphBuilder.AllocParameters<FMobileDistortionPassUniformParameters>();

	EMobileSceneTextureSetupMode SetupMode = EMobileSceneTextureSetupMode::None;
	if (View.bCustomDepthStencilValid)
	{
		SetupMode |= EMobileSceneTextureSetupMode::CustomDepth;
	}

	if (MobileRequiresSceneDepthAux(View.GetShaderPlatform()))
	{
		SetupMode |= EMobileSceneTextureSetupMode::SceneDepthAux;
	}
	else
	{
		SetupMode |= EMobileSceneTextureSetupMode::SceneDepth;
	}

	SetupMobileSceneTextureUniformParameters(GraphBuilder, View.GetSceneTexturesChecked(), SetupMode, Parameters->SceneTextures);

	SetupDistortionParams(Parameters->DistortionParams, View);

	return GraphBuilder.CreateUniformBuffer(Parameters);
}

FMobileDistortionAccumulateOutputs AddMobileDistortionAccumulatePass(FRDGBuilder& GraphBuilder, FScene* Scene, const FViewInfo& View, const FMobileDistortionAccumulateInputs& Inputs)
{
	FRDGTextureDesc DistortionAccumulateDesc = FRDGTextureDesc::Create2D(Inputs.SceneColor.Texture->Desc.Extent, PF_B8G8R8A8, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_RenderTargetable);

	FScreenPassRenderTarget DistortionAccumulateOutput = FScreenPassRenderTarget(GraphBuilder.CreateTexture(DistortionAccumulateDesc, TEXT("DistortionAccumulatePass")), Inputs.SceneColor.ViewRect, ERenderTargetLoadAction::EClear);

	FMobileDistortionPassParameters* PassParameters = GraphBuilder.AllocParameters<FMobileDistortionPassParameters>();
	PassParameters->View = View.GetShaderParameters();
	PassParameters->Pass = CreateMobileDistortionPassUniformBuffer(GraphBuilder, View);
	PassParameters->RenderTargets[0] = DistortionAccumulateOutput.GetRenderTargetBinding();

	const FScreenPassTextureViewport SceneColorViewport(Inputs.SceneColor);

	const_cast<FViewInfo&>(View).ParallelMeshDrawCommandPasses[EMeshPass::Distortion].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("DistortionAccumulate %dx%d", SceneColorViewport.Rect.Width(), SceneColorViewport.Rect.Height()),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, SceneColorViewport, PassParameters](FRHICommandList& RHICmdList)
	{
		RHICmdList.SetViewport(SceneColorViewport.Rect.Min.X, SceneColorViewport.Rect.Min.Y, 0.0f, SceneColorViewport.Rect.Max.X, SceneColorViewport.Rect.Max.Y, 1.0f);
		View.ParallelMeshDrawCommandPasses[EMeshPass::Distortion].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
	});

	FMobileDistortionAccumulateOutputs Outputs;

	Outputs.DistortionAccumulate = DistortionAccumulateOutput;

	return MoveTemp(Outputs);
}

class FMobileDistortionMergePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FMobileDistortionMergePS);
	SHADER_USE_PARAMETER_STRUCT(FMobileDistortionMergePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorTextureSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DistortionAccumulateTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DistortionAccumulateSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileDistortionMergePS, "/Engine/Private/DistortApplyScreenPS.usf", "Merge_Mobile", SF_Pixel);

FScreenPassTexture AddMobileDistortionMergePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FMobileDistortionMergeInputs& Inputs)
{
	FRDGTextureDesc DistortionMergeDesc = FRDGTextureDesc::Create2D(Inputs.DistortionAccumulate.Texture->Desc.Extent, Inputs.SceneColor.Texture->Desc.Format, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable);

	FScreenPassRenderTarget DistortionMergeOutput = FScreenPassRenderTarget(GraphBuilder.CreateTexture(DistortionMergeDesc, TEXT("DistortionMergePass")), Inputs.DistortionAccumulate.ViewRect, ERenderTargetLoadAction::EClear);

	TShaderMapRef<FMobileDistortionMergePS> PixelShader(View.ShaderMap);

	FMobileDistortionMergePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMobileDistortionMergePS::FParameters>();
	PassParameters->RenderTargets[0] = DistortionMergeOutput.GetRenderTargetBinding();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->SceneColorTexture = Inputs.SceneColor.Texture;
	PassParameters->SceneColorTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->DistortionAccumulateTexture = Inputs.DistortionAccumulate.Texture;
	PassParameters->DistortionAccumulateSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const FScreenPassTextureViewport InputViewport(Inputs.SceneColor);
	const FScreenPassTextureViewport OutputViewport(DistortionMergeOutput);

	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("DistortionMerge"), View, OutputViewport, InputViewport, PixelShader, PassParameters);

	return MoveTemp(DistortionMergeOutput);
}
