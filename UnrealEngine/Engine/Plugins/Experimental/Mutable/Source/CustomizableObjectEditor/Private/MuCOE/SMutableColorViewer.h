// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SMutableColorPreviewBox;


class SMutableColorViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMutableColorViewer) {}
	SLATE_END_ARGS()

	/** Builds the widget */
	void Construct(const FArguments& InArgs);

	/** Set the Mutable Bool to be used for this widget */
	void SetColor(const float& InRed, const float& InGreen, const float& InBlue);

private:

	/** Color being displayed */
	float RedValue = 0.0f;
	float GreenValue = 0.0f;
	float BlueValue = 0.0f;
	const float AlphaValue{1};

	/** Color box widget designed to serve as a preview of the color reported by mutable */
	TSharedPtr<SMutableColorPreviewBox> ColorPreview;

	/** Retrieve the values of each color component for the UI Texts to be updated */
	FText GetRedValue() const;
	FText GetGreenValue() const;
	FText GetBlueValue() const;

	/*
	* Get a color object that the SMutableColorPreviewBox is able to display 
	* @return a new FSlateColor generated from the float values held by this widget
	*/
	FSlateColor GetPreviewColor() const;
};
