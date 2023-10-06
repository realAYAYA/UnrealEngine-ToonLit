// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterConfiguratorCompiler.h"
#include "IAssetTypeActions.h"
#include "IDisplayClusterConfigurator.h"

class FPlacementModeID;
class IAssetTools;
class FDisplayClusterConfiguratorAssetTypeActions;
class FExtensibilityManager;

/**
 * Display Cluster Configurator editor module
 */
class FDisplayClusterConfiguratorModule :
	public IDisplayClusterConfigurator
{
public:
	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void OnPostEngineInit();

	/** Gets the extensibility managers for outside entities to extend this editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() const override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const override { return ToolBarExtensibilityManager; }
	
public:
	//~ Begin IDisplayClusterConfigurator Interface
	virtual const FDisplayClusterConfiguratorCommands& GetCommands() const override;
	//~ End IDisplayClusterConfigurator Interface

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	void RegisterSettings();
	void UnregisterSettings();
	void RegisterCustomLayouts();
	void UnregisterCustomLayouts();
	void RegisterSectionMappings();
	void UnregisterSectionMappings();

	/** Register any panel extensions used by this module */
	void RegisterPanelExtensions();

	/** Unregister any active panel extensions used by this module */
	void UnregisterPanelExtensions();

	static TSharedPtr<FKismetCompilerContext> GetCompilerForDisplayClusterBP(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);

	/** Callback when the level editor viewport creates its toolbar */
	static TSharedRef<SWidget> OnExtendLevelEditorViewportToolbar(FWeakObjectPtr ExtensionContext);

	/** Callback when our viewports frozen button is clicked */
	static FReply OnViewportsFrozenWarningClicked();

	/** If the viewports frozen warning button should be displayed */
	static EVisibility GetViewportsFrozenWarningVisibility();

private:
	TArray<TSharedPtr<IAssetTypeActions>> CreatedAssetTypeActions;
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
	FDisplayClusterConfiguratorKismetCompiler  BlueprintCompiler;
	TArray<FName> RegisteredClassLayoutNames;
	TArray<FName> RegisteredPropertyLayoutNames;
	FDelegateHandle FilesLoadedHandle;
	FDelegateHandle PostEngineInitHandle;

private:
	static FOnDisplayClusterConfiguratorReadOnlyChanged OnDisplayClusterConfiguratorReadOnlyChanged;
};
