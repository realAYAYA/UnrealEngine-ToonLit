// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "RendererInterface.h"
#include "ShaderParameterMacros.h"

class IHeterogeneousVolumeInterface;
class FLightSceneInfo;
class FPrimitiveSceneProxy;
class FRayTracingScene;
class FRDGBuilder;
class FScene;
class FSceneView;
class FViewInfo;
class FVirtualShadowMapArray;
class FVisibleLightInfo;
class IHeterogeneousVolumeInterface;

struct FMaterialShaderParameters;
struct FRDGTextureDesc;
struct FSceneTextures;

//
// External API
//

bool ShouldRenderHeterogeneousVolumes(const FScene* Scene);
bool ShouldRenderHeterogeneousVolumesForView(const FViewInfo& View);
bool DoesPlatformSupportHeterogeneousVolumes(EShaderPlatform Platform);
bool DoesMaterialShaderSupportHeterogeneousVolumes(const FMaterialShaderParameters& Parameters);
bool DoesMaterialShaderSupportHeterogeneousVolumes(const FMaterial& Material);
bool ShouldRenderMeshBatchWithHeterogeneousVolumes(
	const FMeshBatch* Mesh,
	const FPrimitiveSceneProxy* Proxy,
	ERHIFeatureLevel::Type FeatureLevel
);

//
// Internal API
//

namespace HeterogeneousVolumes
{
	// CVars
	FIntVector GetVolumeResolution(const IHeterogeneousVolumeInterface*);
	FIntVector GetLightingCacheResolution(const IHeterogeneousVolumeInterface*);

	float GetShadowStepSize();
	float GetMaxTraceDistance();
	float GetMaxShadowTraceDistance();
	float GetStepSize();
	float GetMaxStepCount();
	float GetMinimumVoxelSizeInFrustum();
	float GetMinimumVoxelSizeOutsideFrustum();

	int32 GetMipLevel();
	int32 GetDebugMode();
	int32 GetLightingCacheMode();
	uint32 GetSparseVoxelMipBias();
	int32 GetBottomLevelGridResolution();
	int32 GetIndirectionGridResolution();
	
	bool ShouldJitter();
	bool ShouldRefineSparseVoxels();
	bool UseHardwareRayTracing();
	bool UseIndirectLighting();
	bool UseSparseVoxelPipeline();
	bool UseSparseVoxelPerTileCulling();
	bool UseLightingCacheForInscattering();
	bool UseLightingCacheForTransmittance();

	bool EnableIndirectionGrid();
	bool EnableLinearInterpolation();

	// Convenience Utils
	int GetVoxelCount(FIntVector VolumeResolution);
	int GetVoxelCount(const FRDGTextureDesc& TextureDesc);
	FIntVector GetMipVolumeResolution(FIntVector VolumeResolution, uint32 MipLevel);
}

struct FVoxelDataPacked
{
	uint32 LinearIndex;
	uint32 MipLevel;
};

BEGIN_UNIFORM_BUFFER_STRUCT(FSparseVoxelUniformBufferParameters, )
	// Object data
	SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	SHADER_PARAMETER(FMatrix44f, WorldToLocal)
	SHADER_PARAMETER(FVector3f, LocalBoundsOrigin)
	SHADER_PARAMETER(FVector3f, LocalBoundsExtent)

	// Volume data
	SHADER_PARAMETER(FIntVector, VolumeResolution)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, ExtinctionTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, EmissionTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, AlbedoTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)

	// Resolution
	SHADER_PARAMETER(FIntVector, LightingCacheResolution)

	// Sparse voxel data
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumVoxelsBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVoxelDataPacked>, VoxelBuffer)
	SHADER_PARAMETER(int, MipLevel)

	// Traversal hints
	SHADER_PARAMETER(float, MaxTraceDistance)
	SHADER_PARAMETER(float, MaxShadowTraceDistance)
	SHADER_PARAMETER(float, StepSize)
	SHADER_PARAMETER(float, StepFactor)
	SHADER_PARAMETER(float, ShadowStepSize)
	SHADER_PARAMETER(float, ShadowStepFactor)
END_UNIFORM_BUFFER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FLightingCacheParameters, )
	SHADER_PARAMETER(FIntVector, LightingCacheResolution)
	SHADER_PARAMETER(float, LightingCacheVoxelBias)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D, LightingCacheTexture)
END_SHADER_PARAMETER_STRUCT()

// Adaptive Voxel Grid structures

struct FTopLevelGridBitmaskData
{
	uint32 PackedData[2];
};

struct FTopLevelGridData
{
	uint32 PackedData[1];
};

struct FScalarGridData
{
	uint32 PackedData[2];
};

struct FVectorGridData
{
	uint32 PackedData[2];
};


