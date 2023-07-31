// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthRendering.cpp: Depth rendering implementation.
=============================================================================*/

#include "DepthRendering.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "OneColorShader.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "MeshPassProcessor.inl"
#include "PixelShaderUtils.h"
#include "RenderGraphUtils.h"
#include "SceneRenderingUtils.h"
#include "DebugProbeRendering.h"

static TAutoConsoleVariable<int32> CVarParallelPrePass(
	TEXT("r.ParallelPrePass"),
	1,
	TEXT("Toggles parallel zprepass rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksPrePass(
	TEXT("r.RHICmdFlushRenderThreadTasksPrePass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the pre pass.  A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksPrePass is > 0 we will flush."));

static int32 GEarlyZSortMasked = 1;
static FAutoConsoleVariableRef CVarSortPrepassMasked(
	TEXT("r.EarlyZSortMasked"),
	GEarlyZSortMasked,
	TEXT("Sort EarlyZ masked draws to the end of the draw order.\n"),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarStencilLODDitherMode(
	TEXT("r.StencilLODMode"),
	2,
	TEXT("Specifies the dither LOD stencil mode.\n")
	TEXT(" 0: Graphics pass.\n")
	TEXT(" 1: Compute pass (on supported platforms).\n")
	TEXT(" 2: Compute async pass (on supported platforms)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarStencilForLODDither(
	TEXT("r.StencilForLODDither"),
	0,
	TEXT("Whether to use stencil tests in the prepass, and depth-equal tests in the base pass to implement LOD dithering.\n")
	TEXT("If disabled, LOD dithering will be done through clip() instructions in the prepass and base pass, which disables EarlyZ.\n")
	TEXT("Forces a full prepass when enabled."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

extern bool IsHMDHiddenAreaMaskActive();

FDepthPassInfo GetDepthPassInfo(const FScene* Scene)
{
	FDepthPassInfo Info;
	Info.EarlyZPassMode = Scene ? Scene->EarlyZPassMode : DDM_None;
	Info.bEarlyZPassMovable = Scene ? Scene->bEarlyZPassMovable : false;
	Info.bDitheredLODTransitionsUseStencil = CVarStencilForLODDither.GetValueOnAnyThread() > 0;
	Info.StencilDitherPassFlags = ERDGPassFlags::Raster;

	if (GRHISupportsDepthUAV && !IsHMDHiddenAreaMaskActive())
	{
		switch (CVarStencilLODDitherMode.GetValueOnAnyThread())
		{
		case 1:
			Info.StencilDitherPassFlags = ERDGPassFlags::Compute;
			break;
		case 2:
			Info.StencilDitherPassFlags = ERDGPassFlags::AsyncCompute;
			break;
		}
	}

	return Info;
}

BEGIN_SHADER_PARAMETER_STRUCT(FDepthPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

FDepthPassParameters* GetDepthPassParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef DepthTexture)
{
	auto* PassParameters = GraphBuilder.AllocParameters<FDepthPassParameters>();
	PassParameters->View = View.GetShaderParameters();
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(DepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	return PassParameters;
}

const TCHAR* GetDepthDrawingModeString(EDepthDrawingMode Mode)
{
	switch (Mode)
	{
	case DDM_None:
		return TEXT("DDM_None");
	case DDM_NonMaskedOnly:
		return TEXT("DDM_NonMaskedOnly");
	case DDM_AllOccluders:
		return TEXT("DDM_AllOccluders");
	case DDM_AllOpaque:
		return TEXT("DDM_AllOpaque");
	case DDM_AllOpaqueNoVelocity:
		return TEXT("DDM_AllOpaqueNoVelocity");
	default:
		check(0);
	}

	return TEXT("");
}

DECLARE_GPU_DRAWCALL_STAT(Prepass);

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyVS<true>,TEXT("/Engine/Private/PositionOnlyDepthVertexShader.usf"),TEXT("Main"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyVS<false>,TEXT("/Engine/Private/DepthOnlyVertexShader.usf"),TEXT("Main"),SF_Vertex);

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyPS,TEXT("/Engine/Private/DepthOnlyPixelShader.usf"),TEXT("Main"),SF_Pixel);

IMPLEMENT_SHADERPIPELINE_TYPE_VS(DepthNoPixelPipeline, TDepthOnlyVS<false>, true);
IMPLEMENT_SHADERPIPELINE_TYPE_VS(DepthPosOnlyNoPixelPipeline, TDepthOnlyVS<true>, true);
IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(DepthPipeline, TDepthOnlyVS<false>, FDepthOnlyPS, true);

static bool IsDepthPassWaitForTasksEnabled()
{
	return CVarRHICmdFlushRenderThreadTasksPrePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0;
}

static FORCEINLINE bool UseShaderPipelines(ERHIFeatureLevel::Type InFeatureLevel)
{
	static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderPipelines"));
	return RHISupportsShaderPipelines(GShaderPlatformForFeatureLevel[InFeatureLevel]) && CVar && CVar->GetValueOnAnyThread() != 0;
}

template <bool bPositionOnly>
bool GetDepthPassShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	bool bMaterialUsesPixelDepthOffset,
	TShaderRef<TDepthOnlyVS<bPositionOnly>>& VertexShader,
	TShaderRef<FDepthOnlyPS>& PixelShader,
	FShaderPipelineRef& ShaderPipeline)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<TDepthOnlyVS<bPositionOnly>>();

	if (bPositionOnly)
	{
		ShaderTypes.PipelineType = &DepthPosOnlyNoPixelPipeline;
		/*ShaderPipeline = UseShaderPipelines(FeatureLevel) ? Material.GetShaderPipeline(&DepthPosOnlyNoPixelPipeline, VertexFactoryType) : FShaderPipelineRef();
		VertexShader = ShaderPipeline.IsValid()
			? ShaderPipeline.GetShader<TDepthOnlyVS<bPositionOnly> >()
			: Material.GetShader<TDepthOnlyVS<bPositionOnly> >(VertexFactoryType, 0, false);
		return VertexShader.IsValid();*/
	}
	else
	{
		const bool bNeedsPixelShader = !Material.WritesEveryPixel() || bMaterialUsesPixelDepthOffset || Material.IsTranslucencyWritingCustomDepth();
		if (bNeedsPixelShader)
		{
			ShaderTypes.AddShaderType<FDepthOnlyPS>();
			ShaderTypes.PipelineType = &DepthPipeline;
		}
		else
		{
			ShaderTypes.PipelineType = &DepthNoPixelPipeline;
		}
	}

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetPipeline(ShaderPipeline);
	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

#define IMPLEMENT_GetDepthPassShaders( bPositionOnly ) \
	template bool GetDepthPassShaders< bPositionOnly >( \
		const FMaterial& Material, \
		const FVertexFactoryType* VertexFactoryType, \
		ERHIFeatureLevel::Type FeatureLevel, \
		bool bMaterialUsesPixelDepthOffset, \
		TShaderRef<TDepthOnlyVS<bPositionOnly>>& VertexShader, \
		TShaderRef<FDepthOnlyPS>& PixelShader, \
		FShaderPipelineRef& ShaderPipeline \
	);

IMPLEMENT_GetDepthPassShaders( true );
IMPLEMENT_GetDepthPassShaders( false );

FDepthStencilStateRHIRef GetDitheredLODTransitionDepthStencilState()
{
	return TStaticDepthStencilState<true, CF_DepthNearOrEqual,
		true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK
		>::GetRHI();
}

void SetDepthPassDitheredLODTransitionState(const FSceneView* SceneView, const FMeshBatch& RESTRICT Mesh, int32 StaticMeshId, FMeshPassProcessorRenderState& DrawRenderState)
{
	if (SceneView && StaticMeshId >= 0 && Mesh.bDitheredLODTransition)
	{
		checkSlow(SceneView->bIsViewInfo);
		const FViewInfo* ViewInfo = (FViewInfo*)SceneView;

		if (ViewInfo->bAllowStencilDither)
		{
			if (ViewInfo->StaticMeshFadeOutDitheredLODMap[StaticMeshId])
			{
				DrawRenderState.SetDepthStencilState(GetDitheredLODTransitionDepthStencilState());
				DrawRenderState.SetStencilRef(STENCIL_SANDBOX_MASK);
			}
			else if (ViewInfo->StaticMeshFadeInDitheredLODMap[StaticMeshId])
			{
				DrawRenderState.SetDepthStencilState(GetDitheredLODTransitionDepthStencilState());
			}
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("Prepass"), STAT_CLP_Prepass, STATGROUP_ParallelCommandListMarkers);

/** A pixel shader used to fill the stencil buffer with the current dithered transition mask. */
class FDitheredTransitionStencilPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDitheredTransitionStencilPS);
	SHADER_USE_PARAMETER_STRUCT(FDitheredTransitionStencilPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(float, DitheredTransitionFactor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDitheredTransitionStencilPS, "/Engine/Private/DitheredTransitionStencil.usf", "Main", SF_Pixel);

/** A compute shader used to fill the stencil buffer with the current dithered transition mask. */
class FDitheredTransitionStencilCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDitheredTransitionStencilCS);
	SHADER_USE_PARAMETER_STRUCT(FDitheredTransitionStencilCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, StencilOutput)
		SHADER_PARAMETER(float, DitheredTransitionFactor)
		SHADER_PARAMETER(FIntVector4, StencilOffsetAndValues)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDitheredTransitionStencilCS, "/Engine/Private/DitheredTransitionStencil.usf", "MainCS", SF_Compute);

void AddDitheredStencilFillPass(FRDGBuilder& GraphBuilder, TConstArrayView<FViewInfo> Views, FRDGTextureRef DepthTexture, const FDepthPassInfo& DepthPass)
{
	RDG_EVENT_SCOPE(GraphBuilder, "DitheredStencilPrePass");

	checkf(EnumHasAnyFlags(DepthPass.StencilDitherPassFlags, ERDGPassFlags::Raster | ERDGPassFlags::Compute | ERDGPassFlags::AsyncCompute), TEXT("Stencil dither fill pass flags are invalid."));

	if (DepthPass.StencilDitherPassFlags == ERDGPassFlags::Raster)
	{
		FRHIDepthStencilState* DepthStencilState = TStaticDepthStencilState<false, CF_Always,
			true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK>::GetRHI();

		const uint32 StencilRef = STENCIL_SANDBOX_MASK;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

			TShaderMapRef<FDitheredTransitionStencilPS> PixelShader(View.ShaderMap);

			auto* PassParameters = GraphBuilder.AllocParameters<FDitheredTransitionStencilPS::FParameters>();
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->DitheredTransitionFactor = View.GetTemporalLODTransition();
			PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(DepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, View.ShaderMap, {}, PixelShader, PassParameters, View.ViewRect, nullptr, nullptr, DepthStencilState, StencilRef);
		}
	}
	else
	{
		const int32 MaskedValue = (STENCIL_SANDBOX_MASK & 0xFF);
		const int32 ClearedValue = 0;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

			TShaderMapRef<FDitheredTransitionStencilCS> ComputeShader(View.ShaderMap);

			auto* PassParameters = GraphBuilder.AllocParameters<FDitheredTransitionStencilCS::FParameters>();
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->StencilOutput = GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(DepthTexture, ERDGTextureMetaDataAccess::Stencil));
			PassParameters->DitheredTransitionFactor = View.GetTemporalLODTransition();
			PassParameters->StencilOffsetAndValues = FIntVector4(View.ViewRect.Min.X, View.ViewRect.Min.Y, MaskedValue, ClearedValue);

			const FIntPoint SubExtent(
				FMath::Min(DepthTexture->Desc.Extent.X, View.ViewRect.Width()),
				FMath::Min(DepthTexture->Desc.Extent.Y, View.ViewRect.Height()));
			check(SubExtent.X > 0 && SubExtent.Y > 0);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				{},
				DepthPass.StencilDitherPassFlags,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(SubExtent, FComputeShaderUtils::kGolden2DGroupSize));
		}
	}
}

