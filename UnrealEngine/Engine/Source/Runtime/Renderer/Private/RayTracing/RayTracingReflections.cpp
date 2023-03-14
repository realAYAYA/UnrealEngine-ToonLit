// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingReflections.h"
#include "RendererPrivate.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"

#if RHI_RAYTRACING

#include "ClearQuad.h"
#include "SceneRendering.h"
#include "PostProcess/SceneRenderTargets.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "VisualizeTexture.h"
#include "LightRendering.h"
#include "SystemTextures.h"
#include "RayTracing/RaytracingOptions.h"
#include "RayTracing/RayTracingDeferredMaterials.h"
#include "RayTracing/RayTracingLighting.h"
#include "RenderGraph.h"
#include "RayTracing/RayTracingLighting.h"
#include "RayTracing/RayTracingSkyLight.h"
#include "SceneTextureParameters.h"
#include "PathTracing.h"

static int32 GRayTracingReflections = -1;
static FAutoConsoleVariableRef CVarReflectionsMethod(
	TEXT("r.RayTracing.Reflections"),
	GRayTracingReflections,
	TEXT("-1: Value driven by postprocess volume (default) \n")
	TEXT("0: use traditional rasterized SSR\n")
	TEXT("1: use ray traced reflections\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static TAutoConsoleVariable<float> CVarReflectionScreenPercentage(
	TEXT("r.RayTracing.Reflections.ScreenPercentage"),
	100.0f,
	TEXT("Screen percentage the reflections should be ray traced at (default = 100)."),
	ECVF_RenderThreadSafe);

static int32 GRayTracingReflectionsSamplesPerPixel = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsSamplesPerPixel(
	TEXT("r.RayTracing.Reflections.SamplesPerPixel"),
	GRayTracingReflectionsSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for reflections (default = -1 (driven by postprocesing volume))"),
	ECVF_RenderThreadSafe
);

static float GRayTracingReflectionsMaxRoughness = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMaxRoughness(
	TEXT("r.RayTracing.Reflections.MaxRoughness"),
	GRayTracingReflectionsMaxRoughness,
	TEXT("Sets the maximum roughness until which ray tracing reflections will be visible (default = -1 (max roughness driven by postprocessing volume))"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingReflectionsMaxBounces = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMaxBounces(
	TEXT("r.RayTracing.Reflections.MaxBounces"),
	GRayTracingReflectionsMaxBounces,
	TEXT("Sets the maximum number of ray tracing reflection bounces (default = -1 (max bounces driven by postprocessing volume))"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingReflectionsEmissiveAndIndirectLighting = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsEmissiveAndIndirectLighting(
	TEXT("r.RayTracing.Reflections.EmissiveAndIndirectLighting"),
	GRayTracingReflectionsEmissiveAndIndirectLighting,
	TEXT("Enables ray tracing reflections emissive and indirect lighting (default = 1)"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingReflectionsDirectLighting = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsDirectLighting(
	TEXT("r.RayTracing.Reflections.DirectLighting"),
	GRayTracingReflectionsDirectLighting,
	TEXT("Enables ray tracing reflections direct lighting (default = 1)"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingReflectionsShadows = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsShadows(
	TEXT("r.RayTracing.Reflections.Shadows"),
	GRayTracingReflectionsShadows,
	TEXT("Enables shadows in ray tracing reflections)")
	TEXT(" -1: Shadows driven by postprocessing volume (default)")
	TEXT(" 0: Shadows disabled ")
	TEXT(" 1: Hard shadows")
	TEXT(" 2: Soft area shadows"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingReflectionsTranslucency = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsTranslucency(
	TEXT("r.RayTracing.Reflections.Translucency"),
	GRayTracingReflectionsTranslucency,
	TEXT("Translucent objects visible in ray tracing reflections)")
	TEXT(" -1: Driven by postprocessing volume (default)")
	TEXT(" 0: Translucent objects not visible")
	TEXT(" 1: Translucent objects visible"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingReflectionsCaptures = 0;
static FAutoConsoleVariableRef CVarRayTracingReflectionsCaptures(
	TEXT("r.RayTracing.Reflections.ReflectionCaptures"),
	GRayTracingReflectionsCaptures,
	TEXT("Enables ray tracing reflections to use reflection captures as the last bounce reflection. Particularly usefull for metals in reflection. (default = 0)"),
	ECVF_RenderThreadSafe
);

static float GRayTracingReflectionsMinRayDistance = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMinRayDistance(
	TEXT("r.RayTracing.Reflections.MinRayDistance"),
	GRayTracingReflectionsMinRayDistance,
	TEXT("Sets the minimum ray distance for ray traced reflection rays. Actual reflection ray length is computed as Lerp(MaxRayDistance, MinRayDistance, Roughness), i.e. reflection rays become shorter when traced from rougher surfaces. (default = -1 (infinite rays))"),
	ECVF_RenderThreadSafe
);

static float GRayTracingReflectionsMaxRayDistance = -1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsMaxRayDistance(
	TEXT("r.RayTracing.Reflections.MaxRayDistance"),
	GRayTracingReflectionsMaxRayDistance,
	TEXT("Sets the maximum ray distance for ray traced reflection rays. When ray shortening is used, skybox will not be sampled in RT reflection pass and will be composited later, together with local reflection captures. Negative values turn off this optimization. (default = -1 (infinite rays))"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingReflectionsHeightFog = 1;
static FAutoConsoleVariableRef CVarRayTracingReflectionsHeightFog(
	TEXT("r.RayTracing.Reflections.HeightFog"),
	GRayTracingReflectionsHeightFog,
	TEXT("Enables height fog in ray traced reflections (default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSortMaterials(
	TEXT("r.RayTracing.Reflections.SortMaterials"),
	1,
	TEXT("Sets whether refected materials will be sorted before shading\n")
	TEXT("0: Disabled\n ")
	TEXT("1: Enabled, using Trace->Sort->Trace (Default)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsHybrid(
	TEXT("r.RayTracing.Reflections.Hybrid"),
	0,
	TEXT("Sets whether screen space reflections should be used when possible (experimental).\n")
	TEXT("Forces material sorting and single ray bounce.\n")
	TEXT("0: Disabled (Default)\n ")
	TEXT("1: Enabled\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSortTileSize(
	TEXT("r.RayTracing.Reflections.SortTileSize"),
	64,
	TEXT("Size of pixel tiles for sorted reflections\n")
	TEXT("  Default 64\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsRenderTileSize(
	TEXT("r.RayTracing.Reflections.RenderTileSize"),
	0,
	TEXT("Render ray traced reflections in NxN pixel tiles, where each tile is submitted as separate GPU command buffer, allowing high quality rendering without triggering timeout detection (default = 0, tiling disabled)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsSortSize(
	TEXT("r.RayTracing.Reflections.SortSize"),
	5,
	TEXT("Size of horizon for material ID sort\n")
	TEXT("0: Disabled\n")
	TEXT("1: 256 Elements\n")
	TEXT("2: 512 Elements\n")
	TEXT("3: 1024 Elements\n")
	TEXT("4: 2048 Elements\n")
	TEXT("5: 4096 Elements (Default)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsTestPathRoughness(
	TEXT("r.RayTracing.Reflections.TestPathRoughness"),
	1,
	TEXT("Accumulate roughness along path and test accumulated roughness against MaxRoughness before launching the next bounce (default 1)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarRayTracingReflectionsMinClearCoatLevel(
	TEXT("r.RayTracing.Reflections.MinClearCoatLevel"),
	0.01f,
	TEXT("Minimum level at which to apply clear coat shading (default 0.01)\n")
	TEXT(" Note: causes some variation in height fog due to using the bottom layer path"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsMaxUnderCoatBounces(
	TEXT("r.RayTracing.Reflections.MaxUnderCoatBounces"),
	0,
	TEXT("How many bounces to apply ray traced reflections to the undercoat layer. Extra bounces will use reflection probes. (default 0, always use probes)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsRayTraceSkyLightContribution(
	TEXT("r.RayTracing.Reflections.RayTraceSkyLightContribution"),
	0,
	TEXT("Requests ray tracing reflections to use ray traced visibility rays for sky light contribution similar to sky light ray traced shadows. A Sky Light with ray traced shadows enabled must be present for this flag to take effect. (default = 0)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsExperimentalDeferred(
	TEXT("r.RayTracing.Reflections.ExperimentalDeferred"),
	0,
	TEXT("Whether to use the experimental deferred ray traced reflection rendering algorithm, which only supports a subset of features but runs faster. (default = 0)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarRayTracingReflectionsNormalBias(
	TEXT("r.RayTracing.Reflections.NormalBias"),
	0.1,
	TEXT("Magnitude of normal bias for reflection rays. (default = 0.1)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingReflectionsEnableTwoSidedGeometry(
	TEXT("r.RayTracing.Reflections.EnableTwoSidedGeometry"),
	1,
	TEXT("Two-sided geometry setting for reflection rays. (default = 1)"),
	ECVF_RenderThreadSafe);

// ESamplePhase
enum class ESamplePhase
{
	Monlithic = 0, //single pass for all samples
	Init = 1,      // First sample of the set initialize the accumulators
	Accum = 2,     // Intermediate sample, accumulate results
	Resolve = 3    // Final sample, apply weighting
};

// Counterpart for FImaginaryReflectionGBufferData in RayTracingReflectionsCommon.ush
struct FImaginaryReflectionGBufferData
{
	float WorldNormal[3];
	float SceneDepth;
	float Velocity[2];
	uint32 ValidSamples;
};

static const int32 GReflectionLightCountMaximum = 64;

class FRayTracingReflectionsRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingReflectionsRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingReflectionsRGS, FGlobalShader)

	class FDenoiserOutput : SHADER_PERMUTATION_BOOL("DIM_DENOISER_OUTPUT");

	class FDeferredMaterialMode : SHADER_PERMUTATION_ENUM_CLASS("DIM_DEFERRED_MATERIAL_MODE", EDeferredMaterialMode);
	class FHybrid : SHADER_PERMUTATION_BOOL("DIM_HYBRID");
	class FEnableTwoSidedGeometryForShadowDim : SHADER_PERMUTATION_BOOL("ENABLE_TWO_SIDED_GEOMETRY");
	class FRayTraceSkyLightContribution : SHADER_PERMUTATION_BOOL("DIM_RAY_TRACE_SKY_LIGHT_CONTRIBUTION");

	using FPermutationDomain = TShaderPermutationDomain<
		FDenoiserOutput,
		FDeferredMaterialMode,
		FHybrid,
		FEnableTwoSidedGeometryForShadowDim,
		FRayTraceSkyLightContribution>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, SamplesPerPixel)
		SHADER_PARAMETER(int32, MaxBounces)
		SHADER_PARAMETER(int32, HeightFog)
		SHADER_PARAMETER(int32, UseReflectionCaptures)
		SHADER_PARAMETER(int32, ShouldDoDirectLighting)
		SHADER_PARAMETER(int32, ReflectedShadowsType)
		SHADER_PARAMETER(int32, ShouldDoEmissiveAndIndirectLighting)
		SHADER_PARAMETER(int32, ShouldReflectOnlyWater)
		SHADER_PARAMETER(int32, UpscaleFactor)
		SHADER_PARAMETER(int32, SortTileSize)
		SHADER_PARAMETER(FIntPoint, RayTracingResolution)
		SHADER_PARAMETER(FIntPoint, TileAlignedResolution)
		SHADER_PARAMETER(float, ReflectionMinRayDistance)
		SHADER_PARAMETER(float, ReflectionMaxRayDistance)
		SHADER_PARAMETER(float, ReflectionMaxRoughness)
		SHADER_PARAMETER(float, ReflectionMaxNormalBias)
		SHADER_PARAMETER(float, ShadowMaxNormalBias)
		SHADER_PARAMETER(int32, ReflectionEnableTwoSidedGeometry)
		SHADER_PARAMETER(int32, TestPathRoughness)
		SHADER_PARAMETER(float, MinClearCoatLevel)
		SHADER_PARAMETER(int32, MaxUnderCoatBounces)
		SHADER_PARAMETER(uint32, RenderTileOffsetX)
		SHADER_PARAMETER(uint32, RenderTileOffsetY)
		SHADER_PARAMETER(uint32, EnableTranslucency)
		SHADER_PARAMETER(int32, SkyLightDecoupleSampleGeneration)
		SHADER_PARAMETER(int32, SampleMode)
		SHADER_PARAMETER(int32, SampleOffset)

		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColor)

		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSkyLightVisibilityRaysData, SkyLightVisibilityRaysData)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FRaytracingLightDataPacked, LightDataPacked)
		SHADER_PARAMETER_STRUCT_REF(FReflectionUniformParameters, ReflectionStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FFogUniformParameters, FogUniformParameters)
		SHADER_PARAMETER_STRUCT_REF(FReflectionCaptureShaderData, ReflectionCapture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, Forward)
		SHADER_PARAMETER_STRUCT_REF(FSkyLightData, SkyLightData)
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingSkylight, SkylightParameters)

		// Optional indirection buffer used for sorted materials
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FDeferredMaterialPayload>, MaterialBuffer)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RayHitDistanceOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RayImaginaryDepthOutput)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FImaginaryReflectionGBufferData>, ImaginaryReflectionGBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform)
			&& FDataDrivenShaderPlatformInfo::GetSupportsHighEndRayTracingReflections(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FDeferredMaterialMode>() == EDeferredMaterialMode::Shade)
		{
			OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_DISPATCH_1D"), 1);
		}

		if (PermutationVector.Get<FDeferredMaterialMode>() == EDeferredMaterialMode::Gather)
		{
			OutEnvironment.SetDefine(TEXT("UE_RAY_TRACING_LIGHTWEIGHT_CLOSEST_HIT_SHADER"), 1);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingReflectionsRGS, "/Engine/Private/RayTracing/RayTracingReflections.usf", "RayTracingReflectionsRGS", SF_RayGen);

class FSplitImaginaryReflectionGBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSplitImaginaryReflectionGBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FSplitImaginaryReflectionGBufferCS, FGlobalShader);

	using FPermutationDomain = FShaderPermutationNone;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform)
			&& FDataDrivenShaderPlatformInfo::GetSupportsHighEndRayTracingReflections(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return FComputeShaderUtils::kGolden2DGroupSize;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)

		SHADER_PARAMETER(int32, UpscaleFactor)
		SHADER_PARAMETER(FIntPoint, RayTracingResolution)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FImaginaryReflectionGBufferData>, ImaginaryReflectionGBuffer)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ImaginaryReflectionGBufferA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, ImaginaryReflectionDepthZ)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, ImaginaryReflectionVelocity)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSplitImaginaryReflectionGBufferCS, "/Engine/Private/RayTracing/SplitImaginaryReflectionGBufferCS.usf", "MainCS", SF_Compute);

static int32 GetRayTracingReflectionsSamplesPerPixel(const FViewInfo& View)
{
	return GRayTracingReflectionsSamplesPerPixel >= 0 ? GRayTracingReflectionsSamplesPerPixel : View.FinalPostProcessSettings.RayTracingReflectionsSamplesPerPixel;
}

static float GetRayTracingReflectionsMaxRoughness(const FViewInfo& View)
{
	return FMath::Clamp(GRayTracingReflectionsMaxRoughness >= 0 ? GRayTracingReflectionsMaxRoughness : View.FinalPostProcessSettings.RayTracingReflectionsMaxRoughness, 0.01f, 1.0f);
}

bool ShouldRenderRayTracingReflections(const FViewInfo& View)
{
	const bool bThisViewHasRaytracingReflections = View.FinalPostProcessSettings.ReflectionMethod == EReflectionMethod::RayTraced;
	
	const bool bReflectionsCvarEnabled = GRayTracingReflections < 0 
		? bThisViewHasRaytracingReflections 
		: GRayTracingReflections != 0;

	const bool bReflectionPassEnabled = bReflectionsCvarEnabled && (GetRayTracingReflectionsSamplesPerPixel(View) > 0) && !View.bIsReflectionCapture && !Strata::IsStrataEnabled();
		
	return ShouldRenderRayTracingEffect(bReflectionPassEnabled, ERayTracingPipelineCompatibilityFlags::FullPipeline, &View);
}

static bool ShouldRayTracedReflectionsUseHybridReflections()
{
	return CVarRayTracingReflectionsHybrid.GetValueOnRenderThread() != 0;
}

static bool ShouldRayTracedReflectionsSortMaterials(const FViewInfo& View)
{
	return (ShouldRayTracedReflectionsUseHybridReflections() || CVarRayTracingReflectionsSortMaterials.GetValueOnRenderThread() != 0);
}

static bool ShouldRayTracedReflectionsUseSortedDeferredAlgorithm(const FViewInfo& View)
{
	// Deferred reflections are always used if platform does not support the high-end version
	return !FDataDrivenShaderPlatformInfo::GetSupportsHighEndRayTracingReflections(View.GetShaderPlatform())
		|| (CVarRayTracingReflectionsExperimentalDeferred.GetValueOnRenderThread() != 0);
}

static bool ShouldRayTracedReflectionsRayTraceSkyLightContribution(const FScene& Scene)
{
	// Only ray trace sky light contribution when the ray traced sky light should be rendered in normal conditions (sky light exists, ray traced shadows enabled)
	return CVarRayTracingReflectionsRayTraceSkyLightContribution.GetValueOnRenderThread() && ShouldRenderRayTracingSkyLight(Scene.SkyLight);
}

void FDeferredShadingSceneRenderer::PrepareRayTracingReflections(const FViewInfo& View, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	// Declare all RayGen shaders that require material closest hit shaders to be bound

	if (!ShouldRenderRayTracingReflections(View))
	{
		return;
	}

	if (ShouldRayTracedReflectionsUseSortedDeferredAlgorithm(View))
	{
		// If deferred reflections technique is used, then we only need to gather its shaders and skip the rest.
		PrepareRayTracingDeferredReflections(View, Scene, OutRayGenShaders);
		return;
	}

	const bool bHybridReflections = ShouldRayTracedReflectionsUseHybridReflections();
	const bool bSortMaterials = ShouldRayTracedReflectionsSortMaterials(View);
	const bool bRayTraceSkyLightContribution = ShouldRayTracedReflectionsRayTraceSkyLightContribution(Scene);

	if (bSortMaterials)
	{
		{
			FRayTracingReflectionsRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRayTracingReflectionsRGS::FEnableTwoSidedGeometryForShadowDim>(EnableRayTracingShadowTwoSidedGeometry());
			PermutationVector.Set<FRayTracingReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Gather);
			PermutationVector.Set<FRayTracingReflectionsRGS::FHybrid>(bHybridReflections);
			PermutationVector.Set<FRayTracingReflectionsRGS::FRayTraceSkyLightContribution>(bRayTraceSkyLightContribution);
			auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRGS>(PermutationVector);
			OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
		}
		
		{
			FRayTracingReflectionsRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRayTracingReflectionsRGS::FEnableTwoSidedGeometryForShadowDim>(EnableRayTracingShadowTwoSidedGeometry());
			PermutationVector.Set<FRayTracingReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Shade);
			PermutationVector.Set<FRayTracingReflectionsRGS::FHybrid>(bHybridReflections);
			PermutationVector.Set<FRayTracingReflectionsRGS::FRayTraceSkyLightContribution>(bRayTraceSkyLightContribution);
			auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRGS>(PermutationVector);
			OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
		}
	}
	else
	{
		FRayTracingReflectionsRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingReflectionsRGS::FEnableTwoSidedGeometryForShadowDim>(EnableRayTracingShadowTwoSidedGeometry());
		PermutationVector.Set<FRayTracingReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::None);
		PermutationVector.Set<FRayTracingReflectionsRGS::FRayTraceSkyLightContribution>(bRayTraceSkyLightContribution);
		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRGS>(PermutationVector);
		OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
	}
}

void FDeferredShadingSceneRenderer::PrepareRayTracingReflectionsDeferredMaterial(const FViewInfo& View, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	// Declare all RayGen shaders that are used with deferred material sorts

	if (!ShouldRenderRayTracingReflections(View))
	{
		return;
	}

	if (CVarRayTracingReflectionsExperimentalDeferred.GetValueOnRenderThread())
	{
		// If deferred reflections technique is used, then we only need to gather its shaders and skip the rest.
		PrepareRayTracingDeferredReflectionsDeferredMaterial(View, Scene, OutRayGenShaders);
		return;
	}

	if (!ShouldRayTracedReflectionsSortMaterials(View))
	{
		return;
	}

	if (FDataDrivenShaderPlatformInfo::GetSupportsHighEndRayTracingReflections(View.GetShaderPlatform()))
	{
		const bool bHybridReflections = ShouldRayTracedReflectionsUseHybridReflections();
		const bool bRayTraceSkyLightContribution = ShouldRayTracedReflectionsRayTraceSkyLightContribution(Scene);

		FRayTracingReflectionsRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingReflectionsRGS::FEnableTwoSidedGeometryForShadowDim>(EnableRayTracingShadowTwoSidedGeometry());
		PermutationVector.Set<FRayTracingReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Gather);
		PermutationVector.Set<FRayTracingReflectionsRGS::FHybrid>(bHybridReflections);
		PermutationVector.Set<FRayTracingReflectionsRGS::FRayTraceSkyLightContribution>(bRayTraceSkyLightContribution);
		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRGS>(PermutationVector);
		OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
	}
}

void FDeferredShadingSceneRenderer::PrepareSingleLayerWaterRayTracingReflections(const FViewInfo& View, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (!ShouldRenderRayTracingReflections(View))
	{
		return;
	}

	if (FDataDrivenShaderPlatformInfo::GetSupportsHighEndRayTracingReflections(View.GetShaderPlatform()))
	{
		const bool bHybridReflections = ShouldRayTracedReflectionsUseHybridReflections();
		const bool bRayTraceSkyLightContribution = ShouldRayTracedReflectionsRayTraceSkyLightContribution(Scene);

		FRayTracingReflectionsRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingReflectionsRGS::FEnableTwoSidedGeometryForShadowDim>(EnableRayTracingShadowTwoSidedGeometry());
		PermutationVector.Set<FRayTracingReflectionsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::None);
		PermutationVector.Set<FRayTracingReflectionsRGS::FHybrid>(bHybridReflections);
		PermutationVector.Set<FRayTracingReflectionsRGS::FRayTraceSkyLightContribution>(bRayTraceSkyLightContribution);
		auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRGS>(PermutationVector);
		OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
	}
}

#endif // RHI_RAYTRACING

void FDeferredShadingSceneRenderer::SetupImaginaryReflectionTextureParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FSceneTextureParameters* OutTextures)
#if RHI_RAYTRACING
{
	*OutTextures = FSceneTextureParameters();

	FSceneViewState* SceneViewState = (FSceneViewState*)View.State;

	if (SceneViewState != nullptr)
	{
		OutTextures->SceneDepthTexture = GraphBuilder.RegisterExternalTexture(SceneViewState->ImaginaryReflectionDepthZ, TEXT("ImaginaryReflectionDepthZ"));
		OutTextures->GBufferVelocityTexture = TryRegisterExternalTexture(GraphBuilder, SceneViewState->ImaginaryReflectionVelocity);
		OutTextures->GBufferATexture = TryRegisterExternalTexture(GraphBuilder, SceneViewState->ImaginaryReflectionGBufferA);
	}
}
#else
{
	unimplemented();
}
#endif

void FDeferredShadingSceneRenderer::RenderRayTracingReflections(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FViewInfo& View,
	int DenoiserMode,
	const FRayTracingReflectionOptions& Options,
	IScreenSpaceDenoiser::FReflectionsInputs* OutDenoiserInputs)
#if RHI_RAYTRACING
{
	const FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures);

	if (Options.Algorithm == FRayTracingReflectionOptions::SortedDeferred)
	{
		RenderRayTracingDeferredReflections(
			GraphBuilder,
			SceneTextureParameters,
			View,
			DenoiserMode,
			Options,
			OutDenoiserInputs);
		return;
	}

	const uint32 SortTileSize = CVarRayTracingReflectionsSortTileSize.GetValueOnRenderThread();
	const uint32 EnableTranslucency = GRayTracingReflectionsTranslucency > -1 ? (uint32)GRayTracingReflectionsTranslucency : (uint32)View.FinalPostProcessSettings.RayTracingReflectionsTranslucency;

	const bool bHybridReflections = ShouldRayTracedReflectionsUseHybridReflections();
	const bool bSortMaterials = Options.Algorithm == FRayTracingReflectionOptions::Sorted;
	const bool bRayTraceSkyLightContribution = ShouldRayTracedReflectionsRayTraceSkyLightContribution(*Scene);

	FSceneViewState* SceneViewState = (FSceneViewState*)View.State;
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	int32 UpscaleFactor = int32(1.0f / Options.ResolutionFraction);
	ensure(Options.ResolutionFraction == 1.0 / UpscaleFactor);
	ensureMsgf(FComputeShaderUtils::kGolden2DGroupSize % UpscaleFactor == 0, TEXT("Reflection ray tracing will have uv misalignement."));
	FIntPoint RayTracingResolution = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), UpscaleFactor);
	FIntPoint RayTracingBufferSize = SceneTextures.Config.Extent / UpscaleFactor;

	{
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			RayTracingBufferSize,
			PF_FloatRGBA,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV);

		OutDenoiserInputs->Color = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflections"));
		
		Desc.Format = PF_R16F;
		OutDenoiserInputs->RayHitDistance = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflectionsHitDistance"));
		OutDenoiserInputs->RayImaginaryDepth = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingReflectionsImaginaryDepth"));
	}

	FRDGBufferRef ImaginaryReflectionGBuffer;
	{
		// Create the structured imaginary reflection G-buffer used by the reflection RGS
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(
			sizeof(FImaginaryReflectionGBufferData),
			RayTracingBufferSize.X * RayTracingBufferSize.Y);

		ImaginaryReflectionGBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("ImaginaryReflectionGBuffer"));
	}

	// When deferred materials are used, we need to dispatch the reflection shader twice:
	// - First pass gathers reflected ray hit data and sorts it by hit shader ID.
	// - Second pass re-traces the reflected ray and performs full shading.
	// When deferred materials are not used, everything is done in a single pass.
	const uint32 NumPasses = bSortMaterials ? 2 : 1;
	const EDeferredMaterialMode DeferredMaterialModes[2] = 
	{
		bSortMaterials ? EDeferredMaterialMode::Gather : EDeferredMaterialMode::None,
		bSortMaterials ? EDeferredMaterialMode::Shade : EDeferredMaterialMode::None,
	};

	FRDGBufferRef DeferredMaterialBuffer = nullptr;

	FIntPoint TileAlignedResolution = RayTracingResolution;
	if (SortTileSize)
	{
		TileAlignedResolution = FIntPoint::DivideAndRoundUp(RayTracingResolution, SortTileSize) * SortTileSize;
	}

	const uint32 DeferredMaterialBufferNumElements = TileAlignedResolution.X * TileAlignedResolution.Y;

	FRayTracingReflectionsRGS::FParameters CommonParameters;

	CommonParameters.SamplesPerPixel = Options.SamplesPerPixel;
	CommonParameters.MaxBounces = FMath::Max(1, GRayTracingReflectionsMaxBounces > -1? GRayTracingReflectionsMaxBounces : View.FinalPostProcessSettings.RayTracingReflectionsMaxBounces);
	CommonParameters.HeightFog = GRayTracingReflectionsHeightFog;
	CommonParameters.UseReflectionCaptures = Options.bReflectionCaptures;
	CommonParameters.ShouldDoDirectLighting = Options.bDirectLighting;
	CommonParameters.ReflectedShadowsType = GRayTracingReflectionsShadows > -1 ? GRayTracingReflectionsShadows : (int32)View.FinalPostProcessSettings.RayTracingReflectionsShadows;
	CommonParameters.ShouldDoEmissiveAndIndirectLighting = Options.bEmissiveAndIndirectLighting;
	CommonParameters.ShouldReflectOnlyWater = Options.bReflectOnlyWater;
	CommonParameters.UpscaleFactor = UpscaleFactor;
	CommonParameters.ReflectionMinRayDistance = FMath::Min(GRayTracingReflectionsMinRayDistance, GRayTracingReflectionsMaxRayDistance);
	CommonParameters.ReflectionMaxRayDistance = GRayTracingReflectionsMaxRayDistance;
	CommonParameters.ReflectionMaxRoughness = GetRayTracingReflectionsMaxRoughness(View);
	CommonParameters.ReflectionMaxNormalBias = CVarRayTracingReflectionsNormalBias.GetValueOnRenderThread();
	CommonParameters.ReflectionEnableTwoSidedGeometry = CVarRayTracingReflectionsEnableTwoSidedGeometry.GetValueOnRenderThread();
	CommonParameters.ShadowMaxNormalBias = GetRaytracingMaxNormalBias();
	CommonParameters.RayTracingResolution = RayTracingResolution;
	CommonParameters.TileAlignedResolution = TileAlignedResolution;
	CommonParameters.TestPathRoughness = CVarRayTracingReflectionsTestPathRoughness.GetValueOnRenderThread();
	CommonParameters.MinClearCoatLevel = CVarRayTracingReflectionsMinClearCoatLevel.GetValueOnRenderThread();
	CommonParameters.MaxUnderCoatBounces = CVarRayTracingReflectionsMaxUnderCoatBounces.GetValueOnRenderThread();
	CommonParameters.RenderTileOffsetX = 0;
	CommonParameters.RenderTileOffsetY = 0;
	CommonParameters.EnableTranslucency = EnableTranslucency; 
	CommonParameters.SkyLightDecoupleSampleGeneration = GetRayTracingSkyLightDecoupleSampleGenerationCVarValue();
	CommonParameters.SampleMode = (int32)ESamplePhase::Monlithic;

	CommonParameters.TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
	CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	CommonParameters.LightDataPacked = View.RayTracingLightDataUniformBuffer;

	CommonParameters.SceneTextures = SceneTextureParameters;
	SetupSkyLightVisibilityRaysParameters(GraphBuilder, View, &CommonParameters.SkyLightVisibilityRaysData);

	// Hybrid reflection path samples lit scene color texture instead of performing a ray trace.
	CommonParameters.SceneColor = bHybridReflections ? SceneTextures.Color.Resolve : SystemTextures.Black;

	CommonParameters.ReflectionStruct = CreateReflectionUniformBuffer(View, EUniformBufferUsage::UniformBuffer_SingleFrame);
	CommonParameters.FogUniformParameters = CreateFogUniformBuffer(GraphBuilder, View);
	CommonParameters.ColorOutput = GraphBuilder.CreateUAV(OutDenoiserInputs->Color);
	CommonParameters.RayHitDistanceOutput = GraphBuilder.CreateUAV(OutDenoiserInputs->RayHitDistance);
	CommonParameters.RayImaginaryDepthOutput = GraphBuilder.CreateUAV(OutDenoiserInputs->RayImaginaryDepth);
	CommonParameters.ImaginaryReflectionGBuffer = GraphBuilder.CreateUAV(ImaginaryReflectionGBuffer);
	CommonParameters.SortTileSize = SortTileSize;
	CommonParameters.ReflectionCapture = View.ReflectionCaptureUniformBuffer;
	CommonParameters.Forward = View.ForwardLightingResources.ForwardLightUniformBuffer;

	// Fill Sky Light parameters
	FSkyLightData SkyLightData;
	SetupSkyLightParameters(GraphBuilder, Scene, View, bRayTraceSkyLightContribution, &CommonParameters.SkylightParameters, &SkyLightData);
	CommonParameters.SkyLightData = CreateUniformBufferImmediate(SkyLightData, EUniformBufferUsage::UniformBuffer_SingleDraw);

	for (int32 SamplePassIndex = 0; SamplePassIndex < Options.SamplesPerPixel; SamplePassIndex++)
	{
		if (Options.SamplesPerPixel > 1)
		{
			CommonParameters.SampleMode = (int32)ESamplePhase::Accum;
			CommonParameters.SampleMode = SamplePassIndex == 0 ? (int32)ESamplePhase::Init : CommonParameters.SampleMode;
			CommonParameters.SampleMode = SamplePassIndex == (Options.SamplesPerPixel - 1) ? (int32)ESamplePhase::Resolve : CommonParameters.SampleMode;
		}
		CommonParameters.SampleOffset = SamplePassIndex;

		for (uint32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
		{
			FRayTracingReflectionsRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingReflectionsRGS::FParameters>();
			*PassParameters = CommonParameters;

			const EDeferredMaterialMode DeferredMaterialMode = DeferredMaterialModes[PassIndex];

			if (DeferredMaterialMode != EDeferredMaterialMode::None)
			{
				if (DeferredMaterialMode == EDeferredMaterialMode::Gather)
				{
					FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FDeferredMaterialPayload), DeferredMaterialBufferNumElements);
					DeferredMaterialBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("RayTracingReflectionsMaterialBuffer"));
				}

				PassParameters->MaterialBuffer = GraphBuilder.CreateUAV(DeferredMaterialBuffer);
			}

			FRayTracingReflectionsRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRayTracingReflectionsRGS::FDeferredMaterialMode>(DeferredMaterialMode);
			PermutationVector.Set<FRayTracingReflectionsRGS::FHybrid>(bHybridReflections);
			PermutationVector.Set<FRayTracingReflectionsRGS::FEnableTwoSidedGeometryForShadowDim>(EnableRayTracingShadowTwoSidedGeometry());
			PermutationVector.Set<FRayTracingReflectionsRGS::FRayTraceSkyLightContribution>(bRayTraceSkyLightContribution);
			auto RayGenShader = View.ShaderMap->GetShader<FRayTracingReflectionsRGS>(PermutationVector);

			ClearUnusedGraphResources(RayGenShader, PassParameters);

			if (DeferredMaterialMode == EDeferredMaterialMode::Gather)
			{
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ReflectionRayTracingGatherMaterials %dx%d", TileAlignedResolution.X, TileAlignedResolution.Y),
					PassParameters,
					ERDGPassFlags::Compute,
					[PassParameters, this, &View, RayGenShader, TileAlignedResolution](FRHIRayTracingCommandList& RHICmdList)
				{
					FRayTracingPipelineState* Pipeline = View.RayTracingMaterialGatherPipeline;

					FRayTracingShaderBindingsWriter GlobalResources;
					SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);

					FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
					RHICmdList.RayTraceDispatch(Pipeline, RayGenShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, TileAlignedResolution.X, TileAlignedResolution.Y);
				});

				// A material sorting pass
				const uint32 SortSize = CVarRayTracingReflectionsSortSize.GetValueOnRenderThread();
				if (SortSize)
				{
					SortDeferredMaterials(GraphBuilder, View, SortSize, DeferredMaterialBufferNumElements, DeferredMaterialBuffer);
				}
			}
			else
			{
				// Add optional tiling behavior to avoid TDR events in expensive passes
				int32 RenderTileSize = CVarRayTracingReflectionsRenderTileSize.GetValueOnRenderThread();
				if (NumPasses > 1 || RenderTileSize <= 0)
				{
					GraphBuilder.AddPass(
						RDG_EVENT_NAME("ReflectionRayTracing(spp=%d) %dx%d", Options.SamplesPerPixel, RayTracingResolution.X, RayTracingResolution.Y),
						PassParameters,
						ERDGPassFlags::Compute,
						[PassParameters, this, &View, RayGenShader, RayTracingResolution, DeferredMaterialBufferNumElements, DeferredMaterialMode](FRHIRayTracingCommandList& RHICmdList)
					{
						FRayTracingShaderBindingsWriter GlobalResources;
						SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);

						FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();

						if (DeferredMaterialMode == EDeferredMaterialMode::Shade)
						{
							// Shading pass for sorted materials uses 1D dispatch over all elements in the material buffer.
							// This can be reduced to the number of output pixels if sorting pass guarantees that all invalid entries are moved to the end.
							RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, DeferredMaterialBufferNumElements, 1);
						}
						else // EDeferredMaterialMode::None
						{
							RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, RayTracingResolution.X, RayTracingResolution.Y);
						}
					});
				}
				else
				{
					RenderTileSize = FMath::Max(RenderTileSize, 32);
					int32 NumTilesX = FMath::DivideAndRoundUp(RayTracingResolution.X, RenderTileSize);
					int32 NumTilesY = FMath::DivideAndRoundUp(RayTracingResolution.Y, RenderTileSize);
					for (int32 Y = 0; Y < NumTilesY; ++Y)
					{
						FRayTracingReflectionsRGS::FParameters* TilePassParameters = PassParameters;
						for (int32 X = 0; X < NumTilesX; ++X)
						{
							if (X > 0 || Y > 0)
							{
								TilePassParameters = GraphBuilder.AllocParameters<FRayTracingReflectionsRGS::FParameters>();
								*TilePassParameters = *PassParameters;
								TilePassParameters->RenderTileOffsetX = X * RenderTileSize;
								TilePassParameters->RenderTileOffsetY = Y * RenderTileSize;
							}

							int32 DispatchSizeX = FMath::Min<int32>(RenderTileSize, RayTracingResolution.X - TilePassParameters->RenderTileOffsetX);
							int32 DispatchSizeY = FMath::Min<int32>(RenderTileSize, RayTracingResolution.Y - TilePassParameters->RenderTileOffsetY);
							GraphBuilder.AddPass(
								RDG_EVENT_NAME("ReflectionRayTracing(spp=%d) %dx%d", Options.SamplesPerPixel, DispatchSizeX, DispatchSizeY),
								TilePassParameters,
								ERDGPassFlags::Compute,
								[TilePassParameters, this, &View, RayGenShader, DispatchSizeX, DispatchSizeY, DeferredMaterialBufferNumElements, DeferredMaterialMode](FRHIRayTracingCommandList& RHICmdList)
							{
								FRayTracingShaderBindingsWriter GlobalResources;
								SetShaderParameters(GlobalResources, RayGenShader, *TilePassParameters);

								FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
								RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, DispatchSizeX, DispatchSizeY);
							});
						}
					}
				}
			}
		}
	}

	// Setup imaginary reflection g-buffer outputs
	if (bRayTraceSkyLightContribution && SceneViewState != nullptr)
	{
		// Create a texture for the world-space normal imaginary reflection g-buffer.
		FRDGTextureRef ImaginaryReflectionGBufferATexture;
		{
			const FGBufferBindings& Bindings = SceneTextures.Config.GBufferBindings[GBL_Default];
			FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(RayTracingBufferSize, Bindings.GBufferA.Format, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV));
			ImaginaryReflectionGBufferATexture = GraphBuilder.CreateTexture(Desc, TEXT("ImaginaryReflectionGBufferA"));
		}

		// Create a texture for the depth imaginary reflection g-buffer.
		FRDGTextureRef ImaginaryReflectionDepthZTexture;
		{
			// R32_FLOAT used instead of usual depth/stencil format to work as a normal SRV/UAV rather a depth target
			FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(RayTracingBufferSize, PF_R32_FLOAT, SceneTextures.Config.DepthClearValue, TexCreate_ShaderResource | TexCreate_UAV));
			ImaginaryReflectionDepthZTexture = GraphBuilder.CreateTexture(Desc, TEXT("ImaginaryReflectionDepthZ"));
		}

		// Create a texture for the velocity imaginary reflection g-buffer.
		FRDGTextureRef ImaginaryReflectionGBufferVelocityTexture;
		{
			FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(RayTracingBufferSize, FVelocityRendering::GetFormat(View.GetShaderPlatform()), FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_UAV));
			ImaginaryReflectionGBufferVelocityTexture = GraphBuilder.CreateTexture(Desc, TEXT("ImaginaryReflectionGBufferVelocity"));
		}

		check(ImaginaryReflectionGBufferATexture);
		check(ImaginaryReflectionDepthZTexture);
		check(ImaginaryReflectionGBufferVelocityTexture);

		// Split the imaginary reflection g-buffer data components into the individual textures
		FSplitImaginaryReflectionGBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSplitImaginaryReflectionGBufferCS::FParameters>();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->UpscaleFactor = UpscaleFactor;
		PassParameters->RayTracingResolution = RayTracingResolution;
		PassParameters->ImaginaryReflectionGBuffer = GraphBuilder.CreateSRV(ImaginaryReflectionGBuffer);
		PassParameters->ImaginaryReflectionGBufferA = GraphBuilder.CreateUAV(ImaginaryReflectionGBufferATexture);
		PassParameters->ImaginaryReflectionDepthZ = GraphBuilder.CreateUAV(ImaginaryReflectionDepthZTexture);
		PassParameters->ImaginaryReflectionVelocity = GraphBuilder.CreateUAV(ImaginaryReflectionGBufferVelocityTexture);

		TShaderMapRef<FSplitImaginaryReflectionGBufferCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SplitImaginaryReflectionGBuffer"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FIntPoint(RayTracingResolution.X, RayTracingResolution.Y), FSplitImaginaryReflectionGBufferCS::GetGroupSize())
		);

		// Extract the split textures to their respective pooled render targets
		GraphBuilder.QueueTextureExtraction(ImaginaryReflectionGBufferATexture, &SceneViewState->ImaginaryReflectionGBufferA);
		GraphBuilder.QueueTextureExtraction(ImaginaryReflectionDepthZTexture, &SceneViewState->ImaginaryReflectionDepthZ);
		GraphBuilder.QueueTextureExtraction(ImaginaryReflectionGBufferVelocityTexture, &SceneViewState->ImaginaryReflectionVelocity);
	}
}
#else // !RHI_RAYTRACING
{
	check(0);
}
#endif

