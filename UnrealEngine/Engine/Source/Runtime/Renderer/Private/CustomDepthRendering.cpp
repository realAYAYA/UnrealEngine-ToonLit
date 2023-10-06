// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomDepthRendering.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneUtils.h"
#include "DepthRendering.h"
#include "SceneRendering.h"
#include "SceneCore.h"
#include "ScenePrivate.h"
#include "Materials/Material.h"
#include "MeshPassProcessor.inl"
#include "UnrealEngine.h"

static TAutoConsoleVariable<int32> CVarCustomDepth(
	TEXT("r.CustomDepth"),
	1,
	TEXT("0: feature is disabled\n")
	TEXT("1: feature is enabled, texture is created on demand\n")
	TEXT("2: feature is enabled, texture is not released until required (should be the project setting if the feature should not stall)\n")
	TEXT("3: feature is enabled, stencil writes are enabled, texture is not released until required (should be the project setting if the feature should not stall)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarCustomDepthOrder(
	TEXT("r.CustomDepth.Order"),
	2,
	TEXT("When CustomDepth (and CustomStencil) is getting rendered\n")
	TEXT("  0: Before Base Pass (Allows samping in DBuffer pass. Can be more efficient with AsyncCompute.)\n")
	TEXT("  1: After Base Pass\n")
	TEXT("  2: Default (Before Base Pass if DBuffer enabled.)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarCustomDepthTemporalAAJitter(
	TEXT("r.CustomDepthTemporalAAJitter"),
	1,
	TEXT("If disabled the Engine will remove the TemporalAA Jitter from the Custom Depth Pass. Only has effect when TemporalAA is used."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<bool> CVarCustomDepthEnableFastClear(
	TEXT("r.CustomDepthEnableFastClear"), false,
	TEXT("Enable HTile on the custom depth buffer (default:false).\n"),
	ECVF_RenderThreadSafe);

DECLARE_DWORD_COUNTER_STAT(TEXT("Nanite Custom Depth Instances"), STAT_NaniteCustomDepthInstances, STATGROUP_Nanite);

DECLARE_GPU_DRAWCALL_STAT_NAMED(CustomDepth, TEXT("Custom Depth"));

using FNaniteCustomDepthDrawList = TArray<Nanite::FInstanceDraw, SceneRenderingAllocator>;

ECustomDepthPassLocation GetCustomDepthPassLocation(EShaderPlatform Platform)
{
	const int32 CustomDepthOrder = CVarCustomDepthOrder.GetValueOnRenderThread();
	const bool bCustomDepthBeforeBasePase = CustomDepthOrder == 0 || (CustomDepthOrder == 2 && IsUsingDBuffers(Platform));
	return bCustomDepthBeforeBasePase ? ECustomDepthPassLocation::BeforeBasePass : ECustomDepthPassLocation::AfterBasePass;
}

ECustomDepthMode GetCustomDepthMode()
{
	switch (CVarCustomDepth.GetValueOnAnyThread())
	{
	case 1: // Fallthrough.
	case 2: return ECustomDepthMode::Enabled;
	case 3: return ECustomDepthMode::EnabledWithStencil;
	}
	return ECustomDepthMode::Disabled;
}

bool IsCustomDepthPassWritingStencil()
{
	return GetCustomDepthMode() == ECustomDepthMode::EnabledWithStencil;
}

FCustomDepthTextures FCustomDepthTextures::Create(FRDGBuilder& GraphBuilder, FIntPoint CustomDepthExtent, EShaderPlatform ShaderPlatform)
{
	const ECustomDepthMode CustomDepthMode = GetCustomDepthMode();

	if (!IsCustomDepthPassEnabled())
	{
		return {};
	}

	const bool bWritesCustomStencil = IsCustomDepthPassWritingStencil();

	FCustomDepthTextures CustomDepthTextures;

	ETextureCreateFlags CreateFlags = GFastVRamConfig.CustomDepth | TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;

	// For Nanite, check to create the depth texture as a UAV and force HTILE.
	if (UseNanite(ShaderPlatform) && UseComputeDepthExport() && Nanite::GetSupportsCustomDepthRendering())
	{
		CreateFlags |= TexCreate_UAV;
	}
	else if (!CVarCustomDepthEnableFastClear.GetValueOnRenderThread())
	{
		CreateFlags |= TexCreate_NoFastClear;
	}

	const FRDGTextureDesc CustomDepthDesc = FRDGTextureDesc::Create2D(CustomDepthExtent, PF_DepthStencil, FClearValueBinding::DepthFar, CreateFlags);

	CustomDepthTextures.Depth = GraphBuilder.CreateTexture(CustomDepthDesc, TEXT("CustomDepth"));

	CustomDepthTextures.DepthAction = ERenderTargetLoadAction::EClear;
	CustomDepthTextures.StencilAction = bWritesCustomStencil ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ENoAction;

	return CustomDepthTextures;
}

BEGIN_SHADER_PARAMETER_STRUCT(FCustomDepthPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

static FViewShaderParameters CreateViewShaderParametersWithoutJitter(const FViewInfo& View)
{
	const auto SetupParameters = [](const FViewInfo& View, FViewUniformShaderParameters& Parameters)
	{
		FBox VolumeBounds[TVC_MAX];
		FViewMatrices ModifiedViewMatrices = View.ViewMatrices;
		ModifiedViewMatrices.HackRemoveTemporalAAProjectionJitter();

		Parameters = *View.CachedViewUniformShaderParameters;
		View.SetupUniformBufferParameters(ModifiedViewMatrices, ModifiedViewMatrices, VolumeBounds, TVC_MAX, Parameters);
	};

	FViewUniformShaderParameters ViewUniformParameters;
	SetupParameters(View, ViewUniformParameters);

	FViewShaderParameters Parameters;
	Parameters.View = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(ViewUniformParameters, UniformBuffer_SingleFrame);

	if (View.bShouldBindInstancedViewUB)
	{
		FInstancedViewUniformShaderParameters LocalInstancedViewUniformShaderParameters;
		InstancedViewParametersUtils::CopyIntoInstancedViewParameters(LocalInstancedViewUniformShaderParameters, ViewUniformParameters, 0);

		if (const FViewInfo* InstancedView = View.GetInstancedView())
		{
			SetupParameters(*InstancedView, ViewUniformParameters);
			InstancedViewParametersUtils::CopyIntoInstancedViewParameters(LocalInstancedViewUniformShaderParameters, ViewUniformParameters, 1);
		}

		Parameters.InstancedView = TUniformBufferRef<FInstancedViewUniformShaderParameters>::CreateUniformBufferImmediate(
			LocalInstancedViewUniformShaderParameters,
			UniformBuffer_SingleFrame);
	}

	return Parameters;
}

static FNaniteCustomDepthDrawList BuildNaniteCustomDepthDrawList(const FViewInfo& View, uint32 ViewIndex, const FNaniteVisibilityResults& VisibilityResults)
{
	FNaniteCustomDepthDrawList Output;
	for (const FPrimitiveInstanceRange& InstanceRange : View.NaniteCustomDepthInstances)
	{
		if (VisibilityResults.ShouldRenderCustomDepthPrimitive(InstanceRange.PrimitiveIndex))
		{
			const uint32 FirstOutputIndex = Output.Num();
			Output.AddUninitialized(InstanceRange.NumInstances);
			for (uint32 RelativeIndex = 0; RelativeIndex < uint32(InstanceRange.NumInstances); ++RelativeIndex)
			{
				const Nanite::FInstanceDraw Draw { InstanceRange.InstanceSceneDataOffset + RelativeIndex, ViewIndex };
				Output[FirstOutputIndex + RelativeIndex] = Draw;
			}
		}
	}

	return MoveTemp(Output);
}

bool FSceneRenderer::RenderCustomDepthPass(
	FRDGBuilder& GraphBuilder,
	FCustomDepthTextures& CustomDepthTextures,
	const FSceneTextureShaderParameters& SceneTextures,
	TConstArrayView<Nanite::FRasterResults> PrimaryNaniteRasterResults,
	TConstArrayView<Nanite::FPackedView> PrimaryNaniteViews)
{
	if (!CustomDepthTextures.IsValid())
	{
		return false;
	}

	// Determine if any of the views have custom depth and if any of them have Nanite that is rendering custom depth
	bool bAnyCustomDepth = false;
	TArray<FNaniteCustomDepthDrawList, SceneRenderingAllocator> NaniteDrawLists;
	NaniteDrawLists.AddDefaulted(Views.Num());
	uint32 TotalNaniteInstances = 0;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		if (View.ShouldRenderView() && View.bHasCustomDepthPrimitives)
		{
			if (PrimaryNaniteRasterResults.IsValidIndex(ViewIndex))
			{
				const FNaniteVisibilityResults& VisibilityResults = PrimaryNaniteRasterResults[ViewIndex].VisibilityResults;

				// Get the Nanite instance draw list for this view. (NOTE: Always use view index 0 for now because we're not doing
				// multi-view yet).
				NaniteDrawLists[ViewIndex] = BuildNaniteCustomDepthDrawList(View, 0u, VisibilityResults);

				TotalNaniteInstances += NaniteDrawLists[ViewIndex].Num();
			}
			bAnyCustomDepth = true;
		}
	}

	SET_DWORD_STAT(STAT_NaniteCustomDepthInstances, TotalNaniteInstances);

	if (!bAnyCustomDepth)
	{
		return false;
	}

	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderCustomDepthPass);
	RDG_GPU_STAT_SCOPE(GraphBuilder, CustomDepth);

	// Render non-Nanite Custom Depth primitives
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

		FViewInfo& View = Views[ViewIndex];

		if (View.ShouldRenderView() && View.bHasCustomDepthPrimitives)
		{
			View.BeginRenderView();

			FCustomDepthPassParameters* PassParameters = GraphBuilder.AllocParameters<FCustomDepthPassParameters>();
			PassParameters->SceneTextures = SceneTextures;

			// User requested jitter-free custom depth.
			if (CVarCustomDepthTemporalAAJitter.GetValueOnRenderThread() == 0 && IsTemporalAccumulationBasedMethod(View.AntiAliasingMethod))
			{
				PassParameters->View = CreateViewShaderParametersWithoutJitter(View);
			}
			else
			{
				PassParameters->View = View.GetShaderParameters();
			}

			const ERenderTargetLoadAction DepthLoadAction = GetLoadActionIfProduced(CustomDepthTextures.Depth, CustomDepthTextures.DepthAction);
			const ERenderTargetLoadAction StencilLoadAction = GetLoadActionIfProduced(CustomDepthTextures.Depth, CustomDepthTextures.StencilAction);

			PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
				CustomDepthTextures.Depth,
				DepthLoadAction,
				StencilLoadAction,
				FExclusiveDepthStencil::DepthWrite_StencilWrite);

			View.ParallelMeshDrawCommandPasses[EMeshPass::CustomDepth].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("CustomDepth"),
				PassParameters,
				ERDGPassFlags::Raster,
				[this, &View, PassParameters](FRHICommandList& RHICmdList)
			{
				SetStereoViewport(RHICmdList, View, 1.0f);
				View.ParallelMeshDrawCommandPasses[EMeshPass::CustomDepth].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
			});
		}
	}

	if (TotalNaniteInstances > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Nanite CustomDepth");

		const FIntPoint RasterTextureSize = CustomDepthTextures.Depth->Desc.Extent;
		FIntRect RasterTextureRect(0, 0, RasterTextureSize.X, RasterTextureSize.Y);
		if (Views.Num() == 1)
		{
			const FViewInfo& View = Views[0];
			if (View.ViewRect.Min.X == 0 && View.ViewRect.Min.Y == 0)
			{
				RasterTextureRect = View.ViewRect;
			}
		}

		const bool bWriteCustomStencil = IsCustomDepthPassWritingStencil();

		Nanite::FSharedContext SharedContext{};
		SharedContext.FeatureLevel = Scene->GetFeatureLevel();
		SharedContext.ShaderMap = GetGlobalShaderMap(SharedContext.FeatureLevel);
		SharedContext.Pipeline = Nanite::EPipeline::Primary;

		// TODO: If !bWriteCustomStencil, we could copy off the depth and rasterize depth-only (probable optimization)
		Nanite::FRasterContext RasterContext = Nanite::InitRasterContext(
			GraphBuilder,
			SharedContext,
			ViewFamily,
			RasterTextureSize,
			RasterTextureRect,
			false, // bVisualize
			Nanite::EOutputBufferMode::VisBuffer,
			true, // bClearTarget
			nullptr, // RectMinMaxBufferSRV
			0, // NumRects
			nullptr, // ExternalDepthBuffer
			true // bCustomPass
		);

		Nanite::FCustomDepthContext CustomDepthContext = Nanite::InitCustomDepthStencilContext(
			GraphBuilder,
			CustomDepthTextures,
			bWriteCustomStencil);

		Nanite::FConfiguration CullingConfig = { 0 };
		CullingConfig.bUpdateStreaming = true;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			if (!View.ShouldRenderView() || NaniteDrawLists[ViewIndex].Num() == 0)
			{
				continue;
			}

			auto NaniteRenderer = Nanite::IRenderer::Create(
				GraphBuilder,
				*Scene,
				View,
				GetSceneUniforms(),
				SharedContext,
				RasterContext,
				CullingConfig,
				View.ViewRect,
				/* PrevHZB = */ nullptr
			);

			NaniteRenderer->DrawGeometry(
				Scene->NaniteRasterPipelines[ENaniteMeshPass::BasePass],
				PrimaryNaniteRasterResults[ViewIndex].VisibilityResults,
				*Nanite::FPackedViewArray::Create(GraphBuilder, PrimaryNaniteViews[ViewIndex]),
				NaniteDrawLists[ViewIndex]
			);

			Nanite::FRasterResults RasterResults;
			NaniteRenderer->ExtractResults( RasterResults );

			// Emit depth
			Nanite::EmitCustomDepthStencilTargets(
				GraphBuilder,
				*Scene,
				View,
				RasterResults.PageConstants,
				RasterResults.VisibleClustersSWHW,
				RasterResults.ViewsBuffer,
				RasterContext.VisBuffer64,
				CustomDepthContext
			);
		}

		Nanite::FinalizeCustomDepthStencil(GraphBuilder, CustomDepthContext, CustomDepthTextures);
	}
	else
	{
		const FSceneTexturesConfig& Config = FSceneTexturesConfig::Get();
		// TextureView is not supported in GLES, so we can't lookup CustomDepth and CustomStencil from a single texture
		// Do a copy of the CustomDepthStencil texture if CustomStencil is sampled in a shader.
		if (IsOpenGLPlatform(ShaderPlatform))
		{
			if (Config.bSamplesCustomStencil)
			{
				FRDGTextureRef CustomStencil = GraphBuilder.CreateTexture(CustomDepthTextures.Depth->Desc, TEXT("CustomStencil"));
				AddCopyTexturePass(GraphBuilder, CustomDepthTextures.Depth, CustomStencil);
				CustomDepthTextures.Stencil = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateWithPixelFormat(CustomStencil, PF_X24_G8));
			}
		}
		else
		{
			CustomDepthTextures.Stencil = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateWithPixelFormat(CustomDepthTextures.Depth, PF_X24_G8));
		}
		CustomDepthTextures.bSeparateStencilBuffer = false;
	}

	return true;
}

