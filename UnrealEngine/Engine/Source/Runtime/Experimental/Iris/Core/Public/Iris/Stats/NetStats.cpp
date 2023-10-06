// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Stats/NetStats.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Misc/ScopeLock.h"

namespace UE::Net
{

// Enable Iris category by default on servers
CSV_DEFINE_CATEGORY(Iris, WITH_SERVER_CODE);

void FNetSendStats::Accumulate(const FNetSendStats& Other)
{
	FScopeLock Lock(&CS);

	Stats.ScheduledForReplicationRootObjectCount += Other.Stats.ScheduledForReplicationRootObjectCount;
	Stats.ReplicatedRootObjectCount += Other.Stats.ReplicatedRootObjectCount;
	Stats.ReplicatedObjectCount += Other.Stats.ReplicatedObjectCount;
	Stats.ReplicatedDestructionInfoCount += Other.Stats.ReplicatedDestructionInfoCount;
	Stats.DeltaCompressedObjectCount += Other.Stats.DeltaCompressedObjectCount;
	Stats.ReplicationWasteObjectCount += Other.Stats.ReplicationWasteObjectCount;
	Stats.ReplicatedObjectStatesMaskedOut += Other.Stats.ReplicatedObjectStatesMaskedOut;
	Stats.ActiveHugeObjectCount += Other.Stats.ActiveHugeObjectCount;
	Stats.HugeObjectsWaitingForAckCount += Other.Stats.HugeObjectsWaitingForAckCount;
	Stats.HugeObjectsStallingCount += Other.Stats.HugeObjectsStallingCount;

	Stats.ReplicationWasteTimeInSeconds += Other.Stats.ReplicationWasteTimeInSeconds;
	Stats.HugeObjectWaitingForAckTimeInSeconds += Other.Stats.HugeObjectWaitingForAckTimeInSeconds;
	Stats.HugeObjectStallingTimeInSeconds += Other.Stats.HugeObjectStallingTimeInSeconds;
}

void FNetSendStats::Reset()
{
	FScopeLock Lock(&CS);
	Stats = FStats();
}

void FNetSendStats::ReportCsvStats()
{
#if CSV_PROFILER
	FScopeLock Lock(&CS);

	// Calculate connection averages for some stats
	if (Stats.ReplicatingConnectionCount > 0)
	{
		const float ConnectionCountFloat = float(Stats.ReplicatingConnectionCount);
		CSV_CUSTOM_STAT(Iris, AvgScheduledForReplicationRootObjectCount, Stats.ScheduledForReplicationRootObjectCount/ConnectionCountFloat, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicatedRootObjectCount, Stats.ReplicatedRootObjectCount/ConnectionCountFloat, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicatedObjectCount, Stats.ReplicatedObjectCount/ConnectionCountFloat, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicatedDestructionInfoCount, Stats.ReplicatedDestructionInfoCount/ConnectionCountFloat, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicationWasteObjectCount, Stats.ReplicationWasteObjectCount/ConnectionCountFloat, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicatedObjectStatesMaskedOut, Stats.ReplicatedObjectStatesMaskedOut/ConnectionCountFloat, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgDeltaCompressedObjectCount, Stats.DeltaCompressedObjectCount/ConnectionCountFloat, ECsvCustomStatOp::Set);
	}
	else
	{
		CSV_CUSTOM_STAT(Iris, AvgScheduledForReplicationRootObjectCount, 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicatedRootObjectCount, 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicatedObjectCount, 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicatedDestructionInfoCount, 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicatedHugeObjectCount, 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgReplicationWasteObjectCount, 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Iris, AvgDeltaCompressedObjectCount, 0, ECsvCustomStatOp::Set);
	}

	// Huge object counts
	CSV_CUSTOM_STAT(Iris, ActiveHugeObjectCount, Stats.ActiveHugeObjectCount, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(Iris, HugeObjectsWaitingForAckCount, Stats.HugeObjectsWaitingForAckCount, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(Iris, HugeObjectsStallingCount, Stats.HugeObjectsStallingCount, ECsvCustomStatOp::Set);

	CSV_CUSTOM_STAT(Iris, ReplicatingConnectionCount, Stats.ReplicatingConnectionCount, ECsvCustomStatOp::Set);

	CSV_CUSTOM_STAT(Iris, ReplicationWasteTimeMilliseconds, Stats.ReplicationWasteTimeInSeconds*1000.0, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(Iris, HugeObjectWaitingForAckTimeInSeconds, Stats.HugeObjectWaitingForAckTimeInSeconds, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(Iris, HugeObjectStallingTimeInSeconds, Stats.HugeObjectStallingTimeInSeconds, ECsvCustomStatOp::Set);
#endif
}

}
