// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/Barrier/IDisplayClusterBarrier.h"
#include "Misc/DisplayClusterWatchdogTimer.h"

#include "HAL/Event.h"


/**
 * Thread barrier v2
 */
class FDisplayClusterBarrierV2
	: public IDisplayClusterBarrier
{
public:
	FDisplayClusterBarrierV2(const TArray<FString>& NodesAllowed, const uint32 Timeout, const FString& Name);
	virtual  ~FDisplayClusterBarrierV2();

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
	// Wait until all node threads arrived
	virtual EDisplayClusterBarrierWaitResult Wait(const FString& NodeId, double* OutThreadWaitTime = nullptr, double* OutBarrierWaitTime = nullptr) override;

	// Remove specified node from sync pipeline
	virtual void UnregisterSyncNode(const FString& NodeId) override;

	// Barrier timout notification
	virtual FDisplayClusterBarrierTimeoutEvent& OnBarrierTimeout() override
	{
		return BarrierTimeoutEvent;
	}

private:
	// Handler for barrier timouts
	void HandleBarrierTimeout();

private:
	// Barrier name
	const FString Name;
	// Barrier state
	bool bActive = false;


	// Cluster nodes that are allowed to join the barrier
	TArray<FString> NodesAllowed;

	// Cluster nodes that are already waiting at the barrier
	TArray<FString> NodesAwaiting;

	// Cluster nodes that have been timed out previously
	TArray<FString> NodesTimedout;


	// Timeout for the barrier
	const uint32 Timeout = 0;

	// Overall amount of threads to wait at the barrier
	uint32 ThreadLimit = 0;

	// Amount of threads at the barrier
	uint32 ThreadCount = 0;


	// Events to control when threads can join (input) and leave (output) the barrier
	FEventRef EventInputGateOpen { EEventMode::ManualReset };
	FEventRef EventOutputGateOpen{ EEventMode::ManualReset };

	// Watchdog timer to detect barrier waiting timeouts
	FDisplayClusterWatchdogTimer WatchdogTimer;

	// Barrier timeout event
	FDisplayClusterBarrierTimeoutEvent BarrierTimeoutEvent;

	// Diagnostics data
	double BarrierWaitTimeStart   = 0;
	double BarrierWaitTimeFinish  = 0;
	double BarrierWaitTimeOverall = 0;

	// Barrier state CS
	mutable FCriticalSection DataCS;
	// Barrier entrance CS
	mutable FCriticalSection EntranceCS;
};
