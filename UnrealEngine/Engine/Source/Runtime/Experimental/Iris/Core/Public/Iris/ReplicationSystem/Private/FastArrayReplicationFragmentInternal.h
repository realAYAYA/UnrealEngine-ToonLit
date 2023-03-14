// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/ReplicationFragment.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Iris/ReplicationState/PropertyReplicationState.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"

namespace UE {
namespace Net {

namespace FastArrayPollingPolicies {

struct FPollingState
{
	int32 ArrayReplicationKey = -1;
	struct FEntry
	{
		int32 ReplicationKey = -1;
		int32 ReplicationID = -1;
	};
	TArray<FEntry>  ItemPollData;
};

/*
 * FNeedPollingPolicy, used for Native Iris FastArrays that does not need polling
 */
class FNeedPollingPolicy
{
public:
	FPollingState* GetPollingState() { return &PollingState; }	
private:
	FPollingState PollingState;
};

/*
 * FNeedPollingPolicy, used for Native Iris FastArrays that do not require polling
 */
class FNoPollingPolicy
{
public:
	FPollingState* GetPollingState() { return nullptr; }
};

} // End of namespace FastArrayPollingPolicies

namespace Private {

/** Utility methods to behave similar to FastArraySerializer */
struct FFastArrayReplicationFragmentHelper
{
	/** Create descriptor for the FastArray property if it does not already exist */
	IRISCORE_API static TRefCountPtr<const FReplicationStateDescriptor> GetOrCreateDescriptorForFastArrayProperty(UObject* Object, FFragmentRegistrationContext& Context, int32 RepIndex);

	/** Rebuild IndexMap for FastArrraySerializer */
	template <typename FastArrayType, typename ItemArrayType>
	static void ConditionalRebuildItemMap(FastArrayType& ArraySerializer, const ItemArrayType& Items);

	/** Apply received state and try to behave like current FastArrays */
	template <typename FastArrayType, typename ItemArrayType>
	static void ApplyReplicatedState(FastArrayType* DstFastArray, ItemArrayType* DstWrappedArray, FastArrayType* SrcFastArray, const ItemArrayType* SrcWrappedArray, const FReplicationStateDescriptor* ArrayElementDescriptor, FReplicationStateApplyContext& Context);

	/** Copy array element, only replicated items will be copied */
	IRISCORE_API static void InternalCopyArrayElement(const FReplicationStateDescriptor* ArrayElementDescriptor, void* RESTRICT Dst, const void* RESTRICT Src);

	/** Compare array element, only replicated items will be compared */
	IRISCORE_API static bool InternalCompareArrayElement(const FReplicationStateDescriptor* ArrayElementDescriptor, void* RESTRICT Dst, const void* RESTRICT Src);

	/**
	 * Conditionally invoke PostReplicatedReceive method depending on if it is defined or not
	 * We only want to do this for FastArrays that define PostReplicatedReceive since it might require extra work to calculate the required parameters
	 */
	template<typename FastArrayType>
	static inline typename TEnableIf<TModels<FFastArraySerializer::CPostReplicatedReceiveFuncable, FastArrayType, const FFastArraySerializer::FPostReplicatedReceiveParameters&>::Value, void>::Type CallPostReplicatedReceiveOrNot(FastArrayType& ArraySerializer, bool bHasUnresolvedReferences)
	{
		FFastArraySerializer::FPostReplicatedReceiveParameters PostReceivedParameters;
		PostReceivedParameters.bHasMoreUnmappedReferences = bHasUnresolvedReferences;
		ArraySerializer.PostReplicatedReceive(PostReceivedParameters);
	}

	template<typename FastArrayType>
	static inline typename TEnableIf<!TModels<FFastArraySerializer::CPostReplicatedReceiveFuncable, FastArrayType, const FFastArraySerializer::FPostReplicatedReceiveParameters&>::Value, void>::Type CallPostReplicatedReceiveOrNot(FastArrayType& ArraySerializer, bool bHasUnresolvedReferences) {}
};

class FFastArrayReplicationFragmentBase : public FReplicationFragment
{
public:
	IRISCORE_API void Register(FFragmentRegistrationContext& Context, EReplicationFragmentTraits InTraits);

protected:
	IRISCORE_API FFastArrayReplicationFragmentBase(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor);

	IRISCORE_API const FReplicationStateDescriptor* GetFastArraySerializerPropertyDescriptor() const;
	IRISCORE_API const FReplicationStateDescriptor* GetArrayElementDescriptor() const;

