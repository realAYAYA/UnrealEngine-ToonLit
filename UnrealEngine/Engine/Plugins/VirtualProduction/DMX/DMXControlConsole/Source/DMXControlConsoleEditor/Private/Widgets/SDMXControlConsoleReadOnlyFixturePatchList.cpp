// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleReadOnlyFixturePatchList.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutDefault.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Widgets/SDMXControlConsoleReadOnlyFixturePatchListRow.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleReadOnlyFixturePatchList"

const FName FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::CheckBox = "CheckBox";

void SDMXControlConsoleReadOnlyFixturePatchList::Construct(const FArguments& InArgs)
{
	OnCheckBoxStateChangedDelegate = InArgs._OnCheckBoxStateChanged;
	OnRowCheckBoxStateChangedDelegate = InArgs._OnRowCheckBoxStateChanged;
	IsCheckedDelegate = InArgs._IsChecked;
	IsRowCheckedDelegate = InArgs._IsRowChecked;

	FDMXReadOnlyFixturePatchListDescriptor ListDescriptor = InArgs._ListDescriptor;
	ListDescriptor.ColumnIDToShowStateMap.Add(FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::CheckBox) = true;

	SDMXReadOnlyFixturePatchList::Construct(SDMXReadOnlyFixturePatchList::FArguments()
		.ListDescriptor(ListDescriptor)
		.DMXLibrary(InArgs._DMXLibrary)
		.OnContextMenuOpening(InArgs._OnContextMenuOpening)
		.OnRowSelectionChanged(InArgs._OnRowSelectionChanged)
		.OnRowClicked(InArgs._OnRowClicked)
		.OnRowDoubleClicked(InArgs._OnRowDoubleClicked)
		.IsRowEnabled(InArgs._IsRowEnabled)
		.IsRowVisibile(InArgs._IsRowVisibile));

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	EditorConsoleModel->GetOnConsoleLoaded().AddSP(this, &SDMXControlConsoleReadOnlyFixturePatchList::SyncSelection);
	EditorConsoleModel->GetOnConsoleLoaded().AddSP(this, &SDMXControlConsoleReadOnlyFixturePatchList::Register);
	EditorConsoleModel->GetOnControlConsoleForceRefresh().AddSP(this, &SDMXControlConsoleReadOnlyFixturePatchList::SyncSelection);

	Register();
	SyncSelection();
}

void SDMXControlConsoleReadOnlyFixturePatchList::InitializeByListDescriptor(const FDMXReadOnlyFixturePatchListDescriptor& InListDescriptor)
{
	SortedByColumnID = InListDescriptor.SortedByColumnID;
	SortByColumnID(EColumnSortPriority::None, SortedByColumnID, SortMode);

	ColumnIDToShowStateMap.Append(InListDescriptor.ColumnIDToShowStateMap);
	for (const TPair<FName, bool>& ColumnIDToShowState : ColumnIDToShowStateMap)
	{
		if (!HeaderRow.IsValid())
		{
			break;
		}

		const FName& ColumnID = ColumnIDToShowState.Key;
		// EditorColor and CheckBox columns should always be showed
		if (ColumnID == FDMXReadOnlyFixturePatchListCollumnIDs::EditorColor ||
			ColumnID == FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::CheckBox)
		{
			continue;
		}

		const bool bShowState = ColumnIDToShowState.Value;
		HeaderRow->SetShowGeneratedColumn(ColumnID, bShowState);
	}
}

void SDMXControlConsoleReadOnlyFixturePatchList::RefreshList()
{
	SDMXReadOnlyFixturePatchList::RefreshList();

	SyncSelection();
}

TSharedRef<ITableRow> SDMXControlConsoleReadOnlyFixturePatchList::OnGenerateRow(TSharedPtr<FDMXEntityFixturePatchRef> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const TSharedRef<SDMXControlConsoleReadOnlyFixturePatchListRow> NewRow =
		SNew(SDMXControlConsoleReadOnlyFixturePatchListRow, OwnerTable, InItem.ToSharedRef())
		.OnCheckStateChanged(this, &SDMXControlConsoleReadOnlyFixturePatchList::OnRowCheckBoxStateChanged, InItem)
		.IsEnabled(this, &SDMXControlConsoleReadOnlyFixturePatchList::IsRowEnabled, InItem)
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleReadOnlyFixturePatchList::GetRowVisibility, InItem))
		.IsChecked(this, &SDMXControlConsoleReadOnlyFixturePatchList::IsRowChecked, InItem);

	return NewRow;
}

