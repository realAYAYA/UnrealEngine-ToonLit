// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/ArrayView.h"
#include "Net/Core/NetBitArray.h"
#include "Net/Core/NetHandle/NetHandle.h"
#include "Iris/Core/NetChunkedArray.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Iris/ReplicationSystem/NetDependencyData.h"
#include "UObject/ObjectPtr.h"
#include "Delegates/Delegate.h"

class FReferenceCollector;
namespace UE::Net
{
	struct FReplicationProtocol;
	struct FReplicationInstanceProtocol;

	namespace Private
	{
		class FReplicationProtocolManager;
	}
}

namespace UE::Net::Private
{

typedef uint32 FInternalNetRefIndex;

enum class EAddSubObjectFlags : uint32
{
	None = 0U,
	WarnIfAlreadySubObject = 1U,
	SkipIfAlreadySubObject = WarnIfAlreadySubObject << 1U,
	DestroyWithOwner = SkipIfAlreadySubObject << 1U,
	ReplicateWithSubObject = DestroyWithOwner << 1U,
	Default = WarnIfAlreadySubObject | DestroyWithOwner,
};
ENUM_CLASS_FLAGS(EAddSubObjectFlags);

enum class EAddDependentObjectFlags : uint32
{
	None = 0U,
	WarnIfAlreadyDependentObject = 1U,
};
ENUM_CLASS_FLAGS(EAddDependentObjectFlags);

enum class ERemoveDependentObjectFlags : uint32
{
	None = 0U,
	RemoveFromDependentParentObjects = 1U,
	RemoveFromParentDependentObjects = RemoveFromDependentParentObjects << 1U,
	All = RemoveFromDependentParentObjects | RemoveFromParentDependentObjects,
};
ENUM_CLASS_FLAGS(ERemoveDependentObjectFlags);

// Internal class to manage NetHandles and their internal data
class FNetRefHandleManager
{
public:
	// We will never assign InvalidInternalIndex
	enum { InvalidInternalIndex = 0U };

	// We need to store some internal data for Replicated objects
	// $TODO: This should be split up into separate array according to usage patterns, it is getting a bit too large
	struct FReplicatedObjectData
	{
		FReplicatedObjectData()
		: Protocol(nullptr)
		, InstanceProtocol(nullptr)
		, ReceiveStateBuffer(nullptr)
		, SubObjectRootIndex(InvalidInternalIndex)
		, SubObjectParentIndex(InvalidInternalIndex)
		, Flags(0U)
		{
		}

		FNetRefHandle RefHandle;
		FNetHandle NetHandle;
		const FReplicationProtocol* Protocol;
		const FReplicationInstanceProtocol* InstanceProtocol;
		uint8* ReceiveStateBuffer;
		/** Subobjects only: Internal index of the RootObject of this subobject */
		FInternalNetRefIndex SubObjectRootIndex;
		/** Subobjects only: Internal index of the ParentObject of this subobject */
		FInternalNetRefIndex SubObjectParentIndex;

		union
		{
			uint32 Flags : 32U;
			struct
			{
				uint32 bShouldPropagateChangedStates : 1U;
				uint32 bTearOff : 1U;
				uint32 bDestroySubObjectWithOwner : 1U;
				uint32 bIsDependentObject : 1U;
				uint32 bHasDependentObjects : 1U;
				uint32 bAllowDestroyInstanceFromRemote : 1U;
				uint32 bNeedsFullCopyAndQuantize : 1U;
				uint32 bWantsFullPoll : 1U;
				uint32 bPendingEndReplication : 1U;
			};
		};
	
		// Returns true if this is a SubObject
		bool IsSubObject() const { return SubObjectRootIndex && !!bDestroySubObjectWithOwner; }

		// Returns true if this is a DependentObject
		bool IsDependentObject() const { return bIsDependentObject; }
	};

