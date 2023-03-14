// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/ArrayView.h"
#include "Net/Core/NetBitArray.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationSystem/NetHandle.h"
#include "Iris/ReplicationSystem/NetDependencyData.h"

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

typedef uint32 FInternalNetHandle;

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
class FNetHandleManager
{
public:
	// We will never assign index 0
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

		FNetHandle Handle;
		const FReplicationProtocol* Protocol;
		const FReplicationInstanceProtocol* InstanceProtocol;
		uint8* ReceiveStateBuffer;
		uint32 SubObjectRootIndex;
		uint32 SubObjectParentIndex;

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
				uint32 Padding : 27U;
			};
		};
	
		// Returns true if this is a SubObject
		bool IsSubObject() const { return SubObjectRootIndex && !!bDestroySubObjectWithOwner; }

		// Returns true if this is a DepedentObject
		bool IsDependentObject() const { return bIsDependentObject; }
	};

	// We want to handle both actual Networked and "network addressable" handles in the same way
	// As we expect the number of FNetHandles to be much greater than the actual number of Replicated Objects we
	// use a map to map from Handle to InternalIndex, only Replicated Objects will ever be assigned an InternalNetHandle
	typedef TMap<FNetHandle, FInternalNetHandle> FHandleMap;

public:
	FNetHandleManager(FReplicationProtocolManager& InReplicationProtocolManager, uint32 InReplicationSystemId, uint32 MaxActiveObjects);

	// Return true if this is a scopable index
	bool IsScopableIndex(FInternalNetHandle InternalIndex) const { return ScopableInternalIndices.GetBit(InternalIndex); }

	// Return true if this index is assigned
	bool IsAssignedIndex(FInternalNetHandle InternalIndex) const { return InternalIndex && ScopableInternalIndices.GetBit(InternalIndex); }

	static FNetHandle MakeNetHandle(uint32 Id, uint32 ReplicationSystemId);
	static FNetHandle MakeNetHandleFromId(uint32 Id);

	// Returns a valid handle if the wanted handle can be allocated
	FNetHandle AllocateNetHandle(bool bIsStatic);

	// Create local Net Object
	FNetHandle CreateNetObject(FNetHandle WantedHandle, const FReplicationProtocol* ReplicationProtocol);

	// Create NetObject on request from remote
	FNetHandle CreateNetObjectFromRemote(FNetHandle WantedHandle, const FReplicationProtocol* ReplicationProtocol);

	// Attach Instance protocol to handle
	// Instance can be null, we only track the Instance for legacy support
	void AttachInstanceProtocol(FInternalNetHandle InternalIndex, const FReplicationInstanceProtocol* InstanceProtocol, UObject* Instance);

	// Used when we want to detach the instance protocol due to async destroy
	const FReplicationInstanceProtocol* DetachInstanceProtocol(FInternalNetHandle InternalIndex);

	// Creates a DestructionInfo Handle which is used to replicate persistently destroyed static objects when late joining or streaming in levels with static objects that already has been destroyed
	// Returns a new handle used to replicate the destruction info, and adds a cross-reference to the original handle being destroyed if that still exists 
	// Handle is a reference to an already replicated Handle that is about to be destroyed
	FNetHandle CreateHandleForDestructionInfo(FNetHandle Handle, const FReplicationProtocol* DestroydObjectProtocol);
	
	void DestroyNetObject(FNetHandle Handle);

	// Mark object as no longer scopable, object will be removed from scope for all connections
	void RemoveFromScope(FInternalNetHandle InternalIndex);

	TArrayView<const FInternalNetHandle> GetObjectsPendingDestroy() const { return MakeArrayView(PendingDestroyInternalIndices); }
	void DestroyObjectsPendingDestroy();

	const FReplicatedObjectData& GetReplicatedObjectDataNoCheck(FInternalNetHandle InternalIndex) const { return ReplicatedObjectData[InternalIndex]; }
	FReplicatedObjectData& GetReplicatedObjectDataNoCheck(FInternalNetHandle InternalIndex) { return ReplicatedObjectData[InternalIndex]; }

	inline const FReplicatedObjectData& GetReplicatedObjectData(FInternalNetHandle InternalIndex) const;

	inline const uint8* GetReplicatedObjectStateBufferNoCheck(FInternalNetHandle InternalObjectIndex) const { return ReplicatedObjectStateBuffers.GetData()[InternalObjectIndex]; }
	inline uint8* GetReplicatedObjectStateBufferNoCheck(FInternalNetHandle InternalObjectIndex) { return ReplicatedObjectStateBuffers.GetData()[InternalObjectIndex]; }
	inline const TArray<uint8*>& GetReplicatedObjectStateBuffers() const { return ReplicatedObjectStateBuffers; }

	// Verify a handle against internal handle, A handle is valid if it matches internal storage
	inline bool IsValidNetHandle(FNetHandle Handle) const;

	// Returns true if the handle is for local replicated object
	inline bool IsLocalNetHandle(FNetHandle Handle) const;

	// Returns true if the handle is for a remote replicated object
	inline bool IsRemoteNetHandle(FNetHandle Handle) const;

	// Extract an Full handle from an incomplete one, a incomplete handle is a handle with only an index
	inline FNetHandle GetCompleteNetHandle(FNetHandle IncompleteHandle) const;
	
	// Get Handle from internal index
	inline FNetHandle GetNetHandleFromInternalIndex(FInternalNetHandle InternalIndex) const;

	// Get internal index from handle
	inline FInternalNetHandle GetInternalIndex(FNetHandle Handle) const;

	// Get bitarray for all currently scopable internal indices
	const FNetBitArray& GetScopableInternalIndices() const { return ScopableInternalIndices; }

	// Get bitarray for all internal indices that was scopable last update
	const FNetBitArray& GetPrevFrameScopableInternalIndices() const { return PrevFrameScopableInternalIndices; }
	void SetPrevFrameScopableInternalIndicesToCurrent() { PrevFrameScopableInternalIndices = ScopableInternalIndices; }

	// Get bitarray for all internal indices that currently are assigned
	const FNetBitArray& GetAssignedInternalIndices() const { return AssignedInternalIndices; }

	// SubObjects
	const FNetBitArray& GetSubObjectInternalIndices() const { return SubObjectInternalIndices; }	
	bool AddSubObject(FNetHandle OwnerHandle, FNetHandle SubObjectHandle, FNetHandle RelativeOtherSubObjectHandle, EAddSubObjectFlags Flags = EAddSubObjectFlags::Default);
	inline bool AddSubObject(FNetHandle OwnerHandle, FNetHandle SubObjectHandle, EAddSubObjectFlags Flags = EAddSubObjectFlags::Default) { return AddSubObject(OwnerHandle, SubObjectHandle, FNetHandle(), Flags); }
	void RemoveSubObject(FNetHandle SubObjectHandle);
	FNetHandle GetSubObjectOwner(FNetHandle SubObjectHandle) const;
	bool SetSubObjectNetCondition(FInternalNetHandle SubObjectInternalIndex, FLifeTimeConditionStorage SubObjectCondition);

	// DependentObjects
	const FNetBitArray& GetDependentObjectInternalIndices() const { return DependentObjectInternalIndices; }
	const FNetBitArray& GetObjectsWithDependentObjectsInternalIndices() const { return ObjectsWithDependentObjectsInternalIndices; }
	bool AddDependentObject(FNetHandle ParentHandle, FNetHandle DependentHandle, EAddDependentObjectFlags Flags = EAddDependentObjectFlags::WarnIfAlreadyDependentObject);
	void RemoveDependentObject(FNetHandle ParentHandle, FNetHandle DependentHandle);

	// Remove DependentHandles from all dependent object tracking
	// $TODO: This might need a pass ObjectReplicationBridge as well since most likely also must restore filter/polling logic
	void RemoveDependentObject(FNetHandle DependentHandle);
	
	void SetShouldPropagateChangedStates(FNetHandle Handle, bool bShouldPropagateChangedStates);
	void SetShouldPropagateChangedStates(FInternalNetHandle ObjectInternalIndex, bool bShouldPropagateChangedStates);

	uint32 GetMaxActiveObjectCount() const { return MaxActiveObjectCount; }
	uint32 GetActiveObjectCount() const { return ActiveObjectCount; }

	// We do refcount objects tracked by each connection in order to know when it is safe to reuse an InternalIndex
	void AddNetObjectRef(FInternalNetHandle InternalIndex) { ++ReplicatedObjectRefCount[InternalIndex]; }
	void ReleaseNetObjectRef(FInternalNetHandle InternalIndex) { check(ReplicatedObjectRefCount[InternalIndex] > 0); --ReplicatedObjectRefCount[InternalIndex]; }
	uint16 GetNetObjectRefCount(FInternalNetHandle ObjectInternalIndex) const { return ReplicatedObjectRefCount[ObjectInternalIndex]; }

	// Get dependent objects for the given ParentIndex
	inline TArrayView<const FInternalNetHandle> GetDependentObjects(FInternalNetHandle ParentIndex) const;

	// Get all parents of the given DependentIndex
	inline TArrayView<const FInternalNetHandle> GetDependentObjectParents(FInternalNetHandle DependentIndex) const;

	// Get all subobjects for the given OwnerIndex, Note: only valid for the root
	inline TArrayView<const FInternalNetHandle> GetSubObjects(FInternalNetHandle OwnerIndex) const;

	// Get child subobjects for a object, used when we do hierarchical operations such as conditional serialization
	inline TArrayView<const FInternalNetHandle> GetChildSubObjects(FInternalNetHandle ParentObjectIndex) const;

	// Get child subobjects and condtionals array if one exists, if there are no child subobjects the method returns false
	inline bool GetChildSubObjects(FInternalNetHandle OwnerIndex, FChildSubObjectsInfo& OutInfo) const;

	const FHandleMap& GetReplicatedHandles() const { return HandleMap; }

	const TArray<UObject*>& GetReplicatedInstances() const { return ReplicatedInstances; }

	void AddReferencedObjects(FReferenceCollector& Collector);

	bool GetIsDestroyedStartupObject(FInternalNetHandle InternalIndex) const { return DestroyedStartupObjectInternalIndices.GetBit(InternalIndex); }
	inline uint32 GetOriginalDestroyedStartupObjectIndex(FInternalNetHandle InternalIndex) const;

	const FNetBitArray& GetDestroyedStartupObjectInternalIndices() const { return DestroyedStartupObjectInternalIndices; }

	const FNetBitArray& GetWantToBeDormantInternalIndices() const { return WantToBeDormantInternalIndices; }
	FNetBitArray& GetWantToBeDormantInternalIndices() { return WantToBeDormantInternalIndices; }

	// Iterate over all dependent objects and their dependent objects
	template <typename T>
	void ForAllDependentObjectsRecursive(FInternalNetHandle ObjectIndex, T&& Functor) const
	{
		if (ObjectsWithDependentObjectsInternalIndices.GetBit(ObjectIndex))
		{
			for (const FInternalNetHandle DependentObjectIndex : GetDependentObjects(ObjectIndex))
			{
				Functor(DependentObjectIndex);
				ForAllDependentObjectsRecursive(DependentObjectIndex, Functor);
			}
			return;
		}
	};