// GPUCULL_TODO: Move to Utils file and make templated on params and mesh pass processor
static void AddViewMeshElementsPass(const TIndirectArray<FMeshBatch>& MeshElements, FRDGBuilder& GraphBuilder, FDepthPassParameters* PassParameters, const FScene* Scene, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState, bool bRespectUseAsOccluderFlag, EDepthDrawingMode DepthDrawingMode, FInstanceCullingManager& InstanceCullingManager)
{
	AddSimpleMeshPass(GraphBuilder, PassParameters, Scene, View, &InstanceCullingManager, RDG_EVENT_NAME("ViewMeshElementsPass"), View.ViewRect,
		[&View, Scene, DrawRenderState, &MeshElements, bRespectUseAsOccluderFlag, DepthDrawingMode](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
		{
			FDepthPassMeshProcessor PassMeshProcessor(
				EMeshPass::DepthPass,
				View.Family->Scene->GetRenderScene(),
				View.GetFeatureLevel(),
				&View,
				DrawRenderState,
				bRespectUseAsOccluderFlag,
				DepthDrawingMode,
				false,
				false,
				DynamicMeshPassContext);

			const uint64 DefaultBatchElementMask = ~0ull;

			for (const FMeshBatch& MeshBatch : MeshElements)
			{
				PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
			}
		}
	);
}


static void RenderPrePassEditorPrimitives(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FDepthPassParameters* PassParameters,
	const FMeshPassProcessorRenderState& DrawRenderState,
	EDepthDrawingMode DepthDrawingMode,
	FInstanceCullingManager& InstanceCullingManager)
{
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("EditorPrimitives"),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, DrawRenderState, DepthDrawingMode](FRHICommandList& RHICmdList)
	{
		const bool bRespectUseAsOccluderFlag = true;

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_World);
		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_Foreground);

		if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
		{
			DrawDynamicMeshPass(View, RHICmdList, [&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FDepthPassMeshProcessor PassMeshProcessor(
					EMeshPass::DepthPass,
					View.Family->Scene->GetRenderScene(),
					View.GetFeatureLevel(),
					&View,
					DrawRenderState,
					bRespectUseAsOccluderFlag,
					DepthDrawingMode,
					false,
					false,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = ~0ull;

				for (int32 MeshIndex = 0; MeshIndex < View.ViewMeshElements.Num(); MeshIndex++)
				{
					const FMeshBatch& MeshBatch = View.ViewMeshElements[MeshIndex];
					PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
				}
			});

			// Draw the view's batched simple elements(lines, sprites, etc).
			View.BatchedViewElements.Draw(RHICmdList, DrawRenderState, View.FeatureLevel, View, false);

			DrawDynamicMeshPass(View, RHICmdList, [&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FDepthPassMeshProcessor PassMeshProcessor(
					EMeshPass::DepthPass,
					View.Family->Scene->GetRenderScene(),
					View.GetFeatureLevel(),
					&View,
					DrawRenderState,
					bRespectUseAsOccluderFlag,
					DepthDrawingMode,
					false,
					false,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = ~0ull;

				for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
				{
					const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
					PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
				}
			});

			// Draw the view's batched simple elements(lines, sprites, etc).
			View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, View.FeatureLevel, View, false);
		}
	});
}

void SetupDepthPassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	// Disable color writes, enable depth tests and writes.
	DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
}

extern const TCHAR* GetDepthPassReason(bool bDitheredLODTransitionsUseStencil, EShaderPlatform ShaderPlatform);

void FDeferredShadingSceneRenderer::RenderPrePass(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneDepthTexture, FInstanceCullingManager& InstanceCullingManager)
{
	RDG_EVENT_SCOPE(GraphBuilder, "PrePass %s %s", GetDepthDrawingModeString(DepthPass.EarlyZPassMode), GetDepthPassReason(DepthPass.bDitheredLODTransitionsUseStencil, ShaderPlatform));
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderPrePass);
	RDG_GPU_STAT_SCOPE(GraphBuilder, Prepass);

	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderPrePass, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_DepthDrawTime);

	const bool bParallelDepthPass = GRHICommandList.UseParallelAlgorithms() && CVarParallelPrePass.GetValueOnRenderThread();

	RenderPrePassHMD(GraphBuilder, SceneDepthTexture);

	if (DepthPass.IsRasterStencilDitherEnabled())
	{
		AddDitheredStencilFillPass(GraphBuilder, Views, SceneDepthTexture, DepthPass);
	}

	// Draw a depth pass to avoid overdraw in the other passes.
	if (DepthPass.EarlyZPassMode != DDM_None)
	{
		if (bParallelDepthPass)
		{
			RDG_WAIT_FOR_TASKS_CONDITIONAL(GraphBuilder, IsDepthPassWaitForTasksEnabled());

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				FViewInfo& View = Views[ViewIndex];
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

				FMeshPassProcessorRenderState DrawRenderState;
				SetupDepthPassState(DrawRenderState);

				const bool bShouldRenderView = View.ShouldRenderView();
				if (bShouldRenderView)
				{
					View.BeginRenderView();

					FDepthPassParameters* PassParameters = GetDepthPassParameters(GraphBuilder, View, SceneDepthTexture);
					View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

					GraphBuilder.AddPass(
						RDG_EVENT_NAME("DepthPassParallel"),
						PassParameters,
						ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
						[this, &View, PassParameters](const FRDGPass* InPass, FRHICommandListImmediate& RHICmdList)
					{
						FRDGParallelCommandListSet ParallelCommandListSet(InPass, RHICmdList, GET_STATID(STAT_CLP_Prepass), *this, View, FParallelCommandListBindings(PassParameters));
						ParallelCommandListSet.SetHighPriority();

						View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].DispatchDraw(&ParallelCommandListSet, RHICmdList, &PassParameters->InstanceCullingDrawParams);
					});

					RenderPrePassEditorPrimitives(GraphBuilder, View, PassParameters, DrawRenderState, DepthPass.EarlyZPassMode, InstanceCullingManager);
				}
			}
		}
		else
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				FViewInfo& View = Views[ViewIndex];
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

				FMeshPassProcessorRenderState DrawRenderState;
				SetupDepthPassState(DrawRenderState);

				const bool bShouldRenderView = View.ShouldRenderView();
				if (bShouldRenderView)
				{
					View.BeginRenderView();

					FDepthPassParameters* PassParameters = GetDepthPassParameters(GraphBuilder, View, SceneDepthTexture);
					View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

					GraphBuilder.AddPass(
						RDG_EVENT_NAME("DepthPass"),
						PassParameters,
						ERDGPassFlags::Raster,
						[this, &View, PassParameters](FRHICommandList& RHICmdList)
					{
						SetStereoViewport(RHICmdList, View, 1.0f);
						View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
					});

					RenderPrePassEditorPrimitives(GraphBuilder, View, PassParameters, DrawRenderState, DepthPass.EarlyZPassMode, InstanceCullingManager);
				}
			}
		}
	}

	// Dithered transition stencil mask clear, accounting for all active viewports
	if (DepthPass.bDitheredLODTransitionsUseStencil)
	{
		auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneDepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DitherStencilClear"),
			PassParameters,
			ERDGPassFlags::Raster,
			[this](FRHICommandList& RHICmdList)
		{
			if (Views.Num() > 1)
			{
				FIntRect FullViewRect = Views[0].ViewRect;
				for (int32 ViewIndex = 1; ViewIndex < Views.Num(); ++ViewIndex)
				{
					FullViewRect.Union(Views[ViewIndex].ViewRect);
				}
				RHICmdList.SetViewport(FullViewRect.Min.X, FullViewRect.Min.Y, 0, FullViewRect.Max.X, FullViewRect.Max.Y, 1);
			}
			DrawClearQuad(RHICmdList, false, FLinearColor::Transparent, false, 0, true, 0);
		});
	}

