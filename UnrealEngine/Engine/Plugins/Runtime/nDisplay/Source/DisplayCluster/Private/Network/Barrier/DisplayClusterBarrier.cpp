// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Barrier/DisplayClusterBarrier.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterBarrier::FDisplayClusterBarrier(const FString& InName, const TArray<FString>& InCallersAllowed, const uint32 InTimeout)
	: Name(InName)
	, CallersAllowed(InCallersAllowed)
	, Timeout(InTimeout)
	, ThreadLimit(static_cast<uint32>(CallersAllowed.Num()))
	, WatchdogTimer(InName + FString("_watchdog"))
{
	UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Initialized barrier '%s' with timeout %u ms and threads limit: %u"), *Name, Timeout, ThreadLimit);

	for (int32 Idx = 0; Idx < CallersAllowed.Num(); ++Idx)
	{
		UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': client (%d): '%s'"), *Name, Idx, *CallersAllowed[Idx]);
	}

	// Subscribe for timeout events
	WatchdogTimer.OnWatchdogTimeOut().AddRaw(this, &FDisplayClusterBarrier::HandleBarrierTimeout);
}

FDisplayClusterBarrier::~FDisplayClusterBarrier()
{
	// Release threads if there any
	Deactivate();
}


bool FDisplayClusterBarrier::Activate()
{
	FScopeLock Lock(&DataCS);

	UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Barrier '%s': activating..."), *Name);

	if (!bActive)
	{
		ThreadCount = 0;
		bActive = true;
		CallersAwaiting.Reset();

		// No exit allowed
		EventOutputGateOpen->Reset();
		// Allow join
		EventInputGateOpen->Trigger();
	}

	return true;
}

void FDisplayClusterBarrier::Deactivate()
{
	FScopeLock Lock(&DataCS);

	if (bActive)
	{
		UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Barrier '%s': deactivating..."), *Name);

		bActive = false;

		// Release all threads that are currently at the barrier
		EventInputGateOpen->Trigger();
		EventOutputGateOpen->Trigger();

		// No more threads awaiting
		CallersAwaiting.Reset();

		// And reset timer of course
		WatchdogTimer.ResetTimer();
	}
}

bool FDisplayClusterBarrier::IsActivated() const
{
	FScopeLock Lock(&DataCS);
	return bActive;
}


