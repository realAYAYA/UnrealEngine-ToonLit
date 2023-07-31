// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioWidgetsStyle.h"
#include "AudioWidgetsSlateTypes.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"

FName FAudioWidgetsStyle::StyleName("AudioWidgetsStyle");

FAudioWidgetsStyle::FAudioWidgetsStyle()
	: FSlateStyleSet(StyleName)
{
	SetParentStyleName("CoreStyle");
	SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Runtime/AudioWidgets/Content"));

	/** 
	* AudioTextBox Style
	*/
	const float LabelWidth = 64.0f;
	const float LabelHeight = 28.0f;
	const FVector2D LabelBackgroundSize = FVector2D(LabelWidth, LabelHeight);
	const FLinearColor LabelBackgroundColor = FStyleColors::Recessed.GetSpecifiedColor();
	const float LabelCornerRadius = 4.0f;
	
	Set("AudioTextBox.Style", FAudioTextBoxStyle()
		.SetBackgroundColor(FStyleColors::Recessed)
		.SetBackgroundImage(FSlateRoundedBoxBrush(FStyleColors::White, LabelCornerRadius, LabelBackgroundSize)));

	/**
	* AudioSlider Style
	*/
	// Default orientation is vertical, so width/height are relative to that
	const FVector2D ThumbImageSize = FVector2D(22.0f, 22.0f);

	const float SliderBarWidth = 10.0f;
	const float SliderBarHeight = 432.0f;
	const float SliderBackgroundOutlineWidth = 9.0f;
	const FVector2D SliderBarCapSize = FVector2D(SliderBarWidth, SliderBarWidth / 2.0f);
	const FVector2D SliderBarRectangleSize = FVector2D(SliderBarWidth, SliderBarHeight - SliderBarWidth);

	const float SliderBackgroundWidth = 28.0f;
	const float SliderBackgroundHeight = 450.0f;
	const FVector2D SliderBackgroundCapSize = FVector2D(SliderBackgroundWidth, SliderBackgroundWidth / 2.0f);
	const FVector2D SliderBackgroundRectangleSize = FVector2D(SliderBackgroundWidth, SliderBackgroundHeight - SliderBackgroundWidth);
	const FVector2D SliderBackgroundSize = FVector2D(SliderBackgroundWidth, SliderBackgroundHeight);
	
	const float LabelPadding = 3.0f;
	const FVector2D SliderDesiredWidgetSizeVertical = FVector2D(LabelBackgroundSize.X, LabelBackgroundSize.Y + LabelPadding + SliderBackgroundHeight);
	const FVector2D SliderDesiredWidgetSizeHorizontal = FVector2D(SliderBackgroundHeight + LabelBackgroundSize.X + LabelPadding, SliderBackgroundWidth);

	const FSlateColor ThumbColor = FStyleColors::White;
	FSlateRoundedBoxBrush ThumbImage = FSlateRoundedBoxBrush(ThumbColor, ThumbImageSize.X / 2.0f, ThumbImageSize);
	FSlateRoundedBoxBrush WidgetBackgroundImage = FSlateRoundedBoxBrush(FStyleColors::White, LabelCornerRadius, SliderDesiredWidgetSizeVertical);

	Set("AudioSlider.Slider", FSliderStyle()
		.SetNormalBarImage(FSlateNoResource())
		.SetHoveredBarImage(FSlateNoResource())
		.SetDisabledBarImage(FSlateNoResource())
		.SetNormalThumbImage(ThumbImage)
		.SetHoveredThumbImage(ThumbImage)
		.SetDisabledThumbImage(FSlateNoResource())
	);

	Set("AudioSlider.Style", FAudioSliderStyle()
		.SetSliderStyle(FSliderStyle()
			.SetNormalBarImage(FSlateNoResource())
			.SetHoveredBarImage(FSlateNoResource())
			.SetDisabledBarImage(FSlateNoResource())
			.SetNormalThumbImage(ThumbImage)
			.SetHoveredThumbImage(ThumbImage)
			.SetDisabledThumbImage(FSlateNoResource()))
		.SetTextBoxStyle(FAudioTextBoxStyle::GetDefault())
		.SetWidgetBackgroundImage(WidgetBackgroundImage)
		.SetSliderBackgroundColor(FStyleColors::Recessed)
		.SetSliderBarColor(FStyleColors::Black)
		.SetSliderThumbColor(ThumbColor)
		.SetSliderBackgroundSize(SliderBackgroundSize)
		.SetWidgetBackgroundColor(FStyleColors::Transparent)
		.SetLabelPadding(LabelPadding)
	);

	/**
	* AudioRadialSlider Style
	*/
	const float RadialSliderDefaultRadius = 50.0f;
	const FVector2D RadialSliderDesiredSize = FVector2D(RadialSliderDefaultRadius, RadialSliderDefaultRadius + LabelPadding + LabelHeight);
	Set("AudioRadialSlider.DesiredWidgetSize", RadialSliderDesiredSize);

	Set("AudioRadialSlider.Style", FAudioRadialSliderStyle()
		.SetTextBoxStyle(FAudioTextBoxStyle::GetDefault())
		.SetCenterBackgroundColor(FStyleColors::Recessed)
		.SetSliderProgressColor(FStyleColors::White)
		.SetSliderBarColor(FStyleColors::AccentGray)
		.SetLabelPadding(3.0f)
		.SetDefaultSliderRadius(50.0f));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAudioWidgetsStyle::~FAudioWidgetsStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FAudioWidgetsStyle& FAudioWidgetsStyle::Get()
{
	static FAudioWidgetsStyle Inst;
	return Inst;
}
