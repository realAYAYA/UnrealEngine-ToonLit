// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorModule.h"

#include "DMXControlConsoleEditorManager.h"
#include "DMXEditorModule.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Views/SDMXControlConsoleEditorView.h"

#include "AssetToolsModule.h"
#include "LevelEditor.h"
#include "ToolMenu.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/Docking/SDockTab.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorModule"

const FName FDMXControlConsoleEditorModule::ControlConsoleTabName("DMXControlConsoleTabName");

void FDMXControlConsoleEditorModule::StartupModule()
{
	FDMXControlConsoleEditorCommands::Register();
	RegisterLevelEditorCommands();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ControlConsoleTabName, 
		FOnSpawnTab::CreateStatic(&FDMXControlConsoleEditorModule::OnSpawnControlConsoleTab))
		.SetDisplayName(LOCTEXT("DMXControlConsoleTabTitle", "DMX Control Console"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.TabIcon"));

	FCoreDelegates::OnPostEngineInit.AddStatic(&FDMXControlConsoleEditorModule::RegisterDMXMenuExtender);
}

void FDMXControlConsoleEditorModule::ShutdownModule()
{
}

void FDMXControlConsoleEditorModule::OpenControlConsole()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ControlConsoleTabName);
}

void FDMXControlConsoleEditorModule::RegisterLevelEditorCommands()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	const TSharedRef<FUICommandList> CommandList = LevelEditorModule.GetGlobalLevelEditorActions();

	CommandList->MapAction(
		FDMXControlConsoleEditorCommands::Get().OpenControlConsole,
		FExecuteAction::CreateStatic(&FDMXControlConsoleEditorModule::OpenControlConsole)
	);
}

void FDMXControlConsoleEditorModule::RegisterDMXMenuExtender()
{
	FDMXEditorModule& DMXEditorModule = FModuleManager::GetModuleChecked<FDMXEditorModule>(TEXT("DMXEditor"));
	const TSharedPtr<FExtender> LevelEditorToolbarDMXMenuExtender = DMXEditorModule.GetLevelEditorToolbarDMXMenuExtender();
	
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedRef<FUICommandList> CommandList = LevelEditorModule.GetGlobalLevelEditorActions();

	LevelEditorToolbarDMXMenuExtender->AddMenuExtension("OpenActivityMonitor", EExtensionHook::Position::After, CommandList, FMenuExtensionDelegate::CreateStatic(&FDMXControlConsoleEditorModule::ExtendDMXMenu));
}

void FDMXControlConsoleEditorModule::ExtendDMXMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(FDMXControlConsoleEditorCommands::Get().OpenControlConsole,
		NAME_None,
		LOCTEXT("DMXControlConsoleMenuLabel", "Control Console"),
		LOCTEXT("DMXControlConsoleMenuTooltip", "Opens a small console that can send DMX locally or over the network"),
		FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.TabIcon")
	);
}

TSharedRef<SDockTab> FDMXControlConsoleEditorModule::OnSpawnControlConsoleTab(const FSpawnTabArgs& InSpawnTabArgs)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DMXControlConsoleTitle", "DMX ControlConsole"))
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SDMXControlConsoleEditorView)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDMXControlConsoleEditorModule, DMXControlConsoleEditor)
