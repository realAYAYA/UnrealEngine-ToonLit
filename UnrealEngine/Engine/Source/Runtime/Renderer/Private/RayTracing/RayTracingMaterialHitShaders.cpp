// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracing/RayTracingMaterialHitShaders.h"

#if RHI_RAYTRACING

#include "BasePassRendering.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"
#include "RenderCore.h"
#include "ScenePrivate.h"

#include "Nanite/NaniteRayTracing.h"

#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#include "BuiltInRayTracingShaders.h"
#include "RaytracingOptions.h"
#include "RayTracingLighting.h"
#include "RayTracingDecals.h"
#include "PathTracing.h"

int32 GEnableRayTracingMaterials = 1;
static FAutoConsoleVariableRef CVarEnableRayTracingMaterials(
	TEXT("r.RayTracing.EnableMaterials"),
	GEnableRayTracingMaterials,
	TEXT(" 0: bind default material shader that outputs placeholder data\n")
	TEXT(" 1: bind real material shaders (default)\n"),
	ECVF_RenderThreadSafe
);

int32 GCompileRayTracingMaterialCHS = 1;
static FAutoConsoleVariableRef CVarCompileRayTracingMaterialCHS(
	TEXT("r.RayTracing.CompileMaterialCHS"),
	GCompileRayTracingMaterialCHS,
	TEXT(" 0: skip compilation of closest-hit shaders for materials (useful if only shadows or ambient occlusion effects are needed)\n")
	TEXT(" 1: compile closest hit shaders for all ray tracing materials (default)\n"),
	ECVF_ReadOnly
);

int32 GCompileRayTracingMaterialAHS = 1;
static FAutoConsoleVariableRef CVarCompileRayTracingMaterialAHS(
	TEXT("r.RayTracing.CompileMaterialAHS"),
	GCompileRayTracingMaterialAHS,
	TEXT(" 0: skip compilation of any-hit shaders for materials (useful if alpha masked or translucent materials are not needed)\n")
	TEXT(" 1: compile any hit shaders for all ray tracing materials (default)\n"),
	ECVF_ReadOnly
);

static int32 GRayTracingNonBlockingPipelineCreation = 1;
static FAutoConsoleVariableRef CVarRayTracingNonBlockingPipelineCreation(
	TEXT("r.RayTracing.NonBlockingPipelineCreation"),
	GRayTracingNonBlockingPipelineCreation,
	TEXT("Enable background ray tracing pipeline creation, without blocking RHI or Render thread.\n")
	TEXT("Fallback opaque black material will be used for missing shaders meanwhile.\n")
	TEXT(" 0: off (rendering will always use correct requested material)\n")
	TEXT(" 1: on (default, non-blocking mode may sometimes use the fallback opaque black material outside of offline rendering scenarios)\n"),
	ECVF_RenderThreadSafe);

// CVar defined in DeferredShadingRenderer.cpp
extern int32 GRayTracingUseTextureLod;

static bool IsSupportedVertexFactoryType(const FVertexFactoryType* VertexFactoryType)
{
	return VertexFactoryType->SupportsRayTracing();
}

class FMaterialCHS : public FMeshMaterialShader, public FUniformLightMapPolicyShaderParametersType
{
	DECLARE_INLINE_TYPE_LAYOUT_EXPLICIT_BASES(FMaterialCHS, NonVirtual, FMeshMaterialShader, FUniformLightMapPolicyShaderParametersType);
public:
	FMaterialCHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
		FUniformLightMapPolicyShaderParametersType::Bind(Initializer.ParameterMap);
	}

	FMaterialCHS() {}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const TBasePassShaderElementData<FUniformLightMapPolicy>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);
		
		FUniformLightMapPolicy::GetPixelShaderBindings(
			PrimitiveSceneProxy,
			ShaderElementData.LightMapPolicyElementData,
			this,
			ShaderBindings);
	}

	void GetElementShaderBindings(
		const FShaderMapPointerTable& PointerTable,
		const FScene* Scene,
		const FSceneView* ViewIfDynamicMeshCommand,
		const FVertexFactory* VertexFactory,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch, 
		const FMeshBatchElement& BatchElement,
		const TBasePassShaderElementData<FUniformLightMapPolicy>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		FMeshMaterialShader::GetElementShaderBindings(PointerTable, Scene, ViewIfDynamicMeshCommand, VertexFactory, InputStreamType, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, ShaderBindings, VertexStreams);
	}
};

template<typename LightMapPolicyType, bool UseAnyHitShader, bool UseIntersectionShader, bool UseRayConeTextureLod>
class TMaterialCHS : public FMaterialCHS
{
	DECLARE_SHADER_TYPE(TMaterialCHS, MeshMaterial);
public:

	TMaterialCHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMaterialCHS(Initializer)
	{}

	TMaterialCHS() {}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		if (!GCompileRayTracingMaterialAHS && !GCompileRayTracingMaterialCHS)
		{
			return false;
		}

		const bool bWantAnyHitShader = (GCompileRayTracingMaterialAHS && (Parameters.MaterialParameters.bIsMasked || IsTranslucentOnlyBlendMode(Parameters.MaterialParameters)));
		const bool bSupportProceduralPrimitive = Parameters.VertexFactoryType->SupportsRayTracingProceduralPrimitive() && FDataDrivenShaderPlatformInfo::GetSupportsRayTracingProceduralPrimitive(Parameters.Platform);

		return IsSupportedVertexFactoryType(Parameters.VertexFactoryType)
			&& (bWantAnyHitShader == UseAnyHitShader)
			&& LightMapPolicyType::ShouldCompilePermutation(Parameters)
			&& ShouldCompileRayTracingShadersForProject(Parameters.Platform)
			&& (bool)GRayTracingUseTextureLod == UseRayConeTextureLod
			&& (UseIntersectionShader == bSupportProceduralPrimitive);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// NOTE: Any CVars that are used in this function must be handled in ShaderMapAppendKeyString() to ensure shaders are recompiled when necessary.

