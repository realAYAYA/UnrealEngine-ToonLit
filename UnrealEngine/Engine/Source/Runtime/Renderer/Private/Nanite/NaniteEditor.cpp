// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteEditor.h"

#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "Rendering/NaniteStreamingManager.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "SystemTextures.h"
#include "Nanite/NaniteMaterialsSceneExtension.h"

DEFINE_GPU_STAT(NaniteEditor);

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteSelectionOutlineParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER(FVector2f, OutputToInputScale)
	SHADER_PARAMETER(FVector2f, OutputToInputBias)
	SHADER_PARAMETER(uint32, MaxVisibleClusters)

	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVisibleCluster>, VisibleClustersSWHW)
	SHADER_PARAMETER(FIntVector4, PageConstants)
	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ClusterPageData )

	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint2>, VisBuffer64)

	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, MaterialHitProxyTable)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FEmitHitProxyIdPS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitHitProxyIdPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitHitProxyIdPS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)

		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ClusterPageData )

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)

		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, MaterialHitProxyTable)
	
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FEmitHitProxyIdPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "EmitHitProxyIdPS", SF_Pixel);

class FEmitEditorSelectionDepthPS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FEmitEditorSelectionDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitEditorSelectionDepthPS, FNaniteGlobalShader);

	using FParameters = FNaniteSelectionOutlineParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}
};
IMPLEMENT_GLOBAL_SHADER(FEmitEditorSelectionDepthPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "EmitEditorSelectionDepthPS", SF_Pixel);

namespace Nanite
{

void DrawHitProxies(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View, 
	const FRasterResults& RasterResults,
	FRDGTextureRef HitProxyTexture,
	FRDGTextureRef HitProxyDepthTexture
	)
{
#if WITH_EDITOR
	LLM_SCOPE_BYTAG(Nanite);

	RDG_EVENT_SCOPE(GraphBuilder, "NaniteHitProxyPass");
	RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteEditor);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FRDGTextureRef VisBuffer64 = RasterResults.VisBuffer64 ? RasterResults.VisBuffer64 : SystemTextures.Black;

	FRDGBufferRef VisibleClustersSWHW = RasterResults.VisibleClustersSWHW;

	{
		auto& MaterialsExtension = Scene.GetExtension<Nanite::FMaterialsSceneExtension>();
		auto* PassParameters = GraphBuilder.AllocParameters<FEmitHitProxyIdPS::FParameters>();

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
		PassParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(VisibleClustersSWHW);
		PassParameters->PageConstants = RasterResults.PageConstants;
		PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		PassParameters->VisBuffer64 = VisBuffer64;
		PassParameters->MaterialHitProxyTable = GraphBuilder.CreateSRV(MaterialsExtension.CreateHitProxyIDBuffer(GraphBuilder));

		PassParameters->RenderTargets[0]			= FRenderTargetBinding(HitProxyTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->RenderTargets.DepthStencil	= FDepthStencilBinding(HitProxyDepthTexture, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

		auto PixelShader = View.ShaderMap->GetShader<FEmitHitProxyIdPS>();

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			View.ShaderMap,
			RDG_EVENT_NAME("Nanite::EmitHitProxyId"),
			PixelShader,
			PassParameters,
			View.ViewRect,
			TStaticBlendState<>::GetRHI(),
			TStaticRasterizerState<>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI()
			);
	}
#endif
}

#if WITH_EDITOR

using FInstanceDrawList = TArray<FInstanceDraw, SceneRenderingAllocator>;

static void GetEditorSelectionVisBuffer(
	FRDGBuilder& GraphBuilder,
	FScene& Scene,
	const FViewInfo& SceneView,
	const FViewInfo& EditorView,
	FSceneUniformBuffer &SceneUniformBuffer,
	const FRasterResults& NaniteRasterResults,
	const FInstanceDrawList& DrawList,
	FNaniteSelectionOutlineParameters& OutParameters
)
{
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
	const FIntPoint RasterTextureSize = EditorView.ViewRect.Size();
	const FIntRect RasterViewRect = FIntRect(FIntPoint(0, 0), RasterTextureSize);

	Nanite::FSharedContext SharedContext{};
	SharedContext.FeatureLevel = Scene.GetFeatureLevel();
	SharedContext.ShaderMap = GetGlobalShaderMap(SharedContext.FeatureLevel);
	SharedContext.Pipeline = Nanite::EPipeline::Primary;

	Nanite::FRasterContext RasterContext = Nanite::InitRasterContext(
		GraphBuilder,
		SharedContext,
		*(const FViewFamilyInfo*)SceneView.Family,
		RasterTextureSize,
		RasterViewRect,
		Nanite::EOutputBufferMode::VisBuffer,
		true // bClearTarget
	);

	// Rasterize the view
	{
		Nanite::FConfiguration CullingConfig = { 0 };
		CullingConfig.bUpdateStreaming = true;

		Nanite::FPackedViewParams NaniteViewParams;
		NaniteViewParams.ViewMatrices = EditorView.ViewMatrices;
		NaniteViewParams.PrevViewMatrices = EditorView.PrevViewInfo.ViewMatrices;
		NaniteViewParams.ViewRect = RasterViewRect;
		NaniteViewParams.RasterContextSize = RasterTextureSize;
		NaniteViewParams.HZBTestViewRect = EditorView.PrevViewInfo.ViewRect;
		NaniteViewParams.GlobalClippingPlane = EditorView.GlobalClippingPlane;

		const Nanite::FPackedView NaniteView = Nanite::CreatePackedView(NaniteViewParams);

		auto NaniteRenderer = Nanite::IRenderer::Create(
			GraphBuilder,
			Scene,
			EditorView,
			SceneUniformBuffer,
			SharedContext,
			RasterContext,
			CullingConfig,
			RasterViewRect,
			/* PrevHZB = */ nullptr
		);

		NaniteRenderer->DrawGeometry(
			Scene.NaniteRasterPipelines[ENaniteMeshPass::BasePass],
			NaniteRasterResults.VisibilityQuery,
			*Nanite::FPackedViewArray::Create(GraphBuilder, NaniteView),
			DrawList
		);

		Nanite::FRasterResults RasterResults;
		NaniteRenderer->ExtractResults( RasterResults );

		const FScreenTransform OutputToInputTransform = FScreenTransform::ChangeRectFromTo(EditorView.ViewRect, RasterViewRect);

		OutParameters.VisBuffer64			= RasterResults.VisBuffer64 ? RasterResults.VisBuffer64 : SystemTextures.Black;
		OutParameters.VisibleClustersSWHW	= GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
		OutParameters.OutputToInputScale	= OutputToInputTransform.Scale;
		OutParameters.OutputToInputBias		= OutputToInputTransform.Bias;
	}
}

static void AddEditorSelectionDepthPass(
	FRDGBuilder& GraphBuilder,
	FRDGEventName&& EventName,
	FRDGTextureRef DepthTarget,
	FScene& Scene,
	const FViewInfo& SceneView,
	const FViewInfo& EditorView,
	FSceneUniformBuffer &SceneUniformBuffer,
	const FRasterResults& NaniteRasterResults,
	const FInstanceDrawList& DrawList,
	int32 StencilRefValue
)
{
	LLM_SCOPE_BYTAG(Nanite);
	RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteEditor);
	FRDGEventScopeGuard RDGEventScope(GraphBuilder, MoveTemp(EventName));

