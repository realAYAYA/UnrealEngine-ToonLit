// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"

class FAvaBroadcastOutputTreeItem;

class FAvaBroadcastOutputTreeItemDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAvaOutputClassDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FAvaBroadcastOutputTreeItemDragDropOp> New(const TSharedPtr<FAvaBroadcastOutputTreeItem>& InOutputClassItem);

	bool IsValidToDropInChannel(FName InTargetChannelName) const;

	TSharedPtr<FAvaBroadcastOutputTreeItem> GetOutputTreeItem() const { return OutputTreeItem; }

	FReply OnChannelDrop(FName InTargetChannelName);

protected:
	void Init(const TSharedPtr<FAvaBroadcastOutputTreeItem>& InOutputClassItem);

	/** Keep Reference Count while Drag Dropping */
	TSharedPtr<FAvaBroadcastOutputTreeItem> OutputTreeItem;
};
