// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetHandleManager.h"
#include "ReplicationOperationsInternal.h"
#include "ReplicationProtocol.h"
#include "ReplicationProtocolManager.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Net/Core/Trace/NetTrace.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

namespace UE::Net::Private
{

FNetHandleManager::FNetHandleManager(FReplicationProtocolManager& InReplicationProtocolManager, uint32 InReplicationSystemId, uint32 InMaxActiveObjectCount)
: ActiveObjectCount(0)
, MaxActiveObjectCount(InMaxActiveObjectCount)
, ReplicationSystemId(InReplicationSystemId)
, ScopableInternalIndices(MaxActiveObjectCount)
, PrevFrameScopableInternalIndices(MaxActiveObjectCount)
, AssignedInternalIndices(MaxActiveObjectCount)
, SubObjectInternalIndices(MaxActiveObjectCount)
, DependentObjectInternalIndices(MaxActiveObjectCount)
, ObjectsWithDependentObjectsInternalIndices(MaxActiveObjectCount)
, DestroyedStartupObjectInternalIndices(MaxActiveObjectCount)
, WantToBeDormantInternalIndices(MaxActiveObjectCount)
, NextStaticHandleIndex(1U) // Index 0 is always reserved, for both static and dynamic handles
, NextDynamicHandleIndex(1U)
, ReplicationProtocolManager(InReplicationProtocolManager)
{
	// first is reserved for invalid handle
	AssignedInternalIndices.SetBit(0);

	ReplicatedObjectData.SetNumUninitialized(MaxActiveObjectCount);
	ReplicatedObjectRefCount.SetNumZeroed(MaxActiveObjectCount);
	ReplicatedObjectStateBuffers.SetNumZeroed(MaxActiveObjectCount);
	
	ReplicatedInstances.SetNumZeroed(MaxActiveObjectCount);

	// For convenience we initialize ReplicatedObjectData for InvalidInternalIndex so that GetReplicatedObjectDataNoCheck returns something useful.
	ReplicatedObjectData[InvalidInternalIndex] = FReplicatedObjectData();
}

uint32 FNetHandleManager::GetNextHandleId(uint32 HandleId) const
{
	// Since we use the lowest bit in the index to indicate if the handle is static or dynamic we can not use all bits as the index
	constexpr uint32 NetHandleIdIndexBitMask = (1U << (FNetHandle::IdBits - 1U)) - 1U;

	uint32 NextHandleId = (HandleId + 1) & NetHandleIdIndexBitMask;
	if (NextHandleId == 0)
	{
		++NextHandleId;
	}
	return NextHandleId;
}

FInternalNetHandle FNetHandleManager::GetNextFreeInternalIndex() const
{
	const uint32 NextFreeIndex = AssignedInternalIndices.FindFirstZero();
	return NextFreeIndex != FNetBitArray::InvalidIndex ? NextFreeIndex : InvalidInternalIndex;
}

uint32 FNetHandleManager::InternalCreateNetObject(const FNetHandle NetHandle, const FReplicationProtocol* ReplicationProtocol)
{
	if (ActiveObjectCount >= MaxActiveObjectCount)
	{
		return InvalidInternalIndex;
	}

	// verify that the handle is free
	if (HandleMap.Contains(NetHandle))
	{
		ensureAlwaysMsgf(false, TEXT("NetHandleManager::InternalCreateNetObject %s already exists"), *NetHandle.ToString());
		return InvalidInternalIndex;
	}

	const uint32 InternalIndex = GetNextFreeInternalIndex();
	if (InternalIndex != InvalidInternalIndex)
	{
		// Store data;
		FReplicatedObjectData& Data = ReplicatedObjectData[InternalIndex];

		Data = FReplicatedObjectData();

		Data.Handle = NetHandle;
		Data.Protocol = ReplicationProtocol;
		Data.InstanceProtocol = nullptr;
		ReplicatedObjectStateBuffers.GetData()[InternalIndex] = nullptr;
		Data.ReceiveStateBuffer = nullptr;
		Data.bShouldPropagateChangedStates = 1U;

		++ActiveObjectCount;

		// Add map entry so that we can map from NetHandle to InternalIndex
		HandleMap.Add(Data.Handle, InternalIndex);

		// Mark Handle index as assigned and scopable for now
		AssignedInternalIndices.SetBit(InternalIndex);
		ScopableInternalIndices.SetBit(InternalIndex);

		// When a handle is first created, it is not set to be a subobject
		SubObjectInternalIndices.ClearBit(InternalIndex);

		ReplicatedObjectRefCount[InternalIndex] = 0;

		return InternalIndex;
	}
		
	return InvalidInternalIndex;
}

void FNetHandleManager::AttachInstanceProtocol(FInternalNetHandle InternalIndex, const FReplicationInstanceProtocol* InstanceProtocol, UObject* Instance)
{
	if (ensureAlways((InternalIndex != InvalidInternalIndex) && InstanceProtocol))
	{
		FReplicatedObjectData& Data = ReplicatedObjectData[InternalIndex];
		Data.InstanceProtocol = InstanceProtocol;

		check(ReplicatedInstances[InternalIndex] == nullptr);
		ReplicatedInstances[InternalIndex] = Instance;
	}
}

const FReplicationInstanceProtocol* FNetHandleManager::DetachInstanceProtocol(FInternalNetHandle InternalIndex)
{
	if (ensure(InternalIndex != InvalidInternalIndex))
	{
		FReplicatedObjectData& Data = ReplicatedObjectData[InternalIndex];
		const FReplicationInstanceProtocol* InstanceProtocol = Data.InstanceProtocol;
		Data.InstanceProtocol = nullptr;
		ReplicatedInstances[InternalIndex] = nullptr;
		
		return InstanceProtocol;
	}

	return nullptr;
}

FNetHandle FNetHandleManager::AllocateNetHandle(bool bIsStatic)
{
	uint32& NextHandleId = bIsStatic ? NextStaticHandleIndex : NextDynamicHandleIndex;

	const uint32 NewHandleId = MakeNetHandleId(NextHandleId, bIsStatic);
	FNetHandle NewHandle = MakeNetHandle(NewHandleId, ReplicationSystemId);

	// Verify that the handle is free
	if (HandleMap.Contains(NewHandle))
	{
		checkf(false, TEXT("FNetHandleManager::AllocateNetHandle - Handle %s already exists!"), *NewHandle.ToString());

		return FNetHandle();
	}

	// Bump NextHandleId
	NextHandleId = GetNextHandleId(NextHandleId);

	return NewHandle;
}

FNetHandle FNetHandleManager::CreateNetObject(FNetHandle WantedHandle, const FReplicationProtocol* ReplicationProtocol)
{
	FNetHandle NetHandle = WantedHandle;

	const FInternalNetHandle InternalIndex = InternalCreateNetObject(NetHandle, ReplicationProtocol);
	if (InternalIndex != InvalidInternalIndex)
	{
		FReplicatedObjectData& Data = ReplicatedObjectData[InternalIndex];
		
		// Allocate storage for outgoing data. We need a valid pointer even if the size is zero.
		uint8* StateBuffer = (uint8*)FMemory::Malloc(FPlatformMath::Max(ReplicationProtocol->InternalTotalSize, 1U), ReplicationProtocol->InternalTotalAlignment);
		// Clear state buffer.
		FMemory::Memzero(StateBuffer, ReplicationProtocol->InternalTotalSize);
		if (EnumHasAnyFlags(ReplicationProtocol->ProtocolTraits, EReplicationProtocolTraits::HasConditionalChangeMask))
		{
			// Enable all conditions by default. This is to be compatible with the implementation of FRepChangedPropertyTracker where we have hooks into ReplicationSystem.
			FNetBitArrayView ConditionalChangeMask(reinterpret_cast<uint32*>(StateBuffer + ReplicationProtocol->GetConditionalChangeMaskOffset()), ReplicationProtocol->ChangeMaskBitCount, FNetBitArrayView::NoResetNoValidate);
			ConditionalChangeMask.SetAllBits();
		}

		ReplicatedObjectStateBuffers.GetData()[InternalIndex] = StateBuffer;

		// Bump protocol refcount
		ReplicationProtocol->AddRef();

		return NetHandle;
	}
	else
	{
		return FNetHandle();
	}
}

// Create NetHandle not owned by us
FNetHandle FNetHandleManager::CreateNetObjectFromRemote(FNetHandle WantedHandle, const FReplicationProtocol* ReplicationProtocol)
{
	if (!ensureAlwaysMsgf(WantedHandle.IsValid() && !WantedHandle.IsCompleteHandle(), TEXT("FNetHandleManager::CreateNetObjectFromRemote Expected WantedHandle %s to be valid and incomplete"), *WantedHandle.ToString()))
	{
		return FNetHandle();
	}

	FNetHandle NetHandle = MakeNetHandle(WantedHandle.GetId(), ReplicationSystemId);

	const FInternalNetHandle InternalIndex = InternalCreateNetObject(NetHandle, ReplicationProtocol);
	if (InternalIndex != InvalidInternalIndex)
	{
		FReplicatedObjectData& Data = ReplicatedObjectData[InternalIndex];

		// Allocate storage for incoming data
		Data.ReceiveStateBuffer = (uint8*)FMemory::Malloc(ReplicationProtocol->InternalTotalSize, ReplicationProtocol->InternalTotalAlignment);

		// Since we currently do not have any default values, just initialize to zero
		// N.B. If this Memzero is optimized away for some reason it still needs to be done for protocols with dynamic state.
		FMemory::Memzero(Data.ReceiveStateBuffer, ReplicationProtocol->InternalTotalSize);

		// Note: We could initialize this from default but at the moment it is part of the contract for all serializers to write the value when we serialize and we will only apply dirty states

		// Don't bother initializing the conditional changemask if present since it's currently unused on the receiving end.

		// Bump protocol refcount
		ReplicationProtocol->AddRef();

		return NetHandle;
	}
	else
	{
		return FNetHandle();
	}
}

void FNetHandleManager::InternalDestroyNetObject(FInternalNetHandle InternalIndex)
{
	FReplicatedObjectData& Data = ReplicatedObjectData[InternalIndex];

	UE_LOG(LogIris, Verbose, TEXT("FNetHandleManager::InternalDestroyNetObject ( InternalIndex: %u ) %s"), InternalIndex, *Data.Handle.ToString());

	uint8* StateBuffer = ReplicatedObjectStateBuffers.GetData()[InternalIndex];
	// Free any allocated resources
	if (EnumHasAnyFlags(Data.Protocol->ProtocolTraits, EReplicationProtocolTraits::HasDynamicState))
	{
		FNetSerializationContext FreeContext;
		FInternalNetSerializationContext InternalContext;
		FreeContext.SetInternalContext(&InternalContext);
		if (StateBuffer != nullptr)
		{
			FReplicationProtocolOperationsInternal::FreeDynamicState(FreeContext, StateBuffer, Data.Protocol);
		}
		if (Data.ReceiveStateBuffer != nullptr)
		{
			FReplicationProtocolOperationsInternal::FreeDynamicState(FreeContext, Data.ReceiveStateBuffer, Data.Protocol);
		}
	}
	
	// If we have subobjects remove them from our list
	if (FNetDependencyData::FInternalNetHandleArray* SubObjectArray = SubObjects.GetInternalHandleArray<FNetDependencyData::EArrayType::SubObjects>(InternalIndex))
	{
		for (FInternalNetHandle SubObjectInternalHandle : *SubObjectArray)
		{
			InternalRemoveSubObject(InternalIndex, SubObjectInternalHandle, false);
		}
		SubObjectArray->Reset(0U);
	}
	// Clear ChildSubObjectArray
	if (FNetDependencyData::FInternalNetHandleArray* ChildSubObjectArray = SubObjects.GetInternalHandleArray<FNetDependencyData::EArrayType::ChildSubObjects>(InternalIndex))
	{
		ChildSubObjectArray->Reset(0U);
	}

	// If we are a subobject remove from owner and hierarchical parents
	if (Data.SubObjectRootIndex != InvalidInternalIndex)
	{
		InternalRemoveSubObject(Data.SubObjectRootIndex, InternalIndex);
	}

	// Remove from all dependent object relationships and clear data
	InternalRemoveDependentObject(InternalIndex);

	// Free all stored data for object
	SubObjects.FreeStoredDependencyDataForObject(InternalIndex);

	// Decrease protocol refcount
	Data.Protocol->Release();
	if (Data.Protocol->GetRefCount() == 0)
	{
		ReplicationProtocolManager.DestroyReplicationProtocol(Data.Protocol);
	}

	FMemory::Free(StateBuffer);
	FMemory::Free(Data.ReceiveStateBuffer);

	// Clear pointer to state buffer
	ReplicatedObjectStateBuffers.GetData()[InternalIndex] = nullptr;

	UE_NET_TRACE_NETHANDLE_DESTROYED(Data.Handle);

	Data = FReplicatedObjectData();

	// tracking
	AssignedInternalIndices.ClearBit(InternalIndex);

	// Restore internal state
	SubObjectInternalIndices.ClearBit(InternalIndex);
	WantToBeDormantInternalIndices.ClearBit(InternalIndex);
	ObjectsWithDependentObjectsInternalIndices.ClearBit(InternalIndex);

	// Cleanup cross reference to destruction info
	if (DestroyedStartupObjectInternalIndices.GetBit(InternalIndex))
	{
		DestroyedStartupObjectInternalIndices.ClearBit(InternalIndex);
		uint32 OtherInternalIndex = 0U;
		if (DestroyedStartupObject.RemoveAndCopyValue(InternalIndex, OtherInternalIndex))
		{
			DestroyedStartupObject.Remove(OtherInternalIndex);
		}
	}

	--ActiveObjectCount;
}

FNetHandle FNetHandleManager::CreateHandleForDestructionInfo(FNetHandle Handle, const FReplicationProtocol* DestroydObjectProtocol)
{
	// Create destruction info handle carrying destruction info
	FNetHandle AllocatedHandle = AllocateNetHandle(false);
	FNetHandle DestructionInfoHandle = CreateNetObject(AllocatedHandle, DestroydObjectProtocol);

	if (DestructionInfoHandle.IsValid())
	{
		const uint32 InternalIndex = GetInternalIndex(DestructionInfoHandle);
		const uint32 DestroyedInternalIndex = GetInternalIndex(Handle);
		
		// mark the internal index
		DestroyedStartupObjectInternalIndices.SetBit(InternalIndex);
				
		// Is the object replicated, if so we must make sure that we do not add it to scope by accident
		if (DestroyedInternalIndex)
		{			
			DestroyedStartupObject.FindOrAdd(InternalIndex, DestroyedInternalIndex);
		
			// Mark the replicated index as destroyed
			DestroyedStartupObjectInternalIndices.SetBit(DestroyedInternalIndex);
			DestroyedStartupObject.FindOrAdd(DestroyedInternalIndex, InternalIndex);
		}
	}

	return DestructionInfoHandle;
}

void FNetHandleManager::RemoveFromScope(FInternalNetHandle InternalIndex)
{
	// Can only remove an object from scope if it is assignable
	if (ensure(AssignedInternalIndices.GetBit(InternalIndex)))
	{
		ScopableInternalIndices.ClearBit(InternalIndex);
	}
}

void FNetHandleManager::DestroyNetObject(FNetHandle Handle)
{
	FInternalNetHandle InternalIndex = HandleMap.FindAndRemoveChecked(Handle);
	if (ensure(AssignedInternalIndices.GetBit(InternalIndex)))
	{
		FReplicatedObjectData& Data = ReplicatedObjectData[InternalIndex];
		check(Data.Handle == Handle);

		// Remove from scopable objects if not already done
		ScopableInternalIndices.ClearBit(InternalIndex);

		// We always defer the actual destroy
		PendingDestroyInternalIndices.Add(InternalIndex);
	}
}

void FNetHandleManager::DestroyObjectsPendingDestroy()
{
	// Destroy Objects pending destroy
	for (int32 It = PendingDestroyInternalIndices.Num() -1; It >= 0; --It)
	{
		const FInternalNetHandle InternalIndex = PendingDestroyInternalIndices[It];
		if (ReplicatedObjectRefCount[InternalIndex] == 0)
		{
			InternalDestroyNetObject(InternalIndex);
			PendingDestroyInternalIndices.RemoveAtSwap(It);
		}
	}
}

bool FNetHandleManager::AddSubObject(FNetHandle OwnerHandle, FNetHandle SubObjectHandle, FNetHandle RelativeOtherSubObjectHandle, EAddSubObjectFlags Flags)
{
	check(OwnerHandle != SubObjectHandle);

	// validate objects
	const FInternalNetHandle OwnerInternalIndex = GetInternalIndex(OwnerHandle);
	const FInternalNetHandle SubObjectInternalIndex = GetInternalIndex(SubObjectHandle);

	const bool bIsValidOwner = ensure(OwnerInternalIndex != InvalidInternalIndex);
	const bool bIsValidSubObejct = ensure(SubObjectInternalIndex != InvalidInternalIndex);

	if (!(bIsValidOwner && bIsValidSubObejct))
	{
		return false;
	}

	FReplicatedObjectData& SubObjectData = GetReplicatedObjectDataNoCheck(SubObjectInternalIndex);
	if (!ensureAlwaysMsgf(SubObjectData.SubObjectRootIndex == InvalidInternalIndex, TEXT("FNetHandleManager::AddSubObject %s is already marked as a subobject"), *SubObjectHandle.ToString()))
	{
		return false;
	}

	FNetDependencyData::FInternalNetHandleArray& SubObjectArray = SubObjects.GetOrCreateInternalHandleArray<FNetDependencyData::SubObjects>(OwnerInternalIndex);
	SubObjectArray.Add(SubObjectInternalIndex);
	SubObjectData.SubObjectRootIndex = OwnerInternalIndex;
	SubObjectData.bDestroySubObjectWithOwner = EnumHasAnyFlags(Flags, EAddSubObjectFlags::DestroyWithOwner);
	// Mark the object as a subobject
	SetIsSubObject(SubObjectInternalIndex, true);

	const FInternalNetHandle RelativeOtherSubObjectInternalIndex = EnumHasAnyFlags(Flags, EAddSubObjectFlags::ReplicateWithSubObject) ? GetInternalIndex(RelativeOtherSubObjectHandle) : InvalidInternalIndex;
	if (RelativeOtherSubObjectInternalIndex != InvalidInternalIndex && ensureAlwaysMsgf(SubObjectArray.Find(RelativeOtherSubObjectInternalIndex) != INDEX_NONE, TEXT("RelativeOtherSubObjectHandle %s Must be a Subobject of %s"), *RelativeOtherSubObjectHandle.ToString(), *OwnerHandle.ToString()))
	{
		// Add to child array of RelativeOtherSubObjectRelativeIndex for hierarchical replication order
		FNetDependencyData::FSubObjectConditionalsArray* SubObjectConditionalsArray = nullptr;
		FNetDependencyData::FInternalNetHandleArray& ChildSubObjectArray = SubObjects.GetOrCreateInternalChildSubObjectsArray(RelativeOtherSubObjectInternalIndex, SubObjectConditionalsArray);
		ChildSubObjectArray.Add(SubObjectInternalIndex);
		if (SubObjectConditionalsArray)
		{
			SubObjectConditionalsArray->Add(ELifetimeCondition::COND_None);
		}
		SubObjectData.SubObjectParentIndex = RelativeOtherSubObjectInternalIndex;
	}
	else
	{
		// Add to child array of root
		FNetDependencyData::FSubObjectConditionalsArray* SubObjectConditionalsArray = nullptr;
		FNetDependencyData::FInternalNetHandleArray& ChildSubObjectArray = SubObjects.GetOrCreateInternalChildSubObjectsArray(OwnerInternalIndex, SubObjectConditionalsArray);
		ChildSubObjectArray.Add(SubObjectInternalIndex);
		if (SubObjectConditionalsArray)
		{
			SubObjectConditionalsArray->Add(ELifetimeCondition::COND_None);
		}
		SubObjectData.SubObjectParentIndex = OwnerInternalIndex;
	}

	return true;
}

void FNetHandleManager::InternalRemoveSubObject(FInternalNetHandle OwnerInternalIndex, FInternalNetHandle SubObjectInternalIndex, bool bRemoveFromSubObjectArray)
{
	// both must be valid
	if (OwnerInternalIndex != InvalidInternalIndex && SubObjectInternalIndex != InvalidInternalIndex)
	{
		FReplicatedObjectData& SubObjectData = GetReplicatedObjectDataNoCheck(SubObjectInternalIndex);
		check(SubObjectData.SubObjectRootIndex == OwnerInternalIndex);
		
		if (bRemoveFromSubObjectArray)
		{
			// Remove from root parent
			if (FNetDependencyData::FInternalNetHandleArray* SubObjectArray = SubObjects.GetInternalHandleArray<FNetDependencyData::EArrayType::SubObjects>(OwnerInternalIndex))
			{
				SubObjectArray->Remove(SubObjectInternalIndex);
			}
			// Remove from parents ChildSubObjectArray
			if (SubObjectData.SubObjectParentIndex != InvalidInternalIndex)
			{
				FNetDependencyData::FInternalNetHandleArray* ChildSubObjectArray;
				FNetDependencyData::FSubObjectConditionalsArray* SubObjectConditionsArray;

				if (SubObjects.GetInternalChildSubObjectAndConditionalArrays(SubObjectData.SubObjectParentIndex, ChildSubObjectArray, SubObjectConditionsArray))
				{
					const int32 ArrayIndex = ChildSubObjectArray->Find(SubObjectInternalIndex);
					if (ensureAlways(ArrayIndex != INDEX_NONE))
					{
						ChildSubObjectArray->RemoveAt(ArrayIndex);
						if (SubObjectConditionsArray)
						{
							SubObjectConditionsArray->RemoveAt(ArrayIndex);
							check(SubObjectConditionsArray->Num() == ChildSubObjectArray->Num());
						}
					}
				}
			}
		}

		SubObjectData.SubObjectRootIndex = InvalidInternalIndex;
		SubObjectData.bDestroySubObjectWithOwner = false;

		SetIsSubObject(SubObjectInternalIndex, false);
	}
}

void FNetHandleManager::RemoveSubObject(FNetHandle Handle)
{
	FInternalNetHandle SubObjectInternalIndex = GetInternalIndex(Handle);
	checkSlow(SubObjectInternalIndex);

	if (SubObjectInternalIndex != InvalidInternalIndex)
	{
		const uint32 OwnerInternalIndex = ReplicatedObjectData[SubObjectInternalIndex].SubObjectRootIndex;
		if (OwnerInternalIndex != InvalidInternalIndex)
		{
			InternalRemoveSubObject(OwnerInternalIndex, SubObjectInternalIndex);
		}
	}
}

bool FNetHandleManager::SetSubObjectNetCondition(FInternalNetHandle SubObjectInternalIndex, FLifeTimeConditionStorage SubObjectCondition)
{
	if (ensure(SubObjectInternalIndex != InvalidInternalIndex))
	{
		const FInternalNetHandle SubObjectParentIndex = ReplicatedObjectData[SubObjectInternalIndex].SubObjectParentIndex;
		if (ensure(SubObjectParentIndex != InvalidInternalIndex))
		{
			FNetDependencyData::FInternalNetHandleArray* SubObjectsArray;
			FNetDependencyData::FSubObjectConditionalsArray* SubObjectConditionals;
			
			if (SubObjects.GetInternalChildSubObjectAndConditionalArrays(SubObjectParentIndex, SubObjectsArray, SubObjectConditionals))
			{
				const int32 SubObjectArrayIndex = SubObjectsArray->Find(SubObjectInternalIndex);
				if (ensure(SubObjectArrayIndex != INDEX_NONE))
				{
					if (!SubObjectConditionals)
					{
						SubObjectConditionals = &SubObjects.GetOrCreateSubObjectConditionalsArray(SubObjectParentIndex);
					}

					check(SubObjectConditionals->Num() == SubObjectsArray->Num());

					SubObjectConditionals->GetData()[SubObjectArrayIndex] = SubObjectCondition;
					return true;
				}
			}

		}
	}

	return false;
}

FNetHandle FNetHandleManager::GetSubObjectOwner(FNetHandle SubObjectHandle) const
{
	const FInternalNetHandle SubObjectInternalIndex = GetInternalIndex(SubObjectHandle);
	const FInternalNetHandle OwnerInternalIndex = SubObjectInternalIndex != InvalidInternalIndex ? ReplicatedObjectData[SubObjectInternalIndex].SubObjectRootIndex : InvalidInternalIndex;

	return OwnerInternalIndex != InvalidInternalIndex ? ReplicatedObjectData[OwnerInternalIndex].Handle : FNetHandle();
}

bool FNetHandleManager::AddDependentObject(FNetHandle ParentHandle, FNetHandle DependentObjectHandle, EAddDependentObjectFlags Flags)
{
	check(ParentHandle != DependentObjectHandle);

	// validate objects
	FInternalNetHandle ParentInternalIndex = GetInternalIndex(ParentHandle);
	FInternalNetHandle DependentObjectInternalIndex = GetInternalIndex(DependentObjectHandle);

	const bool bIsValidOwner = ensure(ParentInternalIndex != InvalidInternalIndex);
	const bool bIsValidDependentObject = ensure(DependentObjectInternalIndex != InvalidInternalIndex);

	if (!(bIsValidOwner && bIsValidDependentObject))
	{
		return false;
	}

	FReplicatedObjectData& DependentObjectData = GetReplicatedObjectDataNoCheck(DependentObjectInternalIndex);
	FReplicatedObjectData& ParentObjectData = GetReplicatedObjectDataNoCheck(ParentInternalIndex);

	// SubObjects cannot have dependent objects or be a dependent object (for now)
	check(!DependentObjectData.IsSubObject() && !ParentObjectData.IsSubObject());
	check(!SubObjectInternalIndices.GetBit(DependentObjectInternalIndex));
	check(!SubObjectInternalIndices.GetBit(ParentInternalIndex));

	// Add dependent to parents dependent object list
	FNetDependencyData::FInternalNetHandleArray& ParentDependentObjectArray = SubObjects.GetOrCreateInternalHandleArray<FNetDependencyData::DependentObjects>(ParentInternalIndex);
	const bool bDependentIsAlreadyDependant = ParentDependentObjectArray.Find(DependentObjectInternalIndex) != INDEX_NONE;
	if (!bDependentIsAlreadyDependant)
	{
		ParentDependentObjectArray.Add(DependentObjectInternalIndex);
	}

	FNetDependencyData::FInternalNetHandleArray& DependentParentObjectArray = SubObjects.GetOrCreateInternalHandleArray<FNetDependencyData::ParentObjects>(DependentObjectInternalIndex);
	const bool bDependentHadParentAlready = DependentParentObjectArray.Find(ParentInternalIndex) != INDEX_NONE;
	if (!bDependentHadParentAlready)
	{
		DependentParentObjectArray.Add(ParentInternalIndex);
	}

	// Update cached info to avoid to do map lookups to find out if we have are a dependent object or have dependent objects
	DependentObjectData.bIsDependentObject = true;
	ParentObjectData.bHasDependentObjects = true;
	ObjectsWithDependentObjectsInternalIndices.SetBit(ParentInternalIndex);
	DependentObjectInternalIndices.SetBit(DependentObjectInternalIndex);

	if (EnumHasAnyFlags(Flags, EAddDependentObjectFlags::WarnIfAlreadyDependentObject) && (bDependentHadParentAlready || bDependentIsAlreadyDependant))
	{
		// If this gets out of sync something is messed up
		check(bDependentHadParentAlready == bDependentIsAlreadyDependant);
		ensureAlwaysMsgf(false, TEXT("FNetHandleManager::AddDependentObject %s already is a child of %s parent %s"), *DependentObjectHandle.ToString(), *GetReplicatedObjectDataNoCheck(DependentObjectData.SubObjectRootIndex).Handle.ToString(), *ParentHandle.ToString());
	}

	return true;
}

void FNetHandleManager::RemoveDependentObject(FNetHandle DependentHandle)
{
	FInternalNetHandle DependentInternalIndex = GetInternalIndex(DependentHandle);

	if (DependentInternalIndex != InvalidInternalIndex)
	{
		InternalRemoveDependentObject(DependentInternalIndex);
	}
}

void FNetHandleManager::InternalRemoveDependentObject(FInternalNetHandle ParentInternalIndex, FInternalNetHandle DependentInternalIndex, ERemoveDependentObjectFlags Flags)
{
	if (EnumHasAnyFlags(Flags, ERemoveDependentObjectFlags::RemoveFromDependentParentObjects))
	{
		if (FNetDependencyData::FInternalNetHandleArray* ParentObjectArray = SubObjects.GetInternalHandleArray<FNetDependencyData::ParentObjects>(DependentInternalIndex))
		{
			ParentObjectArray->Remove(ParentInternalIndex);
			if (ParentObjectArray->Num() == 0)
			{
				FReplicatedObjectData& DependentObjectData = GetReplicatedObjectDataNoCheck(DependentInternalIndex);
				DependentObjectData.bIsDependentObject = false;
				DependentObjectInternalIndices.ClearBit(DependentInternalIndex);
			}
		}
	}

	if (EnumHasAnyFlags(Flags, ERemoveDependentObjectFlags::RemoveFromParentDependentObjects))
	{
		if (FNetDependencyData::FInternalNetHandleArray* ParentDependentObjectArray = SubObjects.GetInternalHandleArray<FNetDependencyData::DependentObjects>(ParentInternalIndex))
		{
			ParentDependentObjectArray->Remove(DependentInternalIndex);
			if (ParentDependentObjectArray->Num() == 0)
			{
				FReplicatedObjectData& ParentObjectData = GetReplicatedObjectDataNoCheck(ParentInternalIndex);
				ParentObjectData.bHasDependentObjects = false;
				ObjectsWithDependentObjectsInternalIndices.ClearBit(ParentInternalIndex);
			}
		}
	}
}

void FNetHandleManager::InternalRemoveDependentObject(FInternalNetHandle DependentInternalIndex)
{
	// Remove from all parents
	if (FNetDependencyData::FInternalNetHandleArray* ParentObjectArray = SubObjects.GetInternalHandleArray<FNetDependencyData::ParentObjects>(DependentInternalIndex))
	{
		for (FInternalNetHandle ParentInternalIndex : *ParentObjectArray)
		{
			// Flag is set to only update data on the parent to avoid modifying the array we iterate over
			InternalRemoveDependentObject(ParentInternalIndex, DependentInternalIndex, ERemoveDependentObjectFlags::RemoveFromParentDependentObjects);
		}
		ParentObjectArray->Reset();
	}

	// Remove from our dependents
	if (FNetDependencyData::FInternalNetHandleArray* DependentObjectArray = SubObjects.GetInternalHandleArray<FNetDependencyData::DependentObjects>(DependentInternalIndex))
	{
		for (FInternalNetHandle ChildDepedentInternalIndex : *DependentObjectArray)
		{
			// Flag is set to only update data on the childDependentObject to avoid modifying the array we iterate over
			InternalRemoveDependentObject(DependentInternalIndex, ChildDepedentInternalIndex, ERemoveDependentObjectFlags::RemoveFromDependentParentObjects);
		}
		DependentObjectArray->Reset();		
	}

	FReplicatedObjectData& DependentObjectData = GetReplicatedObjectDataNoCheck(DependentInternalIndex);

	// Clear out flags on this object	
	DependentObjectData.bIsDependentObject = false;
	DependentObjectData.bHasDependentObjects = false;
	ObjectsWithDependentObjectsInternalIndices.ClearBit(DependentInternalIndex);
	DependentObjectInternalIndices.ClearBit(DependentInternalIndex);
}

void FNetHandleManager::RemoveDependentObject(FNetHandle ParentHandle, FNetHandle DependentHandle)
{
	// Validate objects
	FInternalNetHandle ParentInternalIndex = GetInternalIndex(ParentHandle);
	FInternalNetHandle DependentInternalIndex = GetInternalIndex(DependentHandle);

	if (!(ParentInternalIndex != InvalidInternalIndex) && (DependentInternalIndex != InvalidInternalIndex))
	{
		return;
	}

	InternalRemoveDependentObject(ParentInternalIndex, DependentInternalIndex);
}

void FNetHandleManager::SetShouldPropagateChangedStates(FInternalNetHandle ObjectInternalIndex, bool bShouldPropagateChangedStates)
{
	if (ObjectInternalIndex != InvalidInternalIndex)
	{
		if (bShouldPropagateChangedStates)
		{
			// Currently we do not support re-enabling state propagation
			// $IRIS: $TODO: Implement method to force dirty all changes 
			// https://jira.it.epicgames.com/browse/UE-127368

			checkf(false, TEXT("Re-enabling state change propagation is currently Not implemented."));			
			return;
		}

		ReplicatedObjectData[ObjectInternalIndex].bShouldPropagateChangedStates = bShouldPropagateChangedStates ? 1U : 0U;
	}
}

void FNetHandleManager::SetShouldPropagateChangedStates(FNetHandle Handle, bool bShouldPropagateChangedStates)
{
	FInternalNetHandle ObjectInternalIndex = GetInternalIndex(Handle);
	return SetShouldPropagateChangedStates(ObjectInternalIndex, bShouldPropagateChangedStates);
}

void FNetHandleManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ReplicatedInstances);
}

uint32 FNetHandleManager::MakeNetHandleId(uint32 Id, bool bIsStatic)
{
	return (Id << 1U) | (bIsStatic ? 1U : 0U);
}

FNetHandle FNetHandleManager::MakeNetHandle(uint32 Id, uint32 ReplicationSystemId)
{
	check((Id & FNetHandle::IdMask) == Id);
	check(ReplicationSystemId < FNetHandle::MaxReplicationSystemId);

	FNetHandle Handle;

	Handle.Id = Id;
	Handle.ReplicationSystemId = ReplicationSystemId + 1U;

	return Handle;
}

FNetHandle FNetHandleManager::MakeNetHandleFromId(uint32 Id)
{
	check((Id & FNetHandle::IdMask) == Id);

	FNetHandle Handle;

	Handle.Id = Id;
	Handle.ReplicationSystemId = 0U;

	return Handle;
}


}
