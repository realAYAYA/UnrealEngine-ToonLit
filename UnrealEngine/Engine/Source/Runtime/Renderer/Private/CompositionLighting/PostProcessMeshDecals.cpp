// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "HitProxies.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "DepthRendering.h"
#include "DecalRenderingCommon.h"
#include "DecalRenderingShared.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "UnrealEngine.h"
#include "DebugViewModeRendering.h"
#include "MeshPassProcessor.inl"
#include "SimpleMeshDrawCommandPass.h"

class FMeshDecalsVS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FMeshDecalsVS, MeshMaterial);

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return (Parameters.MaterialParameters.MaterialDomain == MD_DeferredDecal) &&
			DecalRendering::GetBaseRenderStage(DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters)) != EDecalRenderStage::None;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	}

	FMeshDecalsVS() = default;
	FMeshDecalsVS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
		: FMeshMaterialShader(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalsVS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainVS"),SF_Vertex); 

class FMeshDecalsPS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FMeshDecalsPS, MeshMaterial);

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return (Parameters.MaterialParameters.MaterialDomain == MD_DeferredDecal) &&
			DecalRendering::GetBaseRenderStage(DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters)) != EDecalRenderStage::None;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		DecalRendering::ModifyCompilationEnvironment(Parameters.Platform, DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::None, OutEnvironment);
	}

	FMeshDecalsPS() = default;
	FMeshDecalsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalsPS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainPS"),SF_Pixel);

class FMeshDecalsEmissivePS : public FMeshDecalsPS
{
public:
	DECLARE_SHADER_TYPE(FMeshDecalsEmissivePS, MeshMaterial);

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return (Parameters.MaterialParameters.MaterialDomain == MD_DeferredDecal) &&
			DecalRendering::IsCompatibleWithRenderStage(DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::Emissive);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		DecalRendering::ModifyCompilationEnvironment(Parameters.Platform, DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::Emissive, OutEnvironment);
	}

	FMeshDecalsEmissivePS() = default;
	FMeshDecalsEmissivePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshDecalsPS(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FMeshDecalsEmissivePS, TEXT("/Engine/Private/MeshDecals.usf"), TEXT("MainPS"), SF_Pixel);

class FMeshDecalsAmbientOcclusionPS : public FMeshDecalsPS
{
public:
	DECLARE_SHADER_TYPE(FMeshDecalsAmbientOcclusionPS, MeshMaterial);

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return (Parameters.MaterialParameters.MaterialDomain == MD_DeferredDecal) &&
			DecalRendering::IsCompatibleWithRenderStage(DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::AmbientOcclusion);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		DecalRendering::ModifyCompilationEnvironment(Parameters.Platform, DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::AmbientOcclusion, OutEnvironment);
	}

	FMeshDecalsAmbientOcclusionPS() = default;
	FMeshDecalsAmbientOcclusionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshDecalsPS(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FMeshDecalsAmbientOcclusionPS, TEXT("/Engine/Private/MeshDecals.usf"), TEXT("MainPS"), SF_Pixel);

class FMeshDecalMeshProcessor : public FMeshPassProcessor
{
public:
	FMeshDecalMeshProcessor(const FScene* Scene, 
		ERHIFeatureLevel::Type FeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand, 
		EDecalRenderStage InPassDecalStage, 
		EDecalRenderTargetMode InRenderTargetMode,
		FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

	virtual void CollectPSOInitializers(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FMaterial& Material,
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FPSOPrecacheParams& PreCacheParams, 
		TArray<FPSOPrecacheData>& PSOInitializers) override final;

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);

	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	void CollectDeferredDecalMeshPSOInitializers(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FPSOPrecacheParams& PreCacheParams,
		const FMaterial& Material,
		const FDecalBlendDesc DecalBlendDesc,
		EDecalRenderStage DecalRenderStage,
		TArray<FPSOPrecacheData>& PSOInitializers);

	FMeshPassProcessorRenderState PassDrawRenderState;
	const EDecalRenderStage PassDecalStage;
	const EDecalRenderTargetMode RenderTargetMode;
};

IMPLEMENT_STATIC_UNIFORM_BUFFER_SLOT(DeferredDecals);
IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FDeferredDecalUniformParameters, "DeferredDecal", DeferredDecals);

