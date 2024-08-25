// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutlinerMode.h"
#include "WorldPartition/WorldPartitionHandle.h"

namespace SceneOutliner
{
	/** Functor which can be used to get weak actor pointers from a selection */
	struct SCENEOUTLINER_API FWeakActorSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, TWeakObjectPtr<AActor>& DataOut) const;
	};

	/** Functor which can be used to get actors from a selection including component parents */
	struct SCENEOUTLINER_API FActorSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, AActor*& ActorPtrOut) const;
	};

	struct UE_DEPRECATED(5.4, "Use FActorHandleSelector instead") FActorDescSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>&Item, FWorldPartitionActorDesc * &ActorDescPtrOut) const { return false; }
	};

	/** Functor which can be used to get actor descriptors from a selection  */
	struct SCENEOUTLINER_API FActorHandleSelector
	{
		bool operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, FWorldPartitionHandle& ActorHandleOut) const;
	};
}

struct SCENEOUTLINER_API FActorModeParams
{
	FActorModeParams() {}

	FActorModeParams(SSceneOutliner* InSceneOutliner, const TWeakObjectPtr<UWorld>& InSpecifiedWorldToDisplay = nullptr, bool bInHideComponents = true, bool bInHideLevelInstanceHierarchy = true, bool bInHideUnloadedActors = true, bool bInHideEmptyFolders = true, bool bInCanInteractWithSelectableActorsOnly = true)
		: SpecifiedWorldToDisplay(InSpecifiedWorldToDisplay)
		, SceneOutliner(InSceneOutliner)
		, bHideComponents(bInHideComponents)
		, bHideLevelInstanceHierarchy(bInHideLevelInstanceHierarchy)
		, bHideUnloadedActors(bInHideUnloadedActors)
		, bHideEmptyFolders(bInHideEmptyFolders)
		, bCanInteractWithSelectableActorsOnly(bInCanInteractWithSelectableActorsOnly)
	{}

	TWeakObjectPtr<UWorld> SpecifiedWorldToDisplay = nullptr;
	SSceneOutliner* SceneOutliner = nullptr;
	bool bHideComponents = true;
	bool bHideActorWithNoComponent = false;
	bool bHideLevelInstanceHierarchy = true;
	bool bHideUnloadedActors = true;
	bool bHideEmptyFolders = true;
	bool bCanInteractWithSelectableActorsOnly = true;
};

class SCENEOUTLINER_API FActorMode : public ISceneOutlinerMode
{
public:
	struct EItemSortOrder
	{
		enum Type { World = 0, Level = 10, Folder = 20, Actor = 30 };
	};
	
	FActorMode(const FActorModeParams& Params);
	virtual ~FActorMode();

	virtual void Rebuild() override;

	void BuildWorldPickerMenu(FMenuBuilder& MenuBuilder);

	virtual void SynchronizeSelection() override { SynchronizeActorSelection(); }

	virtual void OnFilterTextChanged(const FText& InFilterText) override;

	virtual int32 GetTypeSortPriority(const ISceneOutlinerTreeItem& Item) const override;

	static bool IsActorDisplayable(const SSceneOutliner* SceneOutliner, const AActor* Actor, bool bShowLevelInstanceContent = false);
	static bool IsActorLevelDisplayable(ULevel* InLevel);

	virtual FFolder::FRootObject GetRootObject() const override;
	virtual FFolder::FRootObject GetPasteTargetRootObject() const override;

	virtual bool CanInteract(const ISceneOutlinerTreeItem& Item) const override;
	
	virtual TSharedPtr<FDragDropOperation> CreateDragDropOperation(const FPointerEvent& MouseEvent, const TArray<FSceneOutlinerTreeItemPtr>& InTreeItems) const override;
	virtual bool ParseDragDrop(FSceneOutlinerDragDropPayload& OutPayload, const FDragDropOperation& Operation) const override;
	virtual FSceneOutlinerDragValidationInfo ValidateDrop(const ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload) const override;
	virtual void OnDrop(ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload, const FSceneOutlinerDragValidationInfo& ValidationInfo) const override;


private:
	/** Called when the user selects a world in the world picker menu */
	void OnSelectWorld(TWeakObjectPtr<UWorld> World);
private:
	/* Private Helpers */

	void ChooseRepresentingWorld();
	bool IsWorldChecked(TWeakObjectPtr<UWorld> World) const;
	bool GetFolderNamesFromPayload(const FSceneOutlinerDragDropPayload& InPayload, TArray<FName>& OutFolders, FFolder::FRootObject& OutCommonRootObject) const;

protected:
	void SynchronizeActorSelection();
	bool IsActorDisplayable(const AActor* InActor) const;

	/** Set the Scene Outliner attached to this mode as the most recently used outliner in the Level Editor */
	void SetAsMostRecentOutliner() const;

	virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;
	
	FFolder GetWorldDefaultRootFolder() const;
protected:
	/** The world which we are currently representing */
	TWeakObjectPtr<UWorld> RepresentingWorld;
	/** The world which the user manually selected */
	TWeakObjectPtr<UWorld> UserChosenWorld;

	/** If this mode was created to display a specific world, don't allow it to be reassigned */
	const TWeakObjectPtr<UWorld> SpecifiedWorldToDisplay;

	/** Should components be hidden */
	bool bHideComponents;
	/** Should actor with no component be hidden. */
	bool bHideActorWithNoComponent;
	/** Should the level instance hierarchy be hidden */
	bool bHideLevelInstanceHierarchy;
	/** Should unloaded actors be hidden */
	bool bHideUnloadedActors;
	/** Should empty folders be hidden */
	bool bHideEmptyFolders;
	/** Should the outliner scroll to the item on selection */
	bool bAlwaysFrameSelection;
	/** If True, CanInteract will be restricted to selectable actors only. */
	bool bCanInteractWithSelectableActorsOnly;
};
