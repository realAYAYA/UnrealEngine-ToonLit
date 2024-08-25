// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleAddEmptyFaderGroupMenu.h"

#include "Algo/AllOf.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleAddEmptyFaderGroupMenu"

void SDMXControlConsoleAddEmptyFaderGroupMenu::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
{
	EditorModel = InEditorModel;

	RegisterCommands();
	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("AddEmptyButtonMainSection", "Add Empty"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddEmptyGroupRight,
			NAME_None,
			LOCTEXT("AddEmptyRightButtonLabel", "To the right")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddEmptyGroupNextRow,
			NAME_None,
			LOCTEXT("AddEmptyNextRowButtonLabel", "To next row")
		);
	}
	MenuBuilder.EndSection();

	ChildSlot
		[
			MenuBuilder.MakeWidget()
		];
}

void SDMXControlConsoleAddEmptyFaderGroupMenu::RegisterCommands()
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddEmptyGroupRight,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddEmptyFaderGroupMenu::AddEmptyToTheRight),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddEmptyFaderGroupMenu::CanAddEmptyToTheRight)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddEmptyGroupNextRow,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddEmptyFaderGroupMenu::AddEmptyOnNewRow),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddEmptyFaderGroupMenu::CanAddEmptyOnNewRow)
	);
}

bool SDMXControlConsoleAddEmptyFaderGroupMenu::CanAddEmptyToTheRight() const
{
	if (!EditorModel.IsValid())
	{
		return false;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ControlConsoleData || !ActiveLayout)
	{
		return false;
	}

	// No layout editing with active global filter
	if (!ControlConsoleData->FilterString.IsEmpty())
	{
		return false;
	}

	// True if there's no global filter and no vertical sorting
	const EDMXControlConsoleLayoutMode LayoutMode = ActiveLayout->GetLayoutMode();
	switch (LayoutMode)
	{
	case EDMXControlConsoleLayoutMode::Horizontal:
		return true;
	case EDMXControlConsoleLayoutMode::Vertical:
		return false;
	case EDMXControlConsoleLayoutMode::Grid:
		return !ActiveLayout->GetAllFaderGroupControllers().IsEmpty();
	default:
		return false;
		break;
	}
}

void SDMXControlConsoleAddEmptyFaderGroupMenu::AddEmptyToTheRight()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ActiveLayout)
	{
		return;
	}

	const FScopedTransaction AddEmptyToTheRightTransaction(LOCTEXT("AddEmptyToTheRightTransaction", "Add Fader Group"));

	// Create a new row if there's none in the active layout
	ActiveLayout->PreEditChange(nullptr);
	if (ActiveLayout->GetLayoutRows().IsEmpty())
	{
		ActiveLayout->AddNewRowToLayout();
	}

	int32 RowIndex = ActiveLayout->GetLayoutRows().Num() - 1;
	int32 ColumnIndex = INDEX_NONE;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllersObjects = SelectionHandler->GetSelectedFaderGroupControllers();
	if (!SelectedFaderGroupControllersObjects.IsEmpty())
	{
		UDMXControlConsoleFaderGroupController* SelectedFaderGroupController = SelectionHandler->GetFirstSelectedFaderGroupController(true);
		RowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(SelectedFaderGroupController);
		ColumnIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(SelectedFaderGroupController);
	}

	if (ColumnIndex != INDEX_NONE)
	{
		ColumnIndex++;
	}

	UDMXControlConsoleFaderGroup* NewFaderGroup = AddEmptyFaderGroup();
	const FString NewFaderGroupName = NewFaderGroup ? NewFaderGroup->GetFaderGroupName() : TEXT("");

	UDMXControlConsoleFaderGroupController* NewController = ActiveLayout->AddToLayout(NewFaderGroup, NewFaderGroupName, RowIndex, ColumnIndex);
	if (NewController)
	{
		NewController->Modify();
		NewController->SetIsActive(true);
		ActiveLayout->AddToActiveFaderGroupControllers(NewController);
	}
	ActiveLayout->PostEditChange();
}