		OutEnvironment.SetDefine(TEXT("USE_MATERIAL_CLOSEST_HIT_SHADER"), GCompileRayTracingMaterialCHS ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("USE_MATERIAL_ANY_HIT_SHADER"), GCompileRayTracingMaterialAHS ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("USE_MATERIAL_INTERSECTION_SHADER"), UseIntersectionShader ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("USE_RAYTRACED_TEXTURE_RAYCONE_LOD"), UseRayConeTextureLod ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1);
		LightMapPolicyType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTexturedLightmaps"));
		const bool VirtualTextureLightmaps = (CVar->GetValueOnAnyThread() != 0) && UseVirtualTexturing(Parameters.Platform);
		OutEnvironment.SetDefine(TEXT("LIGHTMAP_VT_ENABLED"), VirtualTextureLightmaps);
	}

	static bool ValidateCompiledResult(EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutError)
	{
		if (ParameterMap.ContainsParameterAllocation(FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName()))
		{
			OutError.Add(TEXT("Ray tracing closest hit shaders cannot read from the SceneTexturesStruct."));
			return false;
		}

		for (const auto& It : ParameterMap.GetParameterMap())
		{
			const FParameterAllocation& ParamAllocation = It.Value;
			if (ParamAllocation.Type != EShaderParameterType::UniformBuffer
				&& ParamAllocation.Type != EShaderParameterType::LooseData)
			{
				OutError.Add(FString::Printf(TEXT("Invalid ray tracing shader parameter '%s'. Only uniform buffers and loose data parameters are supported."), *(It.Key)));
				return false;
			}
		}

		return true;
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::RayTracingMaterial;
	}
};

class FTrivialMaterialCHS : public FMaterialCHS
{
	DECLARE_SHADER_TYPE(FTrivialMaterialCHS, MeshMaterial);
public:

	FTrivialMaterialCHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMaterialCHS(Initializer)
	{}

	FTrivialMaterialCHS() {}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		if (GCompileRayTracingMaterialAHS || GCompileRayTracingMaterialCHS)
		{
			return false;
		}

		return IsSupportedVertexFactoryType(Parameters.VertexFactoryType)
			&& ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ValidateCompiledResult(EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutError)
	{
		return true;
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::RayTracingMaterial;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FTrivialMaterialCHS, TEXT("/Engine/Private/RayTracing/RayTracingMaterialDefaultHitShaders.usf"), TEXT("closesthit=OpaqueShadowCHS"), SF_RayHitGroup);

#define IMPLEMENT_MATERIALCHS_TYPE(LightMapPolicyType, LightMapPolicyName, AnyHitShaderName) \
	typedef TMaterialCHS<LightMapPolicyType, false, false, false> TMaterialCHS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHS##LightMapPolicyName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS"), SF_RayHitGroup); \
	typedef TMaterialCHS<LightMapPolicyType, true, false, false> TMaterialCHS##LightMapPolicyName##AnyHitShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHS##LightMapPolicyName##AnyHitShaderName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS anyhit=MaterialAHS"), SF_RayHitGroup) \
	typedef TMaterialCHS<LightMapPolicyType, false, false, true> TMaterialCHSLod##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHSLod##LightMapPolicyName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS"), SF_RayHitGroup); \
	typedef TMaterialCHS<LightMapPolicyType, true, false, true> TMaterialCHSLod##LightMapPolicyName##AnyHitShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHSLod##LightMapPolicyName##AnyHitShaderName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS anyhit=MaterialAHS"), SF_RayHitGroup); \
	typedef TMaterialCHS<LightMapPolicyType, false, true, false> TMaterialCHS_IS_##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHS_IS_##LightMapPolicyName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS intersection=MaterialIS"), SF_RayHitGroup); \
	typedef TMaterialCHS<LightMapPolicyType, true, true, false> TMaterialCHS_IS_##LightMapPolicyName##AnyHitShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHS_IS_##LightMapPolicyName##AnyHitShaderName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS anyhit=MaterialAHS intersection=MaterialIS"), SF_RayHitGroup) \
	typedef TMaterialCHS<LightMapPolicyType, false, true, true> TMaterialCHS_IS_Lod##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHS_IS_Lod##LightMapPolicyName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS intersection=MaterialIS"), SF_RayHitGroup); \
	typedef TMaterialCHS<LightMapPolicyType, true, true, true> TMaterialCHS_IS_Lod##LightMapPolicyName##AnyHitShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMaterialCHS_IS_Lod##LightMapPolicyName##AnyHitShaderName, TEXT("/Engine/Private/RayTracing/RayTracingMaterialHitShaders.usf"), TEXT("closesthit=MaterialCHS anyhit=MaterialAHS intersection=MaterialIS"), SF_RayHitGroup);

IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy, FAnyHitShader);
IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>, FPrecomputedVolumetricLightmapLightingPolicy, FAnyHitShader);
IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, TLightMapPolicyLQ, FAnyHitShader);
IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>, TLightMapPolicyHQ, FAnyHitShader);
IMPLEMENT_MATERIALCHS_TYPE(TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>, TDistanceFieldShadowsAndLightMapPolicyHQ, FAnyHitShader);

