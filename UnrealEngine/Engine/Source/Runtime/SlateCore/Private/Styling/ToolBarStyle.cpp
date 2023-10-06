// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styling/ToolBarStyle.h"
#include "Brushes/SlateNoResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ToolBarStyle)

const FName FToolBarStyle::TypeName(TEXT("FToolbarStyle"));

FToolBarStyle::FToolBarStyle()
	: BackgroundBrush(FSlateNoResource())
	, ExpandBrush(FSlateNoResource())
	, SeparatorBrush(FSlateNoResource())
	, LabelStyle()
	, EditableTextStyle()
	, ToggleButton()
	, SettingsComboButton()
	, ButtonStyle()
	, LabelPadding(0)
	, UniformBlockWidth(0.f)
	, UniformBlockHeight(0.f)
	, NumColumns(0)
	, IconPadding(0)
	, SeparatorPadding(0)
	, ComboButtonPadding(0)
	, ButtonPadding(0)
	, CheckBoxPadding(0)
	, BlockPadding(0)
	, IndentedBlockPadding(0)
	, BackgroundPadding(0)
	, IconSize(16,16)
	, bShowLabels(true)
{}

const FToolBarStyle& FToolBarStyle::GetDefault()
{
	static FToolBarStyle Default;
	return Default;
}

void FToolBarStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	OutBrushes.Add(&BackgroundBrush);
	OutBrushes.Add(&ExpandBrush);
	OutBrushes.Add(&SeparatorBrush);

	LabelStyle.GetResources(OutBrushes);
	EditableTextStyle.GetResources(OutBrushes);
	ToggleButton.GetResources(OutBrushes);
	SettingsComboButton.GetResources(OutBrushes);
	SettingsToggleButton.GetResources(OutBrushes);
	SettingsButtonStyle.GetResources(OutBrushes);
	ButtonStyle.GetResources(OutBrushes);
}