#if !(UE_BUILD_SHIPPING)
	const bool bForwardShadingEnabled = IsForwardShadingEnabled(ShaderPlatform);
	if (!bForwardShadingEnabled)
	{
		StampDeferredDebugProbeDepthPS(GraphBuilder, Views, SceneDepthTexture);
	}
#endif
}

bool FMobileSceneRenderer::ShouldRenderPrePass() const
{
	// Draw a depth pass to avoid overdraw in the other passes.
	return Scene->EarlyZPassMode == DDM_MaskedOnly || Scene->EarlyZPassMode == DDM_AllOpaque;
}

void FMobileSceneRenderer::RenderPrePass(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	if (!ShouldRenderPrePass())
	{
		return;
	}
	
	checkSlow(RHICmdList.IsInsideRenderPass());

	SCOPED_NAMED_EVENT(FMobileSceneRenderer_RenderPrePass, FColor::Emerald);
	SCOPED_DRAW_EVENT(RHICmdList, MobileRenderPrePass);

	SCOPE_CYCLE_COUNTER(STAT_DepthDrawTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderPrePass);
	SCOPED_GPU_STAT(RHICmdList, Prepass);

	SetStereoViewport(RHICmdList, View);
	View.ParallelMeshDrawCommandPasses[EMeshPass::DepthPass].DispatchDraw(nullptr, RHICmdList, &MeshPassInstanceCullingDrawParams[EMeshPass::DepthPass]);
}

