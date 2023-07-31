// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PostProcess/PostProcessCompositeEditorPrimitives.h"
#include "EditorPrimitivesRendering.h"
#include "MeshPassProcessor.inl"
#include "BasePassRendering.h"
#include "MobileBasePassRendering.h"
#include "SceneRenderingUtils.h"
#include "PixelShaderUtils.h"
#include "ScenePrivate.h"

namespace
{
TAutoConsoleVariable<int32> CVarEditorTemporalUpsampleDepth(
	TEXT("r.Editor.TemporalUpsampleDepth"), 2,
	TEXT("Temporal upsample factor of the depth buffer for depth testing editor primitives against."),
	ECVF_RenderThreadSafe);

class FTemporalUpsampleEditorDepthPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTemporalUpsampleEditorDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FTemporalUpsampleEditorDepthPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Depth)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PrevHistory)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, History)
		SHADER_PARAMETER(FVector2f, DepthTextureJitter)
		SHADER_PARAMETER(int32, bCameraCut)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VelocityTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VelocitySampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, PrevHistorySampler)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Only PC platforms render editor primitives.
		return IsPCPlatform(Parameters.Platform);
	}
};

class FPopulateEditorDepthPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPopulateEditorDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FPopulateEditorDepthPS, FGlobalShader);

	class FUseMSAADimension : SHADER_PERMUTATION_BOOL("USE_MSAA");
	using FPermutationDomain = TShaderPermutationDomain<FUseMSAADimension>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Color)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Depth)
		SHADER_PARAMETER(FVector2f, DepthTextureJitter)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const bool bUseMSAA = PermutationVector.Get<FUseMSAADimension>();

		// Only SM5+ platforms supports MSAA.
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && bUseMSAA)
		{
			return false;
		}

		// Only PC platforms render editor primitives.
		return IsPCPlatform(Parameters.Platform);
	}
};

class FCompositeEditorPrimitivesPS : public FEditorPrimitiveShader
{
public:
	DECLARE_GLOBAL_SHADER(FCompositeEditorPrimitivesPS);
	SHADER_USE_PARAMETER_STRUCT(FCompositeEditorPrimitivesPS, FEditorPrimitiveShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Color)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Depth)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_ARRAY(FVector4f, SampleOffsetArray, [FEditorPrimitiveShader::kEditorMSAASampleCountMax])
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EditorPrimitivesDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EditorPrimitivesColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
		SHADER_PARAMETER(uint32, bOpaqueEditorGizmo)
		SHADER_PARAMETER(uint32, bCompositeAnyNonNullDepth)
		SHADER_PARAMETER(FVector2f, DepthTextureJitter)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTemporalUpsampleEditorDepthPS, "/Engine/Private/PostProcessCompositeEditorPrimitives.usf", "MainTemporalUpsampleEditorDepthPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FPopulateEditorDepthPS, "/Engine/Private/PostProcessCompositeEditorPrimitives.usf", "MainPopulateSceneDepthPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FCompositeEditorPrimitivesPS, "/Engine/Private/PostProcessCompositeEditorPrimitives.usf", "MainCompositeEditorPrimitivesPS", SF_Pixel);

void RenderEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, FMeshPassProcessorRenderState& DrawRenderState, FInstanceCullingManager& InstanceCullingManager)
{
	// Always depth test against other editor primitives
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
		true, CF_DepthNearOrEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		0xFF, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1) | STENCIL_LIGHTING_CHANNELS_MASK(0x7)>::GetRHI());

	DrawDynamicMeshPass(View, RHICmdList,
		[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
	{
		FEditorPrimitivesBasePassMeshProcessor PassMeshProcessor(
			View.Family->Scene->GetRenderScene(),
			View.GetFeatureLevel(),
			&View,
			DrawRenderState,
			false,
			DynamicMeshPassContext);

		const uint64 DefaultBatchElementMask = ~0ull;
		const int32 NumDynamicEditorMeshBatches = View.DynamicEditorMeshElements.Num();

		for (int32 MeshIndex = 0; MeshIndex < NumDynamicEditorMeshBatches; MeshIndex++)
		{
			const FMeshBatchAndRelevance& MeshAndRelevance = View.DynamicEditorMeshElements[MeshIndex];

			if (MeshAndRelevance.GetHasOpaqueOrMaskedMaterial() || View.Family->EngineShowFlags.Wireframe)
			{
				PassMeshProcessor.AddMeshBatch(*MeshAndRelevance.Mesh, DefaultBatchElementMask, MeshAndRelevance.PrimitiveSceneProxy);
			}
		}

		for (int32 MeshIndex = 0; MeshIndex < View.ViewMeshElements.Num(); MeshIndex++)
		{
			const FMeshBatch& MeshBatch = View.ViewMeshElements[MeshIndex];
			PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
		}
	});

	View.EditorSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_World);

	const auto FeatureLevel = View.GetFeatureLevel();
	const auto ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];

	// Draw the view's batched simple elements(lines, sprites, etc).
	View.BatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, View, false, 1.0f);
}

void RenderForegroundTranslucentEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, FMeshPassProcessorRenderState& DrawRenderState, FInstanceCullingManager& InstanceCullingManager)
{
	const auto FeatureLevel = View.GetFeatureLevel();
	const auto ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];

	// Force all translucent editor primitives to standard translucent rendering
	const ETranslucencyPass::Type TranslucencyPass = ETranslucencyPass::TPT_StandardTranslucency;

	if (TranslucencyPass == ETranslucencyPass::TPT_StandardTranslucency)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	}

	View.EditorSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::Translucent, SDPG_Foreground);

	DrawDynamicMeshPass(View, RHICmdList,
		[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
		{
			FEditorPrimitivesBasePassMeshProcessor PassMeshProcessor(
				View.Family->Scene->GetRenderScene(),
				View.GetFeatureLevel(),
				&View,
				DrawRenderState,
				true,
				DynamicMeshPassContext);

			const uint64 DefaultBatchElementMask = ~0ull;

			for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
			{
				const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
				PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
			}
		});
	
	View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, View, false, 1.0f, EBlendModeFilter::Translucent);
}

void RenderForegroundEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, FMeshPassProcessorRenderState& DrawRenderState, FInstanceCullingManager& InstanceCullingManager)
{
	const auto FeatureLevel = View.GetFeatureLevel();
	const auto ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];

	// Draw a first time the foreground primitive without depth test to over right depth from non-foreground editor primitives.
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());

		View.EditorSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_Foreground);

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
		{
			FEditorPrimitivesBasePassMeshProcessor PassMeshProcessor(
				View.Family->Scene->GetRenderScene(),
				View.GetFeatureLevel(),
				&View,
				DrawRenderState,
				false,
				DynamicMeshPassContext);

			const uint64 DefaultBatchElementMask = ~0ull;

			for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
			{
				const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
				PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
			}
		});

		View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, View, false);
	}

	// Draw a second time the foreground primitive with depth test to have proper depth test between foreground primitives.
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

		View.EditorSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_Foreground);

		DrawDynamicMeshPass(View, RHICmdList,
			[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
		{
			FEditorPrimitivesBasePassMeshProcessor PassMeshProcessor(
				View.Family->Scene->GetRenderScene(),
				View.GetFeatureLevel(),
				&View,
				DrawRenderState,
				false,
				DynamicMeshPassContext);

			const uint64 DefaultBatchElementMask = ~0ull;

			for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
			{
				const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
				PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
			}
		});

		View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, View, false);
	}
}

} //! namespace

const FViewInfo* CreateEditorPrimitiveView(const FViewInfo& ParentView, FIntRect ViewRect, uint32 NumSamples)
{
	FViewInfo* EditorView = ParentView.CreateSnapshot();

	// Patch view rect.
	EditorView->ViewRect = ViewRect;

	// Override pre exposure to 1.0f, because rendering after tonemapper. 
	EditorView->PreExposure = 1.0f;

	// Kills material texture mipbias because after TAA.
	EditorView->MaterialTextureMipBias = 0.0f;

	// Disable decals so that we don't do a SetDepthStencilState() in TMobileBasePassDrawingPolicy::SetupPipelineState()
	EditorView->bSceneHasDecals = false;

	if (IsTemporalAccumulationBasedMethod(EditorView->AntiAliasingMethod))
	{
		EditorView->ViewMatrices.HackRemoveTemporalAAProjectionJitter();
	}

	EditorView->InitRHIResources(NumSamples);

	return EditorView;
}

