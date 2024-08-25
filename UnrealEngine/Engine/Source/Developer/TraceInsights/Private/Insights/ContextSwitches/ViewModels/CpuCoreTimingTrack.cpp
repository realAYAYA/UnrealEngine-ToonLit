// Copyright Epic Games, Inc. All Rights Reserved.

#include "CpuCoreTimingTrack.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "TraceServices/Model/ContextSwitches.h"
#include "TraceServices/Model/Threads.h"

// Insights
#include "Insights/Common/TimeUtils.h"
#include "Insights/ContextSwitches/ViewModels/ContextSwitchesSharedState.h"
#include "Insights/ContextSwitches/ViewModels/ContextSwitchTimingEvent.h"
#include "Insights/InsightsManager.h"
#include "Insights/ViewModels/FilterConfigurator.h"
#include "Insights/ViewModels/Filters.h"
#include "Insights/ViewModels/TimingTrackViewport.h"
#include "Insights/ViewModels/TimingViewDrawHelper.h"
#include "Insights/ViewModels/TooltipDrawState.h"
#include "Insights/Widgets/STimingView.h"

#define LOCTEXT_NAMESPACE "FCpuCoreTimingTrack"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FCpuCoreTimingTrack)

////////////////////////////////////////////////////////////////////////////////////////////////////

