// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAudioCurveView.h"
#include "Algo/MaxElement.h"
#include "Algo/MinElement.h"
#include "Audio.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "SAudioCurveView"

void SAudioCurveView::Construct( const SAudioCurveView::FArguments& InArgs )
{
	// A lot of this should go into a style 
    ViewRange = InArgs._ViewRange;
    GridLineColor = InArgs._GridLineColor;
    AxesLabelColor = InArgs._AxesLabelColor;
	YMargin = FMath::Clamp(InArgs._YMargin.Get(), 0.0f, 0.5f);
	HorizontalAxisIncrement = InArgs._HorizontalAxisIncrement;
	DesiredSize = InArgs._DesiredSize;
	YDataRange = FVector2f(0.0f, 1.0f);
	NumHorizontalGridLines = 10;

	// set clipping on by default, since the OnPaint function is drawing outside the bounds
	Clipping = EWidgetClipping::ClipToBounds;
	XValueFormattingOptions.MaximumFractionalDigits = 3;
	LineDrawEffects = ESlateDrawEffect::NoPixelSnapping;
	LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 6);

	SetToolTip(CreateCurveTooltip());
}

int32 SAudioCurveView::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 NewLayer = PaintCurves( AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

	return FMath::Max( NewLayer, SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NewLayer, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) ) );
}

FReply SAudioCurveView::OnMouseMove(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	UpdateCurveToolTip(InMyGeometry, InMouseEvent);
	return FReply::Handled();
}

TSharedRef<SToolTip> SAudioCurveView::CreateCurveTooltip()
{
	return SNew(SToolTip)
		.Visibility_Lambda([this]()
		{ 
			return MetadataPerCurve == nullptr ? EVisibility::Collapsed : EVisibility::Visible; 
		})
		.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.BrightBackground"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SAudioCurveView::GetCurveToolTipDisplayNameText)
				.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
				.ColorAndOpacity(FLinearColor::Black)
			]
			+ SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SAudioCurveView::GetCurveToolTipXValueText)
				.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
				.ColorAndOpacity(FLinearColor::Black)
			]
			+ SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SAudioCurveView::GetCurveToolTipYValueText)
				.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
				.ColorAndOpacity(FLinearColor::Black)
			]
		];
}

void SAudioCurveView::UpdateCurveToolTip(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMyGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()) && MetadataPerCurve)
	{
		// Mouse position in widget space
		const FVector2f HitPosition = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

		// Range helper struct
		const SSimpleTimeSlider::FScrubRangeToScreen RangeToScreen(ViewRange.Get(), InMyGeometry.GetLocalSize());

		// Mouse position from widget space to curve input space
		const double TargetX = RangeToScreen.LocalXToInput(HitPosition.X);

		// Keep track of closest curve index, closest point, value 
		int32 ClosestCurveId = INDEX_NONE;
		FCurvePoint ClosestPoint;
		float ClosestDistance = TNumericLimits<float>::Max();

		for (auto Iter = MetadataPerCurve->CreateConstIterator(); Iter; ++Iter)
		{
			const FCurveMetadata& CurveMetadata = Iter->Value;
			const int32 CurveId = CurveMetadata.CurveId;
			const TArray<FCurvePoint>* CurvePoints = PointDataPerCurve->Find(CurveId);
			const int32 NumPoints = CurvePoints->Num();
			if (NumPoints > 0)
			{
				for (int32 i = 1; i < NumPoints; ++i)
				{
					const FCurvePoint& Point1 = (*CurvePoints)[i - 1];
					const FCurvePoint& Point2 = (*CurvePoints)[i];

					// Find points that contain mouse hit-point x
					if (Point1.Key >= TargetX && TargetX <= Point2.Key)
					{
						// Choose point with the smallest x delta
						const float Delta1 = abs(TargetX - Point1.Key);
						const float Delta2 = abs(TargetX - Point2.Key);
						const FCurvePoint& TargetPoint = Delta1 < Delta2 ? Point1 : Point2;

						// Convert target point Y to widget space 
						const FVector2f LocalSize = InMyGeometry.GetLocalSize();
						const float WidgetSpaceY = ValueToLocalY(LocalSize, TargetPoint.Value);
						
						// Compare distance in widget space between HitPosition and closest point by x value on this curve
						const float Distance = FVector2f::Distance(HitPosition, FVector2f(RangeToScreen.InputToLocalX(TargetPoint.Key), WidgetSpaceY));

						if (Distance < ClosestDistance)
						{
							ClosestDistance = Distance;
							ClosestCurveId = CurveId;
							ClosestPoint = TargetPoint;
						}
						break;
					}
				}
			}
		}

		// Set tooltip text values  
		if (ClosestCurveId != INDEX_NONE)
		{
			CurveToolTipXValueText = FText::Format(LOCTEXT("CurveToolTipValueXFormat", "X: {0}"), FText::AsNumber(ClosestPoint.Key, &XValueFormattingOptions));
			CurveToolTipYValueText = FText::Format(LOCTEXT("CurveToolTipValueYFormat", "Y: {0}"), FText::AsNumber(ClosestPoint.Value, &YValueFormattingOptions));
			CurveToolTipDisplayNameText = MetadataPerCurve->Find(ClosestCurveId)->DisplayName;
		}
	}
}

