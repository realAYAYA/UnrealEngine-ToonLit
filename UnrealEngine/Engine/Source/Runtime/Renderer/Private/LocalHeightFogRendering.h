// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineDefines.h"
#include "RenderGraph.h"
#include "RenderResource.h"
#include "RenderGraphResources.h"
#include "SceneView.h"
#include "Containers/Array.h"

class FScene;
class FViewInfo;
struct FMinimalSceneTextures;



/*=============================================================================
	Local height fog rendering common data
=============================================================================*/

class FLocalHeightFogGPUInstanceData
{
public:
	FMatrix44f Transform;
	FMatrix44f InvTransform;

	FMatrix44f InvTranformNoScale;
	FMatrix44f TransformScaleOnly;

	float Density;
	float HeightFalloff;
	float HeightOffset;
	float RadialAttenuation;

	FVector3f Albedo;
	float PhaseG;
	FVector3f Emissive;
	float FogMode;
};

class RENDERER_API FLocalFogVolumeSortKey
{
public:
	union
	{
		uint64 PackedData;

		struct
		{
			uint64 Index : 16; // Index of the volume
			uint64 Distance : 32; // then by distance
			uint64 Priority : 16; // First order by priority
		} FogVolume;
	};

	FORCEINLINE bool operator!=(FLocalFogVolumeSortKey B) const
	{
		return PackedData != B.PackedData;
	}

	FORCEINLINE bool operator<(FLocalFogVolumeSortKey B) const
	{
		return PackedData > B.PackedData;
	}
};

// This can be generated once for a scene and then shared between all the views to generate the GPU buffer data needed for rendering while accounting for sorting.
struct FLocalFogVolumeSortingData
{
	uint32 LocalHeightFogInstanceCount;
	uint32 LocalHeightFogInstanceCountFinal;
	FLocalHeightFogGPUInstanceData* LocalHeightFogGPUInstanceData;
	FVector* LocalHeightFogCenterPos;
	TArray<FLocalFogVolumeSortKey>	LocalHeightFogSortKeys;
};


/*=============================================================================
	Local height fog rendering functions
=============================================================================*/

bool ShouldRenderLocalHeightFog(const FScene* Scene, const FSceneViewFamily& Family);

void GetLocalFogVolumeSortingData(const FScene* Scene, FRDGBuilder& GraphBuilder, FLocalFogVolumeSortingData& Out);

void CreateViewLocalFogVolumeBufferSRV(FViewInfo& View, FRDGBuilder& GraphBuilder, FLocalFogVolumeSortingData& SortingData);

void RenderLocalHeightFog(
	const FScene* Scene,
	TArray<FViewInfo>& Views,
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FRDGTextureRef LightShaftOcclusionTexture);

void RenderLocalHeightFogMobile(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View);

