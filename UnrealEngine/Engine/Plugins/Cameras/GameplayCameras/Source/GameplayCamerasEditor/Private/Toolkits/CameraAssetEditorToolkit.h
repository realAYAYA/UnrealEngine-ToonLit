// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/ICameraAssetEditorToolkit.h"

class UCameraAsset;
class FDocumentTracker;
class FToolBarBuilder;
class FAssetDragDropOp;
class FClassDragDropOp;
class FActorDragDropGraphEdOp;

/**
 * Implements an editor toolkit for camera modes.
 */
class FCameraAssetEditorToolkit : public ICameraAssetEditorToolkit, public FGCObject
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use.
	 */
	FCameraAssetEditorToolkit(const TSharedRef<ISlateStyle>& InStyle);

	/** Virtual destructor */
	virtual ~FCameraAssetEditorToolkit();

public:

	/**
	 * Initialize this asset editor.
	 *
	 * @param Mode Asset editing mode for this editor (standalone or world-centric).
	 * @param InitToolkitHost When Mode is WorldCentric, this is the level editor instance to spawn this editor within.
	 * @param CameraAsset The camera mode to edit.
	 * @param TrackEditorDelegates Delegates to call to create auto-key handlers for this sequencer.
	 */
	void Initialize(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCameraAsset* CameraAsset);

	void InternalRegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager);

public:

	//~ FAssetEditorToolkit interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(CameraAsset);
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCameraAssetEditorToolkit");
	}

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

public:

	//~ IToolkit interface
	virtual FText GetBaseToolkitName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FText GetTabSuffix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:

	void CreateWidgets();

	TSharedRef<SDockTab> SpawnTab_DetailsView(const FSpawnTabArgs& Args);

private:

	/** Camera mode for our edit operation. */
	TObjectPtr<UCameraAsset> CameraAsset;

	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style;

	/** Document manager for workflow tabs */
	TSharedPtr<FDocumentTracker> DocumentManager;

	/** Details view */
	TSharedPtr<class IDetailsView> DetailsView;

	/** Tab that holds the details panel */
	TWeakPtr<SDockTab> DetailsViewTab;

	TSharedPtr<class SWidget> Stats;

	TSharedPtr<class IMessageLogListing> StatsListing;
};

