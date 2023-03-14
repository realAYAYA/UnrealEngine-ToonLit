// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Components/TextWidgetTypes.h"
#include "Widgets/Text/ISlateEditableTextWidget.h"
#include "EditableTextBox.generated.h"

class SEditableTextBox;
class USlateWidgetStyleAsset;

/**
 * Allows the user to type in custom text.  Only permits a single line of text to be entered.
 * 
 * * No Children
 * * Text Entry
 */
UCLASS(meta=(DisplayName="Text Box"))
class UMG_API UEditableTextBox : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditableTextBoxChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEditableTextBoxCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	/** The text content for this editable text box widget */
	UE_DEPRECATED(5.1, "Direct access to Text is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetText", BlueprintSetter = "SetText", FieldNotify, Category = "Content")
	FText Text;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance, meta=(DisplayName="Style", ShowOnlyInnerProperties))
	FEditableTextBoxStyle WidgetStyle;

	/** Hint text that appears when there is no text in the text box */
	UE_DEPRECATED(5.1, "Direct access to Hint Text is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetHintText", Category = Content, meta = (MultiLine = "true"))
	FText HintText;

	/** A bindable delegate to allow logic to drive the hint text of the widget */
	UPROPERTY()
	FGetText HintTextDelegate;

	/** Sets the Text Box as Readonly to prevent it from being modified interactively by the user */
	UE_DEPRECATED(5.1, "Direct access to IsReadOnly is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = Appearance)
	bool IsReadOnly;

	/** Sets whether this text box is for storing a password */
	UE_DEPRECATED(5.1, "Direct access to IsPassword is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetIsPassword", Category = Appearance)
	bool IsPassword;

	/** The minimum desired size for the text */
	UE_DEPRECATED(5.1, "Direct access to MinimumDesiredWidth is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter=SetMinDesiredWidth, Category = "Appearance")
	float MinimumDesiredWidth;

	/** Workaround as we lose focus when the auto completion closes. */
	UE_DEPRECATED(5.1, "Direct access to IsCaretMovedWhenGainFocus is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Behavior, AdvancedDisplay)
	bool IsCaretMovedWhenGainFocus;

	/** Whether to select all text when the user clicks to give focus on the widget */
	UE_DEPRECATED(5.1, "Direct access to SelectAllTextWhenFocused is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextWhenFocused;

	/** Whether to allow the user to back out of changes when they press the escape key */
	UE_DEPRECATED(5.1, "Direct access to RevertTextOnEscape is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Behavior, AdvancedDisplay)
	bool RevertTextOnEscape;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	UE_DEPRECATED(5.1, "Direct access to ClearKeyboardFocusOnCommit is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Behavior, AdvancedDisplay)
	bool ClearKeyboardFocusOnCommit;

	/** Whether to select all text when pressing enter to commit changes */
	UE_DEPRECATED(5.1, "Direct access to SelectAllTextOnCommit is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextOnCommit;

	/** Whether the context menu can be opened */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool AllowContextMenu;

	/** If we're on a platform that requires a virtual keyboard, what kind of keyboard should this widget use? */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	TEnumAsByte<EVirtualKeyboardType::Type> KeyboardType;

	/** Additional options to use for the virtual keyboard summoned by this widget */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	FVirtualKeyboardOptions VirtualKeyboardOptions;

	/** The type of event that will trigger the display of the virtual keyboard */
	UPROPERTY(EditAnywhere, Category = Behavior, AdvancedDisplay)
	EVirtualKeyboardTrigger VirtualKeyboardTrigger;

	/** What action should be taken when the virtual keyboard is dismissed? */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	EVirtualKeyboardDismissAction VirtualKeyboardDismissAction;
	
	/** How the text should be aligned with the margin. */
	UE_DEPRECATED(5.1, "Direct access to Justification is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = SetJustification, Category = Appearance)
	TEnumAsByte<ETextJustify::Type> Justification;

	/** Sets what happens to text that is clipped and doesn't fit within the clip rect for this widget */
	UE_DEPRECATED(5.1, "Direct access to OverflowPolicy is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter="GetTextOverflowPolicy", Setter= "SetTextOverflowPolicy", BlueprintSetter = "SetTextOverflowPolicy", Category = "Clipping", AdvancedDisplay, meta = (DisplayName = "Overflow Policy"))
	ETextOverflowPolicy OverflowPolicy;


	/** Controls how the text within this widget should be shaped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Localization, AdvancedDisplay, meta=(ShowOnlyInnerProperties))
	FShapedTextOptions ShapedTextOptions;

public:

	/** Called whenever the text is changed programmatically or interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="TextBox|Event")
	FOnEditableTextBoxChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="TextBox|Event")
	FOnEditableTextBoxCommittedEvent OnTextCommitted;

	/** Provide a alternative mechanism for error reporting. */
	//SLATE_ARGUMENT(TSharedPtr<class IErrorReportingWidget>, ErrorReporting)

public:

	/**
	* Gets the widget text
	* @return The widget text
	*/
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Text Box)"))
	FText GetText() const;

	/**
	* Directly sets the widget text.
	* Warning: This will wipe any binding created for the Text property!
	* @param InText The text to assign to the widget
	*/
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Text Box)"))
	void SetText(FText InText);
	
	/** Gets the Hint text that appears when there is no text in the text box */
	FText GetHintText() const;

	/**
	* Sets the Hint text that appears when there is no text in the text box
	* @param InHintText The text that appears when there is no text in the text box
	*/
	UFUNCTION(BlueprintCallable, Category = "Widget", meta = (DisplayName = "Set Hint Text (Text Box)"))
	void SetHintText(FText InText);

	/** @return the minimum desired width for this text box */
	float GetMinimumDesiredWidth() const;

	/**
	*  Set the minimum desired width for this text box
	*
	*  @param InMinDesiredWidth new minimum desired width
	*/
	void SetMinDesiredWidth(float InMinDesiredWidth);
	
	/** When set to true the caret is moved when gaining focus */
	void SetIsCaretMovedWhenGainFocus(bool bIsCaretMovedWhenGainFocus);

	/** Return true when the caret is moved when gaining focus */
	bool GetIsCaretMovedWhenGainFocus() const;

	/** Set to true to select all text when the user clicks to give focus on the widget */
	void SetSelectAllTextWhenFocused(bool bSelectAllTextWhenFocused);

	/** Whether to select all text when the user clicks to give focus on the widget */
	bool GetSelectAllTextWhenFocused() const;

	/** Set to true to allow the user to back out of changes when they press the escape key */
	void SetRevertTextOnEscape(bool bRevertTextOnEscape);

	/** Whether to allow the user to back out of changes when they press the escape key  */
	bool GetRevertTextOnEscape() const;

	/** Set to true to clear keyboard focus when pressing enter to commit changes */
	void SetClearKeyboardFocusOnCommit(bool bClearKeyboardFocusOnCommit);

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	bool GetClearKeyboardFocusOnCommit() const;

	/** Set to true to select all text when pressing enter to commit changes */
	void SetSelectAllTextOnCommit(bool bSelectAllTextOnCommit);

	/** Whether to select all text when pressing enter to commit changes */
	bool GetSelectAllTextOnCommit() const;

	UFUNCTION(BlueprintCallable, Category="Widget",  meta=(DisplayName="SetError (Text Box)"))
	void SetError(FText InError);

	/** Return true when this text cannot be modified interactively by the user */
	bool GetIsReadOnly() const;

	/** Sets the Text as Readonly to prevent it from being modified interactively by the user */
	UFUNCTION(BlueprintCallable, Category = "Widget", meta = (DisplayName = "SetIsReadOnly (Editable Text)"))
	void SetIsReadOnly(UPARAM(DisplayName = "ReadyOnly") bool bReadOnly);

	bool GetIsPassword() const;

	UFUNCTION(BlueprintCallable, Category = "Widget", meta = (DisplayName = "IsPassword"))
	void SetIsPassword(bool bIsPassword);

	UFUNCTION(BlueprintCallable, Category="Widget")
	void ClearError();

	UFUNCTION(BlueprintCallable, Category = "Widget")
	bool HasError() const;

	ETextJustify::Type GetJustification() const;

	UFUNCTION(BlueprintSetter)
	void SetJustification(ETextJustify::Type InJustification);

	/** @return the text overflow policy for this text block. */
	ETextOverflowPolicy GetTextOverflowPolicy() const;

	/**
	 * Set the text overflow policy for this text box.
	 *
	 * @param InOverflowPolicy the new text overflow policy.
	 */
	UFUNCTION(BlueprintSetter)
	void SetTextOverflowPolicy(ETextOverflowPolicy InOverflowPolicy);

	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetForegroundColor (Text Box)"))
	void SetForegroundColor(FLinearColor color);

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
	virtual void Serialize(FArchive& Ar) override;

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	virtual void HandleOnTextChanged(const FText& Text);
	virtual void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

#if WITH_ACCESSIBILITY
	virtual TSharedPtr<SWidget> GetAccessibleWidget() const override;
#endif

protected:
	TSharedPtr<SEditableTextBox> MyEditableTextBlock;

	PROPERTY_BINDING_IMPLEMENTATION(FText, Text);
	PROPERTY_BINDING_IMPLEMENTATION(FText, HintText);

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bIsFontDeprecationDone;
#endif
};