FVector2D SAudioCurveView::ComputeDesiredSize( float ) const
{
	return DesiredSize.Get();
}

void SAudioCurveView::UpdateYDataRange()
{
	const static auto FCurvePointCompare = [](const FCurvePoint& A, const FCurvePoint& B) 
	{
		return A.Value < B.Value;
	};

	float MinValue = TNumericLimits<float>::Max();
	float MaxValue = TNumericLimits<float>::Min();
	for (auto Iter = PointDataPerCurve->CreateConstIterator(); Iter; ++Iter)
	{
		const TArray<FCurvePoint>& CurvePoints = Iter->Value;
		if (CurvePoints.IsEmpty())
		{
			continue;
		}
		const FCurvePoint* MinPoint = Algo::MinElement(CurvePoints, FCurvePointCompare);
		const FCurvePoint* MaxPoint = Algo::MaxElement(CurvePoints, FCurvePointCompare);
		MinValue = MinPoint ? FMath::Min(MinValue, MinPoint->Value) : 0.0f;
		MaxValue = MaxPoint ? FMath::Max(MaxValue, MaxPoint->Value) : 1.0f;
	}
	YDataRange.X = MinValue;
	YDataRange.Y = MaxValue;
}

void SAudioCurveView::SetCurvesPointData(TSharedPtr<TMap<int32, TArray<FCurvePoint>>> InPointDataPerCurve)
{
	PointDataPerCurve = InPointDataPerCurve;
	UpdateYDataRange();
}

void SAudioCurveView::SetCurvesMetadata(TSharedPtr<TMap<int32, FCurveMetadata>> InMetadataPerCurve)
{
	MetadataPerCurve = InMetadataPerCurve;
}

void SAudioCurveView::SetYValueFormattingOptions(const FNumberFormattingOptions InYValueFormattingOptions)
{
	YValueFormattingOptions = InYValueFormattingOptions;
}

float SAudioCurveView::ValueToLocalY(const FVector2f AllottedLocalSize, const float Value) const
{
	// Slate Y values increase going down the screen, so base < top but base is above top on the screen
	const float MarginBase = YMargin.Get() * AllottedLocalSize.Y;
	const float MarginTop = AllottedLocalSize.Y - MarginBase;
	
	// Special case to add padding based on YMargin if YDataRange min/max are the same 
	if (FMath::IsNearlyEqual(YDataRange.X, YDataRange.Y))
	{
		const FVector2f PaddedYDataRange = FVector2f(YDataRange.Y * (1.0f - YMargin.Get()), YDataRange.X * (1.0f + YMargin.Get()));
		return FMath::GetMappedRangeValueUnclamped(PaddedYDataRange, FVector2f(MarginTop, MarginBase), Value);
	}
	return FMath::GetMappedRangeValueUnclamped(YDataRange, FVector2f(MarginTop, MarginBase), Value);
}

float SAudioCurveView::LocalYToValue(const FVector2f AllottedLocalSize, const float LocalY) const
{
	// Slate Y values increase going down the screen, so base < top but base is above top on the screen
	const float MarginBase = YMargin.Get() * AllottedLocalSize.Y;
	const float MarginTop = AllottedLocalSize.Y - MarginBase;

	// Special case to add padding based on YMargin if YDataRange min/max are the same 
	if (FMath::IsNearlyEqual(YDataRange.X, YDataRange.Y))
	{
		const FVector2f PaddedYDataRange = FVector2f(YDataRange.Y * (1.0f - YMargin.Get()), YDataRange.X * (1.0f + YMargin.Get()));
		return FMath::GetMappedRangeValueUnclamped(FVector2f(MarginTop, MarginBase), PaddedYDataRange, LocalY);
	}
	return FMath::GetMappedRangeValueUnclamped(FVector2f(MarginTop, MarginBase), YDataRange, LocalY);
}

