// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimingGraphTrack.h"

#include <limits>

#include "TraceServices/Model/Frames.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "TraceServices/Model/Counters.h"

// Insights
#include "Insights/Common/PaintUtils.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsManager.h"
#include "Insights/TimingProfilerManager.h"
#include "Insights/ViewModels/AxisViewportDouble.h"
#include "Insights/ViewModels/FrameStatsHelper.h"
#include "Insights/ViewModels/GraphTrackBuilder.h"
#include "Insights/ViewModels/ITimingViewDrawHelper.h"
#include "Insights/ViewModels/TimingTrackViewport.h"
#include "Insights/ViewModels/ThreadTimingTrack.h"
#include "Insights/Widgets/STimingProfilerWindow.h"
#include "Insights/Widgets/STimersView.h"
#include "Insights/Widgets/STimingView.h"

#define LOCTEXT_NAMESPACE "GraphTrack"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimingGraphSeries
////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingGraphSeries::FTimingGraphSeries(FTimingGraphSeries::ESeriesType InType)
	: FGraphSeries()
	, Type(InType)
	, TimerId(0)
	, CachedSessionDuration(0.0)
	, CachedEvents()
	, bIsTime(InType == FTimingGraphSeries::ESeriesType::Frame || InType == FTimingGraphSeries::ESeriesType::Timer)
	, bIsMemory(false)
	, bIsFloatingPoint(false)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingGraphSeries::~FTimingGraphSeries()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FTimingGraphSeries::FormatValue(double Value) const
{
	switch (Type)
	{
	case FTimingGraphSeries::ESeriesType::Frame:
		return FString::Printf(TEXT("%s (%g fps)"), *TimeUtils::FormatTimeAuto(Value), 1.0 / Value);

	case FTimingGraphSeries::ESeriesType::Timer:
	case FTimingGraphSeries::ESeriesType::FrameStatsTimer:
		return TimeUtils::FormatTimeAuto(Value);

	case FTimingGraphSeries::ESeriesType::StatsCounter:
		if (bIsTime)
		{
			return TimeUtils::FormatTimeAuto(Value);
		}
		else if (bIsMemory)
		{
			const int64 MemValue = static_cast<int64>(Value);
			if (MemValue > 0)
			{
				if (MemValue < 1024)
				{
					return FString::Printf(TEXT("%s bytes"), *FText::AsNumber(MemValue).ToString());
				}
				else
				{
					FNumberFormattingOptions FormattingOptions;
					FormattingOptions.MaximumFractionalDigits = 2;
					return FString::Printf(TEXT("%s (%s bytes)"), *FText::AsMemory(MemValue, &FormattingOptions).ToString(), *FText::AsNumber(MemValue).ToString());
				}
			}
			else if (MemValue == 0)
			{
				return TEXT("0");
			}
			else
			{
				if (-MemValue < 1024)
				{
					return FString::Printf(TEXT("-%s bytes"), *FText::AsNumber(-MemValue).ToString());
				}
				else
				{
					FNumberFormattingOptions FormattingOptions;
					FormattingOptions.MaximumFractionalDigits = 2;
					return FString::Printf(TEXT("-%s (-%s bytes)"), *FText::AsMemory(-MemValue, &FormattingOptions).ToString(), *FText::AsNumber(-MemValue).ToString());
				}
			}
		}
		else if (bIsFloatingPoint)
		{
			return FString::Printf(TEXT("%g"), Value);
		}
		else
		{
			const int64 Int64Value = static_cast<int64>(Value);
			return FText::AsNumber(Int64Value).ToString();
		}
	}

	return FGraphSeries::FormatValue(Value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphSeries::SetVisibility(bool bOnOff)
{
	FGraphSeries::SetVisibility(bOnOff);

	VisibilityChangedDelegate.Broadcast(bOnOff);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimingGraphTrack
////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FTimingGraphTrack)

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingGraphTrack::FTimingGraphTrack(TSharedPtr<STimingView> InTimingView)
	: FGraphTrack()
	, TimingView(InTimingView)
	//, SharedValueViewport()
{
	LoadDefaultSettings();
	
	// Add non editable options.
	EnabledOptions = EnabledOptions | EGraphOptions::ShowBaseline | EGraphOptions::ShowVerticalAxisGrid | EGraphOptions::ShowHeader;

	bNotifyTimersOnDestruction = InTimingView->GetName() == FInsightsManagerTabs::TimingProfilerTabId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingGraphTrack::~FTimingGraphTrack()
{
	if (OnTrackVisibilityChangedHandle.IsValid())
	{
		TSharedPtr<STimingView> TimingViewPtr = TimingView.Pin();
		if (TimingView.IsValid())
		{
			TimingViewPtr->OnTrackVisibilityChanged().Remove(OnTrackVisibilityChangedHandle);
			TimingViewPtr->OnTrackAdded().Remove(OnTrackAddedHandle);
			TimingViewPtr->OnTrackRemoved().Remove(OnTrackRemovedHandle);
		}
	}

	if (GameFrameSeriesVisibilityHandle.IsValid())
	{
		TSharedPtr<FTimingGraphSeries> GameFramesSeries = GetFrameSeries(ETraceFrameType::TraceFrameType_Game);
		if (GameFramesSeries.IsValid())
		{
			GameFramesSeries->VisibilityChangedDelegate.Remove(GameFrameSeriesVisibilityHandle);
		}
	}

	if (RenderingFrameSeriesVisibilityHandle.IsValid())
	{
		TSharedPtr<FTimingGraphSeries> RenderingFramesSeries = GetFrameSeries(ETraceFrameType::TraceFrameType_Game);
		if (RenderingFramesSeries.IsValid())
		{
			RenderingFramesSeries->VisibilityChangedDelegate.Remove(RenderingFrameSeriesVisibilityHandle);
		}
	}

	TSharedPtr<STimersView> TimersView;
	if (bNotifyTimersOnDestruction)
	{
		TSharedPtr<STimingProfilerWindow> ProfilerWindow = FTimingProfilerManager::Get()->GetProfilerWindow();
		if (ProfilerWindow.IsValid())
		{
			TimersView = ProfilerWindow->GetTimersView();
		}
	}

	if(TimersView)
	{
		for (const TSharedPtr<FGraphSeries>& Series : AllSeries)
		{
			const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
			if (TimingSeries.IsValid() &&
				(TimingSeries->Type == FTimingGraphSeries::ESeriesType::Timer || TimingSeries->Type == FTimingGraphSeries::ESeriesType::FrameStatsTimer))
			{
				FTimerNodePtr TimerNode = TimersView->GetTimerNode(TimingSeries->TimerId);
				if (TimerNode)
				{
					TimerNode->OnRemovedFromGraph();
				}
			}
		};
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::Update(const ITimingTrackUpdateContext& Context)
{
	FGraphTrack::Update(Context);

	if (!OnTrackVisibilityChangedHandle.IsValid())
	{
		TSharedPtr<STimingView> TimingViewPtr = TimingView.Pin();
		if (TimingViewPtr.IsValid())
		{
			auto OnTrackAddedRemovedLamda = [this](const TSharedPtr<const FBaseTimingTrack> Track)
			{
				if (Track->Is<FThreadTimingTrack>())
				{
					// If there are more series than the default frame series.
					if (this->AllSeries.Num() > ETraceFrameType::TraceFrameType_Count)
					{
						this->SetDirtyFlag();
					}
				}
			};

			OnTrackAddedHandle = TimingViewPtr->OnTrackAdded().AddLambda(OnTrackAddedRemovedLamda);
			OnTrackRemovedHandle = TimingViewPtr->OnTrackRemoved().AddLambda(OnTrackAddedRemovedLamda);

			OnTrackVisibilityChangedHandle = TimingViewPtr->OnTrackVisibilityChanged().AddLambda([this]()
				{
					this->SetDirtyFlag();
				});
		}
	}

	const bool bIsEntireGraphTrackDirty = IsDirty() || Context.GetViewport().IsHorizontalViewportDirty();
	bool bNeedsUpdate = bIsEntireGraphTrackDirty;

	if (!bNeedsUpdate)
	{
		for (TSharedPtr<FGraphSeries>& Series : AllSeries)
		{
			if (Series->IsVisible() && Series->IsDirty())
			{
				// At least one series is dirty.
				bNeedsUpdate = true;
				break;
			}
		}
	}

	if (bNeedsUpdate)
	{
		ClearDirtyFlag();

		NumAddedEvents = 0;

		const FTimingTrackViewport& Viewport = Context.GetViewport();

		for (TSharedPtr<FGraphSeries>& Series : AllSeries)
		{
			if (Series->IsVisible() && (bIsEntireGraphTrackDirty || Series->IsDirty()))
			{
				// Clear the flag before updating, because the update itself may further need to set the series as dirty.
				Series->ClearDirtyFlag();

				TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
				switch (TimingSeries->Type)
				{
				case FTimingGraphSeries::ESeriesType::Frame:
					UpdateFrameSeries(*TimingSeries, Viewport);
					break;

				case FTimingGraphSeries::ESeriesType::Timer:
					UpdateTimerSeries(*TimingSeries, Viewport);
					break;

				case FTimingGraphSeries::ESeriesType::FrameStatsTimer:
					UpdateFrameStatsTimerSeries(*TimingSeries, Viewport);
					break;

				case FTimingGraphSeries::ESeriesType::StatsCounter:
					UpdateStatsCounterSeries(*TimingSeries, Viewport);
					break;
				}
			}
		}

		UpdateStats();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Frame Series
////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::AddDefaultFrameSeries()
{
	const FInsightsSettings& Settings = FInsightsManager::Get()->GetSettings();
	TSharedPtr<STimingView> TimingViewPtr = TimingView.Pin();

	TSharedRef<FTimingGraphSeries> GameFramesSeries = MakeShared<FTimingGraphSeries>(FTimingGraphSeries::ESeriesType::Frame);
	GameFramesSeries->SetName(TEXT("Game Frames"));
	GameFramesSeries->SetDescription(TEXT("Duration of Game frames"));
	GameFramesSeries->SetColor(FLinearColor(0.3f, 0.3f, 1.0f, 1.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	GameFramesSeries->FrameType = TraceFrameType_Game;
	GameFramesSeries->SetBaselineY(SharedValueViewport.GetBaselineY());
	GameFramesSeries->SetScaleY(SharedValueViewport.GetScaleY());
	GameFramesSeries->EnableSharedViewport();
	if (TimingViewPtr.IsValid() && TimingViewPtr->GetName() == FInsightsManagerTabs::TimingProfilerTabId)
	{
		GameFramesSeries->SetVisibility(Settings.GetTimingViewMainGraphShowGameFrames());
		GameFrameSeriesVisibilityHandle = GameFramesSeries->VisibilityChangedDelegate.AddLambda([](bool bOnOff)
			{
				FInsightsSettings& Settings = FInsightsManager::Get()->GetSettings();
				Settings.SetAndSaveTimingViewMainGraphShowGameFrames(bOnOff);
			});
	}
	AllSeries.Add(GameFramesSeries);

	TSharedRef<FTimingGraphSeries> RenderingFramesSeries = MakeShared<FTimingGraphSeries>(FTimingGraphSeries::ESeriesType::Frame);
	RenderingFramesSeries->SetName(TEXT("Rendering Frames"));
	RenderingFramesSeries->SetDescription(TEXT("Duration of Rendering frames"));
	RenderingFramesSeries->SetColor(FLinearColor(1.0f, 0.3f, 0.3f, 1.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	RenderingFramesSeries->FrameType = TraceFrameType_Rendering;
	RenderingFramesSeries->SetBaselineY(SharedValueViewport.GetBaselineY());
	RenderingFramesSeries->SetScaleY(SharedValueViewport.GetScaleY());
	RenderingFramesSeries->EnableSharedViewport();
	if (TimingViewPtr.IsValid() && TimingViewPtr->GetName() == FInsightsManagerTabs::TimingProfilerTabId)
	{
		RenderingFramesSeries->SetVisibility(Settings.GetTimingViewMainGraphShowRenderingFrames());
		RenderingFrameSeriesVisibilityHandle = RenderingFramesSeries->VisibilityChangedDelegate.AddLambda([](bool bOnOff)
			{
				FInsightsSettings& Settings = FInsightsManager::Get()->GetSettings();
				Settings.SetAndSaveTimingViewMainGraphShowRenderingFrames(bOnOff);
			});
	}
	AllSeries.Add(RenderingFramesSeries);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FTimingGraphSeries> FTimingGraphTrack::GetFrameSeries(ETraceFrameType FrameType)
{
	TSharedPtr<FGraphSeries>* Ptr = AllSeries.FindByPredicate([FrameType](const TSharedPtr<FGraphSeries>& Series)
		{
			const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
			return TimingSeries->Type == FTimingGraphSeries::ESeriesType::Frame && TimingSeries->FrameType == FrameType;
		});
	return (Ptr != nullptr) ? StaticCastSharedPtr<FTimingGraphSeries>(*Ptr) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::UpdateFrameSeries(FTimingGraphSeries& Series, const FTimingTrackViewport& Viewport)
{
	FGraphTrackBuilder Builder(*this, Series, Viewport);
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
		const TraceServices::IFrameProvider& FramesProvider = ReadFrameProvider(*Session.Get());

		const TArray64<double>& FrameStartTimes = FramesProvider.GetFrameStartTimes(Series.FrameType);

		const int64 StartLowerBound = Algo::LowerBound(FrameStartTimes, Viewport.GetStartTime());
		const uint64 StartIndex = (StartLowerBound > 1) ? StartLowerBound - 2 : 0;

		const int64 EndLowerBound = Algo::LowerBound(FrameStartTimes, Viewport.GetEndTime());
		const uint64 EndIndex = EndLowerBound + 1;

		FramesProvider.EnumerateFrames(Series.FrameType, StartIndex, EndIndex, [&Builder](const TraceServices::FFrame& Frame)
		{
			//TODO: add a "frame converter" (i.e. to fps, miliseconds or seconds)
			const double Duration = Frame.EndTime - Frame.StartTime;
			Builder.AddEvent(Frame.StartTime, Duration, Duration);
		});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer Series
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FTimingGraphSeries> FTimingGraphTrack::GetTimerSeries(uint32 TimerId)
{
	TSharedPtr<FGraphSeries>* Ptr = AllSeries.FindByPredicate([TimerId](const TSharedPtr<FGraphSeries>& Series)
	{
		const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
		return TimingSeries->Type == FTimingGraphSeries::ESeriesType::Timer && TimingSeries->TimerId == TimerId;
	});
	return (Ptr != nullptr) ? StaticCastSharedPtr<FTimingGraphSeries>(*Ptr) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FTimingGraphSeries> FTimingGraphTrack::AddTimerSeries(uint32 TimerId, FLinearColor Color)
{
	TSharedRef<FTimingGraphSeries> Series = MakeShared<FTimingGraphSeries>(FTimingGraphSeries::ESeriesType::Timer);

	Series->SetName(TEXT("<Timer>"));
	Series->SetDescription(TEXT("Timer series"));

	const FLinearColor BorderColor(Color.R + 0.4f, Color.G + 0.4f, Color.B + 0.4f, 1.0f);
	Series->SetColor(Color, BorderColor);

	Series->TimerId = TimerId;
	//Series->CpuOrGpu = ;
	//Series->TimelineIndex = ;

	// Use shared viewport.
	Series->SetBaselineY(SharedValueViewport.GetBaselineY());
	Series->SetScaleY(SharedValueViewport.GetScaleY());
	Series->EnableSharedViewport();

	Series->CachedSessionDuration = 0.0;

	AllSeries.Add(Series);
	return Series;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::RemoveTimerSeries(uint32 TimerId)
{
	AllSeries.RemoveAll([TimerId](const TSharedPtr<FGraphSeries>& Series)
	{
		const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
		return TimingSeries->Type == FTimingGraphSeries::ESeriesType::Timer && TimingSeries->TimerId == TimerId;
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::UpdateTimerSeries(FTimingGraphSeries& Series, const FTimingTrackViewport& Viewport)
{
	FGraphTrackBuilder Builder(*this, Series, Viewport);
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		TSet<uint32> Timelines;
		GetVisibleTimelineIndexes(Timelines);
		const double SessionDuration = Session->GetDurationSeconds();
		if (Series.CachedSessionDuration != SessionDuration || Series.CachedTimelinesNum != Timelines.Num())
		{
			Series.CachedSessionDuration = SessionDuration;

			const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

			const TraceServices::ITimingProfilerTimerReader* TimerReader;
			TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

			const uint32 TimelineCount = TimingProfilerProvider.GetTimelineCount();
			uint32 NumTimelinesContainingEvent = 0;

			Series.CachedTimelinesNum = Timelines.Num();
			Series.CachedEvents.Empty();

			for (uint32 TimelineIndex : Timelines)
			{
				TimingProfilerProvider.ReadTimeline(TimelineIndex,
					[SessionDuration, &Series, TimerReader, &Viewport, &NumTimelinesContainingEvent](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						TArray<TArray<FTimingGraphSeries::FSimpleTimingEvent>> Events;
						TraceServices::ITimeline<TraceServices::FTimingProfilerEvent>::EnumerateAsyncParams Params;
						Params.IntervalStart = 0;
						Params.IntervalEnd = SessionDuration;
						Params.Resolution = 0.0;
						Params.SetupCallback = [&Events](uint32 NumTasks)
						{
							Events.AddDefaulted(NumTasks);
						};
						Params.Callback = [&Events, TimerReader, &Series](double StartTime, double EndTime, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event, uint32 TaskIndex)
						{
							const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
							if (ensure(Timer != nullptr))
							{
								if (Timer->Id == Series.TimerId)
								{
									const double Duration = EndTime - StartTime;
									Events[TaskIndex].Add({ StartTime, Duration });
								}
							}
							return TraceServices::EEventEnumerate::Continue;
						};

						Timeline.EnumerateEventsDownSampledAsync(Params);

						int32 NumOfCachedEvents = Series.CachedEvents.Num();
						for (auto& Array : Events)
						{
							for (auto& Event : Array)
							{
								Series.CachedEvents.Add(Event);
							}

							Array.Empty();
						}

						if (NumOfCachedEvents != Series.CachedEvents.Num())
						{
							++NumTimelinesContainingEvent;
						}
					});
			}

			//If events come from multiple timelines, we have to sort the whole thing.
			//If they come from a single timeline, they are already sorted.
			if (NumTimelinesContainingEvent > 1)
			{
				Series.CachedEvents.Sort(&FTimingGraphSeries::CompareEventsByStartTime);
			}
		}

		int32 StartIndex = Algo::UpperBoundBy(Series.CachedEvents, Viewport.GetStartTime(), &FTimingGraphSeries::FSimpleTimingEvent::StartTime);
		if (StartIndex > 0)
		{
			StartIndex--;
		}
		int32 EndIndex = Algo::UpperBoundBy(Series.CachedEvents, Viewport.GetEndTime(), &FTimingGraphSeries::FSimpleTimingEvent::StartTime);
		if (EndIndex < Series.CachedEvents.Num())
		{
			EndIndex++;
		}
		for (int32 Index = StartIndex; Index < EndIndex; ++Index)
		{
			const FTimingGraphSeries::FSimpleTimingEvent& Event = Series.CachedEvents[Index];
			Builder.AddEvent(Event.StartTime, Event.Duration, Event.Duration);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Frams Stats Timer Series
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FTimingGraphSeries> FTimingGraphTrack::GetFrameStatsTimerSeries(uint32 TimerId, ETraceFrameType FrameType)
{
	TSharedPtr<FGraphSeries>* Ptr = AllSeries.FindByPredicate([TimerId, FrameType](const TSharedPtr<FGraphSeries>& Series)
		{
			const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
			return TimingSeries->Type == FTimingGraphSeries::ESeriesType::FrameStatsTimer && TimingSeries->TimerId == TimerId && TimingSeries->FrameType == FrameType;
		});
	return (Ptr != nullptr) ? StaticCastSharedPtr<FTimingGraphSeries>(*Ptr) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FTimingGraphSeries> FTimingGraphTrack::AddFrameStatsTimerSeries(uint32 TimerId, ETraceFrameType FrameType, FLinearColor Color)
{
	TSharedRef<FTimingGraphSeries> Series = MakeShared<FTimingGraphSeries>(FTimingGraphSeries::ESeriesType::FrameStatsTimer);

	Series->SetName(TEXT("<Frame Stats Timer>"));
	Series->SetDescription(TEXT("Frame Stats Timer series"));

	const FLinearColor BorderColor(Color.R + 0.4f, Color.G + 0.4f, Color.B + 0.4f, 1.0f);
	Series->SetColor(Color, BorderColor);

	Series->TimerId = TimerId;
	Series->FrameType = FrameType;

	// Use shared viewport.
	Series->SetBaselineY(SharedValueViewport.GetBaselineY());
	Series->SetScaleY(SharedValueViewport.GetScaleY());
	Series->EnableSharedViewport();

	Series->CachedSessionDuration = 0.0;

	AllSeries.Add(Series);
	return Series;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::RemoveFrameStatsTimerSeries(uint32 TimerId, ETraceFrameType FrameType)
{
	AllSeries.RemoveAll([TimerId, FrameType](const TSharedPtr<FGraphSeries>& Series)
		{
			const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
			return TimingSeries->Type == FTimingGraphSeries::ESeriesType::FrameStatsTimer && TimingSeries->TimerId == TimerId && TimingSeries->FrameType == FrameType;
		});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::UpdateFrameStatsTimerSeries(FTimingGraphSeries& Series, const FTimingTrackViewport& Viewport)
{
	FGraphTrackBuilder Builder(*this, Series, Viewport);
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		TSet<uint32> VisibleTimelines;
		GetVisibleTimelineIndexes(VisibleTimelines);
		const double SessionDuration = Session->GetDurationSeconds();
		if (Series.CachedSessionDuration != SessionDuration || Series.CachedTimelinesNum != VisibleTimelines.Num())
		{
			Series.CachedSessionDuration = SessionDuration;
			Series.CachedTimelinesNum = VisibleTimelines.Num();

			const TraceServices::IFrameProvider& FramesProvider = ReadFrameProvider(*Session.Get());

			Series.FrameStatsCachedEvents.Empty();
			uint64 FrameCount = FramesProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);
			if (FrameCount == 0)
			{
				return;
			}

			FramesProvider.EnumerateFrames(Series.FrameType, 0ull, FrameCount, [&Series](const TraceServices::FFrame& Frame)
				{
					Insights::FFrameStatsCachedEvent Event;
					Event.FrameStartTime = Frame.StartTime;
					Event.FrameEndTime = Frame.EndTime;
					Event.Duration.store(0.0f);
					Series.FrameStatsCachedEvents.Add(Event);
				});

			Insights::FFrameStatsHelper::ComputeFrameStatsForTimer(Series.FrameStatsCachedEvents, Series.TimerId, VisibleTimelines);
		}

		int32 StartIndex = Algo::UpperBoundBy(Series.FrameStatsCachedEvents, Viewport.GetStartTime(), &Insights::FFrameStatsCachedEvent::FrameStartTime);
		if (StartIndex > 0)
		{
			StartIndex--;
		}
		int32 EndIndex = Algo::UpperBoundBy(Series.FrameStatsCachedEvents, Viewport.GetEndTime(), &Insights::FFrameStatsCachedEvent::FrameStartTime);
		if (EndIndex < Series.FrameStatsCachedEvents.Num())
		{
			EndIndex++;
		}
		for (int32 Index = StartIndex; Index < EndIndex; ++Index)
		{
			const Insights::FFrameStatsCachedEvent& Entry = Series.FrameStatsCachedEvents[Index];
			Builder.AddEvent(Entry.FrameStartTime, Entry.Duration.load(), Entry.Duration.load());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Stats Counter Series
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FTimingGraphSeries> FTimingGraphTrack::GetStatsCounterSeries(uint32 CounterId)
{
	TSharedPtr<FGraphSeries>* Ptr = AllSeries.FindByPredicate([CounterId](const TSharedPtr<FGraphSeries>& Series)
	{
		const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
		return TimingSeries->Type == FTimingGraphSeries::ESeriesType::StatsCounter && TimingSeries->CounterId == CounterId;
	});
	return (Ptr != nullptr) ? StaticCastSharedPtr<FTimingGraphSeries>(*Ptr) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FTimingGraphSeries> FTimingGraphTrack::AddStatsCounterSeries(uint32 CounterId, FLinearColor Color)
{
	TSharedRef<FTimingGraphSeries> Series = MakeShared<FTimingGraphSeries>(FTimingGraphSeries::ESeriesType::StatsCounter);

	const TCHAR* CounterName = nullptr;
	bool bIsTime = false;
	bool bIsMemory = false;
	bool bIsFloatingPoint = false;

	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
		const TraceServices::ICounterProvider& CountersProvider = TraceServices::ReadCounterProvider(*Session.Get());
		if (CounterId < CountersProvider.GetCounterCount())
		{
			CountersProvider.ReadCounter(CounterId, [&](const TraceServices::ICounter& Counter)
			{
				CounterName = Counter.GetName();
				//bIsTime = (Counter.GetDisplayHint() == TraceServices::CounterDisplayHint_Time);
				bIsMemory = (Counter.GetDisplayHint() == TraceServices::CounterDisplayHint_Memory);
				bIsFloatingPoint = Counter.IsFloatingPoint();
			});
		}
	}

	Series->SetName(CounterName != nullptr ? CounterName : TEXT("<StatsCounter>"));
	Series->SetDescription(TEXT("Stats counter series"));

	FLinearColor BorderColor(Color.R + 0.4f, Color.G + 0.4f, Color.B + 0.4f, 1.0f);
	Series->SetColor(Color, BorderColor);

	Series->CounterId = CounterId;

	Series->bIsTime = bIsTime;
	Series->bIsMemory = bIsMemory;
	Series->bIsFloatingPoint = bIsFloatingPoint;

	Series->SetBaselineY(GetHeight() - 1.0f);
	Series->SetScaleY(1.0);
	Series->EnableAutoZoom();

	AllSeries.Add(Series);
	return Series;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::RemoveStatsCounterSeries(uint32 CounterId)
{
	AllSeries.RemoveAll([CounterId](const TSharedPtr<FGraphSeries>& Series)
	{
		const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
		return TimingSeries->Type == FTimingGraphSeries::ESeriesType::StatsCounter && TimingSeries->CounterId == CounterId;
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::UpdateStatsCounterSeries(FTimingGraphSeries& Series, const FTimingTrackViewport& Viewport)
{
	FGraphTrackBuilder Builder(*this, Series, Viewport);

	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
		const TraceServices::ICounterProvider& CounterProvider = TraceServices::ReadCounterProvider(*Session.Get());
		CounterProvider.ReadCounter(Series.CounterId, [this, &Viewport, &Builder, &Series](const TraceServices::ICounter& Counter)
		{
			const float TopY = 4.0f;
			const float BottomY = GetHeight() - 4.0f;

			if (Series.IsAutoZoomEnabled() && TopY < BottomY)
			{
				double MinValue =  std::numeric_limits<double>::infinity();
				double MaxValue = -std::numeric_limits<double>::infinity();

				if (Counter.IsFloatingPoint())
				{
					Counter.EnumerateFloatValues(Viewport.GetStartTime(), Viewport.GetEndTime(), true, [&Builder, &MinValue, &MaxValue](double Time, double Value)
					{
						if (Value < MinValue)
						{
							MinValue = Value;
						}
						if (Value > MaxValue)
						{
							MaxValue = Value;
						}
					});
				}
				else
				{
					Counter.EnumerateValues(Viewport.GetStartTime(), Viewport.GetEndTime(), true, [&Builder, &MinValue, &MaxValue](double Time, int64 IntValue)
					{
						const double Value = static_cast<double>(IntValue);
						if (Value < MinValue)
						{
							MinValue = Value;
						}
						if (Value > MaxValue)
						{
							MaxValue = Value;
						}
					});
				}

				Series.UpdateAutoZoom(TopY, BottomY, MinValue, MaxValue);
			}

			if (Counter.IsFloatingPoint())
			{
				Counter.EnumerateFloatValues(Viewport.GetStartTime(), Viewport.GetEndTime(), true, [&Builder](double Time, double Value)
				{
					//TODO: add a "value unit converter"
					Builder.AddEvent(Time, 0.0, Value);
				});
			}
			else
			{
				Counter.EnumerateValues(Viewport.GetStartTime(), Viewport.GetEndTime(), true, [&Builder](double Time, int64 IntValue)
				{
					//TODO: add a "value unit converter"
					Builder.AddEvent(Time, 0.0, static_cast<double>(IntValue));
				});
			}
		});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::GetVisibleTimelineIndexes(TSet<uint32>& TimelineIndexes)
{
	TSharedPtr<STimingView> TimingViewPtr = TimingView.Pin();
	if (!TimingViewPtr.IsValid())
	{
		return;
	}

	TSharedPtr<FThreadTimingSharedState> ThreadSharedState = TimingViewPtr->GetThreadTimingSharedState();
	ThreadSharedState->GetVisibleTimelineIndexes(TimelineIndexes);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::ContextMenu_ToggleOption_Execute(EGraphOptions Option)
{
	FGraphTrack::ContextMenu_ToggleOption_Execute(Option);

	TSharedPtr<STimingView> TimingViewPtr = TimingView.Pin();
	if (!TimingViewPtr.IsValid())
	{
		return;
	}
	if (TimingViewPtr->GetName() != FInsightsManagerTabs::TimingProfilerTabId)
	{
		return;
	}

	FInsightsSettings& Settings = FInsightsManager::Get()->GetSettings();
	if (EnumHasAnyFlags(Option, EGraphOptions::ShowPoints))
	{
		Settings.SetAndSaveTimingViewMainGraphShowPoints(EnumHasAnyFlags(EnabledOptions, EGraphOptions::ShowPoints));
	}
	if (EnumHasAnyFlags(Option, EGraphOptions::ShowPointsWithBorder))
	{
		Settings.SetAndSaveTimingViewMainGraphShowPointsWithBorder(EnumHasAnyFlags(EnabledOptions, EGraphOptions::ShowPointsWithBorder));
	}
	if (EnumHasAnyFlags(Option, EGraphOptions::ShowLines))
	{
		Settings.SetAndSaveTimingViewMainGraphShowConnectedLines(EnumHasAnyFlags(EnabledOptions, EGraphOptions::ShowLines));
	}
	if (EnumHasAnyFlags(Option, EGraphOptions::ShowPolygon))
	{
		Settings.SetAndTimingViewMainGraphShowPolygons(EnumHasAnyFlags(EnabledOptions, EGraphOptions::ShowPolygon));
	}
	if (EnumHasAnyFlags(Option, EGraphOptions::UseEventDuration))
	{
		Settings.SetAndSaveTimingViewMainGraphShowEventDuration(EnumHasAnyFlags(EnabledOptions, EGraphOptions::UseEventDuration));
	}
	if (EnumHasAnyFlags(Option, EGraphOptions::ShowBars))
	{
		Settings.SetAndSaveTimingViewMainGraphShowBars(EnumHasAnyFlags(EnabledOptions, EGraphOptions::ShowBars));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::LoadDefaultSettings()
{
	TSharedPtr<STimingView> TimingViewPtr = TimingView.Pin();
	if (TimingViewPtr.IsValid() && TimingViewPtr->GetName() == FInsightsManagerTabs::TimingProfilerTabId)
	{
		const FInsightsSettings& Settings = FInsightsManager::Get()->GetSettings();
		if (Settings.GetTimingViewMainGraphShowPoints())
		{
			EnabledOptions |= EGraphOptions::ShowPoints;
		}
		if (Settings.GetTimingViewMainGraphShowPointsWithBorder())
		{
			EnabledOptions |= EGraphOptions::ShowPointsWithBorder;
		}
		if (Settings.GetTimingViewMainGraphShowConnectedLines())
		{
			EnabledOptions |= EGraphOptions::ShowLines;
		}
		if (Settings.GetTimingViewMainGraphShowPolygons())
		{
			EnabledOptions |= EGraphOptions::ShowPolygon;
		}
		if (Settings.GetTimingViewMainGraphShowEventDuration())
		{
			EnabledOptions |= EGraphOptions::UseEventDuration;
		}
		if (Settings.GetTimingViewMainGraphShowBars())
		{
			EnabledOptions |= EGraphOptions::ShowBars;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingGraphTrack::DrawVerticalAxisGrid(const ITimingTrackDrawContext& Context) const
{
	TSharedPtr<FTimingGraphSeries> FirstTimeUnitSeries;
	for (const TSharedPtr<FGraphSeries>& Series : AllSeries)
	{
		if (Series->IsVisible())
		{
			TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
			if (TimingSeries->bIsTime)
			{
				FirstTimeUnitSeries = TimingSeries;
				break;
			}
		}
	}
	if (!FirstTimeUnitSeries)
	{
		return;
	}

	FAxisViewportDouble ViewportY;
	ViewportY.SetSize(GetHeight());
	ViewportY.SetScaleLimits(std::numeric_limits<double>::min(), std::numeric_limits<double>::max());
	ViewportY.SetScale(SharedValueViewport.GetScaleY());
	ViewportY.ScrollAtPos(static_cast<float>(SharedValueViewport.GetBaselineY()) - GetHeight());

	const float ViewWidth = Context.GetViewport().GetWidth();
	const float RoundedViewHeight = FMath::RoundToFloat(GetHeight());

	const float X0 = ViewWidth - 12.0f; // let some space for the vertical scrollbar
	const float Y0 = GetPosY();

	constexpr float MinDY = 32.0f; // min vertical distance between horizontal grid lines
	constexpr float TextH = 14.0f; // label height

	FDrawContext& DrawContext = Context.GetDrawContext();
	const FSlateBrush* Brush = Context.GetHelper().GetWhiteBrush();
	//const FSlateFontInfo& Font = Context.GetHelper().GetEventFont();
	const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float FontScale = DrawContext.Geometry.Scale;

	const double TopValue = ViewportY.GetValueAtOffset(RoundedViewHeight);
	const double GridValue = ViewportY.GetValueAtOffset(MinDY);
	const double BottomValue = ViewportY.GetValueAtOffset(0.0f);
	const double Delta = GridValue - BottomValue;

	if (Delta > 0.0)
	{
		const double Thresholds[] =
		{
			1.0e-9,	// 1ns
			1.0e-8,	// 10ns
			1.0e-7,	// 100ns
			1.0e-6,	// 1us
			1.0e-5,	// 10us
			0.0001,	// 100us
			0.001,	// 1ms
			0.01,	// 10ms
			0.1,	// 100ms
			1.0,	// 1s
			10.0,	// 10s
			60.0,	// 1m
			600.0,	// 10m
			3600.0,	// 1h
			36000.0,// 10h
			86400.0	// 1d
		};
		constexpr int32 NumThresholds = sizeof(Thresholds) / sizeof(double);
		int32 Index = static_cast<int32>(Algo::LowerBound(Thresholds, Delta));
		if (Index > 0)
		{
			Index--;
		}
		double TickUnit = Thresholds[Index];
		int64 DeltaTicks = static_cast<int64>(FMath::CeilToDouble(Delta / TickUnit));
		if (Index < NumThresholds - 1)
		{
			const double NextTickUnit = Thresholds[Index + 1];
			if (NextTickUnit <= static_cast<double>(DeltaTicks + 1) * TickUnit)
			{
				TickUnit = NextTickUnit;
				DeltaTicks = 1;
			}
			else if (DeltaTicks != 1 && DeltaTicks != 5 && DeltaTicks % 2 == 1) // prefer even grid values
			{
				DeltaTicks++;
			}
		}
		const double Grid = static_cast<double>(DeltaTicks) * TickUnit;
		const double StartValue = FMath::GridSnap(BottomValue, Grid);

		const FLinearColor GridColor(0.0f, 0.0f, 0.0f, 0.1f);
		const FLinearColor TextBgColor(0.05f, 0.05f, 0.05f, 1.0f);
		const FLinearColor TextColor = FirstTimeUnitSeries->GetColor().CopyWithNewOpacity(1.0f);

		for (double Value = StartValue; Value < TopValue; Value += Grid)
		{
			const float Y = Y0 + RoundedViewHeight - FMath::RoundToFloat(ViewportY.GetOffsetForValue(Value));

			const FString LabelText = TimeUtils::FormatTimeAuto(Value);

			// Draw horizontal grid line.
			DrawContext.DrawBox(0, Y, ViewWidth, 1, Brush, GridColor);

			const FVector2D LabelTextSize = FontMeasureService->Measure(LabelText, Font, FontScale) / FontScale;
			const float LabelX = X0 - static_cast<float>(LabelTextSize.X) - 4.0f;
			const float LabelY = FMath::Min(Y0 + GetHeight() - TextH, FMath::Max(Y0, Y - TextH / 2));

			// Draw background for value text.
			DrawContext.DrawBox(LabelX, LabelY, static_cast<float>(LabelTextSize.X) + 4.0f, TextH, Brush, TextBgColor);

			// Draw value text.
			DrawContext.DrawText(LabelX + 2.0f, LabelY + 1.0f, LabelText, Font, TextColor);
		}

		DrawContext.LayerId++;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FTimingGraphTrack::GetNumSeriesForTimer(uint32 TimerId)
{
	uint32 NumSeries = 0;

	for(const TSharedPtr<FGraphSeries>& Series : AllSeries)
	{
		const TSharedPtr<FTimingGraphSeries> TimingSeries = StaticCastSharedPtr<FTimingGraphSeries>(Series);
		if (TimingSeries.IsValid() && 
			(TimingSeries->Type == FTimingGraphSeries::ESeriesType::Timer || TimingSeries->Type == FTimingGraphSeries::ESeriesType::FrameStatsTimer) && 
			TimingSeries->TimerId == TimerId)
		{
			++NumSeries;
		}
	};

	return NumSeries;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
