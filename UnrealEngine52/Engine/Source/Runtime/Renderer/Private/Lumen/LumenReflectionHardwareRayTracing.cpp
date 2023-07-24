// Copyright Epic Games, Inc. All Rights Reserved.

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

#if RHI_RAYTRACING
#include "RayTracing/RayTracingDeferredMaterials.h"
#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracing(
	TEXT("r.Lumen.Reflections.HardwareRayTracing"),
	1,
	TEXT("Enables hardware ray tracing for Lumen reflections (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracingIndirect(
	TEXT("r.Lumen.Reflections.HardwareRayTracing.Indirect"),
	1,
	TEXT("Enables indirect ray tracing dispatch on compatible hardware (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracingDefaultThreadCount(
	TEXT("r.Lumen.Reflections.HardwareRayTracing.Default.ThreadCount"),
	32768,
	TEXT("Determines the active number of threads (Default = 32768)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracingDefaultGroupCount(
	TEXT("r.Lumen.Reflections.HardwareRayTracing.Default.GroupCount"),
	1,
	TEXT("Determines the active number of groups (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracingBucketMaterials(
	TEXT("r.Lumen.Reflections.HardwareRayTracing.BucketMaterials"),
	1,
	TEXT("Determines whether a secondary traces will be bucketed for coherent material access (default = 1"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracingRetraceHitLighting(
	TEXT("r.Lumen.Reflections.HardwareRayTracing.Retrace.HitLighting"),
	0,
	TEXT("Determines whether a second trace will be fired for hit-lighting for invalid surface-cache hits (Default = 0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracingRetraceFarField(
	TEXT("r.Lumen.Reflections.HardwareRayTracing.Retrace.FarField"),
	1,
	TEXT("Determines whether a second trace will be fired for far-field contribution (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracingRetraceThreadCount(
	TEXT("r.Lumen.Reflections.HardwareRayTracing.Retrace.ThreadCount"),
	32768,
	TEXT("Determines the active number of threads for re-traces (Default = 32768)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenReflectionsHardwareRayTracingRetraceGroupCount(
	TEXT("r.Lumen.Reflections.HardwareRayTracing.Retrace.GroupCount"),
	1,
	TEXT("Determines the active number of groups for re-traces (Default = 1)"),
	ECVF_RenderThreadSafe
);

#endif // RHI_RAYTRACING

namespace Lumen
{
	bool UseHardwareRayTracedReflections(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled() 
			&& Lumen::UseHardwareRayTracing(ViewFamily) 
			&& (CVarLumenReflectionsHardwareRayTracing.GetValueOnAnyThread() != 0);
#else
		return false;
#endif
	}
} // namespace Lumen

bool LumenReflections::UseFarField(const FSceneViewFamily& ViewFamily)
{
#if RHI_RAYTRACING
	return Lumen::UseFarField(ViewFamily) && CVarLumenReflectionsHardwareRayTracingRetraceFarField.GetValueOnRenderThread();
#else
	return false;
#endif
}

#if RHI_RAYTRACING

bool IsHardwareRayTracingReflectionsIndirectDispatch()
{
	return GRHISupportsRayTracingDispatchIndirect && (CVarLumenReflectionsHardwareRayTracingIndirect.GetValueOnRenderThread() == 1);
}

class FLumenReflectionHardwareRayTracing : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenReflectionHardwareRayTracing, Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)

	class FLightingModeDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_LIGHTING_MODE", LumenHWRTPipeline::ELightingMode);
	class FEnableNearFieldTracing : SHADER_PERMUTATION_BOOL("ENABLE_NEAR_FIELD_TRACING");
	class FEnableFarFieldTracing : SHADER_PERMUTATION_BOOL("ENABLE_FAR_FIELD_TRACING");
	class FRadianceCache : SHADER_PERMUTATION_BOOL("DIM_RADIANCE_CACHE");
	class FWriteFinalLightingDim : SHADER_PERMUTATION_BOOL("DIM_WRITE_FINAL_LIGHTING");
	class FHairStrandsOcclusionDim : SHADER_PERMUTATION_BOOL("DIM_HAIRSTRANDS_VOXEL");
	class FPackTraceDataDim : SHADER_PERMUTATION_BOOL("DIM_PACK_TRACE_DATA");
	class FIndirectDispatchDim : SHADER_PERMUTATION_BOOL("DIM_INDIRECT_DISPATCH");
	using FPermutationDomain = TShaderPermutationDomain<FLightingModeDim, FEnableNearFieldTracing, FEnableFarFieldTracing, FRadianceCache, FWriteFinalLightingDim, FHairStrandsOcclusionDim, FIndirectDispatchDim, FPackTraceDataDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		RDG_BUFFER_ACCESS(HardwareRayTracingIndirectArgs, ERHIAccess::IndirectArgs | ERHIAccess::SRVCompute)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, TraceTexelDataPacked)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHZBScreenTraceParameters, HZBScreenTraceParameters)
		SHADER_PARAMETER(float, RelativeDepthThickness)
		SHADER_PARAMETER(float, SampleSceneColorNormalTreshold)
		SHADER_PARAMETER(int32, SampleSceneColor)

		SHADER_PARAMETER(int, ThreadCount)
		SHADER_PARAMETER(int, GroupCount)
		SHADER_PARAMETER(int, NearFieldLightingMode)

		SHADER_PARAMETER(float, FarFieldBias)
		SHADER_PARAMETER(float, FarFieldMaxTraceDistance)
		SHADER_PARAMETER(float, FarFieldDitheredStartDistanceFactor)
		SHADER_PARAMETER(float, PullbackBias)
		SHADER_PARAMETER(int, MaxTranslucentSkipCount)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(int, ApplySkyLight)
		SHADER_PARAMETER(FVector3f, FarFieldReferencePos)

		// Reflection-specific includes (includes output targets)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTracingParameters, ReflectionTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenReflectionTileParameters, ReflectionTileParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, HairStrandsVoxel)

		// Ray continuation buffer
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenHWRTPipeline::FTraceDataPacked>, RWTraceDataPacked)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		if (!FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		LumenHWRTPipeline::ELightingMode LightingMode = PermutationVector.Get<FLightingModeDim>();
		bool bEnableNearFieldTracing = PermutationVector.Get<FEnableNearFieldTracing>();
		bool bInlineRayTracing = ShaderDispatchType == Lumen::ERayTracingShaderDispatchType::Inline;

		if (PermutationVector.Get<FRadianceCache>() && !bEnableNearFieldTracing)
		{
			return false;
		}

		if (PermutationVector.Get<FHairStrandsOcclusionDim>() && (!PermutationVector.Get<FWriteFinalLightingDim>() || !PermutationVector.Get<FEnableNearFieldTracing>()))
		{
			return false;
		}

		if (LightingMode == LumenHWRTPipeline::ELightingMode::SurfaceCache)
		{
			bool bEnableFarFieldTracing = PermutationVector.Get<FEnableFarFieldTracing>();
			bool bWriteFinalLighting = PermutationVector.Get<FWriteFinalLightingDim>();

			return (bEnableNearFieldTracing && !bEnableFarFieldTracing) ||
				(!bEnableNearFieldTracing && bEnableFarFieldTracing && bWriteFinalLighting);
		}
		else if (LightingMode == LumenHWRTPipeline::ELightingMode::HitLighting)
		{
			bool bFWriteFinalLighting = PermutationVector.Get<FWriteFinalLightingDim>();
			bool bFPackTraceData = PermutationVector.Get<FPackTraceDataDim>();

			return !bInlineRayTracing && bEnableNearFieldTracing && bFWriteFinalLighting && !bFPackTraceData;
		}

		return false;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::HighResPages, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		FPermutationDomain PermutationVector(PermutationId);
		if (PermutationVector.Get<FLightingModeDim>() == LumenHWRTPipeline::ELightingMode::SurfaceCache)
		{
			return ERayTracingPayloadType::LumenMinimal;
		}
		else
		{
			return ERayTracingPayloadType::RayTracingMaterial;
		}
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenReflectionHardwareRayTracing)

IMPLEMENT_GLOBAL_SHADER(FLumenReflectionHardwareRayTracingCS, "/Engine/Private/Lumen/LumenReflectionHardwareRayTracing.usf", "LumenReflectionHardwareRayTracingCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLumenReflectionHardwareRayTracingRGS, "/Engine/Private/Lumen/LumenReflectionHardwareRayTracing.usf", "LumenReflectionHardwareRayTracingRGS", SF_RayGen);

class FLumenReflectionHardwareRayTracingIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenReflectionHardwareRayTracingIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenReflectionHardwareRayTracingIndirectArgsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWHardwareRayTracingIndirectArgs)
		SHADER_PARAMETER(FIntPoint, OutputThreadGroupSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenReflectionHardwareRayTracingIndirectArgsCS, "/Engine/Private/Lumen/LumenReflectionHardwareRayTracing.usf", "FLumenReflectionHardwareRayTracingIndirectArgsCS", SF_Compute);

bool LumenReflections::IsHitLightingForceEnabled(const FViewInfo& View)
{
	return Lumen::GetHardwareRayTracingLightingMode(View) != Lumen::EHardwareRayTracingLightingMode::LightingFromSurfaceCache;
}

bool LumenReflections::UseHitLighting(const FViewInfo& View)
{
	return IsHitLightingForceEnabled(View) || (CVarLumenReflectionsHardwareRayTracingRetraceHitLighting.GetValueOnRenderThread() != 0);
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingReflections(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedReflections(*View.Family) && LumenReflections::UseHitLighting(View))
	{
		for (int RadianceCacheDim = 0; RadianceCacheDim < FLumenReflectionHardwareRayTracingRGS::FRadianceCache::PermutationCount; ++RadianceCacheDim)
		{
			for (int32 HairOcclusion = 0; HairOcclusion < 2; HairOcclusion++)
			{
				FLumenReflectionHardwareRayTracingRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::HitLighting);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FRadianceCache>(RadianceCacheDim != 0);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FEnableNearFieldTracing>(true);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FEnableFarFieldTracing>(LumenReflections::UseFarField(*View.Family));
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FWriteFinalLightingDim>(true);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FHairStrandsOcclusionDim>(HairOcclusion == 0);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FIndirectDispatchDim>(IsHardwareRayTracingReflectionsIndirectDispatch());
				TShaderRef<FLumenReflectionHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenReflectionHardwareRayTracingRGS>(PermutationVector);
				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}
		}
	}
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingReflectionsDeferredMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingReflectionsLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedReflections(*View.Family))
	{
		const bool bUseFarFieldForReflections = LumenReflections::UseFarField(*View.Family);
		const bool bUseHitLightingForReflections = LumenReflections::UseHitLighting(View);
		const bool bIsHitLightingForceEnabled = LumenReflections::IsHitLightingForceEnabled(View);

		// Default
		for (int RadianceCacheDim = 0; RadianceCacheDim < FLumenReflectionHardwareRayTracingRGS::FRadianceCache::PermutationCount; ++RadianceCacheDim)
		{
			for (int32 HairOcclusion = 0; HairOcclusion < 2; HairOcclusion++)
			{
				const bool bWriteFinalLighting = !bIsHitLightingForceEnabled || !bUseFarFieldForReflections;

				FLumenReflectionHardwareRayTracingRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::SurfaceCache);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FRadianceCache>(RadianceCacheDim != 0);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FEnableNearFieldTracing>(true);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FEnableFarFieldTracing>(false);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FWriteFinalLightingDim>(bWriteFinalLighting);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FHairStrandsOcclusionDim>(bWriteFinalLighting && HairOcclusion == 0);
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FIndirectDispatchDim>(IsHardwareRayTracingReflectionsIndirectDispatch());
				PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FPackTraceDataDim>(bUseHitLightingForReflections || bUseFarFieldForReflections);
				TShaderRef<FLumenReflectionHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenReflectionHardwareRayTracingRGS>(PermutationVector);
				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}
		}

		// Far-field continuation
		if (bUseFarFieldForReflections)
		{
			FLumenReflectionHardwareRayTracingRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::SurfaceCache);
			PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FRadianceCache>(false);
			PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FEnableNearFieldTracing>(false);
			PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FEnableFarFieldTracing>(true);
			PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FWriteFinalLightingDim>(true);
			PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FHairStrandsOcclusionDim>(false);
			PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FIndirectDispatchDim>(IsHardwareRayTracingReflectionsIndirectDispatch());
			PermutationVector.Set<FLumenReflectionHardwareRayTracingRGS::FPackTraceDataDim>(bUseHitLightingForReflections);
			TShaderRef<FLumenReflectionHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenReflectionHardwareRayTracingRGS>(PermutationVector);
			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

void SetLumenHardwareRayTracingReflectionParameters(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FSceneTextureParameters& SceneTextureParameters,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	const FLumenReflectionTracingParameters& ReflectionTracingParameters,
	const FLumenReflectionTileParameters& ReflectionTileParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	float MaxTraceDistance,
	bool bApplySkyLight,
	bool bEnableHitLighting,
	bool bEnableFarFieldTracing,
	bool bSampleSceneColorAtHit,
	bool bNeedTraceHairVoxel,
	FRDGBufferRef HardwareRayTracingIndirectArgsBuffer,
	FRDGBufferRef RayAllocatorBuffer,
	FRDGBufferRef TraceTexelDataPackedBuffer,
	FRDGBufferRef TraceDataPackedBuffer,
	FLumenReflectionHardwareRayTracing::FParameters* Parameters
)
{
	uint32 DefaultThreadCount = CVarLumenReflectionsHardwareRayTracingDefaultThreadCount.GetValueOnRenderThread();
	uint32 DefaultGroupCount = CVarLumenReflectionsHardwareRayTracingDefaultGroupCount.GetValueOnRenderThread();

	SetLumenHardwareRayTracingSharedParameters(
		GraphBuilder,
		SceneTextureParameters,
		View,
		TracingParameters,
		&Parameters->SharedParameters
	);
	Parameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
	Parameters->HardwareRayTracingIndirectArgs = HardwareRayTracingIndirectArgsBuffer;
	Parameters->TraceTexelDataPacked = GraphBuilder.CreateSRV(TraceTexelDataPackedBuffer, PF_R32G32_UINT);

	Parameters->HZBScreenTraceParameters = SetupHZBScreenTraceParameters(GraphBuilder, View, SceneTextures);

	if (Parameters->HZBScreenTraceParameters.PrevSceneColorTexture == SceneTextures.Color.Resolve || !Parameters->SharedParameters.SceneTextures.GBufferVelocityTexture)
	{
		Parameters->SharedParameters.SceneTextures.GBufferVelocityTexture = GSystemTextures.GetBlackDummy(GraphBuilder);
	}

	extern float GLumenReflectionSampleSceneColorRelativeDepthThreshold;
	Parameters->RelativeDepthThickness = GLumenReflectionSampleSceneColorRelativeDepthThreshold;
	Parameters->SampleSceneColorNormalTreshold = LumenReflections::GetSampleSceneColorNormalTreshold();
	Parameters->SampleSceneColor = bSampleSceneColorAtHit ? 1 : 0;

	Parameters->ThreadCount = DefaultThreadCount;
	Parameters->GroupCount = DefaultGroupCount;
	Parameters->NearFieldLightingMode = static_cast<int32>(Lumen::GetHardwareRayTracingLightingMode(View));
	Parameters->FarFieldBias = LumenHardwareRayTracing::GetFarFieldBias();
	Parameters->FarFieldMaxTraceDistance = Lumen::GetFarFieldMaxTraceDistance();
	Parameters->FarFieldDitheredStartDistanceFactor = LumenReflections::UseFarField(*View.Family) ? Lumen::GetFarFieldDitheredStartDistanceFactor() : 1.0;
	Parameters->FarFieldReferencePos = (FVector3f)Lumen::GetFarFieldReferencePos();
	Parameters->PullbackBias = Lumen::GetHardwareRayTracingPullbackBias();
	Parameters->MaxTranslucentSkipCount = Lumen::GetMaxTranslucentSkipCount();
	Parameters->MaxTraversalIterations = LumenHardwareRayTracing::GetMaxTraversalIterations();
	Parameters->ApplySkyLight = bApplySkyLight;

	// Reflection-specific
	Parameters->ReflectionTracingParameters = ReflectionTracingParameters;
	Parameters->ReflectionTileParameters = ReflectionTileParameters;
	Parameters->RadianceCacheParameters = RadianceCacheParameters;

	if (bNeedTraceHairVoxel)
	{
		Parameters->HairStrandsVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
	}

	// Ray continuation buffer
	Parameters->RWTraceDataPacked = GraphBuilder.CreateUAV(TraceDataPackedBuffer);
}

void DispatchLumenReflectionHardwareRayTracingIndirectArgs(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGBufferRef HardwareRayTracingIndirectArgsBuffer, FRDGBufferRef RayAllocatorBuffer, FIntPoint OutputThreadGroupSize, ERDGPassFlags ComputePassFlags)
{
	FLumenReflectionHardwareRayTracingIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionHardwareRayTracingIndirectArgsCS::FParameters>();
	
	PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
	PassParameters->RWHardwareRayTracingIndirectArgs = GraphBuilder.CreateUAV(HardwareRayTracingIndirectArgsBuffer, PF_R32_UINT);
	PassParameters->OutputThreadGroupSize = OutputThreadGroupSize;

	TShaderRef<FLumenReflectionHardwareRayTracingIndirectArgsCS> ComputeShader = View.ShaderMap->GetShader<FLumenReflectionHardwareRayTracingIndirectArgsCS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ReflectionCompactRaysIndirectArgs"),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1));
}

void DispatchRayGenOrComputeShader(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FSceneTextureParameters& SceneTextureParameters,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	const FLumenReflectionTracingParameters& ReflectionTracingParameters,
	const FLumenReflectionTileParameters& ReflectionTileParameters,
	const FCompactedReflectionTraceParameters& CompactedTraceParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	const FLumenReflectionHardwareRayTracing::FPermutationDomain& PermutationVector,
	float MaxTraceDistance,
	uint32 RayCount,
	bool bApplySkyLight,
	bool bUseRadianceCache,
	bool bInlineRayTracing,
	bool bSampleSceneColorAtHit,
	bool bNeedTraceHairVoxel,
	FRDGBufferRef RayAllocatorBuffer,
	FRDGBufferRef TraceTexelDataPackedBuffer,
	FRDGBufferRef TraceDataPackedBuffer,
	ERDGPassFlags ComputePassFlags
)
{
	FRDGBufferRef HardwareRayTracingIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Reflection.CompactTracingIndirectArgs"));
	FIntPoint OutputThreadGroupSize = bInlineRayTracing ? FLumenReflectionHardwareRayTracingCS::GetThreadGroupSize() : FLumenReflectionHardwareRayTracingRGS::GetThreadGroupSize();
	DispatchLumenReflectionHardwareRayTracingIndirectArgs(GraphBuilder, View, HardwareRayTracingIndirectArgsBuffer, RayAllocatorBuffer, OutputThreadGroupSize, ComputePassFlags);

	uint32 DefaultThreadCount = CVarLumenReflectionsHardwareRayTracingDefaultThreadCount.GetValueOnRenderThread();
	uint32 DefaultGroupCount = CVarLumenReflectionsHardwareRayTracingDefaultGroupCount.GetValueOnRenderThread();

	bool bEnableHitLighting = PermutationVector.Get<FLumenReflectionHardwareRayTracingRGS::FLightingModeDim>() == LumenHWRTPipeline::ELightingMode::HitLighting;
	bool bEnableFarFieldTracing = PermutationVector.Get<FLumenReflectionHardwareRayTracingRGS::FEnableFarFieldTracing>();

	FLumenReflectionHardwareRayTracing::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenReflectionHardwareRayTracing::FParameters>();
	SetLumenHardwareRayTracingReflectionParameters(
		GraphBuilder,
		SceneTextures,
		SceneTextureParameters,
		View,
		TracingParameters,
		ReflectionTracingParameters,
		ReflectionTileParameters,
		RadianceCacheParameters,
		MaxTraceDistance,
		bApplySkyLight,
		bEnableHitLighting,
		bEnableFarFieldTracing,
		bSampleSceneColorAtHit,
		bNeedTraceHairVoxel,
		HardwareRayTracingIndirectArgsBuffer,
		RayAllocatorBuffer,
		TraceTexelDataPackedBuffer,
		TraceDataPackedBuffer,
		PassParameters
	);
	
	auto GenerateModeString = [bEnableHitLighting, bEnableFarFieldTracing]()
	{
		FString ModeStr = bEnableHitLighting ? FString::Printf(TEXT("[hit-lighting]")) :
			(bEnableFarFieldTracing ? FString::Printf(TEXT("[far-field]")) : FString::Printf(TEXT("[default]")));

		return ModeStr;
	};

	FIntPoint DispatchResolution = FIntPoint(DefaultThreadCount, DefaultGroupCount);
	auto GenerateResolutionString = [DispatchResolution]()
	{
		FString ResolutionStr = IsHardwareRayTracingReflectionsIndirectDispatch() ? FString::Printf(TEXT("<indirect>")) :
			FString::Printf(TEXT("%ux%u"), DispatchResolution.X, DispatchResolution.Y);

		return ResolutionStr;
	};

	if (bInlineRayTracing)
	{
		TShaderRef<FLumenReflectionHardwareRayTracingCS> ComputeShader = View.ShaderMap->GetShader<FLumenReflectionHardwareRayTracingCS>(PermutationVector);

		// Inline always runs as an indirect compute shader
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ReflectionHardwareRayTracing (inline) %s <indirect>", *GenerateModeString()),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			PassParameters->HardwareRayTracingIndirectArgs,
			0);		
	}
	else
	{
		const bool bUseMinimalPayload = !bEnableHitLighting;

		TShaderRef<FLumenReflectionHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenReflectionHardwareRayTracingRGS>(PermutationVector);		
		if (IsHardwareRayTracingReflectionsIndirectDispatch())
		{
			AddLumenRayTraceDispatchIndirectPass(
				GraphBuilder,
				RDG_EVENT_NAME("ReflectionHardwareRayTracing (raygen) %s %s", *GenerateModeString(), *GenerateResolutionString()),
				RayGenerationShader,
				PassParameters,
				PassParameters->HardwareRayTracingIndirectArgs,
				0,
				View,
				bUseMinimalPayload);
		}
		else
		{
			AddLumenRayTraceDispatchPass(
				GraphBuilder,
				RDG_EVENT_NAME("ReflectionHardwareRayTracing (raygen) %s %s", *GenerateModeString(), *GenerateResolutionString()),
				RayGenerationShader,
				PassParameters,
				DispatchResolution,
				View,
				bUseMinimalPayload);
		}
	}
}

#endif // RHI_RAYTRACING

void RenderLumenHardwareRayTracingReflections(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FSceneTextureParameters& SceneTextureParameters,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	const FLumenReflectionTracingParameters& ReflectionTracingParameters,
	const FLumenReflectionTileParameters& ReflectionTileParameters,
	const FCompactedReflectionTraceParameters& CompactedTraceParameters,
	float MaxVoxelTraceDistance,
	bool bUseRadianceCache,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	bool bSampleSceneColorAtHit,
	ERDGPassFlags ComputePassFlags
)
{
#if RHI_RAYTRACING
	FIntPoint BufferSize = ReflectionTracingParameters.ReflectionTracingBufferSize;
	int32 RayCount = BufferSize.X * BufferSize.Y;

	FRDGBufferRef TraceDataPackedBufferCached = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenHWRTPipeline::FTraceDataPacked), RayCount), TEXT("Lumen.Reflections.TraceDataPacked"));
	FRDGBufferRef RayAllocatorBufferCached = CompactedTraceParameters.CompactedTraceTexelAllocator->Desc.Buffer;
	FRDGBufferRef TraceTexelDataPackedBufferCached = CompactedTraceParameters.CompactedTraceTexelData->Desc.Buffer;

	const bool bUseHitLighting = LumenReflections::UseHitLighting(View);
	const bool bIsHitLightingForceEnabled = LumenReflections::IsHitLightingForceEnabled(View);
	const bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family) && !bIsHitLightingForceEnabled;
	const bool bUseFarFieldForReflections = LumenReflections::UseFarField(*View.Family);
	extern int32 GLumenReflectionHairStrands_VoxelTrace;
	const bool bNeedTraceHairVoxel = HairStrands::HasViewHairStrandsVoxelData(View) && GLumenReflectionHairStrands_VoxelTrace > 0;
	const bool bIndirectDispatch = IsHardwareRayTracingReflectionsIndirectDispatch() || bInlineRayTracing;

	checkf(ComputePassFlags != ERDGPassFlags::AsyncCompute || bInlineRayTracing, TEXT("Async Lumen HWRT is only supported for inline ray tracing"));

	// Default tracing of near-field, extract surface cache and material-id
	{
		bool bApplySkyLight = !bUseFarFieldForReflections;
		const bool bWriteFinalLighting = !bIsHitLightingForceEnabled || !bUseFarFieldForReflections;

		FLumenReflectionHardwareRayTracing::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::SurfaceCache);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FRadianceCache>(bUseRadianceCache);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FEnableNearFieldTracing>(true);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FEnableFarFieldTracing>(false);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FWriteFinalLightingDim>(bWriteFinalLighting);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FHairStrandsOcclusionDim>(bWriteFinalLighting && bNeedTraceHairVoxel);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FIndirectDispatchDim>(bIndirectDispatch);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FPackTraceDataDim>(bUseHitLighting || bUseFarFieldForReflections);

		DispatchRayGenOrComputeShader(GraphBuilder, SceneTextures, SceneTextureParameters, Scene, View, TracingParameters, ReflectionTracingParameters, ReflectionTileParameters, CompactedTraceParameters, RadianceCacheParameters,
			PermutationVector, MaxVoxelTraceDistance, RayCount, bApplySkyLight, bUseRadianceCache, bInlineRayTracing, bSampleSceneColorAtHit, bNeedTraceHairVoxel,
			RayAllocatorBufferCached, TraceTexelDataPackedBufferCached, TraceDataPackedBufferCached, ComputePassFlags);

	}

	// Default tracing of far-field, extract material-id
	FRDGBufferRef FarFieldRayAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.Reflection.FarFieldRayAllocator"));
	//FRDGBufferRef FarFieldTraceTexelDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32) * 2, RayCount), TEXT("Lumen.Reflection.FarFieldTraceTexelDataPacked"));
	FRDGBufferRef FarFieldTraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenHWRTPipeline::FTraceDataPacked), RayCount), TEXT("Lumen.Reflection.FarFieldTraceDataPacked"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FarFieldRayAllocatorBuffer, PF_R32_UINT), 0, ComputePassFlags);
	if (bUseFarFieldForReflections)
	{
		LumenHWRTCompactRays(GraphBuilder, Scene, View, RayCount, LumenHWRTPipeline::ECompactMode::FarFieldRetrace,
			RayAllocatorBufferCached, TraceDataPackedBufferCached,
			FarFieldRayAllocatorBuffer, FarFieldTraceDataPackedBuffer, ComputePassFlags);

		bool bApplySkyLight = true;

		FLumenReflectionHardwareRayTracing::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::SurfaceCache);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FRadianceCache>(false);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FEnableNearFieldTracing>(false);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FEnableFarFieldTracing>(true);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FWriteFinalLightingDim>(true);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FIndirectDispatchDim>(IsHardwareRayTracingReflectionsIndirectDispatch());
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FHairStrandsOcclusionDim>(false);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FIndirectDispatchDim>(IsHardwareRayTracingReflectionsIndirectDispatch());
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FIndirectDispatchDim>(bIndirectDispatch);
		PermutationVector.Set<FLumenReflectionHardwareRayTracing::FPackTraceDataDim>(bUseHitLighting);

		// Trace continuation rays
		DispatchRayGenOrComputeShader(GraphBuilder, SceneTextures, SceneTextureParameters, Scene, View, TracingParameters, ReflectionTracingParameters, ReflectionTileParameters, CompactedTraceParameters, RadianceCacheParameters,
			PermutationVector, MaxVoxelTraceDistance, RayCount, bApplySkyLight, bUseRadianceCache, bInlineRayTracing, bSampleSceneColorAtHit, bNeedTraceHairVoxel,
			FarFieldRayAllocatorBuffer, TraceTexelDataPackedBufferCached, FarFieldTraceDataPackedBuffer, ComputePassFlags);
	}

	// Re-trace for hit-lighting
	if (bUseHitLighting)
	{
		FRDGBufferRef RayAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.Reflection.CompactedRayAllocator"));
		//FRDGBufferRef TraceTexelDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32) * 2, RayCount), TEXT("Lumen.Reflection.BucketedTexelTraceDataPackedBuffer"));
		FRDGBufferRef TraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenHWRTPipeline::FTraceDataPacked), RayCount), TEXT("Lumen.Reflection.CompactedTraceDataPacked"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RayAllocatorBuffer, PF_R32_UINT), 0, ComputePassFlags);

		LumenHWRTPipeline::ECompactMode CompactMode = bIsHitLightingForceEnabled ?
			LumenHWRTPipeline::ECompactMode::ForceHitLighting : LumenHWRTPipeline::ECompactMode::HitLightingRetrace;
		LumenHWRTCompactRays(GraphBuilder, Scene, View, RayCount, CompactMode,
			RayAllocatorBufferCached, TraceDataPackedBufferCached,
			RayAllocatorBuffer, TraceDataPackedBuffer, ComputePassFlags);

		// Append far-field rays
		if (bUseFarFieldForReflections)
		{
			LumenHWRTCompactRays(GraphBuilder, Scene, View, RayCount, LumenHWRTPipeline::ECompactMode::AppendRays,
				FarFieldRayAllocatorBuffer, FarFieldTraceDataPackedBuffer,
				RayAllocatorBuffer, TraceDataPackedBuffer, ComputePassFlags);
		}

		// Sort rays by material
		if (CVarLumenReflectionsHardwareRayTracingBucketMaterials.GetValueOnRenderThread())
		{
			LumenHWRTBucketRaysByMaterialID(GraphBuilder, Scene, View, RayCount, RayAllocatorBuffer, TraceDataPackedBuffer, ComputePassFlags);
		}

		// Trace with hit-lighting
		{
			bool bApplySkyLight = true;
			bool bUseInline = false;

			FLumenReflectionHardwareRayTracing::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenReflectionHardwareRayTracing::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::HitLighting);
			PermutationVector.Set<FLumenReflectionHardwareRayTracing::FRadianceCache>(bUseRadianceCache);
			PermutationVector.Set<FLumenReflectionHardwareRayTracing::FEnableNearFieldTracing>(true);
			PermutationVector.Set<FLumenReflectionHardwareRayTracing::FEnableFarFieldTracing>(bUseFarFieldForReflections);
			PermutationVector.Set<FLumenReflectionHardwareRayTracing::FWriteFinalLightingDim>(true);
			PermutationVector.Set<FLumenReflectionHardwareRayTracing::FHairStrandsOcclusionDim>(bNeedTraceHairVoxel);
			PermutationVector.Set<FLumenReflectionHardwareRayTracing::FIndirectDispatchDim>(IsHardwareRayTracingReflectionsIndirectDispatch());
			PermutationVector.Set<FLumenReflectionHardwareRayTracing::FPackTraceDataDim>(false);

			DispatchRayGenOrComputeShader(GraphBuilder, SceneTextures, SceneTextureParameters, Scene, View, TracingParameters, ReflectionTracingParameters, ReflectionTileParameters, CompactedTraceParameters, RadianceCacheParameters,
				PermutationVector, MaxVoxelTraceDistance, RayCount, bApplySkyLight, bUseRadianceCache, bUseInline, bSampleSceneColorAtHit, bNeedTraceHairVoxel,
				RayAllocatorBuffer, TraceTexelDataPackedBufferCached, TraceDataPackedBuffer, ComputePassFlags);
		}
	}
#else
	unimplemented();
#endif
}
