// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectsEditorModule.h"

#include "PropertyEditorModule.h"
#include "SmartObjectComponent.h"
#include "SmartObjectComponentVisualizer.h"
#include "SmartObjectAssetTypeActions.h"
#include "SmartObjectEditorStyle.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Customizations/SmartObjectSlotDefinitionDetails.h"
#include "Customizations/SmartObjectSlotReferenceDetails.h"
#include "Customizations/SmartObjectDefinitionDetails.h"

#define LOCTEXT_NAMESPACE "SmartObjects"

class FSmartObjectsEditorModule : public ISmartObjectsEditorModule
{
protected:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer);

private:
	TArray<FName> RegisteredComponentClassNames;
	TArray<TSharedPtr<FAssetTypeActions_Base>> AssetTypeActions;
};

void FSmartObjectsEditorModule::StartupModule()
{
	FSmartObjectEditorStyle::Get();
	
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Create or find existing category
	const EAssetTypeCategories::Type Category = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("AI")), LOCTEXT("AIAssetCategory", "Artificial Intelligence"));

	// Create action to associate USmartObjectDefinition to the SmartObjectAssetEditor
	const TSharedPtr<FAssetTypeActions_SmartObject> AssetActions = MakeShareable(new FAssetTypeActions_SmartObject(Category));
	AssetTypeActions.Add(AssetActions);

	// Register action
	AssetTools.RegisterAssetTypeActions(AssetActions.ToSharedRef());

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("SmartObjectSlotDefinition", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSmartObjectSlotDefinitionDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("SmartObjectSlotReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSmartObjectSlotReferenceDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("SmartObjectDefinition", FOnGetDetailCustomizationInstance::CreateStatic(&FSmartObjectDefinitionDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();

	// Register component visualizer for SmartObjectComponent
	RegisterComponentVisualizer(USmartObjectComponent::StaticClass()->GetFName(), MakeShareable(new FSmartObjectComponentVisualizer));
}

void FSmartObjectsEditorModule::ShutdownModule()
{
	// Unregister all asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (TSharedPtr<FAssetTypeActions_Base> Action : AssetTypeActions)
		{
			if (Action.IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(Action.ToSharedRef());
			}
		}
	}
	AssetTypeActions.Empty();

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("SmartObjectSlotDefinition");
		PropertyModule.UnregisterCustomPropertyTypeLayout("SmartObjectSlotReference");
		PropertyModule.UnregisterCustomClassLayout("SmartObjectDefinition");
	}
	
	// Unregister all component visualizers
	if (GEngine)
	{
		for (const FName ClassName : RegisteredComponentClassNames)
		{
			GUnrealEd->UnregisterComponentVisualizer(ClassName);
		}
	}

	FSmartObjectEditorStyle::Shutdown();
}

void FSmartObjectsEditorModule::RegisterComponentVisualizer(const FName ComponentClassName, const TSharedPtr<FComponentVisualizer> Visualizer)
{
	if (GUnrealEd != nullptr && Visualizer.IsValid())
	{
		GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
		Visualizer->OnRegister();

		RegisteredComponentClassNames.Add(ComponentClassName);
	}
}

IMPLEMENT_MODULE(FSmartObjectsEditorModule, SmartObjectsEditorModule)

#undef LOCTEXT_NAMESPACE
