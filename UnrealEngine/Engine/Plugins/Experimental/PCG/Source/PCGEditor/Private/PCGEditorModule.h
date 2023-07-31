// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "IAssetTypeActions.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

// Logs
DECLARE_LOG_CATEGORY_EXTERN(LogPCGEditor, Log, All);

class FMenuBuilder;
class FPCGEditorGraphNodeFactory;

class FPCGEditorModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	// ~IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override;
	// ~End IModuleInterface implementation

	//~ IHasMenuExtensibility interface
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	//~ End IHasMenuExtensibility interface

	//~ IHasToolBarExtensibility interface
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }
	//~ End IHasToolBarExtensibility interface

	static EAssetTypeCategories::Type GetAssetCategory() { return PCGAssetCategory; }

protected:
	void RegisterDetailsCustomizations();
	void UnregisterDetailsCustomizations();
	void RegisterAssetTypeActions();
	void UnregisterAssetTypeActions();
	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();
	void AddMenuEntry(FMenuBuilder& MenuBuilder);
	void PopulateMenuActions(FMenuBuilder& MenuBuilder);
	void RegisterSettings();
	void UnregisterSettings();

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
	static EAssetTypeCategories::Type PCGAssetCategory;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	TSharedPtr<FPCGEditorGraphNodeFactory> GraphNodeFactory;
};