void FDeferredShadingSceneRenderer::RenderPrePassHMD(FRDGBuilder& GraphBuilder, FRDGTextureRef DepthTexture)
{
	// Early out before we change any state if there's not a mask to render
	if (!IsHMDHiddenAreaMaskActive())
	{
		return;
	}

	auto* HMDDevice = GEngine->XRSystem->GetHMDDevice();
	if (!HMDDevice)
	{
		return;
	}

	for (const FViewInfo& View : Views)
	{
		if (IStereoRendering::IsStereoEyeView(View))
		{
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);

			FDepthPassParameters* PassParameters = GetDepthPassParameters(GraphBuilder, View, DepthTexture);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("HiddenAreaMask"),
				PassParameters,
				ERDGPassFlags::Raster,
				[this, &View, HMDDevice](FRHICommandList& RHICmdList)
			{
				extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

				TShaderMapRef<TOneColorVS<true>> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
				VertexShader->SetDepthParameter(RHICmdList, 1.0f);
				HMDDevice->DrawHiddenAreaMesh(RHICmdList, View.StereoViewIndex);
			});
		}
	}
}

FMeshDrawCommandSortKey CalculateDepthPassMeshStaticSortKey(EBlendMode BlendMode, const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader)
{
	FMeshDrawCommandSortKey SortKey;
	if (GEarlyZSortMasked)
	{
		SortKey.BasePass.VertexShaderHash = (VertexShader ? VertexShader->GetSortKey() : 0) & 0xFFFF;
		SortKey.BasePass.PixelShaderHash = PixelShader ? PixelShader->GetSortKey() : 0;
		SortKey.BasePass.Masked = BlendMode == EBlendMode::BLEND_Masked ? 1 : 0;
	}
	else
	{
		SortKey.Generic.VertexShaderHash = VertexShader ? VertexShader->GetSortKey() : 0;
		SortKey.Generic.PixelShaderHash = PixelShader ? PixelShader->GetSortKey() : 0;
	}
	
	return SortKey;
}

void SetMobileDepthPassRenderState(const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, FMeshPassProcessorRenderState& DrawRenderState, const FMeshBatch& RESTRICT MeshBatch, bool bUsesDeferredShading)
{
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
		true, CF_DepthNearOrEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		// don't use masking as it has significant performance hit on Mali GPUs (T860MP2)
		0x00, 0xff >::GetRHI());

	uint8 StencilValue = 0;
	
	uint8 ReceiveDecals = (PrimitiveSceneProxy && !PrimitiveSceneProxy->ReceivesDecals() ? 0x01 : 0x00);
	StencilValue |= GET_STENCIL_BIT_MASK(RECEIVE_DECAL, ReceiveDecals);

	if (bUsesDeferredShading)
	{
		extern uint8 GetMobileShadingModelStencilValue(FMaterialShadingModelField ShadingModel);

		// store into [1-3] bits
		const FMaterial& MaterialResource = MeshBatch.MaterialRenderProxy->GetIncompleteMaterialWithFallback(ERHIFeatureLevel::ES3_1);
		uint8 ShadingModel = GetMobileShadingModelStencilValue(MaterialResource.GetShadingModels());
		StencilValue |= GET_STENCIL_MOBILE_SM_MASK(ShadingModel);
		StencilValue |= STENCIL_LIGHTING_CHANNELS_MASK(PrimitiveSceneProxy ? PrimitiveSceneProxy->GetLightingChannelStencilValue() : 0x00);
	}

	DrawRenderState.SetStencilRef(StencilValue);
}