IMPLEMENT_GLOBAL_SHADER(FHiddenMaterialHitGroup, "/Engine/Private/RayTracing/RayTracingMaterialDefaultHitShaders.usf", "closesthit=HiddenMaterialCHS anyhit=HiddenMaterialAHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FOpaqueShadowHitGroup, "/Engine/Private/RayTracing/RayTracingMaterialDefaultHitShaders.usf", "closesthit=OpaqueShadowCHS", SF_RayHitGroup);
IMPLEMENT_GLOBAL_SHADER(FDefaultCallableShader, "/Engine/Private/RayTracing/RayTracingMaterialDefaultHitShaders.usf", "DefaultCallableShader", SF_RayCallable);

// Select TextureLOD
template<typename LightMapPolicyType, bool bUseAnyHitShader, bool bUseIntersectionShader>
inline void GetMaterialHitShader_TextureLOD(FMaterialShaderTypes& ShaderTypes, bool bUseTextureLod)
{
	if (bUseTextureLod)
	{
		ShaderTypes.AddShaderType<TMaterialCHS<LightMapPolicyType, bUseAnyHitShader, bUseIntersectionShader, true>>();
	}
	else
	{
		ShaderTypes.AddShaderType<TMaterialCHS<LightMapPolicyType, bUseAnyHitShader, bUseIntersectionShader, false>>();
	}
}

// Select Intersection shader
template<typename LightMapPolicyType, bool bUseAnyHitShader>
inline void GetMaterialHitShader_Intersection_TextureLOD(FMaterialShaderTypes& ShaderTypes, bool bUseIntersectionShader, bool bUseTextureLod)
{
	if (bUseIntersectionShader)
	{
		GetMaterialHitShader_TextureLOD<LightMapPolicyType, bUseAnyHitShader, true>(ShaderTypes, bUseTextureLod);
	}
	else
	{
		GetMaterialHitShader_TextureLOD<LightMapPolicyType, bUseAnyHitShader, false>(ShaderTypes, bUseTextureLod);
	}
}

// Select AnyHit shader
template<typename LightMapPolicyType>
inline void GetMaterialHitShader_AnyHit_Intersection_TextureLOD(FMaterialShaderTypes& ShaderTypes, bool bUseAnyHitShader, bool bUseIntersectionShader, bool bUseTextureLod)
{
	if (bUseAnyHitShader)
	{
		GetMaterialHitShader_Intersection_TextureLOD<LightMapPolicyType, true>(ShaderTypes, bUseIntersectionShader, bUseTextureLod);
	}
	else
	{
		GetMaterialHitShader_Intersection_TextureLOD<LightMapPolicyType, false>(ShaderTypes, bUseIntersectionShader, bUseTextureLod);
	}
}

template<typename LightMapPolicyType>
static bool GetMaterialHitShader(const FMaterial& RESTRICT MaterialResource, const FVertexFactory* VertexFactory, bool UseTextureLod, TShaderRef<FMaterialCHS>& OutShader)
{
	const bool bMaterialsCompiled = GCompileRayTracingMaterialAHS || GCompileRayTracingMaterialCHS;
	checkf(bMaterialsCompiled, TEXT(""));

	FMaterialShaderTypes ShaderTypes;
	const FVertexFactoryType* VFType = VertexFactory->GetType();
	const bool bUseIntersectionShader = VFType->HasFlags(EVertexFactoryFlags::SupportsRayTracingProceduralPrimitive) && FDataDrivenShaderPlatformInfo::GetSupportsRayTracingProceduralPrimitive(GMaxRHIShaderPlatform);
	const bool UseAnyHitShader = (MaterialResource.IsMasked() || IsTranslucentOnlyBlendMode(MaterialResource)) && GCompileRayTracingMaterialAHS;

	GetMaterialHitShader_AnyHit_Intersection_TextureLOD<LightMapPolicyType>(ShaderTypes, UseAnyHitShader, bUseIntersectionShader, UseTextureLod);

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
	{
		return false;
	}

	Shaders.TryGetShader(SF_RayHitGroup, OutShader);
	return true;
}

static bool GetRayTracingMeshProcessorShaders(
	const FUniformLightMapPolicy& RESTRICT LightMapPolicy,
	const FVertexFactory* VertexFactory,
	const FMaterial& RESTRICT MaterialResource,
	TShaderRef<FMaterialCHS>& OutRayHitGroupShader)
{
	check(GRHISupportsRayTracingShaders);

	const bool bMaterialsCompiled = GCompileRayTracingMaterialAHS || GCompileRayTracingMaterialCHS;

	if (bMaterialsCompiled)
	{
		const bool bUseTextureLOD = bool(GRayTracingUseTextureLod);

		switch (LightMapPolicy.GetIndirectPolicy())
		{
		case LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING:
			if (!GetMaterialHitShader<TUniformLightMapPolicy<LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING>>(MaterialResource, VertexFactory, bUseTextureLOD, OutRayHitGroupShader))
			{
				return false;
			}
			break;
		case LMP_LQ_LIGHTMAP:
			if (!GetMaterialHitShader<TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>>(MaterialResource, VertexFactory, bUseTextureLOD, OutRayHitGroupShader))
			{
				return false;
			}
			break;
		case LMP_HQ_LIGHTMAP:
			if (!GetMaterialHitShader<TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>>(MaterialResource, VertexFactory, bUseTextureLOD, OutRayHitGroupShader))
			{
				return false;
			}
			break;
		case LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP:
			if (!GetMaterialHitShader<TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP>>(MaterialResource, VertexFactory, bUseTextureLOD, OutRayHitGroupShader))
			{
				return false;
			}
			break;
		case LMP_NO_LIGHTMAP:
			if (!GetMaterialHitShader<TUniformLightMapPolicy<LMP_NO_LIGHTMAP>>(MaterialResource, VertexFactory, bUseTextureLOD, OutRayHitGroupShader))
			{
				return false;
			}
			break;
		default:
			check(false);
		}
	}
	else
	{
		FMaterialShaderTypes ShaderTypes;
		ShaderTypes.AddShaderType<FTrivialMaterialCHS>();

		FMaterialShaders Shaders;
		if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
		{
			return false;
		}

		Shaders.TryGetShader(SF_RayHitGroup, OutRayHitGroupShader);
	}

	return true;
}

FRayTracingMeshProcessor::FRayTracingMeshProcessor(FRayTracingMeshCommandContext* InCommandContext, const FScene* InScene, const FSceneView* InViewIfDynamicMeshCommand, ERayTracingMeshCommandsMode InRayTracingMeshCommandsMode)
	:
	CommandContext(InCommandContext),
	Scene(InScene),
	ViewIfDynamicMeshCommand(InViewIfDynamicMeshCommand),
	FeatureLevel(InScene ? InScene->GetFeatureLevel() : ERHIFeatureLevel::SM5),
	RayTracingMeshCommandsMode(InRayTracingMeshCommandsMode)
{
}

FRayTracingMeshProcessor::~FRayTracingMeshProcessor() = default;

bool FRayTracingMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	const FUniformLightMapPolicy& RESTRICT LightMapPolicy)
{
	TShaderRef<FMaterialCHS> RayTracingShader;
	if (GRHISupportsRayTracingShaders)
	{
		if (!GetRayTracingMeshProcessorShaders(LightMapPolicy, MeshBatch.VertexFactory, MaterialResource, RayTracingShader))
		{
			return false;
		}
	}

	TBasePassShaderElementData<FUniformLightMapPolicy> ShaderElementData(MeshBatch.LCI);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, -1, true);

	BuildRayTracingMeshCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		RayTracingShader,
		ShaderElementData,
		ERayTracingViewMaskMode::RayTracing);

	return true;
}

void FRayTracingMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy)
{
	if (!MeshBatch.bUseForMaterial || !IsSupportedVertexFactoryType(MeshBatch.VertexFactory->GetType()))
	{
		return;
	}

	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = MeshBatch.MaterialRenderProxy;
	while (FallbackMaterialRenderProxyPtr)
	{
		const FMaterial* Material = FallbackMaterialRenderProxyPtr->GetMaterialNoFallback(FeatureLevel);
		if (Material && Material->GetRenderingThreadShaderMap())
		{
			if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, -1, *FallbackMaterialRenderProxyPtr, *Material))
			{
				break;
			}
		}
		FallbackMaterialRenderProxyPtr = FallbackMaterialRenderProxyPtr->GetFallback(FeatureLevel);
	}
}

bool FRayTracingMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material
)
{
	// Only draw opaque materials.
	if ((!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInMainPass())
		&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()))
	{
		if (RayTracingMeshCommandsMode == ERayTracingMeshCommandsMode::PATH_TRACING ||
			RayTracingMeshCommandsMode == ERayTracingMeshCommandsMode::LIGHTMAP_TRACING)
		{
			// Path Tracer has its own process call so that it can attach its own material permutation
			return ProcessPathTracing(MeshBatch, BatchElementMask, PrimitiveSceneProxy, MaterialRenderProxy, Material);
		}

		// Check for a cached light-map.
		const bool bIsLitMaterial = Material.GetShadingModels().IsLit();
		const bool bAllowStaticLighting = IsStaticLightingAllowed();

		const FLightMapInteraction LightMapInteraction = (bAllowStaticLighting && MeshBatch.LCI && bIsLitMaterial)
			? MeshBatch.LCI->GetLightMapInteraction(FeatureLevel)
			: FLightMapInteraction();

		// force LQ lightmaps based on system settings
		const bool bPlatformAllowsHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel);
		const bool bAllowHighQualityLightMaps = bPlatformAllowsHighQualityLightMaps && LightMapInteraction.AllowsHighQualityLightmaps();

		const bool bAllowIndirectLightingCache = Scene && Scene->PrecomputedLightVolumes.Num() > 0;
		const bool bUseVolumetricLightmap = Scene && Scene->VolumetricLightmapSceneData.HasData();

		{
			static const auto CVarSupportLowQualityLightmap = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
			const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmap) || (CVarSupportLowQualityLightmap->GetValueOnAnyThread() != 0);

			switch (LightMapInteraction.GetType())
			{
			case LMIT_Texture:
				if (bAllowHighQualityLightMaps)
				{
					const FShadowMapInteraction ShadowMapInteraction = (bAllowStaticLighting && MeshBatch.LCI && bIsLitMaterial)
						? MeshBatch.LCI->GetShadowMapInteraction(FeatureLevel)
						: FShadowMapInteraction();

					if (ShadowMapInteraction.GetType() == SMIT_Texture)
					{
						return Process(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							FUniformLightMapPolicy(LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP));
					}
					else
					{
						return Process(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							FUniformLightMapPolicy(LMP_HQ_LIGHTMAP));
					}
				}
				else if (bAllowLowQualityLightMaps)
				{
					return Process(
						MeshBatch,
						BatchElementMask,
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						Material,
						FUniformLightMapPolicy(LMP_LQ_LIGHTMAP));
				}
				else
				{
					return Process(
						MeshBatch,
						BatchElementMask,
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						Material,
						FUniformLightMapPolicy(LMP_NO_LIGHTMAP));
				}
				break;
			default:
				if (bIsLitMaterial
					&& bAllowStaticLighting
					&& Scene
					&& Scene->VolumetricLightmapSceneData.HasData()
					&& PrimitiveSceneProxy
					&& (PrimitiveSceneProxy->IsMovable()
						|| PrimitiveSceneProxy->NeedsUnbuiltPreviewLighting()
						|| PrimitiveSceneProxy->GetLightmapType() == ELightmapType::ForceVolumetric))
				{
					return Process(
						MeshBatch,
						BatchElementMask,
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						Material,
						FUniformLightMapPolicy(LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING));
				}
				else
				{
					return Process(
						MeshBatch,
						BatchElementMask,
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						Material,
						FUniformLightMapPolicy(LMP_NO_LIGHTMAP));
				}
				break;
			};
		}
	}

	return true;
}

