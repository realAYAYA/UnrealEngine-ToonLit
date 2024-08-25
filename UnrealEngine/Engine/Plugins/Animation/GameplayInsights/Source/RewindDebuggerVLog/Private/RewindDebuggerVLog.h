// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IRewindDebugger.h"
#include "IRewindDebuggerExtension.h"
#include "UObject/WeakObjectPtr.h"
#include "VLogRenderingActor.h"
#include "ToolMenu.h"

// Rewind debugger extension for Visual Logger support

class FRewindDebuggerVLog : public IRewindDebuggerExtension
{
public:
	FRewindDebuggerVLog();
	virtual ~FRewindDebuggerVLog() {};

	void Initialize();
	void MakeCategoriesMenu(UToolMenu* Menu);
	void MakeLogLevelMenu(UToolMenu* Menu);
	void ToggleCategory(const FName& Category);
	bool IsCategoryActive(const FName& Category);

	ELogVerbosity::Type GetMinLogVerbosity() const;
	void SetMinLogVerbosity(ELogVerbosity::Type Value);
	
	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;

	virtual void RecordingStarted(IRewindDebugger* RewindDebugger) override;
	virtual void RecordingStopped(IRewindDebugger* RewindDebugger) override;

private:
	void AddLogEntries(const TArray<TSharedPtr<FDebugObjectInfo>>& Components, float StartTime, float EndTime, const class IVisualLoggerProvider* Provider);
	void ImmediateRender(const UObject* Object, const FVisualLogEntry& Entry);
	void RenderLogEntry(const FVisualLogEntry& Entry);

	AVLogRenderingActor* GetRenderingActor();

	TWeakObjectPtr<AVLogRenderingActor> VLogActor; 
};

