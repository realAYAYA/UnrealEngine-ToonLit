// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaDepthAlignment.h"
#include "AvaDefs.h"
#include "AvaEditorStyle.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SSegmentedControl.h"

#define LOCTEXT_NAMESPACE "SAvaDepthAlignment"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAvaDepthAlignment::Construct(const FArguments& InArgs)
{
	Alignment = InArgs._Alignment;
	OnAlignmentChanged = InArgs._OnAlignmentChanged;

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(SSegmentedControl<EAvaDepthAlignment>)
		.UniformPadding(InArgs._UniformPadding)
		.Value(this, &SAvaDepthAlignment::GetCurrentAlignment)
		.OnValueChanged(this, &SAvaDepthAlignment::OnCurrentAlignmentChanged)
		+ SSegmentedControl<EAvaDepthAlignment>::Slot(EAvaDepthAlignment::Front)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Translation.Front"))
			.ToolTip(LOCTEXT("DAlignFront", "Front Align Depth"))
		+ SSegmentedControl<EAvaDepthAlignment>::Slot(EAvaDepthAlignment::Center)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Translation.Center_X"))
			.ToolTip(LOCTEXT("DAlignCenter", "Center Align Depth"))
		+ SSegmentedControl<EAvaDepthAlignment>::Slot(EAvaDepthAlignment::Back)
			.Icon(FAvaEditorStyle::Get().GetBrush("Icons.Alignment.Translation.Back"))
			.ToolTip(LOCTEXT("DAlignBack", "Back Align Depth"))
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EAvaDepthAlignment SAvaDepthAlignment::GetCurrentAlignment() const
{
	return Alignment.Get(EAvaDepthAlignment::Center);
}

void SAvaDepthAlignment::OnCurrentAlignmentChanged(const EAvaDepthAlignment NewAlignment)
{
	OnAlignmentChanged.ExecuteIfBound(NewAlignment);
}

#undef LOCTEXT_NAMESPACE
