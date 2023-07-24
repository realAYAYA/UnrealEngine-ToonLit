// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimpleComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

void SSimpleComboButton::Construct(const FArguments& InArgs)
{

	TAttribute<FText> Text = InArgs._Text;


	TSharedRef<SHorizontalBox> ButtonContent = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(InArgs._Icon)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(3, 0, 0, 0))
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "SmallButtonText")
			.Text(InArgs._Text)
			.Visibility_Lambda([Text]() { return Text.Get(FText::GetEmpty()).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
		];

	SComboButton::Construct(SComboButton::FArguments()
		.HasDownArrow(InArgs._HasDownArrow)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.ForegroundColor(FSlateColor::UseStyle())
		.ButtonContent() [ButtonContent]
		.MenuContent()[InArgs._MenuContent.Widget]
		.OnGetMenuContent(InArgs._OnGetMenuContent)
		.OnMenuOpenChanged(InArgs._OnMenuOpenChanged)
		.OnComboBoxOpened(InArgs._OnComboBoxOpened)
	);
}

