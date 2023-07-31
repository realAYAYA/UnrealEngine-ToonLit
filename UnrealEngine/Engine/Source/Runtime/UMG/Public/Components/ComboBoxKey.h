// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/Widget.h"
#include "Widgets/Input/SComboBox.h"

#include "ComboBoxKey.generated.h"


/**
 * The combobox allows you to display a list of options to the user in a dropdown menu for them to select one.
 * Use OnGenerateConentWidgetEvent to return a custom built widget.
 */
UCLASS(meta = (DisplayName = "ComboBox (Key)"))
class UMG_API UComboBoxKey : public UWidget
{
	GENERATED_BODY()

public:
	UComboBoxKey();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSelectionChangedEvent, FName, SelectedItem, ESelectInfo::Type, SelectionType);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOpeningEvent);
	DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(UWidget*, FGenerateWidgetEvent, FName, Item);

private:
	/** . */
	UPROPERTY(EditAnywhere, Category = Content)
	TArray<FName> Options;

	/** */
	UPROPERTY(EditAnywhere, Category = Content)
	FName SelectedOption;

public:
	/** The combobox style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (DisplayName = "Style"))
	FComboBoxStyle WidgetStyle;

	/** The item row style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style)
	FTableRowStyle ItemStyle;

	/** The foreground color to pass through the hierarchy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, meta = (DesignerRebuild))
	FSlateColor ForegroundColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style)
	FMargin ContentPadding;

	/** The max height of the combobox list that opens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, AdvancedDisplay)
	float MaxListHeight;

	/**
	 * When false, the down arrow is not generated and it is up to the API consumer
	 * to make their own visual hint that this is a drop down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, AdvancedDisplay)
	bool bHasDownArrow;

	/**
	 * When false, directional keys will change the selection. When true, ComboBox
	 * must be activated and will only capture arrow input while activated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style, AdvancedDisplay)
	bool bEnableGamepadNavigationMode;

	/** When true, allows the combo box to receive keyboard focus */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Style)
	bool bIsFocusable;

public: // Events

	/** Called when the widget is needed for the content. */
	UPROPERTY(EditAnywhere, Category = Events, meta = (IsBindableEvent = "True"))
	FGenerateWidgetEvent OnGenerateContentWidget;
	
	/** Called when the widget is needed for the item. */
	UPROPERTY(EditAnywhere, Category = Events, meta = (IsBindableEvent = "True"))
	FGenerateWidgetEvent OnGenerateItemWidget;

	/** Called when a new item is selected in the combobox. */
	UPROPERTY(BlueprintAssignable, Category = Events)
	FOnSelectionChangedEvent OnSelectionChanged;

	/** Called when the combobox is opening */
	UPROPERTY(BlueprintAssignable, Category = Events)
	FOnOpeningEvent OnOpening;

public:

	

	/** Add an element to the option list. */
	UFUNCTION(BlueprintCallable, Category = "ComboBox")
	void AddOption(FName Option);

	/** Remove an element to the option list. */
	UFUNCTION(BlueprintCallable, Category = "ComboBox")
	bool RemoveOption(FName Option);

	/** Remove all the elements of the option list. */
	UFUNCTION(BlueprintCallable, Category = "ComboBox")
	void ClearOptions();

	/** Clear the current selection. */
	UFUNCTION(BlueprintCallable, Category = "ComboBox")
	void ClearSelection();

	/** Set the current selected option. */
	UFUNCTION(BlueprintCallable, Category = "ComboBox")
	void SetSelectedOption(FName Option);

	/** Get the current selected option */
	UFUNCTION(BlueprintCallable, Category = "ComboBox")
	FName GetSelectedOption() const;

	/** Is the combobox menu openned. */
	UFUNCTION(BlueprintCallable, Category = "ComboBox", Meta = (ReturnDisplayName = "bOpen"))
	bool IsOpen() const;

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	/** Called by slate when it needs to generate the widget in the content box */
	void GenerateContent();

	/** Called by slate when it needs to generate a new item for the combobox */
	TSharedRef<SWidget> HandleGenerateItemWidget(FName Item);

	/** Called by slate when the underlying combobox selection changes */
	void HandleSelectionChanged(FName Item, ESelectInfo::Type SelectionType);

	/** Called by slate when the underlying combobox is opening */
	void HandleOpening();

	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

private:
	/** A shared pointer to the underlying slate combobox */
	TSharedPtr<SComboBox<FName>> MyComboBox;

	/** A shared pointer to a container that holds the combobox content that is selected */
	TSharedPtr< SBox > ComboBoxContent;
};
