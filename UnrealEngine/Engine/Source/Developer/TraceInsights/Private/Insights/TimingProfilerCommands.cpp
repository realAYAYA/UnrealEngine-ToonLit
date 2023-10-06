// Copyright Epic Games, Inc. All Rights Reserved.

#include "Insights/TimingProfilerCommands.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

// Insights
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/TimingProfilerManager.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "FTimingProfilerCommands"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimingProfilerMenuBuilder
////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingProfilerMenuBuilder::AddMenuEntry(FMenuBuilder& MenuBuilder, const TSharedPtr<FUICommandInfo>& UICommandInfo, const FUIAction& UIAction)
{
	MenuBuilder.AddMenuEntry
	(
		UICommandInfo->GetLabel(),
		UICommandInfo->GetDescription(),
		UICommandInfo->GetIcon(),
		UIAction,
		NAME_None,
		UICommandInfo->GetUserInterfaceType()
	);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimingProfilerCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingProfilerCommands::FTimingProfilerCommands()
: TCommands<FTimingProfilerCommands>(
	TEXT("TimingProfilerCommands"),
	NSLOCTEXT("Contexts", "TimingProfilerCommands", "Insights - Timing Insights"),
	NAME_None,
	FInsightsStyle::GetStyleSetName())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingProfilerCommands::~FTimingProfilerCommands()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// UI_COMMAND takes long for the compiler to optimize
UE_DISABLE_OPTIMIZATION_SHIP
void FTimingProfilerCommands::RegisterCommands()
{
	UI_COMMAND(ToggleFramesTrackVisibility, "Frames", "Toggles the visibility of the Frames track.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleTimingViewVisibility, "Timing", "Toggles the visibility of the main Timing view.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleTimersViewVisibility, "Timers", "Toggles the visibility of the Timers view.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleCallersTreeViewVisibility, "Callers", "Toggles the visibility of the Callers tree view.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleCalleesTreeViewVisibility, "Callees", "Toggles the visibility of the Callees tree view.", EUserInterfaceActionType::ToggleButton,FInputChord());
	UI_COMMAND(ToggleStatsCountersViewVisibility, "Counters", "Toggles the visibility of the Counters view.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleLogViewVisibility, "Log", "Toggles the visibility of the Log view.", EUserInterfaceActionType::ToggleButton,FInputChord());
}
UE_ENABLE_OPTIMIZATION_SHIP

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimingViewCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingViewCommands::FTimingViewCommands()
: TCommands<FTimingViewCommands>(
	TEXT("TimingViewCommands"),
	NSLOCTEXT("Contexts", "TimingViewCommands", "Insights - Timing View"),
	NAME_None,
	FInsightsStyle::GetStyleSetName())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingViewCommands::~FTimingViewCommands()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// UI_COMMAND takes long for the compiler to optimize
UE_DISABLE_OPTIMIZATION_SHIP
void FTimingViewCommands::RegisterCommands()
{
	UI_COMMAND(AutoHideEmptyTracks,
		"Auto Hide Empty Tracks",
		"Auto hide empty tracks (ex.: ones without timing events in the current viewport).\nThis option is persistent to the UnrealInsightsSettings.ini file.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::V));

	UI_COMMAND(PanningOnScreenEdges,
		"Allow Panning on Screen Edges",
		"If enabled, the panning is allowed to continue when the mouse cursor reaches the edges of the screen.\nThis option is persistent to the UnrealInsightsSettings.ini file.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord());

	UI_COMMAND(ToggleCompactMode,
		"Compact Mode",
		"Toggle compact mode for supporting tracks.\n(ex.: the timing tracks will be displayed with reduced height)",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::C));

	UI_COMMAND(ShowMainGraphTrack,
		"Graph Track",
		"Shows/hides the main Graph track.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::G));

	UI_COMMAND(QuickFind,
		"Quick Find...",
		"Quick find or filter events in the timing view.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::F));
}
UE_ENABLE_OPTIMIZATION_SHIP

////////////////////////////////////////////////////////////////////////////////////////////////////
// Toggle Commands
////////////////////////////////////////////////////////////////////////////////////////////////////

#define IMPLEMENT_TOGGLE_COMMAND(CmdName, IsEnabled, SetIsEnabled) \
	\
	void FTimingProfilerActionManager::Map_##CmdName##_Global()\
	{\
		This->CommandList->MapAction(This->GetCommands().CmdName, CmdName##_Custom());\
	}\
	\
	const FUIAction FTimingProfilerActionManager::CmdName##_Custom() \
	{\
		FUIAction UIAction;\
		UIAction.ExecuteAction = FExecuteAction::CreateRaw(this, &FTimingProfilerActionManager::CmdName##_Execute);\
		UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw(this, &FTimingProfilerActionManager::CmdName##_CanExecute);\
		UIAction.GetActionCheckState = FGetActionCheckState::CreateRaw(this, &FTimingProfilerActionManager::CmdName##_GetCheckState);\
		return UIAction;\
	}\
	\
	void FTimingProfilerActionManager::CmdName##_Execute()\
	{\
		const bool b##IsEnabled = !This->IsEnabled();\
		This->SetIsEnabled(b##IsEnabled);\
	}\
	\
	bool FTimingProfilerActionManager::CmdName##_CanExecute() const\
	{\
		return FInsightsManager::Get()->GetSession().IsValid();\
	}\
	\
	ECheckBoxState FTimingProfilerActionManager::CmdName##_GetCheckState() const\
	{\
		const bool b##IsEnabled = This->IsEnabled();\
		return b##IsEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;\
	}

IMPLEMENT_TOGGLE_COMMAND(ToggleFramesTrackVisibility, IsFramesTrackVisible, ShowHideFramesTrack)
IMPLEMENT_TOGGLE_COMMAND(ToggleTimingViewVisibility, IsTimingViewVisible, ShowHideTimingView)
IMPLEMENT_TOGGLE_COMMAND(ToggleTimersViewVisibility, IsTimersViewVisible, ShowHideTimersView)
IMPLEMENT_TOGGLE_COMMAND(ToggleCallersTreeViewVisibility, IsCallersTreeViewVisible, ShowHideCallersTreeView)
IMPLEMENT_TOGGLE_COMMAND(ToggleCalleesTreeViewVisibility, IsCalleesTreeViewVisible, ShowHideCalleesTreeView)
IMPLEMENT_TOGGLE_COMMAND(ToggleStatsCountersViewVisibility, IsStatsCountersViewVisible, ShowHideStatsCountersView)
IMPLEMENT_TOGGLE_COMMAND(ToggleLogViewVisibility, IsLogViewVisible, ShowHideLogView)

#undef IMPLEMENT_TOGGLE_COMMAND

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
