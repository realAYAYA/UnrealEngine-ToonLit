// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "HAL/CriticalSection.h"

namespace UE::Net
{

/**
 * Send stats for Iris replication reported to the CSV profiler. Mostly of interest on the server side due to the server authoritative network model.
 * Its intended use is to do thread local tracking to an instance and then use the Accumulate function for thread safe updating of the ReplicationSystem owned instance.
 * The ReplicationSystem owned instance is the one reporting to the CSV profiler.
 */
class FNetSendStats
{
public:
	FNetSendStats() = default;
	FNetSendStats(const FNetSendStats&) = delete;
	FNetSendStats& operator=(const FNetSendStats&) = delete;

	/** Set number of objects scheduled for replication. */
	void SetNumberOfObjectsScheduledForReplication(uint32 Count);

	/** Set number of replicated objects. */
	void SetNumberOfReplicatedObjects(uint32 Count);

	/** Get the number of replicated objects. */
	uint32 GetNumberOfReplicatedObjects() const;

	/** Set number of replicated objects that used delta compression. */
	void SetNumberOfDeltaCompressedReplicatedObjects(uint32 Count);

	/** Set the number of huge objects in sending or waiting to be acked. */
	void SetNumberOfActiveHugeObjects(uint32 Count);

	/** Add time in seconds waiting for completely sent huge object to be acked. */
	void AddHugeObjectWaitingTime(double Seconds);

	/** Add time in seconds waiting to be able to continue sending huge object. */
	void AddHugeObjectStallTime(double Seconds);

	/** Add time spent on trying to replicate an object in the packet but that ended up not fitting. */
	void AddReplicationWasteTime(double Seconds);

	/** Add stats from another instance. */
	IRISCORE_API void Accumulate(const FNetSendStats& Stats);

	/** Reset stats. */
	IRISCORE_API void Reset();

	/** Report the stats to the CSV profiler. Does nothing if CSV profiler support is compiled out. */
	IRISCORE_API void ReportCsvStats();

	/** Set number of replicating connections. */
	void SetNumberOfReplicatingConnections(uint32 Count);

private:
	// Helper struct to facilitate reset of stats.
	struct FStats
	{
		double HugeObjectWaitingForAckTimeInSeconds = 0;
		double HugeObjectStallingTimeInSeconds = 0;
		double ReplicationWasteTimeInSeconds = 0;

		int32 ScheduledForReplicationObjectCount = 0;
		int32 ReplicatedObjectCount = 0;
		int32 DeltaCompressedObjectCount = 0;
		int32 ReplicationWasteObjectCount = 0;
		int32 ActiveHugeObjectCount = 0;
		int32 HugeObjectsWaitingForAckCount = 0;
		int32 HugeObjectsStallingCount = 0;
		int32 ReplicatingConnectionCount = 0;
	};

	FCriticalSection CS;
	FStats Stats;
};

inline void FNetSendStats::SetNumberOfObjectsScheduledForReplication(uint32 Count)
{
	Stats.ScheduledForReplicationObjectCount = Count;
}

inline void FNetSendStats::SetNumberOfReplicatedObjects(uint32 Count)
{
	Stats.ReplicatedObjectCount = Count;
}

inline uint32 FNetSendStats::GetNumberOfReplicatedObjects() const
{
	return Stats.ReplicatedObjectCount;
}

inline void FNetSendStats::SetNumberOfDeltaCompressedReplicatedObjects(uint32 Count)
{
	Stats.DeltaCompressedObjectCount = Count;
}

inline void FNetSendStats::SetNumberOfActiveHugeObjects(uint32 Count)
{
	Stats.ActiveHugeObjectCount = static_cast<int32>(Count);
}

inline void FNetSendStats::AddHugeObjectWaitingTime(double Seconds)
{
	++Stats.HugeObjectsWaitingForAckCount;
	Stats.HugeObjectWaitingForAckTimeInSeconds += Seconds;
}

inline void FNetSendStats::AddHugeObjectStallTime(double Seconds)
{
	++Stats.HugeObjectsStallingCount;
	Stats.HugeObjectStallingTimeInSeconds += Seconds;
}

inline void FNetSendStats::AddReplicationWasteTime(double Seconds)
{
	++Stats.ReplicationWasteObjectCount;
	Stats.ReplicationWasteTimeInSeconds += Seconds;
}

inline void FNetSendStats::SetNumberOfReplicatingConnections(uint32 Count)
{
	Stats.ReplicatingConnectionCount = Count;
}

}
