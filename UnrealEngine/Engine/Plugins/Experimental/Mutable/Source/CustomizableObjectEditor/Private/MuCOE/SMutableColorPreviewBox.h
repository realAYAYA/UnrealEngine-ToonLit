// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/** Custom widget designed to display a color on a box */
class SMutableColorPreviewBox : public SCompoundWidget
{
public:
	/** Slate Constructor that does allow the caller to provide the default value for the preview */
	SLATE_BEGIN_ARGS(SMutableColorPreviewBox) {}
		SLATE_ATTRIBUTE(FSlateColor, BoxColor)
	SLATE_END_ARGS()

	/** Builds the widget */
	void Construct(const FArguments& InArgs);

	/** Sets the color to be displayed by this widget */
	void SetColor(const FSlateColor& InColor);

private:

	/** Defines the color used by this widget*/
	FSlateColor Color;

	/*
	* Method that returns the FSLateColor being currently used by the preview 
	* @return the FSLateColor that is being used by the Preview
	*/
	FSlateColor GetColor() const;
};

