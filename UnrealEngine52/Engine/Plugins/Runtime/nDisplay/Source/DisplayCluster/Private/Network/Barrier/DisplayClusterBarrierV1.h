// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/Barrier/IDisplayClusterBarrier.h"
#include "Misc/DisplayClusterWatchdogTimer.h"

#include <condition_variable>
#include <mutex>


/**
 * Thread barrier v1
 */
class FDisplayClusterBarrierV1
	: public IDisplayClusterBarrier
{
public:
	FDisplayClusterBarrierV1(const uint32 ThreadsAmount, const uint32 Timeout, const FString& Name);
	virtual  ~FDisplayClusterBarrierV1();

public:
	// Barrier name
	virtual const FString& GetName() const override
	{
		return Name;
	}

	// Activate barrier
	virtual bool Activate() override;
	// Deactivate barrier, no threads will be blocked
	virtual void Deactivate() override;
	// Wait until all threads arrive
	virtual EDisplayClusterBarrierWaitResult Wait(const FString& ThreadMarker, double* OutThreadWaitTime = nullptr, double* OutBarrierWaitTime = nullptr) override;

	// Remove specified node from sync pipeline
	virtual void UnregisterSyncNode(const FString& NodeId) override
	{
		/* Not supported in barriers v1 */
	}

	// Barrier timout notification
	virtual FDisplayClusterBarrierTimeoutEvent& OnBarrierTimeout() override
	{
		return BarrierTimeoutEvent;
	}

private:
	// Barrier name for logging
	const FString Name;

	// Barrier state
	bool bEnabled = true;

	// Amount of threads to wait at the barrier
	const uint32 ThreadsAmount;
	// Waiting threads amount
	uint32 ThreadsLeft;
	// Iteration counter (kind of barrier sync transaction)
	size_t IterationCounter;

	std::condition_variable CondVar;
	mutable std::mutex Mutex;

	uint32 Timeout = 0;

	// Watchdog timer to detect barrier waiting timeouts
	FDisplayClusterWatchdogTimer WatchdogTimer;

	// Barrier timeout event
	FDisplayClusterBarrierTimeoutEvent BarrierTimeoutEvent;

	double WaitTimeStart   = 0;
	double WaitTimeFinish  = 0;
	double WaitTimeOverall = 0;
};
