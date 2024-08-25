// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenReSTIRGather.h"
#include "LumenScreenProbeGather.h"
#include "BasePassRendering.h"
#include "Lumen/LumenRadianceCache.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "ScreenSpaceDenoise.h"
#include "HairStrands/HairStrandsEnvironment.h"
#include "ShaderPrint.h"
#include "Substrate/Substrate.h"
#include "LumenReflections.h"
#include "DepthCopy.h"

#if RHI_RAYTRACING
#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"
#endif

static TAutoConsoleVariable<int32> CVarLumenReSTIRGather(
	TEXT("r.Lumen.ReSTIRGather"),
	0,
	TEXT("Whether to use the prototype ReSTIR Final Gather.  Disabled by default, as quality is currently much lower than LumenScreenProbeGather, and fewer features supported."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRDownsampleFactor = 2;
FAutoConsoleVariableRef CVarLumenReSTIRDownsampleFactor(
	TEXT("r.Lumen.ReSTIRGather.DownsampleFactor"),
	GLumenReSTIRDownsampleFactor,
	TEXT("Downsample factor from the main viewport to trace rays, create and resample reservoirs at.  This is the main performance control for the tracing / resampling part of the algorithm."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRGatherFixedJitterIndex = -1;
FAutoConsoleVariableRef CVarLumenReSTIRGatherFixedJitterIndex(
	TEXT("r.Lumen.ReSTIRGather.FixedJitterIndex"),
	GLumenReSTIRGatherFixedJitterIndex,
	TEXT("When 0 or larger, overrides the frame index with a constant for debugging"),
	ECVF_RenderThreadSafe
);

int32 GLumenReSTIRTemporalResampling = 1;
FAutoConsoleVariableRef CVarLumenReSTIRTemporalResampling(
	TEXT("r.Lumen.ReSTIRGather.TemporalResampling"),
	GLumenReSTIRTemporalResampling,
	TEXT("Whether to do a temporal resampling pass on the reservoirs"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenReSTIRValidateTemporalReservoirsEveryNFrames = 0;
FAutoConsoleVariableRef CVarLumenReSTIRTemporalResamplingValidate(
	TEXT("r.Lumen.ReSTIRGather.TemporalResampling.ValidateEveryNFrames"),
	GLumenReSTIRValidateTemporalReservoirsEveryNFrames,
	TEXT("Validate temporal reservoirs by re-tracing their rays and comparing hit positions and radiance every N frames.  Used to reduce lag in lighting changes, but introduces noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRTemporalResamplingHistoryDistanceThreshold = .005f;
FAutoConsoleVariableRef CVarReSTIRTemporalResamplingHistoryDistanceThreshold(
	TEXT("r.Lumen.ReSTIRGather.TemporalResampling.DistanceThreshold"),
	GLumenReSTIRTemporalResamplingHistoryDistanceThreshold,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRTemporalResamplingResetHistory = 0;
FAutoConsoleVariableRef CVarLumenReSTIRTemporalResamplingResetHistory(
	TEXT("r.Lumen.ReSTIRGather.TemporalResampling.ResetHistory"),
	GLumenReSTIRTemporalResamplingResetHistory,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRSpatialResampling = 1;
FAutoConsoleVariableRef CVarLumenReSTIRSpatialResampling(
	TEXT("r.Lumen.ReSTIRGather.SpatialResampling"),
	GLumenReSTIRSpatialResampling,
	TEXT("Whether to use spatial resampling passes on the reservoirs"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRNumSpatialResamplingPasses = 2;
FAutoConsoleVariableRef CVarLumenNumSpatialResamplingPasses(
	TEXT("r.Lumen.ReSTIRGather.SpatialResampling.NumPasses"),
	GLumenReSTIRNumSpatialResamplingPasses,
	TEXT("The number of spatial resampling passes to do"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRNumSpatialSamples = 4;
FAutoConsoleVariableRef CVarLumenReSTIRNumSpatialSamples(
	TEXT("r.Lumen.ReSTIRGather.SpatialResampling.NumSamples"),
	GLumenReSTIRNumSpatialSamples,
	TEXT("The number of spatial samples for each resampling pass."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRSpatialResamplingKernelRadius = .05f;
FAutoConsoleVariableRef CVarLumenReSTIRSpatialResamplingKernelRadius(
	TEXT("r.Lumen.ReSTIRGather.SpatialResampling.KernelRadius"),
	GLumenReSTIRSpatialResamplingKernelRadius,
	TEXT("Radius of the spatial resampling kernel as a fraction of the screen."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRResamplingAngleThreshold = 25.0f;
FAutoConsoleVariableRef CVarLumenReSTIRResamplingAngleThreshold(
	TEXT("r.Lumen.ReSTIRGather.ResamplingAngleThreshold"),
	GLumenReSTIRResamplingAngleThreshold,
	TEXT("Largest angle between two reservoirs that will be allowed during reservoir resampling, in degrees"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRResamplingDepthErrorThreshold = .01f;
FAutoConsoleVariableRef CVarLumenReSTIRSpatialResamplingDepthErrorThreshold(
	TEXT("r.Lumen.ReSTIRGather.ResamplingDepthErrorThreshold"),
	GLumenReSTIRResamplingDepthErrorThreshold,
	TEXT("Largest depth error between two reservoirs that will be allowed during reservoir resampling"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRSpatialResamplingOcclusionScreenTraceDistance = .2f;
FAutoConsoleVariableRef CVarLumenReSTIRSpatialResamplingOcclusionScreenTraceDistance(
	TEXT("r.Lumen.ReSTIRGather.SpatialResampling.OcclusionScreenTraceDistance"),
	GLumenReSTIRSpatialResamplingOcclusionScreenTraceDistance,
	TEXT("Length of occlusion screen traces used to validate the neighbor's reservoir hit position before reuse, to reduce leaking.  As a fraction of the screen size."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRMaxRayIntensity = 100;
FAutoConsoleVariableRef CVarLumenReSTIRMaxRayIntensity(
	TEXT("r.Lumen.ReSTIRGather.MaxRayIntensity"),
	GLumenReSTIRMaxRayIntensity,
	TEXT("Clamps the maximum ray lighting intensity (with PreExposure) to reduce fireflies."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRUpsampleMethod = 1;
FAutoConsoleVariableRef CVarLumenReSTIRUpsampleMethod(
	TEXT("r.Lumen.ReSTIRGather.Upsample.Method"),
	GLumenReSTIRUpsampleMethod,
	TEXT("Upsample method, when calculating full resolution irradiance from downsampled reservoir radiance\n")
	TEXT("0 - Jittered bilinear sample, when jittered position lies in the same plane, and fallback to unjittered when all samples fail\n")
	TEXT("1 - Spiral sample pattern\n")
	TEXT("2 - Passthrough for debugging.  Also effectively disables the bilateral filter as variance is not computed."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRUpsampleKernelSize = 3.0f;
FAutoConsoleVariableRef CVarLumenReSTIRUpsampleKernelSize(
	TEXT("r.Lumen.ReSTIRGather.Upsample.KernelSize"),
	GLumenReSTIRUpsampleKernelSize,
	TEXT("Upsample kernel size, in reservoir texels"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRUpsampleNumSamples = 16;
FAutoConsoleVariableRef CVarLumenReSTIRNumSamples(
	TEXT("r.Lumen.ReSTIRGather.Upsample.NumSamples"),
	GLumenReSTIRUpsampleNumSamples,
	TEXT("Number of reservoir samples to take while upsampling. Only used when r.Lumen.ReSTIRGather.Upsample.Method is set to Spiral Pattern."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRGatherTemporalFilter = 1;
FAutoConsoleVariableRef CVarLumenReSTIRGatherTemporalFilter(
	TEXT("r.Lumen.ReSTIRGather.Temporal"),
	GLumenReSTIRGatherTemporalFilter,
	TEXT("Whether to use a temporal filter"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRGatherClearHistoryEveryFrame = 0;
FAutoConsoleVariableRef CVarLumenReSTIRGatherClearHistoryEveryFrame(
	TEXT("r.Lumen.ReSTIRGather.Temporal.ClearHistoryEveryFrame"),
	GLumenReSTIRGatherClearHistoryEveryFrame,
	TEXT("Whether to clear the history every frame for debugging"),
	ECVF_RenderThreadSafe
);

float GLumenReSTIRGatherHistoryDistanceThreshold = .005f;
FAutoConsoleVariableRef CVarLumenReSTIRGatherHistoryDistanceThreshold(
	TEXT("r.Lumen.ReSTIRGather.Temporal.DistanceThreshold"),
	GLumenReSTIRGatherHistoryDistanceThreshold,
	TEXT("Relative distance threshold needed to discard last frame's lighting results.  Lower values reduce ghosting from characters when near a wall but increase flickering artifacts."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRGatherTemporalMaxFramesAccumulated = 10.0f;
FAutoConsoleVariableRef CVarLumenReSTIRGatherTemporalMaxFramesAccumulated(
	TEXT("r.Lumen.ReSTIRGather.Temporal.MaxFramesAccumulated"),
	GLumenReSTIRGatherTemporalMaxFramesAccumulated,
	TEXT("Lower values cause the temporal filter to propagate lighting changes faster, but also increase flickering from noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRGatherBilateralFilter = 1;
FAutoConsoleVariableRef CVarLumenReSTIRGatherBilateralFilter(
	TEXT("r.Lumen.ReSTIRGather.BilateralFilter"),
	GLumenReSTIRGatherBilateralFilter,
	TEXT("Whether to do a bilateral filter as a last step in denoising Lumen ReSTIRGathers."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRGatherBilateralFilterSpatialKernelRadius = .002f;
FAutoConsoleVariableRef CVarLumenReSTIRGatherBilateralFilterSpatialKernelRadius(
	TEXT("r.Lumen.ReSTIRGather.BilateralFilter.SpatialKernelRadius"),
	GLumenReSTIRGatherBilateralFilterSpatialKernelRadius,
	TEXT("Spatial kernel radius, as a fraction of the viewport size"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRGatherBilateralFilterNumSamples = 8;
FAutoConsoleVariableRef CVarLumenReSTIRGatherBilateralFilterNumSamples(
	TEXT("r.Lumen.ReSTIRGather.BilateralFilter.NumSamples"),
	GLumenReSTIRGatherBilateralFilterNumSamples,
	TEXT("Number of bilateral filter samples."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRGatherBilateralFilterDepthWeightScale = 10000.0f;
FAutoConsoleVariableRef CVarLumenReSTIRGatherBilateralFilterDepthWeightScale(
	TEXT("r.Lumen.ReSTIRGather.BilateralFilter.DepthWeightScale"),
	GLumenReSTIRGatherBilateralFilterDepthWeightScale,
	TEXT("Scales the depth weight of the bilateral filter"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRGatherBilateralFilterNormalAngleThresholdScale = .2f;
FAutoConsoleVariableRef CVarLumenReSTIRGatherBilateralFilterNormalAngleThresholdScale(
	TEXT("r.Lumen.ReSTIRGather.BilateralFilter.NormalAngleThresholdScale"),
	GLumenReSTIRGatherBilateralFilterNormalAngleThresholdScale,
	TEXT("Scales the Normal angle threshold of the bilateral filter"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRGatherBilateralFilterStrongBlurVarianceThreshold = .5f;
FAutoConsoleVariableRef CVarLumenReSTIRGatherBilateralFilterStrongBlurVarianceThreshold(
	TEXT("r.Lumen.ReSTIRGather.BilateralFilter.StrongBlurVarianceThreshold"),
	GLumenReSTIRGatherBilateralFilterStrongBlurVarianceThreshold,
	TEXT("Pixels whose variance from the spatial resolve filter are higher than this value get a stronger bilateral blur."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenReSTIRGatherShortRangeAmbientOcclusion = 1;
FAutoConsoleVariableRef GVarLumenReSTIRGatherShortRangeAO(
	TEXT("r.Lumen.ReSTIRGather.ShortRangeAO"),
	GLumenReSTIRGatherShortRangeAmbientOcclusion,
	TEXT("Whether to compute a short range, full resolution AO to add high frequency occlusion (contact shadows) which ReSTIR GI lacks due to spatial filtering."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenReSTIRGatherShortRangeAOMaxScreenTraceFraction = .02f;
FAutoConsoleVariableRef CVarLumenReSTIRGatherShortRangeAOMaxScreenTraceFraction(
	TEXT("r.Lumen.ReSTIRGather.ShortRangeAO.MaxScreenTraceFraction"),
	GLumenReSTIRGatherShortRangeAOMaxScreenTraceFraction,
	TEXT("Short range AO tracing distance, as a fraction of the screen size."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace LumenReSTIRGather 
{
	int32 GetScreenDownsampleFactor(const FViewInfo& View)
	{
		return FMath::Clamp(GLumenReSTIRDownsampleFactor, 1, 8);
	}
}

bool DoesPlatformSupportLumenReSTIRGather(EShaderPlatform Platform)
{
	// Prototype feature, don't want to spend time fixing other platforms until it matters
	return Platform == SP_PCD3D_SM6 && DoesPlatformSupportLumenGI(Platform);
}

namespace Lumen
{
	bool UseReSTIRGather(const FSceneViewFamily& ViewFamily, EShaderPlatform ShaderPlatform)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& (CVarLumenReSTIRGather.GetValueOnAnyThread() != 0)
			&& DoesPlatformSupportLumenReSTIRGather(ShaderPlatform);
#else
		return false;
#endif
	}
} // namespace Lumen

#if RHI_RAYTRACING

class FLumenValidateReservoirs : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenValidateReservoirs, Lumen::ERayTracingShaderDispatchSize::DispatchSize2D)

	class FHitLighting : SHADER_PERMUTATION_BOOL("HIT_LIGHTING");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FHitLighting>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FReSTIRParameters, ReSTIRParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWTemporalReservoirTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWTemporalReservoirWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TemporalReservoirRayDirection)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TemporalReservoirTraceHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TemporalReservoirTraceHitNormal)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FIntPoint, HistoryReservoirViewSize)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DownsampledDepthHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DownsampledNormalHistory)
		SHADER_PARAMETER(float, PullbackBias)
		SHADER_PARAMETER(int32, ApplySkyLight)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, MaxRayIntensity)
	END_SHADER_PARAMETER_STRUCT()


	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::HighResPages, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		FPermutationDomain PermutationVector(PermutationId);
		if (PermutationVector.Get<FHitLighting>())
		{
			return ERayTracingPayloadType::RayTracingMaterial;
		}
		else
		{
			return ERayTracingPayloadType::LumenMinimal;
		}
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenValidateReservoirs)

IMPLEMENT_GLOBAL_SHADER(FLumenValidateReservoirsRGS, "/Engine/Private/Lumen/LumenReSTIRGather.usf", "LumenValidateReservoirsRGS", SF_RayGen);

class FLumenInitialSampling : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenInitialSampling, Lumen::ERayTracingShaderDispatchSize::DispatchSize2D)

	class FHitLighting : SHADER_PERMUTATION_BOOL("HIT_LIGHTING");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FHitLighting>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FReSTIRParameters, ReSTIRParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledWorldNormal)
		SHADER_PARAMETER(float, PullbackBias)
		SHADER_PARAMETER(int32, ApplySkyLight)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, MaxRayIntensity)
	END_SHADER_PARAMETER_STRUCT()


	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::HighResPages, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		FPermutationDomain PermutationVector(PermutationId);
		if (PermutationVector.Get<FHitLighting>())
		{
			return ERayTracingPayloadType::RayTracingMaterial;
		}
		else
		{
			return ERayTracingPayloadType::LumenMinimal;
		}
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenInitialSampling)

IMPLEMENT_GLOBAL_SHADER(FLumenInitialSamplingRGS, "/Engine/Private/Lumen/LumenReSTIRGather.usf", "LumenInitialSamplingRGS", SF_RayGen);

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingReSTIRLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseReSTIRGather(*View.Family, ShaderPlatform))
	{
		{
			FLumenValidateReservoirsRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenValidateReservoirsRGS::FHitLighting>(false);
			TShaderRef<FLumenValidateReservoirsRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenValidateReservoirsRGS>(PermutationVector);
			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}

		{
			FLumenInitialSamplingRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenInitialSamplingRGS::FHitLighting>(false);
			TShaderRef<FLumenInitialSamplingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenInitialSamplingRGS>(PermutationVector);
			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
	}
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingReSTIR(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseReSTIRGather(*View.Family, ShaderPlatform))
	{
		const bool bLumenGIEnabled = GetViewPipelineState(View).DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen;
		const bool bUseHitLighting = LumenReflections::UseHitLighting(View, bLumenGIEnabled);

		if (bUseHitLighting)
		{
			{
				FLumenValidateReservoirsRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenValidateReservoirsRGS::FHitLighting>(true);
				TShaderRef<FLumenValidateReservoirsRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenValidateReservoirsRGS>(PermutationVector);
				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}

			{
				FLumenInitialSamplingRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FLumenInitialSamplingRGS::FHitLighting>(true);
				TShaderRef<FLumenInitialSamplingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenInitialSamplingRGS>(PermutationVector);
				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}
		}
	}
}

#endif

class FTemporalResamplingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTemporalResamplingCS)
	SHADER_USE_PARAMETER_STRUCT(FTemporalResamplingCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWTemporalReservoirRayDirection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWTemporalReservoirTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWTemporalReservoirTraceHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWTemporalReservoirTraceHitNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWTemporalReservoirWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TemporalReservoirRayDirection)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TemporalReservoirTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TemporalReservoirTraceHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TemporalReservoirTraceHitNormal)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TemporalReservoirWeights)
		SHADER_PARAMETER_STRUCT_INCLUDE(FReSTIRParameters, ReSTIRParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FIntPoint, HistoryReservoirViewSize)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DownsampledDepthHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DownsampledNormalHistory)
		SHADER_PARAMETER(float, HistoryDistanceThreshold)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenReSTIRGather(Parameters.Platform);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	using FPermutationDomain = TShaderPermutationDomain<>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FTemporalResamplingCS, "/Engine/Private/Lumen/LumenReSTIRGather.usf", "TemporalResamplingCS", SF_Compute);

class FClearTemporalHistoryCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearTemporalHistoryCS)
	SHADER_USE_PARAMETER_STRUCT(FClearTemporalHistoryCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWTemporalReservoirRayDirection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWTemporalReservoirTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWTemporalReservoirTraceHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWTemporalReservoirTraceHitNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWTemporalReservoirWeights)
		SHADER_PARAMETER_STRUCT_INCLUDE(FReSTIRParameters, ReSTIRParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenReSTIRGather(Parameters.Platform);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	using FPermutationDomain = TShaderPermutationDomain<>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearTemporalHistoryCS, "/Engine/Private/Lumen/LumenReSTIRGather.usf", "ClearTemporalHistoryCS", SF_Compute);

class FSpatialResamplingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSpatialResamplingCS)
	SHADER_USE_PARAMETER_STRUCT(FSpatialResamplingCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FReSTIRParameters, ReSTIRParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER(float, SpatialResamplingKernelRadius)
		SHADER_PARAMETER(float, SpatialResamplingOcclusionScreenTraceDistance)
		SHADER_PARAMETER(uint32, NumSpatialSamples)
		SHADER_PARAMETER(uint32, SpatialResamplingPassIndex)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenReSTIRGather(Parameters.Platform);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	using FPermutationDomain = TShaderPermutationDomain<>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FSpatialResamplingCS, "/Engine/Private/Lumen/LumenReSTIRGather.usf", "SpatialResamplingCS", SF_Compute);

// Must match LumenReSTIRGather.ush
enum class EReSTIRUpsampleMethod : uint8
{
	JitteredBilinearWithFallback,
	SpiralPattern,
	Passthrough,
	MAX
};

class FUpsampleAndIntegrateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FUpsampleAndIntegrateCS)
	SHADER_USE_PARAMETER_STRUCT(FUpsampleAndIntegrateCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWRoughSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWResolveVariance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FReSTIRParameters, ReSTIRParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenReflections::FCompositeParameters, ReflectionsCompositeParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER(float, UpsampleKernelSize)
		SHADER_PARAMETER(uint32, UpsampleNumSamples)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenReSTIRGather(Parameters.Platform);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FUpsampleMethod : SHADER_PERMUTATION_ENUM_CLASS("UPSAMPLE_METHOD", EReSTIRUpsampleMethod);
	class FBilateralFilter : SHADER_PERMUTATION_BOOL("USE_BILATERAL_FILTER");
	using FPermutationDomain = TShaderPermutationDomain<FUpsampleMethod, FBilateralFilter>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FUpsampleAndIntegrateCS, "/Engine/Private/Lumen/LumenReSTIRGather.usf", "UpsampleAndIntegrateCS", SF_Compute);

class FTemporalAccumulationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTemporalAccumulationCS)
	SHADER_USE_PARAMETER_STRUCT(FTemporalAccumulationCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWNewHistoryDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWNewHistoryRoughSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWResolveVariance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWNumHistoryFramesAccumulated)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RoughSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ResolveVariance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffuseIndirectHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RoughSpecularIndirectHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ResolveVarianceHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffuseIndirectDepthHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryNumFramesAccumulated)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NormalHistory)
		SHADER_PARAMETER(float,HistoryDistanceThreshold)
		SHADER_PARAMETER(float,PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER(float,MaxFramesAccumulated)
		SHADER_PARAMETER(FVector4f,HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f,HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, EffectiveResolution)
		SHADER_PARAMETER(int32, FixedJitterIndex)
	END_SHADER_PARAMETER_STRUCT()

	class FBilateralFilter : SHADER_PERMUTATION_BOOL("USE_BILATERAL_FILTER");
	using FPermutationDomain = TShaderPermutationDomain<FBilateralFilter>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const bool bCompile = DoesPlatformSupportLumenReSTIRGather(Parameters.Platform);

#if WITH_EDITOR
		if (bCompile)
		{
			ensureMsgf(VelocityEncodeDepth(Parameters.Platform), TEXT("Platform did not return true from VelocityEncodeDepth().  Lumen requires velocity depth."));
		}
#endif
		return DoesPlatformSupportLumenReSTIRGather(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FTemporalAccumulationCS, "/Engine/Private/Lumen/LumenReSTIRGather.usf", "TemporalAccumulationCS", SF_Compute);

class FBilateralFilterCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FBilateralFilterCS)
	SHADER_USE_PARAMETER_STRUCT(FBilateralFilterCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWRoughSpecularIndirect)
		SHADER_PARAMETER_STRUCT_INCLUDE(FReSTIRParameters, ReSTIRParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, DiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, RoughSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ResolveVariance)
		SHADER_PARAMETER(float, BilateralFilterSpatialKernelRadius)
		SHADER_PARAMETER(uint32, BilateralFilterNumSamples)
		SHADER_PARAMETER(float, BilateralFilterDepthWeightScale)
		SHADER_PARAMETER(float, BilateralFilterNormalAngleThresholdScale)
		SHADER_PARAMETER(float, BilateralFilterStrongBlurVarianceThreshold)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenReSTIRGather(Parameters.Platform);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FBilateralFilterCS, "/Engine/Private/Lumen/LumenReSTIRGather.usf", "BilateralFilterCS", SF_Compute);

void DispatchTemporalAccumulation(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FSceneTextures& SceneTextures,
	bool bPropagateGlobalLightingChange,
	bool bUseBilaterialFilter,
	FRDGTextureRef& DiffuseIndirect,
	FRDGTextureRef& RoughSpecularIndirect,
	FRDGTextureRef& ResolveVariance,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);
	
	if (View.ViewState)
	{
		FReSTIRTemporalAccumulationState& TemporalAccumulationState = View.ViewState->Lumen.ReSTIRGatherState.TemporalAccumulationState;

		TRefCountPtr<IPooledRenderTarget>* DiffuseIndirectHistoryState = &TemporalAccumulationState.DiffuseIndirectHistoryRT;
		FIntRect* DiffuseIndirectHistoryViewRect = &TemporalAccumulationState.DiffuseIndirectHistoryViewRect;
		FVector4f* DiffuseIndirectHistoryScreenPositionScaleBias = &TemporalAccumulationState.DiffuseIndirectHistoryScreenPositionScaleBias;
		TRefCountPtr<IPooledRenderTarget>* HistoryNumFramesAccumulated = &TemporalAccumulationState.NumFramesAccumulatedRT;

		ensureMsgf(SceneTextures.Velocity->Desc.Format != PF_G16R16, TEXT("Lumen requires 3d velocity.  Update Velocity format code."));

		// If the scene render targets reallocate, toss the history so we don't read uninitialized data
		const FIntPoint EffectiveResolution = Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
		const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);
		const FIntPoint HistoryEffectiveResolution = TemporalAccumulationState.HistoryEffectiveResolution;
		const bool bSceneTextureExtentMatchHistory = TemporalAccumulationState.HistorySceneTexturesExtent == SceneTextures.Config.Extent;

		const FIntRect NewHistoryViewRect = View.ViewRect;

		if (*DiffuseIndirectHistoryState
			&& !View.bCameraCut 
			&& !View.bPrevTransformsReset
			&& !GLumenReSTIRGatherClearHistoryEveryFrame
			&& bSceneTextureExtentMatchHistory
			&& (!bUseBilaterialFilter || TemporalAccumulationState.ResolveVarianceHistoryRT)
			&& !bPropagateGlobalLightingChange)
		{
			FRDGTextureDesc DiffuseIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
			FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
			FRDGTextureDesc ResolveVarianceDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_R16F, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);

			FRDGTextureRef NewDiffuseIndirect = GraphBuilder.CreateTexture(DiffuseIndirectDesc, TEXT("Lumen.ReSTIRGather.DiffuseIndirect"));
			FRDGTextureRef NewRoughSpecularIndirect = GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("Lumen.ReSTIRGather.RoughSpecularIndirect"));
			FRDGTextureRef NewResolveVariance = GraphBuilder.CreateTexture(ResolveVarianceDesc, TEXT("Lumen.ReSTIRGather.ResolveVariance"));

			FRDGTextureRef OldDiffuseIndirectHistory = GraphBuilder.RegisterExternalTexture(TemporalAccumulationState.DiffuseIndirectHistoryRT);
			FRDGTextureRef OldRoughSpecularIndirectHistory = GraphBuilder.RegisterExternalTexture(TemporalAccumulationState.RoughSpecularIndirectHistoryRT);
			FRDGTextureRef OldResolveVarianceHistory = GraphBuilder.RegisterExternalTexture(TemporalAccumulationState.ResolveVarianceHistoryRT);
			
			FRDGTextureDesc NumHistoryFramesAccumulatedDesc(FRDGTextureDesc::Create2D(EffectiveResolution, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
			FRDGTextureRef NewNumHistoryFramesAccumulated = GraphBuilder.CreateTexture(NumHistoryFramesAccumulatedDesc, TEXT("Lumen.ReSTIRGather.NumHistoryFramesAccumulated"));

			{
				
				FRDGTextureRef OldDepthHistory = View.ViewState->Lumen.DepthHistoryRT ? GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.DepthHistoryRT) : SceneTextures.Depth.Target;
				FRDGTextureRef OldHistoryNumFramesAccumulated = GraphBuilder.RegisterExternalTexture(*HistoryNumFramesAccumulated);

				{
					FRDGTextureUAVRef RWNewHistoryDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
					FRDGTextureUAVRef RWNewHistoryRoughSpecularIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewRoughSpecularIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
					FRDGTextureUAVRef RWNewResolveVariance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewResolveVariance), ERDGUnorderedAccessViewFlags::SkipBarrier);
					FRDGTextureUAVRef RWNumHistoryFramesAccumulated = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewNumHistoryFramesAccumulated), ERDGUnorderedAccessViewFlags::SkipBarrier);

					auto TemporalAccumulationPass = [&]()
					{
						FTemporalAccumulationCS::FPermutationDomain PermutationVector;
						PermutationVector.Set< FTemporalAccumulationCS::FBilateralFilter >(bUseBilaterialFilter);
						auto ComputeShader = View.ShaderMap->GetShader<FTemporalAccumulationCS>(PermutationVector);

						FTemporalAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTemporalAccumulationCS::FParameters>();
						PassParameters->RWNewHistoryDiffuseIndirect = RWNewHistoryDiffuseIndirect;
						PassParameters->RWNewHistoryRoughSpecularIndirect = RWNewHistoryRoughSpecularIndirect;
						PassParameters->RWResolveVariance = RWNewResolveVariance;
						PassParameters->RWNumHistoryFramesAccumulated = RWNumHistoryFramesAccumulated;

						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
						PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
						PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

						PassParameters->DiffuseIndirectHistory = OldDiffuseIndirectHistory;
						PassParameters->RoughSpecularIndirectHistory = OldRoughSpecularIndirectHistory;
						PassParameters->ResolveVarianceHistory = OldResolveVarianceHistory;
						PassParameters->DiffuseIndirectDepthHistory = OldDepthHistory;
						PassParameters->HistoryNumFramesAccumulated = OldHistoryNumFramesAccumulated;

						PassParameters->HistoryDistanceThreshold = GLumenReSTIRGatherHistoryDistanceThreshold;
						PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;

						const float MaxFramesAccumulatedScale = 1.0f / FMath::Sqrt(FMath::Clamp(View.FinalPostProcessSettings.LumenFinalGatherLightingUpdateSpeed, .5f, 8.0f));
						const float EditingScale = View.Family->bCurrentlyBeingEdited ? .5f : 1.0f;
						PassParameters->MaxFramesAccumulated = FMath::RoundToInt(GLumenReSTIRGatherTemporalMaxFramesAccumulated * MaxFramesAccumulatedScale * EditingScale);
						PassParameters->HistoryScreenPositionScaleBias = *DiffuseIndirectHistoryScreenPositionScaleBias;

						// History uses HistoryDepth which has the same resolution than SceneTextures (no extented/overflow space)
						const FIntPoint BufferSize = SceneTextures.Config.Extent;
						const FVector2D InvBufferSize(1.0f / BufferSize.X, 1.0f / BufferSize.Y);

						// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
						PassParameters->HistoryUVMinMax = FVector4f(
							(DiffuseIndirectHistoryViewRect->Min.X + 0.5f) * InvBufferSize.X,
							(DiffuseIndirectHistoryViewRect->Min.Y + 0.5f) * InvBufferSize.Y,
							(DiffuseIndirectHistoryViewRect->Max.X - 1.0f) * InvBufferSize.X,
							(DiffuseIndirectHistoryViewRect->Max.Y - 1.0f) * InvBufferSize.Y);

						PassParameters->FixedJitterIndex = GLumenReSTIRGatherFixedJitterIndex;

						PassParameters->DiffuseIndirect = DiffuseIndirect;
						PassParameters->RoughSpecularIndirect = RoughSpecularIndirect;
						PassParameters->ResolveVariance = ResolveVariance;

						{
							FComputeShaderUtils::AddPass(
								GraphBuilder,
								RDG_EVENT_NAME("TemporalAccumulation(%ux%u)", View.ViewRect.Width(), View.ViewRect.Height()),
								ComputePassFlags,
								ComputeShader,
								PassParameters,
								FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FTemporalAccumulationCS::GetGroupSize()));
						}
					};

					TemporalAccumulationPass();
				}

				if (!View.bStatePrevViewInfoIsReadOnly)
				{
					// Queue updating the view state's render target reference with the new history
					GraphBuilder.QueueTextureExtraction(NewDiffuseIndirect, &TemporalAccumulationState.DiffuseIndirectHistoryRT);
					GraphBuilder.QueueTextureExtraction(NewRoughSpecularIndirect, &TemporalAccumulationState.RoughSpecularIndirectHistoryRT);
					GraphBuilder.QueueTextureExtraction(NewNumHistoryFramesAccumulated, HistoryNumFramesAccumulated);

					if (bUseBilaterialFilter)
					{
						GraphBuilder.QueueTextureExtraction(NewResolveVariance, &TemporalAccumulationState.ResolveVarianceHistoryRT);
					}
				}
			}

			RoughSpecularIndirect = NewRoughSpecularIndirect;
			DiffuseIndirect = NewDiffuseIndirect;
			ResolveVariance = NewResolveVariance;
		}
		else
		{
			if (!View.bStatePrevViewInfoIsReadOnly)
			{
				// Queue updating the view state's render target reference with the new values
				GraphBuilder.QueueTextureExtraction(DiffuseIndirect, &TemporalAccumulationState.DiffuseIndirectHistoryRT);
				GraphBuilder.QueueTextureExtraction(RoughSpecularIndirect, &TemporalAccumulationState.RoughSpecularIndirectHistoryRT);
				*HistoryNumFramesAccumulated = GSystemTextures.BlackDummy;

				if (bUseBilaterialFilter)
				{
					GraphBuilder.QueueTextureExtraction(ResolveVariance, &TemporalAccumulationState.ResolveVarianceHistoryRT);
				}
			}
		}

		if (!View.bStatePrevViewInfoIsReadOnly)
		{
			*DiffuseIndirectHistoryViewRect = NewHistoryViewRect;
			*DiffuseIndirectHistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(SceneTextures.Config.Extent, View.ViewRect);
			TemporalAccumulationState.HistoryEffectiveResolution = EffectiveResolution;
			TemporalAccumulationState.HistorySceneTexturesExtent = SceneTextures.Config.Extent;
		}
	}
	else
	{
		// Temporal reprojection is disabled or there is no view state - pass through
	}
}

FReservoirTextures AllocateReservoirTextures(FRDGBuilder& GraphBuilder, FIntPoint ReservoirBufferSize)
{
	FReservoirTextures Textures;

	FRDGTextureDesc RayDirectionDesc(FRDGTextureDesc::Create2D(ReservoirBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	Textures.ReservoirRayDirection = GraphBuilder.CreateTexture(RayDirectionDesc, TEXT("Lumen.ReSTIRGather.ReservoirRayDirection"));

	FRDGTextureDesc TraceRadianceDesc(FRDGTextureDesc::Create2D(ReservoirBufferSize, PF_FloatR11G11B10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	Textures.ReservoirTraceRadiance = GraphBuilder.CreateTexture(TraceRadianceDesc, TEXT("Lumen.ReSTIRGather.ReservoirTraceRadiance"));

	FRDGTextureDesc TraceHitDistanceDesc(FRDGTextureDesc::Create2D(ReservoirBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	Textures.ReservoirTraceHitDistance = GraphBuilder.CreateTexture(TraceHitDistanceDesc, TEXT("Lumen.ReSTIRGather.ReservoirTraceHitDistance"));

	FRDGTextureDesc TraceHitNormalDesc(FRDGTextureDesc::Create2D(ReservoirBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	Textures.ReservoirTraceHitNormal = GraphBuilder.CreateTexture(TraceHitNormalDesc, TEXT("Lumen.ReSTIRGather.ReservoirTraceHitNormal"));

	FRDGTextureDesc ReservoirWeightsDesc(FRDGTextureDesc::Create2D(ReservoirBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	Textures.ReservoirWeights = GraphBuilder.CreateTexture(ReservoirWeightsDesc, TEXT("Lumen.ReSTIRGather.ReservoirWeights"));
	return Textures;
}

FReservoirUAVs CreateReservoirUAVs(FRDGBuilder& GraphBuilder, const FReservoirTextures& Textures)
{
	FReservoirUAVs UAVs;
	UAVs.RWReservoirRayDirection = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Textures.ReservoirRayDirection));
	UAVs.RWReservoirTraceRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Textures.ReservoirTraceRadiance));
	UAVs.RWReservoirTraceHitDistance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Textures.ReservoirTraceHitDistance));
	UAVs.RWReservoirTraceHitNormal = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Textures.ReservoirTraceHitNormal));
	UAVs.RWReservoirWeights = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Textures.ReservoirWeights));
	return UAVs;
}

FSSDSignalTextures FDeferredShadingSceneRenderer::RenderLumenReSTIRGather(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	FRDGTextureRef LightingChannelsTexture,
	FViewInfo& View,
	FPreviousViewInfo* PreviousViewInfos,
	ERDGPassFlags ComputePassFlags,
	FLumenScreenSpaceBentNormalParameters& ScreenSpaceBentNormalParameters)
{
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

#if RHI_RAYTRACING	

	RDG_EVENT_SCOPE(GraphBuilder, "LumenReSTIRGather");

	const FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);

	FReSTIRParameters ReSTIRParameters;
	ReSTIRParameters.ReservoirDownsampleFactor = GLumenReSTIRDownsampleFactor;

	const FIntPoint ViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), (int32)ReSTIRParameters.ReservoirDownsampleFactor);
	FIntPoint ReservoirBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, (int32)ReSTIRParameters.ReservoirDownsampleFactor);

	ReSTIRParameters.ReservoirViewSize = ViewSize;
	ReSTIRParameters.ReservoirBufferSize = ReservoirBufferSize;
	ReSTIRParameters.FixedJitterIndex = GLumenReSTIRGatherFixedJitterIndex;
	ReSTIRParameters.ResamplingNormalDotThreshold = FMath::Cos(GLumenReSTIRResamplingAngleThreshold * PI / 180.0f);
	ReSTIRParameters.ResamplingDepthErrorThreshold = GLumenReSTIRResamplingDepthErrorThreshold;

	ReSTIRParameters.Textures = AllocateReservoirTextures(GraphBuilder, ReservoirBufferSize);
	ReSTIRParameters.UAVs = CreateReservoirUAVs(GraphBuilder, ReSTIRParameters.Textures);
	
	FRDGTextureDesc DownsampledSceneDepthDesc(FRDGTextureDesc::Create2D(ReservoirBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ReSTIRParameters.DownsampledSceneDepth = GraphBuilder.CreateTexture(DownsampledSceneDepthDesc, TEXT("Lumen.ReSTIRGather.DownsampledSceneDepth"));

	FRDGTextureDesc DownsampledWorldNormalDesc(FRDGTextureDesc::Create2D(ReservoirBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ReSTIRParameters.DownsampledWorldNormal = GraphBuilder.CreateTexture(DownsampledWorldNormalDesc, TEXT("Lumen.ReSTIRGather.DownsampledWorldNormal"));

	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	ReSTIRParameters.BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleFrame);

	FLumenCardTracingParameters TracingParameters;
	GetLumenCardTracingParameters(GraphBuilder, View, *Scene->GetLumenSceneData(View), FrameTemporaries, false, TracingParameters);

	const bool bValidateTemporalReservoirs = View.ViewState 
		&& GLumenReSTIRValidateTemporalReservoirsEveryNFrames > 0
		&& (View.ViewState->GetFrameIndex() % GLumenReSTIRValidateTemporalReservoirsEveryNFrames) == 0;

	if (bValidateTemporalReservoirs)
	{
		FReSTIRTemporalResamplingState& TemporalResamplingState = View.ViewState->Lumen.ReSTIRGatherState.TemporalResamplingState;

		if (TemporalResamplingState.TemporalReservoirRayDirectionRT
			&& ReSTIRParameters.ReservoirBufferSize == TemporalResamplingState.HistoryReservoirBufferSize)
		{
			FLumenValidateReservoirs::FParameters* Parameters = GraphBuilder.AllocParameters<FLumenValidateReservoirs::FParameters>();
			{
				SetLumenHardwareRayTracingSharedParameters(
					GraphBuilder,
					SceneTextureParameters,
					View,
					TracingParameters,
					&Parameters->SharedParameters
				);

				Parameters->RWTemporalReservoirTraceRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirTraceRadianceRT)));
				Parameters->RWTemporalReservoirWeights = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirWeightsRT)));
				Parameters->TemporalReservoirRayDirection = GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirRayDirectionRT);
				Parameters->TemporalReservoirTraceHitDistance = GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirTraceHitDistanceRT);
				Parameters->TemporalReservoirTraceHitNormal = GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirTraceHitNormalRT);
				Parameters->ReSTIRParameters = ReSTIRParameters;
				Parameters->HistoryScreenPositionScaleBias = TemporalResamplingState.HistoryScreenPositionScaleBias;
				Parameters->HistoryReservoirViewSize = TemporalResamplingState.HistoryReservoirViewSize;
				Parameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
				Parameters->DownsampledDepthHistory = TemporalResamplingState.DownsampledDepthHistoryRT ? GraphBuilder.RegisterExternalTexture(TemporalResamplingState.DownsampledDepthHistoryRT) : Parameters->ReSTIRParameters.DownsampledSceneDepth;
				Parameters->DownsampledNormalHistory = TemporalResamplingState.DownsampledNormalHistoryRT ? GraphBuilder.RegisterExternalTexture(TemporalResamplingState.DownsampledNormalHistoryRT) : Parameters->ReSTIRParameters.DownsampledWorldNormal;
				Parameters->ReSTIRParameters = ReSTIRParameters;
				Parameters->PullbackBias = Lumen::GetHardwareRayTracingPullbackBias();
				Parameters->ApplySkyLight = 1;
				Parameters->MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
				Parameters->MaxRayIntensity = GLumenReSTIRMaxRayIntensity;
			}

			const bool bUseHitLighting = LumenReflections::UseHitLighting(View, true);
			const bool bUseMinimalPayload = !bUseHitLighting;

			FLumenValidateReservoirsRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenValidateReservoirsRGS::FHitLighting>(bUseHitLighting);
			TShaderRef<FLumenValidateReservoirsRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenValidateReservoirsRGS>(PermutationVector);

			AddLumenRayTraceDispatchPass(
				GraphBuilder,
				RDG_EVENT_NAME("ValidateReservoirs"),
				RayGenerationShader,
				Parameters,
				TemporalResamplingState.HistoryReservoirViewSize,
				View,
				bUseMinimalPayload);
		}
	}

	{
		FLumenInitialSampling::FParameters* Parameters = GraphBuilder.AllocParameters<FLumenInitialSampling::FParameters>();
		{
			SetLumenHardwareRayTracingSharedParameters(
				GraphBuilder,
				SceneTextureParameters,
				View,
				TracingParameters,
				&Parameters->SharedParameters
			);
			
			Parameters->RWDownsampledSceneDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReSTIRParameters.DownsampledSceneDepth));
			Parameters->RWDownsampledWorldNormal = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReSTIRParameters.DownsampledWorldNormal));

			Parameters->ReSTIRParameters = ReSTIRParameters;
			Parameters->PullbackBias = Lumen::GetHardwareRayTracingPullbackBias();
			Parameters->ApplySkyLight = 1;
			Parameters->MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
			Parameters->MaxRayIntensity = GLumenReSTIRMaxRayIntensity;
		}

		const bool bUseHitLighting = LumenReflections::UseHitLighting(View, true);
		const bool bUseMinimalPayload = !bUseHitLighting;

		FLumenInitialSamplingRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenInitialSamplingRGS::FHitLighting>(bUseHitLighting);
		TShaderRef<FLumenInitialSamplingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenInitialSamplingRGS>(PermutationVector);

		AddLumenRayTraceDispatchPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitialSampling %ux%u", ReSTIRParameters.ReservoirViewSize.X, ReSTIRParameters.ReservoirViewSize.Y),
			RayGenerationShader,
			Parameters,
			ReSTIRParameters.ReservoirViewSize,
			View,
			bUseMinimalPayload);
	}

	const bool bUseTemporalResampling = GLumenReSTIRTemporalResampling != 0 && View.ViewState;

	if (bUseTemporalResampling)
	{
		FReSTIRTemporalResamplingState& TemporalResamplingState = View.ViewState->Lumen.ReSTIRGatherState.TemporalResamplingState;

		FReservoirTextures TemporalReservoirTextures = AllocateReservoirTextures(GraphBuilder, ReservoirBufferSize);
		FReservoirUAVs TemporalReservoirUAVs = CreateReservoirUAVs(GraphBuilder, TemporalReservoirTextures);

		if (TemporalResamplingState.TemporalReservoirRayDirectionRT
			&& !View.bCameraCut
			&& !View.bPrevTransformsReset
			&& ReSTIRParameters.ReservoirBufferSize == TemporalResamplingState.HistoryReservoirBufferSize
			&& GLumenReSTIRTemporalResamplingResetHistory == 0)
		{
			FTemporalResamplingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTemporalResamplingCS::FParameters>();
			PassParameters->RWTemporalReservoirRayDirection = TemporalReservoirUAVs.RWReservoirRayDirection;
			PassParameters->RWTemporalReservoirTraceRadiance = TemporalReservoirUAVs.RWReservoirTraceRadiance;
			PassParameters->RWTemporalReservoirTraceHitDistance = TemporalReservoirUAVs.RWReservoirTraceHitDistance;
			PassParameters->RWTemporalReservoirTraceHitNormal = TemporalReservoirUAVs.RWReservoirTraceHitNormal;
			PassParameters->RWTemporalReservoirWeights = TemporalReservoirUAVs.RWReservoirWeights;
			PassParameters->TemporalReservoirRayDirection = GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirRayDirectionRT);
			PassParameters->TemporalReservoirTraceRadiance = GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirTraceRadianceRT);
			PassParameters->TemporalReservoirTraceHitDistance = GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirTraceHitDistanceRT);
			PassParameters->TemporalReservoirTraceHitNormal = GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirTraceHitNormalRT);
			PassParameters->TemporalReservoirWeights = GraphBuilder.RegisterExternalTexture(TemporalResamplingState.TemporalReservoirWeightsRT);
			PassParameters->ReSTIRParameters = ReSTIRParameters;
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

			PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
			PassParameters->HistoryScreenPositionScaleBias = TemporalResamplingState.HistoryScreenPositionScaleBias;

			const FIntPoint BufferSize = SceneTextures.Config.Extent;
			const FVector2D InvBufferSize(1.0f / BufferSize.X, 1.0f / BufferSize.Y);
			PassParameters->HistoryUVMinMax = FVector4f(
				(TemporalResamplingState.HistoryViewRect.Min.X) * InvBufferSize.X,
				(TemporalResamplingState.HistoryViewRect.Min.Y) * InvBufferSize.Y,
				(TemporalResamplingState.HistoryViewRect.Max.X) * InvBufferSize.X,
				(TemporalResamplingState.HistoryViewRect.Max.Y) * InvBufferSize.Y);
			PassParameters->HistoryReservoirViewSize = TemporalResamplingState.HistoryReservoirViewSize;
			PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
			PassParameters->DownsampledDepthHistory = TemporalResamplingState.DownsampledDepthHistoryRT ? GraphBuilder.RegisterExternalTexture(TemporalResamplingState.DownsampledDepthHistoryRT) : PassParameters->ReSTIRParameters.DownsampledSceneDepth;
			PassParameters->DownsampledNormalHistory = TemporalResamplingState.DownsampledNormalHistoryRT ? GraphBuilder.RegisterExternalTexture(TemporalResamplingState.DownsampledNormalHistoryRT) : PassParameters->ReSTIRParameters.DownsampledWorldNormal;
			PassParameters->HistoryDistanceThreshold = GLumenReSTIRTemporalResamplingHistoryDistanceThreshold;

			FTemporalResamplingCS::FPermutationDomain PermutationVector;
			auto ComputeShader = View.ShaderMap->GetShader<FTemporalResamplingCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TemporalResampling"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(ReSTIRParameters.ReservoirViewSize, FTemporalResamplingCS::GetGroupSize()));

			ReSTIRParameters.Textures = TemporalReservoirTextures;
		}
		else
		{
			FClearTemporalHistoryCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearTemporalHistoryCS::FParameters>();
			PassParameters->RWTemporalReservoirRayDirection = TemporalReservoirUAVs.RWReservoirRayDirection;
			PassParameters->RWTemporalReservoirTraceRadiance = TemporalReservoirUAVs.RWReservoirTraceRadiance;
			PassParameters->RWTemporalReservoirTraceHitDistance = TemporalReservoirUAVs.RWReservoirTraceHitDistance;
			PassParameters->RWTemporalReservoirTraceHitNormal = TemporalReservoirUAVs.RWReservoirTraceHitNormal;
			PassParameters->RWTemporalReservoirWeights = TemporalReservoirUAVs.RWReservoirWeights;
			PassParameters->ReSTIRParameters = ReSTIRParameters;
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

			FClearTemporalHistoryCS::FPermutationDomain PermutationVector;
			auto ComputeShader = View.ShaderMap->GetShader<FClearTemporalHistoryCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClearTemporalHistory"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(ReSTIRParameters.ReservoirViewSize, FTemporalResamplingCS::GetGroupSize()));

			GLumenReSTIRTemporalResamplingResetHistory = 0;
		}

		if (!View.bStatePrevViewInfoIsReadOnly)
		{
			// Queue updating the view state's render target reference with the new history
			TemporalResamplingState.HistoryViewRect = View.ViewRect;
			TemporalResamplingState.HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(SceneTextures.Config.Extent, View.ViewRect);
			TemporalResamplingState.HistoryReservoirViewSize = ReSTIRParameters.ReservoirViewSize;
			TemporalResamplingState.HistoryReservoirBufferSize = ReSTIRParameters.ReservoirBufferSize;
			GraphBuilder.QueueTextureExtraction(TemporalReservoirTextures.ReservoirRayDirection, &TemporalResamplingState.TemporalReservoirRayDirectionRT);
			GraphBuilder.QueueTextureExtraction(TemporalReservoirTextures.ReservoirTraceRadiance, &TemporalResamplingState.TemporalReservoirTraceRadianceRT);
			GraphBuilder.QueueTextureExtraction(TemporalReservoirTextures.ReservoirTraceHitDistance, &TemporalResamplingState.TemporalReservoirTraceHitDistanceRT);
			GraphBuilder.QueueTextureExtraction(TemporalReservoirTextures.ReservoirTraceHitNormal, &TemporalResamplingState.TemporalReservoirTraceHitNormalRT);
			GraphBuilder.QueueTextureExtraction(TemporalReservoirTextures.ReservoirWeights, &TemporalResamplingState.TemporalReservoirWeightsRT);
			GraphBuilder.QueueTextureExtraction(ReSTIRParameters.DownsampledSceneDepth, &TemporalResamplingState.DownsampledDepthHistoryRT);
			GraphBuilder.QueueTextureExtraction(ReSTIRParameters.DownsampledWorldNormal, &TemporalResamplingState.DownsampledNormalHistoryRT);
		}
	}

	const int32 NumSpatialResamplingPasses = FMath::Clamp<int32>(GLumenReSTIRSpatialResampling > 0 ? GLumenReSTIRNumSpatialResamplingPasses : 0, 0, 16);

	for (int32 SpatialResamplingPassIndex = 0; SpatialResamplingPassIndex < NumSpatialResamplingPasses; SpatialResamplingPassIndex++)
	{
		FReservoirTextures SpatialReservoirTextures = AllocateReservoirTextures(GraphBuilder, ReservoirBufferSize);
		ReSTIRParameters.UAVs = CreateReservoirUAVs(GraphBuilder, SpatialReservoirTextures);

		FSpatialResamplingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSpatialResamplingCS::FParameters>();
		PassParameters->ReSTIRParameters = ReSTIRParameters;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		PassParameters->SpatialResamplingKernelRadius = GLumenReSTIRSpatialResamplingKernelRadius;
		PassParameters->SpatialResamplingOcclusionScreenTraceDistance = GLumenReSTIRSpatialResamplingOcclusionScreenTraceDistance;
		PassParameters->NumSpatialSamples = GLumenReSTIRNumSpatialSamples;
		PassParameters->SpatialResamplingPassIndex = SpatialResamplingPassIndex;

		FSpatialResamplingCS::FPermutationDomain PermutationVector;
		auto ComputeShader = View.ShaderMap->GetShader<FSpatialResamplingCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SpatialResampling"),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(ReSTIRParameters.ReservoirViewSize, FSpatialResamplingCS::GetGroupSize()));

		ReSTIRParameters.Textures = SpatialReservoirTextures;
	}

	FRDGTextureDesc DiffuseIndirectDesc = FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef DiffuseIndirect = GraphBuilder.CreateTexture(DiffuseIndirectDesc, TEXT("Lumen.ReSTIRGather.DiffuseIndirect"));

	FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef RoughSpecularIndirect = GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("Lumen.ReSTIRGather.RoughSpecularIndirect"));

	FRDGTextureDesc ResolveVarianceDesc = FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef ResolveVariance = GraphBuilder.CreateTexture(ResolveVarianceDesc, TEXT("Lumen.ReSTIRGather.ResolveVariance"));

	const bool bBilateralFilter = GLumenReSTIRGatherBilateralFilter != 0;

	{
		FUpsampleAndIntegrateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpsampleAndIntegrateCS::FParameters>();
		PassParameters->RWDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseIndirect));
		PassParameters->RWRoughSpecularIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RoughSpecularIndirect));
		PassParameters->RWResolveVariance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ResolveVariance));
		PassParameters->ReSTIRParameters = ReSTIRParameters;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		LumenReflections::SetupCompositeParameters(View, PassParameters->ReflectionsCompositeParameters);
		PassParameters->UpsampleKernelSize = GLumenReSTIRUpsampleKernelSize;
		PassParameters->UpsampleNumSamples = GLumenReSTIRUpsampleNumSamples;

		FUpsampleAndIntegrateCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FUpsampleAndIntegrateCS::FUpsampleMethod >((EReSTIRUpsampleMethod)GLumenReSTIRUpsampleMethod);
		PermutationVector.Set< FUpsampleAndIntegrateCS::FBilateralFilter >(bBilateralFilter);
		auto ComputeShader = View.ShaderMap->GetShader<FUpsampleAndIntegrateCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("IntegrateAndUpsample Type=%u %ux%u", GLumenReSTIRUpsampleMethod, View.ViewRect.Width(), View.ViewRect.Height()),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FUpsampleAndIntegrateCS::GetGroupSize()));
	}

	if (GLumenReSTIRGatherTemporalFilter)
	{
		DispatchTemporalAccumulation(
			GraphBuilder,
			View,
			SceneTextures,
			LumenCardRenderer.bPropagateGlobalLightingChange,
			bBilateralFilter,
			DiffuseIndirect,
			RoughSpecularIndirect,
			ResolveVariance,
			ComputePassFlags);
	}

	if (bBilateralFilter)
	{
		FRDGTextureRef FilteredDiffuseIndirect = GraphBuilder.CreateTexture(DiffuseIndirectDesc, TEXT("Lumen.ReSTIRGather.FilteredDiffuseIndirect"));
		FRDGTextureRef FilteredRoughSpecularIndirect = GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("Lumen.ReSTIRGather.FilteredRoughSpecularIndirect"));

		FBilateralFilterCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBilateralFilterCS::FParameters>();
		PassParameters->RWDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(FilteredDiffuseIndirect));
		PassParameters->RWRoughSpecularIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(FilteredRoughSpecularIndirect));
		PassParameters->DiffuseIndirect = DiffuseIndirect;
		PassParameters->RoughSpecularIndirect = RoughSpecularIndirect;
		PassParameters->ResolveVariance = ResolveVariance;
		PassParameters->BilateralFilterSpatialKernelRadius = GLumenReSTIRGatherBilateralFilterSpatialKernelRadius;
		PassParameters->BilateralFilterNumSamples = GLumenReSTIRGatherBilateralFilterNumSamples;
		PassParameters->BilateralFilterDepthWeightScale = GLumenReSTIRGatherBilateralFilterDepthWeightScale;
		PassParameters->BilateralFilterNormalAngleThresholdScale = GLumenReSTIRGatherBilateralFilterNormalAngleThresholdScale;
		PassParameters->BilateralFilterStrongBlurVarianceThreshold = GLumenReSTIRGatherBilateralFilterStrongBlurVarianceThreshold;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

		FBilateralFilterCS::FPermutationDomain PermutationVector;
		auto ComputeShader = View.ShaderMap->GetShader<FBilateralFilterCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BilateralFilter"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FBilateralFilterCS::GetGroupSize()));

		DiffuseIndirect = FilteredDiffuseIndirect;
		RoughSpecularIndirect = FilteredRoughSpecularIndirect;
	}

	FSSDSignalTextures DenoiserOutputs;
	DenoiserOutputs.Textures[0] = DiffuseIndirect;
	DenoiserOutputs.Textures[1] = SystemTextures.Black;
	DenoiserOutputs.Textures[2] = RoughSpecularIndirect;

	if (GLumenReSTIRGatherShortRangeAmbientOcclusion != 0 && ViewFamily.EngineShowFlags.LumenShortRangeAmbientOcclusion)
	{
		float MaxScreenTraceFraction = GLumenReSTIRGatherShortRangeAOMaxScreenTraceFraction;
		ScreenSpaceBentNormalParameters = ComputeScreenSpaceShortRangeAO(GraphBuilder, Scene, View, SceneTextures, LightingChannelsTexture, BlueNoise, MaxScreenTraceFraction, 1.0f, ComputePassFlags);
	}

	return DenoiserOutputs;
#else
	FSSDSignalTextures DenoiserOutputs;
	return DenoiserOutputs;
#endif
}