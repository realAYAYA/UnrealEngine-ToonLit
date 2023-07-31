// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class IDisplayClusterRenderSyncPolicy;


/**
 * V-blank monitor. It continuously polls the current sync policy
 * for V-blanks and reports the time data to utraces.
 */
class FDisplayClusterVBlankMonitor
	: protected FRunnable
{
public:
	FDisplayClusterVBlankMonitor(IDisplayClusterRenderSyncPolicy& SyncPolicy);
	~FDisplayClusterVBlankMonitor();

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FRunnable
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual uint32 Run() override;
	virtual void Stop() override final;

private:
	// Holds the thread object
	TUniquePtr<FRunnableThread> WorkingThread;

	// Current sync policy
	IDisplayClusterRenderSyncPolicy& SyncPolicy;

	// V-blank counter
	uint64 VBlankCounter = 0;

	// Flag to ask working thread to terminate
	bool bThreadExitRequested = false;

	// Access control mutex
	mutable FCriticalSection CritSecInternals;
};
