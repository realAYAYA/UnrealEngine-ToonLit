// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaVerticalAlignment.h"
#include "AvaDefs.h"
#include "AvaEditorStyle.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SSegmentedControl.h"

#define LOCTEXT_NAMESPACE "SAvaVerticalAlignment"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAvaVerticalAlignment::Construct(const FArguments& InArgs)
{
	Alignment = InArgs._Alignment;
	OnAlignmentChanged = InArgs._OnAlignmentChanged;

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(SSegmentedControl<EAvaVerticalAlignment>)
		.UniformPadding(InArgs._UniformPadding)
		.Value(this, &SAvaVerticalAlignment::GetCurrentAlignment)
		.OnValueChanged(this, &SAvaVerticalAlignment::OnCurrentAlignmentChanged)
		+ SSegmentedControl<EAvaVerticalAlignment>::Slot(EAvaVerticalAlignment::Top)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Top"))
			.ToolTip(LOCTEXT("VAlignTop", "Top Align Vertically"))
		+ SSegmentedControl<EAvaVerticalAlignment>::Slot(EAvaVerticalAlignment::Center)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Center_Z"))
			.ToolTip(LOCTEXT("VAlignCenter", "Center Align Vertically"))
		+ SSegmentedControl<EAvaVerticalAlignment>::Slot(EAvaVerticalAlignment::Bottom)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Bottom"))
			.ToolTip(LOCTEXT("VAlignBottom", "Bottom Align Vertically"))
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EAvaVerticalAlignment SAvaVerticalAlignment::GetCurrentAlignment() const
{
	return Alignment.Get(EAvaVerticalAlignment::Center);
}

void SAvaVerticalAlignment::OnCurrentAlignmentChanged(const EAvaVerticalAlignment NewAlignment)
{
	OnAlignmentChanged.ExecuteIfBound(NewAlignment);
}

#undef LOCTEXT_NAMESPACE