	auto& MaterialsExtension = Scene.GetExtension<Nanite::FMaterialsSceneExtension>();
	auto PassParameters = GraphBuilder.AllocParameters<FNaniteSelectionOutlineParameters>();

	GetEditorSelectionVisBuffer(
		GraphBuilder,
		Scene,
		SceneView,
		EditorView,
		SceneUniformBuffer,
		NaniteRasterResults,
		DrawList,
		*PassParameters);

	PassParameters->View					= EditorView.ViewUniformBuffer;
	PassParameters->Scene					= SceneView.GetSceneUniforms().GetBuffer(GraphBuilder);
	PassParameters->MaxVisibleClusters		= Nanite::FGlobalResources::GetMaxVisibleClusters();
	PassParameters->PageConstants			= NaniteRasterResults.PageConstants;
	PassParameters->ClusterPageData			= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	PassParameters->MaterialHitProxyTable	= GraphBuilder.CreateSRV(MaterialsExtension.CreateHitProxyIDBuffer(GraphBuilder));
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
		DepthTarget,
		ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad,
		FExclusiveDepthStencil::DepthWrite_StencilWrite);

	auto PixelShader = SceneView.ShaderMap->GetShader<FEmitEditorSelectionDepthPS>();

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		SceneView.ShaderMap,
		RDG_EVENT_NAME("EditorSelectionDepth"),
		PixelShader,
		PassParameters,
		EditorView.ViewRect,
		TStaticBlendState<>::GetRHI(),
		TStaticRasterizerState<>::GetRHI(),
		TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(),
		StencilRefValue
	);
}

void DrawEditorSelection(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef DepthTarget,
	FScene& Scene,
	const FViewInfo& SceneView,
	const FViewInfo& EditorView,
	FSceneUniformBuffer &SceneUniformBuffer,
	const FRasterResults* NaniteRasterResults
)
{
	if (NaniteRasterResults == nullptr || SceneView.EditorSelectedInstancesNanite.Num() == 0)
	{
		return;
	}

	AddEditorSelectionDepthPass(
		GraphBuilder,
		RDG_EVENT_NAME("NaniteEditorSelection"),
		DepthTarget,
		Scene,
		SceneView,
		EditorView,
		SceneUniformBuffer,
		*NaniteRasterResults,
		SceneView.EditorSelectedInstancesNanite,
		3
	);
}

void DrawEditorVisualizeLevelInstance(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef DepthTarget,
	FScene& Scene,
	const FViewInfo& SceneView,
	const FViewInfo& EditorView,
	FSceneUniformBuffer &SceneUniformBuffer,
	const FRasterResults* NaniteRasterResults
)
{
	if (NaniteRasterResults == nullptr || SceneView.EditorVisualizeLevelInstancesNanite.Num() == 0)
	{
		return;
	}

	AddEditorSelectionDepthPass(
		GraphBuilder,
		RDG_EVENT_NAME("NaniteVisualizeLevelInstances"),
		DepthTarget,
		Scene,
		SceneView,
		EditorView,
		SceneUniformBuffer,
		*NaniteRasterResults,
		SceneView.EditorVisualizeLevelInstancesNanite,
		2
	);
}

#endif // WITH_EDITOR

} // namespace Nanite
