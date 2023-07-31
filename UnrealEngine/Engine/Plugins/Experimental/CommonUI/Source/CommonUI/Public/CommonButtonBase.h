// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "CommonUITypes.h"
#include "Components/Button.h"
#include "Misc/Optional.h"
#include "Types/ISlateMetaData.h"
#include "CommonInputModeTypes.h"
#include "CommonButtonBase.generated.h"

class UCommonTextStyle;
class SBox;
class UCommonActionWidget;

class COMMONUI_API FCommonButtonMetaData : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FCommonButtonMetaData, ISlateMetaData)

	FCommonButtonMetaData(const UCommonButtonBase& InOwningCommonButtonInternal):OwningCommonButton(&InOwningCommonButtonInternal)
	{}

	const TWeakObjectPtr<const UCommonButtonBase> OwningCommonButton;
};

USTRUCT(BlueprintType)
struct FCommonButtonStyleOptionalSlateSound
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bHasSound = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties", meta = (EditCondition = "bHasSound"))
	FSlateSound Sound;

	explicit operator bool() const
	{
		return bHasSound;
	}
};


/* ---- All properties must be EditDefaultsOnly, BlueprintReadOnly !!! -----
 *       we return the CDO to blueprints, so we cannot allow any changes (blueprint doesn't support const variables)
 */
