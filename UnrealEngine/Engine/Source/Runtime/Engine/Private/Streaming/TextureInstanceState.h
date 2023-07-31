// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureInstanceState.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "TextureInstanceView.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Containers/ArrayView.h"

class FStreamingTextureLevelContext;
class ULevel;
class UPrimitiveComponent;
struct FStreamingRenderAssetPrimitiveInfo;

enum class EAddComponentResult : uint8
{
	Fail,
	Fail_UIDensityConstraint,
	Success
};

// Can be used either for static primitives or dynamic primitives
class FRenderAssetInstanceState : public FRenderAssetInstanceView
{
public:
	FRenderAssetInstanceState(bool bForDynamicInstances);

	// Will also remove bounds
	EAddComponentResult AddComponent(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext, float MaxAllowedUIDensity);

	// Similar to AddComponent, but ignore the streaming data bounds. Used for dynamic components. A faster implementation that does less processing.
	EAddComponentResult AddComponentIgnoreBounds(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext);

	FORCEINLINE bool HasComponentReferences(const UPrimitiveComponent* Component) const { return ComponentMap.Contains(Component); }
	void RemoveComponent(const UPrimitiveComponent* Component, FRemovedRenderAssetArray* RemovedTextures);
	bool RemoveComponentReferences(const UPrimitiveComponent* Component);

	void GetReferencedComponents(TArray<const UPrimitiveComponent*>& Components) const;

	void UpdateBounds(const UPrimitiveComponent* Component);
	bool UpdateBounds(int32 BoundIndex);
	bool ConditionalUpdateBounds(int32 BoundIndex);
	void UpdateLastRenderTimeAndMaxDrawDistance(int32 BoundIndex);

	uint32 GetAllocatedSize() const;

	// Generate the compiled elements.
	int32 CompileElements();
	int32 CheckRegistrationAndUnpackBounds(TArray<const UPrimitiveComponent*>& RemovedComponents);

	/** Move around one bound to free the last bound indices. This allows to keep the number of dynamic bounds low. */
	bool MoveBound(int32 SrcBoundIndex, int32 DstBoundIndex);
	void TrimBounds();
	void OffsetBounds(const FVector& Offset);

	FORCEINLINE int32 NumBounds() const { return Bounds4Components.Num(); }
	FORCEINLINE bool HasComponent(int32 BoundIndex) const { return Bounds4Components[BoundIndex] != nullptr; }

private:

	void AddElement(const UPrimitiveComponent* Component, const UStreamableRenderAsset* Asset, int BoundsIndex, float TexelFactor, bool bForceLoad, int32*& ComponentLink, int32 IterationCount_DebuggingOnly);
	// Returns the next elements using the same component.
	void RemoveElement(int32 ElementIndex, int32& NextComponentLink, int32& BoundsIndex, const UStreamableRenderAsset*& Asset, int32 IterationCount_DebuggingOnly);

	int32 AddBounds(const FBoxSphereBounds& Bounds, uint32 PackedRelativeBox, const UPrimitiveComponent* Component, float LastRenderTime, const FVector4& RangeOrigin, float MinDistanceSq, float MinRangeSq, float MaxRangeSq);
	FORCEINLINE int32 AddBounds(const UPrimitiveComponent* Component);
	void RemoveBounds(int32 Index);

	void AddRenderAssetElements(const UPrimitiveComponent* Component, const TArrayView<FStreamingRenderAssetPrimitiveInfo>& RenderAssetInstanceInfos, int32 BoundsIndex, int32*& ComponentLink);

private:

	/** 
	 * Components related to each of the Bounds4 elements. This is stored in another array to allow 
	 * passing Bounds4 to the threaded task without loosing the bound components, allowing incremental update.
	 */
	TArray<const UPrimitiveComponent*> Bounds4Components;

	TArray<int32> FreeBoundIndices;
	TArray<int32> FreeElementIndices;

	/** 
	 * When adding components that are not yet registered, bounds are not yet valid, and must be unpacked after the level becomes visible for the first time.
	 * We keep a list of bound require such unpacking as it would be risky to figure it out from the data itself. Some component data also shouldn't be unpacked
	 * if GetStreamingTextureInfo() returned entries with null PackedRelativeBox.
	 */
	TArray<int32>  BoundsToUnpack;

	TMap<const UPrimitiveComponent*, int32> ComponentMap;

#if DO_CHECK
	bool bIsDynamicInstanceState;
#endif

	friend class FRenderAssetLinkIterator;
	friend class FRenderAssetIterator;
};

template <typename TTasks>
class FRenderAssetInstanceStateTaskSync
{
public:

	FRenderAssetInstanceStateTaskSync(bool bForDynamicInstances) : State(new FRenderAssetInstanceState(bForDynamicInstances)) {}

	FORCEINLINE void Sync()
	{
		Tasks.SyncResults();
	}

	FORCEINLINE FRenderAssetInstanceState* SyncAndGetState()
	{
		Tasks.SyncResults();
		return State.GetReference();
	}

	// Get State but must be constant as async tasks could be reading data.
	FORCEINLINE const FRenderAssetInstanceState* GetState() const
	{
		return State.GetReference();
	}

	// Used when updating the state, but with no possible reallocation.
	FORCEINLINE FRenderAssetInstanceState* GetStateUnsafe()
	{
		return State.GetReference();
	}

	TTasks& GetTasks() { return Tasks; }
	const TTasks& GetTasks() const { return Tasks; }

private:

	TRefCountPtr<FRenderAssetInstanceState> State;
	TTasks Tasks;
};