FCpuCoreTimingTrack::FCpuCoreTimingTrack(FContextSwitchesSharedState& InSharedState, const FString& InName, uint32 InCoreNumber)
	: FTimingEventsTrack(InName)
	, SharedState(InSharedState)
	, CoreNumber(InCoreNumber)
	, NonTargetProcessEventsDrawState(MakeShared<FTimingEventsTrackDrawState>())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuCoreTimingTrack::Reset()
{
	NonTargetProcessEventsMaxDepth = -1;
	NonTargetProcessEventsDrawState->Reset();

	FTimingEventsTrack::Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuCoreTimingTrack::BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (!Session.IsValid())
	{
		return;
	}

	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

	const TraceServices::IContextSwitchesProvider* ContextSwitchesProvider = TraceServices::ReadContextSwitchesProvider(*Session.Get());
	if (ContextSwitchesProvider == nullptr)
	{
		return;
	}

	const FTimingTrackViewport& Viewport = Context.GetViewport();
	FTimingEventsTrackDrawStateBuilder NonTargetProcessEventsBuilder(*NonTargetProcessEventsDrawState, Viewport, Context.GetGeometry().Scale);
	bool bShowNonTargetProcessEvents = SharedState.AreNonTargetProcessEventsVisible();
	ContextSwitchesProvider->EnumerateCpuCoreEvents(CoreNumber, Viewport.GetStartTime(), Viewport.GetEndTime(), 
		[&Builder, &NonTargetProcessEventsBuilder, ContextSwitchesProvider, bShowNonTargetProcessEvents, this](const TraceServices::FCpuCoreEvent& CpuCoreEvent)
		{
			uint32 ThreadId = 0;
			if (ContextSwitchesProvider->GetThreadId(CpuCoreEvent.SystemThreadId, ThreadId))
			{
				this->AddCoreTimingEvent(Builder, CpuCoreEvent);
			}
			else if(bShowNonTargetProcessEvents)
			{
				this->AddCoreTimingEvent(NonTargetProcessEventsBuilder, CpuCoreEvent);
			}

			return TraceServices::EContextSwitchEnumerationResult::Continue;
		});

	NonTargetProcessEventsMaxDepth = NonTargetProcessEventsBuilder.GetMaxDepth();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuCoreTimingTrack::BuildFilteredDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context)
{
	const TSharedPtr<ITimingEventFilter> EventFilterPtr = Context.GetEventFilter();
	if (EventFilterPtr.IsValid() && EventFilterPtr->FilterTrack(*this))
	{
		bool bFilterOnlyByEventType = false; // this is the most often use case, so the below code tries to optimize it
		uint64 FilterEventType = 0;
		if (EventFilterPtr->Is<FTimingEventFilterByEventType>())
		{
			bFilterOnlyByEventType = true;
			const FTimingEventFilterByEventType& EventFilter = EventFilterPtr->As<FTimingEventFilterByEventType>();
			FilterEventType = EventFilter.GetEventType();
		}

		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadContextSwitchesProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

			const TraceServices::IContextSwitchesProvider* ContextSwitchesProvider = TraceServices::ReadContextSwitchesProvider(*Session.Get());

			const FTimingTrackViewport& Viewport = Context.GetViewport();

			if (bFilterOnlyByEventType)
			{
				bool bShowNonTargetProcessEvents = SharedState.AreNonTargetProcessEventsVisible();
				ContextSwitchesProvider->EnumerateCpuCoreEvents(CoreNumber, Viewport.GetStartTime(), Viewport.GetEndTime(), 
					[&Builder, ContextSwitchesProvider, FilterEventType, this, bShowNonTargetProcessEvents](const TraceServices::FCpuCoreEvent& CpuCoreEvent)
					{
						if (CpuCoreEvent.SystemThreadId != FilterEventType)
						{
							return TraceServices::EContextSwitchEnumerationResult::Continue;
						}

						uint32 ThreadId;
						if (!bShowNonTargetProcessEvents && !ContextSwitchesProvider->GetThreadId(CpuCoreEvent.SystemThreadId, ThreadId))
						{
							return TraceServices::EContextSwitchEnumerationResult::Continue;
						}

						this->AddCoreTimingEvent(Builder, CpuCoreEvent);

						return TraceServices::EContextSwitchEnumerationResult::Continue;
					});
			}
			else // generic filter
			{
				//TODO: if (EventFilterPtr->FilterEvent(TimingEvent))
			}
		}
	}

	if (HasCustomFilter())
	{
		TSharedPtr<STimingView> TimingView = SharedState.GetTimingView();
		if (!TimingView)
		{
			return;
		}

		TSharedPtr<FFilterConfigurator> FilterConfigurator = TimingView->GetFilterConfigurator();
		if (!FilterConfigurator.IsValid())
		{
			return;
		}

		FFilterContext FilterContext;
		FilterContext.SetReturnValueForUnsetFilters(false);

		FilterContext.AddFilterData<double>(static_cast<int32>(EFilterField::StartTime), 0.0f);
		FilterContext.AddFilterData<double>(static_cast<int32>(EFilterField::EndTime), 0.0f);
		FilterContext.AddFilterData<double>(static_cast<int32>(EFilterField::Duration), 0.0f);
		FilterContext.AddFilterData<FString>(static_cast<int32>(EFilterField::TrackName), this->GetName());
		FilterContext.AddFilterData<int64>(static_cast<int32>(EFilterField::CoreEventName), 0);

		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
		const TraceServices::IContextSwitchesProvider* ContextSwitchesProvider = TraceServices::ReadContextSwitchesProvider(*Session.Get());
		const FTimingTrackViewport& Viewport = Context.GetViewport();

		bool bShowNonTargetProcessEvents = SharedState.AreNonTargetProcessEventsVisible();

		ContextSwitchesProvider->EnumerateCpuCoreEvents(CoreNumber, Viewport.GetStartTime(), Viewport.GetEndTime(), 
			[&Builder, ContextSwitchesProvider, &FilterContext, &FilterConfigurator, bShowNonTargetProcessEvents, this](const TraceServices::FCpuCoreEvent& CpuCoreEvent)
			{
				FilterContext.SetFilterData<double>(static_cast<int32>(EFilterField::StartTime), CpuCoreEvent.Start);
				FilterContext.SetFilterData<double>(static_cast<int32>(EFilterField::EndTime), CpuCoreEvent.End);
				FilterContext.SetFilterData<double>(static_cast<int32>(EFilterField::Duration), CpuCoreEvent.End - CpuCoreEvent.Start);
				FilterContext.SetFilterData<int64>(static_cast<int32>(EFilterField::CoreEventName), CpuCoreEvent.SystemThreadId);

				if (!FilterConfigurator->ApplyFilters(FilterContext))
				{
					return TraceServices::EContextSwitchEnumerationResult::Continue;
				}

				uint32 ThreadId;
				if (!bShowNonTargetProcessEvents && !ContextSwitchesProvider->GetThreadId(CpuCoreEvent.SystemThreadId, ThreadId))
				{
					return TraceServices::EContextSwitchEnumerationResult::Continue;
				}

				this->AddCoreTimingEvent(Builder, CpuCoreEvent);

				return TraceServices::EContextSwitchEnumerationResult::Continue;
			});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuCoreTimingTrack::AddCoreTimingEvent(ITimingEventsTrackDrawStateBuilder& Builder, const TraceServices::FCpuCoreEvent& CpuCoreEvent)
{
	constexpr uint32 EventDepth = 0;
	const uint32 EventColor = FTimingEvent::ComputeEventColor(CpuCoreEvent.SystemThreadId);
	const uint32 SystemThreadId = CpuCoreEvent.SystemThreadId;

	Builder.AddEvent(CpuCoreEvent.Start, CpuCoreEvent.End, EventDepth, EventColor,
		[SystemThreadId, &CpuCoreEvent, this](float Width) -> const FString
		{
			FString EventName = GetThreadName(SystemThreadId);

			const float MinWidth = static_cast<float>(EventName.Len()) * 4.0f + 32.0f;
			if (Width > MinWidth)
			{
				const double Duration = CpuCoreEvent.End - CpuCoreEvent.Start;
				FTimingEventsTrackDrawStateBuilder::AppendDurationToEventName(EventName, Duration);
			}

			return EventName;
		});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuCoreTimingTrack::Draw(const ITimingTrackDrawContext& Context) const
{
	const FTimingViewDrawHelper& Helper = *static_cast<const FTimingViewDrawHelper*>(&Context.GetHelper());

	Helper.DrawFadedEvents(*NonTargetProcessEventsDrawState, *this, 1.0f, 0.1f);

	FTimingEventsTrack::Draw(Context);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuCoreTimingTrack::PostDraw(const ITimingTrackDrawContext& Context) const
{
	if (ChildTrack.IsValid())
	{
		ChildTrack->PostDraw(Context);
	}

	const TSharedPtr<const ITimingEvent> SelectedEventPtr = Context.GetSelectedEvent();
	if (SelectedEventPtr.IsValid() &&
		SelectedEventPtr->CheckTrack(this) &&
		SelectedEventPtr->Is<FTimingEvent>())
	{
		const FTimingEvent& SelectedEvent = SelectedEventPtr->As<FTimingEvent>();
		const ITimingViewDrawHelper& Helper = Context.GetHelper();

		FString Str = FString::Printf(TEXT("%s (Duration.: %s)"),
			*GetThreadName(static_cast<uint32>(SelectedEvent.GetType())),
			*TimeUtils::FormatTimeAuto(SelectedEvent.GetDuration()));

		DrawSelectedEventInfo(Str, Context.GetViewport(), Context.GetDrawContext(), Helper.GetWhiteBrush(), Helper.GetEventFont());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TSharedPtr<const ITimingEvent> FCpuCoreTimingTrack::GetEvent(double InTime, double SecondsPerPixel, int32 Depth) const
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (!Session.IsValid())
	{
		return nullptr;
	}

	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

	const TraceServices::IContextSwitchesProvider* ContextSwitchesProvider = TraceServices::ReadContextSwitchesProvider(*Session.Get());
	if (ContextSwitchesProvider == nullptr)
	{
		return nullptr;
	}

	TraceServices::FCpuCoreEvent BestMatchEvent;
	double Delta = 2 * SecondsPerPixel;
	bool bShowNonTargetProcessEvents = SharedState.AreNonTargetProcessEventsVisible();

	auto ShouldShowEvent = [bShowNonTargetProcessEvents, ContextSwitchesProvider](const TraceServices::FCpuCoreEvent& CpuCoreEvent)
	{
		uint32 ThreadId;
		if (!bShowNonTargetProcessEvents)
		{
			return ContextSwitchesProvider->GetThreadId(CpuCoreEvent.SystemThreadId, ThreadId);
		}

		return true;
	};

	ContextSwitchesProvider->EnumerateCpuCoreEvents(CoreNumber, InTime - Delta, InTime + Delta,
		[InTime, &BestMatchEvent, &Delta, ShouldShowEvent](const TraceServices::FCpuCoreEvent& CpuCoreEvent)
		{
			if (CpuCoreEvent.Start <= InTime && CpuCoreEvent.End >= InTime && ShouldShowEvent(CpuCoreEvent))
			{
				BestMatchEvent = CpuCoreEvent;
				Delta = 0.0f;
				return TraceServices::EContextSwitchEnumerationResult::Stop;
			}

			double DeltaLeft = InTime - CpuCoreEvent.End;
			if (DeltaLeft >= 0 && DeltaLeft < Delta && ShouldShowEvent(CpuCoreEvent))
			{
				Delta = DeltaLeft;
				BestMatchEvent = CpuCoreEvent;
			}

			double DeltaRight = CpuCoreEvent.Start - InTime;
			if (DeltaRight >= 0 && DeltaRight < Delta && ShouldShowEvent(CpuCoreEvent))
			{
				Delta = DeltaRight;
				BestMatchEvent = CpuCoreEvent;
			}

			return TraceServices::EContextSwitchEnumerationResult::Continue;
		});

	if (Delta < 2 * SecondsPerPixel)
	{
		TSharedPtr<FCpuCoreTimingEvent> TimingEvent = MakeShared<FCpuCoreTimingEvent>(SharedThis(this), BestMatchEvent.Start, BestMatchEvent.End, 0);
		TimingEvent->SetSystemThreadId(BestMatchEvent.SystemThreadId);
		return TimingEvent;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TSharedPtr<const ITimingEvent> FCpuCoreTimingTrack::SearchEvent(const FTimingEventSearchParameters& InSearchParameters) const
{
	TSharedPtr<const ITimingEvent> FoundEvent;

	FFilterContext FilterConfiguratorContext;
	FilterConfiguratorContext.SetReturnValueForUnsetFilters(false);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::StartTime), 0.0f);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::EndTime), 0.0f);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::Duration), 0.0f);
	FilterConfiguratorContext.AddFilterData<FString>(static_cast<int32>(EFilterField::TrackName), this->GetName());
	FilterConfiguratorContext.AddFilterData<int64>(static_cast<int32>(EFilterField::CoreEventName), 0);

	TTimingEventSearch<TraceServices::FCpuCoreEvent>::Search(
		InSearchParameters,

		[this, &InSearchParameters](TTimingEventSearch<TraceServices::FCpuCoreEvent>::FContext& InContext)
		{
			TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
			if (Session.IsValid())
			{
				TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
				if (Session.IsValid() && TraceServices::ReadContextSwitchesProvider(*Session.Get()))
				{
					const TraceServices::IContextSwitchesProvider* ContextSwitchesProvider = TraceServices::ReadContextSwitchesProvider(*Session.Get());

					auto Callback = [&InContext](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FCpuCoreEvent& Event)
					{
						InContext.Check(EventStartTime, EventEndTime, EventDepth, Event);
						return InContext.ShouldContinueSearching() ? TraceServices::EContextSwitchEnumerationResult::Continue : TraceServices::EContextSwitchEnumerationResult::Stop;
					};

					if (InContext.GetParameters().SearchDirection == FTimingEventSearchParameters::ESearchDirection::Forward)
					{
						ContextSwitchesProvider->EnumerateCpuCoreEvents(CoreNumber, InSearchParameters.StartTime, InSearchParameters.EndTime,
							[ContextSwitchesProvider, Callback](const TraceServices::FCpuCoreEvent& CpuCoreEvent)
							{
								return Callback(CpuCoreEvent.Start, CpuCoreEvent.End, 0, CpuCoreEvent);
							});
					}
					else
					{
						ContextSwitchesProvider->EnumerateCpuCoreEventsBackwards(CoreNumber, InSearchParameters.EndTime, InSearchParameters.StartTime,
							[ContextSwitchesProvider, Callback](const TraceServices::FCpuCoreEvent& CpuCoreEvent)
							{
								return Callback(CpuCoreEvent.Start, CpuCoreEvent.End, 0, CpuCoreEvent);
							});
					}
				}
			}
		},

		[&FilterConfiguratorContext, &InSearchParameters](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FCpuCoreEvent& Event)
		{
			if (!InSearchParameters.FilterExecutor.IsValid())
			{
				return true;
			}

			TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
			if (Session.IsValid())
			{
				FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::StartTime), EventStartTime);
				FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::EndTime), EventEndTime);
				FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::Duration), EventEndTime - EventStartTime);
				FilterConfiguratorContext.SetFilterData<int64>(static_cast<int32>(EFilterField::CoreEventName), Event.SystemThreadId);

				return InSearchParameters.FilterExecutor->ApplyFilters(FilterConfiguratorContext);
			}

			return false;
		},

		[&FoundEvent, this](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const TraceServices::FCpuCoreEvent& InEvent)
		{
			FoundEvent = MakeShared<FTimingEvent>(SharedThis(this), InFoundStartTime, InFoundEndTime, InFoundDepth, InEvent.SystemThreadId);
		},

		TTimingEventSearch<TraceServices::FCpuCoreEvent>::NoMatch
		);

	return FoundEvent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuCoreTimingTrack::InitTooltip(FTooltipDrawState& InOutTooltip, const ITimingEvent& InTooltipEvent) const
{
	InOutTooltip.ResetContent();

	if (!InTooltipEvent.CheckTrack(this) || !InTooltipEvent.Is<FCpuCoreTimingEvent>())
	{
		return;
	}

	const FCpuCoreTimingEvent& CpuCoreEvent = InTooltipEvent.As<FCpuCoreTimingEvent>();

	const uint32 SystemThreadId = CpuCoreEvent.GetSystemThreadId();
	uint32 ThreadId;
	const TCHAR* ThreadName;
	SharedState.GetThreadInfo(SystemThreadId, ThreadId, ThreadName);

	InOutTooltip.AddTitle(ThreadName);
	InOutTooltip.AddNameValueTextLine(TEXT("System Thread Id:"), FString::Printf(TEXT("%d"), SystemThreadId));
	if (ThreadId != ~0)
	{
		InOutTooltip.AddNameValueTextLine(TEXT("Thread Id:"), FString::Printf(TEXT("%d"), ThreadId));
	}

	InOutTooltip.AddNameValueTextLine(TEXT("Start Time:"), TimeUtils::FormatTimeAuto(InTooltipEvent.GetStartTime(), 6));
	InOutTooltip.AddNameValueTextLine(TEXT("End Time:"), TimeUtils::FormatTimeAuto(InTooltipEvent.GetEndTime(), 6));
	InOutTooltip.AddNameValueTextLine(TEXT("Duration:"), TimeUtils::FormatTimeAuto(InTooltipEvent.GetDuration()));

	InOutTooltip.UpdateLayout();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuCoreTimingTrack::BuildContextMenu(FMenuBuilder& InOutMenuBuilder)
{
	if (SharedState.GetTimingView())
	{
		const TSharedPtr<const ITimingEvent> HoveredEvent = SharedState.GetTimingView()->GetHoveredEvent();
		if (HoveredEvent.IsValid() &&
			&HoveredEvent->GetTrack().Get() == this &&
			HoveredEvent->Is<FCpuCoreTimingEvent>())
		{
			const FCpuCoreTimingEvent& CpuCoreEvent = HoveredEvent->As<FCpuCoreTimingEvent>();
			const uint32 SystemThreadId = CpuCoreEvent.GetSystemThreadId();
			uint32 ThreadId;
			const TCHAR* ThreadName;
			SharedState.GetThreadInfo(SystemThreadId, ThreadId, ThreadName);

			if (ThreadId != ~0)
			{
				SharedState.SetTargetTimingEvent(HoveredEvent);
			}
			else
			{
				SharedState.SetTargetTimingEvent(nullptr);
			}

			InOutMenuBuilder.BeginSection("CpuThread", FText::FromString(ThreadName));
			InOutMenuBuilder.AddMenuEntry(FContextSwitchesStateCommands::Get().Command_NavigateToCpuThreadEvent);
			InOutMenuBuilder.AddMenuEntry(FContextSwitchesStateCommands::Get().Command_DockCpuThreadTrackToBottom);
			InOutMenuBuilder.EndSection();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FCpuCoreTimingTrack::HasCustomFilter() const
{
	TSharedPtr<STimingView> TimingView = SharedState.GetTimingView();
	if (!TimingView)
	{
		return false;
	}

	TSharedPtr<FFilterConfigurator> FilterConfigurator = TimingView->GetFilterConfigurator();
	return FilterConfigurator.IsValid() && !FilterConfigurator->IsEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FCpuCoreTimingTrack::GetThreadName(uint32 InSystemThreadId) const
{
	const TCHAR* ThreadName = nullptr;

	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
		const TraceServices::IContextSwitchesProvider* ContextSwitchesProvider = TraceServices::ReadContextSwitchesProvider(*Session.Get());
		if (ContextSwitchesProvider)
		{
			uint32 ThreadId;
			if (ContextSwitchesProvider->GetThreadId(InSystemThreadId, ThreadId))
			{
				const TraceServices::IThreadProvider& ThreadProvider = TraceServices::ReadThreadProvider(*Session.Get());
				ThreadName = ThreadProvider.GetThreadName(ThreadId);
			}
		}
	}

	if (ThreadName)
	{
		return FString(ThreadName);
	}
	else
	{
		return FString::Printf(TEXT("Unknown %d"), InSystemThreadId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
