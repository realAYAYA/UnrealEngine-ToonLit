// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenFrontLayerTranslucency.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "MeshPassProcessor.inl"
#include "PixelShaderUtils.h"
#include "Lumen/LumenSceneData.h"
#include "Lumen/LumenTracingUtils.h"

// Whether to enable Front Layer Translucency reflections from scalability
int32 GLumenFrontLayerTranslucencyReflectionsEnabled = 0;
FAutoConsoleVariableRef CVarLumenTranslucencyReflectionsFrontLayerEnabled(
	TEXT("r.Lumen.TranslucencyReflections.FrontLayer.Enable"),
	GLumenFrontLayerTranslucencyReflectionsEnabled,
	TEXT("Whether to render Lumen Reflections on the frontmost layer of Translucent Surfaces.  Other layers will use the lower quality Radiance Cache method that can only produce glossy reflections."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

// Note: Driven by URendererSettings
static TAutoConsoleVariable<int32> CVarLumenFrontLayerTranslucencyReflectionsEnabledForProject(
	TEXT("r.Lumen.TranslucencyReflections.FrontLayer.EnableForProject"),
	0,
	TEXT("Whether to render Lumen Reflections on the frontmost layer of Translucent Surfaces.  Other layers will use the lower quality Radiance Cache method that can only produce glossy reflections."),
	ECVF_RenderThreadSafe
);

// Whether the user setting should be respected based on the current scalability level
int32 GLumenFrontLayerTranslucencyReflectionsAllowed = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyReflectionsFrontLayerAllowed(
	TEXT("r.Lumen.TranslucencyReflections.FrontLayer.Allow"),
	GLumenFrontLayerTranslucencyReflectionsAllowed,
	TEXT("Whether to render Lumen Reflections on the frontmost layer of Translucent Surfaces.  Other layers will use the lower quality Radiance Cache method that can only produce glossy reflections."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenFrontLayerDepthThreshold(
	TEXT("r.Lumen.TranslucencyReflections.FrontLayer.DepthThreshold"),
	1024.0f,
	TEXT("Depth test threshold used to determine whether the fragments being rendered match the single layer that reflections were calculated for. In float ULP units."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool IsVSMTranslucentHighQualityEnabled();

namespace Lumen
{
	bool UseLumenFrontLayerTranslucencyReflections(const FViewInfo& View)
	{
		return (View.FinalPostProcessSettings.LumenFrontLayerTranslucencyReflections || GLumenFrontLayerTranslucencyReflectionsEnabled)
			&& GLumenFrontLayerTranslucencyReflectionsAllowed != 0 
			&& View.Family->EngineShowFlags.LumenReflections;
	}

	bool ShouldRenderInFrontLayerTranslucencyGBufferPass(bool bShouldRenderInMainPass, const FMaterial& Material)
	{
		return bShouldRenderInMainPass
			&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain())
			&& Material.IsTranslucencyWritingFrontLayerTransparency()
			&& !Material.IsTranslucencyAfterMotionBlurEnabled(); // see MTP_AfterMotionBlur
	}
}

class FLumenFrontLayerTranslucencyClearGBufferPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenFrontLayerTranslucencyClearGBufferPS);
	SHADER_USE_PARAMETER_STRUCT(FLumenFrontLayerTranslucencyClearGBufferPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenFrontLayerTranslucencyClearGBufferPS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "LumenFrontLayerTranslucencyClearGBufferPS", SF_Pixel);


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLumenFrontLayerTranslucencyGBufferPassUniformParameters, )
	SHADER_PARAMETER_STRUCT(FSceneTextureUniformParameters, SceneTextures)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FLumenFrontLayerTranslucencyGBufferPassUniformParameters, "LumenFrontLayerTranslucencyGBufferPass", SceneTextures);

class FLumenFrontLayerTranslucencyGBufferVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenFrontLayerTranslucencyGBufferVS, MeshMaterial);

protected:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform) && IsTranslucentBlendMode(Parameters.MaterialParameters) && Parameters.MaterialParameters.bIsTranslucencySurface;
	}

	FLumenFrontLayerTranslucencyGBufferVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FLumenFrontLayerTranslucencyGBufferVS() = default;
};


IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenFrontLayerTranslucencyGBufferVS, TEXT("/Engine/Private/Lumen/LumenFrontLayerTranslucency.usf"), TEXT("MainVS"), SF_Vertex);

