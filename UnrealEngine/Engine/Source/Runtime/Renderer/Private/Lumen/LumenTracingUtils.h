// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"
#include "DistanceFieldLightingShared.h"
#include "LumenSceneData.h"
#include "IndirectLightRendering.h"
#include "ReflectionEnvironment.h"
#include "FogRendering.h"

DECLARE_UNIFORM_BUFFER_STRUCT(FSceneUniformParameters, RENDERER_API)

class FLumenCardUpdateContext;
namespace LumenRadianceCache { class FRadianceCacheInputs; }

class FHemisphereDirectionSampleGenerator
{
public:
	TArray<FVector4f> SampleDirections;
	float ConeHalfAngle = 0;
	int32 Seed = 0;
	int32 PowerOfTwoDivisor = 1;
	bool bFullSphere = false;
	bool bCosineDistribution = false;

	void GenerateSamples(int32 TargetNumSamples, int32 InPowerOfTwoDivisor, int32 InSeed, bool bInFullSphere = false, bool bInCosineDistribution = false);

	void GetSampleDirections(const FVector4f*& OutDirections, int32& OutNumDirections) const
	{
		OutDirections = SampleDirections.GetData();
		OutNumDirections = SampleDirections.Num();
	}
};

BEGIN_SHADER_PARAMETER_STRUCT(FLumenCardTracingParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FReflectionUniformParameters, ReflectionStruct)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
	SHADER_PARAMETER(float, DiffuseColorBoost)
	SHADER_PARAMETER(float, SkylightLeaking)
	SHADER_PARAMETER(float, SkylightLeakingRoughness)
	SHADER_PARAMETER(float, InvFullSkylightLeakingDistance)

	SHADER_PARAMETER(uint32, SampleHeightFog)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FFogUniformParameters, FogUniformParameters)

	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCardPageLastUsedBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCardPageHighResLastUsedBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWSurfaceCacheFeedbackBufferAllocator)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWSurfaceCacheFeedbackBuffer)
	SHADER_PARAMETER(uint32, SurfaceCacheFeedbackBufferSize)
	SHADER_PARAMETER(uint32, SurfaceCacheFeedbackBufferTileWrapMask)
	SHADER_PARAMETER(FIntPoint, SurfaceCacheFeedbackBufferTileJitter)
	SHADER_PARAMETER(float, SurfaceCacheFeedbackResLevelBias)
	SHADER_PARAMETER(uint32, SurfaceCacheUpdateFrameIndex)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DirectLightingAtlas)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IndirectLightingAtlas)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FinalLightingAtlas)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AlbedoAtlas)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OpacityAtlas)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NormalAtlas)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EmissiveAtlas)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthAtlas)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GlobalDistanceFieldPageObjectGridBuffer)
	SHADER_PARAMETER(uint32, NumGlobalSDFClipmaps)
END_SHADER_PARAMETER_STRUCT()

extern void GetLumenCardTracingParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FLumenSceneData& LumenSceneData,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	bool bSurfaceCacheFeedback,
	FLumenCardTracingParameters& TracingParameters);