template<bool bPositionOnly>
bool FDepthPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	EBlendMode BlendMode,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		TDepthOnlyVS<bPositionOnly>,
		FDepthOnlyPS> DepthPassShaders;

	FShaderPipelineRef ShaderPipeline;

	if (!GetDepthPassShaders<bPositionOnly>(
		MaterialResource,
		VertexFactory->GetType(),
		FeatureLevel,
		MaterialResource.MaterialUsesPixelDepthOffset_RenderThread(),
		DepthPassShaders.VertexShader,
		DepthPassShaders.PixelShader,
		ShaderPipeline))
	{
		return false;
	}

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	if (!bDitheredLODFadingOutMaskPass && !bShadowProjection)
	{
		SetDepthPassDitheredLODTransitionState(ViewIfDynamicMeshCommand, MeshBatch, StaticMeshId, DrawRenderState);
	}

	// Use StencilMask for DecalOutput on mobile
	if (FeatureLevel == ERHIFeatureLevel::ES3_1 && !bShadowProjection)
	{
		SetMobileDepthPassRenderState(PrimitiveSceneProxy, DrawRenderState, MeshBatch, IsMobileDeferredShadingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)));
	}

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	const FMeshDrawCommandSortKey SortKey = CalculateDepthPassMeshStaticSortKey(BlendMode, DepthPassShaders.VertexShader.GetShader(), DepthPassShaders.PixelShader.GetShader());

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		DepthPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		bPositionOnly ? EMeshPassFeatures::PositionOnly : EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

template<bool bPositionOnly>
void FDepthPassMeshProcessor::CollectPSOInitializersInternal(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FVertexFactoryType* VertexFactoryType,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	bool bDitheredLODTransition,
	EPrimitiveType PrimitiveType,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	TMeshProcessorShaders<
		TDepthOnlyVS<bPositionOnly>,
		FDepthOnlyPS> DepthPassShaders;

	FShaderPipelineRef ShaderPipeline;

	if (!GetDepthPassShaders<bPositionOnly>(
		MaterialResource,
		VertexFactoryType,
		FeatureLevel,
		MaterialResource.MaterialUsesPixelDepthOffset_GameThread(),
		DepthPassShaders.VertexShader,
		DepthPassShaders.PixelShader,
		ShaderPipeline))
	{
		return;
	}

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	// If bDitheredLODTransition option is set, then swap to that depth stencil state (see logic in SetDepthPassDitheredLODTransitionState())
	if (!bDitheredLODFadingOutMaskPass && !bShadowProjection && bDitheredLODTransition)
	{
		DrawRenderState.SetDepthStencilState(GetDitheredLODTransitionDepthStencilState());
	}

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	RenderTargetsInfo.NumSamples = 1;

	ETextureCreateFlags DepthStencilCreateFlags = SceneTexturesConfig.DepthCreateFlags;
	SetupDepthStencilInfo(PF_DepthStencil, DepthStencilCreateFlags, ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite, RenderTargetsInfo);

	AddGraphicsPipelineStateInitializer(
		VertexFactoryType,
		MaterialResource,
		DrawRenderState,
		RenderTargetsInfo,
		DepthPassShaders,
		MeshFillMode,
		MeshCullMode,
		PrimitiveType,
		bPositionOnly ? EMeshPassFeatures::PositionOnly : EMeshPassFeatures::Default,
		PSOInitializers);
}

bool FDepthPassMeshProcessor::UseDefaultMaterial(const FMaterial& Material, bool bMaterialModifiesMeshPosition, bool bSupportPositionOnlyStream, bool& bPositionOnly)
{
	bool bUseDefaultMaterial = false;

	const EBlendMode BlendMode = Material.GetBlendMode();
	if (BlendMode == BLEND_Opaque
		&& EarlyZPassMode != DDM_MaskedOnly
		&& bSupportPositionOnlyStream
		&& !bMaterialModifiesMeshPosition
		&& Material.WritesEveryPixel())
	{
		bUseDefaultMaterial = true;
		bPositionOnly = true;
	}
	else
	{
		// still possible to use default material
		const bool bMaterialMasked = !Material.WritesEveryPixel() || Material.IsTranslucencyWritingCustomDepth();
		if ((!bMaterialMasked && EarlyZPassMode != DDM_MaskedOnly) || (bMaterialMasked && EarlyZPassMode != DDM_NonMaskedOnly))
		{
			if (!bMaterialMasked && !bMaterialModifiesMeshPosition)
			{
				bUseDefaultMaterial = true;
				bPositionOnly = false;
			}
		}
	}

	return bUseDefaultMaterial;
}