class FLumenFrontLayerTranslucencyGBufferPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenFrontLayerTranslucencyGBufferPS, MeshMaterial);

public:
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform) && IsTranslucentBlendMode(Parameters.MaterialParameters) && Parameters.MaterialParameters.bIsTranslucencySurface;
	}

	FLumenFrontLayerTranslucencyGBufferPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FLumenFrontLayerTranslucencyGBufferPS() = default;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenFrontLayerTranslucencyGBufferPS, TEXT("/Engine/Private/Lumen/LumenFrontLayerTranslucency.usf"), TEXT("MainPS"), SF_Pixel);

class FLumenFrontLayerTranslucencyGBufferMeshProcessor : public FSceneRenderingAllocatorObject<FLumenFrontLayerTranslucencyGBufferMeshProcessor>, public FMeshPassProcessor
{
public:

	FLumenFrontLayerTranslucencyGBufferMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override final;

	FMeshPassProcessorRenderState PassDrawRenderState;
};

bool GetLumenFrontLayerTranslucencyGBufferShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	TShaderRef<FLumenFrontLayerTranslucencyGBufferVS>& VertexShader,
	TShaderRef<FLumenFrontLayerTranslucencyGBufferPS>& PixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLumenFrontLayerTranslucencyGBufferVS>();
	ShaderTypes.AddShaderType<FLumenFrontLayerTranslucencyGBufferPS>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

bool CanMaterialRenderInLumenFrontLayerTranslucencyGBufferPass(
	const FScene& Scene,
	const FSceneViewFamily& ViewFamily,
	const FPrimitiveSceneProxy& PrimitiveSceneProxy,
	const FMaterial& Material)
{
	const FSceneView* View = ViewFamily.Views[0];
	check(View);

	return ShouldRenderLumenReflections(*View) && Lumen::ShouldRenderInFrontLayerTranslucencyGBufferPass(PrimitiveSceneProxy.ShouldRenderInMainPass(), Material);
}

void FLumenFrontLayerTranslucencyGBufferMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (MeshBatch.bUseForMaterial 
		&& PrimitiveSceneProxy
		&& ViewIfDynamicMeshCommand
		//@todo - this filter should be done at a higher level
		&& ShouldRenderLumenReflections(*ViewIfDynamicMeshCommand))
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material)
			{
				auto TryAddMeshBatch = [this](const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& Material) -> bool
				{
					if (Lumen::ShouldRenderInFrontLayerTranslucencyGBufferPass(PrimitiveSceneProxy->ShouldRenderInMainPass(), Material))
					{
						const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
						FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

						TMeshProcessorShaders<
							FLumenFrontLayerTranslucencyGBufferVS,
							FLumenFrontLayerTranslucencyGBufferPS> PassShaders;

						if (!GetLumenFrontLayerTranslucencyGBufferShaders(
							Material,
							VertexFactory->GetType(),
							PassShaders.VertexShader,
							PassShaders.PixelShader))
						{
							return false;
						}

						FMeshMaterialShaderElementData ShaderElementData;
						ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

						const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
						const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
						const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);
						const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

						BuildMeshDrawCommands(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							PassDrawRenderState,
							PassShaders,
							MeshFillMode,
							MeshCullMode,
							SortKey,
							EMeshPassFeatures::Default,
							ShaderElementData);
					}

					return true;
				};

				if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
				{
					break;
				}
			}

			MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
		}
	}
}

void FLumenFrontLayerTranslucencyGBufferMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	LLM_SCOPE_BYTAG(Lumen);
		
	if (PreCacheParams.bRenderInMainPass && !Lumen::ShouldRenderInFrontLayerTranslucencyGBufferPass(PreCacheParams.bRenderInMainPass, Material))
	{
		return;
	}

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

	TMeshProcessorShaders<
		FLumenFrontLayerTranslucencyGBufferVS,
		FLumenFrontLayerTranslucencyGBufferPS> PassShaders;

	if (!GetLumenFrontLayerTranslucencyGBufferShaders(
		Material,
		VertexFactoryData.VertexFactoryType,
		PassShaders.VertexShader,
		PassShaders.PixelShader))
	{
		return;
	}

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	RenderTargetsInfo.NumSamples = 1;
	AddRenderTargetInfo(PF_FloatRGBA, TexCreate_ShaderResource | TexCreate_RenderTargetable, RenderTargetsInfo);
	ETextureCreateFlags DepthStencilCreateFlags = SceneTexturesConfig.DepthCreateFlags;
	SetupDepthStencilInfo(PF_DepthStencil, DepthStencilCreateFlags, ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ENoAction, FExclusiveDepthStencil::DepthWrite_StencilNop, RenderTargetsInfo);

	AddGraphicsPipelineStateInitializer(
		VertexFactoryData,
		Material,
		PassDrawRenderState,
		RenderTargetsInfo,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		(EPrimitiveType)PreCacheParams.PrimitiveType,
		EMeshPassFeatures::Default,
		true /*bRequired*/,
		PSOInitializers);
}

