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

#include "LumenRadianceCache.h"

#if RHI_RAYTRACING

#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"

static TAutoConsoleVariable<int32> CVarLumenRadianceCacheHardwareRayTracing(
	TEXT("r.Lumen.RadianceCache.HardwareRayTracing"),
	1,
	TEXT("Enables hardware ray tracing for Lumen radiance cache (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenRadianceCacheTemporaryBufferAllocationDownsampleFactor(
	TEXT("r.Lumen.RadianceCache.HardwareRayTracing.TemporaryBufferAllocationDownsampleFactor"),
	8,
	TEXT("Downsample factor on the temporary buffer used by Hardware Ray Tracing Radiance Cache.  Higher downsample factors save more transient allocator memory, but may cause overflow and artifacts."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenRadianceCacheHardwareRayTracingRetraceFarField(
	TEXT("r.Lumen.RadianceCache.HardwareRayTracing.Retrace.FarField"),
	1,
	TEXT("Determines whether a second trace will be fired for far-field contribution (Default = 1)"),
	ECVF_RenderThreadSafe
);

#endif // RHI_RAYTRACING

namespace Lumen
{
	bool UseHardwareRayTracedRadianceCache(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& (CVarLumenRadianceCacheHardwareRayTracing.GetValueOnRenderThread() != 0);
#else
		return false;
#endif
	}
}

#if RHI_RAYTRACING

namespace LumenRadianceCache
{
	enum class ERayTracingPass
	{
		Default,
		FarField,
		MAX
	};
}

class FLumenRadianceCacheHardwareRayTracing : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenRadianceCacheHardwareRayTracing, Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)

	class FRayTracingPass : SHADER_PERMUTATION_ENUM_CLASS("RAY_TRACING_PASS", LumenRadianceCache::ERayTracingPass);
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FRayTracingPass>;

	// Parameters
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		RDG_BUFFER_ACCESS(HardwareRayTracingIndirectArgs, ERHIAccess::IndirectArgs | ERHIAccess::SRVCompute)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelAllocator)

		// Probe data
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenIndirectTracingParameters, IndirectTracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, ProbeTraceTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)

		// Constants
		SHADER_PARAMETER(float, FarFieldBias)
		SHADER_PARAMETER(float, NearFieldMaxTraceDistance)
		SHADER_PARAMETER(float, NearFieldSceneRadius)
		SHADER_PARAMETER(float, FarFieldMaxTraceDistance)
		SHADER_PARAMETER(float, PullbackBias)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(FVector3f, FarFieldReferencePos)
		SHADER_PARAMETER(uint32, TempAtlasNumTraceTiles)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWTraceRadianceTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWTraceHitTexture)
	END_SHADER_PARAMETER_STRUCT()

	static uint32 GetGroupSize()
	{
		// Must match RADIANCE_CACHE_TRACE_TILE_SIZE_2D
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FRayTracingPass>() == LumenRadianceCache::ERayTracingPass::Default)
		{
			OutEnvironment.SetDefine(TEXT("ENABLE_NEAR_FIELD_TRACING"), 1);
		}

		if (PermutationVector.Get<FRayTracingPass>() == LumenRadianceCache::ERayTracingPass::FarField)
		{
			OutEnvironment.SetDefine(TEXT("ENABLE_FAR_FIELD_TRACING"), 1);
		}
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		return FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenRadianceCacheHardwareRayTracing)

IMPLEMENT_GLOBAL_SHADER(FLumenRadianceCacheHardwareRayTracingCS, "/Engine/Private/Lumen/LumenRadianceCacheHardwareRayTracing.usf", "LumenRadianceCacheHardwareRayTracingCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLumenRadianceCacheHardwareRayTracingRGS, "/Engine/Private/Lumen/LumenRadianceCacheHardwareRayTracing.usf", "LumenRadianceCacheHardwareRayTracingRGS", SF_RayGen);

class FLumenRadianceCacheHardwareRayTracingIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenRadianceCacheHardwareRayTracingIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenRadianceCacheHardwareRayTracingIndirectArgsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelAllocator)
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
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenRadianceCacheHardwareRayTracingIndirectArgsCS, "/Engine/Private/Lumen/LumenRadianceCacheHardwareRayTracing.usf", "LumenRadianceCacheHardwareRayTracingIndirectArgsCS", SF_Compute);

class FSplatRadianceCacheIntoAtlasCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSplatRadianceCacheIntoAtlasCS)
	SHADER_USE_PARAMETER_STRUCT(FSplatRadianceCacheIntoAtlasCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWRadianceProbeAtlasTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWDepthProbeAtlasTexture)	
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TraceHitTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TraceRadianceTexture)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenIndirectTracingParameters, IndirectTracingParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, ProbeTraceTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceTileAllocator)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		RDG_BUFFER_ACCESS(TraceProbesIndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER(uint32, TempAtlasNumTraceTiles)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		// Must match RADIANCE_CACHE_TRACE_TILE_SIZE_2D
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Workaround for an internal PC FXC compiler crash when compiling with disabled optimizations
		if (Parameters.Platform == SP_PCD3D_SM5)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_ForceOptimization);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FSplatRadianceCacheIntoAtlasCS, "/Engine/Private/Lumen/LumenRadianceCacheHardwareRayTracing.usf", "SplatRadianceCacheIntoAtlasCS", SF_Compute);

class FRadianceCacheCompactTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRadianceCacheCompactTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FRadianceCacheCompactTracesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompactedTraceTexelAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompactedTraceTexelData)
		RDG_BUFFER_ACCESS(TraceProbesIndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, ProbeTraceTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TraceHitTexture)
		SHADER_PARAMETER(uint32, TempAtlasNumTraceTiles)
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

IMPLEMENT_GLOBAL_SHADER(FRadianceCacheCompactTracesCS, "/Engine/Private/Lumen/LumenRadianceCacheHardwareRayTracing.usf", "RadianceCacheCompactTracesCS", SF_Compute);

