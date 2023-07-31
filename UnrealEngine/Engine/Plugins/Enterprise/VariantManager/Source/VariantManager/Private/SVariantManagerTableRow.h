// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "DisplayNodes/VariantManagerDisplayNode.h"
#include "Widgets/Views/STableRow.h"

typedef TSharedRef<FVariantManagerDisplayNode> FDisplayNodeRef;

/** Represents a row in the VariantManager's tree views and list views */
class SVariantManagerTableRow : public STableRow<FDisplayNodeRef>
{
public:
	SLATE_BEGIN_ARGS(SVariantManagerTableRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedRef<FVariantManagerDisplayNode>& InNode);

	TSharedPtr<FVariantManagerDisplayNode> GetDisplayNode() const
	{
		return Node.Pin();
	}

	// We subscribe these to base class STableRow's events
	FReply DragDetected(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent);
	void DragLeave(const FDragDropEvent& DragDropEvent);
	TOptional<EItemDropZone> CanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, FDisplayNodeRef DisplayNode);
	FReply AcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, FDisplayNodeRef DisplayNode);

	// STableRow interface
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	//~ End STableRow interface

private:

	mutable TWeakPtr<FVariantManagerDisplayNode> Node;
};