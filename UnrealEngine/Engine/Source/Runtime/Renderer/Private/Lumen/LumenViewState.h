// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenViewState.h:
=============================================================================*/

#pragma once

#include "RenderGraphResources.h"
#include "SceneTexturesConfig.h"

const static int32 NumLumenDiffuseIndirectTextures = 2;
// Must match shader
const static int32 MaxVoxelClipmapLevels = 8;

class FLumenGatherCvarState
{
public:

	FLumenGatherCvarState();

	int32 TraceMeshSDFs;
	float MeshSDFTraceDistance;
	float SurfaceBias;
	int32 VoxelTracingMode;
	int32 DirectLighting;

	inline bool operator==(const FLumenGatherCvarState& Rhs) const
	{
		return TraceMeshSDFs == Rhs.TraceMeshSDFs &&
			MeshSDFTraceDistance == Rhs.MeshSDFTraceDistance &&
			SurfaceBias == Rhs.SurfaceBias &&
			VoxelTracingMode == Rhs.VoxelTracingMode &&
			DirectLighting == Rhs.DirectLighting;
	}
};

class FScreenProbeGatherTemporalState
{
public:
	FIntRect DiffuseIndirectHistoryViewRect;
	FVector4f DiffuseIndirectHistoryScreenPositionScaleBias;
	TRefCountPtr<IPooledRenderTarget> DiffuseIndirectHistoryRT;
	TRefCountPtr<IPooledRenderTarget> BackfaceDiffuseIndirectHistoryRT;
	TRefCountPtr<IPooledRenderTarget> RoughSpecularIndirectHistoryRT; 
	TRefCountPtr<IPooledRenderTarget> NumFramesAccumulatedRT;
	TRefCountPtr<IPooledRenderTarget> FastUpdateModeHistoryRT;
	FIntRect ProbeHistoryViewRect;
	FVector4f ProbeHistoryScreenPositionScaleBias;
	TRefCountPtr<IPooledRenderTarget> HistoryScreenProbeSceneDepth;
	TRefCountPtr<IPooledRenderTarget> HistoryScreenProbeTranslatedWorldPosition;
	TRefCountPtr<IPooledRenderTarget> ProbeHistoryScreenProbeRadiance;
	TRefCountPtr<IPooledRenderTarget> ImportanceSamplingHistoryScreenProbeRadiance;
	FLumenGatherCvarState LumenGatherCvars;
	FIntPoint HistorySceneTexturesExtent;
	FIntPoint HistoryEffectiveResolution;
	uint32 HistorySubstrateMaxClosureCount;

	FScreenProbeGatherTemporalState()
	{
		DiffuseIndirectHistoryViewRect = FIntRect(0, 0, 0, 0);
		DiffuseIndirectHistoryScreenPositionScaleBias = FVector4f(0, 0, 0, 0);
		ProbeHistoryViewRect = FIntRect(0, 0, 0, 0);
		ProbeHistoryScreenPositionScaleBias = FVector4f(0, 0, 0, 0);
		HistorySceneTexturesExtent = FIntPoint(0,0);
		HistoryEffectiveResolution = FIntPoint(0,0);
		HistorySubstrateMaxClosureCount = 0;
	}

	void SafeRelease()
	{
		DiffuseIndirectHistoryRT.SafeRelease();
		BackfaceDiffuseIndirectHistoryRT.SafeRelease();
		RoughSpecularIndirectHistoryRT.SafeRelease();
		NumFramesAccumulatedRT.SafeRelease();
		FastUpdateModeHistoryRT.SafeRelease();
		HistoryScreenProbeSceneDepth.SafeRelease();
		HistoryScreenProbeTranslatedWorldPosition.SafeRelease();
		ProbeHistoryScreenProbeRadiance.SafeRelease();
		ImportanceSamplingHistoryScreenProbeRadiance.SafeRelease();
	}

#if WITH_MGPU
	void AddCrossGPUTransfers(uint32 SourceGPUIndex, uint32 DestGPUIndex, TArray<FTransferResourceParams>& OutTransfers)
	{
		#define TRANSFER_LUMEN_RESOURCE(NAME) \
			if (NAME) OutTransfers.Add(FTransferResourceParams(NAME->GetRHI(), SourceGPUIndex, DestGPUIndex, false, false))

		TRANSFER_LUMEN_RESOURCE(DiffuseIndirectHistoryRT);
		TRANSFER_LUMEN_RESOURCE(RoughSpecularIndirectHistoryRT);
		TRANSFER_LUMEN_RESOURCE(NumFramesAccumulatedRT);
		TRANSFER_LUMEN_RESOURCE(FastUpdateModeHistoryRT);
		TRANSFER_LUMEN_RESOURCE(HistoryScreenProbeSceneDepth);
		TRANSFER_LUMEN_RESOURCE(HistoryScreenProbeTranslatedWorldPosition);
		TRANSFER_LUMEN_RESOURCE(ProbeHistoryScreenProbeRadiance);
		TRANSFER_LUMEN_RESOURCE(ImportanceSamplingHistoryScreenProbeRadiance);

		#undef TRANSFER_LUMEN_RESOURCE
	}
#endif  // WITH_MGPU

