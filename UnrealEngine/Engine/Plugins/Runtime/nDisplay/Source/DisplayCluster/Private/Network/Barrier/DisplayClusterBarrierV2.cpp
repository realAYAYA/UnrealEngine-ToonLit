// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Barrier/DisplayClusterBarrierV2.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterBarrierV2::FDisplayClusterBarrierV2(const TArray<FString>& InNodesAllowed, const uint32 InTimeout, const FString& InName)
	: Name(InName)
	, NodesAllowed(InNodesAllowed)
	, Timeout(InTimeout)
	, ThreadLimit(static_cast<uint32>(NodesAllowed.Num()))
	, WatchdogTimer(InName + FString("_watchdog"))
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Initialized barrier %s with timeout %u ms and threads limit: %u"), *Name, Timeout, ThreadLimit);

	// Subscribe for timeout events
	WatchdogTimer.OnWatchdogTimeOut().AddRaw(this, &FDisplayClusterBarrierV2::HandleBarrierTimeout);
}

FDisplayClusterBarrierV2::~FDisplayClusterBarrierV2()
{
	// Release threads if there any
	Deactivate();
}


bool FDisplayClusterBarrierV2::Activate()
{
	FScopeLock Lock(&DataCS);

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Barrier %s: activating..."), *Name);

	if (!bActive)
	{
		ThreadCount = 0;
		bActive = true;
		NodesAwaiting.Reset();

		// No exit allowed
		EventOutputGateOpen->Reset();
		// Allow join
		EventInputGateOpen->Trigger();
	}

	return true;
}

void FDisplayClusterBarrierV2::Deactivate()
{
	FScopeLock Lock(&DataCS);

	if (bActive)
	{
		UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Barrier %s: deactivating..."), *Name);

		bActive = false;

		// Release all threads that are currently at the barrier
		EventInputGateOpen->Trigger();
		EventOutputGateOpen->Trigger();

		// No more threads awaiting
		NodesAwaiting.Reset();

		// And reset timer of course
		WatchdogTimer.ResetTimer();
	}
}

EDisplayClusterBarrierWaitResult FDisplayClusterBarrierV2::Wait(const FString& NodeId, double* ThreadWaitTime /*= nullptr*/, double* BarrierWaitTime /*= nullptr*/)
{
	double ThreadWaitTimeStart = 0;

	{
		FScopeLock LockEntrance(&EntranceCS);

		// Wait unless the barrier allows new threads to join. This will happen
		// once all threads from the previous sync iteration leave the barrier,
		// or the barrier gets deactivated.
		EventInputGateOpen->Wait();

		{
			FScopeLock LockData(&DataCS);

			// Check if this thread has been previously dropped
			if (NodesTimedout.Contains(NodeId))
			{
				UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("Barrier %s: node %s not allowed to join, it has been timed out previously"), *Name, *NodeId);
				return EDisplayClusterBarrierWaitResult::TimeOut;
			}

			// Check if the barrier is active
			if (!bActive)
			{
				UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("Barrier %s: not active"), *Name);
				return EDisplayClusterBarrierWaitResult::NotActive;
			}

			// Check if this thread is allowed to sync at this barrier
			if (!NodesAllowed.Contains(NodeId))
			{
				UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("Barrier %s: node %s not allowed to join, no permission"), *Name, *NodeId);
				return EDisplayClusterBarrierWaitResult::NotAllowed;
			}

			// Register node
			NodesAwaiting.Add(NodeId);

			// Make sure the barrier was not deactivated while this thread was waiting at the input gate
			if (bActive)
			{
				// Fixate awaiting start for this particular thread
				ThreadWaitTimeStart = FPlatformTime::Seconds();

				// One more thread awaiting
				++ThreadCount;

				// In case this thread came first to the barrier, we need:
				// - to fixate barrier awaiting start time
				// - start watchdog timer
				if (ThreadCount == 1)
				{
					UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("Barrier %s: sync start"), *Name);

					BarrierWaitTimeStart = ThreadWaitTimeStart;
					WatchdogTimer.SetTimer(Timeout);
				}

				UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("Barrier %s: awaiting threads amount - %d"), *Name, ThreadCount);

				// In case this thread is the last one the barrier is awaiting for, we need:
				// - to fixate barrier awaiting finish time
				// - to open the output gate (release the barrier)
				// - to close the input gate
				// - reset watchdog timer
				if (ThreadCount == ThreadLimit)
				{
					BarrierWaitTimeFinish = FPlatformTime::Seconds();
					BarrierWaitTimeOverall = BarrierWaitTimeFinish - BarrierWaitTimeStart;

					UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("Barrier %s: sync end, barrier wait time %f"), *Name, BarrierWaitTimeOverall);

					// All nodes here, pet the watchdog
					WatchdogTimer.ResetTimer();

					// Close input gate, open output gate
					EventInputGateOpen->Reset();
					EventOutputGateOpen->Trigger();
				}
			}
		}
	}

	// Wait for the barrier to open
	EventOutputGateOpen->Wait();

	// Fixate awaiting finish for this particular thread
	const double ThreadWaitTimeFinish = FPlatformTime::Seconds();

	{
		UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("Barrier %s: node %s is leaving the barrier"), *Name, *NodeId);

		{
			FScopeLock LockData(&DataCS);

			// Unregister node
			NodesAwaiting.Remove(NodeId);

			// Make sure the barrier was not deactivated while this thread was waiting at the output gate
			if (bActive)
			{
				// In case this thread is leaving last, close output and open input
				if (--ThreadCount == 0)
				{
					EventOutputGateOpen->Reset();
					EventInputGateOpen->Trigger();
				}
			}
		}
	}

	// Export barrier overall waiting time
	if (BarrierWaitTime)
	{
		*BarrierWaitTime = BarrierWaitTimeOverall;
	}

	// Export thread waiting time
	if (ThreadWaitTime)
	{
		*ThreadWaitTime = ThreadWaitTimeFinish - ThreadWaitTimeStart;
	}

	return EDisplayClusterBarrierWaitResult::Ok;
}

