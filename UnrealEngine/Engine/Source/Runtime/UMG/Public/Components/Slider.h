// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Slider.generated.h"

class SSlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMouseCaptureBeginEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMouseCaptureEndEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControllerCaptureBeginEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControllerCaptureEndEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFloatValueChangedEvent, float, Value);

/**
 * A simple widget that shows a sliding bar with a handle that allows you to control the value between 0..1.
 *
 * * No Children
 */
UCLASS()
class UMG_API USlider : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The volume value to display. */
	UE_DEPRECATED(5.1, "Direct access to Value is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, FieldNotify, BlueprintGetter="GetValue", BlueprintSetter="SetValue", Category="Appearance", meta=(UIMin="0", UIMax="1"))
	float Value;

	/** A bindable delegate to allow logic to drive the value of the widget */
	UPROPERTY()
	FGetFloat ValueDelegate;

	/** The minimum value the slider can be set to. */
	UE_DEPRECATED(5.1, "Direct access to MinValue is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetMinValue", Category="Appearance")
	float MinValue;

	/** The maximum value the slider can be set to. */
	UE_DEPRECATED(5.1, "Direct access to MaxValue is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetMaxValue", Category="Appearance")
	float MaxValue;

public:
	/** The progress bar style */
	UE_DEPRECATED(5.1, "Direct access to WidgetStyle is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Style", meta=( DisplayName="Style" ))
	FSliderStyle WidgetStyle;

	/** The slider's orientation. */
	UE_DEPRECATED(5.1, "Direct access to Orientation is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Appearance)
	TEnumAsByte<EOrientation> Orientation;

	/** The color to draw the slider bar in. */
	UE_DEPRECATED(5.1, "Direct access to SliderBarColor is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetSliderBarColor", Category="Appearance")
	FLinearColor SliderBarColor;

	/** The color to draw the slider handle in. */
	UE_DEPRECATED(5.1, "Direct access to SliderHandleColor is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetSliderHandleColor", Category="Appearance")
	FLinearColor SliderHandleColor;

	/** Whether the slidable area should be indented to fit the handle. */
	UE_DEPRECATED(5.1, "Direct access to IndentHandle is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter="HasIndentHandle", Setter, BlueprintSetter="SetIndentHandle", Category="Appearance", AdvancedDisplay)
	bool IndentHandle;

	/** Whether the handle is interactive or fixed. */
	UE_DEPRECATED(5.1, "Direct access to Locked is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter="IsLocked", Setter, BlueprintSetter="SetLocked", Category="Appearance", AdvancedDisplay)
	bool Locked;

	/** Sets new value if mouse position is greater/less than half the step size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance, AdvancedDisplay)
	bool MouseUsesStep;

	/** Sets whether we have to lock input to change the slider value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance, AdvancedDisplay)
	bool RequiresControllerLock;

	/** The amount to adjust the value by, when using a controller or keyboard */
	UE_DEPRECATED(5.1, "Direct access to StepSize is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetStepSize", Category="Appearance", meta=(UIMin="0", UIMax="1"))
	float StepSize;

	/** Should the slider be focusable? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	bool IsFocusable;

public:

	/** Invoked when the mouse is pressed and a capture begins. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnMouseCaptureBeginEvent OnMouseCaptureBegin;

	/** Invoked when the mouse is released and a capture ends. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnMouseCaptureEndEvent OnMouseCaptureEnd;

	/** Invoked when the controller capture begins. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnControllerCaptureBeginEvent OnControllerCaptureBegin;

	/** Invoked when the controller capture ends. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnControllerCaptureEndEvent OnControllerCaptureEnd;

	/** Called when the value is changed by slider or typing. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnFloatValueChangedEvent OnValueChanged;

	/** Gets the current value of the slider. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	float GetValue() const;

	/** Get the current value scaled from 0 to 1 */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	float GetNormalizedValue() const;

	/** Sets the current value of the slider. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetValue(float InValue);

	/** Gets the minimum value of the slider. */
	float GetMinValue() const;

	/** Sets the minimum value of the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetMinValue(float InValue);

	/** Gets the maximum value of the slider. */
	float GetMaxValue() const;

	/** Sets the maximum value of the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetMaxValue(float InValue);

	/** Get the style used by the widget. */
	const FSliderStyle& GetWidgetStyle() const;

	/** Set the style used by the widget. */
	void SetWidgetStyle(const FSliderStyle& InStyle);

	/** Getg the slider's orientation. */
	EOrientation GetOrientation() const;

	/** Sets the slider's orientation. */
	void SetOrientation(EOrientation InOrientation);

	/** Gets if the slidable area should be indented to fit the handle. */
	bool HasIndentHandle() const;

	/** Sets if the slidable area should be indented to fit the handle. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetIndentHandle(bool InValue);

	/** Returns true when the handle is fixed. */
	bool IsLocked() const;

	/** Sets the handle to be interactive or fixed. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetLocked(bool InValue);

	/** Gets the amount to adjust the value by. */
	float GetStepSize() const;

	/** Sets the amount to adjust the value by, when using a controller or keyboard. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetStepSize(float InValue);

	/** Gets the color of the slider bar. */
	FLinearColor GetSliderBarColor() const;

	/** Sets the color of the slider bar. */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetSliderBarColor(FLinearColor InValue);

	/** Gets the color of the handle bar */
	FLinearColor GetSliderHandleColor() const;

	/** Sets the color of the handle bar */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetSliderHandleColor(FLinearColor InValue);
	
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
	/** Native Slate Widget */
	TSharedPtr<SSlider> MySlider;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	void HandleOnValueChanged(float InValue);
	void HandleOnMouseCaptureBegin();
	void HandleOnMouseCaptureEnd();
	void HandleOnControllerCaptureBegin();
	void HandleOnControllerCaptureEnd();

#if WITH_ACCESSIBILITY
	virtual TSharedPtr<SWidget> GetAccessibleWidget() const override;
#endif

	PROPERTY_BINDING_IMPLEMENTATION(float, Value);
};
