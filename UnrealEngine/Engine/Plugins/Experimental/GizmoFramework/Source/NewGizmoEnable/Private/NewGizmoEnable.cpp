// Copyright Epic Games, Inc. All Rights Reserved.

#include "NewGizmoEnable.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "HAL/IConsoleManager.h"
#include "LevelEditor.h"
#include "Serialization/MemoryLayout.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "NewGizmoEnableCommands.h"
#include "UnrealEdGlobals.h"
#include "GizmoEdMode.h"

static const FName NewGizmoEnableTabName("NewGizmoEnable");
static FAutoConsoleCommand ShowGizmoOptions(TEXT("Editor.ShowGizmoOptions"), TEXT("Shows the new gizmo mode in menus"), FConsoleCommandDelegate::CreateStatic(&FNewGizmoEnableModule::ShowGizmoOptions));
bool FNewGizmoEnableModule::bTestingModeEnabled = false;
#define LOCTEXT_NAMESPACE "FNewGizmoEnableModule"

void FNewGizmoEnableModule::StartupModule()
{
	FNewGizmoEnableCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FNewGizmoEnableCommands::Get().ToggleNewGizmos,
		FExecuteAction::CreateRaw(this, &FNewGizmoEnableModule::ToggleNewGizmosActive),
		FCanExecuteAction(),
		FGetActionCheckState::CreateRaw(this, &FNewGizmoEnableModule::AreNewGizmosActive),
		FIsActionButtonVisible::CreateRaw(this, &FNewGizmoEnableModule::CanToggleNewGizmos));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNewGizmoEnableModule::RegisterMenus));
}

void FNewGizmoEnableModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	PluginCommands.Reset();
	FNewGizmoEnableCommands::Unregister();
}

void FNewGizmoEnableModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
}

void FNewGizmoEnableModule::ToggleNewGizmosActive()
{
	if ( GLevelEditorModeTools().IsModeActive(GetDefault<UGizmoEdMode>()->GetID()) )
	{
		GLevelEditorModeTools().DeactivateMode(GetDefault<UGizmoEdMode>()->GetID());
	}
	else
	{
		GLevelEditorModeTools().ActivateMode(GetDefault<UGizmoEdMode>()->GetID());
	}
}

ECheckBoxState FNewGizmoEnableModule::AreNewGizmosActive()
{
	return GLevelEditorModeTools().IsModeActive(GetDefault<UGizmoEdMode>()->GetID()) ? ECheckBoxState::Checked :
	                                                                                   ECheckBoxState::Unchecked;
}

bool FNewGizmoEnableModule::CanToggleNewGizmos()
{
	return FNewGizmoEnableModule::bTestingModeEnabled;
}

void FNewGizmoEnableModule::ShowGizmoOptions()
{
	FNewGizmoEnableModule::bTestingModeEnabled = !FNewGizmoEnableModule::bTestingModeEnabled;
}

void FNewGizmoEnableModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("ExperimentalTabSpawners");
			Section.AddMenuEntryWithCommandList(FNewGizmoEnableCommands::Get().ToggleNewGizmos , PluginCommands);
		}
	}
	// Append to level editor module so that shortcuts are accessible in level editor
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNewGizmoEnableModule, NewGizmoEnable)
