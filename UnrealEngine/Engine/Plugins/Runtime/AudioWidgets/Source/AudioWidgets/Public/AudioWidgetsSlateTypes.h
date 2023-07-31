// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/StyleDefaults.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateWidgetStyle.h"
#include "UObject/ObjectMacros.h"

#include "AudioWidgetsSlateTypes.generated.h"

/**
 * Represents the appearance of an Audio Text Box 
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioTextBoxStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioTextBoxStyle();

	virtual ~FAudioTextBoxStyle() {}

	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FAudioTextBoxStyle& GetDefault();

	/** Image for the label border */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BackgroundImage;
	FAudioTextBoxStyle& SetBackgroundImage(const FSlateBrush& InBackgroundImage) { BackgroundImage = InBackgroundImage; return *this; }

	/** Color used to draw the label background */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor BackgroundColor;
	FAudioTextBoxStyle& SetBackgroundColor(const FSlateColor& InBackgroundColor) { BackgroundColor = InBackgroundColor; return *this; }

	/**
	* Unlinks all colors in this style.
	* @see FSlateColor::Unlink
	 */
	void UnlinkColors()
	{
		BackgroundImage.UnlinkColors();
	}
};

/**
 * Represents the appearance of an Audio Slider
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioSliderStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioSliderStyle();

	virtual ~FAudioSliderStyle() {}

	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FAudioSliderStyle& GetDefault();

	/** The style to use for the underlying SSlider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSliderStyle SliderStyle;
	FAudioSliderStyle& SetSliderStyle(const FSliderStyle& InSliderStyle) { SliderStyle = InSliderStyle; return *this; }

	/** The style to use for the audio text box widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FAudioTextBoxStyle TextBoxStyle;
	FAudioSliderStyle& SetTextBoxStyle(const FAudioTextBoxStyle& InTextBoxStyle) { TextBoxStyle = InTextBoxStyle; return *this; }

	/** Image for the widget background */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush WidgetBackgroundImage;
	FAudioSliderStyle& SetWidgetBackgroundImage(const FSlateBrush& InWidgetBackgroundImage) { WidgetBackgroundImage = InWidgetBackgroundImage; return *this; }

	/** Color used to draw the slider background */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor SliderBackgroundColor;
	FAudioSliderStyle& SetSliderBackgroundColor(const FSlateColor& InSliderBackgroundColor) { SliderBackgroundColor = InSliderBackgroundColor; return *this; }

	/** Size of the slider background (slider default is vertical)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D SliderBackgroundSize;
	FAudioSliderStyle& SetSliderBackgroundSize(const FVector2D& InSliderBackgroundSize) { SliderBackgroundSize = InSliderBackgroundSize; return *this; }

	/** Size of the padding between the label and the slider */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float LabelPadding;
	FAudioSliderStyle& SetLabelPadding(const float& InLabelPadding) { LabelPadding = InLabelPadding; return *this; }

	/** Color used to draw the slider bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor SliderBarColor;
	FAudioSliderStyle& SetSliderBarColor(const FSlateColor& InSliderBarColor) { SliderBarColor = InSliderBarColor; return *this; }

	/** Color used to draw the slider thumb (handle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor SliderThumbColor;
	FAudioSliderStyle& SetSliderThumbColor(const FSlateColor& InSliderThumbColor) { SliderThumbColor = InSliderThumbColor; return *this; }

	/** Color used to draw the widget background */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor WidgetBackgroundColor;
	FAudioSliderStyle& SetWidgetBackgroundColor(const FSlateColor& InWidgetBackgroundColor) { WidgetBackgroundColor = InWidgetBackgroundColor; return *this; }

	/**
	* Unlinks all colors in this style.
	* @see FSlateColor::Unlink
	 */
	void UnlinkColors()
	{
		WidgetBackgroundImage.UnlinkColors();
	}
};


/**
 * Represents the appearance of an Audio Radial Slider
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioRadialSliderStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioRadialSliderStyle();

	virtual ~FAudioRadialSliderStyle() {}

	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override {}

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FAudioRadialSliderStyle& GetDefault();
	
	/** The style to use for the audio text box widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FAudioTextBoxStyle TextBoxStyle;
	FAudioRadialSliderStyle& SetTextBoxStyle(const FAudioTextBoxStyle& InTextBoxStyle) { TextBoxStyle = InTextBoxStyle; return *this; }

	/** Color used to draw the slider center background */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor CenterBackgroundColor;
	FAudioRadialSliderStyle& SetCenterBackgroundColor(const FSlateColor& InCenterBackgroundColor) { CenterBackgroundColor = InCenterBackgroundColor; return *this; }

	/** Color used to draw the unprogressed slider bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor SliderBarColor;
	FAudioRadialSliderStyle& SetSliderBarColor(const FSlateColor& InSliderBarColor) { SliderBarColor = InSliderBarColor; return *this; }

	/** Color used to draw the progress bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor SliderProgressColor;
	FAudioRadialSliderStyle& SetSliderProgressColor(const FSlateColor& InSliderProgressColor) { SliderProgressColor = InSliderProgressColor; return *this; }
	
	/** Size of the padding between the label and the slider */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float LabelPadding;
	FAudioRadialSliderStyle& SetLabelPadding(const float& InLabelPadding) { LabelPadding = InLabelPadding; return *this; }

	/** Default size of the slider itself (not including label) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float DefaultSliderRadius;
	FAudioRadialSliderStyle& SetDefaultSliderRadius(const float& InDefaultSliderRadius) { DefaultSliderRadius = InDefaultSliderRadius; return *this; }
};
