// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenTranslucencyRadianceCache.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "MeshPassProcessor.inl"
#include "LumenTranslucencyVolumeLighting.h"
#include "LumenRadianceCache.h"

int32 GLumenTranslucencyRadianceCacheReflections = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyRadianceCache(
	TEXT("r.Lumen.TranslucencyReflections.RadianceCache"),
	GLumenTranslucencyRadianceCacheReflections,
	TEXT("Whether to use the Radiance Cache to provide Lumen Reflections on Translucent Surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenTranslucencyReflectionsMarkDownsampleFactor = 4;
FAutoConsoleVariableRef CVarLumenTranslucencyRadianceCacheDownsampleFactor(
	TEXT("r.Lumen.TranslucencyReflections.MarkDownsampleFactor"),
	GLumenTranslucencyReflectionsMarkDownsampleFactor,
	TEXT("Downsample factor for marking translucent surfaces in the Lumen Radiance Cache.  Too low of factors will cause incorrect Radiance Cache coverage.  Should be a power of 2."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenTranslucencyReflectionsRadianceCacheReprojectionRadiusScale = 10;
FAutoConsoleVariableRef CVarLumenTranslucencyRadianceCacheReprojectionRadiusScale(
	TEXT("r.Lumen.TranslucencyReflections.ReprojectionRadiusScale"),
	GLumenTranslucencyReflectionsRadianceCacheReprojectionRadiusScale,
	TEXT("Larger values treat the Radiance Cache lighting as more distant."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize = 4.0f;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeRadianceCacheClipmapFadeSize(
	TEXT("r.Lumen.TranslucencyReflections.ClipmapFadeSize"),
	GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize,
	TEXT("Size in Radiance Cache probes of the dithered transition region between clipmaps"),
	ECVF_RenderThreadSafe
);

namespace Lumen
{
	bool UseLumenTranslucencyRadianceCacheReflections(const FViewInfo& View)
	{
		return GLumenTranslucencyRadianceCacheReflections != 0 && View.Family->EngineShowFlags.LumenReflections;
	}

	bool ShouldRenderInTranslucencyRadianceCacheMarkPass(bool bShouldRenderInMainPass, const FMaterial& Material)
	{
		const bool bIsTranslucent = IsTranslucentBlendMode(Material);
		const ETranslucencyLightingMode TranslucencyLightingMode = Material.GetTranslucencyLightingMode();

		return bIsTranslucent
			&& (TranslucencyLightingMode == TLM_Surface || TranslucencyLightingMode == TLM_SurfacePerPixelLighting)
			&& bShouldRenderInMainPass
			&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain());
	}
}

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLumenTranslucencyRadianceCacheMarkPassUniformParameters, )
	SHADER_PARAMETER_STRUCT(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FurthestHZBTexture)
	SHADER_PARAMETER(FVector2f, ViewportUVToHZBBufferUV)
	SHADER_PARAMETER(float, HZBMipLevel)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FLumenTranslucencyRadianceCacheMarkPassUniformParameters, "LumenTranslucencyRadianceCacheMarkPass", SceneTextures);

class FLumenTranslucencyRadianceCacheMarkVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenTranslucencyRadianceCacheMarkVS, MeshMaterial);

protected:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform) && IsTranslucentBlendMode(Parameters.MaterialParameters) && Parameters.MaterialParameters.bIsTranslucencySurface;
	}

	FLumenTranslucencyRadianceCacheMarkVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FLumenTranslucencyRadianceCacheMarkVS() = default;
};


IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenTranslucencyRadianceCacheMarkVS, TEXT("/Engine/Private/Lumen/LumenTranslucencyRadianceCacheMarkShaders.usf"), TEXT("MainVS"), SF_Vertex);

class FLumenTranslucencyRadianceCacheMarkPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenTranslucencyRadianceCacheMarkPS, MeshMaterial);

