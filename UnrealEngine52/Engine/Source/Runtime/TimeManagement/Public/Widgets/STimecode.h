// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

#include "Fonts/SlateFontInfo.h"
#include "Misc/Attribute.h"
#include "Misc/Timecode.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"

class TIMEMANAGEMENT_API STimecode : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STimecode)
		: _TimecodeColor(FLinearColor::White)
		, _DisplayLabel(true)
		, _LabelColor(FLinearColor::Gray)
	{
		FSlateFontInfo NormalFontInfo = FCoreStyle::Get().GetFontStyle(TEXT("NormalText"));
		_LabelFont = NormalFontInfo;
		NormalFontInfo.Size += 16;
		_TimecodeFont = NormalFontInfo;
	}
		/** The timecode to display */
		SLATE_ATTRIBUTE(FTimecode, Timecode)
		/** The font for the timecode text */
		SLATE_ATTRIBUTE(FSlateFontInfo, TimecodeFont)
		/** The color for the timecode text */
		SLATE_ATTRIBUTE(FSlateColor, TimecodeColor)

		/** Should display the label (hours, mins, secs, frames) */
		SLATE_ATTRIBUTE(bool, DisplayLabel)
		/** The font for this label text */
		SLATE_ATTRIBUTE(FSlateFontInfo, LabelFont)
		/** The color for this label text */
		SLATE_ATTRIBUTE(FSlateColor, LabelColor)
	SLATE_END_ARGS()

	STimecode();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

protected:
	// SWidget overrides
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool ComputeVolatility() const override;

private:
	TAttribute<FTimecode> Timecode;
	TAttribute<FSlateFontInfo> TimecodeFont;
	TAttribute<FSlateColor> TimecodeColor;

	TAttribute<bool> bDisplayLabel;
	TAttribute<FSlateFontInfo> LabelFont;
	TAttribute<FSlateColor> LabelColor;
};
