// Copyright Epic Games, Inc. All Rights Reserved.

#include "FileActivityTimingTrack.h"

#include "Algo/BinarySearch.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateBrush.h"
#include "TraceServices/Model/LoadTimeProfiler.h"

// Insights
#include "Insights/Common/InsightsMenuBuilder.h"
#include "Insights/Common/PaintUtils.h"
#include "Insights/Common/Stopwatch.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/ITimingViewSession.h"
#include "Insights/TimingProfilerCommon.h"
#include "Insights/ViewModels/TimingEvent.h"
#include "Insights/ViewModels/TimingTrackViewport.h"
#include "Insights/ViewModels/TooltipDrawState.h"
#include "Insights/ViewModels/TimingEventSearch.h"
#include "Insights/Widgets/STimingView.h"

#include <limits>

#define LOCTEXT_NAMESPACE "FileActivityTimingTrack"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FFileActivityTimingViewCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

FFileActivityTimingViewCommands::FFileActivityTimingViewCommands()
: TCommands<FFileActivityTimingViewCommands>(
	TEXT("FileActivityTimingViewCommands"),
	NSLOCTEXT("Contexts", "FileActivityTimingViewCommands", "Insights - Timing View - File Activity"),
	NAME_None,
	FInsightsStyle::GetStyleSetName())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FFileActivityTimingViewCommands::~FFileActivityTimingViewCommands()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// UI_COMMAND takes long for the compiler to optimize
