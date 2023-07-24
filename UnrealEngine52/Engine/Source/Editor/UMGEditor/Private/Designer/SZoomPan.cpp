// Copyright Epic Games, Inc. All Rights Reserved.

#include "Designer/SZoomPan.h"

#include "Layout/ArrangedChildren.h"
#include "Layout/Children.h"
#include "Layout/Geometry.h"
#include "Layout/LayoutUtils.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Types/SlateEnums.h"
#include "Widgets/SWidget.h"

/////////////////////////////////////////////////////
// SZoomPan

void SZoomPan::Construct(const FArguments& InArgs)
{
	bHasRelativeLayoutScale = true;

	ViewOffset = InArgs._ViewOffset;
	ZoomAmount = InArgs._ZoomAmount;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void SZoomPan::SetContent(const TSharedRef< SWidget >& InContent)
{
	ChildSlot
	[
		InContent
	];
}

void SZoomPan::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ArrangedChildren.Accepts(ChildVisibility) )
	{
		const FMargin SlotPadding(ChildSlot.GetPadding());
		AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(AllottedGeometry.Size.X, ChildSlot, SlotPadding, 1);
		AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.Size.Y, ChildSlot, SlotPadding, 1);

		ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild(
				ChildSlot.GetWidget(),
				FVector2D(XResult.Offset, YResult.Offset) - ViewOffset.Get(),
				ChildSlot.GetWidget()->GetDesiredSize(),
				ZoomAmount.Get()
		) );
	}
}

float SZoomPan::GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const
{
	return ZoomAmount.Get();
}