	// We want to handle both actual Networked and "network addressable" handles in the same way
	// As we expect the number of FNetRefHandles to be much greater than the actual number of Replicated Objects we
	// use a map to map from Handle to InternalIndex, only Replicated Objects will ever be assigned an InternalNetHandle
	typedef TMap<FNetRefHandle, FInternalNetRefIndex> FRefHandleMap;
	typedef TMap<FNetHandle, FInternalNetRefIndex> FNetHandleMap;

public:
	FNetRefHandleManager(FReplicationProtocolManager& InReplicationProtocolManager, uint32 InReplicationSystemId, uint32 InMaxActiveObjectCount, uint32 InPreAllocatedObjectCount);

	/** Callback triggered at the beginning of PreSendUpdate. Used to sync current frame data. */
	void OnPreSendUpdate();

	/** Callback triggered at the end of SendUpdate. Used to clear current frame data. */
	void OnPostSendUpdate();

	// Return true if this is a scopable index
	bool IsScopableIndex(FInternalNetRefIndex InternalIndex) const { return GlobalScopableInternalIndices.GetBit(InternalIndex); }

	static FNetRefHandle MakeNetRefHandle(uint64 Id, uint32 ReplicationSystemId);
	static FNetRefHandle MakeNetRefHandleFromId(uint64 Id);

	// Returns a valid handle if the wanted handle can be allocated
	FNetRefHandle AllocateNetRefHandle(bool bIsStatic);

	// Create local Net Object
	FNetRefHandle CreateNetObject(FNetRefHandle WantedHandle, FNetHandle GlobalHandle, const FReplicationProtocol* ReplicationProtocol);

	// Create NetObject on request from remote
	FNetRefHandle CreateNetObjectFromRemote(FNetRefHandle WantedHandle, const FReplicationProtocol* ReplicationProtocol);

	// Attach Instance protocol to handle
	// Instance can be null, we only track the Instance for legacy support
	void AttachInstanceProtocol(FInternalNetRefIndex InternalIndex, const FReplicationInstanceProtocol* InstanceProtocol, UObject* Instance);

	// Used when we want to detach the instance protocol due to async destroy
	const FReplicationInstanceProtocol* DetachInstanceProtocol(FInternalNetRefIndex InternalIndex);

	// Creates a DestructionInfo Handle which is used to replicate persistently destroyed static objects when late joining or streaming in levels with static objects that already has been destroyed
	// Returns a new handle used to replicate the destruction info, and adds a cross-reference to the original handle being destroyed if that still exists 
	// Handle is a reference to an already replicated Handle that is about to be destroyed
	FNetRefHandle CreateHandleForDestructionInfo(FNetRefHandle Handle, const FReplicationProtocol* DestroydObjectProtocol);
	
	void DestroyNetObject(FNetRefHandle Handle);

	// Mark object as no longer scopable, object will be removed from scope for all connections
	void RemoveFromScope(FInternalNetRefIndex InternalIndex);

	TArrayView<const FInternalNetRefIndex> GetObjectsPendingDestroy() const { return MakeArrayView(PendingDestroyInternalIndices); }
	void DestroyObjectsPendingDestroy();

	const FReplicatedObjectData& GetReplicatedObjectDataNoCheck(FInternalNetRefIndex InternalIndex) const { return ReplicatedObjectData[InternalIndex]; }
	FReplicatedObjectData& GetReplicatedObjectDataNoCheck(FInternalNetRefIndex InternalIndex) { return ReplicatedObjectData[InternalIndex]; }

	inline const FReplicatedObjectData& GetReplicatedObjectData(FInternalNetRefIndex InternalIndex) const;

	inline const uint8* GetReplicatedObjectStateBufferNoCheck(FInternalNetRefIndex InternalObjectIndex) const { return ReplicatedObjectStateBuffers[InternalObjectIndex]; }
	inline uint8* GetReplicatedObjectStateBufferNoCheck(FInternalNetRefIndex InternalObjectIndex) { return ReplicatedObjectStateBuffers[InternalObjectIndex]; }
	inline const TNetChunkedArray<uint8*>& GetReplicatedObjectStateBuffers() const { return ReplicatedObjectStateBuffers; }