UE_DISABLE_OPTIMIZATION_SHIP
void FFileActivityTimingViewCommands::RegisterCommands()
{
	// This command is used only for its key binding (to toggle both ShowHideIoOverviewTrack and ShowHideIoActivityTrack in the same time).
	UI_COMMAND(ShowHideAllIoTracks,
		"File Activity Tracks",
		"Shows/hides the File Activity tracks.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::I));

	UI_COMMAND(ShowHideIoOverviewTrack,
		"I/O Overview Track",
		"Shows/hides the I/O Overview track.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord());

	UI_COMMAND(ToggleOnlyErrors,
		"Only Errors (I/O Overview Track)",
		"Shows only the events with errors, in the I/O Overview track.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord());

	UI_COMMAND(ShowHideIoActivityTrack,
		"I/O Activity Track",
		"Shows/hides the I/O Activity track.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord());

	UI_COMMAND(ToggleBackgroundEvents,
		"Background Events (I/O Activity Track)",
		"Shows/hides background events for file activities, in the I/O Activity track.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::O));
}
UE_ENABLE_OPTIMIZATION_SHIP

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* GetFileActivityTypeName(TraceServices::EFileActivityType Type)
{
	static_assert(TraceServices::FileActivityType_Open == 0, "TraceServices::EFileActivityType enum has changed!?");
	static_assert(TraceServices::FileActivityType_ReOpen == 1, "TraceServices::EFileActivityType enum has changed!?");
	static_assert(TraceServices::FileActivityType_Close == 2, "TraceServices::EFileActivityType enum has changed!?");
	static_assert(TraceServices::FileActivityType_Read == 3, "TraceServices::EFileActivityType enum has changed!?");
	static_assert(TraceServices::FileActivityType_Write == 4, "TraceServices::EFileActivityType enum has changed!?");
	static_assert(TraceServices::FileActivityType_Count == 5, "TraceServices::EFileActivityType enum has changed!?");
	static const TCHAR* GFileActivityTypeNames[] =
	{
		TEXT("Open"),
		TEXT("ReOpen"),
		TEXT("Close"),
		TEXT("Read"),
		TEXT("Write"),
		TEXT("Idle"), // virtual events added for cases where Close event is more than 1s away from last Open/Read/Write event.
		TEXT("NotClosed") // virtual events added when an Open activity never closes
	};
	return GFileActivityTypeNames[Type];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 GetFileActivityTypeColor(TraceServices::EFileActivityType Type)
{
	static const uint32 GFileActivityTypeColors[] =
	{
		0xFFCCAA33, // open
		0xFFBB9922, // reopen
		0xFF33AACC, // close
		0xFF33AA33, // read
		0xFFDD33CC, // write
		0x55333333, // idle
		0x55553333, // close
	};
	return GFileActivityTypeColors[Type];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FFileActivitySharedState
////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32 FFileActivitySharedState::MaxLanes = 10000;

void FFileActivitySharedState::OnBeginSession(Insights::ITimingViewSession& InSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	IoOverviewTrack.Reset();
	IoActivityTrack.Reset();

	bShowHideAllIoTracks = false;
	bForceIoEventsUpdate = false;

	FileActivities.Reset();
	FileActivityMap.Reset();
	AllIoEvents.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::OnEndSession(Insights::ITimingViewSession& InSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	IoOverviewTrack.Reset();
	IoActivityTrack.Reset();

	bShowHideAllIoTracks = false;
	bForceIoEventsUpdate = false;

	FileActivities.Reset();
	FileActivityMap.Reset();
	AllIoEvents.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession)
{
	if (&InSession != TimingView)
	{
		return;
	}

	if (!TraceServices::ReadFileActivityProvider(InAnalysisSession))
	{
		return;
	}

	if (!IoOverviewTrack.IsValid())
	{
		IoOverviewTrack = MakeShared<FOverviewFileActivityTimingTrack>(*this);
		IoOverviewTrack->SetOrder(FTimingTrackOrder::First);
		IoOverviewTrack->SetVisibilityFlag(bShowHideAllIoTracks);
		InSession.AddScrollableTrack(IoOverviewTrack);
	}

	if (!IoActivityTrack.IsValid())
	{
		IoActivityTrack = MakeShared<FDetailedFileActivityTimingTrack>(*this);
		IoActivityTrack->SetOrder(FTimingTrackOrder::Last);
		IoActivityTrack->SetVisibilityFlag(bShowHideAllIoTracks);
		InSession.AddScrollableTrack(IoActivityTrack);
	}

	if (bForceIoEventsUpdate)
	{
		bForceIoEventsUpdate = false;

		FileActivities.Reset();
		FileActivityMap.Reset();
		AllIoEvents.Reset();

		FStopwatch Stopwatch;
		Stopwatch.Start();

		// Enumerate all IO events and cache them.
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(InAnalysisSession);
			const TraceServices::IFileActivityProvider& FileActivityProvider = *TraceServices::ReadFileActivityProvider(InAnalysisSession);
			FileActivityProvider.EnumerateFileActivity([this](const TraceServices::FFileInfo& FileInfo, const TraceServices::IFileActivityProvider::Timeline& Timeline)
			{
				TSharedPtr<FIoFileActivity> Activity = MakeShared<FIoFileActivity>();

				Activity->Id = FileInfo.Id;
				Activity->Path = FileInfo.Path;
				Activity->StartTime = +std::numeric_limits<double>::infinity();
				Activity->EndTime = -std::numeric_limits<double>::infinity();
				Activity->CloseStartTime = +std::numeric_limits<double>::infinity();
				Activity->CloseEndTime = +std::numeric_limits<double>::infinity();
				Activity->EventCount = 0;
				Activity->Index = -1;
				Activity->MaxConcurrentEvents = 0;
				Activity->StartingDepth = 0;

				const int32 ActivityIndex = FileActivities.Num();
				FileActivities.Add(Activity);
				FileActivityMap.Add(FileInfo.Id, Activity);

				TArray<double> ConcurrentEvents;
				Timeline.EnumerateEvents(-std::numeric_limits<double>::infinity(), +std::numeric_limits<double>::infinity(),
				[this, &Activity, ActivityIndex, &FileInfo, &Timeline, &ConcurrentEvents](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FFileActivity* FileActivity)
				{
					if (FileActivity->ActivityType != TraceServices::FileActivityType_Close)
					{
						// events should be ordered by start time, but Activity->StartTime may not be initialized
						ensure(Activity->StartTime == +std::numeric_limits<double>::infinity() || EventStartTime >= Activity->StartTime);
						if (EventStartTime < Activity->StartTime)
						{
							Activity->StartTime = EventStartTime;
						}

						if (EventEndTime > Activity->EndTime)
						{
							Activity->EndTime = EventEndTime;
						}
					}
					else
					{
						// The time range for the Close event is stored separated;
						// this allows us to insert lanes into the idle time between the last read from a file and when the file is actually closed
						Activity->CloseStartTime = EventStartTime;
						Activity->CloseEndTime = EventEndTime;
					}

					Activity->EventCount++;

					uint32 LocalDepth = MAX_uint32;
					for (int32 i = 0; i < ConcurrentEvents.Num(); ++i)
					{
						if (EventStartTime >= ConcurrentEvents[i])
						{
							LocalDepth = i;
							ConcurrentEvents[i] = EventEndTime;
							break;
						}
					}

					if (LocalDepth == MAX_uint32)
					{
						LocalDepth = ConcurrentEvents.Num();
						ConcurrentEvents.Add(EventEndTime);
						Activity->MaxConcurrentEvents = ConcurrentEvents.Num();
					}

					uint32 Type = ((uint32)FileActivity->ActivityType & 0x0F) | (FileActivity->Failed ? 0x80 : 0);
					AllIoEvents.Add(FIoTimingEvent{ EventStartTime, EventEndTime, LocalDepth, Type, FileActivity->Offset, FileActivity->Size, FileActivity->ActualSize, ActivityIndex, FileActivity->FileHandle, FileActivity->ReadWriteHandle });
					return TraceServices::EEventEnumerate::Continue;
				});

				return true;
			});
		}

		Stopwatch.Stop();
		UE_LOG(TimingProfiler, Log, TEXT("[IO] Enumerated %s events (%s file activities) in %s."),
			*FText::AsNumber(AllIoEvents.Num()).ToString(),
			*FText::AsNumber(FileActivities.Num()).ToString(),
			*TimeUtils::FormatTimeAuto(Stopwatch.GetAccumulatedTime()));
		Stopwatch.Restart();

		// Sort cached IO file activities by Start Time.
		FileActivities.Sort([](const TSharedPtr<FIoFileActivity>& A, const TSharedPtr<FIoFileActivity>& B) { return A->StartTime < B->StartTime; });

		// Sort cached IO events by Start Time.
		AllIoEvents.Sort([](const FIoTimingEvent& A, const FIoTimingEvent& B) { return A.StartTime < B.StartTime; });

		Stopwatch.Stop();
		UE_LOG(TimingProfiler, Log, TEXT("[IO] Sorted file activities and events in %s."), *TimeUtils::FormatTimeAuto(Stopwatch.GetAccumulatedTime()));

		if (FileActivities.Num() > 0)
		{
			//////////////////////////////////////////////////
			// Compute depth for file activities (avoids overlaps).

			Stopwatch.Restart();

			struct FLane
			{
				double EndTime = 0.0f;
				double CloseStartTime;
				double CloseEndTime;
			};

			TArray<FLane> Lanes; // one lane per event depth, a file activity occupies multiple lanes

			for (const TSharedPtr<FIoFileActivity>& FileActivityPtr : FileActivities)
			{
				FIoFileActivity& Activity = *FileActivityPtr;

				// Find lane (avoiding overlaps with other file activities).
				int32 Depth = 0;
				while (Depth < Lanes.Num())
				{
					bool bOverlap = false;
					for (int32 LocalDepth = 0; LocalDepth < Activity.MaxConcurrentEvents; ++LocalDepth)
					{
						if (Depth + LocalDepth >= Lanes.Num())
						{
							break;
						}
						const FLane& Lane = Lanes[Depth + LocalDepth];
						if (Activity.StartTime < Lane.EndTime ||
							(Activity.StartTime < Lane.CloseEndTime && Activity.EndTime > Lane.CloseStartTime)) // overlaps with a Close event
						{
							bOverlap = true;
							Depth += LocalDepth;
							break;
						}
					}
					if (!bOverlap)
					{
						break;
					}
					++Depth;
				}

				int32 NewLaneNum = Depth + Activity.MaxConcurrentEvents;

				if (NewLaneNum > MaxLanes)
				{
					// Snap to the bottom; allows overlaps in this case.
					Activity.StartingDepth = MaxLanes - Activity.MaxConcurrentEvents;
				}
				else
				{
					if (NewLaneNum > Lanes.Num())
					{
						Lanes.AddDefaulted(NewLaneNum - Lanes.Num());
					}

					Activity.StartingDepth = Depth;

					// Set close event only for first lane of the activity.
					Lanes[Depth].CloseStartTime = Activity.CloseStartTime;
					Lanes[Depth].CloseEndTime = Activity.CloseEndTime;

					for (int32 LocalDepth = 0; LocalDepth < Activity.MaxConcurrentEvents; ++LocalDepth)
					{
						Lanes[Depth + LocalDepth].EndTime = Activity.EndTime;
					}
				}
			}

			Stopwatch.Stop();
			UE_LOG(TimingProfiler, Log, TEXT("[IO] Computed layout for file activities in %s."), *TimeUtils::FormatTimeAuto(Stopwatch.GetAccumulatedTime()));

			//////////////////////////////////////////////////

			Stopwatch.Restart();

			for (FIoTimingEvent& Event : AllIoEvents)
			{
				Event.Depth += FileActivities[Event.FileActivityIndex]->StartingDepth;
				ensure(Event.Depth < MaxLanes);
			}

			Stopwatch.Stop();
			UE_LOG(TimingProfiler, Log, TEXT("[IO] Updated depth for events in %s."), *TimeUtils::FormatTimeAuto(Stopwatch.GetAccumulatedTime()));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::ExtendOtherTracksFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InOutMenuBuilder)
{
	if (&InSession != TimingView)
	{
		return;
	}

	BuildSubMenu(InOutMenuBuilder);

	//InOutMenuBuilder.BeginSection("File Activity");
	//{
	//	InOutMenuBuilder.AddSubMenu(
	//		LOCTEXT("FileActivity_SubMenu", "File Activity"),
	//		LOCTEXT("FileActivity_SubMenu_Desc", "File Activity track options"),
	//		FNewMenuDelegate::CreateSP(this, &FFileActivitySharedState::BuildSubMenu),
	//		false,
	//		FSlateIcon()
	//	);
	//}
	//InOutMenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::BindCommands()
{
	FFileActivityTimingViewCommands::Register();

	TSharedPtr<FUICommandList> CommandList = TimingView->GetCommandList();
	ensure(CommandList.IsValid());

	// This command is used only for its key binding (to toggle both ShowHideIoOverviewTrack and ShowHideIoActivityTrack in the same time).
	CommandList->MapAction(
		FFileActivityTimingViewCommands::Get().ShowHideAllIoTracks,
		FExecuteAction::CreateSP(this, &FFileActivitySharedState::ShowHideAllIoTracks),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FFileActivitySharedState::IsAllIoTracksToggleOn));

	CommandList->MapAction(
		FFileActivityTimingViewCommands::Get().ShowHideIoOverviewTrack,
		FExecuteAction::CreateSP(this, &FFileActivitySharedState::ShowHideIoOverviewTrack),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FFileActivitySharedState::IsIoOverviewTrackVisible));

	CommandList->MapAction(
		FFileActivityTimingViewCommands::Get().ToggleOnlyErrors,
		FExecuteAction::CreateSP(this, &FFileActivitySharedState::ToggleOnlyErrors),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FFileActivitySharedState::IsOnlyErrorsToggleOn));

	CommandList->MapAction(
		FFileActivityTimingViewCommands::Get().ShowHideIoActivityTrack,
		FExecuteAction::CreateSP(this, &FFileActivitySharedState::ShowHideIoActivityTrack),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FFileActivitySharedState::IsIoActivityTrackVisible));

	CommandList->MapAction(
		FFileActivityTimingViewCommands::Get().ToggleBackgroundEvents,
		FExecuteAction::CreateSP(this, &FFileActivitySharedState::ToggleBackgroundEvents),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FFileActivitySharedState::AreBackgroundEventsVisible));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::BuildSubMenu(FMenuBuilder& InOutMenuBuilder)
{
	InOutMenuBuilder.BeginSection("File Activity", LOCTEXT("ContextMenu_Section_FileActivity", "File Activity"));
	{
		// Note: We use the custom AddMenuEntry in order to set the same key binding text for multiple menu items.

		//InOutMenuBuilder.AddMenuEntry(FFileActivityTimingViewCommands::Get().ShowHideIoOverviewTrack);
		FInsightsMenuBuilder::AddMenuEntry(InOutMenuBuilder,
			FUIAction(
				FExecuteAction::CreateSP(this, &FFileActivitySharedState::ShowHideIoOverviewTrack),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FFileActivitySharedState::IsIoOverviewTrackVisible)),
			FFileActivityTimingViewCommands::Get().ShowHideIoOverviewTrack->GetLabel(),
			FFileActivityTimingViewCommands::Get().ShowHideIoOverviewTrack->GetDescription(),
			LOCTEXT("FileActivityTracksKeybinding", "I"),
			EUserInterfaceActionType::ToggleButton);

		InOutMenuBuilder.AddMenuEntry(FFileActivityTimingViewCommands::Get().ToggleOnlyErrors);

		//InOutMenuBuilder.AddMenuEntry(FFileActivityTimingViewCommands::Get().ShowHideIoActivityTrack);
		FInsightsMenuBuilder::AddMenuEntry(InOutMenuBuilder,
			FUIAction(
				FExecuteAction::CreateSP(this, &FFileActivitySharedState::ShowHideIoActivityTrack),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FFileActivitySharedState::IsIoActivityTrackVisible)),
			FFileActivityTimingViewCommands::Get().ShowHideIoActivityTrack->GetLabel(),
			FFileActivityTimingViewCommands::Get().ShowHideIoActivityTrack->GetDescription(),
			LOCTEXT("FileActivityTracksKeybinding", "I"),
			EUserInterfaceActionType::ToggleButton);

		InOutMenuBuilder.AddMenuEntry(FFileActivityTimingViewCommands::Get().ToggleBackgroundEvents);
	}
	InOutMenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::SetAllIoTracksToggle(bool bOnOff)
{
	bShowHideAllIoTracks = bOnOff;

	if (IoOverviewTrack.IsValid())
	{
		IoOverviewTrack->SetVisibilityFlag(bShowHideAllIoTracks);
	}
	if (IoActivityTrack.IsValid())
	{
		IoActivityTrack->SetVisibilityFlag(bShowHideAllIoTracks);
	}

	if (TimingView)
	{
		TimingView->HandleTrackVisibilityChanged();
	}

	if (bShowHideAllIoTracks)
	{
		RequestUpdate();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFileActivitySharedState::IsIoOverviewTrackVisible() const
{
	return IoOverviewTrack && IoOverviewTrack->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::ShowHideIoOverviewTrack()
{
	if (IoOverviewTrack.IsValid())
	{
		IoOverviewTrack->ToggleVisibility();
	}

	if (TimingView)
	{
		TimingView->HandleTrackVisibilityChanged();
	}

	const bool bIsOverviewTrackVisible = IsIoOverviewTrackVisible();
	const bool bIsActivityTrackVisible = IsIoActivityTrackVisible();

	if (bIsOverviewTrackVisible == bIsActivityTrackVisible)
	{
		bShowHideAllIoTracks = bIsOverviewTrackVisible;
	}

	if (bIsOverviewTrackVisible)
	{
		RequestUpdate();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFileActivitySharedState::IsIoActivityTrackVisible() const
{
	return IoActivityTrack && IoActivityTrack->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::ShowHideIoActivityTrack()
{
	if (IoActivityTrack.IsValid())
	{
		IoActivityTrack->ToggleVisibility();
	}

	if (TimingView)
	{
		TimingView->HandleTrackVisibilityChanged();
	}

	const bool bIsOverviewTrackVisible = IsIoOverviewTrackVisible();
	const bool bIsActivityTrackVisible = IsIoActivityTrackVisible();

	if (bIsOverviewTrackVisible == bIsActivityTrackVisible)
	{
		bShowHideAllIoTracks = bIsOverviewTrackVisible;
	}

	if (bIsActivityTrackVisible)
	{
		RequestUpdate();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFileActivitySharedState::IsOnlyErrorsToggleOn() const
{
	return IoOverviewTrack && IoOverviewTrack->IsOnlyErrorsToggleOn();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::ToggleOnlyErrors()
{
	if (IoOverviewTrack)
	{
		IoOverviewTrack->ToggleOnlyErrors();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFileActivitySharedState::AreBackgroundEventsVisible() const
{
	return IoActivityTrack && IoActivityTrack->AreBackgroundEventsVisible();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivitySharedState::ToggleBackgroundEvents()
{
	if (IoActivityTrack)
	{
		IoActivityTrack->ToggleBackgroundEvents();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FFileActivityTimingTrack
////////////////////////////////////////////////////////////////////////////////////////////////////

INSIGHTS_IMPLEMENT_RTTI(FFileActivityTimingTrack)

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFileActivityTimingTrack::InitTooltip(FTooltipDrawState& InOutTooltip, const ITimingEvent& InTooltipEvent) const
{
	InOutTooltip.ResetContent();

	if (InTooltipEvent.CheckTrack(this) && InTooltipEvent.Is<FTimingEvent>())
	{
		const FTimingEvent& TooltipEvent = InTooltipEvent.As<FTimingEvent>();

		auto MatchEvent = [this, &TooltipEvent](double InStartTime, double InEndTime, uint32 InDepth)
		{
			return InDepth == TooltipEvent.GetDepth()
				&& InStartTime == TooltipEvent.GetStartTime()
				&& InEndTime == TooltipEvent.GetEndTime();
		};

		FTimingEventSearchParameters SearchParameters(TooltipEvent.GetStartTime(), TooltipEvent.GetEndTime(), ETimingEventSearchFlags::StopAtFirstMatch, MatchEvent);
		FindIoTimingEvent(SearchParameters, [this, &InOutTooltip, &TooltipEvent](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const FFileActivitySharedState::FIoTimingEvent& InEvent)
		{
			const TraceServices::EFileActivityType ActivityType = static_cast<TraceServices::EFileActivityType>(InEvent.Type & 0x0F);
			const bool bHasFailed = ((InEvent.Type & 0xF0) != 0);

			FString TypeStr;
			uint32 TypeColor;
			if (bHasFailed)
			{
				TypeStr = TEXT("Failed ");
				TypeStr += GetFileActivityTypeName(ActivityType);
				TypeColor = 0xFFFF3333;
			}
			else
			{
				TypeStr = GetFileActivityTypeName(ActivityType);
				TypeColor = GetFileActivityTypeColor(ActivityType);
			}
			if (InEvent.ActualSize != InEvent.Size)
			{
				TypeStr += TEXT(" [!]");
			}
			FLinearColor TypeLinearColor = FLinearColor(FColor(TypeColor));
			TypeLinearColor.R *= 2.0f;
			TypeLinearColor.G *= 2.0f;
			TypeLinearColor.B *= 2.0f;
			InOutTooltip.AddTitle(TypeStr, TypeLinearColor);

			if (ensure(InEvent.FileActivityIndex >= 0 && InEvent.FileActivityIndex < SharedState.FileActivities.Num()))
			{
				const TSharedPtr<FFileActivitySharedState::FIoFileActivity>& ActivityPtr = SharedState.FileActivities[InEvent.FileActivityIndex];
				check(ActivityPtr.IsValid());
				FFileActivitySharedState::FIoFileActivity& Activity = *ActivityPtr;

				InOutTooltip.AddTitle(Activity.Path);
			}

			if (InEvent.FileHandle != uint64(-1))
			{
				const FString Value = FString::Printf(TEXT("0x%llX"), InEvent.FileHandle);
				InOutTooltip.AddNameValueTextLine(TEXT("File Handle:"), Value);
			}

			if (InEvent.ReadWriteHandle != uint64(-1))
			{
				const FString Value = FString::Printf(TEXT("0x%llX"), InEvent.ReadWriteHandle);
				InOutTooltip.AddNameValueTextLine(TEXT("Read/Write Handle:"), Value);
			}

			const double Duration = InEvent.EndTime - InEvent.StartTime;
			InOutTooltip.AddNameValueTextLine(TEXT("Duration:"), TimeUtils::FormatTimeAuto(Duration));

			if (ActivityType == TraceServices::FileActivityType_Read || ActivityType == TraceServices::FileActivityType_Write)
			{
				InOutTooltip.AddNameValueTextLine(TEXT("Offset:"), FText::AsNumber(InEvent.Offset).ToString() + TEXT(" bytes"));
				InOutTooltip.AddNameValueTextLine(TEXT("Size:"), FText::AsNumber(InEvent.Size).ToString() + TEXT(" bytes"));
				FString ActualSizeStr = FText::AsNumber(InEvent.ActualSize).ToString() + TEXT(" bytes");
				if (InEvent.ActualSize != InEvent.Size)
				{
					ActualSizeStr += TEXT(" [!]");
				}
				InOutTooltip.AddNameValueTextLine(TEXT("Actual Size:"), ActualSizeStr);
			}

			if (!bIgnoreEventDepth)
			{
				InOutTooltip.AddNameValueTextLine(TEXT("Depth:"), FString::Printf(TEXT("%d"), InEvent.Depth));
			}

			InOutTooltip.UpdateLayout();
		});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FFileActivityTimingTrack::FindIoTimingEvent(const FTimingEventSearchParameters& InParameters, TFunctionRef<void(double, double, uint32, const FFileActivitySharedState::FIoTimingEvent&)> InFoundPredicate) const
{
	return TTimingEventSearch<FFileActivitySharedState::FIoTimingEvent>::Search(
		InParameters,

		// Search...
		[this](TTimingEventSearch<FFileActivitySharedState::FIoTimingEvent>::FContext& InContext)
		{
			const TArray<FFileActivitySharedState::FIoTimingEvent>& Events = SharedState.GetAllEvents();

			if (bIgnoreDuration)
			{
				// Events are sorted by start time.
				// Find the first event with StartTime >= searched StartTime.
				int32 StartIndex = Algo::LowerBoundBy(Events, InContext.GetParameters().StartTime,
					[](const FFileActivitySharedState::FIoTimingEvent& Event) { return Event.StartTime; });

				for (int32 Index = StartIndex; Index < Events.Num(); ++Index)
				{
					const FFileActivitySharedState::FIoTimingEvent& Event = Events[Index];

					if (bShowOnlyErrors && ((Event.Type & 0xF0) == 0))
					{
						continue;
					}

					ensure(Event.StartTime >= InContext.GetParameters().StartTime);

					if (Event.StartTime > InContext.GetParameters().EndTime)
					{
						break;
					}

					InContext.Check(Event.StartTime, Event.StartTime, bIgnoreEventDepth ? 0 : Event.Depth, Event);

					if (!InContext.ShouldContinueSearching())
					{
						break;
					}
				}
			}
			else
			{
				// Events are sorted by start time.
				// Find the first event with StartTime >= searched EndTime.
				int32 StartIndex = Algo::LowerBoundBy(Events, InContext.GetParameters().EndTime,
					[](const FFileActivitySharedState::FIoTimingEvent& Event) { return Event.StartTime; });

				// Start at the last event with StartTime < searched EndTime.
				for (int32 Index = StartIndex - 1; Index >= 0; --Index)
				{
					const FFileActivitySharedState::FIoTimingEvent& Event = Events[Index];

					if (bShowOnlyErrors && ((Event.Type & 0xF0) == 0))
					{
						continue;
					}

					if (Event.EndTime <= InContext.GetParameters().StartTime ||
						Event.StartTime >= InContext.GetParameters().EndTime)
					{
						continue;
					}

					InContext.Check(Event.StartTime, Event.EndTime, bIgnoreEventDepth ? 0 : Event.Depth, Event);

					if (!InContext.ShouldContinueSearching())
					{
						break;
					}
				}
			}
		},

		// Found!
		[&InFoundPredicate](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const FFileActivitySharedState::FIoTimingEvent& InEvent)
		{
			InFoundPredicate(InFoundStartTime, InFoundEndTime, InFoundDepth, InEvent);
		});
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FOverviewFileActivityTimingTrack
////////////////////////////////////////////////////////////////////////////////////////////////////

void FOverviewFileActivityTimingTrack::BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context)
{
	const FTimingTrackViewport& Viewport = Context.GetViewport();

	for (const FFileActivitySharedState::FIoTimingEvent& Event : SharedState.AllIoEvents)
	{
		const TraceServices::EFileActivityType ActivityType = static_cast<TraceServices::EFileActivityType>(Event.Type & 0x0F);
		const uint64 EventType = static_cast<uint64>(ActivityType);

		if (ActivityType >= TraceServices::FileActivityType_Count)
		{
			// Ignore "Idle" and "NotClosed" events.
			continue;
		}

		//const double EventEndTime = Event.EndTime; // keep duration of events
		const double EventEndTime = Event.StartTime; // make all 0 duration events

		if (EventEndTime <= Viewport.GetStartTime())
		{
			continue;
		}
		if (Event.StartTime >= Viewport.GetEndTime())
		{
			break;
		}

		const bool bHasFailed = ((Event.Type & 0xF0) != 0);

		if (bShowOnlyErrors && !bHasFailed)
		{
			continue;
		}

		uint32 Color = bHasFailed ? 0xFFAA0000 : GetFileActivityTypeColor(ActivityType);
		if (Event.ActualSize != Event.Size)
		{
			Color = (Color & 0xFF000000) | ((Color & 0xFEFEFE) >> 1);
		}

		Builder.AddEvent(Event.StartTime, EventEndTime, 0, Color,
			[&Event](float Width)
			{
				FString EventName;

				const bool bHasFailed = ((Event.Type & 0xF0) != 0);
				if (bHasFailed)
				{
					EventName += TEXT("Failed ");
				}

				const TraceServices::EFileActivityType ActivityType = static_cast<TraceServices::EFileActivityType>(Event.Type & 0x0F);
				EventName += GetFileActivityTypeName(ActivityType);

				if (Event.ActualSize != Event.Size)
				{
					EventName += TEXT(" [!]");
				}

				const float MinWidth = static_cast<float>(EventName.Len()) * 4.0f + 32.0f;
				if (Width > MinWidth)
				{
					const double Duration = Event.EndTime - Event.StartTime; // actual event duration
					FTimingEventsTrackDrawStateBuilder::AppendDurationToEventName(EventName, Duration);
				}

				return EventName;
			});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TSharedPtr<const ITimingEvent> FOverviewFileActivityTimingTrack::SearchEvent(const FTimingEventSearchParameters& InSearchParameters) const
{
	TSharedPtr<const ITimingEvent> FoundEvent;

	FindIoTimingEvent(InSearchParameters, [this, &FoundEvent](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const FFileActivitySharedState::FIoTimingEvent& InEvent)
	{
		FoundEvent = MakeShared<FTimingEvent>(SharedThis(this), InFoundStartTime, InFoundEndTime, InFoundDepth);
	});

	return FoundEvent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FOverviewFileActivityTimingTrack::BuildContextMenu(FMenuBuilder& InOutMenuBuilder)
{
	InOutMenuBuilder.BeginSection(TEXT("Misc"));
	{
		InOutMenuBuilder.AddMenuEntry(
			LOCTEXT("OverviewTrack_ShowOnlyErrors", "Show Only Errors"),
			LOCTEXT("OverviewTrack_ShowOnlyErrors_Tooltip", "Show only the events with errors"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FOverviewFileActivityTimingTrack::ToggleOnlyErrors),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FOverviewFileActivityTimingTrack::IsOnlyErrorsToggleOn)),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	InOutMenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FDetailedFileActivityTimingTrack
////////////////////////////////////////////////////////////////////////////////////////////////////

void FDetailedFileActivityTimingTrack::BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context)
{
	const FTimingTrackViewport& Viewport = Context.GetViewport();

	// Add IO file activity background events.
	if (bShowBackgroundEvents)
	{
		for (const TSharedPtr<FFileActivitySharedState::FIoFileActivity>& Activity : SharedState.FileActivities)
		{
			if (Activity->EndTime <= Viewport.GetStartTime())
			{
				continue;
			}
			if (Activity->StartTime >= Viewport.GetEndTime())
			{
				break;
			}

			ensure(Activity->StartingDepth < FFileActivitySharedState::MaxLanes);

			Builder.AddEvent(Activity->StartTime, Activity->EndTime, Activity->StartingDepth, 0x55333333,
				[&Activity](float Width)
				{
					FString EventName = Activity->Path;

					const float MinWidth = static_cast<float>(EventName.Len()) * 4.0f + 32.0f;
					if (Width > MinWidth)
					{
						const double Duration = Activity->EndTime - Activity->StartTime;
						FTimingEventsTrackDrawStateBuilder::AppendDurationToEventName(EventName, Duration);
					}

					return EventName;
				});
		}
	}

	// Add IO file activity foreground events.
	for (const FFileActivitySharedState::FIoTimingEvent& Event : SharedState.AllIoEvents)
	{
		if (Event.EndTime <= Viewport.GetStartTime())
		{
			continue;
		}
		if (Event.StartTime >= Viewport.GetEndTime())
		{
			break;
		}

		ensure(Event.Depth < FFileActivitySharedState::MaxLanes);
		const TraceServices::EFileActivityType ActivityType = static_cast<TraceServices::EFileActivityType>(Event.Type & 0x0F);

		const bool bHasFailed = ((Event.Type & 0xF0) != 0);

		if (bShowOnlyErrors && !bHasFailed)
		{
			continue;
		}

		uint32 Color = bHasFailed ? 0xFFAA0000 : GetFileActivityTypeColor(ActivityType);
		if (Event.ActualSize != Event.Size)
		{
			Color = (Color & 0xFF000000) | ((Color & 0xFEFEFE) >> 1);
		}

		Builder.AddEvent(Event.StartTime, Event.EndTime, Event.Depth, Color,
			[&Event, FileActivity= SharedState.FileActivities[Event.FileActivityIndex]](float Width)
			{
				FString EventName;

				const bool bHasFailed = ((Event.Type & 0xF0) != 0);
				if (bHasFailed)
				{
					EventName += TEXT("Failed ");
				}

				const TraceServices::EFileActivityType ActivityType = static_cast<TraceServices::EFileActivityType>(Event.Type & 0x0F);
				EventName += GetFileActivityTypeName(ActivityType);

				if (Event.ActualSize != Event.Size)
				{
					EventName += TEXT(" [!]");
				}

				if (ActivityType >= TraceServices::FileActivityType_Count)
				{
					EventName += " [";
					EventName += FileActivity->Path;
					EventName += "]";
				}

				const float MinWidth = static_cast<float>(EventName.Len()) * 4.0f + 32.0f;
				if (Width > MinWidth)
				{
					const double Duration = Event.EndTime - Event.StartTime;
					FTimingEventsTrackDrawStateBuilder::AppendDurationToEventName(EventName, Duration);
				}

				return EventName;
			});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TSharedPtr<const ITimingEvent> FDetailedFileActivityTimingTrack::SearchEvent(const FTimingEventSearchParameters& InSearchParameters) const
{
	TSharedPtr<const ITimingEvent> FoundEvent;

	FindIoTimingEvent(InSearchParameters, [this, &FoundEvent](double InFoundStartTime, double InFoundEndTime, uint32 InFoundDepth, const FFileActivitySharedState::FIoTimingEvent& InEvent)
	{
		FoundEvent = MakeShared<FTimingEvent>(SharedThis(this), InFoundStartTime, InFoundEndTime, InFoundDepth);
	});

	return FoundEvent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FDetailedFileActivityTimingTrack::BuildContextMenu(FMenuBuilder& InOutMenuBuilder)
{
	InOutMenuBuilder.BeginSection(TEXT("Misc"));
	{
		InOutMenuBuilder.AddMenuEntry(
			LOCTEXT("ActivityTrack_ShowOnlyErrors", "Show Only Errors"),
			LOCTEXT("ActivityTrack_ShowOnlyErrors_Tooltip", "Show only the events with errors"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FDetailedFileActivityTimingTrack::ToggleOnlyErrors),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FDetailedFileActivityTimingTrack::IsOnlyErrorsToggleOn)),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		InOutMenuBuilder.AddMenuEntry(
			LOCTEXT("ActivityTrack_ShowBackgroundEvents", "Show Background Events - O"),
			LOCTEXT("ActivityTrack_ShowBackgroundEvents_Tooltip", "Show background events for file activities."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FDetailedFileActivityTimingTrack::ToggleBackgroundEvents),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FDetailedFileActivityTimingTrack::AreBackgroundEventsVisible)),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	InOutMenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
