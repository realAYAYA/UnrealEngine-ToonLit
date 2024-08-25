// Copyright Epic Games, Inc. All Rights Reserved.

#include "ManyLights.h"
#include "ManyLightsInternal.h"
#include "Lumen/LumenTracingUtils.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"
#include "BasePassRendering.h"

static TAutoConsoleVariable<int32> CVarManyLightsScreenTraces(
	TEXT("r.ManyLights.ScreenTraces"),
	1,
	TEXT("Whether to use screen space tracing for shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsScreenTracesMaxIterations(
	TEXT("r.ManyLights.ScreenTraces.MaxIterations"),
	50,
	TEXT("Max iterations for HZB tracing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsScreenTracesMinimumOccupancy(
	TEXT("r.ManyLights.ScreenTraces.MinimumOccupancy"),
	0,
	TEXT("Minimum number of threads still tracing before aborting the trace. Can be used for scalability to abandon traces that have a disproportionate cost."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarManyLightsScreenTraceRelativeDepthThreshold(
	TEXT("r.ManyLights.ScreenTraces.RelativeDepthThickness"),
	0.005f,
	TEXT("Determines depth thickness of objects hit by HZB tracing, as a relative depth threshold."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsWorldSpaceTraces(
	TEXT("r.ManyLights.WorldSpaceTraces"),
	1,
	TEXT("Whether to trace world space shadow rays for samples. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsHardwareRayTracing(
	TEXT("r.ManyLights.HardwareRayTracing"),
	1,
	TEXT("Whether to use hardware ray tracing for shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsHardwareRayTracingInline(
	TEXT("r.ManyLights.HardwareRayTracing.Inline"),
	1,
	TEXT("Uses hardware inline ray tracing for ray traced lighting, when available."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<float> CVarManyLightsHardwareRayTracingBias(
	TEXT("r.ManyLights.HardwareRayTracing.Bias"),
	1.0f,
	TEXT("Constant bias for hardware ray traced shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarManyLightsHardwareRayTracingNormalBias(
	TEXT("r.ManyLights.HardwareRayTracing.NormalBias"),
	0.1f,
	TEXT("Normal bias for hardware ray traced shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsHardwareRayTracingMaxIterations(
	TEXT("r.ManyLights.HardwareRayTracing.MaxIterations"),
	8192,
	TEXT("Limit number of ray tracing traversal iterations on supported platfoms. Improves performance, but may add over-occlusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

// #ml_todo: Separate config cvars from Lumen once we support multiple SBT with same RayTracingPipeline or Global Uniform Buffers in Ray Tracing
static TAutoConsoleVariable<bool> CVarManyLightsHardwareRayTracingAvoidSelfIntersections(
	TEXT("r.ManyLights.HardwareRayTracing.AvoidSelfIntersections"),
	true,
	TEXT("Whether to avoid self-intersections.\n")
	TEXT("Currently shares config with Lumen:\n")
	TEXT("- r.Lumen.HardwareRayTracing.SkipBackFaceHitDistance\n")
	TEXT("- r.Lumen.HardwareRayTracing.SkipTwoSidedHitDistance\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsHairVoxelTraces(
	TEXT("r.ManyLights.HairVoxelTraces"),
	1,
	TEXT("Whether to trace hair voxels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace ManyLights
{
	bool UseHardwareRayTracing()
	{
		#if RHI_RAYTRACING
		{
			return IsRayTracingEnabled() 
				&& CVarManyLightsHardwareRayTracing.GetValueOnRenderThread() != 0;
		}
		#else
		{
			return false;
		}
		#endif
	}

	bool UseInlineHardwareRayTracing()
	{
		#if RHI_RAYTRACING
		{
			return UseHardwareRayTracing()
				&& GRHISupportsInlineRayTracing
				&& CVarManyLightsHardwareRayTracingInline.GetValueOnRenderThread() != 0;
		}
		#else
		{
			return false;
		}
		#endif
	}

	bool IsUsingClosestHZB()
	{
		return IsEnabled() && CVarManyLightsScreenTraces.GetValueOnRenderThread() != 0;
	}

	bool IsUsingGlobalSDF()
	{
		return IsEnabled() && CVarManyLightsWorldSpaceTraces.GetValueOnRenderThread() != 0 && !UseHardwareRayTracing();
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FHairVoxelTraceParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, VirtualVoxel)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FCompactedTraceParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelAllocator)
	END_SHADER_PARAMETER_STRUCT()

	enum class ECompactedTraceIndirectArgs
	{
		NumTracesDiv64 = 0 * sizeof(FRHIDispatchIndirectParameters),
		NumTracesDiv32 = 1 * sizeof(FRHIDispatchIndirectParameters),
		NumTraces = 2 * sizeof(FRHIDispatchIndirectParameters),
		MAX = 3
	};

	FCompactedTraceParameters CompactManyLightsTraces(
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FIntPoint SampleBufferSize,
		FRDGTextureRef LightSamples,
		const FManyLightsParameters& ManyLightsParameters);
};

class FCompactLightSampleTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompactLightSampleTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FCompactLightSampleTracesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompactedTraceTexelData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompactedTraceTexelAllocator)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 16;
	}

	class FWaveOps : SHADER_PERMUTATION_BOOL("WAVE_OPS");
	using FPermutationDomain = TShaderPermutationDomain<FWaveOps>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompactLightSampleTracesCS, "/Engine/Private/ManyLights/ManyLightsRayTracing.usf", "CompactLightSampleTracesCS", SF_Compute);

class FInitCompactedTraceTexelIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompactedTraceTexelIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompactedTraceTexelIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompactedTraceTexelAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitCompactedTraceTexelIndirectArgsCS, "/Engine/Private/ManyLights/ManyLightsRayTracing.usf", "InitCompactedTraceTexelIndirectArgsCS", SF_Compute);

#if RHI_RAYTRACING

class FHardwareRayTraceLightSamples : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FHardwareRayTraceLightSamples, Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(ManyLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(ManyLights::FHairVoxelTraceParameters, HairVoxelTraceParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, LightSampleRayDistance)
		SHADER_PARAMETER(float, RayTracingBias)
		SHADER_PARAMETER(float, RayTracingNormalBias)
		// Ray Tracing
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_SRV(StructuredBuffer, RayTracingSceneMetadata)
		// Inline Ray Tracing
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<Lumen::FHitGroupRootConstants>, HitGroupData)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	class FAvoidSelfIntersections : SHADER_PERMUTATION_BOOL("AVOID_SELF_INTERSECTIONS");
	class FHairVoxelTraces : SHADER_PERMUTATION_BOOL("HAIR_VOXEL_TRACES");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FAvoidSelfIntersections, FHairVoxelTraces, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType)
	{
		return ManyLights::ShouldCompileShaders(Parameters)  
			&& FLumenHardwareRayTracingShaderBase::ShouldCompilePermutation(Parameters, ShaderDispatchType);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback, OutEnvironment);
		ManyLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FHardwareRayTraceLightSamples)

IMPLEMENT_GLOBAL_SHADER(FHardwareRayTraceLightSamplesCS, "/Engine/Private/ManyLights/ManyLightsHardwareRayTracing.usf", "HardwareRayTraceLightSamplesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FHardwareRayTraceLightSamplesRGS, "/Engine/Private/ManyLights/ManyLightsHardwareRayTracing.usf", "HardwareRayTraceLightSamplesRGS", SF_RayGen);

#endif // RHI_RAYTRACING

class FSoftwareRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoftwareRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FSoftwareRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(ManyLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(ManyLights::FHairVoxelTraceParameters, HairVoxelTraceParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, LightSampleRayDistance)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FHairVoxelTraces : SHADER_PERMUTATION_BOOL("HAIR_VOXEL_TRACES");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FHairVoxelTraces, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ManyLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());

		// GPU Scene definitions
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSoftwareRayTraceLightSamplesCS, "/Engine/Private/ManyLights/ManyLightsRayTracing.usf", "SoftwareRayTraceLightSamplesCS", SF_Compute);

class FScreenSpaceRayTraceLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenSpaceRayTraceLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenSpaceRayTraceLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(ManyLights::FCompactedTraceParameters, CompactedTraceParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWLightSampleRayDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHZBScreenTraceParameters, HZBScreenTraceParameters)
		SHADER_PARAMETER(float, MaxHierarchicalScreenTraceIterations)
		SHADER_PARAMETER(float, RelativeDepthThickness)
		SHADER_PARAMETER(float, HistoryDepthTestRelativeThickness)
		SHADER_PARAMETER(uint32, MinimumTracingThreadOccupancy)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 64;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ManyLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenSpaceRayTraceLightSamplesCS, "/Engine/Private/ManyLights/ManyLightsRayTracing.usf", "ScreenSpaceRayTraceLightSamplesCS", SF_Compute);

#if RHI_RAYTRACING
void FDeferredShadingSceneRenderer::PrepareManyLightsLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	using namespace ManyLights;

	if (ManyLights::IsEnabled() && ManyLights::UseHardwareRayTracing())
	{
		for (int32 HairVoxelTraces = 0; HairVoxelTraces < 2; ++HairVoxelTraces)
		{
			FHardwareRayTraceLightSamplesRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FAvoidSelfIntersections>(CVarManyLightsHardwareRayTracingAvoidSelfIntersections.GetValueOnRenderThread());
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FHairVoxelTraces>(HairVoxelTraces != 0);
			PermutationVector.Set<FHardwareRayTraceLightSamplesRGS::FDebugMode>(ManyLights::GetDebugMode() != 0);
			TShaderRef<FHardwareRayTraceLightSamplesRGS> RayGenerationShader = View.ShaderMap->GetShader<FHardwareRayTraceLightSamplesRGS>(PermutationVector);
			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

namespace ManyLights
{
	void SetHardwareRayTracingPassParameters(
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		const FCompactedTraceParameters& CompactedTraceParameters,
		const FManyLightsParameters& ManyLightsParameters,
		const FHairVoxelTraceParameters& HairVoxelTraceParameters,
		FRDGTextureRef LightSamples,
		FRDGTextureRef LightSampleRayDistance,
		FHardwareRayTraceLightSamples::FParameters* PassParameters);
};

void ManyLights::SetHardwareRayTracingPassParameters(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const ManyLights::FCompactedTraceParameters& CompactedTraceParameters,
	const FManyLightsParameters& ManyLightsParameters,
	const FHairVoxelTraceParameters& HairVoxelTraceParameters,
	FRDGTextureRef LightSamples,
	FRDGTextureRef LightSampleRayDistance,
	FHardwareRayTraceLightSamples::FParameters* PassParameters)
{
	PassParameters->CompactedTraceParameters = CompactedTraceParameters;
	PassParameters->ManyLightsParameters = ManyLightsParameters;
	PassParameters->HairVoxelTraceParameters = HairVoxelTraceParameters;
	PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
	PassParameters->LightSampleRayDistance = LightSampleRayDistance;
	PassParameters->RayTracingBias = CVarManyLightsHardwareRayTracingBias.GetValueOnRenderThread();
	PassParameters->RayTracingNormalBias = CVarManyLightsHardwareRayTracingNormalBias.GetValueOnRenderThread();

	checkf(View.HasRayTracingScene(), TEXT("TLAS does not exist. Verify that the current pass is represented in Lumen::AnyLumenHardwareRayTracingPassEnabled()."));
	PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
	PassParameters->MaxTraversalIterations = FMath::Max(CVarManyLightsHardwareRayTracingMaxIterations.GetValueOnRenderThread(), 1);

	// Inline
	PassParameters->HitGroupData = View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer ? GraphBuilder.CreateSRV(View.GetPrimaryView()->LumenHardwareRayTracingHitDataBuffer) : nullptr;
	PassParameters->LumenHardwareRayTracingUniformBuffer = View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer ? View.GetPrimaryView()->LumenHardwareRayTracingUniformBuffer : nullptr;
	checkf(View.RayTracingSceneInitTask == nullptr, TEXT("RayTracingSceneInitTask must be completed before creating SRV for RayTracingSceneMetadata."));
	PassParameters->RayTracingSceneMetadata = View.GetRayTracingSceneChecked()->GetOrCreateMetadataBufferSRV(GraphBuilder.RHICmdList);
}
#endif

ManyLights::FCompactedTraceParameters ManyLights::CompactManyLightsTraces(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FIntPoint SampleBufferSize,
	FRDGTextureRef LightSamples,
	const FManyLightsParameters& ManyLightsParameters)
{
	FRDGBufferRef CompactedTraceTexelData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SampleBufferSize.X * SampleBufferSize.Y),
		TEXT("ManyLightsParameters.CompactedTraceTexelData"));

	FRDGBufferRef CompactedTraceTexelAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("ManyLightsParameters.CompactedTraceTexelAllocator"));

	FRDGBufferRef CompactedTraceTexelIndirectArgs = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)ECompactedTraceIndirectArgs::MAX),
		TEXT("ManyLights.CompactedTraceTexelIndirectArgs"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedTraceTexelAllocator), 0);

	// Compact light sample traces before tracing
	{
		FCompactLightSampleTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompactLightSampleTracesCS::FParameters>();
		PassParameters->RWCompactedTraceTexelData = GraphBuilder.CreateUAV(CompactedTraceTexelData);
		PassParameters->RWCompactedTraceTexelAllocator = GraphBuilder.CreateUAV(CompactedTraceTexelAllocator);
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->LightSamples = LightSamples;

		const bool bWaveOps = ManyLights::UseWaveOps(View.GetShaderPlatform())
			&& GRHIMinimumWaveSize <= 32
			&& GRHIMaximumWaveSize >= 32;

		FCompactLightSampleTracesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCompactLightSampleTracesCS::FWaveOps>(bWaveOps);
		auto ComputeShader = View.ShaderMap->GetShader<FCompactLightSampleTracesCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ManyLightsParameters.SampleViewSize, FCompactLightSampleTracesCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactLightSampleTraces"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	// Setup indirect args for tracing
	{
		FInitCompactedTraceTexelIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompactedTraceTexelIndirectArgsCS::FParameters>();
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(CompactedTraceTexelIndirectArgs);
		PassParameters->CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator);

		auto ComputeShader = View.ShaderMap->GetShader<FInitCompactedTraceTexelIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitCompactedTraceTexelIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FCompactedTraceParameters Parameters;
	Parameters.CompactedTraceTexelAllocator = GraphBuilder.CreateSRV(CompactedTraceTexelAllocator);
	Parameters.CompactedTraceTexelData = GraphBuilder.CreateSRV(CompactedTraceTexelData);
	Parameters.IndirectArgs = CompactedTraceTexelIndirectArgs;
	return Parameters;
}

/**
 * Ray trace light samples using a variety of tracing methods depending on the feature configuration.
 */
void ManyLights::RayTraceLightSamples(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FIntPoint SampleBufferSize,
	FRDGTextureRef LightSamples,
	FRDGTextureRef LightSampleRayDistance,
	const FManyLightsParameters& ManyLightsParameters)
{
	const bool bDebug = ManyLights::GetDebugMode() != 0;

	if (CVarManyLightsScreenTraces.GetValueOnRenderThread() != 0)
	{
		FCompactedTraceParameters CompactedTraceParameters = ManyLights::CompactManyLightsTraces(
			View,
			GraphBuilder,
			SampleBufferSize,
			LightSamples,
			ManyLightsParameters);

		FScreenSpaceRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenSpaceRayTraceLightSamplesCS::FParameters>();
		PassParameters->CompactedTraceParameters = CompactedTraceParameters;
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
		PassParameters->RWLightSampleRayDistance = GraphBuilder.CreateUAV(LightSampleRayDistance);
		PassParameters->HZBScreenTraceParameters = SetupHZBScreenTraceParameters(GraphBuilder, View, SceneTextures, /*bBindLumenHistory*/ false);
		PassParameters->MaxHierarchicalScreenTraceIterations = CVarManyLightsScreenTracesMaxIterations.GetValueOnRenderThread();
		PassParameters->RelativeDepthThickness = CVarManyLightsScreenTraceRelativeDepthThreshold.GetValueOnRenderThread() * View.ViewMatrices.GetPerProjectionDepthThicknessScale();
		PassParameters->HistoryDepthTestRelativeThickness = 0.0f;
		PassParameters->MinimumTracingThreadOccupancy = CVarManyLightsScreenTracesMinimumOccupancy.GetValueOnRenderThread();

		FScreenSpaceRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FScreenSpaceRayTraceLightSamplesCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FScreenSpaceRayTraceLightSamplesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ScreenSpaceRayTraceLightSamples"),
			ComputeShader,
			PassParameters,
			CompactedTraceParameters.IndirectArgs,
			(int32)ManyLights::ECompactedTraceIndirectArgs::NumTracesDiv64);
	}
	else
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LightSampleRayDistance), 0.0f);
	}

	const bool bHairVoxelTraces = HairStrands::HasViewHairStrandsData(View)
		&& HairStrands::HasViewHairStrandsVoxelData(View)
		&& CVarManyLightsHairVoxelTraces.GetValueOnRenderThread() != 0;

	FHairVoxelTraceParameters HairVoxelTraceParameters;
	if (bHairVoxelTraces)
	{
		HairVoxelTraceParameters.HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
		HairVoxelTraceParameters.VirtualVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
	}

	if (CVarManyLightsWorldSpaceTraces.GetValueOnRenderThread() != 0)
	{
		FCompactedTraceParameters CompactedTraceParameters = ManyLights::CompactManyLightsTraces(
			View,
			GraphBuilder,
			SampleBufferSize,
			LightSamples,
			ManyLightsParameters);

		if (ManyLights::UseHardwareRayTracing())
		{
#if RHI_RAYTRACING
			FHardwareRayTraceLightSamples::FParameters* PassParameters = GraphBuilder.AllocParameters<FHardwareRayTraceLightSamples::FParameters>();
			ManyLights::SetHardwareRayTracingPassParameters(
				View,
				GraphBuilder,
				CompactedTraceParameters,
				ManyLightsParameters,
				HairVoxelTraceParameters,
				LightSamples,
				LightSampleRayDistance,
				PassParameters);

			FHardwareRayTraceLightSamples::FPermutationDomain PermutationVector;
			PermutationVector.Set<FHardwareRayTraceLightSamples::FAvoidSelfIntersections>(CVarManyLightsHardwareRayTracingAvoidSelfIntersections.GetValueOnRenderThread());
			PermutationVector.Set<FHardwareRayTraceLightSamples::FHairVoxelTraces>(bHairVoxelTraces);
			PermutationVector.Set<FHardwareRayTraceLightSamples::FDebugMode>(bDebug);
			if (ManyLights::UseInlineHardwareRayTracing())
			{
				FHardwareRayTraceLightSamplesCS::AddLumenRayTracingDispatchIndirect(
					GraphBuilder,
					RDG_EVENT_NAME("HardwareRayTraceLightSamples Inline"),
					View,
					PermutationVector,
					PassParameters,
					CompactedTraceParameters.IndirectArgs,
					(int32)ManyLights::ECompactedTraceIndirectArgs::NumTracesDiv32,
					ERDGPassFlags::Compute);
			}
			else
			{
				FHardwareRayTraceLightSamplesRGS::AddLumenRayTracingDispatchIndirect(
					GraphBuilder,
					RDG_EVENT_NAME("HardwareRayTraceLightSamples RayGen"),
					View,
					PermutationVector,
					PassParameters,
					PassParameters->CompactedTraceParameters.IndirectArgs,
					(int32)ManyLights::ECompactedTraceIndirectArgs::NumTraces,
					/*bUseMinimalPayload*/ true);
			}
			#endif
		}
		else
		{
			ensure(ManyLights::IsUsingGlobalSDF());

			FSoftwareRayTraceLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSoftwareRayTraceLightSamplesCS::FParameters>();
			PassParameters->CompactedTraceParameters = CompactedTraceParameters;
			PassParameters->ManyLightsParameters = ManyLightsParameters;
			PassParameters->HairVoxelTraceParameters = HairVoxelTraceParameters;
			PassParameters->RWLightSamples = GraphBuilder.CreateUAV(LightSamples);
			PassParameters->LightSampleRayDistance = LightSampleRayDistance;

			FSoftwareRayTraceLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSoftwareRayTraceLightSamplesCS::FHairVoxelTraces>(bHairVoxelTraces);
			PermutationVector.Set<FSoftwareRayTraceLightSamplesCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FSoftwareRayTraceLightSamplesCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("SoftwareRayTraceLightSamples"),
				ComputeShader,
				PassParameters,
				CompactedTraceParameters.IndirectArgs,
				0);
		}
	}
}