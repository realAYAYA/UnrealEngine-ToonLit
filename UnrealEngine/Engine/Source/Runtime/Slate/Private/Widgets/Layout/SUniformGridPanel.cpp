// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SUniformGridPanel.h"
#include "Layout/LayoutUtils.h"

SUniformGridPanel::SUniformGridPanel()
	: Children(this)
	, SlotPadding(*this, FMargin(0.0f))
	, MinDesiredSlotWidth(*this, 0.f)
	, MinDesiredSlotHeight(*this, 0.f)
{
}

SUniformGridPanel::FSlot::FSlotArguments SUniformGridPanel::Slot(int32 Column, int32 Row)
{
	return FSlot::FSlotArguments(MakeUnique<FSlot>(Column, Row));
}

void SUniformGridPanel::Construct( const FArguments& InArgs )
{
	SlotPadding.Assign(*this, InArgs._SlotPadding);
	NumColumns = 0;
	NumRows = 0;
	MinDesiredSlotWidth.Assign(*this, InArgs._MinDesiredSlotWidth);
	MinDesiredSlotHeight.Assign(*this, InArgs._MinDesiredSlotHeight);

	Children.AddSlots(MoveTemp(const_cast<TArray<FSlot::FSlotArguments>&>(InArgs._Slots)));
}

void SUniformGridPanel::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	if ( Children.Num() > 0 )
	{
		const FVector2D CellSize(AllottedGeometry.GetLocalSize().X / NumColumns, AllottedGeometry.GetLocalSize().Y / NumRows);
		const FMargin& CurrentSlotPadding(SlotPadding.Get());
		for ( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
		{
			const FSlot& Child = Children[ChildIndex];
			const EVisibility ChildVisibility = Child.GetWidget()->GetVisibility();
			if ( ArrangedChildren.Accepts(ChildVisibility) )
			{
				// Do the standard arrangement of elements within a slot
				// Takes care of alignment and padding.
				AlignmentArrangeResult XAxisResult = AlignChild<Orient_Horizontal>(CellSize.X, Child, CurrentSlotPadding);
				AlignmentArrangeResult YAxisResult = AlignChild<Orient_Vertical>(CellSize.Y, Child, CurrentSlotPadding);

				ArrangedChildren.AddWidget(ChildVisibility,
					AllottedGeometry.MakeChild(Child.GetWidget(),
					FVector2D(CellSize.X*Child.GetColumn() + XAxisResult.Offset, CellSize.Y*Child.GetRow() + YAxisResult.Offset),
					FVector2D(XAxisResult.Size, YAxisResult.Size)
					));
			}

		}
	}
}

FVector2D SUniformGridPanel::ComputeDesiredSize( float ) const
{
	FVector2D MaxChildDesiredSize = FVector2D::ZeroVector;
	const FVector2D SlotPaddingDesiredSize = SlotPadding.Get().GetDesiredSize();
	
	const float CachedMinDesiredSlotWidth = MinDesiredSlotWidth.Get();
	const float CachedMinDesiredSlotHeight = MinDesiredSlotHeight.Get();
	
	NumColumns = 0;
	NumRows = 0;

	for ( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		const FSlot& Child = Children[ ChildIndex ];

		if (Child.GetWidget()->GetVisibility() != EVisibility::Collapsed)
		{
			// A single cell at (N,M) means our grid size is (N+1, M+1)
			NumColumns = FMath::Max(Child.GetColumn() + 1, NumColumns);
			NumRows = FMath::Max(Child.GetRow() + 1, NumRows);

			FVector2D ChildDesiredSize = Child.GetWidget()->GetDesiredSize() + SlotPaddingDesiredSize;

			ChildDesiredSize.X = FMath::Max( ChildDesiredSize.X, CachedMinDesiredSlotWidth);
			ChildDesiredSize.Y = FMath::Max( ChildDesiredSize.Y, CachedMinDesiredSlotHeight);

			MaxChildDesiredSize.X = FMath::Max( MaxChildDesiredSize.X, ChildDesiredSize.X );
			MaxChildDesiredSize.Y = FMath::Max( MaxChildDesiredSize.Y, ChildDesiredSize.Y );
		}
	}

	return FVector2D( NumColumns*MaxChildDesiredSize.X, NumRows*MaxChildDesiredSize.Y );
}

FChildren* SUniformGridPanel::GetChildren()
{
	return &Children;
}

void SUniformGridPanel::SetSlotPadding(TAttribute<FMargin> InSlotPadding)
{
	SlotPadding.Assign(*this, MoveTemp(InSlotPadding));
}

void SUniformGridPanel::SetMinDesiredSlotWidth(TAttribute<float> InMinDesiredSlotWidth)
{
	MinDesiredSlotWidth.Assign(*this, MoveTemp(InMinDesiredSlotWidth));
}

void SUniformGridPanel::SetMinDesiredSlotHeight(TAttribute<float> InMinDesiredSlotHeight)
{
	MinDesiredSlotHeight.Assign(*this, MoveTemp(InMinDesiredSlotHeight));
}

SUniformGridPanel::FScopedWidgetSlotArguments SUniformGridPanel::AddSlot( int32 Column, int32 Row )
{
	return FScopedWidgetSlotArguments{ MakeUnique<FSlot>(Column, Row), Children, INDEX_NONE };
}

bool SUniformGridPanel::RemoveSlot( const TSharedRef<SWidget>& SlotWidget )
{
	return Children.Remove(SlotWidget) != INDEX_NONE;
}

void SUniformGridPanel::ClearChildren()
{
	NumColumns = 0;
	NumRows = 0;
	Children.Empty();
}
