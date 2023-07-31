// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Implements an experimental ray tracing reflection rendering algorithm based on ray and material sorting.
 * The algorithm consists of several separate stages:
 * - Generate reflection rays based on GBuffer (sorted in tiles by direction). Sorting may be optional in the future, based on performance measurements.
 * - Trace screen space reflection rays and output validity mask to avoid tracing/shading full rays [TODO; currently always tracing full rays]
 * - Trace reflection rays using lightweight RayGen shader and output material IDs
 * - Sort material IDs
 * - Execute material shaders and produce "Reflection GBuffer" [TODO; all lighting currently done in material eval RGS]
 * - Apply lighting to produce the final reflection buffer [TODO; all lighting currently done in material eval RGS]
 * 
 * Other features that are currently not implemented, but may be in the future:
 * - Shadow maps instead of ray traced shadows
 * 
 * Features that will never be supported due to performance:
 * - Multi-bounce
 * - Multi-SPP
 * - Clearcoat (only approximation will be supported)
 * - Translucency
 **/

#include "RayTracing/RayTracingLighting.h"
#include "RayTracing/RayTracingDeferredMaterials.h"
#include "RayTracing/RayTracingReflections.h"
#include "RendererPrivate.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "SceneTextureParameters.h"
#include "ReflectionEnvironment.h"

#if RHI_RAYTRACING

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsGenerateRaysWithRGS(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.GenerateRaysWithRGS"),
	1,
	TEXT("Whether to generate reflection rays directly in RGS or in a separate compute shader (default: 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsGlossy(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.Glossy"),
	1,
	TEXT("Whether to use glossy reflections with GGX sampling or to force mirror-like reflections for performance (default: 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarRayTracingReflectionsAnyHitMaxRoughness(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.AnyHitMaxRoughness"),
	0.1,
	TEXT("Allows skipping AnyHit shader execution for rough reflection rays (default: 0.1)"),
	ECVF_RenderThreadSafe
);


static TAutoConsoleVariable<float> CVarRayTracingReflectionsSmoothBias(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.SmoothBias"),
	0.0,
	TEXT("Whether to bias reflections towards smooth / mirror-like directions. Improves performance, but is not physically based. (default: 0)\n")
	TEXT("The bias is implemented as a non-linear function, affecting low roughness values more than high roughness ones.\n")
	TEXT("Roughness values higher than this CVar value remain entirely unaffected.\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarRayTracingReflectionsMipBias(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.MipBias"),
	0.0,
	TEXT("Global texture mip bias applied during ray tracing material evaluation. (default: 0)\n")
	TEXT("Improves ray tracing reflection performance at the cost of lower resolution textures in reflections. Values are clamped to range [0..15].\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSpatialResolve(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.SpatialResolve"),
	1,
	TEXT("Whether to use a basic spatial resolve (denoising) filter on reflection output. Not compatible with regular screen space denoiser. (default: 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarRayTracingReflectionsSpatialResolveMaxRadius(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.SpatialResolve.MaxRadius"),
	8.0f,
	TEXT("Maximum radius in pixels of the native reflection image. Actual radius depends on output pixel roughness, rougher reflections using larger radius. (default: 8)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSpatialResolveNumSamples(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.SpatialResolve.NumSamples"),
	8,
	TEXT("Maximum number of screen space samples to take during spatial resolve step. More samples produces smoother output at higher GPU cost. Specialized shader is used for 4, 8, 12 and 16 samples. (default: 8)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarRayTracingReflectionsTemporalWeight(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.SpatialResolve.TemporalWeight"),
	0.95f, // Up to 95% of the reflection can come from history buffer, at least 5% always from current frame
	TEXT("Defines whether to perform temporal accumulation during reflection spatial resolve and how much weight to give to history. Valid values in range [0..1]. (default: 0.90)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsTemporalQuality(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.SpatialResolve.TemporalQuality"),
	2,
	TEXT("0: Disable temporal accumulation\n")
	TEXT("1: Tile-based temporal accumulation (low quality)\n")
	TEXT("2: Tile-based temporal accumulation with randomized tile offsets per frame (medium quality)\n")
	TEXT("(default: 2)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarRayTracingReflectionsHorizontalResolutionScale(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred.HorizontalResolutionScale"),
	1.0,
	TEXT("Reflection resolution scaling for the X axis between 0.25 and 4.0. Can only be used when spatial resolve is enabled. (default: 1)"),
	ECVF_RenderThreadSafe
);

