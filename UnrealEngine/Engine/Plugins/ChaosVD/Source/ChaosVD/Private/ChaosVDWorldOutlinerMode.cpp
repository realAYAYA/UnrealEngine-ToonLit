// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDWorldOutlinerMode.h"

#include "ActorTreeItem.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "Elements/Framework/TypedElementSelectionSet.h"

static FAutoConsoleVariable CVarChaosVDQueueAndCombineSceneOutlinerEvents(
	TEXT("p.Chaos.VD.Tool.QueueAndCombineSceneOutlinerEvents"),
	true,
	TEXT("If set to true, scene outliner events will be queued and sent once per frame. If there was a unprocessed event for an item, the las queued event will replace it"));

const FSceneOutlinerTreeItemType FChaosVDActorTreeItem::Type(&FActorTreeItem::Type);

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

const TSharedRef<SWidget> FChaosVDSceneOutlinerGutter::ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row)
{
	if (TreeItem->ShouldShowVisibilityState())
	{
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SVisibilityWidget, SharedThis(this), WeakOutliner, TreeItem, &Row)
				.IsEnabled_Raw(this, &FChaosVDSceneOutlinerGutter::IsEnabled, TreeItem->AsWeak())
				.ToolTipText_Raw(this, &FChaosVDSceneOutlinerGutter::GetVisibilityTooltip, TreeItem->AsWeak())
			];
	}
	return SNullWidget::NullWidget;
}

FText FChaosVDSceneOutlinerGutter::GetVisibilityTooltip(TWeakPtr<ISceneOutlinerTreeItem> WeakTreeItem) const
{

	return IsEnabled(WeakTreeItem) ? LOCTEXT("SceneOutlinerVisibilityToggleTooltip", "Toggles the visibility of this object in the level editor.") :
										LOCTEXT("SceneOutlinerVisibilityToggleDisabkedTooltip", "Visibility of this object is being controlled by another visibility setting");
}

bool FChaosVDSceneOutlinerGutter::IsEnabled(TWeakPtr<ISceneOutlinerTreeItem> WeakTreeItem) const
{
	bool bIsEnabled = true;
	if (const TSharedPtr<ISceneOutlinerTreeItem> TreeItem = WeakTreeItem.Pin())
	{
		if (const FActorTreeItem* ActorItem = TreeItem->CastTo<FActorTreeItem>())
		{
			if (const AChaosVDParticleActor* CVDActor = Cast<AChaosVDParticleActor>(ActorItem->Actor.Get()))
			{
				bIsEnabled = CVDActor->GetHideFlags() == EChaosVDHideParticleFlags::HiddenBySceneOutliner || CVDActor->GetHideFlags() == EChaosVDHideParticleFlags::None;
			}
		}
	}

	return bIsEnabled;
}

bool FChaosVDActorTreeItem::GetVisibility() const
{
	if (const AChaosVDParticleActor* CVDActor = Cast<AChaosVDParticleActor>(Actor.Get()))
	{
		return CVDActor->IsVisible();
	}
	else
	{
		return FActorTreeItem::GetVisibility();
	}
}

void FChaosVDActorTreeItem::OnVisibilityChanged(const bool bNewVisibility)
{
	if (AChaosVDParticleActor* CVDActor = Cast<AChaosVDParticleActor>(Actor.Get()))
	{
		if (bNewVisibility)
		{
			CVDActor->RemoveHiddenFlag(EChaosVDHideParticleFlags::HiddenBySceneOutliner);
		}
		else
		{
			CVDActor->AddHiddenFlag(EChaosVDHideParticleFlags::HiddenBySceneOutliner);
		}
	}
	else
	{
		FActorTreeItem::OnVisibilityChanged(bNewVisibility);
	}
}