UCLASS(Abstract, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI"))
class COMMONUI_API UCommonButtonStyle : public UObject
{
	GENERATED_BODY()

	virtual bool NeedsLoadForServer() const override;

public:
	
	/** Whether or not the style uses a drop shadow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	bool bSingleMaterial;

	/** The normal (un-selected) brush to apply to each size of this button */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties", meta = (EditCondition = "bSingleMaterial"))
	FSlateBrush SingleMaterialBrush;

	/** The normal (un-selected) brush to apply to each size of this button */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Normal", meta = (EditCondition = "!bSingleMaterial"))
	FSlateBrush NormalBase;

	/** The normal (un-selected) brush to apply to each size of this button when hovered */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Normal", meta = (EditCondition = "!bSingleMaterial"))
	FSlateBrush NormalHovered;

	/** The normal (un-selected) brush to apply to each size of this button when pressed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Normal", meta = (EditCondition = "!bSingleMaterial"))
	FSlateBrush NormalPressed;

	/** The selected brush to apply to each size of this button */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selected", meta = (EditCondition = "!bSingleMaterial"))
	FSlateBrush SelectedBase;

	/** The selected brush to apply to each size of this button when hovered */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selected", meta = (EditCondition = "!bSingleMaterial"))
	FSlateBrush SelectedHovered;

	/** The selected brush to apply to each size of this button when pressed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Selected", meta = (EditCondition = "!bSingleMaterial"))
	FSlateBrush SelectedPressed;

	/** The disabled brush to apply to each size of this button */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Disabled", meta = (EditCondition = "!bSingleMaterial"))
	FSlateBrush Disabled;

	/** The button content padding to apply for each size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	FMargin ButtonPadding;
	
	/** The custom padding of the button to use for each size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	FMargin CustomPadding;

	/** The minimum width of buttons using this style */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	int32 MinWidth;

	/** The minimum height of buttons using this style */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	int32 MinHeight;

	/** The text style to use when un-selected */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	TSubclassOf<UCommonTextStyle> NormalTextStyle;

	/** The text style to use when un-selected */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	TSubclassOf<UCommonTextStyle> NormalHoveredTextStyle;

	/** The text style to use when selected */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	TSubclassOf<UCommonTextStyle> SelectedTextStyle;

	/** The text style to use when un-selected */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	TSubclassOf<UCommonTextStyle> SelectedHoveredTextStyle;

	/** The text style to use when disabled */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	TSubclassOf<UCommonTextStyle> DisabledTextStyle;

	/** The sound to play when the button is pressed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties", meta = (DisplayName = "Pressed Sound"))
	FSlateSound PressedSlateSound;

	/** The sound to play when the button is pressed while selected */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties", meta = (DisplayName = "Selected Pressed Sound"))
	FCommonButtonStyleOptionalSlateSound SelectedPressedSlateSound;

	/** The sound to play when the button is pressed while locked */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties", meta = (DisplayName = "Locked Pressed Sound"))
	FCommonButtonStyleOptionalSlateSound LockedPressedSlateSound;
	
	/** The sound to play when the button is hovered */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties", meta = (DisplayName = "Hovered Sound"))
	FSlateSound HoveredSlateSound;

	/** The sound to play when the button is hovered while selected */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties", meta = (DisplayName = "Selected Hovered Sound"))
	FCommonButtonStyleOptionalSlateSound SelectedHoveredSlateSound;
	
	/** The sound to play when the button is hovered while locked */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties", meta = (DisplayName = "Locked Hovered Sound"))
	FCommonButtonStyleOptionalSlateSound LockedHoveredSlateSound;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetMaterialBrush(FSlateBrush& Brush) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetNormalBaseBrush(FSlateBrush& Brush) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetNormalHoveredBrush(FSlateBrush& Brush) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetNormalPressedBrush(FSlateBrush& Brush) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetSelectedBaseBrush(FSlateBrush& Brush) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetSelectedHoveredBrush(FSlateBrush& Brush) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetSelectedPressedBrush(FSlateBrush& Brush) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetDisabledBrush(FSlateBrush& Brush) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetButtonPadding(FMargin& OutButtonPadding) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	void GetCustomPadding(FMargin& OutCustomPadding) const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	UCommonTextStyle* GetNormalTextStyle() const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	UCommonTextStyle* GetNormalHoveredTextStyle() const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	UCommonTextStyle* GetSelectedTextStyle() const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	UCommonTextStyle* GetSelectedHoveredTextStyle() const;

	UFUNCTION(BlueprintCallable, Category = "Common ButtonStyle|Getters")
	UCommonTextStyle* GetDisabledTextStyle() const;

};

DECLARE_DELEGATE_RetVal(FReply, FOnButtonDoubleClickedEvent);

/** Custom UButton override that allows us to disable clicking without disabling the widget entirely */
UCLASS(Experimental)	// "Experimental" to hide it in the designer
class COMMONUI_API UCommonButtonInternalBase : public UButton
{
	GENERATED_UCLASS_BODY()

public:
	void SetButtonEnabled(bool bInIsButtonEnabled);
	void SetInteractionEnabled(bool bInIsInteractionEnabled);

	/** Updates the IsFocusable flag and updates the bIsFocusable flag of the underlying slate button widget */
	void SetButtonFocusable(bool bInIsButtonFocusable);
	bool IsHovered() const;
	bool IsPressed() const;

	void SetMinDesiredHeight(int32 InMinHeight);
	void SetMinDesiredWidth(int32 InMinWidth);

	/** Called when the button is clicked */
	FOnButtonDoubleClickedEvent HandleDoubleClicked;

	/** Called when the button is clicked */
	UPROPERTY(BlueprintAssignable, Category = "Common Button Internal|Event")
	FOnButtonClickedEvent OnDoubleClicked;

	/** Called when the button receives focus */
	FSimpleDelegate OnReceivedFocus;

	/** Called when the button loses focus */
	FSimpleDelegate OnLostFocus;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UWidget interface

	virtual FReply SlateHandleClickedOverride();
	virtual void SlateHandlePressedOverride();
	virtual void SlateHandleReleasedOverride();
	virtual FReply SlateHandleDoubleClicked();

	/** Called when internal slate button receives focus; Fires OnReceivedFocus */
	void SlateHandleOnReceivedFocus();

	/** Called when internal slate button loses focus; Fires OnLostFocus */
	void SlateHandleOnLostFocus();

	/** The minimum width of the button */
	UPROPERTY()
	int32 MinWidth;

	/** The minimum height of the button */
	UPROPERTY()
	int32 MinHeight;

	/** If true, this button is enabled. */
	UPROPERTY()
	bool bButtonEnabled;

	/** If true, this button can be interacted with it normally. Otherwise, it will not react to being hovered or clicked. */
	UPROPERTY()
	bool bInteractionEnabled;

	/** Cached pointer to the underlying slate button owned by this UWidget */
	TSharedPtr<SBox> MyBox;

	/** Cached pointer to the underlying slate button owned by this UWidget */
	TSharedPtr<class SCommonButton> MyCommonButton;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonSelectedStateChangedBase, class UCommonButtonBase*, Button, bool, Selected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonButtonBaseClicked, class UCommonButtonBase*, Button);

/**
 * Button that disables itself when not active. Also updates actions for CommonActionWidget if bound to display platform-specific icons.
 */
UCLASS(Abstract, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI", DisableNativeTick))
class COMMONUI_API UCommonButtonBase : public UCommonUserWidget
{
	GENERATED_UCLASS_BODY()

public:
	// UWidget interface
	virtual bool IsHovered() const override;
	// End of UWidget interface

	// UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool Initialize() override;
	virtual void SetIsEnabled(bool bInIsEnabled) override;
	virtual bool NativeIsInteractable() const override;
	// End of UUserWidget interface
	
	/** Disables this button with a reason (use instead of SetIsEnabled) */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void DisableButtonWithReason(const FText& DisabledReason);

	/** Change whether this widget is selectable at all. If false and currently selected, will deselect. */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetIsInteractionEnabled(bool bInIsInteractionEnabled);

	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetHideInputAction(bool bInHideInputAction);

	/** Is this button currently interactable? (use instead of GetIsEnabled) */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	bool IsInteractionEnabled() const;

	/** Is this button currently pressed? */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	bool IsPressed() const;

	/** Set the click method for mouse interaction */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetClickMethod(EButtonClickMethod::Type InClickMethod);

	/** Set the click method for touch interaction */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetTouchMethod(EButtonTouchMethod::Type InTouchMethod);

	/** Set the click method for keyboard/gamepad button press interaction */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetPressMethod(EButtonPressMethod::Type InPressMethod);

	/** Change whether this widget is selectable at all. If false and currently selected, will deselect. */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetIsSelectable(bool bInIsSelectable);

	/** Change whether this widget is selectable at all. If false and currently selected, will deselect. */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetIsInteractableWhenSelected(bool bInInteractableWhenSelected);

	/** Change whether this widget is toggleable. If toggleable, clicking when selected will deselect. */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetIsToggleable(bool bInIsToggleable);

	/** Change whether this widget should use the fallback default input action. */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetShouldUseFallbackDefaultInputAction(bool bInShouldUseFallbackDefaultInputAction);

	/** 
	 * Change the selected state manually.
	 * @param bGiveClickFeedback	If true, the button may give user feedback as if it were clicked. IE: Play a click sound, trigger animations as if it were clicked.
	 */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetIsSelected(bool InSelected, bool bGiveClickFeedback = true);

	/** Change whether this widget is locked. If locked, the button can be focusable and responsive to mouse input but will not broadcast OnClicked events. */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetIsLocked(bool bInIsLocked);

	/** @returns True if the button is currently in a selected state, False otherwise */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	bool GetSelected() const;

	/** @returns True if the button is currently locked, False otherwise */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	bool GetLocked() const;

	UFUNCTION(BlueprintCallable, Category = "Common Button" )
	void ClearSelection();

	/** Set whether the button should become selected upon receiving focus or not; Only settable for buttons that are selectable */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetShouldSelectUponReceivingFocus(bool bInShouldSelectUponReceivingFocus);

	/** Get whether the button should become selected upon receiving focus or not */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	bool GetShouldSelectUponReceivingFocus() const;

	/** Sets the style of this button, rebuilds the internal styling */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetStyle(TSubclassOf<UCommonButtonStyle> InStyle = nullptr);

	/** @Returns Current button style*/
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	UCommonButtonStyle* GetStyle() const;

	const UCommonButtonStyle* GetStyleCDO() const;

	/** @return The current button padding that corresponds to the current size and selection state */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	void GetCurrentButtonPadding(FMargin& OutButtonPadding) const;

	/** @return The custom padding that corresponds to the current size and selection state */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	void GetCurrentCustomPadding(FMargin& OutCustomPadding) const;
	
	/** @return The text style that corresponds to the current size and selection state */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	UCommonTextStyle* GetCurrentTextStyle() const;

	/** @return The class of the text style that corresponds to the current size and selection state */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	TSubclassOf<UCommonTextStyle> GetCurrentTextStyleClass() const;

	/** Sets the minimum dimensions of this button */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetMinDimensions(int32 InMinWidth, int32 InMinHeight);

	/** Updates the current triggered action */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetTriggeredInputAction(const FDataTableRowHandle &InputActionRow);

	/** Updates the current triggering action */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Setters")
	void SetTriggeringInputAction(const FDataTableRowHandle & InputActionRow);

	/** Gets the appropriate input action that is set */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	bool GetInputAction(FDataTableRowHandle &InputActionRow) const;

	/** Updates the bIsFocusable flag */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	void SetIsFocusable(bool bInIsFocusable);

	/** Gets the bIsFocusable flag */
	UFUNCTION(BlueprintCallable, Category = "Common Button|Getters")
	bool GetIsFocusable() const;

	/** Returns the dynamic instance of the material being used for this button, if it is using a single material style. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Common Button|Getters")
	UMaterialInstanceDynamic* GetSingleMaterialStyleMID() const;

	UFUNCTION(BlueprintCallable, Category = "Common Button|Input")
	void SetInputActionProgressMaterial(const FSlateBrush& InProgressMaterialBrush, const FName& InProgressMaterialParam);

	UFUNCTION(BlueprintCallable, Category = "Common Button|Sound")
	void SetPressedSoundOverride(USoundBase* Sound);

	UFUNCTION(BlueprintCallable, Category = "Common Button|Sound")
	void SetHoveredSoundOverride(USoundBase* Sound);

	UFUNCTION(BlueprintCallable, Category = "Common Button|Sound")
	void SetSelectedPressedSoundOverride(USoundBase* Sound);

	UFUNCTION(BlueprintCallable, Category = "Common Button|Sound")
	void SetSelectedHoveredSoundOverride(USoundBase* Sound);
	
	UFUNCTION(BlueprintCallable, Category = "Common Button|Sound")
	void SetLockedPressedSoundOverride(USoundBase* Sound);

	UFUNCTION(BlueprintCallable, Category = "Common Button|Sound")
	void SetLockedHoveredSoundOverride(USoundBase* Sound);

	DECLARE_EVENT(UCommonButtonBase, FCommonButtonEvent);
	FCommonButtonEvent& OnClicked() const { return OnClickedEvent; }
	FCommonButtonEvent& OnDoubleClicked() const { return OnDoubleClickedEvent; }
	FCommonButtonEvent& OnPressed() const { return OnPressedEvent; }
	FCommonButtonEvent& OnReleased() const { return OnReleasedEvent; }
	FCommonButtonEvent& OnHovered() const { return OnHoveredEvent; }
	FCommonButtonEvent& OnUnhovered() const { return OnUnhoveredEvent; }
	FCommonButtonEvent& OnFocusReceived() const { return OnFocusReceivedEvent; }
	FCommonButtonEvent& OnFocusLost() const { return OnFocusLostEvent; }
	FCommonButtonEvent& OnLockClicked() const { return OnLockClickedEvent; }
	FCommonButtonEvent& OnLockDoubleClicked() const { return OnLockDoubleClickedEvent; }

	DECLARE_EVENT_OneParam(UCommonButtonBase, FOnIsSelectedChanged, bool);
	FOnIsSelectedChanged& OnIsSelectedChanged() const { return OnIsSelectedChangedEvent; }

protected:
	bool IsPersistentBinding() const { return bIsPersistentBinding; }
	ECommonInputMode GetInputModeOverride() const { return InputModeOverride; }
	
	virtual UCommonButtonInternalBase* ConstructInternalButton();

	virtual void OnWidgetRebuilt();
	virtual void PostLoad() override;
	virtual void SynchronizeProperties() override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

#if WITH_EDITOR
	virtual void OnCreationFromPalette() override;
	const FText GetPaletteCategory() override;
#endif // WITH_EDITOR

	/** Helper function to bind to input method change events */
	virtual void BindInputMethodChangedDelegate();

	/** Helper function to unbind from input method change events */
	virtual void UnbindInputMethodChangedDelegate();

	/** Called via delegate when the input method changes */
	UFUNCTION()
	virtual void OnInputMethodChanged(ECommonInputType CurrentInputType);

	/** Associates this button at its priority with the given key */
	virtual void BindTriggeringInputActionToClick();

	/** Associates this button at its priority with the given key */
	virtual void UnbindTriggeringInputActionToClick();

	UFUNCTION()
	virtual void HandleTriggeringActionCommited(bool& bPassthrough);

	//@TODO: DarenC - API decision, consider removing
	virtual void ExecuteTriggeredInput();

	/** Helper function to update the associated input action widget, if any, based upon the state of the button */
	virtual void UpdateInputActionWidget();

	/** Handler function registered to the underlying button's click. */
	UFUNCTION()
	void HandleButtonClicked();

	/** Handler function registered to the underlying button's double click. */
	FReply HandleButtonDoubleClicked();

	/** Helper function registered to the underlying button receiving focus */
	UFUNCTION()
	virtual void HandleFocusReceived();
	
	/** Helper function registered to the underlying button losing focus */
	UFUNCTION()
	virtual void HandleFocusLost();

	/** Helper function registered to the underlying button when pressed */
	UFUNCTION()
	void HandleButtonPressed();

	/** Helper function registered to the underlying button when released */
	UFUNCTION()
	void HandleButtonReleased();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Selected"))
	void BP_OnSelected();
	virtual void NativeOnSelected(bool bBroadcast);

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Deselected"))
	void BP_OnDeselected();
	virtual void NativeOnDeselected(bool bBroadcast);

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Hovered"))
	void BP_OnHovered();
	virtual void NativeOnHovered();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Unhovered"))
	void BP_OnUnhovered();
	virtual void NativeOnUnhovered();
	
	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Focused"))
	void BP_OnFocusReceived();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Unfocused"))
	void BP_OnFocusLost();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Locked Changed"))
	void BP_OnLockedChanged(bool bIsLocked);
	
	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Lock Clicked"))
	void BP_OnLockClicked();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Lock Double Clicked"))
	void BP_OnLockDoubleClicked();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Clicked"))
	void BP_OnClicked();
	virtual void NativeOnClicked();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Double Clicked"))
	void BP_OnDoubleClicked();
	virtual void NativeOnDoubleClicked();

	/** Unless this is called, we will assume the double click should be converted into a normal click. */
	UFUNCTION(BlueprintCallable, Category = CommonButton)
	void StopDoubleClickPropagation();
	
	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Pressed"))
	void BP_OnPressed();
	virtual void NativeOnPressed();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Released"))
	void BP_OnReleased();
	virtual void NativeOnReleased();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Enabled"))
	void BP_OnEnabled();
	virtual void NativeOnEnabled();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Disabled"))
	void BP_OnDisabled();
	virtual void NativeOnDisabled();

	UFUNCTION(BlueprintImplementableEvent, Category = CommonButton, meta = (DisplayName = "On Input Method Changed"))
	void BP_OnInputMethodChanged(ECommonInputType CurrentInputType);

	/** Allows derived classes to take action when the current text style has changed */
	UFUNCTION(BlueprintImplementableEvent, meta=(BlueprintProtected="true"), Category = "Common Button")
	void OnCurrentTextStyleChanged();
	virtual void NativeOnCurrentTextStyleChanged();

	/** Internal method to allow the selected state to be set regardless of selectability or toggleability */
	UFUNCTION(BlueprintCallable, meta=(BlueprintProtected="true"), Category = "Common Button")
	void SetSelectedInternal(bool bInSelected, bool bAllowSound = true, bool bBroadcast = true);

	UFUNCTION(BlueprintImplementableEvent, Category = "Common Button")
	void OnTriggeredInputActionChanged(const FDataTableRowHandle& NewTriggeredAction);

	UFUNCTION(BlueprintImplementableEvent, Category = "Common Button")
	void OnTriggeringInputActionChanged(const FDataTableRowHandle& NewTriggeredAction);

	UFUNCTION(BlueprintImplementableEvent, Category = "Common Button")
	void OnActionProgress(float HeldPercent);
	
	UFUNCTION()
	virtual void NativeOnActionProgress(float HeldPercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Common Button")
	void OnActionComplete();

	UFUNCTION()
	virtual void NativeOnActionComplete();

	virtual bool GetButtonAnalyticInfo(FString& ButtonName, FString& ABTestName, FString& ExtraData) const;

	void RefreshDimensions();
	virtual void NativeOnMouseEnter( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual void NativeOnMouseLeave( const FPointerEvent& InMouseEvent ) override;

	/** The minimum width of the button (only used if greater than the style's minimum) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Layout, meta = (ClampMin = "0"))
	int32 MinWidth;

	/** The minimum height of the button (only used if greater than the style's minimum) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Layout, meta = (ClampMin = "0"))
	int32 MinHeight;

	/** References the button style asset that defines a style in multiple sizes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (ExposeOnSpawn = true))
	TSubclassOf<UCommonButtonStyle> Style;

	/** Whether to hide the input action widget at all times (useful for textless small buttons) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style)
	bool bHideInputAction;

	/**
	 * Optional override for the sound to play when this button is pressed.
	 * Also used for the Selected and Locked Pressed state if their respective overrides are empty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (DisplayName = "Pressed Sound Override"))
	FSlateSound PressedSlateSoundOverride;

	/**
	 * Optional override for the sound to play when this button is hovered.
	 * Also used for the Selected and Locked Hovered state if their respective overrides are empty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (DisplayName = "Hovered Sound Override"))
	FSlateSound HoveredSlateSoundOverride;

	/** Optional override for the sound to play when this button is pressed while Selected */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (DisplayName = "Selected Pressed Sound Override"))
	FSlateSound SelectedPressedSlateSoundOverride;

	/** Optional override for the sound to play when this button is hovered while Selected */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (DisplayName = "Selected Hovered Sound Override"))
	FSlateSound SelectedHoveredSlateSoundOverride;

	/** Optional override for the sound to play when this button is pressed while Locked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (DisplayName = "Locked Pressed Sound Override"))
	FSlateSound LockedPressedSlateSoundOverride;

	/** Optional override for the sound to play when this button is hovered while Locked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sound, meta = (DisplayName = "Locked Hovered Sound Override"))
	FSlateSound LockedHoveredSlateSoundOverride;

	/** The type of mouse action required by the user to trigger the button's 'Click' */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (ExposeOnSpawn = true))
	uint8 bApplyAlphaOnDisable:1;

	/**
	 * True if this button is currently locked.
	 * Locked button can be hovered, focused, and pressed, but the Click event will not go through.
	 * Business logic behind it will not be executed. Designed for progressive disclosure
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Locked, meta = (ExposeOnSpawn = true))
	uint8 bLocked:1;
	
	/** True if the button supports being in a "selected" state, which will update the style accordingly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Selection, meta = (ExposeOnSpawn = true))
	uint8 bSelectable:1;
	
	/** If true, the button will be selected when it receives focus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Selection, meta = (ExposeOnSpawn = true, EditCondition = "bSelectable"))
	uint8 bShouldSelectUponReceivingFocus:1;

	/** If true, the button may be clicked while selected. Otherwise, interaction is disabled in the selected state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Selection, meta = (ExposeOnSpawn = true, EditCondition = "bSelectable"))
	uint8 bInteractableWhenSelected:1;

	/** True if the button can be deselected by clicking it when selected */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Selection, meta = (ExposeOnSpawn = true, EditCondition = "bSelectable"))
	uint8 bToggleable:1;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Selection, meta = (ExposeOnSpawn = true, EditCondition = "bSelectable"))
	uint8 bTriggerClickedAfterSelection:1;

	/** True if the input action should be displayed when the button is not interactable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (ExposeOnSpawn = true))
	uint8 bDisplayInputActionWhenNotInteractable:1;

	/** True if the input action should be hidden while the user is using a keyboard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (ExposeOnSpawn = true))
	uint8 bHideInputActionWithKeyboard:1;

	/** True if this button should use the default fallback input action (bool is useful for buttons that shouldn't because they are never directly hit via controller) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (ExposeOnSpawn = true))
	uint8 bShouldUseFallbackDefaultInputAction:1;

private:

	/** True if this button is currently selected */
	uint8 bSelected:1;

	/** True if this button is currently enabled */
	uint8 bButtonEnabled:1;

	/** True if interaction with this button is currently enabled */
	uint8 bInteractionEnabled:1;

