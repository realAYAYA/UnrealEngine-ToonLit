// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenSceneLighting.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "DistanceFieldLightingShared.h"
#include "VolumetricCloudRendering.h"

#if RHI_RAYTRACING

#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"

static TAutoConsoleVariable<int32> CVarLumenSceneDirectLightingHardwareRayTracing(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing"),
	1,
	TEXT("Enables hardware ray tracing for Lumen direct lighting (Default = 1)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarLumenSceneDirectLightingHardwareRayTracingHeightfieldProjectionBias(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing.HeightfieldProjectionBias"),
	0,
	TEXT("Applies a projection bias such that an occlusion ray starts on the ray-tracing heightfield representation.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenSceneDirectLightingHardwareRayTracingHeightfieldProjectionBiasSearchRadius(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing.HeightfieldProjectionBiasSearchRadius"),
	256,
	TEXT("Determines the search radius for heightfield projection bias. Larger search radius corresponds to increased traversal cost (default = 256).\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

#endif // RHI_RAYTRACING

namespace Lumen
{
	bool UseHardwareRayTracedDirectLighting(const FSceneViewFamily& ViewFamily)
	{
#if RHI_RAYTRACING
		return IsRayTracingEnabled()
			&& Lumen::UseHardwareRayTracing(ViewFamily)
			&& (CVarLumenSceneDirectLightingHardwareRayTracing.GetValueOnRenderThread() != 0);
#else
		return false;
#endif
	}
} // namespace Lumen

#if RHI_RAYTRACING

class FLumenDirectLightingHardwareRayTracingBatched : public FLumenHardwareRayTracingShaderBase
{
	DECLARE_LUMEN_RAYTRACING_SHADER(FLumenDirectLightingHardwareRayTracingBatched, Lumen::ERayTracingShaderDispatchSize::DispatchSize2D)

	class FEnableFarFieldTracing : SHADER_PERMUTATION_BOOL("ENABLE_FAR_FIELD_TRACING");
	class FEnableHeightfieldProjectionBias : SHADER_PERMUTATION_BOOL("ENABLE_HEIGHTFIELD_PROJECTION_BIAS");
	using FPermutationDomain = TShaderPermutationDomain<FLumenHardwareRayTracingShaderBase::FBasePermutationDomain, FEnableFarFieldTracing, FEnableHeightfieldProjectionBias>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenHardwareRayTracingShaderBase::FSharedParameters, SharedParameters)
		RDG_BUFFER_ACCESS(HardwareRayTracingIndirectArgs, ERHIAccess::IndirectArgs | ERHIAccess::SRVCompute)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, LightTiles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FLumenPackedLight>, LumenPackedLights)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowTraceAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowTraces)

		// Constants
		SHADER_PARAMETER(float, PullbackBias)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(uint32, ViewIndex)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, FarFieldMaxTraceDistance)
		SHADER_PARAMETER(FVector3f, FarFieldReferencePos)

		SHADER_PARAMETER(float, HardwareRayTracingShadowRayBias)
		SHADER_PARAMETER(float, HeightfieldShadowReceiverBias)
		SHADER_PARAMETER(float, HeightfieldProjectionBiasSearchRadius)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowMaskTiles)
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

IMPLEMENT_LUMEN_RAYGEN_AND_COMPUTE_RAYTRACING_SHADERS(FLumenDirectLightingHardwareRayTracingBatched)

IMPLEMENT_GLOBAL_SHADER(FLumenDirectLightingHardwareRayTracingBatchedCS, "/Engine/Private/Lumen/LumenSceneDirectLightingHardwareRayTracing.usf", "LumenSceneDirectLightingHardwareRayTracingCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLumenDirectLightingHardwareRayTracingBatchedRGS, "/Engine/Private/Lumen/LumenSceneDirectLightingHardwareRayTracing.usf", "LumenSceneDirectLightingHardwareRayTracingRGS", SF_RayGen);

class FLumenDirectLightingHardwareRayTracingIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenDirectLightingHardwareRayTracingIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenDirectLightingHardwareRayTracingIndirectArgsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, DispatchLightTilesIndirectArgs)
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

