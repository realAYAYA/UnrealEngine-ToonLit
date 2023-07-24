// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorExpandArrowButton.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorExpandArrowButton"

void SDMXControlConsoleEditorExpandArrowButton::Construct(const FArguments& InArgs)
{
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
			.OnClicked(this, &SDMXControlConsoleEditorExpandArrowButton::OnExpandArrowClicked)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Image(this, &SDMXControlConsoleEditorExpandArrowButton::GetExpandArrowImage)
			]
		];
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
	FName ResourceName;
	if (IsExpanded())
	{
		if (IsHovered())
		{
			constexpr TCHAR ExpandedHoveredName[] = TEXT("TreeArrow_Collapsed_Hovered");
			ResourceName = ExpandedHoveredName;
		}
		else
		{
			constexpr TCHAR ExpandedName[] = TEXT("TreeArrow_Collapsed");
			ResourceName = ExpandedName;
		}
	}
	else
	{
		if (IsHovered())
		{
			constexpr TCHAR CollapsedHoveredName[] = TEXT("TreeArrow_Expanded_Hovered");
			ResourceName = CollapsedHoveredName;
		}
		else
		{
			constexpr TCHAR CollapsedName[] = TEXT("TreeArrow_Expanded");
			ResourceName = CollapsedName;
		}
	}

	return FCoreStyle::Get().GetBrush(ResourceName);
}

#undef LOCTEXT_NAMESPACE
