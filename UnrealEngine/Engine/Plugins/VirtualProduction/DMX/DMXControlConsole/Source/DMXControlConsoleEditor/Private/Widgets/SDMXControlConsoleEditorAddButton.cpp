// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorAddButton.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Widgets/Images/SImage.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorAddButton"

namespace UE::Private::DMXControlConsoleEditorAddButton
{
	static FSlateColor HoveringColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f));
};

void SDMXControlConsoleEditorAddButton::Construct(const FArguments& InArgs)
{
	OnClicked = InArgs._OnClicked;

	ChildSlot
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.ClickMethod(EButtonClickMethod::MouseDown)
			.ContentPadding(0.f)
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			.OnClicked(OnClicked)
			.Visibility(InArgs._Visibility)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
				.ColorAndOpacity(this, &SDMXControlConsoleEditorAddButton::GetButtonColor)
			]
		];
}

FSlateColor SDMXControlConsoleEditorAddButton::GetButtonColor() const
{
	if (IsHovered())
	{
		return UE::Private::DMXControlConsoleEditorAddButton::HoveringColor;
	}
	else
	{
		return FSlateColor::UseSubduedForeground();
	}
}

#undef LOCTEXT_NAMESPACE
