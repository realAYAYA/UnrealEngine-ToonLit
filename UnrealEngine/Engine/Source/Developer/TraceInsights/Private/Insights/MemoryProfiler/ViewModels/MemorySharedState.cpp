// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemorySharedState.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Logging/MessageLog.h"
#include "TraceServices/Model/Memory.h"

// Insights
#include "Insights/Common/PaintUtils.h"
#include "Insights/Common/Stopwatch.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"
#include "Insights/MemoryProfiler/ViewModels/MemoryTag.h"
#include "Insights/MemoryProfiler/ViewModels/MemoryTracker.h"
#include "Insights/MemoryProfiler/ViewModels/Report.h"
#include "Insights/MemoryProfiler/ViewModels/ReportXmlParser.h"
#include "Insights/ViewModels/TimingEvent.h"
#include "Insights/ViewModels/TimingTrackViewport.h"
#include "Insights/ViewModels/TimingGraphTrack.h"
#include "Insights/ViewModels/TooltipDrawState.h"
#include "Insights/ViewModels/TimingEventSearch.h"
#include "Insights/Widgets/STimingView.h"

#include <limits>

static_assert(Insights::FMemoryTracker::InvalidTrackerId == TraceServices::FMemoryTrackerInfo::InvalidTrackerId, "InvalidTrackerId");
static_assert(Insights::FMemoryTag::InvalidTagId == TraceServices::FMemoryTagInfo::InvalidTagId, "InvalidTagId");

#define LOCTEXT_NAMESPACE "MemorySharedState"

const FName Insights::FQueryTargetWindowSpec::NewWindow = TEXT("New Window");

////////////////////////////////////////////////////////////////////////////////////////////////////
// FMemoryTimingViewCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

FMemoryTimingViewCommands::FMemoryTimingViewCommands()
	: TCommands<FMemoryTimingViewCommands>(
		TEXT("MemoryTimingViewCommands"),
		NSLOCTEXT("Contexts", "MemoryTimingViewCommands", "Insights - Timing View - Memory"),
		NAME_None,
		FInsightsStyle::GetStyleSetName())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FMemoryTimingViewCommands::~FMemoryTimingViewCommands()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// UI_COMMAND takes long for the compiler to optimize