private:
	FInternalNetHandle InternalCreateNetObject(const FNetHandle NetHandle, const FReplicationProtocol* ReplicationProtocol);
	void InternalDestroyNetObject(FInternalNetHandle InternalIndex);

	static uint32 MakeNetHandleId(uint32 Seed, bool bIsStatic);
	uint32 GetNextHandleId(uint32 HandleIndex) const;

	// Get the next free internal index, returns InvalidInternalIndex if a free one cannot be found
	FInternalNetHandle GetNextFreeInternalIndex() const;

	inline void SetIsSubObject(FInternalNetHandle InternalIndex, bool IsSubObject) { SubObjectInternalIndices.SetBitValue(InternalIndex, IsSubObject); }
	void InternalRemoveSubObject(FInternalNetHandle OwnerInternalIndex, FInternalNetHandle SubObjectInternalIndex, bool bRemoveFromSubObjectArray = true);

	void InternalRemoveDependentObject(FInternalNetHandle ParentInternalIndex, FInternalNetHandle DependentInternalIndex, ERemoveDependentObjectFlags Flags = ERemoveDependentObjectFlags::All);
	void InternalRemoveDependentObject(FInternalNetHandle DependentInternalIndex);

	// The current replicated object count
	uint32 ActiveObjectCount;

	// Max allowed replicated object count
	uint32 MaxActiveObjectCount;

	uint32 ReplicationSystemId;

	FHandleMap HandleMap;

	// Bitset used in order to track assigned internal which are scopable
	FNetBitArray ScopableInternalIndices;

	// Which internal indices were used last net frame. This can be used to find out which ones are new and deleted this frame. 
	FNetBitArray PrevFrameScopableInternalIndices;

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
	TArray<FInternalNetHandle> PendingDestroyInternalIndices;

	// Just an array containing data about our replicated objects
	TArray<FReplicatedObjectData> ReplicatedObjectData;

	// Pointers to state buffers for all replicated objects
	TArray<uint8*> ReplicatedObjectStateBuffers;

	// Refcounts for all tracked objects
	TArray<uint16> ReplicatedObjectRefCount;

	// Raw pointers to all bound instances
	TArray<UObject*> ReplicatedInstances;

	// Assign handles
	uint32 NextStaticHandleIndex;
	uint32 NextDynamicHandleIndex;

	FNetDependencyData SubObjects;

	FReplicationProtocolManager& ReplicationProtocolManager;
};

