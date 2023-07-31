// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZoneGraphEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "ZoneShapeComponent.h"
#include "ZoneShapeComponentDetails.h"
#include "ZoneShapeComponentVisualizer.h"
#include "ZoneLaneProfileRefDetails.h"
#include "ZoneLaneProfileDetails.h"
#include "ZoneLaneDescDetails.h"
#include "ZoneGraphTagDetails.h"
#include "ZoneGraphTagMaskDetails.h"
#include "ZoneGraphTagInfoDetails.h"
#include "ZoneGraphTagFilterDetails.h"
#include "ZoneGraphTessellationSettingsDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "ToolMenus.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"
#include "ZoneGraphDelegates.h"
#include "ZoneGraphEditorStyle.h"


#define LOCTEXT_NAMESPACE "ZoneGraphEditor"

IMPLEMENT_MODULE(FZoneGraphEditorModule, ZoneGraphEditor)

namespace UE { namespace ZoneGraph { namespace Editor {

TCustomShowFlag<> ShowZoneGraph(TEXT("ZoneGraph"), false /*DefaultEnabled*/, SFG_Developer, LOCTEXT("ShowZoneGraph", "Zone Graph"));

} } } // UE::ZoneGraphEditor::Editor


class FZoneGraphEditorCommands : public TCommands<FZoneGraphEditorCommands>
{
public:

	FZoneGraphEditorCommands()
		: TCommands<FZoneGraphEditorCommands>(TEXT("ZoneGraphEditorCommands"), NSLOCTEXT("Contexts", "FZoneGraphEditorModule", "Zone Graph Editor Plugin"), FName(), FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override
	{
		UI_COMMAND(BuildZoneGraph, "Build ZoneGraph", "Builds ZoneGraph data", EUserInterfaceActionType::Button, FInputChord());
	}

public:
	TSharedPtr<FUICommandInfo> BuildZoneGraph;
};


void FZoneGraphEditorModule::StartupModule()
{
	FZoneGraphEditorCommands::Register();

	FZoneGraphEditorStyle::Initialize();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FZoneGraphEditorCommands::Get().BuildZoneGraph,
		FExecuteAction::CreateRaw(this, &FZoneGraphEditorModule::OnBuildZoneGraph),
		FCanExecuteAction());

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(TEXT("ZoneShapeComponent"), FOnGetDetailCustomizationInstance::CreateStatic(&FZoneShapeComponentDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ZoneLaneProfileRef"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FZoneLaneProfileRefDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ZoneLaneProfile"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FZoneLaneProfileDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ZoneLaneDesc"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FZoneLaneDescDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ZoneGraphTag"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FZoneGraphTagDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ZoneGraphTagMask"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FZoneGraphTagMaskDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ZoneGraphTagInfo"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FZoneGraphTagInfoDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ZoneGraphTagFilter"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FZoneGraphTagFilterDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("ZoneGraphTessellationSettings"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FZoneGraphTessellationSettingsDetails::MakeInstance));

	RegisterComponentVisualizer(UZoneShapeComponent::StaticClass()->GetFName(), MakeShareable(new FZoneShapeComponentVisualizer));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FZoneGraphEditorModule::RegisterMenus));
}

void FZoneGraphEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	PluginCommands.Reset();
	FZoneGraphEditorCommands::Unregister();

	// Unregister the data asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
	}

	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("ZoneShapeComponent"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ZoneLaneProfileRef"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ZoneLaneProfile"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ZoneLane"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ZoneGraphTag"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ZoneGraphTagMask"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ZoneGraphTagInfo"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ZoneGraphTagFilter"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("ZoneGraphTessellationSettings"));
	}

	if (GEngine)
	{
		// Iterate over all class names we registered for
		for (FName ClassName : RegisteredComponentClassNames)
		{
			GUnrealEd->UnregisterComponentVisualizer(ClassName);
		}
	}

	ItemDataAssetTypeActions.Empty();
}

void FZoneGraphEditorModule::RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer)
{
	if (GUnrealEd != NULL)
	{
		GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
	}

	RegisteredComponentClassNames.Add(ComponentClassName);

	if (Visualizer.IsValid())
	{
		Visualizer->OnRegister();
	}
}

void FZoneGraphEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* BuildMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Build"))
	{
		FToolMenuSection& Section = BuildMenu->FindOrAddSection("LevelEditorNavigation");
		Section.AddMenuEntryWithCommandList(FZoneGraphEditorCommands::Get().BuildZoneGraph, PluginCommands);
	}
}

void FZoneGraphEditorModule::OnBuildZoneGraph()
{
#if WITH_EDITOR
	UE::ZoneGraphDelegates::OnZoneGraphRequestRebuild.Broadcast();
#endif
}


#undef LOCTEXT_NAMESPACE
