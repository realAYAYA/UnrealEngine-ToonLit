// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleFixturePatchListRow.h"

#include "Library/DMXEntityFixturePatch.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleFixturePatchListRowModel.h"
#include "Widgets/DMXReadOnlyFixturePatchListItem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleFixturePatchList.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleFixturePatchListRow"

namespace UE::DMX::Private
{ 
	void SDMXControlConsoleFixturePatchListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXReadOnlyFixturePatchListItem>& InItem, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
	{
		if (!InWeakEditorModel.IsValid())
		{
			return;
		}

		WeakEditorModel = InWeakEditorModel;
		OnFaderGroupMutedChanged = InArgs._OnFaderGroupMutedChanged;

		RowModel = MakeShared<FDMXControlConsoleFixturePatchListRowModel>(InItem->GetFixturePatch(), WeakEditorModel);
	
		SetEnabled(TAttribute<bool>::CreateSP(RowModel.Get(), &FDMXControlConsoleFixturePatchListRowModel::IsRowEnabled));

		SDMXReadOnlyFixturePatchListRow::Construct(
			SDMXReadOnlyFixturePatchListRow::FArguments(),
			InOwnerTable,
			InItem);
	}

	TSharedRef<SWidget> SDMXControlConsoleFixturePatchListRow::GenerateWidgetForColumn(const FName& ColumnName)
	{
		if (ColumnName == FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::FaderGroupEnabled)
		{
			// Add the fixture group enabled checkbox
			return GenerateCheckBoxRow();
		}

		return SDMXReadOnlyFixturePatchListRow::GenerateWidgetForColumn(ColumnName);
	}

	TSharedRef<SWidget> SDMXControlConsoleFixturePatchListRow::GenerateCheckBoxRow()
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
					.IsChecked_Lambda([this]()
						{
							return RowModel->GetFaderGroupEnabledState();
						})
					.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
					{
						const bool bEnabled = InCheckBoxState == ECheckBoxState::Checked;
						RowModel->SetFaderGroupEnabled(bEnabled);

						OnFaderGroupMutedChanged.ExecuteIfBound();
					})
				]
			];
	}
}

#undef LOCTEXT_NAMESPACE
