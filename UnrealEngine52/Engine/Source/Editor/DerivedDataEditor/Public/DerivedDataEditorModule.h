// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDerivedDataCacheNotifications.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

class FSpawnTabArgs;
class SDerivedDataCacheSettingsDialog;
class SDockTab;
class SWidget;
class SWindow;

/**
 * The module holding all of the UI related pieces for DerivedData
 */
class DERIVEDDATAEDITOR_API FDerivedDataEditorModule : public IModuleInterface
{
public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override;

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override;

	TSharedRef<SWidget>	CreateStatusBarWidget();
	IDerivedDataCacheNotifications& GetCacheNotifcations() { return *DerivedDataCacheNotifications; }

	void ShowResourceUsageTab();
	void ShowCacheStatisticsTab();
	
private:

	TSharedPtr<SWidget> CreateResourceUsageDialog();
	TSharedPtr<SWidget> CreateCacheStatisticsDialog();

	TSharedRef<SDockTab> CreateResourceUsageTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> CreateCacheStatisticsTab(const FSpawnTabArgs& Args);

	TWeakPtr<SDockTab> ResourceUsageTab;
	TWeakPtr<SDockTab> CacheStatisticsTab;

	TSharedPtr<SWindow>	SettingsWindow;
	TSharedPtr<SDerivedDataCacheSettingsDialog> SettingsDialog;
	TUniquePtr<IDerivedDataCacheNotifications>	DerivedDataCacheNotifications;
};