EDisplayClusterBarrierWaitResult FDisplayClusterBarrier::Wait(const FString& CallerId, double* ThreadWaitTime /*= nullptr*/, double* BarrierWaitTime /*= nullptr*/)
{
	UE_LOG(LogDisplayClusterBarrier, VeryVerbose, TEXT("Barrier '%s': caller arrived '%s'"), *Name, *CallerId);

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
			if (CallersTimedout.Contains(CallerId))
			{
				UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': caller '%s' not allowed to join, it has been timed out previously"), *Name, *CallerId);
				return EDisplayClusterBarrierWaitResult::TimeOut;
			}

			// Check if the barrier is active
			if (!bActive)
			{
				UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': not active"), *Name);
				return EDisplayClusterBarrierWaitResult::NotActive;
			}

			// Check if this thread is allowed to sync at this barrier
			if (!CallersAllowed.Contains(CallerId))
			{
				UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': caller '%s' not allowed to join, no permission"), *Name, *CallerId);
				return EDisplayClusterBarrierWaitResult::NotAllowed;
			}

			// Register caller
			CallersAwaiting.Add(CallerId);

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
					UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': sync start"), *Name);

					// Prepare for new sync iteration
					HandleBarrierPreSyncStart();

					BarrierWaitTimeStart = ThreadWaitTimeStart;
					WatchdogTimer.SetTimer(Timeout);
				}

				UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': awaiting threads amount - %d"), *Name, ThreadCount);

				// In case this thread is the last one the barrier is awaiting for, we need:
				// - to fixate barrier awaiting finish time
				// - to open the output gate (release the barrier)
				// - to close the input gate
				// - reset watchdog timer
				if (ThreadCount == ThreadLimit)
				{
					BarrierWaitTimeFinish = FPlatformTime::Seconds();
					BarrierWaitTimeOverall = BarrierWaitTimeFinish - BarrierWaitTimeStart;

					UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': sync end, barrier wait time %f"), *Name, BarrierWaitTimeOverall);

					// All callers are here, pet the watchdog
					WatchdogTimer.ResetTimer();

					// Process sync done before allowing the threads to leave
					HandleBarrierPreSyncEnd();

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
		UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': caller '%s' is leaving the barrier"), *Name, *CallerId);

		{
			FScopeLock LockData(&DataCS);

			// Unregister caller
			CallersAwaiting.Remove(CallerId);

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

	UE_LOG(LogDisplayClusterBarrier, VeryVerbose, TEXT("Barrier '%s': caller left '%s'"), *Name, *CallerId);

	return EDisplayClusterBarrierWaitResult::Ok;
}

EDisplayClusterBarrierWaitResult FDisplayClusterBarrier::WaitWithData(const FString& ThreadMarker, const TArray<uint8>& RequestData, TArray<uint8>& OutResponseData, double* OutThreadWaitTime /* = nullptr */, double* OutBarrierWaitTime /* = nullptr */)
{
	{
		FScopeLock Lock(&CommDataCS);
		// Store request data so it can be used once all the threads arrived
		ClientsRequestData.Emplace(ThreadMarker, RequestData);
	}

	// Wait at the barrier
	const EDisplayClusterBarrierWaitResult WaitResult = Wait(ThreadMarker, OutThreadWaitTime, OutBarrierWaitTime);

	{
		FScopeLock Lock(&CommDataCS);

		if (TArray<uint8>* const FoundThreadResponseData = ClientsResponseData.Find(ThreadMarker))
		{
			OutResponseData = MoveTemp(*FoundThreadResponseData);
			ClientsResponseData.Remove(ThreadMarker);
		}
	}

	return WaitResult;
}

void FDisplayClusterBarrier::UnregisterSyncCaller(const FString& CallerId)
{
	UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Barrier '%s': unregistering caller '%s'..."), *Name, *CallerId);

	FScopeLock Lock(&DataCS);

	// Remove specified callers from the management lists
	if (CallersAllowed.Remove(CallerId) > 0 || CallersTimedout.Remove(CallerId) > 0)
	{
		// Update thread limit
		ThreadLimit = static_cast<uint32>(CallersAllowed.Num());

		UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Barrier '%s': caller '%s' has been unregistered, new thread limit is %u"), *Name, *CallerId, ThreadLimit);

		// In case it's a last missing caller, we need to open the barrier
		if (ThreadCount == ThreadLimit)
		{
			BarrierWaitTimeFinish  = FPlatformTime::Seconds();
			BarrierWaitTimeOverall = BarrierWaitTimeFinish - BarrierWaitTimeStart;

			UE_LOG(LogDisplayClusterBarrier, Verbose, TEXT("Barrier '%s': sync end, barrier wait time %f"), *Name, BarrierWaitTimeOverall);

			// All callers here, pet the watchdog
			WatchdogTimer.ResetTimer();

			// Close the input gate, and open the output gate
			EventInputGateOpen->Reset();
			EventOutputGateOpen->Trigger();
		}
	}
	else
	{
		UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Barrier '%s': caller '%s' not found"), *Name, *CallerId);
	}
}

void FDisplayClusterBarrier::HandleBarrierPreSyncStart()
{
}

void FDisplayClusterBarrier::HandleBarrierPreSyncEnd()
{
	// Execute sync delegate with data requested
	BarrierPreSyncEndDelegate.ExecuteIfBound(Name, ClientsRequestData, ClientsResponseData);

	// We can clean request data now before next iteration
	ClientsRequestData.Empty(CallersAllowed.Num());
}

void FDisplayClusterBarrier::HandleBarrierTimeout()
{
	// Being here means some callers have not come to the barrier yet during specific time period. Those
	// missing callers will be considered as the lost ones. The barrier will continue working with the remaining callers only.

	FScopeLock Lock(&DataCS);

	UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Barrier '%s': Time out! %d callers missing"), *Name, CallersAllowed.Num() - CallersAwaiting.Num());

	// First of all, update the time variables
	BarrierWaitTimeFinish = FPlatformTime::Seconds();
	BarrierWaitTimeOverall = BarrierWaitTimeFinish - BarrierWaitTimeStart;

	// List of timed out threads
	TArray<FString> CallersTimedOutOnLastSync;

	// Update the list of timed out threads. Copy all missing callers to the list of timed out ones.
	for (const FString& CallerAllowed : CallersAllowed)
	{
		if (!CallersAwaiting.Contains(CallerAllowed))
		{
			UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Barrier '%s': caller '%s' was moved to the 'TimedOut' list"), *Name, *CallerAllowed);
			CallersTimedOutOnLastSync.Add(CallerAllowed);
		}
	}

	// Update timedout list
	CallersTimedout.Append(CallersTimedOutOnLastSync);
	// Update the list of permitted callers
	CallersAllowed = CallersAwaiting;
	// Update thread limit
	ThreadLimit = ThreadCount; // Same as CallersAllowed.Num()

	UE_LOG(LogDisplayClusterBarrier, Log, TEXT("Barrier '%s': new threads limit %d"), *Name, ThreadLimit);

	// Notify listeners
	OnBarrierTimeout().Broadcast(Name, CallersTimedOutOnLastSync);

	// Close the input gate, and open the output gate to let the remaining callers go
	EventInputGateOpen->Reset();
	EventOutputGateOpen->Trigger();
}
