// Copyright Epic Games, Inc. All Rights Reserved.

#include "FrameStatsHelper.h"

#include "TraceServices/Model/TimingProfiler.h"

#include "Insights/InsightsManager.h"

namespace Insights
{

void FFrameStatsHelper::ComputeFrameStatsForTimer(TArray<FFrameStatsCachedEvent>& FrameStatsEvents, uint32 TimerId, const TSet<uint32>& Timelines)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		for (uint32 TimelineIndex : Timelines)
		{
			ProcessTimeline(FrameStatsEvents, TimerId, TimelineIndex);
		}
	}
}

void FFrameStatsHelper::ComputeFrameStatsForTimer(TArray<FFrameStatsCachedEvent>& FrameStatsEvents, uint32 TimerId)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		Session->ReadAccessCheck();

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		for (uint32 TimelineIndex = 0; TimelineIndex < TimingProfilerProvider.GetTimelineCount(); ++TimelineIndex)
		{
			ProcessTimeline(FrameStatsEvents, TimerId, TimelineIndex);
		}
	}
}

void FFrameStatsHelper::ProcessTimeline(TArray<FFrameStatsCachedEvent>& FrameStatsEvents, uint32 TimerId, uint32 TimelineIndex)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		Session->ReadAccessCheck();

		const double SessionDuration = Session->GetDurationSeconds();

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		TimingProfilerProvider.ReadTimeline(TimelineIndex,
			[SessionDuration, &FrameStatsEvents, TimerReader, TimerId](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
			{
				TraceServices::ITimeline<TraceServices::FTimingProfilerEvent>::EnumerateAsyncParams Params;
				Params.IntervalStart = 0;
				Params.IntervalEnd = SessionDuration;
				Params.Resolution = 0.0;
				Params.SetupCallback = [](uint32 NumTasks) {};
				Params.Callback = [TimerReader, &FrameStatsEvents, SessionDuration, TimerId](double StartTime, double EndTime, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event, uint32 TaskIndex)
				{
					const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
					if (ensure(Timer != nullptr))
					{
						if (Timer->Id == TimerId)
						{
							int32 Index = Algo::UpperBoundBy(FrameStatsEvents, StartTime, &FFrameStatsCachedEvent::FrameStartTime);
							if (Index > 0)
							{
								--Index;
							}

							// This can can happen when the event is between frames.
							if (StartTime > FrameStatsEvents[Index].FrameEndTime)
							{
								Index++;
								if (Index >= FrameStatsEvents.Num())
								{
									return TraceServices::EEventEnumerate::Continue;
								}
							}

							do
							{
								FFrameStatsCachedEvent& Entry = FrameStatsEvents[Index];

								if (EndTime < Entry.FrameStartTime)
								{
									return TraceServices::EEventEnumerate::Continue;
								}

								if (StartTime < Entry.FrameStartTime)
								{
									StartTime = Entry.FrameStartTime;
								}

								const double Duration = FMath::Min(EndTime, Entry.FrameEndTime) - StartTime;
								ensure(Duration >= 0.0f);
								for (double Value = Entry.Duration.load(); !Entry.Duration.compare_exchange_strong(Value, Value + Duration););

								Index++;
							} while (Index < FrameStatsEvents.Num());
						}
					}
					return TraceServices::EEventEnumerate::Continue;
				};

				Timeline.EnumerateEventsDownSampledAsync(Params);
			});
	}
}

// namespace Insights
};