IMPLEMENT_GLOBAL_SHADER(FLumenDirectLightingHardwareRayTracingIndirectArgsCS, "/Engine/Private/Lumen/LumenSceneDirectLightingHardwareRayTracing.usf", "LumenDirectLightingHardwareRayTracingIndirectArgsCS", SF_Compute);

float GetHeightfieldProjectionBiasSearchRadius()
{
	return FMath::Max(CVarLumenSceneDirectLightingHardwareRayTracingHeightfieldProjectionBiasSearchRadius.GetValueOnRenderThread(), 0);
}

void FDeferredShadingSceneRenderer::PrepareLumenHardwareRayTracingDirectLightingLumenMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (Lumen::UseHardwareRayTracedDirectLighting(*View.Family))
	{
		FLumenDirectLightingHardwareRayTracingBatchedRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenDirectLightingHardwareRayTracingBatchedRGS::FEnableFarFieldTracing>(Lumen::UseFarField(*View.Family));
		PermutationVector.Set<FLumenDirectLightingHardwareRayTracingBatchedRGS::FEnableHeightfieldProjectionBias>(CVarLumenSceneDirectLightingHardwareRayTracingHeightfieldProjectionBias.GetValueOnRenderThread() != 0);
		TShaderRef<FLumenDirectLightingHardwareRayTracingBatchedRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenDirectLightingHardwareRayTracingBatchedRGS>(PermutationVector);
		OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
	}
}

void SetLumenHardwareRayTracedDirectLightingShadowsParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	int32 ViewIndex,
	const FLumenCardTracingParameters& TracingParameters,
	FRDGBufferRef LightTileAllocator,
	FRDGBufferRef LightTiles,
	FRDGBufferRef LumenPackedLights,
	FRDGBufferUAVRef ShadowMaskTilesUAV,
	FRDGBufferRef HardwareRayTracingIndirectArgsBuffer,
	FLumenDirectLightingHardwareRayTracingBatchedRGS::FParameters* Parameters
)
{
	SetLumenHardwareRayTracingSharedParameters(
		GraphBuilder,
		GetSceneTextureParameters(GraphBuilder, View),
		View,
		TracingParameters,
		&Parameters->SharedParameters
	);

	Parameters->HardwareRayTracingIndirectArgs = HardwareRayTracingIndirectArgsBuffer;
	Parameters->LightTileAllocator = GraphBuilder.CreateSRV(LightTileAllocator);
	Parameters->LightTiles = GraphBuilder.CreateSRV(LightTiles);
	Parameters->LumenPackedLights = GraphBuilder.CreateSRV(LumenPackedLights);

	Parameters->PullbackBias = 0.0f;
	Parameters->MaxTraversalIterations = LumenHardwareRayTracing::GetMaxTraversalIterations();
	Parameters->ViewIndex = ViewIndex;
	Parameters->MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
	Parameters->FarFieldMaxTraceDistance = Lumen::GetFarFieldMaxTraceDistance();
	Parameters->FarFieldReferencePos = (FVector3f)Lumen::GetFarFieldReferencePos();
	
	Parameters->HardwareRayTracingShadowRayBias = LumenSceneDirectLighting::GetHardwareRayTracingShadowRayBias();
	Parameters->HeightfieldShadowReceiverBias = Lumen::GetHeightfieldReceiverBias();
	Parameters->HeightfieldProjectionBiasSearchRadius = GetHeightfieldProjectionBiasSearchRadius();

	// Output
	Parameters->RWShadowMaskTiles = ShadowMaskTilesUAV;
}

#endif // RHI_RAYTRACING

