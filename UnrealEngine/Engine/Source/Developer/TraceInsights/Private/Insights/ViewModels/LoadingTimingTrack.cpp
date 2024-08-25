// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingTimingTrack.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateBrush.h"
#include "TraceServices/Model/Threads.h"

// Insights
#include "Insights/Common/PaintUtils.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/ITimingViewSession.h"
#include "Insights/ViewModels/Filters.h"
#include "Insights/ViewModels/FilterConfigurator.h"
#include "Insights/ViewModels/TimingEvent.h"
#include "Insights/ViewModels/TimingTrackViewport.h"
#include "Insights/ViewModels/TooltipDrawState.h"
#include "Insights/ViewModels/TimingEventSearch.h"
#include "Insights/Widgets/STimingView.h"

#define LOCTEXT_NAMESPACE "LoadingTimingTrack"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FLoadingTimingViewCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

FLoadingTimingViewCommands::FLoadingTimingViewCommands()
: TCommands<FLoadingTimingViewCommands>(
	TEXT("LoadingTimingViewCommands"),
	NSLOCTEXT("Contexts", "LoadingTimingViewCommands", "Insights - Timing View - Asset Loading"),
	NAME_None,
	FInsightsStyle::GetStyleSetName())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FLoadingTimingViewCommands::~FLoadingTimingViewCommands()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// UI_COMMAND takes long for the compiler to optimize