bool FDepthPassMeshProcessor::TryAddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& Material)
{
	const EBlendMode BlendMode = Material.GetBlendMode();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	bool ShouldRenderInDepthPass = (!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInDepthPass());

	bool bResult = true;
	if (!bIsTranslucent
		&& ShouldRenderInDepthPass
		&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain())
		&& ShouldIncludeMaterialInDefaultOpaquePass(Material))
	{
		bool bPositionOnly = false;
		bool bUseDefaultMaterial = UseDefaultMaterial(Material, Material.MaterialModifiesMeshPosition_RenderThread(), MeshBatch.VertexFactory->SupportsPositionOnlyStream(), bPositionOnly);

		const FMaterialRenderProxy* EffectiveMaterialRenderProxy = &MaterialRenderProxy;
		const FMaterial* EffectiveMaterial = &Material;
		if (bUseDefaultMaterial)
		{
			// Override with the default material
			EffectiveMaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
			EffectiveMaterial = EffectiveMaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			check(EffectiveMaterial);
		}

		const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

		if (bPositionOnly)
		{
			bResult = Process<true>(MeshBatch, BatchElementMask, StaticMeshId, BlendMode, PrimitiveSceneProxy, *EffectiveMaterialRenderProxy, *EffectiveMaterial, MeshFillMode, MeshCullMode);
		}
		else
		{
			bResult = Process<false>(MeshBatch, BatchElementMask, StaticMeshId, BlendMode, PrimitiveSceneProxy, *EffectiveMaterialRenderProxy, *EffectiveMaterial, MeshFillMode, MeshCullMode);
		}
	}

	return bResult;
}

void FDepthPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	bool bDraw = MeshBatch.bUseForDepthPass;
	
	// Mobile rendering does not use depth prepass by default
	if (FeatureLevel == ERHIFeatureLevel::ES3_1 && EarlyZPassMode == DDM_None)
	{
		bDraw = false;
	}

	// Filter by occluder flags and settings if required.
	if (bDraw && bRespectUseAsOccluderFlag && !MeshBatch.bUseAsOccluder && EarlyZPassMode < DDM_AllOpaque)
	{
		if (PrimitiveSceneProxy)
		{
			// Only render primitives marked as occluders.
			bDraw = PrimitiveSceneProxy->ShouldUseAsOccluder()
			// Only render static objects unless movable are requested.
			&& (!PrimitiveSceneProxy->IsMovable() || bEarlyZPassMovable);

			// Filter dynamic mesh commands by screen size.
			if (ViewIfDynamicMeshCommand)
			{
				extern float GMinScreenRadiusForDepthPrepass;
				const float LODFactorDistanceSquared = (PrimitiveSceneProxy->GetBounds().Origin - ViewIfDynamicMeshCommand->ViewMatrices.GetViewOrigin()).SizeSquared() * FMath::Square(ViewIfDynamicMeshCommand->LODDistanceFactor);
				bDraw = bDraw && FMath::Square(PrimitiveSceneProxy->GetBounds().SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared;
			}
		}
		else
		{
			bDraw = false;
		}
	}

	// When using DDM_AllOpaqueNoVelocity we skip objects that will write depth+velocity in the subsequent velocity pass.
	if (EarlyZPassMode == DDM_AllOpaqueNoVelocity && PrimitiveSceneProxy)
	{
		// We should ideally check to see if we this primitive is using the FOpaqueVelocityMeshProcessor or FTranslucentVelocityMeshProcessor. 
		// But for the object to get here, it would already be culled if it was translucent, so we can assume FOpaqueVelocityMeshProcessor.
		// This logic needs to match the logic in FOpaqueVelocityMeshProcessor::AddMeshBatch()
		// todo: Move that logic to a single place.

		const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
		if (FOpaqueVelocityMeshProcessor::PrimitiveCanHaveVelocity(ShaderPlatform, PrimitiveSceneProxy))
		{
			if (ViewIfDynamicMeshCommand)
			{
				if (FOpaqueVelocityMeshProcessor::PrimitiveHasVelocityForFrame(PrimitiveSceneProxy))
				{
					checkSlow(ViewIfDynamicMeshCommand->bIsViewInfo);
					FViewInfo* ViewInfo = (FViewInfo*)ViewIfDynamicMeshCommand;

					if (FOpaqueVelocityMeshProcessor::PrimitiveHasVelocityForView(*ViewInfo, PrimitiveSceneProxy))
					{
						bDraw = false;
					}
				}
			}
		}
	}

	if (bDraw)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material && Material->GetRenderingThreadShaderMap())
			{
				if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
				{
					break;
				}
			}

			MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
		}
	}
}

void FDepthPassMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FVertexFactoryType* VertexFactoryType, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{		
	// Are we currently collecting PSO's for the default material
	if (Material.IsDefaultMaterial())
	{
		CollectDefaultMaterialPSOInitializers(SceneTexturesConfig, Material, VertexFactoryType, PSOInitializers);
		return;
	}

	const EBlendMode BlendMode = Material.GetBlendMode();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);

	// Early out if translucent or material shouldn't be used during this pass
	if (bIsTranslucent ||
		!PreCacheParams.bRenderInDepthPass ||
		!ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()) || 
		!ShouldIncludeMaterialInDefaultOpaquePass(Material))
	{
		return;
	}

	// assume we can always do this when collecting PSO's for now (vertex factory instance might actually not support it)
	bool bSupportPositionOnlyStream = VertexFactoryType->SupportsPositionOnly();  

	bool bPositionOnly = false;
	bool bUseDefaultMaterial = UseDefaultMaterial(Material, Material.MaterialModifiesMeshPosition_GameThread(), bSupportPositionOnlyStream, bPositionOnly);
	if (!bUseDefaultMaterial)
	{
		check(!bPositionOnly);

		const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

		bool bIsMoveable = PreCacheParams.IsMoveable();
		const bool bAllowDitheredLODTransition = !bIsMoveable && Material.IsDitheredLODTransition();

		bool bDitheredLODTransition = false;
		CollectPSOInitializersInternal<false>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, (EPrimitiveType)PreCacheParams.PrimitiveType, PSOInitializers);
		if (bAllowDitheredLODTransition)
		{
			bDitheredLODTransition = true;
			CollectPSOInitializersInternal<false>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, (EPrimitiveType)PreCacheParams.PrimitiveType, PSOInitializers);
		}
	}
}

