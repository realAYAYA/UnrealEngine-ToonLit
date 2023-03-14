// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/IWorldPartitionEditorModule.h"

class FTabManager;
class FLayoutExtender;
class SDockTab;
class FSpawnTabArgs;
class SContentBundleBrowser;


/**
 * The module holding all of the UI related pieces for SubLevels management
 */
class FWorldPartitionEditorModule : public IWorldPartitionEditorModule
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
	
	/**
	 * Creates a world partition widget
	 */
	virtual TSharedRef<class SWidget> CreateWorldPartitionEditor();

	/**
	 * Returns placement grid size setting that should be assigned to new AWorldSettings actors.
	 */
	virtual int32 GetPlacementGridSize() const override;

	/**
	 * Returns foliage grid size setting that should be assigned to new AWorldSettings actors.
	 */
	virtual int32 GetInstancedFoliageGridSize() const override;

	/**
	 * Returns the threshold from which minimap generates a warning if its WorldUnitsPerPixel is above this value.
	 */
	virtual int32 GetMinimapLowQualityWorldUnitsPerPixelThreshold() const override;

	/**
	 * Returns if loading in the editor is disabled or not.
	 */
	virtual bool GetDisableLoadingInEditor() const override;
	virtual void SetDisableLoadingInEditor(bool bInDisableLoadingInEditor) override;

	/**
	 * Returns if pie is disabled or not..
	 */
	virtual bool GetDisablePIE() const override;
	virtual void SetDisablePIE(bool bInDisablePIE) override;

	/**
	 * Convert the specified map to a world partition map.
	 */
	virtual bool ConvertMap(const FString& InLongPackageName) override;

	/**
	 * Run a world partition builder for the current map. Will display a dialog to specify options for the generation.
	 */
	virtual bool RunBuilder(const FRunBuilderParams& InParams) override;

	/** Return the world added event. */
	virtual FWorldPartitionCreated& OnWorldPartitionCreated() override { return WorldPartitionCreatedEvent; }

	/** Return the commandlet execution event */
	virtual FOnExecuteCommandlet& OnExecuteCommandlet() override { return OnExecuteCommandletEvent; }

	/**
	 * Creates a Content Bundle Browser widget
	 */
	TSharedRef<class SWidget> CreateContentBundleBrowser();
private:
	/** Called when the level editors map changes. We will determine if the new map is a valid world partition world and close world partition tabs if not */
	void OnMapChanged(uint32 MapFlags);

	/** Register menus */
	void RegisterMenus();

	/** Registers world partition tabs spawners with the level editor */
	void RegisterWorldPartitionTabs(TSharedPtr<FTabManager> InTabManager);

	/** Inserts world partition tabs into the level editor layout */
	void RegisterWorldPartitionLayout(FLayoutExtender& Extender);
	
	/** Spawns the world partition tab */
	TSharedRef<SDockTab> SpawnWorldPartitionTab(const FSpawnTabArgs& Args);

	/** Spawns the content bundle tab */
	TSharedRef<SDockTab> SpawnContentBundleTab(const FSpawnTabArgs& Args);

	bool Build(const FRunBuilderParams& InParams);
	bool BuildMinimap(const FRunBuilderParams& InParams);
	bool BuildHLODs(const FRunBuilderParams& InParams);
	bool BuildLandscapeSplineMeshes(UWorld* InWorld);

private:
	void RunCommandletAsExternalProcess(const FString& InCommandletArgs, const FText& InOperationDescription, int32& OutResult, bool& bOutCancelled, FString& OutCommandletOutput);
	void OnConvertMap();

	FDelegateHandle LevelEditorExtenderDelegateHandle;

	TSharedPtr<class FHLODLayerAssetTypeActions> HLODLayerAssetTypeActions;

	TWeakPtr<SDockTab> WorldPartitionTab;

	TWeakPtr<SDockTab> ContentBundleTab;

	TWeakPtr<SContentBundleBrowser> ContentBundleBrowser;

	FWorldPartitionCreated WorldPartitionCreatedEvent;

	FOnExecuteCommandlet OnExecuteCommandletEvent;
};
