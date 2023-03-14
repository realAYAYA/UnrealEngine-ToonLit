// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IRewindDebugger.h"
#include "IRewindDebuggerExtension.h"
#include "UObject/WeakObjectPtr.h"
#include "VLogRenderingActor.h"

// Rewind debugger extension for Visual Logger support

class FRewindDebuggerVLog : public IRewindDebuggerExtension
{
public:
	FRewindDebuggerVLog();
	virtual ~FRewindDebuggerVLog() {};

	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;

	virtual void RecordingStarted(IRewindDebugger* RewindDebugger) override;
	virtual void RecordingStopped(IRewindDebugger* RewindDebugger) override;

private:
	void AddLogEntries(const TArray<TSharedPtr<FDebugObjectInfo>>& Components, float StartTime, float EndTime, const class IVisualLoggerProvider* Provider);

	TWeakObjectPtr<AVLogRenderingActor> VLogActor; 
};