bool UseFarFieldForRadianceCache(const FSceneViewFamily& ViewFamily)
{
	return Lumen::UseFarField(ViewFamily) && CVarLumenRadianceCacheHardwareRayTracingRetraceFarField.GetValueOnRenderThread();
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingRadianceCache(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingRadianceCacheLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedRadianceCache(*View.Family))
	{
		// Default trace
		{
			FLumenRadianceCacheHardwareRayTracingRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenRadianceCacheHardwareRayTracingRGS::FRayTracingPass>(LumenRadianceCache::ERayTracingPass::Default);
			TShaderRef<FLumenRadianceCacheHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenRadianceCacheHardwareRayTracingRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}

		if (UseFarFieldForRadianceCache(*View.Family))
		{
			FLumenRadianceCacheHardwareRayTracingRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenRadianceCacheHardwareRayTracingRGS::FRayTracingPass>(LumenRadianceCache::ERayTracingPass::FarField);
			TShaderRef<FLumenRadianceCacheHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenRadianceCacheHardwareRayTracingRGS>(PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

void DispatchRayGenOrComputeShader(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FSceneTextureParameters& SceneTextures,
	const FLumenCardTracingParameters& TracingParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	const FLumenRadianceCacheHardwareRayTracing::FPermutationDomain& PermutationVector,
	bool bInlineRayTracing,
	bool bUseFarField,
	uint32 TempAtlasNumTraceTiles,
	FRDGBufferRef ProbeTraceTileAllocator,
	FRDGBufferRef ProbeTraceTileData,
	FRDGBufferRef ProbeTraceData,
	FRDGBufferRef CompactedTraceTexelAllocator,
	FRDGBufferRef CompactedTraceTexelData,
	FRDGTextureRef TraceRadianceTexture,
	FRDGTextureRef TraceHitTexture,
	ERDGPassFlags ComputePassFlags
)
{
	// Setup indirect parameters
	FRDGBufferRef HardwareRayTracingIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.RadianceCache.HardwareRayTracing.IndirectArgsBuffer"));
	{
		FLumenRadianceCacheHardwareRayTracingIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenRadianceCacheHardwareRayTracingIndirectArgsCS::FParameters>();
		{
			PassParameters->CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CompactedTraceTexelAllocator, PF_R32_UINT));
			PassParameters->RWHardwareRayTracingIndirectArgs = GraphBuilder.CreateUAV(HardwareRayTracingIndirectArgsBuffer, PF_R32_UINT);
			PassParameters->OutputThreadGroupSize = bInlineRayTracing ? FLumenRadianceCacheHardwareRayTracingCS::GetThreadGroupSize(View.GetShaderPlatform()) : FLumenRadianceCacheHardwareRayTracingRGS::GetThreadGroupSize();
		}

		TShaderRef<FLumenRadianceCacheHardwareRayTracingIndirectArgsCS> ComputeShader = View.ShaderMap->GetShader<FLumenRadianceCacheHardwareRayTracingIndirectArgsCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HardwareRayTracingIndirectArgsCS"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FLumenRadianceCacheHardwareRayTracing::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenRadianceCacheHardwareRayTracing::FParameters>();
	{
		SetLumenHardwareRayTracingSharedParameters(
			GraphBuilder,
			SceneTextures,
			View,
			TracingParameters,
			&PassParameters->SharedParameters);

		SetupLumenDiffuseTracingParametersForProbe(View, PassParameters->IndirectTracingParameters, /*DiffuseConeHalfAngle*/ -1.0f);

		PassParameters->RadianceCacheParameters = RadianceCacheParameters;
		PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData, PF_A32B32G32R32F));
		PassParameters->ProbeTraceTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileData, PF_R32G32_UINT));
		PassParameters->ProbeTraceTileAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileAllocator, PF_R32_UINT));
		PassParameters->HardwareRayTracingIndirectArgs = HardwareRayTracingIndirectArgsBuffer;
		PassParameters->CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator);
		PassParameters->CompactedTraceTexelData = CompactedTraceTexelData ? GraphBuilder.CreateSRV(CompactedTraceTexelData) : nullptr;

		// Constants
		PassParameters->NearFieldMaxTraceDistance = PassParameters->IndirectTracingParameters.MaxTraceDistance;
		PassParameters->NearFieldSceneRadius = Lumen::GetNearFieldSceneRadius(View, bUseFarField);
		PassParameters->FarFieldBias = LumenHardwareRayTracing::GetFarFieldBias();
		PassParameters->FarFieldMaxTraceDistance = Lumen::GetFarFieldMaxTraceDistance();
		PassParameters->FarFieldReferencePos = (FVector3f)Lumen::GetFarFieldReferencePos();
		PassParameters->PullbackBias = Lumen::GetHardwareRayTracingPullbackBias();
		PassParameters->MaxTraversalIterations = LumenHardwareRayTracing::GetMaxTraversalIterations();
		PassParameters->TempAtlasNumTraceTiles = TempAtlasNumTraceTiles;

		PassParameters->RWTraceRadianceTexture = GraphBuilder.CreateUAV(TraceRadianceTexture);
		PassParameters->RWTraceHitTexture = GraphBuilder.CreateUAV(TraceHitTexture);
	}

	const FString RayTracingPassName = PermutationVector.Get<FLumenRadianceCacheHardwareRayTracingRGS::FRayTracingPass>() == LumenRadianceCache::ERayTracingPass::FarField ? TEXT("(far-field)") : TEXT("");

	if (bInlineRayTracing)
	{
		// Inline always runs as an indirect compute shader
		FLumenRadianceCacheHardwareRayTracingCS::AddLumenRayTracingDispatchIndirect(
			GraphBuilder,
			RDG_EVENT_NAME("HardwareRayTracingCS%s", *RayTracingPassName),
			View,
			PermutationVector,
			PassParameters,
			PassParameters->HardwareRayTracingIndirectArgs,
			0, 
			ComputePassFlags);
	}
	else
	{
		FLumenRadianceCacheHardwareRayTracingRGS::AddLumenRayTracingDispatchIndirect(
			GraphBuilder,
			RDG_EVENT_NAME("HardwareRayTracingRGS%s", *RayTracingPassName),
			View,
			PermutationVector,
			PassParameters,
			PassParameters->HardwareRayTracingIndirectArgs,
			0,
			/*bUseMinimalPayload*/ true);
	}	
}

