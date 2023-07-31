// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FPaintArgs;
class FSlateWindowElementList;
class FTimeSliderController;

class SEventTimelineView : public SCompoundWidget
{
public:
	struct FTimelineEventData
	{
	public:
		struct EventPoint
		{
			double Time;
			FText Type;
			FText Description;
			FLinearColor Color;
		};
		
		struct EventWindow
		{
			double TimeStart;
			double TimeEnd;
			FText Type;
			FText Description;
			FLinearColor Color;
		};

		TArray<EventPoint> Points;
		TArray<EventWindow> Windows;
	};
	
	SLATE_BEGIN_ARGS(SEventTimelineView)
		: _ViewRange(TRange<double>(0,10))
		, _DesiredSize(FVector2D(100.f,20.f))
	{}
    	/** View time range */
    	SLATE_ATTRIBUTE(TRange<double>, ViewRange);

		/** Data for events to render */
		SLATE_ATTRIBUTE(TSharedPtr<FTimelineEventData>, EventData);

		/** Desired widget size */
		SLATE_ATTRIBUTE(FVector2D, DesiredSize);
	
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
	
	int32 PaintEvents(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const;
	
	TAttribute<TRange<double>> ViewRange;
	TAttribute<TSharedPtr<FTimelineEventData>> EventData;
	TAttribute<FVector2D> DesiredSize;
	
	const FSlateBrush* EventBrush;
	const FSlateBrush* EventBorderBrush;
};