class FCustomDepthPassMeshProcessor : public FSceneRenderingAllocatorObject<FCustomDepthPassMeshProcessor>, public FMeshPassProcessor
{
public:
	FCustomDepthPassMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override final;

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);

	template<bool bPositionOnly>
	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	bool UseDefaultMaterial(const FMaterial& Material, bool bMaterialModifiesMeshPosition, bool bSupportPositionOnlyStream, bool bVFTypeSupportsNullPixelShader, bool& bPositionOnly, bool& bIgnoreThisMaterial);

	void CollectDefaultMaterialPSOInitializers(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FMaterial& Material,
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		TArray<FPSOPrecacheData>& PSOInitializers);

	template<bool bPositionOnly>
	void CollectPSOInitializers(
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode, 
		TArray<FPSOPrecacheData>& PSOInitializers);

	FMeshPassProcessorRenderState PassDrawRenderState;
};

FCustomDepthPassMeshProcessor::FCustomDepthPassMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::CustomDepth, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
{
	PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
}

void FCustomDepthPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (PrimitiveSceneProxy->ShouldRenderCustomDepth())
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material)
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

FRHIDepthStencilState* GetCustomDepthStencilState(bool bWriteCustomStencilValues, EStencilMask StencilWriteMask)
{
	if (bWriteCustomStencilValues)
	{
		static FRHIDepthStencilState* StencilStates[EStencilMask::SM_Count] =
		{
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 255>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 255>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 1>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 2>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 4>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 8>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 16>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 32>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 64>::GetRHI(),
			TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Replace, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 255, 128>::GetRHI()
		};
		checkSlow(EStencilMask::SM_Count == UE_ARRAY_COUNT(StencilStates));
		return StencilStates[(int32)StencilWriteMask];
	}
	else
	{
		return TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
	}
}