#endif // RHI_RAYTRACING

extern int32 GRadianceCacheForceFullUpdate;

void LumenRadianceCache::RenderLumenHardwareRayTracingRadianceCache(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	FRadianceCacheConfiguration Configuration,
	int32 MaxNumProbes,
	int32 MaxProbeTraceTileResolution,
	FRDGBufferRef ProbeTraceData,
	FRDGBufferRef ProbeTraceTileData,
	FRDGBufferRef ProbeTraceTileAllocator,
	FRDGBufferRef TraceProbesIndirectArgs,
	FRDGBufferRef HardwareRayTracingRayAllocatorBuffer,
	FRDGBufferRef RadianceCacheHardwareRayTracingIndirectArgs,
	FRDGTextureUAVRef RadianceProbeAtlasTextureUAV,
	FRDGTextureUAVRef DepthProbeTextureUAV,
	ERDGPassFlags ComputePassFlags
)
{
#if RHI_RAYTRACING
	// Must match usf
	const int32 TempAtlasTraceTileStride = 1024;
	const int32 TraceTileSize = 8;

	// Overflow is possible however unlikely - only nearby probes trace at max resolution
	const int32 TemporaryBufferAllocationDownsampleFactor = GRadianceCacheForceFullUpdate ? 4 : CVarLumenRadianceCacheTemporaryBufferAllocationDownsampleFactor.GetValueOnRenderThread();
	const int32 TempAtlasNumTraceTilesPerProbe = FMath::DivideAndRoundUp(MaxProbeTraceTileResolution * MaxProbeTraceTileResolution, TemporaryBufferAllocationDownsampleFactor);
	const int32 TempAtlasNumTraceTiles = MaxNumProbes * TempAtlasNumTraceTilesPerProbe;
	const FIntPoint WrappedTraceTileLayout(
		TempAtlasTraceTileStride,
		FMath::DivideAndRoundUp(TempAtlasNumTraceTiles, TempAtlasTraceTileStride));
	const FIntPoint TempTraceAtlasResolution = WrappedTraceTileLayout * TraceTileSize;

	FRDGTextureRef TraceRadianceTexture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(TempTraceAtlasResolution, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("Lumen.RadianceCache.TraceRadiance"));

	FRDGTextureRef TraceHitTexture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(TempTraceAtlasResolution, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("Lumen.RadianceCache.TraceHit"));

	const bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);
	const bool bUseFarField = UseFarFieldForRadianceCache(*View.Family) && Configuration.bFarField;
	
	checkf(ComputePassFlags != ERDGPassFlags::AsyncCompute || bInlineRayTracing, TEXT("Async Lumen HWRT is only supported for inline ray tracing"));

	// Default tracing of near-field, extract surface cache and material-id
	{
		FLumenRadianceCacheHardwareRayTracing::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenRadianceCacheHardwareRayTracing::FRayTracingPass>(ERayTracingPass::Default);

		DispatchRayGenOrComputeShader(GraphBuilder, Scene, View, SceneTextures, TracingParameters, RadianceCacheParameters, PermutationVector,
			bInlineRayTracing, bUseFarField, TempAtlasNumTraceTiles, ProbeTraceTileAllocator, ProbeTraceTileData, ProbeTraceData, HardwareRayTracingRayAllocatorBuffer, nullptr,
			TraceRadianceTexture, TraceHitTexture, ComputePassFlags);
	}

	if (bUseFarField)
	{
		FRDGBufferRef CompactedTraceTexelAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2), TEXT("Lumen.RadianceCache.CompactedTraceTexelAllocator"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedTraceTexelAllocator, PF_R32_UINT), 0, ComputePassFlags);

		const int32 NumCompactedTraceTexelDataElements = TempTraceAtlasResolution.X * TempTraceAtlasResolution.Y;
		FRDGBufferRef CompactedTraceTexelData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumCompactedTraceTexelDataElements), TEXT("Lumen.RadianceCache.CompactedTraceTexelData"));

		// Compact unfinished traces
		{
			FRadianceCacheCompactTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRadianceCacheCompactTracesCS::FParameters>();
			PassParameters->RWCompactedTraceTexelAllocator = GraphBuilder.CreateUAV(CompactedTraceTexelAllocator, PF_R32_UINT);
			PassParameters->RWCompactedTraceTexelData = GraphBuilder.CreateUAV(CompactedTraceTexelData, PF_R32_UINT);
			PassParameters->TraceProbesIndirectArgs = TraceProbesIndirectArgs;
			PassParameters->RadianceCacheParameters = RadianceCacheParameters;
			PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData, PF_A32B32G32R32F));
			PassParameters->ProbeTraceTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileData, PF_R32G32_UINT));
			PassParameters->ProbeTraceTileAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileAllocator, PF_R32_UINT));
			PassParameters->TraceHitTexture = TraceHitTexture;
			PassParameters->TempAtlasNumTraceTiles = TempAtlasNumTraceTiles;

			auto ComputeShader = View.ShaderMap->GetShader<FRadianceCacheCompactTracesCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("CompactTraces"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				TraceProbesIndirectArgs,
				0);
		}

		FLumenRadianceCacheHardwareRayTracing::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenRadianceCacheHardwareRayTracing::FRayTracingPass>(ERayTracingPass::FarField);

		DispatchRayGenOrComputeShader(GraphBuilder, Scene, View, SceneTextures, TracingParameters, RadianceCacheParameters, PermutationVector,
			bInlineRayTracing, bUseFarField, TempAtlasNumTraceTiles, ProbeTraceTileAllocator, ProbeTraceTileData, ProbeTraceData, CompactedTraceTexelAllocator, CompactedTraceTexelData,
			TraceRadianceTexture, TraceHitTexture, ComputePassFlags);
	}

	// Write temporary results to atlas, possibly up-sampling
	{
		FSplatRadianceCacheIntoAtlasCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSplatRadianceCacheIntoAtlasCS::FParameters>();
		PassParameters->TracingParameters = TracingParameters;
		SetupLumenDiffuseTracingParametersForProbe(View, PassParameters->IndirectTracingParameters, -1.0f);
		PassParameters->RWRadianceProbeAtlasTexture = RadianceProbeAtlasTextureUAV;
		PassParameters->RWDepthProbeAtlasTexture = DepthProbeTextureUAV;
		PassParameters->TraceRadianceTexture = TraceRadianceTexture;
		PassParameters->TraceHitTexture = TraceHitTexture;
		PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData, PF_A32B32G32R32F));
		PassParameters->ProbeTraceTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileData, PF_R32G32_UINT));
		PassParameters->ProbeTraceTileAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileAllocator, PF_R32_UINT));
		PassParameters->RadianceCacheParameters = RadianceCacheParameters;
		PassParameters->TraceProbesIndirectArgs = TraceProbesIndirectArgs;
		PassParameters->TempAtlasNumTraceTiles = TempAtlasNumTraceTiles;

		FSplatRadianceCacheIntoAtlasCS::FPermutationDomain PermutationVector;
		auto ComputeShader = View.ShaderMap->GetShader<FSplatRadianceCacheIntoAtlasCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompositeTracesIntoAtlas"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			PassParameters->TraceProbesIndirectArgs,
			0);
	}
#else
	unimplemented();
#endif // RHI_RAYTRACING
}