void TraceLumenHardwareRayTracedDirectLightingShadows(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	int32 ViewIndex,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	FRDGBufferRef ShadowTraceIndirectArgs,
	FRDGBufferRef ShadowTraceAllocator,
	FRDGBufferRef ShadowTraces,
	FRDGBufferRef LightTileAllocator,
	FRDGBufferRef LightTiles,
	FRDGBufferRef LumenPackedLights,
	FRDGBufferUAVRef ShadowMaskTilesUAV,
	ERDGPassFlags ComputePassFlags)
{
#if RHI_RAYTRACING
	const bool bInlineRayTracing = Lumen::UseHardwareInlineRayTracing(*View.Family);
	const bool bUseMinimalPayload = true;

	checkf(ComputePassFlags != ERDGPassFlags::AsyncCompute || bInlineRayTracing, TEXT("Async Lumen HWRT is only supported for inline ray tracing"));

	// Set indirect dispatch arguments
	FRDGBufferRef HardwareRayTracingIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Reflection.CompactTracingIndirectArgs"));
	{
		FLumenDirectLightingHardwareRayTracingIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenDirectLightingHardwareRayTracingIndirectArgsCS::FParameters>();
		{
			PassParameters->DispatchLightTilesIndirectArgs = GraphBuilder.CreateSRV(ShadowTraceIndirectArgs, PF_R32_UINT);
			PassParameters->RWHardwareRayTracingIndirectArgs = GraphBuilder.CreateUAV(HardwareRayTracingIndirectArgsBuffer, PF_R32_UINT);
			PassParameters->OutputThreadGroupSize = bInlineRayTracing ? FLumenDirectLightingHardwareRayTracingBatchedCS::GetThreadGroupSize(View.GetShaderPlatform()) : FLumenDirectLightingHardwareRayTracingBatchedRGS::GetThreadGroupSize();
		}

		TShaderRef<FLumenDirectLightingHardwareRayTracingIndirectArgsCS> ComputeShader = View.ShaderMap->GetShader<FLumenDirectLightingHardwareRayTracingIndirectArgsCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("FLumenDirectLightingHardwareRayTracingIndirectArgsCS"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FLumenCardTracingParameters TracingParameters;
	GetLumenCardTracingParameters(GraphBuilder, View, *Scene->GetLumenSceneData(View), FrameTemporaries, /*bSurfaceCacheFeedback*/ false, TracingParameters);

	FLumenDirectLightingHardwareRayTracingBatched::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenDirectLightingHardwareRayTracingBatched::FParameters>();
	SetLumenHardwareRayTracedDirectLightingShadowsParameters(
		GraphBuilder,
		View,
		ViewIndex,
		TracingParameters,
		LightTileAllocator,
		LightTiles,
		LumenPackedLights,
		ShadowMaskTilesUAV,
		HardwareRayTracingIndirectArgsBuffer,
		PassParameters
	);
	PassParameters->ShadowTraceAllocator = ShadowTraceAllocator ? GraphBuilder.CreateSRV(ShadowTraceAllocator) : nullptr;
	PassParameters->ShadowTraces = ShadowTraces ? GraphBuilder.CreateSRV(ShadowTraces) : nullptr;

	FLumenDirectLightingHardwareRayTracingBatchedRGS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FLumenDirectLightingHardwareRayTracingBatchedRGS::FEnableFarFieldTracing>(Lumen::UseFarField(*View.Family));
	PermutationVector.Set<FLumenDirectLightingHardwareRayTracingBatchedRGS::FEnableHeightfieldProjectionBias>(CVarLumenSceneDirectLightingHardwareRayTracingHeightfieldProjectionBias.GetValueOnRenderThread() != 0);

	if (bInlineRayTracing)
	{
		FLumenDirectLightingHardwareRayTracingBatchedCS::AddLumenRayTracingDispatchIndirect(
			GraphBuilder, 
			RDG_EVENT_NAME("LumenDirectLightingHardwareRayTracingCS"),
			View,
			PermutationVector,
			PassParameters,
			PassParameters->HardwareRayTracingIndirectArgs,
			0,
			ComputePassFlags);
	}
	else
	{
		FLumenDirectLightingHardwareRayTracingBatchedRGS::AddLumenRayTracingDispatchIndirect(
			GraphBuilder, 
			RDG_EVENT_NAME("LumenDirectLightingHardwareRayTracingRGS"),
			View, 
			PermutationVector, 
			PassParameters, 
			PassParameters->HardwareRayTracingIndirectArgs, 
			0, 
			bUseMinimalPayload);
	}
#else
	unimplemented();
#endif // RHI_RAYTRACING
}