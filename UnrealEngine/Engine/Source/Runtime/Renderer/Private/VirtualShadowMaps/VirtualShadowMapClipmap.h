// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
VirtualShadowMapClipmap.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ConvexVolume.h"
#include "Templates/RefCounting.h"
#include "VirtualShadowMapProjection.h"

struct FViewMatrices;
struct FVirtualShadowMapProjectionShaderData;
class FPrimitiveSceneInfo;
class FVirtualShadowMap;
class FVirtualShadowMapArray;
class FVirtualShadowMapArrayCacheManager;
class FVirtualShadowMapPerLightCacheEntry;

class FVirtualShadowMapClipmap : FRefCountedObject
{
public:	
	FVirtualShadowMapClipmap(
		FVirtualShadowMapArray& VirtualShadowMapArray,
		const FLightSceneInfo& InLightSceneInfo,
		const FViewMatrices& CameraViewMatrices,
		FIntPoint CameraViewRectSize,
		const FViewInfo* InDependentView,
		float LightMobilityFactor
	);

	FViewMatrices GetViewMatrices(int32 ClipmapIndex) const;

	int32 GetVirtualShadowMapId(int32 ClipmapIndex = 0) const
	{
		return VirtualShadowMapId + ClipmapIndex;
	}

	int32 GetLevelCount() const
	{
		return LevelData.Num();
	}

	// Get absolute clipmap level from index (0..GetLevelCount())
	int32 GetClipmapLevel(int32 ClipmapIndex) const
	{
		return FirstLevel + ClipmapIndex;
	}

	FVector GetPreViewTranslation(int32 ClipmapIndex) const
	{
		return -LevelData[ClipmapIndex].WorldCenter;
	}

	FMatrix GetViewToClipMatrix(int32 ClipmapIndex) const
	{
		return LevelData[ClipmapIndex].ViewToClip;
	}

	FMatrix GetWorldToLightViewRotationMatrix() const
	{
		return WorldToLightViewRotationMatrix;
	}

	const FLightSceneInfo& GetLightSceneInfo() const
	{
		return LightSceneInfo;
	}

	const FVirtualShadowMapProjectionShaderData& GetProjectionShaderData(int32 ClipmapIndex) const;

	FVector GetWorldOrigin() const
	{
		return WorldOrigin;
	}

	// Returns the max radius the clipmap is guaranteed to cover (i.e. the radius of the last clipmap level)
	// Note that this is not a conservative radius of the level projection, which is snapped
	float GetMaxRadius() const;
	FSphere GetBoundingSphere() const { return BoundingSphere; }
	FConvexVolume GetViewFrustumBounds() const { return ViewFrustumBounds; }

	const FViewInfo* GetDependentView() const { return DependentView; }

	static int32 GetFirstLevel();
	// Returns a mask with one bit per level of which coarse pages to mark (based on cvars)
	// Bits relative to FirstLevel (i.e. in terms of ClipmapIndex, not ClipmapLevel)	
	static uint32 GetCoarsePageClipmapIndexMask();

	/**
	 * Called when a primitive passes CPU-culling, note that this applies to non-nanite primitives only. Not thread safe in general.
	 */ 
	void OnPrimitiveRendered(const FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/**
	 * Called to push any cache data to cache entry at the end of the frame.
	 */
	void UpdateCachedFrameData();

	/**
	 * Return array with one bit per primitive, a set bit indicates that the primitive transitioned from not rendered to rendered this frame (see OnPrimitiveRendered above).
	 */
	TConstArrayView<uint32> GetRevealedPrimitivesMask() const { return RevealedPrimitivesMask.IsEmpty() ? MakeArrayView<uint32>(nullptr, 0) : MakeArrayView(RevealedPrimitivesMask.GetData(), FBitSet::CalculateNumWords(RevealedPrimitivesMask.Num())); }
	int32 GetNumRevealedPrimitives() const { return RevealedPrimitivesMask.Num(); }

	TSharedPtr<FVirtualShadowMapPerLightCacheEntry>& GetCacheEntry() { return PerLightCacheEntry; }

private:
	FVirtualShadowMapProjectionShaderData ComputeProjectionShaderData(int32 ClipmapIndex) const;
	void ComputeBoundingVolumes(const FVector CameraOrigin);

	const FLightSceneInfo& LightSceneInfo;

	/**
	 * DependentView is the 'main' or visible geometry view that this view-dependent clipmap was created for. Should only be used to 
	 * identify the view during shadow projection (note: this should be refactored to be more explicit instead).
	 */
	const FViewInfo* DependentView;

	/** Origin of the clipmap in world space
	* Usually aligns with the camera position from which it was created.
	* Note that the centers of each of the levels can be different as they are snapped to page alignment at their respective scales
	* */
	FVector WorldOrigin;
	FVector CameraToViewTarget;

	FVector LightDirection;

	/** Directional light rotation matrix (no translation) */
	FMatrix WorldToLightViewRotationMatrix;

	int32 FirstLevel;
	float ResolutionLodBias;
	float MaxRadius;

	struct FLevelData
	{
		FMatrix ViewToClip;
		FVector WorldCenter;
		//Offset from (0,0) to clipmap corner, in level radii
		FInt64Point CornerOffset;
		//Offset from LastLevel-snapped WorldCenter to clipmap corner, in level radii
		FIntPoint RelativeCornerOffset;
		double WPODistanceDisableThresholdSquared;
	};
	TArray< FLevelData, TInlineAllocator<32> > LevelData;

	FSphere BoundingSphere;
	FConvexVolume ViewFrustumBounds;

	int32 VirtualShadowMapId = INDEX_NONE;		// Base ID; levels are contiguous
	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> PerLightCacheEntry;

	// Rendered primitives are marked during culling (through OnPrimitiveRendered being called).
	TBitArray<> RenderedPrimitives;
	// Set to 1 for each primitives that went from not being rendered to being rendered this frame
	TBitArray<> RevealedPrimitivesMask;
};
