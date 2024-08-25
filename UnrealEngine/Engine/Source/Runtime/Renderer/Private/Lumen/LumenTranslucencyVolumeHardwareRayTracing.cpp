// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenTranslucencyVolumeHardwareRayTracing.cpp
=============================================================================*/

#include "LumenTranslucencyVolumeLighting.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "LumenTracingUtils.h"
#include "LumenRadianceCache.h"

#if RHI_RAYTRACING

#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"

// Console variables
static TAutoConsoleVariable<int32> CVarLumenTranslucencyVolumeHardwareRayTracing(
	TEXT("r.Lumen.TranslucencyVolume.HardwareRayTracing"),
	1,
	TEXT("Enables hardware ray tracing for Lumen translucency volume (Default = 1)"),
	ECVF_RenderThreadSafe
);

#endif // RHI_RAYTRACING

namespace Lumen
{
	bool UseHardwareRayTracedTranslucencyVolume(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& (CVarLumenTranslucencyVolumeHardwareRayTracing.GetValueOnRenderThread() != 0);
#else
		return false;
#endif
	}
}

#if RHI_RAYTRACING

class FLumenTranslucencyVolumeHardwareRayTracing : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenTranslucencyVolumeHardwareRayTracing, Lumen::ERayTracingShaderDispatchSize::DispatchSize1D)

	class FRadianceCache : SHADER_PERMUTATION_BOOL("USE_RADIANCE_CACHE");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FRadianceCache>;

	// Parameters
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, Lumen::ERayTracingShaderDispatchType ShaderDispatchType, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLumenHardwareRayTracingShaderBase::ModifyCompilationEnvironment(Parameters, ShaderDispatchType, Lumen::ESurfaceCacheSampling::AlwaysResidentPagesWithoutFeedback, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
};

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenTranslucencyVolumeHardwareRayTracing)

IMPLEMENT_GLOBAL_SHADER(FLumenTranslucencyVolumeHardwareRayTracingRGS, "/Engine/Private/Lumen/LumenTranslucencyVolumeHardwareRayTracing.usf", "LumenTranslucencyVolumeHardwareRayTracingRGS", SF_RayGen);
IMPLEMENT_GLOBAL_SHADER(FLumenTranslucencyVolumeHardwareRayTracingCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeHardwareRayTracing.usf", "LumenTranslucencyVolumeHardwareRayTracingCS", SF_Compute);

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingTranslucencyVolumeLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedTranslucencyVolume(*View.Family))
	{
		extern int32 GLumenTranslucencyVolumeRadianceCache;

		FLumenTranslucencyVolumeHardwareRayTracingRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenTranslucencyVolumeHardwareRayTracingRGS::FRadianceCache>(GLumenTranslucencyVolumeRadianceCache != 0);
		TShaderRef<FLumenTranslucencyVolumeHardwareRayTracingRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenTranslucencyVolumeHardwareRayTracingRGS>(PermutationVector);

		OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
	}
}

#endif // RHI_RAYTRACING

void HardwareRayTraceTranslucencyVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters,
	FRDGTextureRef VolumeTraceRadiance,
	FRDGTextureRef VolumeTraceHitDistance,
	ERDGPassFlags ComputePassFlags
)
{
#if RHI_RAYTRACING
	bool bUseMinimalPayload = true;
	bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);

	checkf(ComputePassFlags != ERDGPassFlags::AsyncCompute || bInlineRayTracing, TEXT("Async Lumen HWRT is only supported for inline ray tracing"));

	// Cast rays
	{
		FLumenTranslucencyVolumeHardwareRayTracingRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenTranslucencyVolumeHardwareRayTracingRGS::FParameters>();

		SetLumenHardwareRayTracingSharedParameters(
			GraphBuilder,
			GetSceneTextureParameters(GraphBuilder, View),
			View,
			TracingParameters,
			&PassParameters->SharedParameters);

		PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(VolumeTraceRadiance);
		PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeTraceHitDistance);
		PassParameters->VolumeParameters = VolumeParameters;
		PassParameters->TraceSetupParameters = TraceSetupParameters;
		PassParameters->MaxTraversalIterations = LumenHardwareRayTracing::GetMaxTraversalIterations();

		PassParameters->RadianceCacheParameters = RadianceCacheParameters;

		FLumenTranslucencyVolumeHardwareRayTracingRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenTranslucencyVolumeHardwareRayTracingRGS::FRadianceCache>(RadianceCacheParameters.RadianceProbeIndirectionTexture != nullptr);

		const FIntPoint DispatchResolution(VolumeTraceRadiance->Desc.Extent * FIntPoint(VolumeTraceRadiance->Desc.Depth, 1));

		if (bInlineRayTracing)
		{
			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DispatchResolution, FLumenTranslucencyVolumeHardwareRayTracingCS::GetThreadGroupSize(View.GetShaderPlatform()));
			FLumenTranslucencyVolumeHardwareRayTracingCS::AddLumenRayTracingDispatch(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing (inline) %ux%u", DispatchResolution.X, DispatchResolution.Y),
				View, 
				PermutationVector,
				PassParameters,
				GroupCount,
				ComputePassFlags);
		}
		else
		{
			FLumenTranslucencyVolumeHardwareRayTracingRGS::AddLumenRayTracingDispatch(
				GraphBuilder,
				RDG_EVENT_NAME("HardwareRayTracing (raygen) %ux%u", DispatchResolution.X, DispatchResolution.Y),
				View, 
				PermutationVector,
				PassParameters,
				DispatchResolution,
				bUseMinimalPayload);
		}
	}

#else
	unimplemented();
#endif // RHI_RAYTRACING
}