UE_DISABLE_OPTIMIZATION_SHIP
void FMemoryTimingViewCommands::RegisterCommands()
{
	UI_COMMAND(ShowHideAllMemoryTracks,
		"Memory Tracks",
		"Shows/hides the Memory tracks.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EModifierKey::Control, EKeys::M));
}
UE_ENABLE_OPTIMIZATION_SHIP

////////////////////////////////////////////////////////////////////////////////////////////////////
// FMemorySharedState
////////////////////////////////////////////////////////////////////////////////////////////////////

FMemorySharedState::FMemorySharedState()
	: TimingView(nullptr)
	, DefaultTracker(nullptr)
	, PlatformTracker(nullptr)
	, MainGraphTrack(nullptr)
	, LiveAllocsGraphTrack(nullptr)
	, AllocFreeGraphTrack(nullptr)
	, TrackHeightMode(EMemoryTrackHeightMode::Medium)
	, bShowHideAllMemoryTracks(false)
	, CreatedDefaultTracks()
	, CurrentMemoryRule(nullptr)
{
	InitMemoryRules();

	CurrentQueryTarget = MakeShared<Insights::FQueryTargetWindowSpec>(Insights::FQueryTargetWindowSpec::NewWindow, LOCTEXT("NewWindow", "New Window"));
	QueryTargetSpecs.Add(CurrentQueryTarget);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FMemorySharedState::~FMemorySharedState()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::OnBeginSession(Insights::ITimingViewSession& InSession)
{
	if (&InSession != TimingView.Get())
	{
		return;
	}

	TagList.Reset();

	Trackers.Reset();
	DefaultTracker = nullptr;
	PlatformTracker = nullptr;

	MainGraphTrack = nullptr;
	LiveAllocsGraphTrack = nullptr;
	AllocFreeGraphTrack = nullptr;
	AllTracks.Reset();

	bShowHideAllMemoryTracks = true;

	CreatedDefaultTracks.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::OnEndSession(Insights::ITimingViewSession& InSession)
{
	if (&InSession != TimingView.Get())
	{
		return;
	}

	TagList.Reset();

	Trackers.Reset();
	DefaultTracker = nullptr;
	PlatformTracker = nullptr;

	MainGraphTrack = nullptr;
	LiveAllocsGraphTrack = nullptr;
	AllocFreeGraphTrack = nullptr;
	AllTracks.Reset();

	bShowHideAllMemoryTracks = false;

	CreatedDefaultTracks.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession)
{
	if (&InSession != TimingView.Get())
	{
		return;
	}

	if (!MainGraphTrack.IsValid())
	{
		MainGraphTrack = CreateMemoryGraphTrack();
		check(MainGraphTrack);

		MainGraphTrack->SetOrder(FTimingTrackOrder::First);
		MainGraphTrack->SetName(TEXT("Main Memory Graph"));

		MainGraphTrack->AddTimelineSeries(FMemoryGraphSeries::ETimelineType::MaxTotalMem);
		MainGraphTrack->AddTimelineSeries(FMemoryGraphSeries::ETimelineType::MinTotalMem);

		MainGraphTrack->SetVisibilityFlag(bShowHideAllMemoryTracks);

		MainGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Small, 100.0f);
		MainGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Medium, 200.0f);
		MainGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Large, 400.0f);
		MainGraphTrack->SetCurrentTrackHeight(TrackHeightMode);

		TimingView->InvalidateScrollableTracksOrder();
	}

	if (!LiveAllocsGraphTrack.IsValid())
	{
		LiveAllocsGraphTrack = CreateMemoryGraphTrack();
		check(LiveAllocsGraphTrack);

		LiveAllocsGraphTrack->SetOrder(FTimingTrackOrder::First + 1);
		LiveAllocsGraphTrack->SetName(TEXT("Live Allocation Count"));
		LiveAllocsGraphTrack->SetLabelUnit(EGraphTrackLabelUnit::Count, 0);

		LiveAllocsGraphTrack->AddTimelineSeries(FMemoryGraphSeries::ETimelineType::MaxLiveAllocs);
		LiveAllocsGraphTrack->AddTimelineSeries(FMemoryGraphSeries::ETimelineType::MinLiveAllocs);

		LiveAllocsGraphTrack->SetVisibilityFlag(bShowHideAllMemoryTracks);

		LiveAllocsGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Small, 50.0f);
		LiveAllocsGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Medium, 100.0f);
		LiveAllocsGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Large, 200.0f);
		LiveAllocsGraphTrack->SetCurrentTrackHeight(TrackHeightMode);

		TimingView->InvalidateScrollableTracksOrder();
	}

	if (!AllocFreeGraphTrack.IsValid())
	{
		AllocFreeGraphTrack = CreateMemoryGraphTrack();
		check(AllocFreeGraphTrack);

		AllocFreeGraphTrack->SetOrder(FTimingTrackOrder::First + 2);
		AllocFreeGraphTrack->SetName(TEXT("Alloc/Free Event Count"));
		AllocFreeGraphTrack->SetLabelUnit(EGraphTrackLabelUnit::Count, 0);

		AllocFreeGraphTrack->AddTimelineSeries(FMemoryGraphSeries::ETimelineType::AllocEvents);
		AllocFreeGraphTrack->AddTimelineSeries(FMemoryGraphSeries::ETimelineType::FreeEvents);

		AllocFreeGraphTrack->SetVisibilityFlag(bShowHideAllMemoryTracks);

		AllocFreeGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Small, 50.0f);
		AllocFreeGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Medium, 100.0f);
		AllocFreeGraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Large, 200.0f);
		AllocFreeGraphTrack->SetCurrentTrackHeight(TrackHeightMode);

		TimingView->InvalidateScrollableTracksOrder();
	}

	const int32 PrevTagCount = TagList.GetTags().Num();

	TagList.Update();

	if (!DefaultTracker)
	{
		SyncTrackers();
	}

	// Scan for mem tags to show as default, but only when new mem tags are added.
	const int32 NewTagCount = TagList.GetTags().Num();
	if (NewTagCount > PrevTagCount)
	{
		CreateDefaultTracks();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::CreateDefaultTracks()
{
	if (!DefaultTracker)
	{
		return;
	}

	Insights::FMemoryTrackerId DefaultTrackerId = DefaultTracker->GetId();

	static const TCHAR* DefaultTags[] =
	{
		TEXT("Total"),
		TEXT("TrackedTotal"),
		TEXT("Untracked"),
		TEXT("Meshes"),
		TEXT("Textures"),
		TEXT("Physics"),
		TEXT("Audio"),
		TEXT("Animation"),
		TEXT("Lumen"),
		TEXT("Nanite"),
		TEXT("ProgramSize"),
		TEXT("RenderTargets"),
		TEXT("SceneRender"),
		TEXT("UObject")
	};
	constexpr int32 DefaultTagCount = UE_ARRAY_COUNT(DefaultTags);

	if (CreatedDefaultTracks.Num() != DefaultTagCount)
	{
		CreatedDefaultTracks.Init(false, DefaultTagCount);
	}

	const auto Tags = TagList.GetTags();
	for (int32 DefaultTagIndex = 0; DefaultTagIndex < DefaultTagCount; ++DefaultTagIndex)
	{
		if (!CreatedDefaultTracks[DefaultTagIndex])
		{
			for (const Insights::FMemoryTag* Tag : Tags)
			{
				if (Tag->GetTrackerId() == DefaultTrackerId && // is it used by the default tracker?
					Tag->GetGraphTracks().Num() == 0 && // a graph isn't already added for this llm tag?
					FCString::Stricmp(*Tag->GetStatName(), DefaultTags[DefaultTagIndex]) == 0) // is it one of the llm tags to show as default?
				{
					CreateMemTagGraphTrack(DefaultTrackerId, Tag->GetId());
					CreatedDefaultTracks[DefaultTagIndex] = true;
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FMemorySharedState::TrackersToString(uint64 Flags, const TCHAR* Conjunction) const
{
	FString Str;
	if (Flags != 0)
	{
		for (const TSharedPtr<Insights::FMemoryTracker>& Tracker : Trackers)
		{
			const uint64 TrackerFlag = Insights::FMemoryTracker::AsFlag(Tracker->GetId());
			if ((Flags & TrackerFlag) != 0)
			{
				if (!Str.IsEmpty())
				{
					Str.Append(Conjunction);
				}
				Str.Append(Tracker->GetName());
				Flags &= ~TrackerFlag;
				if (Flags == 0)
				{
					break;
				}
			}
		}
	}
	return Str;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const Insights::FMemoryTracker* FMemorySharedState::GetTrackerById(Insights::FMemoryTrackerId InMemTrackerId) const
{
	const TSharedPtr<Insights::FMemoryTracker>* TrackerPtr = Trackers.FindByPredicate([InMemTrackerId](TSharedPtr<Insights::FMemoryTracker>& Tracker) { return Tracker->GetId() == InMemTrackerId; });
	return TrackerPtr ? TrackerPtr->Get() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::SyncTrackers()
{
	DefaultTracker = nullptr;
	PlatformTracker = nullptr;
	Trackers.Reset();

	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
		const TraceServices::IMemoryProvider* MemoryProvider = TraceServices::ReadMemoryProvider(*Session.Get());
		if (MemoryProvider)
		{
			MemoryProvider->EnumerateTrackers([this](const TraceServices::FMemoryTrackerInfo& Tracker)
			{
				Trackers.Add(MakeShared<Insights::FMemoryTracker>(Tracker.Id, Tracker.Name));
			});

			Trackers.Sort([](const TSharedPtr<Insights::FMemoryTracker>& A, const TSharedPtr<Insights::FMemoryTracker>& B) { return A->GetId() < B->GetId(); });
		}
	}

	if (Trackers.Num() > 0)
	{
		for (const TSharedPtr<Insights::FMemoryTracker>& Tracker : Trackers)
		{
			if (FCString::Stricmp(*Tracker->GetName(), TEXT("Default")) == 0)
			{
				DefaultTracker = Tracker;
			}
			if (FCString::Stricmp(*Tracker->GetName(), TEXT("Platform")) == 0)
			{
				PlatformTracker = Tracker;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::SetTrackHeightMode(EMemoryTrackHeightMode InTrackHeightMode)
{
	TrackHeightMode = InTrackHeightMode;

	for (TSharedPtr<FMemoryGraphTrack>& GraphTrack : AllTracks)
	{
		GraphTrack->SetCurrentTrackHeight(InTrackHeightMode);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::ExtendOtherTracksFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InOutMenuBuilder)
{
	if (&InSession != TimingView.Get())
	{
		return;
	}

	InOutMenuBuilder.BeginSection("Memory", LOCTEXT("ContextMenu_Section_Memory", "Memory"));
	{
		InOutMenuBuilder.AddMenuEntry(FMemoryTimingViewCommands::Get().ShowHideAllMemoryTracks);
	}
	InOutMenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::BindCommands()
{
	FMemoryTimingViewCommands::Register();

	TSharedPtr<FUICommandList> CommandList = TimingView->GetCommandList();
	ensure(CommandList.IsValid());

	CommandList->MapAction(
		FMemoryTimingViewCommands::Get().ShowHideAllMemoryTracks,
		FExecuteAction::CreateSP(this, &FMemorySharedState::ShowHideAllMemoryTracks),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMemorySharedState::IsAllMemoryTracksToggleOn));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::SetAllMemoryTracksToggle(bool bOnOff)
{
	bShowHideAllMemoryTracks = bOnOff;

	for (TSharedPtr<FMemoryGraphTrack>& GraphTrack : AllTracks)
	{
		GraphTrack->SetVisibilityFlag(bShowHideAllMemoryTracks);
	}

	if (TimingView)
	{
		TimingView->HandleTrackVisibilityChanged();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FMemorySharedState::GetNextMemoryGraphTrackOrder()
{
	int32 Order = FTimingTrackOrder::Memory;
	for (const TSharedPtr<FMemoryGraphTrack>& GraphTrack : AllTracks)
	{
		Order = FMath::Max(Order, GraphTrack->GetOrder() + 1);
	}
	return Order;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphTrack> FMemorySharedState::CreateMemoryGraphTrack()
{
	if (!TimingView.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<FMemoryGraphTrack> GraphTrack = MakeShared<FMemoryGraphTrack>(*this);

	const int32 Order = GetNextMemoryGraphTrackOrder();
	GraphTrack->SetOrder(Order);
	GraphTrack->SetName(TEXT("Memory Graph"));
	GraphTrack->SetVisibilityFlag(bShowHideAllMemoryTracks);

	GraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Small, 100.0f);
	GraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Medium, 300.0f);
	GraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Large, 600.0f);
	GraphTrack->SetCurrentTrackHeight(TrackHeightMode);

	GraphTrack->SetLabelUnit(EGraphTrackLabelUnit::MiB, 1);
	GraphTrack->EnableAutoZoom();

	TimingView->AddScrollableTrack(GraphTrack);
	AllTracks.Add(GraphTrack);

	return GraphTrack;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FMemorySharedState::RemoveMemoryGraphTrack(TSharedPtr<FMemoryGraphTrack> GraphTrack)
{
	if (!GraphTrack)
	{
		return 0;
	}

	if (GraphTrack == MainGraphTrack)
	{
		RemoveTrackFromMemTags(GraphTrack);
		GraphTrack->RemoveAllMemTagSeries();
		if (GraphTrack->GetSeries().Num() == 0)
		{
			GraphTrack->Hide();
			TimingView->HandleTrackVisibilityChanged();
		}
		return -1;
	}

	if (AllTracks.Remove(GraphTrack))
	{
		RemoveTrackFromMemTags(GraphTrack);
		GraphTrack->RemoveAllMemTagSeries();
		TimingView->RemoveTrack(GraphTrack);
		return 1;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::RemoveTrackFromMemTags(TSharedPtr<FMemoryGraphTrack>& GraphTrack)
{
	for (TSharedPtr<FGraphSeries>& Series : GraphTrack->GetSeries())
	{
		//TODO: if (Series->Is<FMemoryGraphSeries>())
		TSharedPtr<FMemoryGraphSeries> MemorySeries = StaticCastSharedPtr<FMemoryGraphSeries>(Series);
		Insights::FMemoryTag* TagPtr = TagList.GetTagById(MemorySeries->GetTrackerId(), MemorySeries->GetTagId());
		if (TagPtr)
		{
			TagPtr->RemoveTrack(GraphTrack);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphTrack> FMemorySharedState::GetMemTagGraphTrack(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId)
{
	if (!TimingView.IsValid())
	{
		return nullptr;
	}

	Insights::FMemoryTag* TagPtr = TagList.GetTagById(InMemTrackerId, InMemTagId);
	if (TagPtr)
	{
		for (TSharedPtr<FMemoryGraphTrack> MemoryGraph : TagPtr->GetGraphTracks())
		{
			if (MemoryGraph != MainGraphTrack && MemoryGraph->GetSeries().Num() == 1)
			{
				return MemoryGraph;
			}
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphTrack> FMemorySharedState::CreateMemTagGraphTrack(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId)
{
	if (!TimingView.IsValid())
	{
		return nullptr;
	}

	Insights::FMemoryTag* TagPtr = TagList.GetTagById(InMemTrackerId, InMemTagId);

	FString SeriesName;
	if (TagPtr)
	{
		const Insights::FMemoryTracker* Tracker = GetTrackerById(InMemTrackerId);
		if (Tracker && Tracker != DefaultTracker.Get())
		{
			SeriesName = FString::Printf(TEXT("LLM %s (%s)"), *TagPtr->GetStatFullName(), *Tracker->GetName());
		}
		else
		{
			SeriesName = FString::Printf(TEXT("LLM %s"), *TagPtr->GetStatFullName());
		}
	}
	else
	{
		SeriesName = FString::Printf(TEXT("Unknown LLM Tag (tag id: 0x%llX, tracker id: %i)"), uint64(InMemTagId), int32(InMemTrackerId));
	}

	const FLinearColor Color = TagPtr ? TagPtr->GetColor() : FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);

	// Also create a series in the MainGraphTrack.
	if (MainGraphTrack.IsValid())
	{
		TSharedPtr<FMemoryGraphSeries> Series = MainGraphTrack->AddMemTagSeries(InMemTrackerId, InMemTagId);
		Series->SetName(SeriesName);
		Series->SetColor(Color, BorderColor, Color.CopyWithNewOpacity(0.1f));
		Series->DisableAutoZoom();
		Series->SetScaleY(0.0000002);

		if (TagPtr)
		{
			TagPtr->AddTrack(MainGraphTrack);
		}

		MainGraphTrack->Show();
		TimingView->HandleTrackVisibilityChanged();
	}

	TSharedPtr<FMemoryGraphTrack> GraphTrack = GetMemTagGraphTrack(InMemTrackerId, InMemTagId);

	if (!GraphTrack.IsValid())
	{
		// Create new Graph track.
		GraphTrack = MakeShared<FMemoryGraphTrack>(*this);

		const int32 Order = GetNextMemoryGraphTrackOrder();
		GraphTrack->SetOrder(Order);
		GraphTrack->SetName(SeriesName);
		//GraphTrack->SetVisibilityFlag(bShowHideAllMemoryTracks);
		GraphTrack->Show();

		GraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Small, 32.0f);
		GraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Medium, 100.0f);
		GraphTrack->SetAvailableTrackHeight(EMemoryTrackHeightMode::Large, 200.0f);
		GraphTrack->SetCurrentTrackHeight(TrackHeightMode);

		GraphTrack->EnableAutoZoom();

		// Create series.
		TSharedPtr<FMemoryGraphSeries> Series = GraphTrack->AddMemTagSeries(InMemTrackerId, InMemTagId);
		Series->SetName(SeriesName);
		Series->SetColor(Color, BorderColor);
		Series->SetBaselineY(GraphTrack->GetHeight() - 1.0f);
		Series->EnableAutoZoom();

		if (TagPtr)
		{
			TagPtr->AddTrack(GraphTrack);
		}

		// Add the new Graph in scrollable tracks.
		TimingView->AddScrollableTrack(GraphTrack);
		AllTracks.Add(GraphTrack);
	}
	else
	{
		GraphTrack->Show();
		TimingView->HandleTrackVisibilityChanged();
	}

	return GraphTrack;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FMemorySharedState::RemoveMemTagGraphTrack(Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId)
{
	if (!TimingView.IsValid())
	{
		return -1;
	}

	int32 TrackCount = 0;

	Insights::FMemoryTag* TagPtr = TagList.GetTagById(InMemTrackerId, InMemTagId);
	if (TagPtr)
	{
		for (TSharedPtr<FMemoryGraphTrack> GraphTrack : TagPtr->GetGraphTracks())
		{
			GraphTrack->RemoveMemTagSeries(InMemTrackerId, InMemTagId);
			if (GraphTrack->GetSeries().Num() == 0)
			{
				if (GraphTrack == MainGraphTrack)
				{
					GraphTrack->Hide();
					TimingView->HandleTrackVisibilityChanged();
				}
				else
				{
					++TrackCount;
					AllTracks.Remove(GraphTrack);
					TimingView->RemoveTrack(GraphTrack);
				}
			}
		}
		TagPtr->RemoveAllTracks();
	}

	return TrackCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FMemorySharedState::RemoveAllMemTagGraphTracks()
{
	if (!TimingView.IsValid())
	{
		return -1;
	}

	int32 TrackCount = 0;

	TArray<TSharedPtr<FMemoryGraphTrack>> TracksToRemove;
	for (TSharedPtr<FMemoryGraphTrack>& GraphTrack : AllTracks)
	{
		GraphTrack->RemoveAllMemTagSeries();
		if (GraphTrack->GetSeries().Num() == 0)
		{
			if (GraphTrack == MainGraphTrack)
			{
				GraphTrack->Hide();
				TimingView->HandleTrackVisibilityChanged();
			}
			else
			{
				++TrackCount;
				TimingView->RemoveTrack(GraphTrack);
				TracksToRemove.Add(GraphTrack);
			}
		}
	}
	for (TSharedPtr<FMemoryGraphTrack>& GraphTrack : TracksToRemove)
	{
		AllTracks.Remove(GraphTrack);
	}

	for (Insights::FMemoryTag* TagPtr : TagList.GetTags())
	{
		TagPtr->RemoveAllTracks();
	}

	return TrackCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphSeries> FMemorySharedState::ToggleMemTagGraphSeries(TSharedPtr<FMemoryGraphTrack> InGraphTrack, Insights::FMemoryTrackerId InMemTrackerId, Insights::FMemoryTagId InMemTagId)
{
	if (!InGraphTrack.IsValid())
	{
		return nullptr;
	}

	Insights::FMemoryTag* TagPtr = TagList.GetTagById(InMemTrackerId, InMemTagId);

	TSharedPtr<FMemoryGraphSeries> Series = InGraphTrack->GetMemTagSeries(InMemTrackerId, InMemTagId);
	if (Series.IsValid())
	{
		// Remove existing series.
		InGraphTrack->RemoveMemTagSeries(InMemTrackerId, InMemTagId);
		InGraphTrack->SetDirtyFlag();
		TimingView->HandleTrackVisibilityChanged();

		if (TagPtr)
		{
			TagPtr->RemoveTrack(InGraphTrack);
		}

		return nullptr;
	}
	else
	{
		// Add new series.
		Series = InGraphTrack->AddMemTagSeries(InMemTrackerId, InMemTagId);
		Series->DisableAutoZoom();

		if (TagPtr)
		{
			TagPtr->AddTrack(InGraphTrack);
		}

		InGraphTrack->SetDirtyFlag();
		InGraphTrack->Show();
		TimingView->HandleTrackVisibilityChanged();

		return Series;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Create graphs from LLMReportTypes.xml file
void FMemorySharedState::CreateTracksFromReport(const FString& Filename)
{
	Insights::FReportConfig ReportConfig;

	Insights::FReportXmlParser ReportXmlParser;

	ReportXmlParser.LoadReportTypesXML(ReportConfig, Filename);
	if (ReportXmlParser.GetStatus() != Insights::FReportXmlParser::EStatus::Completed)
	{
		FMessageLog ReportMessageLog(FMemoryProfilerManager::Get()->GetLogListingName());
		ReportMessageLog.AddMessages(ReportXmlParser.GetErrorMessages());
		ReportMessageLog.Notify();
	}

	CreateTracksFromReport(ReportConfig);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::CreateTracksFromReport(const Insights::FReportConfig& ReportConfig)
{
	for (const Insights::FReportTypeConfig& ReportTypeConfig : ReportConfig.ReportTypes)
	{
		CreateTracksFromReport(ReportTypeConfig);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::CreateTracksFromReport(const Insights::FReportTypeConfig& ReportTypeConfig)
{
	int32 Order = GetNextMemoryGraphTrackOrder();
	int32 NumAddedTracks = 0;

	const bool bIsPlatformTracker = ReportTypeConfig.Name.StartsWith(TEXT("LLMPlatform"));

	for (const Insights::FReportTypeGraphConfig& ReportTypeGraphConfig : ReportTypeConfig.Graphs)
	{
		TSharedPtr<FMemoryGraphTrack> GraphTrack = CreateGraphTrack(ReportTypeGraphConfig, bIsPlatformTracker);
		if (GraphTrack)
		{
			GraphTrack->SetOrder(Order++);
			++NumAddedTracks;
		}
	}

	if (NumAddedTracks > 0)
	{
		if (TimingView)
		{
			TimingView->InvalidateScrollableTracksOrder();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FMemoryGraphTrack> FMemorySharedState::CreateGraphTrack(const Insights::FReportTypeGraphConfig& ReportTypeGraphConfig, bool bIsPlatformTracker)
{
	if (ReportTypeGraphConfig.GraphConfig == nullptr)
	{
		// Invalid graph config.
		return nullptr;
	}

	if (!TimingView.IsValid())
	{
		return nullptr;
	}

	const Insights::FGraphConfig& GraphConfig = *ReportTypeGraphConfig.GraphConfig;

	int32 CharIndex;
	const TCHAR* DelimStr;

	if (GraphConfig.StatString.FindChar(TEXT(','), CharIndex))
	{
		DelimStr = TEXT(",");
	}
	else if (GraphConfig.StatString.FindChar(TEXT(';'), CharIndex))
	{
		DelimStr = TEXT(";");
	}
	else
	{
		DelimStr = TEXT(" ");
	}
	TArray<FString> IncludeStats;
	GraphConfig.StatString.ParseIntoArray(IncludeStats, DelimStr);

	if (IncludeStats.Num() == 0)
	{
		// No stats specified!?
		return nullptr;
	}

	if (GraphConfig.IgnoreStats.FindChar(TEXT(';'), CharIndex))
	{
		DelimStr = TEXT(";");
	}
	else if (GraphConfig.IgnoreStats.FindChar(TEXT(','), CharIndex))
	{
		DelimStr = TEXT(",");
	}
	else
	{
		DelimStr = TEXT(" ");
	}
	TArray<FString> IgnoreStats;
	GraphConfig.IgnoreStats.ParseIntoArray(IgnoreStats, DelimStr);

	TArray<Insights::FMemoryTag*> Tags;
	TagList.FilterTags(IncludeStats, IgnoreStats, Tags);

	Insights::FMemoryTrackerId MemTrackerId = bIsPlatformTracker ?
		(PlatformTracker ? PlatformTracker->GetId() : Insights::FMemoryTracker::InvalidTrackerId) :
		(DefaultTracker ? DefaultTracker->GetId() : Insights::FMemoryTracker::InvalidTrackerId);

	TSharedPtr<FMemoryGraphTrack> GraphTrack = CreateMemoryGraphTrack();
	if (GraphTrack)
	{
		if (GraphConfig.Height > 0.0f)
		{
			constexpr float MinGraphTrackHeight = 32.0f;
			constexpr float MaxGraphTrackHeight = 600.0f;
			GraphTrack->SetHeight(FMath::Clamp(GraphConfig.Height, MinGraphTrackHeight, MaxGraphTrackHeight));
		}

		GraphTrack->SetName(ReportTypeGraphConfig.Title);

		const double MinValue = GraphConfig.MinY * 1024.0 * 1024.0;
		const double MaxValue = GraphConfig.MaxY * 1024.0 * 1024.0;
		GraphTrack->SetDefaultValueRange(MinValue, MaxValue);

		UE_LOG(MemoryProfiler, Log, TEXT("[LLM Tags] Created graph \"%s\" (H=%.1f%s, MainStat=%s, Stats=%s)"),
			*ReportTypeGraphConfig.Title,
			GraphTrack->GetHeight(),
			GraphConfig.bStacked ? TEXT(", stacked") : TEXT(""),
			*GraphConfig.MainStat,
			*GraphConfig.StatString);

		TSharedPtr<FMemoryGraphSeries> MainSeries;

		for (Insights::FMemoryTag* TagPtr : Tags)
		{
			Insights::FMemoryTag& Tag = *TagPtr;

			TSharedPtr<FMemoryGraphSeries> Series = GraphTrack->AddMemTagSeries(MemTrackerId, Tag.GetId());
			Series->SetName(FText::FromString(FString::Printf(TEXT("LLM %s"), *Tag.GetStatFullName())));
			const FLinearColor Color = Tag.GetColor();
			const FLinearColor BorderColor(FMath::Min(Color.R + 0.4f, 1.0f), FMath::Min(Color.G + 0.4f, 1.0f), FMath::Min(Color.B + 0.4f, 1.0f), 1.0f);
			Series->SetColor(Color, BorderColor);

			Tag.AddTrack(MainGraphTrack);

			if (GraphConfig.MainStat == Tag.GetStatName())
			{
				MainSeries = Series;
			}
		}

		if (GraphConfig.bStacked)
		{
			GraphTrack->SetStacked(true);
			GraphTrack->SetMainSeries(MainSeries);
		}
	}

	return GraphTrack;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::InitMemoryRules()
{
	using ERule = TraceServices::IAllocationsProvider::EQueryRule;

	MemoryRules.Reset();

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::aAf, 1,
		LOCTEXT("MemRule_aAf_Short", "*A*"),
		LOCTEXT("MemRule_aAf_Verbose", "Active Allocs"),
		LOCTEXT("MemRule_aAf_Desc", "Identifies active allocations at time A.\n(a ≤ A ≤ f)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::afA, 1,
		LOCTEXT("MemRule_afA_Short", "**A"),
		LOCTEXT("MemRule_afA_Verbose", "Before"),
		LOCTEXT("MemRule_afA_Desc", "Identifies allocations allocated and freed before time A.\n(a ≤ f ≤ A)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::Aaf, 1,
		LOCTEXT("MemRule_Aaf_Short", "A**"),
		LOCTEXT("MemRule_Aaf_Verbose", "After"),
		LOCTEXT("MemRule_Aaf_Desc", "Identifies allocations allocated after time A.\n(A ≤ a ≤ f)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::aAfB, 2,
		LOCTEXT("MemRule_aAfB_Short", "*A*B"),
		LOCTEXT("MemRule_aAfB_Verbose", "Decline"),
		LOCTEXT("MemRule_aAfB_Desc", "Identifies allocations allocated before time A and freed between time A and time B.\n(a ≤ A ≤ f ≤ B)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::AaBf, 2,
		LOCTEXT("MemRule_AaBf_Short", "A*B*"),
		LOCTEXT("MemRule_AaBf_Verbose", "Growth"),
		LOCTEXT("MemRule_AaBf_Desc", "Identifies allocations allocated between time A and time B and not freed until at least time B.\n(A ≤ a ≤ B ≤ f)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::aAfaBf, 2,
		LOCTEXT("MemRule_aAfaBf_Short", "A*B*/*A*B"),
		LOCTEXT("MemRule_aAfaBf_Verbose", "Growth vs. Decline"),
		LOCTEXT("MemRule_aAfaBf_Desc", "Identifies \"growth\" allocations, allocated between time A and time B and not freed until at least time B (A ≤ a ≤ B ≤ f)\nand \"decline\" allocations, allocated before time A and freed between time A and time B (a ≤ A ≤ f ≤ B).\nThe \"decline\" allocations are changed to have negative size, so the size aggregation shows variation between A and B.")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::AfB, 2,
		LOCTEXT("MemRule_AfB_Short", "*A**B"),
		LOCTEXT("MemRule_AfB_Verbose", "Free Events"),
		LOCTEXT("MemRule_AfB_Desc", "Identifies allocations freed between time A and time B.\n(A ≤ f ≤ B)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::AaB, 2,
		LOCTEXT("MemRule_AaB_Short", "A**B*"),
		LOCTEXT("MemRule_AaB_Verbose", "Alloc Events"),
		LOCTEXT("MemRule_AaB_Desc", "Identifies allocations allocated between time A and time B.\n(A ≤ a ≤ B)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::AafB, 2,
		LOCTEXT("MemRule_AafB_Short", "A**B"),
		LOCTEXT("MemRule_AafB_Verbose", "Short Living Allocs"),
		LOCTEXT("MemRule_AafB_Desc", "Identifies allocations allocated and freed between time A and time B.\n(A ≤ a ≤ f ≤ B)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::aABf, 2,
		LOCTEXT("MemRule_aABf_Short", "*A B*"),
		LOCTEXT("MemRule_aABf_Verbose", "Long Living Allocs"),
		LOCTEXT("MemRule_aABf_Desc", "Identifies allocations allocated before time A and not freed until at least time B.\n(a ≤ A ≤ B ≤ f)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::AaBCf, 3,
		LOCTEXT("MemRule_AaBCf_Short", "A*B C*"),
		LOCTEXT("MemRule_AaBCf_Verbose", "Memory Leaks"),
		LOCTEXT("MemRule_AaBCf_Desc", "Identifies allocations allocated between time A and time B and not freed until at least time C.\n(A ≤ a ≤ B ≤ C ≤ f)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::AaBfC, 3,
		LOCTEXT("MemRule_AaBfC_Short", "A*B*C"),
		LOCTEXT("MemRule_AaBfC_Verbose", "Limited Lifetime"),
		LOCTEXT("MemRule_AaBfC_Desc", "Identifies allocations allocated between time A and time B and freed between time B and time C.\n(A ≤ a ≤ B ≤ f ≤ C)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::aABfC, 3,
		LOCTEXT("MemRule_aABfC_Short", "*A B*C"),
		LOCTEXT("MemRule_aABfC_Verbose", "Decline of Long Living Allocs"),
		LOCTEXT("MemRule_aABfC_Desc", "Identifies allocations allocated before time A and freed between time B and time C.\n(a ≤ A ≤ B ≤ f ≤ C)")));

	MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
		ERule::AaBCfD, 4,
		LOCTEXT("MemRule_AaBCfD_Short", "A*B C*D"),
		LOCTEXT("MemRule_AaBCfD_Verbose", "Specific Lifetime"),
		LOCTEXT("MemRule_AaBCfD_Desc", "Identifies allocations allocated between time A and time B and freed between time C and time D.\n(A ≤ a ≤ B ≤ C ≤ f ≤ D)")));

	//TODO
	//MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
	//	ERule::A_vs_B, 2,
	//	LOCTEXT("MemRule_A_vs_B_Short", "*A* + *B*"),
	//	LOCTEXT("MemRule_A_vs_B_Verbose", "Compare A vs. B"),
	//	LOCTEXT("MemRule_A_vs_B_Desc", "Compares live allocations at time A with live allocations at time B.\n(*A* vs. *B*)")));

	//TODO
	//MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
	//	ERule::A_or_B, 2,
	//	LOCTEXT("MemRule_A_or_B_Short", "*A* | *B*"),
	//	LOCTEXT("MemRule_A_or_B_Verbose", "A or B"),
	//	LOCTEXT("MemRule_A_or_B_Desc", "Identifies allocations live at time A or at time B.\n(a ≤ A ≤ f OR a ≤ B ≤ f)\n{*A*} U {*B*}")));

	//TODO
	//MemoryRules.Add(MakeShared<Insights::FMemoryRuleSpec>(
	//	ERule::A_xor_B, 2,
	//	LOCTEXT("MemRule_A_xor_B_Short", "*A* ^ *B*"),
	//	LOCTEXT("MemRule_A_xor_B_Verbose", "A xor B"),
	//	LOCTEXT("MemRule_A_xor_B_Desc", "Identifies allocations live either at time A or at time B (but not both).\n(a ≤ A ≤ f XOR a ≤ B ≤ f)\n({*A*} U {*B*}) \\ {*AB*}")));

	CurrentMemoryRule = MemoryRules[0];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::OnMemoryRuleChanged()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemorySharedState::AddQueryTarget(TSharedPtr<Insights::FQueryTargetWindowSpec> InPtr)
{
	QueryTargetSpecs.Add(InPtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////


void FMemorySharedState::RemoveQueryTarget(TSharedPtr<Insights::FQueryTargetWindowSpec> InPtr)
{
	QueryTargetSpecs.Remove(InPtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
