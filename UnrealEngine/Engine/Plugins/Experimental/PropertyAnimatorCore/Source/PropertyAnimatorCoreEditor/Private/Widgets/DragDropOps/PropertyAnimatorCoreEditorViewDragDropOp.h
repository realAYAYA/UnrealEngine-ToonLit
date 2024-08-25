// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Widgets/Views/PropertyAnimatorCoreEditorViewItem.h"

/** A custom drag operation for properties/controllers */
class FPropertyAnimatorCoreEditorViewDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FPropertyAnimatorCoreEditorViewDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FPropertyAnimatorCoreEditorViewDragDropOp> New(const TSet<FPropertiesViewControllerItem>& InItems)
	{
		TSharedRef<FPropertyAnimatorCoreEditorViewDragDropOp> DragDropOp = MakeShared<FPropertyAnimatorCoreEditorViewDragDropOp>();
		DragDropOp->DraggedItems = InItems;
		return DragDropOp;
	}

	const TSet<FPropertiesViewControllerItem>& GetDraggedItems() const
	{
		return DraggedItems;
	}

protected:
	TSet<FPropertiesViewControllerItem> DraggedItems;
};