// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonUIEditorModule.h"
#include "CommonUIPrivate.h"
#include "CommonVideoPlayerCustomization.h"

#include "GameplayTagsEditorModule.h"
#include "CommonAssetTypeActions_GenericInputActionDataTable.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UITag.h"

IMPLEMENT_MODULE(FCommonUIEditorModule, CommonUIEditor);

void FCommonUIEditorModule::StartupModule() 
{
	IAssetTools& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TSharedPtr<FCommonAssetTypeActions_GenericInputActionDataTable> InputDataTableAction = MakeShared<FCommonAssetTypeActions_GenericInputActionDataTable>();
	ItemDataAssetTypeActions.Add(InputDataTableAction);
	AssetToolsModule.RegisterAssetTypeActions(InputDataTableAction.ToSharedRef());

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomClassLayout(
		TEXT("CommonVideoPlayer"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FCommonVideoPlayerCustomization::MakeInstance));
	
	PropertyModule.RegisterCustomPropertyTypeLayout(FUITag::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagCustomizationPublic::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FUIActionTag::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagCustomizationPublic::MakeInstance));
}

void FCommonUIEditorModule::ShutdownModule()
{
	if (!UObjectInitialized())
	{
		return;
	}

	// Unregister the CommonUI item data asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (auto& AssetTypeAction : ItemDataAssetTypeActions)
		{
			if (AssetTypeAction.IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
			}
		}
	}
	ItemDataAssetTypeActions.Empty();

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("CommonVideoPlayer"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("UITag"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("UIActionTag"));
	}
}
