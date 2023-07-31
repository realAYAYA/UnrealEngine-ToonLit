// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

class FGraphNodeDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FGraphNodeDragDropOp, FDecoratedDragDropOp)

	DECLARE_DELEGATE_FourParams( FOnPerformDropToGraph, TSharedPtr<FGraphNodeDragDropOp>, class UEdGraph*, const FVector2D&, const FVector2D&);

	FOnPerformDropToGraph OnPerformDropToGraph;
};