FRayTracingReflectionOptions GetRayTracingReflectionOptions(const FViewInfo& View, const FScene& Scene)
{
	FRayTracingReflectionOptions Result;

#if RHI_RAYTRACING

	Result.bEnabled = ShouldRenderRayTracingReflections(View);

	if (ShouldRayTracedReflectionsUseSortedDeferredAlgorithm(View))
	{
		Result.Algorithm = FRayTracingReflectionOptions::SortedDeferred;
	}
	else if (ShouldRayTracedReflectionsSortMaterials(View))
	{
		Result.Algorithm = FRayTracingReflectionOptions::Sorted;
	}
	else
	{
		Result.Algorithm = FRayTracingReflectionOptions::EAlgorithm::BruteForce;
	}

	Result.SamplesPerPixel = GetRayTracingReflectionsSamplesPerPixel(View);
	Result.ResolutionFraction = FMath::Clamp(CVarReflectionScreenPercentage.GetValueOnRenderThread() / 100.0f, 0.25f, 1.0f);
	Result.MaxRoughness = GetRayTracingReflectionsMaxRoughness(View);
	Result.bSkyLight = ShouldRayTracedReflectionsRayTraceSkyLightContribution(Scene);

	Result.bDirectLighting = GRayTracingReflectionsDirectLighting != 0;
	Result.bEmissiveAndIndirectLighting = GRayTracingReflectionsEmissiveAndIndirectLighting != 0;
	Result.bReflectionCaptures = GRayTracingReflectionsCaptures != 0;

#else // RHI_RAYTRACING

	Result.bEnabled = false;

#endif // RHI_RAYTRACING

	return Result;
}


float GetRayTracingReflectionScreenPercentage()
{
#if RHI_RAYTRACING
	return FMath::Clamp(CVarReflectionScreenPercentage.GetValueOnRenderThread() / 100.0f, 0.25f, 1.0f);
#else // RHI_RAYTRACING
	return 1.0f;
#endif // RHI_RAYTRACING
}