static bool IsCompatibleFallbackPipelineSignature(FRayTracingPipelineStateSignature& B, FRayTracingPipelineStateSignature& A)
{
	// Compare everything except hit group table
	return A.MaxPayloadSizeInBytes == B.MaxPayloadSizeInBytes
		&& A.bAllowHitGroupIndexing == B.bAllowHitGroupIndexing
		&& A.GetRayGenHash() == B.GetRayGenHash()
		&& A.GetRayMissHash() == B.GetRayMissHash()
		&& A.GetCallableHash() == B.GetCallableHash();
}

static bool PipelineContainsHitShaders(FRayTracingPipelineState* Pipeline, const TArrayView<FRHIRayTracingShader*>& Shaders)
{
	for (FRHIRayTracingShader* Shader : Shaders)
	{
		int32 Index = FindRayTracingHitGroupIndex(Pipeline, Shader, false);
		if (Index == INDEX_NONE)
		{
			return false;
		}
	}
	return true;
}

FRHIRayTracingShader* GetRayTracingDefaultMissShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FPackedMaterialClosestHitPayloadMS>().GetRayTracingShader();
}

FRHIRayTracingShader* GetRayTracingDefaultOpaqueShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FOpaqueShadowHitGroup>().GetRayTracingShader();
}

FRHIRayTracingShader* GetRayTracingDefaultHiddenShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FHiddenMaterialHitGroup>().GetRayTracingShader();
}



