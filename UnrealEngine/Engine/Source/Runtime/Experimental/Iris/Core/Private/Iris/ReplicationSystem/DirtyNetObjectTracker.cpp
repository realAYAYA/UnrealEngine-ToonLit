// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirtyNetObjectTracker.h"
#include "HAL/PlatformAtomics.h"
#include "Iris/IrisConstants.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Traits/IntType.h"
#include <atomic>

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_ENABLE_DIRTYOBJECTTRACKER_LOG 0
#else
#	define UE_NET_ENABLE_DIRTYOBJECTTRACKER_LOG 0
#endif 

#if UE_NET_ENABLE_DIRTYOBJECTTRACKER_LOG
#	define UE_LOG_DIRTYOBJECTTRACKER(Format, ...)  UE_LOG(LogIris, Log, Format, ##__VA_ARGS__)
#else
#	define UE_LOG_DIRTYOBJECTTRACKER(...)
#endif

#define UE_LOG_DIRTYOBJECTTRACKER_WARNING(Format, ...)  UE_LOG(LogIris, Warning, Format, ##__VA_ARGS__)

namespace UE::Net::Private
{

FDirtyNetObjectTracker::FDirtyNetObjectTracker()
: ReplicationSystemId(InvalidReplicationSystemId)
{
#if UE_NET_IRIS_VALIDATE_POLLED_OBJECT
	CurrentPolledObject = FNetRefHandleManager::InvalidInternalIndex;
#endif
}

FDirtyNetObjectTracker::~FDirtyNetObjectTracker()
{
	Deinit();
}

void FDirtyNetObjectTracker::Init(const FDirtyNetObjectTrackerInitParams& Params)
{
	check(Params.NetObjectIndexRangeEnd >= Params.NetObjectIndexRangeStart);
	check(Params.NetRefHandleManager != nullptr);
	check(DirtyNetObjectContainer == nullptr);

	NetRefHandleManager = Params.NetRefHandleManager;
	ReplicationSystemId = Params.ReplicationSystemId;
	NetObjectIdRangeStart = Params.NetObjectIndexRangeStart;
	NetObjectIdRangeEnd = Params.NetObjectIndexRangeEnd;
	/* 
	 * For now we support all IDs up to RangeEnd. This could be expensive if we partition things more in some way or other.
	 * In the latter case we would have to add functionality to FNetBitArrayView to handle an offset or add a "FNetSparseBitArray".
	 */
	NetObjectIdCount = Params.MaxObjectCount;

	GlobalDirtyTrackerPollHandle = FGlobalDirtyNetObjectTracker::CreatePoller();

	DirtyNetObjectWordCount = (NetObjectIdCount + StorageTypeBitCount - 1)/StorageTypeBitCount;
	DirtyNetObjectContainer = new StorageType[DirtyNetObjectWordCount];
	FMemory::Memzero(DirtyNetObjectContainer, DirtyNetObjectWordCount * sizeof(StorageType));

	AccumulatedDirtyNetObjects.Init(NetObjectIdCount);
	ForceNetUpdateObjects.Init(NetObjectIdCount);

	AllowExternalAccess();

	UE_LOG_DIRTYOBJECTTRACKER(TEXT("FDirtyNetObjectTracker::Init %u Id, Start:%u, End: %u"), ReplicationSystemId, NetObjectIdRangeStart, NetObjectIdRangeEnd);
}

void FDirtyNetObjectTracker::Deinit()
{
	GlobalDirtyTrackerPollHandle.Destroy();
	bHasPolledGlobalDirtyTracker = false;

	delete[] DirtyNetObjectContainer;
	DirtyNetObjectContainer = nullptr;
}

void FDirtyNetObjectTracker::UpdateDirtyNetObjects()
{
	if (!GlobalDirtyTrackerPollHandle.IsValid())
	{
		return;
	}

	IRIS_PROFILER_SCOPE(FDirtyNetObjectTracker_UpdateDirtyNetObjects)

	LockExternalAccess();

	bHasPolledGlobalDirtyTracker = true;

	const TSet<FNetHandle>& GlobalDirtyNetObjects = FGlobalDirtyNetObjectTracker::GetDirtyNetObjects(GlobalDirtyTrackerPollHandle);
	for (FNetHandle NetHandle : GlobalDirtyNetObjects)
	{
		const FInternalNetRefIndex NetObjectIndex = NetRefHandleManager->GetInternalIndexFromNetHandle(NetHandle);
		if (NetObjectIndex != FNetRefHandleManager::InvalidInternalIndex)
		{
			const uint32 BitOffset = NetObjectIndex;
			const StorageType BitMask = StorageType(1) << (BitOffset & (StorageTypeBitCount - 1));
			DirtyNetObjectContainer[BitOffset/StorageTypeBitCount] |= BitMask;
		}
	}

	const uint32* GlobalScopeListData = NetRefHandleManager->GetScopableInternalIndicesView().GetData();
	uint32* AccumulatedDirtyNetObjectsData = AccumulatedDirtyNetObjects.GetData();

	const uint32 NumWords = AccumulatedDirtyNetObjects.GetNumWords();
	for (uint32 WordIndex = 0; WordIndex != NumWords; ++WordIndex)
	{
		// Due to objects having been marked as dirty and later removed we must make sure that all dirty objects are still in scope.
		uint32 DirtyObjectWord = DirtyNetObjectContainer[WordIndex] & GlobalScopeListData[WordIndex];
		DirtyNetObjectContainer[WordIndex] = DirtyObjectWord;

		// Add new dirty objects to the accumulated list
		AccumulatedDirtyNetObjectsData[WordIndex] = AccumulatedDirtyNetObjectsData[WordIndex] | DirtyObjectWord;
	}

	AllowExternalAccess();
}

void FDirtyNetObjectTracker::UpdateAccumulatedDirtyList()
{
	IRIS_PROFILER_SCOPE(FDirtyNetObjectTracker_UpdateDirtyNetObjects)

	FNetBitArrayView DirtyObjectsThisFrame(DirtyNetObjectContainer, NetObjectIdCount);

	MakeNetBitArrayView(AccumulatedDirtyNetObjects).Combine(DirtyObjectsThisFrame, FNetBitArrayView::OrOp);
}

void FDirtyNetObjectTracker::MarkNetObjectDirty(FInternalNetRefIndex NetObjectIndex)
{
#if UE_NET_THREAD_SAFETY_CHECK
	checkf(bIsExternalAccessAllowed, TEXT("Cannot mark objects dirty while the bitarray is locked for modifications."));
#endif

	if ((NetObjectIndex >= NetObjectIdRangeStart) & (NetObjectIndex <= NetObjectIdRangeEnd))
	{
#if UE_NET_IRIS_VALIDATE_POLLED_OBJECT
		ensureMsgf(FNetRefHandleManager::InvalidInternalIndex == CurrentPolledObject || CurrentPolledObject == NetObjectIndex, 
			TEXT("While calling NetUpdate on %s, a different replicated object %s was dirtied. Only the updated object can be dirtied for now."),
			*GetNameSafe(NetRefHandleManager->GetReplicatedObjectInstance(CurrentPolledObject)), *GetNameSafe(NetRefHandleManager->GetReplicatedObjectInstance(NetObjectIndex)));
#endif

		const uint32 BitOffset = NetObjectIndex;
		const StorageType BitMask = StorageType(1) << (BitOffset & (StorageTypeBitCount - 1));

		// ideally we'd have c++20 std::atomic_ref for this
		FPlatformAtomics::InterlockedOr((TSignedIntType<sizeof(StorageType)>::Type*)(&DirtyNetObjectContainer[BitOffset/StorageTypeBitCount]), BitMask);

		UE_LOG_DIRTYOBJECTTRACKER(TEXT("FDirtyNetObjectTracker::MarkNetObjectDirty %u ( InternalIndex: %u )"), ReplicationSystemId, NetObjectIndex);
	}
}

void FDirtyNetObjectTracker::ForceNetUpdate(FInternalNetRefIndex NetObjectIndex)
{
	ForceNetUpdateObjects.SetBit(NetObjectIndex);

	// Flag the object dirty so we update his filters too
	MarkNetObjectDirty(NetObjectIndex);

	UE_LOG_DIRTYOBJECTTRACKER(TEXT("FDirtyNetObjectTracker::ForceNetUpdateObjects %u ( InternalIndex: %u )"), ReplicationSystemId, NetObjectIndex);
}

void FDirtyNetObjectTracker::LockExternalAccess()
{
#if UE_NET_THREAD_SAFETY_CHECK
	bIsExternalAccessAllowed = false;
#endif
}

void FDirtyNetObjectTracker::AllowExternalAccess()
{
#if UE_NET_THREAD_SAFETY_CHECK
	bIsExternalAccessAllowed = true;
#endif
}

FNetBitArrayView FDirtyNetObjectTracker::GetDirtyNetObjectsThisFrame()
{
#if UE_NET_THREAD_SAFETY_CHECK
	checkf(!bIsExternalAccessAllowed, TEXT("Cannot access the DirtyNetObjects bitarray unless its locked for multithread access."));
#endif
	return FNetBitArrayView(DirtyNetObjectContainer, NetObjectIdCount);
}

void FDirtyNetObjectTracker::ClearDirtyNetObjects(const FNetBitArrayView& CleanNetObjects)
{
	LockExternalAccess();

	if (bHasPolledGlobalDirtyTracker)
	{
		bHasPolledGlobalDirtyTracker = false;
		FGlobalDirtyNetObjectTracker::ResetDirtyNetObjects(GlobalDirtyTrackerPollHandle);
	}

	// Keep dirty flags for objects not cleaned this frame.
	MakeNetBitArrayView(AccumulatedDirtyNetObjects).Combine(CleanNetObjects, FNetBitArrayView::AndNotOp);

	// Clear the current frame dirty objects
	FMemory::Memzero(DirtyNetObjectContainer, DirtyNetObjectWordCount*sizeof(StorageType));

	ForceNetUpdateObjects.Reset();

#if UE_NET_IRIS_VALIDATE_POLLED_OBJECT
	CurrentPolledObject = 0;
#endif

	AllowExternalAccess();

	std::atomic_thread_fence(std::memory_order_seq_cst);
}

void MarkNetObjectStateDirty(uint32 ReplicationSystemId, FInternalNetRefIndex NetObjectIndex)
{
	if (UReplicationSystem* ReplicationSystem = GetReplicationSystem(ReplicationSystemId))
	{
		FDirtyNetObjectTracker& DirtyNetObjectTracker = ReplicationSystem->GetReplicationSystemInternal()->GetDirtyNetObjectTracker();
		DirtyNetObjectTracker.MarkNetObjectDirty(NetObjectIndex);
	}
}

void ForceNetUpdate(uint32 ReplicationSystemId, FInternalNetRefIndex NetObjectIndex)
{
	if (UReplicationSystem* ReplicationSystem = GetReplicationSystem(ReplicationSystemId))
	{
		FDirtyNetObjectTracker& DirtyNetObjectTracker = ReplicationSystem->GetReplicationSystemInternal()->GetDirtyNetObjectTracker();
		DirtyNetObjectTracker.ForceNetUpdate(NetObjectIndex);
	}
}

}
