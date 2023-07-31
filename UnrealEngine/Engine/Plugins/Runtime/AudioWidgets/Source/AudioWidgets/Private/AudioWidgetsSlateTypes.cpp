// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioWidgetsSlateTypes.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/StyleColors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioWidgetsSlateTypes)

// Audio Text Box Style 
FAudioTextBoxStyle::FAudioTextBoxStyle()
	: BackgroundImage(FSlateRoundedBoxBrush(FStyleColors::White, 4.0f, FVector2D(56.0f, 28.0f)))
	, BackgroundColor(FStyleColors::Recessed)
{
}

void FAudioTextBoxStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&BackgroundImage);
}

const FName FAudioTextBoxStyle::TypeName("FAudioTextBoxStyle");

const FAudioTextBoxStyle& FAudioTextBoxStyle::GetDefault()
{
	static FAudioTextBoxStyle Default;
	return Default;
}

// Audio Slider Style 
FAudioSliderStyle::FAudioSliderStyle()
	: SliderStyle(FSliderStyle::GetDefault())
	, TextBoxStyle(FAudioTextBoxStyle::GetDefault())
	, WidgetBackgroundImage(FSlateOptionalBrush())
	, SliderBackgroundColor(FStyleColors::AccentGray)
	, SliderBackgroundSize(FVector2D(28.0f, 450.0f))
	, LabelPadding(0.0f)
	, SliderBarColor(FStyleColors::Black)
	, SliderThumbColor(FStyleColors::White)
{
}

void FAudioSliderStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&WidgetBackgroundImage);
}

const FName FAudioSliderStyle::TypeName("FAudioSliderStyle");

const FAudioSliderStyle& FAudioSliderStyle::GetDefault()
{
	static FAudioSliderStyle Default;
	return Default;
}

// Audio Radial Slider Style 
FAudioRadialSliderStyle::FAudioRadialSliderStyle()
	: TextBoxStyle(FAudioTextBoxStyle::GetDefault())
	, CenterBackgroundColor(FStyleColors::Recessed)
	, SliderBarColor(FStyleColors::AccentGray)
	, SliderProgressColor(FStyleColors::White)
	, LabelPadding(0.0f)
	, DefaultSliderRadius(50.0f)
{
}

const FName FAudioRadialSliderStyle::TypeName("FAudioRadialSliderStyle");

const FAudioRadialSliderStyle& FAudioRadialSliderStyle::GetDefault()
{
	static FAudioRadialSliderStyle Default;
	return Default;
}