BEGIN_UNIFORM_BUFFER_STRUCT(FOrthoVoxelGridUniformBufferParameters, )
	SHADER_PARAMETER(FVector3f, TopLevelGridWorldBoundsMin)
	SHADER_PARAMETER(FVector3f, TopLevelGridWorldBoundsMax)
	SHADER_PARAMETER(FIntVector, TopLevelGridResolution)

	SHADER_PARAMETER(int32, bUseOrthoGrid)
	SHADER_PARAMETER(int32, bUseMajorantGrid)
	SHADER_PARAMETER(int32, bEnableIndirectionGrid)

	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTopLevelGridBitmaskData>, TopLevelGridBitmaskBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTopLevelGridData>, TopLevelGridBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTopLevelGridData>, IndirectionGridBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FScalarGridData>, ExtinctionGridBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVectorGridData>, EmissionGridBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVectorGridData>, ScatteringGridBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FScalarGridData>, MajorantGridBuffer)
END_UNIFORM_BUFFER_STRUCT()

BEGIN_UNIFORM_BUFFER_STRUCT(FFrustumVoxelGridUniformBufferParameters, )
	SHADER_PARAMETER(FMatrix44f, WorldToClip)
	SHADER_PARAMETER(FMatrix44f, ClipToWorld)

	SHADER_PARAMETER(FMatrix44f, WorldToView)
	SHADER_PARAMETER(FMatrix44f, ViewToWorld)

	SHADER_PARAMETER(FMatrix44f, ViewToClip)
	SHADER_PARAMETER(FMatrix44f, ClipToView)

	SHADER_PARAMETER(FVector3f, TopLevelGridWorldBoundsMin)
	SHADER_PARAMETER(FVector3f, TopLevelGridWorldBoundsMax)
	SHADER_PARAMETER(FIntVector, TopLevelFroxelGridResolution)
	SHADER_PARAMETER(FIntVector, VoxelDimensions)

	SHADER_PARAMETER(int32, bUseFrustumGrid)

	SHADER_PARAMETER(float, NearPlaneDepth)
	SHADER_PARAMETER(float, FarPlaneDepth)
	SHADER_PARAMETER(float, TanHalfFOV)

	SHADER_PARAMETER_ARRAY(FVector4f, ViewFrustumPlanes, [6])

	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTopLevelGridData>, TopLevelFroxelGridBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FScalarGridData>, ExtinctionFroxelGridBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVectorGridData>, EmissionFroxelGridBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVectorGridData>, ScatteringFroxelGridBuffer)
END_UNIFORM_BUFFER_STRUCT()

// Render specializations

void BuildOrthoVoxelGrid(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	/*const*/ TArray<FViewInfo>& Views,
	TRDGUniformBufferRef<FOrthoVoxelGridUniformBufferParameters>& OrthoVoxelGridUniformBuffer
);

void BuildFrustumVoxelGrid(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	TRDGUniformBufferRef<FFrustumVoxelGridUniformBufferParameters>& FrustumVoxelGridUniformBuffer
);

struct FAdaptiveFrustumGridParameterCache
{
	FMatrix44f WorldToClip;
	FMatrix44f ClipToWorld;

	FMatrix44f WorldToView;
	FMatrix44f ViewToWorld;

	FMatrix44f ViewToClip;
	FMatrix44f ClipToView;

	FVector3f TopLevelGridWorldBoundsMin;
	FVector3f TopLevelGridWorldBoundsMax;
	FIntVector TopLevelGridResolution;
	FIntVector VoxelDimensions;

	int32 bUseFrustumGrid;

	float NearPlaneDepth;
	float FarPlaneDepth;
	float TanHalfFOV;

	FVector4f ViewFrustumPlanes[6];

	TRefCountPtr<FRDGPooledBuffer> TopLevelGridBuffer;
	TRefCountPtr<FRDGPooledBuffer> ExtinctionGridBuffer;
	TRefCountPtr<FRDGPooledBuffer> EmissionGridBuffer;
	TRefCountPtr<FRDGPooledBuffer> ScatteringGridBuffer;
};

struct FAdaptiveOrthoGridParameterCache
{
	FVector3f TopLevelGridWorldBoundsMin;
	FVector3f TopLevelGridWorldBoundsMax;
	FIntVector TopLevelGridResolution;
	int32 bUseOrthoGrid;
	int32 bUseMajorantGrid;
	int32 bEnableIndirectionGrid;

	TRefCountPtr<FRDGPooledBuffer> TopLevelGridBitmaskBuffer;
	TRefCountPtr<FRDGPooledBuffer> TopLevelGridBuffer;
	TRefCountPtr<FRDGPooledBuffer> IndirectionGridBuffer;

	TRefCountPtr<FRDGPooledBuffer> ExtinctionGridBuffer;
	TRefCountPtr<FRDGPooledBuffer> EmissionGridBuffer;
	TRefCountPtr<FRDGPooledBuffer> ScatteringGridBuffer;

	TRefCountPtr<FRDGPooledBuffer> MajorantGridBuffer;
};

void ExtractFrustumVoxelGridUniformBuffer(
	FRDGBuilder& GraphBuilder,
	const TRDGUniformBufferRef<FFrustumVoxelGridUniformBufferParameters>& FrustumGridUniformBuffer,
	FAdaptiveFrustumGridParameterCache& AdaptiveFrustumGridParameterCache
);

