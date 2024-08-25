// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rendering/DrawElements.h"
#include "SampledSequenceDrawingUtils.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "TriggerThresholdLineStyle.h"
#include "Widgets/SLeafWidget.h"

struct FTriggerThresholdLineStyle;

class AUDIOWIDGETS_API STriggerThresholdLineWidget : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(STriggerThresholdLineWidget)
        : _TriggerThreshold(0.0f)
		, _Style(&FTriggerThresholdLineStyle::GetDefault())
    {}
    
		SLATE_ARGUMENT(float, TriggerThreshold)
		SLATE_ARGUMENT(SampledSequenceDrawingUtils::FSampledSequenceDrawingParams, SequenceDrawingParams)
		SLATE_STYLE_ARGUMENT(FTriggerThresholdLineStyle, Style)

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual int32 OnPaint(const FPaintArgs& Args, 
		const FGeometry& AllottedGeometry, 
		const FSlateRect& MyCullingRect, 
		FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, 
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float) const override;

	void OnStyleUpdated(const FTriggerThresholdLineStyle UpdatedStyle);

	void SetTriggerThreshold(float InTriggerThreshold);

private:
    float TriggerThreshold;
	FLinearColor LineColor;
	SampledSequenceDrawingUtils::FSampledSequenceDrawingParams DrawingParams;
	const FTriggerThresholdLineStyle* Style = nullptr;
};
