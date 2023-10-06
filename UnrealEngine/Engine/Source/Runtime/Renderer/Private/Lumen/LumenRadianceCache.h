// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LumenRadianceCacheInterpolation.h"
#include "LumenTracingUtils.h"

class FSceneTextureParameters;
class FScreenProbeParameters;

namespace LumenRadianceCache
{
	class FRadianceCacheInputs;

	BEGIN_SHADER_PARAMETER_STRUCT(FRadianceCacheMarkParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, RWRadianceProbeIndirectionTexture)
		SHADER_PARAMETER_ARRAY(FVector4f, PackedWorldPositionToRadianceProbeCoord, [MaxClipmaps])
		SHADER_PARAMETER_ARRAY(FVector4f, PackedRadianceProbeCoordToWorldPosition, [MaxClipmaps])
		SHADER_PARAMETER(uint32, RadianceProbeClipmapResolutionForMark)
		SHADER_PARAMETER(uint32, NumRadianceProbeClipmapsForMark)
		SHADER_PARAMETER(float, InvClipmapFadeSizeForMark)
	END_SHADER_PARAMETER_STRUCT()
}

inline void SetWorldPositionToRadianceProbeCoord(FVector4f& PackedParams, const FVector3f& BiasForMark, const float ScaleForMark)
{
	PackedParams = FVector4f(BiasForMark, ScaleForMark);
}

inline void SetRadianceProbeCoordToWorldPosition(FVector4f& PackedParams, const FVector3f& BiasForMark, const float ScaleForMark)
{
	PackedParams = FVector4f(BiasForMark, ScaleForMark);
}

DECLARE_MULTICAST_DELEGATE_ThreeParams(FMarkUsedRadianceCacheProbes, FRDGBuilder&, const FViewInfo&, const LumenRadianceCache::FRadianceCacheMarkParameters&);

struct FRadianceCacheConfiguration
{
	bool bFarField = true;
};

namespace LumenRadianceCache
{
	template<typename T, uint32 NumInlineElements = 4>
	class TInlineArray : public TArray<T, TInlineAllocator<NumInlineElements>>
	{
	public:
		TInlineArray()
		{}

		TInlineArray(int32 InNum) :
			TArray<T, TInlineAllocator<NumInlineElements>>()
		{
			TArray<T, TInlineAllocator<NumInlineElements>>::AddZeroed(InNum);
		}
	};

	// The read-only inputs to a Radiance Cache update
	class FUpdateInputs
	{
	public:
		FRadianceCacheInputs RadianceCacheInputs;
		FRadianceCacheConfiguration Configuration;
		const FViewInfo& View;
		const FScreenProbeParameters* ScreenProbeParameters;
		FRDGBufferSRVRef BRDFProbabilityDensityFunctionSH;

		FMarkUsedRadianceCacheProbes GraphicsMarkUsedRadianceCacheProbes;
		FMarkUsedRadianceCacheProbes ComputeMarkUsedRadianceCacheProbes;

		FUpdateInputs(
			const FRadianceCacheInputs& InRadianceCacheInputs,
			FRadianceCacheConfiguration InConfiguration,
			const FViewInfo& InView,
			const FScreenProbeParameters* InScreenProbeParameters,
			FRDGBufferSRVRef InBRDFProbabilityDensityFunctionSH,
			FMarkUsedRadianceCacheProbes&& InGraphicsMarkUsedRadianceCacheProbes,
			FMarkUsedRadianceCacheProbes&& InComputeMarkUsedRadianceCacheProbes) :
			RadianceCacheInputs(InRadianceCacheInputs),
			Configuration(InConfiguration),
			View(InView),
			ScreenProbeParameters(InScreenProbeParameters),
			BRDFProbabilityDensityFunctionSH(InBRDFProbabilityDensityFunctionSH),
			GraphicsMarkUsedRadianceCacheProbes(MoveTemp(InGraphicsMarkUsedRadianceCacheProbes)),
			ComputeMarkUsedRadianceCacheProbes(MoveTemp(InComputeMarkUsedRadianceCacheProbes))
		{}

		bool IsAnyCallbackBound() const
		{
			return ComputeMarkUsedRadianceCacheProbes.IsBound() || GraphicsMarkUsedRadianceCacheProbes.IsBound();
		}
	};

	// The outputs of a Radiance Cache update
	class FUpdateOutputs
	{
	public:

		FRadianceCacheState& RadianceCacheState;
		FRadianceCacheInterpolationParameters& RadianceCacheParameters;

		FUpdateOutputs(FRadianceCacheState& InRadianceCacheState,
			FRadianceCacheInterpolationParameters& InRadianceCacheParameters) :
			RadianceCacheState(InRadianceCacheState),
			RadianceCacheParameters(InRadianceCacheParameters)
		{}
	};

	/**
	* Updates the requested Radiance Caches, overlapping their dispatches for better GPU utilization
	* Places radiance probes around the positions marked in MarkUsedRadianceCacheProbes, re-using cached results where possible, then traces to update a subset of them.
	* The Radiance Caches are then available for interpolating from the marked positions using FRadianceCacheInterpolationParameters.
	*/
	void UpdateRadianceCaches(
		FRDGBuilder& GraphBuilder, 
		const FLumenSceneFrameTemporaries& FrameTemporaries,
		const TInlineArray<FUpdateInputs>& InputArray,
		TInlineArray<FUpdateOutputs>& OutputArray,
		const FScene* Scene,
		const FViewFamilyInfo& ViewFamily,
		bool bPropagateGlobalLightingChange,
		ERDGPassFlags ComputePassFlags = ERDGPassFlags::Compute);

	void RenderLumenHardwareRayTracingRadianceCache(
		FRDGBuilder& GraphBuilder,
		const FScene* Scene,
		const FSceneTextureParameters& SceneTextures,
		const FViewInfo& View,
		const FLumenCardTracingParameters& TracingParameters,
		const LumenRadianceCache::FRadianceCacheInterpolationParameters& RadianceCacheParameters,
		FRadianceCacheConfiguration Configuration,
		int32 MaxNumProbes,
		int32 MaxProbeTraceTileResolution,
		FRDGBufferRef ProbeTraceData,
		FRDGBufferRef ProbeTraceTileData,
		FRDGBufferRef ProbeTraceTileAllocator,
		FRDGBufferRef TraceProbesIndirectArgs,
		FRDGBufferRef HardwareRayTracingRayAllocatorBuffer,
		FRDGBufferRef RadianceCacheHardwareRayTracingIndirectArgs,
		FRDGTextureUAVRef RadianceProbeAtlasTextureUAV,
		FRDGTextureUAVRef DepthProbeTextureUAV,
		ERDGPassFlags ComputePassFlags);

	ERDGPassFlags GetLumenSceneLightingComputePassFlags(const FEngineShowFlags& EngineShowFlags);
}

extern void MarkUsedProbesForVisualize(FRDGBuilder& GraphBuilder, const FViewInfo& View, const class LumenRadianceCache::FRadianceCacheMarkParameters& RadianceCacheMarkParameters, ERDGPassFlags ComputePassFlags = ERDGPassFlags::Compute);