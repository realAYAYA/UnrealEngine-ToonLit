// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"

enum class EUsdDragDropOpType
{
    None,
    Prims,
    Layers,
    Attributes
};

class FUsdDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE( FUsdDragDropOp, FDecoratedDragDropOp )

    EUsdDragDropOpType OpType;
	TSet<TSharedRef<IUsdTreeViewItem>> DraggedItems;
};