FLumenFrontLayerTranslucencyGBufferMeshProcessor::FLumenFrontLayerTranslucencyGBufferMeshProcessor(const FScene* Scene,	ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::LumenFrontLayerTranslucencyGBuffer, Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
{}

FMeshPassProcessor* CreateLumenFrontLayerTranslucencyGBufferPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	LLM_SCOPE_BYTAG(Lumen);

	FMeshPassProcessorRenderState PassState;

	PassState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

	PassState.SetBlendState(TStaticBlendState<>::GetRHI());

	return new FLumenFrontLayerTranslucencyGBufferMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassState, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(LumenFrontLayerTranslucencyGBufferPass, CreateLumenFrontLayerTranslucencyGBufferPassProcessor, EShadingPath::Deferred, EMeshPass::LumenFrontLayerTranslucencyGBuffer, EMeshPassFlags::MainView);

BEGIN_SHADER_PARAMETER_STRUCT(FLumenFrontLayerTranslucencyGBufferPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenFrontLayerTranslucencyGBufferPassUniformParameters, GBufferPass)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void RenderFrontLayerTranslucencyGBuffer(
	FRDGBuilder& GraphBuilder,
	const FSceneRenderer& SceneRenderer,
	FViewInfo& View,
	const FSceneTextures& SceneTextures,
	const FFrontLayerTranslucencyData& FrontLayerTranslucencyData)
{
	const EMeshPass::Type MeshPass = EMeshPass::LumenFrontLayerTranslucencyGBuffer;
	const float ViewportScale = 1.0f;
	FIntRect GBufferViewRect = GetScaledRect(View.ViewRect, ViewportScale);

	View.BeginRenderView();

	FLumenFrontLayerTranslucencyGBufferPassParameters* PassParameters = GraphBuilder.AllocParameters<FLumenFrontLayerTranslucencyGBufferPassParameters>();

	PassParameters->RenderTargets[0] = FRenderTargetBinding(FrontLayerTranslucencyData.Normal, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(FrontLayerTranslucencyData.SceneDepth, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilNop);

	{
		FViewUniformShaderParameters DownsampledTranslucencyViewParameters = *View.CachedViewUniformShaderParameters;

		FViewMatrices ViewMatrices = View.ViewMatrices;
		FViewMatrices PrevViewMatrices = View.PrevViewInfo.ViewMatrices;

		// Update the parts of DownsampledTranslucencyParameters which are dependent on the buffer size and view rect
		View.SetupViewRectUniformBufferParameters(
			DownsampledTranslucencyViewParameters,
			SceneTextures.Config.Extent,
			GBufferViewRect,
			ViewMatrices,
			PrevViewMatrices);

		PassParameters->View.View = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(DownsampledTranslucencyViewParameters, UniformBuffer_SingleFrame);
		
		if (View.bShouldBindInstancedViewUB)
		{
			FInstancedViewUniformShaderParameters LocalInstancedViewUniformShaderParameters;
			InstancedViewParametersUtils::CopyIntoInstancedViewParameters(LocalInstancedViewUniformShaderParameters, DownsampledTranslucencyViewParameters, 0);

			if (const FViewInfo* InstancedView = View.GetInstancedView())
			{
				InstancedView->SetupViewRectUniformBufferParameters(
					DownsampledTranslucencyViewParameters,
					SceneTextures.Config.Extent,
					GetScaledRect(InstancedView->ViewRect, ViewportScale),
					ViewMatrices,
					PrevViewMatrices);

				InstancedViewParametersUtils::CopyIntoInstancedViewParameters(LocalInstancedViewUniformShaderParameters, DownsampledTranslucencyViewParameters, 1);
			}

			PassParameters->View.InstancedView = TUniformBufferRef<FInstancedViewUniformShaderParameters>::CreateUniformBufferImmediate(
				reinterpret_cast<const FInstancedViewUniformShaderParameters&>(LocalInstancedViewUniformShaderParameters),
				UniformBuffer_SingleFrame);
		}
	}

	{
		FLumenFrontLayerTranslucencyGBufferPassUniformParameters& GBufferPassParameters = *GraphBuilder.AllocParameters<FLumenFrontLayerTranslucencyGBufferPassUniformParameters>();
		SetupSceneTextureUniformParameters(GraphBuilder, &SceneTextures, View.FeatureLevel, ESceneTextureSetupMode::All, GBufferPassParameters.SceneTextures);

		PassParameters->GBufferPass = GraphBuilder.CreateUniformBuffer(&GBufferPassParameters);
	}

	View.ParallelMeshDrawCommandPasses[MeshPass].BuildRenderingCommands(GraphBuilder, SceneRenderer.Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("TranslucencyGBuffer"),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, &SceneRenderer, MeshPass, PassParameters, ViewportScale, GBufferViewRect](FRHICommandListImmediate& RHICmdList)
	{
		FSceneRenderer::SetStereoViewport(RHICmdList, View, ViewportScale);
		View.ParallelMeshDrawCommandPasses[MeshPass].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
	});

	// Extract front layer depth depth (only needed when VSM high quality shadow on translucency is enabled)
	if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly && IsVSMTranslucentHighQualityEnabled())
	{
		// Queue updating the view state's render target reference with the new values
		GraphBuilder.QueueTextureExtraction(FrontLayerTranslucencyData.SceneDepth, &View.ViewState->Lumen.TranslucentReflectionState.DepthHistoryRT);
		GraphBuilder.QueueTextureExtraction(FrontLayerTranslucencyData.Normal, &View.ViewState->Lumen.TranslucentReflectionState.NormalHistoryRT);
	}
}

