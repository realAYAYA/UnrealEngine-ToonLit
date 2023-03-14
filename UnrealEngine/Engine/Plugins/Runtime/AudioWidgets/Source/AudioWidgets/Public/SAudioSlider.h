// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioWidgetsSlateTypes.h"
#include "Curves/CurveFloat.h"
#include "Framework/SlateDelegates.h"
#include "Misc/Attribute.h"
#include "SAudioInputWidget.h"
#include "SAudioTextBox.h"
#include "Styling/SlateStyleRegistry.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

/**
 * Slate audio sliders that wrap SSlider and provides additional audio specific functionality.
 * This is a nativized version of the previous Audio Fader widget. 
 */
class AUDIOWIDGETS_API SAudioSliderBase
	: public SAudioInputWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioSliderBase)
	{
		_SliderValue = 0.0f;
		_Orientation = Orient_Vertical;

		const ISlateStyle* AudioWidgetsStyle = FSlateStyleRegistry::FindSlateStyle("AudioWidgetsStyle");
		if (ensure(AudioWidgetsStyle))
		{
			_Style = &AudioWidgetsStyle->GetWidgetStyle<FAudioSliderStyle>("AudioSlider.Style");
			_SliderBackgroundColor = _Style->SliderBackgroundColor;
			_SliderBarColor = _Style->SliderBarColor;
			_SliderThumbColor = _Style->SliderThumbColor;
			_WidgetBackgroundColor = _Style->WidgetBackgroundColor;
		}
	}
		/** The style used to draw the audio slider. */
		SLATE_STYLE_ARGUMENT(FAudioSliderStyle, Style)

		/** A value representing the normalized linear (0 - 1) audio slider value position. */
		SLATE_ATTRIBUTE(float, SliderValue)

		/** Whether the text label is always shown or only on hover. */
		SLATE_ATTRIBUTE(bool, AlwaysShowLabel)
			
		/** The orientation of the slider. */
		SLATE_ARGUMENT(EOrientation, Orientation)

		/** The color to draw the slider background in. */
		SLATE_ATTRIBUTE(FSlateColor, SliderBackgroundColor)

		/** The color to draw the slider bar in. */
		SLATE_ATTRIBUTE(FSlateColor, SliderBarColor)

		/** The color to draw the slider thumb in. */
		SLATE_ATTRIBUTE(FSlateColor, SliderThumbColor)
		
		/** The color to draw the widget background in. */
		SLATE_ATTRIBUTE(FSlateColor, WidgetBackgroundColor)
		
		/** When specified, use this as the slider's desired size */
		SLATE_ATTRIBUTE(TOptional<FVector2D>, DesiredSizeOverride)

		/** Called when the value is changed by slider or typing */
		SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)

		/** Called when the value is committed by mouse capture ending */
		SLATE_EVENT(FOnFloatValueChanged, OnValueCommitted)

	SLATE_END_ARGS()

	SAudioSliderBase();
	virtual ~SAudioSliderBase() {};

	// Holds a delegate that is executed when the slider's value changed.
	FOnFloatValueChanged OnValueChanged;

	// Holds a delegate that is executed when the slider's value is committed (mouse capture ends).
	FOnFloatValueChanged OnValueCommitted;

	/**
	 * Construct the widget.
	 *
	 * @param InDeclaration A declaration from which to construct the widget.
	 */
	virtual void Construct(const SAudioSliderBase::FArguments& InDeclaration);
	virtual const float GetOutputValue(const float InSliderValue);
	virtual const float GetSliderValue(const float OutputValue);
	virtual const float GetOutputValueForText(const float InSliderValue);
	virtual const float GetSliderValueForText(const float OutputValue);
	
	/**
	 * Set the slider's linear (0-1 normalized) value. 
	 */
	void SetSliderValue(float InSliderValue);
	FVector2D ComputeDesiredSize(float) const;
	void SetDesiredSizeOverride(const FVector2D DesiredSize);

	void SetOrientation(EOrientation InOrientation);
	void SetSliderBackgroundColor(FSlateColor InSliderBackgroundColor);
	void SetSliderBarColor(FSlateColor InSliderBarColor);
	void SetSliderThumbColor(FSlateColor InSliderThumbColor);
	void SetWidgetBackgroundColor(FSlateColor InWidgetBackgroundColor);
	virtual void SetOutputRange(const FVector2D Range);

	// Text label functions 
	void SetLabelBackgroundColor(FSlateColor InColor);
	void SetUnitsText(const FText Units);
	void SetUnitsTextReadOnly(const bool bIsReadOnly);
	void SetValueTextReadOnly(const bool bIsReadOnly);
	void SetShowLabelOnlyOnHover(const bool bShowLabelOnlyOnHover);
	void SetShowUnitsText(const bool bShowUnitsText);

