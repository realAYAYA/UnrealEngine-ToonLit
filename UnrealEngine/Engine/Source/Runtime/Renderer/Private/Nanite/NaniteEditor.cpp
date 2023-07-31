// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteEditor.h"

#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "Rendering/NaniteStreamingManager.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"

DEFINE_GPU_STAT(NaniteEditor);

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

		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ClusterPageData )

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)

		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialHitProxyTable)
		
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FEmitHitProxyIdPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "EmitHitProxyIdPS", SF_Pixel);

class FEmitEditingLevelInstanceDepthPS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FEmitEditingLevelInstanceDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitEditingLevelInstanceDepthPS, FNaniteGlobalShader);

	using FParameters = FNaniteVisualizeLevelInstanceParameters;

	class FSearchBufferCountDim : SHADER_PERMUTATION_INT("EDITOR_LEVELINSTANCE_BUFFER_COUNT_LOG_2", 25);
	using FPermutationDomain = TShaderPermutationDomain<FSearchBufferCountDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		uint32 LevelInstanceBufferCount = 1u << (uint32)PermutationVector.Get<FSearchBufferCountDim>();
		OutEnvironment.SetDefine(TEXT("EDITOR_LEVELINSTANCE_BUFFER_COUNT"), LevelInstanceBufferCount);
	}
};
IMPLEMENT_GLOBAL_SHADER(FEmitEditingLevelInstanceDepthPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "EmitEditorLevelInstanceDepthPS", SF_Pixel);

class FEmitEditorSelectionDepthPS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FEmitEditorSelectionDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitEditorSelectionDepthPS, FNaniteGlobalShader);

	using FParameters = FNaniteSelectionOutlineParameters;

	class FSearchBufferCountDim : SHADER_PERMUTATION_INT("EDITOR_SELECTED_BUFFER_COUNT_LOG_2", 25);
	using FPermutationDomain = TShaderPermutationDomain<FSearchBufferCountDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		uint32 SelectedBufferCount = 1u << (uint32)PermutationVector.Get<FSearchBufferCountDim>();
		OutEnvironment.SetDefine(TEXT("EDITOR_SELECTED_BUFFER_COUNT"), SelectedBufferCount);
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
		auto* PassParameters = GraphBuilder.AllocParameters<FEmitHitProxyIdPS::FParameters>();

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(VisibleClustersSWHW);
		PassParameters->PageConstants = RasterResults.PageConstants;
		PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		PassParameters->VisBuffer64 = VisBuffer64;
		PassParameters->MaterialHitProxyTable = Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetHitProxyTableSRV();

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

void GetEditorSelectionPassParameters(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntRect ViewportRect,
	const FRasterResults* NaniteRasterResults,
	FNaniteSelectionOutlineParameters* OutPassParameters
	)
{
	if (!NaniteRasterResults)
	{
		return;
	}

	LLM_SCOPE_BYTAG(Nanite);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
	FRDGTextureRef VisBuffer64 = NaniteRasterResults->VisBuffer64 ? NaniteRasterResults->VisBuffer64 : SystemTextures.Black;
	FRDGBufferRef VisibleClustersSWHW = NaniteRasterResults->VisibleClustersSWHW;

	OutPassParameters->View						= View.ViewUniformBuffer;
	OutPassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(VisibleClustersSWHW);
	OutPassParameters->MaxVisibleClusters		= Nanite::FGlobalResources::GetMaxVisibleClusters();
	OutPassParameters->PageConstants			= NaniteRasterResults->PageConstants;
	OutPassParameters->ClusterPageData			= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	OutPassParameters->VisBuffer64				= VisBuffer64;
	OutPassParameters->MaterialHitProxyTable	= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetHitProxyTableSRV();
	OutPassParameters->OutputToInputScale		= FScreenTransform::ChangeRectFromTo(ViewportRect, View.ViewRect).Scale;
	OutPassParameters->OutputToInputBias		= FScreenTransform::ChangeRectFromTo(ViewportRect, View.ViewRect).Bias;
}

