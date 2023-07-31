// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExampleAssetEditor.h"
#include "ExampleAssetEditorCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"
#include "UExampleAssetEditor.h"

static const FName ExampleAssetEditorTabName("ExampleAssetEditor");

#define LOCTEXT_NAMESPACE "FExampleAssetEditorModule"

void FExampleAssetEditorModule::StartupModule()
{
	FExampleAssetEditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FExampleAssetEditorCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FExampleAssetEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FExampleAssetEditorModule::RegisterMenus));
}

void FExampleAssetEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	PluginCommands.Reset();
	FExampleAssetEditorCommands::Unregister();
}

void FExampleAssetEditorModule::PluginButtonClicked()
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	UAssetEditor* AssetEditor = NewObject<UAssetEditor>(AssetEditorSubsystem, UExampleAssetEditor::StaticClass());
	AssetEditor->Initialize();
}

void FExampleAssetEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("ExperimentalTabSpawners");
			Section.AddMenuEntryWithCommandList(FExampleAssetEditorCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FExampleAssetEditorModule, ExampleAssetEditor)