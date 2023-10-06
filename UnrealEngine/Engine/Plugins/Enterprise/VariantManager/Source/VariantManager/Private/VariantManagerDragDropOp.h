// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"

class FVariantManagerDisplayNode;


class FVariantManagerDragDropOp : public FDecoratedDragDropOp
{
public:

	DRAG_DROP_OPERATOR_TYPE(FVariantManagerDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FVariantManagerDragDropOp> New(TArray<TSharedRef<FVariantManagerDisplayNode>>& InDraggedNodes);

	TArray<TSharedRef<FVariantManagerDisplayNode>>& GetDraggedNodes();

private:

	// Force usage of factory method
	FVariantManagerDragDropOp() {}

private:

	TArray<TSharedRef<FVariantManagerDisplayNode>> DraggedNodes;
};
