// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeferredShadingRenderer.h"

#if RHI_RAYTRACING

#include "RayTracingSkyLight.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneRenderTargets.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "RayGenShaderUtils.h"
#include "SceneTextureParameters.h"
#include "ScreenSpaceDenoise.h"
#include "ClearQuad.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "RayTracing/RaytracingOptions.h"
#include "BlueNoise.h"
#include "SceneTextureParameters.h"
#include "RayTracingDefinitions.h"
#include "RayTracingDeferredMaterials.h"
#include "RayTracingTypes.h"
#include "PathTracingDefinitions.h"
#include "PathTracing.h"

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIllumination(
	TEXT("r.RayTracing.GlobalIllumination"),
	-1,
	TEXT("-1: Value driven by postprocess volume (default) \n")
	TEXT(" 0: ray tracing global illumination off \n")
	TEXT(" 1: ray tracing global illumination enabled (brute force) \n")
	TEXT(" 2: ray tracing global illumination enabled (final gather)"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static int32 GRayTracingGlobalIlluminationSamplesPerPixel = -1;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationSamplesPerPixel(
	TEXT("r.RayTracing.GlobalIllumination.SamplesPerPixel"),
	GRayTracingGlobalIlluminationSamplesPerPixel,
	TEXT("Samples per pixel (default = -1 (driven by postprocesing volume))"),
	ECVF_RenderThreadSafe
);

static float GRayTracingGlobalIlluminationMaxRayDistance = 1.0e27;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationMaxRayDistance(
	TEXT("r.RayTracing.GlobalIllumination.MaxRayDistance"),
	GRayTracingGlobalIlluminationMaxRayDistance,
	TEXT("Max ray distance (default = 1.0e27)")
);

static float GRayTracingGlobalIlluminationMaxShadowDistance = -1.0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationMaxShadowDistance(
	TEXT("r.RayTracing.GlobalIllumination.MaxShadowDistance"),
	GRayTracingGlobalIlluminationMaxShadowDistance,
	TEXT("Max shadow distance (default = -1.0, distance adjusted automatically so shadow rays do not hit the sky sphere) ")
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationMaxBounces(
	TEXT("r.RayTracing.GlobalIllumination.MaxBounces"),
	-1,
	TEXT("Max bounces (default = -1 (driven by postprocesing volume))"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingGlobalIlluminationNextEventEstimationSamples = 2;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationNextEventEstimationSamples(
	TEXT("r.RayTracing.GlobalIllumination.NextEventEstimationSamples"),
	GRayTracingGlobalIlluminationNextEventEstimationSamples,
	TEXT("Number of sample draws for next-event estimation (default = 2)")
	TEXT("NOTE: This parameter is experimental")
);

static float GRayTracingGlobalIlluminationDiffuseThreshold = 0.01;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationDiffuseThreshold(
	TEXT("r.RayTracing.GlobalIllumination.DiffuseThreshold"),
	GRayTracingGlobalIlluminationDiffuseThreshold,
	TEXT("Diffuse luminance threshold for evaluating global illumination")
	TEXT("NOTE: This parameter is experimental")
);

static int32 GRayTracingGlobalIlluminationDenoiser = 1;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationDenoiser(
	TEXT("r.RayTracing.GlobalIllumination.Denoiser"),
	GRayTracingGlobalIlluminationDenoiser,
	TEXT("Denoising options (default = 1)")
);

static int32 GRayTracingGlobalIlluminationEvalSkyLight = 0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationEvalSkyLight(
	TEXT("r.RayTracing.GlobalIllumination.EvalSkyLight"),
	GRayTracingGlobalIlluminationEvalSkyLight,
	TEXT("Evaluate SkyLight multi-bounce contribution")
	TEXT("NOTE: This parameter is experimental")
);

static int32 GRayTracingGlobalIlluminationUseRussianRoulette = 0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationUseRussianRoulette(
	TEXT("r.RayTracing.GlobalIllumination.UseRussianRoulette"),
	GRayTracingGlobalIlluminationUseRussianRoulette,
	TEXT("Perform Russian Roulette to only cast diffuse rays on surfaces with brighter albedos (default = 0)")
	TEXT("NOTE: This parameter is experimental")
);

static float GRayTracingGlobalIlluminationScreenPercentage = 50.0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationScreenPercentage(
	TEXT("r.RayTracing.GlobalIllumination.ScreenPercentage"),
	GRayTracingGlobalIlluminationScreenPercentage,
	TEXT("Screen percentage for ray tracing global illumination (default = 50)")
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationEnableTwoSidedGeometry(
	TEXT("r.RayTracing.GlobalIllumination.EnableTwoSidedGeometry"),
	1,
	TEXT("Enables two-sided geometry when tracing GI rays (default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationEnableTransmission(
	TEXT("r.RayTracing.GlobalIllumination.EnableTransmission"),
	1,
	TEXT("Enables transmission when tracing GI rays (default = 1)"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingGlobalIlluminationRenderTileSize = 0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationRenderTileSize(
	TEXT("r.RayTracing.GlobalIllumination.RenderTileSize"),
	GRayTracingGlobalIlluminationRenderTileSize,
	TEXT("Render ray traced global illumination in NxN pixel tiles, where each tile is submitted as separate GPU command buffer, allowing high quality rendering without triggering timeout detection. (default = 0, tiling disabled)")
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationMaxLightCount(
	TEXT("r.RayTracing.GlobalIllumination.MaxLightCount"),
	RAY_TRACING_LIGHT_COUNT_MAXIMUM,
	TEXT("Enables two-sided geometry when tracing GI rays (default = 256)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationFireflySuppression(
	TEXT("r.RayTracing.GlobalIllumination.FireflySuppression"),
	0,
	TEXT("Applies tonemap operator to suppress potential fireflies (default = 0). "),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationFinalGatherIterations(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.Iterations"),
	1,
	TEXT("Determines the number of iterations for gather point creation\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationFinalGatherFilterWidth(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.FilterWidth"),
	0,
	TEXT("Determines the local neighborhood for sample stealing (default = 0)\n"),
	ECVF_RenderThreadSafe
);

static float GRayTracingGlobalIlluminationFinalGatherDistance = 10.0;
static FAutoConsoleVariableRef CVarRayTracingGlobalIlluminationFinalGatherDistance(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.Distance"),
	GRayTracingGlobalIlluminationFinalGatherDistance,
	TEXT("Maximum screen-space distance for valid, reprojected final gather points (default = 10)")
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationFinalGatherSortMaterials(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.SortMaterials"),
	1,
	TEXT("Sets whether refected materials will be sorted before shading\n")
	TEXT("0: Disabled\n ")
	TEXT("1: Enabled, using Trace->Sort->Trace (Default)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationFinalGatherSortTileSize(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.SortTileSize"),
	64,
	TEXT("Size of pixel tiles for sorted global illumination (default = 64)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationFinalGatherSortSize(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.SortSize"),
	5,
	TEXT("Size of horizon for material ID sort\n")
	TEXT("0: Disabled\n")
	TEXT("1: 256 Elements\n")
	TEXT("2: 512 Elements\n")
	TEXT("3: 1024 Elements\n")
	TEXT("4: 2048 Elements\n")
	TEXT("5: 4096 Elements (Default)\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationFinalGatherEnableNeighborVisbilityTest(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.EnableNeighborVisibilityTest"),
	0,
	TEXT("Enables neighbor visibility tests when FilterWidth > 0 (default = 0)")
);

static TAutoConsoleVariable<float> CVarRayTracingGlobalIlluminationFinalGatherDepthRejectionKernel(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.DepthRejectionKernel"),
	1.0e-2,
	TEXT("Gather point relative Z-depth rejection tolerance (default = 1.0e-2)\n")
);

static TAutoConsoleVariable<float> CVarRayTracingGlobalIlluminationFinalGatherNormalRejectionKernel(
	TEXT("r.RayTracing.GlobalIllumination.FinalGather.NormalRejectionKernel"),
	0.2,
	TEXT("Gather point WorldNormal rejection tolerance (default = 1.0e-2)\n")
);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationDirectionalLight(
	TEXT("r.RayTracing.GlobalIllumination.Lights.DirectionalLight"),
	1,
	TEXT("Enables DirectionalLight sampling for global illumination (default = 1)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationSkyLight(
	TEXT("r.RayTracing.GlobalIllumination.Lights.SkyLight"),
	1,
	TEXT("Enables SkyLight sampling for global illumination (default = 1)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationPointLight(
	TEXT("r.RayTracing.GlobalIllumination.Lights.PointLight"),
	1,
	TEXT("Enables PointLight sampling for global illumination (default = 1)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationSpotLight(
	TEXT("r.RayTracing.GlobalIllumination.Lights.SpotLight"),
	1,
	TEXT("Enables SpotLight sampling for global illumination (default = 1)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingGlobalIlluminationRectLight(
	TEXT("r.RayTracing.GlobalIllumination.Lights.RectLight"),
	1,
	TEXT("Enables RectLight sampling for global illumination (default = 1)"),
	ECVF_RenderThreadSafe);

DECLARE_GPU_STAT_NAMED(RayTracingGIBruteForce, TEXT("Ray Tracing GI: Brute Force"));
DECLARE_GPU_STAT_NAMED(RayTracingGIFinalGather, TEXT("Ray Tracing GI: Final Gather"));
DECLARE_GPU_STAT_NAMED(RayTracingGICreateGatherPoints, TEXT("Ray Tracing GI: Create Gather Points"));

static void SetupLightParameters(
	FScene* Scene,
	const FViewInfo& View, FRDGBuilder& GraphBuilder,
	FRDGBufferSRV** OutLightBuffer, uint32* OutLightCount, FPathTracingSkylight* SkylightParameters)
{
	FPathTracingLight Lights[RAY_TRACING_LIGHT_COUNT_MAXIMUM];
	unsigned LightCount = 0;

	// Get the SkyLight color

	FSkyLightSceneProxy* SkyLight = Scene->SkyLight;

	const bool bUseMISCompensation = true;
	const bool bSkylightEnabled = SkyLight && SkyLight->bAffectGlobalIllumination && CVarRayTracingGlobalIlluminationSkyLight.GetValueOnRenderThread() != 0;

	// Prepend SkyLight to light buffer (if it is active)
	if (PrepareSkyTexture(GraphBuilder, Scene, View, bSkylightEnabled, bUseMISCompensation, SkylightParameters))
	{
		FPathTracingLight& DestLight = Lights[LightCount];

		DestLight.Color = FVector3f::OneVector;
		DestLight.Flags = SkyLight->bTransmission ? PATHTRACER_FLAG_TRANSMISSION_MASK : 0;
		// SkyLight does not have a LightingChannelMask
		DestLight.Flags |= PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= PATHTRACING_LIGHT_SKY;

		LightCount++;
	}

	
	const uint32 MaxLightCount = FMath::Min(CVarRayTracingGlobalIlluminationMaxLightCount.GetValueOnRenderThread(), RAY_TRACING_LIGHT_COUNT_MAXIMUM);
	for (auto Light : Scene->Lights)
	{
		if (LightCount >= MaxLightCount) break;

		if (Light.LightSceneInfo->Proxy->HasStaticLighting() && Light.LightSceneInfo->IsPrecomputedLightingValid()) continue;
		if (!Light.LightSceneInfo->Proxy->AffectGlobalIllumination()) continue;

		FPathTracingLight& DestLight = Lights[LightCount]; // don't increment LightCount yet -- we might still skip this light

		FLightRenderParameters LightShaderParameters;
		Light.LightSceneInfo->Proxy->GetLightShaderParameters(LightShaderParameters);

		LightShaderParameters.Color *= LightShaderParameters.GetLightExposureScale(View.GetLastEyeAdaptationExposure());

		uint32 Transmission = Light.LightSceneInfo->Proxy->Transmission();
		uint8 LightingChannelMask = Light.LightSceneInfo->Proxy->GetLightingChannelMask();
		DestLight.Flags  = Transmission ? PATHTRACER_FLAG_TRANSMISSION_MASK : 0;
		DestLight.Flags |= LightingChannelMask & PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= Light.LightSceneInfo->Proxy->IsInverseSquared() ? 0 : PATHTRACER_FLAG_NON_INVERSE_SQUARE_FALLOFF_MASK;

		DestLight.FalloffExponent = LightShaderParameters.FalloffExponent;
		DestLight.Attenuation = LightShaderParameters.InvRadius;
		DestLight.IESTextureSlice = -1; // not used by this path at the moment

		ELightComponentType LightComponentType = (ELightComponentType)Light.LightSceneInfo->Proxy->GetLightType();
		switch (LightComponentType)
		{
		case LightType_Directional:
		{
			if (CVarRayTracingGlobalIlluminationDirectionalLight.GetValueOnRenderThread() == 0) continue;

			DestLight.Normal = LightShaderParameters.Direction;
			DestLight.Color = FVector3f(LightShaderParameters.Color);
			DestLight.Flags |= PATHTRACING_LIGHT_DIRECTIONAL;
			break;
		}
		case LightType_Rect:
		{
			if (CVarRayTracingGlobalIlluminationRectLight.GetValueOnRenderThread() == 0) continue;

			DestLight.TranslatedWorldPosition = FVector3f(LightShaderParameters.WorldPosition + View.ViewMatrices.GetPreViewTranslation());
			DestLight.Normal = -LightShaderParameters.Direction;
			DestLight.dPdu = FVector3f::CrossProduct(LightShaderParameters.Direction, LightShaderParameters.Tangent);
			DestLight.dPdv = LightShaderParameters.Tangent;
			DestLight.Color = FVector3f(LightShaderParameters.Color);
			DestLight.Dimensions = FVector2f(2.0f * LightShaderParameters.SourceRadius, 2.0f * LightShaderParameters.SourceLength);
			DestLight.Shaping = FVector2f(LightShaderParameters.RectLightBarnCosAngle, LightShaderParameters.RectLightBarnLength);
			DestLight.Flags |= PATHTRACING_LIGHT_RECT;
			break;
		}
		case LightType_Point:
		default:
		{
			if (CVarRayTracingGlobalIlluminationPointLight.GetValueOnRenderThread() == 0) continue;

			DestLight.TranslatedWorldPosition = FVector3f(LightShaderParameters.WorldPosition + View.ViewMatrices.GetPreViewTranslation());
			// #dxr_todo: UE-72556 define these differences from Lit..
			DestLight.Color = FVector3f(LightShaderParameters.Color);
			float SourceRadius = 0.0; // LightShaderParameters.SourceRadius causes too much noise for little pay off at this time
			DestLight.Dimensions = FVector2f(SourceRadius, 0.0);
			DestLight.Flags |= PATHTRACING_LIGHT_POINT;
			break;
		}
		case LightType_Spot:
		{
			if (CVarRayTracingGlobalIlluminationSpotLight.GetValueOnRenderThread() == 0) continue;

			DestLight.TranslatedWorldPosition = FVector3f(LightShaderParameters.WorldPosition + View.ViewMatrices.GetPreViewTranslation());
			DestLight.Normal = -LightShaderParameters.Direction;
			// #dxr_todo: UE-72556 define these differences from Lit..
			DestLight.Color = FVector3f(LightShaderParameters.Color);
			float SourceRadius = 0.0; // LightShaderParameters.SourceRadius causes too much noise for little pay off at this time
			DestLight.Dimensions = FVector2f(SourceRadius, 0.0);
			DestLight.Shaping = LightShaderParameters.SpotAngles;
			DestLight.Flags |= PATHTRACING_LIGHT_SPOT;
			break;
		}
		};

		DestLight.Color *= Light.LightSceneInfo->Proxy->GetIndirectLightingScale();

		// we definitely added the light if we reach this point
		LightCount++;
	}

	{
		// Upload the buffer of lights to the GPU (send at least one)
		size_t DataSize = sizeof(FPathTracingLight) * FMath::Max(LightCount, 1u);
		*OutLightBuffer = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CreateStructuredBuffer(GraphBuilder, TEXT("RTGILightsBuffer"), sizeof(FPathTracingLight), FMath::Max(LightCount, 1u), Lights, DataSize)));
		*OutLightCount = LightCount;
	}
}

int32 GetRayTracingGlobalIlluminationSamplesPerPixel(const FViewInfo& View)
{
	int32 SamplesPerPixel = GRayTracingGlobalIlluminationSamplesPerPixel > -1 ? GRayTracingGlobalIlluminationSamplesPerPixel : View.FinalPostProcessSettings.RayTracingGISamplesPerPixel;
	return SamplesPerPixel;
}

bool ShouldRenderRayTracingGlobalIllumination(const FViewInfo& View)
{
	if (GetRayTracingGlobalIlluminationSamplesPerPixel(View) <= 0)
	{
		return false;
	}

	if (View.FinalPostProcessSettings.DynamicGlobalIlluminationMethod != EDynamicGlobalIlluminationMethod::RayTraced)
	{
		return false;
	}

	if (!View.ViewState)
	{
		return false;
	}

    const int32 CVarRayTracingGlobalIlluminationValue = CVarRayTracingGlobalIllumination.GetValueOnRenderThread();
	const bool bEnabled = CVarRayTracingGlobalIlluminationValue >= 0
		? CVarRayTracingGlobalIlluminationValue > 0
		: View.FinalPostProcessSettings.RayTracingGIType > ERayTracingGlobalIlluminationType::Disabled;

	return ShouldRenderRayTracingEffect(bEnabled, ERayTracingPipelineCompatibilityFlags::FullPipeline, &View);
}

bool IsFinalGatherEnabled(const FViewInfo& View)
{

	int32 CVarRayTracingGlobalIlluminationValue = CVarRayTracingGlobalIllumination.GetValueOnRenderThread();
	if (CVarRayTracingGlobalIlluminationValue >= 0)
	{
		return CVarRayTracingGlobalIlluminationValue == 2;
	}

	return View.FinalPostProcessSettings.RayTracingGIType == ERayTracingGlobalIlluminationType::FinalGather;
}

class FGlobalIlluminationRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGlobalIlluminationRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FGlobalIlluminationRGS, FGlobalShader)

	class FEnableTwoSidedGeometryDim : SHADER_PERMUTATION_BOOL("ENABLE_TWO_SIDED_GEOMETRY");
	class FEnableTransmissionDim : SHADER_PERMUTATION_INT("ENABLE_TRANSMISSION", 2);

	using FPermutationDomain = TShaderPermutationDomain<FEnableTwoSidedGeometryDim, FEnableTransmissionDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// We need the skylight to do its own form of MIS because RTGI doesn't do its own
		OutEnvironment.SetDefine(TEXT("PATHTRACING_SKY_MIS"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, SamplesPerPixel)
		SHADER_PARAMETER(uint32, MaxBounces)
		SHADER_PARAMETER(uint32, UpscaleFactor)
		SHADER_PARAMETER(float, MaxRayDistanceForGI)
		SHADER_PARAMETER(float, MaxRayDistanceForAO)
		SHADER_PARAMETER(float, MaxShadowDistance)
		SHADER_PARAMETER(float, NextEventEstimationSamples)
		SHADER_PARAMETER(float, DiffuseThreshold)
		SHADER_PARAMETER(uint32, EvalSkyLight)
		SHADER_PARAMETER(uint32, UseRussianRoulette)
		SHADER_PARAMETER(uint32, UseFireflySuppression)
		SHADER_PARAMETER(float, MaxNormalBias)
		SHADER_PARAMETER(uint32, RenderTileOffsetX)
		SHADER_PARAMETER(uint32, RenderTileOffsetY)

		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWGlobalIlluminationUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, RWGlobalIlluminationRayDistanceUAV)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPathTracingLight>, SceneLights)
		SHADER_PARAMETER(uint32, SceneLightCount)
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingSkylight, SkylightParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FGlobalIlluminationRGS, "/Engine/Private/RayTracing/RayTracingGlobalIlluminationRGS.usf", "GlobalIlluminationRGS", SF_RayGen);

// Note: This constant must match the definition in RayTracingGatherPoints.ush
constexpr int32 MAXIMUM_GATHER_POINTS_PER_PIXEL = 32;

struct FGatherPoint
{
	FVector CreationPoint;
	FVector Position;
	FIntPoint Irradiance;
};

class FRayTracingGlobalIlluminationCreateGatherPointsRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingGlobalIlluminationCreateGatherPointsRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingGlobalIlluminationCreateGatherPointsRGS, FGlobalShader)

	class FEnableTwoSidedGeometryDim : SHADER_PERMUTATION_BOOL("ENABLE_TWO_SIDED_GEOMETRY");
	class FDeferredMaterialMode : SHADER_PERMUTATION_ENUM_CLASS("DIM_DEFERRED_MATERIAL_MODE", EDeferredMaterialMode);
	class FEnableTransmissionDim : SHADER_PERMUTATION_INT("ENABLE_TRANSMISSION", 2);

	using FPermutationDomain = TShaderPermutationDomain<FEnableTwoSidedGeometryDim, FDeferredMaterialMode, FEnableTransmissionDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// We need the skylight to do its own form of MIS because RTGI doesn't do its own
		OutEnvironment.SetDefine(TEXT("PATHTRACING_SKY_MIS"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GatherSamplesPerPixel)
		SHADER_PARAMETER(uint32, SamplesPerPixel)
		SHADER_PARAMETER(uint32, GatherPointIteration)
		SHADER_PARAMETER(uint32, GatherFilterWidth)
		SHADER_PARAMETER(uint32, SampleIndex)
		SHADER_PARAMETER(uint32, MaxBounces)
		SHADER_PARAMETER(uint32, UpscaleFactor)
		SHADER_PARAMETER(uint32, RenderTileOffsetX)
		SHADER_PARAMETER(uint32, RenderTileOffsetY)
		SHADER_PARAMETER(float, MaxRayDistanceForGI)
		SHADER_PARAMETER(float, MaxShadowDistance)
		SHADER_PARAMETER(float, NextEventEstimationSamples)
		SHADER_PARAMETER(float, DiffuseThreshold)
		SHADER_PARAMETER(float, MaxNormalBias)
		SHADER_PARAMETER(uint32, EvalSkyLight)
		SHADER_PARAMETER(uint32, UseRussianRoulette)

		// Scene data
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)

		// Light data
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPathTracingLight>, SceneLights)
		SHADER_PARAMETER(uint32, SceneLightCount)
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingSkylight, SkylightParameters)

		// Shading data
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)

		SHADER_PARAMETER(FIntPoint, GatherPointsResolution)
		SHADER_PARAMETER(FIntPoint, TileAlignedResolution)
		SHADER_PARAMETER(int32, SortTileSize)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<GatherPoints>, RWGatherPointsBuffer)
		// Optional indirection buffer used for sorted materials
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FDeferredMaterialPayload>, MaterialBuffer)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingGlobalIlluminationCreateGatherPointsRGS, "/Engine/Private/RayTracing/RayTracingCreateGatherPointsRGS.usf", "RayTracingCreateGatherPointsRGS", SF_RayGen);

// Auxillary gather point data for reprojection
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FGatherPointData, )
	SHADER_PARAMETER(uint32, Count)
	SHADER_PARAMETER_ARRAY(FMatrix44f, ViewMatrices, [MAXIMUM_GATHER_POINTS_PER_PIXEL])
END_GLOBAL_SHADER_PARAMETER_STRUCT()
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGatherPointData, "GatherPointData");

class FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS, FGlobalShader)

	class FEnableTwoSidedGeometryDim : SHADER_PERMUTATION_BOOL("ENABLE_TWO_SIDED_GEOMETRY");
	class FDeferredMaterialMode : SHADER_PERMUTATION_ENUM_CLASS("DIM_DEFERRED_MATERIAL_MODE", EDeferredMaterialMode);

	using FPermutationDomain = TShaderPermutationDomain<FEnableTwoSidedGeometryDim, FDeferredMaterialMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GatherSamplesPerPixel)
		SHADER_PARAMETER(uint32, SamplesPerPixel)
		SHADER_PARAMETER(uint32, GatherPointIteration)
		SHADER_PARAMETER(uint32, GatherFilterWidth)
		SHADER_PARAMETER(uint32, SampleIndex)
		SHADER_PARAMETER(uint32, MaxBounces)
		SHADER_PARAMETER(uint32, UpscaleFactor)
		SHADER_PARAMETER(uint32, RenderTileOffsetX)
		SHADER_PARAMETER(uint32, RenderTileOffsetY)
		SHADER_PARAMETER(float, MaxRayDistanceForGI)
		SHADER_PARAMETER(float, MaxShadowDistance)
		SHADER_PARAMETER(float, NextEventEstimationSamples)
		SHADER_PARAMETER(float, DiffuseThreshold)
		SHADER_PARAMETER(float, MaxNormalBias)
		SHADER_PARAMETER(uint32, EvalSkyLight)
		SHADER_PARAMETER(uint32, UseRussianRoulette)

		// Scene data
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)

		// Light data
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPathTracingLight>, SceneLights)
		SHADER_PARAMETER(uint32, SceneLightCount)
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingSkylight, SkylightParameters)

		// Shading data
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)

		SHADER_PARAMETER(FIntPoint, GatherPointsResolution)
		SHADER_PARAMETER(FIntPoint, TileAlignedResolution)
		SHADER_PARAMETER(int32, SortTileSize)

		// Output
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<GatherPoints>, RWGatherPointsBuffer)
		// Optional indirection buffer used for sorted materials
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FDeferredMaterialPayload>, MaterialBuffer)
		END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS, "/Engine/Private/RayTracing/RayTracingCreateGatherPointsRGS.usf", "RayTracingCreateGatherPointsTraceRGS", SF_RayGen);

