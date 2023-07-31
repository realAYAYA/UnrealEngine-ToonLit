// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Input/Reply.h"
#include "Misc/Attribute.h"
#include "NiagaraTypes.h"
#include "SGraphPalette.h"
#include "SlateFwd.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class NIAGARAEDITOR_API SNiagaraPinTypeSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraPinTypeSelector)
	{}
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, UEdGraphPin * InGraphPin);

protected:
	virtual TSharedRef<SWidget>	GetMenuContent();
	FText GetTooltipText() const;

private:
	UEdGraphPin* Pin = nullptr;
	TSharedPtr<SComboButton> SelectorButton;
};

class NIAGARAEDITOR_API SNiagaraIconWidget : public SGraphPaletteItem
{
public:

	SLATE_BEGIN_ARGS(SNiagaraIconWidget)
	{}
		SLATE_ARGUMENT(FText, IconToolTip)
		SLATE_ARGUMENT(const FSlateBrush*, IconBrush)
		SLATE_ARGUMENT(FSlateColor, IconColor)
		SLATE_ARGUMENT(FString, DocLink)
		SLATE_ARGUMENT(FString, DocExcerpt)
		SLATE_ARGUMENT(const FSlateBrush*, SecondaryIconBrush)
		SLATE_ARGUMENT(FSlateColor, SecondaryIconColor)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			CreateIconWidget(
				InArgs._IconToolTip
				, InArgs._IconBrush
				, InArgs._IconColor
				, InArgs._DocLink
				, InArgs._DocExcerpt
				, InArgs._SecondaryIconBrush
				, InArgs._SecondaryIconColor
			)
		];
	}
};