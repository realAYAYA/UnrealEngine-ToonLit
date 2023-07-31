// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenHardwareRayTracingCommon.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"

static TAutoConsoleVariable<int32> CVarLumenUseHardwareRayTracing(
	TEXT("r.Lumen.HardwareRayTracing"),
	0,
	TEXT("Uses Hardware Ray Tracing for Lumen features, when available.\n")
	TEXT("Lumen will fall back to Software Ray Tracing otherwise.\n")
	TEXT("Note: Hardware ray tracing has significant scene update costs for\n")
	TEXT("scenes with more than 100k instances."),
	ECVF_RenderThreadSafe
);

// Note: Driven by URendererSettings and must match the enum exposed there
static TAutoConsoleVariable<int32> CVarLumenHardwareRayTracingLightingMode(
	TEXT("r.Lumen.HardwareRayTracing.LightingMode"),
	0,
	TEXT("Determines the lighting mode (Default = 0)\n")
	TEXT("0: interpolate final lighting from the surface cache\n")
	TEXT("1: evaluate material, and interpolate irradiance and indirect irradiance from the surface cache\n")
	TEXT("2: evaluate material and direct lighting, and interpolate indirect irradiance from the surface cache\n")
	TEXT("3: evaluate material, direct lighting, and unshadowed skylighting at the hit point"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarLumenUseHardwareRayTracingInline(
	TEXT("r.Lumen.HardwareRayTracing.Inline"),
	1,
	TEXT("Uses Hardware Inline Ray Tracing for selected Lumen passes, when available.\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingPullbackBias(
	TEXT("r.Lumen.HardwareRayTracing.PullbackBias"),
	8.0,
	TEXT("Determines the pull-back bias when resuming a screen-trace ray (default = 8.0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingFarFieldBias(
	TEXT("r.Lumen.HardwareRayTracing.FarFieldBias"),
	200.0f,
	TEXT("Determines bias for the far field traces. Default = 200"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenHardwareRayTracingMaxIterations(
	TEXT("r.Lumen.HardwareRayTracing.MaxIterations"),
	8192,
	TEXT("Limit number of ray tracing traversal iterations on supported platfoms.\n"
		"Incomplete misses will be treated as hitting a black surface (can cause overocculsion).\n"
		"Incomplete hits will be treated as a hit (can cause leaking)."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenHardwareRayTracingMaxTranslucentSkipCount(
	TEXT("r.Lumen.HardwareRayTracing.MaxTranslucentSkipCount"),
	2,
	TEXT("Determines the maximum number of translucent surfaces skipped during ray traversal (Default = 2)"),
	ECVF_RenderThreadSafe
);

bool Lumen::UseHardwareRayTracing(const FSceneViewFamily& ViewFamily)
{
#if RHI_RAYTRACING
	return IsRayTracingEnabled()
		&& (GRHISupportsRayTracingShaders || GRHISupportsInlineRayTracing)
		&& CVarLumenUseHardwareRayTracing.GetValueOnAnyThread() != 0
		// Ray Tracing does not support split screen yet, but stereo views can be allowed
		&& (ViewFamily.Views.Num() == 1 || (ViewFamily.Views.Num() == 2 && IStereoRendering::IsStereoEyeView(*ViewFamily.Views[0])));
#else
	return false;
#endif
}

int32 Lumen::GetMaxTranslucentSkipCount()
{
	return CVarLumenHardwareRayTracingMaxTranslucentSkipCount.GetValueOnRenderThread();
}

Lumen::EHardwareRayTracingLightingMode Lumen::GetHardwareRayTracingLightingMode(const FViewInfo& View)
{
#if RHI_RAYTRACING
		
	int32 LightingModeInt = CVarLumenHardwareRayTracingLightingMode.GetValueOnRenderThread();

	// Without ray tracing shaders support we can only use Surface Cache mode.
	if (View.FinalPostProcessSettings.LumenRayLightingMode == ELumenRayLightingModeOverride::SurfaceCache || !GRHISupportsRayTracingShaders)
	{
		LightingModeInt = static_cast<int32>(Lumen::EHardwareRayTracingLightingMode::LightingFromSurfaceCache);
	}
	else if (View.FinalPostProcessSettings.LumenRayLightingMode == ELumenRayLightingModeOverride::HitLighting)
	{
		LightingModeInt = static_cast<int32>(Lumen::EHardwareRayTracingLightingMode::EvaluateMaterialAndDirectLighting);
	}

	LightingModeInt = FMath::Clamp<int32>(LightingModeInt, 0, (int32)Lumen::EHardwareRayTracingLightingMode::MAX - 1);
	return static_cast<Lumen::EHardwareRayTracingLightingMode>(LightingModeInt);
#else
	return Lumen::EHardwareRayTracingLightingMode::LightingFromSurfaceCache;
#endif
}

const TCHAR* Lumen::GetRayTracedLightingModeName(Lumen::EHardwareRayTracingLightingMode LightingMode)
{
	switch (LightingMode)
	{
	case Lumen::EHardwareRayTracingLightingMode::LightingFromSurfaceCache:
		return TEXT("LightingFromSurfaceCache");
	case Lumen::EHardwareRayTracingLightingMode::EvaluateMaterial:
		return TEXT("EvaluateMaterial");
	case Lumen::EHardwareRayTracingLightingMode::EvaluateMaterialAndDirectLighting:
		return TEXT("EvaluateMaterialAndDirectLighting");
	case Lumen::EHardwareRayTracingLightingMode::EvaluateMaterialAndDirectLightingAndSkyLighting:
		return TEXT("EvaluateMaterialAndDirectLightingAndSkyLighting");
	default:
		checkf(0, TEXT("Unhandled EHardwareRayTracingLightingMode"));
	}
	return nullptr;
}

bool Lumen::UseHardwareInlineRayTracing(const FSceneViewFamily& ViewFamily)
{
#if RHI_RAYTRACING
	return (Lumen::UseHardwareRayTracing(ViewFamily) && CVarLumenUseHardwareRayTracingInline.GetValueOnRenderThread() != 0 && GRHISupportsInlineRayTracing);
#else
	return false;
#endif
}

float LumenHardwareRayTracing::GetFarFieldBias()
{
	return FMath::Max(CVarLumenHardwareRayTracingFarFieldBias.GetValueOnRenderThread(), 0.0f);
}

uint32 LumenHardwareRayTracing::GetMaxTraversalIterations()
{
	return FMath::Max(CVarLumenHardwareRayTracingMaxIterations.GetValueOnRenderThread(), 1);
}

#if RHI_RAYTRACING

#include "LumenHardwareRayTracingCommon.h"

namespace Lumen
{
	const TCHAR* GetRayTracedNormalModeName(int NormalMode)
	{
		if (NormalMode == 0)
		{
			return TEXT("SDF");
		}

		return TEXT("Geometry");
	}

	float GetHardwareRayTracingPullbackBias()
	{
		return CVarLumenHardwareRayTracingPullbackBias.GetValueOnRenderThread();
	}
}

void SetLumenHardwareRayTracingSharedParameters(
	FRDGBuilder& GraphBuilder,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	const FLumenCardTracingInputs& TracingInputs,
	FLumenHardwareRayTracingShaderBase::FSharedParameters* SharedParameters
)
{
	SharedParameters->SceneTextures = SceneTextures;
	SharedParameters->SceneTexturesStruct = View.GetSceneTextures().UniformBuffer;
	SharedParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);

	//SharedParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	checkf(View.HasRayTracingScene(), TEXT("TLAS does not exist. Verify that the current pass is represented in Lumen::AnyLumenHardwareRayTracingPassEnabled()."));
	SharedParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);

	// Lighting data
	SharedParameters->LightDataPacked = View.RayTracingLightDataUniformBuffer;

	// Inline
	SharedParameters->HitGroupData = View.LumenHardwareRayTracingHitDataBufferSRV;
	SharedParameters->RayTracingSceneMetadata = View.GetRayTracingSceneChecked()->GetMetadataBufferSRV();

	// Use surface cache, instead
	GetLumenCardTracingParameters(GraphBuilder, View, TracingInputs, SharedParameters->TracingParameters);
}

class FLumenHWRTCompactRaysIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHWRTCompactRaysIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenHWRTCompactRaysIndirectArgsCS, FGlobalShader)

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
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenHWRTCompactRaysIndirectArgsCS, "/Engine/Private/Lumen/LumenHardwareRayTracingPipeline.usf", "FLumenHWRTCompactRaysIndirectArgsCS", SF_Compute);

class FLumenHWRTCompactRaysCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHWRTCompactRaysCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenHWRTCompactRaysCS, FGlobalShader);

	class FCompactModeDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_COMPACT_MODE", LumenHWRTPipeline::ECompactMode);
	using FPermutationDomain = TShaderPermutationDomain<FCompactModeDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenHWRTPipeline::FTraceDataPacked>, TraceDataPacked)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWRayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenHWRTPipeline::FTraceDataPacked>, RWTraceDataPacked)

		// Indirect args
		RDG_BUFFER_ACCESS(CompactRaysIndirectArgs, ERHIAccess::IndirectArgs)
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

IMPLEMENT_GLOBAL_SHADER(FLumenHWRTCompactRaysCS, "/Engine/Private/Lumen/LumenHardwareRayTracingPipeline.usf", "FLumenHWRTCompactRaysCS", SF_Compute);


class FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS, FGlobalShader)

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
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 16; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS, "/Engine/Private/Lumen/LumenHardwareRayTracingPipeline.usf", "FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS", SF_Compute);