namespace 
{
	struct FSortedReflectionRay
	{
		float  Origin[3];
		uint32 PixelCoordinates; // X in low 16 bits, Y in high 16 bits
		uint32 Direction[2]; // FP16
		float  Pdf;
		float  Roughness; // Only technically need 8 bits, the rest could be repurposed
	};

	struct FRayIntersectionBookmark
	{
		uint32 Data[2];
	};
} // anon namespace

class FGenerateReflectionRaysCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateReflectionRaysCS);
	SHADER_USE_PARAMETER_STRUCT(FGenerateReflectionRaysCS, FGlobalShader);

	class FWaveOps : SHADER_PERMUTATION_BOOL("DIM_WAVE_OPS");
	using FPermutationDomain = TShaderPermutationDomain<FWaveOps>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER(FIntPoint, RayTracingResolution)
	SHADER_PARAMETER(FIntPoint, TileAlignedResolution)
	SHADER_PARAMETER(float, ReflectionMaxNormalBias)
	SHADER_PARAMETER(float, ReflectionMaxRoughness)
	SHADER_PARAMETER(float, ReflectionSmoothBias)
	SHADER_PARAMETER(FVector2f, UpscaleFactor)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSortedReflectionRay>, RayBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>() && !RHISupportsWaveOperations(Parameters.Platform))
		{
			return false;
		}

		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 1024; // this shader generates rays and sorts them in 32x32 tiles using LDS
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
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
IMPLEMENT_GLOBAL_SHADER(FGenerateReflectionRaysCS, "/Engine/Private/RayTracing/RayTracingReflectionsGenerateRaysCS.usf", "GenerateReflectionRaysCS", SF_Compute);

class FRayTracingReflectionResolveCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionResolveCS);
	SHADER_USE_PARAMETER_STRUCT(FRayTracingReflectionResolveCS, FGlobalShader);

	// Static 
	class FNumSamples : SHADER_PERMUTATION_SPARSE_INT("DIM_NUM_SAMPLES", 0, 4, 8, 12, 16);

	using FPermutationDomain = TShaderPermutationDomain<FNumSamples>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, RayTracingBufferSize)
		SHADER_PARAMETER(FVector2f, UpscaleFactor)
		SHADER_PARAMETER(float, SpatialResolveMaxRadius)
		SHADER_PARAMETER(int, SpatialResolveNumSamples)
		SHADER_PARAMETER(float, ReflectionMaxRoughness)
		SHADER_PARAMETER(float, ReflectionSmoothBias)
		SHADER_PARAMETER(float, ReflectionHistoryWeight)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(uint32, ThreadIdOffset)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthBufferHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReflectionHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RawReflectionColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReflectionDenoiserData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ColorOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static FIntPoint GetGroupSize()
	{
		return FIntPoint(8, 8);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), GetGroupSize().X);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), GetGroupSize().Y);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRayTracingReflectionResolveCS, "/Engine/Private/RayTracing/RayTracingReflectionResolve.usf", "RayTracingReflectionResolveCS", SF_Compute);

class FRayTracingDeferredReflectionsRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingDeferredReflectionsRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingDeferredReflectionsRGS, FGlobalShader)

	class FDeferredMaterialMode : SHADER_PERMUTATION_ENUM_CLASS("DIM_DEFERRED_MATERIAL_MODE", EDeferredMaterialMode);
	class FGenerateRays : SHADER_PERMUTATION_BOOL("DIM_GENERATE_RAYS"); // Whether to generate rays in the RGS or in a separate CS
	class FAMDHitToken : SHADER_PERMUTATION_BOOL("DIM_AMD_HIT_TOKEN");
	using FPermutationDomain = TShaderPermutationDomain<FDeferredMaterialMode, FGenerateRays, FAMDHitToken>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, RayTracingResolution)
		SHADER_PARAMETER(FIntPoint, TileAlignedResolution)
		SHADER_PARAMETER(float, ReflectionMaxNormalBias)
		SHADER_PARAMETER(float, ReflectionMaxRoughness)
		SHADER_PARAMETER(float, ReflectionSmoothBias)
		SHADER_PARAMETER(float, AnyHitMaxRoughness)
		SHADER_PARAMETER(float, TextureMipBias)
		SHADER_PARAMETER(FVector2f, UpscaleFactor)
		SHADER_PARAMETER(int, ShouldDoDirectLighting)
		SHADER_PARAMETER(int, ShouldDoEmissiveAndIndirectLighting)
		SHADER_PARAMETER(int, ShouldDoReflectionCaptures)
		SHADER_PARAMETER(int, DenoisingOutputFormat)
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSortedReflectionRay>, RayBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FRayIntersectionBookmark>, BookmarkBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FDeferredMaterialPayload>, MaterialBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ReflectionDenoiserData)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FReflectionUniformParameters, ReflectionStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FRaytracingLightDataPacked, LightDataPacked)
		SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, Forward)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!ShouldCompileRayTracingShadersForProject(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDeferredMaterialMode>() == EDeferredMaterialMode::None)
		{
			return false;
		}

		if (PermutationVector.Get<FDeferredMaterialMode>() != EDeferredMaterialMode::Gather
			&& PermutationVector.Get<FGenerateRays>())
		{
			// DIM_GENERATE_RAYS only makes sense for "gather" mode
			return false;
		}

		if (PermutationVector.Get<FAMDHitToken>() && !(IsD3DPlatform(Parameters.Platform) && IsPCPlatform(Parameters.Platform)))
		{
			return false;
		}

		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DISPATCH_1D"), 1); // Always using 1D dispatches
		OutEnvironment.SetDefine(TEXT("ENABLE_TWO_SIDED_GEOMETRY"), 1); // Always using double-sided ray tracing for shadow rays
	}
};
IMPLEMENT_GLOBAL_SHADER(FRayTracingDeferredReflectionsRGS, "/Engine/Private/RayTracing/RayTracingDeferredReflections.usf", "RayTracingDeferredReflectionsRGS", SF_RayGen);

void FDeferredShadingSceneRenderer::PrepareRayTracingDeferredReflections(const FViewInfo& View, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (!ShouldRenderRayTracingReflections(View))
	{
		return;
	}

	FRayTracingDeferredReflectionsRGS::FPermutationDomain PermutationVector;

	const bool bGenerateRaysWithRGS = CVarRayTracingReflectionsGenerateRaysWithRGS.GetValueOnRenderThread() == 1;
	const bool bHitTokenEnabled = CanUseRayTracingAMDHitToken();

	PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FAMDHitToken>(bHitTokenEnabled);

	{
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Gather);
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FGenerateRays>(bGenerateRaysWithRGS);
		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingDeferredReflectionsRGS>(PermutationVector);
		OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
	}

	{
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Shade);
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FGenerateRays>(false); // shading is independent of how rays are generated
		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingDeferredReflectionsRGS>(PermutationVector);
		OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
	}
}

