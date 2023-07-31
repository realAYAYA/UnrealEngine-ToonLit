// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorModule.h"

#include "AssetTypeActions/PCGGraphAssetTypeActions.h"
#include "AssetTypeActions/PCGSettingsAssetTypeActions.h"
#include "PCGComponentDetails.h"
#include "PCGEditorCommands.h"
#include "PCGEditorGraphNodeFactory.h"
#include "PCGEditorSettings.h"
#include "PCGEditorStyle.h"
#include "PCGGraphDetails.h"
#include "PCGSubsystem.h"
#include "PCGVolumeDetails.h"
#include "PCGVolumeFactory.h"
#include "PCGWorldActor.h"

#include "EdGraphUtilities.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Toolkits/IToolkit.h"

#define LOCTEXT_NAMESPACE "FPCGEditorModule"

EAssetTypeCategories::Type FPCGEditorModule::PCGAssetCategory;

void FPCGEditorModule::StartupModule()
{
	RegisterDetailsCustomizations();
	RegisterAssetTypeActions();
	RegisterMenuExtensions();
	RegisterSettings();

	FPCGEditorCommands::Register();
	FPCGEditorStyle::Register();

	GraphNodeFactory = MakeShareable(new FPCGEditorGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphNodeFactory);

	if (GEditor)
	{
		GEditor->ActorFactories.Add(NewObject<UPCGVolumeFactory>());
	}
}

void FPCGEditorModule::ShutdownModule()
{
	UnregisterSettings();
	UnregisterAssetTypeActions();
	UnregisterDetailsCustomizations();
	UnregisterMenuExtensions();

	FPCGEditorCommands::Unregister();
	FPCGEditorStyle::Unregister();

	FEdGraphUtilities::UnregisterVisualNodeFactory(GraphNodeFactory);

	if (GEditor)
	{
		GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return ActorFactory->IsA<UPCGVolumeFactory>(); });
	}
}

bool FPCGEditorModule::SupportsDynamicReloading()
{
	return true;
}

void FPCGEditorModule::RegisterDetailsCustomizations()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.RegisterCustomClassLayout("PCGComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FPCGComponentDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout("PCGGraph", FOnGetDetailCustomizationInstance::CreateStatic(&FPCGGraphDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout("PCGVolume", FOnGetDetailCustomizationInstance::CreateStatic(&FPCGVolumeDetails::MakeInstance));
}

void FPCGEditorModule::UnregisterDetailsCustomizations()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("PCGComponent");
		PropertyModule.UnregisterCustomClassLayout("PCGGraph");
		PropertyModule.UnregisterCustomClassLayout("PCGVolume");
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

void FPCGEditorModule::RegisterAssetTypeActions()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	PCGAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("PCG")), LOCTEXT("PCGAssetCategory", "PCG"));

	RegisteredAssetTypeActions.Emplace(MakeShareable(new FPCGGraphAssetTypeActions()));
	RegisteredAssetTypeActions.Emplace(MakeShareable(new FPCGSettingsAssetTypeActions()));

	for (auto Action : RegisteredAssetTypeActions)
	{
		AssetTools.RegisterAssetTypeActions(Action);
	}
}

void FPCGEditorModule::UnregisterAssetTypeActions()
{
	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

	if (!AssetToolsModule)
	{
		return;
	}

	IAssetTools& AssetTools = AssetToolsModule->Get();

	for (auto Action : RegisteredAssetTypeActions)
	{
		AssetTools.UnregisterAssetTypeActions(Action);
	}
}

void FPCGEditorModule::RegisterMenuExtensions()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	{
		TSharedPtr<FExtender> NewMenuExtender = MakeShareable(new FExtender);
		NewMenuExtender->AddMenuExtension("LevelEditor",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FPCGEditorModule::AddMenuEntry));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(NewMenuExtender);
	}
}

void FPCGEditorModule::UnregisterMenuExtensions()
{
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();
}

void FPCGEditorModule::AddMenuEntry(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("PCGMenu", TAttribute<FText>(FText::FromString("PCG Tools")));

	MenuBuilder.AddSubMenu(
		LOCTEXT("PCGSubMenu", "PCG Framework"),
		LOCTEXT("PCGSubMenu_Tooltip", "PCG Framework related functionality"),
		FNewMenuDelegate::CreateRaw(this, &FPCGEditorModule::PopulateMenuActions));
	
	MenuBuilder.EndSection();
}

void FPCGEditorModule::PopulateMenuActions(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeletePCGPartitionActors", "Delete all PCG partition actors"),
		LOCTEXT("DeletePCGPartitionActors_Tooltip", "Deletes all PCG partition actors in the current world"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UWorld* World = GEditor->GetEditorWorldContext().World())
					{
						World->GetSubsystem<UPCGSubsystem>()->DeletePartitionActors(/*bOnlyDeleteUnused=*/false);
					}
				}
			})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteUnusedPCGPartitionActors", "Delete all unused PCG partition actors"),
		LOCTEXT("DeleteUnusedPCGPartitionActors_Tooltip", "Deletes all PCG partition actors in the current world that doesn't intersect with any PCG Component."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UWorld* World = GEditor->GetEditorWorldContext().World())
					{
						World->GetSubsystem<UPCGSubsystem>()->DeletePartitionActors(/*bOnlyDeleteUnused=*/true);
					}
				}
			})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeletePCGWorldActor", "Delete PCG World Actor"),
		LOCTEXT("DeletePCGWorldActor_Tooltip", "Deletes the PCG World Actor"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UWorld* World = GEditor->GetEditorWorldContext().World())
					{
						if (APCGWorldActor* PCGWorldActor = World->GetSubsystem<UPCGSubsystem>()->GetPCGWorldActor())
						{
							World->GetSubsystem<UPCGSubsystem>()->DestroyPCGWorldActor();
						}
					}
				}
			})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BuildLandscapeCache", "Build Landscape Data Cache"),
		LOCTEXT("BuildLandscapeCache_Tooltip", "Caches the landscape data in the PCG World Actor"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UWorld* World = GEditor->GetEditorWorldContext().World())
					{
						if (UPCGSubsystem* Subsystem = World->GetSubsystem<UPCGSubsystem>())
						{
							Subsystem->BuildLandscapeCache();
						}
					}
				}
			})),
		NAME_None);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ClearLandscapeCache", "Clear Landscape Data Cache"),
		LOCTEXT("ClearLandscapeCache_Tooltip", "Clears the landscape data cache in the PCG World Actor"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]() {
				if (GEditor)
				{
					if (UWorld* World = GEditor->GetEditorWorldContext().World())
					{
						if (UPCGSubsystem* Subsystem = World->GetSubsystem<UPCGSubsystem>())
						{
							Subsystem->ClearLandscapeCache();
						}
					}
				}
				})),
		NAME_None);
}

void FPCGEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "ContentEditors", "PCGEditor",
			LOCTEXT("PCGEditorSettingsName", "PCG Editor"),
			LOCTEXT("PCGEditorSettingsDescription", "Configure the look and feel of the PCG Editor."),
			GetMutableDefault<UPCGEditorSettings>());
	}
}

void FPCGEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "ContentEditors", "PCGEditor");
	}
}

IMPLEMENT_MODULE(FPCGEditorModule, PCGEditor);

DEFINE_LOG_CATEGORY(LogPCGEditor);

#undef LOCTEXT_NAMESPACE
