// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/Core/NetBitArray.h"

namespace UE::Net::Private
{

IRISCORE_API void MarkNetObjectStateDirty(uint32 ReplicationSystemId, uint32 NetObjectIndex);

struct FDirtyNetObjectTrackerInitParams
{
	uint32 ReplicationSystemId;
	uint32 NetObjectIndexRangeStart;
	uint32 NetObjectIndexRangeEnd;
};

class FDirtyNetObjectTracker
{
public:
	FDirtyNetObjectTracker();
	~FDirtyNetObjectTracker();

	void Init(const FDirtyNetObjectTrackerInitParams& Params);

	// N.B. These methods do not use atomics.
	// It's up to the user to determine whether it's safe to call the functions or not.
	FNetBitArrayView GetDirtyNetObjects() const;
	void ClearDirtyNetObjects();

private:
	friend IRISCORE_API void MarkNetObjectStateDirty(uint32 ReplicationSystemId, uint32 NetObjectIndex);

	using StorageType = FNetBitArrayView::StorageWordType;
	static constexpr uint32 StorageTypeBitCount = FNetBitArrayView::WordBitCount;

	void Shutdown();
	void MarkNetObjectDirty(uint32 NetObjectIndex);

	uint32 ReplicationSystemId;
	StorageType* DirtyNetObjectContainer;
	uint32 DirtyNetObjectWordCount;
	uint32 NetObjectIdRangeStart;
	uint32 NetObjectIdRangeEnd;
	uint32 NetObjectIdCount;
};

}
