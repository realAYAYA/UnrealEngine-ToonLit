// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"

#include "CheckBox.generated.h"

class SCheckBox;
class USlateBrushAsset;
class USlateWidgetStyleAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FOnCheckBoxComponentStateChanged, bool, bIsChecked );

/**
 * The checkbox widget allows you to display a toggled state of 'unchecked', 'checked' and 
 * 'indeterminable.  You can use the checkbox for a classic checkbox, or as a toggle button,
 * or as radio buttons.
 * 
 * * Single Child
 * * Toggle
 */
UCLASS()
class UMG_API UCheckBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** Whether the check box is currently in a checked state */
	UE_DEPRECATED(5.1, "Direct access to CheckedState is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter="GetCheckedState", BlueprintSetter="SetCheckedState", FieldNotify, Category="Appearance")
	ECheckBoxState CheckedState;

	/** A bindable delegate for the IsChecked. */
	UPROPERTY()
	FGetCheckBoxState CheckedStateDelegate;

	/** The checkbox bar style */
	UE_DEPRECATED(5.1, "Direct access to WidgetStyle is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Style", meta = (DisplayName="Style"))
	FCheckBoxStyle WidgetStyle;

	/** How the content of the toggle button should align within the given space */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The type of mouse action required by the user to trigger the buttons 'Click' */
	UE_DEPRECATED(5.1, "Direct access to ClickMethod is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetClickMethod", Category="Interaction", AdvancedDisplay)
	TEnumAsByte<EButtonClickMethod::Type> ClickMethod;

	/** The type of touch action required by the user to trigger the buttons 'Click' */
	UE_DEPRECATED(5.1, "Direct access to TouchMethod is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetTouchMethod", Category="Interaction", AdvancedDisplay)
	TEnumAsByte<EButtonTouchMethod::Type> TouchMethod;

	/** The type of keyboard/gamepad button press action required by the user to trigger the buttons 'Click' */
	UE_DEPRECATED(5.1, "Direct access to PressMethod is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetPressMethod", Category="Interaction", AdvancedDisplay)
	TEnumAsByte<EButtonPressMethod::Type> PressMethod;

	/** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	bool IsFocusable;

public:

	/** Called when the checked state has changed */
	UPROPERTY(BlueprintAssignable, Category="CheckBox|Event")
	FOnCheckBoxComponentStateChanged OnCheckStateChanged;

public:

	/** Returns true if this button is currently pressed */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool IsPressed() const;
	
	/** Returns true if the checkbox is currently checked */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool IsChecked() const;

	/** Returns the full current checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	ECheckBoxState GetCheckedState() const;

	/** Sets the checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetIsChecked(bool InIsChecked);

	/** Sets the checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetCheckedState(ECheckBoxState InCheckedState);

	/** Returns the local style. */
	const FCheckBoxStyle& GetWidgetStyle() const;

	/** Sets the style. */
	void SetWidgetStyle(const FCheckBoxStyle& InStyle);

	/** Returns the click method. */
	EButtonClickMethod::Type GetClickMethod() const;

	/** Sets the click method. */
	UFUNCTION(BlueprintCallable, Category="Button")
	void SetClickMethod(EButtonClickMethod::Type InClickMethod);

	/** Returns the touch method. */
	EButtonTouchMethod::Type GetTouchMethod() const;

	/** Sets the touch method. */
	UFUNCTION(BlueprintCallable, Category="Button")
	void SetTouchMethod(EButtonTouchMethod::Type InTouchMethod);

	/** Returns the press method. */
	EButtonPressMethod::Type GetPressMethod() const;

	/** Sets the press method. */
	UFUNCTION(BlueprintCallable, Category="Button")
	void SetPressMethod(EButtonPressMethod::Type InPressMethod);

public:
	
	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
#if WITH_EDITOR
	virtual TSharedRef<SWidget> RebuildDesignWidget(TSharedRef<SWidget> Content) override
	{
		return Content;
	}
#endif
	//~ End UWidget Interface

	void SlateOnCheckStateChangedCallback(ECheckBoxState NewState);

#if WITH_ACCESSIBILITY
	virtual TSharedPtr<SWidget> GetAccessibleWidget() const override;
#endif
	
protected:
	TSharedPtr<SCheckBox> MyCheckbox;

	PROPERTY_BINDING_IMPLEMENTATION(ECheckBoxState, CheckedState)
};