bool FCustomDepthPassMeshProcessor::UseDefaultMaterial(const FMaterial& Material, bool bMaterialModifiesMeshPosition, bool bSupportPositionOnlyStream, bool bVFTypeSupportsNullPixelShader, bool& bPositionOnly, bool& bIgnoreThisMaterial)
{
	bool bUseDefaultMaterial = false;
	bIgnoreThisMaterial = false;

	const bool bIsOpaque = IsOpaqueBlendMode(Material);
	const bool bIsTranslucent = IsTranslucentBlendMode(Material);
	if (bIsOpaque
		&& bSupportPositionOnlyStream
		&& !bMaterialModifiesMeshPosition
		&& Material.WritesEveryPixel(false, bVFTypeSupportsNullPixelShader))
	{
		bUseDefaultMaterial = true;
		bPositionOnly = true;
	}
	else if (!bIsTranslucent || Material.IsTranslucencyWritingCustomDepth())
	{
		const bool bMaterialMasked = !Material.WritesEveryPixel(false, bVFTypeSupportsNullPixelShader) || Material.IsTranslucencyWritingCustomDepth();
		if (!bMaterialMasked && !bMaterialModifiesMeshPosition)
		{
			bUseDefaultMaterial = true;
			bPositionOnly = false;
		}
	}
	else
	{
		// E.g., ignore translucent materials without allowing custom depth writes.
		bIgnoreThisMaterial = true;
	}

	return bUseDefaultMaterial;
}