void FDepthPassMeshProcessor::CollectDefaultMaterialPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	const ERasterizerFillMode MeshFillMode = FM_Solid;

	// Collect PSOs for all possible default material combinations
	{
		ERasterizerCullMode MeshCullMode = CM_None;
		bool bDitheredLODTransition = false;

		CollectPSOInitializersInternal<true>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
		CollectPSOInitializersInternal<false>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);

		bDitheredLODTransition = true;
		CollectPSOInitializersInternal<true>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
		CollectPSOInitializersInternal<false>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
	}
	{
		ERasterizerCullMode MeshCullMode = CM_CW;
		bool bDitheredLODTransition = false;

		CollectPSOInitializersInternal<true>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
		CollectPSOInitializersInternal<false>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);

		bDitheredLODTransition = true;
		CollectPSOInitializersInternal<true>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
		CollectPSOInitializersInternal<false>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
	}
	{
		ERasterizerCullMode MeshCullMode = CM_CCW;
		bool bDitheredLODTransition = false;

		CollectPSOInitializersInternal<true>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
		CollectPSOInitializersInternal<false>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);

		bDitheredLODTransition = true;
		CollectPSOInitializersInternal<true>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
		CollectPSOInitializersInternal<false>(SceneTexturesConfig, VertexFactoryType, Material, MeshFillMode, MeshCullMode, bDitheredLODTransition, PT_TriangleList, PSOInitializers);
	}
}

FDepthPassMeshProcessor::FDepthPassMeshProcessor(
	EMeshPass::Type InMeshPassType, 
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	const bool InbRespectUseAsOccluderFlag,
	const EDepthDrawingMode InEarlyZPassMode,
	const bool InbEarlyZPassMovable,
	const bool bDitheredLODFadingOutMaskPass,
	FMeshPassDrawListContext* InDrawListContext,
	const bool bInShadowProjection)
	: FMeshPassProcessor(InMeshPassType, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, bRespectUseAsOccluderFlag(InbRespectUseAsOccluderFlag)
	, EarlyZPassMode(InEarlyZPassMode)
	, bEarlyZPassMovable(InbEarlyZPassMovable)
	, bDitheredLODFadingOutMaskPass(bDitheredLODFadingOutMaskPass)
	, bShadowProjection(bInShadowProjection)
{
	PassDrawRenderState = InPassDrawRenderState;
}

FMeshPassProcessor* CreateDepthPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	EDepthDrawingMode EarlyZPassMode;
	bool bEarlyZPassMovable;
	FScene::GetEarlyZPassMode(FeatureLevel, EarlyZPassMode, bEarlyZPassMovable);

	FMeshPassProcessorRenderState DepthPassState;
	SetupDepthPassState(DepthPassState);
		
	return new FDepthPassMeshProcessor(EMeshPass::DepthPass, Scene, FeatureLevel, InViewIfDynamicMeshCommand, DepthPassState, true, EarlyZPassMode, bEarlyZPassMovable, false, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(DepthPass, CreateDepthPassProcessor, EShadingPath::Deferred, EMeshPass::DepthPass, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterMobileDepthPass(&CreateDepthPassProcessor, EShadingPath::Mobile, EMeshPass::DepthPass, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);

FMeshPassProcessor* CreateDitheredLODFadingOutMaskPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	EDepthDrawingMode EarlyZPassMode;
	bool bEarlyZPassMovable;
	FScene::GetEarlyZPassMode(FeatureLevel, EarlyZPassMode, bEarlyZPassMovable);

	FMeshPassProcessorRenderState DrawRenderState;

	DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	DrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<true, CF_Equal,
		true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK
		>::GetRHI());
	DrawRenderState.SetStencilRef(STENCIL_SANDBOX_MASK);

	return new FDepthPassMeshProcessor(EMeshPass::DitheredLODFadingOutMaskPass, Scene, FeatureLevel, InViewIfDynamicMeshCommand, DrawRenderState, true, EarlyZPassMode, bEarlyZPassMovable, true, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(DitheredLODFadingOutMaskPass, CreateDitheredLODFadingOutMaskPassProcessor, EShadingPath::Deferred, EMeshPass::DitheredLODFadingOutMaskPass, EMeshPassFlags::MainView);
