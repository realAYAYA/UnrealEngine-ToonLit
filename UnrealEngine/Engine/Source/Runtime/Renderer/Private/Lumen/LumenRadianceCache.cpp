// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenRadianceCache.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "LumenScreenProbeGather.h"
#include "LumenSceneLighting.h"
#include "ShaderPrintParameters.h"

int32 GRadianceCacheUpdate = 1;
FAutoConsoleVariableRef CVarRadianceCacheUpdate(
	TEXT("r.Lumen.RadianceCache.Update"),
	GRadianceCacheUpdate,
	TEXT("Whether to update radiance cache every frame"),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheForceFullUpdate = 0;
FAutoConsoleVariableRef CVarRadianceForceFullUpdate(
	TEXT("r.Lumen.RadianceCache.ForceFullUpdate"),
	GRadianceCacheForceFullUpdate,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRadianceCacheNumFramesToKeepCachedProbes(
	TEXT("r.Lumen.RadianceCache.NumFramesToKeepCachedProbes"),
	8,
	TEXT("Number of frames to keep unused probes in cache. Higher values enable more reuse between frames, but too high values will cause filtering from stale probes."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GRadianceCacheOverrideCacheOcclusionLighting = 0;
FAutoConsoleVariableRef CVarRadianceCacheShowOnlyRadianceCacheLighting(
	TEXT("r.Lumen.RadianceCache.OverrideCacheOcclusionLighting"),
	GRadianceCacheOverrideCacheOcclusionLighting,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheShowBlackRadianceCacheLighting = 0;
FAutoConsoleVariableRef CVarRadianceCacheShowBlackRadianceCacheLighting(
	TEXT("r.Lumen.RadianceCache.ShowBlackRadianceCacheLighting"),
	GRadianceCacheShowBlackRadianceCacheLighting,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheFilterProbes = 1;
FAutoConsoleVariableRef CVarRadianceCacheFilterProbes(
	TEXT("r.Lumen.RadianceCache.SpatialFilterProbes"),
	GRadianceCacheFilterProbes,
	TEXT("Whether to filter probe radiance between neighbors"),
	ECVF_RenderThreadSafe
);

int32 GRadianceCacheSortTraceTiles = 0;
FAutoConsoleVariableRef CVarRadianceCacheSortTraceTiles(
	TEXT("r.Lumen.RadianceCache.SortTraceTiles"),
	GRadianceCacheSortTraceTiles,
	TEXT("Whether to sort Trace Tiles by direction before tracing to extract coherency"),
	ECVF_RenderThreadSafe
);

float GLumenRadianceCacheFilterMaxRadianceHitAngle = .2f;
FAutoConsoleVariableRef GVarLumenRadianceCacheFilterMaxRadianceHitAngle(
	TEXT("r.Lumen.RadianceCache.SpatialFilterMaxRadianceHitAngle"),
	GLumenRadianceCacheFilterMaxRadianceHitAngle,
	TEXT("In Degrees.  Larger angles allow filtering of nearby features but more leaking."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenRadianceCacheForceUniformTraceTileLevel = -1;
FAutoConsoleVariableRef CVarLumenRadianceCacheForceUniformTraceTileLevel(
	TEXT("r.Lumen.RadianceCache.ForceUniformTraceTileLevel"),
	GLumenRadianceCacheForceUniformTraceTileLevel,
	TEXT("When set to >= 0, forces a uniform trace tile level for debugging, and overrides trace tile BRDF importance sampling.  Valid range is [0, 2].  0 = half res, 1 = full res, 2 = supersampled"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenRadianceCacheSupersampleTileBRDFThreshold = .1f;
FAutoConsoleVariableRef CVarLumenRadianceCacheSupersampleTileBRDFThreshold(
	TEXT("r.Lumen.RadianceCache.SupersampleTileBRDFThreshold"),
	GLumenRadianceCacheSupersampleTileBRDFThreshold,
	TEXT("Value of the BRDF [0-1] above which to trace more rays to supersample the probe radiance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenRadianceCacheSupersampleDistanceFromCamera = 2000.0f;
FAutoConsoleVariableRef CVarLumenRadianceCacheSupersampleDistanceFromCamera(
	TEXT("r.Lumen.RadianceCache.SupersampleDistanceFromCamera"),
	GLumenRadianceCacheSupersampleDistanceFromCamera,
	TEXT("Only probes closer to the camera than this distance can be supersampled."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenRadianceCacheDownsampleDistanceFromCamera = 4000.0f;
FAutoConsoleVariableRef CVarLumenRadianceCacheDownsampleDistanceFromCamera(
	TEXT("r.Lumen.RadianceCache.DownsampleDistanceFromCamera"),
	GLumenRadianceCacheDownsampleDistanceFromCamera,
	TEXT("Probes further than this distance from the camera are always downsampled."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace LumenRadianceCache
{
	// Must match LumenRadianceCacheCommon.ush
	constexpr uint32 PRIORITY_HISTOGRAM_SIZE = 16;
	constexpr uint32 PROBES_TO_UPDATE_TRACE_COST_STRIDE = 2;

	FRadianceCacheInputs GetDefaultRadianceCacheInputs()
	{
		FRadianceCacheInputs RadianceCacheInputs;
		RadianceCacheInputs.CalculateIrradiance = 0;
		RadianceCacheInputs.IrradianceProbeResolution = 0;
		RadianceCacheInputs.InvClipmapFadeSize = 1.0f;
		return RadianceCacheInputs;
	}

	void GetInterpolationParametersNoResources(
		FRDGBuilder& GraphBuilder, 
		const FRadianceCacheState& RadianceCacheState,
		const LumenRadianceCache::FRadianceCacheInputs& RadianceCacheInputs, 
		FRadianceCacheInterpolationParameters& OutParameters)
	{
		OutParameters.RadianceCacheInputs = RadianceCacheInputs;
		OutParameters.RadianceCacheInputs.NumProbesToTraceBudget = GRadianceCacheForceFullUpdate ? UINT32_MAX : OutParameters.RadianceCacheInputs.NumProbesToTraceBudget;
		OutParameters.RadianceProbeIndirectionTexture = nullptr;
		OutParameters.RadianceCacheFinalRadianceAtlas = nullptr;
		OutParameters.RadianceCacheFinalIrradianceAtlas = nullptr;
		OutParameters.RadianceCacheProbeOcclusionAtlas = nullptr;
		OutParameters.RadianceCacheDepthAtlas = nullptr;
		OutParameters.ProbeWorldOffset = nullptr;
		OutParameters.OverrideCacheOcclusionLighting = GRadianceCacheOverrideCacheOcclusionLighting;
		OutParameters.ShowBlackRadianceCacheLighting = GRadianceCacheShowBlackRadianceCacheLighting;
		OutParameters.ProbeAtlasResolutionModuloMask = (1u << FMath::FloorLog2(RadianceCacheInputs.ProbeAtlasResolutionInProbes.X)) - 1;
		OutParameters.ProbeAtlasResolutionDivideShift = FMath::FloorLog2(RadianceCacheInputs.ProbeAtlasResolutionInProbes.X);

		for (int32 ClipmapIndex = 0; ClipmapIndex < RadianceCacheState.Clipmaps.Num(); ++ClipmapIndex)
		{
			const FRadianceCacheClipmap& Clipmap = RadianceCacheState.Clipmaps[ClipmapIndex];

			SetRadianceProbeClipmapTMin(OutParameters, ClipmapIndex, Clipmap.ProbeTMin);
			SetWorldPositionToRadianceProbeCoordScale(OutParameters, ClipmapIndex, Clipmap.WorldPositionToProbeCoordScale);
			SetWorldPositionToRadianceProbeCoordBias(OutParameters, ClipmapIndex, (FVector3f)Clipmap.WorldPositionToProbeCoordBias);
			SetRadianceProbeCoordToWorldPositionScale(OutParameters, ClipmapIndex, Clipmap.ProbeCoordToWorldCenterScale);
			SetRadianceProbeCoordToWorldPositionBias(OutParameters, ClipmapIndex, (FVector3f)Clipmap.ProbeCoordToWorldCenterBias);
		}

		const FVector2f ProbeAtlasResolutionInProbesAsFloat = FVector2f(RadianceCacheInputs.ProbeAtlasResolutionInProbes);
		OutParameters.InvProbeFinalRadianceAtlasResolution = FVector2f::UnitVector / (RadianceCacheInputs.FinalProbeResolution * ProbeAtlasResolutionInProbesAsFloat);	// LWC_TODO: Fix! Used to be FVector2D(RadianceCacheInputs.FinalProbeResolution * RadianceCacheInputs.ProbeAtlasResolutionInProbes). No auto conversion of ProbeAtlastResolutionInProbes to FVector2D. ADL thing?
		const int32 FinalIrradianceProbeResolution = RadianceCacheInputs.IrradianceProbeResolution + 2 * (1 << RadianceCacheInputs.FinalRadianceAtlasMaxMip);
		OutParameters.InvProbeFinalIrradianceAtlasResolution = FVector2f::UnitVector / (FinalIrradianceProbeResolution * ProbeAtlasResolutionInProbesAsFloat);
		OutParameters.InvProbeDepthAtlasResolution = FVector2f::UnitVector / (RadianceCacheInputs.RadianceProbeResolution * ProbeAtlasResolutionInProbesAsFloat);
	}

	void GetInterpolationParameters(
		const FViewInfo& View, 
		FRDGBuilder& GraphBuilder, 
		const FRadianceCacheState& RadianceCacheState,
		const LumenRadianceCache::FRadianceCacheInputs& RadianceCacheInputs,
		FRadianceCacheInterpolationParameters& OutParameters)
	{
		GetInterpolationParametersNoResources(GraphBuilder, RadianceCacheState, RadianceCacheInputs, OutParameters);

		OutParameters.RadianceProbeIndirectionTexture = RadianceCacheState.RadianceProbeIndirectionTexture ? GraphBuilder.RegisterExternalTexture(RadianceCacheState.RadianceProbeIndirectionTexture, TEXT("Lumen.RadianceCacheIndirectionTexture")) : nullptr;
		OutParameters.RadianceCacheFinalRadianceAtlas = RadianceCacheState.FinalRadianceAtlas ? GraphBuilder.RegisterExternalTexture(RadianceCacheState.FinalRadianceAtlas, TEXT("Lumen.RadianceCacheFinalRadianceAtlas")) : nullptr;
		OutParameters.RadianceCacheFinalIrradianceAtlas = RadianceCacheState.FinalIrradianceAtlas ? GraphBuilder.RegisterExternalTexture(RadianceCacheState.FinalIrradianceAtlas, TEXT("Lumen.RadianceCacheFinalIrradianceAtlas")) : nullptr;
		OutParameters.RadianceCacheProbeOcclusionAtlas = RadianceCacheState.ProbeOcclusionAtlas ? GraphBuilder.RegisterExternalTexture(RadianceCacheState.ProbeOcclusionAtlas, TEXT("Lumen.RadianceCacheProbeOcclusionAtlas")) : nullptr;
		OutParameters.RadianceCacheDepthAtlas = RadianceCacheState.DepthProbeAtlasTexture ? GraphBuilder.RegisterExternalTexture(RadianceCacheState.DepthProbeAtlasTexture, TEXT("Lumen.RadianceCacheDepthAtlas")) : nullptr;
		FRDGBufferRef ProbeWorldOffset = RadianceCacheState.ProbeWorldOffset ? GraphBuilder.RegisterExternalBuffer(RadianceCacheState.ProbeWorldOffset) : nullptr;
		OutParameters.ProbeWorldOffset = ProbeWorldOffset ? GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeWorldOffset, PF_A32B32G32R32F)) : nullptr;
	}

	FRadianceCacheMarkParameters GetMarkParameters(
		FRDGTextureUAVRef RadianceProbeIndirectionTextureUAV, 
		const FRadianceCacheState& RadianceCacheState, 
		const LumenRadianceCache::FRadianceCacheInputs& RadianceCacheInputs)
	{
		FRadianceCacheMarkParameters MarkParameters;
		MarkParameters.RWRadianceProbeIndirectionTexture = RadianceProbeIndirectionTextureUAV;

		for (int32 ClipmapIndex = 0; ClipmapIndex < RadianceCacheState.Clipmaps.Num(); ++ClipmapIndex)
		{
			const FRadianceCacheClipmap& Clipmap = RadianceCacheState.Clipmaps[ClipmapIndex];

			SetWorldPositionToRadianceProbeCoord(MarkParameters.PackedWorldPositionToRadianceProbeCoord[ClipmapIndex], (FVector3f)Clipmap.WorldPositionToProbeCoordBias, Clipmap.WorldPositionToProbeCoordScale);
			SetRadianceProbeCoordToWorldPosition(MarkParameters.PackedRadianceProbeCoordToWorldPosition[ClipmapIndex], (FVector3f)Clipmap.ProbeCoordToWorldCenterBias, Clipmap.ProbeCoordToWorldCenterScale);
		}

		MarkParameters.RadianceProbeClipmapResolutionForMark = RadianceCacheInputs.RadianceProbeClipmapResolution;
		MarkParameters.NumRadianceProbeClipmapsForMark = RadianceCacheInputs.NumRadianceProbeClipmaps;
		MarkParameters.InvClipmapFadeSizeForMark = RadianceCacheInputs.InvClipmapFadeSize;

		return MarkParameters;
	}
};

class FMarkRadianceProbesUsedByVisualizeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMarkRadianceProbesUsedByVisualizeCS)
	SHADER_USE_PARAMETER_STRUCT(FMarkRadianceProbesUsedByVisualizeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
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

IMPLEMENT_GLOBAL_SHADER(FMarkRadianceProbesUsedByVisualizeCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "MarkRadianceProbesUsedByVisualizeCS", SF_Compute);

void MarkUsedProbesForVisualize(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters,
	ERDGPassFlags ComputePassFlags)
{
	extern int32 GVisualizeLumenSceneTraceRadianceCache;

	if (View.Family->EngineShowFlags.VisualizeLumen && GVisualizeLumenSceneTraceRadianceCache != 0)
	{
		FMarkRadianceProbesUsedByVisualizeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarkRadianceProbesUsedByVisualizeCS::FParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->RadianceCacheMarkParameters = RadianceCacheMarkParameters;

		auto ComputeShader = View.ShaderMap->GetShader<FMarkRadianceProbesUsedByVisualizeCS>(0);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("MarkRadianceProbes(Visualize)"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}
}

class FClearProbeFreeList : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearProbeFreeList)
	SHADER_USE_PARAMETER_STRUCT(FClearProbeFreeList, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, RWProbeFreeListAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeFreeList)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeLastUsedFrame)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, RWProbeWorldOffset)
		SHADER_PARAMETER(uint32, MaxNumProbes)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearProbeFreeList, "/Engine/Private/Lumen/LumenRadianceCacheUpdate.usf", "ClearProbeFreeListCS", SF_Compute);

class FClearProbeIndirectionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearProbeIndirectionCS)
	SHADER_USE_PARAMETER_STRUCT(FClearProbeIndirectionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, RWRadianceProbeIndirectionTexture)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 4;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearProbeIndirectionCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "ClearProbeIndirectionCS", SF_Compute);

class FUpdateCacheForUsedProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FUpdateCacheForUsedProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FUpdateCacheForUsedProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, RWRadianceProbeIndirectionTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, RWProbeFreeListAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeFreeList)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeLastUsedFrame)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, LastFrameRadianceProbeIndirectionTexture)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_ARRAY(FVector4f, PackedLastFrameRadianceProbeCoordToWorldPosition, [LumenRadianceCache::MaxClipmaps])
		SHADER_PARAMETER(uint32, FrameNumber)
		SHADER_PARAMETER(uint32, NumFramesToKeepCachedProbes)
		SHADER_PARAMETER(uint32, MaxNumProbes)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 4;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FUpdateCacheForUsedProbesCS, "/Engine/Private/Lumen/LumenRadianceCacheUpdate.usf", "UpdateCacheForUsedProbesCS", SF_Compute);

class FClearRadianceCacheUpdateResourcesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearRadianceCacheUpdateResourcesCS);
	SHADER_USE_PARAMETER_STRUCT(FClearRadianceCacheUpdateResourcesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeTraceAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWPriorityHistogram)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWMaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWMaxTracesFromMaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWProbesToUpdateTraceCost)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearRadianceCacheUpdateResourcesCS, "/Engine/Private/Lumen/LumenRadianceCacheUpdate.usf", "ClearRadianceCacheUpdateResourcesCS", SF_Compute);

class FAllocateUsedProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FAllocateUsedProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FAllocateUsedProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, RWRadianceProbeIndirectionTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWProbeLastTracedFrame)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWPriorityHistogram)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeLastUsedFrame)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, RWProbeFreeListAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeFreeList)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(float, DownsampleDistanceFromCameraSq)
		SHADER_PARAMETER(float, SupersampleDistanceFromCameraSq)
		SHADER_PARAMETER(float, FirstClipmapWorldExtentRcp)
		SHADER_PARAMETER(uint32, FrameNumber)
		SHADER_PARAMETER(uint32, MaxNumProbes)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
	END_SHADER_PARAMETER_STRUCT()

	class FPersistentCache : SHADER_PERMUTATION_BOOL("PERSISTENT_CACHE");
	using FPermutationDomain = TShaderPermutationDomain<FPersistentCache>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:

	static uint32 GetGroupSize()
	{
		return 4;
	}
};

