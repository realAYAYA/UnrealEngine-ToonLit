// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorExpandArrowButton.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorExpandArrowButton"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorExpandArrowButton::Construct(const FArguments& InArgs)
	{
		bIsExpanded = InArgs._IsExpanded;
		OnExpandClicked = InArgs._OnExpandClicked;

		ChildSlot
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.ContentPadding(0.f)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(InArgs._ToolTipText)
				.OnClicked(this, &SDMXControlConsoleEditorExpandArrowButton::OnExpandArrowClicked)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(this, &SDMXControlConsoleEditorExpandArrowButton::GetExpandArrowImage)
				]
			];

		SetExpandArrow(bIsExpanded);
	}

	void SDMXControlConsoleEditorExpandArrowButton::SetExpandArrow(bool bExpand)
	{
		bIsExpanded = bExpand;
		OnExpandClicked.ExecuteIfBound(bIsExpanded);
	}

	void SDMXControlConsoleEditorExpandArrowButton::ToggleExpandArrow()
	{
		bIsExpanded = !bIsExpanded;
		OnExpandClicked.ExecuteIfBound(bIsExpanded);
	}

	FReply SDMXControlConsoleEditorExpandArrowButton::OnExpandArrowClicked()
	{
		ToggleExpandArrow();

		return FReply::Handled();
	}

	const FSlateBrush* SDMXControlConsoleEditorExpandArrowButton::GetExpandArrowImage() const
	{
		if (IsExpanded())
		{
			if (IsHovered())
			{
				return FAppStyle::GetBrush("TreeArrow_Collapsed_Hovered");
			}
			else
			{
				return FAppStyle::GetBrush("TreeArrow_Collapsed");
			}
		}
		else
		{
			if (IsHovered())
			{
				return FAppStyle::GetBrush("TreeArrow_Expanded_Hovered");
			}
			else
			{
				return FAppStyle::GetBrush("TreeArrow_Expanded");
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