bool FCustomDepthPassMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	// Setup the depth stencil state
	const bool bWriteCustomStencilValues = IsCustomDepthPassWritingStencil();
	PassDrawRenderState.SetDepthStencilState(GetCustomDepthStencilState(bWriteCustomStencilValues, PrimitiveSceneProxy->GetStencilWriteMask()));
	if (bWriteCustomStencilValues)
	{
		const uint32 CustomDepthStencilValue = PrimitiveSceneProxy->GetCustomDepthStencilValue();
		PassDrawRenderState.SetStencilRef(CustomDepthStencilValue);
	}

	// Using default material?
	bool bIgnoreThisMaterial = false;
	bool bPositionOnly = false;
	const bool bSupportPositionOnlyStream = MeshBatch.VertexFactory->SupportsPositionOnlyStream();
	const bool bVFTypeSupportsNullPixelShader = MeshBatch.VertexFactory->SupportsNullPixelShader();
	const bool bEvaluateWPO = Material.MaterialModifiesMeshPosition_RenderThread()
		&& (!ShouldOptimizedWPOAffectNonNaniteShaderSelection() || PrimitiveSceneProxy->EvaluateWorldPositionOffset());
	bool bUseDefaultMaterial = UseDefaultMaterial(Material, bEvaluateWPO, bSupportPositionOnlyStream, bVFTypeSupportsNullPixelShader, bPositionOnly, bIgnoreThisMaterial);
	if (bIgnoreThisMaterial)
	{
		return true;
	}
	// Swap to default material
	const FMaterialRenderProxy* EffectiveMaterialRenderProxy = &MaterialRenderProxy;
	const FMaterial* EffectiveMaterial = &Material;
	if (bUseDefaultMaterial)
	{
		// Override with the default material
		EffectiveMaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		EffectiveMaterial = EffectiveMaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		check(EffectiveMaterial);
	}

	// Get the fill & cull mode
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

	if (bPositionOnly)
	{
		return Process<true>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, *EffectiveMaterialRenderProxy, *EffectiveMaterial, MeshFillMode, MeshCullMode);
	}
	else
	{
		return Process<false>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, *EffectiveMaterialRenderProxy, *EffectiveMaterial, MeshFillMode, MeshCullMode);
	}
}

