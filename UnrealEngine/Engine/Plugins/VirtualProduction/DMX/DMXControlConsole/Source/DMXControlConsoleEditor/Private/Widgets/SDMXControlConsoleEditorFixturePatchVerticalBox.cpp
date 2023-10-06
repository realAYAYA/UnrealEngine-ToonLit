// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFixturePatchVerticalBox.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/ForEach.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleEditorSelection.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutDefault.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutUser.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityReference.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleReadOnlyFixturePatchList.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFixturePatchVerticalBox"

SDMXControlConsoleEditorFixturePatchVerticalBox::~SDMXControlConsoleEditorFixturePatchVerticalBox()
{
	if (FixturePatchList.IsValid())
	{
		const FDMXReadOnlyFixturePatchListDescriptor ListDescriptor = FixturePatchList->MakeListDescriptor();

		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		EditorConsoleModel->SaveFixturePatchListDescriptorToConfig(ListDescriptor);
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::Construct(const FArguments& InArgs)
{
	RegisterCommands();

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const FDMXReadOnlyFixturePatchListDescriptor ListDescriptor = EditorConsoleModel->GetFixturePatchListDescriptor();

	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	UDMXLibrary* DMXLibrary = EditorConsoleData ? EditorConsoleData->GetDMXLibrary() : nullptr;

	ChildSlot
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				GenerateFixturePatchListToolbar()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(FixturePatchList, SDMXControlConsoleReadOnlyFixturePatchList)
				.ListDescriptor(ListDescriptor)
				.DMXLibrary(DMXLibrary)
				.OnContextMenuOpening(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CreateRowContextMenu)
				.OnRowSelectionChanged(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnRowSelectionChanged)
				.OnRowClicked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnRowClicked)
				.OnRowDoubleClicked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnRowDoubleClicked)
				.OnCheckBoxStateChanged(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnListCheckBoxStateChanged)
				.OnRowCheckBoxStateChanged(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnRowCheckBoxStateChanged)
				.IsChecked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsListChecked)
				.IsRowChecked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsRowChecked)
				.IsRowEnabled(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsFixturePatchListRowEnabled)
				.IsRowVisibile(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsFixturePatchListRowVisible)
			]
		];
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::ForceRefresh()
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (EditorConsoleData && FixturePatchList.IsValid())
	{
		UDMXLibrary* NewLibrary = EditorConsoleData->GetDMXLibrary();
		FixturePatchList->SetDMXLibrary(NewLibrary);
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::RegisterCommands()
{
	CommandList = MakeShared<FUICommandList>();

	const auto MapActionLambda = [this](TSharedPtr<FUICommandInfo> CommandInfo, bool bMute, bool bOnlyActive)
		{
			CommandList->MapAction
			(
				CommandInfo,
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnMuteAllFaderGroups, bMute, bOnlyActive),
				FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsAnyFaderGroupsMuted, !bMute, bOnlyActive),
				FGetActionCheckState(),
				FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsAnyFaderGroupsMuted, !bMute, bOnlyActive)
			);
		};

	MapActionLambda(FDMXControlConsoleEditorCommands::Get().Mute, true, true);
	MapActionLambda(FDMXControlConsoleEditorCommands::Get().MuteAll, true, false);
	MapActionLambda(FDMXControlConsoleEditorCommands::Get().Unmute, false, true);
	MapActionLambda(FDMXControlConsoleEditorCommands::Get().UnmuteAll, false, false);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchNext,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateFromFixturePatchOnLastRow),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CanExecuteAddNext)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchNextRow,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateFromFixturePatchOnNewRow),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CanExecuteAddRow)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchToSelection,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateSelectedFaderGroupFromFixturePatch),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CanExecuteAddSelected)
	);
}

TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::GenerateFixturePatchListToolbar()
{
	const auto GenerateAddButtonContentLambda = [](const FText& AddButtonText, const FText& AddButtonToolTip)
		{
			return
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(FStyleColors::AccentGreen)
					.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(AddButtonText)
					.ToolTipText(AddButtonToolTip)
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
				];
		};

	const TSharedRef<SWidget> FixturePatchListToolbar =
		SNew(SHorizontalBox)
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::GetFixturePatchListToolbarVisibility))

		// Add All Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(160.f)
		.HAlign(HAlign_Left)
		.Padding(8.f, 0.f, 4.f, 8.f)
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ForegroundColor(FSlateColor::UseStyle())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddAllPatchesButtonEnabled)
			.OnClicked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked)
			[
				GenerateAddButtonContentLambda
				(
					LOCTEXT("AddAllFixturePatchFromList", "Add All Patches"),
					LOCTEXT("AddAllFixturePatchFromList_ToolTip", "Add all Fixture Patches from the list.")
				)
			]
		]

		// Add Combo Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(160.f)
		.HAlign(HAlign_Left)
		.Padding(4.f, 0.f, 8.f, 8.f)
		[
			SNew(SComboButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ForegroundColor(FSlateColor::UseStyle())
			.HasDownArrow(true)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnGetMenuContent(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddPatchMenu)
			.ButtonContent()
			[
				GenerateAddButtonContentLambda
				(
					LOCTEXT("AddFixturePatchFromList", "Add Patch"),
					LOCTEXT("AddFixturePatchFromList_ToolTip", "Add a Fixture Patch from the list.")
				)
			]
		];

	return FixturePatchListToolbar;
}