	/** Verify a handle against internal handle. A handle is valid if it matches internal storage. */
	inline bool IsValidNetRefHandle(FNetRefHandle Handle) const;

	/** Returns true if the InternaIndex belongs to a replicated object owned by the local peer. */
	inline bool IsLocal(FInternalNetRefIndex InternalIndex) const;

	/** Returns true if the handle belongs to a replicated object owned by the local peer. */
	inline bool IsLocalNetRefHandle(FNetRefHandle Handle) const;

	/** Returns true if the handle is for a remote replicated object. */
	inline bool IsRemoteNetRefHandle(FNetRefHandle Handle) const;

	/** Extract a full handle from an incomplete one consisting of only an index. */
	inline FNetRefHandle GetCompleteNetRefHandle(FNetRefHandle IncompleteHandle) const;
	
	// Get Handle from internal index
	inline FNetRefHandle GetNetRefHandleFromInternalIndex(FInternalNetRefIndex InternalIndex) const;

	// Get internal index from handle
	inline FInternalNetRefIndex GetInternalIndex(FNetRefHandle Handle) const;

	// Get internal index from NetHandle
	inline FInternalNetRefIndex GetInternalIndexFromNetHandle(FNetHandle Handle) const;

	/** All scopable internal indices of the ReplicationSystem. Always up to date but should mostly be accessed in operations executed outside PreSendUpdate. */
	const FNetBitArrayView GetGlobalScopableInternalIndices() const { return MakeNetBitArrayView(GlobalScopableInternalIndices); }

	/** All scopable internal indices of the current frame at the start of PreSendUpdate. Only accessible during that operation. */
	const FNetBitArrayView GetCurrentFrameScopableInternalIndices() const { check(ScopeFrameData.bIsValid); return MakeNetBitArrayView(ScopeFrameData.CurrentFrameScopableInternalIndices); }

	/** All scopable internal indices of the previous PreSendUpdate. Only accessible during that operation */
	const FNetBitArrayView GetPrevFrameScopableInternalIndices() const { check(ScopeFrameData.bIsValid); return MakeNetBitArrayView(ScopeFrameData.PrevFrameScopableInternalIndices); }

	/** List of objects that are always relevant or currently relevant to at least one connection. */
	FNetBitArrayView GetRelevantObjectsInternalIndices() const { return MakeNetBitArrayView(RelevantObjectsInternalIndices); }

	/** List of objects that we polled this frame */
	FNetBitArrayView GetPolledObjectsInternalIndices() const { return MakeNetBitArrayView(PolledObjectsInternalIndices); }

	/** List of objects that have dirty state data that needs to be quantized */
	FNetBitArrayView GetDirtyObjectsToQuantize() const { return MakeNetBitArrayView(DirtyObjectsToQuantize); }

	// Get bitarray for all internal indices that currently are assigned
	const FNetBitArray& GetAssignedInternalIndices() const { return AssignedInternalIndices; }

	// SubObjects
	const FNetBitArray& GetSubObjectInternalIndices() const { return SubObjectInternalIndices; }
	const FNetBitArrayView GetSubObjectInternalIndicesView() const { return MakeNetBitArrayView(SubObjectInternalIndices); }

	bool AddSubObject(FNetRefHandle OwnerHandle, FNetRefHandle SubObjectHandle, FNetRefHandle RelativeOtherSubObjectHandle, EAddSubObjectFlags Flags = EAddSubObjectFlags::Default);
	bool AddSubObject(FNetRefHandle OwnerHandle, FNetRefHandle SubObjectHandle, EAddSubObjectFlags Flags = EAddSubObjectFlags::Default);
	void RemoveSubObject(FNetRefHandle SubObjectHandle);
	
	FNetRefHandle GetRootObjectOfSubObject(FNetRefHandle SubObjectHandle) const;
	FInternalNetRefIndex GetRootObjectInternalIndexOfSubObject(FInternalNetRefIndex SubObjectIndex) const;

