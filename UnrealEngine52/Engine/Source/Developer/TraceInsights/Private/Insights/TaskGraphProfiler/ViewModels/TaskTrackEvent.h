// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/TaskTrace.h"

// Insights
#include "Insights/TaskGraphProfiler/TaskGraphProfilerManager.h"
#include "Insights/ViewModels/TimingEvent.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTaskTrackEvent : public FTimingEvent
{
	INSIGHTS_DECLARE_RTTI(FTaskTrackEvent, FTimingEvent)

public:
	FTaskTrackEvent(const TSharedRef<const FBaseTimingTrack> InTrack, double InStartTime, double InEndTime, uint32 InDepth, ETaskEventType InTaskEventType);
	virtual ~FTaskTrackEvent() {}

	FString GetStartLabel() const;
	FString GetEndLabel() const;

	FString GetEventName() const;

	ETaskEventType GetTaskEventType() const { return TaskEventType; }

	TaskTrace::FId GetTaskId() const { return TaskId; }
	void SetTaskId(TaskTrace::FId InTaskId) { TaskId = InTaskId; }

private:
	ETaskEventType TaskEventType;
	TaskTrace::FId TaskId = TaskTrace::InvalidId;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