BEGIN_SHADER_PARAMETER_STRUCT(FLumenMeshSDFTracingParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldObjectBufferParameters, DistanceFieldObjectBuffers)
	SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldAtlasParameters, DistanceFieldAtlas)
	SHADER_PARAMETER(float, MeshSDFNotCoveredExpandSurfaceScale)
	SHADER_PARAMETER(float, MeshSDFNotCoveredMinStepScale)
	SHADER_PARAMETER(float, MeshSDFDitheredTransparencyStepThreshold)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FLumenMeshSDFGridParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenMeshSDFTracingParameters, TracingParameters)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumGridCulledMeshSDFObjects)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, GridCulledMeshSDFObjectStartOffsetArray)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, GridCulledMeshSDFObjectIndicesArray)
	SHADER_PARAMETER(uint32, CardGridPixelSizeShift)
	SHADER_PARAMETER(FVector3f, CardGridZParams)
	SHADER_PARAMETER(FIntVector, CullGridSize)
	// Heightfield data
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumCulledHeightfieldObjects)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CulledHeightfieldObjectIndexBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumGridCulledHeightfieldObjects)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, GridCulledHeightfieldObjectStartOffsetArray)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, GridCulledHeightfieldObjectIndicesArray)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FLumenIndirectTracingParameters, )
	SHADER_PARAMETER(float, StepFactor)
	SHADER_PARAMETER(float, CardTraceEndDistanceFromCamera)
	SHADER_PARAMETER(float, DiffuseConeHalfAngle)
	SHADER_PARAMETER(float, TanDiffuseConeHalfAngle)
	SHADER_PARAMETER(float, MinSampleRadius)
	SHADER_PARAMETER(float, MinTraceDistance)
	SHADER_PARAMETER(float, MaxTraceDistance)
	SHADER_PARAMETER(float, MaxMeshSDFTraceDistance)
	SHADER_PARAMETER(float, SurfaceBias)
	SHADER_PARAMETER(float, CardInterpolateInfluenceRadius)
	SHADER_PARAMETER(float, SpecularFromDiffuseRoughnessStart)
	SHADER_PARAMETER(float, SpecularFromDiffuseRoughnessEnd)
	SHADER_PARAMETER(int32, HeightfieldMaxTracingSteps)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FLumenDiffuseTracingParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(HybridIndirectLighting::FCommonParameters, CommonDiffuseParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenIndirectTracingParameters, IndirectTracingParameters)
	SHADER_PARAMETER(float, SampleWeight)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DownsampledDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DownsampledNormal)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FLumenHZBScreenTraceParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevSceneColorTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistorySceneDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ClosestHZBTexture)
	SHADER_PARAMETER(FVector4f, HZBUvFactorAndInvFactor)
	SHADER_PARAMETER(FVector2f, PrevSceneColorBilinearUVMin)
	SHADER_PARAMETER(FVector2f, PrevSceneColorBilinearUVMax)
	SHADER_PARAMETER(FVector4f, PrevScreenPositionScaleBias)
	SHADER_PARAMETER(FVector4f, PrevScreenPositionScaleBiasForDepth)
	SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
	SHADER_PARAMETER(FVector2f, HZBBaseTexelSize)
	SHADER_PARAMETER(FVector4f, HZBUVToScreenUVScaleBias)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FLumenScreenSpaceBentNormalParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, ScreenBentNormal)
	SHADER_PARAMETER(uint32, UseShortRangeAO)
END_SHADER_PARAMETER_STRUCT()

extern void CullHeightfieldObjectsForView(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	float MaxMeshSDFInfluenceRadius,
	float CardTraceEndDistanceFromCamera,
	FRDGBufferRef& NumCulledObjects,
	FRDGBufferRef& CulledObjectIndexBuffer);

extern void CullMeshObjectsToViewGrid(
	const FViewInfo& View,
	const FScene* Scene,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	float MaxMeshSDFInfluenceRadius,
	float CardTraceEndDistanceFromCamera,
	int32 GridPixelsPerCellXY,
	int32 GridSizeZ,
	FVector ZParams,
	FRDGBuilder& GraphBuilder,
	FLumenMeshSDFGridParameters& OutGridParameters,
	ERDGPassFlags ComputePassFlags = ERDGPassFlags::Compute);

extern void CullForCardTracing(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FLumenIndirectTracingParameters& IndirectTracingParameters,
	FLumenMeshSDFGridParameters& MeshSDFGridParameters,
	ERDGPassFlags ComputePassFlags = ERDGPassFlags::Compute);

extern void SetupLumenDiffuseTracingParameters(const FViewInfo& View, FLumenIndirectTracingParameters& OutParameters);
extern void SetupLumenDiffuseTracingParametersForProbe(const FViewInfo& View, FLumenIndirectTracingParameters& OutParameters, float DiffuseConeAngle);
extern void SetupLumenMeshSDFTracingParameters(FRDGBuilder& GraphBuilder, const FScene* Scene, const FViewInfo& View,FLumenMeshSDFTracingParameters& OutParameters);

extern FLumenHZBScreenTraceParameters SetupHZBScreenTraceParameters(
	FRDGBuilder& GraphBuilder, 
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	bool bBindLumenHistory = true);

extern int32 GLumenIrradianceFieldGather;

namespace LumenIrradianceFieldGather
{
	LumenRadianceCache::FRadianceCacheInputs SetupRadianceCacheInputs();
}

namespace LumenDiffuseIndirect
{
	bool IsAllowed();
	bool UseAsyncCompute(const FViewFamilyInfo& ViewFamily);
}