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

// Actual screen-probe requirements..
#include "LumenRadianceCache.h"
#include "LumenScreenProbeGather.h"

#if RHI_RAYTRACING

#include "RayTracing/RayTracingDeferredMaterials.h"
#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"

static TAutoConsoleVariable<int32> CVarLumenScreenProbeGatherHardwareRayTracing(
	TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing"),
	1,
	TEXT("0. Software raytracing of diffuse indirect from Lumen cubemap tree.")
	TEXT("1. Enable hardware ray tracing of diffuse indirect. (Default)\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeGatherHardwareRayTracingIndirect(
	TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing.Indirect"),
	1,
	TEXT("Enables indirect ray tracing dispatch on compatible hardware (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingNormalBias(
	TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing.NormalBias"),
	.1f,
	TEXT("Bias along the shading normal, useful when the Ray Tracing geometry doesn't match the GBuffer (Nanite Proxy geometry)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingAvoidSelfIntersectionTraceDistance(
	TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing.AvoidSelfIntersectionTraceDistance"),
	5.0f,
	TEXT("Distance to trace with backface culling enabled, useful when the Ray Tracing geometry doesn't match the GBuffer (Nanite Proxy geometry)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingSkipFirstTwoSidedHitDistance(
	TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing.SkipFirstTwoSidedHitDistance"),
	1.0f,
	TEXT("When the AvoidSelfIntersectionTrace is enabled, the first Two sided material hit within this distance will be skipped.  This is useful for avoiding self-intersections with the Nanite fallback mesh on foliage, as AvoidSelfIntersectionTrace doesn't work on two sided materials."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeGatherHardwareRayTracingRetraceFarField(
	TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing.Retrace.FarField"),
	1,
	TEXT("Determines whether a second trace will be fired for far-field contribution (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeGatherHardwareRayTracingDefaultThreadCount(
	TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing.Default.ThreadCount"),
	32768,
	TEXT("Determines the active number of threads (Default = 32768)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeGatherHardwareRayTracingDefaultGroupCount(
	TEXT("r.Lumen.ScreenProbeGather.HardwareRayTracing.Default.GroupCount"),
	1,
	TEXT("Determines the active number of groups (Default = 1)"),
	ECVF_RenderThreadSafe
);

#endif // RHI_RAYTRACING

namespace Lumen
{
	bool UseHardwareRayTracedScreenProbeGather(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& (CVarLumenScreenProbeGatherHardwareRayTracing.GetValueOnAnyThread() != 0);
#else
		return false;
#endif
	}
}

#if RHI_RAYTRACING

class FConvertRayAllocatorCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FConvertRayAllocatorCS)
	SHADER_USE_PARAMETER_STRUCT(FConvertRayAllocatorCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, Allocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWRayAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FConvertRayAllocatorCS, "/Engine/Private/Lumen/LumenScreenProbeHardwareRayTracing.usf", "FConvertRayAllocatorCS", SF_Compute);

class FLumenScreenProbeGatherHardwareRayTracing : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenScreenProbeGatherHardwareRayTracing, Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)

	class FLightingModeDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_LIGHTING_MODE", LumenHWRTPipeline::ELightingMode);
	class FEnableNearFieldTracing : SHADER_PERMUTATION_BOOL("ENABLE_NEAR_FIELD_TRACING");
	class FEnableFarFieldTracing : SHADER_PERMUTATION_BOOL("ENABLE_FAR_FIELD_TRACING");
	class FRadianceCache : SHADER_PERMUTATION_BOOL("DIM_RADIANCE_CACHE");
	class FWriteFinalLightingDim : SHADER_PERMUTATION_BOOL("DIM_WRITE_FINAL_LIGHTING");
	class FIndirectDispatchDim : SHADER_PERMUTATION_BOOL("DIM_INDIRECT_DISPATCH");
	class FPackTraceDataDim : SHADER_PERMUTATION_BOOL("DIM_PACK_TRACE_DATA");
	class FStructuredImportanceSamplingDim : SHADER_PERMUTATION_BOOL("STRUCTURED_IMPORTANCE_SAMPLING");
	using FPermutationDomain = TShaderPermutationDomain<FLightingModeDim, FEnableNearFieldTracing, FEnableFarFieldTracing, FRadianceCache, FWriteFinalLightingDim, FIndirectDispatchDim, FStructuredImportanceSamplingDim, FPackTraceDataDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		RDG_BUFFER_ACCESS(HardwareRayTracingIndirectArgs, ERHIAccess::IndirectArgs | ERHIAccess::SRVCompute)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, TraceTexelDataPacked)

		// Screen probes
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenIndirectTracingParameters, IndirectTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)

		// Constants
		SHADER_PARAMETER(int, ThreadCount)
		SHADER_PARAMETER(int, GroupCount)
		SHADER_PARAMETER(float, PullbackBias)
		SHADER_PARAMETER(float, NormalBias)
		SHADER_PARAMETER(float, AvoidSelfIntersectionTraceDistance)
		SHADER_PARAMETER(float, SkipFirstTwoSidedHitDistance)
		SHADER_PARAMETER(int, MaxTranslucentSkipCount)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(int, ApplySkyLight)
		SHADER_PARAMETER(float, FarFieldBias)
		SHADER_PARAMETER(float, FarFieldMaxTraceDistance)
		SHADER_PARAMETER(FVector3f, FarFieldReferencePos)

		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FCompactedTraceParameters, CompactedTraceParameters)

		// Ray continuation buffer
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenHWRTPipeline::FTraceDataPacked>, RWRetraceDataPackedBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::AlwaysResidentPages, OutEnvironment);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		if (!FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType))
		{
			return false;
		}

		// Currently disable hit-lighting
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		bool bSurfaceCacheLightingMode = PermutationVector.Get<FLightingModeDim>() == LumenHWRTPipeline::ELightingMode::SurfaceCache;
		bool bWriteFinalLighting = PermutationVector.Get<FWriteFinalLightingDim>();

		return bSurfaceCacheLightingMode && bWriteFinalLighting;
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenScreenProbeGatherHardwareRayTracing)

