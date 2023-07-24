// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFixturePatchVerticalBox.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleEditorManager.h"
#include "DMXControlConsoleEditorSelection.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityReference.h"
#include "Library/DMXLibrary.h"
#include "Widgets/SDMXControlConsoleEditorFixturePatchRowWidget.h"

#include "ScopedTransaction.h"
#include "Layout/Visibility.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFixturePatchVerticalBox"

void SDMXControlConsoleEditorFixturePatchVerticalBox::Construct(const FArguments& InArgs)
{
	ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(5.f)
				.Visibility(TAttribute<EVisibility>(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::GetAddAllPatchesButtonVisibility))
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.OnClicked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked)
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(LOCTEXT("Add All Patches", "Add All Patches"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(FixturePatchRowsBoxWidget, SVerticalBox)
			]
		];
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::UpdateFixturePatchRows()
{
	UDMXControlConsoleData* EditorConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	UDMXLibrary* CurrentDMXLibrary = EditorConsoleData->GetDMXLibrary();
	if (CurrentDMXLibrary == DMXLibrary)
	{
		return;
	}

	DMXLibrary = CurrentDMXLibrary;

	FixturePatchRowsBoxWidget->ClearChildren();
	FixturePatchRowWidgets.Reset();
	FixturePatches.Reset();
	UpdateFixturePatches();

	for (const FDMXEntityFixturePatchRef FixturePatchRef : FixturePatches)
	{
		auto DetailsRowVisibilityLambda = [EditorConsoleData, FixturePatchRef]()
		{
			const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
			for (UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
			{
				if (!FaderGroup)
				{
					continue;
				}

				if (!FaderGroup->HasFixturePatch())
				{
					continue;
				}

				UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
				if (FixturePatch != FixturePatchRef.GetFixturePatch())
				{
					continue;
				}

				return EVisibility::Collapsed;
			}

			return EVisibility::Visible;
		};

		const TSharedRef<SDMXControlConsoleEditorFixturePatchRowWidget> ControlConsoleDetailsRow =
			SNew(SDMXControlConsoleEditorFixturePatchRowWidget, FixturePatchRef)
			.Visibility(TAttribute<EVisibility>::CreateLambda(DetailsRowVisibilityLambda))
			.OnSelectFixturePatchRow(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnSelectFixturePatchDetailsRow)
			.OnGenerateOnLastRow(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateFromFixturePatchOnLastRow)
			.OnGenerateOnNewRow(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateFromFixturePatchOnNewRow)
			.OnGenerateOnSelectedFaderGroup(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateSelectedFaderGroupFromFixturePatch);

		FixturePatchRowsBoxWidget->AddSlot()
			.AttachWidget(ControlConsoleDetailsRow);

		FixturePatchRowWidgets.Add(ControlConsoleDetailsRow);
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::UpdateFixturePatches()
{
	if (!DMXLibrary.IsValid())
	{
		return;
	}

	const TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatchesInLibrary)
	{
		if (!FixturePatch)
		{
			continue;
		}

		FDMXEntityFixturePatchRef FixturePatchRef;
		FixturePatchRef.SetEntity(FixturePatch);

		FixturePatches.Add(FixturePatchRef);
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::GenerateFaderGroupFromFixturePatch(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch)
{
	if (!FaderGroup || !FixturePatch)
	{
		return;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	SelectionHandler->ClearFadersSelection(FaderGroup);

	const FScopedTransaction FaderGroupTransaction(LOCTEXT("FaderGroupTransaction", "Generate Fader Group from Fixture Patch"));
	FaderGroup->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetSoftFixturePatchPtrPropertyName()));

	FaderGroup->GenerateFromFixturePatch(FixturePatch);

	FaderGroup->PostEditChange();
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnSelectFixturePatchDetailsRow(const TSharedRef<SDMXControlConsoleEditorFixturePatchRowWidget>& FixturePatchRow)
{
	if (SelectedFixturePatchRowWidget.IsValid())
	{
		SelectedFixturePatchRowWidget.Pin()->Unselect();
	}

	SelectedFixturePatchRowWidget = FixturePatchRow;
	FixturePatchRow->Select();
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateFromFixturePatchOnLastRow(const TSharedRef<SDMXControlConsoleEditorFixturePatchRowWidget>& FixturePatchRow)
{
	UDMXControlConsoleData* EditorConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = EditorConsoleData->GetFaderGroupRows();
	if (FaderGroupRows.IsEmpty())
	{
		return;
	}

	UDMXControlConsoleFaderGroupRow* FaderGroupRow = nullptr;
	int32 Index = -1;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		FaderGroupRow = FaderGroupRows.Last();
		if (!FaderGroupRow)
		{
			return;
		}

		Index = FaderGroupRow->GetFaderGroups().Num();
	}
	else
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup();
		if (!SelectedFaderGroup)
		{
			return;
		}

		FaderGroupRow = &SelectedFaderGroup->GetOwnerFaderGroupRowChecked();
		Index = SelectedFaderGroup->GetIndex() + 1;
	}
	
	UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupRow->AddFaderGroup(Index);
	UDMXEntityFixturePatch* FixturePatch = FixturePatchRow->GetFixturePatchRef().GetFixturePatch();

	GenerateFaderGroupFromFixturePatch(FaderGroup, FixturePatch);
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateFromFixturePatchOnNewRow(const TSharedRef<SDMXControlConsoleEditorFixturePatchRowWidget>& FixturePatchRow)
{
	UDMXControlConsoleData* EditorConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	int32 NewRowIndex = -1;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		NewRowIndex = EditorConsoleData->GetFaderGroupRows().Num();
	}
	else
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup();
		if (!SelectedFaderGroup)
		{
			return;
		}

		const int32 SelectedFaderGroupRowIndex = SelectedFaderGroup->GetOwnerFaderGroupRowChecked().GetRowIndex();
		NewRowIndex = SelectedFaderGroupRowIndex + 1;
	}

	UDMXControlConsoleFaderGroupRow* NewRow = EditorConsoleData->AddFaderGroupRow(NewRowIndex);
	if (NewRow->GetFaderGroups().IsEmpty())
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = NewRow->GetFaderGroups()[0];
	UDMXEntityFixturePatch* FixturePatch = FixturePatchRow->GetFixturePatchRef().GetFixturePatch();

	GenerateFaderGroupFromFixturePatch(FaderGroup, FixturePatch);
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateSelectedFaderGroupFromFixturePatch(const TSharedRef<SDMXControlConsoleEditorFixturePatchRowWidget>& FixturePatchRow)
{
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		return;
	}
	
	UDMXControlConsoleFaderGroup* FaderGroup = SelectionHandler->GetFirstSelectedFaderGroup();
	UDMXEntityFixturePatch* FixturePatch = FixturePatchRow->GetFixturePatchRef().GetFixturePatch();

	GenerateFaderGroupFromFixturePatch(FaderGroup, FixturePatch);
}

FReply SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked()
{
	UDMXControlConsoleData* EditorConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	if (EditorConsoleData)
	{
		const FScopedTransaction AddAllPatchesTransaction(LOCTEXT("AddAllPatchesTransaction", "Generate from Library"));
		EditorConsoleData->PreEditChange(nullptr);
		EditorConsoleData->GenerateFromDMXLibrary();
		EditorConsoleData->PostEditChange();
	}

	return FReply::Handled();
}

EVisibility SDMXControlConsoleEditorFixturePatchVerticalBox::GetAddAllPatchesButtonVisibility() const
{
	UDMXControlConsoleData* EditorConsoleData = FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return EVisibility::Collapsed;
	}

	return DMXLibrary.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
