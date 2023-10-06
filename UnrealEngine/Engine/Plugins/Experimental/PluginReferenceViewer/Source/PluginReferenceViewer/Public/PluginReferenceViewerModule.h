// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

class FPluginReferenceViewerGraphPanelNodeFactory;

class PLUGINREFERENCEVIEWER_API FPluginReferenceViewerModule : public IModuleInterface
{
public:
	static inline FPluginReferenceViewerModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FPluginReferenceViewerModule>("PluginReferenceViewer");
	}

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End of IModuleInterface interface

	void OpenPluginReferenceViewerUI(const TSharedRef<class IPlugin>& Plugin);

private:
	void OnLaunchReferenceViewerFromPluginBrowser(TSharedPtr<IPlugin> Plugin);

	static const FName PluginReferenceViewerTabName;

	TSharedRef<SDockTab> SpawnPluginReferenceViewerTab(const FSpawnTabArgs& Args);

	TSharedPtr<FPluginReferenceViewerGraphPanelNodeFactory> PluginReferenceViewerGraphPanelNodeFactory;
};