static void AddReflectionResolvePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FRayTracingDeferredReflectionsRGS::FParameters& CommonParameters,
	FRDGTextureRef DepthBufferHistory,
	FRDGTextureRef ReflectionHistory, float ReflectionHistoryWeight, const FVector4f& HistoryScreenPositionScaleBias,
	FRDGTextureRef RawReflectionColor,
	FRDGTextureRef ReflectionDenoiserData,
	FIntPoint RayTracingBufferSize,
	FIntPoint ResolvedOutputSize,
	FRDGTextureRef ColorOutput)
{
	auto* PassParameters = GraphBuilder.AllocParameters<FRayTracingReflectionResolveCS::FParameters>();
	PassParameters->RayTracingBufferSize           = RayTracingBufferSize;
	PassParameters->UpscaleFactor                  = CommonParameters.UpscaleFactor;
	PassParameters->SpatialResolveMaxRadius        = FMath::Clamp<float>(CVarRayTracingReflectionsSpatialResolveMaxRadius.GetValueOnRenderThread(), 0.0f, 32.0f);
	PassParameters->SpatialResolveNumSamples       = FMath::Clamp<int32>(CVarRayTracingReflectionsSpatialResolveNumSamples.GetValueOnRenderThread(), 1, 32);
	PassParameters->ReflectionMaxRoughness         = CommonParameters.ReflectionMaxRoughness;
	PassParameters->ReflectionSmoothBias           = CommonParameters.ReflectionSmoothBias;
	PassParameters->ReflectionHistoryWeight        = ReflectionHistoryWeight;
	PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
	PassParameters->ViewUniformBuffer              = CommonParameters.ViewUniformBuffer;
	PassParameters->SceneTextures                  = CommonParameters.SceneTextures;
	PassParameters->DepthBufferHistory             = DepthBufferHistory;
	PassParameters->ReflectionHistory              = ReflectionHistory;
	PassParameters->RawReflectionColor             = RawReflectionColor;
	PassParameters->ReflectionDenoiserData         = ReflectionDenoiserData;
	PassParameters->ColorOutput                    = GraphBuilder.CreateUAV(ColorOutput);

	// 
	const uint32 FrameIndex = View.ViewState ? View.ViewState->GetFrameIndex() : 0;
	static const uint32 Offsets[8] = { 7, 2, 0, 5, 3, 1, 4, 6 }; // Just a randomized list of offsets (added to DispatchThreadId in the shader)
	PassParameters->ThreadIdOffset = ReflectionHistoryWeight > 0 && CVarRayTracingReflectionsTemporalQuality.GetValueOnRenderThread() == 2 
		? Offsets[FrameIndex % UE_ARRAY_COUNT(Offsets)] : 0;

	FRayTracingReflectionResolveCS::FPermutationDomain PermutationVector;
	if ((PassParameters->SpatialResolveNumSamples % 4 == 0) && PassParameters->SpatialResolveNumSamples <= 16)
	{
		// Static unrolled loop
		PermutationVector.Set<FRayTracingReflectionResolveCS::FNumSamples>(PassParameters->SpatialResolveNumSamples);
	}
	else
	{
		// Dynamic loop
		PermutationVector.Set<FRayTracingReflectionResolveCS::FNumSamples>(0);
	}

	auto ComputeShader = View.ShaderMap->GetShader<FRayTracingReflectionResolveCS>(PermutationVector);
	ClearUnusedGraphResources(ComputeShader, PassParameters);

	FIntVector GroupCount;
	GroupCount.X = FMath::DivideAndRoundUp(ResolvedOutputSize.X, FRayTracingReflectionResolveCS::GetGroupSize().X);
	GroupCount.Y = FMath::DivideAndRoundUp(ResolvedOutputSize.Y, FRayTracingReflectionResolveCS::GetGroupSize().Y);
	GroupCount.Z = 1;
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RayTracingReflectionResolve"), ComputeShader, PassParameters, GroupCount);
}

void FDeferredShadingSceneRenderer::PrepareRayTracingDeferredReflectionsDeferredMaterial(const FViewInfo& View, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (!ShouldRenderRayTracingReflections(View))
	{
		return;
	}

	FRayTracingDeferredReflectionsRGS::FPermutationDomain PermutationVector;

	const bool bGenerateRaysWithRGS = CVarRayTracingReflectionsGenerateRaysWithRGS.GetValueOnRenderThread() == 1;
	const bool bHitTokenEnabled = CanUseRayTracingAMDHitToken();

	PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FAMDHitToken>(bHitTokenEnabled);
	PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Gather);
	PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FGenerateRays>(bGenerateRaysWithRGS);
	auto RayGenShader = View.ShaderMap->GetShader<FRayTracingDeferredReflectionsRGS>(PermutationVector);
	OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());

}

static void AddGenerateReflectionRaysPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGBufferRef RayBuffer,
	const FRayTracingDeferredReflectionsRGS::FParameters& CommonParameters)
{
	FGenerateReflectionRaysCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateReflectionRaysCS::FParameters>();
	PassParameters->RayTracingResolution    = CommonParameters.RayTracingResolution;
	PassParameters->TileAlignedResolution   = CommonParameters.TileAlignedResolution;
	PassParameters->ReflectionMaxNormalBias = CommonParameters.ReflectionMaxNormalBias;
	PassParameters->ReflectionMaxRoughness  = CommonParameters.ReflectionMaxRoughness;
	PassParameters->ReflectionSmoothBias    = CommonParameters.ReflectionSmoothBias;
	PassParameters->UpscaleFactor           = CommonParameters.UpscaleFactor;
	PassParameters->ViewUniformBuffer       = CommonParameters.ViewUniformBuffer;
	PassParameters->SceneTextures           = CommonParameters.SceneTextures;
	PassParameters->RayBuffer               = GraphBuilder.CreateUAV(RayBuffer);

	FGenerateReflectionRaysCS::FPermutationDomain PermutationVector;
	const bool bUseWaveOps = GRHISupportsWaveOperations && GRHIMinimumWaveSize >= 32 && RHISupportsWaveOperations(View.GetShaderPlatform());
	PermutationVector.Set<FGenerateReflectionRaysCS::FWaveOps>(bUseWaveOps);

	auto ComputeShader = View.ShaderMap->GetShader<FGenerateReflectionRaysCS>(PermutationVector);
	ClearUnusedGraphResources(ComputeShader, PassParameters);

	const uint32 NumRays = CommonParameters.TileAlignedResolution.X * CommonParameters.TileAlignedResolution.Y;
	FIntVector GroupCount;
	GroupCount.X = FMath::DivideAndRoundUp(NumRays, FGenerateReflectionRaysCS::GetGroupSize());
	GroupCount.Y = 1;
	GroupCount.Z = 1;
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("GenerateReflectionRays"), ComputeShader, PassParameters, GroupCount);
}

