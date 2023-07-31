// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisualLoggerProvider.h"

FName FVisualLoggerProvider::ProviderName("VisualLoggerProvider");

#define LOCTEXT_NAMESPACE "VisualLoggerProvider"

FVisualLoggerProvider::FVisualLoggerProvider(TraceServices::IAnalysisSession& InSession)
	: Session(InSession)
{
}

bool FVisualLoggerProvider::ReadVisualLogEntryTimeline(uint64 InObjectId, TFunctionRef<void(const VisualLogEntryTimeline&)> Callback) const
{
	Session.ReadAccessCheck();

	const uint32* IndexPtr = ObjectIdToLogEntryTimelines.Find(InObjectId);
	if(IndexPtr != nullptr)
	{
		if (*IndexPtr < uint32(LogEntryTimelines.Num()))
		{
			Callback(*LogEntryTimelines[*IndexPtr]);
			return true;
		}
	}

	return false;
}

void  FVisualLoggerProvider::AppendVisualLogEntry(uint64 InObjectId, double InTime, const FVisualLogEntry& Entry)
{
	Session.WriteAccessCheck();

	TSharedPtr<TraceServices::TPointTimeline<FVisualLogEntry>> Timeline;
	uint32* IndexPtr = ObjectIdToLogEntryTimelines.Find(InObjectId);
	if(IndexPtr != nullptr)
	{
		Timeline = LogEntryTimelines[*IndexPtr];
	}
	else
	{
		Timeline = MakeShared<TraceServices::TPointTimeline<FVisualLogEntry>>(Session.GetLinearAllocator());
		ObjectIdToLogEntryTimelines.Add(InObjectId, LogEntryTimelines.Num());
		LogEntryTimelines.Add(Timeline.ToSharedRef());
	}

	Timeline->AppendEvent(InTime, Entry);
}

#undef LOCTEXT_NAMESPACE
