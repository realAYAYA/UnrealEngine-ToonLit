// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDWorldOutlinerMode.h"

#include "ActorTreeItem.h"
#include "ChaosVDScene.h"
#include "Elements/Framework/TypedElementSelectionSet.h"


FChaosVDWorldOutlinerMode::FChaosVDWorldOutlinerMode(const FActorModeParams& InModeParams, TWeakPtr<FChaosVDScene> InScene)
	: FActorMode(InModeParams),
	CVDScene(InScene)
{
	TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin();
	if (!ensure(ScenePtr.IsValid()))
	{
		return;
	}

	RegisterSelectionSetObject(ScenePtr->GetElementSelectionSet());

	ActorLabelChangedDelegateHandle = FCoreDelegates::OnActorLabelChanged.AddRaw(this, &FChaosVDWorldOutlinerMode::HandleActorLabelChanged);
}

FChaosVDWorldOutlinerMode::~FChaosVDWorldOutlinerMode()
{
	FCoreDelegates::OnActorLabelChanged.Remove(ActorLabelChangedDelegateHandle);
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
		ensureMsgf(SceneSelectedActors.Num() < 2, TEXT("Multi Selection is not supported, but [%s] Actors are selected... Choosing the first one"));

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

void FChaosVDWorldOutlinerMode::HandleActorLabelChanged(AActor* ChangedActor)
{
	if (!ensure(ChangedActor))
	{
		return;
	}

	if (IsActorDisplayable(ChangedActor) && RepresentingWorld.Get() == ChangedActor->GetWorld())
	{
		// Force create the item otherwise the outliner may not be notified of a change to the item if it is filtered out
		if (FSceneOutlinerTreeItemPtr Item = CreateItemFor<FActorTreeItem>(ChangedActor, true))
		{
			SceneOutliner->OnItemLabelChanged(Item);
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

		FSceneOutlinerTreeItemPtr ObjectItem = SceneOutliner->GetTreeItem(SelectedActors[0]);
		SceneOutliner->SetItemSelection(ObjectItem, true, ESelectInfo::OnMouseClick);
	}
}