	bool SetSubObjectNetCondition(FInternalNetRefIndex SubObjectInternalIndex, FLifeTimeConditionStorage SubObjectCondition);

	// DependentObjects
	const FNetBitArray& GetDependentObjectInternalIndices() const { return DependentObjectInternalIndices; }
	const FNetBitArray& GetObjectsWithDependentObjectsInternalIndices() const { return ObjectsWithDependentObjectsInternalIndices; }
	bool AddDependentObject(FNetRefHandle ParentHandle, FNetRefHandle DependentHandle, EDependentObjectSchedulingHint SchedulingHint, EAddDependentObjectFlags Flags = EAddDependentObjectFlags::WarnIfAlreadyDependentObject);
	void RemoveDependentObject(FNetRefHandle ParentHandle, FNetRefHandle DependentHandle);

	// Remove DependentHandles from all dependent object tracking
	// $TODO: This might need a pass ObjectReplicationBridge as well since most likely also must restore filter/polling logic
	void RemoveDependentObject(FNetRefHandle DependentHandle);
	
	void SetShouldPropagateChangedStates(FNetRefHandle Handle, bool bShouldPropagateChangedStates);
	void SetShouldPropagateChangedStates(FInternalNetRefIndex ObjectInternalIndex, bool bShouldPropagateChangedStates);

	uint32 GetMaxActiveObjectCount() const { return MaxActiveObjectCount; }
	uint32 GetActiveObjectCount() const { return ActiveObjectCount; }
	uint32 GetPreAllocatedObjectCount() const { return PreAllocatedObjectCount; }

	// We do refcount objects tracked by each connection in order to know when it is safe to reuse an InternalIndex
	void AddNetObjectRef(FInternalNetRefIndex InternalIndex) { ++ReplicatedObjectRefCount[InternalIndex]; }
	void ReleaseNetObjectRef(FInternalNetRefIndex InternalIndex) { check(ReplicatedObjectRefCount[InternalIndex] > 0); --ReplicatedObjectRefCount[InternalIndex]; }
	uint16 GetNetObjectRefCount(FInternalNetRefIndex ObjectInternalIndex) const { return ReplicatedObjectRefCount[ObjectInternalIndex]; }

	// Get dependent objects for the given ParentIndex
	TArrayView<const FDependentObjectInfo> GetDependentObjectInfos(FInternalNetRefIndex ParentIndex) const { return SubObjects.GetDependentObjectInfoArray(ParentIndex); }

	// Get all parents of the given DependentIndex
	inline TArrayView<const FInternalNetRefIndex> GetDependentObjectParents(FInternalNetRefIndex DependentIndex) const;

	// Get all subobjects for the given OwnerIndex, Note: only valid for the root
	inline TArrayView<const FInternalNetRefIndex> GetSubObjects(FInternalNetRefIndex OwnerIndex) const;

	// Get child subobjects for a object, used when we do hierarchical operations such as conditional serialization
	inline TArrayView<const FInternalNetRefIndex> GetChildSubObjects(FInternalNetRefIndex ParentObjectIndex) const;

	// Get child subobjects and condtionals array if one exists, if there are no child subobjects the method returns false
	inline bool GetChildSubObjects(FInternalNetRefIndex OwnerIndex, FChildSubObjectsInfo& OutInfo) const;

	/** Get the map that translates RefHandles into InternalIndexes */
	const FRefHandleMap& GetReplicatedHandles() const { return RefHandleToInternalIndex; }

	// Get the replicated object represented by a given internal index.
	UObject* GetReplicatedObjectInstance(FInternalNetRefIndex ObjectIndex) const { return ReplicatedInstances[ObjectIndex]; }

	/** Get the array of all held object pointers */
	const TNetChunkedArray<TObjectPtr<UObject>>& GetReplicatedInstances() const { return ReplicatedInstances; }

	void AddReferencedObjects(FReferenceCollector& Collector);

