// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleAddFixturePatchMenu.h"

#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleAddFixturePatchMenu"

void SDMXControlConsoleAddFixturePatchMenu::Construct(const FArguments& InArgs, TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> InFixturePatches, UDMXControlConsoleEditorModel* InEditorModel)
{
	EditorModel = InEditorModel;
	FixturePatches = InFixturePatches;

	RegisterCommands();
	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("AddPatchButtonMainSection", "Add Patch"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddPatchRight,
			NAME_None,
			LOCTEXT("AddPatchRightButtonLabel", "To the right")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddPatchNextRow,
			NAME_None,
			LOCTEXT("AddPatchNextRowButtonLabel", "To next row")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddPatchToSelection,
			NAME_None,
			LOCTEXT("AddPatchToSelectionButtonLabel", "To selection")
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("GroupPatchButtonMainSection", "Group Patch"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().GroupPatchRight,
			NAME_None,
			LOCTEXT("GroupPatchRightButtonLabel", "Group to the right")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().GroupPatchNextRow,
			NAME_None,
			LOCTEXT("GroupPatchNextRowButtonLabel", "Group to next row")
		);
	}
	MenuBuilder.EndSection();

	ChildSlot
		[
			MenuBuilder.MakeWidget()
		];
}

void SDMXControlConsoleAddFixturePatchMenu::SetFixturePatches(TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> InFixturePatches)
{
	FixturePatches = InFixturePatches;
}

void SDMXControlConsoleAddFixturePatchMenu::RegisterCommands()
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchRight,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::AddPatchesToTheRight),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesToTheRight)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchNextRow,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::AddPatchesOnNewRow),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesOnNewRow)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchToSelection,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::SetPatchOnFaderGroup),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::CanSetPatchOnFaderGroup)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().GroupPatchRight,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::GroupPatchesToTheRight),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::CanGroupPatchesToTheRight)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().GroupPatchNextRow,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::GroupPatchesOnNewRow),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::CanGroupPatchesOnNewRow)
	);
}

bool SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesToTheRight() const
{
	if (!EditorModel.IsValid() || FixturePatches.IsEmpty())
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

void SDMXControlConsoleAddFixturePatchMenu::AddPatchesToTheRight()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ControlConsoleData || !ActiveLayout)
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroup*> SelectedFaderGroups = GetFaderGroupsFromFixturePatches();
	if (SelectedFaderGroups.IsEmpty())
	{
		return;
	}

	const FScopedTransaction AddToLastRowTransaction(LOCTEXT("AddToLastRowTransaction", "Add Fader Group"));

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

	// Add all fader groups from selected patches in the fixture patch list
	for (UDMXControlConsoleFaderGroup* FaderGroup : SelectedFaderGroups)
	{
		if (!FaderGroup)
		{
			continue;
		}

		if (ColumnIndex != INDEX_NONE)
		{
			ColumnIndex++;
		}

		UDMXControlConsoleFaderGroupController* NewController = ActiveLayout->AddToLayout(FaderGroup, FaderGroup->GetFaderGroupName(), RowIndex, ColumnIndex);
		if (NewController)
		{
			NewController->Modify();
			NewController->SetIsActive(true);
			ActiveLayout->AddToActiveFaderGroupControllers(NewController);
		}
	}
	ActiveLayout->PostEditChange();
}

bool SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesOnNewRow() const
{
	if (!EditorModel.IsValid() || FixturePatches.IsEmpty())
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

void SDMXControlConsoleAddFixturePatchMenu::AddPatchesOnNewRow()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ControlConsoleData || !ActiveLayout)
	{
		return;
	}

	// Generate on last row if vertical sorting
	if (ActiveLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Vertical)
	{
		AddPatchesToTheRight();
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
		if (!SelectedFaderGroupController)
		{
			return;
		}

		NewRowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(SelectedFaderGroupController) + 1;
	}

	const TArray<UDMXControlConsoleFaderGroup*> SelectedFaderGroups = GetFaderGroupsFromFixturePatches();
	if (SelectedFaderGroups.IsEmpty())
	{
		return;
	}

	// Add all fader groups from selected patches in the fixture patch list
	const FScopedTransaction AddToNewtRowTransaction(LOCTEXT("AddToNewRowTransaction", "Add Fader Group"));
	ActiveLayout->PreEditChange(nullptr);
	UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = ActiveLayout->AddNewRowToLayout(NewRowIndex);
	if (NewLayoutRow)
	{
		NewLayoutRow->PreEditChange(nullptr);
		for (UDMXControlConsoleFaderGroup* FaderGroup : SelectedFaderGroups)
		{
			UDMXControlConsoleFaderGroupController* NewController = NewLayoutRow->CreateFaderGroupController(FaderGroup, FaderGroup->GetFaderGroupName());
			if (NewController)
			{
				NewController->Modify();
				NewController->SetIsActive(true);
				ActiveLayout->AddToActiveFaderGroupControllers(NewController);
			}
		}
		NewLayoutRow->PostEditChange();
	}
	ActiveLayout->PostEditChange();
}

