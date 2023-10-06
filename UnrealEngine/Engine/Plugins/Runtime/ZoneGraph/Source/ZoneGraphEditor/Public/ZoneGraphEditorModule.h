// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "Modules/ModuleInterface.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"

class FComponentVisualizer;
class FUICommandList;

/**
* The public interface to this module
*/
class ZONEGRAPHEDITOR_API FZoneGraphEditorModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer);
	void RegisterMenus();

	void OnBuildZoneGraph();

	TArray<FName> RegisteredComponentClassNames;
	TSharedPtr<FUICommandList> PluginCommands;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#endif