public:
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform) && IsTranslucentBlendMode(Parameters.MaterialParameters) && Parameters.MaterialParameters.bIsTranslucencySurface;
	}

	FLumenTranslucencyRadianceCacheMarkPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FLumenTranslucencyRadianceCacheMarkPS() = default;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenTranslucencyRadianceCacheMarkPS, TEXT("/Engine/Private/Lumen/LumenTranslucencyRadianceCacheMarkShaders.usf"), TEXT("MainPS"), SF_Pixel);

class FLumenTranslucencyRadianceCacheMarkMeshProcessor : public FSceneRenderingAllocatorObject<FLumenTranslucencyRadianceCacheMarkMeshProcessor>, public FMeshPassProcessor
{
public:

	FLumenTranslucencyRadianceCacheMarkMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override final;

	FMeshPassProcessorRenderState PassDrawRenderState;
};

bool GetLumenTranslucencyRadianceCacheMarkShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	TShaderRef<FLumenTranslucencyRadianceCacheMarkVS>& VertexShader,
	TShaderRef<FLumenTranslucencyRadianceCacheMarkPS>& PixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLumenTranslucencyRadianceCacheMarkVS>();
	ShaderTypes.AddShaderType<FLumenTranslucencyRadianceCacheMarkPS>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

bool CanMaterialRenderInLumenTranslucencyRadianceCacheMarkPass(
	const FScene& Scene,
	const FSceneViewFamily& ViewFamily,
	const FPrimitiveSceneProxy& PrimitiveSceneProxy,
	const FMaterial& Material)
{
	const FSceneView* View = ViewFamily.Views[0];
	check(View);

	return ShouldRenderLumenDiffuseGI(&Scene, *View) && Lumen::ShouldRenderInTranslucencyRadianceCacheMarkPass(PrimitiveSceneProxy.ShouldRenderInMainPass(), Material);
}