	bool GetIsDestroyedStartupObject(FInternalNetRefIndex InternalIndex) const { return DestroyedStartupObjectInternalIndices.GetBit(InternalIndex); }
	inline uint32 GetOriginalDestroyedStartupObjectIndex(FInternalNetRefIndex InternalIndex) const;

	const FNetBitArray& GetDestroyedStartupObjectInternalIndices() const { return DestroyedStartupObjectInternalIndices; }

	/** List of replicated objects that want to be dormant */
	const FNetBitArray& GetWantToBeDormantInternalIndices() const { return WantToBeDormantInternalIndices; }
	FNetBitArray& GetWantToBeDormantInternalIndices() { return WantToBeDormantInternalIndices; }

	/** Return a string to identify the object linked to an index in logs */
	FString PrintObjectFromIndex(FInternalNetRefIndex ObjectIndex) const;
	FString PrintObjectFromNetRefHandle(FNetRefHandle ObjectHandle) const { return PrintObjectFromIndex(GetInternalIndex(ObjectHandle)); }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLargestIndexIncrease, uint32 LargestIndex);

	/** Return a delegate that will notify when the largest internal index has increased. */
	FOnLargestIndexIncrease& GetLargestIndexIncreaseDelegate() { return OnLargestIndexIncreaseDelegate; };

	/** Return the largest internal index that has been used. */
	uint32 GetLargestInternalIndex() const { return LargestInternalIndex; };

public:

	// Iterate over all dependent objects and their dependent objects
	template <typename T>
	void ForAllDependentObjectsRecursive(FInternalNetRefIndex ObjectIndex, T&& Functor) const
	{
		if (ObjectsWithDependentObjectsInternalIndices.GetBit(ObjectIndex))
		{
			for (const FDependentObjectInfo& DependentObjectInfo : GetDependentObjectInfos(ObjectIndex))
			{
				Functor(DependentObjectInfo.NetRefIndex);
				ForAllDependentObjectsRecursive(DependentObjectInfo.NetRefIndex, Functor);
			}
			return;
		}
	};

private:
	FInternalNetRefIndex InternalCreateNetObject(const FNetRefHandle NetRefHandle, const FNetHandle GlobalHandle, const FReplicationProtocol* ReplicationProtocol);
	void InternalDestroyNetObject(FInternalNetRefIndex InternalIndex);

	static uint64 MakeNetRefHandleId(uint64 Seed, bool bIsStatic);
	uint64 GetNextNetRefHandleId(uint64 HandleIndex) const;

	// Get the next free internal index, returns InvalidInternalIndex if a free one cannot be found
	FInternalNetRefIndex GetNextFreeInternalIndex() const;

	bool InternalAddSubObject(FInternalNetRefIndex OwnerInternalIndex, FInternalNetRefIndex SubObjectInternalIndex, FInternalNetRefIndex RelativeOtherSubObjectInternalIndex, EAddSubObjectFlags Flags);

	inline void SetIsSubObject(FInternalNetRefIndex InternalIndex, bool IsSubObject) { SubObjectInternalIndices.SetBitValue(InternalIndex, IsSubObject); }
	void InternalRemoveSubObject(FInternalNetRefIndex OwnerInternalIndex, FInternalNetRefIndex SubObjectInternalIndex, bool bRemoveFromSubObjectArray = true);

	void InternalRemoveDependentObject(FInternalNetRefIndex ParentInternalIndex, FInternalNetRefIndex DependentInternalIndex, ERemoveDependentObjectFlags Flags = ERemoveDependentObjectFlags::All);
	void InternalRemoveDependentObject(FInternalNetRefIndex DependentInternalIndex);

	// Ensure that all buffers that depend on the largest internal index are sized correctly.
	void GrowBuffersToLargestIndex(uint32 InternalIndex);

	// The current replicated object count
	uint32 ActiveObjectCount;

	// Max allowed replicated object count
	uint32 MaxActiveObjectCount;

	// The number of pre-allocated objects used by internal buffers.
	uint32 PreAllocatedObjectCount;

	// The largest internal index value that has been used.
	uint32 LargestInternalIndex;