class FRayTracingGlobalIlluminationFinalGatherRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingGlobalIlluminationFinalGatherRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingGlobalIlluminationFinalGatherRGS, FGlobalShader)

	class FEnableTwoSidedGeometryDim : SHADER_PERMUTATION_BOOL("ENABLE_TWO_SIDED_GEOMETRY");
	class FEnableNeighborVisibilityTestDim : SHADER_PERMUTATION_BOOL("USE_NEIGHBOR_VISIBILITY_TEST");

	using FPermutationDomain = TShaderPermutationDomain<FEnableTwoSidedGeometryDim, FEnableNeighborVisibilityTestDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, SampleIndex)
		SHADER_PARAMETER(uint32, SamplesPerPixel)
		SHADER_PARAMETER(uint32, GatherPointIterations)
		SHADER_PARAMETER(uint32, GatherFilterWidth)
		SHADER_PARAMETER(uint32, UseFireflySuppression)
		SHADER_PARAMETER(uint32, UpscaleFactor)
		SHADER_PARAMETER(uint32, RenderTileOffsetX)
		SHADER_PARAMETER(uint32, RenderTileOffsetY)
		SHADER_PARAMETER(float, DiffuseThreshold)
		SHADER_PARAMETER(float, MaxNormalBias)
		SHADER_PARAMETER(float, FinalGatherDistance)
		SHADER_PARAMETER(float, DepthRejectionKernel)
		SHADER_PARAMETER(float, NormalRejectionKernel)

		// Reprojection data
		SHADER_PARAMETER_STRUCT_REF(FGatherPointData, GatherPointData)

		// Scene data
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)

		// Shading data
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)

		// Gather points
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<GatherPoints>, GatherPointsBuffer)
		SHADER_PARAMETER(FIntPoint, GatherPointsResolution)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWGlobalIlluminationUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, RWGlobalIlluminationRayDistanceUAV)
		END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingGlobalIlluminationFinalGatherRGS, "/Engine/Private/RayTracing/RayTracingFinalGatherRGS.usf", "RayTracingFinalGatherRGS", SF_RayGen);