bool SDMXControlConsoleAddFixturePatchMenu::CanSetPatchOnFaderGroup() const
{
	if (!EditorModel.IsValid() || FixturePatches.IsEmpty())
	{
		return false;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	if (!ControlConsoleData)
	{
		return false;
	}
		
	// True if there's no global filter and at least one selected fader group
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	return ControlConsoleData->FilterString.IsEmpty() && !SelectionHandler->GetSelectedFaderGroupControllers().IsEmpty();
}

void SDMXControlConsoleAddFixturePatchMenu::SetPatchOnFaderGroup()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ControlConsoleData || !ActiveLayout)
	{
		return;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	const UDMXControlConsoleFaderGroupController* FirstSelectedFaderGroupController = SelectionHandler->GetFirstSelectedFaderGroupController();
	if (!FirstSelectedFaderGroupController)
	{
		return;
	}

	const int32 RowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(FirstSelectedFaderGroupController);

	const TArray<UDMXControlConsoleFaderGroup*> SelectedFaderGroups = GetFaderGroupsFromFixturePatches();
	if (SelectedFaderGroups.IsEmpty())
	{
		return;
	}

	// Add all fader groups from selected patches in the fixture patch list
	const FScopedTransaction ReplaceSelectedFaderGroupTransaction(LOCTEXT("ReplaceSelectedFaderGroupTransaction", "Replace Fader Group"));
	ActiveLayout->PreEditChange(nullptr);

	TArray<UObject*> FaderGroupControllersToSelect;
	int32 ColumnIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(FirstSelectedFaderGroupController);
	for (UDMXControlConsoleFaderGroup* FaderGroup : SelectedFaderGroups)
	{
		UDMXControlConsoleFaderGroupController*	NewController = ActiveLayout->AddToLayout(FaderGroup, FaderGroup->GetFaderGroupName(), RowIndex, ColumnIndex);
		if (NewController)
		{
			NewController->Modify();
			NewController->SetIsActive(true);
			NewController->SetIsExpanded(FirstSelectedFaderGroupController->IsExpanded());

			ActiveLayout->AddToActiveFaderGroupControllers(NewController);
		}

		FaderGroupControllersToSelect.Add(NewController);

		ColumnIndex++;
	}

	// Remove all selected fader group controllers from the layout
	TArray<UObject*> FaderGroupControllersToUnselect;
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllersObjects = SelectionHandler->GetSelectedFaderGroupControllers();
	for (const TWeakObjectPtr<UObject> SelectedFaderGroupControllerObject : SelectedFaderGroupControllersObjects)
	{
		UDMXControlConsoleFaderGroupController* SelectedFaderGroupController =  Cast<UDMXControlConsoleFaderGroupController>(SelectedFaderGroupControllerObject.Get());
		if (!SelectedFaderGroupController)
		{
			continue;
		}

		FaderGroupControllersToUnselect.Add(SelectedFaderGroupController);
		if (SelectedFaderGroupController->HasFixturePatch())
		{
			continue;
		}

		// Destroy all unpatched fader groups in the controller
		SelectedFaderGroupController->Modify();
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = SelectedFaderGroupController->GetFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
		{
			if (FaderGroup.IsValid())
			{
				SelectedFaderGroupController->UnPossess(FaderGroup.Get());

				FaderGroup->Modify();
				FaderGroup->Destroy();
			}
		}

		ActiveLayout->RemoveFromActiveFaderGroupControllers(SelectedFaderGroupController);
		SelectedFaderGroupController->Destroy();
	}

	ActiveLayout->ClearEmptyLayoutRows();
	ActiveLayout->PostEditChange();

	constexpr bool bNotifySelectionChange = false;
	SelectionHandler->AddToSelection(FaderGroupControllersToSelect, bNotifySelectionChange);
	SelectionHandler->RemoveFromSelection(FaderGroupControllersToUnselect);

	EditorModel->RequestUpdateEditorModel();
}

