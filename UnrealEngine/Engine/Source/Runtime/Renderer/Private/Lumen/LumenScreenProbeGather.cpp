// Copyright Epic Games, Inc. All Rights Reserved.

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

extern FLumenGatherCvarState GLumenGatherCvars;

int32 GLumenScreenProbeGather = 1;
FAutoConsoleVariableRef GVarLumenScreenProbeGather(
	TEXT("r.Lumen.ScreenProbeGather"),
	GLumenScreenProbeGather,
	TEXT("Whether to use the Screen Probe Final Gather"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

FAutoConsoleVariableRef CVarLumenScreenProbeGatherTraceMeshSDFs(
	TEXT("r.Lumen.ScreenProbeGather.TraceMeshSDFs"),
	GLumenGatherCvars.TraceMeshSDFs,
	TEXT("Whether to trace against Mesh Signed Distance fields for Lumen's Screen Probe Gather."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeGatherAdaptiveProbeMinDownsampleFactor = 4;
FAutoConsoleVariableRef GVarLumenScreenProbeGatherAdaptiveProbeMinDownsampleFactor(
	TEXT("r.Lumen.ScreenProbeGather.AdaptiveProbeMinDownsampleFactor"),
	GLumenScreenProbeGatherAdaptiveProbeMinDownsampleFactor,
	TEXT("Screen probes will be placed where needed down to this downsample factor of the GBuffer."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeGatherAdaptiveProbeAllocationFraction = .5f;
FAutoConsoleVariableRef GVarAdaptiveProbeAllocationFraction(
	TEXT("r.Lumen.ScreenProbeGather.AdaptiveProbeAllocationFraction"),
	GLumenScreenProbeGatherAdaptiveProbeAllocationFraction,
	TEXT("Fraction of uniform probes to allow for adaptive probe placement."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeGatherReferenceMode = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeGatherReferenceMode(
	TEXT("r.Lumen.ScreenProbeGather.ReferenceMode"),
	GLumenScreenProbeGatherReferenceMode,
	TEXT("When enabled, traces 1024 uniform rays per probe with no filtering, Importance Sampling or Radiance Caching."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeTracingOctahedronResolution = 8;
FAutoConsoleVariableRef GVarLumenScreenProbeTracingOctahedronResolution(
	TEXT("r.Lumen.ScreenProbeGather.TracingOctahedronResolution"),
	GLumenScreenProbeTracingOctahedronResolution,
	TEXT("Resolution of the tracing octahedron.  Determines how many traces are done per probe."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeGatherOctahedronResolutionScale = 1.0f;
FAutoConsoleVariableRef GVarLumenScreenProbeGatherOctahedronResolutionScale(
	TEXT("r.Lumen.ScreenProbeGather.GatherOctahedronResolutionScale"),
	GLumenScreenProbeGatherOctahedronResolutionScale,
	TEXT("Resolution that probe filtering and integration will happen at, as a scale of TracingOctahedronResolution"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeDownsampleFactor = 16;
FAutoConsoleVariableRef GVarLumenScreenProbeDownsampleFactor(
	TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"),
	GLumenScreenProbeDownsampleFactor,
	TEXT("Pixel size of the screen tile that a screen probe will be placed on."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

FAutoConsoleVariableRef CVarLumenScreenProbeGatherDirectLighting(
	TEXT("r.Lumen.ScreenProbeGather.DirectLighting"),
	GLumenGatherCvars.DirectLighting,
	TEXT("Whether to render all local lights through Lumen's Final Gather, when enabled.  This gives very cheap but low quality direct lighting."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeLightSampleResolutionXY = 2;
FAutoConsoleVariableRef CVarLumenScreenProbeLightSampleResolutionXY(
	TEXT("r.Lumen.ScreenProbeGather.LightSampleResolutionXY"),
	GLumenScreenProbeLightSampleResolutionXY,
	TEXT("Number of light samples per screen probe, in one dimension.  When the number of lights overlapping a pixel is larger, noise in the direct lighting will increase."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeInjectLightsToProbes = 0;
FAutoConsoleVariableRef CVarLumenScreenProbeInjectLightsToProbes(
	TEXT("r.Lumen.ScreenProbeGather.InjectLightsToProbes"),
	GLumenScreenProbeInjectLightsToProbes,
	TEXT("Whether to inject local lights into probes.  Experimental - fast but causes wrap-around lighting due to lack of directionality and SH ringing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeFullResolutionJitterWidth = 1;
FAutoConsoleVariableRef GVarLumenScreenProbeFullResolutionJitterWidth(
	TEXT("r.Lumen.ScreenProbeGather.FullResolutionJitterWidth"),
	GLumenScreenProbeFullResolutionJitterWidth,
	TEXT("Size of the full resolution jitter applied to Screen Probe upsampling, as a fraction of a screen tile.  A width of 1 results in jittering by DownsampleFactor number of pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeIntegrationTileClassification = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeIntegrationTileClassification(
	TEXT("r.Lumen.ScreenProbeGather.IntegrationTileClassification"),
	GLumenScreenProbeIntegrationTileClassification,
	TEXT("Whether to use tile classification during diffuse integration.  Tile Classification splits compute dispatches by VGPRs for better occupancy, but can introduce errors if implemented incorrectly."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeSupportTwoSidedFoliageBackfaceDiffuse = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeSupportBackfaceDiffuse(
	TEXT("r.Lumen.ScreenProbeGather.TwoSidedFoliageBackfaceDiffuse"),
	GLumenScreenProbeSupportTwoSidedFoliageBackfaceDiffuse,
	TEXT("Whether to gather lighting along the backface for the Two Sided Foliage shading model, which adds some GPU cost.  The final lighting is then DiffuseColor * FrontfaceLighting + SubsurfaceColor * BackfaceLighting.  When disabled, SubsurfaceColor will simply be added to DiffuseColor instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeDiffuseIntegralMethod = 0;
FAutoConsoleVariableRef CVarLumenScreenProbeDiffuseIntegralMethod(
	TEXT("r.Lumen.ScreenProbeGather.DiffuseIntegralMethod"),
	GLumenScreenProbeDiffuseIntegralMethod,
	TEXT("Spherical Harmonic = 0, Importance Sample BRDF = 1, Numerical Integral Reference = 2"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeMaterialAO = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeMaterialAO(
	TEXT("r.Lumen.ScreenProbeGather.MaterialAO"),
	GLumenScreenProbeMaterialAO,
	TEXT("Whether to apply Material Ambient Occlusion or Material Bent Normal to Lumen GI."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeTemporalFilter = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalFilter(
	TEXT("r.Lumen.ScreenProbeGather.Temporal"),
	GLumenScreenProbeTemporalFilter,
	TEXT("Whether to use a temporal filter"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeClearHistoryEveryFrame = 0;
FAutoConsoleVariableRef CVarLumenScreenProbeClearHistoryEveryFrame(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.ClearHistoryEveryFrame"),
	GLumenScreenProbeClearHistoryEveryFrame,
	TEXT("Whether to clear the history every frame for debugging"),
	ECVF_RenderThreadSafe
	);

float GLumenScreenProbeHistoryDistanceThreshold = .005f;
FAutoConsoleVariableRef CVarLumenScreenProbeHistoryDistanceThreshold(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.DistanceThreshold"),
	GLumenScreenProbeHistoryDistanceThreshold,
	TEXT("Relative distance threshold needed to discard last frame's lighting results.  Lower values reduce ghosting from characters when near a wall but increase flickering artifacts."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeFractionOfLightingMovingForFastUpdateMode = .1f;
FAutoConsoleVariableRef CVarLumenScreenProbeFractionOfLightingMovingForFastUpdateMode(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.FractionOfLightingMovingForFastUpdateMode"),
	GLumenScreenProbeFractionOfLightingMovingForFastUpdateMode,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeTemporalMaxFastUpdateModeAmount = .9f;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalMaxFastUpdateModeAmount(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFastUpdateModeAmount"),
	GLumenScreenProbeTemporalMaxFastUpdateModeAmount,
	TEXT("Maximum amount of fast-responding temporal filter to use when traces hit a moving object.  Values closer to 1 cause more noise, but also faster reaction to scene changes."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeTemporalFastUpdateModeUseNeighborhoodClamp = 0;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalFastUpdateModeUseNeighborhoodClamp(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.FastUpdateModeUseNeighborhoodClamp"),
	GLumenScreenProbeTemporalFastUpdateModeUseNeighborhoodClamp,
	TEXT("Whether to clamp history values to the current frame's screen space neighborhood, in areas around moving objects."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarLumenScreenProbeTemporalRejectBasedOnNormal(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.RejectBasedOnNormal"),
	0,
	TEXT("Whether to reject history lighting based on their normal.  Increases cost of the temporal filter but can reduce streaking especially around character feet."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeRelativeSpeedDifferenceToConsiderLightingMoving = .005f;
FAutoConsoleVariableRef CVarLumenScreenProbeRelativeSpeedDifferenceToConsiderLightingMoving(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.RelativeSpeedDifferenceToConsiderLightingMoving"),
	GLumenScreenProbeRelativeSpeedDifferenceToConsiderLightingMoving,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeTemporalMaxFramesAccumulated = 10.0f;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalMaxFramesAccumulated(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated"),
	GLumenScreenProbeTemporalMaxFramesAccumulated,
	TEXT("Lower values cause the temporal filter to propagate lighting changes faster, but also increase flickering from noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<int32> CVarLumenScreenProbeTemporalMaxRayDirections(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.MaxRayDirections"),
	8,
	TEXT("Number of possible random directions per pixel. Should be tweaked based on MaxFramesAccumulated."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

float GLumenScreenProbeTemporalHistoryNormalThreshold = 45.0f;
FAutoConsoleVariableRef CVarLumenScreenProbeTemporalHistoryNormalThreshold(
	TEXT("r.Lumen.ScreenProbeGather.Temporal.NormalThreshold"),
	GLumenScreenProbeTemporalHistoryNormalThreshold,
	TEXT("Maximum angle that the history texel's normal can be from the current pixel to accept it's history lighting, in degrees."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenScreenProbeScreenTracesThicknessScaleWhenNoFallback = 2;
FAutoConsoleVariableRef CVarLumenScreenProbeScreenTracesThicknessScaleWhenNoFallback(
	TEXT("r.Lumen.ScreenProbeGather.ScreenTraces.ThicknessScaleWhenNoFallback"),
	GLumenScreenProbeScreenTracesThicknessScaleWhenNoFallback,
	TEXT("Larger scales effectively treat depth buffer surfaces as thicker for screen traces when there is no Distance Field present to resume the occluded ray."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeSpatialFilter = 1;
FAutoConsoleVariableRef GVarLumenScreenProbeFilter(
	TEXT("r.Lumen.ScreenProbeGather.SpatialFilterProbes"),
	GLumenScreenProbeSpatialFilter,
	TEXT("Whether to spatially filter probe traces to reduce noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeTemporalFilterProbes = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeTemporalFilter(
	TEXT("r.Lumen.ScreenProbeGather.TemporalFilterProbes"),
	GLumenScreenProbeTemporalFilterProbes,
	TEXT("Whether to temporally filter probe traces to reduce noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenShortRangeAmbientOcclusion = 1;
FAutoConsoleVariableRef GVarLumenScreenSpaceShortRangeAO(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO"),
	GLumenShortRangeAmbientOcclusion,
	TEXT("Whether to compute a short range, full resolution AO to add high frequency occlusion (contact shadows) which Screen Probes lack due to downsampling."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenShortRangeAOApplyDuringIntegration = 0;
FAutoConsoleVariableRef CVarLumenShortRangeAOApplyDuringIntegration(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.ApplyDuringIntegration"),
	GLumenShortRangeAOApplyDuringIntegration,
	TEXT("Whether Screen Space Bent Normal should be applied during BRDF integration, which has higher quality but is before the temporal filter so causes streaking on moving objects."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeFixedJitterIndex = -1;
FAutoConsoleVariableRef CVarLumenScreenProbeUseJitter(
	TEXT("r.Lumen.ScreenProbeGather.FixedJitterIndex"),
	GLumenScreenProbeFixedJitterIndex,
	TEXT("If zero or greater, overrides the temporal jitter index with a fixed index.  Useful for debugging and inspecting sampling patterns."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenRadianceCache = 1;
FAutoConsoleVariableRef CVarRadianceCache(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache"),
	GLumenRadianceCache,
	TEXT("Whether to enable the Persistent world space Radiance Cache"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenScreenProbeIrradianceFormat = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeIrradianceFormat(
	TEXT("r.Lumen.ScreenProbeGather.IrradianceFormat"),
	GLumenScreenProbeIrradianceFormat,
	TEXT("Prefilter irradiance format\n")
	TEXT("0 - SH3 slower\n")
	TEXT("1 - Octahedral probe. Faster, but reverts to SH3 when ShortRangeAO.ApplyDuringIntegration is enabled"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeStochasticInterpolation = 1;
FAutoConsoleVariableRef CVarLumenScreenProbeStochasticInterpolation(
	TEXT("r.Lumen.ScreenProbeGather.StochasticInterpolation"),
	GLumenScreenProbeStochasticInterpolation,
	TEXT("Where to interpolate screen probes stochastically (1 sample) or bilinearly (4 samples)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular = .8f;
FAutoConsoleVariableRef GVarLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular(
	TEXT("r.Lumen.ScreenProbeGather.MaxRoughnessToEvaluateRoughSpecular"),
	GLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular,
	TEXT("Maximum roughness value to evaluate rough specular in Screen Probe Gather.  Lower values reduce GPU cost of integration, but also lose rough specular."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeRoughSpecularSamplingMode = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeRoughSpecularSamplingMode(
	TEXT("r.Lumen.ScreenProbeGather.RoughSpecularSamplingMode"),
	GLumenScreenProbeRoughSpecularSamplingMode,
	TEXT("Mode 0: use the diffuse SH sample as specular. Mode 1: sample the SH along the main GGX specular reflection vector."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenScreenProbeTileDebugMode = 0;
FAutoConsoleVariableRef GVarLumenScreenProbeTileDebugMode(
	TEXT("r.Lumen.ScreenProbeGather.TileDebugMode"),
	GLumenScreenProbeTileDebugMode,
	TEXT("Display Lumen screen probe tile classification."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace LumenScreenProbeGather 
{
	int32 GetTracingOctahedronResolution(const FViewInfo& View)
	{
		const float SqrtQuality = FMath::Sqrt(FMath::Max(View.FinalPostProcessSettings.LumenFinalGatherQuality, 0.0f));
		const int32 TracingOctahedronResolution = FMath::Clamp(FMath::RoundToInt(SqrtQuality * GLumenScreenProbeTracingOctahedronResolution), 4, 16);
		ensureMsgf(IsProbeTracingResolutionSupportedForImportanceSampling(TracingOctahedronResolution), TEXT("Tracing resolution %u requested that is not supported by importance sampling"), TracingOctahedronResolution);
		return GLumenScreenProbeGatherReferenceMode ? 32 : TracingOctahedronResolution;
	}

	int32 GetGatherOctahedronResolution(int32 TracingOctahedronResolution)
	{
		if (GLumenScreenProbeGatherReferenceMode)
		{
			return 8;
		}

		if (GLumenScreenProbeGatherOctahedronResolutionScale >= 1.0f)
		{
			const int32 Multiplier = FMath::RoundToInt(GLumenScreenProbeGatherOctahedronResolutionScale);
			return TracingOctahedronResolution * Multiplier;
		}
		else
		{
			const int32 Divisor = FMath::RoundToInt(1.0f / FMath::Max(GLumenScreenProbeGatherOctahedronResolutionScale, .1f));
			return TracingOctahedronResolution / Divisor;
		}
	}
	
	int32 GetScreenDownsampleFactor(const FViewInfo& View)
	{
		if (GLumenScreenProbeGatherReferenceMode)
		{
			return 16;
		}

		return FMath::Clamp(GLumenScreenProbeDownsampleFactor / (View.FinalPostProcessSettings.LumenFinalGatherQuality >= 6.0f ? 2 : 1), 4, 64);
	}

	bool UseShortRangeAmbientOcclusion(const FEngineShowFlags& ShowFlags)
	{
		return GLumenScreenProbeGatherReferenceMode ? false : (GLumenShortRangeAmbientOcclusion != 0 && ShowFlags.LumenShortRangeAmbientOcclusion);
	}

	bool ApplyShortRangeAODuringIntegration()
	{
		return GLumenShortRangeAOApplyDuringIntegration != 0;
	}

	bool UseProbeSpatialFilter()
	{
		return GLumenScreenProbeGatherReferenceMode ? false : GLumenScreenProbeSpatialFilter != 0;
	}

	bool UseProbeTemporalFilter()
	{
		return GLumenScreenProbeGatherReferenceMode ? false : GLumenScreenProbeTemporalFilterProbes != 0;
	}

	bool UseRadianceCache(const FViewInfo& View)
	{
		return GLumenScreenProbeGatherReferenceMode ? false : GLumenRadianceCache != 0;
	}

	int32 GetDiffuseIntegralMethod()
	{
		return GLumenScreenProbeGatherReferenceMode ? 2 : GLumenScreenProbeDiffuseIntegralMethod;
	}

	EScreenProbeIrradianceFormat GetScreenProbeIrradianceFormat(const FEngineShowFlags& ShowFlags)
	{
		const bool bApplyShortRangeAO = UseShortRangeAmbientOcclusion(ShowFlags) && ApplyShortRangeAODuringIntegration();
		if (bApplyShortRangeAO)
		{
			// At the moment only SH3 support bent normal path
			return EScreenProbeIrradianceFormat::SH3;
		}

		return (EScreenProbeIrradianceFormat)FMath::Clamp(GLumenScreenProbeIrradianceFormat, 0, 1);
	}

	float GetScreenProbeFullResolutionJitterWidth(const FViewInfo& View)
	{
		return GLumenScreenProbeFullResolutionJitterWidth * (View.FinalPostProcessSettings.LumenFinalGatherQuality >= 4.0f ? .5f : 1.0f);
	}

	bool UseRejectBasedOnNormal()
	{
		return GLumenScreenProbeGather != 0
			&& CVarLumenScreenProbeTemporalRejectBasedOnNormal.GetValueOnRenderThread() != 0;
	}
}

int32 GRadianceCacheNumClipmaps = 4;
FAutoConsoleVariableRef CVarRadianceCacheNumClipmaps(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumClipmaps"),
	GRadianceCacheNumClipmaps,
	TEXT("Number of radiance cache clipmaps."),
	ECVF_RenderThreadSafe
);

float GLumenRadianceCacheClipmapWorldExtent = 2500.0f;
FAutoConsoleVariableRef CVarLumenRadianceCacheClipmapWorldExtent(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ClipmapWorldExtent"),
	GLumenRadianceCacheClipmapWorldExtent,
	TEXT("World space extent of the first clipmap"),
	ECVF_RenderThreadSafe
);

float GLumenRadianceCacheClipmapDistributionBase = 2.0f;
FAutoConsoleVariableRef CVarLumenRadianceCacheClipmapDistributionBase(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ClipmapDistributionBase"),
	GLumenRadianceCacheClipmapDistributionBase,
	TEXT("Base of the Pow() that controls the size of each successive clipmap relative to the first."),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheNumProbesToTraceBudget = 300;
FAutoConsoleVariableRef CVarRadianceCacheNumProbesToTraceBudget(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumProbesToTraceBudget"),
	GRadianceCacheNumProbesToTraceBudget,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GRadianceCacheGridResolution = 48;
FAutoConsoleVariableRef CVarRadianceCacheResolution(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.GridResolution"),
	GRadianceCacheGridResolution,
	TEXT("Resolution of the probe placement grid within each clipmap"),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheProbeResolution = 32;
FAutoConsoleVariableRef CVarRadianceCacheProbeResolution(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution"),
	GRadianceCacheProbeResolution,
	TEXT("Resolution of the probe's 2d radiance layout.  The number of rays traced for the probe will be ProbeResolution ^ 2"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GRadianceCacheNumMipmaps = 1;
FAutoConsoleVariableRef CVarRadianceCacheNumMipmaps(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.NumMipmaps"),
	GRadianceCacheNumMipmaps,
	TEXT("Number of radiance cache mipmaps."),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheProbeAtlasResolutionInProbes = 128;
FAutoConsoleVariableRef CVarRadianceCacheProbeAtlasResolutionInProbes(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ProbeAtlasResolutionInProbes"),
	GRadianceCacheProbeAtlasResolutionInProbes,
	TEXT("Number of probes along one dimension of the probe atlas cache texture.  This controls the memory usage of the cache.  Overflow currently results in incorrect rendering."),
	ECVF_RenderThreadSafe
);

float GRadianceCacheReprojectionRadiusScale = 1.5f;
FAutoConsoleVariableRef CVarRadianceCacheProbeReprojectionRadiusScale(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ReprojectionRadiusScale"),
	GRadianceCacheReprojectionRadiusScale,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheStats = 0;
FAutoConsoleVariableRef CVarRadianceCacheStats(
	TEXT("r.Lumen.ScreenProbeGather.RadianceCache.Stats"),
	GRadianceCacheStats,
	TEXT("GPU print out Radiance Cache update stats."),
	ECVF_RenderThreadSafe
);

namespace LumenScreenProbeGatherRadianceCache
{
	int32 GetNumClipmaps()
	{
		return FMath::Clamp(GRadianceCacheNumClipmaps, 1, LumenRadianceCache::MaxClipmaps);
	}

	int32 GetClipmapGridResolution()
	{
		const int32 GridResolution = GRadianceCacheGridResolution / (GLumenFastCameraMode ? 2 : 1);
		return FMath::Clamp(GridResolution, 1, 256);
	}

	int32 GetProbeResolution()
	{
		return GRadianceCacheProbeResolution / (GLumenFastCameraMode ? 2 : 1);
	}

	int32 GetFinalProbeResolution()
	{
		return GetProbeResolution() + 2 * (1 << (GRadianceCacheNumMipmaps - 1));
	}

	FIntVector GetProbeIndirectionTextureSize()
	{
		return FIntVector(GetClipmapGridResolution() * GRadianceCacheNumClipmaps, GetClipmapGridResolution(), GetClipmapGridResolution());
	}

	FIntPoint GetProbeAtlasTextureSize()
	{
		return FIntPoint(GRadianceCacheProbeAtlasResolutionInProbes * GetProbeResolution());
	}

	FIntPoint GetFinalRadianceAtlasTextureSize()
	{
		return FIntPoint(GRadianceCacheProbeAtlasResolutionInProbes * GetFinalProbeResolution(), GRadianceCacheProbeAtlasResolutionInProbes * GetFinalProbeResolution());
	}

	int32 GetMaxNumProbes()
	{
		return GRadianceCacheProbeAtlasResolutionInProbes * GRadianceCacheProbeAtlasResolutionInProbes;
	}

	LumenRadianceCache::FRadianceCacheInputs SetupRadianceCacheInputs(const FViewInfo& View)
	{
		LumenRadianceCache::FRadianceCacheInputs Parameters = LumenRadianceCache::GetDefaultRadianceCacheInputs();
		Parameters.ReprojectionRadiusScale = GRadianceCacheReprojectionRadiusScale;
		Parameters.ClipmapWorldExtent = GLumenRadianceCacheClipmapWorldExtent;
		Parameters.ClipmapDistributionBase = GLumenRadianceCacheClipmapDistributionBase;
		Parameters.RadianceProbeClipmapResolution = GetClipmapGridResolution();
		Parameters.ProbeAtlasResolutionInProbes = FIntPoint(GRadianceCacheProbeAtlasResolutionInProbes, GRadianceCacheProbeAtlasResolutionInProbes);
		Parameters.NumRadianceProbeClipmaps = GetNumClipmaps();
		Parameters.RadianceProbeResolution = FMath::Max(GetProbeResolution(), LumenRadianceCache::MinRadianceProbeResolution);
		Parameters.FinalProbeResolution = GetFinalProbeResolution();
		Parameters.FinalRadianceAtlasMaxMip = GRadianceCacheNumMipmaps - 1;
		const float LightingUpdateSpeed = FMath::Clamp(View.FinalPostProcessSettings.LumenFinalGatherLightingUpdateSpeed, .5f, 4.0f);
		const float EditingBudgetScale = View.Family->bCurrentlyBeingEdited ? 10.0f : 1.0f;
		Parameters.NumProbesToTraceBudget = FMath::RoundToInt(GRadianceCacheNumProbesToTraceBudget * LightingUpdateSpeed * EditingBudgetScale);
		Parameters.RadianceCacheStats = GRadianceCacheStats;
		return Parameters;
	}
};

class FScreenProbeDownsampleDepthUniformCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeDownsampleDepthUniformCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeDownsampleDepthUniformCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWScreenProbeSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeWorldSpeed)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeTranslatedWorldPosition)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeDownsampleDepthUniformCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeDownsampleDepthUniformCS", SF_Compute);


class FScreenProbeAdaptivePlacementCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeAdaptivePlacementCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeAdaptivePlacementCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWScreenProbeWorldSpeed)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeTranslatedWorldPosition)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumAdaptiveScreenProbes)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWAdaptiveScreenProbeData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWScreenTileAdaptiveProbeHeader)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWScreenTileAdaptiveProbeIndices)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER(uint32, PlacementDownsampleFactor)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeAdaptivePlacementCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeAdaptivePlacementCS", SF_Compute);

class FSetupAdaptiveProbeIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSetupAdaptiveProbeIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FSetupAdaptiveProbeIndirectArgsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWScreenProbeIndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSetupAdaptiveProbeIndirectArgsCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "SetupAdaptiveProbeIndirectArgsCS", SF_Compute);


class FMarkRadianceProbesUsedByScreenProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMarkRadianceProbesUsedByScreenProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FMarkRadianceProbesUsedByScreenProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FMarkRadianceProbesUsedByScreenProbesCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "MarkRadianceProbesUsedByScreenProbesCS", SF_Compute);

class FMarkRadianceProbesUsedByHairStrandsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMarkRadianceProbesUsedByHairStrandsCS)
	SHADER_USE_PARAMETER_STRUCT(FMarkRadianceProbesUsedByHairStrandsCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, HairStrandsResolution)
		SHADER_PARAMETER(FVector2f, HairStrandsInvResolution)
		SHADER_PARAMETER(uint32, HairStrandsMip)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
		RDG_BUFFER_ACCESS(IndirectBufferArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
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

IMPLEMENT_GLOBAL_SHADER(FMarkRadianceProbesUsedByHairStrandsCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "MarkRadianceProbesUsedByHairStrandsCS", SF_Compute);

class FScreenProbeGenerateLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeGenerateLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeGenerateLightSamplesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWScreenProbeLightSampleDirection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWScreenProbeLightSampleFlags)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWScreenProbeLightSampleRadiance)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, Forward)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeGenerateLightSamplesCS, "/Engine/Private/Lumen/LumenScreenProbeLightSampling.usf", "ScreenProbeGenerateLightSamplesCS", SF_Compute);

// Must match usf INTEGRATE_TILE_SIZE
const int32 GScreenProbeIntegrateTileSize = 8;

class FScreenProbeTileClassificationMarkCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeTileClassificationMarkCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeTileClassificationMarkCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWBackfaceDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWRoughSpecularIndirect)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIntegrateIndirectArgs)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>, RWTileClassificationModes)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenReflections::FCompositeParameters, ReflectionsCompositeParameters)
		SHADER_PARAMETER(uint32, DefaultDiffuseIntegrationMethod)
		SHADER_PARAMETER(float, MaxRoughnessToEvaluateRoughSpecular)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflowTile>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	class FSupportBackfaceDiffuse : SHADER_PERMUTATION_BOOL("SUPPORT_BACKFACE_DIFFUSE");
	using FPermutationDomain = TShaderPermutationDomain<FOverflowTile, FSupportBackfaceDiffuse>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeTileClassificationMarkCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeTileClassificationMarkCS", SF_Compute);


class FScreenProbeTileClassificationBuildListsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeTileClassificationBuildListsCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeTileClassificationBuildListsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIntegrateIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWIntegrateTileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, TileClassificationModes)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER(FIntPoint, ViewportTileDimensions)
		SHADER_PARAMETER(FIntPoint, ViewportTileDimensionsWithOverflow)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflowTile>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	using FPermutationDomain = TShaderPermutationDomain<FOverflowTile>;

	static int32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeTileClassificationBuildListsCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeTileClassificationBuildListsCS", SF_Compute);


class FScreenProbeIntegrateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeIntegrateCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeIntegrateCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float4>, RWDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWBackfaceDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWRoughSpecularIndirect)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, IntegrateTileData)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeGatherParameters, GatherParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenScreenSpaceBentNormalParameters, ScreenSpaceBentNormalParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenReflections::FCompositeParameters, ReflectionsCompositeParameters)
		SHADER_PARAMETER(float, FullResolutionJitterWidth)
		SHADER_PARAMETER(float, MaxRoughnessToEvaluateRoughSpecular)
		SHADER_PARAMETER(uint32, ApplyMaterialAO)
		SHADER_PARAMETER(float, MaxAOMultibounceAlbedo)
		SHADER_PARAMETER(uint32, LumenReflectionInputIsSSR)
		SHADER_PARAMETER(uint32, DefaultDiffuseIntegrationMethod)
		SHADER_PARAMETER(FIntPoint, ViewportTileDimensions)
		SHADER_PARAMETER(FIntPoint, ViewportTileDimensionsWithOverflow)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflowTile>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	class FShortRangeAO : SHADER_PERMUTATION_BOOL("SHORT_RANGE_AO");
	class FTileClassificationMode : SHADER_PERMUTATION_INT("INTEGRATE_TILE_CLASSIFICATION_MODE", 4);
	class FProbeIrradianceFormat : SHADER_PERMUTATION_ENUM_CLASS("PROBE_IRRADIANCE_FORMAT", EScreenProbeIrradianceFormat);
	class FStochasticProbeInterpolation : SHADER_PERMUTATION_BOOL("STOCHASTIC_PROBE_INTERPOLATION");
	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	class FDirectLighting : SHADER_PERMUTATION_BOOL("SUPPORT_DIRECT_LIGHTING");
	class FSupportBackfaceDiffuse : SHADER_PERMUTATION_BOOL("SUPPORT_BACKFACE_DIFFUSE");
	class FRoughSpecularSamplingMode : SHADER_PERMUTATION_INT("ROUGH_SPECULAR_SAMPLING_MODE", 2);
	using FPermutationDomain = TShaderPermutationDomain<FTileClassificationMode, FShortRangeAO, FProbeIrradianceFormat, FStochasticProbeInterpolation, FOverflowTile, FDirectLighting, FSupportBackfaceDiffuse, FRoughSpecularSamplingMode>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenProbeIntegrateCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeIntegrateCS", SF_Compute);


class FScreenProbeTemporalReprojectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenProbeTemporalReprojectionCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenProbeTemporalReprojectionCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWNewHistoryDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWNewHistoryBackfaceDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWNewHistoryRoughSpecularIndirect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, RWNumHistoryFramesAccumulated)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, RWNewHistoryFastUpdateMode)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, DiffuseIndirectHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, BackfaceDiffuseIndirectHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, RoughSpecularIndirectHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, HistoryNumFramesAccumulated)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, FastUpdateModeHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffuseIndirectDepthHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffuseIndirectNormalHistory)
		SHADER_PARAMETER(float,HistoryDistanceThreshold)
		SHADER_PARAMETER(float,PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER(float,InvFractionOfLightingMovingForFastUpdateMode)
		SHADER_PARAMETER(float,MaxFastUpdateModeAmount)
		SHADER_PARAMETER(float,MaxFramesAccumulated)
		SHADER_PARAMETER(float,HistoryNormalCosThreshold)
		SHADER_PARAMETER(FVector4f,HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f,HistoryUVToScreenPositionScaleBias)
		SHADER_PARAMETER(FVector4f,HistoryUVMinMax)
		SHADER_PARAMETER(FIntVector4,HistoryViewportMinMax)
		SHADER_PARAMETER(uint32, bIsSubstrateTileHistoryValid)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, DiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, BackfaceDiffuseIndirect)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, RoughSpecularIndirect)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FHistoryRejectBasedOnNormal : SHADER_PERMUTATION_BOOL("HISTORY_REJECT_BASED_ON_NORMAL");
	class FFastUpdateModeNeighborhoodClamp : SHADER_PERMUTATION_BOOL("FAST_UPDATE_MODE_NEIGHBORHOOD_CLAMP");
	class FOverflowTile : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE");
	class FSupportBackfaceDiffuse : SHADER_PERMUTATION_BOOL("SUPPORT_BACKFACE_DIFFUSE");
	using FPermutationDomain = TShaderPermutationDomain<FFastUpdateModeNeighborhoodClamp, FHistoryRejectBasedOnNormal, FOverflowTile, FSupportBackfaceDiffuse>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const bool bCompile = DoesPlatformSupportLumenGI(Parameters.Platform);

#if WITH_EDITOR
		if (bCompile)
		{
			ensureMsgf(VelocityEncodeDepth(Parameters.Platform), TEXT("Platform did not return true from VelocityEncodeDepth().  Lumen requires velocity depth."));
		}
#endif
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflowTile>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
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

IMPLEMENT_GLOBAL_SHADER(FScreenProbeTemporalReprojectionCS, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeTemporalReprojectionCS", SF_Compute);

class FLumenScreenProbeSubstrateDebugPass : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenScreenProbeSubstrateDebugPass)
	SHADER_USE_PARAMETER_STRUCT(FLumenScreenProbeSubstrateDebugPass, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, LayerCount)
		SHADER_PARAMETER(FIntPoint, ViewportIntegrateTileDimensions)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, IntegrateTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, IntegrateIndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.SetDefine(TEXT("SHADER_MATERIAL_DEBUG"), 1); 
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenScreenProbeSubstrateDebugPass, "/Engine/Private/Lumen/LumenScreenProbeGather.usf", "ScreenProbeDebugMain", SF_Compute);

void AddLumenScreenProbeDebugPass(
	FRDGBuilder& GraphBuilder, 
	FViewInfo& View,
	const FIntPoint& ViewportIntegrateTileDimensions,
	const FIntPoint& ViewportIntegrateTileDimensionsWithOverflow,
	FRDGBufferRef IntegrateTileData,
	FRDGBufferRef IntegrateIndirectArgs)
{
	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);

	ShaderPrint::RequestSpaceForCharacters(1024);
	ShaderPrint::RequestSpaceForLines(1024);
	ShaderPrint::RequestSpaceForTriangles(ViewportIntegrateTileDimensionsWithOverflow.X * ViewportIntegrateTileDimensionsWithOverflow.Y * 2);

	FLumenScreenProbeSubstrateDebugPass::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenScreenProbeSubstrateDebugPass::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
	PassParameters->LayerCount = Substrate::GetSubstrateMaxClosureCount(View);
	PassParameters->ViewportIntegrateTileDimensions = ViewportIntegrateTileDimensions;
	PassParameters->IntegrateTileData = GraphBuilder.CreateSRV(IntegrateTileData);
	PassParameters->IntegrateIndirectArgs = GraphBuilder.CreateSRV(IntegrateIndirectArgs, PF_R32_UINT);
	ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrint);

	FLumenScreenProbeSubstrateDebugPass::FPermutationDomain PermutationVector;
	auto ComputeShader = View.ShaderMap->GetShader<FLumenScreenProbeSubstrateDebugPass>(PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ScreenProbeDebug"),
		ComputeShader,
		PassParameters,
		FIntVector(ViewportIntegrateTileDimensions.X, ViewportIntegrateTileDimensions.Y, 1));
}

const TCHAR* GetClassificationModeString(EScreenProbeIntegrateTileClassification Mode)
{
	if (Mode == EScreenProbeIntegrateTileClassification::SimpleDiffuse)
	{
		return TEXT("SimpleDiffuse");
	}
	else if (Mode == EScreenProbeIntegrateTileClassification::SupportImportanceSampleBRDF)
	{
		return TEXT("SupportImportanceSampleBRDF");
	}
	else if (Mode == EScreenProbeIntegrateTileClassification::SupportAll)
	{
		return TEXT("SupportAll");
	}

	return TEXT("");
}

extern float GLumenMaxShortRangeAOMultibounceAlbedo;

void InterpolateAndIntegrate(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	FViewInfo& View,
	FScreenProbeParameters ScreenProbeParameters,
	FScreenProbeGatherParameters GatherParameters,
	FLumenScreenSpaceBentNormalParameters ScreenSpaceBentNormalParameters,
	bool bRenderDirectLighting,
	bool bSSREnabled,
	FRDGTextureRef DiffuseIndirect,
	FRDGTextureRef BackfaceDiffuseIndirect,
	FRDGTextureRef RoughSpecularIndirect,
	ERDGPassFlags ComputePassFlags)
{
	const bool bApplyShortRangeAO = ScreenSpaceBentNormalParameters.UseShortRangeAO != 0 && LumenScreenProbeGather::ApplyShortRangeAODuringIntegration();
	const bool bUseTileClassification = GLumenScreenProbeIntegrationTileClassification != 0 && LumenScreenProbeGather::GetDiffuseIntegralMethod() != 2;
	const bool bSupportBackfaceDiffuse = BackfaceDiffuseIndirect != nullptr;
	const int32 RoughSpecularSamplingMode = GLumenScreenProbeRoughSpecularSamplingMode > 0 ? 1 : 0;

	LumenReflections::FCompositeParameters ReflectionsCompositeParameters;
	LumenReflections::SetupCompositeParameters(View, ReflectionsCompositeParameters);

	if (bSSREnabled)
	{
		// SSR may not have a hit for any pixel, we need to have rough reflections to fall back to
		ReflectionsCompositeParameters.MaxRoughnessToTrace = -.1f;
	}

	if (bUseTileClassification)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Integrate");

		const uint32 ClassificationScaleFactor = Substrate::IsSubstrateEnabled() ? 2u : 1u;
		FRDGBufferRef IntegrateIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(ClassificationScaleFactor * (uint32)EScreenProbeIntegrateTileClassification::Num), TEXT("Lumen.ScreenProbeGather.IntegrateIndirectArgs"));
		if (Substrate::IsSubstrateEnabled())
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(IntegrateIndirectArgs, PF_R32_UINT), 0u);
		}

		const FIntPoint ViewportIntegrateTileDimensions(
			FMath::DivideAndRoundUp(View.ViewRect.Size().X, GScreenProbeIntegrateTileSize), 
			FMath::DivideAndRoundUp(View.ViewRect.Size().Y, GScreenProbeIntegrateTileSize));

		checkf(ViewportIntegrateTileDimensions.X > 0 && ViewportIntegrateTileDimensions.Y > 0, TEXT("Compute shader needs non-zero dispatch to clear next pass's indirect args"));

		const FIntPoint EffectiveBufferResolution = Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
		const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);
		const FIntPoint TileClassificationBufferDimensions(
			FMath::DivideAndRoundUp(EffectiveBufferResolution.X, GScreenProbeIntegrateTileSize),
			FMath::DivideAndRoundUp(EffectiveBufferResolution.Y, GScreenProbeIntegrateTileSize));

		FRDGTextureDesc TileClassificationModesDesc = FRDGTextureDesc::Create2DArray(TileClassificationBufferDimensions, PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
		FRDGTextureRef TileClassificationModes = GraphBuilder.CreateTexture(TileClassificationModesDesc, TEXT("Lumen.ScreenProbeGather.TileClassificationModes"));

		{
			FRDGTextureUAVRef RWDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef RWBackfaceDiffuseIndirect = bSupportBackfaceDiffuse ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(BackfaceDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
			FRDGTextureUAVRef RWRoughSpecularIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RoughSpecularIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGBufferUAVRef RWIntegrateIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(IntegrateIndirectArgs, PF_R32_UINT), ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGTextureUAVRef RWTileClassificationModes = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TileClassificationModes), ERDGUnorderedAccessViewFlags::SkipBarrier);

			auto ScreenProbeTileClassificationMark = [&](bool bOverflow)
			{
				FScreenProbeTileClassificationMarkCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeTileClassificationMarkCS::FParameters>();
				PassParameters->RWDiffuseIndirect = RWDiffuseIndirect;
				PassParameters->RWBackfaceDiffuseIndirect = RWBackfaceDiffuseIndirect;
				PassParameters->RWRoughSpecularIndirect = RWRoughSpecularIndirect;
				PassParameters->RWIntegrateIndirectArgs = RWIntegrateIndirectArgs;
				PassParameters->RWTileClassificationModes = RWTileClassificationModes;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
				PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
				PassParameters->DefaultDiffuseIntegrationMethod = (uint32)LumenScreenProbeGather::GetDiffuseIntegralMethod();
				PassParameters->ReflectionsCompositeParameters = ReflectionsCompositeParameters;
				PassParameters->MaxRoughnessToEvaluateRoughSpecular = GLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular;

				FScreenProbeTileClassificationMarkCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FScreenProbeTileClassificationMarkCS::FOverflowTile>(bOverflow);
				PermutationVector.Set<FScreenProbeTileClassificationMarkCS::FSupportBackfaceDiffuse>(bSupportBackfaceDiffuse);
				auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeTileClassificationMarkCS>(PermutationVector);

				if (bOverflow)
				{
					PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTileDispatchIndirectBuffer;
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("TileClassificationMark(Overflow)"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						View.SubstrateViewData.ClosureTileDispatchIndirectBuffer,
						0u);
				}
				else
				{
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("TileClassificationMark"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						FIntVector(ViewportIntegrateTileDimensions.X, ViewportIntegrateTileDimensions.Y, 1));
				}
			};
		
			ScreenProbeTileClassificationMark(false);
			if (Substrate::IsSubstrateEnabled())
			{
				ScreenProbeTileClassificationMark(true);
			}
		}

		FRDGBufferRef IntegrateTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ClassificationScaleFactor * TileClassificationBufferDimensions.X * TileClassificationBufferDimensions.Y * (uint32)EScreenProbeIntegrateTileClassification::Num), TEXT("Lumen.ScreenProbeGather.IntegrateTileData"));

		{
			FRDGBufferUAVRef RWIntegrateIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(IntegrateIndirectArgs, PF_R32_UINT), ERDGUnorderedAccessViewFlags::SkipBarrier );
			FRDGBufferUAVRef RWIntegrateTileData = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(IntegrateTileData), ERDGUnorderedAccessViewFlags::SkipBarrier);

			auto ScreenProbeTileClassificationBuildLists = [&](bool bOverflow)
			{
				FScreenProbeTileClassificationBuildListsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeTileClassificationBuildListsCS::FParameters>();
				PassParameters->RWIntegrateIndirectArgs = RWIntegrateIndirectArgs;
				PassParameters->RWIntegrateTileData = RWIntegrateTileData;
				PassParameters->TileClassificationModes = TileClassificationModes;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
				PassParameters->ViewportTileDimensions = ViewportIntegrateTileDimensions;
				PassParameters->ViewportTileDimensionsWithOverflow = TileClassificationBufferDimensions;

				FScreenProbeTileClassificationBuildListsCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FScreenProbeTileClassificationBuildListsCS::FOverflowTile>(bOverflow);
				auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeTileClassificationBuildListsCS>(PermutationVector);

				if (bOverflow)
				{
					PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTilePerThreadDispatchIndirectBuffer;
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("TileClassificationBuildLists(Overflow)"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						View.SubstrateViewData.ClosureTilePerThreadDispatchIndirectBuffer, 0u);
				}
				else
				{
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("TileClassificationBuildLists"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						FComputeShaderUtils::GetGroupCount(ViewportIntegrateTileDimensions, 8));
				}
			};
		
			ScreenProbeTileClassificationBuildLists(false);
			if (Substrate::IsSubstrateEnabled())
			{
				ScreenProbeTileClassificationBuildLists(true);
			}
		}

		// Allow integration passes to overlap
		FRDGTextureUAVRef DiffuseIndirectUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef BackfaceDiffuseIndirectUAV = bSupportBackfaceDiffuse ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(BackfaceDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
		FRDGTextureUAVRef RoughSpecularIndirectUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RoughSpecularIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);

		for (uint32 ClassificationMode = 0; ClassificationMode < (uint32)EScreenProbeIntegrateTileClassification::Num; ClassificationMode++)
		{
			auto ScreenProbeIntegrate = [&](bool bOverflow)
			{
				FScreenProbeIntegrateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeIntegrateCS::FParameters>();
				PassParameters->RWDiffuseIndirect = DiffuseIndirectUAV;
				PassParameters->RWBackfaceDiffuseIndirect = BackfaceDiffuseIndirectUAV;
				PassParameters->RWRoughSpecularIndirect = RoughSpecularIndirectUAV;
				PassParameters->IntegrateTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(IntegrateTileData));
				PassParameters->GatherParameters = GatherParameters;
				PassParameters->ScreenProbeParameters = ScreenProbeParameters;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
				PassParameters->FullResolutionJitterWidth = LumenScreenProbeGather::GetScreenProbeFullResolutionJitterWidth(View);
				PassParameters->ReflectionsCompositeParameters = ReflectionsCompositeParameters;
				PassParameters->MaxRoughnessToEvaluateRoughSpecular = GLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular;
				PassParameters->ApplyMaterialAO = GLumenScreenProbeMaterialAO;
				PassParameters->MaxAOMultibounceAlbedo = GLumenMaxShortRangeAOMultibounceAlbedo;
				PassParameters->LumenReflectionInputIsSSR = bSSREnabled ? 1 : 0;
				PassParameters->ScreenSpaceBentNormalParameters = ScreenSpaceBentNormalParameters;
				PassParameters->DefaultDiffuseIntegrationMethod = (uint32)LumenScreenProbeGather::GetDiffuseIntegralMethod();
				PassParameters->ViewportTileDimensions = ViewportIntegrateTileDimensions;
				PassParameters->ViewportTileDimensionsWithOverflow = TileClassificationBufferDimensions;
				PassParameters->IndirectArgs = IntegrateIndirectArgs;
				PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

				FScreenProbeIntegrateCS::FPermutationDomain PermutationVector;
				PermutationVector.Set< FScreenProbeIntegrateCS::FOverflowTile >(bOverflow);
				PermutationVector.Set< FScreenProbeIntegrateCS::FTileClassificationMode >(ClassificationMode);
				PermutationVector.Set< FScreenProbeIntegrateCS::FShortRangeAO >(bApplyShortRangeAO);
				PermutationVector.Set< FScreenProbeIntegrateCS::FProbeIrradianceFormat >(LumenScreenProbeGather::GetScreenProbeIrradianceFormat(View.Family->EngineShowFlags));
				PermutationVector.Set< FScreenProbeIntegrateCS::FStochasticProbeInterpolation >(GLumenScreenProbeStochasticInterpolation != 0);
				PermutationVector.Set< FScreenProbeIntegrateCS::FDirectLighting >(bRenderDirectLighting && !GLumenScreenProbeInjectLightsToProbes);
				PermutationVector.Set< FScreenProbeIntegrateCS::FSupportBackfaceDiffuse >(bSupportBackfaceDiffuse);
				PermutationVector.Set< FScreenProbeIntegrateCS::FRoughSpecularSamplingMode >(RoughSpecularSamplingMode);
				auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeIntegrateCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("%s%s", GetClassificationModeString((EScreenProbeIntegrateTileClassification)ClassificationMode), bOverflow ? TEXT("(Overflow)") : TEXT("")),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					IntegrateIndirectArgs,
					((bOverflow ? uint32(EScreenProbeIntegrateTileClassification::Num) : 0u) + ClassificationMode) * sizeof(FRHIDispatchIndirectParameters));
			};

			ScreenProbeIntegrate(false);
			if (Substrate::IsSubstrateEnabled())
			{
				ScreenProbeIntegrate(true);
			}
		}

		// Debug pass
		if (GLumenScreenProbeTileDebugMode > 0)
		{
			AddLumenScreenProbeDebugPass(GraphBuilder, View, ViewportIntegrateTileDimensions, TileClassificationBufferDimensions, IntegrateTileData, IntegrateIndirectArgs);
		}
	}
	else // No tile classification
	{	
		const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);

		auto ScreenProbeIntegrate = [&](bool bOverflow)
		{
			FScreenProbeIntegrateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeIntegrateCS::FParameters>();
			PassParameters->RWDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseIndirect));
			PassParameters->RWBackfaceDiffuseIndirect = bSupportBackfaceDiffuse ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(BackfaceDiffuseIndirect)) : nullptr;
			PassParameters->RWRoughSpecularIndirect =  GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RoughSpecularIndirect));
			PassParameters->GatherParameters = GatherParameters;

			const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
			if (!PassParameters->GatherParameters.ScreenProbeRadianceSHAmbient)
			{
				PassParameters->GatherParameters.ScreenProbeRadianceSHAmbient = SystemTextures.Black;
				PassParameters->GatherParameters.ScreenProbeRadianceSHDirectional = SystemTextures.Black;
			}

			PassParameters->ScreenProbeParameters = ScreenProbeParameters;
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->FullResolutionJitterWidth = LumenScreenProbeGather::GetScreenProbeFullResolutionJitterWidth(View);
			PassParameters->ReflectionsCompositeParameters = ReflectionsCompositeParameters;
			PassParameters->MaxRoughnessToEvaluateRoughSpecular = GLumenScreenProbeMaxRoughnessToEvaluateRoughSpecular;
			PassParameters->ApplyMaterialAO = GLumenScreenProbeMaterialAO;
			PassParameters->MaxAOMultibounceAlbedo = GLumenMaxShortRangeAOMultibounceAlbedo;
			PassParameters->ScreenSpaceBentNormalParameters = ScreenSpaceBentNormalParameters;
			PassParameters->DefaultDiffuseIntegrationMethod = (uint32)LumenScreenProbeGather::GetDiffuseIntegralMethod();
			PassParameters->ViewportTileDimensions = FIntPoint(0, 0);
			PassParameters->ViewportTileDimensionsWithOverflow = FIntPoint(0, 0);
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

			FScreenProbeIntegrateCS::FPermutationDomain PermutationVector;
			PermutationVector.Set< FScreenProbeIntegrateCS::FOverflowTile >(bOverflow);
			PermutationVector.Set< FScreenProbeIntegrateCS::FTileClassificationMode >((uint32)EScreenProbeIntegrateTileClassification::Num);
			PermutationVector.Set< FScreenProbeIntegrateCS::FShortRangeAO >(bApplyShortRangeAO);
			PermutationVector.Set< FScreenProbeIntegrateCS::FProbeIrradianceFormat >(LumenScreenProbeGather::GetScreenProbeIrradianceFormat(View.Family->EngineShowFlags));
			PermutationVector.Set< FScreenProbeIntegrateCS::FStochasticProbeInterpolation >(GLumenScreenProbeStochasticInterpolation != 0);
			PermutationVector.Set< FScreenProbeIntegrateCS::FDirectLighting >(bRenderDirectLighting && !GLumenScreenProbeInjectLightsToProbes);
			PermutationVector.Set< FScreenProbeIntegrateCS::FSupportBackfaceDiffuse >(bSupportBackfaceDiffuse);
			PermutationVector.Set< FScreenProbeIntegrateCS::FRoughSpecularSamplingMode >(RoughSpecularSamplingMode);
			auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeIntegrateCS>(PermutationVector);

			const FIntPoint DispatchViewRect = View.ViewRect.Size();
			FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(DispatchViewRect, GScreenProbeIntegrateTileSize);
			DispatchCount.Z = ClosureCount;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Integrate%s", bOverflow ? TEXT("(Overflow)") : TEXT("")),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				DispatchCount);
		};

		ScreenProbeIntegrate(false);
		if (Substrate::IsSubstrateEnabled())
		{
			ScreenProbeIntegrate(true);
		}

	}
}

void UpdateHistoryScreenProbeGather(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FSceneTextures& SceneTextures,
	bool bPropagateGlobalLightingChange,
	FRDGTextureRef& DiffuseIndirect,
	FRDGTextureRef& BackfaceDiffuseIndirect,
	FRDGTextureRef& RoughSpecularIndirect,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);
	
	if (View.ViewState)
	{
		FScreenProbeGatherTemporalState& ScreenProbeGatherState = View.ViewState->Lumen.ScreenProbeGatherState;
		TRefCountPtr<IPooledRenderTarget>* DiffuseIndirectHistoryState = &ScreenProbeGatherState.DiffuseIndirectHistoryRT;
		TRefCountPtr<IPooledRenderTarget>* RoughSpecularIndirectHistoryState = &ScreenProbeGatherState.RoughSpecularIndirectHistoryRT;
		FIntRect* DiffuseIndirectHistoryViewRect = &ScreenProbeGatherState.DiffuseIndirectHistoryViewRect;
		FVector4f* DiffuseIndirectHistoryScreenPositionScaleBias = &ScreenProbeGatherState.DiffuseIndirectHistoryScreenPositionScaleBias;
		TRefCountPtr<IPooledRenderTarget>* HistoryNumFramesAccumulated = &ScreenProbeGatherState.NumFramesAccumulatedRT;
		TRefCountPtr<IPooledRenderTarget>* FastUpdateModeHistoryState = &ScreenProbeGatherState.FastUpdateModeHistoryRT;

		FRDGTextureRef OldNormalHistory = View.ViewState->Lumen.NormalHistoryRT ? GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.NormalHistoryRT) : nullptr;

		const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);
		const bool bRejectBasedOnNormal = LumenScreenProbeGather::UseRejectBasedOnNormal() && OldNormalHistory;
		const bool bSupportBackfaceDiffuse = BackfaceDiffuseIndirect != nullptr;
		const bool bOverflowTileHistoryValid = Substrate::IsSubstrateEnabled() ? ClosureCount == ScreenProbeGatherState.HistorySubstrateMaxClosureCount : true;

		ensureMsgf(SceneTextures.Velocity->Desc.Format != PF_G16R16, TEXT("Lumen requires 3d velocity.  Update Velocity format code."));

		// If the scene render targets reallocate, toss the history so we don't read uninitialized data
		const FIntPoint EffectiveResolution = Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
		const FIntPoint HistoryEffectiveResolution = ScreenProbeGatherState.HistoryEffectiveResolution;
		const bool bSceneTextureExtentMatchHistory = ScreenProbeGatherState.HistorySceneTexturesExtent == SceneTextures.Config.Extent;

		const FIntRect NewHistoryViewRect = View.ViewRect;

		if (*DiffuseIndirectHistoryState
			&& (!bSupportBackfaceDiffuse || ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT)
			&& !View.bCameraCut 
			&& !View.bPrevTransformsReset
			&& !GLumenScreenProbeClearHistoryEveryFrame
			&& bSceneTextureExtentMatchHistory
			&& ScreenProbeGatherState.LumenGatherCvars == GLumenGatherCvars
			&& !bPropagateGlobalLightingChange)
		{
			FRDGTextureDesc DiffuseIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
			FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);

			FRDGTextureRef NewDiffuseIndirect = GraphBuilder.CreateTexture(DiffuseIndirectDesc, TEXT("Lumen.ScreenProbeGather.DiffuseIndirect"));
			FRDGTextureRef NewBackfaceDiffuseIndirect = bSupportBackfaceDiffuse ? GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("Lumen.ScreenProbeGather.BackfaceDiffuseIndirect")) : nullptr;

			FRDGTextureRef OldDiffuseIndirectHistory = GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.DiffuseIndirectHistoryRT);
			FRDGTextureRef OldBackfaceDiffuseIndirectHistory = bSupportBackfaceDiffuse ? GraphBuilder.RegisterExternalTexture(ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT) : nullptr;
			FRDGTextureRef NewRoughSpecularIndirect = GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("Lumen.ScreenProbeGather.RoughSpecularIndirect"));

			FRDGTextureDesc NumHistoryFramesAccumulatedDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_R8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
			FRDGTextureRef NewNumHistoryFramesAccumulated = GraphBuilder.CreateTexture(NumHistoryFramesAccumulatedDesc, TEXT("Lumen.ScreenProbeGather.NumHistoryFramesAccumulated"));
			FRDGTextureRef NewHistoryFastUpdateMode = GraphBuilder.CreateTexture(NumHistoryFramesAccumulatedDesc, TEXT("Lumen.ScreenProbeGather.FastUpdateMode"));

			{
				FRDGTextureRef OldRoughSpecularIndirectHistory = GraphBuilder.RegisterExternalTexture(*RoughSpecularIndirectHistoryState);
				FRDGTextureRef OldDepthHistory = View.ViewState->Lumen.DepthHistoryRT ? GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.DepthHistoryRT) : SceneTextures.Depth.Target;
				FRDGTextureRef OldHistoryNumFramesAccumulated = GraphBuilder.RegisterExternalTexture(*HistoryNumFramesAccumulated);
				FRDGTextureRef OldFastUpdateModeHistory = GraphBuilder.RegisterExternalTexture(*FastUpdateModeHistoryState);

				{
					FRDGTextureUAVRef RWNewHistoryDiffuseIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
					FRDGTextureUAVRef RWNewHistoryBackfaceDiffuseIndirect = bSupportBackfaceDiffuse ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewBackfaceDiffuseIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
					FRDGTextureUAVRef RWNewHistoryRoughSpecularIndirect = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewRoughSpecularIndirect), ERDGUnorderedAccessViewFlags::SkipBarrier);
					FRDGTextureUAVRef RWNumHistoryFramesAccumulated = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewNumHistoryFramesAccumulated), ERDGUnorderedAccessViewFlags::SkipBarrier);
					FRDGTextureUAVRef RWNewHistoryFastUpdateMode = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NewHistoryFastUpdateMode), ERDGUnorderedAccessViewFlags::SkipBarrier);

					auto ScreenProbeTemporalReprojection = [&](bool bOverflow)
					{
						FScreenProbeTemporalReprojectionCS::FPermutationDomain PermutationVector;
						PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FOverflowTile>(bOverflow);
						PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FFastUpdateModeNeighborhoodClamp>(GLumenScreenProbeTemporalFastUpdateModeUseNeighborhoodClamp != 0);
						PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FHistoryRejectBasedOnNormal>(bRejectBasedOnNormal);
						PermutationVector.Set< FScreenProbeTemporalReprojectionCS::FSupportBackfaceDiffuse >(bSupportBackfaceDiffuse);
						auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeTemporalReprojectionCS>(PermutationVector);

						FScreenProbeTemporalReprojectionCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeTemporalReprojectionCS::FParameters>();
						PassParameters->RWNewHistoryDiffuseIndirect = RWNewHistoryDiffuseIndirect;
						PassParameters->RWNewHistoryBackfaceDiffuseIndirect = RWNewHistoryBackfaceDiffuseIndirect;
						PassParameters->RWNewHistoryRoughSpecularIndirect = RWNewHistoryRoughSpecularIndirect;
						PassParameters->RWNumHistoryFramesAccumulated = RWNumHistoryFramesAccumulated;
						PassParameters->RWNewHistoryFastUpdateMode = RWNewHistoryFastUpdateMode;

						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
						PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
						PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);

						PassParameters->DiffuseIndirectHistory = OldDiffuseIndirectHistory;
						PassParameters->BackfaceDiffuseIndirectHistory = OldBackfaceDiffuseIndirectHistory;
						PassParameters->RoughSpecularIndirectHistory = OldRoughSpecularIndirectHistory;
						PassParameters->HistoryNumFramesAccumulated = OldHistoryNumFramesAccumulated;
						PassParameters->FastUpdateModeHistory = OldFastUpdateModeHistory;
						PassParameters->DiffuseIndirectDepthHistory = OldDepthHistory;
						PassParameters->DiffuseIndirectNormalHistory = OldNormalHistory;

						PassParameters->HistoryDistanceThreshold = GLumenScreenProbeHistoryDistanceThreshold;
						PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
						PassParameters->InvFractionOfLightingMovingForFastUpdateMode = 1.0f / FMath::Max(GLumenScreenProbeFractionOfLightingMovingForFastUpdateMode, .001f);
						PassParameters->MaxFastUpdateModeAmount = GLumenScreenProbeTemporalMaxFastUpdateModeAmount;
						PassParameters->bIsSubstrateTileHistoryValid = bOverflowTileHistoryValid ? 1u : 0u;

						const float MaxFramesAccumulatedScale = 1.0f / FMath::Sqrt(FMath::Clamp(View.FinalPostProcessSettings.LumenFinalGatherLightingUpdateSpeed, .5f, 8.0f));
						const float EditingScale = View.Family->bCurrentlyBeingEdited ? .5f : 1.0f;
						PassParameters->MaxFramesAccumulated = FMath::RoundToInt(GLumenScreenProbeTemporalMaxFramesAccumulated * MaxFramesAccumulatedScale * EditingScale);
						PassParameters->HistoryNormalCosThreshold = FMath::Cos(GLumenScreenProbeTemporalHistoryNormalThreshold * (float)PI / 180.0f);
						PassParameters->HistoryScreenPositionScaleBias = *DiffuseIndirectHistoryScreenPositionScaleBias;

						const FVector2f HistoryUVToScreenPositionScale(1.0f / PassParameters->HistoryScreenPositionScaleBias.X, 1.0f / PassParameters->HistoryScreenPositionScaleBias.Y);
						const FVector2f HistoryUVToScreenPositionBias = -FVector2f(PassParameters->HistoryScreenPositionScaleBias.W, PassParameters->HistoryScreenPositionScaleBias.Z) * HistoryUVToScreenPositionScale;
						PassParameters->HistoryUVToScreenPositionScaleBias = FVector4f(HistoryUVToScreenPositionScale, HistoryUVToScreenPositionBias);

						// History uses HistoryDepth which has the same resolution than SceneTextures (no extented/overflow space)
						const FIntPoint BufferSize = SceneTextures.Config.Extent;
						const FVector2D InvBufferSize(1.0f / BufferSize.X, 1.0f / BufferSize.Y);

						// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
						PassParameters->HistoryUVMinMax = FVector4f(
							(DiffuseIndirectHistoryViewRect->Min.X + 0.5f) * InvBufferSize.X,
							(DiffuseIndirectHistoryViewRect->Min.Y + 0.5f) * InvBufferSize.Y,
							(DiffuseIndirectHistoryViewRect->Max.X - 1.0f) * InvBufferSize.X,
							(DiffuseIndirectHistoryViewRect->Max.Y - 1.0f) * InvBufferSize.Y);

						PassParameters->HistoryViewportMinMax = FIntVector4(
							DiffuseIndirectHistoryViewRect->Min.X,
							DiffuseIndirectHistoryViewRect->Min.Y,
							DiffuseIndirectHistoryViewRect->Max.X,
							DiffuseIndirectHistoryViewRect->Max.Y);

						PassParameters->DiffuseIndirect = DiffuseIndirect;
						PassParameters->BackfaceDiffuseIndirect = BackfaceDiffuseIndirect;
						PassParameters->RoughSpecularIndirect = RoughSpecularIndirect;

						// SUBSTRATE_TODO: Reenable once history tracking is correct
						#if 0
						if (bOverflow)
						{
							PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTileDispatchIndirectBuffer;
							FComputeShaderUtils::AddPass(
								GraphBuilder,
								RDG_EVENT_NAME("TemporalReprojection(Overflow)"),
								ComputePassFlags,
								ComputeShader,
								PassParameters,
								View.SubstrateViewData.ClosureTileDispatchIndirectBuffer,
								0u);
						}
						else
						#endif
						{
							FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FScreenProbeTemporalReprojectionCS::GetGroupSize());
							DispatchCount.Z = Substrate::IsSubstrateEnabled() ? ClosureCount : 1u;
							FComputeShaderUtils::AddPass(
								GraphBuilder,
								RDG_EVENT_NAME("TemporalReprojection(%ux%u)", View.ViewRect.Width(), View.ViewRect.Height()),
								ComputePassFlags,
								ComputeShader,
								PassParameters,
								DispatchCount);
						}
					};

					ScreenProbeTemporalReprojection(false);
					// SUBSTRATE_TODO: Reenable once history tracking is correct
					#if 0
					if (Substrate::IsSubstrateEnabled())
					{
						ScreenProbeTemporalReprojection(true);
					}
					#endif
				}
				if (!View.bStatePrevViewInfoIsReadOnly)
				{
					// Queue updating the view state's render target reference with the new history
					GraphBuilder.QueueTextureExtraction(NewDiffuseIndirect, &ScreenProbeGatherState.DiffuseIndirectHistoryRT);

					if (bSupportBackfaceDiffuse)
					{
						GraphBuilder.QueueTextureExtraction(NewBackfaceDiffuseIndirect, &ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT);
					}
					else
					{
						ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT = nullptr;
					}
					
					GraphBuilder.QueueTextureExtraction(NewRoughSpecularIndirect, RoughSpecularIndirectHistoryState);
					GraphBuilder.QueueTextureExtraction(NewNumHistoryFramesAccumulated, HistoryNumFramesAccumulated);
					GraphBuilder.QueueTextureExtraction(NewHistoryFastUpdateMode, FastUpdateModeHistoryState);
				}
			}

			RoughSpecularIndirect = NewRoughSpecularIndirect;
			DiffuseIndirect = NewDiffuseIndirect;
			BackfaceDiffuseIndirect = NewBackfaceDiffuseIndirect;
		}
		else
		{
			if (!View.bStatePrevViewInfoIsReadOnly)
			{
				// Queue updating the view state's render target reference with the new values
				GraphBuilder.QueueTextureExtraction(DiffuseIndirect, &ScreenProbeGatherState.DiffuseIndirectHistoryRT);

				if (bSupportBackfaceDiffuse)
				{
					GraphBuilder.QueueTextureExtraction(BackfaceDiffuseIndirect, &ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT);
				}
				else
				{
					ScreenProbeGatherState.BackfaceDiffuseIndirectHistoryRT = nullptr;
				}

				GraphBuilder.QueueTextureExtraction(RoughSpecularIndirect, RoughSpecularIndirectHistoryState);
				*HistoryNumFramesAccumulated = GSystemTextures.BlackArrayDummy;
				*FastUpdateModeHistoryState = GSystemTextures.BlackArrayDummy;
			}
		}

		if (!View.bStatePrevViewInfoIsReadOnly)
		{
			*DiffuseIndirectHistoryViewRect = NewHistoryViewRect;
			*DiffuseIndirectHistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(SceneTextures.Config.Extent, View.ViewRect);
			ScreenProbeGatherState.LumenGatherCvars = GLumenGatherCvars;
			ScreenProbeGatherState.HistoryEffectiveResolution = EffectiveResolution;
			ScreenProbeGatherState.HistorySubstrateMaxClosureCount = ClosureCount;
			ScreenProbeGatherState.HistorySceneTexturesExtent = SceneTextures.Config.Extent;
		}
	}
	else
	{
		// Temporal reprojection is disabled or there is no view state - pass through
	}
}

class FStoreLumenDepthCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStoreLumenDepthCS)
	SHADER_USE_PARAMETER_STRUCT(FStoreLumenDepthCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWNormalTexture)
	END_SHADER_PARAMETER_STRUCT()

	class FStoreNormal : SHADER_PERMUTATION_BOOL("STORE_NORMAL");
	using FPermutationDomain = TShaderPermutationDomain<FStoreNormal>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
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

IMPLEMENT_GLOBAL_SHADER(FStoreLumenDepthCS, "/Engine/Private/Lumen/LumenDenoising.usf", "StoreLumenDepthCS", SF_Compute);

/**
 * Copy depth and normal for opaque before it gets possibly overwritten by water or other translucency writing depth
 */
void FDeferredShadingSceneRenderer::StoreLumenDepthHistory(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, FViewInfo& View)
{
	if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
	{
		const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
		const FSceneTextureParameters& SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures);
		const bool bStoreNormal = ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen && LumenScreenProbeGather::UseRejectBasedOnNormal();

		FRDGTextureRef DepthHistory = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("Lumen.DepthHistory"));

		FRDGTextureRef NormalHistory = bStoreNormal ? GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("Lumen.NormalHistory")) : nullptr;

		FStoreLumenDepthCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FStoreLumenDepthCS::FStoreNormal>(bStoreNormal);
		auto ComputeShader = View.ShaderMap->GetShader<FStoreLumenDepthCS>(PermutationVector);

		FStoreLumenDepthCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStoreLumenDepthCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->RWDepthTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DepthHistory));
		PassParameters->RWNormalTexture = NormalHistory ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NormalHistory)) : nullptr;

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("StoreLumenDepth%s", bStoreNormal ? TEXT("AndNormal") : TEXT("")),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FStoreLumenDepthCS::GetGroupSize()));

		GraphBuilder.QueueTextureExtraction(DepthHistory, &View.ViewState->Lumen.DepthHistoryRT);

		if (bStoreNormal)
		{
			GraphBuilder.QueueTextureExtraction(NormalHistory, &View.ViewState->Lumen.NormalHistoryRT);
		}
		else
		{
			View.ViewState->Lumen.NormalHistoryRT = nullptr;
		}
	}
}

static void ScreenGatherMarkUsedProbes(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	FScreenProbeParameters& ScreenProbeParameters,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters,
	ERDGPassFlags ComputePassFlags)
{
	FMarkRadianceProbesUsedByScreenProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarkRadianceProbesUsedByScreenProbesCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
	PassParameters->ScreenProbeParameters = ScreenProbeParameters;
	PassParameters->RadianceCacheMarkParameters = RadianceCacheMarkParameters;

	auto ComputeShader = View.ShaderMap->GetShader<FMarkRadianceProbesUsedByScreenProbesCS>(0);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MarkRadianceProbes(ScreenProbes) %ux%u", PassParameters->ScreenProbeParameters.ScreenProbeAtlasViewSize.X, PassParameters->ScreenProbeParameters.ScreenProbeAtlasViewSize.Y),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		PassParameters->ScreenProbeParameters.ProbeIndirectArgs,
		(uint32)EScreenProbeIndirectArgs::ThreadPerProbe * sizeof(FRHIDispatchIndirectParameters));
}

static void HairStrandsMarkUsedProbes(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters,
	ERDGPassFlags ComputePassFlags)
{
	check(View.HairStrandsViewData.VisibilityData.TileData.IsValid());
	const uint32 TileMip = 3u; // 8x8 tiles
	const int32 TileSize = 1u<<TileMip;
	const FIntPoint Resolution(View.ViewRect.Width(), View.ViewRect.Height());
	const FIntPoint TileResolution = FIntPoint(
		FMath::DivideAndRoundUp(Resolution.X, TileSize), 
		FMath::DivideAndRoundUp(Resolution.Y, TileSize));

	FMarkRadianceProbesUsedByHairStrandsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarkRadianceProbesUsedByHairStrandsCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->HairStrandsResolution = TileResolution;
	PassParameters->HairStrandsInvResolution = FVector2f(1.f / float(TileResolution.X), 1.f / float(TileResolution.Y));
	PassParameters->HairStrandsMip = TileMip;
	PassParameters->HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
	PassParameters->RadianceCacheMarkParameters = RadianceCacheMarkParameters;
	PassParameters->IndirectBufferArgs = View.HairStrandsViewData.VisibilityData.TileData.TilePerThreadIndirectDispatchBuffer;

	auto ComputeShader = View.ShaderMap->GetShader<FMarkRadianceProbesUsedByHairStrandsCS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MarkRadianceProbes(HairStrands,Tile)"),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		View.HairStrandsViewData.VisibilityData.TileData.TilePerThreadIndirectDispatchBuffer,
		0);
}

DECLARE_GPU_STAT(LumenScreenProbeGather);


FSSDSignalTextures FDeferredShadingSceneRenderer::RenderLumenFinalGather(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	FRDGTextureRef LightingChannelsTexture,
	FViewInfo& View,
	FPreviousViewInfo* PreviousViewInfos,
	bool bRenderDirectLighting,
	FLumenMeshSDFGridParameters& MeshSDFGridParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	FLumenScreenSpaceBentNormalParameters& ScreenSpaceBentNormalParameters,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
	ScreenSpaceBentNormalParameters.UseShortRangeAO = 0;
	ScreenSpaceBentNormalParameters.ScreenBentNormal = SystemTextures.Black;
	RadianceCacheParameters.RadianceProbeIndirectionTexture = nullptr;

	FSSDSignalTextures Outputs;
	LumenRadianceCache::FRadianceCacheInterpolationParameters TranslucencyVolumeRadianceCacheParameters;

	if (GLumenIrradianceFieldGather != 0)
	{
		Outputs = RenderLumenIrradianceFieldGather(GraphBuilder, SceneTextures, FrameTemporaries, View, TranslucencyVolumeRadianceCacheParameters, ComputePassFlags);
	}
	else if (Lumen::UseReSTIRGather(*View.Family, ShaderPlatform))
	{
		Outputs = RenderLumenReSTIRGather(
			GraphBuilder,
			SceneTextures,
			FrameTemporaries,
			LightingChannelsTexture,
			View,
			PreviousViewInfos,
			ComputePassFlags,
			ScreenSpaceBentNormalParameters);
	}
	else
	{
		Outputs = RenderLumenScreenProbeGather(
			GraphBuilder, 
			SceneTextures, 
			FrameTemporaries, 
			LightingChannelsTexture, 
			View, 
			PreviousViewInfos, 
			bRenderDirectLighting,
			MeshSDFGridParameters, 
			RadianceCacheParameters, 
			ScreenSpaceBentNormalParameters,
			TranslucencyVolumeRadianceCacheParameters,
			ComputePassFlags);
	}

	ComputeLumenTranslucencyGIVolume(GraphBuilder, View, FrameTemporaries, TranslucencyVolumeRadianceCacheParameters, ComputePassFlags);

	return Outputs;
}

FSSDSignalTextures FDeferredShadingSceneRenderer::RenderLumenScreenProbeGather(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	FRDGTextureRef LightingChannelsTexture,
	FViewInfo& View,
	FPreviousViewInfo* PreviousViewInfos,
	bool bRenderDirectLighting,
	FLumenMeshSDFGridParameters& MeshSDFGridParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	FLumenScreenSpaceBentNormalParameters& ScreenSpaceBentNormalParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& TranslucencyVolumeRadianceCacheParameters,
	ERDGPassFlags ComputePassFlags)
{
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	RDG_EVENT_SCOPE(GraphBuilder, "LumenScreenProbeGather");
	RDG_GPU_STAT_SCOPE(GraphBuilder, LumenScreenProbeGather);

	check(ShouldRenderLumenDiffuseGI(Scene, View));

	if (!LightingChannelsTexture)
	{
		LightingChannelsTexture = SystemTextures.Black;
	}

	if (!GLumenScreenProbeGather)
	{
		FSSDSignalTextures ScreenSpaceDenoiserInputs;
		ScreenSpaceDenoiserInputs.Textures[0] = SystemTextures.Black;
		FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
		ScreenSpaceDenoiserInputs.Textures[1] = GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("Lumen.ScreenProbeGather.RoughSpecularIndirect"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenSpaceDenoiserInputs.Textures[1])), FLinearColor::Black);
		return ScreenSpaceDenoiserInputs;
	}

	// Pull from uniform buffer to get fallback textures.
	const FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);

	FScreenProbeParameters ScreenProbeParameters;

	ScreenProbeParameters.ScreenProbeTracingOctahedronResolution = LumenScreenProbeGather::GetTracingOctahedronResolution(View);
	ensureMsgf(ScreenProbeParameters.ScreenProbeTracingOctahedronResolution < (1 << 6) - 1, TEXT("Tracing resolution %u was larger than supported by PackRayInfo()"), ScreenProbeParameters.ScreenProbeTracingOctahedronResolution);
	ScreenProbeParameters.ScreenProbeGatherOctahedronResolution = LumenScreenProbeGather::GetGatherOctahedronResolution(ScreenProbeParameters.ScreenProbeTracingOctahedronResolution);
	ScreenProbeParameters.ScreenProbeGatherOctahedronResolutionWithBorder = ScreenProbeParameters.ScreenProbeGatherOctahedronResolution + 2 * (1 << (GLumenScreenProbeGatherNumMips - 1));
	ScreenProbeParameters.ScreenProbeDownsampleFactor = LumenScreenProbeGather::GetScreenDownsampleFactor(View);

	ScreenProbeParameters.ScreenProbeLightSampleResolutionXY = FMath::Clamp<uint32>(GLumenScreenProbeLightSampleResolutionXY, 1, 8);

	ScreenProbeParameters.ScreenProbeViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), (int32)ScreenProbeParameters.ScreenProbeDownsampleFactor);
	ScreenProbeParameters.ScreenProbeAtlasViewSize = ScreenProbeParameters.ScreenProbeViewSize;
	ScreenProbeParameters.ScreenProbeAtlasViewSize.Y += FMath::TruncToInt(ScreenProbeParameters.ScreenProbeViewSize.Y * GLumenScreenProbeGatherAdaptiveProbeAllocationFraction);

	ScreenProbeParameters.ScreenProbeAtlasBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, (int32)ScreenProbeParameters.ScreenProbeDownsampleFactor);
	ScreenProbeParameters.ScreenProbeAtlasBufferSize.Y += FMath::TruncToInt(ScreenProbeParameters.ScreenProbeAtlasBufferSize.Y * GLumenScreenProbeGatherAdaptiveProbeAllocationFraction);

	ScreenProbeParameters.ScreenProbeGatherMaxMip = GLumenScreenProbeGatherNumMips - 1;
	ScreenProbeParameters.RelativeSpeedDifferenceToConsiderLightingMoving = GLumenScreenProbeRelativeSpeedDifferenceToConsiderLightingMoving;
	ScreenProbeParameters.ScreenTraceNoFallbackThicknessScale = (Lumen::UseHardwareRayTracedScreenProbeGather(ViewFamily) ? 1.0f : GLumenScreenProbeScreenTracesThicknessScaleWhenNoFallback) * View.ViewMatrices.GetPerProjectionDepthThicknessScale();
	ScreenProbeParameters.NumUniformScreenProbes = ScreenProbeParameters.ScreenProbeViewSize.X * ScreenProbeParameters.ScreenProbeViewSize.Y;
	ScreenProbeParameters.MaxNumAdaptiveProbes = FMath::TruncToInt(ScreenProbeParameters.NumUniformScreenProbes * GLumenScreenProbeGatherAdaptiveProbeAllocationFraction);
	
	ScreenProbeParameters.FixedJitterIndex = GLumenScreenProbeFixedJitterIndex;

	{
		FVector2f InvAtlasWithBorderBufferSize = FVector2f(1.0f) / (FVector2f(ScreenProbeParameters.ScreenProbeGatherOctahedronResolutionWithBorder) * FVector2f(ScreenProbeParameters.ScreenProbeAtlasBufferSize));
		ScreenProbeParameters.SampleRadianceProbeUVMul = FVector2f(ScreenProbeParameters.ScreenProbeGatherOctahedronResolution) * InvAtlasWithBorderBufferSize;
		ScreenProbeParameters.SampleRadianceProbeUVAdd = FMath::Exp2(ScreenProbeParameters.ScreenProbeGatherMaxMip) * InvAtlasWithBorderBufferSize;
		ScreenProbeParameters.SampleRadianceAtlasUVMul = FVector2f(ScreenProbeParameters.ScreenProbeGatherOctahedronResolutionWithBorder) * InvAtlasWithBorderBufferSize;
	}

	extern int32 GLumenScreenProbeGatherVisualizeTraces;
	// Automatically set a fixed jitter if we are visualizing, but don't override existing fixed jitter
	if (GLumenScreenProbeGatherVisualizeTraces != 0 && ScreenProbeParameters.FixedJitterIndex < 0)
	{
		ScreenProbeParameters.FixedJitterIndex = 6;
	}

	uint32 StateFrameIndex = View.ViewState ? View.ViewState->GetFrameIndex() : 0;
	if (ScreenProbeParameters.FixedJitterIndex >= 0)
	{
		StateFrameIndex = ScreenProbeParameters.FixedJitterIndex;
	}
	ScreenProbeParameters.ScreenProbeRayDirectionFrameIndex = StateFrameIndex % FMath::Clamp(CVarLumenScreenProbeTemporalMaxRayDirections.GetValueOnRenderThread(), 1, 128);

	FRDGTextureDesc DownsampledDepthDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenProbeSceneDepth = GraphBuilder.CreateTexture(DownsampledDepthDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeSceneDepth"));

	FRDGTextureDesc DownsampledNormalDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_R8G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenProbeWorldNormal = GraphBuilder.CreateTexture(DownsampledNormalDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeWorldNormal"));

	FRDGTextureDesc DownsampledSpeedDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenProbeWorldSpeed = GraphBuilder.CreateTexture(DownsampledSpeedDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeWorldSpeed"));

	FRDGTextureDesc DownsampledWorldPositionDesc(FRDGTextureDesc::Create2D(ScreenProbeParameters.ScreenProbeAtlasBufferSize, PF_A32B32G32R32F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenProbeTranslatedWorldPosition = GraphBuilder.CreateTexture(DownsampledWorldPositionDesc, TEXT("Lumen.ScreenProbeGather.ScreenProbeTranslatedWorldPosition"));


	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	ScreenProbeParameters.BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);
	
	{
		FScreenProbeDownsampleDepthUniformCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeDownsampleDepthUniformCS::FParameters>();
		PassParameters->RWScreenProbeSceneDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeSceneDepth));
		PassParameters->RWScreenProbeWorldNormal = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeWorldNormal));
		PassParameters->RWScreenProbeWorldSpeed = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeWorldSpeed));
		PassParameters->RWScreenProbeTranslatedWorldPosition = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeTranslatedWorldPosition));
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
		PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		PassParameters->SceneTextures = SceneTextureParameters;
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeDownsampleDepthUniformCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("UniformPlacement DownsampleFactor=%u", ScreenProbeParameters.ScreenProbeDownsampleFactor),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(ScreenProbeParameters.ScreenProbeViewSize, FScreenProbeDownsampleDepthUniformCS::GetGroupSize()));
	}

	FRDGBufferRef NumAdaptiveScreenProbes = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.ScreenProbeGather.NumAdaptiveScreenProbes"));
	FRDGBufferRef AdaptiveScreenProbeData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), FMath::Max<uint32>(ScreenProbeParameters.MaxNumAdaptiveProbes, 1)), TEXT("Lumen.ScreenProbeGather.daptiveScreenProbeData"));

	ScreenProbeParameters.NumAdaptiveScreenProbes = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(NumAdaptiveScreenProbes, PF_R32_UINT));
	ScreenProbeParameters.AdaptiveScreenProbeData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(AdaptiveScreenProbeData, PF_R32_UINT));

	const FIntPoint ScreenProbeViewportBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, (int32)ScreenProbeParameters.ScreenProbeDownsampleFactor);
	FRDGTextureDesc ScreenTileAdaptiveProbeHeaderDesc(FRDGTextureDesc::Create2D(ScreenProbeViewportBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_AtomicCompatible));
	FIntPoint ScreenTileAdaptiveProbeIndicesBufferSize = FIntPoint(ScreenProbeViewportBufferSize.X * ScreenProbeParameters.ScreenProbeDownsampleFactor, ScreenProbeViewportBufferSize.Y * ScreenProbeParameters.ScreenProbeDownsampleFactor);
	FRDGTextureDesc ScreenTileAdaptiveProbeIndicesDesc(FRDGTextureDesc::Create2D(ScreenTileAdaptiveProbeIndicesBufferSize, PF_R16_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.ScreenTileAdaptiveProbeHeader = GraphBuilder.CreateTexture(ScreenTileAdaptiveProbeHeaderDesc, TEXT("Lumen.ScreenProbeGather.ScreenTileAdaptiveProbeHeader"));
	ScreenProbeParameters.ScreenTileAdaptiveProbeIndices = GraphBuilder.CreateTexture(ScreenTileAdaptiveProbeIndicesDesc, TEXT("Lumen.ScreenProbeGather.ScreenTileAdaptiveProbeIndices"));

	FUintVector4 ClearValues(0, 0, 0, 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenTileAdaptiveProbeHeader)), ClearValues, ComputePassFlags);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(NumAdaptiveScreenProbes), 0, ComputePassFlags);

	const uint32 AdaptiveProbeMinDownsampleFactor = FMath::Clamp(GLumenScreenProbeGatherAdaptiveProbeMinDownsampleFactor, 1, 64);

	if (ScreenProbeParameters.MaxNumAdaptiveProbes > 0 && AdaptiveProbeMinDownsampleFactor < ScreenProbeParameters.ScreenProbeDownsampleFactor)
	{ 
		uint32 PlacementDownsampleFactor = ScreenProbeParameters.ScreenProbeDownsampleFactor;
		do
		{
			PlacementDownsampleFactor /= 2;
			FScreenProbeAdaptivePlacementCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeAdaptivePlacementCS::FParameters>();
			PassParameters->RWScreenProbeSceneDepth = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeSceneDepth));
			PassParameters->RWScreenProbeWorldNormal = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeWorldNormal));
			PassParameters->RWScreenProbeWorldSpeed = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeWorldSpeed));
			PassParameters->RWScreenProbeTranslatedWorldPosition = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeTranslatedWorldPosition));
			PassParameters->RWNumAdaptiveScreenProbes = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(NumAdaptiveScreenProbes, PF_R32_UINT));
			PassParameters->RWAdaptiveScreenProbeData = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(AdaptiveScreenProbeData, PF_R32_UINT));
			PassParameters->RWScreenTileAdaptiveProbeHeader = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenTileAdaptiveProbeHeader));
			PassParameters->RWScreenTileAdaptiveProbeIndices = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenTileAdaptiveProbeIndices));
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->SceneTextures = SceneTextureParameters;
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
			PassParameters->ScreenProbeParameters = ScreenProbeParameters;
			PassParameters->PlacementDownsampleFactor = PlacementDownsampleFactor;

			auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeAdaptivePlacementCS>(0);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("AdaptivePlacement DownsampleFactor=%u", PlacementDownsampleFactor),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(FIntPoint::DivideAndRoundDown(View.ViewRect.Size(), (int32)PlacementDownsampleFactor), FScreenProbeAdaptivePlacementCS::GetGroupSize()));
		}
		while (PlacementDownsampleFactor > AdaptiveProbeMinDownsampleFactor);
	}
	else
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AdaptiveScreenProbeData), 0, ComputePassFlags);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenTileAdaptiveProbeIndices)), ClearValues, ComputePassFlags);
	}

	FRDGBufferRef ScreenProbeIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((uint32)EScreenProbeIndirectArgs::Max), TEXT("Lumen.ScreenProbeGather.ScreenProbeIndirectArgs"));

	{
		FSetupAdaptiveProbeIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSetupAdaptiveProbeIndirectArgsCS::FParameters>();
		PassParameters->RWScreenProbeIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ScreenProbeIndirectArgs, PF_R32_UINT));
		PassParameters->ScreenProbeParameters = ScreenProbeParameters;

		auto ComputeShader = View.ShaderMap->GetShader<FSetupAdaptiveProbeIndirectArgsCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SetupAdaptiveProbeIndirectArgs"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	ScreenProbeParameters.ProbeIndirectArgs = ScreenProbeIndirectArgs;

	FRDGTextureRef BRDFProbabilityDensityFunction = nullptr;
	FRDGBufferSRVRef BRDFProbabilityDensityFunctionSH = nullptr;
	GenerateBRDF_PDF(GraphBuilder, View, SceneTextures, BRDFProbabilityDensityFunction, BRDFProbabilityDensityFunctionSH, ScreenProbeParameters, ComputePassFlags);

	const LumenRadianceCache::FRadianceCacheInputs RadianceCacheInputs = LumenScreenProbeGatherRadianceCache::SetupRadianceCacheInputs(View);

	if (LumenScreenProbeGather::UseRadianceCache(View))
	{
		// Using !View.IsInstancedSceneView() to skip actual secondary stereo views only, View.ShouldRenderView() returns false for empty views as well
		if (!ShouldUseStereoLumenOptimizations() || !View.IsInstancedSceneView())
		{
			FMarkUsedRadianceCacheProbes GraphicsMarkUsedRadianceCacheProbesCallbacks;
			FMarkUsedRadianceCacheProbes ComputeMarkUsedRadianceCacheProbesCallbacks;

			ComputeMarkUsedRadianceCacheProbesCallbacks.AddLambda([ComputePassFlags](
				FRDGBuilder& GraphBuilder,
				const FViewInfo& View,
				const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
				{
					MarkUsedProbesForVisualize(GraphBuilder, View, RadianceCacheMarkParameters, ComputePassFlags);
				});

			// Mark radiance caches for screen probes
			ComputeMarkUsedRadianceCacheProbesCallbacks.AddLambda([&SceneTextures, &ScreenProbeParameters, ComputePassFlags](
				FRDGBuilder& GraphBuilder,
				const FViewInfo& View,
				const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
				{
					ScreenGatherMarkUsedProbes(
						GraphBuilder,
						View,
						SceneTextures,
						ScreenProbeParameters,
						RadianceCacheMarkParameters,
						ComputePassFlags);
				});

			// Mark radiance caches for hair strands
			if (HairStrands::HasViewHairStrandsData(View))
			{
				ComputeMarkUsedRadianceCacheProbesCallbacks.AddLambda([ComputePassFlags](
					FRDGBuilder& GraphBuilder,
					const FViewInfo& View,
					const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
					{
						HairStrandsMarkUsedProbes(
							GraphBuilder,
							View,
							RadianceCacheMarkParameters,
							ComputePassFlags);
					});
			}

			if (Lumen::UseLumenTranslucencyRadianceCacheReflections(View))
			{
				const FSceneRenderer& SceneRenderer = *this;
				FViewInfo& ViewNonConst = View;

				GraphicsMarkUsedRadianceCacheProbesCallbacks.AddLambda([&SceneTextures, &SceneRenderer, &ViewNonConst](
					FRDGBuilder& GraphBuilder,
					const FViewInfo& View,
					const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
					{
						LumenTranslucencyReflectionsMarkUsedProbes(
							GraphBuilder,
							SceneRenderer,
							ViewNonConst,
							SceneTextures,
							RadianceCacheMarkParameters);
					});
			}

			LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateInputs> InputArray;
			LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateOutputs> OutputArray;

			InputArray.Add(LumenRadianceCache::FUpdateInputs(
				RadianceCacheInputs,
				FRadianceCacheConfiguration(),
				View,
				nullptr,
				nullptr,
				MoveTemp(GraphicsMarkUsedRadianceCacheProbesCallbacks),
				MoveTemp(ComputeMarkUsedRadianceCacheProbesCallbacks)));

			OutputArray.Add(LumenRadianceCache::FUpdateOutputs(
				View.ViewState->Lumen.RadianceCacheState,
				RadianceCacheParameters));

			// Add the Translucency Volume radiance cache to the update so its dispatches can overlap
			{
				LumenRadianceCache::FUpdateInputs TranslucencyVolumeRadianceCacheUpdateInputs = GetLumenTranslucencyGIVolumeRadianceCacheInputs(
					GraphBuilder,
					View,
					FrameTemporaries,
					ComputePassFlags);

				if (TranslucencyVolumeRadianceCacheUpdateInputs.IsAnyCallbackBound())
				{
					InputArray.Add(TranslucencyVolumeRadianceCacheUpdateInputs);
					OutputArray.Add(LumenRadianceCache::FUpdateOutputs(
						View.ViewState->Lumen.TranslucencyVolumeRadianceCacheState,
						TranslucencyVolumeRadianceCacheParameters));
				}
			}

			LumenRadianceCache::UpdateRadianceCaches(
				GraphBuilder,
				FrameTemporaries,
				InputArray,
				OutputArray,
				Scene,
				ViewFamily,
				LumenCardRenderer.bPropagateGlobalLightingChange,
				ComputePassFlags);

			if (Lumen::UseLumenTranslucencyRadianceCacheReflections(View))
			{
				View.GetOwnLumenTranslucencyGIVolume().RadianceCacheInterpolationParameters = RadianceCacheParameters;

				extern float GLumenTranslucencyReflectionsRadianceCacheReprojectionRadiusScale;
				extern float GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize;
				View.GetOwnLumenTranslucencyGIVolume().RadianceCacheInterpolationParameters.RadianceCacheInputs.ReprojectionRadiusScale = GLumenTranslucencyReflectionsRadianceCacheReprojectionRadiusScale;
				View.GetOwnLumenTranslucencyGIVolume().RadianceCacheInterpolationParameters.RadianceCacheInputs.InvClipmapFadeSize = 1.0f / FMath::Clamp(GLumenTranslucencyVolumeRadianceCacheClipmapFadeSize, .001f, 16.0f);
			}
		}
		else
		{
			RadianceCacheParameters = View.GetLumenTranslucencyGIVolume().RadianceCacheInterpolationParameters;
		}
	}

	if (LumenScreenProbeGather::UseImportanceSampling(View))
	{
		GenerateImportanceSamplingRays(
			GraphBuilder,
			View,
			SceneTextures,
			RadianceCacheParameters,
			BRDFProbabilityDensityFunction,
			BRDFProbabilityDensityFunctionSH,
			ScreenProbeParameters,
			ComputePassFlags);
	}

	if (bRenderDirectLighting)
	{
		const FIntPoint ScreenProbeLightSampleBufferSize = ScreenProbeParameters.ScreenProbeAtlasBufferSize * ScreenProbeParameters.ScreenProbeLightSampleResolutionXY;
		FRDGTextureDesc LightSampleDirectionDesc(FRDGTextureDesc::Create2D(ScreenProbeLightSampleBufferSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
		ScreenProbeParameters.ScreenProbeLightSampleDirection = GraphBuilder.CreateTexture(LightSampleDirectionDesc, TEXT("Lumen.ScreenProbeGather.LightSampleDirection"));

		FRDGTextureDesc LightSampleFlagsDesc(FRDGTextureDesc::Create2D(ScreenProbeLightSampleBufferSize, PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
		ScreenProbeParameters.ScreenProbeLightSampleFlags = GraphBuilder.CreateTexture(LightSampleFlagsDesc, TEXT("Lumen.ScreenProbeGather.LightSampleFlags"));

		FRDGTextureDesc LightSampleRadianceDesc(FRDGTextureDesc::Create2D(ScreenProbeLightSampleBufferSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
		ScreenProbeParameters.ScreenProbeLightSampleRadiance = GraphBuilder.CreateTexture(LightSampleRadianceDesc, TEXT("Lumen.ScreenProbeGather.LightSampleRadiance"));

		{
			FScreenProbeGenerateLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenProbeGenerateLightSamplesCS::FParameters>();
			PassParameters->RWScreenProbeLightSampleDirection = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeLightSampleDirection));
			PassParameters->RWScreenProbeLightSampleFlags = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeLightSampleFlags));
			PassParameters->RWScreenProbeLightSampleRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.ScreenProbeLightSampleRadiance));
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->ScreenProbeParameters = ScreenProbeParameters;
			PassParameters->Forward = View.ForwardLightingResources.ForwardLightUniformBuffer;

			auto ComputeShader = View.ShaderMap->GetShader<FScreenProbeGenerateLightSamplesCS>(0);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GenerateLightSamples"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				PassParameters->ScreenProbeParameters.ProbeIndirectArgs,
				(uint32)EScreenProbeIndirectArgs::ThreadPerProbe * sizeof(FRHIDispatchIndirectParameters));
		}

		FRDGTextureDesc LightSampleTraceHitDesc(FRDGTextureDesc::Create2D(ScreenProbeLightSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
		ScreenProbeParameters.LightSampleTraceHit = GraphBuilder.CreateTexture(LightSampleTraceHitDesc, TEXT("Lumen.ScreenProbeGather.LightSampleTraceHit"));
		ScreenProbeParameters.RWLightSampleTraceHit = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.LightSampleTraceHit));
	}
	
	const FIntPoint ScreenProbeTraceBufferSize = ScreenProbeParameters.ScreenProbeAtlasBufferSize * ScreenProbeParameters.ScreenProbeTracingOctahedronResolution;
	FRDGTextureDesc TraceRadianceDesc(FRDGTextureDesc::Create2D(ScreenProbeTraceBufferSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.TraceRadiance = GraphBuilder.CreateTexture(TraceRadianceDesc, TEXT("Lumen.ScreenProbeGather.TraceRadiance"));
	ScreenProbeParameters.RWTraceRadiance = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.TraceRadiance));

	FRDGTextureDesc TraceHitDesc(FRDGTextureDesc::Create2D(ScreenProbeTraceBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	ScreenProbeParameters.TraceHit = GraphBuilder.CreateTexture(TraceHitDesc, TEXT("Lumen.ScreenProbeGather.TraceHit"));
	ScreenProbeParameters.RWTraceHit = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenProbeParameters.TraceHit));

	TraceScreenProbes(
		GraphBuilder, 
		Scene,
		View, 
		FrameTemporaries,
		GLumenGatherCvars.TraceMeshSDFs != 0 && Lumen::UseMeshSDFTracing(ViewFamily),
		bRenderDirectLighting,
		SceneTextures,
		LightingChannelsTexture,
		RadianceCacheParameters,
		ScreenProbeParameters,
		MeshSDFGridParameters,
		ComputePassFlags);
	
	FScreenProbeGatherParameters GatherParameters;
	FilterScreenProbes(GraphBuilder, View, SceneTextures, ScreenProbeParameters, bRenderDirectLighting, GatherParameters, ComputePassFlags);

	if (LumenScreenProbeGather::UseShortRangeAmbientOcclusion(ViewFamily.EngineShowFlags))
	{
		float MaxScreenTraceFraction = ScreenProbeParameters.ScreenProbeDownsampleFactor * 2.0f / (float)View.ViewRect.Width();
		ScreenSpaceBentNormalParameters = ComputeScreenSpaceShortRangeAO(GraphBuilder, Scene, View, SceneTextures, LightingChannelsTexture, BlueNoise, MaxScreenTraceFraction, ScreenProbeParameters.ScreenTraceNoFallbackThicknessScale, ComputePassFlags);
	}

	const FIntPoint EffectiveResolution = Substrate::GetSubstrateTextureResolution(View, SceneTextures.Config.Extent);
	const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);
	FRDGTextureDesc DiffuseIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
	FRDGTextureRef DiffuseIndirect = GraphBuilder.CreateTexture(DiffuseIndirectDesc, TEXT("Lumen.ScreenProbeGather.DiffuseIndirect"));

	const bool bSupportBackfaceDiffuse = GLumenScreenProbeSupportTwoSidedFoliageBackfaceDiffuse != 0;
	FRDGTextureRef BackfaceDiffuseIndirect = nullptr;

	if (bSupportBackfaceDiffuse)
	{
		FRDGTextureDesc BackfaceDiffuseIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
		BackfaceDiffuseIndirect = GraphBuilder.CreateTexture(BackfaceDiffuseIndirectDesc, TEXT("Lumen.ScreenProbeGather.BackfaceDiffuseIndirect"));
	}

	FRDGTextureDesc RoughSpecularIndirectDesc = FRDGTextureDesc::Create2DArray(EffectiveResolution, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount);
	FRDGTextureRef RoughSpecularIndirect = GraphBuilder.CreateTexture(RoughSpecularIndirectDesc, TEXT("Lumen.ScreenProbeGather.RoughSpecularIndirect"));

	const bool bSSREnabled = GetViewPipelineState(View).ReflectionsMethod == EReflectionsMethod::SSR;

	InterpolateAndIntegrate(
		GraphBuilder,
		SceneTextures,
		View,
		ScreenProbeParameters,
		GatherParameters,
		ScreenSpaceBentNormalParameters,
		bRenderDirectLighting,
		bSSREnabled,
		DiffuseIndirect,
		BackfaceDiffuseIndirect,
		RoughSpecularIndirect,
		ComputePassFlags);

	// Set for DiffuseIndirectComposite
	ScreenSpaceBentNormalParameters.UseShortRangeAO = ScreenSpaceBentNormalParameters.UseShortRangeAO != 0 && !LumenScreenProbeGather::ApplyShortRangeAODuringIntegration();

	FSSDSignalTextures DenoiserOutputs;
	DenoiserOutputs.Textures[0] = DiffuseIndirect;
	DenoiserOutputs.Textures[1] = bSupportBackfaceDiffuse ? BackfaceDiffuseIndirect : SystemTextures.Black;
	DenoiserOutputs.Textures[2] = RoughSpecularIndirect;

	if (GLumenScreenProbeTemporalFilter)
	{
		UpdateHistoryScreenProbeGather(
			GraphBuilder,
			View,
			SceneTextures,
			LumenCardRenderer.bPropagateGlobalLightingChange,
			DiffuseIndirect,
			BackfaceDiffuseIndirect,
			RoughSpecularIndirect,
			ComputePassFlags);

		DenoiserOutputs.Textures[0] = DiffuseIndirect;
		DenoiserOutputs.Textures[1] = bSupportBackfaceDiffuse ? BackfaceDiffuseIndirect : SystemTextures.Black;
		DenoiserOutputs.Textures[2] = RoughSpecularIndirect;
	}

	// Sample radiance caches for hair strands lighting. Only used wht radiance cache is enabled
	if (LumenScreenProbeGather::UseRadianceCache(View) && HairStrands::HasViewHairStrandsData(View))
	{
		RenderHairStrandsLumenLighting(GraphBuilder, Scene, View);
	}

	return DenoiserOutputs;
}