protected:
	const FAudioSliderStyle* Style;

	// Holds the slider's current linear value, from 0.0 - 1.0f
	TAttribute<float> SliderValueAttribute;
	// Holds the slider's orientation
	TAttribute<EOrientation> Orientation;
	// Optional override for desired size 
	TAttribute<TOptional<FVector2D>> DesiredSizeOverride;

	// Various colors 
	TAttribute<FSlateColor> LabelBackgroundColor;
	TAttribute<FSlateColor> SliderBackgroundColor;
	TAttribute<FSlateColor> SliderBarColor;
	TAttribute<FSlateColor> SliderThumbColor;
	TAttribute<FSlateColor> WidgetBackgroundColor;

	// Widget components
	TSharedPtr<SSlider> Slider;
	TSharedPtr<SAudioTextBox> Label;
	TSharedPtr<SImage> SliderBackgroundImage;
	TSharedPtr<SImage> WidgetBackgroundImage;

	// Range for output, currently only used for frequency sliders and sliders without curves
	FVector2D OutputRange = FVector2D(0.0f, 1.0f);
	static const FVector2D NormalizedLinearSliderRange;
private:
	FSlateBrush SliderBackgroundBrush;
	FVector2D SliderBackgroundSize;
	/** Switches between the vertical and horizontal views */
	TSharedPtr<SWidgetSwitcher> LayoutWidgetSwitcher;

	TSharedRef<SWidgetSwitcher> CreateWidgetLayout();
};

/* 
*	An Audio Slider widget with customizable curves. 
*/
class AUDIOWIDGETS_API SAudioSlider
	: public SAudioSliderBase
{
public:
	SAudioSlider();
	virtual ~SAudioSlider() {};
	virtual void Construct(const SAudioSliderBase::FArguments& InDeclaration);
	void SetLinToOutputCurve(const TWeakObjectPtr<const UCurveFloat> LinToOutputCurve);
	void SetOutputToLinCurve(const TWeakObjectPtr<const UCurveFloat> OutputToLinCurve);
	const TWeakObjectPtr<const UCurveFloat> GetOutputToLinCurve();
	const TWeakObjectPtr<const UCurveFloat> GetLinToOutputCurve();
	const float GetOutputValue(const float InSliderValue);
	const float GetSliderValue(const float OutputValue);

protected:
	// Curves for mapping linear (0.0 - 1.0) to output (ex. dB for volume)  
	TWeakObjectPtr<const UCurveFloat> LinToOutputCurve = nullptr;
	TWeakObjectPtr<const UCurveFloat> OutputToLinCurve = nullptr;
};

/*
* An Audio Slider widget intended to be used for volume output, with output decibel range but no customizable curves.
*/
class AUDIOWIDGETS_API SAudioVolumeSlider
	: public SAudioSliderBase
{
public:
	SAudioVolumeSlider();
	void Construct(const SAudioSlider::FArguments& InDeclaration);

	const float GetOutputValue(const float InSliderValue);
	const float GetSliderValue(const float OutputValue);
	const float GetOutputValueForText(const float InSliderValue) override;
	const float GetSliderValueForText(const float OutputValue) override;
	void SetUseLinearOutput(bool InUseLinearOutput);
	void SetOutputRange(const FVector2D Range) override;

private:
	// Min/max possible values for output range, derived to avoid Audio::ConvertToLinear/dB functions returning NaN
	static const float MinDbValue;
	static const float MaxDbValue; 
	
	// Use linear (converted from dB, not normalized) output value. Only applies to the output value reported by GetOutputValue(); the text displayed will still be in decibels. 
	bool bUseLinearOutput = true;

	const float GetDbValueFromSliderValue(const float InSliderValue);
	const float GetSliderValueFromDb(const float DbValue);
};

/*
* An Audio Slider widget intended to be used for frequency output, with output frequency range but no customizable curves. 
*/
class AUDIOWIDGETS_API SAudioFrequencySlider
	: public SAudioSliderBase
{
public:
	SAudioFrequencySlider();
	void Construct(const SAudioSlider::FArguments& InDeclaration);
	const float GetOutputValue(const float InSliderValue);
	const float GetSliderValue(const float OutputValue);
};