void DrawEditorSelection(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FIntRect ViewportRect,
	const FNaniteSelectionOutlineParameters& PassParameters
	)
{
	LLM_SCOPE_BYTAG(Nanite);

	if (View.EditorSelectedHitProxyIds.Num() == 0)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, NaniteEditorSelection);
	SCOPED_GPU_STAT(RHICmdList, NaniteEditor);

	uint32 SelectionCount = FMath::RoundUpToPowerOfTwo(View.EditorSelectedHitProxyIds.Num());
	uint32 SearchBufferCountDim = FMath::Min((uint32)FEmitEditorSelectionDepthPS::FSearchBufferCountDim::MaxValue, FMath::FloorLog2(SelectionCount));

	FEmitEditorSelectionDepthPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FEmitEditorSelectionDepthPS::FSearchBufferCountDim>(SearchBufferCountDim);

	auto PixelShader = View.ShaderMap->GetShader<FEmitEditorSelectionDepthPS>(PermutationVector.ToDimensionValueId());

	FPixelShaderUtils::DrawFullscreenPixelShader(
		RHICmdList,
		View.ShaderMap,
		PixelShader,
		PassParameters,
		ViewportRect,
		TStaticBlendState<>::GetRHI(),
		TStaticRasterizerState<>::GetRHI(),
		TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(),
		3
	);
}

void GetEditorVisualizeLevelInstancePassParameters(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntRect ViewportRect,
	const FRasterResults* NaniteRasterResults,
	FNaniteVisualizeLevelInstanceParameters* OutPassParameters
)
{
	if (!NaniteRasterResults)
	{
		return;
	}

	LLM_SCOPE_BYTAG(Nanite);
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FRDGTextureRef VisBuffer64 = NaniteRasterResults->VisBuffer64 ? NaniteRasterResults->VisBuffer64 : SystemTextures.Black;
	FRDGBufferRef VisibleClustersSWHW = NaniteRasterResults->VisibleClustersSWHW;

	OutPassParameters->View = View.ViewUniformBuffer;
	OutPassParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(VisibleClustersSWHW);
	OutPassParameters->MaxVisibleClusters = Nanite::FGlobalResources::GetMaxVisibleClusters();
	OutPassParameters->PageConstants = NaniteRasterResults->PageConstants;
	OutPassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	OutPassParameters->MaterialResolve = NaniteRasterResults->MaterialResolve;
	OutPassParameters->VisBuffer64 = VisBuffer64;
	OutPassParameters->MaterialHitProxyTable = Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetHitProxyTableSRV();
	OutPassParameters->OutputToInputScale = FScreenTransform::ChangeRectFromTo(ViewportRect, View.ViewRect).Scale;
	OutPassParameters->OutputToInputBias = FScreenTransform::ChangeRectFromTo(ViewportRect, View.ViewRect).Bias;
}

void DrawEditorVisualizeLevelInstance(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FIntRect ViewportRect,
	const FNaniteVisualizeLevelInstanceParameters& PassParameters
)
{
	LLM_SCOPE_BYTAG(Nanite);

	if (View.EditorVisualizeLevelInstanceIds.Num() == 0)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, NaniteEditorLevelInstance);
	SCOPED_GPU_STAT(RHICmdList, NaniteEditor);

	uint32 LevelInstancePrimCount = FMath::RoundUpToPowerOfTwo(View.EditorVisualizeLevelInstanceIds.Num());
	uint32 SearchBufferCountDim = FMath::Min((uint32)FEmitEditingLevelInstanceDepthPS::FSearchBufferCountDim::MaxValue, FMath::FloorLog2(LevelInstancePrimCount));

	FEmitEditingLevelInstanceDepthPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FEmitEditingLevelInstanceDepthPS::FSearchBufferCountDim>(SearchBufferCountDim);

	auto PixelShader = View.ShaderMap->GetShader<FEmitEditingLevelInstanceDepthPS>(PermutationVector.ToDimensionValueId());

	FPixelShaderUtils::DrawFullscreenPixelShader(
		RHICmdList,
		View.ShaderMap,
		PixelShader,
		PassParameters,
		ViewportRect,
		TStaticBlendState<>::GetRHI(),
		TStaticRasterizerState<>::GetRHI(),
		TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(),
		2
	);
}

#endif // WITH_EDITOR

} // namespace Nanite
