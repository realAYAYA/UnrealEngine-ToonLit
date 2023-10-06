// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/Core/NetBitArray.h"

// Forward declarations
class UObjectReplicationBridge;

namespace UE::Net
{
	class FNetRefHandle;

	namespace Private
	{
		typedef uint32 FInternalNetRefIndex;

		class FReplicationSystemInternal;
		class FNetRefHandleManager;
	}
}

namespace UE::Net::Private
{

/** Class that holds the required information needed to execute the poll phase on one or multiple replicated objects. */
class FObjectPoller
{
public:

	/** Holds statistics on how the polling went */
	struct FPreUpdateAndPollStats
	{
		uint32 PreUpdatedObjectCount = 0;
		uint32 PolledObjectCount = 0;
		uint32 PolledReferencesObjectCount = 0;
	};

	struct FInitParams
	{
		FReplicationSystemInternal* ReplicationSystemInternal = nullptr;
		UObjectReplicationBridge* ObjectReplicationBridge = nullptr;
	};

public:

	FObjectPoller(const FInitParams& InitParams);

	const FPreUpdateAndPollStats& GetPollStats() const { return PollStats; }

	/** Poll all the objects whose bit index is set in the array */
	void PollObjects(const FNetBitArrayView& ObjectsConsideredForPolling);

	/** Poll a single replicated object */
	void PollSingleObject(FNetRefHandle Handle);

private:

	/** Polls an object in every circumstance */
	void ForcePollObject(FInternalNetRefIndex ObjectIndex);

	/** Polls an object only if it is required or considered dirty */
	void PushModelPollObject(FInternalNetRefIndex ObjectIndex);

private:

	UObjectReplicationBridge* ObjectReplicationBridge;
	FReplicationSystemInternal* ReplicationSystemInternal;

	FNetRefHandleManager& LocalNetRefHandleManager;
	const TArray<UObject*>& ReplicatedInstances;

	const FNetBitArrayView AccumulatedDirtyObjects;

	FNetBitArrayView DirtyObjectsToCopy;
	FNetBitArrayView DirtyObjectsThisFrame;
	FNetBitArrayView GarbageCollectionAffectedObjects;

	FPreUpdateAndPollStats PollStats;
};

} // end namespace UE::Net::Private