const FNetHandleManager::FReplicatedObjectData& FNetHandleManager::GetReplicatedObjectData(FInternalNetHandle InternalIndex) const
{
	check(AssignedInternalIndices.GetBit(InternalIndex));
	return GetReplicatedObjectDataNoCheck(InternalIndex);
}

FInternalNetHandle FNetHandleManager::GetInternalIndex(FNetHandle Handle) const
{
	const FInternalNetHandle* InternalIndex = HandleMap.Find(Handle);
	return InternalIndex ? *InternalIndex : FNetHandleManager::InvalidInternalIndex;
}

FNetHandle FNetHandleManager::GetNetHandleFromInternalIndex(FInternalNetHandle InternalIndex) const
{
	check(AssignedInternalIndices.GetBit(InternalIndex));
	return GetReplicatedObjectDataNoCheck(InternalIndex).Handle;
}

FNetHandle FNetHandleManager::GetCompleteNetHandle(FNetHandle IncompleteHandle) const
{
	if (const FInternalNetHandle* InternalIndex = HandleMap.Find(IncompleteHandle))
	{
		return GetReplicatedObjectDataNoCheck(*InternalIndex).Handle;
	}
	else
	{
		return FNetHandle();
	}
}

bool FNetHandleManager::IsValidNetHandle(FNetHandle Handle) const
{
	return HandleMap.Contains(Handle);
}

