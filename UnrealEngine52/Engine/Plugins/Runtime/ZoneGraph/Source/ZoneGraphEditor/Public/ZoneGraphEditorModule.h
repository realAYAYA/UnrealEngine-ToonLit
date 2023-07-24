// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeCategories.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"


class IZoneGraphEditor;
class FAssetTypeActions_Base;
struct FGraphPanelNodeFactory;
class FComponentVisualizer;

/**
* The public interface to this module
*/
class ZONEGRAPHEDITOR_API FZoneGraphEditorModule : public IModuleInterface//, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer);
	void RegisterMenus();

	void OnBuildZoneGraph();

	TArray<TSharedPtr<class FAssetTypeActions_Base>> ItemDataAssetTypeActions;
	TArray<FName> RegisteredComponentClassNames;
	TSharedPtr<class FUICommandList> PluginCommands;
};