	// A delegate that is triggered when the largest encountered internal index increases.
	FOnLargestIndexIncrease OnLargestIndexIncreaseDelegate;

	uint32 ReplicationSystemId;

	FRefHandleMap RefHandleToInternalIndex;
	FNetHandleMap NetHandleToInternalIndex;

	struct FScopeFrameData
	{
		FScopeFrameData(uint32 InMaxActiveObjectCount)
			: bIsValid(false)
			, CurrentFrameScopableInternalIndices(InMaxActiveObjectCount)
			, PrevFrameScopableInternalIndices(InMaxActiveObjectCount)
			
		{ }

		// Controls if the frame data can be read or not
		uint32 bIsValid : 1;

		// Bitset used in order to track assigned internal which are scopable
		FNetBitArray CurrentFrameScopableInternalIndices;

		// Which internal indices were used last net frame. This can be used to find out which ones are new and deleted this frame. 
		FNetBitArray PrevFrameScopableInternalIndices;
	};

	/** Bitset used in order to track assigned internal which are scopable */
	FNetBitArray GlobalScopableInternalIndices;

	/** Stores scope lists relevant to the current frame. Only valid during SendUpdate(). */
	FScopeFrameData ScopeFrameData;

	/** This contains the ScopableInternalIndices list minus filtered objects that are not relevant to any connection this frame. */
	FNetBitArray RelevantObjectsInternalIndices;

	/** List of objects that we polled this frame */
	FNetBitArray PolledObjectsInternalIndices;

	/** List of the objects that are considered dirty and for whom we will quantize their state data */
	FNetBitArray DirtyObjectsToQuantize;

	// Bitset containing all internal indices that are assigned
	FNetBitArray AssignedInternalIndices;

	// Bitset containing all internal indices that are SubObjects
	FNetBitArray SubObjectInternalIndices;

	// Bitset containing all internal indices that are dependentObjects
	FNetBitArray DependentObjectInternalIndices;

	// Bitset containing all internal indices that have dependentObjects
	FNetBitArray ObjectsWithDependentObjectsInternalIndices;

	// Bitset marking internal indices of static objects that are permanently destroyed
	// We use this for special scoping
	FNetBitArray DestroyedStartupObjectInternalIndices;

	// Bitset marking internal indices that wants to be dormant
	FNetBitArray WantToBeDormantInternalIndices;

	// Both are marked in DestroyedStartupObjectsInternalIndicies
	// When one is destroyed both mapping will be cleared
	// Map OriginalIndex -> DestructionInfoIndex
	// Map DestructionInfoIndex -> Orignal
	TMap<uint32, uint32> DestroyedStartupObject;

private:

	// Array used in order to track objects pending destroy
	TArray<FInternalNetRefIndex> PendingDestroyInternalIndices;

	// Just an array containing data about our replicated objects
	TNetChunkedArray<FReplicatedObjectData> ReplicatedObjectData;

	// Pointers to state buffers for all replicated objects
	TNetChunkedArray<uint8*> ReplicatedObjectStateBuffers;

	// Refcounts for all tracked objects
	TNetChunkedArray<uint16> ReplicatedObjectRefCount;

	// Raw pointers to all bound instances
	TNetChunkedArray<TObjectPtr<UObject>> ReplicatedInstances;

	// Assign handles
	uint64 NextStaticHandleIndex;
	uint64 NextDynamicHandleIndex;

	FNetDependencyData SubObjects;

	FReplicationProtocolManager& ReplicationProtocolManager;
};

const FNetRefHandleManager::FReplicatedObjectData& FNetRefHandleManager::GetReplicatedObjectData(FInternalNetRefIndex InternalIndex) const
{
	check(AssignedInternalIndices.GetBit(InternalIndex));
	return GetReplicatedObjectDataNoCheck(InternalIndex);
}

