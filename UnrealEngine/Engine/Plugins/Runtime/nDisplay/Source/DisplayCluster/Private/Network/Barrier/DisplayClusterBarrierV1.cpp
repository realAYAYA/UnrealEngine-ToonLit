// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Barrier/DisplayClusterBarrierV1.h"

#include "Misc/DisplayClusterLog.h"

#include <chrono>


FDisplayClusterBarrierV1::FDisplayClusterBarrierV1(const uint32 InThreadsAmount, const uint32 InTimeout, const FString& InName)
	: Name(InName)
	, ThreadsAmount(InThreadsAmount)
	, ThreadsLeft(InThreadsAmount)
	, IterationCounter(0)
	, Timeout(InTimeout)
	, WatchdogTimer(FString("stub_watchdog_not_used"))
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Initialized barrier %s with timeout %u for threads count: %u"), *Name, Timeout, ThreadsAmount);
}

FDisplayClusterBarrierV1::~FDisplayClusterBarrierV1()
{
	// Free currently blocked threads
	Deactivate();
}


bool FDisplayClusterBarrierV1::Activate()
{
	std::unique_lock<std::mutex> lock{ Mutex };

	IterationCounter = 0;
	ThreadsLeft = ThreadsAmount;
	bEnabled = true;
	CondVar.notify_all();

	return true;
}

void FDisplayClusterBarrierV1::Deactivate()
{
	std::unique_lock<std::mutex> lock{ Mutex };

	bEnabled = false;
	CondVar.notify_all();
}

EDisplayClusterBarrierWaitResult FDisplayClusterBarrierV1::Wait(const FString& ThreadMarker, double* ThreadWaitTime /*= nullptr*/, double* BarrierWaitTime /*= nullptr*/)
{
	if (bEnabled == false)
	{
		UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s barrier is not active"), *Name);
		return EDisplayClusterBarrierWaitResult::NotActive;
	}

	const double ThreadWaitTimeStart = FPlatformTime::Seconds();

	{
		std::unique_lock<std::mutex> lock{ Mutex };

		size_t curIter = IterationCounter;

		if (ThreadsLeft == ThreadsAmount)
		{
			WaitTimeStart = FPlatformTime::Seconds();
			UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s barrier start time: %lf"), *Name, WaitTimeStart);
		}

		// Check if all threads are in front of the barrier
		if (--ThreadsLeft == 0)
		{
			UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s barrier trigger!"), *Name);
			++IterationCounter;
			ThreadsLeft = ThreadsAmount;

			WaitTimeFinish = FPlatformTime::Seconds();
			UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s barrier finish time: %lf"), *Name, WaitTimeFinish);

			WaitTimeOverall = WaitTimeFinish - WaitTimeStart;
			UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s barrier overall wait time: %lf"), *Name, WaitTimeOverall);

			// This is the last node. Unblock the barrier.
			CondVar.notify_all();
		}
		else
		{
			UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s barrier waiting, %u threads left"), *Name, ThreadsLeft);
			// Not all of threads have came here. Wait.
			if (!CondVar.wait_for(lock, std::chrono::milliseconds(Timeout), [this, curIter] { return curIter != IterationCounter || bEnabled == false; }))
			{
				UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s barrier waiting timeout"), *Name);
				return EDisplayClusterBarrierWaitResult::TimeOut;
			}
		}
	}

	const double ThreadWaitTimeFinish = FPlatformTime::Seconds();

	if (BarrierWaitTime)
	{
		*BarrierWaitTime = WaitTimeOverall;
	}

	if (ThreadWaitTime)
	{
		*ThreadWaitTime = ThreadWaitTimeFinish - ThreadWaitTimeStart;
	}

	// Go ahead
	return EDisplayClusterBarrierWaitResult::Ok;
}