	uint64 GetGPUSizeBytes(bool bLogSizes) const;
};

class FReSTIRTemporalResamplingState
{
public:

	FIntRect HistoryViewRect;
	FVector4f HistoryScreenPositionScaleBias;
	FIntPoint HistoryReservoirViewSize;
	FIntPoint HistoryReservoirBufferSize;
	TRefCountPtr<IPooledRenderTarget> TemporalReservoirRayDirectionRT;
	TRefCountPtr<IPooledRenderTarget> TemporalReservoirTraceRadianceRT;
	TRefCountPtr<IPooledRenderTarget> TemporalReservoirTraceHitDistanceRT;
	TRefCountPtr<IPooledRenderTarget> TemporalReservoirTraceHitNormalRT;
	TRefCountPtr<IPooledRenderTarget> TemporalReservoirWeightsRT;
	TRefCountPtr<IPooledRenderTarget> DownsampledDepthHistoryRT;
	TRefCountPtr<IPooledRenderTarget> DownsampledNormalHistoryRT;

	FReSTIRTemporalResamplingState()
	{
		HistoryViewRect = FIntRect(0, 0, 0, 0);
		HistoryScreenPositionScaleBias = FVector4f(0, 0, 0, 0);
		HistoryReservoirViewSize = FIntPoint(0, 0);
		HistoryReservoirBufferSize = FIntPoint(0, 0);
	}

	void SafeRelease()
	{
		TemporalReservoirRayDirectionRT.SafeRelease();
		TemporalReservoirTraceRadianceRT.SafeRelease();
		TemporalReservoirTraceHitDistanceRT.SafeRelease();
		TemporalReservoirTraceHitNormalRT.SafeRelease();
		TemporalReservoirWeightsRT.SafeRelease();
		DownsampledDepthHistoryRT.SafeRelease();
		DownsampledNormalHistoryRT.SafeRelease();
	}
};

class FReSTIRTemporalAccumulationState
{
public:
	FIntRect DiffuseIndirectHistoryViewRect;
	FVector4f DiffuseIndirectHistoryScreenPositionScaleBias;
	TRefCountPtr<IPooledRenderTarget> DiffuseIndirectHistoryRT;
	TRefCountPtr<IPooledRenderTarget> RoughSpecularIndirectHistoryRT;
	TRefCountPtr<IPooledRenderTarget> ResolveVarianceHistoryRT;
	TRefCountPtr<IPooledRenderTarget> NumFramesAccumulatedRT;
	FIntPoint HistorySceneTexturesExtent;
	FIntPoint HistoryEffectiveResolution;

	FReSTIRTemporalAccumulationState()
	{
		DiffuseIndirectHistoryViewRect = FIntRect(0, 0, 0, 0);
		DiffuseIndirectHistoryScreenPositionScaleBias = FVector4f(0, 0, 0, 0);
	}

	void SafeRelease()
	{
		DiffuseIndirectHistoryRT.SafeRelease();
		RoughSpecularIndirectHistoryRT.SafeRelease();
		ResolveVarianceHistoryRT.SafeRelease();
		NumFramesAccumulatedRT.SafeRelease();
	}
};

class FReSTIRGatherTemporalState
{
public:

	FReSTIRTemporalResamplingState TemporalResamplingState;
	FReSTIRTemporalAccumulationState TemporalAccumulationState;

	void SafeRelease()
	{
		TemporalResamplingState.SafeRelease();
		TemporalAccumulationState.SafeRelease();
	}
};

class FReflectionTemporalState
{
public:
	uint32 HistoryFrameIndex;
	FIntRect HistoryViewRect;
	FVector4f HistoryScreenPositionScaleBias;
	FIntPoint HistorySceneTexturesExtent;
	FIntPoint HistoryEffectiveResolution;
	uint32 HistorySubstrateMaxClosureCount;

	TRefCountPtr<IPooledRenderTarget> SpecularIndirectHistoryRT;
	TRefCountPtr<IPooledRenderTarget> NumFramesAccumulatedRT;
	TRefCountPtr<IPooledRenderTarget> ResolveVarianceHistoryRT;
	TRefCountPtr<IPooledRenderTarget> DepthHistoryRT;
	TRefCountPtr<IPooledRenderTarget> NormalHistoryRT;