IMPLEMENT_GLOBAL_SHADER(FAllocateUsedProbesCS, "/Engine/Private/Lumen/LumenRadianceCacheUpdate.usf", "AllocateUsedProbesCS", SF_Compute);

class FAllocateProbeTracesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FAllocateProbeTracesCS)
	SHADER_USE_PARAMETER_STRUCT(FAllocateProbeTracesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, RWRadianceProbeIndirectionTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWProbeLastTracedFrame)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWProbesToUpdateTraceCost)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeTraceAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, RWProbeTraceData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, RWProbeFreeListAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MaxTracesFromMaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeLastUsedFrame)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeFreeList)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(float, FirstClipmapWorldExtentRcp)
		SHADER_PARAMETER(float, DownsampleDistanceFromCameraSq)
		SHADER_PARAMETER(float, SupersampleDistanceFromCameraSq)
		SHADER_PARAMETER(uint32, FrameNumber)
		SHADER_PARAMETER(uint32, MaxNumProbes)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:

	static uint32 GetGroupSize()
	{
		return 4;
	}
};

IMPLEMENT_GLOBAL_SHADER(FAllocateProbeTracesCS, "/Engine/Private/Lumen/LumenRadianceCacheUpdate.usf", "AllocateProbeTracesCS", SF_Compute);

class FSelectMaxPriorityBucketCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSelectMaxPriorityBucketCS)
	SHADER_USE_PARAMETER_STRUCT(FSelectMaxPriorityBucketCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWMaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWMaxTracesFromMaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PriorityHistogram)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceAllocator)
		SHADER_PARAMETER(uint32, NumProbesToTraceBudget)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:

	static uint32 GetGroupSize()
	{
		return 1;
	}
};

IMPLEMENT_GLOBAL_SHADER(FSelectMaxPriorityBucketCS, "/Engine/Private/Lumen/LumenRadianceCacheUpdate.usf", "SelectMaxPriorityBucketCS", SF_Compute);

class FRadianceCacheUpdateStatsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRadianceCacheUpdateStatsCS)
	SHADER_USE_PARAMETER_STRUCT(FRadianceCacheUpdateStatsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PriorityHistogram)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MaxTracesFromMaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ProbesToUpdateTraceCost)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeFreeListAllocator)
		SHADER_PARAMETER(uint32, MaxNumProbes)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:

	static uint32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FRadianceCacheUpdateStatsCS, "/Engine/Private/Lumen/LumenRadianceCacheDebug.usf", "RadianceCacheUpdateStatsCS", SF_Compute);

class FSetupProbeIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSetupProbeIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FSetupProbeIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, RWProbeFreeListAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWClearProbePDFsIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWGenerateProbeTraceTilesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeTraceTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWFilterProbesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWPrepareProbeOcclusionIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWFixupProbeBordersIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceAllocator)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER(uint32, TraceFromProbesGroupSizeXY)
		SHADER_PARAMETER(uint32, FilterProbesGroupSizeXY)
		SHADER_PARAMETER(uint32, ClearProbePDFGroupSize)
		SHADER_PARAMETER(uint32, MaxNumProbes)
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

IMPLEMENT_GLOBAL_SHADER(FSetupProbeIndirectArgsCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "SetupProbeIndirectArgsCS", SF_Compute);


class FComputeProbeWorldOffsetsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FComputeProbeWorldOffsetsCS)
	SHADER_USE_PARAMETER_STRUCT(FComputeProbeWorldOffsetsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWProbeWorldOffset)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FComputeProbeWorldOffsetsCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "ComputeProbeWorldOffsetsCS", SF_Compute);


class FClearProbePDFs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearProbePDFs)
	SHADER_USE_PARAMETER_STRUCT(FClearProbePDFs, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWRadianceProbeSH_PDF)
		RDG_BUFFER_ACCESS(ClearProbePDFsIndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearProbePDFs, "/Engine/Private/Lumen/LumenRadianceCache.usf", "ClearProbePDFs", SF_Compute);


class FScatterScreenProbeBRDFToRadianceProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScatterScreenProbeBRDFToRadianceProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FScatterScreenProbeBRDFToRadianceProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWRadianceProbeSH_PDF)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, BRDFProbabilityDensityFunctionSH)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FScreenProbeParameters, ScreenProbeParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
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

IMPLEMENT_GLOBAL_SHADER(FScatterScreenProbeBRDFToRadianceProbesCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "ScatterScreenProbeBRDFToRadianceProbesCS", SF_Compute);

class FGenerateProbeTraceTilesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateProbeTraceTilesCS)
	SHADER_USE_PARAMETER_STRUCT(FGenerateProbeTraceTilesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeTraceTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, RWProbeTraceTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, RadianceProbeSH_PDF)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ProbesToUpdateTraceCost)
		SHADER_PARAMETER(float, SupersampleTileBRDFThreshold)
		SHADER_PARAMETER(float, SupersampleDistanceFromCameraSq)
		SHADER_PARAMETER(float, DownsampleDistanceFromCameraSq)
		SHADER_PARAMETER(int32, ForcedUniformLevel)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDebugBRDFProbabilityDensityFunction)
		SHADER_PARAMETER(uint32, DebugProbeBRDFOctahedronResolution)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		RDG_BUFFER_ACCESS(GenerateProbeTraceTilesIndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

public:

	class FUniformTraces : SHADER_PERMUTATION_BOOL("FORCE_UNIFORM_TRACES");
	using FPermutationDomain = TShaderPermutationDomain<FUniformTraces>;
	
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

		// Workaround for an internal PC FXC compiler crash when compiling with disabled optimizations
		if (Parameters.Platform == SP_PCD3D_SM5)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_ForceOptimization);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateProbeTraceTilesCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "GenerateProbeTraceTilesCS", SF_Compute);


class FSetupTraceFromProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSetupTraceFromProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FSetupTraceFromProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTraceProbesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWSortProbeTraceTilesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWRadianceCacheHardwareRayTracingIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWHardwareRayTracingRayAllocatorBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceTileAllocator)
		SHADER_PARAMETER(uint32, SortTraceTilesGroupSize)
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

IMPLEMENT_GLOBAL_SHADER(FSetupTraceFromProbesCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "SetupTraceFromProbesCS", SF_Compute);

class FSortProbeTraceTilesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSortProbeTraceTilesCS)
	SHADER_USE_PARAMETER_STRUCT(FSortProbeTraceTilesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWProbeTraceTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, ProbeTraceTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceTileAllocator)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInputs, RadianceCacheInputs)
		RDG_BUFFER_ACCESS(SortProbeTraceTilesIndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		// Group size affects sorting window, the larger the group the more coherency can be extracted
		return 1024;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SORT_TILES_THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FSortProbeTraceTilesCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "SortProbeTraceTilesCS", SF_Compute);



class FRadianceCacheTraceFromProbesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRadianceCacheTraceFromProbesCS)
	SHADER_USE_PARAMETER_STRUCT(FRadianceCacheTraceFromProbesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWRadianceProbeAtlasTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWDepthProbeAtlasTexture)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenCardTracingParameters, TracingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenIndirectTracingParameters, IndirectTracingParameters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, ProbeTraceTileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProbeTraceTileAllocator)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		RDG_BUFFER_ACCESS(TraceProbesIndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FTraceGlobalSDF : SHADER_PERMUTATION_BOOL("TRACE_GLOBAL_SDF");
	class FSimpleCoverageBasedExpand : SHADER_PERMUTATION_BOOL("GLOBALSDF_SIMPLE_COVERAGE_BASED_EXPAND");
	class FDynamicSkyLight : SHADER_PERMUTATION_BOOL("ENABLE_DYNAMIC_SKY_LIGHT");

	using FPermutationDomain = TShaderPermutationDomain<FTraceGlobalSDF, FSimpleCoverageBasedExpand, FDynamicSkyLight>;

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!PermutationVector.Get<FTraceGlobalSDF>() && PermutationVector.Get<FSimpleCoverageBasedExpand>())
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		// Must match RADIANCE_CACHE_TRACE_TILE_SIZE_2D
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Workaround for an internal PC FXC compiler crash when compiling with disabled optimizations
		if (Parameters.Platform == SP_PCD3D_SM5)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_ForceOptimization);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FRadianceCacheTraceFromProbesCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "TraceFromProbesCS", SF_Compute);


class FFilterProbeRadianceWithGatherCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFilterProbeRadianceWithGatherCS)
	SHADER_USE_PARAMETER_STRUCT(FFilterProbeRadianceWithGatherCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWRadianceProbeAtlasTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RadianceProbeAtlasTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthProbeAtlasTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		RDG_BUFFER_ACCESS(FilterProbesIndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER(float, SpatialFilterMaxRadianceHitAngle)
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

		// Workaround for an internal PC FXC compiler crash when compiling with disabled optimizations
		if (Parameters.Platform == SP_PCD3D_SM5)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_ForceOptimization);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FFilterProbeRadianceWithGatherCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "FilterProbeRadianceWithGatherCS", SF_Compute);


class FCalculateProbeIrradianceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateProbeIrradianceCS)
	SHADER_USE_PARAMETER_STRUCT(FCalculateProbeIrradianceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWFinalIrradianceAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RadianceProbeAtlasTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		RDG_BUFFER_ACCESS(CalculateProbeIrradianceIndirectArgs, ERHIAccess::IndirectArgs)
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

IMPLEMENT_GLOBAL_SHADER(FCalculateProbeIrradianceCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "CalculateProbeIrradianceCS", SF_Compute);


class FPrepareProbeOcclusionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPrepareProbeOcclusionCS)
	SHADER_USE_PARAMETER_STRUCT(FPrepareProbeOcclusionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWRadianceCacheProbeOcclusionAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthProbeAtlasTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		RDG_BUFFER_ACCESS(PrepareProbeOcclusionIndirectArgs, ERHIAccess::IndirectArgs)
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

IMPLEMENT_GLOBAL_SHADER(FPrepareProbeOcclusionCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "PrepareProbeOcclusionCS", SF_Compute);


class FFixupBordersAndGenerateMipsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FFixupBordersAndGenerateMipsCS)
	SHADER_USE_PARAMETER_STRUCT(FFixupBordersAndGenerateMipsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWFinalRadianceAtlasMip0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWFinalRadianceAtlasMip1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWFinalRadianceAtlasMip2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RadianceProbeAtlasTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, ProbeTraceData)
		SHADER_PARAMETER_STRUCT_INCLUDE(LumenRadianceCache::FRadianceCacheInterpolationParameters, RadianceCacheParameters)
		RDG_BUFFER_ACCESS(FixupProbeBordersIndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

public:

	class FGenerateMips : SHADER_PERMUTATION_BOOL("GENERATE_MIPS");
	using FPermutationDomain = TShaderPermutationDomain<FGenerateMips>;

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

IMPLEMENT_GLOBAL_SHADER(FFixupBordersAndGenerateMipsCS, "/Engine/Private/Lumen/LumenRadianceCache.usf", "FixupBordersAndGenerateMipsCS", SF_Compute);

bool UpdateRadianceCacheState(FRDGBuilder& GraphBuilder, const FViewInfo& View, const LumenRadianceCache::FRadianceCacheInputs& RadianceCacheInputs, FRadianceCacheState& CacheState)
{
	bool bResetState = CacheState.ClipmapWorldExtent != RadianceCacheInputs.ClipmapWorldExtent || CacheState.ClipmapDistributionBase != RadianceCacheInputs.ClipmapDistributionBase;

	CacheState.ClipmapWorldExtent = RadianceCacheInputs.ClipmapWorldExtent;
	CacheState.ClipmapDistributionBase = RadianceCacheInputs.ClipmapDistributionBase;

	const int32 ClipmapResolution = RadianceCacheInputs.RadianceProbeClipmapResolution;
	const int32 NumClipmaps = RadianceCacheInputs.NumRadianceProbeClipmaps;

	const FVector NewViewOrigin = View.ViewMatrices.GetViewOrigin();

	CacheState.Clipmaps.SetNum(NumClipmaps);

	for (int32 ClipmapIndex = 0; ClipmapIndex < NumClipmaps; ++ClipmapIndex)
	{
		FRadianceCacheClipmap& Clipmap = CacheState.Clipmaps[ClipmapIndex];

		const float ClipmapExtent = RadianceCacheInputs.ClipmapWorldExtent * FMath::Pow(RadianceCacheInputs.ClipmapDistributionBase, ClipmapIndex);
		const float CellSize = (2.0f * ClipmapExtent) / ClipmapResolution;

		FIntVector GridCenter;
		GridCenter.X = FMath::FloorToInt(NewViewOrigin.X / CellSize);
		GridCenter.Y = FMath::FloorToInt(NewViewOrigin.Y / CellSize);
		GridCenter.Z = FMath::FloorToInt(NewViewOrigin.Z / CellSize);

		const FVector SnappedCenter = FVector(GridCenter) * CellSize;

		Clipmap.Center = SnappedCenter;
		Clipmap.Extent = ClipmapExtent;
		Clipmap.VolumeUVOffset = FVector(0.0f, 0.0f, 0.0f);
		Clipmap.CellSize = CellSize;

		// Shift the clipmap grid down so that probes align with other clipmaps
		const FVector ClipmapMin = Clipmap.Center - Clipmap.Extent - 0.5f * Clipmap.CellSize;

		Clipmap.ProbeCoordToWorldCenterBias = ClipmapMin + 0.5f * Clipmap.CellSize;
		Clipmap.ProbeCoordToWorldCenterScale = Clipmap.CellSize;

		Clipmap.WorldPositionToProbeCoordScale = 1.0f / CellSize;
		Clipmap.WorldPositionToProbeCoordBias = -ClipmapMin / CellSize;
		
		Clipmap.ProbeTMin = RadianceCacheInputs.CalculateIrradiance ? 0.0f : FVector(CellSize, CellSize, CellSize).Size();
	}

	return bResetState;
}

namespace LumenRadianceCache
{

bool ShouldImportanceSampleBRDF(const FUpdateInputs& Inputs)
{
	return Inputs.ScreenProbeParameters && Inputs.BRDFProbabilityDensityFunctionSH && GLumenRadianceCacheForceUniformTraceTileLevel < 0;
}

float GetSupersampleDistanceFromCamera(const FUpdateInputs& Inputs)
{
	return GLumenRadianceCacheSupersampleDistanceFromCamera;
}

class FRadianceCacheSetup
{
public:
	TArray<FRadianceCacheClipmap> LastFrameClipmaps;
	FRDGTextureRef DepthProbeAtlasTexture;
	FRDGTextureRef FinalIrradianceAtlas;
	FRDGTextureRef ProbeOcclusionAtlas;
	FRDGTextureRef FinalRadianceAtlas;
	FRDGTextureRef RadianceProbeAtlasTextureSource;
	bool bPersistentCache;
};

void UpdateRadianceCaches(
	FRDGBuilder& GraphBuilder, 
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const TInlineArray<FUpdateInputs>& InputArray,
	TInlineArray<FUpdateOutputs>& OutputArray,
	const FScene* Scene,
	const FViewFamilyInfo& ViewFamily,
	bool bPropagateGlobalLightingChange,
	ERDGPassFlags ComputePassFlags)
{
	if (GRadianceCacheUpdate != 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "UpdateRadianceCaches");
		check(InputArray.Num() == OutputArray.Num());

		TInlineArray<FRadianceCacheSetup> SetupOutputArray(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			FRadianceCacheSetup& SetupOutputs = SetupOutputArray[RadianceCacheIndex];

			SetupOutputs.LastFrameClipmaps = RadianceCacheState.Clipmaps;
			bool bResizedHistoryState = UpdateRadianceCacheState(GraphBuilder, View, RadianceCacheInputs, RadianceCacheState);

			const FIntPoint RadianceProbeAtlasTextureSize(RadianceCacheInputs.ProbeAtlasResolutionInProbes * RadianceCacheInputs.RadianceProbeResolution);

			if (RadianceCacheState.DepthProbeAtlasTexture.IsValid()
				&& RadianceCacheState.DepthProbeAtlasTexture->GetDesc().Extent == RadianceProbeAtlasTextureSize)
			{
				SetupOutputs.DepthProbeAtlasTexture = GraphBuilder.RegisterExternalTexture(RadianceCacheState.DepthProbeAtlasTexture);
			}
			else
			{
				FRDGTextureDesc ProbeAtlasDesc = FRDGTextureDesc::Create2D(
					RadianceProbeAtlasTextureSize,
					PF_R16F,
					FClearValueBinding::None,
					TexCreate_ShaderResource | TexCreate_UAV);

				SetupOutputs.DepthProbeAtlasTexture = GraphBuilder.CreateTexture(ProbeAtlasDesc, TEXT("Lumen.RadianceCache.DepthProbeAtlasTexture"));
				bResizedHistoryState = true;
			}

			SetupOutputs.FinalIrradianceAtlas = nullptr;
			SetupOutputs.ProbeOcclusionAtlas = nullptr;
			SetupOutputs.FinalRadianceAtlas = nullptr;

			if (RadianceCacheInputs.CalculateIrradiance)
			{
				const FIntPoint FinalIrradianceAtlasSize(RadianceCacheInputs.ProbeAtlasResolutionInProbes * (RadianceCacheInputs.IrradianceProbeResolution + 2 * (1 << RadianceCacheInputs.FinalRadianceAtlasMaxMip)));

				if (RadianceCacheState.FinalIrradianceAtlas.IsValid()
					&& RadianceCacheState.FinalIrradianceAtlas->GetDesc().Extent == FinalIrradianceAtlasSize
					&& RadianceCacheState.FinalIrradianceAtlas->GetDesc().NumMips == RadianceCacheInputs.FinalRadianceAtlasMaxMip + 1)
				{
					SetupOutputs.FinalIrradianceAtlas = GraphBuilder.RegisterExternalTexture(RadianceCacheState.FinalIrradianceAtlas);
				}
				else
				{
					FRDGTextureDesc FinalRadianceAtlasDesc = FRDGTextureDesc::Create2D(
						FinalIrradianceAtlasSize,
						PF_FloatRGB,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_UAV,
						RadianceCacheInputs.FinalRadianceAtlasMaxMip + 1);

					SetupOutputs.FinalIrradianceAtlas = GraphBuilder.CreateTexture(FinalRadianceAtlasDesc, TEXT("Lumen.RadianceCache.FinalIrradianceAtlas"));
					bResizedHistoryState = true;
				}

				if (GRadianceCacheForceFullUpdate)
				{
					AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SetupOutputs.FinalIrradianceAtlas)), FLinearColor::Black, ComputePassFlags);
				}

				const FIntPoint ProbeOcclusionAtlasSize(RadianceCacheInputs.ProbeAtlasResolutionInProbes * (RadianceCacheInputs.OcclusionProbeResolution + 2 * (1 << RadianceCacheInputs.FinalRadianceAtlasMaxMip)));

				if (RadianceCacheState.ProbeOcclusionAtlas.IsValid()
					&& RadianceCacheState.ProbeOcclusionAtlas->GetDesc().Extent == ProbeOcclusionAtlasSize
					&& RadianceCacheState.ProbeOcclusionAtlas->GetDesc().NumMips == RadianceCacheInputs.FinalRadianceAtlasMaxMip + 1)
				{
					SetupOutputs.ProbeOcclusionAtlas = GraphBuilder.RegisterExternalTexture(RadianceCacheState.ProbeOcclusionAtlas);
				}
				else
				{
					FRDGTextureDesc ProbeOcclusionAtlasDesc = FRDGTextureDesc::Create2D(
						ProbeOcclusionAtlasSize,
						PF_G16R16F,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_UAV,
						RadianceCacheInputs.FinalRadianceAtlasMaxMip + 1);

					SetupOutputs.ProbeOcclusionAtlas = GraphBuilder.CreateTexture(ProbeOcclusionAtlasDesc, TEXT("Lumen.RadianceCache.ProbeOcclusionAtlas"));
					bResizedHistoryState = true;
				}
			}
			else
			{
				const FIntPoint FinalRadianceAtlasSize(RadianceCacheInputs.ProbeAtlasResolutionInProbes * RadianceCacheInputs.FinalProbeResolution);

				if (RadianceCacheState.FinalRadianceAtlas.IsValid()
					&& RadianceCacheState.FinalRadianceAtlas->GetDesc().Extent == FinalRadianceAtlasSize
					&& RadianceCacheState.FinalRadianceAtlas->GetDesc().NumMips == RadianceCacheInputs.FinalRadianceAtlasMaxMip + 1)
				{
					SetupOutputs.FinalRadianceAtlas = GraphBuilder.RegisterExternalTexture(RadianceCacheState.FinalRadianceAtlas);
				}
				else
				{
					FRDGTextureDesc FinalRadianceAtlasDesc = FRDGTextureDesc::Create2D(
						FinalRadianceAtlasSize,
						PF_FloatRGB,
						FClearValueBinding::None,
						TexCreate_ShaderResource | TexCreate_UAV,
						RadianceCacheInputs.FinalRadianceAtlasMaxMip + 1);

					SetupOutputs.FinalRadianceAtlas = GraphBuilder.CreateTexture(FinalRadianceAtlasDesc, TEXT("Lumen.RadianceCache.FinalRadianceAtlas"));
					bResizedHistoryState = true;
				}

				if (GRadianceCacheForceFullUpdate)
				{
					AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SetupOutputs.FinalRadianceAtlas)), FLinearColor::Black, ComputePassFlags);
				}
			}

			SetupOutputs.RadianceProbeAtlasTextureSource = nullptr;

			FRDGTextureDesc ProbeAtlasDesc = FRDGTextureDesc::Create2D(
				RadianceProbeAtlasTextureSize,
				PF_FloatRGB,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV);

			if (RadianceCacheState.RadianceProbeAtlasTexture.IsValid()
				&& RadianceCacheState.RadianceProbeAtlasTexture->GetDesc().Extent == RadianceProbeAtlasTextureSize)
			{
				SetupOutputs.RadianceProbeAtlasTextureSource = GraphBuilder.RegisterExternalTexture(RadianceCacheState.RadianceProbeAtlasTexture);
			}
			else
			{
				SetupOutputs.RadianceProbeAtlasTextureSource = GraphBuilder.CreateTexture(ProbeAtlasDesc, TEXT("Lumen.RadianceCache.RadianceProbeAtlasTextureSource"));
			}

			GetInterpolationParametersNoResources(GraphBuilder, RadianceCacheState, RadianceCacheInputs, RadianceCacheParameters);

			const FIntVector RadianceProbeIndirectionTextureSize = FIntVector(
				RadianceCacheInputs.RadianceProbeClipmapResolution * RadianceCacheInputs.NumRadianceProbeClipmaps,
				RadianceCacheInputs.RadianceProbeClipmapResolution,
				RadianceCacheInputs.RadianceProbeClipmapResolution);

			FRDGTextureDesc ProbeIndirectionDesc = FRDGTextureDesc::Create3D(
				RadianceProbeIndirectionTextureSize,
				PF_R32_UINT,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV | TexCreate_3DTiling);

			RadianceCacheParameters.RadianceProbeIndirectionTexture = GraphBuilder.CreateTexture(FRDGTextureDesc(ProbeIndirectionDesc), TEXT("Lumen.RadianceCache.RadianceProbeIndirectionTexture"));

			SetupOutputs.bPersistentCache = !GRadianceCacheForceFullUpdate
				&& View.ViewState
				&& IsValidRef(RadianceCacheState.RadianceProbeIndirectionTexture)
				&& RadianceCacheState.RadianceProbeIndirectionTexture->GetDesc().GetSize() == RadianceProbeIndirectionTextureSize
				&& !bResizedHistoryState
				&& !bPropagateGlobalLightingChange;
		}

		const bool bLumenSceneLightingAsync = LumenSceneLighting::UseAsyncCompute(ViewFamily);

		// Clear each clipmap indirection entry to invalid probe index
		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];

			FClearProbeIndirectionCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearProbeIndirectionCS::FParameters>();
			PassParameters->RWRadianceProbeIndirectionTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Outputs.RadianceCacheParameters.RadianceProbeIndirectionTexture));

			auto ComputeShader = Inputs.View.ShaderMap->GetShader<FClearProbeIndirectionCS>(0);

			const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(Outputs.RadianceCacheParameters.RadianceProbeIndirectionTexture->Desc.GetSize(), FClearProbeIndirectionCS::GetGroupSize());

			// Do clear on graphics if there is any graphics mark pass and LumenSeneLighting is async so the mark pass is not blocked.
			// If LumenSceneLighting isn't async, it will block graphics mark passes anyway. May as well finish the clear early on the compute pipe.
			// TODO: Is it possible to move graphics mark passes and their clears before LumenSceneLighting without heavy code change?
			const ERDGPassFlags ClearPassFlags = Inputs.GraphicsMarkUsedRadianceCacheProbes.IsBound() && bLumenSceneLightingAsync ? ERDGPassFlags::Compute : ComputePassFlags;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClearProbeIndirectionCS"),
				ClearPassFlags,
				ComputeShader,
				PassParameters,
				GroupSize);
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];

			FRDGTextureUAVRef RadianceProbeIndirectionTextureMarkUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Outputs.RadianceCacheParameters.RadianceProbeIndirectionTexture), ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRadianceCacheMarkParameters RadianceCacheMarkParameters = GetMarkParameters(RadianceProbeIndirectionTextureMarkUAV, Outputs.RadianceCacheState, Inputs.RadianceCacheInputs);

			// Mark indirection entries around positions that will be sampled by dependent features as used
			Inputs.GraphicsMarkUsedRadianceCacheProbes.Broadcast(GraphBuilder, Inputs.View, RadianceCacheMarkParameters);
			Inputs.ComputeMarkUsedRadianceCacheProbes.Broadcast(GraphBuilder, Inputs.View, RadianceCacheMarkParameters);
		}

		TInlineArray<FRDGBufferRef> ProbeFreeListAllocator(InputArray.Num());
		TInlineArray<FRDGBufferRef> ProbeFreeList(InputArray.Num());
		TInlineArray<FRDGBufferRef> ProbeLastUsedFrame(InputArray.Num());
		TInlineArray<FRDGBufferRef> ProbeLastTracedFrame(InputArray.Num());
		TInlineArray<FRDGBufferRef> ProbeWorldOffset(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;

			if (IsValidRef(RadianceCacheState.ProbeFreeList) && RadianceCacheState.ProbeFreeList->Desc.NumElements == MaxNumProbes)
			{
				ProbeFreeListAllocator[RadianceCacheIndex] = GraphBuilder.RegisterExternalBuffer(RadianceCacheState.ProbeFreeListAllocator);
				ProbeFreeList[RadianceCacheIndex] = GraphBuilder.RegisterExternalBuffer(RadianceCacheState.ProbeFreeList);
				ProbeLastUsedFrame[RadianceCacheIndex] = GraphBuilder.RegisterExternalBuffer(RadianceCacheState.ProbeLastUsedFrame);
				ProbeLastTracedFrame[RadianceCacheIndex] = GraphBuilder.RegisterExternalBuffer(RadianceCacheState.ProbeLastTracedFrame);
				ProbeWorldOffset[RadianceCacheIndex] = GraphBuilder.RegisterExternalBuffer(RadianceCacheState.ProbeWorldOffset);
			}
			else
			{
				ProbeFreeListAllocator[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 1), TEXT("Lumen.RadianceCache.ProbeFreeListAllocator"));
				ProbeFreeList[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), MaxNumProbes), TEXT("Lumen.RadianceCache.ProbeFreeList"));
				ProbeLastUsedFrame[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), MaxNumProbes), TEXT("Lumen.RadianceCache.ProbeLastUsedFrame"));
				ProbeLastTracedFrame[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxNumProbes), TEXT("Lumen.RadianceCache.ProbeLastTracedFrame"));
				ProbeWorldOffset[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), MaxNumProbes), TEXT("Lumen.RadianceCache.ProbeWorldOffset"));
			}

			FRDGBufferUAVRef ProbeFreeListAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeFreeListAllocator[RadianceCacheIndex], PF_R32_SINT));
			FRDGBufferUAVRef ProbeFreeListUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeFreeList[RadianceCacheIndex], PF_R32_UINT));
			FRDGBufferUAVRef ProbeLastUsedFrameUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeLastUsedFrame[RadianceCacheIndex], PF_R32_UINT));
			FRDGBufferUAVRef ProbeWorldOffsetUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeWorldOffset[RadianceCacheIndex], PF_A32B32G32R32F));

			if (!Setup.bPersistentCache || !IsValidRef(RadianceCacheState.ProbeFreeListAllocator))
			{
				FClearProbeFreeList::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearProbeFreeList::FParameters>();
				PassParameters->RWProbeFreeListAllocator = ProbeFreeListAllocatorUAV;
				PassParameters->RWProbeFreeList = ProbeFreeListUAV;
				PassParameters->RWProbeLastUsedFrame = ProbeLastUsedFrameUAV;
				PassParameters->RWProbeWorldOffset = ProbeWorldOffsetUAV;
				PassParameters->MaxNumProbes = MaxNumProbes;

				auto ComputeShader = View.ShaderMap->GetShader<FClearProbeFreeList>();

				const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(MaxNumProbes, FClearProbeFreeList::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ClearProbeFreeList"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					GroupSize);
			}
		}

		TInlineArray<FRDGBufferRef> ProbeAllocator(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			if (IsValidRef(RadianceCacheState.ProbeAllocator))
			{
				ProbeAllocator[RadianceCacheIndex] = GraphBuilder.RegisterExternalBuffer(RadianceCacheState.ProbeAllocator, TEXT("Lumen.RadianceCache.ProbeAllocator"));
			}
			else
			{
				ProbeAllocator[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.RadianceCache.ProbeAllocator"));
			}

			FRDGBufferUAVRef ProbeAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeAllocator[RadianceCacheIndex], PF_R32_UINT));

			if (!Setup.bPersistentCache || !IsValidRef(RadianceCacheState.ProbeAllocator))
			{
				AddClearUAVPass(GraphBuilder, ProbeAllocatorUAV, 0);
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;
		
			// Propagate probes from last frame to the new frame's indirection
			if (Setup.bPersistentCache)
			{
				FRDGTextureUAVRef RadianceProbeIndirectionTextureUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RadianceCacheParameters.RadianceProbeIndirectionTexture));
				FRDGTextureRef LastFrameRadianceProbeIndirectionTexture = GraphBuilder.RegisterExternalTexture(RadianceCacheState.RadianceProbeIndirectionTexture);

				{
					FRDGBufferUAVRef ProbeFreeListAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeFreeListAllocator[RadianceCacheIndex], PF_R32_SINT));
					FRDGBufferUAVRef ProbeFreeListUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeFreeList[RadianceCacheIndex], PF_R32_UINT));
					FRDGBufferUAVRef ProbeLastUsedFrameUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeLastUsedFrame[RadianceCacheIndex], PF_R32_UINT));

					FUpdateCacheForUsedProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpdateCacheForUsedProbesCS::FParameters>();
					PassParameters->View = View.ViewUniformBuffer;
					PassParameters->RWRadianceProbeIndirectionTexture = RadianceProbeIndirectionTextureUAV;
					PassParameters->ProbeAllocator = GraphBuilder.CreateSRV(ProbeAllocator[RadianceCacheIndex], PF_R32_UINT);
					PassParameters->RWProbeFreeListAllocator = ProbeFreeListAllocatorUAV;
					PassParameters->RWProbeFreeList = ProbeFreeListUAV;
					PassParameters->RWProbeLastUsedFrame = ProbeLastUsedFrameUAV;
					PassParameters->LastFrameRadianceProbeIndirectionTexture = LastFrameRadianceProbeIndirectionTexture;
					PassParameters->RadianceCacheParameters = RadianceCacheParameters;
					PassParameters->FrameNumber = View.ViewState->GetFrameIndex();
					PassParameters->NumFramesToKeepCachedProbes = FMath::Max(CVarRadianceCacheNumFramesToKeepCachedProbes.GetValueOnRenderThread(), 0);
					PassParameters->MaxNumProbes = MaxNumProbes;

					for (int32 ClipmapIndex = 0; ClipmapIndex < Setup.LastFrameClipmaps.Num(); ++ClipmapIndex)
					{
						const FRadianceCacheClipmap& Clipmap = Setup.LastFrameClipmaps[ClipmapIndex];

						SetRadianceProbeCoordToWorldPosition(PassParameters->PackedLastFrameRadianceProbeCoordToWorldPosition[ClipmapIndex], (FVector3f)Clipmap.ProbeCoordToWorldCenterBias, Clipmap.ProbeCoordToWorldCenterScale);
					}

					auto ComputeShader = View.ShaderMap->GetShader<FUpdateCacheForUsedProbesCS>(0);

					const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(RadianceCacheParameters.RadianceProbeIndirectionTexture->Desc.GetSize(), FUpdateCacheForUsedProbesCS::GetGroupSize());

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("UpdateCacheForUsedProbes"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						GroupSize);
				}
			}
		}

		TInlineArray<FRDGBufferRef> ProbeTraceData(InputArray.Num());
		TInlineArray<FRDGBufferRef> ProbeTraceAllocator(InputArray.Num());
		TInlineArray<FRDGBufferRef> PriorityHistogram(InputArray.Num());
		TInlineArray<FRDGBufferRef> MaxUpdateBucket(InputArray.Num());
		TInlineArray<FRDGBufferRef> MaxTracesFromMaxUpdateBucket(InputArray.Num());
		TInlineArray<FRDGBufferRef> ProbesToUpdateTraceCost(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;
			ProbeTraceData[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), MaxNumProbes), TEXT("Lumen.RadianceCache.ProbeTraceData"));
			ProbeTraceAllocator[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.RadianceCache.ProbeTraceAllocator"));
			FRDGBufferUAVRef ProbeTraceAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeTraceAllocator[RadianceCacheIndex], PF_R32_UINT));

			PriorityHistogram[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), PRIORITY_HISTOGRAM_SIZE), TEXT("Lumen.RadianceCache.PriorityHistogram"));
			MaxUpdateBucket[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.RadianceCache.MaxUpdateBucket"));
			MaxTracesFromMaxUpdateBucket[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.RadianceCache.MaxTracesFromMaxUpdateBucket"));
			ProbesToUpdateTraceCost[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), PROBES_TO_UPDATE_TRACE_COST_STRIDE), TEXT("Lumen.RadianceCache.ProbesToUpdateTraceCost"));

			// Batch clear all resources required for the subsequent radiance cache probe update pass
			{
				FClearRadianceCacheUpdateResourcesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearRadianceCacheUpdateResourcesCS::FParameters>();
				PassParameters->RWProbeTraceAllocator = ProbeTraceAllocatorUAV;
				PassParameters->RWPriorityHistogram = GraphBuilder.CreateUAV(PriorityHistogram[RadianceCacheIndex]);
				PassParameters->RWMaxUpdateBucket = GraphBuilder.CreateUAV(MaxUpdateBucket[RadianceCacheIndex]);
				PassParameters->RWMaxTracesFromMaxUpdateBucket = GraphBuilder.CreateUAV(MaxTracesFromMaxUpdateBucket[RadianceCacheIndex]);
				PassParameters->RWProbesToUpdateTraceCost = GraphBuilder.CreateUAV(ProbesToUpdateTraceCost[RadianceCacheIndex]);

				auto ComputeShader = View.ShaderMap->GetShader<FClearRadianceCacheUpdateResourcesCS>();

				const FIntVector GroupSize(FMath::DivideAndRoundUp<int32>(PRIORITY_HISTOGRAM_SIZE, FClearRadianceCacheUpdateResourcesCS::GetGroupSize()), 1, 1);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ClearRadianceCacheUpdateResources"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					GroupSize);
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			// Allocated used probes
			{	
				FRDGBufferUAVRef ProbeFreeListAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeFreeListAllocator[RadianceCacheIndex], PF_R32_SINT));
				FRDGBufferUAVRef ProbeLastUsedFrameUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeLastUsedFrame[RadianceCacheIndex], PF_R32_UINT));
				FRDGTextureUAVRef RadianceProbeIndirectionTextureUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RadianceCacheParameters.RadianceProbeIndirectionTexture));
				FRDGBufferUAVRef ProbeAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeAllocator[RadianceCacheIndex], PF_R32_UINT));
				const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;

				FAllocateUsedProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FAllocateUsedProbesCS::FParameters>();
				PassParameters->RWRadianceProbeIndirectionTexture = RadianceProbeIndirectionTextureUAV;
				PassParameters->RWPriorityHistogram = GraphBuilder.CreateUAV(PriorityHistogram[RadianceCacheIndex]);
				PassParameters->RWProbeLastTracedFrame = GraphBuilder.CreateUAV(ProbeLastTracedFrame[RadianceCacheIndex]);
				PassParameters->RWProbeLastUsedFrame = ProbeLastUsedFrameUAV;
				PassParameters->RWProbeAllocator = ProbeAllocatorUAV;
				PassParameters->RWProbeFreeListAllocator = Setup.bPersistentCache ? ProbeFreeListAllocatorUAV : nullptr;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->ProbeFreeList = Setup.bPersistentCache ? GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeFreeList[RadianceCacheIndex], PF_R32_UINT)) : nullptr;
				PassParameters->FirstClipmapWorldExtentRcp = 1.0f / FMath::Max(RadianceCacheInputs.ClipmapWorldExtent, 1.0f);
				PassParameters->SupersampleDistanceFromCameraSq = GetSupersampleDistanceFromCamera(Inputs) * GetSupersampleDistanceFromCamera(Inputs);
				PassParameters->DownsampleDistanceFromCameraSq = GLumenRadianceCacheDownsampleDistanceFromCamera * GLumenRadianceCacheDownsampleDistanceFromCamera;
				PassParameters->FrameNumber = View.ViewState ? View.ViewState->GetFrameIndex() : View.Family->FrameNumber;
				PassParameters->MaxNumProbes = MaxNumProbes;
				PassParameters->RadianceCacheParameters = RadianceCacheParameters;

				FAllocateUsedProbesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FAllocateUsedProbesCS::FPersistentCache>(Setup.bPersistentCache);
				auto ComputeShader = View.ShaderMap->GetShader<FAllocateUsedProbesCS>(PermutationVector);

				const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(RadianceCacheParameters.RadianceProbeIndirectionTexture->Desc.GetSize(), FAllocateUsedProbesCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("AllocateUsedProbes"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					GroupSize);
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			// Selected max priority bucket
			{
				FSelectMaxPriorityBucketCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSelectMaxPriorityBucketCS::FParameters>();
				PassParameters->RWMaxUpdateBucket = GraphBuilder.CreateUAV(MaxUpdateBucket[RadianceCacheIndex]);
				PassParameters->RWMaxTracesFromMaxUpdateBucket = GraphBuilder.CreateUAV(MaxTracesFromMaxUpdateBucket[RadianceCacheIndex]);
				PassParameters->PriorityHistogram = GraphBuilder.CreateSRV(PriorityHistogram[RadianceCacheIndex]);
				PassParameters->ProbeTraceAllocator = GraphBuilder.CreateSRV(ProbeTraceAllocator[RadianceCacheIndex], PF_R32_UINT);
				const float TraceBudgetScale = bPropagateGlobalLightingChange ? 4.0f : 1.0f;
				PassParameters->NumProbesToTraceBudget = GRadianceCacheForceFullUpdate ? UINT32_MAX : RadianceCacheInputs.NumProbesToTraceBudget * TraceBudgetScale;

				auto ComputeShader = View.ShaderMap->GetShader<FSelectMaxPriorityBucketCS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("SelectMaxPriorityBucket"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					FIntVector(1, 1, 1));
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			// Trace probes up to selected priority bucket
			{
				FRDGBufferUAVRef ProbeFreeListAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeFreeListAllocator[RadianceCacheIndex], PF_R32_SINT));
				FRDGTextureUAVRef RadianceProbeIndirectionTextureUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RadianceCacheParameters.RadianceProbeIndirectionTexture));
				FRDGBufferUAVRef ProbeTraceAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeTraceAllocator[RadianceCacheIndex], PF_R32_UINT));
				const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;

				FAllocateProbeTracesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FAllocateProbeTracesCS::FParameters>();
				PassParameters->RWRadianceProbeIndirectionTexture = RadianceProbeIndirectionTextureUAV;
				PassParameters->RWProbesToUpdateTraceCost = GraphBuilder.CreateUAV(ProbesToUpdateTraceCost[RadianceCacheIndex]);
				PassParameters->RWProbeLastTracedFrame = GraphBuilder.CreateUAV(ProbeLastTracedFrame[RadianceCacheIndex]);
				PassParameters->RWProbeTraceAllocator = ProbeTraceAllocatorUAV;
				PassParameters->RWProbeTraceData = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
				PassParameters->RWProbeFreeListAllocator = Setup.bPersistentCache ? ProbeFreeListAllocatorUAV : nullptr;
				PassParameters->MaxUpdateBucket = GraphBuilder.CreateSRV(MaxUpdateBucket[RadianceCacheIndex], PF_R32_UINT);
				PassParameters->MaxTracesFromMaxUpdateBucket = GraphBuilder.CreateSRV(MaxTracesFromMaxUpdateBucket[RadianceCacheIndex]);
				PassParameters->ProbeLastUsedFrame = GraphBuilder.CreateSRV(ProbeLastUsedFrame[RadianceCacheIndex], PF_R32_UINT);
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->ProbeFreeList = Setup.bPersistentCache ? GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeFreeList[RadianceCacheIndex], PF_R32_UINT)) : nullptr;
				PassParameters->SupersampleDistanceFromCameraSq = GetSupersampleDistanceFromCamera(Inputs) * GetSupersampleDistanceFromCamera(Inputs);
				PassParameters->DownsampleDistanceFromCameraSq = GLumenRadianceCacheDownsampleDistanceFromCamera * GLumenRadianceCacheDownsampleDistanceFromCamera;
				PassParameters->FirstClipmapWorldExtentRcp = 1.0f / FMath::Max(RadianceCacheInputs.ClipmapWorldExtent, 1.0f);
				PassParameters->FrameNumber = View.ViewState ? View.ViewState->GetFrameIndex() : View.Family->FrameNumber;
				PassParameters->MaxNumProbes = MaxNumProbes;
				PassParameters->RadianceCacheParameters = RadianceCacheParameters;

				auto ComputeShader = View.ShaderMap->GetShader<FAllocateProbeTracesCS>();

				const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(RadianceCacheParameters.RadianceProbeIndirectionTexture->Desc.GetSize(), FAllocateProbeTracesCS::GetGroupSize());

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("AllocateProbeTraces"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					GroupSize);
			}
		}

		TInlineArray<FRDGBufferRef> ClearProbePDFsIndirectArgs(InputArray.Num());
		TInlineArray<FRDGBufferRef> GenerateProbeTraceTilesIndirectArgs(InputArray.Num());
		TInlineArray<FRDGBufferRef> ProbeTraceTileAllocator(InputArray.Num());
		TInlineArray<FRDGBufferRef> FilterProbesIndirectArgs(InputArray.Num());
		TInlineArray<FRDGBufferRef> PrepareProbeOcclusionIndirectArgs(InputArray.Num());
		TInlineArray<FRDGBufferRef> FixupProbeBordersIndirectArgs(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			ClearProbePDFsIndirectArgs[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(2), TEXT("Lumen.RadianceCache.ClearProbePDFsIndirectArgs"));
			GenerateProbeTraceTilesIndirectArgs[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(3), TEXT("Lumen.RadianceCache.GenerateProbeTraceTilesIndirectArgs"));
			ProbeTraceTileAllocator[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.RadianceCache.ProbeTraceTileAllocator"));
			FilterProbesIndirectArgs[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(5), TEXT("Lumen.RadianceCache.FilterProbesIndirectArgs"));
			PrepareProbeOcclusionIndirectArgs[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(7), TEXT("Lumen.RadianceCache.PrepareProbeOcclusionIndirectArgs"));
			FixupProbeBordersIndirectArgs[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(8), TEXT("Lumen.RadianceCache.FixupProbeBordersIndirectArgs"));

			{
				FRDGBufferUAVRef ProbeFreeListAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeFreeListAllocator[RadianceCacheIndex], PF_R32_SINT));
				FRDGBufferUAVRef ProbeAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeAllocator[RadianceCacheIndex], PF_R32_UINT));
				const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;

				FSetupProbeIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSetupProbeIndirectArgsCS::FParameters>();
				PassParameters->RWProbeAllocator = ProbeAllocatorUAV;
				PassParameters->RWProbeFreeListAllocator = ProbeFreeListAllocatorUAV;
				PassParameters->RWClearProbePDFsIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ClearProbePDFsIndirectArgs[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWGenerateProbeTraceTilesIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(GenerateProbeTraceTilesIndirectArgs[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWProbeTraceTileAllocator = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeTraceTileAllocator[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWFilterProbesIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(FilterProbesIndirectArgs[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWPrepareProbeOcclusionIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PrepareProbeOcclusionIndirectArgs[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWFixupProbeBordersIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(FixupProbeBordersIndirectArgs[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->ProbeTraceAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceAllocator[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->TraceFromProbesGroupSizeXY = FRadianceCacheTraceFromProbesCS::GetGroupSize();
				PassParameters->FilterProbesGroupSizeXY = FFilterProbeRadianceWithGatherCS::GetGroupSize();
				PassParameters->ClearProbePDFGroupSize = FClearProbePDFs::GetGroupSize();
				PassParameters->MaxNumProbes = MaxNumProbes;
				PassParameters->RadianceCacheParameters = RadianceCacheParameters;
				auto ComputeShader = View.ShaderMap->GetShader<FSetupProbeIndirectArgsCS>(0);

				const FIntVector GroupSize = FIntVector(1);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("SetupProbeIndirectArgsCS"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					GroupSize);
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			if (RadianceCacheInputs.CalculateIrradiance)
			{
				FRDGBufferUAVRef ProbeWorldOffsetUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeWorldOffset[RadianceCacheIndex], PF_A32B32G32R32F));

				FComputeProbeWorldOffsetsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeProbeWorldOffsetsCS::FParameters>();
				PassParameters->RWProbeWorldOffset = ProbeWorldOffsetUAV;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
				PassParameters->RadianceCacheParameters = RadianceCacheParameters;
				PassParameters->IndirectArgs = GenerateProbeTraceTilesIndirectArgs[RadianceCacheIndex];

				auto ComputeShader = View.ShaderMap->GetShader<FComputeProbeWorldOffsetsCS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ComputeProbeWorldOffsets"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					PassParameters->IndirectArgs,
					0);
			}

			RadianceCacheParameters.ProbeWorldOffset = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeWorldOffset[RadianceCacheIndex], PF_A32B32G32R32F));

		}

		TInlineArray<FRDGBufferRef> RadianceProbeSH_PDF(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			const bool bGenerateBRDF_PDF = ShouldImportanceSampleBRDF(Inputs);

			if (bGenerateBRDF_PDF)
			{
				const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;
				RadianceProbeSH_PDF[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(int32), MaxNumProbes * (9 + 1)), TEXT("Lumen.RadianceCache.RadianceProbeSH_PDF"));

				{
					FClearProbePDFs::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearProbePDFs::FParameters>();
					PassParameters->RWRadianceProbeSH_PDF = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(RadianceProbeSH_PDF[RadianceCacheIndex], PF_R32_SINT));
					PassParameters->ClearProbePDFsIndirectArgs = ClearProbePDFsIndirectArgs[RadianceCacheIndex];
					PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));

					auto ComputeShader = View.ShaderMap->GetShader<FClearProbePDFs>(0);

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("ClearProbePDFs"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						PassParameters->ClearProbePDFsIndirectArgs,
						0);
				}
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			if (RadianceProbeSH_PDF[RadianceCacheIndex])
			{
				FScatterScreenProbeBRDFToRadianceProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScatterScreenProbeBRDFToRadianceProbesCS::FParameters>();
				PassParameters->RWRadianceProbeSH_PDF = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(RadianceProbeSH_PDF[RadianceCacheIndex], PF_R32_SINT));
				PassParameters->BRDFProbabilityDensityFunctionSH = Inputs.BRDFProbabilityDensityFunctionSH;
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->ScreenProbeParameters = *Inputs.ScreenProbeParameters;
				PassParameters->RadianceCacheParameters = RadianceCacheParameters;

				auto ComputeShader = View.ShaderMap->GetShader<FScatterScreenProbeBRDFToRadianceProbesCS>(0);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ScatterScreenProbeBRDFToRadianceProbes"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					Inputs.ScreenProbeParameters->ProbeIndirectArgs,
					(uint32)EScreenProbeIndirectArgs::GroupPerProbe * sizeof(FRHIDispatchIndirectParameters));
			}
		}

		TInlineArray<FRDGBufferRef> ProbeTraceTileData(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;
			const int32 MaxProbeTraceTileResolution = RadianceCacheInputs.RadianceProbeResolution / FRadianceCacheTraceFromProbesCS::GetGroupSize() * 2;
			ProbeTraceTileData[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FIntPoint), MaxNumProbes * MaxProbeTraceTileResolution * MaxProbeTraceTileResolution), TEXT("Lumen.RadianceCache.ProbeTraceTileData"));

			const int32 DebugProbeBRDFOctahedronResolution = 8;
			FRDGTextureDesc DebugBRDFProbabilityDensityFunctionDesc = FRDGTextureDesc::Create2D(
				FIntPoint(RadianceCacheInputs.ProbeAtlasResolutionInProbes * DebugProbeBRDFOctahedronResolution),
				PF_R16F,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV);

			FRDGTextureRef DebugBRDFProbabilityDensityFunction = GraphBuilder.CreateTexture(DebugBRDFProbabilityDensityFunctionDesc, TEXT("Lumen.RadianceCache.DebugBRDFProbabilityDensityFunction"));

			{
				FGenerateProbeTraceTilesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateProbeTraceTilesCS::FParameters>();
				PassParameters->RWProbeTraceTileAllocator = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeTraceTileAllocator[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWProbeTraceTileData = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ProbeTraceTileData[RadianceCacheIndex], PF_R32G32_UINT));
				PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
				PassParameters->RadianceProbeSH_PDF = RadianceProbeSH_PDF[RadianceCacheIndex] ? GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RadianceProbeSH_PDF[RadianceCacheIndex], PF_R32_SINT)) : nullptr;
				PassParameters->ProbesToUpdateTraceCost = GraphBuilder.CreateSRV(ProbesToUpdateTraceCost[RadianceCacheIndex]);
				PassParameters->SupersampleTileBRDFThreshold = GLumenRadianceCacheSupersampleTileBRDFThreshold;
				PassParameters->SupersampleDistanceFromCameraSq = GLumenRadianceCacheSupersampleDistanceFromCamera * GLumenRadianceCacheSupersampleDistanceFromCamera;
				PassParameters->DownsampleDistanceFromCameraSq = GLumenRadianceCacheDownsampleDistanceFromCamera * GLumenRadianceCacheDownsampleDistanceFromCamera;
				PassParameters->ForcedUniformLevel = GLumenRadianceCacheForceUniformTraceTileLevel >= 0 ? FMath::Clamp<int32>(GLumenRadianceCacheForceUniformTraceTileLevel, 0, 2) : 1;

				PassParameters->RWDebugBRDFProbabilityDensityFunction = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DebugBRDFProbabilityDensityFunction));
				PassParameters->DebugProbeBRDFOctahedronResolution = DebugProbeBRDFOctahedronResolution;

				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->RadianceCacheParameters = RadianceCacheParameters;
				PassParameters->GenerateProbeTraceTilesIndirectArgs = GenerateProbeTraceTilesIndirectArgs[RadianceCacheIndex];

				FGenerateProbeTraceTilesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FGenerateProbeTraceTilesCS::FUniformTraces>(RadianceProbeSH_PDF[RadianceCacheIndex] == nullptr);
				auto ComputeShader = View.ShaderMap->GetShader<FGenerateProbeTraceTilesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("GenerateProbeTraceTiles"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					PassParameters->GenerateProbeTraceTilesIndirectArgs,
					0);
			}
		}

		TInlineArray<FRDGBufferRef> TraceProbesIndirectArgs(InputArray.Num());
		TInlineArray<FRDGBufferRef> SortProbeTraceTilesIndirectArgs(InputArray.Num());
		TInlineArray<FRDGBufferRef> RadianceCacheHardwareRayTracingIndirectArgs(InputArray.Num());
		TInlineArray<FRDGBufferRef> HardwareRayTracingRayAllocatorBuffer(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			TraceProbesIndirectArgs[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(4), TEXT("Lumen.RadianceCache.TraceProbesIndirectArgs"));
			SortProbeTraceTilesIndirectArgs[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(5), TEXT("Lumen.RadianceCache.SortProbeTraceTilesIndirectArgs"));
			RadianceCacheHardwareRayTracingIndirectArgs[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(6), TEXT("Lumen.RadianceCache.RadianceCacheHardwareRayTracingIndirectArgs"));
			HardwareRayTracingRayAllocatorBuffer[RadianceCacheIndex] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.RadianceCache.HardwareRayTracing.RayAllocatorBuffer"));

			{
				FSetupTraceFromProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSetupTraceFromProbesCS::FParameters>();
				PassParameters->RWTraceProbesIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(TraceProbesIndirectArgs[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWSortProbeTraceTilesIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(SortProbeTraceTilesIndirectArgs[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWRadianceCacheHardwareRayTracingIndirectArgs = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(RadianceCacheHardwareRayTracingIndirectArgs[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RWHardwareRayTracingRayAllocatorBuffer = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(HardwareRayTracingRayAllocatorBuffer[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->ProbeTraceTileAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileAllocator[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->SortTraceTilesGroupSize = FSortProbeTraceTilesCS::GetGroupSize();
				auto ComputeShader = View.ShaderMap->GetShader<FSetupTraceFromProbesCS>(0);

				const FIntVector GroupSize = FIntVector(1);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("SetupTraceFromProbesCS"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					GroupSize);
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			if (GRadianceCacheSortTraceTiles)
			{
				FRDGBufferRef SortedProbeTraceTileData = GraphBuilder.CreateBuffer(ProbeTraceTileData[RadianceCacheIndex]->Desc, TEXT("Lumen.RadianceCache.SortedProbeTraceTileData"));

				FSortProbeTraceTilesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSortProbeTraceTilesCS::FParameters>();
				PassParameters->RWProbeTraceTileData = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(SortedProbeTraceTileData, PF_R32G32_UINT));
				PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
				PassParameters->ProbeTraceTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileData[RadianceCacheIndex], PF_R32G32_UINT));
				PassParameters->ProbeTraceTileAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileAllocator[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->SortProbeTraceTilesIndirectArgs = SortProbeTraceTilesIndirectArgs[RadianceCacheIndex];
				PassParameters->RadianceCacheInputs = RadianceCacheInputs;

				auto ComputeShader = View.ShaderMap->GetShader<FSortProbeTraceTilesCS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("SortTraceTiles"),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					PassParameters->SortProbeTraceTilesIndirectArgs,
					0);

				ProbeTraceTileData[RadianceCacheIndex] = SortedProbeTraceTileData;
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FLumenCardTracingParameters TracingParameters;
			GetLumenCardTracingParameters(GraphBuilder, View, *Scene->GetLumenSceneData(View), FrameTemporaries, /*bSurfaceCacheFeedback*/ false, TracingParameters);

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			FRDGTextureUAVRef RadianceProbeAtlasTextureUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Setup.RadianceProbeAtlasTextureSource));
			FRDGTextureUAVRef DepthProbeTextureUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Setup.DepthProbeAtlasTexture));
			const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;
			const int32 MaxProbeTraceTileResolution = RadianceCacheInputs.RadianceProbeResolution / FRadianceCacheTraceFromProbesCS::GetGroupSize() * 2;

			if (Lumen::UseHardwareRayTracedRadianceCache(*View.Family))
			{
				RenderLumenHardwareRayTracingRadianceCache(
					GraphBuilder,
					Scene,
					GetSceneTextureParameters(GraphBuilder, View),
					View,
					TracingParameters,
					RadianceCacheParameters,
					Inputs.Configuration,
					MaxNumProbes,
					MaxProbeTraceTileResolution,
					ProbeTraceData[RadianceCacheIndex],
					ProbeTraceTileData[RadianceCacheIndex],
					ProbeTraceTileAllocator[RadianceCacheIndex],
					TraceProbesIndirectArgs[RadianceCacheIndex],
					HardwareRayTracingRayAllocatorBuffer[RadianceCacheIndex],
					RadianceCacheHardwareRayTracingIndirectArgs[RadianceCacheIndex],
					RadianceProbeAtlasTextureUAV,
					DepthProbeTextureUAV,
					ComputePassFlags);
			}
			else
			{
				FRadianceCacheTraceFromProbesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRadianceCacheTraceFromProbesCS::FParameters>();
				PassParameters->TracingParameters = TracingParameters;
				SetupLumenDiffuseTracingParametersForProbe(View, PassParameters->IndirectTracingParameters, -1.0f);
				PassParameters->RWRadianceProbeAtlasTexture = RadianceProbeAtlasTextureUAV;
				PassParameters->RWDepthProbeAtlasTexture = DepthProbeTextureUAV;
				PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
				PassParameters->ProbeTraceTileData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileData[RadianceCacheIndex], PF_R32G32_UINT));
				PassParameters->ProbeTraceTileAllocator = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceTileAllocator[RadianceCacheIndex], PF_R32_UINT));
				PassParameters->RadianceCacheParameters = RadianceCacheParameters;
				PassParameters->TraceProbesIndirectArgs = TraceProbesIndirectArgs[RadianceCacheIndex];

				FRadianceCacheTraceFromProbesCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FRadianceCacheTraceFromProbesCS::FTraceGlobalSDF>(Lumen::UseGlobalSDFTracing(*View.Family));
				PermutationVector.Set<FRadianceCacheTraceFromProbesCS::FSimpleCoverageBasedExpand>(Lumen::UseGlobalSDFTracing(*View.Family) && Lumen::UseGlobalSDFSimpleCoverageBasedExpand());
				PermutationVector.Set<FRadianceCacheTraceFromProbesCS::FDynamicSkyLight>(Lumen::ShouldHandleSkyLight(Scene, *View.Family));
				auto ComputeShader = View.ShaderMap->GetShader<FRadianceCacheTraceFromProbesCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("TraceFromProbes Res=%ux%u", RadianceCacheInputs.RadianceProbeResolution, RadianceCacheInputs.RadianceProbeResolution),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					PassParameters->TraceProbesIndirectArgs,
					0);
			}
		}

		TInlineArray<FRDGTextureRef> RadianceProbeAtlasTexture(InputArray.Num());

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			RadianceProbeAtlasTexture[RadianceCacheIndex] = Setup.RadianceProbeAtlasTextureSource;

			if (GRadianceCacheFilterProbes)
			{
				FRDGTextureRef FilteredRadianceProbeAtlasTexture = GraphBuilder.CreateTexture(RadianceProbeAtlasTexture[RadianceCacheIndex]->Desc, TEXT("Lumen.RadianceCache.FilteredRadianceProbeAtlasTexture"));

				{
					FFilterProbeRadianceWithGatherCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFilterProbeRadianceWithGatherCS::FParameters>();
					PassParameters->RWRadianceProbeAtlasTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(FilteredRadianceProbeAtlasTexture));
					PassParameters->RadianceProbeAtlasTexture = RadianceProbeAtlasTexture[RadianceCacheIndex];
					PassParameters->DepthProbeAtlasTexture = Setup.DepthProbeAtlasTexture;
					PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
					PassParameters->View = View.ViewUniformBuffer;
					PassParameters->RadianceCacheParameters = RadianceCacheParameters;
					PassParameters->FilterProbesIndirectArgs = FilterProbesIndirectArgs[RadianceCacheIndex];
					PassParameters->SpatialFilterMaxRadianceHitAngle = GLumenRadianceCacheFilterMaxRadianceHitAngle;

					auto ComputeShader = View.ShaderMap->GetShader<FFilterProbeRadianceWithGatherCS>(0);

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("FilterProbeRadiance Res=%ux%u", RadianceCacheInputs.RadianceProbeResolution, RadianceCacheInputs.RadianceProbeResolution),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						PassParameters->FilterProbesIndirectArgs,
						0);
				}

				RadianceProbeAtlasTexture[RadianceCacheIndex] = FilteredRadianceProbeAtlasTexture;
			}
		}

		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			const FRadianceCacheInputs& RadianceCacheInputs = Inputs.RadianceCacheInputs;
			const FViewInfo& View = Inputs.View;
			const FRadianceCacheSetup& Setup = SetupOutputArray[RadianceCacheIndex];

			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];
			FRadianceCacheState& RadianceCacheState = Outputs.RadianceCacheState;
			FRadianceCacheInterpolationParameters& RadianceCacheParameters = Outputs.RadianceCacheParameters;

			if (RadianceCacheInputs.CalculateIrradiance)
			{
				{
					FCalculateProbeIrradianceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCalculateProbeIrradianceCS::FParameters>();
					PassParameters->RWFinalIrradianceAtlas = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Setup.FinalIrradianceAtlas));
					PassParameters->RadianceProbeAtlasTexture = RadianceProbeAtlasTexture[RadianceCacheIndex];
					PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
					PassParameters->RadianceCacheParameters = RadianceCacheParameters;
					PassParameters->View = View.ViewUniformBuffer;
					// GenerateProbeTraceTilesIndirectArgs is the same so we can reuse it
					PassParameters->CalculateProbeIrradianceIndirectArgs = GenerateProbeTraceTilesIndirectArgs[RadianceCacheIndex];

					auto ComputeShader = View.ShaderMap->GetShader<FCalculateProbeIrradianceCS>();

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("CalculateProbeIrradiance Res=%ux%u", RadianceCacheInputs.IrradianceProbeResolution, RadianceCacheInputs.IrradianceProbeResolution),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						GenerateProbeTraceTilesIndirectArgs[RadianceCacheIndex],
						0);
				}

				RadianceCacheParameters.RadianceCacheFinalIrradianceAtlas = Setup.FinalIrradianceAtlas;

				{
					FPrepareProbeOcclusionCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPrepareProbeOcclusionCS::FParameters>();
					PassParameters->RWRadianceCacheProbeOcclusionAtlas = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Setup.ProbeOcclusionAtlas));
					PassParameters->DepthProbeAtlasTexture = Setup.DepthProbeAtlasTexture;
					PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
					PassParameters->RadianceCacheParameters = RadianceCacheParameters;
					PassParameters->PrepareProbeOcclusionIndirectArgs = PrepareProbeOcclusionIndirectArgs[RadianceCacheIndex];

					auto ComputeShader = View.ShaderMap->GetShader<FPrepareProbeOcclusionCS>();

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("PrepareProbeOcclusion Res=%ux%u", RadianceCacheInputs.OcclusionProbeResolution, RadianceCacheInputs.OcclusionProbeResolution),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						PrepareProbeOcclusionIndirectArgs[RadianceCacheIndex],
						0);
				}

				RadianceCacheParameters.RadianceCacheProbeOcclusionAtlas = Setup.ProbeOcclusionAtlas;
			}
			else
			{
				FRDGTextureUAVRef FinalRadianceAtlasUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Setup.FinalRadianceAtlas));

				const bool bGenerateMips = RadianceCacheInputs.FinalRadianceAtlasMaxMip > 0;

				ensureMsgf(RadianceCacheInputs.FinalRadianceAtlasMaxMip <= 2, TEXT("Requested mip is more than supported by shader"));
				{
					FFixupBordersAndGenerateMipsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFixupBordersAndGenerateMipsCS::FParameters>();
					PassParameters->RWFinalRadianceAtlasMip0 = FinalRadianceAtlasUAV;

					if (bGenerateMips)
					{
						PassParameters->RWFinalRadianceAtlasMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Setup.FinalRadianceAtlas, 1));
						PassParameters->RWFinalRadianceAtlasMip2 = PassParameters->RWFinalRadianceAtlasMip1;

						if (RadianceCacheInputs.FinalRadianceAtlasMaxMip > 1)
						{
							PassParameters->RWFinalRadianceAtlasMip2 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Setup.FinalRadianceAtlas, 2));
						}
					}
				
					PassParameters->RadianceProbeAtlasTexture = RadianceProbeAtlasTexture[RadianceCacheIndex];
					PassParameters->ProbeTraceData = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(ProbeTraceData[RadianceCacheIndex], PF_A32B32G32R32F));
					PassParameters->RadianceCacheParameters = RadianceCacheParameters;
					PassParameters->FixupProbeBordersIndirectArgs = FixupProbeBordersIndirectArgs[RadianceCacheIndex];

					FFixupBordersAndGenerateMipsCS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FFixupBordersAndGenerateMipsCS::FGenerateMips>(bGenerateMips);
					auto ComputeShader = View.ShaderMap->GetShader<FFixupBordersAndGenerateMipsCS>(PermutationVector);

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("FixupBordersAndGenerateMips"),
						ComputePassFlags,
						ComputeShader,
						PassParameters,
						FixupProbeBordersIndirectArgs[RadianceCacheIndex],
						0);
				}

				RadianceCacheParameters.RadianceCacheFinalRadianceAtlas = Setup.FinalRadianceAtlas;
			}

			if (RadianceCacheInputs.RadianceCacheStats != 0)
			{
				ShaderPrint::SetEnabled(true);

				const int32 MaxNumProbes = RadianceCacheInputs.ProbeAtlasResolutionInProbes.X * RadianceCacheInputs.ProbeAtlasResolutionInProbes.Y;

				FRadianceCacheUpdateStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRadianceCacheUpdateStatsCS::FParameters>();
				ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
				PassParameters->View = View.ViewUniformBuffer;
				PassParameters->PriorityHistogram = GraphBuilder.CreateSRV(PriorityHistogram[RadianceCacheIndex]);
				PassParameters->MaxUpdateBucket = GraphBuilder.CreateSRV(MaxUpdateBucket[RadianceCacheIndex]);
				PassParameters->MaxTracesFromMaxUpdateBucket = GraphBuilder.CreateSRV(MaxTracesFromMaxUpdateBucket[RadianceCacheIndex]);
				PassParameters->ProbesToUpdateTraceCost = GraphBuilder.CreateSRV(ProbesToUpdateTraceCost[RadianceCacheIndex]);
				PassParameters->ProbeTraceAllocator = GraphBuilder.CreateSRV(ProbeTraceAllocator[RadianceCacheIndex], PF_R32_UINT);
				PassParameters->ProbeAllocator = GraphBuilder.CreateSRV(ProbeAllocator[RadianceCacheIndex], PF_R32_UINT);
				PassParameters->ProbeFreeListAllocator = GraphBuilder.CreateSRV(ProbeFreeListAllocator[RadianceCacheIndex], PF_R32_UINT);
				PassParameters->MaxNumProbes = MaxNumProbes;

				auto ComputeShader = View.ShaderMap->GetShader<FRadianceCacheUpdateStatsCS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("RadianceCacheUpdateStats"),
					ComputeShader,
					PassParameters,
					FIntVector(1, 1, 1));
			}

			RadianceCacheState.ProbeFreeListAllocator = GraphBuilder.ConvertToExternalBuffer(ProbeFreeListAllocator[RadianceCacheIndex]);
			RadianceCacheState.ProbeFreeList = GraphBuilder.ConvertToExternalBuffer(ProbeFreeList[RadianceCacheIndex]);
			RadianceCacheState.ProbeAllocator = GraphBuilder.ConvertToExternalBuffer(ProbeAllocator[RadianceCacheIndex]);
			RadianceCacheState.ProbeLastUsedFrame = GraphBuilder.ConvertToExternalBuffer(ProbeLastUsedFrame[RadianceCacheIndex]);
			RadianceCacheState.ProbeLastTracedFrame = GraphBuilder.ConvertToExternalBuffer(ProbeLastTracedFrame[RadianceCacheIndex]);
			RadianceCacheState.ProbeWorldOffset = GraphBuilder.ConvertToExternalBuffer(ProbeWorldOffset[RadianceCacheIndex]);
			RadianceCacheState.RadianceProbeIndirectionTexture = GraphBuilder.ConvertToExternalTexture(RadianceCacheParameters.RadianceProbeIndirectionTexture);
			RadianceCacheState.DepthProbeAtlasTexture = GraphBuilder.ConvertToExternalTexture(Setup.DepthProbeAtlasTexture);
			RadianceCacheState.RadianceProbeAtlasTexture = GraphBuilder.ConvertToExternalTexture(Setup.RadianceProbeAtlasTextureSource);

			if (Setup.FinalRadianceAtlas)
			{
				RadianceCacheState.FinalRadianceAtlas = GraphBuilder.ConvertToExternalTexture(Setup.FinalRadianceAtlas);
			}
			else
			{
				RadianceCacheState.FinalRadianceAtlas = nullptr;
			}
		
			if (Setup.FinalIrradianceAtlas)
			{
				RadianceCacheState.FinalIrradianceAtlas = GraphBuilder.ConvertToExternalTexture(Setup.FinalIrradianceAtlas);
				RadianceCacheState.ProbeOcclusionAtlas = GraphBuilder.ConvertToExternalTexture(Setup.ProbeOcclusionAtlas);
			}
			else
			{
				RadianceCacheState.FinalIrradianceAtlas = nullptr;
				RadianceCacheState.ProbeOcclusionAtlas = nullptr;
			}

			RadianceCacheParameters.RadianceCacheDepthAtlas = Setup.DepthProbeAtlasTexture;
		}
	}
	else // GRadianceCacheUpdate != 0
	{
		for (int32 RadianceCacheIndex = 0; RadianceCacheIndex < InputArray.Num(); RadianceCacheIndex++)
		{
			const FUpdateInputs& Inputs = InputArray[RadianceCacheIndex];
			FUpdateOutputs& Outputs = OutputArray[RadianceCacheIndex];

			GetInterpolationParameters(Inputs.View, GraphBuilder, Outputs.RadianceCacheState, Inputs.RadianceCacheInputs, Outputs.RadianceCacheParameters);
		}
	}
}

}