TUniquePtr<FChaosVDOutlinerHierarchy> FChaosVDOutlinerHierarchy::Create(ISceneOutlinerMode* Mode, const TWeakObjectPtr<UWorld>& World)
{
	FChaosVDOutlinerHierarchy* Hierarchy = new FChaosVDOutlinerHierarchy(Mode, World);

	Create_Internal(Hierarchy, World);

	return TUniquePtr<FChaosVDOutlinerHierarchy>(Hierarchy);
}


FSceneOutlinerTreeItemPtr FChaosVDOutlinerHierarchy::CreateItemForActor(AActor* InActor, bool bForce) const
{
	return Mode->CreateItemFor<FChaosVDActorTreeItem>(InActor, bForce);
}

FChaosVDWorldOutlinerMode::FChaosVDWorldOutlinerMode(const FActorModeParams& InModeParams, TWeakPtr<FChaosVDScene> InScene)
	: FActorMode(InModeParams),
	CVDScene(InScene)
{
	TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin();
	if (!ensure(ScenePtr.IsValid()))
	{
		return;
	}

	ScenePtr->OnActorActiveStateChanged().AddRaw(this, &FChaosVDWorldOutlinerMode::HandleActorActiveStateChanged);

	RegisterSelectionSetObject(ScenePtr->GetElementSelectionSet());

	ActorLabelChangedDelegateHandle = FCoreDelegates::OnActorLabelChanged.AddRaw(this, &FChaosVDWorldOutlinerMode::HandleActorLabelChanged);
}

FChaosVDWorldOutlinerMode::~FChaosVDWorldOutlinerMode()
{
	FCoreDelegates::OnActorLabelChanged.Remove(ActorLabelChangedDelegateHandle);

	if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{
		ScenePtr->OnActorActiveStateChanged().RemoveAll(this);
	}
}

void FChaosVDWorldOutlinerMode::OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection)
{
	TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin();
	if (!ScenePtr.IsValid())
	{
		return;
	}

	TArray<AActor*> OutlinerSelectedActors = Selection.GetData<AActor*>(SceneOutliner::FActorSelector());

	if (const UTypedElementSelectionSet* SelectionSet = GetSelectionSetObject())
	{
		TArray<AActor*> SceneSelectedActors = SelectionSet->GetSelectedObjects<AActor>();
		ensureMsgf(SceneSelectedActors.Num() < 2, TEXT("Multi Selection is not supported, but [%d] Actors are selected... Choosing the first one"), SceneSelectedActors.Num());

		AActor* SelectedActor = OutlinerSelectedActors.IsEmpty() ? nullptr : OutlinerSelectedActors[0];

		// If the actor ptr is null, the selection will be cleared
		ScenePtr->SetSelectedObject(SelectedActor);
	}
}

void FChaosVDWorldOutlinerMode::OnItemDoubleClick(FSceneOutlinerTreeItemPtr Item)
{
	TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin();
	if (!ScenePtr.IsValid())
	{
		return;
	}

	if (const FActorTreeItem* ActorItem = Item->CastTo<FActorTreeItem>())
	{
		if (AActor* Actor = ActorItem->Actor.Get())
		{
			ScenePtr->OnObjectFocused().Broadcast(Actor);
		}
	}
}

void FChaosVDWorldOutlinerMode::ProcessPendingHierarchyEvents()
{
	for (const TPair<FSceneOutlinerTreeItemID, FSceneOutlinerHierarchyChangedData>& PendingEvent : PendingOutlinerEventsMap)
	{
		Hierarchy->OnHierarchyChanged().Broadcast(PendingEvent.Value);
	}

	PendingOutlinerEventsMap.Reset();
}

bool FChaosVDWorldOutlinerMode::Tick(float DeltaTime)
{
	ProcessPendingHierarchyEvents();
	return true;
}

