// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaHorizontalAlignment.h"
#include "AvaDefs.h"
#include "AvaEditorStyle.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SSegmentedControl.h"

#define LOCTEXT_NAMESPACE "SAvaHorizontalAlignment"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAvaHorizontalAlignment::Construct(const FArguments& InArgs)
{
	Alignment = InArgs._Alignment;
	OnAlignmentChanged = InArgs._OnAlignmentChanged;

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(SSegmentedControl<EAvaHorizontalAlignment>)
		.UniformPadding(InArgs._UniformPadding)
		.Value(this, &SAvaHorizontalAlignment::GetCurrentAlignment)
		.OnValueChanged(this, &SAvaHorizontalAlignment::OnCurrentAlignmentChanged)
		+ SSegmentedControl<EAvaHorizontalAlignment>::Slot(EAvaHorizontalAlignment::Left)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Left"))
			.ToolTip(LOCTEXT("HAlignLeft", "Left Align Horizontally"))
		+ SSegmentedControl<EAvaHorizontalAlignment>::Slot(EAvaHorizontalAlignment::Center)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Center_Y"))
			.ToolTip(LOCTEXT("HAlignCenter", "Center Align Horizontally"))
		+ SSegmentedControl<EAvaHorizontalAlignment>::Slot(EAvaHorizontalAlignment::Right)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Right"))
			.ToolTip(LOCTEXT("HAlignRight", "Right Align Horizontally"))
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EAvaHorizontalAlignment SAvaHorizontalAlignment::GetCurrentAlignment() const
{
	return Alignment.Get(EAvaHorizontalAlignment::Center);
}

void SAvaHorizontalAlignment::OnCurrentAlignmentChanged(const EAvaHorizontalAlignment NewAlignment)
{
	OnAlignmentChanged.ExecuteIfBound(NewAlignment);
}

#undef LOCTEXT_NAMESPACE
