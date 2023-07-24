// Copyright Epic Games, Inc. All Rights Reserved.

#include "Insights/InsightsCommands.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManagerGeneric.h"
#include "Misc/Paths.h"

// Insights
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "FInsightsCommands"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FInsightsCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

FInsightsCommands::FInsightsCommands()
: TCommands<FInsightsCommands>(
	TEXT("InsightsCommands"), // Context name for fast lookup
	NSLOCTEXT("Contexts", "InsightsCommands", "Insights"), // Localized context name for displaying
	NAME_None, // Parent
	FInsightsStyle::GetStyleSetName()) // Icon Style Set
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// UI_COMMAND takes long for the compile to optimize
UE_DISABLE_OPTIMIZATION_SHIP
void FInsightsCommands::RegisterCommands()
{
	UI_COMMAND(InsightsManager_Load,
		"Load...",
		"Loads profiler data from a trace file.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::L));

	UI_COMMAND(ToggleDebugInfo,
		"Debug",
		"Toggles the display of debug info.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EModifierKey::Control, EKeys::D));

	UI_COMMAND(OpenSettings,
		"Settings",
		"Opens the Unreal Insights settings.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::O));
}
UE_ENABLE_OPTIMIZATION_SHIP

////////////////////////////////////////////////////////////////////////////////////////////////////
// ToggleDebugInfo
////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsActionManager::Map_ToggleDebugInfo_Global()
{
	This->CommandList->MapAction(This->GetCommands().ToggleDebugInfo, ToggleDebugInfo_Custom());
}

const FUIAction FInsightsActionManager::ToggleDebugInfo_Custom()
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw(this, &FInsightsActionManager::ToggleDebugInfo_Execute);
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw(this, &FInsightsActionManager::ToggleDebugInfo_CanExecute);
	UIAction.GetActionCheckState = FGetActionCheckState::CreateRaw(this, &FInsightsActionManager::ToggleDebugInfo_GetCheckState);
	return UIAction;
}

void FInsightsActionManager::ToggleDebugInfo_Execute()
{
	const bool bIsDebugInfoEnabled = !This->IsDebugInfoEnabled();
	This->SetDebugInfo(bIsDebugInfoEnabled);
}

bool FInsightsActionManager::ToggleDebugInfo_CanExecute() const
{
	return FInsightsManager::Get()->GetSession().IsValid();
}

ECheckBoxState FInsightsActionManager::ToggleDebugInfo_GetCheckState() const
{
	const bool bIsDebugInfoEnabled = This->IsDebugInfoEnabled();
	return bIsDebugInfoEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// InsightsManager_Load
////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsActionManager::Map_InsightsManager_Load()
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw(this, &FInsightsActionManager::InsightsManager_Load_Execute);
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw(this, &FInsightsActionManager::InsightsManager_Load_CanExecute);

	This->CommandList->MapAction(This->GetCommands().InsightsManager_Load, UIAction);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsActionManager::InsightsManager_Load_Execute()
{
	//const FString ProfilingDirectory(FPaths::ConvertRelativePathToFull(*FPaths::ProfilingDir()));
	const FString ProfilingDirectory(This->GetStoreDir());

	TArray<FString> OutFiles;

	bool bOpened = false;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != nullptr)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->OpenFileDialog
		(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("LoadTrace_FileDesc", "Open trace file...").ToString(),
			ProfilingDirectory,
			TEXT(""),
			LOCTEXT("LoadTrace_FileFilter", "Trace files (*.utrace)|*.utrace|All files (*.*)|*.*").ToString(),
			EFileDialogFlags::None,
			OutFiles
		);
	}

	if (bOpened == true)
	{
		if (OutFiles.Num() == 1)
		{
			This->LoadTraceFile(OutFiles[0]);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsActionManager::InsightsManager_Load_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// OpenSettings
////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsActionManager::Map_OpenSettings_Global()
{
	This->CommandList->MapAction(This->GetCommands().OpenSettings, OpenSettings_Custom());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FUIAction FInsightsActionManager::OpenSettings_Custom()
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw(this, &FInsightsActionManager::OpenSettings_Execute);
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw(this, &FInsightsActionManager::OpenSettings_CanExecute);
	return UIAction;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsActionManager::OpenSettings_Execute()
{
	This->OpenSettings();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsActionManager::OpenSettings_CanExecute() const
{
	return !This->Settings.IsEditing();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