TSharedPtr<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::CreateRowContextMenu()
{
	ensureMsgf(CommandList.IsValid(), TEXT("Invalid command list for control console asset picker."));
	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("MuteFaderGroupContextMenu", "Mute"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().Mute,
			NAME_None,
			LOCTEXT("MuteContextMenu_Label", "Only Active"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Mute")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().MuteAll,
			NAME_None,
			LOCTEXT("MuteAllContextMenu_Label", "All"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Mute")
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("UnmuteFaderGroupContextMenu", "Unmute"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().Unmute,
			NAME_None,
			LOCTEXT("UnmuteContextMenu_Label", "Only Active"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Unmute")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().UnmuteAll,
			NAME_None,
			LOCTEXT("UnmuteAllContextMenu_Label", "All"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Unmute")
		);
	}
	MenuBuilder.EndSection();

	// Show Add Patch buttons only if current layout is User Layout
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (EditorConsoleLayouts)
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		if (CurrentLayout && CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass())
		{
			MenuBuilder.AddWidget(CreateAddPatchMenu(), FText::GetEmpty());
		}
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddPatchMenu()
{
	ensureMsgf(CommandList.IsValid(), TEXT("Invalid command list for control console asset picker."));
	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("AddPatchButtonMainSection", "Add Patch"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddPatchNext,
			NAME_None,
			LOCTEXT("AddPatchNextButtonLabel", "To the right")
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

	return MenuBuilder.MakeWidget();
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::GenerateFaderGroupFromFixturePatch(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch)
{
	if (!FaderGroup || !FixturePatch)
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	SelectionHandler->ClearFadersSelection(FaderGroup);

	const FScopedTransaction GenerateFaderGroupFromFixturePatchTransaction(LOCTEXT("GenerateFaderGroupFromFixturePatchTransaction", "Generate Fader Group from Fixture Patch"));
	FaderGroup->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetSoftFixturePatchPtrPropertyName()));
	FaderGroup->GenerateFromFixturePatch(FixturePatch);
	FaderGroup->PostEditChange();
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnRowSelectionChanged(const TSharedPtr<FDMXEntityFixturePatchRef> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!FixturePatchList.IsValid() || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	// Do only if current layout is Default Layout
	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	UDMXControlConsoleEditorGlobalLayoutDefault* DefaultLayout = Cast<UDMXControlConsoleEditorGlobalLayoutDefault>(CurrentLayout);
	if (!DefaultLayout)
	{
		return;
	}

	const TArray<TSharedPtr<FDMXEntityFixturePatchRef>> SelectedItems = FixturePatchList->GetSelectedFixturePatchRefs();
	const TArray<TSharedPtr<FDMXEntityFixturePatchRef>> VisibleItems = FixturePatchList->GetVisibleFixturePatchRefs();
	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
	TArray<UObject*> FaderGroupsToAddToSelection;
	TArray<UObject*> FaderGroupsToRemoveFromSelection;
	for (UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup || !FaderGroup->HasFixturePatch())
		{
			continue;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		const TSharedPtr<FDMXEntityFixturePatchRef>* FixturePatchRefPtr = Algo::FindByPredicate(SelectedItems, [FixturePatch](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
			{
				return FixturePatchRef.IsValid() && FixturePatchRef->GetFixturePatch() == FixturePatch;
			});

		const bool bIsFixturePatchSelected = FixturePatchRefPtr && FixturePatchRefPtr->IsValid();
		const bool bIsFixturePatchVisible = VisibleItems.ContainsByPredicate([FixturePatch](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
			{
				return FixturePatchRef.IsValid() && FixturePatchRef->GetFixturePatch() == FixturePatch;
			});

		// Set fader group active if is selected and visible
		const bool bIsFaderGroupActive = bIsFixturePatchSelected && bIsFixturePatchVisible;
		FaderGroup->SetIsActive(bIsFaderGroupActive);
		if (bIsFaderGroupActive)
		{
			DefaultLayout->AddToActiveFaderGroups(FaderGroup);
			const bool bAutoSelect = EditorConsoleModel->GetAutoSelectActivePatches();
			if (bAutoSelect)
			{
				const TArray<UDMXControlConsoleFaderBase*> AllFaders = FaderGroup->GetAllFaders();
				FaderGroupsToAddToSelection.Append(AllFaders);
			}
		}
		else
		{
			DefaultLayout->RemoveFromActiveFaderGroups(FaderGroup);
			FaderGroupsToRemoveFromSelection.Add(FaderGroup);
			if (bIsFixturePatchSelected)
			{
				FixturePatchList->SetItemSelection(*FixturePatchRefPtr, false);
			}
		}
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	SelectionHandler->AddToSelection(FaderGroupsToAddToSelection);
	SelectionHandler->RemoveFromSelection(FaderGroupsToRemoveFromSelection);
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnRowClicked(const TSharedPtr<FDMXEntityFixturePatchRef> ItemClicked)
{
	if (!ItemClicked.IsValid())
	{
		return;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	const UDMXEntityFixturePatch* FixturePatch = ItemClicked->GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	if (FaderGroup && FaderGroup->IsActive())
	{
		EditorConsoleModel->ScrollIntoView(FaderGroup);
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnRowDoubleClicked(const TSharedPtr<FDMXEntityFixturePatchRef> ItemClicked)
{
	if (!ItemClicked.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	const UDMXEntityFixturePatch* FixturePatch = ItemClicked->GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	if (FaderGroup && FaderGroup->IsActive())
	{
		FaderGroup->SetIsExpanded(true);
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnListCheckBoxStateChanged(ECheckBoxState CheckBoxState)
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
		Algo::ForEach(AllFaderGroups, [CheckBoxState](UDMXControlConsoleFaderGroup* FaderGroup)
			{
				if (FaderGroup && FaderGroup->HasFixturePatch())
				{
					const bool bIsMuted = CheckBoxState == ECheckBoxState::Unchecked;
					FaderGroup->SetMute(bIsMuted);
				}
			});
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnRowCheckBoxStateChanged(ECheckBoxState CheckBoxState, const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef)
{
	const UDMXEntityFixturePatch* FixturePatch = InFixturePatchRef.IsValid() ? InFixturePatchRef->GetFixturePatch() : nullptr;
	if (!FixturePatch)
	{
		return;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	if (FaderGroup)
	{
		const bool bIsMuted = CheckBoxState == ECheckBoxState::Unchecked;
		FaderGroup->SetMute(bIsMuted);
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateFromFixturePatchOnLastRow()
{
	if (!FixturePatchList.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	int32 RowIndex = CurrentLayout->GetLayoutRows().Num() - 1;
	int32 ColumnIndex = INDEX_NONE;
	
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (!SelectedFaderGroupsObjects.IsEmpty())
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup(true);
		RowIndex = CurrentLayout->GetFaderGroupRowIndex(SelectedFaderGroup);
		ColumnIndex = CurrentLayout->GetFaderGroupColumnIndex(SelectedFaderGroup) + 1;
	}

	// Add all selected Fixture Patches from Fixture Patch List
	for (const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef : FixturePatchList->GetSelectedFixturePatchRefs())
	{
		if (!FixturePatchRef.IsValid())
		{
			continue;
		}

		UDMXEntityFixturePatch* FixturePatch = FixturePatchRef->GetFixturePatch();
		UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		if (!FaderGroup)
		{
			continue;
		}

		const FScopedTransaction AddToLastRowTransaction(LOCTEXT("AddToLastRowTransaction", "Add Fader Group"));
		CurrentLayout->PreEditChange(nullptr);
		if (ColumnIndex == INDEX_NONE)
		{
			CurrentLayout->AddToLayout(FaderGroup, RowIndex);
		}
		else
		{
			CurrentLayout->AddToLayout(FaderGroup, RowIndex, ColumnIndex);
			ColumnIndex++;
		}

		FaderGroup->Modify();
		FaderGroup->SetIsActive(true);

		CurrentLayout->PostEditChange();
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateFromFixturePatchOnNewRow()
{
	if (!FixturePatchList.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	// Generate on last row if vertical sorting
	if (CurrentLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Vertical)
	{
		OnGenerateFromFixturePatchOnLastRow();
		return;
	}

	int32 NewRowIndex = INDEX_NONE;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		NewRowIndex = CurrentLayout->GetLayoutRows().Num();
	}
	else
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup(true);
		if (!SelectedFaderGroup)
		{
			return;
		}

		NewRowIndex = CurrentLayout->GetFaderGroupRowIndex(SelectedFaderGroup) + 1;
	}

	const FScopedTransaction AddToNewtRowTransaction(LOCTEXT("AddToNewtRowTransaction", "Add Fader Group"));
	CurrentLayout->PreEditChange(nullptr);
	UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = CurrentLayout->AddNewRowToLayout(NewRowIndex);
	CurrentLayout->PostEditChange();
	
	if (NewLayoutRow)
	{
		// Add all selected Fixture Patches from Fixture Patch List
		NewLayoutRow->PreEditChange(nullptr);
		for (const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef : FixturePatchList->GetSelectedFixturePatchRefs())
		{
			if (!FixturePatchRef.IsValid())
			{
				continue;
			}

			UDMXEntityFixturePatch* FixturePatch = FixturePatchRef->GetFixturePatch();
			UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
			if (!FaderGroup)
			{
				continue;
			}

			NewLayoutRow->AddToLayoutRow(FaderGroup);
			FaderGroup->Modify();
			FaderGroup->SetIsActive(true);
		}

		NewLayoutRow->PostEditChange();
	}
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnGenerateSelectedFaderGroupFromFixturePatch()
{
	if (!FixturePatchList.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const UDMXControlConsoleFaderGroup* FirstSelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup();
	if (!FirstSelectedFaderGroup)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	const int32 RowIndex = CurrentLayout->GetFaderGroupRowIndex(FirstSelectedFaderGroup);

	const FScopedTransaction ReplaceSelectedFaderGroupTransaction(LOCTEXT("ReplaceSelectedFaderGroupTransaction", "Replace Fader Group"));
	CurrentLayout->PreEditChange(nullptr);

	// Add all Selected Patches Fader Groups to layout
	TArray<UObject*> FaderGroupsToSelect;
	const TArray<TSharedPtr<FDMXEntityFixturePatchRef>> SelectedFixturePatches = FixturePatchList->GetSelectedFixturePatchRefs();
	int32 ColumnIndex = CurrentLayout->GetFaderGroupColumnIndex(FirstSelectedFaderGroup);
	for (const TSharedPtr<FDMXEntityFixturePatchRef>& SelectedFixturePatch : SelectedFixturePatches)
	{
		const UDMXEntityFixturePatch* FixturePatch = SelectedFixturePatch.IsValid() ? SelectedFixturePatch->GetFixturePatch() : nullptr;
		if (!FixturePatch)
		{
			continue;
		}

		UDMXControlConsoleFaderGroup* FaderGroupToAdd = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		CurrentLayout->AddToLayout(FaderGroupToAdd, RowIndex, ColumnIndex);

		FaderGroupToAdd->Modify();
		FaderGroupToAdd->SetIsActive(true);
		FaderGroupToAdd->SetIsExpanded(FirstSelectedFaderGroup->IsExpanded());

		FaderGroupsToSelect.Add(FaderGroupToAdd);

		ColumnIndex++;
	}

	// Remove all Selected Fader Groups from layout
	TArray<UObject*> FaderGroupsToUnselect;
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	for (const TWeakObjectPtr<UObject> SelectedFaderGroupObject : SelectedFaderGroupsObjects)
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectedFaderGroupObject.IsValid() ? Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupObject.Get()) : nullptr;
		if (!SelectedFaderGroup)
		{
			continue;
		}

		CurrentLayout->RemoveFromLayout(SelectedFaderGroup);
		if (!SelectedFaderGroup->HasFixturePatch())
		{
			SelectedFaderGroup->Destroy();
		}

		FaderGroupsToUnselect.Add(SelectedFaderGroup);
	}

	CurrentLayout->ClearEmptyLayoutRows();
	CurrentLayout->PostEditChange();

	constexpr bool bNotifySelectionChange = false;
	SelectionHandler->AddToSelection(FaderGroupsToSelect, bNotifySelectionChange);
	SelectionHandler->RemoveFromSelection(FaderGroupsToUnselect);

	EditorConsoleModel->RequestRefresh();
}

FReply SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked()
{
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return FReply::Handled();
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return FReply::Handled();
	}

	const FScopedTransaction AddAllPatchesTransaction(LOCTEXT("AddAllPatchesTransaction", "Add All Patches"));
	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = EditorConsoleData->GetFaderGroupRows();
	for (const UDMXControlConsoleFaderGroupRow* FaderGroupRow : FaderGroupRows)
	{
		if (!FaderGroupRow)
		{
			continue;
		}

		// Remove Fader Groups already in the layout and all unpatched Fader Groups
		TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow->GetFaderGroups();
		FaderGroups.RemoveAll([&CurrentLayout](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && 
					(!FaderGroup->HasFixturePatch() ||
					CurrentLayout->ContainsFaderGroup(FaderGroup));
			});

		CurrentLayout->PreEditChange(nullptr);
		UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = CurrentLayout->AddNewRowToLayout();
		CurrentLayout->PostEditChange();

		if (LayoutRow)
		{
			LayoutRow->PreEditChange(nullptr);
			LayoutRow->AddToLayoutRow(FaderGroups);
			LayoutRow->PostEditChange();

			Algo::ForEach(FaderGroups,[](UDMXControlConsoleFaderGroup* FaderGroup)
				{
					FaderGroup->Modify();
					FaderGroup->SetIsActive(true);
				});
		}
	}

	return FReply::Handled();
}

ECheckBoxState SDMXControlConsoleEditorFixturePatchVerticalBox::IsListChecked() const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		// Get all patched Fader Groups
		TArray<UDMXControlConsoleFaderGroup*> AllPatchedFaderGroups = EditorConsoleData->GetAllFaderGroups();
		AllPatchedFaderGroups.RemoveAll([](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && !FaderGroup->HasFixturePatch();
			});

		const bool bAreAllFaderGroupsUnmuted = Algo::AllOf(AllPatchedFaderGroups, [](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && !FaderGroup->IsMuted();
			});

		if (bAreAllFaderGroupsUnmuted)
		{
			return ECheckBoxState::Checked;
		}

		const bool bIsAnyFaderGroupUnmuted = Algo::AnyOf(AllPatchedFaderGroups, [](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && !FaderGroup->IsMuted();
			});

		return bIsAnyFaderGroupUnmuted ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

ECheckBoxState SDMXControlConsoleEditorFixturePatchVerticalBox::IsRowChecked(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	const UDMXEntityFixturePatch* FixturePatch = InFixturePatchRef.IsValid() ? InFixturePatchRef->GetFixturePatch() : nullptr;
	if (!FixturePatch)
	{
		return ECheckBoxState::Undetermined;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		return IsValid(FaderGroup) && FaderGroup->IsMuted() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
	}

	return ECheckBoxState::Undetermined;
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::OnMuteAllFaderGroups(bool bMute, bool bOnlyActive) const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = bOnlyActive ? EditorConsoleData->GetAllActiveFaderGroups() : EditorConsoleData->GetAllFaderGroups();
		Algo::ForEach(AllFaderGroups, [bMute](UDMXControlConsoleFaderGroup* FaderGroup)
			{
				if (FaderGroup)
				{
					FaderGroup->SetMute(bMute);
				}
			});
	}
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsAnyFaderGroupsMuted(bool bMute, bool bOnlyActive) const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = bOnlyActive ? EditorConsoleData->GetAllActiveFaderGroups() : EditorConsoleData->GetAllFaderGroups();
		return Algo::AnyOf(AllFaderGroups, [bMute](UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && FaderGroup->IsMuted() == bMute;
			});
	}

	return false;
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsFixturePatchListRowEnabled(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	const UDMXEntityFixturePatch* FixturePatch = InFixturePatchRef.IsValid() ? InFixturePatchRef->GetFixturePatch() : nullptr;
	if (!FixturePatch)
	{
		return true;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return true;
	}

	// Do only if current layout is User Layout
	const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return true;
	}

	if (CurrentLayout->GetClass() != UDMXControlConsoleEditorGlobalLayoutUser::StaticClass())
	{
		return true;
	}
	
	return !IsValid(CurrentLayout->FindFaderGroupByFixturePatch(FixturePatch));
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsFixturePatchListRowVisible(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	if (!InFixturePatchRef.IsValid())
	{
		return true;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return true;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (CurrentLayout && CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutDefault::StaticClass())
	{
		return IsRowVisibleInDefaultLayout(InFixturePatchRef);
	}
	else if (CurrentLayout && CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass())
	{
		return IsRowVisibleInUserLayout(InFixturePatchRef);
	}

	return true;
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsRowVisibleInDefaultLayout(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	const UDMXEntityFixturePatch* FixturePatch = InFixturePatchRef.IsValid() ? InFixturePatchRef->GetFixturePatch() : nullptr;
	if (!FixturePatch)
	{
		return true;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return true;
	}

	const UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	if (!FaderGroup)
	{
		return true;
	}

	const bool bIsMuted = FaderGroup->IsMuted();
	const EDMXReadOnlyFixturePatchListShowMode ShowMode = FixturePatchList.IsValid() ? FixturePatchList->GetShowMode() : EDMXReadOnlyFixturePatchListShowMode::All;
	switch (ShowMode)
	{
	case EDMXReadOnlyFixturePatchListShowMode::Active:
		return !bIsMuted;
		break;
	case EDMXReadOnlyFixturePatchListShowMode::Inactive:
		return bIsMuted;
		break;
	default:
		return true;
	}
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsRowVisibleInUserLayout(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	if (!InFixturePatchRef.IsValid())
	{
		return true;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return true;
	}

	const bool bIsEnabled = IsFixturePatchListRowEnabled(InFixturePatchRef);
	const EDMXReadOnlyFixturePatchListShowMode ShowMode = FixturePatchList.IsValid() ? FixturePatchList->GetShowMode() : EDMXReadOnlyFixturePatchListShowMode::All;
	switch (ShowMode)
	{
	case EDMXReadOnlyFixturePatchListShowMode::Active:
		return bIsEnabled;
		break;
	case EDMXReadOnlyFixturePatchListShowMode::Inactive:
		return !bIsEnabled;
		break;
	default:
		return true;
	}
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddAllPatchesButtonEnabled() const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	return IsValid(EditorConsoleData) && IsValid(EditorConsoleData->GetDMXLibrary());
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::CanExecuteAddNext() const
{
	bool bCanExecute =
		FixturePatchList.IsValid() &&
		!FixturePatchList->GetSelectedFixturePatchRefs().IsEmpty();

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (EditorConsoleData && EditorConsoleLayouts)
	{
		// True if there's no global filter and no vertical sorting
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		bCanExecute &=
			IsValid(CurrentLayout) &&
			CurrentLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Vertical &&
			!CurrentLayout->GetAllFaderGroups().IsEmpty() &&
			EditorConsoleData->FilterString.IsEmpty();
	}

	return bCanExecute;
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::CanExecuteAddRow() const
{
	bool bCanExecute =
		FixturePatchList.IsValid() &&
		!FixturePatchList->GetSelectedFixturePatchRefs().IsEmpty();

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (EditorConsoleData && EditorConsoleLayouts)
	{
		// True if there's no global filter and no horizontal sorting
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		bCanExecute &=
			IsValid(CurrentLayout) &&
			CurrentLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Horizontal &&
			EditorConsoleData->FilterString.IsEmpty();
	}

	return bCanExecute;
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::CanExecuteAddSelected() const
{
	bool bCanExecute =
		FixturePatchList.IsValid() &&
		!FixturePatchList->GetSelectedFixturePatchRefs().IsEmpty();

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		// True if there's if there's no global filter and at least one selected Fader Group
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		bCanExecute &=
			EditorConsoleData->FilterString.IsEmpty() &&
			!SelectionHandler->GetSelectedFaderGroups().IsEmpty();
	}

	return bCanExecute;
}

EVisibility SDMXControlConsoleEditorFixturePatchVerticalBox::GetFixturePatchListToolbarVisibility() const
{
	bool bIsVisible = false;
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts())
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		bIsVisible = IsValid(CurrentLayout) && CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass();
	}

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
