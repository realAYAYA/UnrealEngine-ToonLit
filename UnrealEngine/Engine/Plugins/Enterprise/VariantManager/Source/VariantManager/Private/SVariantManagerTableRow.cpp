// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVariantManagerTableRow.h"

#include "SVariantManager.h"
#include "VariantManager.h"
#include "VariantManagerDragDropOp.h"
#include "VariantManagerSelection.h"
#include "DisplayNodes/VariantManagerPropertyNode.h"

#include "CoreMinimal.h"
#include "Framework/Commands/GenericCommands.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "SVariantManagerTableRow"

/** Construct function for this widget */
void SVariantManagerTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedRef<FVariantManagerDisplayNode>& InNode)
{
	Node = InNode;
	bool bIsSelectable = InNode->IsSelectable();

	STableRow::Construct(
		STableRow::FArguments()
		.Padding(0)
		.Style(InNode->GetRowStyle())
		.OnDragDetected(this, &SVariantManagerTableRow::DragDetected)
		.OnCanAcceptDrop(this, &SVariantManagerTableRow::CanAcceptDrop)
		.OnAcceptDrop(this, &SVariantManagerTableRow::AcceptDrop)
		.OnDragLeave(this, &SVariantManagerTableRow::DragLeave)
		.ShowSelection(bIsSelectable),
		OwnerTableView);

	SetRowContent(InNode->GetCustomOutlinerContent(SharedThis(this)));
}

FReply SVariantManagerTableRow::DragDetected( const FGeometry& InGeometry, const FPointerEvent& InPointerEvent )
{
	TSharedPtr<FVariantManagerDisplayNode> PinnedNode = Node.Pin();
	TSharedPtr<FVariantManager> VarMan = PinnedNode->GetVariantManager().Pin();

	const FSlateBrush* Icon = PinnedNode->GetIconBrush();

	if (PinnedNode.IsValid() && VarMan.IsValid())
	{
		FVariantManagerSelection& Selection = VarMan->GetSelection();

		TArray<FDisplayNodeRef> DraggableNodes;
		FText DefaultHoverText;

		// We'll drag a group of nodes based on what type we are (e.g. if we're variant or variant
		// set, drag all of those)
		switch (PinnedNode->GetType())
		{
		case EVariantManagerNodeType::Actor:
		{
			for (const FDisplayNodeRef SelectedNode : Selection.GetSelectedActorNodes())
			{
				if (SelectedNode->CanDrag())
				{
					DraggableNodes.Add(SelectedNode);
				}
			}

			int32 NumNodes = DraggableNodes.Num();
			if (NumNodes > 1)
			{
				Icon = FSlateIconFinder::FindIconForClass(AActor::StaticClass()).GetOptionalIcon();
			}

			DefaultHoverText = FText::Format(NSLOCTEXT("VariantManagerTableRow", "DragActorNode", "{0} actor {0}|plural(one=binding,other=bindings)" ), NumNodes);
			break;
		}
		case EVariantManagerNodeType::Function:
		case EVariantManagerNodeType::Property:
		{
			if (PinnedNode.IsValid() && PinnedNode->CanDrag())
			{
				// We only support dragging one node property/function nodes for now
				ensure(DraggableNodes.Num() == 0);

				DraggableNodes.Add(PinnedNode.ToSharedRef());
				DefaultHoverText = FText::Format(NSLOCTEXT("VariantManagerTableRow", "DragPropertyNode", "{0}"), PinnedNode->GetDisplayName());
			}
			break;
		}
		case EVariantManagerNodeType::Variant:  // Intended fallthrough
		case EVariantManagerNodeType::VariantSet:
		{
			for (const FDisplayNodeRef& SelectedNode : Selection.GetSelectedOutlinerNodes())
			{
				if (SelectedNode->CanDrag())
				{
					DraggableNodes.Add(SelectedNode);
				}
			}

			int32 NumNodes = DraggableNodes.Num();

			DefaultHoverText = FText::Format(NSLOCTEXT("VariantManagerTableRow", "DragVariants", "{0} {0}|plural(one=variant,other=variants) and/or variant {0}|plural(one=set,other=sets)" ),
				NumNodes);

			break;
		}
		default:
			break;
		}

		if (DraggableNodes.Num() == 0)
		{
			return FReply::Unhandled();
		}

		VarMan->GetVariantManagerWidget()->SortDisplayNodes(DraggableNodes);

		TSharedRef<FVariantManagerDragDropOp> DragDropOp = FVariantManagerDragDropOp::New(DraggableNodes);
		DragDropOp->SetToolTip(DefaultHoverText, Icon);
		DragDropOp->SetupDefaults();

		return FReply::Handled().BeginDragDrop(DragDropOp);
	}

	return FReply::Unhandled();
}

void SVariantManagerTableRow::DragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDecoratedDragDropOp> DecoratedDragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
	if (DecoratedDragDropOp.IsValid())
	{
		DecoratedDragDropOp->ResetToDefaultToolTip();
	}
}

TOptional<EItemDropZone> SVariantManagerTableRow::CanAcceptDrop( const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone, FDisplayNodeRef DisplayNode )
{
	TSharedPtr<FVariantManagerDisplayNode> PinnedNode = Node.Pin();
	if (PinnedNode.IsValid())
	{
		return PinnedNode->CanDrop(DragDropEvent, InItemDropZone);
	}

	return TOptional<EItemDropZone>();
}

FReply SVariantManagerTableRow::AcceptDrop( const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone, FDisplayNodeRef DisplayNode )
{
	TSharedPtr<FVariantManagerDisplayNode> PinnedNode = Node.Pin();
	if (PinnedNode.IsValid())
	{
		PinnedNode->Drop(DragDropEvent, InItemDropZone);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SVariantManagerTableRow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	STableRow::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);

	return Node.Pin()->OnDoubleClick(InMyGeometry, InMouseEvent);
}

// Small hack to bypass CanDrop calls to spacer nodes, letting the underlying tree handle the events instead
FReply SVariantManagerTableRow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FVariantManagerDisplayNode> PinnedNode = Node.Pin();
	if (PinnedNode.IsValid())
	{
		if (PinnedNode->GetType() == EVariantManagerNodeType::Spacer)
		{
			return FReply::Unhandled();
		}
	}

	return STableRow::OnDragOver(MyGeometry, DragDropEvent);
}

#undef LOCTEXT_NAMESPACE
