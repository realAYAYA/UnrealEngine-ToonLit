// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleReadOnlyFixturePatchListRow.h"

#include "Models/DMXControlConsoleEditorModel.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleReadOnlyFixturePatchListRow"

void SDMXControlConsoleReadOnlyFixturePatchListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXEntityFixturePatchRef>& InFixturePatchRef)
{
	IsCheckedDelegate = InArgs._IsChecked;
	OnCheckStateChanged = InArgs._OnCheckStateChanged;

	SDMXReadOnlyFixturePatchListRow::Construct(
		SDMXReadOnlyFixturePatchListRow::FArguments(),
		InOwnerTable,
		InFixturePatchRef);
}

TSharedRef<SWidget> SDMXControlConsoleReadOnlyFixturePatchListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::CheckBox)
	{
		return GenerateCheckBoxRow();
	}

	return SDMXReadOnlyFixturePatchListRow::GenerateWidgetForColumn(ColumnName);
}

TSharedRef<SWidget> SDMXControlConsoleReadOnlyFixturePatchListRow::GenerateCheckBoxRow()
{
	return
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			SNew(SBox)
			.WidthOverride(20.f)
			.HeightOverride(20.f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SDMXControlConsoleReadOnlyFixturePatchListRow::IsChecked)
				.OnCheckStateChanged(OnCheckStateChanged)
			]
		];
}

ECheckBoxState SDMXControlConsoleReadOnlyFixturePatchListRow::IsChecked() const
{
	if (IsCheckedDelegate.IsBound())
	{
		return IsCheckedDelegate.Execute();
	}

	return ECheckBoxState::Undetermined;
}

#undef LOCTEXT_NAMESPACE