	FReflectionTemporalState()
	{
		HistoryFrameIndex = 0;
		HistoryViewRect = FIntRect(0, 0, 0, 0);
		HistoryScreenPositionScaleBias = FVector4f(0, 0, 0, 0);
		HistorySceneTexturesExtent = FIntPoint(0,0);
		HistoryEffectiveResolution = FIntPoint(0,0);
		HistorySubstrateMaxClosureCount = 0;
	}

	void SafeRelease()
	{
		SpecularIndirectHistoryRT.SafeRelease();
		NumFramesAccumulatedRT.SafeRelease();
		ResolveVarianceHistoryRT.SafeRelease();
		DepthHistoryRT.SafeRelease();
		NormalHistoryRT.SafeRelease();
	}

#if WITH_MGPU
	void AddCrossGPUTransfers(uint32 SourceGPUIndex, uint32 DestGPUIndex, TArray<FTransferResourceParams>& OutTransfers)
	{
		#define TRANSFER_LUMEN_RESOURCE(NAME) \
			if (NAME) OutTransfers.Add(FTransferResourceParams(NAME->GetRHI(), SourceGPUIndex, DestGPUIndex, false, false))

		TRANSFER_LUMEN_RESOURCE(SpecularIndirectHistoryRT);
		TRANSFER_LUMEN_RESOURCE(NumFramesAccumulatedRT);
		TRANSFER_LUMEN_RESOURCE(ResolveVarianceHistoryRT);
		TRANSFER_LUMEN_RESOURCE(DepthHistoryRT);
		TRANSFER_LUMEN_RESOURCE(NormalHistoryRT);

		#undef TRANSFER_LUMEN_RESOURCE
	}
#endif  // WITH_MGPU

	uint64 GetGPUSizeBytes(bool bLogSizes) const;
};

class FRadianceCacheClipmap
{
public:
	/** World space bounds. */
	FVector Center;
	float Extent;

	FVector ProbeCoordToWorldCenterBias;
	float ProbeCoordToWorldCenterScale;

	FVector WorldPositionToProbeCoordBias;
	float WorldPositionToProbeCoordScale;

	float ProbeTMin;

	/** Offset applied to UVs so that only new or dirty areas of the volume texture have to be updated. */
	FVector VolumeUVOffset;

	/* Distance between two probes. */
	float CellSize;
};

class FRadianceCacheState
{
public:
	FRadianceCacheState()
	{}

	TArray<FRadianceCacheClipmap> Clipmaps;

	float ClipmapWorldExtent = 0.0f;
	float ClipmapDistributionBase = 0.0f;

	/** Clipmaps of probe indexes, used to lookup the probe index for a world space position. */
	TRefCountPtr<IPooledRenderTarget> RadianceProbeIndirectionTexture;

	TRefCountPtr<IPooledRenderTarget> RadianceProbeAtlasTexture;
	/** Texture containing radiance cache probes, ready for sampling with bilinear border. */
	TRefCountPtr<IPooledRenderTarget> FinalRadianceAtlas;
	TRefCountPtr<IPooledRenderTarget> FinalIrradianceAtlas;
	TRefCountPtr<IPooledRenderTarget> ProbeOcclusionAtlas;

	TRefCountPtr<IPooledRenderTarget> DepthProbeAtlasTexture;

	TRefCountPtr<FRDGPooledBuffer> ProbeAllocator;
	TRefCountPtr<FRDGPooledBuffer> ProbeFreeListAllocator;
	TRefCountPtr<FRDGPooledBuffer> ProbeFreeList;
	TRefCountPtr<FRDGPooledBuffer> ProbeLastUsedFrame;
	TRefCountPtr<FRDGPooledBuffer> ProbeLastTracedFrame;
	TRefCountPtr<FRDGPooledBuffer> ProbeWorldOffset;