class FLumenHWRTBucketRaysByMaterialIdCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHWRTBucketRaysByMaterialIdCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenHWRTBucketRaysByMaterialIdCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RayAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<LumenHWRTPipeline::FTraceDataPacked>, TraceDataPacked)

		SHADER_PARAMETER(int, MaxRayAllocationCount)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<LumenHWRTPipeline::FTraceDataPacked>, RWTraceDataPacked)

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
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 16; }
};

IMPLEMENT_GLOBAL_SHADER(FLumenHWRTBucketRaysByMaterialIdCS, "/Engine/Private/Lumen/LumenHardwareRayTracingPipeline.usf", "FLumenHWRTBucketRaysByMaterialIdCS", SF_Compute);

void LumenHWRTCompactRays(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	int32 RayCount,
	LumenHWRTPipeline::ECompactMode CompactMode,
	const FRDGBufferRef& RayAllocatorBuffer,
	const FRDGBufferRef& TraceDataPackedBuffer,
	FRDGBufferRef& OutputRayAllocatorBuffer,
	FRDGBufferRef& OutputTraceDataPackedBuffer
)
{
	FRDGBufferRef CompactRaysIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.HWRT.CompactTracingIndirectArgs"));
	{
		FLumenHWRTCompactRaysIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenHWRTCompactRaysIndirectArgsCS::FParameters>();
		{
			PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
			PassParameters->RWCompactRaysIndirectArgs = GraphBuilder.CreateUAV(CompactRaysIndirectArgsBuffer, PF_R32_UINT);
		}

		TShaderRef<FLumenHWRTCompactRaysIndirectArgsCS> ComputeShader = View.ShaderMap->GetShader<FLumenHWRTCompactRaysIndirectArgsCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactRaysIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	{
		FLumenHWRTCompactRaysCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenHWRTCompactRaysCS::FParameters>();
		{
			// Input
			PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
			PassParameters->TraceDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(TraceDataPackedBuffer));

			// Output
			PassParameters->RWRayAllocator = GraphBuilder.CreateUAV(OutputRayAllocatorBuffer, PF_R32_UINT);
			PassParameters->RWTraceDataPacked = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputTraceDataPackedBuffer));

			// Indirect args
			PassParameters->CompactRaysIndirectArgs = CompactRaysIndirectArgsBuffer;
		}

		FLumenHWRTCompactRaysCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenHWRTCompactRaysCS::FCompactModeDim>(CompactMode);
		TShaderRef<FLumenHWRTCompactRaysCS> ComputeShader = View.ShaderMap->GetShader<FLumenHWRTCompactRaysCS>(PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactRays"),
			ComputeShader,
			PassParameters,
			PassParameters->CompactRaysIndirectArgs,
			0);
	}
}

