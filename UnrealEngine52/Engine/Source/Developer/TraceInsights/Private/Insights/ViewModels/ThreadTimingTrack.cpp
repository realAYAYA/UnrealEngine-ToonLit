// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTimingTrack.h"

#include "CborReader.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Serialization/MemoryReader.h"
#include "Styling/SlateBrush.h"
#include "TraceServices/Model/LoadTimeProfiler.h"
#include "TraceServices/Model/TasksProfiler.h"
#include "TraceServices/Model/Threads.h"
#include "Async/TaskGraphInterfaces.h"

// Insights
#include "Insights/Common/PaintUtils.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/ITimingViewSession.h"
#include "Insights/Log.h"
#include "Insights/TimingProfilerManager.h"
#include "Insights/ViewModels/Filters.h"
#include "Insights/ViewModels/FilterConfigurator.h"
#include "Insights/ViewModels/TimerNode.h"
#include "Insights/ViewModels/ThreadTrackEvent.h"
#include "Insights/ViewModels/TimingEventSearch.h"
#include "Insights/ViewModels/TimingTrackViewport.h"
#include "Insights/ViewModels/TimingViewDrawHelper.h"
#include "Insights/ViewModels/TooltipDrawState.h"
#include "Insights/Widgets/STimingView.h"

#define LOCTEXT_NAMESPACE "ThreadTimingTrack"

using namespace Insights;

////////////////////////////////////////////////////////////////////////////////////////////////////
// FThreadTimingViewCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

FThreadTimingViewCommands::FThreadTimingViewCommands()
: TCommands<FThreadTimingViewCommands>(
	TEXT("ThreadTimingViewCommands"),
	NSLOCTEXT("Contexts", "ThreadTimingViewCommands", "Insights - Timing View - Threads"),
	NAME_None,
	FInsightsStyle::GetStyleSetName())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FThreadTimingViewCommands::~FThreadTimingViewCommands()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// UI_COMMAND takes long for the compiler to optimize
UE_DISABLE_OPTIMIZATION_SHIP
void FThreadTimingViewCommands::RegisterCommands()
{
	UI_COMMAND(ShowHideAllGpuTracks,
		"GPU Track(s)",
		"Shows/hides the GPU track(s).",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::Y));

	UI_COMMAND(ShowHideAllCpuTracks,
		"CPU Thread Tracks",
		"Shows/hides all CPU tracks (and all CPU thread groups).",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::U));
}
UE_ENABLE_OPTIMIZATION_SHIP

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

