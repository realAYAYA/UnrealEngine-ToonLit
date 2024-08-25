// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstanceDataSceneProxy.h"
#include "Containers/StridedView.h"
#include "InstanceAttributeTracker.h"

class HHitProxy;
class FOpaqueHitProxyContainer;

/**
 * Helper to make it possible to use the same paths for scattering through the remap...
 */
class FIdentityDeltaRange
{
public:
	FIdentityDeltaRange(int32 InNum) : NumItems(InNum) {}

	/**
		*/
	inline bool IsEmpty() const { return NumItems == 0; }

	/**
		*/
	inline bool IsDelta() const { return false; }

	/**
		* Returns the number of items in this range - i.e., the number of items that need to be copied to collect an update.
		*/
	inline int32 GetNumItems() const { return NumItems; }

	/**
		*/
	struct FConstIterator
	{
		int32 ItemIndex = 0;
		int32 MaxNum = 0;

		FConstIterator(int32 InIndex, int32 InMaxNum)
		:	ItemIndex(InIndex),
			MaxNum(InMaxNum)
		{
		}

		void operator++() {  ++ItemIndex; }

		/**
			* Get the index of the data in the source / destination arrays. 
			*/
		int32 GetIndex() const { return ItemIndex; }

		/**
			* Get the continuous index of the data item in the collected item array.
			*/
		int32 GetItemIndex() const { return ItemIndex; }

		explicit operator bool() const {  return ItemIndex < MaxNum; }
	};

	FConstIterator GetIterator() const
	{
		return FConstIterator(0, NumItems);
	}

private:
	int32 NumItems = 0;
};

class FISMInstanceUpdateChangeSet
{
public:
	FISMInstanceUpdateChangeSet(bool bInNeedFullUpdate, FInstanceAttributeTracker &&InInstanceAttributeTracker) 
		: InstanceAttributeTracker(MoveTemp(InInstanceAttributeTracker))
		, bNeedFullUpdate(bInNeedFullUpdate) 
	{
	}
#if WITH_EDITOR
	// Set editor data 
	void SetEditorData(const TArray<TRefCountPtr<HHitProxy>>& HitProxies, const TBitArray<> &SelectedInstances);//, bool bWasHitProxiesReallocated);
#endif

	template <FInstanceAttributeTracker::EFlag Flag>
	FInstanceAttributeTracker::FDeltaRange<Flag> GetDelta(bool bForceEmpty, bool bForceFull = false) const 
	{ 
		if (bForceEmpty)
		{
			return FInstanceAttributeTracker::FDeltaRange<Flag>();
		}
		return InstanceAttributeTracker.GetDeltaRange<Flag>(bNeedFullUpdate || bForceFull, NumSourceInstances); 
	}


	FInstanceAttributeTracker::FDeltaRange<FInstanceAttributeTracker::EFlag::TransformChanged> GetTransformDelta() const 
	{ 
		return GetDelta<FInstanceAttributeTracker::EFlag::TransformChanged>(false, bUpdateAllInstanceTransforms);
	}

	FInstanceAttributeTracker::FDeltaRange<FInstanceAttributeTracker::EFlag::CustomDataChanged> GetCustomDataDelta() const 
	{ 
		// Force empty range if no custom data
		return GetDelta<FInstanceAttributeTracker::EFlag::CustomDataChanged>(NumCustomDataFloats == 0 || !Flags.bHasPerInstanceCustomData);
	}

	FInstanceAttributeTracker::FDeltaRange<FInstanceAttributeTracker::EFlag::IndexChanged> GetIndexChangedDelta() const 
	{ 
		return GetDelta<FInstanceAttributeTracker::EFlag::IndexChanged>(false);
	}

	FIdentityDeltaRange GetInstanceLightShadowUVBiasDelta() const
	{
		return FIdentityDeltaRange(InstanceLightShadowUVBias.Num());
	}

#if WITH_EDITOR
	FIdentityDeltaRange GetInstanceEditorDataDelta() const
	{
		return FIdentityDeltaRange(InstanceEditorData.Num());
	}
#endif

	TFunction<void(TArray<float> &InstanceRandomIDs)> GeneratePerInstanceRandomIds;

	/**
	 * Add a value, must be done in the order represented in the InstanceLightShadowUVBiasDelta.
	 */
	inline void AddInstanceLightShadowUVBias(const FVector4f &Value)
	{
		InstanceLightShadowUVBias.Emplace(Value); 
	}

	ENGINE_API void SetInstanceTransforms(TStridedView<FMatrix> InInstanceTransforms, const FVector Offset);
	ENGINE_API void SetInstanceTransforms(TStridedView<FMatrix> InInstanceTransforms);

	/**
	 * This version produces the bounds of the gathered transforms as a side-effect.
	 */
	ENGINE_API void SetInstanceTransforms(TStridedView<FMatrix> InInstanceTransforms, FBox const& InInstanceBounds, FBox& OutGatheredBounds);

	ENGINE_API void SetInstancePrevTransforms(TArrayView<FMatrix> InPrevInstanceTransforms, const FVector &Offset);
	ENGINE_API void SetInstancePrevTransforms(TArrayView<FMatrix> InPrevInstanceTransforms);
	ENGINE_API void SetCustomData(const TArrayView<const float> &InPerInstanceCustomData, int32 InNumCustomDataFloats);
	ENGINE_API void SetInstanceLocalBounds(const FRenderBounds &Bounds);

	bool IsFullUpdate() const
	{
		return bNeedFullUpdate;
	}

	FInstanceAttributeTracker InstanceAttributeTracker;
	bool bNeedFullUpdate = false;
	bool bUpdateAllInstanceTransforms = false;
	
	bool bNumCustomDataChanged = false;
	bool bBakedLightingDataChanged = false;
	bool bAnyEditorDataChanged = false;

	bool bIdentityIdMap = false;
	TArray<FPrimitiveInstanceId> IndexToIdMapDeltaData;
	int32 NumCustomDataFloats = 0;

	TArray<FRenderTransform> Transforms;
	TArray<FRenderTransform> PrevTransforms;

	TArray<float> PerInstanceCustomData;

	TArray<FVector4f> InstanceLightShadowUVBias;
	FInstanceDataFlags Flags;
#if WITH_EDITOR
	TArray<uint32> InstanceEditorData;
	TBitArray<> SelectedInstances;
	TPimplPtr<FOpaqueHitProxyContainer> HitProxyContainer;
#endif
	TArray<FRenderBounds, TInlineAllocator<1>> InstanceLocalBounds;
	TArray<int32> LegacyInstanceReorderTable;
	FRenderTransform PrimitiveToRelativeWorld;
	FVector PrimitiveWorldSpaceOffset;
	TOptional<FRenderTransform> PreviousPrimitiveToRelativeWorld;
	float AbsMaxDisplacement = 0.0f;
	int32 NumSourceInstances = 0;
	int32 PostUpdateNumInstances = 0;
	int32 MaxInstanceId = 0;
};


#if WITH_EDITOR

ENGINE_API TPimplPtr<FOpaqueHitProxyContainer> MakeOpaqueHitProxyContainer(const TArray<TRefCountPtr<HHitProxy>>& InHitProxies);


#endif