void FLumenTranslucencyRadianceCacheMarkMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (MeshBatch.bUseForMaterial 
		&& PrimitiveSceneProxy
		&& ViewIfDynamicMeshCommand
		//@todo - this filter should be done at a higher level
		&& ShouldRenderLumenDiffuseGI(Scene, *ViewIfDynamicMeshCommand))
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material)
			{
				auto TryAddMeshBatch = [this](const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& Material) -> bool
				{
					if (Lumen::ShouldRenderInTranslucencyRadianceCacheMarkPass(PrimitiveSceneProxy->ShouldRenderInMainPass(), Material))
					{
						const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
						FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

						TMeshProcessorShaders<
							FLumenTranslucencyRadianceCacheMarkVS,
							FLumenTranslucencyRadianceCacheMarkPS> PassShaders;

						if (!GetLumenTranslucencyRadianceCacheMarkShaders(
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

void FLumenTranslucencyRadianceCacheMarkMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	LLM_SCOPE_BYTAG(Lumen);
	
	if (PreCacheParams.bRenderInMainPass && !Lumen::ShouldRenderInTranslucencyRadianceCacheMarkPass(PreCacheParams.bRenderInMainPass, Material))
	{
		return;
	}

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

	TMeshProcessorShaders<
		FLumenTranslucencyRadianceCacheMarkVS,
		FLumenTranslucencyRadianceCacheMarkPS> PassShaders;

	if (!GetLumenTranslucencyRadianceCacheMarkShaders(
		Material,
		VertexFactoryData.VertexFactoryType,
		PassShaders.VertexShader,
		PassShaders.PixelShader))
	{
		return;
	}

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
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

FLumenTranslucencyRadianceCacheMarkMeshProcessor::FLumenTranslucencyRadianceCacheMarkMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::LumenTranslucencyRadianceCacheMark, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
{}

FMeshPassProcessor* CreateLumenTranslucencyRadianceCacheMarkPassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	LLM_SCOPE_BYTAG(Lumen);

	FMeshPassProcessorRenderState PassState;

	// We use HZB tests in the shader instead of hardware depth testing
	PassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	PassState.SetBlendState(TStaticBlendState<>::GetRHI());

	return new FLumenTranslucencyRadianceCacheMarkMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassState, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(LumenTranslucencyRadianceCacheMarkPass, CreateLumenTranslucencyRadianceCacheMarkPassProcessor, EShadingPath::Deferred, EMeshPass::LumenTranslucencyRadianceCacheMark, EMeshPassFlags::MainView);

BEGIN_SHADER_PARAMETER_STRUCT(FLumenTranslucencyRadianceCacheMarkParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenTranslucencyRadianceCacheMarkPassUniformParameters, MarkPass)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void LumenTranslucencyReflectionsMarkUsedProbes(
	FRDGBuilder& GraphBuilder,
	const FSceneRenderer& SceneRenderer,
	FViewInfo& View,
	const FSceneTextures& SceneTextures,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
{
	check(GLumenTranslucencyRadianceCacheReflections != 0);

	const EMeshPass::Type MeshPass = EMeshPass::LumenTranslucencyRadianceCacheMark;
	const float ViewportScale = 1.0f / GLumenTranslucencyReflectionsMarkDownsampleFactor;
	FIntRect DownsampledViewRect = GetScaledRect(View.ViewRect, ViewportScale);

	View.BeginRenderView();

	FLumenTranslucencyRadianceCacheMarkParameters* PassParameters = GraphBuilder.AllocParameters<FLumenTranslucencyRadianceCacheMarkParameters>();

	{
		FViewUniformShaderParameters DownsampledTranslucencyViewParameters = *View.CachedViewUniformShaderParameters;

		FViewMatrices ViewMatrices = View.ViewMatrices;
		FViewMatrices PrevViewMatrices = View.PrevViewInfo.ViewMatrices;

		// Update the parts of DownsampledTranslucencyParameters which are dependent on the buffer size and view rect
		View.SetupViewRectUniformBufferParameters(
			DownsampledTranslucencyViewParameters,
			SceneTextures.Config.Extent,
			DownsampledViewRect,
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
		FLumenTranslucencyRadianceCacheMarkPassUniformParameters& MarkPassParameters = *GraphBuilder.AllocParameters<FLumenTranslucencyRadianceCacheMarkPassUniformParameters>();
		SetupSceneTextureUniformParameters(GraphBuilder, &SceneTextures, View.FeatureLevel, ESceneTextureSetupMode::All, MarkPassParameters.SceneTextures);
		MarkPassParameters.RadianceCacheMarkParameters = RadianceCacheMarkParameters;
		MarkPassParameters.RadianceCacheMarkParameters.InvClipmapFadeSizeForMark = 1.0f / FMath::Clamp(GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize, .001f, 16.0f);

		MarkPassParameters.FurthestHZBTexture = View.HZB;
		MarkPassParameters.ViewportUVToHZBBufferUV = FVector2f(
				float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
				float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y));
		MarkPassParameters.HZBMipLevel = FMath::Max<float>((int32)FMath::FloorLog2((float)GLumenTranslucencyReflectionsMarkDownsampleFactor) - 1, 0.0f);

		PassParameters->MarkPass = GraphBuilder.CreateUniformBuffer(&MarkPassParameters);
	}

	View.ParallelMeshDrawCommandPasses[MeshPass].BuildRenderingCommands(GraphBuilder, SceneRenderer.Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("TranslucentSurfacesMarkPass"),
		PassParameters,
		ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
		[&View, &SceneRenderer, MeshPass, PassParameters, ViewportScale, DownsampledViewRect](FRHICommandList& RHICmdList)
	{
		FRHIRenderPassInfo RPInfo;
		RPInfo.ResolveRect = FResolveRect(DownsampledViewRect);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("LumenTranslucencyRadianceCacheMark"));

		FSceneRenderer::SetStereoViewport(RHICmdList, View, ViewportScale);
		View.ParallelMeshDrawCommandPasses[MeshPass].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);

		RHICmdList.EndRenderPass();
	});
}