void RegisterExternalFrustumVoxelGridUniformBuffer(
	FRDGBuilder& GraphBuilder,
	const FAdaptiveFrustumGridParameterCache& AdaptiveFrustumGridParameterCache,
	TRDGUniformBufferRef<FFrustumVoxelGridUniformBufferParameters>& FrustumGridUniformBuffer
);

void ExtractOrthoVoxelGridUniformBuffer(
	FRDGBuilder& GraphBuilder,
	const TRDGUniformBufferRef<FOrthoVoxelGridUniformBufferParameters>& OrthoGridUniformBuffer,
	FAdaptiveOrthoGridParameterCache& AdaptiveOrthoGridParameterCache
);

void RegisterExternalOrthoVoxelGridUniformBuffer(
	FRDGBuilder& GraphBuilder,
	const FAdaptiveOrthoGridParameterCache& AdaptiveOrthoGridParameterCache,
	TRDGUniformBufferRef<FOrthoVoxelGridUniformBufferParameters>& OrthoGridUniformBuffer
);

void RenderWithLiveShading(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FScene* Scene,
	const FViewInfo& View,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const int32 PrimitiveId,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
);

void RenderWithPreshading(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	const FSceneViewFamily& ViewFamily,
	FViewInfo& View,
	// Shadow data
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const int32 PrimitiveId,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Transmittance acceleration
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
);

void RenderTransmittanceWithVoxelGrid(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FSceneTextures& SceneTextures,
	FScene* Scene,
	const FSceneViewFamily& ViewFamily,
	FViewInfo& View,
	const TRDGUniformBufferRef<FOrthoVoxelGridUniformBufferParameters>& OrthoGridUniformBuffer,
	const TRDGUniformBufferRef<FFrustumVoxelGridUniformBufferParameters>& FrustumGridUniformBuffer,
	// Output
	FRDGTextureRef& HeterogeneousVolumeRadiance
);

// Preshading Pipeline
void ComputeHeterogeneousVolumeBakeMaterial(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const int32 PrimitiveId,
	const FBoxSphereBounds LocalBoxSphereBounds,
	// Volume data
	FIntVector VolumeResolution,
	// Output
	FRDGTextureRef& HeterogeneousVolumeExtinctionTexture,
	FRDGTextureRef& HeterogeneousVolumeEmissionTexture,
	FRDGTextureRef& HeterogeneousVolumeAlbedoTexture
);

// Sparse Voxel Pipeline

void CopyTexture3D(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef Texture,
	uint32 InputMipLevel,
	FRDGTextureRef& OutputTexture
);

void GenerateSparseVoxels(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef VoxelMinTexture,
	FIntVector VolumeResolution,
	uint32 MipLevel,
	FRDGBufferRef& NumVoxelsBuffer,
	FRDGBufferRef& VoxelBuffer
);

#if RHI_RAYTRACING

void GenerateRayTracingGeometryInstance(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	// Sparse voxel data
	FRDGBufferRef NumVoxelsBuffer,
	TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters> SparseVoxelUniformBuffer,
	// Output
	TArray<FRayTracingGeometryRHIRef>& RayTracingGeometries,
	TArray<FMatrix>& RayTracingTransforms
);

void GenerateRayTracingScene(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	// Ray tracing data
	TArray<FRayTracingGeometryRHIRef>& RayTracingGeometries,
	TArray<FMatrix>& RayTracingTransforms,
	// Output
	FRayTracingScene& RayTracingScene
);

void RenderLightingCacheWithPreshadingHardwareRayTracing(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	// Light data
	bool bApplyEmissionAndTransmittance,
	bool bApplyDirectLighting,
	bool bApplyShadowTransmittance,
	uint32 LightType,
	const FLightSceneInfo* LightSceneInfo,
	// Shadow data
	const FVisibleLightInfo* VisibleLightInfo,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	// Sparse voxel data
	TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters> SparseVoxelUniformBuffer,
	// Ray tracing data
	FRayTracingScene& RayTracingScene,
	// Output
	FRDGTextureRef& LightingCacheTexture
);

void RenderSingleScatteringWithPreshadingHardwareRayTracing(
	FRDGBuilder& GraphBuilder,
	// Scene data
	const FScene* Scene,
	const FViewInfo& View,
	const FSceneTextures& SceneTextures,
	// Light data
	bool bApplyEmissionAndTransmittance,
	bool bApplyDirectLighting,
	bool bApplyShadowTransmittance,
	uint32 LightType,
	const FLightSceneInfo* LightSceneInfo,
	// Shadow data
	const FVisibleLightInfo* VisibleLightInfo,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	// Object data
	const IHeterogeneousVolumeInterface* HeterogeneousVolumeInterface,
	// Sparse voxel data
	TRDGUniformBufferRef<FSparseVoxelUniformBufferParameters> SparseVoxelUniformBuffer,
	// Ray tracing data
	FRayTracingScene& RayTracingScene,
	// Transmittance volume
	FRDGTextureRef LightingCacheTexture,
	// Output
	FRDGTextureRef& HeterogeneousVolumeTexture
);

#endif // RHI_RAYTRACING