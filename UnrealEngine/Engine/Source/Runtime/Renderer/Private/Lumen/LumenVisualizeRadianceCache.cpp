// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenVisualizeRadianceCache.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "LumenRadianceCache.h"
#include "DeferredShadingRenderer.h"
#include "LumenScreenProbeGather.h"
#include "LumenTranslucencyVolumeLighting.h"

int32 GLumenRadianceCacheVisualize = 0;
FAutoConsoleVariableRef CVarLumenRadianceCacheVisualize(
	TEXT("r.Lumen.RadianceCache.Visualize"),
	GLumenRadianceCacheVisualize,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GLumenVisualizeTranslucencyVolumeRadianceCache = 0;
FAutoConsoleVariableRef CVarLumenRadianceCacheVisualizeTranslucencyVolume(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.Visualize"),
	GLumenVisualizeTranslucencyVolumeRadianceCache,
	TEXT(""),
	ECVF_RenderThreadSafe
);

float GLumenRadianceCacheVisualizeRadiusScale = .05f;
FAutoConsoleVariableRef CVarLumenRadianceCacheVisualizeRadiusScale(
	TEXT("r.Lumen.RadianceCache.VisualizeRadiusScale"),
	GLumenRadianceCacheVisualizeRadiusScale,
	TEXT("Scales the size of the spheres used to visualize radiance cache samples."),
	ECVF_RenderThreadSafe
);

int32 GLumenRadianceCacheVisualizeClipmapIndex = -1;
FAutoConsoleVariableRef CVarLumenRadianceCacheVisualizeClipmapIndex(
	TEXT("r.Lumen.RadianceCache.VisualizeClipmapIndex"),
	GLumenRadianceCacheVisualizeClipmapIndex,
	TEXT("Selects which radiance cache clipmap should be visualized. -1 visualizes all clipmaps at once."),
	ECVF_RenderThreadSafe
);

int32 GLumenRadianceCacheVisualizeProbeRadius = 0;
FAutoConsoleVariableRef CVarLumenRadianceCacheVisualizeProbeRadius(
	TEXT("r.Lumen.RadianceCache.VisualizeProbeRadius"),
	GLumenRadianceCacheVisualizeProbeRadius,
	TEXT("Whether to visualize radiance cache probe radius"),
	ECVF_RenderThreadSafe
);

BEGIN_SHADER_PARAMETER_STRUCT(FVisualizeRadianceCacheCommonParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
	SHADER_PARAMETER(FVector3f, ProbeCoordToWorldCenterBias)
	SHADER_PARAMETER(float, ProbeCoordToWorldCenterScale)
	SHADER_PARAMETER(float, VisualizeProbeRadiusScale)
	SHADER_PARAMETER(uint32, ProbeClipmapIndex)
END_SHADER_PARAMETER_STRUCT()

class FVisualizeRadianceCacheVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeRadianceCacheVS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeRadianceCacheVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVisualizeRadianceCacheCommonParameters, VisualizeCommonParameters)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeRadianceCacheVS,"/Engine/Private/Lumen/LumenVisualizeRadianceCache.usf", "VisualizeRadianceCacheVS", SF_Vertex);

class FVisualizeRadianceCachePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeRadianceCachePS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeRadianceCachePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVisualizeRadianceCacheCommonParameters, VisualizeCommonParameters)
	END_SHADER_PARAMETER_STRUCT()

public:

	class FVisualizeIrradiance : SHADER_PERMUTATION_BOOL("VISUALIZE_IRRADIANCE");
	using FPermutationDomain = TShaderPermutationDomain<FVisualizeIrradiance>;
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeRadianceCachePS, "/Engine/Private/Lumen/LumenVisualizeRadianceCache.usf", "VisualizeRadianceCachePS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FVisualizeRadianceCacheParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FVisualizeRadianceCacheVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FVisualizeRadianceCachePS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

LumenRadianceCache::FRadianceCacheInputs GetFinalGatherRadianceCacheInputs(const FViewInfo& View)
{
	if (GLumenVisualizeTranslucencyVolumeRadianceCache)
	{
		return LumenTranslucencyVolumeRadianceCache::SetupRadianceCacheInputs(View);
	}
	else
	{
		if (GLumenIrradianceFieldGather)
		{
			return LumenIrradianceFieldGather::SetupRadianceCacheInputs();
		}
		else
		{
			return LumenScreenProbeGatherRadianceCache::SetupRadianceCacheInputs(View);
		}
	}
}

extern int32 GLumenTranslucencyVolume;
extern int32 GLumenVisualizeTranslucencyVolumeRadianceCache;