TUniquePtr<ISceneOutlinerHierarchy> FChaosVDWorldOutlinerMode::CreateHierarchy()
{
	TUniquePtr<FChaosVDOutlinerHierarchy> ActorHierarchy = FChaosVDOutlinerHierarchy::Create(this, RepresentingWorld);

	ActorHierarchy->SetShowingComponents(!bHideComponents);
	ActorHierarchy->SetShowingOnlyActorWithValidComponents(!bHideComponents && bHideActorWithNoComponent);
	ActorHierarchy->SetShowingLevelInstances(!bHideLevelInstanceHierarchy);
	ActorHierarchy->SetShowingUnloadedActors(!bHideUnloadedActors);
	ActorHierarchy->SetShowingEmptyFolders(!bHideEmptyFolders);
	
	return ActorHierarchy;
}

void FChaosVDWorldOutlinerMode::EnqueueAndCombineHierarchyEvent(const FSceneOutlinerTreeItemID& ItemID, const FSceneOutlinerHierarchyChangedData& EnventToProcess)
{
	if (FSceneOutlinerHierarchyChangedData* EventData = PendingOutlinerEventsMap.Find(ItemID))
	{
		*EventData = EnventToProcess;
	}
	else
	{
		PendingOutlinerEventsMap.Add(ItemID, EnventToProcess);
	}	
}
void FChaosVDWorldOutlinerMode::HandleActorLabelChanged(AActor* ChangedActor)
{
	if (!ensure(ChangedActor))
	{
		return;
	}

	if (IsActorDisplayable(ChangedActor) && RepresentingWorld.Get() == ChangedActor->GetWorld())
	{
		// Force create the item otherwise the outliner may not be notified of a change to the item if it is filtered out
		if (FSceneOutlinerTreeItemPtr Item = CreateItemFor<FChaosVDActorTreeItem>(ChangedActor, true))
		{
			SceneOutliner->OnItemLabelChanged(Item);
		}
	}
}

void FChaosVDWorldOutlinerMode::HandleActorActiveStateChanged(AChaosVDParticleActor* ChangedActor)
{
	if (!ChangedActor)
	{
		return;
	}

	if (Hierarchy.IsValid())
	{
		FSceneOutlinerHierarchyChangedData EventData;

		if (ChangedActor->IsActive())
		{
			EventData.Type = FSceneOutlinerHierarchyChangedData::Added;
			EventData.Items.Emplace(CreateItemFor<FChaosVDActorTreeItem>(ChangedActor));
		}
		else
		{
			EventData.ItemIDs.Emplace(ChangedActor);
			EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;
		}

		// There is currently a bug in the Scene Outliner where if opposite events happen multiple times within the same tick, the last ones get dropped
		// (UE-193877). As our current use case is fairly simple, as a workaround we can just queue the events and process them once per frame
		// Only taking into account only the last requested event for each item.
		// Keeping this behind a cvar enabled by default so when the Scene Outliner bug is fixed, we can test it easily.
		if (CVarChaosVDQueueAndCombineSceneOutlinerEvents->GetBool())
		{
			EnqueueAndCombineHierarchyEvent(ChangedActor, EventData);
		}
		else
		{
			Hierarchy->OnHierarchyChanged().Broadcast(EventData);
		}
	}
}

void FChaosVDWorldOutlinerMode::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet)
{
	TArray<AActor*> SelectedActors = ChangesSelectionSet->GetSelectedObjects<AActor>();

	if (SelectedActors.Num() > 0)
	{
		// We don't support multi selection yet
		ensure(SelectedActors.Num() == 1);
		AActor* SelectedActor = SelectedActors[0];
		
		if (FSceneOutlinerTreeItemPtr TreeItem = SceneOutliner->GetTreeItem(SelectedActor, false))
		{
			SceneOutliner->ScrollItemIntoView(TreeItem);
			SceneOutliner->SetItemSelection(TreeItem, true, ESelectInfo::OnMouseClick);
		}
		else
		{
			UE_LOG(LogChaosVDEditor, Verbose, TEXT("Selected actor is not in the outliner. It might be filtered out"))	
		}
	}
}

#undef LOCTEXT_NAMESPACE
