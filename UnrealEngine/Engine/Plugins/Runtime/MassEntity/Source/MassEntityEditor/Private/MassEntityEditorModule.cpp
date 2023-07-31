// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityEditorModule.h"
#include "AIGraphTypes.h" // Class cache
#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "IAssetTools.h"
#include "MassEditorStyle.h"
#include "MassProcessor.h"
#include "Modules/ModuleManager.h"


#define LOCTEXT_NAMESPACE "MassEntityEditor"

IMPLEMENT_MODULE(FMassEntityEditorModule, MassEntityEditor)

void FMassEntityEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FMassEntityEditorStyle::Initialize();

	// Do register asset types here
	//	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	//	ItemDataAssetTypeActions.Add(MakeShareable(new FAssetTypeActions_MassThingy));
	//	AssetTools.RegisterAssetTypeActions(ItemDataAssetTypeActions.Last().ToSharedRef());

	// Register the details customizers
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	//PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeVariableDesc", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeVariableDescDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FMassEntityEditorModule::ShutdownModule()
{
	ProcessorClassCache.Reset();
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	FMassEntityEditorStyle::Shutdown();

	// Unregister the data asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (int i = 0; i < ItemDataAssetTypeActions.Num(); i++)
		{
			if (ItemDataAssetTypeActions[i].IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(ItemDataAssetTypeActions[i].ToSharedRef());
			}
		}
	}
	ItemDataAssetTypeActions.Empty();

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		//PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeVariableDesc");
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}
#undef LOCTEXT_NAMESPACE
