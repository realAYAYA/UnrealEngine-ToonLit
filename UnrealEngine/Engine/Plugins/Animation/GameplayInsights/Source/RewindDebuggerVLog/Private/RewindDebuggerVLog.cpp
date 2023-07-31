// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerVLog.h"
#include "IVisualLoggerProvider.h"
#include "Insights/IUnrealInsightsModule.h"
#include "IRewindDebugger.h"
#include "Modules/ModuleManager.h"
#include "TraceServices/Model/Frames.h"
#include "VisualLogger/VisualLogger.h"

#define LOCTEXT_NAMESPACE "RewindDebuggerVLog"

FRewindDebuggerVLog::FRewindDebuggerVLog()
{

}

void FRewindDebuggerVLog::AddLogEntries(const TArray<TSharedPtr<FDebugObjectInfo>>& Components, float StartTime, float EndTime, const IVisualLoggerProvider* VisualLoggerProvider)
{
	for(const TSharedPtr<FDebugObjectInfo>& ComponentInfo : Components)
	{
		VisualLoggerProvider->ReadVisualLogEntryTimeline(ComponentInfo->ObjectId, [this,StartTime, EndTime](const IVisualLoggerProvider::VisualLogEntryTimeline &TimelineData)
		{
			TimelineData.EnumerateEvents(StartTime, EndTime, [this](double InStartTime, double InEndTime, uint32 InDepth, const FVisualLogEntry& LogEntry)
			{
				VLogActor->AddLogEntry(LogEntry);
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		AddLogEntries(ComponentInfo->Children, StartTime, EndTime, VisualLoggerProvider);
	}
}

void FRewindDebuggerVLog::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
{
	if (RewindDebugger->IsPIESimulating() && !RewindDebugger->IsRecording())
	{
		// output debug rendering if we are paused, or actively recording.
		// otherwise clear the debug rendering actor to avoid any debug rendering being left over from last time we were scrubbing or recording
		if (VLogActor.IsValid())
		{
			VLogActor->Reset();
		}
	}
	else
	{
		if (const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession())
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);
			double CurrentTraceTime = RewindDebugger->CurrentTraceTime();

			const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(*Session);
			TraceServices::FFrame CurrentFrame;

			if(FrameProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, CurrentTraceTime, CurrentFrame))
			{
				if (const IVisualLoggerProvider* VisualLoggerProvider = Session->ReadProvider<IVisualLoggerProvider>("VisualLoggerProvider"))
				{
					if (!VLogActor.IsValid())
					{
						FActorSpawnParameters SpawnParameters;
						SpawnParameters.ObjectFlags |= RF_Transient;
						VLogActor = RewindDebugger->GetWorldToVisualize()->SpawnActor<AVLogRenderingActor>(SpawnParameters);
					}

					VLogActor->Reset();

					AddLogEntries(RewindDebugger->GetDebugComponents(), CurrentFrame.StartTime, CurrentFrame.EndTime, VisualLoggerProvider);
				}
			}
		}
	}
}

void FRewindDebuggerVLog::RecordingStarted(IRewindDebugger*)
{
	// start recording visual logger data
	FVisualLogger::Get().SetIsRecordingToTrace(true);
}

void FRewindDebuggerVLog::RecordingStopped(IRewindDebugger*)
{
	// stop recording visual logger data
	FVisualLogger::Get().SetIsRecordingToTrace(false);
}

#undef LOCTEXT_NAMESPACE