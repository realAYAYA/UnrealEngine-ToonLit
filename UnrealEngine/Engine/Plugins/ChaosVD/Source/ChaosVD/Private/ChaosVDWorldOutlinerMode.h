// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ActorBrowsingMode.h"
#include "ActorHierarchy.h"
#include "ActorTreeItem.h"
#include "ChaosVDScene.h"
#include "ChaosVDSceneSelectionObserver.h"
#include "SceneOutlinerGutter.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"

class FChaosVDScene;

/** Outliner Gutter used to override the behaviour of the visibility widget in the Scene outliner  */
class FChaosVDSceneOutlinerGutter final : public FSceneOutlinerGutter
{
public:
	explicit FChaosVDSceneOutlinerGutter(ISceneOutliner& Outliner)
		: FSceneOutlinerGutter(Outliner)
	{
	}

	virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override;
	
	FText GetVisibilityTooltip(TWeakPtr<ISceneOutlinerTreeItem> WeakTreeItem) const;

	bool IsEnabled(TWeakPtr<ISceneOutlinerTreeItem> WeakTreeItem) const;
};

/** Scene outliner Actor Tree Item used to override the behaviour of the visibility changes for CVD Actors in the Scene outliner  */
class FChaosVDActorTreeItem final : public FActorTreeItem
{
public:
	
	FChaosVDActorTreeItem(AActor* InActor) : FActorTreeItem(InActor)
	{
		
	};

	FChaosVDActorTreeItem(const FSceneOutlinerTreeItemType& TypeIn, AActor* InActor)
		: FActorTreeItem(TypeIn, InActor)
	{
	}

	virtual bool GetVisibility() const override;

	virtual void OnVisibilityChanged(const bool bNewVisibility) override;

	static const FSceneOutlinerTreeItemType Type;
};

/** Actor Hierarchy used to override the what Tree items will be used for actors in CVD's Scene outliner  */
class FChaosVDOutlinerHierarchy final : public FActorHierarchy
{
public:

	static TUniquePtr<FChaosVDOutlinerHierarchy> Create(ISceneOutlinerMode* Mode, const TWeakObjectPtr<UWorld>& World);

protected:

	FChaosVDOutlinerHierarchy(ISceneOutlinerMode* Mode, const TWeakObjectPtr<UWorld>& Worlds)
	: FActorHierarchy(Mode, Worlds)
	{
	}
	virtual FSceneOutlinerTreeItemPtr CreateItemForActor(AActor* InActor, bool bForce) const override;
};

/**
 * Scene outliner mode used to represent a CVD (Chaos Visual Debugger) world
 * It has a more limited view compared to the normal outliner, hiding features we don't support,
 * and it is integrated with the CVD local selection system
 */
class FChaosVDWorldOutlinerMode : public FActorMode, public FChaosVDSceneSelectionObserver, public FTSTickerObjectBase
{
public:

	FChaosVDWorldOutlinerMode(const FActorModeParams& InModeParams, TWeakPtr<FChaosVDScene> InScene);

	virtual ~FChaosVDWorldOutlinerMode() override;

	virtual void OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection) override;
	virtual void OnItemDoubleClick(FSceneOutlinerTreeItemPtr Item) override;
	void ProcessPendingHierarchyEvents();


	virtual void CreateViewContent(FMenuBuilder& MenuBuilder) override
	{
		// Intentionally not implemented, as we don't support the built in menu to switch worlds
	}

	virtual bool ShouldShowFolders() const override { return true;}

	virtual bool Tick(float DeltaTime) override;

	virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;

private:

	void EnqueueAndCombineHierarchyEvent(const FSceneOutlinerTreeItemID& ItemID, const FSceneOutlinerHierarchyChangedData& EnventToProcess);
	void HandleActorLabelChanged(AActor* ChangedActor);
	void HandleActorActiveStateChanged(AChaosVDParticleActor* ChangedActor);

	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet) override;

private:
	FDelegateHandle ActorLabelChangedDelegateHandle;

	TWeakPtr<FChaosVDScene> CVDScene;

	TMap<FSceneOutlinerTreeItemID, FSceneOutlinerHierarchyChangedData> PendingOutlinerEventsMap;
};
