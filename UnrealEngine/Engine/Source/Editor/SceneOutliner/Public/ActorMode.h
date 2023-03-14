// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutlinerMode.h"

namespace SceneOutliner
{
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
}

struct SCENEOUTLINER_API FActorModeParams
{
	FActorModeParams() {}

	FActorModeParams(SSceneOutliner* InSceneOutliner, const TWeakObjectPtr<UWorld>& InSpecifiedWorldToDisplay = nullptr, bool bInHideComponents = true, bool bInHideLevelInstanceHierarchy = true, bool bInHideUnloadedActors = true, bool bInHideEmptyFolders = true)
		: SpecifiedWorldToDisplay(InSpecifiedWorldToDisplay)
		, SceneOutliner(InSceneOutliner)
		, bHideComponents(bInHideComponents)
		, bHideLevelInstanceHierarchy(bInHideLevelInstanceHierarchy)
		, bHideUnloadedActors(bInHideUnloadedActors)
		, bHideEmptyFolders(bInHideEmptyFolders)
	{}

	TWeakObjectPtr<UWorld> SpecifiedWorldToDisplay = nullptr;
	SSceneOutliner* SceneOutliner = nullptr;
	bool bHideComponents = true;
	bool bHideActorWithNoComponent = false;
	bool bHideLevelInstanceHierarchy = true;
	bool bHideUnloadedActors = true;
	bool bHideEmptyFolders = true;
};

class SCENEOUTLINER_API FActorMode : public ISceneOutlinerMode
{
public:
	struct EItemSortOrder
	{
		enum Type { World = 0, Level = 10, Folder = 20, Actor = 30, Unloaded = 40 };
	};
	
	FActorMode(const FActorModeParams& Params);
	virtual ~FActorMode();

	virtual void Rebuild() override;

	void BuildWorldPickerMenu(FMenuBuilder& MenuBuilder);

	virtual void SynchronizeSelection() override { SynchronizeActorSelection(); }

	virtual void OnFilterTextChanged(const FText& InFilterText) override;

	virtual int32 GetTypeSortPriority(const ISceneOutlinerTreeItem& Item) const override;

	static bool IsActorDisplayable(const SSceneOutliner* SceneOutliner, const AActor* Actor);
	static bool IsActorLevelDisplayable(ULevel* InLevel);

	virtual FFolder::FRootObject GetRootObject() const override;
	virtual FFolder::FRootObject GetPasteTargetRootObject() const override;

private:
	/** Called when the user selects a world in the world picker menu */
	void OnSelectWorld(TWeakObjectPtr<UWorld> World);
private:
	/* Private Helpers */

	void ChooseRepresentingWorld();
	bool IsWorldChecked(TWeakObjectPtr<UWorld> World) const;
protected:
	void SynchronizeActorSelection();
	bool IsActorDisplayable(const AActor* InActor) const;

	/** Set the Scene Outliner attached to this mode as the most recently used outliner in the Level Editor */
	void SetAsMostRecentOutliner() const;

	virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;
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
};
