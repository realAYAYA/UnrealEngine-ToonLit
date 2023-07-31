// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirtyNetObjectTracker.h"
#include "HAL/PlatformAtomics.h"
#include "Iris/IrisConstants.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
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

#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
constexpr uint32 DirtyNetObjectTrackerCount = FReplicationSystemFactory::MaxReplicationSystemCount;
#else
constexpr uint32 DirtyNetObjectTrackerCount = 1;
#endif
static FDirtyNetObjectTracker* DirtyNetObjectTrackers[DirtyNetObjectTrackerCount + 1]; // nullptr as last tracker

FDirtyNetObjectTracker::FDirtyNetObjectTracker()
: ReplicationSystemId(InvalidReplicationSystemId)
, DirtyNetObjectContainer(nullptr)
, DirtyNetObjectWordCount(0)
, NetObjectIdRangeStart(0U)
, NetObjectIdRangeEnd(0U)
, NetObjectIdCount(0)
{
}

FDirtyNetObjectTracker::~FDirtyNetObjectTracker()
{
	if (ReplicationSystemId < DirtyNetObjectTrackerCount && DirtyNetObjectTrackers[ReplicationSystemId] == this)
	{
		DirtyNetObjectTrackers[ReplicationSystemId] = nullptr;
	}

	Shutdown();
}

void FDirtyNetObjectTracker::Init(const FDirtyNetObjectTrackerInitParams& Params)
{
	check(Params.ReplicationSystemId < DirtyNetObjectTrackerCount);
	check(Params.NetObjectIndexRangeEnd >= Params.NetObjectIndexRangeStart);
	check(DirtyNetObjectContainer == nullptr);

	ReplicationSystemId = Params.ReplicationSystemId;
	NetObjectIdRangeStart = Params.NetObjectIndexRangeStart;
	NetObjectIdRangeEnd = Params.NetObjectIndexRangeEnd;
	/* 
	 * For now we support all IDs up to RangeEnd. This could be expensive if we partition things more in some way or other.
	 * In the latter case we would have to add functionality to FNetBitArrayView to handle an offset or add a "FNetSparseBitArray".
	 */
	NetObjectIdCount = NetObjectIdRangeEnd + 1;

	DirtyNetObjectWordCount = (NetObjectIdCount + StorageTypeBitCount - 1)/StorageTypeBitCount;
	DirtyNetObjectContainer = new StorageType[DirtyNetObjectWordCount];
	ClearDirtyNetObjects();

	if (DirtyNetObjectTrackers[ReplicationSystemId] != nullptr)
	{
		LowLevelFatalError(TEXT("DirtyNetObjectTrackerAlready initialized for ReplicationSystemId %u"), ReplicationSystemId);
	}
	
	DirtyNetObjectTrackers[ReplicationSystemId] = this;

	UE_LOG_DIRTYOBJECTTRACKER(TEXT("FDirtyNetObjectTracker::Init %u Id, Start:%u, End: %u"), ReplicationSystemId, NetObjectIdRangeStart, NetObjectIdRangeEnd);
}

void FDirtyNetObjectTracker::Shutdown()
{
	delete[] DirtyNetObjectContainer;
	DirtyNetObjectContainer = nullptr;
}

void FDirtyNetObjectTracker::MarkNetObjectDirty(uint32 NetObjectIndex)
{
	if ((NetObjectIndex >= NetObjectIdRangeStart) & (NetObjectIndex <= NetObjectIdRangeEnd))
	{
		const uint32 BitOffset = NetObjectIndex;
		const StorageType BitMask = StorageType(1) << (BitOffset & (StorageTypeBitCount - 1));

		// ideally we'd have c++20 std::atomic_ref for this
		FPlatformAtomics::InterlockedOr((TSignedIntType<sizeof(StorageType)>::Type*)(&DirtyNetObjectContainer[BitOffset/StorageTypeBitCount]), BitMask);

		UE_LOG_DIRTYOBJECTTRACKER(TEXT("FDirtyNetObjectTracker::MarkNetObjectDirty %u ( InternalIndex: %u )"), ReplicationSystemId, NetObjectIndex);
	}
}

FNetBitArrayView FDirtyNetObjectTracker::GetDirtyNetObjects() const
{
	return FNetBitArrayView(DirtyNetObjectContainer, NetObjectIdCount);
}

void FDirtyNetObjectTracker::ClearDirtyNetObjects()
{
	FMemory::Memset(DirtyNetObjectContainer, 0, DirtyNetObjectWordCount*sizeof(StorageType));

	std::atomic_thread_fence(std::memory_order_seq_cst);
}

void MarkNetObjectStateDirty(uint32 ReplicationSystemId, uint32 NetObjectIndex)
{
	if (FDirtyNetObjectTracker* DirtyNetObjectTracker = DirtyNetObjectTrackers[FMath::Min(ReplicationSystemId, DirtyNetObjectTrackerCount)])
	{
		DirtyNetObjectTracker->MarkNetObjectDirty(NetObjectIndex);
	}
}

}