BEGIN_SHADER_PARAMETER_STRUCT(FEditorPrimitivesPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
	SHADER_PARAMETER_STRUCT_REF(FMobileReflectionCaptureShaderData, MobileReflectionCaptureData)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, BasePass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FTranslucentBasePassUniformParameters, TranslucentBasePass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileBasePassUniformParameters, MobileBasePass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FInstanceCullingGlobalUniforms, InstanceCulling)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

FScreenPassTexture AddEditorPrimitivePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEditorPrimitiveInputs& Inputs,
	FInstanceCullingManager& InstanceCullingManager)
{
	check(Inputs.SceneColor.IsValid());
	check(Inputs.SceneDepth.IsValid());
	check(Inputs.BasePassType != FEditorPrimitiveInputs::EBasePassType::MAX);

	const FSceneTextures& SceneTextures = View.GetSceneTextures();
	const uint32 NumSamples = SceneTextures.Config.EditorPrimitiveNumSamples;
	const FViewInfo* EditorView = CreateEditorPrimitiveView(View, Inputs.SceneColor.ViewRect, NumSamples);

	// Load the color target if it already exists.
	const bool bProducedByPriorPass = HasBeenProduced(SceneTextures.EditorPrimitiveColor);

	FRDGTextureRef EditorPrimitiveColor;
	FRDGTextureRef EditorPrimitiveDepth;
	if (bProducedByPriorPass)
	{
		ensureMsgf(
			Inputs.SceneColor.ViewRect == Inputs.SceneDepth.ViewRect,
			TEXT("Temporal upsampling should be disabled when drawing directly to EditorPrimitivesColor."));
		EditorPrimitiveColor = SceneTextures.EditorPrimitiveColor;
		EditorPrimitiveDepth = SceneTextures.EditorPrimitiveDepth;
	}
	else
	{
		const FSceneTexturesConfig& Config = SceneTextures.Config;

		FIntPoint Extent = Inputs.SceneColor.Texture->Desc.Extent;

		const FRDGTextureDesc ColorDesc = FRDGTextureDesc::Create2D(
			Extent,
			PF_B8G8R8A8,
			FClearValueBinding::Transparent,
			TexCreate_ShaderResource | TexCreate_RenderTargetable,
			1,
			Config.EditorPrimitiveNumSamples);

		const FRDGTextureDesc DepthDesc = FRDGTextureDesc::Create2D(
			Extent,
			PF_DepthStencil,
			FClearValueBinding::DepthFar,
			TexCreate_ShaderResource | TexCreate_DepthStencilTargetable,
			1,
			Config.EditorPrimitiveNumSamples);

		EditorPrimitiveColor = GraphBuilder.CreateTexture(ColorDesc, TEXT("Editor.PrimitivesColor"));
		EditorPrimitiveDepth = GraphBuilder.CreateTexture(DepthDesc, TEXT("Editor.PrimitivesDepth"));
	}

	// Load the color target if it already exists.
	const FScreenPassTextureViewport EditorPrimitivesViewport(EditorPrimitiveColor, Inputs.SceneColor.ViewRect);

	RDG_GPU_STAT_SCOPE(GraphBuilder, EditorPrimitives);
	RDG_EVENT_SCOPE(GraphBuilder, "CompositeEditorPrimitives %dx%d MSAA=%d",
		EditorPrimitivesViewport.Rect.Width(),
		EditorPrimitivesViewport.Rect.Height(),
		NumSamples);

	FScreenPassTexture SceneDepth = Inputs.SceneDepth;
	FVector2f SceneDepthJitter = FVector2f(View.TemporalJitterPixels);

	// The editor primitive composition pass is also used when rendering VMI_WIREFRAME in order to use MSAA.
	// So we need to check whether the editor primitives are enabled inside this function.
	if (View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		// Populate depth if a prior pass did not already do it.
		if (!bProducedByPriorPass)
		{
			// Upscale factor of the depth buffer that might be needed.
			const float UpscaleFactor = float(EditorPrimitivesViewport.Rect.Width()) / float(Inputs.SceneDepth.ViewRect.Width());

			// Upscale factor shouldn't be higher than there is TAA samples, or that means there will be unrendered pixels.
			const int32 ComputeMaxUpsampleFactorDueToTAA = FMath::FloorToInt(FMath::Sqrt(float(View.TemporalJitterSequenceLength) / (UpscaleFactor * UpscaleFactor)));
			
			const int32 ComputeMaxUpsampleFactor = FMath::Clamp(ComputeMaxUpsampleFactorDueToTAA, 0, 4);

			const int32 DepthUpsampleFactor = FMath::Clamp(CVarEditorTemporalUpsampleDepth.GetValueOnRenderThread(), 0, ComputeMaxUpsampleFactor);

			// Upsample the depth at higher resolution to reduce depth intersection instability of editor primitives.
			if (DepthUpsampleFactor > 0 && View.ViewState && IsTemporalAccumulationBasedMethod(View.AntiAliasingMethod))
			{
				FScreenPassTexture History;
				{
					const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
						EditorPrimitivesViewport.Extent * DepthUpsampleFactor,
						PF_R32_FLOAT,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_RenderTargetable);

					History.Texture = GraphBuilder.CreateTexture(Desc, TEXT("Editor.PrimitivesDepthHistory"));
					History.ViewRect = EditorPrimitivesViewport.Rect * DepthUpsampleFactor;
				}

				FTemporalUpsampleEditorDepthPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTemporalUpsampleEditorDepthPS::FParameters>();
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->Depth = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Inputs.SceneDepth));
				PassParameters->History = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(History));
				PassParameters->DepthTextureJitter = FVector2f(View.TemporalJitterPixels);

				PassParameters->DepthTexture = Inputs.SceneDepth.Texture;
				PassParameters->DepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

				// TODO
				PassParameters->VelocityTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
				PassParameters->VelocitySampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

				if (View.PrevViewInfo.EditorPrimtiveDepthHistory.IsValid())
				{
					PassParameters->PrevHistory = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
						View.PrevViewInfo.EditorPrimtiveDepthHistory.RT[0]->GetDesc().Extent, View.PrevViewInfo.EditorPrimtiveDepthHistory.ViewportRect));
					PassParameters->PrevHistoryTexture = GraphBuilder.RegisterExternalTexture(View.PrevViewInfo.EditorPrimtiveDepthHistory.RT[0]);
					PassParameters->PrevHistorySampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
					PassParameters->bCameraCut = false;
				}
				else
				{
					PassParameters->PrevHistory = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
						FIntPoint(1, 1), FIntRect(FIntPoint(0, 0), FIntPoint(1, 1))));
					PassParameters->PrevHistoryTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
					PassParameters->PrevHistorySampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
					PassParameters->bCameraCut = true;
				}

				PassParameters->RenderTargets[0] = FRenderTargetBinding(History.Texture, ERenderTargetLoadAction::ENoAction);

				TShaderMapRef<FTemporalUpsampleEditorDepthPS> PixelShader(View.ShaderMap);
				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder,
					View.ShaderMap,
					RDG_EVENT_NAME("TemporalUpsampleDepth %dx%d -> %dx%d",
						Inputs.SceneDepth.ViewRect.Width(),
						Inputs.SceneDepth.ViewRect.Height(),
						History.ViewRect.Width(),
						History.ViewRect.Height()),
					PixelShader,
					PassParameters,
					History.ViewRect);

				if (!View.bStatePrevViewInfoIsReadOnly)
				{
					FTemporalAAHistory* OutputHistory = &View.ViewState->PrevFrameViewInfo.EditorPrimtiveDepthHistory;
					OutputHistory->SafeRelease();

					GraphBuilder.QueueTextureExtraction(History.Texture, &OutputHistory->RT[0]);
					OutputHistory->ViewportRect = History.ViewRect;
					OutputHistory->ReferenceBufferSize = History.Texture->Desc.Extent;
				}

				SceneDepth = History;
				SceneDepthJitter = FVector2f::ZeroVector;
			}

			// Populate the MSAA depth buffer from depth buffer or temporally upscaled depth buffer
			{
				FPopulateEditorDepthPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPopulateEditorDepthPS::FParameters>();
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->Color = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Inputs.SceneColor));
				PassParameters->Depth = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(SceneDepth));
				PassParameters->DepthTextureJitter = SceneDepthJitter;
				PassParameters->DepthTexture = SceneDepth.Texture;
				PassParameters->DepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
				PassParameters->RenderTargets[0] = FRenderTargetBinding(EditorPrimitiveColor, ERenderTargetLoadAction::EClear);
				PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(EditorPrimitiveDepth, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear, FExclusiveDepthStencil::DepthWrite_StencilWrite);

				FPopulateEditorDepthPS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FPopulateEditorDepthPS::FUseMSAADimension>(NumSamples > 1);
				TShaderMapRef<FPopulateEditorDepthPS> PixelShader(View.ShaderMap, PermutationVector);

				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder,
					View.ShaderMap,
					RDG_EVENT_NAME("PopulateDepth %dx%d%s",
						EditorPrimitivesViewport.Rect.Width(),
						EditorPrimitivesViewport.Rect.Height(),
						NumSamples > 1 ? TEXT(" MSAA") : TEXT("")),
					PixelShader,
					PassParameters,
					EditorPrimitivesViewport.Rect,
					/* BlendState = */ nullptr,
					/* RasterizerState = */ nullptr,
					TStaticDepthStencilState<true, CF_Always>::GetRHI());
			}
		}

		// Draws the editors opaque primitives
		{
			FEditorPrimitivesPassParameters* PassParameters = GraphBuilder.AllocParameters<FEditorPrimitivesPassParameters>();
			PassParameters->View = EditorView->GetShaderParameters();
			PassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
			PassParameters->MobileReflectionCaptureData = View.MobileReflectionCaptureUniformBuffer;
			PassParameters->InstanceCulling = InstanceCullingManager.GetDummyInstanceCullingUniformBuffer();
			PassParameters->RenderTargets[0] = FRenderTargetBinding(EditorPrimitiveColor, ERenderTargetLoadAction::ELoad);
			PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(EditorPrimitiveDepth, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

			const FEditorPrimitiveInputs::EBasePassType BasePassType = Inputs.BasePassType;

			if (BasePassType == FEditorPrimitiveInputs::EBasePassType::Deferred)
			{
				PassParameters->BasePass = CreateOpaqueBasePassUniformBuffer(GraphBuilder, *EditorView, 0);
			}
			else
			{
				PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, *EditorView, EMobileBasePass::Translucent, EMobileSceneTextureSetupMode::None);
			}

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("EditorPrimitives %dx%d MSAA=%d",
					EditorPrimitivesViewport.Rect.Width(),
					EditorPrimitivesViewport.Rect.Height(),
					NumSamples),
				PassParameters,
				ERDGPassFlags::Raster,
				[&View, &InstanceCullingManager, PassParameters, EditorView, EditorPrimitivesViewport, BasePassType, NumSamples](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(EditorPrimitivesViewport.Rect.Min.X, EditorPrimitivesViewport.Rect.Min.Y, 0.0f, EditorPrimitivesViewport.Rect.Max.X, EditorPrimitivesViewport.Rect.Max.Y, 1.0f);

				FMeshPassProcessorRenderState DrawRenderState;
				DrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite);
				DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());

				// Draw editor primitives.
				{
					SCOPED_DRAW_EVENTF(RHICmdList, EditorPrimitives,
						TEXT("RenderViewEditorPrimitives %dx%d msaa=%d"),
						EditorPrimitivesViewport.Rect.Width(), EditorPrimitivesViewport.Rect.Height(), NumSamples);

					RenderEditorPrimitives(RHICmdList, *EditorView, DrawRenderState, InstanceCullingManager);
				}

				// Draw foreground editor primitives.
				{
					SCOPED_DRAW_EVENTF(RHICmdList, EditorPrimitives,
						TEXT("RenderViewEditorForegroundPrimitives %dx%d msaa=%d"),
						EditorPrimitivesViewport.Rect.Width(), EditorPrimitivesViewport.Rect.Height(), NumSamples);

					RenderForegroundEditorPrimitives(RHICmdList, *EditorView, DrawRenderState, InstanceCullingManager);
				}
			});
		}
	}

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;

	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, Inputs.SceneColor, View.GetOverwriteLoadAction(), TEXT("Editor.Primitives"));
	}

	{
		FRHISamplerState* PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		const bool bOpaqueEditorGizmo = View.Family->EngineShowFlags.OpaqueCompositeEditorPrimitives || View.Family->EngineShowFlags.Wireframe;

		FCompositeEditorPrimitivesPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompositeEditorPrimitivesPS::FParameters>();
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Color = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Inputs.SceneColor));
		PassParameters->Depth = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(SceneDepth));
		PassParameters->Output = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Output));
		PassParameters->ColorTexture = Inputs.SceneColor.Texture;
		PassParameters->ColorSampler = PointClampSampler;
		PassParameters->DepthTexture = SceneDepth.Texture;
		PassParameters->DepthSampler = PointClampSampler;
		PassParameters->EditorPrimitivesDepth = EditorPrimitiveDepth;
		PassParameters->EditorPrimitivesColor = EditorPrimitiveColor;
		PassParameters->bOpaqueEditorGizmo = bOpaqueEditorGizmo;
		PassParameters->bCompositeAnyNonNullDepth = bProducedByPriorPass;
		PassParameters->DepthTextureJitter = SceneDepthJitter;

		for (int32 i = 0; i < int32(NumSamples); i++)
		{
			PassParameters->SampleOffsetArray[i].X = GetMSAASampleOffsets(NumSamples, i).X;
			PassParameters->SampleOffsetArray[i].Y = GetMSAASampleOffsets(NumSamples, i).Y;
		}

		FCompositeEditorPrimitivesPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCompositeEditorPrimitivesPS::FSampleCountDimension>(NumSamples);

		TShaderMapRef<FCompositeEditorPrimitivesPS> PixelShader(View.ShaderMap, PermutationVector);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			View.ShaderMap,
			RDG_EVENT_NAME("Composite %dx%d MSAA=%d", Output.ViewRect.Width(), Output.ViewRect.Height(), NumSamples),
			PixelShader,
			PassParameters,
			Output.ViewRect);
	}

	// Draws the editor translucent primitives on top of the opaque scene primitives
	if (View.Family->EngineShowFlags.CompositeEditorPrimitives && View.bHasTranslucentViewMeshElements)
	{
		FEditorPrimitivesPassParameters* PassParameters = GraphBuilder.AllocParameters<FEditorPrimitivesPassParameters>();
		PassParameters->View = EditorView->GetShaderParameters();
		PassParameters->ReflectionCapture = View.ReflectionCaptureUniformBuffer;
		PassParameters->MobileReflectionCaptureData = View.MobileReflectionCaptureUniformBuffer;
		PassParameters->InstanceCulling = InstanceCullingManager.GetDummyInstanceCullingUniformBuffer();
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		PassParameters->RenderTargets[0].SetLoadAction(ERenderTargetLoadAction::ELoad);

		const FEditorPrimitiveInputs::EBasePassType BasePassType = Inputs.BasePassType;

		if (BasePassType == FEditorPrimitiveInputs::EBasePassType::Deferred)
		{
			PassParameters->TranslucentBasePass = CreateTranslucentBasePassUniformBuffer(GraphBuilder, nullptr, *EditorView, 0);
		}
		else
		{
			PassParameters->MobileBasePass = CreateMobileBasePassUniformBuffer(GraphBuilder, *EditorView, EMobileBasePass::Translucent, EMobileSceneTextureSetupMode::None);
		}

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("EditorPrimitives Translucent %dx%d MSAA=%d",
				EditorPrimitivesViewport.Rect.Width(),
				EditorPrimitivesViewport.Rect.Height(),
				NumSamples),
			PassParameters,
			ERDGPassFlags::Raster,
			[&View, &InstanceCullingManager, PassParameters, EditorView, EditorPrimitivesViewport, OutputViewportRect = Output.ViewRect, BasePassType, NumSamples](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.SetViewport(OutputViewportRect.Min.X, OutputViewportRect.Min.Y, 0.0f, OutputViewportRect.Max.X, OutputViewportRect.Max.Y, 1.0f);

				FMeshPassProcessorRenderState DrawRenderState;
				DrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilNop);
				DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());

				// Draw foreground editor primitives.
				{
					SCOPED_DRAW_EVENTF(RHICmdList, EditorPrimitives,
						TEXT("RenderViewEditorForegroundTranslucentPrimitives %dx%d msaa=%d"),
						EditorPrimitivesViewport.Rect.Width(), EditorPrimitivesViewport.Rect.Height(), NumSamples);

					RenderForegroundTranslucentEditorPrimitives(RHICmdList, *EditorView, DrawRenderState, InstanceCullingManager);
				}
			});
	}

	return MoveTemp(Output);
}

#endif