IMPLEMENT_GLOBAL_SHADER(FLumenScreenProbeGatherHardwareRayTracingCS, "/Engine/Private/Lumen/LumenScreenProbeHardwareRayTracing.usf", "LumenScreenProbeGatherHardwareRayTracingCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLumenScreenProbeGatherHardwareRayTracingRGS, "/Engine/Private/Lumen/LumenScreenProbeHardwareRayTracing.usf", "LumenScreenProbeGatherHardwareRayTracingRGS", SF_RayGen);

class FLumenScreenProbeGatherHardwareRayTracingIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenScreenProbeGatherHardwareRayTracingIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenScreenProbeGatherHardwareRayTracingIndirectArgsCS, FGlobalShader)

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

IMPLEMENT_GLOBAL_SHADER(FLumenScreenProbeGatherHardwareRayTracingIndirectArgsCS, "/Engine/Private/Lumen/LumenScreenProbeHardwareRayTracing.usf", "FLumenScreenProbeHardwareRayTracingIndirectArgsCS", SF_Compute);

bool UseFarFieldForScreenProbeGather(const FSceneViewFamily& ViewFamily)
{
	return Lumen::UseFarField(ViewFamily) && CVarLumenScreenProbeGatherHardwareRayTracingRetraceFarField.GetValueOnRenderThread();
}

bool IsHitLightingForceEnabledForScreenProbeGather()
{
	return false;
}