void FDeferredShadingSceneRenderer::PrepareRayTracingGlobalIllumination(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (!ShouldRenderRayTracingGlobalIllumination(View))
	{
		return;
	}

	const bool bSortMaterials = CVarRayTracingGlobalIlluminationFinalGatherSortMaterials.GetValueOnRenderThread() != 0;
	int EnableTransmission = CVarRayTracingGlobalIlluminationEnableTransmission.GetValueOnRenderThread();

	// Declare all RayGen shaders that require material closest hit shaders to be bound
	for (int EnableTwoSidedGeometry = 0; EnableTwoSidedGeometry < 2; ++EnableTwoSidedGeometry)
	{
		FGlobalIlluminationRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FGlobalIlluminationRGS::FEnableTwoSidedGeometryDim>(EnableTwoSidedGeometry == 1);
		PermutationVector.Set<FGlobalIlluminationRGS::FEnableTransmissionDim>(EnableTransmission);
		TShaderMapRef<FGlobalIlluminationRGS> RayGenerationShader(View.ShaderMap, PermutationVector);
		OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());

		if (bSortMaterials)
		{
			// Gather
			{
				FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FPermutationDomain CreateGatherPointsPermutationVector;
				CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FEnableTwoSidedGeometryDim>(EnableTwoSidedGeometry == 1);
				CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Gather);
				TShaderMapRef<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS> CreateGatherPointsRayGenerationShader(View.ShaderMap, CreateGatherPointsPermutationVector);
				OutRayGenShaders.Add(CreateGatherPointsRayGenerationShader.GetRayTracingShader());
			}

			// Shade
			{
				FRayTracingGlobalIlluminationCreateGatherPointsRGS::FPermutationDomain CreateGatherPointsPermutationVector;
				CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FEnableTwoSidedGeometryDim>(EnableTwoSidedGeometry == 1);
				CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Shade);
				CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FEnableTransmissionDim>(EnableTransmission);
				TShaderMapRef<FRayTracingGlobalIlluminationCreateGatherPointsRGS> CreateGatherPointsRayGenerationShader(View.ShaderMap, CreateGatherPointsPermutationVector);
				OutRayGenShaders.Add(CreateGatherPointsRayGenerationShader.GetRayTracingShader());
			}
		}
		else
		{
			FRayTracingGlobalIlluminationCreateGatherPointsRGS::FPermutationDomain CreateGatherPointsPermutationVector;
			CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FEnableTwoSidedGeometryDim>(EnableTwoSidedGeometry == 1);
			CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::None);
			CreateGatherPointsPermutationVector.Set < FRayTracingGlobalIlluminationCreateGatherPointsRGS::FEnableTransmissionDim>(EnableTransmission);
			TShaderMapRef<FRayTracingGlobalIlluminationCreateGatherPointsRGS> CreateGatherPointsRayGenerationShader(View.ShaderMap, CreateGatherPointsPermutationVector);
			OutRayGenShaders.Add(CreateGatherPointsRayGenerationShader.GetRayTracingShader());
		}

		for (int EnableNeighborVisibilityTest = 0; EnableNeighborVisibilityTest < 2; ++EnableNeighborVisibilityTest)
		{
			FRayTracingGlobalIlluminationFinalGatherRGS::FPermutationDomain GatherPassPermutationVector;
			GatherPassPermutationVector.Set<FRayTracingGlobalIlluminationFinalGatherRGS::FEnableTwoSidedGeometryDim>(EnableTwoSidedGeometry == 1);
			GatherPassPermutationVector.Set<FRayTracingGlobalIlluminationFinalGatherRGS::FEnableNeighborVisibilityTestDim>(EnableNeighborVisibilityTest == 1);
			TShaderMapRef<FRayTracingGlobalIlluminationFinalGatherRGS> GatherPassRayGenerationShader(View.ShaderMap, GatherPassPermutationVector);
			OutRayGenShaders.Add(GatherPassRayGenerationShader.GetRayTracingShader());
		}
	}
}