UE_DISABLE_OPTIMIZATION_SHIP
void FLoadingTimingViewCommands::RegisterCommands()
{
	UI_COMMAND(ShowHideAllLoadingTracks,
		"Asset Loading Tracks",
		"Shows/hides the Asset Loading tracks.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::L));
}
UE_ENABLE_OPTIMIZATION_SHIP

////////////////////////////////////////////////////////////////////////////////////////////////////
// FLoadingSharedState
////////////////////////////////////////////////////////////////////////////////////////////////////

FLoadingSharedState::FLoadingSharedState(STimingView* InTimingView)
	: TimingView(InTimingView)
	, bShowHideAllLoadingTracks(false)
	//, LoadingTracks
	, LoadTimeProfilerTimelineCount(0)
	//, GetEventNameDelegate
{
	check(TimingView != nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingSharedState::OnBeginSession(Insights::ITimingViewSession& InSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	if (TimingView->GetName() == FInsightsManagerTabs::LoadingProfilerTabId)
	{
		bShowHideAllLoadingTracks = true;
	}
	else
	{
		bShowHideAllLoadingTracks = false;
	}

	LoadingTracks.Reset();

	LoadTimeProfilerTimelineCount = 0;

	SetColorSchema(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingSharedState::OnEndSession(Insights::ITimingViewSession& InSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	bShowHideAllLoadingTracks = false;

	LoadingTracks.Reset();

	LoadTimeProfilerTimelineCount = 0;

	GetEventNameDelegate = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingSharedState::Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	const TraceServices::ILoadTimeProfilerProvider* LoadTimeProfilerProvider = TraceServices::ReadLoadTimeProfilerProvider(InAnalysisSession);
	if (LoadTimeProfilerProvider)
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(InAnalysisSession);

		const uint64 CurrentLoadTimeProfilerTimelineCount = LoadTimeProfilerProvider->GetTimelineCount();
		if (CurrentLoadTimeProfilerTimelineCount != LoadTimeProfilerTimelineCount)
		{
			LoadTimeProfilerTimelineCount = CurrentLoadTimeProfilerTimelineCount;

			// Iterate through threads.
			const TraceServices::IThreadProvider& ThreadProvider = TraceServices::ReadThreadProvider(InAnalysisSession);
			ThreadProvider.EnumerateThreads([this, &InSession, LoadTimeProfilerProvider](const TraceServices::FThreadInfo& ThreadInfo)
			{
				// Check available Asset Loading tracks.
				uint32 LoadingTimelineIndex;
				if (LoadTimeProfilerProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, LoadingTimelineIndex))
				{
					if (!LoadingTracks.Contains(LoadingTimelineIndex))
					{
						//const TCHAR* const GroupName = ThreadInfo.GroupName ? ThreadInfo.GroupName : ThreadInfo.Name;
						const FString TrackName(ThreadInfo.Name && *ThreadInfo.Name ? FString::Printf(TEXT("Loading - %s"), ThreadInfo.Name) : FString::Printf(TEXT("Loading - Thread %u"), ThreadInfo.Id));
						TSharedRef<FLoadingTimingTrack> LoadingThreadTrack = MakeShared<FLoadingTimingTrack>(*this, LoadingTimelineIndex, TrackName);
						static_assert(FTimingTrackOrder::GroupRange > 1000, "Order group range too small");
						LoadingThreadTrack->SetOrder(FTimingTrackOrder::Cpu - 1000 + LoadingTracks.Num() * 10);
						LoadingThreadTrack->SetVisibilityFlag(bShowHideAllLoadingTracks);
						InSession.AddScrollableTrack(LoadingThreadTrack);
						LoadingTracks.Add(LoadingTimelineIndex, LoadingThreadTrack);
					}
				}
			});
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingSharedState::ExtendOtherTracksFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InOutMenuBuilder)
{
	if (&InSession != TimingView)
	{
		return;
	}

	InOutMenuBuilder.BeginSection("Asset Loading", LOCTEXT("ContextMenu_Section_AssetLoading", "Asset Loading"));
	{
		InOutMenuBuilder.AddMenuEntry(FLoadingTimingViewCommands::Get().ShowHideAllLoadingTracks);
	}
	InOutMenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingSharedState::BindCommands()
{
	FLoadingTimingViewCommands::Register();

	TSharedPtr<FUICommandList> CommandList = TimingView->GetCommandList();
	ensure(CommandList.IsValid());

	CommandList->MapAction(
		FLoadingTimingViewCommands::Get().ShowHideAllLoadingTracks,
		FExecuteAction::CreateSP(this, &FLoadingSharedState::ShowHideAllLoadingTracks),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FLoadingSharedState::IsAllLoadingTracksToggleOn));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingSharedState::SetAllLoadingTracksToggle(bool bOnOff)
{
	bShowHideAllLoadingTracks = bOnOff;

	for (const auto& KV : LoadingTracks)
	{
		FLoadingTimingTrack& Track = *KV.Value;
		Track.SetVisibilityFlag(bShowHideAllLoadingTracks);
	}

	TimingView->HandleTrackVisibilityChanged();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* FLoadingSharedState::GetEventNameByEventType(uint32 Depth, const TraceServices::FLoadTimeProfilerCpuEvent& Event) const
{
	if (Event.Export)
	{
		return TraceServices::GetLoadTimeProfilerObjectEventTypeString(Event.EventType);
	}
	else
	{
		return TEXT("ProcessPackageHeader");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* FLoadingSharedState::GetEventNameByPackageName(uint32 Depth, const TraceServices::FLoadTimeProfilerCpuEvent& Event) const
{
	return Event.Package ? Event.Package->Name : TEXT("");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* FLoadingSharedState::GetEventNameByExportClassName(uint32 Depth, const TraceServices::FLoadTimeProfilerCpuEvent& Event) const
{
	return Event.Export && Event.Export->Class ? Event.Export->Class->Name : TEXT("");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* FLoadingSharedState::GetEventNameByPackageAndExportClassName(uint32 Depth, const TraceServices::FLoadTimeProfilerCpuEvent& Event) const
{
	if (Depth == 0)
	{
		if (Event.Package)
		{
			return Event.Package->Name;
		}
	}

	if (Event.Export && Event.Export->Class)
	{
		return Event.Export->Class->Name;
	}

	return TEXT("");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* FLoadingSharedState::GetEventName(uint32 Depth, const TraceServices::FLoadTimeProfilerCpuEvent& Event) const
{
	return GetEventNameDelegate.Execute(Depth, Event);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingSharedState::SetColorSchema(int32 Schema)
{
	switch (Schema)
	{
		case 0: GetEventNameDelegate = FLoadingTrackGetEventNameDelegate::CreateRaw(this, &FLoadingSharedState::GetEventNameByEventType); break;
		case 1: GetEventNameDelegate = FLoadingTrackGetEventNameDelegate::CreateRaw(this, &FLoadingSharedState::GetEventNameByPackageName); break;
		case 2: GetEventNameDelegate = FLoadingTrackGetEventNameDelegate::CreateRaw(this, &FLoadingSharedState::GetEventNameByExportClassName); break;
		case 3: GetEventNameDelegate = FLoadingTrackGetEventNameDelegate::CreateRaw(this, &FLoadingSharedState::GetEventNameByPackageAndExportClassName); break;
	};

	for (const auto& KV : LoadingTracks)
	{
		FLoadingTimingTrack& Track = *KV.Value;
		Track.SetDirtyFlag();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FLoadingTimingTrack
////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FLoadingTimingTrack)

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingTimingTrack::BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadLoadTimeProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ILoadTimeProfilerProvider& LoadTimeProfilerProvider = *TraceServices::ReadLoadTimeProfilerProvider(*Session.Get());

		const FTimingTrackViewport& Viewport = Context.GetViewport();

		LoadTimeProfilerProvider.ReadTimeline(TimelineIndex, [this, &Builder, &Viewport](const TraceServices::ILoadTimeProfilerProvider::CpuTimeline& Timeline)
		{
			if (FTimingEventsTrack::bUseDownSampling)
			{
				const double SecondsPerPixel = 1.0 / Viewport.GetScaleX();
				Timeline.EnumerateEventsDownSampled(Viewport.GetStartTime(), Viewport.GetEndTime(), SecondsPerPixel, [this, &Builder](double StartTime, double EndTime, uint32 Depth, const TraceServices::FLoadTimeProfilerCpuEvent& Event)
				{
					if (Event.Package)
					{
						const TCHAR* Name = SharedState.GetEventName(Depth, Event);
						const uint64 Type = static_cast<uint64>(Event.EventType);
						const uint32 Color = 0;
						Builder.AddEvent(StartTime, EndTime, Depth, Name, Type, Color);
					}
					return TraceServices::EEventEnumerate::Continue;
				});
			}
			else
			{
				Timeline.EnumerateEvents(Viewport.GetStartTime(), Viewport.GetEndTime(), [this, &Builder](double StartTime, double EndTime, uint32 Depth, const TraceServices::FLoadTimeProfilerCpuEvent& Event)
				{
					if (Event.Package)
					{
						const TCHAR* Name = SharedState.GetEventName(Depth, Event);
						const uint64 Type = static_cast<uint64>(Event.EventType);
						const uint32 Color = 0;
						Builder.AddEvent(StartTime, EndTime, Depth, Name, Type, Color);
					}
					return TraceServices::EEventEnumerate::Continue;
				});
			}
		});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingTimingTrack::BuildFilteredDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context)
{
	if (HasCustomFilter())
	{
		using namespace Insights;

		FFilterContext FilterConfiguratorContext;
		FilterConfiguratorContext.SetReturnValueForUnsetFilters(false);
		FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::StartTime), 0.0f);
		FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::EndTime), 0.0f);
		FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::Duration), 0.0f);
		FilterConfiguratorContext.AddFilterData<FString>(static_cast<int32>(EFilterField::TrackName), this->GetName());

		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadLoadTimeProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

			const TraceServices::ILoadTimeProfilerProvider& LoadTimeProfilerProvider = *TraceServices::ReadLoadTimeProfilerProvider(*Session.Get());

			const FTimingTrackViewport& Viewport = Context.GetViewport();

			LoadTimeProfilerProvider.ReadTimeline(TimelineIndex, [this, &Builder, &Viewport, &FilterConfiguratorContext](const TraceServices::ILoadTimeProfilerProvider::CpuTimeline& Timeline)
			{
				Timeline.EnumerateEvents(Viewport.GetStartTime(), Viewport.GetEndTime(), [this, &Builder, &FilterConfiguratorContext](double StartTime, double EndTime, uint32 Depth, const TraceServices::FLoadTimeProfilerCpuEvent& Event)
				{
					if (Event.Package)
					{
						FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::StartTime), StartTime);
						FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::EndTime), EndTime);
						FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::Duration), EndTime - StartTime);

						if (FilterConfigurator->ApplyFilters(FilterConfiguratorContext))
						{
							const TCHAR* Name = SharedState.GetEventName(Depth, Event);
							const uint64 Type = static_cast<uint64>(Event.EventType);
							const uint32 Color = 0;
							Builder.AddEvent(StartTime, EndTime, Depth, Name, Type, Color);
						}
					}

					return TraceServices::EEventEnumerate::Continue;
				});
			});
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingTimingTrack::InitTooltip(FTooltipDrawState& InOutTooltip, const ITimingEvent& InTooltipEvent) const
{
	if (InTooltipEvent.CheckTrack(this) && InTooltipEvent.Is<FTimingEvent>())
	{
		const FTimingEvent& TooltipEvent = InTooltipEvent.As<FTimingEvent>();

		auto MatchEvent = [&TooltipEvent](double InStartTime, double InEndTime, uint32 InDepth)
		{
			return InDepth == TooltipEvent.GetDepth()
				&& InStartTime == TooltipEvent.GetStartTime()
				&& InEndTime == TooltipEvent.GetEndTime();
		};

		FTimingEventSearchParameters SearchParameters(TooltipEvent.GetStartTime(), TooltipEvent.GetEndTime(), ETimingEventSearchFlags::StopAtFirstMatch, MatchEvent);
		FindLoadTimeProfilerCpuEvent(SearchParameters, [this, &InOutTooltip, &TooltipEvent](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const TraceServices::FLoadTimeProfilerCpuEvent& InFoundEvent)
		{
			InOutTooltip.ResetContent();

			InOutTooltip.AddTitle(SharedState.GetEventName(TooltipEvent.GetDepth(), InFoundEvent));

			const TraceServices::FPackageExportInfo* Export = InFoundEvent.Export;
			const TraceServices::FPackageInfo* Package = InFoundEvent.Export ? InFoundEvent.Export->Package : InFoundEvent.Package;

			InOutTooltip.AddNameValueTextLine(TEXT("Duration:"), TimeUtils::FormatTimeAuto(TooltipEvent.GetDuration()));
			InOutTooltip.AddNameValueTextLine(TEXT("Depth:"), FString::Printf(TEXT("%d"), TooltipEvent.GetDepth()));

			if (Package)
			{
				InOutTooltip.AddNameValueTextLine(TEXT("Package Name:"), Package->Name);
				InOutTooltip.AddNameValueTextLine(TEXT("Header Size:"), FString::Printf(TEXT("%s bytes"), *FText::AsNumber(Package->Summary.TotalHeaderSize).ToString()));
				InOutTooltip.AddNameValueTextLine(TEXT("Package Summary:"), FString::Printf(TEXT("%d imports, %d exports"), Package->Summary.ImportCount, Package->Summary.ExportCount));
				InOutTooltip.AddNameValueTextLine(TEXT("Request Priority:"), FString::Printf(TEXT("%d"), Package->Summary.Priority));
				if (!Export)
				{
					InOutTooltip.AddNameValueTextLine(TEXT("Event:"), TEXT("ProcessPackageSummary"));
				}
			}

			if (Export)
			{
				InOutTooltip.AddNameValueTextLine(TEXT("Event:"), FString::Printf(TEXT("%s"), TraceServices::GetLoadTimeProfilerObjectEventTypeString(InFoundEvent.EventType)));
				InOutTooltip.AddNameValueTextLine(TEXT("Export Class:"), Export->Class ? Export->Class->Name : TEXT("N/A"));
				InOutTooltip.AddNameValueTextLine(TEXT("Serial Size:"), FString::Printf(TEXT("%s bytes"), *FText::AsNumber(Export->SerialSize).ToString()));
			}

			InOutTooltip.UpdateLayout();
		});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TSharedPtr<const ITimingEvent> FLoadingTimingTrack::SearchEvent(const FTimingEventSearchParameters& InSearchParameters) const
{
	TSharedPtr<const ITimingEvent> FoundEvent;

	FindLoadTimeProfilerCpuEvent(InSearchParameters, [this, &FoundEvent](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const TraceServices::FLoadTimeProfilerCpuEvent& InFoundEvent)
	{
		FoundEvent = MakeShared<FTimingEvent>(SharedThis(this), InFoundStartTime, InFoundEndTime, InFoundDepth);
	});

	return FoundEvent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FLoadingTimingTrack::FindLoadTimeProfilerCpuEvent(const FTimingEventSearchParameters& InParameters, TFunctionRef<void(double, double, uint32, const TraceServices::FLoadTimeProfilerCpuEvent&)> InFoundPredicate) const
{
	using namespace Insights;

	FFilterContext FilterConfiguratorContext;
	FilterConfiguratorContext.SetReturnValueForUnsetFilters(false);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::StartTime), 0.0f);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::EndTime), 0.0f);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::Duration), 0.0f);
	FilterConfiguratorContext.AddFilterData<FString>(static_cast<int32>(EFilterField::TrackName), this->GetName());

	return TTimingEventSearch<TraceServices::FLoadTimeProfilerCpuEvent>::Search(
		InParameters,

		[this](TTimingEventSearch<TraceServices::FLoadTimeProfilerCpuEvent>::FContext& InContext)
		{
			TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
			if (Session.IsValid())
			{
				TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

				if (TraceServices::ReadLoadTimeProfilerProvider(*Session.Get()))
				{
					const TraceServices::ILoadTimeProfilerProvider& LoadTimeProfilerProvider = *TraceServices::ReadLoadTimeProfilerProvider(*Session.Get());

					LoadTimeProfilerProvider.ReadTimeline(TimelineIndex, [&InContext](const TraceServices::ILoadTimeProfilerProvider::CpuTimeline& Timeline)
					{
						auto Callback = [&InContext](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FLoadTimeProfilerCpuEvent& Event)
						{
							if (Event.Package)
							{
								InContext.Check(EventStartTime, EventEndTime, EventDepth, Event);
								return InContext.ShouldContinueSearching() ? TraceServices::EEventEnumerate::Continue : TraceServices::EEventEnumerate::Stop;
							}
							else
							{
								return TraceServices::EEventEnumerate::Continue;
							}
						};

						if (InContext.GetParameters().SearchDirection == FTimingEventSearchParameters::ESearchDirection::Forward)
						{
							Timeline.EnumerateEvents(InContext.GetParameters().StartTime, InContext.GetParameters().EndTime, Callback);
						}
						else
						{
							Timeline.EnumerateEventsBackwards(InContext.GetParameters().EndTime, InContext.GetParameters().StartTime, Callback);
						}
					});
				}
			}
		},

		[&FilterConfiguratorContext, &InParameters](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FLoadTimeProfilerCpuEvent& Event)
		{
			if (!InParameters.FilterExecutor.IsValid())
			{
				return true;
			}

			TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
			if (Session.IsValid())
			{
				TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

				if (TraceServices::ReadTimingProfilerProvider(*Session.Get()))
				{
					FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::StartTime), EventStartTime);
					FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::EndTime), EventEndTime);
					FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::Duration), EventEndTime - EventStartTime);

					return InParameters.FilterExecutor->ApplyFilters(FilterConfiguratorContext);
				}
			}

			return false;
		},

		[&InFoundPredicate](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const TraceServices::FLoadTimeProfilerCpuEvent& InEvent)
		{
			InFoundPredicate(InFoundStartTime, InFoundEndTime, InFoundDepth, InEvent);
		},

		TTimingEventSearch<TraceServices::FLoadTimeProfilerCpuEvent>::NoMatch);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FLoadingTimingTrack::HasCustomFilter() const
{
	return FilterConfigurator.IsValid() && !FilterConfigurator->IsEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLoadingTimingTrack::SetFilterConfigurator(TSharedPtr<Insights::FFilterConfigurator> InFilterConfigurator)
{
	if (FilterConfigurator != InFilterConfigurator)
	{
		FilterConfigurator = InFilterConfigurator;
		SetDirtyFlag();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
