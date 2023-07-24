// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/Core/NetBitArray.h"
#include "Net/Core/DirtyNetObjectTracker/GlobalDirtyNetObjectTracker.h"

namespace UE::Net::Private
{
	class FNetRefHandleManager;
}

namespace UE::Net::Private
{

IRISCORE_API void MarkNetObjectStateDirty(uint32 ReplicationSystemId, uint32 NetObjectIndex);

struct FDirtyNetObjectTrackerInitParams
{
	const FNetRefHandleManager* NetRefHandleManager = nullptr;
	uint32 ReplicationSystemId = 0;
	uint32 NetObjectIndexRangeStart = 0;
	uint32 NetObjectIndexRangeEnd = 0;
};

class FDirtyNetObjectTracker
{
public:
	FDirtyNetObjectTracker();
	~FDirtyNetObjectTracker();

	void Init(const FDirtyNetObjectTrackerInitParams& Params);

	/* Update dirty objects with the set of globally marked dirty objects. **/
	void UpdateDirtyNetObjects();

	// N.B. These methods do not use atomics.
	// It's up to the user to determine whether it's safe to call the functions or not.
	FNetBitArrayView GetDirtyNetObjects() const;
	void ClearDirtyNetObjects();

private:
	friend IRISCORE_API void MarkNetObjectStateDirty(uint32 ReplicationSystemId, uint32 NetObjectIndex);

	using StorageType = FNetBitArrayView::StorageWordType;
	static constexpr uint32 StorageTypeBitCount = FNetBitArrayView::WordBitCount;

	void Deinit();
	void MarkNetObjectDirty(uint32 NetObjectIndex);

	const FNetRefHandleManager* NetRefHandleManager;
	StorageType* DirtyNetObjectContainer;
	FGlobalDirtyNetObjectTracker::FPollHandle GlobalDirtyTrackerPollHandle;
	uint32 ReplicationSystemId;
	uint32 DirtyNetObjectWordCount;
	uint32 NetObjectIdRangeStart;
	uint32 NetObjectIdRangeEnd;
	uint32 NetObjectIdCount;
	bool bHasPolledGlobalDirtyTracker;
};

}