static const TCHAR* MeshDecalPassName = TEXT("MeshDecal");

FMeshDecalMeshProcessor::FMeshDecalMeshProcessor(const FScene* Scene, 
	ERHIFeatureLevel::Type InFeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand, 
	EDecalRenderStage InPassDecalStage, 
	EDecalRenderTargetMode InRenderTargetMode,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(MeshDecalPassName, Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDecalStage(InPassDecalStage)
	, RenderTargetMode(InRenderTargetMode)
{
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
}

void FMeshDecalMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.bUseForMaterial && MeshBatch.IsDecal(FeatureLevel))
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

bool FMeshDecalMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	if (Material.IsDeferredDecal())
	{
		// We have no special engine material for decals since we don't want to eat the compilation & memory cost, so just skip if it failed to compile
		if (Material.GetRenderingThreadShaderMap())
		{
			const EShaderPlatform ShaderPlatform = ViewIfDynamicMeshCommand->GetShaderPlatform();
			const FDecalBlendDesc DecalBlendDesc = DecalRendering::ComputeDecalBlendDesc(ShaderPlatform, Material);

			const bool bShouldRender =
				DecalRendering::IsCompatibleWithRenderStage(DecalBlendDesc, PassDecalStage) &&
				DecalRendering::GetRenderTargetMode(DecalBlendDesc, PassDecalStage) == RenderTargetMode;

			if (bShouldRender)
			{
				const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
				ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
				ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

				if (ViewIfDynamicMeshCommand->Family->UseDebugViewPS())
				{
					// Deferred decals can only use translucent blend mode
					if (ViewIfDynamicMeshCommand->Family->EngineShowFlags.ShaderComplexity)
					{
						// If we are in the translucent pass then override the blend mode, otherwise maintain additive blending.
						PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
					}
					else if (ViewIfDynamicMeshCommand->Family->GetDebugViewShaderMode() != DVSM_OutputMaterialTextureScales)
					{
						// Otherwise, force translucent blend mode (shaders will use an hardcoded alpha).
						PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
					}
				}
				else
				{
					PassDrawRenderState.SetBlendState(DecalRendering::GetDecalBlendState(DecalBlendDesc, PassDecalStage, RenderTargetMode));
				}

				return Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, MeshFillMode, MeshCullMode);
			}
		}
	}

	return true;
}

bool FMeshDecalMeshProcessor::Process(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FMeshDecalsVS>();

	if (PassDecalStage == EDecalRenderStage::Emissive)
	{
		ShaderTypes.AddShaderType<FMeshDecalsEmissivePS>();
	}
	else if (PassDecalStage == EDecalRenderStage::AmbientOcclusion)
	{
		ShaderTypes.AddShaderType<FMeshDecalsAmbientOcclusionPS>();
	}
	else
	{
		ShaderTypes.AddShaderType<FMeshDecalsPS>();
	}

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		// Skip rendering if any shaders missing
		return false;
	}

	TMeshProcessorShaders<
		FMeshDecalsVS,
		FMeshDecalsPS> MeshDecalPassShaders;
	Shaders.TryGetVertexShader(MeshDecalPassShaders.VertexShader);
	Shaders.TryGetPixelShader(MeshDecalPassShaders.PixelShader);

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	// Use BasePass sort key layout but replace the "Masked" highest priority bits with TranslucencySortPriority.
	FMeshDrawCommandSortKey SortKey;
	SortKey.BasePass.VertexShaderHash = (MeshDecalPassShaders.VertexShader.IsValid() ? MeshDecalPassShaders.VertexShader->GetSortKey() : 0) & 0xFFFF;
	SortKey.BasePass.PixelShaderHash = MeshDecalPassShaders.PixelShader.IsValid() ? MeshDecalPassShaders.PixelShader->GetSortKey() : 0;
	SortKey.BasePass.Masked = PrimitiveSceneProxy->GetTranslucencySortPriority();

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		MeshDecalPassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

