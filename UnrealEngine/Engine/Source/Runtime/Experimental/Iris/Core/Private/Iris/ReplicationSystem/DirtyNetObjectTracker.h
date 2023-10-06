// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/Core/NetBitArray.h"
#include "Net/Core/DirtyNetObjectTracker/GlobalDirtyNetObjectTracker.h"

#include "Iris/IrisConfig.h"

namespace UE::Net::Private
{
	class FNetRefHandleManager;
	class FDirtyObjectsAccessor;
	
	typedef uint32 FInternalNetRefIndex;
}

// When enabled will detect if a polled object is dirtying another object. This is an unsupported behavior
#ifndef UE_NET_IRIS_VALIDATE_POLLED_OBJECT
#define UE_NET_IRIS_VALIDATE_POLLED_OBJECT !UE_BUILD_SHIPPING
#endif

namespace UE::Net::Private
{

IRISCORE_API void MarkNetObjectStateDirty(uint32 ReplicationSystemId, FInternalNetRefIndex NetObjectIndex);
IRISCORE_API void ForceNetUpdate(uint32 ReplicationSystemId, FInternalNetRefIndex NetObjectIndex);

struct FDirtyNetObjectTrackerInitParams
{
	const FNetRefHandleManager* NetRefHandleManager = nullptr;
	uint32 ReplicationSystemId = 0;
	uint32 MaxObjectCount = 0;
	uint32 NetObjectIndexRangeStart = 0;
	uint32 NetObjectIndexRangeEnd = 0;
};

class FDirtyNetObjectTracker
{
public:
	FDirtyNetObjectTracker();
	~FDirtyNetObjectTracker();

	void Init(const FDirtyNetObjectTrackerInitParams& Params);

	/** Update dirty objects with the set of globally marked dirty objects. */
	void UpdateDirtyNetObjects();

	/** Add all the current frame dirty objects set into the accumulated list */
	void UpdateAccumulatedDirtyList();

	/** Set safety permissions so no one can write in the bit array via the public methods */
	void LockExternalAccess();

	/** Release safety permissions and allow to write in the bit array via the public methods */
	void AllowExternalAccess();

	/** Reset the global and local dirty objects lists for those objects that are now clean */
	void ClearDirtyNetObjects(const FNetBitArrayView& CleanNetObjects);

	/** Track which object is currently being polled */
	inline void SetCurrentPolledObject(FInternalNetRefIndex PolledObject);

	/** Returns the list of objects that are dirty this frame or were dirty in previous frames but not cleaned up at that time. */
	const FNetBitArrayView GetAccumulatedDirtyNetObjects() const { return MakeNetBitArrayView(AccumulatedDirtyNetObjects); }

	/** Returns the list of objects who asked to force a replication this frame */
	FNetBitArrayView GetForceNetUpdateObjects() { return MakeNetBitArrayView(ForceNetUpdateObjects); }
	const FNetBitArrayView GetForceNetUpdateObjects() const { return MakeNetBitArrayView(ForceNetUpdateObjects); }

private:
	friend IRISCORE_API void MarkNetObjectStateDirty(uint32 ReplicationSystemId, FInternalNetRefIndex NetObjectIndex);
	friend IRISCORE_API void ForceNetUpdate(uint32 ReplicationSystemId, FInternalNetRefIndex NetObjectIndex);

	friend FDirtyObjectsAccessor;

	using StorageType = FNetBitArrayView::StorageWordType;
	static constexpr uint32 StorageTypeBitCount = FNetBitArrayView::WordBitCount;

	void Deinit();
	void MarkNetObjectDirty(FInternalNetRefIndex NetObjectIndex);
	void ForceNetUpdate(FInternalNetRefIndex NetObjectIndex);

	/** Can only be accessed via FDirtyObjectsAccessor */
	FNetBitArrayView GetDirtyNetObjectsThisFrame();

private:

	// Dirty objects that persist across frames.
	FNetBitArray AccumulatedDirtyNetObjects;

    // Objects that want to force a replication this frame
	FNetBitArray ForceNetUpdateObjects;

	// List of objects set to be dirty this frame. Is always reset at the end of the net tick flush
	StorageType* DirtyNetObjectContainer = nullptr;

	const FNetRefHandleManager* NetRefHandleManager = nullptr;
	
	FGlobalDirtyNetObjectTracker::FPollHandle GlobalDirtyTrackerPollHandle;

#if UE_NET_IRIS_VALIDATE_POLLED_OBJECT
	FInternalNetRefIndex CurrentPolledObject;
#endif

	uint32 ReplicationSystemId;

	uint32 DirtyNetObjectWordCount = 0;
	uint32 NetObjectIdRangeStart = 0;
	uint32 NetObjectIdRangeEnd = 0;
	uint32 NetObjectIdCount = 0;
	
	bool bHasPolledGlobalDirtyTracker = false;

#if UE_NET_THREAD_SAFETY_CHECK
	std::atomic_bool bIsExternalAccessAllowed = false;
#endif
};


void FDirtyNetObjectTracker::SetCurrentPolledObject(FInternalNetRefIndex PolledObject)
{ 
#if UE_NET_IRIS_VALIDATE_POLLED_OBJECT
	CurrentPolledObject = PolledObject; 
#endif
}

/**
 * Gives access to the list of dirty objects while detecting non-thread safe access to it.
 */
class FDirtyObjectsAccessor
{
public:
	FDirtyObjectsAccessor(FDirtyNetObjectTracker& InDirtyNetObjectTracker)
		: DirtyNetObjectTracker(InDirtyNetObjectTracker)
	{
		DirtyNetObjectTracker.LockExternalAccess();
	}

	~FDirtyObjectsAccessor()
	{
		DirtyNetObjectTracker.AllowExternalAccess();
	}

	FNetBitArrayView GetDirtyNetObjects()				{ return DirtyNetObjectTracker.GetDirtyNetObjectsThisFrame(); }
	const FNetBitArrayView GetDirtyNetObjects() const	{ return DirtyNetObjectTracker.GetDirtyNetObjectsThisFrame(); }

private:
	FDirtyNetObjectTracker& DirtyNetObjectTracker;
};

}