	IRISCORE_API virtual void CollectOwner(FReplicationStateOwnerCollector* Owners) const override;
	IRISCORE_API virtual void CallRepNotifies(FReplicationStateApplyContext& Context) override;	

	IRISCORE_API virtual void ReplicatedStateToString(FStringBuilderBase& StringBuilder, FReplicationStateApplyContext& Context, EReplicationStateToStringFlags Flags) const override;

	IRISCORE_API static void InternalCopyArrayElement(const FReplicationStateDescriptor* ArrayElementDescriptor, void* RESTRICT Dst, const void* RESTRICT Src);
	IRISCORE_API static bool InternalCompareArrayElement(const FReplicationStateDescriptor* ArrayElementDescriptor, void* RESTRICT Dst, const void* RESTRICT Src);

protected:
	// Replication descriptor built for the specific property
	TRefCountPtr<const FReplicationStateDescriptor> ReplicationStateDescriptor;

	// This is the source state from which we source our state data
	TUniquePtr<FPropertyReplicationState> SrcReplicationState;

	// Owner
	UObject* Owner;

	// This allows us to quickly find the wrapped array relative to the owner
	SIZE_T WrappedArrayOffsetRelativeFastArraySerializerProperty;
};

// Native FastArray
class FNativeFastArrayReplicationFragmentBase : public FReplicationFragment
{
protected:
	IRISCORE_API FNativeFastArrayReplicationFragmentBase(EReplicationFragmentTraits InTraits, UObject* InOwner, const FReplicationStateDescriptor* InDescriptor);

	IRISCORE_API virtual void CollectOwner(FReplicationStateOwnerCollector* Owners) const override;

	IRISCORE_API const FReplicationStateDescriptor* GetFastArraySerializerPropertyDescriptor() const;
	IRISCORE_API const FReplicationStateDescriptor* GetArrayElementDescriptor() const;

	// Dequantize state into DstExternalBuffer, note it is expected to be initialized
	IRISCORE_API static void InternalDequantizeFastArray(FNetSerializationContext& Context, uint8* RESTRICT DstExternalBuffer, const uint8* RESTRICT SrcInternalBuffer, const FReplicationStateDescriptor* FastArrayPropertyDescriptor);

	IRISCORE_API static void ToString(FStringBuilderBase& StringBuilder, const uint8* ExternalStateBuffer, const FReplicationStateDescriptor* FastArrayPropertyDescriptor);

protected:
	// Replication descriptor built for the specific property
	TRefCountPtr<const FReplicationStateDescriptor> ReplicationStateDescriptor;

	// Owner
	UObject* Owner;