bool IsHardwareRayTracingScreenProbeGatherIndirectDispatch()
{
	return GRHISupportsRayTracingDispatchIndirect && (CVarLumenScreenProbeGatherHardwareRayTracingIndirect.GetValueOnRenderThread() == 1);
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingScreenProbeGather(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	// Hit-lighting is disabled
	if (Lumen::UseHardwareRayTracedScreenProbeGather(*View.Family) && false)
	{
		bool bUseFarFieldForScreenProbeGather = UseFarFieldForScreenProbeGather(*View.Family);
		bool bApplySkyLight = !bUseFarFieldForScreenProbeGather;
		bool bUseRadianceCache = LumenScreenProbeGather::UseRadianceCache(View);
		const bool bIsForceHitLighting = IsHitLightingForceEnabledForScreenProbeGather();

		FLumenScreenProbeGatherHardwareRayTracingRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::HitLighting);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FRadianceCache>(bUseRadianceCache);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FEnableNearFieldTracing>(true);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FEnableFarFieldTracing>(false);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FWriteFinalLightingDim>(!bIsForceHitLighting || !bUseFarFieldForScreenProbeGather);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FIndirectDispatchDim>(IsHardwareRayTracingScreenProbeGatherIndirectDispatch());
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FStructuredImportanceSamplingDim>(LumenScreenProbeGather::UseImportanceSampling(View));
		TShaderRef<FLumenScreenProbeGatherHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenScreenProbeGatherHardwareRayTracingRGS>(PermutationVector);

		OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
	}
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingScreenProbeGatherDeferredMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingScreenProbeGatherLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedScreenProbeGather(*View.Family))
	{
		const bool bUseFarFieldForScreenProbeGather = UseFarFieldForScreenProbeGather(*View.Family);

		// Default trace
		{
			bool bApplySkyLight = !bUseFarFieldForScreenProbeGather;
			bool bUseRadianceCache = LumenScreenProbeGather::UseRadianceCache(View);
			const bool bIsForceHitLighting = IsHitLightingForceEnabledForScreenProbeGather();

			FLumenScreenProbeGatherHardwareRayTracingRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::SurfaceCache);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FRadianceCache>(bUseRadianceCache);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FEnableNearFieldTracing>(true);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FEnableFarFieldTracing>(false);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FWriteFinalLightingDim>(!bIsForceHitLighting || !bUseFarFieldForScreenProbeGather);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FIndirectDispatchDim>(IsHardwareRayTracingScreenProbeGatherIndirectDispatch());
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FStructuredImportanceSamplingDim>(LumenScreenProbeGather::UseImportanceSampling(View));
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FPackTraceDataDim>(bUseFarFieldForScreenProbeGather);
			TShaderRef<FLumenScreenProbeGatherHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenScreenProbeGatherHardwareRayTracingRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}

		// Far-field
		if (bUseFarFieldForScreenProbeGather)
		{
			bool bApplySkyLight = !bUseFarFieldForScreenProbeGather;
			bool bUseRadianceCache = LumenScreenProbeGather::UseRadianceCache(View);
			const bool bIsForceHitLighting = IsHitLightingForceEnabledForScreenProbeGather();

			FLumenScreenProbeGatherHardwareRayTracingRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::SurfaceCache);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FRadianceCache>(bUseRadianceCache);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FEnableNearFieldTracing>(false);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FEnableFarFieldTracing>(true);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FWriteFinalLightingDim>(true);
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FIndirectDispatchDim>(IsHardwareRayTracingScreenProbeGatherIndirectDispatch());
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FStructuredImportanceSamplingDim>(LumenScreenProbeGather::UseImportanceSampling(View));
			PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracingRGS::FPackTraceDataDim>(false);
			TShaderRef<FLumenScreenProbeGatherHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenScreenProbeGatherHardwareRayTracingRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

void SetLumenHardwareRayTracingScreenProbeParameters(
	FRDGBuilder& GraphBuilder,
	const FSceneTextureParameters& SceneTextures,
	FScreenProbeParameters& ScreenProbeParameters,
	const FViewInfo& View,
	const FLumenCardTracingInputs& TracingInputs,
	FRDGBufferRef HardwareRayTracingIndirectArgsBuffer,
	FLumenIndirectTracingParameters& IndirectTracingParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	const FCompactedTraceParameters& CompactedTraceParameters,
	bool bApplySkyLight,
	bool bUseRadianceCache,
	bool bEnableHitLighting,
	bool bEnableFarFieldTracing,
	FRDGBufferRef RayAllocatorBuffer,
	FRDGBufferRef TraceTexelDataPackedBuffer,
	FRDGBufferRef RetraceDataPackedBuffer,
	FLumenScreenProbeGatherHardwareRayTracingRGS::FParameters* Parameters
)
{
	uint32 DefaultThreadCount = CVarLumenScreenProbeGatherHardwareRayTracingDefaultThreadCount.GetValueOnRenderThread();
	uint32 DefaultGroupCount = CVarLumenScreenProbeGatherHardwareRayTracingDefaultGroupCount.GetValueOnRenderThread();

	SetLumenHardwareRayTracingSharedParameters(
		GraphBuilder,
		SceneTextures,
		View,
		TracingInputs,
		&Parameters->SharedParameters
	);

	Parameters->HardwareRayTracingIndirectArgs = HardwareRayTracingIndirectArgsBuffer;
	Parameters->RayAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayAllocatorBuffer, PF_R32_UINT));
	Parameters->TraceTexelDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(TraceTexelDataPackedBuffer));

	Parameters->IndirectTracingParameters = IndirectTracingParameters;
	Parameters->ScreenProbeParameters = ScreenProbeParameters;
	Parameters->RadianceCacheParameters = RadianceCacheParameters;
	Parameters->CompactedTraceParameters = CompactedTraceParameters;

	// Constants
	Parameters->ThreadCount = DefaultThreadCount;
	Parameters->GroupCount = DefaultGroupCount;
	Parameters->FarFieldBias = LumenHardwareRayTracing::GetFarFieldBias();
	Parameters->FarFieldMaxTraceDistance = Lumen::GetFarFieldMaxTraceDistance();
	Parameters->FarFieldReferencePos = (FVector3f)Lumen::GetFarFieldReferencePos();
	Parameters->PullbackBias = Lumen::GetHardwareRayTracingPullbackBias();
	Parameters->NormalBias = CVarLumenHardwareRayTracingNormalBias.GetValueOnRenderThread();
	Parameters->AvoidSelfIntersectionTraceDistance = FMath::Max(CVarLumenHardwareRayTracingAvoidSelfIntersectionTraceDistance.GetValueOnRenderThread(), 0.0f);
	Parameters->SkipFirstTwoSidedHitDistance = CVarLumenHardwareRayTracingSkipFirstTwoSidedHitDistance.GetValueOnRenderThread();
	Parameters->MaxTranslucentSkipCount = Lumen::GetMaxTranslucentSkipCount();
	Parameters->MaxTraversalIterations = LumenHardwareRayTracing::GetMaxTraversalIterations();
	Parameters->ApplySkyLight = bApplySkyLight;

	// Ray continuation buffer
	Parameters->RWRetraceDataPackedBuffer = GraphBuilder.CreateUAV(RetraceDataPackedBuffer);
}