void SDMXControlConsoleAddFixturePatchMenu::GroupPatchesToTheRight()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ControlConsoleData || !ActiveLayout)
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroup*> FaderGroupsToGroup = GetFaderGroupsFromFixturePatches();
	if (FaderGroupsToGroup.IsEmpty())
	{
		return;
	}

	const FScopedTransaction GroupToLastRowTransaction(LOCTEXT("GroupToLastRowTransaction", "Add Fader Group"));

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

	// Add all fader groups from selected patches in the fixture patch list
	UDMXControlConsoleFaderGroupController* NewController = ActiveLayout->AddToLayout(FaderGroupsToGroup, FString(), RowIndex, ColumnIndex);
	if (NewController)
	{
		NewController->PreEditChange(nullptr);

		const FString UserName = NewController->GenerateUserNameByFaderGroupsNames();
		NewController->SetUserName(UserName);
		NewController->Group();
		NewController->SetIsActive(true);

		NewController->PostEditChange();

		ActiveLayout->AddToActiveFaderGroupControllers(NewController);
	}
	ActiveLayout->PostEditChange();
}

bool SDMXControlConsoleAddFixturePatchMenu::CanGroupPatchesToTheRight() const
{
	return CanAddPatchesToTheRight() && FixturePatches.Num() > 1;
}

void SDMXControlConsoleAddFixturePatchMenu::GroupPatchesOnNewRow()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ControlConsoleData || !ActiveLayout)
	{
		return;
	}

	// Generate on last row if vertical sorting
	if (ActiveLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Vertical)
	{
		GroupPatchesToTheRight();
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
		if (!SelectedFaderGroupController)
		{
			return;
		}

		NewRowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(SelectedFaderGroupController) + 1;
	}

	const TArray<UDMXControlConsoleFaderGroup*> FaderGroupsToGroup = GetFaderGroupsFromFixturePatches();
	if (FaderGroupsToGroup.IsEmpty())
	{
		return;
	}

	// Add all fader groups from selected patches in the fixture patch list
	const FScopedTransaction GroupToNewtRowTransaction(LOCTEXT("GroupToNewRowTransaction", "Add Fader Group"));
	ActiveLayout->PreEditChange(nullptr);
	UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = ActiveLayout->AddNewRowToLayout(NewRowIndex);
	if (NewLayoutRow)
	{
		NewLayoutRow->PreEditChange(nullptr);
		UDMXControlConsoleFaderGroupController* NewController = NewLayoutRow->CreateFaderGroupController(FaderGroupsToGroup);
		if (NewController)
		{
			NewController->PreEditChange(nullptr);

			const FString UserName = NewController->GenerateUserNameByFaderGroupsNames();
			NewController->SetUserName(UserName);
			NewController->Group();
			NewController->SetIsActive(true);

			NewController->PostEditChange();

			ActiveLayout->AddToActiveFaderGroupControllers(NewController);
		}
		NewLayoutRow->PostEditChange();
	}
	ActiveLayout->PostEditChange();
}

bool SDMXControlConsoleAddFixturePatchMenu::CanGroupPatchesOnNewRow() const
{
	return CanAddPatchesOnNewRow() && FixturePatches.Num() > 1;
}

TArray<UDMXControlConsoleFaderGroup*> SDMXControlConsoleAddFixturePatchMenu::GetFaderGroupsFromFixturePatches()
{
	TArray<UDMXControlConsoleFaderGroup*> FaderGroupsToGroup;
	if (!EditorModel.IsValid())
	{
		return FaderGroupsToGroup;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ControlConsoleData || !ActiveLayout)
	{
		return FaderGroupsToGroup;
	}

	for (const TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch : FixturePatches)
	{
		UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			continue;
		}

		UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		if (!FaderGroup)
		{
			continue;
		}

		if (ActiveLayout->ContainsFaderGroup(FaderGroup))
		{
			continue;
		}

		FaderGroupsToGroup.Add(FaderGroup);
	}

	return FaderGroupsToGroup;
}

#undef LOCTEXT_NAMESPACE
