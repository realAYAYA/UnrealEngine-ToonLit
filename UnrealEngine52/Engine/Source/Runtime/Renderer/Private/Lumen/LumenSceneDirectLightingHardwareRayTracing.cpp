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

#include "RayTracing/RayTracingDeferredMaterials.h"
#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingLighting.h"
#include "LumenHardwareRayTracingCommon.h"

// Console variables
static TAutoConsoleVariable<int32> CVarLumenSceneDirectLightingHardwareRayTracing(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing"),
	1,
	TEXT("Enables hardware ray tracing for Lumen direct lighting (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenSceneDirectLightingHardwareRayTracingIndirect(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing.Indirect"),
	1,
	TEXT("Enables indirect dispatch for hardware ray tracing (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarLumenSceneDirectLightingHardwareRayTracingGroupCount(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing.GroupCount"),
	8192,
	TEXT("Determines the dispatch group count\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarLumenSceneDirectLightingHardwareRayTracingHeightfieldProjectionBias(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing.HeightfieldProjectionBias"),
	0,
	TEXT("Applies a projection bias such that an occlusion ray starts on the ray-tracing heightfield representation.\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<float> CVarLumenSceneDirectLightingHardwareRayTracingHeightfieldProjectionBiasSearchRadius(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing.HeightfieldProjectionBiasSearchRadius"),
	256,
	TEXT("Determines the search radius for heightfield projection bias. Larger search radius corresponds to increased traversal cost (default = 256).\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
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
	class FIndirectDispatchDim : SHADER_PERMUTATION_BOOL("DIM_INDIRECT_DISPATCH");
	using FPermutationDomain = TShaderPermutationDomain<FEnableFarFieldTracing, FEnableHeightfieldProjectionBias, FIndirectDispatchDim>;

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
		SHADER_PARAMETER(int, MaxTranslucentSkipCount)
		SHADER_PARAMETER(uint32, MaxTraversalIterations)
		SHADER_PARAMETER(uint32, GroupCount)
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

bool IsHardwareRayTracedDirectLightingIndirectDispatch()
{
	return GRHISupportsRayTracingDispatchIndirect && (CVarLumenSceneDirectLightingHardwareRayTracingIndirect.GetValueOnRenderThread() == 1);
}

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
		PermutationVector.Set<FLumenDirectLightingHardwareRayTracingBatchedRGS::FIndirectDispatchDim>(IsHardwareRayTracedDirectLightingIndirectDispatch());
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
	Parameters->MaxTranslucentSkipCount = Lumen::GetMaxTranslucentSkipCount();
	Parameters->MaxTraversalIterations = LumenHardwareRayTracing::GetMaxTraversalIterations();
	Parameters->GroupCount = FMath::Max(CVarLumenSceneDirectLightingHardwareRayTracingGroupCount.GetValueOnRenderThread(), 1);
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
	const bool bIndirectDispatch = IsHardwareRayTracedDirectLightingIndirectDispatch() || bInlineRayTracing;

	checkf(ComputePassFlags != ERDGPassFlags::AsyncCompute || bInlineRayTracing, TEXT("Async Lumen HWRT is only supported for inline ray tracing"));

	FRDGBufferRef HardwareRayTracingIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.Reflection.CompactTracingIndirectArgs"));
	if (bIndirectDispatch)
	{
		FLumenDirectLightingHardwareRayTracingIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenDirectLightingHardwareRayTracingIndirectArgsCS::FParameters>();
		{
			PassParameters->DispatchLightTilesIndirectArgs = GraphBuilder.CreateSRV(ShadowTraceIndirectArgs, PF_R32_UINT);
			PassParameters->RWHardwareRayTracingIndirectArgs = GraphBuilder.CreateUAV(HardwareRayTracingIndirectArgsBuffer, PF_R32_UINT);
			PassParameters->OutputThreadGroupSize = bInlineRayTracing ? FLumenDirectLightingHardwareRayTracingBatchedCS::GetThreadGroupSize() : FLumenDirectLightingHardwareRayTracingBatchedRGS::GetThreadGroupSize();
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
	PermutationVector.Set<FLumenDirectLightingHardwareRayTracingBatchedRGS::FIndirectDispatchDim>(bIndirectDispatch);

	FIntPoint DispatchResolution = FIntPoint(Lumen::CardTileSize * Lumen::CardTileSize, PassParameters->GroupCount);
	FString Resolution = FString::Printf(TEXT("%ux%u"), DispatchResolution.X, DispatchResolution.Y);
	if (bIndirectDispatch)
	{
		Resolution = FString::Printf(TEXT("<indirect>"));
	}

	if (bInlineRayTracing)
	{
		TShaderRef<FLumenDirectLightingHardwareRayTracingBatchedCS> ComputeShader = View.ShaderMap->GetShader<FLumenDirectLightingHardwareRayTracingBatchedCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("LumenDirectLightingHardwareRayTracingCS %s", *Resolution),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			PassParameters->HardwareRayTracingIndirectArgs,
			0);
	}
	else
	{
		TShaderRef<FLumenDirectLightingHardwareRayTracingBatchedRGS> RayGenerationShader = View.ShaderMap->GetShader<FLumenDirectLightingHardwareRayTracingBatchedRGS>(PermutationVector);
		if (IsHardwareRayTracedDirectLightingIndirectDispatch())
		{
			AddLumenRayTraceDispatchIndirectPass(
				GraphBuilder,
				RDG_EVENT_NAME("LumenDirectLightingHardwareRayTracingRGS %s", *Resolution),
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
				RDG_EVENT_NAME("LumenDirectLightingHardwareRayTracingRGS %s", *Resolution),
				RayGenerationShader,
				PassParameters,
				DispatchResolution,
				View,
				bUseMinimalPayload);
		}
	}
#else
	unimplemented();
#endif // RHI_RAYTRACING
}