	void ReleaseTextures()
	{
		RadianceProbeIndirectionTexture.SafeRelease();
		RadianceProbeAtlasTexture.SafeRelease();
		FinalRadianceAtlas.SafeRelease();
		FinalIrradianceAtlas.SafeRelease();
		ProbeOcclusionAtlas.SafeRelease();
		DepthProbeAtlasTexture.SafeRelease();
		ProbeAllocator.SafeRelease();
		ProbeFreeListAllocator.SafeRelease();
		ProbeFreeList.SafeRelease();
		ProbeLastUsedFrame.SafeRelease();
		ProbeLastTracedFrame.SafeRelease();
		ProbeWorldOffset.SafeRelease();
	}

#if WITH_MGPU
	void AddCrossGPUTransfers(uint32 SourceGPUIndex, uint32 DestGPUIndex, TArray<FTransferResourceParams>& OutTransfers)
	{
		#define TRANSFER_LUMEN_RESOURCE(NAME) \
			if (NAME) OutTransfers.Add(FTransferResourceParams(NAME->GetRHI(), SourceGPUIndex, DestGPUIndex, false, false))

		TRANSFER_LUMEN_RESOURCE(RadianceProbeIndirectionTexture);
		TRANSFER_LUMEN_RESOURCE(RadianceProbeAtlasTexture);
		TRANSFER_LUMEN_RESOURCE(FinalRadianceAtlas);
		TRANSFER_LUMEN_RESOURCE(FinalIrradianceAtlas);
		TRANSFER_LUMEN_RESOURCE(ProbeOcclusionAtlas);
		TRANSFER_LUMEN_RESOURCE(DepthProbeAtlasTexture);
		TRANSFER_LUMEN_RESOURCE(ProbeAllocator);
		TRANSFER_LUMEN_RESOURCE(ProbeFreeListAllocator);
		TRANSFER_LUMEN_RESOURCE(ProbeFreeList);
		TRANSFER_LUMEN_RESOURCE(ProbeLastUsedFrame);
		TRANSFER_LUMEN_RESOURCE(ProbeLastTracedFrame);
		TRANSFER_LUMEN_RESOURCE(ProbeWorldOffset);

		#undef TRANSFER_LUMEN_RESOURCE
	}
#endif  // WITH_MGPU

	uint64 GetGPUSizeBytes(bool bLogSizes) const;
};

class FLumenViewState
{
public:

	FScreenProbeGatherTemporalState ScreenProbeGatherState;
	FReSTIRGatherTemporalState ReSTIRGatherState;
	FReflectionTemporalState ReflectionState;
	FReflectionTemporalState TranslucentReflectionState;
	TRefCountPtr<IPooledRenderTarget> DepthHistoryRT;
	TRefCountPtr<IPooledRenderTarget> NormalHistoryRT;

	// Translucency
	TRefCountPtr<IPooledRenderTarget> TranslucencyVolume0;
	TRefCountPtr<IPooledRenderTarget> TranslucencyVolume1;

	FRadianceCacheState RadianceCacheState;
	FRadianceCacheState TranslucencyVolumeRadianceCacheState;

	void SafeRelease()
	{
		ScreenProbeGatherState.SafeRelease();
		ReSTIRGatherState.SafeRelease();
		ReflectionState.SafeRelease();
		TranslucentReflectionState.SafeRelease();
		DepthHistoryRT.SafeRelease();
		NormalHistoryRT.SafeRelease();

		TranslucencyVolume0.SafeRelease();
		TranslucencyVolume1.SafeRelease();

		RadianceCacheState.ReleaseTextures();
		TranslucencyVolumeRadianceCacheState.ReleaseTextures();
	}

#if WITH_MGPU
	void AddCrossGPUTransfers(uint32 SourceGPUIndex, uint32 DestGPUIndex, TArray<FTransferResourceParams>& OutTransfers)
	{
		#define TRANSFER_LUMEN_RESOURCE(NAME) \
			if (NAME) OutTransfers.Add(FTransferResourceParams(NAME->GetRHI(), SourceGPUIndex, DestGPUIndex, false, false))

		TRANSFER_LUMEN_RESOURCE(DepthHistoryRT);
		TRANSFER_LUMEN_RESOURCE(TranslucencyVolume0);
		TRANSFER_LUMEN_RESOURCE(TranslucencyVolume1);

		#undef TRANSFER_LUMEN_RESOURCE

		ScreenProbeGatherState.AddCrossGPUTransfers(SourceGPUIndex, DestGPUIndex, OutTransfers);
		ReflectionState.AddCrossGPUTransfers(SourceGPUIndex, DestGPUIndex, OutTransfers);
		TranslucentReflectionState.AddCrossGPUTransfers(SourceGPUIndex, DestGPUIndex, OutTransfers);
		RadianceCacheState.AddCrossGPUTransfers(SourceGPUIndex, DestGPUIndex, OutTransfers);
		TranslucencyVolumeRadianceCacheState.AddCrossGPUTransfers(SourceGPUIndex, DestGPUIndex, OutTransfers);
	}
#endif  // WITH_MGPU

	uint64 GetGPUSizeBytes(bool bLogSizes) const;
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLumenCardPassUniformParameters, RENDERER_API)
	SHADER_PARAMETER_STRUCT(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