public:
	/** The type of mouse action required by the user to trigger the button's 'Click' */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (ExposeOnSpawn = true))
	TEnumAsByte<EButtonClickMethod::Type> ClickMethod;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TEnumAsByte<EButtonTouchMethod::Type> TouchMethod;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TEnumAsByte<EButtonPressMethod::Type> PressMethod;

	/** This is the priority for the TriggeringInputAction.  The first, HIGHEST PRIORITY widget will handle the input action, and no other widgets will be considered.  Additionally, no inputs with a priority below the current ActivatablePanel's Input Priority value will even be considered! */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (ExposeOnSpawn = true))
	int32 InputPriority;

	/** 
	 *	The input action that is bound to this button. The common input manager will trigger this button to 
	 *	click if the action was pressed 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (ExposeOnSpawn = true, RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle TriggeringInputAction;

	/**
	 *	The input action that can be visualized as well as triggered when the user
	 *	clicks the button.
	 */
	FDataTableRowHandle TriggeredInputAction;

#if WITH_EDITORONLY_DATA
	/** Used to track widgets that were created before changing the default style pointer to null */
	UPROPERTY()
	bool bStyleNoLongerNeedsConversion;
#endif

protected:
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (AllowPrivateAccess = true))
	FCommonSelectedStateChangedBase OnSelectedChangedBase;

	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (AllowPrivateAccess = true))
	FCommonButtonBaseClicked OnButtonBaseClicked;

	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (AllowPrivateAccess = true))
	FCommonButtonBaseClicked OnButtonBaseDoubleClicked;

	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (AllowPrivateAccess = true))
	FCommonButtonBaseClicked OnButtonBaseHovered;

	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (AllowPrivateAccess = true))
	FCommonButtonBaseClicked OnButtonBaseUnhovered;

	FUIActionBindingHandle TriggeringBindingHandle;