void FDeferredShadingSceneRenderer::PrepareRayTracingGlobalIlluminationDeferredMaterial(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (!ShouldRenderRayTracingGlobalIllumination(View))
	{
		return;
	}

	const bool bSortMaterials = CVarRayTracingGlobalIlluminationFinalGatherSortMaterials.GetValueOnRenderThread() != 0;
	int EnableTransmission = CVarRayTracingGlobalIlluminationEnableTransmission.GetValueOnRenderThread();

	if (!bSortMaterials)
	{
		return;
	}

	// Declare all RayGen shaders that require material closest hit shaders to be bound
	for (int EnableTwoSidedGeometry = 0; EnableTwoSidedGeometry < 2; ++EnableTwoSidedGeometry)
	{
		FGlobalIlluminationRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FGlobalIlluminationRGS::FEnableTwoSidedGeometryDim>(EnableTwoSidedGeometry == 1);
		PermutationVector.Set<FGlobalIlluminationRGS::FEnableTransmissionDim>(EnableTransmission);
		TShaderMapRef<FGlobalIlluminationRGS> RayGenerationShader(View.ShaderMap, PermutationVector);
		OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());

		// Gather
		{
			FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FPermutationDomain CreateGatherPointsPermutationVector;
			CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FEnableTwoSidedGeometryDim>(EnableTwoSidedGeometry == 1);
			CreateGatherPointsPermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Gather);
			TShaderMapRef<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS> CreateGatherPointsRayGenerationShader(View.ShaderMap, CreateGatherPointsPermutationVector);
			OutRayGenShaders.Add(CreateGatherPointsRayGenerationShader.GetRayTracingShader());
		}

	}
}