FRayTracingPipelineState* FDeferredShadingSceneRenderer::CreateRayTracingMaterialPipeline(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View,
	const TArrayView<FRHIRayTracingShader*>& RayGenShaderTable
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::BindRayTracingMaterialPipeline);
	SCOPE_CYCLE_COUNTER(STAT_BindRayTracingPipeline);

	FRHICommandList& RHICmdList = GraphBuilder.RHICmdList;

	const bool bIsPathTracing = ViewFamily.EngineShowFlags.PathTracing;
	const bool bSupportMeshDecals = bIsPathTracing;

	ERayTracingPayloadType PayloadType = bIsPathTracing
		? (ERayTracingPayloadType::PathTracingMaterial | ERayTracingPayloadType::Decals)
		: ERayTracingPayloadType::RayTracingMaterial;

	FRayTracingPipelineStateInitializer Initializer;

	Initializer.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(PayloadType);
	Initializer.bAllowHitGroupIndexing = true;

	FRHIRayTracingShader* DefaultMissShader = bIsPathTracing ? GetPathTracingDefaultMissShader(View.ShaderMap) : GetRayTracingDefaultMissShader(View.ShaderMap);

	TArray<FRHIRayTracingShader*> RayTracingMissShaderLibrary;
	FShaderMapResource::GetRayTracingMissShaderLibrary(RayTracingMissShaderLibrary, DefaultMissShader);

	// make sure we have at least one miss shader present
	check(RayTracingMissShaderLibrary.Num() > 0);

	Initializer.SetMissShaderTable(RayTracingMissShaderLibrary);

	Initializer.SetRayGenShaderTable(RayGenShaderTable);

	const bool bMaterialsCompiled = GCompileRayTracingMaterialAHS || GCompileRayTracingMaterialCHS;
	const bool bEnableMaterials = bMaterialsCompiled && GEnableRayTracingMaterials != 0;
	static auto CVarEnableShadowMaterials = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows.EnableMaterials"));
	const bool bEnableShadowMaterials = bMaterialsCompiled && (CVarEnableShadowMaterials ? CVarEnableShadowMaterials->GetInt() != 0 : true);

	FRHIRayTracingShader* OpaqueShadowShader   = bIsPathTracing ? GetPathTracingDefaultOpaqueHitShader(View.ShaderMap) : GetRayTracingDefaultOpaqueShader(View.ShaderMap);
	FRHIRayTracingShader* HiddenMaterialShader = bIsPathTracing ? GetPathTracingDefaultHiddenHitShader(View.ShaderMap) : GetRayTracingDefaultHiddenShader(View.ShaderMap);

	FRHIRayTracingShader* OpaqueMeshDecalHitShader = GetDefaultOpaqueMeshDecalHitShader(View.ShaderMap);
	FRHIRayTracingShader* HiddenMeshDecalHitShader = GetDefaultHiddenMeshDecalHitShader(View.ShaderMap);
	
	TArray<FRHIRayTracingShader*> RayTracingHitGroupLibrary;
	if (bEnableMaterials)
	{
		FShaderMapResource::GetRayTracingHitGroupLibrary(RayTracingHitGroupLibrary, OpaqueShadowShader);

		if (bSupportMeshDecals)
		{
			FShaderMapResource::GetRayTracingHitGroupLibrary(RayTracingHitGroupLibrary, OpaqueMeshDecalHitShader);
		}
	}

	FRHIRayTracingShader* RequiredHitShaders[] =
	{
		OpaqueShadowShader,
		HiddenMaterialShader
	};

	for (FRHIRayTracingShader* Shader : RequiredHitShaders)
	{
		RayTracingHitGroupLibrary.Add(Shader);
	}

	Initializer.SetHitGroupTable(RayTracingHitGroupLibrary);

	// For now, only path tracing uses callable shaders (for decals). This is only enabled if the current platform supports callable shaders.
	const bool bCallableShadersRequired = bIsPathTracing && RHISupportsRayTracingCallableShaders(View.Family->GetShaderPlatform());
	TArray<FRHIRayTracingShader*> RayTracingCallableShaderLibrary;
	FRHIRayTracingShader* DefaultCallableShader = nullptr;

	if (bCallableShadersRequired)
	{
		DefaultCallableShader = View.ShaderMap->GetShader<FDefaultCallableShader>().GetRayTracingShader();
		check(DefaultCallableShader != nullptr);

		if (bEnableMaterials)
		{
			FShaderMapResource::GetRayTracingCallableShaderLibrary(RayTracingCallableShaderLibrary, DefaultCallableShader);
		}
		else
		{
			RayTracingCallableShaderLibrary.Add(DefaultCallableShader);
		}

		Initializer.SetCallableTable(RayTracingCallableShaderLibrary);
	}

	const bool bAllowNonBlockingPipelineCreation = GRayTracingNonBlockingPipelineCreation && !View.bIsOfflineRender;
	FRayTracingPipelineState* FallbackPipelineState = bAllowNonBlockingPipelineCreation && View.ViewState
		? PipelineStateCache::GetRayTracingPipelineState(View.ViewState->LastRayTracingMaterialPipelineSignature)
		: nullptr;

	ERayTracingPipelineCacheFlags PipelineCacheFlags = ERayTracingPipelineCacheFlags::Default;
	if (FallbackPipelineState
		&& View.ViewState
		&& IsCompatibleFallbackPipelineSignature(View.ViewState->LastRayTracingMaterialPipelineSignature, Initializer)
		&& PipelineContainsHitShaders(FallbackPipelineState, RequiredHitShaders)
		&& FindRayTracingMissShaderIndex(FallbackPipelineState, DefaultMissShader, false) != INDEX_NONE
		&& (!bCallableShadersRequired || FindRayTracingCallableShaderIndex(FallbackPipelineState, DefaultCallableShader, false) != INDEX_NONE))
	{
		PipelineCacheFlags |= ERayTracingPipelineCacheFlags::NonBlocking;
	}

	FRayTracingPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer, PipelineCacheFlags);

	if (PipelineState)
	{
		if (View.ViewState)
		{
			// Save the current pipeline to be used as fallback in future frames
			View.ViewState->LastRayTracingMaterialPipelineSignature = static_cast<FRayTracingPipelineStateSignature&>(Initializer);
		}
	}
	else
	{
		// If pipeline was not found in cache, use the fallback from previous frame
		check(FallbackPipelineState);
		PipelineState = FallbackPipelineState;
	}

	check(PipelineState);

	const int32 OpaqueShadowMaterialIndex = FindRayTracingHitGroupIndex(PipelineState, OpaqueShadowShader, true);
	const int32 HiddenMaterialIndex = FindRayTracingHitGroupIndex(PipelineState, HiddenMaterialShader, true);

	const int32 OpaqueMeshDecalHitGroupIndex = bSupportMeshDecals ? FindRayTracingHitGroupIndex(PipelineState, OpaqueMeshDecalHitShader, true) : INDEX_NONE;
	const int32 HiddenMeshDecalHitGroupIndex = bSupportMeshDecals ? FindRayTracingHitGroupIndex(PipelineState, HiddenMeshDecalHitShader, true) : INDEX_NONE;

	FViewInfo& ReferenceView = Views[0];

	// material hit groups
	{
		const uint32 NumTotalMeshCommands = ReferenceView.VisibleRayTracingMeshCommands.Num();
		const uint32 TargetCommandsPerTask = 4096; // Granularity chosen based on profiling Infiltrator scene to balance wall time speedup and total CPU thread time.
		const uint32 NumTasks = FMath::Max(1u, FMath::DivideAndRoundUp(NumTotalMeshCommands, TargetCommandsPerTask));
		const uint32 CommandsPerTask = FMath::DivideAndRoundUp(NumTotalMeshCommands, NumTasks); // Evenly divide commands between tasks (avoiding potential short last task)

		FGraphEventArray TaskList;
		TaskList.Reserve(NumTasks);
		View.RayTracingMaterialBindings.SetNum(NumTasks);

		FRHIUniformBuffer* SceneUB = GetSceneUniforms().GetBufferRHI(GraphBuilder);
		for (uint32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
		{
			const uint32 FirstTaskCommandIndex = TaskIndex * CommandsPerTask;
			const FVisibleRayTracingMeshCommand* MeshCommands = ReferenceView.VisibleRayTracingMeshCommands.GetData() + FirstTaskCommandIndex;
			const uint32 NumCommands = FMath::Min(CommandsPerTask, NumTotalMeshCommands - FirstTaskCommandIndex);

			FRayTracingLocalShaderBindingWriter* BindingWriter = new FRayTracingLocalShaderBindingWriter();
			View.RayTracingMaterialBindings[TaskIndex] = BindingWriter;

			TaskList.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(
				[&View, SceneUB, bIsPathTracing, PipelineState, BindingWriter, MeshCommands, NumCommands, bEnableMaterials, bEnableShadowMaterials, bSupportMeshDecals,
				OpaqueShadowMaterialIndex, HiddenMaterialIndex, OpaqueMeshDecalHitGroupIndex, HiddenMeshDecalHitGroupIndex, TaskIndex]()
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(BindRayTracingMaterialPipelineTask);

					for (uint32 CommandIndex = 0; CommandIndex < NumCommands; ++CommandIndex)
					{
						const FVisibleRayTracingMeshCommand VisibleMeshCommand = MeshCommands[CommandIndex];
						const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

						const bool bIsMeshDecalShader = MeshCommand.MaterialShader->RayTracingPayloadType == (uint32)ERayTracingPayloadType::Decals;

						// TODO: Following check is disabled since FRayTracingMeshProcessor non-path-tracing code paths still don't assign the appropriate shader to decal mesh commands.
						// We could also potentially use regular materials to approximate decals in ray tracing in some situations.
						// check(bIsMeshDecalShader == MeshCommand.bDecal);

						// Force the same shader to be used on all geometry unless materials are enabled
						int32 HitGroupIndex;

						if (bIsMeshDecalShader)
						{
							checkf(bSupportMeshDecals && MeshCommand.bDecal, TEXT("Unexpected ray tracing mesh command using Mesh Decal payload. Fix logic adding the command or update bSupportMeshDecals as appropriate."));
							HitGroupIndex = VisibleMeshCommand.bHidden ? HiddenMeshDecalHitGroupIndex : OpaqueMeshDecalHitGroupIndex;
						}
						else
						{
							checkf((!bIsPathTracing && MeshCommand.MaterialShader->RayTracingPayloadType == (uint32)ERayTracingPayloadType::RayTracingMaterial)
								|| (bIsPathTracing && MeshCommand.MaterialShader->RayTracingPayloadType == (uint32)ERayTracingPayloadType::PathTracingMaterial),
								TEXT("Incorrectly using RayTracingMaterial when path tracer is enabled or vice-versa."));
							HitGroupIndex = VisibleMeshCommand.bHidden ? HiddenMaterialIndex : OpaqueShadowMaterialIndex;
						}

						if (bEnableMaterials && !VisibleMeshCommand.bHidden)
						{
							const int32 FoundIndex = FindRayTracingHitGroupIndex(PipelineState, MeshCommand.MaterialShader, false);
							if (FoundIndex != INDEX_NONE)
							{
								HitGroupIndex = FoundIndex;
							}
						}

						// Bind primary material shader

						{
							MeshCommand.SetRayTracingShaderBindingsForHitGroup(BindingWriter,
								View.ViewUniformBuffer,
								SceneUB,
								Nanite::GRayTracingManager.GetUniformBuffer(),
								VisibleMeshCommand.InstanceIndex,
								MeshCommand.GeometrySegmentIndex,
								HitGroupIndex,
								RAY_TRACING_SHADER_SLOT_MATERIAL);
						}

						// Bind shadow shader
						if (bIsMeshDecalShader)
						{
							// mesh decals do not use the shadow slot, so do minimal work
							FRayTracingLocalShaderBindings& Binding = BindingWriter->AddWithExternalParameters();
							Binding.InstanceIndex = VisibleMeshCommand.InstanceIndex;
							Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;
							Binding.ShaderSlot = RAY_TRACING_SHADER_SLOT_SHADOW;
							Binding.ShaderIndexInPipeline = OpaqueMeshDecalHitGroupIndex;

						}
						else if (MeshCommand.bCastRayTracedShadows && !VisibleMeshCommand.bHidden)
						{
							if (MeshCommand.bOpaque || !bEnableShadowMaterials)
							{
								FRayTracingLocalShaderBindings& Binding = BindingWriter->AddWithExternalParameters();
								Binding.InstanceIndex = VisibleMeshCommand.InstanceIndex;
								Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;
								Binding.ShaderSlot = RAY_TRACING_SHADER_SLOT_SHADOW;
								Binding.ShaderIndexInPipeline = OpaqueShadowMaterialIndex;
							}
							else
							{
								// Masked materials require full material evaluation with any-hit shader.
								// Full CHS is bound, however material evaluation is skipped for shadow rays using a dynamic branch on a ray payload flag.
								MeshCommand.SetRayTracingShaderBindingsForHitGroup(BindingWriter,
									View.ViewUniformBuffer,
									SceneUB,
									Nanite::GRayTracingManager.GetUniformBuffer(),
									VisibleMeshCommand.InstanceIndex,
									MeshCommand.GeometrySegmentIndex,
									HitGroupIndex,
									RAY_TRACING_SHADER_SLOT_SHADOW);
							}
						}
						else
						{
							FRayTracingLocalShaderBindings& Binding = BindingWriter->AddWithExternalParameters();
							Binding.InstanceIndex = VisibleMeshCommand.InstanceIndex;
							Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;
							Binding.ShaderSlot = RAY_TRACING_SHADER_SLOT_SHADOW;
							Binding.ShaderIndexInPipeline = HiddenMaterialIndex;
						}
					}
				},
				TStatId(), nullptr, ENamedThreads::AnyThread));
		}

		View.RayTracingMaterialBindingsTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]() {}, TStatId(), &TaskList, ENamedThreads::AnyHiPriThreadHiPriTask);
	}

	if (bCallableShadersRequired)
	{
		const int32 DefaultCallableShaderIndex = FindRayTracingCallableShaderIndex(PipelineState, DefaultCallableShader, true);

		const uint32 TargetCommandsPerTask = 4096;

		const uint32 NumTotalCallableCommands = Scene->RayTracingScene.CallableCommands.Num();
		const uint32 NumTasks = FMath::Max(1u, FMath::DivideAndRoundUp(NumTotalCallableCommands, TargetCommandsPerTask));
		const uint32 CommandsPerTask = FMath::DivideAndRoundUp(NumTotalCallableCommands, NumTasks); // Evenly divide commands between tasks (avoiding potential short last task)

		FGraphEventArray TaskList;
		TaskList.Reserve(NumTasks);
		View.RayTracingCallableBindings.SetNum(NumTasks);
		FRHIUniformBuffer* SceneUB = GetSceneUniforms().GetBufferRHI(GraphBuilder);

		for (uint32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
		{
			const uint32 TaskBaseCommandIndex = TaskIndex * CommandsPerTask;
			const FRayTracingShaderCommand* TaskCallableCommands = Scene->RayTracingScene.CallableCommands.GetData() + TaskBaseCommandIndex;
			const uint32 NumCommands = FMath::Min(CommandsPerTask, NumTotalCallableCommands - TaskBaseCommandIndex);

			FRayTracingLocalShaderBindingWriter* BindingWriter = new FRayTracingLocalShaderBindingWriter();
			View.RayTracingCallableBindings[TaskIndex] = BindingWriter;

			TaskList.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(
				[&View, SceneUB, PipelineState, BindingWriter, TaskCallableCommands, NumCommands, bEnableMaterials, DefaultCallableShaderIndex, TaskIndex]()
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(BindRayTracingMaterialPipelineTask);

					for (uint32 CommandIndex = 0; CommandIndex < NumCommands; ++CommandIndex)
					{
						const FRayTracingShaderCommand& CallableCommand = TaskCallableCommands[CommandIndex];

						int32 CallableShaderIndex = DefaultCallableShaderIndex; // Force the same shader to be used on all geometry unless materials are enabled

						if (bEnableMaterials)
						{
							const int32 FoundIndex = FindRayTracingCallableShaderIndex(PipelineState, CallableCommand.Shader, false);
							if (FoundIndex != INDEX_NONE)
							{
								CallableShaderIndex = FoundIndex;
							}
						}

						CallableCommand.SetRayTracingShaderBindings(
							BindingWriter, 
							View.ViewUniformBuffer, SceneUB, Nanite::GRayTracingManager.GetUniformBuffer(),
							CallableShaderIndex, CallableCommand.SlotInScene);
					}
				},
				TStatId(), nullptr, ENamedThreads::AnyThread));
		}

		View.RayTracingCallableBindingsTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]() {}, TStatId(), &TaskList, ENamedThreads::AnyHiPriThreadHiPriTask);
	}

	return PipelineState;
}