bool FDeferredShadingSceneRenderer::IsLumenFrontLayerTranslucencyEnabled(const FViewInfo& View) const
{ 
	return View.bTranslucentSurfaceLighting && GetViewPipelineState(View).ReflectionsMethod == EReflectionsMethod::Lumen && Lumen::UseLumenFrontLayerTranslucencyReflections(View);
}

bool IsLumenFrontLayerHistoryValid(const FViewInfo& View)
{ 
	return View.ViewState && 
		View.ViewState->PrevFrameNumber == View.ViewState->Lumen.TranslucentReflectionState.HistoryFrameIndex && 
		View.ViewState->Lumen.TranslucentReflectionState.DepthHistoryRT != nullptr && 
		View.ViewState->Lumen.TranslucentReflectionState.NormalHistoryRT != nullptr;
}

FFrontLayerTranslucencyData FDeferredShadingSceneRenderer::RenderFrontLayerTranslucency(
	FRDGBuilder& GraphBuilder,
	TArray<FViewInfo>& InViews,
	const FSceneTextures& SceneTextures, 
	bool bRenderOnlyForVSMPageMarking)
{
	// bNeedFrontLayerData:
	// 0 : No front layer
	// 1 : Front layer data requested by Lumen reflection
	// 2 : Front layer data requested by VSM page marking
	uint32 bNeedFrontLayerData = 0;
	if (bRenderOnlyForVSMPageMarking)
	{
		if (IsVSMTranslucentHighQualityEnabled())
		{
			// Front layer translucency data can be used from different sources (in priority order):
			// * Lumen front layer History data
			// * Skipped if Lumen will render front layer the same frame (as Translucent 'after opaque' might be rendered, e.g. particle, and if we render the front layer here, they won't be present)
			// * Render local front layer data here
			bool bValidHistory = false;
			bool bRenderLumenFrontLayer = false;
			for (FViewInfo& View : InViews)
			{
				bValidHistory = bValidHistory || IsLumenFrontLayerHistoryValid(View);
				bRenderLumenFrontLayer = bRenderLumenFrontLayer || IsLumenFrontLayerTranslucencyEnabled(View);
			}

			if (!bValidHistory && !bRenderLumenFrontLayer)
			{
				bNeedFrontLayerData = 2;
			}
		}
	}
	else
	{
		// Check if any view require translucent front layer data
		for (const FViewInfo& View : InViews)
		{
			if (IsLumenFrontLayerTranslucencyEnabled(View))
			{
				bNeedFrontLayerData = 1;
				break;
			}	
		}
	}

	FFrontLayerTranslucencyData Out;
	if (bNeedFrontLayerData > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "LumenFrontLayerTranslucencyReflections");

		// Allocate resources for all views
		{
			EPixelFormat NormalFormat = PF_FloatRGBA; // Need more precision than PF_A2B10G10R10 for mirror reflections
			FRDGTextureDesc NormalDesc(FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, NormalFormat, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_RenderTargetable));
			Out.Normal = GraphBuilder.CreateTexture(NormalDesc, TEXT("Lumen.TranslucencyReflections.Normal"));
			Out.SceneDepth = GraphBuilder.CreateTexture(SceneTextures.Depth.Target->Desc, TEXT("Lumen.TranslucencyReflections.SceneDepth"));

			for (const FViewInfo& View : InViews)
			{
				if (bNeedFrontLayerData > 1 || IsLumenFrontLayerTranslucencyEnabled(View))
				{
					FLumenFrontLayerTranslucencyClearGBufferPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenFrontLayerTranslucencyClearGBufferPS::FParameters>();
					PassParameters->RenderTargets[0] = FRenderTargetBinding(Out.Normal, ERenderTargetLoadAction::ENoAction, 0);
					PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(Out.SceneDepth, ERenderTargetLoadAction::ENoAction, FExclusiveDepthStencil::DepthWrite_StencilNop);
					PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
	
					TShaderMapRef<FLumenFrontLayerTranslucencyClearGBufferPS> PixelShader(View.ShaderMap);
					FPixelShaderUtils::AddFullscreenPass<FLumenFrontLayerTranslucencyClearGBufferPS>(
						GraphBuilder,
						View.ShaderMap,
						RDG_EVENT_NAME("ClearTranslucencyGBuffer"),
						PixelShader,
						PassParameters,
						View.ViewRect,
						TStaticBlendState<>::GetRHI(),
						TStaticRasterizerState<FM_Solid, CM_None>::GetRHI(),
						TStaticDepthStencilState<true, CF_Always>::GetRHI());
				}
			}
		}

		// Render front layer data for each view
		for (FViewInfo& View : InViews)
		{
			if (bNeedFrontLayerData > 1 || IsLumenFrontLayerTranslucencyEnabled(View))
			{
				RenderFrontLayerTranslucencyGBuffer(GraphBuilder, *this, View, SceneTextures, Out);
			}
		}
	}
	return Out;
}

