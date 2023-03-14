// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/TextWidgetTypes.h"
#include "Widgets/Text/ISlateEditableTextWidget.h"
#include "MultiLineEditableText.generated.h"

class UMaterialInterface;
class SMultiLineEditableText;

/**
 * Editable text box widget
 */
UCLASS(meta=( DisplayName="Editable Text (Multi-Line)" ))
class UMG_API UMultiLineEditableText : public UTextLayoutWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMultiLineEditableTextChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMultiLineEditableTextCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	/** The text content for this editable text box widget */
	UE_DEPRECATED(5.1, "Direct access to Text is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetText", BlueprintSetter = "SetText", FieldNotify, Category = Content, meta = (MultiLine = "true"))
	FText Text;

	/** Hint text that appears when there is no text in the text box */
	UE_DEPRECATED(5.1, "Direct access to HintText is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter = "GetHintText", BlueprintSetter = "SetHintText", Category = Content, meta = (MultiLine = "true"))
	FText HintText;

	/** A bindable delegate to allow logic to drive the hint text of the widget */
	UPROPERTY()
	FGetText HintTextDelegate;
public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetWidgetStyle, Category="Style", meta=(ShowOnlyInnerProperties))
	FTextBlockStyle WidgetStyle;

	/** Sets the Text as Readonly to prevent it from being modified interactively by the user */
	UE_DEPRECATED(5.1, "Direct access to IsReadOnly is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = GetIsReadOnly, Setter = SetIsReadOnly, BlueprintSetter = "SetIsReadOnly", Category = Appearance)
	bool bIsReadOnly;

	/** Whether to select all text when the user clicks to give focus on the widget */
	UE_DEPRECATED(5.1, "Direct access to SelectAllTextWhenFocused is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = Behavior, AdvancedDisplay)
	bool SelectAllTextWhenFocused;

	/** Whether to clear text selection when focus is lost */
	UE_DEPRECATED(5.1, "Direct access to ClearTextSelectionOnFocusLoss is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, Category = Behavior, AdvancedDisplay)
	bool ClearTextSelectionOnFocusLoss;

	/** Whether to allow the user to back out of changes when they press the escape key */
	UE_DEPRECATED(5.1, "Direct access to RevertTextOnEscape is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = Behavior, AdvancedDisplay)
	bool RevertTextOnEscape;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	UE_DEPRECATED(5.1, "Direct access to ClearKeyboardFocusOnCommit is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = Behavior, AdvancedDisplay)
	bool ClearKeyboardFocusOnCommit;

	/** Whether the context menu can be opened */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool AllowContextMenu;
	
	/** Additional options for the virtual keyboard */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	FVirtualKeyboardOptions VirtualKeyboardOptions;

	/** What action should be taken when the virtual keyboard is dismissed? */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	EVirtualKeyboardDismissAction VirtualKeyboardDismissAction;

	/** Called whenever the text is changed programmatically or interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextChanged (Multi-Line Editable Text)"))
	FOnMultiLineEditableTextChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextCommitted (Multi-Line Editable Text)"))
	FOnMultiLineEditableTextCommittedEvent OnTextCommitted;

public:

	/**
	* Gets the widget text
	* @return The widget text
	*/
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Multi-Line Editable Text)"))
	FText GetText() const;

	/**
	* Directly sets the widget text.
	* @param InText The text to assign to the widget
	*/
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Multi-Line Editable Text)"))
	void SetText(FText InText);

	/** Returns the Hint text that appears when there is no text in the text box */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetHintText (Multi-Line Editable Text)"))
	FText GetHintText() const;

	/** 
	* Sets the Hint text that appears when there is no text in the text box 
	* @param InHintText The text that appears when there is no text in the text box 
	*/
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetHintText (Multi-Line Editable Text)"))
	void SetHintText(FText InHintText);

	/** Set to true to select all text when the user clicks to give focus on the widget */
	void SetSelectAllTextWhenFocused(bool bSelectAllTextWhenFocused);

	/** Whether to select all text when the user clicks to give focus on the widget */
	bool GetSelectAllTextWhenFocused() const;

	/** Set to true to clear text selection when focus is lost */
	void SetClearTextSelectionOnFocusLoss(bool bClearTextSelectionOnFocusLoss);

	/** Whether to clear text selection when focus is lost */
	bool GetClearTextSelectionOnFocusLoss() const;

	/** Set to true to allow the user to back out of changes when they press the escape key */
	void SetRevertTextOnEscape(bool bRevertTextOnEscape);

	/** Whether to allow the user to back out of changes when they press the escape key  */
	bool GetRevertTextOnEscape() const;

	/** Set to true to clear keyboard focus when pressing enter to commit changes */
	void SetClearKeyboardFocusOnCommit(bool bClearKeyboardFocusOnCommit);

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	bool GetClearKeyboardFocusOnCommit() const;	

	/** Return true when this text cannot be modified interactively by the user */
	bool GetIsReadOnly() const;

	/** Sets the Text as Readonly to prevent it from being modified interactively by the user */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetIsReadOnly (Multi-Line Editable Text"))
	void SetIsReadOnly(bool bReadOnly);

	UFUNCTION(BlueprintSetter)
	void SetWidgetStyle(const FTextBlockStyle& InWidgetStyle);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	const FSlateFontInfo& GetFont() const;

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetFont(FSlateFontInfo InFontInfo);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetFontMaterial(UMaterialInterface* InMaterial);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetFontOutlineMaterial(UMaterialInterface* InMaterial);

	//~ Begin UTextLayoutWidget Interface
	virtual void SetJustification(ETextJustify::Type InJustification) override;
	//~ End UTextLayoutWidget Interface

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
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnTextChanged(const FText& Text);
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

protected:
	TSharedPtr<SMultiLineEditableText> MyMultiLineEditableText;

	PROPERTY_BINDING_IMPLEMENTATION(FText, HintText);
};
