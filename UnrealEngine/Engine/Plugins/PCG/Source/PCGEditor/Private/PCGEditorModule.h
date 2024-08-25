// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

class IAssetTypeActions;

// Logs
DECLARE_LOG_CATEGORY_EXTERN(LogPCGEditor, Log, All);

class FMenuBuilder;
class FPCGEditorGraphNodeFactory;

class FPCGEditorModule : public IModuleInterface
{
public:
	// ~IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override;
	// ~End IModuleInterface implementation

protected:
	void RegisterDetailsCustomizations();
	void UnregisterDetailsCustomizations();
	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();
	void PopulateMenuActions(FMenuBuilder& MenuBuilder);
	void RegisterSettings();
	void UnregisterSettings();

	void OnPostEngineInit();
	void RegisterOnEditorModeChange();
	void OnEditorModeIDChanged(const FEditorModeID& EditorModeID, bool bIsEntering);

	bool ShouldDisableCPUThrottling();

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
	static EAssetTypeCategories::Type PCGAssetCategory;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	TSharedPtr<FPCGEditorGraphNodeFactory> GraphNodeFactory;

	FDelegateHandle ShouldDisableCPUThrottlingDelegateHandle;
};