void FMeshDecalMeshProcessor::CollectDeferredDecalMeshPSOInitializers(	
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FPSOPrecacheParams& PreCacheParams,
	const FMaterial& Material,
	const FDecalBlendDesc DecalBlendDesc,
	EDecalRenderStage DecalRenderStage,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	EDecalRenderTargetMode LocalRenderTargetMode = DecalRendering::GetRenderTargetMode(DecalBlendDesc, DecalRenderStage);
	PassDrawRenderState.SetBlendState(DecalRendering::GetDecalBlendState(DecalBlendDesc, DecalRenderStage, LocalRenderTargetMode));

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FMeshDecalsVS>();
	if (DecalRenderStage == EDecalRenderStage::Emissive)
	{
		ShaderTypes.AddShaderType<FMeshDecalsEmissivePS>();
	}
	else if (DecalRenderStage == EDecalRenderStage::AmbientOcclusion)
	{
		ShaderTypes.AddShaderType<FMeshDecalsAmbientOcclusionPS>();
	}
	else
	{
		ShaderTypes.AddShaderType<FMeshDecalsPS>();
	}

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryData.VertexFactoryType, Shaders))
	{
		return;
	}

	TMeshProcessorShaders<
		FMeshDecalsVS,
		FMeshDecalsPS> MeshDecalPassShaders;
	Shaders.TryGetVertexShader(MeshDecalPassShaders.VertexShader);
	Shaders.TryGetPixelShader(MeshDecalPassShaders.PixelShader);

	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	RenderTargetsInfo.NumSamples = 1;
	GetDeferredDecalRenderTargetsInfo(SceneTexturesConfig, ShaderPlatform, LocalRenderTargetMode, RenderTargetsInfo);

	AddGraphicsPipelineStateInitializer(
		VertexFactoryData,
		Material,
		PassDrawRenderState,
		RenderTargetsInfo,
		MeshDecalPassShaders,
		MeshFillMode,
		MeshCullMode,
		PT_TriangleList,
		EMeshPassFeatures::Default,
		true /*bRequired*/,
		PSOInitializers);
}

void FMeshDecalMeshProcessor::CollectPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& Material,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FPSOPrecacheParams& PreCacheParams, 
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	if (!Material.IsDeferredDecal())
	{
		return;
	}

	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
	const FDecalBlendDesc DecalBlendDesc = DecalRendering::ComputeDecalBlendDesc(ShaderPlatform, Material);

	for (uint32 DecalStageIter = 0; DecalStageIter < (uint32)EDecalRenderStage::Num; ++DecalStageIter)
	{
		EDecalRenderStage LocalDecalRenderStage = EDecalRenderStage(DecalStageIter);
				
		const bool bShouldRender = DecalRendering::IsCompatibleWithRenderStage(DecalBlendDesc, LocalDecalRenderStage);
		if (!bShouldRender)
		{
			continue;
		}
		
		// Collect decal pass PSOs
		CollectDeferredDecalPassPSOInitializers(PSOCollectorIndex, FeatureLevel, SceneTexturesConfig, Material, LocalDecalRenderStage, PSOInitializers);

		// Collect decal mesh PSOs
		CollectDeferredDecalMeshPSOInitializers(SceneTexturesConfig, VertexFactoryData, PreCacheParams, Material, DecalBlendDesc, LocalDecalRenderStage, PSOInitializers);
	}
}

IPSOCollector* CreateMeshDecalMeshProcessor(ERHIFeatureLevel::Type FeatureLevel)
{
	if (DoesPlatformSupportNanite(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		return new FMeshDecalMeshProcessor(nullptr, FeatureLevel, nullptr, EDecalRenderStage::None, EDecalRenderTargetMode::None, nullptr);
	}
	else
	{
		return nullptr;
	}
}

// Only register for PSO Collection
FRegisterPSOCollectorCreateFunction RegisterPSOCollectorMeshDecal(&CreateMeshDecalMeshProcessor, EShadingPath::Deferred, MeshDecalPassName);

void DrawDecalMeshCommands(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FDeferredDecalPassTextures& DecalPassTextures,
	FInstanceCullingManager& InstanceCullingManager,
	EDecalRenderStage DecalRenderStage,
	EDecalRenderTargetMode RenderTargetMode)
{
	auto* PassParameters = GraphBuilder.AllocParameters<FDeferredDecalPassParameters>();
	GetDeferredDecalPassParameters(GraphBuilder, View, DecalPassTextures, RenderTargetMode, *PassParameters);

	AddSimpleMeshPass(
		GraphBuilder, 
		PassParameters, 
		&Scene, 
		View, 
		&InstanceCullingManager, 
		RDG_EVENT_NAME("MeshDecals"), 
		View.ViewRect,
		[&View, DecalRenderStage, RenderTargetMode](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MeshDecalCommands);

		FMeshDecalMeshProcessor PassMeshProcessor(
			View.Family->Scene->GetRenderScene(),
			View.GetFeatureLevel(),
			&View,
			DecalRenderStage,
			RenderTargetMode,
			DynamicMeshPassContext);

		for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.MeshDecalBatches.Num(); ++MeshBatchIndex)
		{
			const FMeshBatch* Mesh = View.MeshDecalBatches[MeshBatchIndex].Mesh;
			const FPrimitiveSceneProxy* PrimitiveSceneProxy = View.MeshDecalBatches[MeshBatchIndex].Proxy;
			const uint64 DefaultBatchElementMask = ~0ull;

			PassMeshProcessor.AddMeshBatch(*Mesh, DefaultBatchElementMask, PrimitiveSceneProxy);
		}
	});
}