void FDeferredShadingSceneRenderer::RenderRayTracingDeferredReflections(
	FRDGBuilder& GraphBuilder,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	int DenoiserMode,
	const FRayTracingReflectionOptions& Options,
	IScreenSpaceDenoiser::FReflectionsInputs* OutDenoiserInputs)
{
	const float ResolutionFraction = Options.ResolutionFraction;
	const bool bGenerateRaysWithRGS = CVarRayTracingReflectionsGenerateRaysWithRGS.GetValueOnRenderThread()==1;

	const bool bExternalDenoiser = DenoiserMode != 0;
	const bool bSpatialResolve   = !bExternalDenoiser && CVarRayTracingReflectionsSpatialResolve.GetValueOnRenderThread() == 1;
	const bool bTemporalResolve  = bSpatialResolve 
		&& CVarRayTracingReflectionsTemporalQuality.GetValueOnRenderThread() > 0
		&& CVarRayTracingReflectionsTemporalWeight.GetValueOnRenderThread() > 0;

	FVector2D UpscaleFactor = FVector2D(1.0f);
	FIntPoint RayTracingResolution = View.ViewRect.Size();
	FIntPoint RayTracingBufferSize = SceneTextures.SceneDepthTexture->Desc.Extent;

	if (bSpatialResolve && (ResolutionFraction != 1.0f || CVarRayTracingReflectionsHorizontalResolutionScale.GetValueOnAnyThread() != 1.0))
	{
		float ResolutionFractionX = FMath::Clamp(CVarRayTracingReflectionsHorizontalResolutionScale.GetValueOnAnyThread(), 0.25f, 4.0f);
		FVector2D ResolutionFloat = FVector2D::Max(FVector2D(4.0f), FVector2D(RayTracingResolution) * FVector2D(ResolutionFractionX, 1.0f) * ResolutionFraction);
		FVector2D BufferSizeFloat = FVector2D::Max(FVector2D(4.0f), FVector2D(RayTracingBufferSize) * FVector2D(ResolutionFractionX, 1.0f) * ResolutionFraction);

		RayTracingResolution.X = (int32)FMath::CeilToFloat(ResolutionFloat.X);
		RayTracingResolution.Y = (int32)FMath::CeilToFloat(ResolutionFloat.Y);

		RayTracingBufferSize.X = (int32)FMath::CeilToFloat(BufferSizeFloat.X);
		RayTracingBufferSize.Y = (int32)FMath::CeilToFloat(BufferSizeFloat.Y);

		UpscaleFactor = FVector2D(View.ViewRect.Size()) / FVector2D(RayTracingResolution);
	}
	else
	{
		int32 UpscaleFactorInt = int32(1.0f / ResolutionFraction);
		RayTracingResolution = FIntPoint::DivideAndRoundUp(RayTracingResolution, UpscaleFactorInt);
		RayTracingBufferSize = RayTracingBufferSize / UpscaleFactorInt;
		UpscaleFactor = FVector2D((float)UpscaleFactorInt);
	}

	FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
		RayTracingBufferSize,
		PF_FloatRGBA, FClearValueBinding(FLinearColor(0, 0, 0, 0)),
		TexCreate_ShaderResource | TexCreate_UAV);

	OutDenoiserInputs->Color = GraphBuilder.CreateTexture(OutputDesc,
		bSpatialResolve
		? TEXT("RayTracingReflectionsRaw")
		: TEXT("RayTracingReflections"));

	FRDGTextureRef ReflectionDenoiserData;
	if (bSpatialResolve)
	{
		OutputDesc.Format      = PF_FloatRGBA;
		ReflectionDenoiserData = GraphBuilder.CreateTexture(OutputDesc, TEXT("RayTracingReflectionsSpatialResolveData"));
	}
	else
	{
		OutputDesc.Format                  = PF_R16F;
		ReflectionDenoiserData             = GraphBuilder.CreateTexture(OutputDesc, TEXT("RayTracingReflectionsHitDistance"));
		OutDenoiserInputs->RayHitDistance  = ReflectionDenoiserData;
	}

	const uint32 SortTileSize             = 64; // Ray sort tile is 32x32, material sort tile is 64x64, so we use 64 here (tile size is not configurable).
	const FIntPoint TileAlignedResolution = FIntPoint::DivideAndRoundUp(RayTracingResolution, SortTileSize) * SortTileSize;

	FRayTracingDeferredReflectionsRGS::FParameters CommonParameters;
	CommonParameters.UpscaleFactor           = FVector2f(UpscaleFactor); // LWC_TODO: Precision loss
	CommonParameters.RayTracingResolution    = RayTracingResolution;
	CommonParameters.TileAlignedResolution   = TileAlignedResolution;
	CommonParameters.ReflectionMaxRoughness  = Options.MaxRoughness;
	CommonParameters.ReflectionSmoothBias    = CVarRayTracingReflectionsGlossy.GetValueOnRenderThread() ? CVarRayTracingReflectionsSmoothBias.GetValueOnRenderThread() : -1;
	CommonParameters.AnyHitMaxRoughness      = CVarRayTracingReflectionsAnyHitMaxRoughness.GetValueOnRenderThread();
	CommonParameters.TextureMipBias          = FMath::Clamp(CVarRayTracingReflectionsMipBias.GetValueOnRenderThread(), 0.0f, 15.0f);

	CommonParameters.ShouldDoDirectLighting              = Options.bDirectLighting;
	CommonParameters.ShouldDoEmissiveAndIndirectLighting = Options.bEmissiveAndIndirectLighting;
	CommonParameters.ShouldDoReflectionCaptures          = Options.bReflectionCaptures;

	CommonParameters.DenoisingOutputFormat               = bSpatialResolve ? 1 : 0;

	CommonParameters.TLAS                    = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
	CommonParameters.SceneTextures           = SceneTextures;
	CommonParameters.ViewUniformBuffer       = View.ViewUniformBuffer;
	CommonParameters.LightDataPacked         = View.RayTracingLightDataUniformBuffer;
	CommonParameters.ReflectionStruct        = CreateReflectionUniformBuffer(View, EUniformBufferUsage::UniformBuffer_SingleFrame);
	CommonParameters.ReflectionCapture       = View.ReflectionCaptureUniformBuffer;
	CommonParameters.Forward                 = View.ForwardLightingResources.ForwardLightUniformBuffer;
	CommonParameters.ReflectionMaxNormalBias = GetRaytracingMaxNormalBias();

	if (!CommonParameters.SceneTextures.GBufferVelocityTexture)
	{
		CommonParameters.SceneTextures.GBufferVelocityTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
	}

	const bool bHitTokenEnabled = CanUseRayTracingAMDHitToken();

	// Generate sorted reflection rays

	const uint32 TileAlignedNumRays          = TileAlignedResolution.X * TileAlignedResolution.Y;
	const FRDGBufferDesc SortedRayBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FSortedReflectionRay), TileAlignedNumRays);
	FRDGBufferRef SortedRayBuffer            = GraphBuilder.CreateBuffer(SortedRayBufferDesc, TEXT("ReflectionRayBuffer"));

	const FRDGBufferDesc DeferredMaterialBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FDeferredMaterialPayload), TileAlignedNumRays);
	FRDGBufferRef DeferredMaterialBuffer            = GraphBuilder.CreateBuffer(DeferredMaterialBufferDesc, TEXT("RayTracingReflectionsMaterialBuffer"));

	const FRDGBufferDesc BookmarkBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FRayIntersectionBookmark), TileAlignedNumRays);
	FRDGBufferRef BookmarkBuffer            = GraphBuilder.CreateBuffer(BookmarkBufferDesc, TEXT("RayTracingReflectionsBookmarkBuffer"));

	if (!bGenerateRaysWithRGS)
	{
		AddGenerateReflectionRaysPass(GraphBuilder, View, SortedRayBuffer, CommonParameters);
	}

	// Trace reflection material gather rays

	{
		FRayTracingDeferredReflectionsRGS::FParameters& PassParameters = *GraphBuilder.AllocParameters<FRayTracingDeferredReflectionsRGS::FParameters>();
		PassParameters                        = CommonParameters;
		PassParameters.MaterialBuffer         = GraphBuilder.CreateUAV(DeferredMaterialBuffer);
		PassParameters.RayBuffer              = GraphBuilder.CreateUAV(SortedRayBuffer);
		PassParameters.BookmarkBuffer         = GraphBuilder.CreateUAV(BookmarkBuffer);
		PassParameters.ColorOutput            = GraphBuilder.CreateUAV(OutDenoiserInputs->Color);
		PassParameters.ReflectionDenoiserData = GraphBuilder.CreateUAV(ReflectionDenoiserData);

		FRayTracingDeferredReflectionsRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FAMDHitToken>(bHitTokenEnabled);
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Gather);
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FGenerateRays>(bGenerateRaysWithRGS);

		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingDeferredReflectionsRGS>(PermutationVector);
		ClearUnusedGraphResources(RayGenShader, &PassParameters);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("RayTracingDeferredReflectionsGather %dx%d", RayTracingResolution.X, RayTracingResolution.Y),
			&PassParameters,
			ERDGPassFlags::Compute,
		[&PassParameters, this, &View, TileAlignedNumRays, RayGenShader](FRHIRayTracingCommandList& RHICmdList)
		{
			FRayTracingPipelineState* Pipeline = View.RayTracingMaterialGatherPipeline;

			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenShader, PassParameters);
			FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
			RHICmdList.RayTraceDispatch(Pipeline, RayGenShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, TileAlignedNumRays, 1);
		});
	}

	// Sort hit points by material within 64x64 (4096 element) tiles

	SortDeferredMaterials(GraphBuilder, View, 5, TileAlignedNumRays, DeferredMaterialBuffer);

	// Shade reflection points

	{
		FRayTracingDeferredReflectionsRGS::FParameters& PassParameters = *GraphBuilder.AllocParameters<FRayTracingDeferredReflectionsRGS::FParameters>();
		PassParameters                        = CommonParameters;
		PassParameters.MaterialBuffer         = GraphBuilder.CreateUAV(DeferredMaterialBuffer);
		PassParameters.RayBuffer              = GraphBuilder.CreateUAV(SortedRayBuffer);
		PassParameters.BookmarkBuffer         = GraphBuilder.CreateUAV(BookmarkBuffer);
		PassParameters.ColorOutput            = GraphBuilder.CreateUAV(OutDenoiserInputs->Color);
		PassParameters.ReflectionDenoiserData = GraphBuilder.CreateUAV(ReflectionDenoiserData);

		FRayTracingDeferredReflectionsRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FAMDHitToken>(bHitTokenEnabled);
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Shade);
		PermutationVector.Set<FRayTracingDeferredReflectionsRGS::FGenerateRays>(false);

		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingDeferredReflectionsRGS>(PermutationVector);
		ClearUnusedGraphResources(RayGenShader, &PassParameters);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("RayTracingDeferredReflectionsShade %dx%d", RayTracingResolution.X, RayTracingResolution.Y),
			&PassParameters,
			ERDGPassFlags::Compute,
		[&PassParameters, &View, TileAlignedNumRays, RayGenShader](FRHIRayTracingCommandList& RHICmdList)
		{
			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenShader, PassParameters);
			FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
			RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, TileAlignedNumRays, 1);
		});
	}

	// Apply basic reflection spatial resolve filter to reduce noise
	if (bSpatialResolve)
	{
		FRDGTextureDesc ResolvedOutputDesc = FRDGTextureDesc::Create2D(
			SceneTextures.SceneDepthTexture->Desc.Extent, // full res buffer
			PF_FloatRGBA, FClearValueBinding(FLinearColor(0, 0, 0, 0)), 
			TexCreate_ShaderResource | TexCreate_UAV);

		FRDGTextureRef RawReflectionColor = OutDenoiserInputs->Color;
		OutDenoiserInputs->Color = GraphBuilder.CreateTexture(ResolvedOutputDesc, TEXT("RayTracingReflections"));

		const FScreenSpaceDenoiserHistory& ReflectionsHistory = View.PrevViewInfo.ReflectionsHistory;

		const bool bValidHistory = ReflectionsHistory.IsValid() && !View.bCameraCut && bTemporalResolve;

		FRDGTextureRef DepthBufferHistoryTexture = GraphBuilder.RegisterExternalTexture(
			bValidHistory && View.PrevViewInfo.DepthBuffer.IsValid()
			? View.PrevViewInfo.DepthBuffer
			: GSystemTextures.BlackDummy);

		FRDGTextureRef ReflectionHistoryTexture = GraphBuilder.RegisterExternalTexture(
			bValidHistory 
			? ReflectionsHistory.RT[0] 
			: GSystemTextures.BlackDummy);

		const float HistoryWeight = bValidHistory
			? FMath::Clamp(CVarRayTracingReflectionsTemporalWeight.GetValueOnRenderThread(), 0.0f, 0.99f)
			: 0.0;

		FIntPoint ViewportOffset = View.ViewRect.Min;
		FIntPoint ViewportExtent = View.ViewRect.Size();
		FIntPoint BufferSize     = SceneTextures.SceneDepthTexture->Desc.Extent;

		if (bValidHistory)
		{
			ViewportOffset = ReflectionsHistory.Scissor.Min;
			ViewportExtent = ReflectionsHistory.Scissor.Size();
			BufferSize     = ReflectionsHistory.RT[0]->GetDesc().Extent;
		}

		FVector2D InvBufferSize(1.0f / float(BufferSize.X), 1.0f / float(BufferSize.Y));

		FVector4f HistoryScreenPositionScaleBias = FVector4f(
			ViewportExtent.X * 0.5f * InvBufferSize.X,
			-ViewportExtent.Y * 0.5f * InvBufferSize.Y,
			(ViewportExtent.X * 0.5f + ViewportOffset.X) * InvBufferSize.X,
			(ViewportExtent.Y * 0.5f + ViewportOffset.Y) * InvBufferSize.Y);

		AddReflectionResolvePass(GraphBuilder, View, CommonParameters,
			DepthBufferHistoryTexture,
			ReflectionHistoryTexture, HistoryWeight, HistoryScreenPositionScaleBias,
			RawReflectionColor,
			ReflectionDenoiserData,
			RayTracingBufferSize,
			View.ViewRect.Size(),
			OutDenoiserInputs->Color);

		if (bTemporalResolve && View.ViewState)
		{
			GraphBuilder.QueueTextureExtraction(SceneTextures.SceneDepthTexture, &View.ViewState->PrevFrameViewInfo.DepthBuffer);
			GraphBuilder.QueueTextureExtraction(OutDenoiserInputs->Color, &View.ViewState->PrevFrameViewInfo.ReflectionsHistory.RT[0]);
			View.ViewState->PrevFrameViewInfo.ReflectionsHistory.Scissor = View.ViewRect;
		}
	}

}
#else // RHI_RAYTRACING
void FDeferredShadingSceneRenderer::RenderRayTracingDeferredReflections(
	FRDGBuilder& GraphBuilder,
	const FSceneTextureParameters& SceneTextures,
	const FViewInfo& View,
	int DenoiserMode,
	const FRayTracingReflectionOptions& Options,
	IScreenSpaceDenoiser::FReflectionsInputs* OutDenoiserInputs)
{
	checkNoEntry();
}
#endif // RHI_RAYTRACING