bool FNetHandleManager::IsLocalNetHandle(FNetHandle Handle) const
{
	if (const uint32 InternalIndex = GetInternalIndex(Handle))
	{
		// For the time being only replicated objects owned by this peer has a state buffer
		return ReplicatedObjectStateBuffers[InternalIndex] != nullptr;
	}

	return false;
}

bool FNetHandleManager::IsRemoteNetHandle(FNetHandle Handle) const
{
	if (const uint32 InternalIndex = GetInternalIndex(Handle))
	{
		// For the time being only replicated objects owned by this peer has a state buffer
		return ReplicatedObjectStateBuffers[InternalIndex] == nullptr;
	}

	return false;
}

uint32 FNetHandleManager::GetOriginalDestroyedStartupObjectIndex(FInternalNetHandle InternalIndex) const
{
	const uint32* FoundOriginalInternalIndex = DestroyedStartupObject.Find(InternalIndex);
	return FoundOriginalInternalIndex ? *FoundOriginalInternalIndex : 0U;
}

TArrayView<const FInternalNetHandle> FNetHandleManager::GetSubObjects(FInternalNetHandle OwnerIndex) const
{
	return SubObjects.GetInternalHandleArray<FNetDependencyData::EArrayType::SubObjects>(OwnerIndex);
}

bool FNetHandleManager::GetChildSubObjects(FInternalNetHandle OwnerIndex, FChildSubObjectsInfo& OutInfo) const
{
	return SubObjects.GetChildSubObjects(OwnerIndex, OutInfo);
}

TArrayView<const FInternalNetHandle> FNetHandleManager::GetChildSubObjects(FInternalNetHandle OwnerIndex) const
{
	return SubObjects.GetInternalHandleArray<FNetDependencyData::EArrayType::ChildSubObjects>(OwnerIndex);
}

TArrayView<const FInternalNetHandle> FNetHandleManager::GetDependentObjects(FInternalNetHandle ParentIndex) const
{
	return SubObjects.GetInternalHandleArray<FNetDependencyData::EArrayType::DependentObjects>(ParentIndex);
}

TArrayView<const FInternalNetHandle> FNetHandleManager::GetDependentObjectParents(FInternalNetHandle DependentIndex) const
{
	return SubObjects.GetInternalHandleArray<FNetDependencyData::EArrayType::ParentObjects>(DependentIndex);
}
	
}