int32 SAudioCurveView::PaintGridLines(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled, const SSimpleTimeSlider::FScrubRangeToScreen RangeToScreen) const
{
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const int32 GridLineLayer = LayerId++;
	TArray<FVector2f> GridPoints;
	GridPoints.AddDefaulted(2);

	// Draw vertical grid lines on multiples of HorizontalAxisIncrement
	if (HorizontalAxisIncrement.Get() > 0)
	{
		const double Factor = 1.0 / HorizontalAxisIncrement.Get();
		// Start at rounded nearest HorizontalAxisIncrement from the lower bound and increment by HorizontalAxisIncrement (ex. 1.5, 2.0, 2.5... if increment is 0.5)
		double VerticalLineValue = FMath::RoundToDouble(ViewRange.Get().GetLowerBoundValue() * Factor) / Factor;
		while (VerticalLineValue < ViewRange.Get().GetUpperBoundValue())
		{
			const float WidgetX = RangeToScreen.InputToLocalX(VerticalLineValue);
			GridPoints[0].X = WidgetX;
			GridPoints[0].Y = 0;
			GridPoints[1].X = WidgetX;
			GridPoints[1].Y = Size.Y;

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				GridLineLayer,
				AllottedGeometry.ToPaintGeometry(),
				GridPoints,
				LineDrawEffects,
				GridLineColor.Get(),
				false
			);
			VerticalLineValue += HorizontalAxisIncrement.Get();
		}
	}

	// Draw and label NumHorizontalGridLines horizontal grid lines 
	const int32 GridLineLabelLayer = LayerId++;
	const float MarginBase = YMargin.Get() * Size.Y;
	const float MarginTop = (1.0f - YMargin.Get()) * Size.Y;
	const float GridLineYIncrement = (MarginTop - MarginBase) / (NumHorizontalGridLines - 1);
	const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	for (uint32 HorizontalLineIndex = 0; HorizontalLineIndex < NumHorizontalGridLines; ++HorizontalLineIndex)
	{
		const float WidgetY = GridLineYIncrement * HorizontalLineIndex + MarginBase;
		GridPoints[0].X = 0;
		GridPoints[0].Y = WidgetY;
		GridPoints[1].X = Size.X;
		GridPoints[1].Y = WidgetY;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			GridLineLayer,
			AllottedGeometry.ToPaintGeometry(),
			GridPoints,
			LineDrawEffects,
			GridLineColor.Get(),
			false
		);

		// Draw y axis text label every other grid line
		if (HorizontalLineIndex % 2 == 1)
		{
			const float LabelValue = LocalYToValue(Size, WidgetY);
			FText LabelString = FText::AsNumber(LabelValue, &YValueFormattingOptions);

			// Position text slightly above the corresponding horizontal line 
			const FVector2f TextSize = FontMeasureService->Measure(LabelString, LabelFont);
			FVector2f TextOffset(TextSize.X * 0.5f, WidgetY - TextSize.Y * 0.85f);

			FSlateDrawElement::MakeText(
				OutDrawElements,
				GridLineLabelLayer,
				AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextOffset)),
				LabelString,
				LabelFont,
				LineDrawEffects,
				AxesLabelColor.Get()
			);
		}
	}
	return LayerId;
}

int32 SAudioCurveView::PaintCurves(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const 
{
	static const float LargeFrameTime = 0.25;  // ideally, we could check the recorded frame data for actual frame length
	
	const SSimpleTimeSlider::FScrubRangeToScreen RangeToScreen(ViewRange.Get(), AllottedGeometry.GetLocalSize());
	const FVector2f Size = AllottedGeometry.GetLocalSize();

	// Skip drawing if curve data is not initialized yet
	if (!PointDataPerCurve || !MetadataPerCurve)
	{
		return LayerId;
	}

	if (PointDataPerCurve->Num() != MetadataPerCurve->Num())
	{
		UE_LOG(LogAudio, Warning, TEXT("Invalid audio curve view data. Metadata and point curve data nums do not match."))
		return LayerId;
	}

	LayerId = PaintGridLines(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled, RangeToScreen);

	// Create and draw points per curve
	for (auto Iter = MetadataPerCurve->CreateConstIterator(); Iter; ++Iter)
	{
		const FCurveMetadata& CurveMetadata = Iter->Value;
		const int32 CurveId = CurveMetadata.CurveId;
		const TArray<FCurvePoint>* CurvePoints = PointDataPerCurve->Find(CurveId);
		if (!CurvePoints || CurvePoints->Num() <= 0)
		{
			continue;
		}

		TArray<FVector2f> Points;
		Points.Reserve(CurvePoints->Num());

		float PrevX = (*CurvePoints)[0].Key;
		for (int32 i = 0; i < CurvePoints->Num(); i++)
		{
			const FCurvePoint& Point = (*CurvePoints)[i];
			if (Point.Value - PrevX > LargeFrameTime && Points.Num()>1)
			{
				// break the line list - data has stopped and started again
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId++,
					AllottedGeometry.ToPaintGeometry(),
					Points,
					LineDrawEffects,
					CurveMetadata.CurveColor,
					false
				);
				Points.SetNum(0, EAllowShrinking::No);
			}

			const float X = RangeToScreen.InputToLocalX(Point.Key);
			PrevX = Point.Key;
			const float Y = ValueToLocalY(Size, Point.Value);
			Points.Add(FVector2f(X, Y));
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			LineDrawEffects,
			CurveMetadata.CurveColor,
			false
		);
	}
	return LayerId;
}

#undef LOCTEXT_NAMESPACE