template<bool bPositionOnly>
bool FCustomDepthPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
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
		ShaderPipeline
		))
	{
		return false;
	}

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(DepthPassShaders.VertexShader, DepthPassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		DepthPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		bPositionOnly ? EMeshPassFeatures::PositionOnly : EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

void FCustomDepthPassMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Setup the depth stencil state to use
	const bool bWriteCustomStencilValues = IsCustomDepthPassWritingStencil();
	PassDrawRenderState.SetDepthStencilState(GetCustomDepthStencilState(bWriteCustomStencilValues, PreCacheParams.GetStencilWriteMask()));

	// Are we currently collecting PSO's for the default material
	if (PreCacheParams.bDefaultMaterial)
	{		
		CollectDefaultMaterialPSOInitializers(SceneTexturesConfig, Material, VertexFactoryData, PSOInitializers);
		return;
	}

	// assume we can always do this when collecting PSO's for now (vertex factory instance might actually not support it)
	const bool bSupportPositionOnlyStream = VertexFactoryData.VertexFactoryType->SupportsPositionOnly();
	const bool bVFTypeSupportsNullPixelShader = VertexFactoryData.VertexFactoryType->SupportsNullPixelShader();
	bool bIgnoreThisMaterial = false;
	bool bPositionOnly = false;
	bool bUseDefaultMaterial = UseDefaultMaterial(Material, Material.MaterialModifiesMeshPosition_GameThread(), bSupportPositionOnlyStream, bVFTypeSupportsNullPixelShader, bPositionOnly, bIgnoreThisMaterial);

	if (!bIgnoreThisMaterial)
	{
		const FMaterial* EffectiveMaterial = &Material;
		if (bUseDefaultMaterial && !bSupportPositionOnlyStream && VertexFactoryData.CustomDefaultVertexDeclaration)
		{
			EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
			EffectiveMaterial = UMaterial::GetDefaultMaterial(MD_Surface)->GetMaterialResource(FeatureLevel, ActiveQualityLevel);
			bUseDefaultMaterial = false;
		}		

		if (!bUseDefaultMaterial && PreCacheParams.bRenderCustomDepth)
		{
			check(!bPositionOnly);

			const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
			const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
			const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

			CollectPSOInitializers<false>(VertexFactoryData, *EffectiveMaterial, MeshFillMode, MeshCullMode, PSOInitializers);
		}
	}
}

