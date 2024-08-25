// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectsEditorModule.h"

#include "EditorBuildUtils.h"
#include "PropertyEditorModule.h"
#include "SmartObjectComponent.h"
#include "SmartObjectComponentVisualizer.h"
#include "SmartObjectEditorStyle.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Customizations/SmartObjectSlotDefinitionDetails.h"
#include "Customizations/SmartObjectDefinitionDataProxyDetails.h"
#include "Customizations/SmartObjectSlotReferenceDetails.h"
#include "Customizations/SmartObjectDefinitionDetails.h"
#include "Customizations/SmartObjectDefinitionReferenceDetails.h"
#include "WorldPartitionSmartObjectCollectionBuilder.h"

#define LOCTEXT_NAMESPACE "SmartObjects"

namespace UE::SmartObject
{
	FName EditorBuildType(TEXT("SmartObjectCollections"));
}

class FSmartObjectsEditorModule : public ISmartObjectsEditorModule
{
protected:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer);

private:
	TArray<FName> RegisteredComponentClassNames;
};

void FSmartObjectsEditorModule::StartupModule()
{
	FSmartObjectEditorStyle::Get();
	
	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("SmartObjectSlotDefinition", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSmartObjectSlotDefinitionDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("SmartObjectDefinitionDataProxy", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSmartObjectDefinitionDataProxyDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("SmartObjectSlotReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSmartObjectSlotReferenceDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("SmartObjectDefinitionReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSmartObjectDefinitionReferenceDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("SmartObjectDefinition", FOnGetDetailCustomizationInstance::CreateStatic(&FSmartObjectDefinitionDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();

	// Register component visualizer for SmartObjectComponent
	RegisterComponentVisualizer(USmartObjectComponent::StaticClass()->GetFName(), MakeShareable(new FSmartObjectComponentVisualizer));

	// Register Editor build type	
	FEditorBuildUtils::RegisterCustomBuildType(
		UE::SmartObject::EditorBuildType,
		FCanDoEditorBuildDelegate::CreateStatic(&UWorldPartitionSmartObjectCollectionBuilder::CanBuildCollections),
		FDoEditorBuildDelegate::CreateStatic(&UWorldPartitionSmartObjectCollectionBuilder::BuildCollections),
		/*BuildAllExtensionPoint*/NAME_None,
		/*MenuEntryLabel*/LOCTEXT("BuildCollections", "Build Smart Object Collections"),
		/*MenuSectionLabel*/LOCTEXT("Gameplay", "Gameplay"));
}

void FSmartObjectsEditorModule::ShutdownModule()
{
	// Unregister Editor build type
	FEditorBuildUtils::UnregisterCustomBuildType(UE::SmartObject::EditorBuildType);

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("SmartObjectSlotDefinition");
		PropertyModule.UnregisterCustomPropertyTypeLayout("SmartObjectDefinitionDataItem");
		PropertyModule.UnregisterCustomPropertyTypeLayout("SmartObjectSlotReference");
		PropertyModule.UnregisterCustomPropertyTypeLayout("SmartObjectDefinitionReference");
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
