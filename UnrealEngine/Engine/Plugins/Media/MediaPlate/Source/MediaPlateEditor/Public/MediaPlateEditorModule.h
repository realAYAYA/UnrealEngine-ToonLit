// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "TickableEditorObject.h"
#include "UObject/ObjectPtr.h"

class FDragDropEvent;
class IAssetTools;
class IAssetTypeActions;
class ISlateStyle;
class UMediaPlateComponent;
class UMediaSource;
struct FAssetData;

/** Log category for this module. */
DECLARE_LOG_CATEGORY_EXTERN(LogMediaPlateEditor, Log, All);

class FMediaPlateEditorModule : public IModuleInterface, public FTickableEditorObject
{
public:
	
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** FTickableEditorObject interface */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FMediaPlateEditorModule, STATGROUP_Tickables); }
	
	/**
	 * Get the style used by this module.
	 **/
	TSharedPtr<ISlateStyle> GetStyle() { return Style; }
	
	/**
	 * Call this when a media plate starts playing so we can track it.
	 */
	void MediaPlateStartedPlayback(TObjectPtr<UMediaPlateComponent> MediaPlate);

	/**
	 * Call this if you want to check if your media source is a drag and drop media source.
	 * If it is, then this will remove it from this module
	 * and you can then change the outer (as it will be transient.)
	 * 
	 * @param MediaSource	Media source to check.
	 * @return				True if it is a drag and drop media source.
	 */
	bool RemoveMediaSourceFromDragDropCache(UMediaSource* MediaSource);
	
private:

	/** Customization name to avoid reusing staticstruct during shutdown. */
	FName MediaPlateName;
	/** Holds all the media plates that are playing. */
	TArray<TWeakObjectPtr<UMediaPlateComponent>> ActiveMediaPlates;

	/** Holds the plug-ins style set. */
	TSharedPtr<ISlateStyle> Style;
	/** The collection of registered asset type actions. */
	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
	/** Handle for our track editor. */
	FDelegateHandle TrackEditorBindingHandle;

	/** Maps a drag and drop file to a media source we created from it. */
	TMap<FString, TWeakObjectPtr<UMediaSource>> MapFileToMediaSource;
	/** Caches if real time viewports are enabled. */
	bool bIsRealTimeViewportsEnabled = false;

	/**
	 * Registers all of our asset tools.
	 */
	void RegisterAssetTools();

	/**
	 * Registers a single asset type action.
	 *
	 * @param AssetTools	The asset tools object to register with.
	 * @param Action		The asset type action to register.
	 */
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	/**
	 * Unregisters all of our asset tools.
	 */
	void UnregisterAssetTools();

	/**
	 * Called after the engine has initialized.
	 */
	void OnPostEngineInit();

	/**
	 * Extracts an asset for a file if it can.
	 *
	 * @param Files				Files to get the asset from.
	 * @param AssetDataArray	Will add the asset here if it gets one.
	 */
	void ExtractAssetDataFromFiles(const TArray<FString>& Files, TArray<FAssetData>& AssetDataArray);

	/**
	 * Enables real time viewports so media plates can update in the editor.
	 */
	void ForceRealTimeViewports(const bool bEnable);
};