void FCustomDepthPassMeshProcessor::CollectDefaultMaterialPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig, 
	const FMaterial& Material, 
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	const ERasterizerFillMode MeshFillMode = FM_Solid;

	// TODO: Should do this for each stencil write mask?
	
	// Collect PSOs for all possible default material combinations
	{
		ERasterizerCullMode MeshCullMode = CM_None;
		CollectPSOInitializers<true>(VertexFactoryData, Material, MeshFillMode, MeshCullMode, PSOInitializers);
		CollectPSOInitializers<false>(VertexFactoryData, Material, MeshFillMode, MeshCullMode, PSOInitializers);
	}
	{
		ERasterizerCullMode MeshCullMode = CM_CW;
		CollectPSOInitializers<true>(VertexFactoryData, Material, MeshFillMode, MeshCullMode, PSOInitializers);
		CollectPSOInitializers<false>(VertexFactoryData, Material, MeshFillMode, MeshCullMode, PSOInitializers);
	}
	{
		ERasterizerCullMode MeshCullMode = CM_CCW;
		CollectPSOInitializers<true>(VertexFactoryData, Material, MeshFillMode, MeshCullMode, PSOInitializers);
		CollectPSOInitializers<false>(VertexFactoryData, Material, MeshFillMode, MeshCullMode, PSOInitializers);
	}
}

template<bool bPositionOnly>
void FCustomDepthPassMeshProcessor::CollectPSOInitializers(
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode, 
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	TMeshProcessorShaders<
		TDepthOnlyVS<bPositionOnly>,
		FDepthOnlyPS> DepthPassShaders;

	FShaderPipelineRef ShaderPipeline;
	if (!GetDepthPassShaders<bPositionOnly>(
		MaterialResource,
		VertexFactoryData.VertexFactoryType,
		FeatureLevel,
		MaterialResource.MaterialUsesPixelDepthOffset_GameThread(),
		DepthPassShaders.VertexShader,
		DepthPassShaders.PixelShader,
		ShaderPipeline
		))
	{
		return;
	}

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	RenderTargetsInfo.NumSamples = 1;

	ETextureCreateFlags CustomDepthStencilCreateFlags = GFastVRamConfig.CustomDepth | TexCreate_NoFastClear | TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
	SetupDepthStencilInfo(PF_DepthStencil, CustomDepthStencilCreateFlags, ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite, RenderTargetsInfo);

	AddGraphicsPipelineStateInitializer(
		VertexFactoryData,
		MaterialResource,
		PassDrawRenderState,
		RenderTargetsInfo,
		DepthPassShaders,
		MeshFillMode,
		MeshCullMode,
		PT_TriangleList,
		bPositionOnly ? EMeshPassFeatures::PositionOnly : EMeshPassFeatures::Default,
		true /*bRequired*/,
		PSOInitializers);
}

FMeshPassProcessor* CreateCustomDepthPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	return new FCustomDepthPassMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(RegisterCustomDepthPass, CreateCustomDepthPassProcessor, EShadingPath::Deferred, EMeshPass::CustomDepth, EMeshPassFlags::MainView);
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(RegisterMobileCustomDepthPass, CreateCustomDepthPassProcessor, EShadingPath::Mobile, EMeshPass::CustomDepth, EMeshPassFlags::MainView);
