// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNegativeActionButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Styling/StyleColors.h"

void SNegativeActionButton::Construct(const FArguments& InArgs)
{
	TAttribute<FText> Text = InArgs._Text;
	TAttribute<EActionButtonStyle> ActionButtonStyle = InArgs._ActionButtonStyle;
	const FSlateBrush* Icon;

	if (InArgs._Icon.IsSet())
	{
		Icon = InArgs._Icon.Get();
	}
	else
	{
		Icon = FAppStyle::Get().GetBrush(ActionButtonStyle.Get() == EActionButtonStyle::Error ? "Icons.Error" : "Icons.Warning");
	}

	TSharedRef<SHorizontalBox> ButtonContent = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(Icon)
			.ColorAndOpacity(ActionButtonStyle.Get() == EActionButtonStyle::Error ? FStyleColors::Error : FStyleColors::Warning)
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

	if (InArgs._OnClicked.IsBound())
	{
		ChildSlot
		[
			SAssignNew(Button, SButton)
			.ForegroundColor(FSlateColor::UseStyle())
			.IsEnabled(InArgs._IsEnabled)
			.ToolTipText(InArgs._ToolTipText)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(InArgs._OnClicked)
			[
				ButtonContent
			]
		];
	}
	else
	{
		ChildSlot
		[
			SAssignNew(ComboButton, SComboButton)
			.HasDownArrow(false)
			.ContentPadding(FMargin(2.0f, 3.0f))
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ForegroundColor(FSlateColor::UseStyle())
			.IsEnabled(InArgs._IsEnabled)
			.ToolTipText(InArgs._ToolTipText)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonContent()
			[
				ButtonContent
			]
			.MenuContent()
			[
				InArgs._MenuContent.Widget
			]
			.OnGetMenuContent(InArgs._OnGetMenuContent)
			.OnMenuOpenChanged(InArgs._OnMenuOpenChanged)
			.OnComboBoxOpened(InArgs._OnComboBoxOpened)
		];
	}
}

void SNegativeActionButton::SetMenuContentWidgetToFocus(TWeakPtr<SWidget> Widget)
{
	check(ComboButton.IsValid());
	ComboButton->SetMenuContentWidgetToFocus(Widget);
}

void SNegativeActionButton::SetIsMenuOpen(bool bIsOpen, bool bIsFocused)
{
	check(ComboButton.IsValid());
	ComboButton->SetIsOpen(bIsOpen, bIsFocused);
}
