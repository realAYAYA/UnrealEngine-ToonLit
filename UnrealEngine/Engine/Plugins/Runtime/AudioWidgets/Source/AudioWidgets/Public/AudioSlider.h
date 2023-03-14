// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDefines.h"
#include "AudioWidgetsSlateTypes.h"
#include "Components/Widget.h"
#include "Curves/CurveFloat.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "AudioSlider.generated.h"

class SAudioSliderBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFloatValueChangedEvent, float, Value);

/**
 * An audio slider widget. 
 */
UCLASS(Abstract)
class AUDIOWIDGETS_API UAudioSliderBase: public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The normalized linear (0 - 1) slider value. */
	UPROPERTY(EditAnywhere, Category = Appearance, meta = (UIMin = "0", UIMax = "1"))
	float Value;

	/** The text label units */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FText UnitsText;

	/** The color to draw the text label background. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor TextLabelBackgroundColor;

	/** A bindable delegate for the TextLabelBackgroundColor. */
	UPROPERTY()
	FGetLinearColor TextLabelBackgroundColorDelegate;

	/** If true, show text label only on hover; if false always show label. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	bool ShowLabelOnlyOnHover;

	/** Whether to show the units part of the text label. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	bool ShowUnitsText;

	/** Whether to set the units part of the text label read only. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	bool IsUnitsTextReadOnly;

	/** Whether to set the value part of the text label read only. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	bool IsValueTextReadOnly;

	/** A bindable delegate to allow logic to drive the value of the widget */
	UPROPERTY()
	FGetFloat ValueDelegate;

	/** The color to draw the slider background. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor SliderBackgroundColor;

	/** A bindable delegate for the SliderBackgroundColor. */
	UPROPERTY()
	FGetLinearColor SliderBackgroundColorDelegate;

	/** The color to draw the slider bar. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor SliderBarColor;

	/** A bindable delegate for the SliderBarColor. */
	UPROPERTY()
	FGetLinearColor SliderBarColorDelegate;

	/** The color to draw the slider thumb. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor SliderThumbColor;

	/** A bindable delegate for the SliderThumbColor. */
	UPROPERTY()
	FGetLinearColor SliderThumbColorDelegate;

	/** The color to draw the widget background. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FLinearColor WidgetBackgroundColor;

	/** A bindable delegate for the WidgetBackgroundColor. */
	UPROPERTY()
	FGetLinearColor WidgetBackgroundColorDelegate;

public:
	/** Get output value from normalized linear (0 - 1) based on internal lin to output mapping. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	float GetOutputValue(const float InSliderValue);

	/** Get normalized linear (0 - 1) value from output based on internal lin to output mapping. */
	UFUNCTION(BlueprintCallable, Category = "Behavior", meta=(DeprecatedFunction, DeprecationMessage="5.1 - GetLinValue is deprecated, please use GetSliderValue instead."))
	float GetLinValue(const float OutputValue);

	/** Get normalized linear (0 - 1) slider value from output based on internal lin to output mapping. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	float GetSliderValue(const float OutputValue);

	/** Sets the label background color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetTextLabelBackgroundColor(FSlateColor InColor);

	/** Sets the units text */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetUnitsText(const FText Units);
	
	/** Sets whether the units text is read only*/
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetUnitsTextReadOnly(const bool bIsReadOnly);

	/** Sets whether the value text is read only*/
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetValueTextReadOnly(const bool bIsReadOnly);
	
	/** If true, show text label only on hover; if false always show label. */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetShowLabelOnlyOnHover(const bool bShowLabelOnlyOnHover);
	
	/** Sets whether to show the units text */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetShowUnitsText(const bool bShowUnitsText);

	/** The slider's orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	TEnumAsByte<EOrientation> Orientation;

	/** Called when the value is changed by slider or typing. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnFloatValueChangedEvent OnValueChanged;

	/** Sets the slider background color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetSliderBackgroundColor(FLinearColor InValue);

	/** Sets the slider bar color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetSliderBarColor(FLinearColor InValue);

	/** Sets the slider thumb color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetSliderThumbColor(FLinearColor InValue);

	/** Sets the widget background color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetWidgetBackgroundColor(FLinearColor InValue);

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	FAudioSliderStyle WidgetStyle;
	
	/** Native Slate Widget */
	TSharedPtr<SAudioSliderBase> MyAudioSlider;

	void HandleOnValueChanged(float InValue);

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget();
	// End of UWidget interface

	PROPERTY_BINDING_IMPLEMENTATION(float, Value);
	PROPERTY_BINDING_IMPLEMENTATION(FLinearColor, TextLabelBackgroundColor);
	PROPERTY_BINDING_IMPLEMENTATION(FLinearColor, SliderBackgroundColor);
	PROPERTY_BINDING_IMPLEMENTATION(FLinearColor, SliderBarColor);
	PROPERTY_BINDING_IMPLEMENTATION(FLinearColor, SliderThumbColor);
	PROPERTY_BINDING_IMPLEMENTATION(FLinearColor, WidgetBackgroundColor);
};

/**
 * An audio slider widget with customizable curves.
 */
UCLASS()
class AUDIOWIDGETS_API UAudioSlider : public UAudioSliderBase
{
	GENERATED_UCLASS_BODY()

	/** Curves for mapping linear to output values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	TWeakObjectPtr<const UCurveFloat> LinToOutputCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	TWeakObjectPtr <const UCurveFloat> OutputToLinCurve;

protected:
	virtual void SynchronizeProperties() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

/**
 * An audio slider widget with default customizable curves for volume (dB).
 */
UCLASS()
class AUDIOWIDGETS_API UAudioVolumeSlider : public UAudioSlider
{
	GENERATED_UCLASS_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

/**
 * An audio slider widget, for use with frequency. 
 */
UCLASS()
class AUDIOWIDGETS_API UAudioFrequencySlider : public UAudioSliderBase
{
	GENERATED_UCLASS_BODY()
	
	/** Frequency output range */
	UPROPERTY(EditAnywhere, Category = Behavior)
	FVector2D OutputRange = FVector2D(MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