bool SDMXControlConsoleAddEmptyFaderGroupMenu::CanAddEmptyOnNewRow() const
{
	if (!EditorModel.IsValid())
	{
		return false;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ControlConsoleData || !ActiveLayout)
	{
		return false;
	}

	// True if there's no global filter and no horizontal sorting
	const EDMXControlConsoleLayoutMode LayoutMode = ActiveLayout->GetLayoutMode();
	return ControlConsoleData->FilterString.IsEmpty() && LayoutMode != EDMXControlConsoleLayoutMode::Horizontal;
}

void SDMXControlConsoleAddEmptyFaderGroupMenu::AddEmptyOnNewRow()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ActiveLayout)
	{
		return;
	}

	// Generate on last row if vertical sorting
	if (ActiveLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Vertical)
	{
		AddEmptyToTheRight();
		return;
	}

	int32 NewRowIndex = INDEX_NONE;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllersObjects = SelectionHandler->GetSelectedFaderGroupControllers();
	if (SelectedFaderGroupControllersObjects.IsEmpty())
	{
		NewRowIndex = ActiveLayout->GetLayoutRows().Num();
	}
	else
	{
		UDMXControlConsoleFaderGroupController* SelectedFaderGroupController = SelectionHandler->GetFirstSelectedFaderGroupController(true);
		NewRowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(SelectedFaderGroupController) + 1;
	}

	const FScopedTransaction AddEmptyToNewtRowTransaction(LOCTEXT("AddEmptyToNewRowTransaction", "Add Fader Group"));

	ActiveLayout->PreEditChange(nullptr);
	UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = ActiveLayout->AddNewRowToLayout(NewRowIndex);
	if (NewLayoutRow)
	{
		UDMXControlConsoleFaderGroup* NewFaderGroup = AddEmptyFaderGroup();
		const FString NewFaderGroupName = NewFaderGroup ? NewFaderGroup->GetFaderGroupName() : TEXT("");

		NewLayoutRow->PreEditChange(nullptr);
		UDMXControlConsoleFaderGroupController* NewController = NewLayoutRow->CreateFaderGroupController(NewFaderGroup, NewFaderGroupName);
		if (NewController)
		{
			NewController->Modify();
			NewController->SetIsActive(true);
			ActiveLayout->AddToActiveFaderGroupControllers(NewController);
		}
		NewLayoutRow->PostEditChange();
	}
	ActiveLayout->PostEditChange();
}

UDMXControlConsoleFaderGroup* SDMXControlConsoleAddEmptyFaderGroupMenu::AddEmptyFaderGroup() const
{
	UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	if (!ControlConsoleData)
	{
		return nullptr;
	}

	const TArray<UDMXControlConsoleFaderGroupRow*>& FaderGroupRows = ControlConsoleData->GetFaderGroupRows();
	UDMXControlConsoleFaderGroupRow* FaderGroupRow = !FaderGroupRows.IsEmpty() ? FaderGroupRows.Last() : nullptr;
	bool bHasOnlyUnpatchedFaderGroups = false;

	// Find a row with only unpatched fader
	if (FaderGroupRow)
	{
		const TArray<UDMXControlConsoleFaderGroup*>& FaderGroups = FaderGroupRow->GetFaderGroups();
		bHasOnlyUnpatchedFaderGroups = Algo::AllOf(FaderGroups,
			[](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && !FaderGroup->HasFixturePatch();
			});
	}

	// If there's no unpatched fader groups row, create one
	UDMXControlConsoleFaderGroup* NewFaderGroup = nullptr;
	if (!bHasOnlyUnpatchedFaderGroups)
	{
		ControlConsoleData->Modify();
		FaderGroupRow = ControlConsoleData->AddFaderGroupRow(ControlConsoleData->GetFaderGroupRows().Num());
		NewFaderGroup = FaderGroupRow && !FaderGroupRow->GetFaderGroups().IsEmpty() ? FaderGroupRow->GetFaderGroups()[0] : nullptr;
	}

	if (FaderGroupRow && !NewFaderGroup)
	{
		FaderGroupRow->Modify();
		NewFaderGroup = FaderGroupRow->AddFaderGroup(FaderGroupRow->GetFaderGroups().Num());
	}

	return NewFaderGroup;
}

#undef LOCTEXT_NAMESPACE