FInternalNetRefIndex FNetRefHandleManager::GetInternalIndex(FNetRefHandle Handle) const
{
	const FInternalNetRefIndex* InternalIndex = RefHandleToInternalIndex.Find(Handle);
	return InternalIndex ? *InternalIndex : FNetRefHandleManager::InvalidInternalIndex;
}

FInternalNetRefIndex FNetRefHandleManager::GetInternalIndexFromNetHandle(FNetHandle Handle) const
{
	const FInternalNetRefIndex* InternalIndex = NetHandleToInternalIndex.Find(Handle);
	return InternalIndex ? *InternalIndex : FNetRefHandleManager::InvalidInternalIndex;
}

FNetRefHandle FNetRefHandleManager::GetNetRefHandleFromInternalIndex(FInternalNetRefIndex InternalIndex) const
{
	check(AssignedInternalIndices.GetBit(InternalIndex));
	return GetReplicatedObjectDataNoCheck(InternalIndex).RefHandle;
}

FNetRefHandle FNetRefHandleManager::GetCompleteNetRefHandle(FNetRefHandle IncompleteHandle) const
{
	if (const FInternalNetRefIndex* InternalIndex = RefHandleToInternalIndex.Find(IncompleteHandle))
	{
		return GetReplicatedObjectDataNoCheck(*InternalIndex).RefHandle;
	}
	else
	{
		return FNetRefHandle::GetInvalid();
	}
}

bool FNetRefHandleManager::IsValidNetRefHandle(FNetRefHandle Handle) const
{
	return RefHandleToInternalIndex.Contains(Handle);
}

bool FNetRefHandleManager::IsLocal(FInternalNetRefIndex InternalIndex) const
{
	return (InternalIndex != InvalidInternalIndex) && (ReplicatedObjectStateBuffers[InternalIndex] != nullptr);
}

bool FNetRefHandleManager::IsLocalNetRefHandle(FNetRefHandle Handle) const
{
	return IsLocal(GetInternalIndex(Handle));
}

bool FNetRefHandleManager::IsRemoteNetRefHandle(FNetRefHandle Handle) const
{
	if (const uint32 InternalIndex = GetInternalIndex(Handle))
	{
		// For the time being only replicated objects owned by this peer has a state buffer
		return ReplicatedObjectStateBuffers[InternalIndex] == nullptr;
	}

	return false;
}

uint32 FNetRefHandleManager::GetOriginalDestroyedStartupObjectIndex(FInternalNetRefIndex InternalIndex) const
{
	const uint32* FoundOriginalInternalIndex = DestroyedStartupObject.Find(InternalIndex);
	return FoundOriginalInternalIndex ? *FoundOriginalInternalIndex : 0U;
}

inline bool FNetRefHandleManager::AddSubObject(FNetRefHandle OwnerHandle, FNetRefHandle SubObjectHandle, EAddSubObjectFlags Flags)
{
	return AddSubObject(OwnerHandle, SubObjectHandle, FNetRefHandle::GetInvalid(), Flags);
}

TArrayView<const FInternalNetRefIndex> FNetRefHandleManager::GetSubObjects(FInternalNetRefIndex OwnerIndex) const
{
	return SubObjects.GetInternalIndexArray<FNetDependencyData::EArrayType::SubObjects>(OwnerIndex);
}

bool FNetRefHandleManager::GetChildSubObjects(FInternalNetRefIndex OwnerIndex, FChildSubObjectsInfo& OutInfo) const
{
	return SubObjects.GetChildSubObjects(OwnerIndex, OutInfo);
}

TArrayView<const FInternalNetRefIndex> FNetRefHandleManager::GetChildSubObjects(FInternalNetRefIndex OwnerIndex) const
{
	return SubObjects.GetInternalIndexArray<FNetDependencyData::EArrayType::ChildSubObjects>(OwnerIndex);
}

TArrayView<const FInternalNetRefIndex> FNetRefHandleManager::GetDependentObjectParents(FInternalNetRefIndex DependentIndex) const
{
	return SubObjects.GetInternalIndexArray<FNetDependencyData::EArrayType::DependentParentObjects>(DependentIndex);
}
	
}
