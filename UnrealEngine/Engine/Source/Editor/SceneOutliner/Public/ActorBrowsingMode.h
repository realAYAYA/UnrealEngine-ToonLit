// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorModeInteractive.h"

class UActorBrowsingModeSettings;

class FActorBrowsingMode : public FActorModeInteractive
{
public:
	FActorBrowsingMode(SSceneOutliner* InSceneOutliner, TWeakObjectPtr<UWorld> InSpecifiedWorldToDisplay = nullptr);
	virtual ~FActorBrowsingMode();

	/* Begin ISceneOutlinerMode Interface */
	virtual void Rebuild() override;
	virtual FCreateSceneOutlinerMode CreateFolderPickerMode(const FFolder::FRootObject& InRootObject = FFolder::GetInvalidRootObject()) const override;
	virtual void CreateViewContent(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<FDragDropOperation> CreateDragDropOperation(const FPointerEvent& MouseEvent, const TArray<FSceneOutlinerTreeItemPtr>& InTreeItems) const override;
	virtual bool ParseDragDrop(FSceneOutlinerDragDropPayload& OutPayload, const FDragDropOperation& Operation) const override;
	virtual FSceneOutlinerDragValidationInfo ValidateDrop(const ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload) const override;
	virtual TSharedPtr<SWidget> CreateContextMenu() override;
	virtual void OnItemAdded(FSceneOutlinerTreeItemPtr Item) override;
	virtual void OnItemRemoved(FSceneOutlinerTreeItemPtr Item) override;
	virtual void OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection) override;
	virtual void OnItemDoubleClick(FSceneOutlinerTreeItemPtr Item) override;
	virtual void OnFilterTextCommited(FSceneOutlinerItemSelection& Selection, ETextCommit::Type CommitType) override;
	virtual void OnItemPassesFilters(const ISceneOutlinerTreeItem& Item) override;
	virtual FReply OnKeyDown(const FKeyEvent& InKeyEvent) override;
	virtual void OnDuplicateSelected() override;
	virtual void OnDrop(ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload, const FSceneOutlinerDragValidationInfo& ValidationInfo) const override;
	virtual bool CanRenameItem(const ISceneOutlinerTreeItem& Item) const override;
	virtual FText GetStatusText() const override;
	virtual FSlateColor GetStatusTextColor() const override;
	virtual ESelectionMode::Type GetSelectionMode() const override { return ESelectionMode::Multi; }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual bool ShouldShowFolders() const override { return true; }
	virtual bool SupportsCreateNewFolder() const override { return true; }
	virtual bool ShowStatusBar() const override { return true; }
	virtual bool ShowViewButton() const override { return true; }
	virtual bool ShowFilterOptions() const override { return true; }
	virtual bool CanDelete() const override;
	virtual bool CanRename() const override;
	virtual bool CanCut() const override;
	virtual bool CanCopy() const override;
	virtual bool CanPaste() const override;
	virtual bool CanSupportDragAndDrop() const { return true; }
	virtual bool HasErrors() const;
	virtual FText GetErrorsText() const;
	virtual void RepairErrors() const;	
	virtual FFolder CreateNewFolder() override;
	virtual FFolder GetFolder(const FFolder& ParentPath, const FName& LeafName) override;
	virtual bool CreateFolder(const FFolder& NewFolder) override;
	virtual bool ReparentItemToFolder(const FFolder& FolderPath, const FSceneOutlinerTreeItemPtr& Item) override;
	virtual void SelectFoldersDescendants(const TArray<FFolderTreeItem*>& FolderItems, bool bSelectImmediateChildrenOnly) override;
	virtual void PinItems(const TArray<FSceneOutlinerTreeItemPtr>& InItems) override;
	virtual void UnpinItems(const TArray<FSceneOutlinerTreeItemPtr>& InItems) override;
	virtual void PinSelectedItems() override;
	virtual void UnpinSelectedItems() override;
	/* End ISceneOutlinerMode Interface */
public:
	/* External events this mode must respond to */

