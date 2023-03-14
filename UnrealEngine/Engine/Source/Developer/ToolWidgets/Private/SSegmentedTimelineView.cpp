// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSegmentedTimelineView.h"
#include "Rendering/DrawElements.h"
#include "SSimpleTimeSlider.h"

#define LOCTEXT_NAMESPACE "SSegmentedTimelineView"

void SSegmentedTimelineView::Construct(const SSegmentedTimelineView::FArguments& InArgs)
{
	ViewRange = InArgs._ViewRange;
	FillColor = InArgs._FillColor;
	DesiredSize = InArgs._DesiredSize;
	SegmentData = InArgs._SegmentData;
}

int32 SSegmentedTimelineView::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 NewLayer = PaintBlock(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	return FMath::Max(NewLayer, SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NewLayer, InWidgetStyle, ShouldBeEnabled(bParentEnabled)));
}


FVector2D SSegmentedTimelineView::ComputeDesiredSize(float) const
{
	return DesiredSize.Get();
}

int32 SSegmentedTimelineView::PaintBlock(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// convert time range to from rewind debugger times to profiler times
	TRange<double> DebugTimeRange = ViewRange.Get();
	FLinearColor Color = FillColor.Get();

	SSimpleTimeSlider::FScrubRangeToScreen RangeToScreen(DebugTimeRange, AllottedGeometry.GetLocalSize());
	FVector2D Size = AllottedGeometry.GetLocalSize();

	if (TSharedPtr<FSegmentData> SegmentDataPtr = SegmentData.Get())
	{
		for (const TRange<double>& SegmentRange : SegmentDataPtr->Segments)
		{
			double LowerBound = SegmentRange.HasLowerBound() ? SegmentRange.GetLowerBoundValue() : 0;
			double UpperBound = SegmentRange.HasUpperBound() ? SegmentRange.GetUpperBoundValue() : Size.X;
			float BoxMin = FMath::Max(0, RangeToScreen.InputToLocalX(LowerBound));
			float BoxMax = FMath::Min(Size.X, RangeToScreen.InputToLocalX(UpperBound));

			FPaintGeometry BoxGeometry = AllottedGeometry.ToPaintGeometry(FVector2D(BoxMin, 1), FVector2D(BoxMax - BoxMin, Size.Y - 2), 1);

			const FSlateBrush* Brush = FAppStyle::GetBrush("Sequencer.SectionArea.Background");

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, BoxGeometry, Brush, ESlateDrawEffect::None, Color);
		}
		
		++LayerId;
	}

	return LayerId;
}

#undef LOCTEXT_NAMESPACE
