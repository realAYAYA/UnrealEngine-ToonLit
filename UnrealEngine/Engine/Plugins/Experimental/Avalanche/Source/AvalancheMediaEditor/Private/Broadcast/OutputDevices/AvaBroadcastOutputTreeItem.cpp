// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaBroadcastOutputTreeItem.h"

#include "DragDropOps/AvaBroadcastOutputTreeItemDragDropOp.h"

const TWeakPtr<FAvaBroadcastOutputTreeItem>& FAvaBroadcastOutputTreeItem::GetParent() const
{
	return ParentWeak;
}

const TArray<FAvaOutputTreeItemPtr>& FAvaBroadcastOutputTreeItem::GetChildren() const
{
	return Children;
}

FReply FAvaBroadcastOutputTreeItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Handled().BeginDragDrop(FAvaBroadcastOutputTreeItemDragDropOp::New(SharedThis(this)));
	}
	return FReply::Unhandled();
}

void FAvaBroadcastOutputTreeItem::RefreshTree(const FAvaOutputTreeItemPtr& InItem)
{
	TArray<FAvaOutputTreeItemPtr> ItemsRemainingToRefresh;
	ItemsRemainingToRefresh.Add(InItem);
		
	while (ItemsRemainingToRefresh.Num() > 0)
	{
		FAvaOutputTreeItemPtr Item = ItemsRemainingToRefresh.Pop();
		if (Item.IsValid())
		{
			Item->RefreshChildren();
			ItemsRemainingToRefresh.Append(Item->GetChildren());
		}
	}
}