	// This allows us to quickly find the wrapped array relative to the owner
	SIZE_T WrappedArrayOffsetRelativeFastArraySerializerProperty;
};

template <typename FastArrayType, typename ItemArrayType>
void FFastArrayReplicationFragmentHelper::ConditionalRebuildItemMap(FastArrayType& ArraySerializer, const ItemArrayType& Items)
{
	typedef typename ItemArrayType::ElementType ItemType;

	if (ArraySerializer.ItemMap.Num() != Items.Num())
	{
		ArraySerializer.ItemMap.Reset();
			
		const ItemType* SrcItems = Items.GetData();
		for (int32 It = 0, EndIt = Items.Num(); It != EndIt; ++It)
		{
			const ItemType& Item = SrcItems[It];
			if (Item.ReplicationID == INDEX_NONE)
			{
				continue;
			}

			ArraySerializer.ItemMap.Add(Item.ReplicationID, It);
		}
	}
}

template <typename FastArrayType, typename ItemArrayType>
void FFastArrayReplicationFragmentHelper::ApplyReplicatedState(FastArrayType* DstArraySerializer, ItemArrayType* DstWrappedArray, FastArrayType* SrcArraySerializer, const ItemArrayType* SrcWrappedArray, const FReplicationStateDescriptor* ArrayElementDescriptor, FReplicationStateApplyContext& Context)
{
	typedef typename ItemArrayType::ElementType ItemType;

	UE_LOG(LogNetFastTArray, Log, TEXT("FFastArrayReplicationFragmentHelper::ApplyReplicatedState for %s"), Context.Descriptor->DebugName->Name);

	// We need to rebuild our maps for both target array and incoming data
	// Can optimize this later
	ConditionalRebuildItemMap(*DstArraySerializer, *DstWrappedArray);
	ConditionalRebuildItemMap(*SrcArraySerializer, *SrcWrappedArray);

	// Find removed elements in received data, that is elements that exist in old map but not in new map
	TArray<int32> RemovedIndices;
	{
		RemovedIndices.Reserve(DstWrappedArray->Num());
		ItemType* DstItems = DstWrappedArray->GetData();
		for (int32 It=0, EndIt=DstWrappedArray->Num(); It != EndIt; ++It)
		{
			const int32 ReplicationID = DstItems[It].ReplicationID;
			if (ReplicationID != -1 && !SrcArraySerializer->ItemMap.Contains(ReplicationID))
			{
				UE_LOG(LogNetFastTArray, Log, TEXT("   Removed ID: %d local Idx: %d"), ReplicationID, It);
				RemovedIndices.Add(It);

				// Remove callback
				DstItems[It].PreReplicatedRemove(*DstArraySerializer);
			}
		}
	}
	
	const int32 PreRemoveSize = DstWrappedArray->Num();
	const int32 FinalSize = PreRemoveSize - RemovedIndices.Num();

	// Remove callback to FastArraySerializer
	DstArraySerializer->PreReplicatedRemove(MakeArrayView(RemovedIndices), FinalSize);

	// Find new and modified elements in received data, That is elements that do not exist in old map
	TArray<int32> AddedIndices;
	TArray<int32> ModifiedIndices;
	{
		AddedIndices.Reserve(SrcWrappedArray->Num());
		ModifiedIndices.Reserve(SrcWrappedArray->Num());
		const ItemType* SrcItems = SrcWrappedArray->GetData();
		for (int32 It=0, EndIt=SrcWrappedArray->Num(); It != EndIt; ++It)
		{			
			if (int32* ExistingIndex = DstArraySerializer->ItemMap.Find(SrcItems[It].ReplicationID))
			{
				// As we currently always send the full array. To be correct, we must compare the element as well before issuing the callback
				if (!InternalCompareArrayElement(ArrayElementDescriptor, &(*DstWrappedArray)[*ExistingIndex], &SrcItems[It]))
				{
					UE_LOG(LogNetFastTArray, Log, TEXT("   Changed. ID: %d -> Idx: %d"), SrcItems[It].ReplicationID, *ExistingIndex);

					ModifiedIndices.Add(*ExistingIndex);

					// We use per element copy since we do not want to overwrite data that is not replicated
					InternalCopyArrayElement(ArrayElementDescriptor, &(*DstWrappedArray)[*ExistingIndex], &SrcItems[It]);

					// Change callback
					(*DstWrappedArray)[*ExistingIndex].PostReplicatedChange(*DstArraySerializer);
				}
			}
			else
			{
				int32 AddedIndex = DstWrappedArray->Add(SrcItems[It]);

				UE_LOG(LogNetFastTArray, Log, TEXT("   New. ID: %d. New Element! local Idx: %d"), SrcItems[It].ReplicationID, AddedIndex);

				// We need to propagate the ReplicationID in order to find our object
				(*DstWrappedArray)[AddedIndex].ReplicationID = SrcItems[It].ReplicationID;

				// Add callback
				(*DstWrappedArray)[AddedIndex].PostReplicatedAdd(*DstArraySerializer);

				// should we store ids or indices?
				AddedIndices.Add(AddedIndex);
			}
		}	

		// Added and changed callbacks to FastArraySerializer
		DstArraySerializer->PostReplicatedAdd(MakeArrayView(AddedIndices), FinalSize);
		DstArraySerializer->PostReplicatedChange(MakeArrayView(ModifiedIndices), FinalSize);
	}

	// Remove indices
	if (RemovedIndices.Num() > 0)
	{
		RemovedIndices.Sort();
		for (int32 i = RemovedIndices.Num() - 1; i >= 0; --i)
		{
			int32 DeleteIndex = RemovedIndices[i];
			if (DstWrappedArray->IsValidIndex(DeleteIndex))
			{
				DstWrappedArray->RemoveAtSwap(DeleteIndex, 1, false);
			}
		}

		// Clear the map now that the indices are all shifted around. This kind of sucks, we could use slightly better data structures here I think.
		// This will force the ItemMap to be rebuilt for the current Items array
		DstArraySerializer->ItemMap.Empty();
	}

	// Increment keys so that a client can re-serialize the array if needed, such as for client replay recording.
	DstArraySerializer->IncrementArrayReplicationKey();

	// Invoke PostReplicatedReceive if is defined by the serializer
	CallPostReplicatedReceiveOrNot(*DstArraySerializer, Context.bHasUnresolvableReferences);
}

}}} // End of namespaces