void DispatchLumenScreenProbeGatherHardwareRayTracingIndirectArgs(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGBufferRef HardwareRayTracingIndirectArgsBuffer, FRDGBufferRef RayAllocatorBuffer, FIntPoint OutputThreadGroupSize)
{
	FLumenScreenProbeGatherHardwareRayTracingIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenScreenProbeGatherHardwareRayTracingIndirectArgsCS::FParameters>();

	PassParameters->RayAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayAllocatorBuffer, PF_R32_UINT));
	PassParameters->RWHardwareRayTracingIndirectArgs = GraphBuilder.CreateUAV(HardwareRayTracingIndirectArgsBuffer, PF_R32_UINT);
	PassParameters->OutputThreadGroupSize = OutputThreadGroupSize;

	TShaderRef<FLumenScreenProbeGatherHardwareRayTracingIndirectArgsCS> ComputeShader = View.ShaderMap->GetShader<FLumenScreenProbeGatherHardwareRayTracingIndirectArgsCS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("LumenScreenProbeGatherHardwareRayTracingIndirectArgsCS"),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1));
}

void DispatchRayGenOrComputeShader(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	FScreenProbeParameters& ScreenProbeParameters,
	const FLumenCardTracingInputs& TracingInputs,
	FLumenIndirectTracingParameters& IndirectTracingParameters,
	const FCompactedTraceParameters& CompactedTraceParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	const FLumenScreenProbeGatherHardwareRayTracingRGS::FPermutationDomain& PermutationVector,
	uint32 RayCount,
	bool bApplySkyLight,
	bool bUseRadianceCache,
	bool bInlineRayTracing,
	FRDGBufferRef RayAllocatorBuffer,
	FRDGBufferRef TraceTexelDataPackedBuffer,
	FRDGBufferRef RetraceDataPackedBuffer
)
{
	FRDGBufferRef HardwareRayTracingIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.ScreenProbeGather.HardwareRayTracing.IndirectArgsCS"));
	FIntPoint OutputThreadGroupSize = bInlineRayTracing ? FLumenScreenProbeGatherHardwareRayTracingCS::GetThreadGroupSize() : FLumenScreenProbeGatherHardwareRayTracingRGS::GetThreadGroupSize();
	DispatchLumenScreenProbeGatherHardwareRayTracingIndirectArgs(GraphBuilder, View, HardwareRayTracingIndirectArgsBuffer, RayAllocatorBuffer, OutputThreadGroupSize);

	uint32 DefaultThreadCount = CVarLumenScreenProbeGatherHardwareRayTracingDefaultThreadCount.GetValueOnRenderThread();
	uint32 DefaultGroupCount = CVarLumenScreenProbeGatherHardwareRayTracingDefaultGroupCount.GetValueOnRenderThread();

	bool bEnableHitLighting = PermutationVector.Get<FLumenScreenProbeGatherHardwareRayTracingRGS::FLightingModeDim>() == LumenHWRTPipeline::ELightingMode::HitLighting;
	bool bEnableFarFieldTracing = PermutationVector.Get<FLumenScreenProbeGatherHardwareRayTracingRGS::FEnableFarFieldTracing>();

	FLumenScreenProbeGatherHardwareRayTracing::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenScreenProbeGatherHardwareRayTracing::FParameters>();
	SetLumenHardwareRayTracingScreenProbeParameters(GraphBuilder,
		SceneTextures,
		ScreenProbeParameters,
		View,
		TracingInputs,
		HardwareRayTracingIndirectArgsBuffer,
		IndirectTracingParameters,
		RadianceCacheParameters,
		CompactedTraceParameters,
		bApplySkyLight,
		bUseRadianceCache,
		bEnableHitLighting,
		bEnableFarFieldTracing,
		RayAllocatorBuffer,
		TraceTexelDataPackedBuffer,
		RetraceDataPackedBuffer,
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
		FString ResolutionStr = IsHardwareRayTracingScreenProbeGatherIndirectDispatch() ? FString::Printf(TEXT("<indirect>")) :
			FString::Printf(TEXT("%ux%u"), DispatchResolution.X, DispatchResolution.Y);

		return ResolutionStr;
	};

	if (bInlineRayTracing)
	{
		TShaderRef<FLumenScreenProbeGatherHardwareRayTracingCS> ComputeShader = View.ShaderMap->GetShader<FLumenScreenProbeGatherHardwareRayTracingCS>(PermutationVector);
		if (IsHardwareRayTracingScreenProbeGatherIndirectDispatch())
		{			
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing (inline) %s %s", *GenerateModeString(), *GenerateResolutionString()),
				ComputeShader,
				PassParameters,
				PassParameters->HardwareRayTracingIndirectArgs,
				0);
		}
		else
		{
			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DispatchResolution, FLumenScreenProbeGatherHardwareRayTracingCS::GetThreadGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing (inline) %s %s", *GenerateModeString(), *GenerateResolutionString()),
				ComputeShader,
				PassParameters,
				GroupCount);
		}
	}
	else
	{
		const bool bUseMinimalPayload = !bEnableHitLighting;

		TShaderRef<FLumenScreenProbeGatherHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenScreenProbeGatherHardwareRayTracingRGS>(PermutationVector);
		if (IsHardwareRayTracingScreenProbeGatherIndirectDispatch())
		{
			AddLumenRayTraceDispatchIndirectPass(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing (raygen) %s %s", *GenerateModeString(), *GenerateResolutionString()),
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
				RDG_EVENT_NAME("HardwareRayTracing (raygen) %s %s", *GenerateModeString(), *GenerateResolutionString()),
				RayGenerationShader,
				PassParameters,
				DispatchResolution,
				View,
				bUseMinimalPayload);
		}
	}	
}

