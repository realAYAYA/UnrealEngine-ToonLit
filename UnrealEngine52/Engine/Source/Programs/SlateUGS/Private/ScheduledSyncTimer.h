// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Runnable.h"
#include "Templates/Function.h"
#include "Misc/DateTime.h"


class ScheduledSyncTimer : FRunnable
{
public:
	~ScheduledSyncTimer();

	void Start(const FDateTime& InScheduledTime, const TFunction<void()>& InTimerElapsedCallback);
	void Stop();

private:
	virtual uint32 Run() override;

	bool bFinished = false;
	
	FDateTime ScheduledTime;
	FRunnableThread* WorkerThread = nullptr;
	TFunction<void()> TimerElapsedCallback;
};
