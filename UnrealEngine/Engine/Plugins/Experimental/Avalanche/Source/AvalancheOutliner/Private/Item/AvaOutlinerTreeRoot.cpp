// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/AvaOutlinerTreeRoot.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetSelection.h"
#include "AvaOutliner.h"
#include "AvaOutlinerSubsystem.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragDropOps/AvaOutlinerItemDragDropOp.h"
#include "EngineUtils.h"
#include "Item/AvaOutlinerActor.h"
#include "ItemActions/AvaOutlinerAddItem.h"
#include "Textures/SlateIcon.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/STableRow.h"

void FAvaOutlinerTreeRoot::FindChildren(TArray<FAvaOutlinerItemPtr>& OutChildren, bool bRecursive)
{
	Super::FindChildren(OutChildren, bRecursive);

	UWorld* const World = Outliner.GetWorld();
	if (!World)
	{
		return;
	}

	FAvaOutliner& OutlinerPrivate = static_cast<FAvaOutliner&>(Outliner);

	// All the Actors that don't have an Outliner Parent will be at the Root
	const TArray<TWeakObjectPtr<AActor>> RootChildren = OutlinerPrivate.GetActorSceneOutlinerChildren(nullptr);

	// Worst case and most likely case: all Outliner Children are valid.
	// Note: if recursive, re-allocations will still need to be done past this reserve count, as it's unknown at this point how many items are going to be added
	OutChildren.Reserve(OutChildren.Num() + RootChildren.Num());

	for (const TWeakObjectPtr<AActor>& ChildWeak : RootChildren)
	{
		if (AActor* const Child = ChildWeak.Get())
		{
			const FAvaOutlinerItemPtr ChildActorItem = Outliner.FindOrAdd<FAvaOutlinerActor>(Child);
			const FAvaOutlinerItemFlagGuard Guard(ChildActorItem, EAvaOutlinerItemFlags::IgnorePendingKill);
			OutChildren.Add(ChildActorItem);
			if (bRecursive)
			{
				ChildActorItem->FindChildren(OutChildren, bRecursive);
			}
		}
	}
}

bool FAvaOutlinerTreeRoot::CanAddChild(const FAvaOutlinerItemPtr& InChild) const
{
	return Super::CanAddChild(InChild)
		&& InChild->CanBeTopLevel();
}

bool FAvaOutlinerTreeRoot::AddChild(const FAvaOutlinerAddItemParams& InAddItemParams)
{
	// if current parent is the root, it means it's just rearranging
	const bool bRearranging = InAddItemParams.Item.IsValid() && InAddItemParams.Item->GetParent().Get() == this;

	// Is it a new actor that we just spawned
	const bool bSpawning = InAddItemParams.Item.IsValid() && !Children.Contains(InAddItemParams.Item);

	const bool bResult = FAvaOutlinerItem::AddChild(InAddItemParams);

	if ((bSpawning || bRearranging) && InAddItemParams.Item->IsA<FAvaOutlinerActor>())
	{
		FAvaOutliner& OutlinerPrivate = static_cast<FAvaOutliner&>(Outliner);

		UAvaOutlinerSubsystem* const OutlinerSubsystem = OutlinerPrivate.GetOutlinerSubsystem();
		
		AActor* const Actor = StaticCastSharedPtr<FAvaOutlinerActor>(InAddItemParams.Item)->GetActor();
		
		if (OutlinerSubsystem && Actor)
		{
			const EAvaOutlinerHierarchyChangeType Type = bSpawning ? EAvaOutlinerHierarchyChangeType::Attached : EAvaOutlinerHierarchyChangeType::Rearranged;
			
			OutlinerSubsystem->BroadcastActorHierarchyChanged(Actor, nullptr, Type);
		}
	}
	
	return bResult;
}

bool FAvaOutlinerTreeRoot::IsAllowedInOutliner() const
{
	checkNoEntry();
	return false;
}

FText FAvaOutlinerTreeRoot::GetDisplayName() const
{
	checkNoEntry();
	return FText::GetEmpty();
}

FText FAvaOutlinerTreeRoot::GetClassName() const
{
	checkNoEntry();
	return FText::GetEmpty();
}

FText FAvaOutlinerTreeRoot::GetIconTooltipText() const
{
	checkNoEntry();
	return FText::GetEmpty();
}

FSlateIcon FAvaOutlinerTreeRoot::GetIcon() const
{
	checkNoEntry();
	return FSlateIcon();
}

TSharedRef<SWidget> FAvaOutlinerTreeRoot::GenerateLabelWidget(const TSharedRef<SAvaOutlinerTreeRow>& InRow)
{
	checkNoEntry();
	return SNullWidget::NullWidget;
}

bool FAvaOutlinerTreeRoot::CanRename() const
{
	checkNoEntry();
	return false;
}

bool FAvaOutlinerTreeRoot::Rename(const FString& InName)
{
	checkNoEntry();
	return false;
}

TOptional<EItemDropZone> FAvaOutlinerTreeRoot::CanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone)
{
	if (const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = InDragDropEvent.GetOperationAs<FAssetDragDropOp>())
	{
		//Can't drop if a Single of the Assets is an Invalid Asset
		for (const FAssetData& Asset : AssetDragDropOp->GetAssets())
		{
			UActorFactory* ActorFactory = AssetDragDropOp->GetActorFactory();
			if (!ActorFactory)
			{
				ActorFactory = FActorFactoryAssetProxy::GetFactoryForAsset(Asset);
			}
			if (!ActorFactory || !ActorFactory->CanPlaceElementsFromAssetData(Asset))
			{
				return TOptional<EItemDropZone>();
			}
		}
		return InDropZone;
	}

	return Super::CanAcceptDrop(InDragDropEvent, InDropZone);
}

FReply FAvaOutlinerTreeRoot::AcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone)
{
	// Force Tree Root to always be Onto Item so we always create new Items as Children of Root
	const EItemDropZone ForcedDropZone = EItemDropZone::OntoItem;

	if (const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = InDragDropEvent.GetOperationAs<FAssetDragDropOp>())
	{
		const UWorld* const World = Outliner.GetWorld();
		check(World);
		return CreateItemsFromAssetDrop(AssetDragDropOp, ForcedDropZone, World->GetCurrentLevel());
	}

	return Super::AcceptDrop(InDragDropEvent, ForcedDropZone);
}

FAvaOutlinerItemId FAvaOutlinerTreeRoot::CalculateItemId() const
{
	return FAvaOutlinerItemId(TEXT("OutlinerRoot"));
}
