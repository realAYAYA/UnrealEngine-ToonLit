// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelAssetEditor.h"
#include "LevelAssetEditorCommands.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"
#include "ULevelAssetEditor.h"
#include "Settings/LevelEditorMiscSettings.h"

static const FName LevelAssetEditorTabName("LevelAssetEditor");

#define LOCTEXT_NAMESPACE "FLevelAssetEditorModule"

void FLevelAssetEditorModule::StartupModule()
{
	FLevelAssetEditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FLevelAssetEditorCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FLevelAssetEditorModule::PluginButtonClicked),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateRaw(this, &FLevelAssetEditorModule::IsEnabled));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLevelAssetEditorModule::RegisterMenus));
}

void FLevelAssetEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	PluginCommands.Reset();
	FLevelAssetEditorCommands::Unregister();
}

void FLevelAssetEditorModule::PluginButtonClicked()
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	UAssetEditor* AssetEditor = NewObject<UAssetEditor>(AssetEditorSubsystem, ULevelAssetEditor::StaticClass());
	AssetEditor->Initialize();
}

void FLevelAssetEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("ExperimentalTabSpawners");
			Section.AddMenuEntryWithCommandList(FLevelAssetEditorCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}
}

bool FLevelAssetEditorModule::IsEnabled() const
{
	return GetDefault<ULevelEditorMiscSettings>()->bEnableExperimentalLevelEditor;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLevelAssetEditorModule, LevelAssetEditor)