void LumenHWRTBucketRaysByMaterialID(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	int32 RayCount,
	FRDGBufferRef& RayAllocatorBuffer,
	FRDGBufferRef& TraceDataPackedBuffer
)
{
	FRDGBufferRef BucketRaysByMaterialIdIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.HWRT.BucketRaysByMaterialIdIndirectArgsBuffer"));
	{
		FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS::FParameters>();
		{
			PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
			PassParameters->RWBucketRaysByMaterialIdIndirectArgs = GraphBuilder.CreateUAV(BucketRaysByMaterialIdIndirectArgsBuffer, PF_R32_UINT);
		}

		TShaderRef<FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS> ComputeShader = View.ShaderMap->GetShader<FLumenHWRTBucketRaysByMaterialIdIndirectArgsCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BucketRaysByMaterialIdIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FRDGBufferRef BucketedTexelTraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32) * 2, RayCount), TEXT("Lumen.HWRT.BucketedTexelTraceDataPackedBuffer"));
	FRDGBufferRef BucketedTraceDataPackedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(LumenHWRTPipeline::FTraceDataPacked), RayCount), TEXT("Lumen.HWRT.BucketedTraceDataPacked"));
	{
		FLumenHWRTBucketRaysByMaterialIdCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenHWRTBucketRaysByMaterialIdCS::FParameters>();
		{
			// Input
			PassParameters->RayAllocator = GraphBuilder.CreateSRV(RayAllocatorBuffer, PF_R32_UINT);
			PassParameters->TraceDataPacked = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(TraceDataPackedBuffer));
			PassParameters->MaxRayAllocationCount = RayCount;

			// Output
			PassParameters->RWTraceDataPacked = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(BucketedTraceDataPackedBuffer));

			// Indirect args
			PassParameters->BucketRaysByMaterialIdIndirectArgs = BucketRaysByMaterialIdIndirectArgsBuffer;
		}

		TShaderRef<FLumenHWRTBucketRaysByMaterialIdCS> ComputeShader = View.ShaderMap->GetShader<FLumenHWRTBucketRaysByMaterialIdCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BucketRaysByMaterialId"),
			ComputeShader,
			PassParameters,
			PassParameters->BucketRaysByMaterialIdIndirectArgs,
			0);

		TraceDataPackedBuffer = BucketedTraceDataPackedBuffer;
	}
}

#endif // RHI_RAYTRACING