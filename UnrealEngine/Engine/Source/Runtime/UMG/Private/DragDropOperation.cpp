// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/DragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DragDropOperation)

/////////////////////////////////////////////////////
// UDragDropOperation

UDragDropOperation::UDragDropOperation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Pivot = EDragPivot::CenterCenter;
}

/// @cond DOXYGEN_WARNINGS

void UDragDropOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	OnDrop.Broadcast(this);
}

void UDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	OnDragCancelled.Broadcast(this);
}

void UDragDropOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
	OnDragged.Broadcast(this);
}

/// @endcond

