// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FPaintArgs;
class FSlateWindowElementList;
class FTimeSliderController;


// a timeline view which renders a block for in the timeline where the object exists
class TOOLWIDGETS_API SSegmentedTimelineView : public SCompoundWidget
{
public:
	struct FSegmentData
	{
	public:
		TArray<TRange<double>> Segments;
		// This is an optional array of alternating colors. Every segment will use the next color in this array and will wrap around
		// if the number of colors provided is > than the number of segments.
		TOptional<TArray<FLinearColor>> AlternatingSegmentsColors;
	};

	SLATE_BEGIN_ARGS(SSegmentedTimelineView)
		: _ViewRange(TRange<double>(0,10))
		, _DesiredSize(FVector2D(100.f,20.f))
	{}
    	/** View time range */
    	SLATE_ATTRIBUTE(TRange<double>, ViewRange);
	
		/** Existence Time Range */
		SLATE_ATTRIBUTE(TSharedPtr<FSegmentData>, SegmentData);

		/** Desired widget size */
		SLATE_ATTRIBUTE(FVector2D, DesiredSize);
	
		/** Fill Color */
		SLATE_ATTRIBUTE(FLinearColor, FillColor);
     
	SLATE_END_ARGS()


	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

protected:
	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	
	int32 PaintBlock(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const;
	
	TAttribute<TRange<double>> ViewRange;
	TAttribute<TSharedPtr<FSegmentData>> SegmentData;
	TAttribute<FVector2D> DesiredSize;
	TAttribute<FLinearColor> FillColor;
	
	const FSlateBrush* WhiteBrush;
};