	/** Called by the engine when a component is updated */
	void OnComponentsUpdated();
	/** Called by the engine when an actor is deleted */
	void OnLevelActorDeleted(AActor* Actor);

	/** Called by the editor to allow selection of unloaded actors */
	void OnSelectUnloadedActors(const TArray<FGuid>& ActorGuids);
	
	/** Called when an actor desc is removed */
	void OnActorDescRemoved(FWorldPartitionActorDesc* InActorDesc);

	/** Called by engine when edit cut actors begins */
	void OnEditCutActorsBegin();

	/** Called by engine when edit cut actors ends */
	void OnEditCutActorsEnd();

	/** Called by engine when edit copy actors begins */
	void OnEditCopyActorsBegin();

	/** Called by engine when edit copy actors ends */
	void OnEditCopyActorsEnd();

	/** Called by engine when edit paste actors begins */
	void OnEditPasteActorsBegin();

	/** Called by engine when edit paste actors ends */
	void OnEditPasteActorsEnd();

	/** Called by engine when edit duplicate actors begins */
	void OnDuplicateActorsBegin();

	/** Called by engine when edit duplicate actors ends */
	void OnDuplicateActorsEnd();

	/** Called by engine when edit delete actors begins */
	void OnDeleteActorsBegin();

	/** Called by engine when edit delete actors ends */
	void OnDeleteActorsEnd();
	
	/** Function called by the Outliner Filter Bar to compare an item with Type Filters*/
	virtual bool CompareItemWithClassName(SceneOutliner::FilterBarType InItem, const TSet<FTopLevelAssetPath>&) const override;
private:
	/** Build and up the context menu */
	TSharedPtr<SWidget> BuildContextMenu();
	/** Register the context menu with the engine */
	static void RegisterContextMenu();
	bool CanPasteFoldersOnlyFromClipboard() const;

	bool GetFolderNamesFromPayload(const FSceneOutlinerDragDropPayload& InPayload, TArray<FName>& OutFolders, FFolder::FRootObject& OutCommonRootObject) const;
	FFolder GetWorldDefaultRootFolder() const;

	/** Filter factories */
	static TSharedRef<FSceneOutlinerFilter> CreateShowOnlySelectedActorsFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideTemporaryActorsFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateIsInCurrentLevelFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideComponentsFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideLevelInstancesFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideUnloadedActorsFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideEmptyFoldersFilter();

	/** Functions to expose selection framing to the UI */
	void OnToggleAlwaysFrameSelection();
	bool ShouldAlwaysFrameSelection();

	/**
	 * Get a mutable version of the ActorBrowser config for setting values.
	 * @returns		The config for this ActorBrowser.
	 * @note		If OutlinerIdentifier is not set for this outliner, it is not possible to store settings.
	 */
	struct FActorBrowsingModeConfig* GetMutableConfig();

	/**
	 * Get a const version of the ActorBrowser config for getting values.
	 * @returns		The config for this ActorBrowser.
	 * @note		If OutlinerIdentifier is not set for this outliner, it is not possible to store settings.
	 */
	const FActorBrowsingModeConfig* GetConstConfig() const;

	/** Save the config for this ActorBrowser */
	void SaveConfig();
	
private:
	/** Number of actors (including unloaded) which have passed through the filters */
	uint32 FilteredActorCount = 0;
	/** Number of unloaded actors which have passed through all the filters */
	uint32 FilteredUnloadedActorCount = 0;
	/** List of unloaded actors which passed through the regular filters and may or may not have passed the search filter */
	TSet<const FWorldPartitionActorDesc*> ApplicableUnloadedActors;
	/** List of actors which passed the regular filters and may or may not have passed the search filter */
	TSet<TWeakObjectPtr<AActor>> ApplicableActors;

	bool bRepresentingWorldGameWorld = false;
	bool bRepresentingWorldPartitionedWorld = false;
};
