// Copyright Epic Games, Inc. All Rights Reserved.

#include "SEventTimelineView.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "SSimpleTimeSlider.h"

#define LOCTEXT_NAMESPACE "SEventTimelineView"


void SEventTimelineView::Construct( const SEventTimelineView::FArguments& InArgs )
{
    ViewRange = InArgs._ViewRange;
	DesiredSize = InArgs._DesiredSize;
	EventData = InArgs._EventData;

	// set clipping on by default, since the OnPaint function is drawing outside the bounds
	// Clipping = EWidgetClipping::ClipToBounds;
	EventBrush = FAppStyle::GetBrush("Sequencer.KeyDiamond");
	EventBorderBrush = FAppStyle::GetBrush("Sequencer.KeyDiamondBorder");
}

int32 SEventTimelineView::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 NewLayer = PaintEvents( AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

	return FMath::Max( NewLayer, SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NewLayer, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) ) );
}


FVector2D SEventTimelineView::ComputeDesiredSize( float ) const
{
	return DesiredSize.Get();
}

int32 SEventTimelineView::PaintEvents(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	TRange<double> DebugTimeRange = ViewRange.Get();
	
	SSimpleTimeSlider::FScrubRangeToScreen RangeToScreen( DebugTimeRange, AllottedGeometry.GetLocalSize() );
	FVector2D Size = AllottedGeometry.GetLocalSize();

	if (TSharedPtr<FTimelineEventData> Event = EventData.Get())
	{
		auto &EventPoints = Event->Points;
		const int NumPoints = EventPoints.Num();
		
		const ESlateDrawEffect LineDrawEffects = ESlateDrawEffect::NoPixelSnapping;
		
		double PrevPointTime = 0;
		int OverlappingPointCount = 0;

		if (NumPoints > 0)
		{
			for(int i=0; i<NumPoints; i++)
			{
				const FTimelineEventData::EventPoint& Point = EventPoints[i];
			
				FVector2D EventSize = EventBrush->GetImageSize();
				
				float X = RangeToScreen.InputToLocalX(Point.Time);
				X = X-EventSize.X/2;
				float Y = (AllottedGeometry.Size.Y - EventSize.Y)/2;
				if (Point.Time == PrevPointTime)
				{
					OverlappingPointCount++;

					static const int OverlapOffsetAmount = 2;
					Y += OverlapOffsetAmount * OverlappingPointCount;
				}
				else
				{
					OverlappingPointCount = 0;
				}
				
				PrevPointTime = Point.Time;
				
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(X, Y) , EventSize, 1.0), EventBrush, ESlateDrawEffect::None, Point.Color);
				
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(X, Y) , EventSize, 1.0), EventBorderBrush, ESlateDrawEffect::None, FLinearColor::Black); 
			}
		}
		
		auto &EventWindows = Event->Windows;
		const int NumWindows = EventWindows.Num();
		
	
		const FSlateBrush* Brush = FAppStyle::GetBrush("Sequencer.SectionArea.Background");

		
		
		if (NumWindows > 0)
		{
			for(int i=0; i<NumWindows; i++)
			{
				const FTimelineEventData::EventWindow& Window = EventWindows[i];
			
				FVector2D EventSize = EventBrush->GetImageSize();
				
				float XStart = RangeToScreen.InputToLocalX(Window.TimeStart);
				float XStartDiamond = XStart - EventSize.X/2;
				float XEnd = RangeToScreen.InputToLocalX(Window.TimeEnd);
				float XEndDiamond = XEnd - EventSize.X/2;
				float Y = (AllottedGeometry.Size.Y - EventSize.Y)/2;

				// window bar
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId++,
					AllottedGeometry.ToPaintGeometry(FVector2D(XStart,Y + 1),FVector2D(XEnd-XStart, EventSize.Y-2), 1), Brush, ESlateDrawEffect::None, Window.Color);

				// key diamond at start
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(XStartDiamond, Y) , EventSize, 1.0), EventBrush, ESlateDrawEffect::None, Window.Color);
				
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(XStartDiamond, Y) , EventSize, 1.0), EventBorderBrush, ESlateDrawEffect::None, FLinearColor::Black); 

				// key diamond at end
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(XEndDiamond, Y) , EventSize, 1.0), EventBrush, ESlateDrawEffect::None, Window.Color);
				
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2D(XEndDiamond, Y) , EventSize, 1.0), EventBorderBrush, ESlateDrawEffect::None, FLinearColor::Black);
			}
		}
		

		LayerId++;
	}

	return LayerId;
}

#undef LOCTEXT_NAMESPACE