void FDeferredShadingSceneRenderer::RenderLumenFrontLayerTranslucencyReflections(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View,
	const FSceneTextures& SceneTextures,
	const FLumenSceneFrameTemporaries& LumenFrameTemporaries, 
	const FFrontLayerTranslucencyData& FrontLayerTranslucencyData)
{
	if (Lumen::UseLumenFrontLayerTranslucencyReflections(View) && View.bTranslucentSurfaceLighting)
	{
		check(FrontLayerTranslucencyData.IsValid());

		RDG_EVENT_SCOPE(GraphBuilder, "LumenFrontLayerTranslucencyReflections");

		FLumenMeshSDFGridParameters MeshSDFGridParameters;
		LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters;

		FLumenFrontLayerTranslucencyGBufferParameters ReflectionGBuffer;
		ReflectionGBuffer.FrontLayerTranslucencyNormal = FrontLayerTranslucencyData.Normal;
		ReflectionGBuffer.FrontLayerTranslucencySceneDepth = FrontLayerTranslucencyData.SceneDepth;

		FRDGTextureRef ReflectionTexture = RenderLumenReflections(
			GraphBuilder,
			View,
			SceneTextures,
			LumenFrameTemporaries,
			MeshSDFGridParameters,
			RadianceCacheParameters,
			ELumenReflectionPass::FrontLayerTranslucency,
			nullptr,
			&ReflectionGBuffer,
			ERDGPassFlags::Compute);

		View.LumenFrontLayerTranslucency.bEnabled = true;
		View.LumenFrontLayerTranslucency.RelativeDepthThreshold = CVarLumenFrontLayerDepthThreshold.GetValueOnRenderThread();
		View.LumenFrontLayerTranslucency.Radiance = ReflectionTexture;
		View.LumenFrontLayerTranslucency.Normal = ReflectionGBuffer.FrontLayerTranslucencyNormal;
		View.LumenFrontLayerTranslucency.SceneDepth = ReflectionGBuffer.FrontLayerTranslucencySceneDepth;
	}
}