private:
	friend class UCommonButtonGroupBase;
	template <typename> friend class SCommonButtonTableRow;

	/**
	 * DANGER! Be very, very careful with this. Unless you absolutely know what you're doing, this is not the property you're looking for.
	 *
	 * True to register the action bound to this button as a "persistent" binding. False (default) will register a standard activation-based binding.
	 * A persistent binding ignores the standard ruleset for UI input routing - the binding will be live immediately upon construction of the button.
	 */
	UPROPERTY(EditAnywhere, Category = Input, AdvancedDisplay)
	bool bIsPersistentBinding = false;

	//Set this to Game for special cases where an input action needs to be set for an in-game button.
	UPROPERTY(EditAnywhere, Category = Input, AdvancedDisplay)
	ECommonInputMode InputModeOverride = ECommonInputMode::Menu;
	
	void BuildStyles();
	void SetButtonStyle();
	void UpdateInputActionWidgetVisibility();

	/** Enables this button (called in SetIsEnabled override) */
	void EnableButton();

	/** Disables this button (called in SetIsEnabled override) */
	void DisableButton();

	FText EnabledTooltipText;
	FText DisabledTooltipText;

	/** The dynamic material instance of the material set by the single material style, if specified. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SingleMaterialStyleMID;

	/** Internally managed and applied style to use when not selected */
	UPROPERTY()
	FButtonStyle NormalStyle;
	
	/** Internally managed and applied style to use when selected */
	UPROPERTY()
	FButtonStyle SelectedStyle;

	/** Internally managed and applied style to use when disabled */
	UPROPERTY()
	FButtonStyle DisabledStyle;

	/** Internally managed and applied style to use when locked */
	UPROPERTY()
	FButtonStyle LockedStyle;

	UPROPERTY(Transient)
	uint32 bStopDoubleClickPropagation : 1;

	/**
	 * The actual UButton that we wrap this user widget into. 
	 * Allows us to get user widget customization and built-in button functionality. 
	 */
	TWeakObjectPtr<class UCommonButtonInternalBase> RootButton;

	mutable FCommonButtonEvent OnClickedEvent;
	mutable FCommonButtonEvent OnDoubleClickedEvent;
	mutable FCommonButtonEvent OnPressedEvent;
	mutable FCommonButtonEvent OnReleasedEvent;
	mutable FCommonButtonEvent OnHoveredEvent;
	mutable FCommonButtonEvent OnUnhoveredEvent;
	mutable FCommonButtonEvent OnFocusReceivedEvent;
	mutable FCommonButtonEvent OnFocusLostEvent;
	mutable FCommonButtonEvent OnLockClickedEvent;
	mutable FCommonButtonEvent OnLockDoubleClickedEvent;

	mutable FOnIsSelectedChanged OnIsSelectedChangedEvent;

protected:
	/**
	 * Optionally bound widget for visualization behavior of an input action;
	 * NOTE: If specified, will visualize according to the following algorithm:
	 * If TriggeringInputAction is specified, visualize it else:
	 * If TriggeredInputAction is specified, visualize it else:
	 * Visualize the default click action while hovered
	 */
	UPROPERTY(BlueprintReadOnly, Category = Input, meta = (BindWidget, OptionalWidget = true, AllowPrivateAccess = true))
	TObjectPtr<UCommonActionWidget> InputActionWidget;
};