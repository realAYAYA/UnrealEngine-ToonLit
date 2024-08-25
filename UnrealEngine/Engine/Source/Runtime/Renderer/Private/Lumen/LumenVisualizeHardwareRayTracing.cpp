// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenVisualize.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "SceneTextureParameters.h"
#include "IndirectLightRendering.h"
#include "LumenReflections.h"
#include "HairStrands/HairStrandsData.h"

// Actual screen-probe requirements..
#include "LumenRadianceCache.h"
#include "LumenScreenProbeGather.h"

#if RHI_RAYTRACING
#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"
#endif // RHI_RAYTRACING

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracing(
	TEXT("r.Lumen.Visualize.HardwareRayTracing"),
	1,
	TEXT("Enables visualization of hardware ray tracing (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracingDeferredMaterial(
	TEXT("r.Lumen.Visualize.HardwareRayTracing.DeferredMaterial"),
	1,
	TEXT("Enables deferred material pipeline (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracingDeferredMaterialTileSize(
	TEXT("r.Lumen.Visualize.HardwareRayTracing.DeferredMaterial.TileDimension"),
	64,
	TEXT("Determines the tile dimension for material sorting (Default = 64)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracingThreadCount(
	TEXT("r.Lumen.Visualize.HardwareRayTracing.ThreadCount"),
	64,
	TEXT("Determines the active group count when dispatching raygen shader (default = 64"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracingGroupCount(
	TEXT("r.Lumen.Visualize.HardwareRayTracing.GroupCount"),
	4096,
	TEXT("Determines the active group count when dispatching raygen shader (default = 4096"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracingRetraceHitLighting(
	TEXT("r.Lumen.Visualize.HardwareRayTracing.Retrace.HitLighting"),
	0,
	TEXT("Determines whether a second trace will be fired for hit-lighting for invalid surface-cache hits (default = 1"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracingRetraceFarField(
	TEXT("r.Lumen.Visualize.HardwareRayTracing.Retrace.FarField"),
	1,
	TEXT("Determines whether a second trace will be fired for far-field contribution (default = 1"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracingCompact(
	TEXT("r.Lumen.Visualize.HardwareRayTracing.Compact"),
	1,
	TEXT("Determines whether a second trace will be compacted before traversal (default = 1"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenVisualizeHardwareRayTracingBucketMaterials(
	TEXT("r.Lumen.Visualize.HardwareRayTracing.BucketMaterials"),
	1,
	TEXT("Determines whether a secondary traces will be bucketed for coherent material access (default = 1"),
	ECVF_RenderThreadSafe
);

namespace Lumen
{
	bool UseHardwareRayTracedVisualize(const FSceneViewFamily& ViewFamily)
	{
		bool bVisualize = false;
#if RHI_RAYTRACING
		bVisualize = IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& CVarLumenVisualizeHardwareRayTracing.GetValueOnRenderThread() != 0;
#endif
		return bVisualize;
	}

	bool ShouldVisualizeHardwareRayTracing(const FSceneViewFamily& ViewFamily)
	{
		return UseHardwareRayTracedVisualize(ViewFamily) && Lumen::ShouldVisualizeScene(ViewFamily.EngineShowFlags);
	}

#if RHI_RAYTRACING
	FHardwareRayTracingPermutationSettings GetVisualizeHardwareRayTracingPermutationSettings(const FViewInfo& View, bool bLumenGIEnabled)
	{
		FHardwareRayTracingPermutationSettings ModesAndPermutationSettings;
		ModesAndPermutationSettings.LightingMode = GetHardwareRayTracingLightingMode(View, bLumenGIEnabled);
		ModesAndPermutationSettings.bUseMinimalPayload = (ModesAndPermutationSettings.LightingMode == Lumen::EHardwareRayTracingLightingMode::LightingFromSurfaceCache);
		ModesAndPermutationSettings.bUseDeferredMaterial = (CVarLumenVisualizeHardwareRayTracingDeferredMaterial.GetValueOnRenderThread()) != 0 && !ModesAndPermutationSettings.bUseMinimalPayload;
		return ModesAndPermutationSettings;
	}
#endif
} // namespace Lumen

namespace LumenVisualize
{
	bool UseFarField(const FSceneViewFamily& ViewFamily)
	{
		return Lumen::UseFarField(ViewFamily) && CVarLumenVisualizeHardwareRayTracingRetraceFarField.GetValueOnRenderThread() != 0;
	}

	bool IsHitLightingForceEnabled(const FViewInfo& View, bool bLumenGIEnabled)
	{
		return Lumen::GetHardwareRayTracingLightingMode(View, bLumenGIEnabled) != Lumen::EHardwareRayTracingLightingMode::LightingFromSurfaceCache;
	}

	bool UseHitLighting(const FViewInfo& View, bool bLumenGIEnabled)
	{
		#if RHI_RAYTRACING
		if (LumenHardwareRayTracing::IsRayGenSupported())
		{
			return LumenVisualize::IsHitLightingForceEnabled(View, bLumenGIEnabled)
				|| CVarLumenVisualizeHardwareRayTracingRetraceHitLighting.GetValueOnRenderThread() != 0;
		}
		#endif

		return false;
	}
}

#if RHI_RAYTRACING

namespace LumenVisualize
{
	// Struct definitions much match those in LumenVisualizeHardwareRayTracing.usf
	struct FTileDataPacked
	{
		uint32 PackedData;
	};

	struct FRayDataPacked
	{
		uint32 PackedData;
	};

	struct FTraceDataPacked
	{
		uint32 PackedData[2];
	};

	enum class ECompactMode
	{
		// Permutations for compaction modes
		HitLightingRetrace,
		ForceHitLighting,
		MAX
	};

} // namespace LumenVisualize

class FLumenVisualizeCreateTilesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenVisualizeCreateTilesCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenVisualizeCreateTilesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenVisualize::FSceneParameters, VisualizeParameters)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FTileData>, RWTileDataPacked)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_VISUALIZE_MODE"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenVisualizeCreateTilesCS, "/Engine/Private/Lumen/LumenVisualizeHardwareRayTracing.usf", "FLumenVisualizeCreateTilesCS", SF_Compute);

const int32 VisualizeCreateRaysDispatchSizeX = 128;

class FLumenVisualizeCreateRaysCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenVisualizeCreateRaysCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenVisualizeCreateRaysCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenVisualize::FSceneParameters, VisualizeParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenVisualize::FTileDataPacked>, TileDataPacked)
		SHADER_PARAMETER(float, MaxTraceDistance)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWRayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenVisualize::FRayDataPacked>, RWRayDataPacked)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_VISUALIZE_MODE"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
		OutEnvironment.SetDefine(TEXT("DISPATCH_GROUP_SIZE_X"), VisualizeCreateRaysDispatchSizeX);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenVisualizeCreateRaysCS, "/Engine/Private/Lumen/LumenVisualizeHardwareRayTracing.usf", "FLumenVisualizeCreateRaysCS", SF_Compute);

class FLumenVisualizeCompactRaysIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenVisualizeCompactRaysIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenVisualizeCompactRaysIndirectArgsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactRaysIndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_VISUALIZE_MODE"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenVisualizeCompactRaysIndirectArgsCS, "/Engine/Private/Lumen/LumenVisualizeHardwareRayTracing.usf", "FLumenVisualizeCompactRaysIndirectArgsCS", SF_Compute);

class FLumenVisualizeCompactRaysCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenVisualizeCompactRaysCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenVisualizeCompactRaysCS, FGlobalShader);

	class FCompactModeDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_COMPACT_MODE", LumenVisualize::ECompactMode);
	using FPermutationDomain = TShaderPermutationDomain<FCompactModeDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenVisualize::FRayDataPacked>, RayDataPacked)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenVisualize::FTraceDataPacked>, TraceDataPacked)

		SHADER_PARAMETER(int, MaxRayAllocationCount)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactedRayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenVisualize::FRayDataPacked>, RWCompactedRayDataPacked)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenVisualize::FTraceDataPacked>, RWCompactedTraceDataPacked)

		// Indirect
		RDG_BUFFER_ACCESS(CompactRaysIndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_VISUALIZE_MODE"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenVisualizeCompactRaysCS, "/Engine/Private/Lumen/LumenVisualizeHardwareRayTracing.usf", "FLumenVisualizeCompactRaysCS", SF_Compute);

class FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWBucketRaysByMaterialIdIndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_VISUALIZE_MODE"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 16; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS, "/Engine/Private/Lumen/LumenVisualizeHardwareRayTracing.usf", "FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS", SF_Compute);


class FLumenVisualizeBucketRaysByMaterialIdCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenVisualizeBucketRaysByMaterialIdCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenVisualizeBucketRaysByMaterialIdCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenVisualize::FRayDataPacked>, RayDataPacked)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenVisualize::FTraceDataPacked>, TraceDataPacked)

		SHADER_PARAMETER(int, MaxRayAllocationCount)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenVisualize::FRayDataPacked>, RWRayDataPacked)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenVisualize::FTraceDataPacked>, RWTraceDataPacked)

		// Indirect args
		RDG_BUFFER_ACCESS(BucketRaysByMaterialIdIndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_VISUALIZE_MODE"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 16; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenVisualizeBucketRaysByMaterialIdCS, "/Engine/Private/Lumen/LumenVisualizeHardwareRayTracing.usf", "FLumenVisualizeBucketRaysByMaterialIdCS", SF_Compute);

class FLumenVisualizeHardwareRayTracing : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenVisualizeHardwareRayTracing, Lumen::ERayTracingShaderDispatchSize::DispatchSize2D)

	class FHitLightingDim : SHADER_PERMUTATION_BOOL("DIM_HIT_LIGHTING");
	class FHairStrandsOcclusionDim : SHADER_PERMUTATION_BOOL("DIM_HAIRSTRANDS_VOXEL");
	class FFarFieldDim : SHADER_PERMUTATION_BOOL("ENABLE_FAR_FIELD_TRACING");
	class FRecursiveRefractionTraces : SHADER_PERMUTATION_BOOL("RECURSIVE_REFRACTION_TRACES");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FHitLightingDim, FHairStrandsOcclusionDim, FFarFieldDim, FRecursiveRefractionTraces>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenVisualize::FSceneParameters, VisualizeParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenVisualize::FRayDataPacked>, RayDataPacked)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenVisualize::FTraceDataPacked>, TraceDataPacked)

		SHADER_PARAMETER(int, ThreadCount)
		SHADER_PARAMETER(int, GroupCount)
		SHADER_PARAMETER(int, LightingMode)
		SHADER_PARAMETER(uint32, UseReflectionCaptures)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(int, MaxRayAllocationCount)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, FarFieldMaxTraceDistance)
		SHADER_PARAMETER(FVector3f, FarFieldReferencePos)
		SHADER_PARAMETER(float, NearFieldMaxTraceDistanceDitherScale)
		SHADER_PARAMETER(float, NearFieldSceneRadius)
		SHADER_PARAMETER(uint32, ApplySkylightStage)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, HairStrandsVoxel)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWRadiance)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenVisualize::FTraceDataPacked>, RWTraceDataPacked)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::HighResPages, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_VISUALIZE_MODE"), 1);
		OutEnvironment.SetDefine(TEXT("RECURSIVE_REFLECTION_TRACES"), 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		if (!FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FHitLightingDim>() && ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline)
		{
			return false;
		}
		return true;
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		FPermutationDomain PermutationVector(PermutationId);
		if (PermutationVector.Get<FHitLightingDim>())
		{
			return ERayTracingPayloadType::RayTracingMaterial;
		}
		else
		{
			return ERayTracingPayloadType::LumenMinimal;
		}
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenVisualizeHardwareRayTracing)

IMPLEMENT_GLOBAL_SHADER(FLumenVisualizeHardwareRayTracingRGS, "/Engine/Private/Lumen/LumenVisualizeHardwareRayTracing.usf", "LumenVisualizeHardwareRayTracingRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FLumenVisualizeHardwareRayTracingCS, "/Engine/Private/Lumen/LumenVisualizeHardwareRayTracing.usf", "LumenVisualizeHardwareRayTracingCS", SF_Compute);

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingVisualize(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	const bool bLumenGIEnabled = GetViewPipelineState(View).DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen;

	if (Lumen::ShouldVisualizeHardwareRayTracing(*View.Family)
		&& (LumenVisualize::UseHitLighting(View, bLumenGIEnabled) || LumenVisualize::UseFarField(*View.Family) || LumenReflections::UseHitLighting(View, bLumenGIEnabled) || LumenReflections::UseFarField(*View.Family)))
	{
		for (int32 HairOcclusion = 0; HairOcclusion < 2; HairOcclusion++)
		{
			for (int32 RayTracingTranslucent = 0; RayTracingTranslucent < 2; RayTracingTranslucent++)
			{
				FLumenVisualizeHardwareRayTracingRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FHitLightingDim>(true);
				PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FHairStrandsOcclusionDim>(HairOcclusion == 0);
				PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FFarFieldDim>(false);
				PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FRecursiveRefractionTraces>(RayTracingTranslucent > 0);
				TShaderRef<FLumenVisualizeHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenVisualizeHardwareRayTracingRGS>(PermutationVector);
				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}
		}
	}
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingVisualizeLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	// Fixed-function lighting version
	const bool bLumenGIEnabled = GetViewPipelineState(View).DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen;
	Lumen::FHardwareRayTracingPermutationSettings PermutationSettings = Lumen::GetVisualizeHardwareRayTracingPermutationSettings(View, bLumenGIEnabled);

	if (Lumen::ShouldVisualizeHardwareRayTracing(*View.Family))
	{
		for (int32 FarField = 0; FarField < 2; FarField++)
		{
			for (int32 HairOcclusion = 0; HairOcclusion < 2; HairOcclusion++)
			{
				for (int32 RayTracingTranslucent = 0; RayTracingTranslucent < 2; RayTracingTranslucent++)
				{
					FLumenVisualizeHardwareRayTracingRGS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FHitLightingDim>(false);
					PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FHairStrandsOcclusionDim>(HairOcclusion == 0);
					PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FFarFieldDim>(FarField == 0);
					PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FRecursiveRefractionTraces>(RayTracingTranslucent > 0);
					TShaderRef<FLumenVisualizeHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenVisualizeHardwareRayTracingRGS>(PermutationVector);
					OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
				}
			}
		}
	}
}

#endif // RHI_RAYTRACING

extern int32 GLumenReflectionHairStrands_VoxelTrace;

void LumenVisualize::VisualizeHardwareRayTracing(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FLumenCardTracingParameters& TracingParameters,
	FLumenIndirectTracingParameters& IndirectTracingParameters,
	LumenVisualize::FSceneParameters& VisualizeParameters,
	FRDGTextureRef SceneColor,
	bool bVisualizeModeWithHitLighting,
	bool bLumenGIEnabled)
#if RHI_RAYTRACING
{
	bool bTraceFarField = LumenVisualize::UseFarField(*View.Family);
	bool bRetraceForHitLighting = LumenVisualize::UseHitLighting(View, bLumenGIEnabled) && bVisualizeModeWithHitLighting;
	bool bForceHitLighting = LumenVisualize::IsHitLightingForceEnabled(View, bLumenGIEnabled);
	bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);
	const bool bNeedTraceHairVoxel = HairStrands::HasViewHairStrandsVoxelData(View) && GLumenReflectionHairStrands_VoxelTrace > 0;
	const bool bTraceTranslucent = LumenReflections::UseTranslucentRayTracing(View);

	// Reflection scene view uses reflection setup
	if (VisualizeParameters.VisualizeMode == VISUALIZE_MODE_REFLECTION_VIEW)
	{
		bTraceFarField = LumenReflections::UseFarField(*View.Family);
		bRetraceForHitLighting = LumenReflections::UseHitLighting(View, bLumenGIEnabled);
		bForceHitLighting = LumenReflections::IsHitLightingForceEnabled(View, bLumenGIEnabled);
	}

	// Disable modes that use hit-lighting when it's not available
	if (!LumenHardwareRayTracing::IsRayGenSupported())
	{
		bRetraceForHitLighting = false;
		bForceHitLighting = false;
	}	

	// Cache near-field and far-field trace distances
	const float FarFieldMaxTraceDistance = Lumen::GetFarFieldMaxTraceDistance();
	const float MaxTraceDistance = IndirectTracingParameters.MaxTraceDistance;

	// Generate tiles
	FRDGBufferRef TileAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.Visualize.TileAllocator"));
	FIntPoint TileCount = FMath::DivideAndRoundUp(VisualizeParameters.OutputViewSize, FIntPoint(FLumenVisualizeCreateRaysCS::GetThreadGroupSize2D()));
	uint32 MaxTileCount = TileCount.X * TileCount.Y;
	FRDGBufferRef TileDataPackedStructuredBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenVisualize::FTileDataPacked), MaxTileCount), TEXT("Lumen.Visualize.TileDataPacked"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TileAllocatorBuffer, PF_R32_UINT), 0);
	{
		FLumenVisualizeCreateTilesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenVisualizeCreateTilesCS::FParameters>();
		{
			// Input
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->VisualizeParameters = VisualizeParameters;

			// Output
			PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(TileAllocatorBuffer, PF_R32_UINT);
			PassParameters->RWTileDataPacked = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(TileDataPackedStructuredBuffer));
		}

		TShaderRef<FLumenVisualizeCreateTilesCS> ComputeShader = View.ShaderMap->GetShader<FLumenVisualizeCreateTilesCS>();

		const FIntVector GroupSize(TileCount.X, TileCount.Y, 1);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FLumenVisualizeCreateTilesCS"),
			ComputeShader,
			PassParameters,
			GroupSize);
	}

	// Generate rays
	// NOTE: GroupCount for emulated indirect-dispatch of raygen shaders dictates the maximum allocation size if GroupCount > MaxTileCount
	uint32 RayGenThreadCount = CVarLumenVisualizeHardwareRayTracingThreadCount.GetValueOnRenderThread();
	uint32 RayGenGroupCount = CVarLumenVisualizeHardwareRayTracingGroupCount.GetValueOnRenderThread();
	uint32 RayCount = FMath::Max(MaxTileCount, RayGenGroupCount) * FLumenVisualizeCreateRaysCS::GetThreadGroupSize1D();

	// Create rays within tiles
	FRDGBufferRef RayAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.Visualize.RayAllocator"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RayAllocatorBuffer, PF_R32_UINT), 0);

	FRDGBufferRef RayDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenVisualize::FRayDataPacked), RayCount), TEXT("Lumen.Visualize.RayDataPacked"));
	{
		FLumenVisualizeCreateRaysCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenVisualizeCreateRaysCS::FParameters>();
		{
			// Input
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->SceneTextures = SceneTextures;
			PassParameters->VisualizeParameters = VisualizeParameters;
			PassParameters->TileAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(TileAllocatorBuffer, PF_R32_UINT));
			PassParameters->TileDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(TileDataPackedStructuredBuffer));
			PassParameters->MaxTraceDistance = MaxTraceDistance;

			// Output
			PassParameters->RWRayAllocator = GraphBuilder.CreateUAV(RayAllocatorBuffer, PF_R32_UINT);
			PassParameters->RWRayDataPacked = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(RayDataPackedBuffer));
		}

		TShaderRef<FLumenVisualizeCreateRaysCS> ComputeShader = View.ShaderMap->GetShader<FLumenVisualizeCreateRaysCS>();

		const FIntVector GroupSize(VisualizeCreateRaysDispatchSizeX, FMath::DivideAndRoundUp<int32>(RayCount, FLumenVisualizeCreateRaysCS::GetThreadGroupSize1D() * VisualizeCreateRaysDispatchSizeX), 1);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FLumenVisualizeCreateRaysCS"),
			ComputeShader,
			PassParameters,
			GroupSize);
	}

	// Dispatch rays, resolving some of the screen with surface cache entries and collecting secondary rays for hit-lighting
	FRDGBufferRef TraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenVisualize::FTraceDataPacked), RayCount), TEXT("Lumen.Visualize.TraceDataPacked"));
	{
		FLumenVisualizeHardwareRayTracingRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenVisualizeHardwareRayTracingRGS::FParameters>();
		{
			SetLumenHardwareRayTracingSharedParameters(
				GraphBuilder,
				SceneTextures,
				View,
				TracingParameters,
				&PassParameters->SharedParameters);

			// Input
			PassParameters->VisualizeParameters = VisualizeParameters;
			PassParameters->RayAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayAllocatorBuffer, PF_R32_UINT));
			PassParameters->RayDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayDataPackedBuffer));

			PassParameters->ThreadCount = RayGenThreadCount;
			PassParameters->GroupCount = RayGenGroupCount;
			PassParameters->MaxTraversalIterations = LumenHardwareRayTracing::GetMaxTraversalIterations();
			PassParameters->MaxRayAllocationCount = RayCount;
			PassParameters->MaxTraceDistance = MaxTraceDistance;
			PassParameters->FarFieldMaxTraceDistance = FarFieldMaxTraceDistance;
			PassParameters->FarFieldReferencePos = (FVector3f)Lumen::GetFarFieldReferencePos();
			PassParameters->NearFieldMaxTraceDistanceDitherScale = Lumen::GetNearFieldMaxTraceDistanceDitherScale(bTraceFarField);
			PassParameters->NearFieldSceneRadius = Lumen::GetNearFieldSceneRadius(View, bTraceFarField);
			PassParameters->ApplySkylightStage = 1;

			if (bNeedTraceHairVoxel)
			{
				PassParameters->HairStrandsVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
			}

			// Output
			PassParameters->RWRadiance = GraphBuilder.CreateUAV(SceneColor);
			PassParameters->RWTraceDataPacked = GraphBuilder.CreateUAV(TraceDataPackedBuffer);
		}

		FLumenVisualizeHardwareRayTracingRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FHitLightingDim>(false);
		PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FFarFieldDim>(bTraceFarField);
		PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FHairStrandsOcclusionDim>(bNeedTraceHairVoxel);
		PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FRecursiveRefractionTraces>(bTraceTranslucent);

		FIntPoint DispatchResolution = FIntPoint(RayGenThreadCount, RayGenGroupCount);
		if (bInlineRayTracing)
		{
			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DispatchResolution, FLumenVisualizeHardwareRayTracingCS::GetThreadGroupSize(View.GetShaderPlatform()));
			FLumenVisualizeHardwareRayTracingCS::AddLumenRayTracingDispatch(
				GraphBuilder,
				RDG_EVENT_NAME("VisualizeHardwareRayTracing (inline) %ux%u", DispatchResolution.X, DispatchResolution.Y),
				View,
				PermutationVector,
				PassParameters,
				GroupCount,
				ERDGPassFlags::Compute);
		}
		else
		{
			FLumenVisualizeHardwareRayTracingRGS::AddLumenRayTracingDispatch(
				GraphBuilder,
				RDG_EVENT_NAME("VisualizeHardwareRayTracing (raygen) %ux%u", DispatchResolution.X, DispatchResolution.Y), 
				View,
				PermutationVector,
				PassParameters,
				DispatchResolution,
				true);
		}
	}

	FRDGBufferRef CompactRaysIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Visualize.CompactTracingIndirectArgs"));
	{
		FLumenVisualizeCompactRaysIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenVisualizeCompactRaysIndirectArgsCS::FParameters>();
		{
			PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
			PassParameters->RWCompactRaysIndirectArgs = GraphBuilder.CreateUAV(CompactRaysIndirectArgsBuffer, PF_R32_UINT);
		}

		TShaderRef<FLumenVisualizeCompactRaysIndirectArgsCS> ComputeShader = View.ShaderMap->GetShader<FLumenVisualizeCompactRaysIndirectArgsCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FLumenVisualizeCompactRaysIndirectArgsCS"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	// Fire secondary rays for hit-lighting, resolving some of the screen and collecting miss rays
	if (bRetraceForHitLighting)
	{
		// Compact rays which need to be re-traced
		if ((CVarLumenVisualizeHardwareRayTracingCompact.GetValueOnRenderThread() != 0) || bForceHitLighting)
		{
			FRDGBufferRef CompactedRayAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.Visualize.CompactedRayAllocator"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedRayAllocatorBuffer, PF_R32_UINT), 0);

			FRDGBufferRef CompactedRayDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenVisualize::FRayDataPacked), RayCount), TEXT("Lumen.Visualize.CompactedRayDataPacked"));
			FRDGBufferRef CompactedTraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenVisualize::FTraceDataPacked), RayCount), TEXT("Lumen.Visualize.CompactedTraceDataPacked"));
			{
				FLumenVisualizeCompactRaysCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenVisualizeCompactRaysCS::FParameters>();
				{
					// Input
					PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
					PassParameters->RayDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayDataPackedBuffer));
					PassParameters->TraceDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(TraceDataPackedBuffer));
					PassParameters->MaxRayAllocationCount = RayCount;

					// Output
					PassParameters->RWCompactedRayAllocator = GraphBuilder.CreateUAV(CompactedRayAllocatorBuffer, PF_R32_UINT);
					PassParameters->RWCompactedRayDataPacked = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(CompactedRayDataPackedBuffer));
					PassParameters->RWCompactedTraceDataPacked = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(CompactedTraceDataPackedBuffer));

					// Indirect args
					PassParameters->CompactRaysIndirectArgs = CompactRaysIndirectArgsBuffer;
				}

				FLumenVisualizeCompactRaysCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenVisualizeCompactRaysCS::FCompactModeDim>(bForceHitLighting ? LumenVisualize::ECompactMode::ForceHitLighting : LumenVisualize::ECompactMode::HitLightingRetrace);
				TShaderRef<FLumenVisualizeCompactRaysCS> ComputeShader = View.ShaderMap->GetShader<FLumenVisualizeCompactRaysCS>(PermutationVector);
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("FLumenVisualizeCompactRaysCS"),
					ComputeShader,
					PassParameters,
					PassParameters->CompactRaysIndirectArgs,
					0);
			}
			RayAllocatorBuffer = CompactedRayAllocatorBuffer;
			RayDataPackedBuffer = CompactedRayDataPackedBuffer;
			TraceDataPackedBuffer = CompactedTraceDataPackedBuffer;
		}

		// Bucket rays which hit objects, but do not have a surface-cache id by their material id
		if (CVarLumenVisualizeHardwareRayTracingBucketMaterials.GetValueOnRenderThread())
		{
			FRDGBufferRef BucketRaysByMaterialIdIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Visualize.BucketRaysByMaterialIdIndirectArgsBuffer"));
			{
				FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS::FParameters>();
				{
					PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
					PassParameters->RWBucketRaysByMaterialIdIndirectArgs = GraphBuilder.CreateUAV(BucketRaysByMaterialIdIndirectArgsBuffer, PF_R32_UINT);
				}

				TShaderRef<FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS> ComputeShader = View.ShaderMap->GetShader<FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS>();
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("FLumenVisualizeBucketRaysByMaterialIdIndirectArgsCS"),
					ComputeShader,
					PassParameters,
					FIntVector(1, 1, 1));
			}

			FRDGBufferRef BucketedRayDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenVisualize::FRayDataPacked), RayCount), TEXT("Lumen.Visualize.BucketedRayDataPacked"));
			FRDGBufferRef BucketedTraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenVisualize::FTraceDataPacked), RayCount), TEXT("Lumen.Visualize.BucketedTraceDataPacked"));
			{
				FLumenVisualizeBucketRaysByMaterialIdCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenVisualizeBucketRaysByMaterialIdCS::FParameters>();
				{
					// Input
					PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
					PassParameters->RayDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayDataPackedBuffer));
					PassParameters->TraceDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(TraceDataPackedBuffer));
					PassParameters->MaxRayAllocationCount = RayCount;

					// Output
					PassParameters->RWRayDataPacked = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(BucketedRayDataPackedBuffer));
					PassParameters->RWTraceDataPacked = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(BucketedTraceDataPackedBuffer));

					// Indirect args
					PassParameters->BucketRaysByMaterialIdIndirectArgs = BucketRaysByMaterialIdIndirectArgsBuffer;
				}

				TShaderRef<FLumenVisualizeBucketRaysByMaterialIdCS> ComputeShader = View.ShaderMap->GetShader<FLumenVisualizeBucketRaysByMaterialIdCS>();
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("FLumenVisualizeBucketRaysByMaterialIdCS"),
					ComputeShader,
					PassParameters,
					PassParameters->BucketRaysByMaterialIdIndirectArgs,
					0);

				RayDataPackedBuffer = BucketedRayDataPackedBuffer;
				TraceDataPackedBuffer = BucketedTraceDataPackedBuffer;
			}
		}

		FLumenVisualizeHardwareRayTracingRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenVisualizeHardwareRayTracingRGS::FParameters>();
		{
			SetLumenHardwareRayTracingSharedParameters(
				GraphBuilder,
				SceneTextures,
				View,
				TracingParameters,
				&PassParameters->SharedParameters);

			// Input
			PassParameters->VisualizeParameters = VisualizeParameters;
			PassParameters->RayAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayAllocatorBuffer, PF_R32_UINT));
			PassParameters->RayDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayDataPackedBuffer));
			PassParameters->TraceDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(TraceDataPackedBuffer));

			PassParameters->ThreadCount = RayGenThreadCount;
			PassParameters->GroupCount = RayGenGroupCount;
			PassParameters->LightingMode = (int32)Lumen::GetHardwareRayTracingLightingMode(View, bLumenGIEnabled);
			PassParameters->UseReflectionCaptures = Lumen::UseReflectionCapturesForHitLighting();
			PassParameters->MaxTraversalIterations = LumenHardwareRayTracing::GetMaxTraversalIterations();
			PassParameters->MaxRayAllocationCount = RayCount;
			PassParameters->MaxTraceDistance = MaxTraceDistance;
			PassParameters->FarFieldMaxTraceDistance = FarFieldMaxTraceDistance;
			PassParameters->FarFieldReferencePos = (FVector3f)Lumen::GetFarFieldReferencePos();
			PassParameters->NearFieldMaxTraceDistanceDitherScale = Lumen::GetNearFieldMaxTraceDistanceDitherScale(bTraceFarField);
			PassParameters->NearFieldSceneRadius = Lumen::GetNearFieldSceneRadius(View, bTraceFarField);
			// Even though the retrace should only be processing hits, which don't need skylight, the retrace may miss as it uses a different FRayTracingPipelineState
			PassParameters->ApplySkylightStage = 1;

			if (bNeedTraceHairVoxel)
			{
				PassParameters->HairStrandsVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
			}

			// Output
			PassParameters->RWRadiance = GraphBuilder.CreateUAV(SceneColor);
		}

		FLumenVisualizeHardwareRayTracingRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FHitLightingDim>(true);
		PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FFarFieldDim>(false);
		PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FHairStrandsOcclusionDim>(bNeedTraceHairVoxel);
		PermutationVector.Set<FLumenVisualizeHardwareRayTracingRGS::FRecursiveRefractionTraces>(bTraceTranslucent);
		TShaderRef<FLumenVisualizeHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenVisualizeHardwareRayTracingRGS>(PermutationVector);

		FIntPoint DispatchResolution = FIntPoint(RayGenThreadCount, RayGenGroupCount);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("VisualizeHardwareRayTracing[retrace for hit-lighting] %ux%u", DispatchResolution.X, DispatchResolution.Y),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, &View, RayGenerationShader, DispatchResolution](FRHIRayTracingCommandList& RHICmdList)
			{
				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenerationShader, *PassParameters);

				FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
				FRayTracingPipelineState* Pipeline = View.RayTracingMaterialPipeline;
				RHICmdList.RayTraceDispatch(Pipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, DispatchResolution.X, DispatchResolution.Y);
			}
		);
	}
}
#else
{
	unimplemented();
}
#endif