void RenderMeshDecals(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FDeferredDecalPassTextures& DecalPassTextures,
	FInstanceCullingManager& InstanceCullingManager,
	EDecalRenderStage DecalRenderStage)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderMeshDecals);

	switch (DecalRenderStage)
	{
	case EDecalRenderStage::BeforeBasePass:
		DrawDecalMeshCommands(GraphBuilder, Scene, View, DecalPassTextures, InstanceCullingManager, DecalRenderStage, EDecalRenderTargetMode::DBuffer);
		break;

	case EDecalRenderStage::BeforeLighting:
		DrawDecalMeshCommands(GraphBuilder, Scene, View, DecalPassTextures, InstanceCullingManager, DecalRenderStage, EDecalRenderTargetMode::SceneColorAndGBuffer);
		DrawDecalMeshCommands(GraphBuilder, Scene, View, DecalPassTextures, InstanceCullingManager, DecalRenderStage, EDecalRenderTargetMode::SceneColorAndGBufferNoNormal);
		break;

	case EDecalRenderStage::Mobile:
		DrawDecalMeshCommands(GraphBuilder, Scene, View, DecalPassTextures, InstanceCullingManager, DecalRenderStage, EDecalRenderTargetMode::SceneColor);
		break;

	case EDecalRenderStage::MobileBeforeLighting:
		DrawDecalMeshCommands(GraphBuilder, Scene, View, DecalPassTextures, InstanceCullingManager, DecalRenderStage, EDecalRenderTargetMode::SceneColorAndGBuffer);
		break;

	case EDecalRenderStage::Emissive:
		DrawDecalMeshCommands(GraphBuilder, Scene, View, DecalPassTextures, InstanceCullingManager, DecalRenderStage, EDecalRenderTargetMode::SceneColor);
		break;

	case EDecalRenderStage::AmbientOcclusion:
		DrawDecalMeshCommands(GraphBuilder, Scene, View, DecalPassTextures, InstanceCullingManager, DecalRenderStage, EDecalRenderTargetMode::AmbientOcclusion);
		break;
	}
}

void RenderMeshDecalsMobile(FRHICommandList& RHICmdList, const FViewInfo& View, EDecalRenderStage DecalRenderStage, EDecalRenderTargetMode RenderTargetMode)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	DrawDynamicMeshPass(View, RHICmdList, [&View, DecalRenderStage, RenderTargetMode](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
	{
		FMeshDecalMeshProcessor PassMeshProcessor(
			View.Family->Scene->GetRenderScene(),
			View.GetFeatureLevel(),
			&View,
			DecalRenderStage,
			RenderTargetMode,
			DynamicMeshPassContext);

		for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.MeshDecalBatches.Num(); ++MeshBatchIndex)
		{
			const FMeshBatch* Mesh = View.MeshDecalBatches[MeshBatchIndex].Mesh;
			const FPrimitiveSceneProxy* PrimitiveSceneProxy = View.MeshDecalBatches[MeshBatchIndex].Proxy;
			const uint64 DefaultBatchElementMask = ~0ull;

			PassMeshProcessor.AddMeshBatch(*Mesh, DefaultBatchElementMask, PrimitiveSceneProxy);
		}
	}, true);
}