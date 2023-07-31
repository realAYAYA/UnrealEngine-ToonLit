// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/SLeafWidget.h"
#include "AudioMeterStyle.h"
#include "AudioMeterTypes.h"

class FPaintArgs;
class FSlateWindowElementList;

/**
 * A Slate slider control is a linear scale and draggable handle.
 */
class AUDIOWIDGETS_API SAudioMeter
	: public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SAudioMeter)
		: _Orientation(EOrientation::Orient_Horizontal)
		, _BackgroundColor(FLinearColor::Black)
		, _MeterBackgroundColor(FLinearColor::Gray)
		, _MeterValueColor(FLinearColor::Green)
		, _MeterPeakColor(FLinearColor::Blue)
		, _MeterScaleColor(FLinearColor::White)
		, _MeterScaleLabelColor(FLinearColor::Gray)
		, _Style(&FAudioMeterStyle::GetDefault())
	{
	}

	/** Whether the slidable area should be indented to fit the handle. */
	SLATE_ATTRIBUTE(bool, IndentHandle)

		/** The slider's orientation. */
		SLATE_ARGUMENT(EOrientation, Orientation)

		/** The color to draw the background in. */
		SLATE_ATTRIBUTE(FSlateColor, BackgroundColor)

		/** The color to draw the meter background in. */
		SLATE_ATTRIBUTE(FSlateColor, MeterBackgroundColor)

		/** The color to draw the meter value in. */
		SLATE_ATTRIBUTE(FSlateColor, MeterValueColor)

		/** The color to draw the meter peak. */
		SLATE_ATTRIBUTE(FSlateColor, MeterPeakColor)

		/** The color to draw the clipping value in. */
		SLATE_ATTRIBUTE(FSlateColor, MeterClippingColor)

		/** The color to draw the scale in. */
		SLATE_ATTRIBUTE(FSlateColor, MeterScaleColor)

		/** The color to draw the scale in. */
		SLATE_ATTRIBUTE(FSlateColor, MeterScaleLabelColor)

		/** The style used to draw the slider. */
		SLATE_STYLE_ARGUMENT(FAudioMeterStyle, Style)

		/** A value representing the audio meter value. */
		SLATE_ATTRIBUTE(TArray<FMeterChannelInfo>, MeterChannelInfo)

		SLATE_END_ARGS()

		SAudioMeter();

	/**
	 * Construct the widget.
	 *
	 * @param InDeclaration A declaration from which to construct the widget.
	 */
	void Construct(const SAudioMeter::FArguments& InDeclaration);

	void SetMeterChannelInfo(const TAttribute<TArray<FMeterChannelInfo>>& InMeterChannelInfo);
	TArray<FMeterChannelInfo> GetMeterChannelInfo() const;

	/** Set the Orientation attribute */
	void SetOrientation(EOrientation InOrientation);

	void SetBackgroundColor(FSlateColor InBackgroundColor);
	void SetMeterBackgroundColor(FSlateColor InMeterBackgroundColor);
	void SetMeterValueColor(FSlateColor InMeterValueColor);
	void SetMeterPeakColor(FSlateColor InMeterPeakColor);
	void SetMeterClippingColor(FSlateColor InMeterPeakColor);
	void SetMeterScaleColor(FSlateColor InMeterScaleColor);
	void SetMeterScaleLabelColor(FSlateColor InMeterScaleLabelColor);

	/** Is the active timer registered to refresh the meter channel info. */
	bool bIsActiveTimerRegistered = false;

public:

	// SWidget overrides

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool ComputeVolatility() const override;

protected:
	
	// Returns the scale height based off font size and hash height
	float GetScaleHeight() const;
	
	// Holds the style passed to the widget upon construction.
	const FAudioMeterStyle* Style;

	// Holds the slider's orientation.
	EOrientation Orientation;

	// Various colors
	TAttribute<FSlateColor> BackgroundColor;
	TAttribute<FSlateColor> MeterBackgroundColor;
	TAttribute<FSlateColor> MeterValueColor;
	TAttribute<FSlateColor> MeterPeakColor;
	TAttribute<FSlateColor> MeterClippingColor;
	TAttribute<FSlateColor> MeterScaleColor;
	TAttribute<FSlateColor> MeterScaleLabelColor;

	TAttribute<TArray<FMeterChannelInfo>> MeterChannelInfoAttribute;
};