static void AppendMetadataToTooltip(FTooltipDrawState& Tooltip, TArrayView<const uint8>& Metadata)
{
	FMemoryReaderView MemoryReader(Metadata);
	FCborReader CborReader(&MemoryReader, ECborEndianness::StandardCompliant);
	FCborContext Context;

	if (!CborReader.ReadNext(Context) || Context.MajorType() != ECborCode::Map)
	{
		return;
	}

	Tooltip.AddTitle(TEXT("Metadata:"));

	while (true)
	{
		// Read key
		if (!CborReader.ReadNext(Context) || !Context.IsString())
		{
			break;
		}

		FString Key(static_cast<int32>(Context.AsLength()), Context.AsCString());
		Key += TEXT(":");

		if (!CborReader.ReadNext(Context))
		{
			break;
		}

		switch (Context.MajorType())
		{
		case ECborCode::Int:
		case ECborCode::Uint:
			{
				uint64 Value = Context.AsUInt();
				FString ValueStr;
				if (Value > 999'999'999ULL)
				{
					ValueStr = FString::Printf(TEXT("0x%llX"), Value);
				}
				else
				{
					ValueStr = FString::Printf(TEXT("%llu"), Value);
				}
				Tooltip.AddNameValueTextLine(Key, ValueStr);
				continue;
			}

		case ECborCode::TextString:
			{
				FString Value = Context.AsString();
				Tooltip.AddNameValueTextLine(Key, Value);
				continue;
			}

		case ECborCode::ByteString:
			{
				FAnsiStringView Value(Context.AsCString(), static_cast<int32>(Context.AsLength()));
				FString ValueStr(Value);
				Tooltip.AddNameValueTextLine(Key, ValueStr);
				continue;
			}
		}

		if (Context.RawCode() == (ECborCode::Prim|ECborCode::Value_4Bytes))
		{
			float Value = Context.AsFloat();
			FString ValueStr = FString::Printf(TEXT("%f"), Value);
			Tooltip.AddNameValueTextLine(Key, ValueStr);
			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim|ECborCode::Value_8Bytes))
		{
			double Value = Context.AsDouble();
			FString ValueStr = FString::Printf(TEXT("%g"), Value);
			Tooltip.AddNameValueTextLine(Key, ValueStr);
			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::False))
		{
			Tooltip.AddNameValueTextLine(Key, FString(TEXT("false")));
			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::True))
		{
			Tooltip.AddNameValueTextLine(Key, FString(TEXT("true")));
			continue;
		}

		if (Context.IsFiniteContainer())
		{
			CborReader.SkipContainer(ECborCode::Array);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static void AppendMetadataToString(FString& Str, TArrayView<const uint8>& Metadata)
{
	FMemoryReaderView MemoryReader(Metadata);
	FCborReader CborReader(&MemoryReader, ECborEndianness::StandardCompliant);
	FCborContext Context;

	if (!CborReader.ReadNext(Context) || Context.MajorType() != ECborCode::Map)
	{
		return;
	}

	bool bFirst = true;

	while (true)
	{
		// Read key
		if (!CborReader.ReadNext(Context) || !Context.IsString())
		{
			break;
		}

		if (bFirst)
		{
			bFirst = false;
			Str += TEXT(" - ");
		}
		else
		{
			Str += TEXT(", ");
		}

		//FString Key(Context.AsLength(), Context.AsCString());
		//Str += Key;
		//Str += TEXT(":");

		if (!CborReader.ReadNext(Context))
		{
			break;
		}

		switch (Context.MajorType())
		{
		case ECborCode::Int:
		case ECborCode::Uint:
			{
				uint64 Value = Context.AsUInt();
				if (Value > 999'999'999ULL)
				{
					Str += FString::Printf(TEXT("0x%llX"), Value);
				}
				else
				{
					Str += FString::Printf(TEXT("%llu"), Value);
				}
				continue;
			}

		case ECborCode::TextString:
			{
				Str += Context.AsString();
				continue;
			}

		case ECborCode::ByteString:
			{
				Str.AppendChars(Context.AsCString(), static_cast<int32>(Context.AsLength()));
				continue;
			}
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::Value_4Bytes))
		{
			float Value = Context.AsFloat();
			Str += FString::Printf(TEXT("%f"), Value);
			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::Value_8Bytes))
		{
			double Value = Context.AsDouble();
			Str += FString::Printf(TEXT("%g"), Value);
			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::False))
		{
			Str += TEXT("false");
			continue;
		}

		if (Context.RawCode() == (ECborCode::Prim | ECborCode::True))
		{
			Str += TEXT("true");
			continue;
		}

		if (Context.IsFiniteContainer())
		{
			CborReader.SkipContainer(ECborCode::Array);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static void AddTimingEventToBuilder(ITimingEventsTrackDrawStateBuilder& Builder,
									double EventStartTime, double EventEndTime, uint32 EventDepth,
									uint32 TimerIndex, const TraceServices::FTimingProfilerTimer* Timer)
{
	if (EventDepth >= FTimingProfilerManager::Get()->GetEventDepthLimit())
	{
		return;
	}

	uint32 EventColor;
	switch (FTimingProfilerManager::Get()->GetColoringMode())
	{
		case Insights::ETimingEventsColoringMode::ByTimerName:
			EventColor = FTimingEvent::ComputeEventColor(Timer->Name);
			break;
		case Insights::ETimingEventsColoringMode::ByTimerId:
			EventColor = FTimingEvent::ComputeEventColor(Timer->Id);
			break;
		case Insights::ETimingEventsColoringMode::ByDuration:
		{
			const double EventDuration = EventEndTime - EventStartTime;
			EventColor = (EventDuration >= 0.01)     ? 0xFF883333 : // red:    >= 10ms
						 (EventDuration >= 0.001)    ? 0xFF998833 : // yellow: [1ms .. 10ms)
						 (EventDuration >= 0.0001)   ? 0xFF338833 : // green:  [100us .. 1ms)
						 (EventDuration >= 0.00001)  ? 0xFF338888 : // cyan:   [10us .. 100us)
						 (EventDuration >= 0.000001) ? 0xFF333388 : // blue:   [1us .. 10us)
						                               0xFF888888;  // grey:   < 1us
			break;
		}
		default:
			EventColor = 0xFF000000;
	}

	Builder.AddEvent(EventStartTime, EventEndTime, EventDepth, EventColor,
		[TimerIndex, Timer, EventStartTime, EventEndTime](float Width)
		{
			FString EventName = Timer->Name;

			if (Width > EventName.Len() * 4.0f + 32.0f)
			{
				//EventName = TEXT("*") + EventName; // for debugging

				const double Duration = EventEndTime - EventStartTime;
				FTimingEventsTrackDrawStateBuilder::AppendDurationToEventName(EventName, Duration);

				if (int32(TimerIndex) < 0) // has metadata?
				{
					//EventName = TEXT("!") + EventName; // for debugging

					TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
					check(Session.IsValid());

					//TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

					const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

					const TraceServices::ITimingProfilerTimerReader* TimerReader;
					TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

					TArrayView<const uint8> Metadata = TimerReader->GetMetadata(TimerIndex);
					if (Metadata.Num() > 0)
					{
						AppendMetadataToString(EventName, Metadata);
					}
				}
			}

			return EventName;
		});
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FThreadTimingSharedState
////////////////////////////////////////////////////////////////////////////////////////////////////

FThreadTimingSharedState::FThreadTimingSharedState(STimingView* InTimingView)
	: TimingView(InTimingView)
	, bShowHideAllGpuTracks(false)
	, bShowHideAllCpuTracks(false)
	, GpuTrack()
	, Gpu2Track()
	//, CpuTracks
	//, ThreadGroups
	, TimingProfilerTimelineCount(0)
	, LoadTimeProfilerTimelineCount(0)
{
	check(TimingView != nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FCpuTimingTrack> FThreadTimingSharedState::GetCpuTrack(uint32 InThreadId)
{
	TSharedPtr<FCpuTimingTrack>*const TrackPtrPtr = CpuTracks.Find(InThreadId);
	return TrackPtrPtr ? *TrackPtrPtr : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FThreadTimingSharedState::IsGpuTrackVisible() const
{
	return (GpuTrack != nullptr && GpuTrack->IsVisible()) || (Gpu2Track != nullptr && Gpu2Track->IsVisible());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FThreadTimingSharedState::IsCpuTrackVisible(uint32 InThreadId) const
{
	const TSharedPtr<FCpuTimingTrack>*const TrackPtrPtr = CpuTracks.Find(InThreadId);
	return TrackPtrPtr && (*TrackPtrPtr)->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::GetVisibleCpuThreads(TSet<uint32>& OutSet) const
{
	OutSet.Reset();
	for (const auto& KV : CpuTracks)
	{
		const FCpuTimingTrack& Track = *KV.Value;
		if (Track.IsVisible())
		{
			OutSet.Add(KV.Key);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::OnBeginSession(Insights::ITimingViewSession& InSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	if (TimingView->GetName() == FInsightsManagerTabs::LoadingProfilerTabId)
	{
		bShowHideAllGpuTracks = false;
		bShowHideAllCpuTracks = false;
	}
	else
	{
		bShowHideAllGpuTracks = true;
		bShowHideAllCpuTracks = true;
	}

	GpuTrack = nullptr;
	Gpu2Track = nullptr;
	CpuTracks.Reset();
	ThreadGroups.Reset();

	TimingProfilerTimelineCount = 0;
	LoadTimeProfilerTimelineCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::OnEndSession(Insights::ITimingViewSession& InSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	bShowHideAllGpuTracks = false;
	bShowHideAllCpuTracks = false;

	GpuTrack = nullptr;
	Gpu2Track = nullptr;
	CpuTracks.Reset();
	ThreadGroups.Reset();

	TimingProfilerTimelineCount = 0;
	LoadTimeProfilerTimelineCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	const TraceServices::ITimingProfilerProvider* TimingProfilerProvider = TraceServices::ReadTimingProfilerProvider(InAnalysisSession);
	const TraceServices::ILoadTimeProfilerProvider* LoadTimeProfilerProvider = TraceServices::ReadLoadTimeProfilerProvider(InAnalysisSession);

	if (TimingProfilerProvider)
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(InAnalysisSession);

		const uint64 CurrentTimingProfilerTimelineCount = TimingProfilerProvider->GetTimelineCount();
		const uint64 CurrentLoadTimeProfilerTimelineCount = (LoadTimeProfilerProvider) ? LoadTimeProfilerProvider->GetTimelineCount() : 0;

		if (CurrentTimingProfilerTimelineCount != TimingProfilerTimelineCount ||
			CurrentLoadTimeProfilerTimelineCount != LoadTimeProfilerTimelineCount)
		{
			TimingProfilerTimelineCount = CurrentTimingProfilerTimelineCount;
			LoadTimeProfilerTimelineCount = CurrentLoadTimeProfilerTimelineCount;

			LLM_SCOPE_BYTAG(Insights);

			// Check if we have a GPU track.
			if (!GpuTrack.IsValid())
			{
				uint32 GpuTimelineIndex;
				if (TimingProfilerProvider->GetGpuTimelineIndex(GpuTimelineIndex))
				{
					GpuTrack = MakeShared<FGpuTimingTrack>(*this, TEXT("GPU"), nullptr, GpuTimelineIndex, FGpuTimingTrack::Gpu1ThreadId);
					GpuTrack->SetOrder(FTimingTrackOrder::Gpu);
					GpuTrack->SetVisibilityFlag(bShowHideAllGpuTracks);
					InSession.AddScrollableTrack(GpuTrack);
				}
			}
			if (!Gpu2Track.IsValid())
			{
				uint32 GpuTimelineIndex;
				if (TimingProfilerProvider->GetGpu2TimelineIndex(GpuTimelineIndex))
				{
					Gpu2Track = MakeShared<FGpuTimingTrack>(*this, TEXT("GPU2"), nullptr, GpuTimelineIndex, FGpuTimingTrack::Gpu2ThreadId);
					Gpu2Track->SetOrder(FTimingTrackOrder::Gpu + 1);
					Gpu2Track->SetVisibilityFlag(bShowHideAllGpuTracks);
					InSession.AddScrollableTrack(Gpu2Track);
				}
			}

			bool bTracksOrderChanged = false;
			int32 Order = FTimingTrackOrder::Cpu;

			// Iterate through threads.
			const TraceServices::IThreadProvider& ThreadProvider = TraceServices::ReadThreadProvider(InAnalysisSession);
			ThreadProvider.EnumerateThreads([this, &InSession, &bTracksOrderChanged, &Order, TimingProfilerProvider, LoadTimeProfilerProvider](const TraceServices::FThreadInfo& ThreadInfo)
			{
				// Check if this thread is part of a group?
				bool bIsGroupVisible = bShowHideAllCpuTracks;
				const TCHAR* GroupName = ThreadInfo.GroupName;
				if (!GroupName || *GroupName == 0)
				{
					GroupName = ThreadInfo.Name;
				}
				if (!GroupName || *GroupName == 0)
				{
					GroupName = TEXT("Other Threads");
				}
				if (!ThreadGroups.Contains(GroupName))
				{
					// Note: The GroupName pointer should be valid for the duration of the session.
					ThreadGroups.Add(GroupName, { GroupName, bIsGroupVisible, 0, Order });
				}
				else
				{
					FThreadGroup& ThreadGroup = ThreadGroups[GroupName];
					bIsGroupVisible = ThreadGroup.bIsVisible;
					ThreadGroup.Order = Order;
				}

				// Check if there is an available Asset Loading track for this thread.
				bool bIsLoadingThread = false;
				uint32 LoadingTimelineIndex;
				if (LoadTimeProfilerProvider && LoadTimeProfilerProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, LoadingTimelineIndex))
				{
					bIsLoadingThread = true;
				}

				// Check if there is an available CPU track for this thread.
				uint32 CpuTimelineIndex;
				if (TimingProfilerProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, CpuTimelineIndex))
				{
					TSharedPtr<FCpuTimingTrack>* TrackPtrPtr = CpuTracks.Find(ThreadInfo.Id);
					if (TrackPtrPtr == nullptr)
					{
						FString TrackName(ThreadInfo.Name && *ThreadInfo.Name ? ThreadInfo.Name : FString::Printf(TEXT("Thread %u"), ThreadInfo.Id));

						// Create new Timing Events track for the CPU thread.
						TSharedPtr<FCpuTimingTrack> Track = MakeShared<FCpuTimingTrack>(*this, TrackName, GroupName, CpuTimelineIndex, ThreadInfo.Id);
						Track->SetOrder(Order);
						CpuTracks.Add(ThreadInfo.Id, Track);

						FThreadGroup& ThreadGroup = ThreadGroups[GroupName];
						ThreadGroup.NumTimelines++;

						if (bIsLoadingThread &&
							TimingView->GetName() == FInsightsManagerTabs::LoadingProfilerTabId)
						{
							Track->SetVisibilityFlag(true);
							ThreadGroup.bIsVisible = true;
						}
						else
						{
							Track->SetVisibilityFlag(bIsGroupVisible);
						}

						InSession.AddScrollableTrack(Track);
					}
					else
					{
						TSharedPtr<FCpuTimingTrack> Track = *TrackPtrPtr;
						if (Track->GetOrder() != Order)
						{
							Track->SetOrder(Order);
							bTracksOrderChanged = true;
						}
					}
				}

				constexpr int32 OrderIncrement = FTimingTrackOrder::GroupRange / 1000; // distribute max 1000 tracks in the order group range
				static_assert(OrderIncrement >= 1, "Order group range too small");
				Order += OrderIncrement;
			});

			if (bTracksOrderChanged)
			{
				InSession.InvalidateScrollableTracksOrder();
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::ExtendGpuTracksFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InOutMenuBuilder)
{
	if (&InSession != TimingView)
	{
		return;
	}

	InOutMenuBuilder.BeginSection("GpuTracks", LOCTEXT("ContextMenu_Section_GpuTracks", "GPU Tracks"));
	{
		InOutMenuBuilder.AddMenuEntry(FThreadTimingViewCommands::Get().ShowHideAllGpuTracks);
	}
	InOutMenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::ExtendCpuTracksFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InOutMenuBuilder)
{
	if (&InSession != TimingView)
	{
		return;
	}

	InOutMenuBuilder.BeginSection("CpuTracks", LOCTEXT("ContextMenu_Section_CpuTracks", "CPU Tracks"));
	{
		InOutMenuBuilder.AddMenuEntry(FThreadTimingViewCommands::Get().ShowHideAllCpuTracks);
	}
	InOutMenuBuilder.EndSection();

	InOutMenuBuilder.BeginSection("CpuThreadGroups", LOCTEXT("ContextMenu_Section_CpuThreadGroups", "CPU Thread Groups"));
	CreateThreadGroupsMenu(InOutMenuBuilder);
	InOutMenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::BindCommands()
{
	FThreadTimingViewCommands::Register();

	TSharedPtr<FUICommandList> CommandList = TimingView->GetCommandList();
	ensure(CommandList.IsValid());

	CommandList->MapAction(
		FThreadTimingViewCommands::Get().ShowHideAllGpuTracks,
		FExecuteAction::CreateSP(this, &FThreadTimingSharedState::ShowHideAllGpuTracks),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FThreadTimingSharedState::IsAllGpuTracksToggleOn));

	CommandList->MapAction(
		FThreadTimingViewCommands::Get().ShowHideAllCpuTracks,
		FExecuteAction::CreateSP(this, &FThreadTimingSharedState::ShowHideAllCpuTracks),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FThreadTimingSharedState::IsAllCpuTracksToggleOn));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::CreateThreadGroupsMenu(FMenuBuilder& InOutMenuBuilder)
{
	// Sort the list of thread groups.
	TArray<const FThreadGroup*> SortedThreadGroups;
	SortedThreadGroups.Reserve(ThreadGroups.Num());
	for (const auto& KV : ThreadGroups)
	{
		SortedThreadGroups.Add(&KV.Value);
	}
	Algo::SortBy(SortedThreadGroups, &FThreadGroup::GetOrder);

	for (const FThreadGroup* ThreadGroupPtr : SortedThreadGroups)
	{
		const FThreadGroup& ThreadGroup = *ThreadGroupPtr;
		if (ThreadGroup.NumTimelines > 0)
		{
			InOutMenuBuilder.AddMenuEntry(
				//FText::FromString(ThreadGroup.Name),
				FText::Format(LOCTEXT("ThreadGroupFmt", "{0} ({1})"), FText::FromString(ThreadGroup.Name), ThreadGroup.NumTimelines),
				TAttribute<FText>(), // no tooltip
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FThreadTimingSharedState::ToggleTrackVisibilityByGroup_Execute, ThreadGroup.Name),
						  FCanExecuteAction::CreateLambda([] { return true; }),
						  FIsActionChecked::CreateSP(this, &FThreadTimingSharedState::ToggleTrackVisibilityByGroup_IsChecked, ThreadGroup.Name)),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::SetAllCpuTracksToggle(bool bOnOff)
{
	bShowHideAllCpuTracks = bOnOff;

	for (const auto& KV : CpuTracks)
	{
		FCpuTimingTrack& Track = *KV.Value;
		Track.SetVisibilityFlag(bShowHideAllCpuTracks);
	}

	for (auto& KV : ThreadGroups)
	{
		KV.Value.bIsVisible = bShowHideAllCpuTracks;
	}

	TimingView->OnTrackVisibilityChanged();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::SetAllGpuTracksToggle(bool bOnOff)
{
	bShowHideAllGpuTracks = bOnOff;

	if (GpuTrack.IsValid())
	{
		GpuTrack->SetVisibilityFlag(bShowHideAllGpuTracks);
	}
	if (Gpu2Track.IsValid())
	{
		Gpu2Track->SetVisibilityFlag(bShowHideAllGpuTracks);
	}
	if (GpuTrack.IsValid() || Gpu2Track.IsValid())
	{
		TimingView->OnTrackVisibilityChanged();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FThreadTimingSharedState::ToggleTrackVisibilityByGroup_IsChecked(const TCHAR* InGroupName) const
{
	if (ThreadGroups.Contains(InGroupName))
	{
		const FThreadGroup& ThreadGroup = ThreadGroups[InGroupName];
		return ThreadGroup.bIsVisible;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingSharedState::ToggleTrackVisibilityByGroup_Execute(const TCHAR* InGroupName)
{
	if (ThreadGroups.Contains(InGroupName))
	{
		FThreadGroup& ThreadGroup = ThreadGroups[InGroupName];
		ThreadGroup.bIsVisible = !ThreadGroup.bIsVisible;

		for (const auto& KV : CpuTracks)
		{
			FCpuTimingTrack& Track = *KV.Value;
			if (Track.GetGroupName() == InGroupName)
			{
				Track.SetVisibilityFlag(ThreadGroup.bIsVisible);
			}
		}

		TimingView->OnTrackVisibilityChanged();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FThreadTimingTrack
////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FThreadTimingTrack)

////////////////////////////////////////////////////////////////////////////////////////////////////

FThreadTimingTrack::~FThreadTimingTrack()
{
	if (FilterConfigurator.IsValid())
	{
		FilterConfigurator->GetOnChangesCommitedEvent().Remove(OnFilterChangesCommitedHandle);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		const FTimingTrackViewport& Viewport = Context.GetViewport();

		TimingProfilerProvider.ReadTimeline(TimelineIndex,
			[&Viewport, this, &Builder, TimerReader](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
			{
				if (FTimingEventsTrack::bUseDownSampling)
				{
					const double SecondsPerPixel = 1.0 / Viewport.GetScaleX();
					Timeline.EnumerateEventsDownSampled(Viewport.GetStartTime(), Viewport.GetEndTime(), SecondsPerPixel,
						[this, &Builder, TimerReader](double StartTime, double EndTime, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
						{
							const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
							if (ensure(Timer != nullptr))
							{
								AddTimingEventToBuilder(Builder, StartTime, EndTime, Depth, Event.TimerIndex, Timer);
							}
							else
							{
								Builder.AddEvent(StartTime, EndTime, Depth, 0xFF000000, [&Event](float) { return FString::Printf(TEXT("[%u]"), Event.TimerIndex); });
							}
							return TraceServices::EEventEnumerate::Continue;
						});
				}
				else
				{
					Timeline.EnumerateEvents(Viewport.GetStartTime(), Viewport.GetEndTime(),
						[this, &Builder, TimerReader](double StartTime, double EndTime, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
						{
							const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
							if (ensure(Timer != nullptr))
							{
								AddTimingEventToBuilder(Builder, StartTime, EndTime, Depth, Event.TimerIndex, Timer);
							}
							else
							{
								Builder.AddEvent(StartTime, EndTime, Depth, 0xFF000000, [&Event](float) { return FString::Printf(TEXT("[%u]"), Event.TimerIndex); });
							}
							return TraceServices::EEventEnumerate::Continue;
						});
				}
			});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::BuildFilteredDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context)
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
		if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

			const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

			const TraceServices::ITimingProfilerTimerReader* TimerReader;
			TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

			const FTimingTrackViewport& Viewport = Context.GetViewport();

			if (bFilterOnlyByEventType)
			{
				//TODO: Add a setting to switch this on/off
				if (true)
				{
					TimingProfilerProvider.ReadTimeline(TimelineIndex,
						[&Viewport, this, &Builder, TimerReader, FilterEventType](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
						{
							TArray<TArray<FPendingEventInfo>> FilteredEvents;

							TraceServices::ITimeline<TraceServices::FTimingProfilerEvent>::EnumerateAsyncParams Params;
							Params.IntervalStart = Viewport.GetStartTime();
							Params.IntervalEnd = Viewport.GetEndTime();
							Params.Resolution = 0.0;
							Params.SetupCallback = [&FilteredEvents](uint32 NumTasks)
							{
								FilteredEvents.AddDefaulted(NumTasks);
							};
							Params.Callback = [this, &Builder, TimerReader, FilterEventType, &FilteredEvents](double StartTime, double EndTime, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event, uint32 TaskIndex)
							{
								const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
								if (ensure(Timer != nullptr))
								{
									if (Timer->Id == FilterEventType)
									{
										FPendingEventInfo TimelineEvent;
										TimelineEvent.StartTime = StartTime;
										TimelineEvent.EndTime = EndTime;
										TimelineEvent.Depth = Depth;
										TimelineEvent.TimerIndex = Event.TimerIndex;
										FilteredEvents[TaskIndex].Add(TimelineEvent);
									}
								}
								return TraceServices::EEventEnumerate::Continue;
							};

							// Note: Enumerating events for filtering should not use downsampling.
							Timeline.EnumerateEventsDownSampledAsync(Params);

							for (TArray<FPendingEventInfo>& Array : FilteredEvents)
							{
								for (FPendingEventInfo& TimelineEvent : Array)
								{
									const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(TimelineEvent.TimerIndex);
									AddTimingEventToBuilder(Builder, TimelineEvent.StartTime, TimelineEvent.EndTime, TimelineEvent.Depth, TimelineEvent.TimerIndex, Timer);
								}
							}
						});
				}
				else
				{
					TimingProfilerProvider.ReadTimeline(TimelineIndex,
						[&Viewport, this, &Builder, TimerReader, FilterEventType](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
						{
							// Note: Enumerating events for filtering should not use downsampling.
							Timeline.EnumerateEventsDownSampled(Viewport.GetStartTime(), Viewport.GetEndTime(), 0,
								[this, &Builder, TimerReader, FilterEventType](double StartTime, double EndTime, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
								{
									const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
									if (ensure(Timer != nullptr))
									{
										if (Timer->Id == FilterEventType)
										{
											AddTimingEventToBuilder(Builder, StartTime, EndTime, Depth, Event.TimerIndex, Timer);
										}
									}
									return TraceServices::EEventEnumerate::Continue;
								});
						});
				}
			}
			else // generic filter
			{
				TimingProfilerProvider.ReadTimeline(TimelineIndex,
					[&Viewport, this, &Builder, TimerReader, &EventFilterPtr](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						// Note: Enumerating events for filtering should not use downsampling.
						//const double SecondsPerPixel = 1.0 / Viewport.GetScaleX();
						//Timeline.EnumerateEventsDownSampled(Viewport.GetStartTime(), Viewport.GetEndTime(), SecondsPerPixel,
						Timeline.EnumerateEvents(Viewport.GetStartTime(), Viewport.GetEndTime(),
							[this, &Builder, TimerReader, &EventFilterPtr](double StartTime, double EndTime, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
							{
								const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
								if (ensure(Timer != nullptr))
								{
									FThreadTrackEvent TimingEvent(SharedThis(this), StartTime, EndTime, Depth);
									TimingEvent.SetTimerId(Timer->Id);
									TimingEvent.SetTimerIndex(Event.TimerIndex);

									if (EventFilterPtr->FilterEvent(TimingEvent))
									{
										AddTimingEventToBuilder(Builder, StartTime, EndTime, Depth, Event.TimerIndex, Timer);
									}
								}
								return TraceServices::EEventEnumerate::Continue;
							});
					});
			}
		}
	}

	if (HasCustomFilter()) // Custom filter (from the filtering widget)
	{
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

			const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

			const TraceServices::ITimingProfilerTimerReader* TimerReader;
			TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

			const FTimingTrackViewport& Viewport = Context.GetViewport();

			TimingProfilerProvider.ReadTimeline(TimelineIndex,
				[&Viewport, this, &Builder, TimerReader](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
				{
					TArray<TArray<FPendingEventInfo>> FilteredEvents;
					TArray<FFilterContext> FilterContexts;

					TraceServices::ITimeline<TraceServices::FTimingProfilerEvent>::EnumerateAsyncParams Params;
					Params.IntervalStart = Viewport.GetStartTime();
					Params.IntervalEnd = Viewport.GetEndTime();

					// Note: Enumerating events for filtering should not use downsampling.
					Params.Resolution = 0.0;
					Params.SetupCallback = [&FilteredEvents, &FilterContexts, this](uint32 NumTasks)
					{
						FilteredEvents.AddDefaulted(NumTasks);
						FilterContexts.AddDefaulted(NumTasks);
						for (FFilterContext& Context : FilterContexts)
						{
							Context.SetReturnValueForUnsetFilters(false);
							Context.AddFilterData<double>(static_cast<int32>(EFilterField::StartTime), 0.0f);
							Context.AddFilterData<double>(static_cast<int32>(EFilterField::EndTime), 0.0f);
							Context.AddFilterData<double>(static_cast<int32>(EFilterField::Duration), 0.0f);
							Context.AddFilterData<FString>(static_cast<int32>(EFilterField::TrackName), this->GetName());
							Context.AddFilterData<int64>(static_cast<int32>(EFilterField::TimerId), 0);
							Context.AddFilterData<int64>(static_cast<int32>(EFilterField::TimerName), 0);
						}
					};
					Params.Callback = [this, &Builder, TimerReader, &FilteredEvents, &FilterContexts](double StartTime, double EndTime, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event, uint32 TaskIndex)
					{
						const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
						if (ensure(Timer != nullptr))
						{
							FFilterContext& Context = FilterContexts[TaskIndex];
							Context.SetFilterData<double>(static_cast<int32>(EFilterField::StartTime), StartTime);
							Context.SetFilterData<double>(static_cast<int32>(EFilterField::EndTime), EndTime);
							Context.SetFilterData<double>(static_cast<int32>(EFilterField::Duration), EndTime - StartTime);
							// The TimerName filter also translates to the numeric Id for performance reasons.
							Context.SetFilterData<int64>(static_cast<int32>(EFilterField::TimerId), Timer->Id);
							Context.SetFilterData<int64>(static_cast<int32>(EFilterField::TimerName), Timer->Id);

							if (FilterConfigurator->ApplyFilters(Context))
							{
								FPendingEventInfo TimelineEvent;
								TimelineEvent.StartTime = StartTime;
								TimelineEvent.EndTime = EndTime;
								TimelineEvent.Depth = Depth;
								TimelineEvent.TimerIndex = Event.TimerIndex;
								FilteredEvents[TaskIndex].Add(TimelineEvent);
							}
						}
						return TraceServices::EEventEnumerate::Continue;
					};

					Timeline.EnumerateEventsDownSampledAsync(Params);

					for (TArray<FPendingEventInfo>& Array : FilteredEvents)
					{
						for (FPendingEventInfo& TimelineEvent : Array)
						{
							const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(TimelineEvent.TimerIndex);
							AddTimingEventToBuilder(Builder, TimelineEvent.StartTime, TimelineEvent.EndTime, TimelineEvent.Depth, TimelineEvent.TimerIndex, Timer);
						}
					}
				});
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::PostDraw(const ITimingTrackDrawContext& Context) const
{
	if (ChildTrack.IsValid())
	{
		ChildTrack->PostDraw(Context);
	}

	const TSharedPtr<const ITimingEvent> SelectedEventPtr = Context.GetSelectedEvent();
	if (SelectedEventPtr.IsValid() &&
		SelectedEventPtr->CheckTrack(this) &&
		SelectedEventPtr->Is<FThreadTrackEvent>())
	{
		const FThreadTrackEvent& SelectedEvent = SelectedEventPtr->As<FThreadTrackEvent>();
		const ITimingViewDrawHelper& Helper = Context.GetHelper();

		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		check(Session.IsValid());

		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(SelectedEvent.GetTimerIndex());
		if (Timer != nullptr)
		{
			FString Str = FString::Printf(TEXT("%s (Incl.: %s, Excl.: %s)"),
				Timer->Name,
				*TimeUtils::FormatTimeAuto(SelectedEvent.GetDuration()),
				*TimeUtils::FormatTimeAuto(SelectedEvent.GetExclusiveTime()));

			DrawSelectedEventInfo(Str, Context.GetViewport(), Context.GetDrawContext(), Helper.GetWhiteBrush(), Helper.GetEventFont());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::InitTooltip(FTooltipDrawState& InOutTooltip, const ITimingEvent& InTooltipEvent) const
{
	InOutTooltip.ResetContent();

	if (InTooltipEvent.CheckTrack(this) && InTooltipEvent.Is<FThreadTrackEvent>())
	{
		const FThreadTrackEvent& TooltipEvent = InTooltipEvent.As<FThreadTrackEvent>();

		TSharedPtr<FThreadTrackEvent> ParentTimingEvent;
		TSharedPtr<FThreadTrackEvent> RootTimingEvent;
		GetParentAndRoot(TooltipEvent, ParentTimingEvent, RootTimingEvent);

		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		check(Session.IsValid());

		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(TooltipEvent.GetTimerIndex());
		FString TimerName = (Timer != nullptr) ? Timer->Name : TEXT("N/A");
		InOutTooltip.AddTitle(TimerName);

		if (ParentTimingEvent.IsValid() && TooltipEvent.GetDepth() > 0)
		{
			Timer = TimerReader->GetTimer(ParentTimingEvent->GetTimerIndex());
			const TCHAR* ParentTimerName = (Timer != nullptr) ? Timer->Name : TEXT("N/A");
			FNumberFormattingOptions FormattingOptions;
			FormattingOptions.MaximumFractionalDigits = 2;
			const FString ValueStr = FString::Printf(TEXT("%s %s"), *FText::AsPercent(TooltipEvent.GetDuration() / ParentTimingEvent->GetDuration(), &FormattingOptions).ToString(), ParentTimerName);
			InOutTooltip.AddNameValueTextLine(TEXT("% of Parent:"), ValueStr);
		}

		if (RootTimingEvent.IsValid() && TooltipEvent.GetDepth() > 1)
		{
			Timer = TimerReader->GetTimer(RootTimingEvent->GetTimerIndex());
			const TCHAR* RootTimerName = (Timer != nullptr) ? Timer->Name : TEXT("N/A");
			FNumberFormattingOptions FormattingOptions;
			FormattingOptions.MaximumFractionalDigits = 2;
			const FString ValueStr = FString::Printf(TEXT("%s %s"), *FText::AsPercent(TooltipEvent.GetDuration() / RootTimingEvent->GetDuration(), &FormattingOptions).ToString(), RootTimerName);
			InOutTooltip.AddNameValueTextLine(TEXT("% of Root:"), ValueStr);
		}

		InOutTooltip.AddNameValueTextLine(TEXT("Inclusive Time:"), TimeUtils::FormatTimeAuto(TooltipEvent.GetDuration()));

		if (TooltipEvent.GetDuration() > 0.0)
		{
			const double ExclusiveTimePercent = TooltipEvent.GetExclusiveTime() / TooltipEvent.GetDuration();
			FNumberFormattingOptions FormattingOptions;
			FormattingOptions.MaximumFractionalDigits = 2;
			const FString ExclStr = FString::Printf(TEXT("%s (%s)"), *TimeUtils::FormatTimeAuto(TooltipEvent.GetExclusiveTime()), *FText::AsPercent(ExclusiveTimePercent, &FormattingOptions).ToString());
			InOutTooltip.AddNameValueTextLine(TEXT("Exclusive Time:"), ExclStr);
		}

		InOutTooltip.AddNameValueTextLine(TEXT("Depth:"), FString::Printf(TEXT("%d"), TooltipEvent.GetDepth()));

		TArrayView<const uint8> Metadata = TimerReader->GetMetadata(TooltipEvent.GetTimerIndex());
		if (Metadata.Num() > 0)
		{
			AppendMetadataToTooltip(InOutTooltip, Metadata);
		}

		// tasks

		const TraceServices::ITasksProvider* TasksProvider = TraceServices::ReadTasksProvider(*Session.Get());
		if (TasksProvider != nullptr)
		{
			auto AddTaskInfo = [&InOutTooltip, this](const TraceServices::FTaskInfo& Task)
			{
				InOutTooltip.AddTextLine(FString::Printf(TEXT("-------- Task %d%s --------"), Task.Id, Task.bTracked ? TEXT("") : TEXT(" (not tracked)")), FLinearColor::Green);

				if (Task.DebugName != nullptr)
				{
					InOutTooltip.AddTextLine(FString::Printf(TEXT("%s"), Task.DebugName), FLinearColor::Green);
				}

				ENamedThreads::Type ThreadInfo = (ENamedThreads::Type)Task.ThreadToExecuteOn;
				const TCHAR* NamedThreadsStr[] = { TEXT("Stats"), TEXT("RHI"), TEXT("Audio"), TEXT("Game"), TEXT("Rendering") };
				ENamedThreads::Type ThreadIndex = ENamedThreads::GetThreadIndex(ThreadInfo);

				if (ThreadIndex == ENamedThreads::AnyThread)
				{
					const TCHAR* TaskPri = ENamedThreads::GetTaskPriority(ThreadInfo) == ENamedThreads::NormalTaskPriority ? TEXT("Normal") : TEXT("High");

					int32 ThreadPriIndex = ENamedThreads::GetThreadPriorityIndex(ThreadInfo);
					const TCHAR* ThreadPriStrs[] = { TEXT("Normal"), TEXT("High"), TEXT("Low") };
					const TCHAR* ThreadPri = ThreadPriStrs[ThreadPriIndex];

					InOutTooltip.AddTextLine(FString::Printf(TEXT("%s Pri task on %s Pri worker"), TaskPri, ThreadPri), FLinearColor::Green);
				}
				else
				{
					const TCHAR* QueueStr = ENamedThreads::GetQueueIndex(ThreadInfo) == ENamedThreads::MainQueue ? TEXT("Main") : TEXT("Local");
					InOutTooltip.AddTextLine(FString::Printf(TEXT("%s (%s queue)"), NamedThreadsStr[ThreadIndex], QueueStr), FLinearColor::Green);
				}

				{
					TSharedPtr<FCpuTimingTrack> Track = SharedState.GetCpuTrack(Task.CreatedThreadId);
					FString TrackName = Track.IsValid() ? Track->GetName() : TEXT("Unknown");
					InOutTooltip.AddNameValueTextLine(TEXT("Created:"), FString::Printf(TEXT("%f on %s"), Task.CreatedTimestamp, *TrackName));
				}

				{
					TSharedPtr<FCpuTimingTrack> Track = SharedState.GetCpuTrack(Task.LaunchedThreadId);
					FString TrackName = Track.IsValid() ? Track->GetName() : TEXT("Unknown");
					InOutTooltip.AddNameValueTextLine(TEXT("Launched:"), FString::Printf(TEXT("%f (+%s) on %s"), Task.LaunchedTimestamp, *TimeUtils::FormatTimeAuto(Task.LaunchedTimestamp - Task.CreatedTimestamp), *TrackName));
				}

				{
					TSharedPtr<FCpuTimingTrack> Track = SharedState.GetCpuTrack(Task.ScheduledThreadId);
					FString TrackName = Track.IsValid() ? Track->GetName() : TEXT("Unknown");
					InOutTooltip.AddNameValueTextLine(TEXT("Scheduled:"), FString::Printf(TEXT("%f (+%s) on %s"), Task.ScheduledTimestamp, *TimeUtils::FormatTimeAuto(Task.ScheduledTimestamp - Task.LaunchedTimestamp), *TrackName));
				}

				InOutTooltip.AddNameValueTextLine(TEXT("Started:"), FString::Printf(TEXT("%f (+%s)"), Task.StartedTimestamp, *TimeUtils::FormatTimeAuto(Task.StartedTimestamp - Task.ScheduledTimestamp)));
				if (Task.FinishedTimestamp != TraceServices::FTaskInfo::InvalidTimestamp)
				{
					InOutTooltip.AddNameValueTextLine(TEXT("Finished:"), FString::Printf(TEXT("%f (+%s)"), Task.FinishedTimestamp, *TimeUtils::FormatTimeAuto(Task.FinishedTimestamp - Task.StartedTimestamp)));

					if (Task.CompletedTimestamp != TraceServices::FTaskInfo::InvalidTimestamp)
					{
						TSharedPtr<FCpuTimingTrack> CompletedTrack = SharedState.GetCpuTrack(Task.CompletedThreadId);
						FString CompletedTrackName = CompletedTrack.IsValid() ? CompletedTrack->GetName() : TEXT("Unknown");
						InOutTooltip.AddNameValueTextLine(TEXT("Completed:"), FString::Printf(TEXT("%f (+%s) on %s"), Task.CompletedTimestamp, *TimeUtils::FormatTimeAuto(Task.CompletedTimestamp - Task.FinishedTimestamp), *CompletedTrackName));

						if (Task.DestroyedTimestamp != TraceServices::FTaskInfo::InvalidTimestamp)
						{
							TSharedPtr<FCpuTimingTrack> DestroyedTrack = SharedState.GetCpuTrack(Task.DestroyedThreadId);
							FString DestroyedTrackName = DestroyedTrack.IsValid() ? DestroyedTrack->GetName() : TEXT("Unknown");
							InOutTooltip.AddNameValueTextLine(TEXT("Destroyed:"), FString::Printf(TEXT("%f (+%s) on %s"), Task.DestroyedTimestamp, *TimeUtils::FormatTimeAuto(Task.DestroyedTimestamp - Task.CompletedTimestamp), *DestroyedTrackName));
						}
					}
				}
				InOutTooltip.AddNameValueTextLine(TEXT("Prerequisite tasks:"), FString::Printf(TEXT("%d"), Task.Prerequisites.Num()));
				InOutTooltip.AddNameValueTextLine(TEXT("Subsequent tasks:"), FString::Printf(TEXT("%d"), Task.Subsequents.Num()));
				InOutTooltip.AddNameValueTextLine(TEXT("Parent tasks:"), FString::Printf(TEXT("%d"), Task.ParentTasks.Num()));
				InOutTooltip.AddNameValueTextLine(TEXT("Nested tasks:"), FString::Printf(TEXT("%d"), Task.NestedTasks.Num()));
			};

			do // info about a task
			{
				const TraceServices::FTaskInfo* Task = TasksProvider->TryGetTask(ThreadId, TooltipEvent.GetStartTime());
				if (Task == nullptr)
				{
					break;
				}

				if (Task->FinishedTimestamp < TooltipEvent.GetEndTime())
				{
					break;
				}

				AddTaskInfo(*Task);
			} while (false);

			do // info about blocking
			{
				const TraceServices::FWaitingForTasks* Waiting = TasksProvider->TryGetWaiting(*TimerName, ThreadId, TooltipEvent.GetStartTime());
				if (Waiting == nullptr || Waiting->Tasks.Num() == 0)
				{
					break;
				}

				InOutTooltip.AddTextLine(TEXT("-------- Waiting for tasks --------"), FLinearColor::Red);
				constexpr int32 NumIdsOnRow = 4;
				TStringBuilder<1024> StringBuilder;

				// Add the first line of Task Id values.
				for (int32 Index = 0; Index < Waiting->Tasks.Num() && Index < NumIdsOnRow; ++Index)
				{
					StringBuilder.Appendf(TEXT("%d, "), Waiting->Tasks[Index]);
				}

				// Remove separators from last entry.
				if (Waiting->Tasks.Num() <= NumIdsOnRow)
				{
					StringBuilder.RemoveSuffix(2);
				}
				InOutTooltip.AddNameValueTextLine(FString::Printf(TEXT("Tasks[%d]:"), Waiting->Tasks.Num()), StringBuilder.ToString());
				StringBuilder.Reset();

				// Add the rest of the lines with an empty name so they appear as a multi line value.
				for (int32 Index = NumIdsOnRow; Index < Waiting->Tasks.Num(); ++Index)
				{
					StringBuilder.Appendf(TEXT("%d, "), Waiting->Tasks[Index]);

					if ((Index + 1) % NumIdsOnRow == 0)
					{
						InOutTooltip.AddNameValueTextLine(TEXT(""), StringBuilder.ToString());
						StringBuilder.Reset();
					}
				}

				if (StringBuilder.Len() > 1)
				{
					StringBuilder.RemoveSuffix(2);
					InOutTooltip.AddNameValueTextLine(TEXT(""), StringBuilder.ToString());
				}

				InOutTooltip.AddNameValueTextLine(TEXT("Started waiting:"), FString::Printf(TEXT("%f"), Waiting->StartedTimestamp));
				InOutTooltip.AddNameValueTextLine(TEXT("Finished waiting:"),
					FString::Printf(TEXT("%s (+%s)"),
						Waiting->FinishedTimestamp == TraceServices::FTaskInfo::InvalidTimestamp ? TEXT("[not set]") : *FString::SanitizeFloat(Waiting->FinishedTimestamp),
						*TimeUtils::FormatTimeAuto(Waiting->FinishedTimestamp - Waiting->StartedTimestamp)));

				const int32 MaxWaitedTasksToList = 5;
				int32 NumTasksToList = FMath::Min(Waiting->Tasks.Num(), MaxWaitedTasksToList);
				for (int32 i = 0; i != NumTasksToList; ++i)
				{
					const TraceServices::FTaskInfo* Task = TasksProvider->TryGetTask(Waiting->Tasks[i]);
					if (Task != nullptr)
					{
						AddTaskInfo(*Task);
					}
				}
				if (NumTasksToList < Waiting->Tasks.Num())
				{
					InOutTooltip.AddTextLine(TEXT("[...]"), FLinearColor::Green);
				}

			} while (false);
		}
	}
	else if (ChildTrack.IsValid())
	{
		ChildTrack->InitTooltip(InOutTooltip, InTooltipEvent);
	}

	InOutTooltip.UpdateLayout();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::GetParentAndRoot(const FThreadTrackEvent& TimingEvent, TSharedPtr<FThreadTrackEvent>& OutParentTimingEvent, TSharedPtr<FThreadTrackEvent>& OutRootTimingEvent) const
{
	if (TimingEvent.GetDepth() > 0)
	{
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid())
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

			if (TraceServices::ReadTimingProfilerProvider(*Session.Get()))
			{
				const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

				TimingProfilerProvider.ReadTimeline(GetTimelineIndex(), [&TimingEvent, &OutParentTimingEvent, &OutRootTimingEvent](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						const double Time = (TimingEvent.GetStartTime() + TimingEvent.GetEndTime()) / 2;
						TimelineEventInfo EventInfo;
						bool IsFound = Timeline.GetEventInfo(Time, 0, TimingEvent.GetDepth() - 1, EventInfo);
						if (IsFound)
						{
							CreateFThreadTrackEventFromInfo(EventInfo, TimingEvent.GetTrack(), TimingEvent.GetDepth() - 1, OutParentTimingEvent);
						}

						IsFound = Timeline.GetEventInfo(Time, 0, 0, EventInfo);
						if (IsFound)
						{
							CreateFThreadTrackEventFromInfo(EventInfo, TimingEvent.GetTrack(), 0, OutRootTimingEvent);
						}
					});
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TSharedPtr<const ITimingEvent> FThreadTimingTrack::GetEvent(float InPosX, float InPosY, const FTimingTrackViewport& Viewport) const
{
	TSharedPtr<FThreadTrackEvent> TimingEvent;

	const FTimingViewLayout& Layout = Viewport.GetLayout();

	float TopLaneY = 0.0f;
	float TrackLanesHeight = 0.0f;
	if (ChildTrack.IsValid() && ChildTrack->GetHeight() > 0.0f)
	{
		const float HeaderDY = InPosY - ChildTrack->GetPosY();
		if (HeaderDY >= 0.0f && HeaderDY < ChildTrack->GetHeight())
		{
			return ChildTrack->GetEvent(InPosX, InPosY, Viewport);
		}
		TopLaneY = GetPosY() + 1.0f + Layout.TimelineDY + ChildTrack->GetHeight() + Layout.ChildTimelineDY;
		TrackLanesHeight = GetHeight() - ChildTrack->GetHeight() - 1.0f - 2.0f * Layout.TimelineDY - Layout.ChildTimelineDY;
	}
	else
	{
		if (IsChildTrack())
		{
			TopLaneY = GetPosY();
			TrackLanesHeight = GetHeight();
		}
		else
		{
			TopLaneY = GetPosY() + 1.0f + Layout.TimelineDY;
			TrackLanesHeight = GetHeight() - 1.0f - 2.0f * Layout.TimelineDY;
		}
	}

	const float DY = InPosY - TopLaneY;

	// If mouse is not above first sub-track or below last sub-track...
	if (DY >= 0 && DY < TrackLanesHeight)
	{
		const int32 Depth = static_cast<int32>(DY / (Layout.EventH + Layout.EventDY));

		const double SecondsPerPixel = 1.0 / Viewport.GetScaleX();

		const double EventTime = Viewport.SlateUnitsToTime(InPosX);

		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid())
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

			if (TraceServices::ReadTimingProfilerProvider(*Session.Get()))
			{
				const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

				TimingProfilerProvider.ReadTimeline(GetTimelineIndex(), [this, &EventTime, &Depth, &TimingEvent, &SecondsPerPixel](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						TimelineEventInfo EventInfo;
						bool IsFound = Timeline.GetEventInfo(EventTime, 2 * SecondsPerPixel, Depth, EventInfo);
						if (IsFound)
						{
							CreateFThreadTrackEventFromInfo(EventInfo, SharedThis(this), Depth, TimingEvent);
						}
					});
			}
		}
	}

	return TimingEvent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TSharedPtr<const ITimingEvent> FThreadTimingTrack::SearchEvent(const FTimingEventSearchParameters& InSearchParameters) const
{
	TSharedPtr<FThreadTrackEvent> FoundEvent;
	FindTimingProfilerEvent(InSearchParameters, [this, &FoundEvent](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const TraceServices::FTimingProfilerEvent& InFoundEvent)
	{
		FoundEvent = MakeShared<FThreadTrackEvent>(SharedThis(this), InFoundStartTime, InFoundEndTime, InFoundDepth);
		FoundEvent->SetTimerIndex(InFoundEvent.TimerIndex);

		uint32 TimerId = 0;
		bool ret = FThreadTimingTrack::TimerIndexToTimerId(InFoundEvent.TimerIndex, TimerId);
		if (ret)
		{
			FoundEvent->SetTimerId(TimerId);
		}
	});

	return FoundEvent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::UpdateEventStats(ITimingEvent& InOutEvent) const
{
	if (InOutEvent.CheckTrack(this) && InOutEvent.Is<FThreadTrackEvent>())
	{
		FThreadTrackEvent& TrackEvent = InOutEvent.As<FThreadTrackEvent>();
		if (TrackEvent.IsExclusiveTimeComputed())
		{
			return;
		}

		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid())
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

			if (TraceServices::ReadTimingProfilerProvider(*Session.Get()))
			{
				const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

				// Get Exclusive Time.
				TimingProfilerProvider.ReadTimeline(GetTimelineIndex(), [&TrackEvent](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
				{
					TimelineEventInfo EventInfo;
					bool bIsFound = Timeline.GetEventInfo(TrackEvent.GetStartTime(), 0.0, TrackEvent.GetDepth(), EventInfo);
					if (bIsFound)
					{
						TrackEvent.SetExclusiveTime(EventInfo.ExclTime);
						TrackEvent.SetIsExclusiveTimeComputed(true);
					}
				});
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::OnEventSelected(const ITimingEvent& InSelectedEvent) const
{
	if (InSelectedEvent.CheckTrack(this) && InSelectedEvent.Is<FThreadTrackEvent>())
	{
		const FThreadTrackEvent& TrackEvent = InSelectedEvent.As<FThreadTrackEvent>();

		// Select the timer node corresponding to timing event type of selected timing event.
		FTimingProfilerManager::Get()->SetSelectedTimer(TrackEvent.GetTimerId());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::OnClipboardCopyEvent(const ITimingEvent& InSelectedEvent) const
{
	if (InSelectedEvent.CheckTrack(this) && InSelectedEvent.Is<FThreadTrackEvent>())
	{
		const FThreadTrackEvent& TrackEvent = InSelectedEvent.As<FThreadTrackEvent>();

		FTimerNodePtr TimerNodePtr = FTimingProfilerManager::Get()->GetTimerNode(TrackEvent.GetTimerId());
		if (TimerNodePtr)
		{
			FString EventName = TimerNodePtr->GetName().ToString();

			FTimingEventsTrackDrawStateBuilder::AppendDurationToEventName(EventName, TrackEvent.GetDuration());

			const uint32 TimerIndex = TrackEvent.GetTimerIndex();
			if (int32(TimerIndex) < 0) // has metadata?
			{
				TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
				check(Session.IsValid());

				TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
				const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());
				const TraceServices::ITimingProfilerTimerReader* TimerReader;
				TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

				TArrayView<const uint8> Metadata = TimerReader->GetMetadata(TimerIndex);
				if (Metadata.Num() > 0)
				{
					AppendMetadataToString(EventName, Metadata);
				}
			}

			// Copy name of selected timing event to clipboard.
			FPlatformApplicationMisc::ClipboardCopy(*EventName);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	if (GetGroupName() != nullptr)
	{
		MenuBuilder.BeginSection("CpuThread", LOCTEXT("ContextMenu_Section_CpuThread", "CPU Thread"));
		{
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("CpuThreadGroupFmt", "Group: {0}"), FText::FromString(GetGroupName())),
				FText(),
				FSlateIcon(),
				FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			const FString ThreadIdStr = FString::Printf(TEXT("%s%u (0x%X)"), ThreadId & 0x70000000 ? TEXT("*") : TEXT(""), ThreadId & ~0x70000000, ThreadId);
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("CpuThreadIdFmt", "Thread Id: {0}"), FText::FromString(ThreadIdStr)),
				FText(),
				FSlateIcon(),
				FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();
	}

	if (ChildTrack.IsValid())
	{
		ChildTrack->BuildContextMenu(MenuBuilder);
	}

	MenuBuilder.BeginSection("TimingEvents", LOCTEXT("ContextMenu_Section_TimingEvents", "Timing Events"));
	{
		FExecuteAction FilterTrackAction;
		FilterTrackAction.BindSP(this, &FThreadTimingTrack::OnFilterTrackClicked);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("FilterTrack", "Filter Track..."),
			FText(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Filter"),
			FUIAction(FilterTrackAction, FCanExecuteAction::CreateLambda([]() { return true; })),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FThreadTimingTrack::FindTimingProfilerEvent(const FThreadTrackEvent& InTimingEvent, TFunctionRef<void(double, double, uint32, const TraceServices::FTimingProfilerEvent&)> InFoundPredicate) const
{
	auto MatchEvent = [&InTimingEvent](double InStartTime, double InEndTime, uint32 InDepth)
	{
		return InDepth == InTimingEvent.GetDepth()
			&& InStartTime == InTimingEvent.GetStartTime()
			&& InEndTime == InTimingEvent.GetEndTime();
	};

	const double Time = (InTimingEvent.GetStartTime() + InTimingEvent.GetEndTime()) / 2;
	FTimingEventSearchParameters SearchParameters(Time, Time, ETimingEventSearchFlags::StopAtFirstMatch, MatchEvent);
	SearchParameters.SearchHandle = &InTimingEvent.GetSearchHandle();
	return FindTimingProfilerEvent(SearchParameters, InFoundPredicate);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FThreadTimingTrack::FindTimingProfilerEvent(const FTimingEventSearchParameters& InParameters, TFunctionRef<void(double, double, uint32, const TraceServices::FTimingProfilerEvent&)> InFoundPredicate) const
{
	FFilterContext FilterConfiguratorContext;
	FilterConfiguratorContext.SetReturnValueForUnsetFilters(false);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::StartTime), 0.0f);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::EndTime), 0.0f);
	FilterConfiguratorContext.AddFilterData<double>(static_cast<int32>(EFilterField::Duration), 0.0f);
	FilterConfiguratorContext.AddFilterData<FString>(static_cast<int32>(EFilterField::TrackName), this->GetName());
	FilterConfiguratorContext.AddFilterData<int64>(static_cast<int32>(EFilterField::TimerId), 0);
	FilterConfiguratorContext.AddFilterData<int64>(static_cast<int32>(EFilterField::TimerName), 0);

	return TTimingEventSearch<TraceServices::FTimingProfilerEvent>::Search(
		InParameters,

		[this](TTimingEventSearch<TraceServices::FTimingProfilerEvent>::FContext& InContext)
		{
			TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
			if (Session.IsValid())
			{
				TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

				if (TraceServices::ReadTimingProfilerProvider(*Session.Get()))
				{
					const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

					TimingProfilerProvider.ReadTimeline(GetTimelineIndex(), [&InContext](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						auto Callback = [&InContext](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event)
						{
							InContext.Check(EventStartTime, EventEndTime, EventDepth, Event);
							return InContext.ShouldContinueSearching() ? TraceServices::EEventEnumerate::Continue : TraceServices::EEventEnumerate::Stop;
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

		[&FilterConfiguratorContext, &InParameters](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event)
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
					const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());
					const TraceServices::ITimingProfilerTimerReader* TimerReader;
					TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

					const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(Event.TimerIndex);
					if (ensure(Timer != nullptr))
					{
						FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::StartTime), EventStartTime);
						FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::EndTime), EventEndTime);
						FilterConfiguratorContext.SetFilterData<double>(static_cast<int32>(EFilterField::Duration), EventEndTime - EventStartTime);
						FilterConfiguratorContext.SetFilterData<int64>(static_cast<int32>(EFilterField::TimerId), Timer->Id);
						// The TimerName filter also translates to the numeric Id for performance reasons.
						FilterConfiguratorContext.SetFilterData<int64>(static_cast<int32>(EFilterField::TimerName), Timer->Id);
					}
					return InParameters.FilterExecutor->ApplyFilters(FilterConfiguratorContext);
				}
			}

			return false;
		},

		[&InFoundPredicate](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const TraceServices::FTimingProfilerEvent& InEvent)
		{
			InFoundPredicate(InFoundStartTime, InFoundEndTime, InFoundDepth, InEvent);
		},

		TTimingEventSearch<TraceServices::FTimingProfilerEvent>::NoMatch,

		&SearchCache);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::CreateFThreadTrackEventFromInfo(const TimelineEventInfo& InEventInfo, const TSharedRef<const FBaseTimingTrack> InTrack, int32 InDepth, TSharedPtr<FThreadTrackEvent> &OutTimingEvent)
{
	OutTimingEvent = MakeShared<FThreadTrackEvent>(InTrack, InEventInfo.StartTime, InEventInfo.EndTime, InDepth);
	FThreadTrackEvent& Event = OutTimingEvent->As<FThreadTrackEvent>();
	Event.SetExclusiveTime(InEventInfo.ExclTime);
	Event.SetIsExclusiveTimeComputed(true);
	Event.SetTimerIndex(InEventInfo.Event.TimerIndex);

	uint32 TimerId = 0;
	bool ret = FThreadTimingTrack::TimerIndexToTimerId(InEventInfo.Event.TimerIndex, TimerId);
	if (ret)
	{
		Event.SetTimerId(TimerId);
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FThreadTimingTrack::TimerIndexToTimerId(uint32 InTimerIndex, uint32& OutTimerId)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	check(Session.IsValid())

	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

	const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

	const TraceServices::ITimingProfilerTimerReader* TimerReader;
	TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

	const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(InTimerIndex);
	if (Timer == nullptr)
	{
		return false;
	}

	OutTimerId = Timer->Id;
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::OnFilterTrackClicked()
{
	LLM_SCOPE_BYTAG(Insights);

	if (!FilterConfigurator.IsValid())
	{
		FilterConfigurator = MakeShared<FFilterConfigurator>();
		TSharedPtr<TArray<TSharedPtr<struct FFilter>>>& AvailableFilters = FilterConfigurator->GetAvailableFilters();

		AvailableFilters->Add(MakeShared<FFilter>(static_cast<int32>(EFilterField::StartTime), LOCTEXT("StartTime", "Start Time"), LOCTEXT("StartTime", "Start Time"), EFilterDataType::Double, FFilterService::Get()->GetDoubleOperators()));
		AvailableFilters->Add(MakeShared<FFilter>(static_cast<int32>(EFilterField::EndTime), LOCTEXT("EndTime", "End Time"), LOCTEXT("EndTime", "End Time"), EFilterDataType::Double, FFilterService::Get()->GetDoubleOperators()));
		AvailableFilters->Add(MakeShared<FFilter>(static_cast<int32>(EFilterField::Duration), LOCTEXT("Duration", "Duration"), LOCTEXT("Duration", "Duration"), EFilterDataType::Double, FFilterService::Get()->GetDoubleOperators()));
		AvailableFilters->Add(MakeShared<FFilter>(static_cast<int32>(EFilterField::TimerId), LOCTEXT("TimerId", "Timer Id"), LOCTEXT("TimerId", "Timer Id"), EFilterDataType::Int64, FFilterService::Get()->GetIntegerOperators()));
	}
	else
	{
		FilterConfigurator->GetOnChangesCommitedEvent().Remove(OnFilterChangesCommitedHandle);

		// Make a copy, so it will not affect other tracks that shares same filter.
		FilterConfigurator = MakeShared<FFilterConfigurator>(*FilterConfigurator);
	}

	OnFilterChangesCommitedHandle = FilterConfigurator->GetOnChangesCommitedEvent().AddLambda([this]()
		{
			this->SetDirtyFlag();
		});

	FFilterService::Get()->CreateFilterConfiguratorWidget(FilterConfigurator);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FThreadTimingTrack::HasCustomFilter() const
{
	if (!FilterConfigurator.IsValid())
	{
		return false;
	}
	if (FilterConfigurator->GetRootNode().IsValid() && FilterConfigurator->GetRootNode()->GetChildren().Num() > 0)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FThreadTimingTrack::GetDepthAt(double Time) const
{
	int32 Depth = 0;
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		TimingProfilerProvider.ReadTimeline(TimelineIndex,
			[Time, &Depth](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
			{
				Depth = Timeline.GetDepthAt(Time);
			});
	}
	return Depth;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FThreadTimingTrack::SetFilterConfigurator(TSharedPtr<Insights::FFilterConfigurator> InFilterConfigurator)
{
	if (FilterConfigurator != InFilterConfigurator)
	{
		FilterConfigurator = InFilterConfigurator;
		SetDirtyFlag();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
