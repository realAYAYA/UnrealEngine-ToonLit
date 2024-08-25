// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorMode.h"
#include "Containers/Map.h"
#include "EditorConfigBase.h"
#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "WorldPartition/WorldPartitionHandle.h"

#include "ObjectMixerOutlinerMode.generated.h"

class FObjectMixerEditorList;
class IWorldPartitionEditorModule;

namespace ObjectMixerOutliner
{
	struct FWeakActorSelectorAcceptingComponents
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, TWeakObjectPtr<AActor>& DataOut) const;
	};

	struct FComponentSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, UActorComponent*& DataOut) const;
	};

	/** Functor which can be used to get weak actor pointers from a selection */
	struct FWeakActorSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, TWeakObjectPtr<AActor>& DataOut) const;
	};

	/** Functor which can be used to get actors from a selection including component parents */
	struct FActorSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, AActor*& ActorPtrOut) const;
	};
		
	struct UE_DEPRECATED(5.4, "Use FActorHandleSelector instead") FActorDescSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, FWorldPartitionActorDesc*& ActorDescPtrOut) const { return false; }
	};

	/** Functor which can be used to get actor descriptors from a selection  */
	struct FActorHandleSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, FWorldPartitionHandle& ActorHandleOut) const;
	};

	struct FFolderPathSelector
	{
		bool operator()(TWeakPtr<ISceneOutlinerTreeItem> Item, FFolder& DataOut) const;
	};
}

USTRUCT()
struct FObjectMixerOutlinerModeConfig
{
	GENERATED_BODY()

	/** True when the Scene Outliner is hiding temporary/run-time Actors */
	UPROPERTY()
	bool bHideTemporaryActors = false;

	/** True when the Scene Outliner is showing only Actors that exist in the current level */
	UPROPERTY()
	bool bShowOnlyActorsInCurrentLevel = false;

	/** True when the Scene Outliner is showing only Actors that exist in the current data layers */
	UPROPERTY()
	bool bShowOnlyActorsInCurrentDataLayers = false;

	/** True when the Scene Outliner is showing only Actors that exist in the current content bundle */
	UPROPERTY()
	bool bShowOnlyActorsInCurrentContentBundle = false;

	/** True when the Scene Outliner is only displaying selected Actors */
	UPROPERTY()
	bool bShowOnlySelectedActors = false;

	/** True when the Scene Outliner is not displaying Actor Components*/
	UPROPERTY()
	bool bHideActorComponents = true;

	/** True when the Scene Outliner is not displaying LevelInstances */
	UPROPERTY()
	bool bHideLevelInstanceHierarchy = false;

	/** True when the Scene Outliner is not displaying unloaded actors */
	UPROPERTY()
	bool bHideUnloadedActors = false;

	/** True when the Scene Outliner is not displaying empty folders */
	UPROPERTY()
	bool bHideEmptyFolders = false;

	/** True when the Scene Outliner updates when an actor is selected in the viewport */
	UPROPERTY()
	bool bAlwaysFrameSelection = true;
};

UCLASS(EditorConfig="ObjectMixerOutlinerMode")
class UObjectMixerOutlinerModeEditorConfig : public UEditorConfigBase
{
	GENERATED_BODY()
	
public:

	static void Initialize()
	{
		if(!Instance)
		{
			Instance = NewObject<UObjectMixerOutlinerModeEditorConfig>(); 
			Instance->AddToRoot();
		}
	}

	UPROPERTY(meta=(EditorConfig))
	TMap<FName, FObjectMixerOutlinerModeConfig> Browsers;

private:

	static TObjectPtr<UObjectMixerOutlinerModeEditorConfig> Instance;
};

struct OBJECTMIXEREDITOR_API FObjectMixerOutlinerModeParams : FActorModeParams
{
	FObjectMixerOutlinerModeParams() {}