void FDeferredShadingSceneRenderer::RenderLumenRadianceCacheVisualization(FRDGBuilder& GraphBuilder, const FMinimalSceneTextures& SceneTextures)
{
	const FViewInfo& View = Views[0];
	const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
	const bool bAnyLumenActive = ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen;

	if (Views.Num() == 1
		&& View.ViewState
		&& bAnyLumenActive
		&& (LumenScreenProbeGather::UseRadianceCache(Views[0]) || (GLumenVisualizeTranslucencyVolumeRadianceCache && GLumenTranslucencyVolume))
		&& GLumenRadianceCacheVisualize != 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "VisualizeLumenRadianceCache");

		const FRadianceCacheState& RadianceCacheState = GLumenVisualizeTranslucencyVolumeRadianceCache != 0 ? Views[0].ViewState->Lumen.TranslucencyVolumeRadianceCacheState : Views[0].ViewState->Lumen.RadianceCacheState;

		FRDGTextureRef SceneColor = SceneTextures.Color.Resolve;
		FRDGTextureRef SceneDepth = SceneTextures.Depth.Resolve;

		const LumenRadianceCache::FRadianceCacheInputs RadianceCacheInputs = GetFinalGatherRadianceCacheInputs(View);

		const int32 VisualizationClipmapIndex = FMath::Clamp(GLumenRadianceCacheVisualizeClipmapIndex, -1, RadianceCacheState.Clipmaps.Num() - 1);
		for (int32 ClipmapIndex = 0; ClipmapIndex < RadianceCacheState.Clipmaps.Num(); ++ClipmapIndex)
		{
			if (VisualizationClipmapIndex != -1 && VisualizationClipmapIndex != ClipmapIndex)
			{
				continue;
			}

			const FRadianceCacheClipmap& Clipmap = RadianceCacheState.Clipmaps[ClipmapIndex];

			FVisualizeRadianceCacheCommonParameters VisualizeCommonParameters;
			LumenRadianceCache::GetInterpolationParameters(View, GraphBuilder, RadianceCacheState, RadianceCacheInputs, VisualizeCommonParameters.RadianceCacheParameters);
			VisualizeCommonParameters.VisualizeProbeRadiusScale = GLumenRadianceCacheVisualizeRadiusScale;
			VisualizeCommonParameters.ProbeClipmapIndex = ClipmapIndex;
			VisualizeCommonParameters.ProbeCoordToWorldCenterBias = (FVector3f)Clipmap.ProbeCoordToWorldCenterBias;
			VisualizeCommonParameters.ProbeCoordToWorldCenterScale = Clipmap.ProbeCoordToWorldCenterScale;

			FVisualizeRadianceCacheParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeRadianceCacheParameters>();
			PassParameters->VS.VisualizeCommonParameters = VisualizeCommonParameters;
			PassParameters->PS.VisualizeCommonParameters = VisualizeCommonParameters;
			PassParameters->VS.View = GetShaderBinding(View.ViewUniformBuffer);
			PassParameters->PS.View = GetShaderBinding(View.ViewUniformBuffer);

			PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
				SceneDepth,
				ERenderTargetLoadAction::ENoAction,
				ERenderTargetLoadAction::ELoad,
				FExclusiveDepthStencil::DepthWrite_StencilWrite);
			PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColor, ERenderTargetLoadAction::ELoad);

			const int32 NumInstancesPerClipmap = RadianceCacheInputs.RadianceProbeClipmapResolution * RadianceCacheInputs.RadianceProbeClipmapResolution * RadianceCacheInputs.RadianceProbeClipmapResolution;
			const bool bCalculateIrradiance = RadianceCacheInputs.CalculateIrradiance != 0;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Visualize Radiance Cache Clipmap:%d", ClipmapIndex),
				PassParameters,
				ERDGPassFlags::Raster,
				[PassParameters, &View, NumInstancesPerClipmap, bCalculateIrradiance](FRHICommandList& RHICmdList)
				{
					TShaderMapRef<FVisualizeRadianceCacheVS> VertexShader(View.ShaderMap);

					FVisualizeRadianceCachePS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FVisualizeRadianceCachePS::FVisualizeIrradiance>(bCalculateIrradiance);
					auto PixelShader = View.ShaderMap->GetShader<FVisualizeRadianceCachePS>(PermutationVector);

					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB>::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNear>::GetRHI();
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

					RHICmdList.SetStreamSource(0, NULL, 0);
					RHICmdList.DrawIndexedPrimitive(GCubeIndexBuffer.IndexBufferRHI, 0, 0, 8, 0, 2 * 6, NumInstancesPerClipmap);
				});
		}
	}
}