#endif // RHI_RAYTRACING

bool FDeferredShadingSceneRenderer::RenderRayTracingGlobalIllumination(
	FRDGBuilder& GraphBuilder, 
	FSceneTextureParameters& SceneTextures,
	FViewInfo& View,
	IScreenSpaceDenoiser::FAmbientOcclusionRayTracingConfig* OutRayTracingConfig,
	IScreenSpaceDenoiser::FDiffuseIndirectInputs* OutDenoiserInputs)
#if RHI_RAYTRACING
{
	if (!View.ViewState) return false;

	int32 RayTracingGISamplesPerPixel = GetRayTracingGlobalIlluminationSamplesPerPixel(View);
	if (RayTracingGISamplesPerPixel <= 0) return false;

	OutRayTracingConfig->ResolutionFraction = 1.0;
	if (GRayTracingGlobalIlluminationDenoiser != 0)
	{
		OutRayTracingConfig->ResolutionFraction = FMath::Clamp(GRayTracingGlobalIlluminationScreenPercentage / 100.0, 0.25, 1.0);
	}

	OutRayTracingConfig->RayCountPerPixel = RayTracingGISamplesPerPixel;

	int32 UpscaleFactor = int32(1.0 / OutRayTracingConfig->ResolutionFraction);

	// Allocate input for the denoiser.
	{
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			SceneTextures.SceneDepthTexture->Desc.Extent / UpscaleFactor,
			PF_FloatRGBA,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV);

		OutDenoiserInputs->Color = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingDiffuseIndirect"));

		Desc.Format = PF_G16R16;
		OutDenoiserInputs->RayHitDistance = GraphBuilder.CreateTexture(Desc, TEXT("RayTracingDiffuseIndirectHitDistance"));
	}

	// Ray generation pass
	if (IsFinalGatherEnabled(View))
	{
		RenderRayTracingGlobalIlluminationFinalGather(GraphBuilder, SceneTextures, View, *OutRayTracingConfig, UpscaleFactor, OutDenoiserInputs);
	}
	else
	{
		RenderRayTracingGlobalIlluminationBruteForce(GraphBuilder, SceneTextures, View, *OutRayTracingConfig, UpscaleFactor, OutDenoiserInputs);
	}
	return true;
}
#else
{
	unimplemented();
	return false;
}
#endif // RHI_RAYTRACING

#if RHI_RAYTRACING
void CopyGatherPassParameters(
	const FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters& PassParameters,
	FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FParameters* NewParameters
)
{
	NewParameters->GatherSamplesPerPixel = PassParameters.GatherSamplesPerPixel;
	NewParameters->SamplesPerPixel = PassParameters.SamplesPerPixel;
	NewParameters->GatherPointIteration = PassParameters.GatherPointIteration;
	NewParameters->GatherFilterWidth = PassParameters.GatherFilterWidth;
	NewParameters->SampleIndex = PassParameters.SampleIndex;
	NewParameters->MaxBounces = PassParameters.MaxBounces;
	NewParameters->UpscaleFactor = PassParameters.UpscaleFactor;
	NewParameters->RenderTileOffsetX = PassParameters.RenderTileOffsetX;
	NewParameters->RenderTileOffsetY = PassParameters.RenderTileOffsetY;
	NewParameters->MaxRayDistanceForGI = PassParameters.MaxRayDistanceForGI;
	NewParameters->MaxShadowDistance = PassParameters.MaxShadowDistance;
	NewParameters->NextEventEstimationSamples = PassParameters.NextEventEstimationSamples;
	NewParameters->DiffuseThreshold = PassParameters.DiffuseThreshold;
	NewParameters->MaxNormalBias = PassParameters.MaxNormalBias;
	NewParameters->EvalSkyLight = PassParameters.EvalSkyLight;
	NewParameters->UseRussianRoulette = PassParameters.UseRussianRoulette;

	NewParameters->TLAS = PassParameters.TLAS;
	NewParameters->ViewUniformBuffer = PassParameters.ViewUniformBuffer;

	NewParameters->SceneLights = PassParameters.SceneLights;
	NewParameters->SceneLightCount = PassParameters.SceneLightCount;
	NewParameters->SkylightParameters = PassParameters.SkylightParameters;

	NewParameters->SceneTextures = PassParameters.SceneTextures;

	NewParameters->GatherPointsResolution = PassParameters.GatherPointsResolution;
	NewParameters->TileAlignedResolution = PassParameters.TileAlignedResolution;
	NewParameters->SortTileSize = PassParameters.SortTileSize;

	NewParameters->RWGatherPointsBuffer = PassParameters.RWGatherPointsBuffer;
	NewParameters->MaterialBuffer = PassParameters.MaterialBuffer;
}

void CopyGatherPassParameters(
	const FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters& PassParameters,
	FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters* NewParameters
)
{
	NewParameters->GatherSamplesPerPixel = PassParameters.GatherSamplesPerPixel;
	NewParameters->SamplesPerPixel = PassParameters.SamplesPerPixel;
	NewParameters->GatherPointIteration = PassParameters.GatherPointIteration;
	NewParameters->GatherFilterWidth = PassParameters.GatherFilterWidth;
	NewParameters->SampleIndex = PassParameters.SampleIndex;
	NewParameters->MaxBounces = PassParameters.MaxBounces;
	NewParameters->UpscaleFactor = PassParameters.UpscaleFactor;
	NewParameters->RenderTileOffsetX = PassParameters.RenderTileOffsetX;
	NewParameters->RenderTileOffsetY = PassParameters.RenderTileOffsetY;
	NewParameters->MaxRayDistanceForGI = PassParameters.MaxRayDistanceForGI;
	NewParameters->MaxShadowDistance = PassParameters.MaxShadowDistance;
	NewParameters->NextEventEstimationSamples = PassParameters.NextEventEstimationSamples;
	NewParameters->DiffuseThreshold = PassParameters.DiffuseThreshold;
	NewParameters->MaxNormalBias = PassParameters.MaxNormalBias;
	NewParameters->EvalSkyLight = PassParameters.EvalSkyLight;
	NewParameters->UseRussianRoulette = PassParameters.UseRussianRoulette;

	NewParameters->TLAS = PassParameters.TLAS;
	NewParameters->ViewUniformBuffer = PassParameters.ViewUniformBuffer;

	NewParameters->SceneLightCount = PassParameters.SceneLightCount;
	NewParameters->SceneLights = PassParameters.SceneLights;
	NewParameters->SkylightParameters = PassParameters.SkylightParameters;

	NewParameters->SceneTextures = PassParameters.SceneTextures;

	NewParameters->GatherPointsResolution = PassParameters.GatherPointsResolution;
	NewParameters->TileAlignedResolution = PassParameters.TileAlignedResolution;
	NewParameters->SortTileSize = PassParameters.SortTileSize;

	NewParameters->RWGatherPointsBuffer = PassParameters.RWGatherPointsBuffer;
	NewParameters->MaterialBuffer = PassParameters.MaterialBuffer;
}
#endif // RHI_RAYTRACING