	FObjectMixerOutlinerModeParams(
		SSceneOutliner* InSceneOutliner,
		const TWeakObjectPtr<UWorld>& InSpecifiedWorldToDisplay = nullptr,
		bool bInHideComponents = true, bool bInHideLevelInstanceHierarchy = true,
		bool bInHideUnloadedActors = true, bool bInHideEmptyFolders = true, bool bInHideActorWithNoComponent = true)
		: FActorModeParams(InSceneOutliner, InSpecifiedWorldToDisplay, bInHideComponents, bInHideLevelInstanceHierarchy, bInHideUnloadedActors, bInHideEmptyFolders)
	{
		bHideActorWithNoComponent = bInHideActorWithNoComponent;
	}
};

class OBJECTMIXEREDITOR_API FObjectMixerOutlinerMode : public FActorMode
{
public:
	
	struct FFilterClassSelectionInfo
	{
		UClass* Class;
		bool bIsUserSelected;
	};
	
	FObjectMixerOutlinerMode(const FObjectMixerOutlinerModeParams& Params, const TSharedRef<FObjectMixerEditorList> InListModel);
	virtual ~FObjectMixerOutlinerMode();

	TWeakPtr<FObjectMixerEditorList> GetListModelPtr() const
	{
		return ListModelPtr;
	}

	SSceneOutliner* GetSceneOutliner() const
	{
		return SceneOutliner;
	}
	
	/**
	 * Determines the style of the tree (flat list or hierarchy)
	 */
	EObjectMixerTreeViewMode GetTreeViewMode() const;
	/**
	 * Determine the style of the tree (flat list or hierarchy)
	 */
	void SetTreeViewMode(EObjectMixerTreeViewMode InViewMode);

	UWorld* GetRepresentingWorld();

	/* Begin ISceneOutlinerMode Interface */
	virtual bool IsInteractive() const override { return true; }
	virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;
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
	virtual bool ShouldShowFolders() const override { return GetTreeViewMode() == EObjectMixerTreeViewMode::Folders; }
	virtual bool SupportsCreateNewFolder() const override { return true; }
	virtual bool ShowStatusBar() const override { return true; }
	virtual bool ShowViewButton() const override { return true; }
	virtual bool ShowFilterOptions() const override { return true; }
	virtual bool CanDelete() const override;
	virtual bool CanRename() const override;
	virtual bool CanCut() const override;
	virtual bool CanCopy() const override;
	virtual bool CanPaste() const override;
	virtual bool CanSupportDragAndDrop() const override { return true; }
	virtual bool CanCustomizeToolbar() const override { return true; }
	virtual bool HasErrors() const override;
	virtual FText GetErrorsText() const override;
	virtual void RepairErrors() const override;	
	virtual FFolder CreateNewFolder() override;
	virtual FFolder GetFolder(const FFolder& ParentPath, const FName& LeafName) override;
	virtual bool CreateFolder(const FFolder& NewFolder) override;
	virtual bool ReparentItemToFolder(const FFolder& FolderPath, const FSceneOutlinerTreeItemPtr& Item) override;
	virtual void SelectFoldersDescendants(const TArray<FFolderTreeItem*>& FolderItems, bool bSelectImmediateChildrenOnly) override;
	virtual void PinItems(const TArray<FSceneOutlinerTreeItemPtr>& InItems) override;
	virtual void UnpinItems(const TArray<FSceneOutlinerTreeItemPtr>& InItems) override;
	virtual bool CanPinItems(const TArray<FSceneOutlinerTreeItemPtr>& InItems) const override;
	virtual bool CanUnpinItems(const TArray<FSceneOutlinerTreeItemPtr>& InItems) const override;
	virtual void SynchronizeSelection() override;
	/* End ISceneOutlinerMode Interface */

	/* External events this mode must respond to */

	/** Called by the engine when a component is updated */
	void OnComponentsUpdated();
	/** Called by the engine when an actor is deleted */
	void OnLevelActorDeleted(AActor* Actor);

	/** Called by the editor to allow selection of unloaded actors */
	void OnSelectUnloadedActors(const TArray<FGuid>& ActorGuids);
	
	/** Called when an actor desc instance is removed */
	void OnActorDescInstanceRemoved(FWorldPartitionActorDescInstance* InActorDescInstance);
	
	UE_DEPRECATED(5.4, "Use OnActorDescInstanceRemoved instead")
	void OnActorDescRemoved(FWorldPartitionActorDesc* InActorDesc) {}

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
	
protected:

	/* Events */

	void OnMapChange(uint32 MapFlags);
	void OnNewCurrentLevel();

	void OnLevelSelectionChanged(UObject* Obj);
	void OnActorLabelChanged(AActor* ChangedActor);
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);
	void OnLevelActorRequestsRename(const AActor* Actor);
	void OnPostLoadMapWithWorld(UWorld* World);

	bool ShouldSyncSelectionToEditor();
	bool ShouldSyncSelectionFromEditor();
	void SynchronizeAllSelectionsToEditor();
	bool HasActorSelectionChanged(TArray<AActor*>& OutSelectedActors, bool& bOutAreAnyInPIE);
	bool HasComponentSelectionChanged(TArray<UActorComponent*>& OutSelectedComponents, bool& bOutAreAnyInPIE);
	static void SelectActorsInEditor(const TArray<AActor*>& InSelectedActors);
	static void SelectComponentsInEditor(const TArray<UActorComponent*>& InSelectedComponents);
	
	/** Build and up the context menu */
	TSharedPtr<SWidget> BuildContextMenu();
	/** Register the context menu with the engine */
	static void RegisterContextMenu();
	bool CanPasteFoldersOnlyFromClipboard() const;

	bool GetFolderNamesFromPayload(const FSceneOutlinerDragDropPayload& InPayload, TArray<FName>& OutFolders, FFolder::FRootObject& OutCommonRootObject) const;
	FFolder GetWorldDefaultRootFolder() const;

	void SynchronizeComponentSelection();
	void SynchronizeSelectedActorDescs();

	void OnActorEditorContextSubsystemChanged();

	/** Filter factories */
	static TSharedRef<FSceneOutlinerFilter> CreateShowOnlySelectedActorsFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideTemporaryActorsFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateIsInCurrentLevelFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateIsInCurrentDataLayersFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideComponentsFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideLevelInstancesFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideUnloadedActorsFilter();
	static TSharedRef<FSceneOutlinerFilter> CreateHideEmptyFoldersFilter();
	TSharedRef<FSceneOutlinerFilter> CreateIsInCurrentContentBundleFilter();

	/** Functions to expose selection framing to the UI */
	void OnToggleAlwaysFrameSelection();
	bool ShouldAlwaysFrameSelection();

	/**
	 * Get a mutable version of the ActorBrowser config for setting values.
	 * @returns		The config for this ActorBrowser.
	 * @note		If OutlinerIdentifier is not set for this outliner, it is not possible to store settings.
	 */
	FObjectMixerOutlinerModeConfig* GetMutableConfig() const;

	/**
	 * Get a const version of the ActorBrowser config for getting values.
	 * @returns		The config for this ActorBrowser.
	 * @note		If OutlinerIdentifier is not set for this outliner, it is not possible to store settings.
	 */
	const FObjectMixerOutlinerModeConfig* GetConstConfig() const;

	/** Save the config for this ActorBrowser */
	void SaveConfig();

	IWorldPartitionEditorModule* WorldPartitionEditorModule;
	/** Number of actors (including unloaded) which have passed through the filters */
	uint32 FilteredActorCount = 0;
	/** Number of unloaded actors which have passed through all the filters */
	uint32 FilteredUnloadedActorCount = 0;
	/** List of unloaded actors which passed through the regular filters and may or may not have passed the search filter */
	TSet<FWorldPartitionHandle> ApplicableUnloadedActors;
	/** List of actors which passed the regular filters and may or may not have passed the search filter */
	TSet<TWeakObjectPtr<AActor>> ApplicableActors;

	bool bRepresentingWorldGameWorld = false;
	bool bRepresentingWorldPartitionedWorld = false;

	TWeakPtr<FObjectMixerEditorList> ListModelPtr;
	
	TArray<FFilterClassSelectionInfo> FilterClassSelectionInfos;
	TSharedRef<SWidget> OnGenerateFilterClassMenu();

	/** Used in case of a selection sync override. */
	
	bool bShouldTemporarilyForceSelectionSyncFromEditor = false;
	bool bShouldTemporarilyForceSelectionSyncToEditor = false;
};