#endif // RHI_RAYTRACING

void RenderHardwareRayTracingScreenProbe(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextureParameters& SceneTextures,
	FScreenProbeParameters& ScreenProbeParameters,
	const FViewInfo& View,
	const FLumenCardTracingInputs& TracingInputs,
	FLumenIndirectTracingParameters& IndirectTracingParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	const FCompactedTraceParameters& CompactedTraceParameters)
#if RHI_RAYTRACING
{
	const uint32 NumTracesPerProbe = ScreenProbeParameters.ScreenProbeTracingOctahedronResolution * ScreenProbeParameters.ScreenProbeTracingOctahedronResolution;
	FIntPoint RayTracingResolution = FIntPoint(ScreenProbeParameters.ScreenProbeAtlasViewSize.X * ScreenProbeParameters.ScreenProbeAtlasViewSize.Y * NumTracesPerProbe, 1);
	int32 MaxRayCount = RayTracingResolution.X * RayTracingResolution.Y;

	FRDGBufferRef RayAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.ScreenProbeGather.HardwareRayTracing.RayAllocatorBuffer"));
	{
		FConvertRayAllocatorCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FConvertRayAllocatorCS::FParameters>();
		{
			PassParameters->Allocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CompactedTraceParameters.CompactedTraceTexelAllocator->Desc.Buffer));
			PassParameters->RWRayAllocator = GraphBuilder.CreateUAV(RayAllocatorBuffer, PF_R32_UINT);
		}

		TShaderRef<FConvertRayAllocatorCS> ComputeShader = View.ShaderMap->GetShader<FConvertRayAllocatorCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FConvertRayAllocatorCS"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FRDGBufferRef RetraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenHWRTPipeline::FTraceDataPacked), MaxRayCount), TEXT("Lumen.ScreenProbeGather.HardwareRayTracing.TraceDataPacked"));
	FRDGBufferRef TraceTexelDataPackedBuffer = CompactedTraceParameters.CompactedTraceTexelData->Desc.Buffer;

	const bool bUseFarFieldForScreenProbeGather = UseFarFieldForScreenProbeGather(*View.Family);
	const bool bIsForceHitLighting = IsHitLightingForceEnabledForScreenProbeGather();
	const bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);
	const bool bUseRadianceCache = LumenScreenProbeGather::UseRadianceCache(View);

	// Default tracing of near-field, extract surface cache and material-id
	{
		bool bApplySkyLight = !bUseFarFieldForScreenProbeGather;

		FLumenScreenProbeGatherHardwareRayTracing::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::SurfaceCache);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FRadianceCache>(bUseRadianceCache);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FEnableNearFieldTracing>(true);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FEnableFarFieldTracing>(false);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FWriteFinalLightingDim>(!bIsForceHitLighting || !bUseFarFieldForScreenProbeGather);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FIndirectDispatchDim>(IsHardwareRayTracingScreenProbeGatherIndirectDispatch());
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FStructuredImportanceSamplingDim>(LumenScreenProbeGather::UseImportanceSampling(View));
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FPackTraceDataDim>(bUseFarFieldForScreenProbeGather);

		DispatchRayGenOrComputeShader(GraphBuilder, Scene, SceneTextures, View, ScreenProbeParameters, TracingInputs, IndirectTracingParameters, CompactedTraceParameters, RadianceCacheParameters,
			PermutationVector, MaxRayCount, bApplySkyLight, bUseRadianceCache, bInlineRayTracing,
			RayAllocatorBuffer, TraceTexelDataPackedBuffer, RetraceDataPackedBuffer);
	}

	FRDGBufferRef FarFieldRayAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.ScreenProbeGather.HardwareRayTracing.FarFieldRayAllocatorBuffer"));
	FRDGBufferRef FarFieldRetraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenHWRTPipeline::FTraceDataPacked), MaxRayCount), TEXT("Lumen.ScreenProbeGather.HardwareRayTracing.FarFieldRetraceDataPackedBuffer"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FarFieldRayAllocatorBuffer, PF_R32_UINT), 0);
	if (bUseFarFieldForScreenProbeGather)
	{
		LumenHWRTCompactRays(GraphBuilder, Scene, View, MaxRayCount, LumenHWRTPipeline::ECompactMode::FarFieldRetrace,
			RayAllocatorBuffer, RetraceDataPackedBuffer,
			FarFieldRayAllocatorBuffer, FarFieldRetraceDataPackedBuffer);

		bool bApplySkyLight = true;

		FLumenScreenProbeGatherHardwareRayTracing::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FLightingModeDim>(LumenHWRTPipeline::ELightingMode::SurfaceCache);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FRadianceCache>(bUseRadianceCache);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FEnableNearFieldTracing>(false);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FEnableFarFieldTracing>(true);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FWriteFinalLightingDim>(true);
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FIndirectDispatchDim>(IsHardwareRayTracingScreenProbeGatherIndirectDispatch());
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FStructuredImportanceSamplingDim>(LumenScreenProbeGather::UseImportanceSampling(View));
		PermutationVector.Set<FLumenScreenProbeGatherHardwareRayTracing::FPackTraceDataDim>(false);

		// Trace continuation rays
		DispatchRayGenOrComputeShader(GraphBuilder, Scene, SceneTextures, View, ScreenProbeParameters, TracingInputs, IndirectTracingParameters, CompactedTraceParameters, RadianceCacheParameters,
			PermutationVector, MaxRayCount, bApplySkyLight, bUseRadianceCache, bInlineRayTracing,
			FarFieldRayAllocatorBuffer, TraceTexelDataPackedBuffer, FarFieldRetraceDataPackedBuffer);
	}
}
#else // RHI_RAYTRACING
{
	unimplemented();
}
#endif // RHI_RAYTRACING
