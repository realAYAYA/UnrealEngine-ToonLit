// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenTranslucencyVolumeLighting.cpp
=============================================================================*/

#include "LumenTranslucencyVolumeLighting.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "DistanceFieldLightingShared.h"
#include "LumenMeshCards.h"
#include "Math/Halton.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "LumenTracingUtils.h"
#include "LumenRadianceCache.h"

int32 GLumenTranslucencyVolume = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyVolume(
	TEXT("r.Lumen.TranslucencyVolume.Enable"),
	GLumenTranslucencyVolume,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenTranslucencyVolumeTraceFromVolume = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeTraceFromVolume(
	TEXT("r.Lumen.TranslucencyVolume.TraceFromVolume"),
	GLumenTranslucencyVolumeTraceFromVolume,
	TEXT("Whether to ray trace from the translucency volume's voxels to gather indirect lighting.  Only makes sense to disable if TranslucencyVolume.RadianceCache is enabled."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyFroxelGridPixelSize = 32;
FAutoConsoleVariableRef CVarTranslucencyFroxelGridPixelSize(
	TEXT("r.Lumen.TranslucencyVolume.GridPixelSize"),
	GTranslucencyFroxelGridPixelSize,
	TEXT("Size of a cell in the translucency grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyGridDistributionLogZScale = .01f;
FAutoConsoleVariableRef CVarTranslucencyGridDistributionLogZScale(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionLogZScale"),
	GTranslucencyGridDistributionLogZScale,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyGridDistributionLogZOffset = 1.0f;
FAutoConsoleVariableRef CVarTranslucencyGridDistributionLogZOffset(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionLogZOffset"),
	GTranslucencyGridDistributionLogZOffset,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyGridDistributionZScale = 4.0f;
FAutoConsoleVariableRef CVarTranslucencyGridDistributionZScale(
	TEXT("r.Lumen.TranslucencyVolume.GridDistributionZScale"),
	GTranslucencyGridDistributionZScale,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyGridEndDistanceFromCamera = 8000;
FAutoConsoleVariableRef CVarTranslucencyGridEndDistanceFromCamera(
	TEXT("r.Lumen.TranslucencyVolume.EndDistanceFromCamera"),
	GTranslucencyGridEndDistanceFromCamera,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeSpatialFilter = 1;
FAutoConsoleVariableRef CVarTranslucencyVolumeSpatialFilter(
	TEXT("r.Lumen.TranslucencyVolume.SpatialFilter"),
	GTranslucencyVolumeSpatialFilter,
	TEXT("Whether to use a spatial filter on the volume traces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeSpatialFilterNumPasses = 2;
FAutoConsoleVariableRef CVarTranslucencyVolumeSpatialFilterNumPasses(
	TEXT("r.Lumen.TranslucencyVolume.SpatialFilter.NumPasses"),
	GTranslucencyVolumeSpatialFilterNumPasses,
	TEXT("How many passes of the spatial filter to do"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeTemporalReprojection = 1;
FAutoConsoleVariableRef CVarTranslucencyVolumeTemporalReprojection(
	TEXT("r.Lumen.TranslucencyVolume.TemporalReprojection"),
	GTranslucencyVolumeTemporalReprojection,
	TEXT("Whether to use temporal reprojection."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeJitter = 1;
FAutoConsoleVariableRef CVarTranslucencyVolumeJitter(
	TEXT("r.Lumen.TranslucencyVolume.Temporal.Jitter"),
	GTranslucencyVolumeJitter,
	TEXT("Whether to apply jitter to each frame's translucency GI computation, achieving temporal super sampling."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GTranslucencyVolumeHistoryWeight = .9f;
FAutoConsoleVariableRef CVarTranslucencyVolumeHistoryWeight(
	TEXT("r.Lumen.TranslucencyVolume.Temporal.HistoryWeight"),
	GTranslucencyVolumeHistoryWeight,
	TEXT("How much the history value should be weighted each frame.  This is a tradeoff between visible jittering and responsiveness."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GTranslucencyVolumeTraceStepFactor = 2;
FAutoConsoleVariableRef CVarTranslucencyVolumeTraceStepFactor(
	TEXT("r.Lumen.TranslucencyVolume.TraceStepFactor"),
	GTranslucencyVolumeTraceStepFactor,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeTracingOctahedronResolution = 3;
FAutoConsoleVariableRef CVarTranslucencyVolumeTracingOctahedronResolution(
	TEXT("r.Lumen.TranslucencyVolume.TracingOctahedronResolution"),
	GTranslucencyVolumeTracingOctahedronResolution,
	TEXT("Resolution of the tracing octahedron.  Determines how many traces are done per voxel of the translucency lighting volume."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GTranslucencyVolumeVoxelTraceStartDistanceScale = 1.0f;
FAutoConsoleVariableRef CVarTranslucencyVoxelTraceStartDistanceScale(
	TEXT("r.Lumen.TranslucencyVolume.VoxelTraceStartDistanceScale"),
	GTranslucencyVolumeVoxelTraceStartDistanceScale,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GTranslucencyVolumeMaxRayIntensity = 20.0f;
FAutoConsoleVariableRef CVarTranslucencyVolumeMaxRayIntensity(
	TEXT("r.Lumen.TranslucencyVolume.MaxRayIntensity"),
	GTranslucencyVolumeMaxRayIntensity,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenTranslucencyVolumeRadianceCache = 1;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeRadianceCache(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache"),
	GLumenTranslucencyVolumeRadianceCache,
	TEXT("Whether to use the Radiance Cache for Translucency"),
	ECVF_RenderThreadSafe
	);

int32 GTranslucencyVolumeRadianceCacheNumMipmaps = 3;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheNumMipmaps(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.NumMipmaps"),
	GTranslucencyVolumeRadianceCacheNumMipmaps,
	TEXT("Number of radiance cache mipmaps."),
	ECVF_RenderThreadSafe
);

float GLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent = 2500.0f;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ClipmapWorldExtent"),
	GLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent,
	TEXT("World space extent of the first clipmap"),
	ECVF_RenderThreadSafe
);

float GLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase = 2.0f;
FAutoConsoleVariableRef CVarLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ClipmapDistributionBase"),
	GLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase,
	TEXT("Base of the Pow() that controls the size of each successive clipmap relative to the first."),
	ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheNumProbesToTraceBudget = 200;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheNumProbesToTraceBudget(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.NumProbesToTraceBudget"),
	GTranslucencyVolumeRadianceCacheNumProbesToTraceBudget,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheGridResolution = 24;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.GridResolution"),
	GTranslucencyVolumeRadianceCacheGridResolution,
	TEXT("Resolution of the probe placement grid within each clipmap"),
	ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheProbeResolution = 8;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheProbeResolution(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ProbeResolution"),
	GTranslucencyVolumeRadianceCacheProbeResolution,
	TEXT("Resolution of the probe's 2d radiance layout.  The number of rays traced for the probe will be ProbeResolution ^ 2"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheProbeAtlasResolutionInProbes = 128;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheProbeAtlasResolutionInProbes(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ProbeAtlasResolutionInProbes"),
	GTranslucencyVolumeRadianceCacheProbeAtlasResolutionInProbes,
	TEXT("Number of probes along one dimension of the probe atlas cache texture.  This controls the memory usage of the cache.  Overflow currently results in incorrect rendering."),
	ECVF_RenderThreadSafe
);

float GTranslucencyVolumeRadianceCacheReprojectionRadiusScale = 10.0f;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheProbeReprojectionRadiusScale(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.ReprojectionRadiusScale"),
	GTranslucencyVolumeRadianceCacheReprojectionRadiusScale,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheFarField = 0;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheFarField(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.FarField"),
	GTranslucencyVolumeRadianceCacheFarField,
	TEXT("Whether to trace against the FarField representation"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GTranslucencyVolumeRadianceCacheStats = 0;
FAutoConsoleVariableRef CVarTranslucencyVolumeRadianceCacheStats(
	TEXT("r.Lumen.TranslucencyVolume.RadianceCache.Stats"),
	GTranslucencyVolumeRadianceCacheStats,
	TEXT("GPU print out Radiance Cache update stats."),
	ECVF_RenderThreadSafe
);

float GTranslucencyVolumeGridCenterOffsetFromDepthBuffer = 0.5f;
FAutoConsoleVariableRef CVarTranslucencyVolumeGridCenterOffsetFromDepthBuffer(
	TEXT("r.Lumen.TranslucencyVolume.GridCenterOffsetFromDepthBuffer"),
	GTranslucencyVolumeGridCenterOffsetFromDepthBuffer,
	TEXT("Offset in grid units to move grid center sample out form the depth buffer along the Z direction. -1 means disabled. This reduces sample self intersection with geometry when tracing the global distance field buffer, and thus reduces flickering in those areas, as well as results in less leaking sometimes."),
	ECVF_RenderThreadSafe
);

float GTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset = 1.0f;
FAutoConsoleVariableRef CVarTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset(
	TEXT("r.Lumen.TranslucencyVolume.OffsetThresholdToAcceptDepthBufferOffset"),
	GTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset,
	TEXT("Offset in grid units to accept a sample to be moved forward in front of the depth buffer. This is to avoid moving all samples behind the depth buffer forward which would affect the lighting of translucent and volumetric at edges of mesh."),
	ECVF_RenderThreadSafe
);

namespace LumenTranslucencyVolume
{
	float GetEndDistanceFromCamera(const FViewInfo& View)
	{
		// Ideally we'd use LumenSceneViewDistance directly, but direct shadowing via translucency lighting volume only covers 5000.0f units by default (r.TranslucencyLightingVolumeOuterDistance), 
		//		so there isn't much point covering beyond that.  
		const float ViewDistanceScale = FMath::Clamp(View.FinalPostProcessSettings.LumenSceneViewDistance / 20000.0f, .1f, 100.0f);
		return FMath::Clamp<float>(GTranslucencyGridEndDistanceFromCamera * ViewDistanceScale, 1.0f, 100000.0f);
	}
}

namespace LumenTranslucencyVolumeRadianceCache
{
	int32 GetNumClipmaps(float DistanceToCover)
	{
		int32 ClipmapIndex = 0;

		for (; ClipmapIndex < LumenRadianceCache::MaxClipmaps; ++ClipmapIndex)
		{
			const float ClipmapExtent = GLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent * FMath::Pow(GLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase, ClipmapIndex);

			if (ClipmapExtent > DistanceToCover)
			{
				break;
			}
		}

		return FMath::Clamp(ClipmapIndex + 1, 1, LumenRadianceCache::MaxClipmaps);
	}

	int32 GetClipmapGridResolution()
	{
		const int32 GridResolution = GTranslucencyVolumeRadianceCacheGridResolution;
		return FMath::Clamp(GridResolution, 1, 256);
	}

	int32 GetProbeResolution()
	{
		return GTranslucencyVolumeRadianceCacheProbeResolution;
	}

	int32 GetNumMipmaps()
	{
		return GTranslucencyVolumeRadianceCacheNumMipmaps;
	}

	int32 GetFinalProbeResolution()
	{
		return GetProbeResolution() + 2 * (1 << (GetNumMipmaps() - 1));
	}

	LumenRadianceCache::FRadianceCacheInputs SetupRadianceCacheInputs(const FViewInfo& View)
	{
		LumenRadianceCache::FRadianceCacheInputs Parameters = LumenRadianceCache::GetDefaultRadianceCacheInputs();
		Parameters.ReprojectionRadiusScale = GTranslucencyVolumeRadianceCacheReprojectionRadiusScale;
		Parameters.ClipmapWorldExtent = GLumenTranslucencyVolumeRadianceCacheClipmapWorldExtent;
		Parameters.ClipmapDistributionBase = GLumenTranslucencyVolumeRadianceCacheClipmapDistributionBase;
		Parameters.RadianceProbeClipmapResolution = GetClipmapGridResolution();
		Parameters.ProbeAtlasResolutionInProbes = FIntPoint(GTranslucencyVolumeRadianceCacheProbeAtlasResolutionInProbes, GTranslucencyVolumeRadianceCacheProbeAtlasResolutionInProbes);
		Parameters.NumRadianceProbeClipmaps = GetNumClipmaps(LumenTranslucencyVolume::GetEndDistanceFromCamera(View));
		Parameters.RadianceProbeResolution = FMath::Max(GetProbeResolution(), LumenRadianceCache::MinRadianceProbeResolution);
		Parameters.FinalProbeResolution = GetFinalProbeResolution();
		Parameters.FinalRadianceAtlasMaxMip = GetNumMipmaps() - 1;
		const float TraceBudgetScale = View.Family->bCurrentlyBeingEdited ? 10.0f : 1.0f;
		Parameters.NumProbesToTraceBudget = GTranslucencyVolumeRadianceCacheNumProbesToTraceBudget * TraceBudgetScale;
		Parameters.RadianceCacheStats = GTranslucencyVolumeRadianceCacheStats;
		return Parameters;
	}
};


const static uint32 MaxTranslucencyVolumeConeDirections = 64;

FRDGTextureRef OrDefault2dTextureIfNull(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture)
{
	return Texture ? Texture : GSystemTextures.GetBlackDummy(GraphBuilder);
}

FRDGTextureRef OrDefault2dArrayTextureIfNull(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture)
{
    return Texture ? Texture : GSystemTextures.GetBlackArrayDummy(GraphBuilder);
}

FRDGTextureRef OrDefault3dTextureIfNull(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture)
{
	return Texture ? Texture : GSystemTextures.GetVolumetricBlackDummy(GraphBuilder);
}

FRDGTextureRef OrDefault3dUintTextureIfNull(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture)
{
	return Texture ? Texture: GSystemTextures.GetVolumetricBlackUintDummy(GraphBuilder);
}

float GetLumenReflectionSpecularScale();
float GetLumenReflectionContrast();
FLumenTranslucencyLightingParameters GetLumenTranslucencyLightingParameters(
	FRDGBuilder& GraphBuilder, 
	const FLumenTranslucencyGIVolume& LumenTranslucencyGIVolume,
	const FLumenFrontLayerTranslucency& LumenFrontLayerTranslucency)
{
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FLumenTranslucencyLightingParameters Parameters;
	Parameters.RadianceCacheInterpolationParameters = LumenTranslucencyGIVolume.RadianceCacheInterpolationParameters;

	if (!LumenTranslucencyGIVolume.RadianceCacheInterpolationParameters.RadianceCacheFinalRadianceAtlas)
	{
		Parameters.RadianceCacheInterpolationParameters.RadianceCacheInputs.FinalProbeResolution = 0;
	}

	Parameters.RadianceCacheInterpolationParameters.RadianceProbeIndirectionTexture = OrDefault3dUintTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceProbeIndirectionTexture);
	Parameters.RadianceCacheInterpolationParameters.RadianceCacheFinalRadianceAtlas = OrDefault2dTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceCacheFinalRadianceAtlas);
	Parameters.RadianceCacheInterpolationParameters.RadianceCacheFinalIrradianceAtlas = OrDefault2dTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceCacheFinalIrradianceAtlas);
	Parameters.RadianceCacheInterpolationParameters.RadianceCacheProbeOcclusionAtlas = OrDefault2dTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceCacheProbeOcclusionAtlas);
	Parameters.RadianceCacheInterpolationParameters.RadianceCacheDepthAtlas = OrDefault2dTextureIfNull(GraphBuilder, Parameters.RadianceCacheInterpolationParameters.RadianceCacheDepthAtlas);
	
	if (!Parameters.RadianceCacheInterpolationParameters.ProbeWorldOffset)
	{
		Parameters.RadianceCacheInterpolationParameters.ProbeWorldOffset = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f))));
	}

	Parameters.FrontLayerTranslucencyReflectionParameters.Enabled = LumenFrontLayerTranslucency.bEnabled ? 1 : 0;
	Parameters.FrontLayerTranslucencyReflectionParameters.RelativeDepthThreshold = LumenFrontLayerTranslucency.RelativeDepthThreshold;
	Parameters.FrontLayerTranslucencyReflectionParameters.Radiance = OrDefault2dArrayTextureIfNull(GraphBuilder, LumenFrontLayerTranslucency.Radiance);
	Parameters.FrontLayerTranslucencyReflectionParameters.Normal = OrDefault2dTextureIfNull(GraphBuilder, LumenFrontLayerTranslucency.Normal);
	Parameters.FrontLayerTranslucencyReflectionParameters.SceneDepth = OrDefault2dTextureIfNull(GraphBuilder, LumenFrontLayerTranslucency.SceneDepth);
	Parameters.FrontLayerTranslucencyReflectionParameters.SpecularScale = GetLumenReflectionSpecularScale();
	Parameters.FrontLayerTranslucencyReflectionParameters.Contrast = GetLumenReflectionContrast();

	Parameters.TranslucencyGIVolume0            = LumenTranslucencyGIVolume.Texture0        ? LumenTranslucencyGIVolume.Texture0        : SystemTextures.VolumetricBlack;
	Parameters.TranslucencyGIVolume1            = LumenTranslucencyGIVolume.Texture1        ? LumenTranslucencyGIVolume.Texture1        : SystemTextures.VolumetricBlack;
	Parameters.TranslucencyGIVolumeHistory0     = LumenTranslucencyGIVolume.HistoryTexture0 ? LumenTranslucencyGIVolume.HistoryTexture0 : SystemTextures.VolumetricBlack;
	Parameters.TranslucencyGIVolumeHistory1     = LumenTranslucencyGIVolume.HistoryTexture1 ? LumenTranslucencyGIVolume.HistoryTexture1 : SystemTextures.VolumetricBlack;
	Parameters.TranslucencyGIVolumeSampler      = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.TranslucencyGIGridZParams        = (FVector3f)LumenTranslucencyGIVolume.GridZParams;
	Parameters.TranslucencyGIGridPixelSizeShift = LumenTranslucencyGIVolume.GridPixelSizeShift;
	Parameters.TranslucencyGIGridSize           = LumenTranslucencyGIVolume.GridSize;
	return Parameters;
}

void GetTranslucencyGridZParams(float NearPlane, float FarPlane, FVector& OutZParams, int32& OutGridSizeZ)
{
	OutGridSizeZ = FMath::TruncToInt(FMath::Log2((FarPlane - NearPlane) * GTranslucencyGridDistributionLogZScale) * GTranslucencyGridDistributionZScale) + 1;
	OutZParams = FVector(GTranslucencyGridDistributionLogZScale, GTranslucencyGridDistributionLogZOffset, GTranslucencyGridDistributionZScale);
}

FVector TranslucencyVolumeTemporalRandom(uint32 FrameNumber)
{
	// Center of the voxel
	FVector RandomOffsetValue(.5f, .5f, .5f);

	if (GTranslucencyVolumeJitter)
	{
		RandomOffsetValue = FVector(Halton(FrameNumber & 1023, 2), Halton(FrameNumber & 1023, 3), Halton(FrameNumber & 1023, 5));
	}

	return RandomOffsetValue;
}


class FMarkRadianceProbesUsedByTranslucencyVolumeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMarkRadianceProbesUsedByTranslucencyVolumeCS)
	SHADER_USE_PARAMETER_STRUCT(FMarkRadianceProbesUsedByTranslucencyVolumeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheMarkParameters, RadianceCacheMarkParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMarkRadianceProbesUsedByTranslucencyVolumeCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "MarkRadianceProbesUsedByTranslucencyVolumeCS", SF_Compute);


class FTranslucencyVolumeTraceVoxelsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucencyVolumeTraceVoxelsCS)
	SHADER_USE_PARAMETER_STRUCT(FTranslucencyVolumeTraceVoxelsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, RWVolumeTraceHitDistance)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeTraceSetupParameters, TraceSetupParameters)
	END_SHADER_PARAMETER_STRUCT()

	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");
	class FRadianceCache : SHADER_PERMUTATION_BOOL("USE_RADIANCE_CACHE");
	class FTraceFromVolume : SHADER_PERMUTATION_BOOL("TRACE_FROM_VOLUME");
	class FSimpleCoverageBasedExpand : SHADER_PERMUTATION_BOOL("GLOBALSDF_SIMPLE_COVERAGE_BASED_EXPAND");

	using FPermutationDomain = TShaderPermutationDomain<FDynamicSkyLight, FRadianceCache, FTraceFromVolume, FSimpleCoverageBasedExpand>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(8, 8, 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!PermutationVector.Get<FTraceFromVolume>() && PermutationVector.Get<FSimpleCoverageBasedExpand>())
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeTraceVoxelsCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeTraceVoxelsCS", SF_Compute);


class FTranslucencyVolumeSpatialFilterCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucencyVolumeSpatialFilterCS)
	SHADER_USE_PARAMETER_STRUCT(FTranslucencyVolumeSpatialFilterCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float3>, RWVolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeTraceHitDistance)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER(FVector3f, PreviousFrameJitterOffset)
		SHADER_PARAMETER(FMatrix44f, UnjitteredPrevWorldToClip)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(8, 8, 1);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeSpatialFilterCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeSpatialFilterCS", SF_Compute);


class FTranslucencyVolumeIntegrateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranslucencyVolumeIntegrateCS)
	SHADER_USE_PARAMETER_STRUCT(FTranslucencyVolumeIntegrateCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWTranslucencyGI0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWTranslucencyGI1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWTranslucencyGINewHistory0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWTranslucencyGINewHistory1)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenTranslucencyLightingVolumeParameters, VolumeParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeTraceRadiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, VolumeTraceHitDistance)
		SHADER_PARAMETER(float, HistoryWeight)
		SHADER_PARAMETER(FVector3f, PreviousFrameJitterOffset)
		SHADER_PARAMETER(FMatrix44f, UnjitteredPrevWorldToClip)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyGIHistory0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyGIHistory1)
		SHADER_PARAMETER_SAMPLER(SamplerState, TranslucencyGIHistorySampler)
	END_SHADER_PARAMETER_STRUCT()

	class FTemporalReprojection : SHADER_PERMUTATION_BOOL("USE_TEMPORAL_REPROJECTION");

	using FPermutationDomain = TShaderPermutationDomain<FTemporalReprojection>;

	static FIntVector GetGroupSize()
	{
		return FIntVector(4, 4, 4);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize().X);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTranslucencyVolumeIntegrateCS, "/Engine/Private/Lumen/LumenTranslucencyVolumeLighting.usf", "TranslucencyVolumeIntegrateCS", SF_Compute);

FLumenTranslucencyLightingVolumeParameters GetTranslucencyLightingVolumeParameters(const FViewInfo& View)
{
	const FIntPoint GridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GTranslucencyFroxelGridPixelSize);
	const float FarPlane = LumenTranslucencyVolume::GetEndDistanceFromCamera(View);

	FVector ZParams;
	int32 GridSizeZ;
	GetTranslucencyGridZParams(View.NearClippingDistance, FarPlane, ZParams, GridSizeZ);

	const FIntVector TranslucencyGridSize(GridSizeXY.X, GridSizeXY.Y, FMath::Max(GridSizeZ, 1));

	FLumenTranslucencyLightingVolumeParameters Parameters;
	Parameters.TranslucencyGIGridZParams = (FVector3f)ZParams;
	Parameters.TranslucencyGIGridPixelSizeShift = FMath::FloorLog2(GTranslucencyFroxelGridPixelSize);
	Parameters.TranslucencyGIGridSize = TranslucencyGridSize;

	Parameters.UseJitter = GTranslucencyVolumeJitter;
	Parameters.FrameJitterOffset = (FVector3f)TranslucencyVolumeTemporalRandom(View.ViewState ? View.ViewState->GetFrameIndex() : 0);
	Parameters.UnjitteredClipToTranslatedWorld = FMatrix44f(View.ViewMatrices.ComputeInvProjectionNoAAMatrix() * View.ViewMatrices.GetTranslatedViewMatrix().GetTransposed());		// LWC_TODO: Precision loss?
	Parameters.GridCenterOffsetFromDepthBuffer = GTranslucencyVolumeGridCenterOffsetFromDepthBuffer;
	Parameters.GridCenterOffsetThresholdToAcceptDepthBufferOffset = FMath::Max(0, GTranslucencyVolumeOffsetThresholdToAcceptDepthBufferOffset);

	Parameters.SceneTexturesStruct = View.GetSceneTextures().UniformBuffer;
		
	Parameters.TranslucencyVolumeTracingOctahedronResolution = GTranslucencyVolumeTracingOctahedronResolution;
	
	Parameters.FurthestHZBTexture = View.HZB;
	Parameters.HZBMipLevel = FMath::Max<float>((int32)FMath::FloorLog2(GTranslucencyFroxelGridPixelSize) - 1, 0.0f);
	Parameters.ViewportUVToHZBBufferUV = FVector2f(
		float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
		float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y));

	return Parameters;
}

static void MarkRadianceProbesUsedByTranslucencyVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters,
	ERDGPassFlags ComputePassFlags)
{
	FMarkRadianceProbesUsedByTranslucencyVolumeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarkRadianceProbesUsedByTranslucencyVolumeCS::FParameters>();
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->RadianceCacheMarkParameters = RadianceCacheMarkParameters;

	PassParameters->VolumeParameters = VolumeParameters;

	FMarkRadianceProbesUsedByTranslucencyVolumeCS::FPermutationDomain PermutationVector;
	auto ComputeShader = View.ShaderMap->GetShader<FMarkRadianceProbesUsedByTranslucencyVolumeCS>();

	const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(VolumeParameters.TranslucencyGIGridSize, FMarkRadianceProbesUsedByTranslucencyVolumeCS::GetGroupSize());

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MarkRadianceProbesUsedByTranslucencyVolume"),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		GroupSize);
}

void TraceVoxelsTranslucencyVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	bool bDynamicSkyLight,
	const FLumenCardTracingParameters& TracingParameters,
	LumenRadianceCache::FRadianceCacheInterpolationParameters RadianceCacheParameters,
	FLumenTranslucencyLightingVolumeParameters VolumeParameters,
	FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters,
	FRDGTextureRef VolumeTraceRadiance,
	FRDGTextureRef VolumeTraceHitDistance,
	ERDGPassFlags ComputePassFlags)
{
	FTranslucencyVolumeTraceVoxelsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeTraceVoxelsCS::FParameters>();
	PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(VolumeTraceRadiance);
	PassParameters->RWVolumeTraceHitDistance = GraphBuilder.CreateUAV(VolumeTraceHitDistance);

	PassParameters->TracingParameters = TracingParameters;
	PassParameters->RadianceCacheParameters = RadianceCacheParameters;
	PassParameters->VolumeParameters = VolumeParameters;
	PassParameters->TraceSetupParameters = TraceSetupParameters;

	const bool bTraceFromVolume = GLumenTranslucencyVolumeTraceFromVolume != 0;

	FTranslucencyVolumeTraceVoxelsCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FDynamicSkyLight>(bDynamicSkyLight);
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FRadianceCache>(RadianceCacheParameters.RadianceProbeIndirectionTexture != nullptr);
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FTraceFromVolume>(bTraceFromVolume);
	PermutationVector.Set<FTranslucencyVolumeTraceVoxelsCS::FSimpleCoverageBasedExpand>(bTraceFromVolume && Lumen::UseGlobalSDFSimpleCoverageBasedExpand());
	auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeTraceVoxelsCS>(PermutationVector);

	const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(VolumeTraceRadiance->Desc.GetSize(), FTranslucencyVolumeTraceVoxelsCS::GetGroupSize());

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("%s %ux%u", bTraceFromVolume ? TEXT("TraceVoxels") : TEXT("RadianceCacheInterpolate"), GTranslucencyVolumeTracingOctahedronResolution, GTranslucencyVolumeTracingOctahedronResolution),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		GroupSize);
}

LumenRadianceCache::FUpdateInputs FDeferredShadingSceneRenderer::GetLumenTranslucencyGIVolumeRadianceCacheInputs(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	ERDGPassFlags ComputePassFlags)
{
	const FLumenTranslucencyLightingVolumeParameters VolumeParameters = GetTranslucencyLightingVolumeParameters(View);
	const LumenRadianceCache::FRadianceCacheInputs RadianceCacheInputs = LumenTranslucencyVolumeRadianceCache::SetupRadianceCacheInputs(View);

	FRadianceCacheConfiguration Configuration;
	Configuration.bFarField = GTranslucencyVolumeRadianceCacheFarField != 0;

	FMarkUsedRadianceCacheProbes MarkUsedRadianceCacheProbesCallbacks;

	if (GLumenTranslucencyVolume && GLumenTranslucencyVolumeRadianceCache)
	{
		MarkUsedRadianceCacheProbesCallbacks.AddLambda([VolumeParameters, ComputePassFlags](
			FRDGBuilder& GraphBuilder, 
			const FViewInfo& View, 
			const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters)
			{
				MarkRadianceProbesUsedByTranslucencyVolume(
					GraphBuilder,
					View,
					VolumeParameters,
					RadianceCacheMarkParameters,
					ComputePassFlags);
			});
	}

	LumenRadianceCache::FUpdateInputs RadianceCacheUpdateInputs(
		RadianceCacheInputs,
		Configuration,
		View,
		nullptr,
		nullptr,
		FMarkUsedRadianceCacheProbes(),
		MoveTemp(MarkUsedRadianceCacheProbesCallbacks));

	return RadianceCacheUpdateInputs;
}

void FDeferredShadingSceneRenderer::ComputeLumenTranslucencyGIVolume(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View, 
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
	ERDGPassFlags ComputePassFlags)
{
	if (GLumenTranslucencyVolume)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "TranslucencyVolumeLighting");

		if (GLumenTranslucencyVolumeRadianceCache && !RadianceCacheParameters.RadianceProbeIndirectionTexture)
		{
			LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateInputs> InputArray;
			LumenRadianceCache::TInlineArray<LumenRadianceCache::FUpdateOutputs> OutputArray;

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
					RadianceCacheParameters));

				LumenRadianceCache::UpdateRadianceCaches(
					GraphBuilder, 
					FrameTemporaries,
					InputArray,
					OutputArray,
					Scene,
					ViewFamily,
					LumenCardRenderer.bPropagateGlobalLightingChange,
					ComputePassFlags);
			}
		}

		{
			FLumenCardTracingParameters TracingParameters;
			GetLumenCardTracingParameters(GraphBuilder, View, *Scene->GetLumenSceneData(View), FrameTemporaries, /*bSurfaceCacheFeedback*/ false, TracingParameters);

			const FLumenTranslucencyLightingVolumeParameters VolumeParameters = GetTranslucencyLightingVolumeParameters(View);
			const FIntVector TranslucencyGridSize = VolumeParameters.TranslucencyGIGridSize;

			FLumenTranslucencyLightingVolumeTraceSetupParameters TraceSetupParameters;
			{
				TraceSetupParameters.StepFactor = FMath::Clamp(GTranslucencyVolumeTraceStepFactor, .1f, 10.0f);
				TraceSetupParameters.MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
				TraceSetupParameters.VoxelTraceStartDistanceScale = GTranslucencyVolumeVoxelTraceStartDistanceScale;
				TraceSetupParameters.MaxRayIntensity = GTranslucencyVolumeMaxRayIntensity;
			}

			const FIntVector OctahedralAtlasSize(
				TranslucencyGridSize.X * GTranslucencyVolumeTracingOctahedronResolution, 
				TranslucencyGridSize.Y * GTranslucencyVolumeTracingOctahedronResolution,
				TranslucencyGridSize.Z);

			FRDGTextureDesc VolumeTraceRadianceDesc(FRDGTextureDesc::Create3D(OctahedralAtlasSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
			FRDGTextureDesc VolumeTraceHitDistanceDesc(FRDGTextureDesc::Create3D(OctahedralAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV));
	
			FRDGTextureRef VolumeTraceRadiance = GraphBuilder.CreateTexture(VolumeTraceRadianceDesc, TEXT("Lumen.TranslucencyVolume.VolumeTraceRadiance"));
			FRDGTextureRef VolumeTraceHitDistance = GraphBuilder.CreateTexture(VolumeTraceHitDistanceDesc, TEXT("Lumen.TranslucencyVolume.VolumeTraceHitDistance"));

			if (Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily) && GLumenTranslucencyVolumeTraceFromVolume != 0)
			{
				HardwareRayTraceTranslucencyVolume(
					GraphBuilder,
					View,
					TracingParameters,
					RadianceCacheParameters,
					VolumeParameters,
					TraceSetupParameters, 
					VolumeTraceRadiance, 
					VolumeTraceHitDistance,
					ComputePassFlags);
			}
			else
			{
				const bool bDynamicSkyLight = Lumen::ShouldHandleSkyLight(Scene, ViewFamily);
				TraceVoxelsTranslucencyVolume(
					GraphBuilder,
					View,
					bDynamicSkyLight,
					TracingParameters,
					RadianceCacheParameters,
					VolumeParameters,
					TraceSetupParameters,
					VolumeTraceRadiance,
					VolumeTraceHitDistance,
					ComputePassFlags);
			}

			if (GTranslucencyVolumeSpatialFilter)
			{
				for (int32 PassIndex = 0; PassIndex < GTranslucencyVolumeSpatialFilterNumPasses; PassIndex++)
				{
					FRDGTextureRef FilteredVolumeTraceRadiance = GraphBuilder.CreateTexture(VolumeTraceRadianceDesc, TEXT("Lumen.TranslucencyVolume.FilteredVolumeTraceRadiance"));

					FTranslucencyVolumeSpatialFilterCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeSpatialFilterCS::FParameters>();
					PassParameters->RWVolumeTraceRadiance = GraphBuilder.CreateUAV(FilteredVolumeTraceRadiance);

					PassParameters->VolumeTraceRadiance = VolumeTraceRadiance;
					PassParameters->VolumeTraceHitDistance = VolumeTraceHitDistance;
					PassParameters->View = View.ViewUniformBuffer;
					PassParameters->VolumeParameters = VolumeParameters;
					const int32 PreviousFrameIndexOffset = View.bStatePrevViewInfoIsReadOnly ? 0 : 1;
					PassParameters->PreviousFrameJitterOffset = (FVector3f)TranslucencyVolumeTemporalRandom(View.ViewState ? View.ViewState->GetFrameIndex() - PreviousFrameIndexOffset : 0);
					PassParameters->UnjitteredPrevWorldToClip = FMatrix44f(View.PrevViewInfo.ViewMatrices.GetViewMatrix() * View.PrevViewInfo.ViewMatrices.ComputeProjectionNoAAMatrix());		// LWC_TODO: Precision loss?

					FTranslucencyVolumeSpatialFilterCS::FPermutationDomain PermutationVector;
					auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeSpatialFilterCS>(PermutationVector);

					const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(OctahedralAtlasSize, FTranslucencyVolumeSpatialFilterCS::GetGroupSize());

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("SpatialFilter"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						GroupSize);

					VolumeTraceRadiance = FilteredVolumeTraceRadiance;
				}
			}

			FRDGTextureRef TranslucencyGIVolumeHistory0 = nullptr;
			FRDGTextureRef TranslucencyGIVolumeHistory1 = nullptr;

			if (View.ViewState && View.ViewState->Lumen.TranslucencyVolume0)
			{
				TranslucencyGIVolumeHistory0 = GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.TranslucencyVolume0);
				TranslucencyGIVolumeHistory1 = GraphBuilder.RegisterExternalTexture(View.ViewState->Lumen.TranslucencyVolume1);
			}

			FRDGTextureDesc LumenTranslucencyGIDesc0(FRDGTextureDesc::Create3D(TranslucencyGridSize, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_3DTiling));
			FRDGTextureDesc LumenTranslucencyGIDesc1(FRDGTextureDesc::Create3D(TranslucencyGridSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_3DTiling));
	
			FRDGTextureRef TranslucencyGIVolume0 = GraphBuilder.CreateTexture(LumenTranslucencyGIDesc0, TEXT("Lumen.TranslucencyVolume.SHLighting0"));
			FRDGTextureRef TranslucencyGIVolume1 = GraphBuilder.CreateTexture(LumenTranslucencyGIDesc1, TEXT("Lumen.TranslucencyVolume.SHLighting1"));
			FRDGTextureUAVRef TranslucencyGIVolume0UAV = GraphBuilder.CreateUAV(TranslucencyGIVolume0);
			FRDGTextureUAVRef TranslucencyGIVolume1UAV = GraphBuilder.CreateUAV(TranslucencyGIVolume1);

			FRDGTextureRef TranslucencyGIVolumeNewHistory0 = GraphBuilder.CreateTexture(LumenTranslucencyGIDesc0, TEXT("Lumen.TranslucencyVolume.SHLightingNewHistory0"));
			FRDGTextureRef TranslucencyGIVolumeNewHistory1 = GraphBuilder.CreateTexture(LumenTranslucencyGIDesc1, TEXT("Lumen.TranslucencyVolume.SHLightingNewHistory0"));
			FRDGTextureUAVRef TranslucencyGIVolumeNewHistory0UAV = GraphBuilder.CreateUAV(TranslucencyGIVolumeNewHistory0);
			FRDGTextureUAVRef TranslucencyGIVolumeNewHistory1UAV = GraphBuilder.CreateUAV(TranslucencyGIVolumeNewHistory1);

			{
				FTranslucencyVolumeIntegrateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranslucencyVolumeIntegrateCS::FParameters>();
				PassParameters->RWTranslucencyGI0 = TranslucencyGIVolume0UAV;
				PassParameters->RWTranslucencyGI1 = TranslucencyGIVolume1UAV;
				PassParameters->RWTranslucencyGINewHistory0 = TranslucencyGIVolumeNewHistory0UAV;
				PassParameters->RWTranslucencyGINewHistory1 = TranslucencyGIVolumeNewHistory1UAV;

				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->VolumeTraceRadiance = VolumeTraceRadiance;
				PassParameters->VolumeTraceHitDistance = VolumeTraceHitDistance;
				PassParameters->VolumeParameters = VolumeParameters;

				const bool bUseTemporalReprojection =
					GTranslucencyVolumeTemporalReprojection
					&& View.ViewState
					&& !View.bCameraCut
					&& !View.bPrevTransformsReset
					&& ViewFamily.bRealtimeUpdate
					&& TranslucencyGIVolumeHistory0
					&& TranslucencyGIVolumeHistory0->Desc == LumenTranslucencyGIDesc0;

				PassParameters->HistoryWeight = GTranslucencyVolumeHistoryWeight;
				const int32 PreviousFrameIndexOffset = View.bStatePrevViewInfoIsReadOnly ? 0 : 1;
				PassParameters->PreviousFrameJitterOffset = (FVector3f)TranslucencyVolumeTemporalRandom(View.ViewState ? View.ViewState->GetFrameIndex() - PreviousFrameIndexOffset : 0);
				PassParameters->UnjitteredPrevWorldToClip = FMatrix44f(View.PrevViewInfo.ViewMatrices.GetViewMatrix() * View.PrevViewInfo.ViewMatrices.ComputeProjectionNoAAMatrix());		// LWC_TODO: Precision loss?
				PassParameters->TranslucencyGIHistory0 = TranslucencyGIVolumeHistory0;
				PassParameters->TranslucencyGIHistory1 = TranslucencyGIVolumeHistory1;
				PassParameters->TranslucencyGIHistorySampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

				FTranslucencyVolumeIntegrateCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FTranslucencyVolumeIntegrateCS::FTemporalReprojection>(bUseTemporalReprojection);
				auto ComputeShader = View.ShaderMap->GetShader<FTranslucencyVolumeIntegrateCS>(PermutationVector);

				const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(TranslucencyGridSize, FTranslucencyVolumeIntegrateCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Integrate %ux%ux%u", TranslucencyGridSize.X, TranslucencyGridSize.Y, TranslucencyGridSize.Z),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					GroupSize);
			}

			if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
			{
				View.ViewState->Lumen.TranslucencyVolume0 = GraphBuilder.ConvertToExternalTexture(TranslucencyGIVolumeNewHistory0);
				View.ViewState->Lumen.TranslucencyVolume1 = GraphBuilder.ConvertToExternalTexture(TranslucencyGIVolumeNewHistory1);
			}

			View.GetOwnLumenTranslucencyGIVolume().Texture0 = TranslucencyGIVolume0;
			View.GetOwnLumenTranslucencyGIVolume().Texture1 = TranslucencyGIVolume1;

			View.GetOwnLumenTranslucencyGIVolume().HistoryTexture0 = TranslucencyGIVolumeNewHistory0;
			View.GetOwnLumenTranslucencyGIVolume().HistoryTexture1 = TranslucencyGIVolumeNewHistory1;

			View.GetOwnLumenTranslucencyGIVolume().GridZParams = (FVector)VolumeParameters.TranslucencyGIGridZParams;
			View.GetOwnLumenTranslucencyGIVolume().GridPixelSizeShift = FMath::FloorLog2(GTranslucencyFroxelGridPixelSize);
			View.GetOwnLumenTranslucencyGIVolume().GridSize = TranslucencyGridSize;
		}
	}
}
