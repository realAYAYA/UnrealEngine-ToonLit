// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterVblankMonitor.h"

#include "Render/Synchronization/IDisplayClusterRenderSyncPolicy.h"
#include "HAL/RunnableThread.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterVBlankMonitor::FDisplayClusterVBlankMonitor(IDisplayClusterRenderSyncPolicy& InSyncPolicy)
	: SyncPolicy(InSyncPolicy)
{
	WorkingThread.Reset(FRunnableThread::Create(this, TEXT("VBLANK_Monitoring_thread"), 32 * 1024, TPri_AboveNormal));
}

FDisplayClusterVBlankMonitor::~FDisplayClusterVBlankMonitor()
{
	// Stop working thread
	Stop();
}

uint32 FDisplayClusterVBlankMonitor::Run()
{
	UE_LOG(LogDisplayClusterRenderSync, Log, TEXT("V-blank monitoring thread has started"));

	while (!bThreadExitRequested)
	{
		if (SyncPolicy.WaitForVBlank())
		{
			TRACE_BOOKMARK(TEXT("VBLANK %llu"), VBlankCounter);
			++VBlankCounter;
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(VBLANK - not available);
			FPlatformProcess::Sleep(1.f);
		}
	}

	UE_LOG(LogDisplayClusterRenderSync, Log, TEXT("V-blank monitoring thread has finished"));

	return 0;
}

void FDisplayClusterVBlankMonitor::Stop()
{
	FScopeLock Lock(&CritSecInternals);

	// Let the working thread know it has to stop
	bThreadExitRequested = true;

	// Wait unless working thread is finished
	if (WorkingThread)
	{
		WorkingThread->WaitForCompletion();
		WorkingThread.Reset();
	}
}