void FDeferredShadingSceneRenderer::RayTracingGlobalIlluminationCreateGatherPoints(
	FRDGBuilder& GraphBuilder,
	FSceneTextureParameters& SceneTextures,
	FViewInfo& View,
	int32 UpscaleFactor,
	int32 SampleIndex,
	FRDGBufferRef& GatherPointsBuffer,
	FIntVector& GatherPointsResolution
)
#if RHI_RAYTRACING
{	
	RDG_GPU_STAT_SCOPE(GraphBuilder, RayTracingGICreateGatherPoints);
	RDG_EVENT_SCOPE(GraphBuilder, "Ray Tracing GI: Create Gather Points");

	int32 GatherSamples = FMath::Min(GetRayTracingGlobalIlluminationSamplesPerPixel(View), MAXIMUM_GATHER_POINTS_PER_PIXEL);
	int32 SamplesPerPixel = 1;

	// Determine the local neighborhood for a shared sample sequence
	int32 GatherFilterWidth = FMath::Max(CVarRayTracingGlobalIlluminationFinalGatherFilterWidth.GetValueOnRenderThread(), 0);
	GatherFilterWidth = GatherFilterWidth * 2 + 1;


	float MaxShadowDistance = 1.0e27;
	if (GRayTracingGlobalIlluminationMaxShadowDistance > 0.0)
	{
		MaxShadowDistance = GRayTracingGlobalIlluminationMaxShadowDistance;
	}
	else if (Scene->SkyLight)
	{
		// Adjust ray TMax so shadow rays do not hit the sky sphere 
		MaxShadowDistance = FMath::Max(0.0, 0.99 * Scene->SkyLight->SkyDistanceThreshold);
	}

	FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters>();
	PassParameters->SampleIndex = SampleIndex;
	PassParameters->GatherSamplesPerPixel = GatherSamples;
	PassParameters->GatherPointIteration = 0;
	PassParameters->SamplesPerPixel = SamplesPerPixel;
	PassParameters->GatherFilterWidth = GatherFilterWidth;
	PassParameters->MaxBounces = 1;
	PassParameters->MaxNormalBias = GetRaytracingMaxNormalBias();
	PassParameters->MaxRayDistanceForGI = GRayTracingGlobalIlluminationMaxRayDistance;
	PassParameters->MaxShadowDistance = MaxShadowDistance;
	PassParameters->EvalSkyLight = GRayTracingGlobalIlluminationEvalSkyLight != 0;
	PassParameters->UseRussianRoulette = GRayTracingGlobalIlluminationUseRussianRoulette != 0;
	PassParameters->DiffuseThreshold = GRayTracingGlobalIlluminationDiffuseThreshold;
	PassParameters->NextEventEstimationSamples = GRayTracingGlobalIlluminationNextEventEstimationSamples;
	PassParameters->UpscaleFactor = UpscaleFactor;
	PassParameters->RenderTileOffsetX = 0;
	PassParameters->RenderTileOffsetY = 0;

	// Global
	PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;

	// Light data
	SetupLightParameters(Scene, View, GraphBuilder, &PassParameters->SceneLights, &PassParameters->SceneLightCount, &PassParameters->SkylightParameters);
	PassParameters->SceneTextures = SceneTextures;

	// Output
	FIntPoint DispatchResolution = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), UpscaleFactor);
	FIntVector LocalGatherPointsResolution(DispatchResolution.X, DispatchResolution.Y, GatherSamples);
	if (GatherPointsResolution != LocalGatherPointsResolution)
	{
		GatherPointsResolution = LocalGatherPointsResolution;
		FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FGatherPoint), GatherPointsResolution.X * GatherPointsResolution.Y * GatherPointsResolution.Z);
		GatherPointsBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("GatherPointsBuffer"), ERDGBufferFlags::MultiFrame);
	}
	else
	{
		GatherPointsBuffer = GraphBuilder.RegisterExternalBuffer(((FSceneViewState*)View.State)->GatherPointsBuffer, TEXT("GatherPointsBuffer"));
	}
	PassParameters->GatherPointsResolution = FIntPoint(GatherPointsResolution.X, GatherPointsResolution.Y);
	PassParameters->RWGatherPointsBuffer = GraphBuilder.CreateUAV(GatherPointsBuffer, EPixelFormat::PF_R32_UINT);

	// When deferred materials are used, two passes are invoked:
	// 1) Gather ray-hit data and sort by hit-shader ID
	// 2) Re-trace "short" ray and shade
	const bool bSortMaterials = CVarRayTracingGlobalIlluminationFinalGatherSortMaterials.GetValueOnRenderThread() != 0;
	if (!bSortMaterials)
	{
		FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters* GatherPassParameters = PassParameters;
		//if (GatherPointIteration != 0)
		if (false)
		{
			GatherPassParameters = GraphBuilder.AllocParameters<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters>();
			CopyGatherPassParameters(*PassParameters, GatherPassParameters);
		}

		FRayTracingGlobalIlluminationCreateGatherPointsRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FEnableTwoSidedGeometryDim>(CVarRayTracingGlobalIlluminationEnableTwoSidedGeometry.GetValueOnRenderThread() != 0);
		PermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FEnableTransmissionDim>(CVarRayTracingGlobalIlluminationEnableTransmission.GetValueOnRenderThread());
		TShaderMapRef<FRayTracingGlobalIlluminationCreateGatherPointsRGS> RayGenerationShader(GetGlobalShaderMap(FeatureLevel), PermutationVector);
		ClearUnusedGraphResources(RayGenerationShader, GatherPassParameters);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("GatherPoints %d%d", GatherPointsResolution.X, GatherPointsResolution.Y),
			GatherPassParameters,
			ERDGPassFlags::Compute,
			[GatherPassParameters, this, &View, RayGenerationShader, GatherPointsResolution](FRHIRayTracingCommandList& RHICmdList)
		{
			FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenerationShader, *GatherPassParameters);
			RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, GatherPointsResolution.X, GatherPointsResolution.Y);
		});
	}
	else
	{
		// Determines tile-size for sorted-deferred path
		const int32 SortTileSize = CVarRayTracingGlobalIlluminationFinalGatherSortTileSize.GetValueOnRenderThread();
		FIntPoint TileAlignedResolution = FIntPoint(GatherPointsResolution.X, GatherPointsResolution.Y);
		if (SortTileSize)
		{
			TileAlignedResolution = FIntPoint::DivideAndRoundUp(TileAlignedResolution, SortTileSize) * SortTileSize;
		}
		PassParameters->TileAlignedResolution = TileAlignedResolution;
		PassParameters->SortTileSize = SortTileSize;

		FRDGBufferRef DeferredMaterialBuffer = nullptr;
		const uint32 DeferredMaterialBufferNumElements = TileAlignedResolution.X * TileAlignedResolution.Y;

		// Gather pass
		{
			FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FParameters* GatherPassParameters = GraphBuilder.AllocParameters<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FParameters>();
			CopyGatherPassParameters(*PassParameters, GatherPassParameters);

			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FDeferredMaterialPayload), DeferredMaterialBufferNumElements);
			DeferredMaterialBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("RayTracingGlobalIlluminationMaterialBuffer"));
			GatherPassParameters->MaterialBuffer = GraphBuilder.CreateUAV(DeferredMaterialBuffer);

			FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FEnableTwoSidedGeometryDim>(CVarRayTracingGlobalIlluminationEnableTwoSidedGeometry.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Gather);
			TShaderMapRef<FRayTracingGlobalIlluminationCreateGatherPointsTraceRGS> RayGenerationShader(GetGlobalShaderMap(FeatureLevel), PermutationVector);

			ClearUnusedGraphResources(RayGenerationShader, GatherPassParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("GlobalIlluminationRayTracingGatherMaterials %dx%d", TileAlignedResolution.X, TileAlignedResolution.Y),
				GatherPassParameters,
				ERDGPassFlags::Compute,
				[GatherPassParameters, this, &View, RayGenerationShader, TileAlignedResolution](FRHIRayTracingCommandList& RHICmdList)
			{
				FRayTracingPipelineState* Pipeline = View.RayTracingMaterialGatherPipeline;

				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenerationShader, *GatherPassParameters);

				FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
				RHICmdList.RayTraceDispatch(Pipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, TileAlignedResolution.X, TileAlignedResolution.Y);
			});
		}

		// Sort by hit-shader ID
		const uint32 SortSize = CVarRayTracingGlobalIlluminationFinalGatherSortSize.GetValueOnRenderThread();
		SortDeferredMaterials(GraphBuilder, View, SortSize, DeferredMaterialBufferNumElements, DeferredMaterialBuffer);

		// Shade pass
		{
			FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters* GatherPassParameters = PassParameters;
			//if (GatherPointIteration != 0)
			if (false)
			{
				GatherPassParameters = GraphBuilder.AllocParameters<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FParameters>();
				CopyGatherPassParameters(*PassParameters, GatherPassParameters);
			}

			GatherPassParameters->MaterialBuffer = GraphBuilder.CreateUAV(DeferredMaterialBuffer);

			FRayTracingGlobalIlluminationCreateGatherPointsRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FEnableTwoSidedGeometryDim>(CVarRayTracingGlobalIlluminationEnableTwoSidedGeometry.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FDeferredMaterialMode>(EDeferredMaterialMode::Shade);
			PermutationVector.Set<FRayTracingGlobalIlluminationCreateGatherPointsRGS::FEnableTransmissionDim>(CVarRayTracingGlobalIlluminationEnableTransmission.GetValueOnRenderThread());
			TShaderMapRef<FRayTracingGlobalIlluminationCreateGatherPointsRGS> RayGenerationShader(GetGlobalShaderMap(FeatureLevel), PermutationVector);
			ClearUnusedGraphResources(RayGenerationShader, GatherPassParameters);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("GlobalIlluminationRayTracingShadeMaterials %d", DeferredMaterialBufferNumElements),
				GatherPassParameters,
				ERDGPassFlags::Compute,
				[GatherPassParameters, this, &View, RayGenerationShader, DeferredMaterialBufferNumElements](FRHIRayTracingCommandList& RHICmdList)
			{
				FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();
				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenerationShader, *GatherPassParameters);

				// Shading pass for sorted materials uses 1D dispatch over all elements in the material buffer.
				// This can be reduced to the number of output pixels if sorting pass guarantees that all invalid entries are moved to the end.
				RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, DeferredMaterialBufferNumElements, 1);
			});
		}
	}
}
#else
{
	unimplemented();
}
#endif