EVisibility SDMXControlConsoleReadOnlyFixturePatchList::GetRowVisibility(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	const bool bIsVisible = IsRowVisibleDelegate.IsBound() ? IsRowVisibleDelegate.Execute(InFixturePatchRef) : true;
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SHeaderRow> SDMXControlConsoleReadOnlyFixturePatchList::GenerateHeaderRow()
{
	HeaderRow = SDMXReadOnlyFixturePatchList::GenerateHeaderRow();
	if (HeaderRow.IsValid())
	{
		HeaderRow->InsertColumn(SHeaderRow::FColumn::FArguments()
			.ColumnId(FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::CheckBox)
			.DefaultLabel(LOCTEXT("CheckBoxColumnLabel", ""))
			.FixedWidth(32.f)
			.HeaderContent()
			[
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
						.IsChecked(this, &SDMXControlConsoleReadOnlyFixturePatchList::IsChecked)
						.OnCheckStateChanged(OnCheckBoxStateChangedDelegate)
					]
				]
			]
			, 1);
	}

	return HeaderRow.ToSharedRef();
}

void SDMXControlConsoleReadOnlyFixturePatchList::SyncSelection()
{
	if (!ListView.IsValid())
	{
		return;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	// Do only if current layout is Default Layout
	const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout || CurrentLayout->GetClass() != UDMXControlConsoleEditorGlobalLayoutDefault::StaticClass())
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
	for (const UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup || !FaderGroup->HasFixturePatch())
		{
			continue;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		const TSharedPtr<FDMXEntityFixturePatchRef>* FixturePatcheRefPtr = Algo::FindByPredicate(ListItems, [FixturePatch](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
			{
				return FixturePatchRef->GetFixturePatch() == FixturePatch;
			});

		if (FixturePatcheRefPtr)
		{
			const bool bIsSelected = FaderGroup->IsActive();
			ListView->SetItemSelection(*FixturePatcheRefPtr, bIsSelected, ESelectInfo::Direct);
		}
	}
}

void SDMXControlConsoleReadOnlyFixturePatchList::Register()
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	if (!EditorConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
	{
		EditorConsoleData->GetOnFaderGroupAdded().AddSP(this, &SDMXControlConsoleReadOnlyFixturePatchList::OnEditorConsoleDataChanged);
	}

	if (!EditorConsoleData->GetOnFaderGroupRemoved().IsBoundToObject(this))
	{
		EditorConsoleData->GetOnFaderGroupRemoved().AddSP(this, &SDMXControlConsoleReadOnlyFixturePatchList::OnEditorConsoleDataChanged);
	}

	if (!EditorConsoleLayouts->GetOnActiveLayoutChanged().IsBoundToObject(this))
	{
		EditorConsoleLayouts->GetOnActiveLayoutChanged().AddSP(this, &SDMXControlConsoleReadOnlyFixturePatchList::SyncSelection);
	}
}

void SDMXControlConsoleReadOnlyFixturePatchList::OnEditorConsoleDataChanged(const UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (FaderGroup)
	{
		SyncSelection();
	}
}

void SDMXControlConsoleReadOnlyFixturePatchList::OnRowCheckBoxStateChanged(ECheckBoxState CheckBoxState, const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef)
{
	OnRowCheckBoxStateChangedDelegate.ExecuteIfBound(CheckBoxState, InFixturePatchRef);
}

ECheckBoxState SDMXControlConsoleReadOnlyFixturePatchList::IsChecked() const
{
	if (IsCheckedDelegate.IsBound())
	{
		return IsCheckedDelegate.Execute();
	}

	return ECheckBoxState::Undetermined;
}

ECheckBoxState SDMXControlConsoleReadOnlyFixturePatchList::IsRowChecked(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	if (IsRowCheckedDelegate.IsBound())
	{
		return IsRowCheckedDelegate.Execute(InFixturePatchRef);
	}

	return ECheckBoxState::Undetermined;
}

#undef LOCTEXT_NAMESPACE
