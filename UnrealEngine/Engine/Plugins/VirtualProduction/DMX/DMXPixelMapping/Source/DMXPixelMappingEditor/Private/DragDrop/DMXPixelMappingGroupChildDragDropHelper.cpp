// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragDrop/DMXPixelMappingGroupChildDragDropHelper.h"

#include "DMXPixelMappingComponentWidget.h"
#include "DMXPixelMappingEditorUtils.h"
#include "DragDrop/DMXPixelMappingDragDropOp.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"

#include "Layout/ArrangedWidget.h"


TSharedPtr<FDMXPixelMappingGroupChildDragDropHelper> FDMXPixelMappingGroupChildDragDropHelper::Create(TSharedRef<FDragDropOperation> InPixelMappingDragDropOp)
{
	TSharedRef<FDMXPixelMappingGroupChildDragDropHelper> NewHelper = MakeShared<FDMXPixelMappingGroupChildDragDropHelper>();

	TSharedRef<FDMXPixelMappingDragDropOp> DragDropOp = StaticCastSharedRef<FDMXPixelMappingDragDropOp>(InPixelMappingDragDropOp);
	NewHelper->WeakPixelMappingDragDropOp = DragDropOp;

	// No dragged items
	if (DragDropOp->GetDraggedComponents().Num() == 0)
	{
		return nullptr;
	}

	// No parent group
	UDMXPixelMappingFixtureGroupComponent* ParentGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(DragDropOp->GetDraggedComponents()[0]->GetParent());
	if (!ParentGroupComponent)
	{
		return nullptr;
	}
	NewHelper->WeakParentGroupComponent = ParentGroupComponent;
	NewHelper->ParentPosition = ParentGroupComponent->GetPosition();
	NewHelper->ParentSize = ParentGroupComponent->GetSize();

	for (const TWeakObjectPtr<UDMXPixelMappingBaseComponent>& BaseComponent : DragDropOp->GetDraggedComponents())
	{	
		if (UDMXPixelMappingMatrixCellComponent* MatrixCellComponent = Cast<UDMXPixelMappingMatrixCellComponent>(BaseComponent.Get()))
		{		
			// For matrices, don't drag the childs along, they adopt their position from the parent
			continue;
		}

		UDMXPixelMappingOutputComponent* OutputComponent = [BaseComponent]() -> UDMXPixelMappingOutputComponent*
		{
			// Either a matrices or group items
			if ((BaseComponent.IsValid() && BaseComponent->GetClass() == UDMXPixelMappingFixtureGroupItemComponent::StaticClass()) ||
				(BaseComponent.IsValid() && BaseComponent->GetClass() == UDMXPixelMappingMatrixComponent::StaticClass()))
			{
				return Cast<UDMXPixelMappingOutputComponent>(BaseComponent);
			}

			return nullptr;
		}();


		// Invalid component or different parents
		if (!OutputComponent || OutputComponent->GetParent() != ParentGroupComponent)
		{
			return nullptr;
		}

		NewHelper->WeakChildComponents.Add(OutputComponent);
	}

	return NewHelper;
}

void FDMXPixelMappingGroupChildDragDropHelper::LayoutComponents(const FVector2D& Position, bool bAlignCells)
{
	if (bAlignCells)
	{
		LayoutAligned(Position);
	}
	else
	{
		LayoutUnaligned(Position);
	}
}

void FDMXPixelMappingGroupChildDragDropHelper::LayoutAligned(const FVector2D& GraphSpacePosition)
{
	if (TSharedPtr<FDMXPixelMappingDragDropOp> DragDropOp = WeakPixelMappingDragDropOp.Pin())
	{
		if (UDMXPixelMappingFixtureGroupComponent* GroupComponent = WeakParentGroupComponent.Get())
		{
			FVector2D NextPosition = GraphSpacePosition - DragDropOp->GraphSpaceDragOffset;
			float RowHeight = 0.f;

			for (const TWeakObjectPtr<UDMXPixelMappingOutputComponent>& WeakChildComponent : WeakChildComponents)
			{
				if (UDMXPixelMappingOutputComponent* ChildComponent = WeakChildComponent.Get())
				{
					if (GroupComponent->IsOverPosition(NextPosition) && 
						GroupComponent->IsOverPosition(NextPosition + ChildComponent->GetSize()))
					{
						SetPosition(ChildComponent, NextPosition);

						RowHeight = FMath::Max(ChildComponent->GetSize().Y, RowHeight);
						NextPosition = FVector2D(NextPosition.X + ChildComponent->GetSize().X, NextPosition.Y);
					}
					else
					{
						// Try on a new row
						FVector2D NewRowPosition = FVector2D(GraphSpacePosition.X, NextPosition.Y + RowHeight);

						const FVector2D NextPositionOnNewRow = FVector2D(NewRowPosition.X + ChildComponent->GetSize().X, NewRowPosition.Y);
						if (GroupComponent->IsOverPosition(NextPositionOnNewRow) &&
							GroupComponent->IsOverPosition(NextPositionOnNewRow + ChildComponent->GetSize()))
						{
							SetPosition(ChildComponent, NewRowPosition);

							NextPosition = FVector2D(NewRowPosition.X + ChildComponent->GetSize().X, NewRowPosition.Y);
							RowHeight = ChildComponent->GetSize().Y;							
						}
						else
						{
							SetPosition(ChildComponent, NextPosition);

							NextPosition = FVector2D(NextPosition.X + ChildComponent->GetSize().X, NextPosition.Y);
						}
					}
				}
			}
		}
	}
}

void FDMXPixelMappingGroupChildDragDropHelper::LayoutUnaligned(const FVector2D& GraphSpacePosition)
{
	if (TSharedPtr<FDMXPixelMappingDragDropOp> PinnedDragDropOp = WeakPixelMappingDragDropOp.Pin())
	{
		if (WeakChildComponents.Num() > 0)
		{
			if (UDMXPixelMappingOutputComponent* FirstComponent = WeakChildComponents[0].Get())
			{
				const FVector2D Anchor = FirstComponent->GetPosition();

				// Move all grou items to their new position
				for (const TWeakObjectPtr<UDMXPixelMappingOutputComponent>& WeakOutputComponent : WeakChildComponents)
				{
					if (UDMXPixelMappingOutputComponent* ChildComponent = WeakOutputComponent.Get())
					{
						FVector2D AnchorOffset = Anchor - ChildComponent->GetPosition();

						FVector2D NewPosition = GraphSpacePosition - AnchorOffset - PinnedDragDropOp->GraphSpaceDragOffset;
						SetPosition(ChildComponent, NewPosition);
					}
				}
			}
		}
	}
}

void FDMXPixelMappingGroupChildDragDropHelper::SetPosition(UDMXPixelMappingOutputComponent* Component, const FVector2D& Position) const
{
	if (Component)
	{
		Component->Modify();

		Component->SetPosition(Position);
	}
}