void FDeferredShadingSceneRenderer::RenderRayTracingGlobalIlluminationFinalGather(
	FRDGBuilder& GraphBuilder,
	FSceneTextureParameters& SceneTextures,
	FViewInfo& View,
	const IScreenSpaceDenoiser::FAmbientOcclusionRayTracingConfig& RayTracingConfig,
	int32 UpscaleFactor,
	// Output
	IScreenSpaceDenoiser::FDiffuseIndirectInputs* OutDenoiserInputs)
#if RHI_RAYTRACING
{
	int32 SamplesPerPixel = FMath::Min(GetRayTracingGlobalIlluminationSamplesPerPixel(View), MAXIMUM_GATHER_POINTS_PER_PIXEL);

	int32 GatherPointIterations = FMath::Max(CVarRayTracingGlobalIlluminationFinalGatherIterations.GetValueOnRenderThread(), 1);
	GatherPointIterations = FMath::Min(GatherPointIterations, SamplesPerPixel);

	// Generate gather points
	FRDGBufferRef GatherPointsBuffer;
	FSceneViewState* SceneViewState = (FSceneViewState*)View.State;
	int32 SampleIndex = View.ViewState->FrameIndex % ((SamplesPerPixel - 1) / GatherPointIterations + 1);
	SampleIndex *= GatherPointIterations;

	int GatherPointIteration = 0;
	do
	{
		int32 MultiSampleIndex = (SampleIndex + GatherPointIteration) % SamplesPerPixel;
		RayTracingGlobalIlluminationCreateGatherPoints(GraphBuilder, SceneTextures, View, UpscaleFactor, MultiSampleIndex, GatherPointsBuffer, SceneViewState->GatherPointsResolution);
		GatherPointIteration++;
	} while (GatherPointIteration < GatherPointIterations);

	// Perform gather
	RDG_GPU_STAT_SCOPE(GraphBuilder, RayTracingGIFinalGather);
	RDG_EVENT_SCOPE(GraphBuilder, "Ray Tracing GI: Final Gather");

	FRayTracingGlobalIlluminationFinalGatherRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingGlobalIlluminationFinalGatherRGS::FParameters>();
	PassParameters->SampleIndex = SampleIndex;
	PassParameters->SamplesPerPixel = SamplesPerPixel;
	PassParameters->GatherPointIterations = GatherPointIterations;

	// Determine the local neighborhood for a shared sample sequence
	int32 GatherFilterWidth = FMath::Max(CVarRayTracingGlobalIlluminationFinalGatherFilterWidth.GetValueOnRenderThread(), 0);
	GatherFilterWidth = GatherFilterWidth * 2 + 1;
	PassParameters->GatherFilterWidth = GatherFilterWidth;
	PassParameters->UseFireflySuppression = CVarRayTracingGlobalIlluminationFireflySuppression.GetValueOnRenderThread() != 0;

	PassParameters->DiffuseThreshold = GRayTracingGlobalIlluminationDiffuseThreshold;
	PassParameters->MaxNormalBias = GetRaytracingMaxNormalBias();
	PassParameters->FinalGatherDistance = GRayTracingGlobalIlluminationFinalGatherDistance;
	PassParameters->DepthRejectionKernel = CVarRayTracingGlobalIlluminationFinalGatherDepthRejectionKernel.GetValueOnRenderThread();
	PassParameters->NormalRejectionKernel = CVarRayTracingGlobalIlluminationFinalGatherNormalRejectionKernel.GetValueOnRenderThread();
	PassParameters->UpscaleFactor = UpscaleFactor;
	PassParameters->RenderTileOffsetX = 0;
	PassParameters->RenderTileOffsetY = 0;

	// Cache current view matrix for gather point reprojection
	for (int GatherPointIterationLocal = 0; GatherPointIterationLocal < GatherPointIterations; ++GatherPointIterationLocal)
	{
		int32 EntryIndex = (SampleIndex + GatherPointIterationLocal) % SamplesPerPixel;
		View.ViewState->GatherPointsViewHistory[EntryIndex] = View.ViewMatrices.GetViewProjectionMatrix();
	}

	// Build gather point reprojection buffer
	FGatherPointData GatherPointData;
	GatherPointData.Count = SamplesPerPixel;
	for (int ViewHistoryIndex = 0; ViewHistoryIndex < MAXIMUM_GATHER_POINTS_PER_PIXEL; ViewHistoryIndex++)
	{
		GatherPointData.ViewMatrices[ViewHistoryIndex] = FMatrix44f(View.ViewState->GatherPointsViewHistory[ViewHistoryIndex]);			// LWC_TODO: Precision
	}
	PassParameters->GatherPointData = CreateUniformBufferImmediate(GatherPointData, EUniformBufferUsage::UniformBuffer_SingleDraw);

	// Scene data
	PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;

	// Shading data
	PassParameters->SceneTextures = SceneTextures;

	// Gather points
	PassParameters->GatherPointsResolution = FIntPoint(SceneViewState->GatherPointsResolution.X, SceneViewState->GatherPointsResolution.Y);
	PassParameters->GatherPointsBuffer = GraphBuilder.CreateSRV(GatherPointsBuffer);

	// Output
	PassParameters->RWGlobalIlluminationUAV = GraphBuilder.CreateUAV(OutDenoiserInputs->Color);
	PassParameters->RWGlobalIlluminationRayDistanceUAV = GraphBuilder.CreateUAV(OutDenoiserInputs->RayHitDistance);

	FRayTracingGlobalIlluminationFinalGatherRGS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRayTracingGlobalIlluminationFinalGatherRGS::FEnableTwoSidedGeometryDim>(CVarRayTracingGlobalIlluminationEnableTwoSidedGeometry.GetValueOnRenderThread() != 0);
	PermutationVector.Set<FRayTracingGlobalIlluminationFinalGatherRGS::FEnableNeighborVisibilityTestDim>(CVarRayTracingGlobalIlluminationFinalGatherEnableNeighborVisbilityTest.GetValueOnRenderThread() != 0);
	TShaderMapRef<FRayTracingGlobalIlluminationFinalGatherRGS> RayGenerationShader(GetGlobalShaderMap(FeatureLevel), PermutationVector);
	ClearUnusedGraphResources(RayGenerationShader, PassParameters);

	FIntPoint RayTracingResolution = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), UpscaleFactor);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("GlobalIlluminationRayTracing %dx%d", RayTracingResolution.X, RayTracingResolution.Y),
		PassParameters,
		ERDGPassFlags::Compute,
		[PassParameters, this, &View, RayGenerationShader, RayTracingResolution](FRHIRayTracingCommandList& RHICmdList)
	{
		FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();

		FRayTracingShaderBindingsWriter GlobalResources;
		SetShaderParameters(GlobalResources, RayGenerationShader, *PassParameters);
		RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, RayTracingResolution.X, RayTracingResolution.Y);
	});


	GraphBuilder.QueueBufferExtraction(GatherPointsBuffer, &SceneViewState->GatherPointsBuffer, ERHIAccess::SRVMask);
}
#else
{
	unimplemented();
}
#endif

