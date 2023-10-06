// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "BasePassRendering.h"
#include "PixelShaderUtils.h"
#include "MobileBasePassRendering.h"
#include "RendererPrivateUtils.h"
#include "GlobalRenderResources.h"

const int32 GLocalLightPrepassTileSizeX = 8;
class FLocalLightPrepassCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightPrepassCS);
public:
	SHADER_USE_PARAMETER_STRUCT(FLocalLightPrepassCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int32>, RWTileInfo)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER(FIntPoint, GroupSize)
	END_SHADER_PARAMETER_STRUCT()
 
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform) && MobileForwardEnablePrepassLocalLights(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GLocalLightPrepassTileSizeX);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightPrepassCS, "/Engine/Private/MobileLocalLightPrepass.usf", "MainCS", SF_Compute);

class FLocalLightPrepassVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightPrepassVS);
	SHADER_USE_PARAMETER_STRUCT(FLocalLightPrepassVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int32>, TileInfo)
		SHADER_PARAMETER(int32, LightGridPixelSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform) && MobileForwardEnablePrepassLocalLights(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightPrepassVS, "/Engine/Private/MobileLocalLightPrepass.usf", "MainVS", SF_Vertex);

class FLocalLightPrepassPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightPrepassPS);
	SHADER_USE_PARAMETER_STRUCT(FLocalLightPrepassPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
	END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform) && MobileForwardEnablePrepassLocalLights(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_FloatR11G11B10);
		OutEnvironment.SetRenderTargetOutputFormat(1, PF_A2B10G10R10);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightPrepassPS, "/Engine/Private/MobileLocalLightPrepass.usf", "Main", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FLocalLightPrepassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FLocalLightPrepassVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FLocalLightPrepassPS::FParameters, PS)
SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()


void FMobileSceneRenderer::RenderLocalLightPrepass(FRDGBuilder& GraphBuilder, FSceneTextures& SceneTextures)
{
	RDG_EVENT_SCOPE(GraphBuilder, "RenderLocalLightPrepass");
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderLocalLightPrepass);

	static const auto LightGridPixelSizeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Forward.LightGridPixelSize"));
	check(LightGridPixelSizeCVar != nullptr);
	int32 LightGridPixelSize = LightGridPixelSizeCVar->GetInt();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		bool bHasNoLocalLights = (!View.ForwardLightingResources.ForwardLightData) || (View.ForwardLightingResources.ForwardLightData->NumLocalLights == 0);
		if (!View.ShouldRenderView() || bHasNoLocalLights)
		{
			continue;
		}

		const FIntPoint GroupSize(
			FMath::DivideAndRoundUp(View.ViewRect.Size().X, LightGridPixelSize),
			FMath::DivideAndRoundUp(View.ViewRect.Size().Y, LightGridPixelSize));

		FRDGBufferRef TileInfoBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 2 * GroupSize.X * GroupSize.Y), TEXT("TileInfoBuffer"));
		FRDGBufferUAVRef TileInfoBufferUAV = GraphBuilder.CreateUAV(TileInfoBuffer, PF_R32_SINT);
		FRDGBufferSRVRef TileInfoBufferSRV = GraphBuilder.CreateSRV(TileInfoBuffer, PF_R32_SINT);

		{
			auto* PassParameters = GraphBuilder.AllocParameters<FLocalLightPrepassCS::FParameters>();
			PassParameters->RWTileInfo = TileInfoBufferUAV;
			PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
			PassParameters->GroupSize = GroupSize;
			auto ComputeShader = View.ShaderMap->GetShader<FLocalLightPrepassCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("RenderLocalLightPrepass_TiledInfoCS"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FIntVector(FMath::DivideAndRoundUp<uint32>(GroupSize.Y * GroupSize.X, GLocalLightPrepassTileSizeX), 1, 1));
		}


		{
			FLocalLightPrepassParameters* PassParameters = GraphBuilder.AllocParameters<FLocalLightPrepassParameters>();
			PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.MobileLocalLightTextureA, ERenderTargetLoadAction::EClear);;
			PassParameters->RenderTargets[1] = FRenderTargetBinding(SceneTextures.MobileLocalLightTextureB, ERenderTargetLoadAction::EClear);;
			PassParameters->SceneTextures = SceneTextures.GetSceneTextureShaderParameters(View.FeatureLevel);

			PassParameters->VS.View = GetShaderBinding(View.ViewUniformBuffer);
			PassParameters->VS.TileInfo = TileInfoBufferSRV;
			PassParameters->VS.LightGridPixelSize = LightGridPixelSize;
			PassParameters->PS.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
			PassParameters->PS.View = GetShaderBinding(View.ViewUniformBuffer);

			auto VertexShader = View.ShaderMap->GetShader<FLocalLightPrepassVS>();
			auto PixelShader = View.ShaderMap->GetShader<FLocalLightPrepassPS>();

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RenderLocalLightPrepass"),
				PassParameters,
				ERDGPassFlags::Raster,
				[PassParameters, VertexShader, PixelShader, &View, GroupSize](FRHICommandList& RHICmdList)
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GTileVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

					RHICmdList.SetStreamSource(0, GetOneTileQuadVertexBuffer(), 0);
					RHICmdList.DrawIndexedPrimitive(GetOneTileQuadIndexBuffer(),
						0,
						0,
						4,
						0,
						2,
						GroupSize.X * GroupSize.Y);
				});
		}
	}
}