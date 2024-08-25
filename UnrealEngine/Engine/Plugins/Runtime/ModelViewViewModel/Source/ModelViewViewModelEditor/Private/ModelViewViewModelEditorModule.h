// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "UObject/Object.h"

namespace UE::MVVM
{
	class FAssetTypeActions_ViewModelBlueprint;
	class FMVVMListViewBaseExtensionCustomizationExtender;
	class FMVVMPropertyBindingExtension;
}

class FMVVMBindPropertiesDetailView;
class FWidgetBlueprintApplicationMode;
class FWorkflowAllowedTabSet;
class UBlueprint;
class UWidgetBlueprint;
class UWidgetBlueprintGeneratedClass;

/**
 *
 */
class FModelViewViewModelEditorModule : public IModuleInterface
{
public:
	FModelViewViewModelEditorModule() = default;

	//~ Begin IModuleInterface interface
	 virtual void StartupModule() override;
	 virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	void HandleRegisterBlueprintEditorTab(const FWidgetBlueprintApplicationMode& ApplicationMode, FWorkflowAllowedTabSet& TabFactories);
	void HandleRenameVariableReferences(UBlueprint* Blueprint, UClass* VariableClass, const FName& OldVarName, const FName& NewVarName);
	void HandleDeactiveMode(FWidgetBlueprintApplicationMode& InDesignerMode);
	void HandleActivateMode(FWidgetBlueprintApplicationMode& InDesignerMode);
	void HandleWidgetBlueprintAssetTags(const UWidgetBlueprint* Widget, FAssetRegistryTagsContext Context);
	void HandleClassBlueprintAssetTags(const UWidgetBlueprintGeneratedClass* GeneratedClass, FAssetRegistryTagsContext Context);
	void HandleRegisterMenus();
	void UnregisterMenus();

private:
	TSharedPtr<UE::MVVM::FMVVMPropertyBindingExtension> PropertyBindingExtension;
	TSharedPtr<UE::MVVM::FAssetTypeActions_ViewModelBlueprint> ViewModelBlueprintActions;
	TSharedPtr<UE::MVVM::FMVVMListViewBaseExtensionCustomizationExtender> ListViewBaseCustomizationExtender;
};
