// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

/**
  * Abstract class for use by audio sliders and knobs 
  * that consists of a visual representation of a float value
  * and a text label.  
 *//*todo make this an audio value display widget that inherits from swidget 
	include textbox and widget ref */
 class AUDIOWIDGETS_API SAudioInputWidget
	: public SCompoundWidget
{
public:
	virtual const float GetOutputValue(const float InSliderValue) = 0;
	virtual const float GetSliderValue(const float OutputValue) = 0;
	
	/**
	 * Set the slider's linear (0-1 normalized) value. 
	 */
	virtual void SetSliderValue(float InSliderValue) = 0;
	virtual void SetOutputRange(const FVector2D Range) = 0;
	
	virtual void SetLabelBackgroundColor(FSlateColor InColor) = 0;
	virtual void SetUnitsText(const FText Units) = 0;
	virtual void SetUnitsTextReadOnly(const bool bIsReadOnly) = 0;
	virtual void SetShowUnitsText(const bool bShowUnitsText) = 0;
	virtual void SetDesiredSizeOverride(const FVector2D DesiredSize) = 0;
};