void FDeferredShadingSceneRenderer::RenderRayTracingGlobalIlluminationBruteForce(
	FRDGBuilder& GraphBuilder,
	FSceneTextureParameters& SceneTextures,
	FViewInfo& View,
	const IScreenSpaceDenoiser::FAmbientOcclusionRayTracingConfig& RayTracingConfig,
	int32 UpscaleFactor,
	IScreenSpaceDenoiser::FDiffuseIndirectInputs* OutDenoiserInputs)
#if RHI_RAYTRACING
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, RayTracingGIBruteForce);
	RDG_EVENT_SCOPE(GraphBuilder, "Ray Tracing GI: Brute Force");

	int32 RayTracingGISamplesPerPixel = GetRayTracingGlobalIlluminationSamplesPerPixel(View);

	float MaxShadowDistance = 1.0e27;
	if (GRayTracingGlobalIlluminationMaxShadowDistance > 0.0)
	{
		MaxShadowDistance = GRayTracingGlobalIlluminationMaxShadowDistance;
	}
	else if (Scene->SkyLight)
	{
		// Adjust ray TMax so shadow rays do not hit the sky sphere 
		MaxShadowDistance = FMath::Max(0.0, 0.99 * Scene->SkyLight->SkyDistanceThreshold);
	}

	FGlobalIlluminationRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGlobalIlluminationRGS::FParameters>();
	PassParameters->SamplesPerPixel = RayTracingGISamplesPerPixel;
	int32 CVarRayTracingGlobalIlluminationMaxBouncesValue = CVarRayTracingGlobalIlluminationMaxBounces.GetValueOnRenderThread();
	PassParameters->MaxBounces = CVarRayTracingGlobalIlluminationMaxBouncesValue > -1? CVarRayTracingGlobalIlluminationMaxBouncesValue : View.FinalPostProcessSettings.RayTracingGIMaxBounces;
	PassParameters->MaxNormalBias = GetRaytracingMaxNormalBias();
	float MaxRayDistanceForGI = GRayTracingGlobalIlluminationMaxRayDistance;
	if (MaxRayDistanceForGI == -1.0)
	{
		MaxRayDistanceForGI = View.FinalPostProcessSettings.AmbientOcclusionRadius;
	}
	PassParameters->MaxRayDistanceForGI = MaxRayDistanceForGI;
	PassParameters->MaxRayDistanceForAO = View.FinalPostProcessSettings.AmbientOcclusionRadius;
	PassParameters->MaxShadowDistance = MaxShadowDistance;
	PassParameters->UpscaleFactor = UpscaleFactor;
	PassParameters->EvalSkyLight = GRayTracingGlobalIlluminationEvalSkyLight != 0;
	PassParameters->UseRussianRoulette = GRayTracingGlobalIlluminationUseRussianRoulette != 0;
	PassParameters->UseFireflySuppression = CVarRayTracingGlobalIlluminationFireflySuppression.GetValueOnRenderThread() != 0;
	PassParameters->DiffuseThreshold = GRayTracingGlobalIlluminationDiffuseThreshold;
	PassParameters->NextEventEstimationSamples = GRayTracingGlobalIlluminationNextEventEstimationSamples;
	PassParameters->TLAS = View.GetRayTracingSceneLayerViewChecked(ERayTracingSceneLayer::Base);
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	SetupLightParameters(Scene, View, GraphBuilder, &PassParameters->SceneLights, &PassParameters->SceneLightCount, &PassParameters->SkylightParameters);
	PassParameters->SceneTextures = SceneTextures;
	PassParameters->RWGlobalIlluminationUAV = GraphBuilder.CreateUAV(OutDenoiserInputs->Color);
	PassParameters->RWGlobalIlluminationRayDistanceUAV = GraphBuilder.CreateUAV(OutDenoiserInputs->RayHitDistance);
	PassParameters->RenderTileOffsetX = 0;
	PassParameters->RenderTileOffsetY = 0;

	FGlobalIlluminationRGS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FGlobalIlluminationRGS::FEnableTwoSidedGeometryDim>(CVarRayTracingGlobalIlluminationEnableTwoSidedGeometry.GetValueOnRenderThread() != 0);
	PermutationVector.Set<FGlobalIlluminationRGS::FEnableTransmissionDim>(CVarRayTracingGlobalIlluminationEnableTransmission.GetValueOnRenderThread());
	TShaderMapRef<FGlobalIlluminationRGS> RayGenerationShader(GetGlobalShaderMap(FeatureLevel), PermutationVector);
	ClearUnusedGraphResources(RayGenerationShader, PassParameters);

	FIntPoint RayTracingResolution = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), UpscaleFactor);

	if (GRayTracingGlobalIlluminationRenderTileSize <= 0)
	{
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("GlobalIlluminationRayTracing %dx%d", RayTracingResolution.X, RayTracingResolution.Y),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, this, &View, RayGenerationShader, RayTracingResolution](FRHIRayTracingCommandList& RHICmdList)
		{
			FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();

			FRayTracingShaderBindingsWriter GlobalResources;
			SetShaderParameters(GlobalResources, RayGenerationShader, *PassParameters);
			RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, RayTracingResolution.X, RayTracingResolution.Y);
		});
	}
	else
	{
		int32 RenderTileSize = FMath::Max(32, GRayTracingGlobalIlluminationRenderTileSize);
		int32 NumTilesX = FMath::DivideAndRoundUp(RayTracingResolution.X, RenderTileSize);
		int32 NumTilesY = FMath::DivideAndRoundUp(RayTracingResolution.Y, RenderTileSize);
		for (int32 Y = 0; Y < NumTilesY; ++Y)
		{
			for (int32 X = 0; X < NumTilesX; ++X)
			{
				FGlobalIlluminationRGS::FParameters* TilePassParameters = PassParameters;

				if (X > 0 || Y > 0)
				{
					TilePassParameters = GraphBuilder.AllocParameters<FGlobalIlluminationRGS::FParameters>();
					*TilePassParameters = *PassParameters;

					TilePassParameters->RenderTileOffsetX = X * RenderTileSize;
					TilePassParameters->RenderTileOffsetY = Y * RenderTileSize;
				}

				int32 DispatchSizeX = FMath::Min<int32>(RenderTileSize, RayTracingResolution.X - TilePassParameters->RenderTileOffsetX);
				int32 DispatchSizeY = FMath::Min<int32>(RenderTileSize, RayTracingResolution.Y - TilePassParameters->RenderTileOffsetY);

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("GlobalIlluminationRayTracing %dx%d (tile %dx%d)", DispatchSizeX, DispatchSizeY, X, Y),
					TilePassParameters,
					ERDGPassFlags::Compute,
					[TilePassParameters, this, &View, RayGenerationShader, DispatchSizeX, DispatchSizeY](FRHIRayTracingCommandList& RHICmdList)
				{
					FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();

					FRayTracingShaderBindingsWriter GlobalResources;
					SetShaderParameters(GlobalResources, RayGenerationShader, *TilePassParameters);
					RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, 
						GlobalResources, DispatchSizeX, DispatchSizeY);
					RHICmdList.SubmitCommandsHint();
				});
			}
		}
	}
}
#else
{
	unimplemented();
}
#endif // RHI_RAYTRACING