void FDisplayClusterBarrierV2::UnregisterSyncNode(const FString& NodeId)
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Barrier %s: unregistering node %s..."), *Name, *NodeId);

	FScopeLock Lock(&DataCS);

	// Remove specified nodes from the management lists
	if (NodesAllowed.Remove(NodeId) > 0 || NodesTimedout.Remove(NodeId) > 0)
	{
		// Update thread limit
		ThreadLimit = static_cast<uint32>(NodesAllowed.Num());

		UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Barrier %s: node %s has been unregistered, new thread limit is %u"), *Name, *NodeId, ThreadLimit);

		// In case it's a last missing node, we need to open the barrier
		if (ThreadCount == ThreadLimit)
		{
			BarrierWaitTimeFinish  = FPlatformTime::Seconds();
			BarrierWaitTimeOverall = BarrierWaitTimeFinish - BarrierWaitTimeStart;

			UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("Barrier %s: sync end, barrier wait time %f"), *Name, BarrierWaitTimeOverall);

			// All nodes here, pet the watchdog
			WatchdogTimer.ResetTimer();

			// Close the input gate, and open the output gate
			EventInputGateOpen->Reset();
			EventOutputGateOpen->Trigger();
		}
	}
	else
	{
		UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Barrier %s: node %s not found"), *Name, *NodeId);
	}
}

void FDisplayClusterBarrierV2::HandleBarrierTimeout()
{
	// Being here means some nodes have not come to the barrier yet during specific time period. Those
	// missing nodes will be considered as the lost ones. The barrier will continue working with the remaining nodes only.

	FScopeLock Lock(&DataCS);

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Barrier %s: Time out! %d nodes missing"), *Name, NodesAllowed.Num() - NodesAwaiting.Num());

	// First of all, update the time variables
	BarrierWaitTimeFinish = FPlatformTime::Seconds();
	BarrierWaitTimeOverall = BarrierWaitTimeFinish - BarrierWaitTimeStart;

	// List of timed out threads
	TArray<FString> NodesTimedOutOnLastSync;

	// Update the list of timed out threads. Copy all missing nodes to the list of timed out ones.
	for (const FString& NodeAllowed : NodesAllowed)
	{
		if (!NodesAwaiting.Contains(NodeAllowed))
		{
			UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Barrier %s: node %s was moved to the 'TimedOut' list"), *Name, *NodeAllowed);
			NodesTimedOutOnLastSync.Add(NodeAllowed);
		}
	}

	// Update timedout list
	NodesTimedout.Append(NodesTimedOutOnLastSync);
	// Update the list of permitted nodes
	NodesAllowed = NodesAwaiting;
	// Update thread limit
	ThreadLimit = ThreadCount; // Same as NodesAllowed.Num()

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Barrier %s: new threads limit %d"), *Name, ThreadLimit);

	// Notify listeners
	OnBarrierTimeout().Broadcast(Name, NodesTimedOutOnLastSync);

	// Close the input gate, and open the output gate to let the remaining nodes go
	EventInputGateOpen->Reset();
	EventOutputGateOpen->Trigger();
}
