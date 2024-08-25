// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SSimpleTimeSlider.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FPaintArgs;
class FSlateWindowElementList;
class SToolTip;

// Loosely based on SCurveTimelineView
// TODO move this to a more general place like the Audio Widgets plugin once it's further developed
class SAudioCurveView : public SCompoundWidget
{
public:
	// A curve point is a (double XValue, float YValue) pair
	using FCurvePoint = TPair<double, float>;

	struct FCurveMetadata
	{
		int32 CurveId; 
		FLinearColor CurveColor;
		FText DisplayName;
	};

	SLATE_BEGIN_ARGS(SAudioCurveView)
		: _ViewRange(TRange<double>(0,5))
		, _YMargin(0.05f)
		, _HorizontalAxisIncrement(0.5)
		, _GridLineColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.25f))
		, _AxesLabelColor(FLinearColor(0.5f, 0.5f, 0.75f))
		, _DesiredSize(FVector2D(100.f,100.f))
	{}
    	/** View X axis range (in value space) */
    	SLATE_ATTRIBUTE(TRange<double>, ViewRange);

		/** Margin for Y axis, as a 0 - 0.5f proportion, for the space each of above and below the data range. (ex. 0.05 means a 5% margin on the top and bottom, with 90% of the widget's vertical size corresponding to the data range). */
		SLATE_ATTRIBUTE(float, YMargin);

		/** X axis increment for grid lines. */
		SLATE_ATTRIBUTE(double, HorizontalAxisIncrement);

		SLATE_ATTRIBUTE(FLinearColor, GridLineColor);

		SLATE_ATTRIBUTE(FLinearColor, AxesLabelColor);

		/** Desired widget size */
		SLATE_ATTRIBUTE(FVector2D, DesiredSize);
	
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

	void SetYValueFormattingOptions(const FNumberFormattingOptions InValueFormattingOptions);
	void SetCurvesPointData(TSharedPtr<TMap<int32, TArray<FCurvePoint>>> InCurvesPointData);
	void SetCurvesMetadata(TSharedPtr<TMap<int32, FCurveMetadata>> InMetadataPerCurve);

	FText GetCurveToolTipXValueText() const { return CurveToolTipXValueText; }
	FText GetCurveToolTipYValueText() const { return CurveToolTipYValueText; }
	FText GetCurveToolTipDisplayNameText() const { return CurveToolTipDisplayNameText; }

	/** Helper functions for converting between widget local Y position and a given data value (within DataRange). */
	float ValueToLocalY(const FVector2f AllottedLocalSize, const float Value) const;
	float LocalYToValue(const FVector2f AllottedLocalSize, const float LocalY) const;
	
protected:
	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
	int32 PaintCurves(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	int32 PaintGridLines(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled, const SSimpleTimeSlider::FScrubRangeToScreen RangeToScreen) const;
	void UpdateCurveToolTip(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void UpdateYDataRange();
	TSharedRef<SToolTip> CreateCurveTooltip();

	TAttribute<TRange<double>> ViewRange;
	TAttribute<float> YMargin;
	TAttribute<double> HorizontalAxisIncrement;
	TAttribute<FLinearColor> GridLineColor;
	TAttribute<FLinearColor> AxesLabelColor;
	TAttribute<FVector2D> DesiredSize;

	// Point data and metadata, keyed by curve id
	TSharedPtr<TMap<int32, TArray<FCurvePoint>>> PointDataPerCurve;
	TSharedPtr<TMap<int32, FCurveMetadata>> MetadataPerCurve; 
	// Y axis data range in value space
	FVector2f YDataRange;

	// Tooltip text
	FText CurveToolTipXValueText;		
	FText CurveToolTipYValueText;
	FText CurveToolTipDisplayNameText;
	
	// Tooltip and axis text formatting
	FNumberFormattingOptions XValueFormattingOptions;
	FNumberFormattingOptions YValueFormattingOptions;

	ESlateDrawEffect LineDrawEffects;
	uint32 NumHorizontalGridLines;
	FSlateFontInfo LabelFont;
};
