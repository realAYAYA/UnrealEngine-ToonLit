// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"

#include "ToolBarStyle.generated.h"

/**
 * Represents the appearance of a toolbar 
 */
USTRUCT(BlueprintType)
struct FToolBarStyle : public FSlateWidgetStyle
{
	GENERATED_BODY()

	SLATECORE_API FToolBarStyle();

	virtual ~FToolBarStyle() {}

	SLATECORE_API virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;

	static SLATECORE_API const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static SLATECORE_API const FToolBarStyle& GetDefault();

	/** The brush used for the background of the toolbar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BackgroundBrush;
	FToolBarStyle& SetBackground(const FSlateBrush& InBackground) { BackgroundBrush = InBackground; return *this; }

	/** The brush used for the expand arrow when the toolbar runs out of room and needs to display toolbar items in a menu*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush ExpandBrush;
	FToolBarStyle& SetExpandBrush(const FSlateBrush& InExpandBrush) { ExpandBrush = InExpandBrush; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush SeparatorBrush;
	FToolBarStyle& SetSeparatorBrush(const FSlateBrush& InSeparatorBrush) { SeparatorBrush = InSeparatorBrush; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FTextBlockStyle LabelStyle;
	FToolBarStyle& SetLabelStyle(const FTextBlockStyle& InLabelStyle) { LabelStyle = InLabelStyle; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FEditableTextBoxStyle EditableTextStyle;
	FToolBarStyle& SetEditableTextStyle(const FEditableTextBoxStyle& InEditableTextStyle) { EditableTextStyle = InEditableTextStyle; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FCheckBoxStyle ToggleButton;
	FToolBarStyle& SetToggleButtonStyle(const FCheckBoxStyle& InToggleButton) { ToggleButton = InToggleButton; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FComboButtonStyle ComboButtonStyle;
	FToolBarStyle& SetComboButtonStyle(const FComboButtonStyle& InComboButtonStyle) { ComboButtonStyle = InComboButtonStyle; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FButtonStyle SettingsButtonStyle;
	FToolBarStyle& SetSettingsButtonStyle(const FButtonStyle& InSettingsButton) { SettingsButtonStyle = InSettingsButton; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FComboButtonStyle SettingsComboButton;
	FToolBarStyle& SetSettingsComboButtonStyle(const FComboButtonStyle& InSettingsComboButton) { SettingsComboButton = InSettingsComboButton; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FCheckBoxStyle SettingsToggleButton;
	FToolBarStyle& SetSettingsToggleButtonStyle(const FCheckBoxStyle& InSettingsToggleButton) { SettingsToggleButton = InSettingsToggleButton; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FButtonStyle ButtonStyle;
	FToolBarStyle& SetButtonStyle(const FButtonStyle& InButtonStyle) { ButtonStyle = InButtonStyle; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin LabelPadding;
	FToolBarStyle& SetLabelPadding(const FMargin& InLabelPadding) { LabelPadding = InLabelPadding; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float UniformBlockWidth;
	FToolBarStyle& SetUniformBlockWidth(const float InUniformBlockWidth) { UniformBlockWidth = InUniformBlockWidth; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float UniformBlockHeight;
	FToolBarStyle& SetUniformBlockHeight(const float InUniformBlockHeight) { UniformBlockHeight = InUniformBlockHeight; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	int32 NumColumns;
	FToolBarStyle& SetNumColumns(const int32 InNumColumns) { NumColumns = InNumColumns; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin IconPadding;
	FToolBarStyle& SetIconPadding(const FMargin& InIconPadding) { IconPadding = InIconPadding; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin SeparatorPadding;
	FToolBarStyle& SetSeparatorPadding(const FMargin& InSeparatorPadding) { SeparatorPadding = InSeparatorPadding; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin ComboButtonPadding;
	FToolBarStyle& SetComboButtonPadding(const FMargin& InComboButtonPadding) { ComboButtonPadding = InComboButtonPadding; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin ButtonPadding;
	FToolBarStyle& SetButtonPadding(const FMargin& InButtonPadding) { ButtonPadding = InButtonPadding; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin CheckBoxPadding;
	FToolBarStyle& SetCheckBoxPadding(const FMargin& InCheckBoxPadding) { CheckBoxPadding = InCheckBoxPadding; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin BlockPadding;
	FToolBarStyle& SetBlockPadding(const FMargin& InBlockPadding) { BlockPadding = InBlockPadding; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin IndentedBlockPadding;
	FToolBarStyle& SetIndentedBlockPadding(const FMargin& InIndentedBlockPadding) { IndentedBlockPadding = InIndentedBlockPadding; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin BackgroundPadding;
	FToolBarStyle& SetBackgroundPadding(const FMargin& InMargin) { BackgroundPadding = InMargin; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FDeprecateSlateVector2D IconSize;
	FToolBarStyle& SetIconSize(const UE::Slate::FDeprecateVector2DParameter& InIconSize) { IconSize = InIconSize; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	bool bShowLabels;
	FToolBarStyle& SetShowLabels(bool bInShowLabels) { bShowLabels = bInShowLabels; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	float ButtonContentMaxWidth = 64.0f;
	FToolBarStyle& SetButtonContentMaxWidth(float InButtonContentMaxWidth) { ButtonContentMaxWidth = InButtonContentMaxWidth; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	float ButtonContentFillWidth = 1.0f;
	FToolBarStyle& SetButtonContentFillWidth(float InButtonContentFillWidth) { ButtonContentFillWidth = InButtonContentFillWidth; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin IconPaddingWithVisibleLabel;
	FToolBarStyle& SetIconPaddingWithVisibleLabel(const FMargin& InIconPaddingWithVisibleLabel) { IconPaddingWithVisibleLabel = InIconPaddingWithVisibleLabel; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin IconPaddingWithCollapsedLabel;
	FToolBarStyle& SetIconPaddingWithCollapsedLabel(const FMargin& InIconPaddingWithCollapsedLabel) { IconPaddingWithCollapsedLabel = InIconPaddingWithCollapsedLabel; return *this; }
	
};