void FDeferredShadingSceneRenderer::BindRayTracingMaterialPipeline(
	FRHICommandListImmediate& RHICmdList,
	FViewInfo& View,
	FRayTracingPipelineState* PipelineState
)
{
	// Gather bindings from all chunks and submit them all as a single batch to allow RHI to bind all shader parameters in parallel.

	auto MergeAndSetBindings =
		[
			&Allocator = Allocator,
			&RHICmdList,
			RayTracingScene = View.GetRayTracingSceneChecked(),
			Pipeline = PipelineState
		](TConstArrayView<FRayTracingLocalShaderBindingWriter*> Bindings, ERayTracingBindingType BindingType)
	{
		uint32 NumTotalBindings = 0;

		for (FRayTracingLocalShaderBindingWriter* BindingWriter : Bindings)
		{
			const FRayTracingLocalShaderBindingWriter::FChunk* Chunk = BindingWriter->GetFirstChunk();
			while (Chunk)
			{
				NumTotalBindings += Chunk->Num;
				Chunk = Chunk->Next;
			}
		}

		if (NumTotalBindings == 0)
		{
			return;
		}

		const uint32 MergedBindingsSize = sizeof(FRayTracingLocalShaderBindings) * NumTotalBindings;
		FRayTracingLocalShaderBindings* MergedBindings = (FRayTracingLocalShaderBindings*)(RHICmdList.Bypass()
			? Allocator.Malloc(MergedBindingsSize, alignof(FRayTracingLocalShaderBindings))
			: RHICmdList.Alloc(MergedBindingsSize, alignof(FRayTracingLocalShaderBindings)));

		uint32 MergedBindingIndex = 0;
		for (FRayTracingLocalShaderBindingWriter* BindingWriter : Bindings)
		{
			const FRayTracingLocalShaderBindingWriter::FChunk* Chunk = BindingWriter->GetFirstChunk();
			while (Chunk)
			{
				const uint32 Num = Chunk->Num;
				for (uint32_t i = 0; i < Num; ++i)
				{
					MergedBindings[MergedBindingIndex] = Chunk->Bindings[i];
					MergedBindingIndex++;
				}
				Chunk = Chunk->Next;
			}
		}

		const bool bCopyDataToInlineStorage = false; // Storage is already allocated from RHICmdList, no extra copy necessary
		RHICmdList.SetRayTracingBindings(
			RayTracingScene,
			Pipeline,
			NumTotalBindings, MergedBindings,
			BindingType,
			bCopyDataToInlineStorage);
	};

	FTaskGraphInterface::Get().WaitUntilTaskCompletes(View.RayTracingMaterialBindingsTask, ENamedThreads::GetRenderThread_Local()); // TODO: move this sync point to the end of RDG setup, before execution
	MergeAndSetBindings(View.RayTracingMaterialBindings, ERayTracingBindingType::HitGroup);

	FTaskGraphInterface::Get().WaitUntilTaskCompletes(View.RayTracingCallableBindingsTask, ENamedThreads::GetRenderThread_Local()); // TODO: move this sync point to the end of RDG setup, before execution
	MergeAndSetBindings(View.RayTracingCallableBindings, ERayTracingBindingType::CallableShader);

	// Move the ray tracing binding container ownership to the command list, so that memory will be
	// released on the RHI thread timeline, after the commands that reference it are processed.
	RHICmdList.EnqueueLambda([PtrsA = MoveTemp(View.RayTracingMaterialBindings), PtrsB = MoveTemp(View.RayTracingCallableBindings)](FRHICommandListImmediate&)
	{
		for (auto Ptr : PtrsA)
		{
			delete Ptr;
		}
		for (auto Ptr : PtrsB)
		{
			delete Ptr;
		}
	});
}

#endif // RHI_RAYTRACING
