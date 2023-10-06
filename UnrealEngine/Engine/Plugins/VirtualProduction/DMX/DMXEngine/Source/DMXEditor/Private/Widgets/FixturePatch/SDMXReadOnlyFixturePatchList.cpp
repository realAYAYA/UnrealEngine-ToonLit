// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXReadOnlyFixturePatchList.h"

#include "Algo/Sort.h"
#include "DMXEditorUtils.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SDMXReadOnlyFixturePatchListRow.h"
#include "Widgets/Views/SListView.h"


#define LOCTEXT_NAMESPACE "SDMXReadOnlyFixturePatchList"

namespace UE::DMX::SDMXReadOnlyFixturePatchListNamespace::Private
{
	class FScopedRestoreSelection
	{
	public:
		FScopedRestoreSelection(const TSharedRef<SDMXReadOnlyFixturePatchList>& InFixturePatchList)
			: FixturePatchList(InFixturePatchList)
		{
			SelectedItemsToRestore = FixturePatchList->GetSelectedFixturePatchRefs();
		}

		~FScopedRestoreSelection()
		{
			const TArray<TSharedPtr<FDMXEntityFixturePatchRef>> NewListItems = FixturePatchList->GetListItems();
			
			TArray<TSharedPtr<FDMXEntityFixturePatchRef>> NewSelection;
			for (const TSharedPtr<FDMXEntityFixturePatchRef>& ItemToRestore : SelectedItemsToRestore)
			{
				const TSharedPtr<FDMXEntityFixturePatchRef>* CorrespondingItemPtr = Algo::FindByPredicate(NewListItems, [ItemToRestore](const TSharedPtr<FDMXEntityFixturePatchRef> NewItem)
					{
						return NewItem->GetFixturePatch() == ItemToRestore->GetFixturePatch();
					});
				if (CorrespondingItemPtr)
				{
					NewSelection.Add(*CorrespondingItemPtr);
				}
			}

			FixturePatchList->SelectItems(NewSelection);
		}

	private:
		/** Addressed fixture patch list */
		TSharedRef<SDMXReadOnlyFixturePatchList> FixturePatchList;

		/** Items to restore */
		TArray<TSharedPtr<FDMXEntityFixturePatchRef>> SelectedItemsToRestore;
	};
}

const FName FDMXReadOnlyFixturePatchListCollumnIDs::EditorColor = "EditorColor";
const FName FDMXReadOnlyFixturePatchListCollumnIDs::FixturePatchName = "Name";
const FName FDMXReadOnlyFixturePatchListCollumnIDs::FixtureID = "FID";
const FName FDMXReadOnlyFixturePatchListCollumnIDs::FixtureType = "Type";
const FName FDMXReadOnlyFixturePatchListCollumnIDs::Mode = "Mode";
const FName FDMXReadOnlyFixturePatchListCollumnIDs::Patch = "Patch";

void SDMXReadOnlyFixturePatchList::Construct(const FArguments& InArgs)
{
	WeakDMXLibrary = InArgs._DMXLibrary;

	IsRowEnabledDelegate = InArgs._IsRowEnabled;
	IsRowVisibleDelegate = InArgs._IsRowVisibile;
	OnRowDragDetectedDelegate = InArgs._OnRowDragDetected;

	UpdateListItems();

	ChildSlot
		[
			SNew(SVerticalBox)

			// SearchBox section
			+ SVerticalBox::Slot()
			.Padding(8.f, 0.f, 8.f, 8.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.Padding(0.f, 0.f, 4.f, 0.f)
				.AutoWidth()
				[
					SNew(SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.HasDownArrow(true)
					.OnGetMenuContent(this, &SDMXReadOnlyFixturePatchList::GenerateHeaderRowVisibilityMenu)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Filter"))
					]
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SAssignNew(SearchBox, SSearchBox)
					.OnTextChanged(this, &SDMXReadOnlyFixturePatchList::OnSearchTextChanged)
					.ToolTipText(LOCTEXT("SearchBarTooltip", "Examples:\n\n* PatchName\n* FixtureTypeName\n* SomeMode\n* 1.\n* 1.1\n* Universe 1\n* Uni 1-3\n* Uni 1, 3\n* Uni 1, 4-5'."))
				]
			]

			// ListView section
			+ SVerticalBox::Slot()
			[
				SAssignNew(ListView, SListView<TSharedPtr<FDMXEntityFixturePatchRef>>)
				.HeaderRow(GenerateHeaderRow())
				.ItemHeight(60.0f)
				.ListItemsSource(&ListItems)
				.OnGenerateRow(this, &SDMXReadOnlyFixturePatchList::OnGenerateRow)
				.OnContextMenuOpening(InArgs._OnContextMenuOpening)
				.OnSelectionChanged(InArgs._OnRowSelectionChanged)
				.OnMouseButtonClick(InArgs._OnRowClicked)
				.OnMouseButtonDoubleClick(InArgs._OnRowDoubleClicked)
			]
		];


	// Handle Entity changes
	UDMXLibrary::GetOnEntitiesAdded().AddSP(this, &SDMXReadOnlyFixturePatchList::OnEntityAddedOrRemoved);
	UDMXLibrary::GetOnEntitiesRemoved().AddSP(this, &SDMXReadOnlyFixturePatchList::OnEntityAddedOrRemoved);
	UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddSP(this, &SDMXReadOnlyFixturePatchList::OnFixturePatchChanged);
	UDMXEntityFixtureType::GetOnFixtureTypeChanged().AddSP(this, &SDMXReadOnlyFixturePatchList::OnFixtureTypeChanged);
	
	// 5.3 Hotfix for crashes and bugs when reloading DMX Library assets. 
	// Note, the workaround here is not ideal and should not be transported to future versions. A proper fix already exists in 5.4 with 27111707. 
	FCoreUObjectDelegates::OnPackageReloaded.AddLambda(
		[WeakThis = TWeakPtr<SDMXReadOnlyFixturePatchList>(StaticCastSharedRef<SDMXReadOnlyFixturePatchList>(AsShared()))](const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent)
		{
			if (WeakThis.IsValid())
			{
				WeakThis.Pin()->RefreshList();
			}
		});

	InitializeByListDescriptor(InArgs._ListDescriptor);
}

void SDMXReadOnlyFixturePatchList::RequestListRefresh()
{
	if (!ListRefreshTimerHandle.IsValid())
	{
		ListRefreshTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXReadOnlyFixturePatchList::RefreshList));
	}
}

void SDMXReadOnlyFixturePatchList::SetDMXLibrary(UDMXLibrary* InDMXLibrary)
{
	WeakDMXLibrary = InDMXLibrary;
	RequestListRefresh();
}

void SDMXReadOnlyFixturePatchList::SetItemSelection(const TSharedPtr<FDMXEntityFixturePatchRef> SelectedItem, bool bSelected)
{
	if (ListView.IsValid() && SelectedItem.IsValid())
	{
		ListView->SetItemSelection(SelectedItem, bSelected);
	}
}

void SDMXReadOnlyFixturePatchList::SetExcludedFixturePatches(const TArray<FDMXEntityFixturePatchRef>& NewExcludedFixturePatches)
{
	ExcludedFixturePatches = NewExcludedFixturePatches;
	RequestListRefresh();
}

void SDMXReadOnlyFixturePatchList::SelectItems(const TArray<TSharedPtr<FDMXEntityFixturePatchRef>>& ItemsToSelect, ESelectInfo::Type SelectInfo)
{
	constexpr bool bSelected = true;
	ListView->ClearSelection();
	ListView->SetItemSelection(ItemsToSelect, bSelected, SelectInfo);
}

TArray<TSharedPtr<FDMXEntityFixturePatchRef>> SDMXReadOnlyFixturePatchList::GetSelectedFixturePatchRefs() const
{
	return ListView.IsValid() ? ListView->GetSelectedItems() : TArray<TSharedPtr<FDMXEntityFixturePatchRef>>();
}

TArray<TSharedPtr<FDMXEntityFixturePatchRef>> SDMXReadOnlyFixturePatchList::GetVisibleFixturePatchRefs() const
{
	TArray<TSharedPtr<FDMXEntityFixturePatchRef>> VisilbleFixturePatchRefs = ListItems;
	VisilbleFixturePatchRefs.RemoveAll([this](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
		{
			if (FixturePatchRef.IsValid() && IsRowVisibleDelegate.IsBound())
			{
				return !IsRowVisibleDelegate.Execute(FixturePatchRef);
			}
			return false;
		});
	
	return VisilbleFixturePatchRefs;
}

FDMXReadOnlyFixturePatchListDescriptor SDMXReadOnlyFixturePatchList::MakeListDescriptor() const
{
	FDMXReadOnlyFixturePatchListDescriptor ListDescriptor;
	ListDescriptor.SortedByColumnID = SortedByColumnID;
	ListDescriptor.ColumnIDToShowStateMap.Append(ColumnIDToShowStateMap);

	return ListDescriptor;
}

void SDMXReadOnlyFixturePatchList::InitializeByListDescriptor(const FDMXReadOnlyFixturePatchListDescriptor& InListDescriptor)
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
		// EditorColor column should always be showed
		if (ColumnID == FDMXReadOnlyFixturePatchListCollumnIDs::EditorColor)
		{
			continue;
		}

		const bool bShowState = ColumnIDToShowState.Value;
		HeaderRow->SetShowGeneratedColumn(ColumnID, bShowState);
	}
}

void SDMXReadOnlyFixturePatchList::RefreshList()
{
	ListRefreshTimerHandle.Invalidate();

	using namespace UE::DMX::SDMXReadOnlyFixturePatchListNamespace::Private;
	const FScopedRestoreSelection ScopedRestoreSelection(StaticCastSharedRef<SDMXReadOnlyFixturePatchList>(AsShared()));

	UpdateListItems();
	ListRows.Reset(ListItems.Num());

	SortByColumnID(EColumnSortPriority::Max, SortedByColumnID, SortMode);
	ListItems = FilterListItems(SearchBox->GetText());

	ListItems.RemoveAll([this](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
		{
			return ExcludedFixturePatches.Contains(FixturePatchRef->GetFixturePatch());
		});

	if (ListView.IsValid())
	{
		ListView->RebuildList();
	}
}

TArray<TSharedPtr<FDMXEntityFixturePatchRef>> SDMXReadOnlyFixturePatchList::FilterListItems(const FText& SearchText)
{
	// Filter and return in order of precendence
	if (SearchText.IsEmpty())
	{
		return ListItems;
	}

	TArray<TSharedPtr<FDMXEntityFixturePatchRef>> Items = ListItems;
	const FString SearchString = SearchText.ToString();

	const TArray<int32> Universes = FDMXEditorUtils::ParseUniverses(SearchString);
	if (!Universes.IsEmpty())
	{
		Items.RemoveAll([Universes](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
			{
				const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
				if (FixturePatch)
				{
					return !Universes.Contains(FixturePatch->GetUniverseID());
				}

				return true;
			});

		return Items;
	}

	int32 Address;
	if (FDMXEditorUtils::ParseAddress(SearchString, Address))
	{
		Items.RemoveAll([Address](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
			{
				const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
				if (FixturePatch)
				{
					return FixturePatch->GetStartingChannel() != Address;
				}

				return true;
			});

		return Items;
	}

	const TArray<int32> FixtureIDs = FDMXEditorUtils::ParseFixtureIDs(SearchString);
	for (int32 FixtureID : FixtureIDs)
	{
		Items.RemoveAll([FixtureID](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
			{
				const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
				if (FixturePatch)
				{
					int32 FID;
					if (FixturePatch->FindFixtureID(FID))
					{
						return FID != FixtureID;
					}
				}

				return true;
			});

		if (ListItems.Num() > 0)
		{
			return Items;
		}
	}

	Items.RemoveAll([SearchString](const TSharedPtr<FDMXEntityFixturePatchRef>& FixturePatchRef)
		{
			const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
			if (FixturePatch)
			{
				return !FixturePatch->Name.Contains(SearchString);
			}

			return true;
		});

	return Items;
}

void SDMXReadOnlyFixturePatchList::UpdateListItems()
{
	ListItems.Empty();

	if (!WeakDMXLibrary.IsValid())
	{
		return;
	}

	const TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = WeakDMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatchesInLibrary)
	{
		if (!FixturePatch)
		{
			continue;
		}

		const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef = MakeShared<FDMXEntityFixturePatchRef>();
		FixturePatchRef->SetEntity(FixturePatch);
		ListItems.Add(FixturePatchRef);
	}
}

TSharedRef<ITableRow> SDMXReadOnlyFixturePatchList::OnGenerateRow(TSharedPtr<FDMXEntityFixturePatchRef> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const TSharedRef<SDMXReadOnlyFixturePatchListRow> NewRow =
		SNew(SDMXReadOnlyFixturePatchListRow, OwnerTable, InItem.ToSharedRef())
		.IsEnabled(this, &SDMXReadOnlyFixturePatchList::IsRowEnabled, InItem)
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXReadOnlyFixturePatchList::GetRowVisibility, InItem))
		.OnRowDragDetected(OnRowDragDetectedDelegate);

	return NewRow;
}

bool SDMXReadOnlyFixturePatchList::IsRowEnabled(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	if (IsRowEnabledDelegate.IsBound())
	{
		return IsRowEnabledDelegate.Execute(InFixturePatchRef);
	}

	return true;
}

EVisibility SDMXReadOnlyFixturePatchList::GetRowVisibility(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const
{
	const bool bIsEnabled = IsRowEnabled(InFixturePatchRef);
	bool bIsVisible = IsRowVisibleDelegate.IsBound() ? IsRowVisibleDelegate.Execute(InFixturePatchRef) : true;

	switch (ShowMode)
	{
	case EDMXReadOnlyFixturePatchListShowMode::All:
		break;
	case EDMXReadOnlyFixturePatchListShowMode::Active:
		bIsVisible &= bIsEnabled;
		break;
	case EDMXReadOnlyFixturePatchListShowMode::Inactive:
		bIsVisible &= !bIsEnabled;
		break;
	}

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

void SDMXReadOnlyFixturePatchList::OnSearchTextChanged(const FText& SearchText)
{
	RequestListRefresh();
}

void SDMXReadOnlyFixturePatchList::OnEntityAddedOrRemoved(UDMXLibrary* InDMXLibrary, TArray<UDMXEntity*> Entities)
{
	if (InDMXLibrary == WeakDMXLibrary)
	{	
		// Requires forcing the refresh to hotfix for crases and bugs when deleting patches in use in the list.
		// Note, the workaround here is not ideal and should not be transported to future versions. A proper fix already exists in 5.4 with 27111707. 
		RefreshList();
	}
}

void SDMXReadOnlyFixturePatchList::OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch)
{
	// Refresh only if the fixture patch is in the library this editor handles
	if (FixturePatch && FixturePatch->GetParentLibrary() == WeakDMXLibrary)
	{
		RequestListRefresh();
	}
}

void SDMXReadOnlyFixturePatchList::OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType)
{
	// Refresh only if the fixture type is in the library this editor handles
	if (FixtureType && FixtureType->GetParentLibrary() == WeakDMXLibrary)
	{
		RequestListRefresh();
	}
}

TSharedRef<SHeaderRow> SDMXReadOnlyFixturePatchList::GenerateHeaderRow()
{
	HeaderRow = SNew(SHeaderRow);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXReadOnlyFixturePatchListCollumnIDs::EditorColor)
		.DefaultLabel(LOCTEXT("EditorColorColumnLabel", ""))
		.FixedWidth(16.f)
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXReadOnlyFixturePatchListCollumnIDs::FixturePatchName)
		.SortMode(this, &SDMXReadOnlyFixturePatchList::GetColumnSortMode, FDMXReadOnlyFixturePatchListCollumnIDs::FixturePatchName)
		.OnSort(this, &SDMXReadOnlyFixturePatchList::SortByColumnID)
		.FillWidth(0.25f)
		.VAlignHeader(VAlign_Center)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FixturePatchNameColumnLabel", "Name"))
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXReadOnlyFixturePatchListCollumnIDs::FixtureID)
		.SortMode(this, &SDMXReadOnlyFixturePatchList::GetColumnSortMode, FDMXReadOnlyFixturePatchListCollumnIDs::FixtureID)
		.OnSort(this, &SDMXReadOnlyFixturePatchList::SortByColumnID)
		.FillWidth(0.1f)
		.VAlignHeader(VAlign_Center)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FixtureIDColumnLabel", "FID"))
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXReadOnlyFixturePatchListCollumnIDs::FixtureType)
		.SortMode(this, &SDMXReadOnlyFixturePatchList::GetColumnSortMode, FDMXReadOnlyFixturePatchListCollumnIDs::FixtureType)
		.OnSort(this, &SDMXReadOnlyFixturePatchList::SortByColumnID)
		.FillWidth(0.2f)
		.VAlignHeader(VAlign_Center)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text((LOCTEXT("FixtureTypeColumnLabel", "Type")))
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXReadOnlyFixturePatchListCollumnIDs::Mode)
		.SortMode(this, &SDMXReadOnlyFixturePatchList::GetColumnSortMode, FDMXReadOnlyFixturePatchListCollumnIDs::Mode)
		.OnSort(this, &SDMXReadOnlyFixturePatchList::SortByColumnID)
		.FillWidth(0.2f)
		.VAlignHeader(VAlign_Center)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ModeColumnLabel", "Mode"))
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXReadOnlyFixturePatchListCollumnIDs::Patch)
		.SortMode(this, &SDMXReadOnlyFixturePatchList::GetColumnSortMode, FDMXReadOnlyFixturePatchListCollumnIDs::Patch)
		.OnSort(this, &SDMXReadOnlyFixturePatchList::SortByColumnID)
		.FillWidth(0.1f)
		.VAlignHeader(VAlign_Center)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PatchColumnLabel", "Patch"))
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	);

	return HeaderRow.ToSharedRef();
}

TSharedRef<SWidget> SDMXReadOnlyFixturePatchList::GenerateHeaderRowVisibilityMenu()
{
	FMenuBuilder MenuBuilder(false, nullptr);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("FixturePatchListShowPatchFilterSection", "Filter"));
	{
		auto AddMenuEntryLambda = [this, &MenuBuilder](const FText& Label, const FText& ToolTip, const EDMXReadOnlyFixturePatchListShowMode InShowMode)
		{
			MenuBuilder.AddMenuEntry(
				Label,
				ToolTip,
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SDMXReadOnlyFixturePatchList::SetShowMode, InShowMode),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SDMXReadOnlyFixturePatchList::IsUsingShowMode, InShowMode)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		};

		AddMenuEntryLambda(
			LOCTEXT("FixturePatchAllPatchesFilter_Label", "All Patches"),
			LOCTEXT("FixturePatchAllPatchesFilter", "Show all the Fixture Patches in the list"),
			EDMXReadOnlyFixturePatchListShowMode::All
		);

		AddMenuEntryLambda(
			LOCTEXT("FixturePatchActivePatchesFilter_Label", "Only Active"),
			LOCTEXT("FixturePatchActivePatchesFilter", "Show only active Fixture Patches in the list"),
			EDMXReadOnlyFixturePatchListShowMode::Active
		);

		AddMenuEntryLambda(
			LOCTEXT("FixturePatchInactivePatchesFilter_Label", "Only Inactive"),
			LOCTEXT("FixturePatchInactivePatchesFilter", "Show only inactive Fixture Patches in the list"),
			EDMXReadOnlyFixturePatchListShowMode::Inactive
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("FixturePatchListShowColumnSection", "Columns"));
	{
		auto AddMenuEntryLambda = [this, &MenuBuilder](const FText& Label, const FText& ToolTip, const FName& ColumnID)
		{
			MenuBuilder.AddMenuEntry(
				Label,
				ToolTip,
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SDMXReadOnlyFixturePatchList::ToggleColumnShowState, ColumnID),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SDMXReadOnlyFixturePatchList::IsColumnShown, ColumnID)
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		};

		AddMenuEntryLambda(
			LOCTEXT("FixturePatchNameColumn_Label", "Show Name"),
			LOCTEXT("FixturePatchLNameColumn_Tooltip", "Show/Hide Name Column"),
			FDMXReadOnlyFixturePatchListCollumnIDs::FixturePatchName
		);

		AddMenuEntryLambda(
			LOCTEXT("FixturePatchFIDColumn_Label", "Show FID"),
			LOCTEXT("FixturePatchFIDColumn_Tooltip", "Show/Hide FID Column"),
			FDMXReadOnlyFixturePatchListCollumnIDs::FixtureID
		);

		AddMenuEntryLambda(
			LOCTEXT("FixturePatchTypeColumn_Label", "Show Type"),
			LOCTEXT("FixturePatchTypeColumn_Tooltip", "Show/Hide Type Column"),
			FDMXReadOnlyFixturePatchListCollumnIDs::FixtureType
		);

		AddMenuEntryLambda(
			LOCTEXT("FixturePatchModeColumn_Label", "Show Mode"),
			LOCTEXT("FixturePatchModeColumn_Tooltip", "Show/Hide Mode Column"),
			FDMXReadOnlyFixturePatchListCollumnIDs::Mode
		);

		AddMenuEntryLambda(
			LOCTEXT("FixturePatchyColumn_Label", "Show Patch"),
			LOCTEXT("FixturePatchColumn_Tooltip", "Show/Hide Patch Column"),
			FDMXReadOnlyFixturePatchListCollumnIDs::Patch
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SDMXReadOnlyFixturePatchList::ToggleColumnShowState(const FName ColumnID)
{
	if (!HeaderRow.IsValid())
	{
		return;
	}

	const bool bShowState = ColumnIDToShowStateMap[ColumnID];
	ColumnIDToShowStateMap.Add(ColumnID) = !bShowState;

	HeaderRow->SetShowGeneratedColumn(ColumnID, !bShowState);
}

bool SDMXReadOnlyFixturePatchList::IsColumnShown(const FName ColumnID) const
{
	return ColumnIDToShowStateMap[ColumnID];
}

EColumnSortMode::Type SDMXReadOnlyFixturePatchList::GetColumnSortMode(const FName ColumnID) const
{
	if (SortedByColumnID != ColumnID)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}

void SDMXReadOnlyFixturePatchList::SortByColumnID(const EColumnSortPriority::Type SortPriority, const FName& ColumnID, const EColumnSortMode::Type InSortMode)
{
	SortMode = InSortMode;
	SortedByColumnID = ColumnID;

	const bool bAscending = InSortMode == EColumnSortMode::Ascending ? true : false;

	if (ColumnID == FDMXReadOnlyFixturePatchListCollumnIDs::FixturePatchName)
	{
		Algo::Sort(ListItems, [bAscending](const TSharedPtr<FDMXEntityFixturePatchRef>& ItemA, const TSharedPtr<FDMXEntityFixturePatchRef>& ItemB)
			{
				const UDMXEntityFixturePatch* FixturePatchA = ItemA->GetFixturePatch();
				const UDMXEntityFixturePatch* FixturePatchB = ItemB->GetFixturePatch();
				if (!FixturePatchA || !FixturePatchB)
				{
					return false;
				}

				const FString FixturePatchNameA = FixturePatchA->Name;
				const FString FixturePatchNameB = FixturePatchB->Name;

				const bool bIsGreater = FixturePatchNameA >= FixturePatchNameB;
				return bAscending ? !bIsGreater : bIsGreater;
			});
	}
	else if (ColumnID == FDMXReadOnlyFixturePatchListCollumnIDs::FixtureID)
	{
		Algo::Sort(ListItems, [bAscending](const TSharedPtr<FDMXEntityFixturePatchRef>& ItemA, const TSharedPtr<FDMXEntityFixturePatchRef>& ItemB)
			{
				bool bIsGreater = [ItemA, ItemB]()
				{
					const UDMXEntityFixturePatch* FixturePatchA = ItemA->GetFixturePatch();
					const UDMXEntityFixturePatch* FixturePatchB = ItemB->GetFixturePatch();
					if (!FixturePatchA || !FixturePatchB)
					{
						return false;
					}

					int32 FixtureIDA;
					int32 FixtureIDB;
					FixturePatchA->FindFixtureID(FixtureIDA);
					FixturePatchB->FindFixtureID(FixtureIDB);

					return FixtureIDA >= FixtureIDB;
				}();

				return bAscending ? !bIsGreater : bIsGreater;
			});
	}
	else if (ColumnID == FDMXReadOnlyFixturePatchListCollumnIDs::FixtureType)
	{
		Algo::Sort(ListItems, [bAscending](const TSharedPtr<FDMXEntityFixturePatchRef>& ItemA, const TSharedPtr<FDMXEntityFixturePatchRef>& ItemB)
			{
				const UDMXEntityFixturePatch* FixturePatchA = ItemA->GetFixturePatch();
				const UDMXEntityFixturePatch* FixturePatchB = ItemB->GetFixturePatch();
				if (!FixturePatchA || !FixturePatchB)
				{
					return false;
				}

				const FString FixtureTypeA = FixturePatchA->GetFixtureType()->Name;
				const FString FixtureTypeB = FixturePatchB->GetFixtureType()->Name;

				const bool bIsGreater = FixtureTypeA >= FixtureTypeB;
				return bAscending ? !bIsGreater : bIsGreater;
			});
	}
	else if (ColumnID == FDMXReadOnlyFixturePatchListCollumnIDs::Mode)
	{
		Algo::Sort(ListItems, [bAscending](const TSharedPtr<FDMXEntityFixturePatchRef>& ItemA, const TSharedPtr<FDMXEntityFixturePatchRef>& ItemB)
			{
				const UDMXEntityFixturePatch* FixturePatchA = ItemA->GetFixturePatch();
				const UDMXEntityFixturePatch* FixturePatchB = ItemB->GetFixturePatch();
				if (!FixturePatchA || !FixturePatchB)
				{
					return false;
				}

				const bool bIsGreater = FixturePatchA->GetActiveModeIndex() >= FixturePatchB->GetActiveModeIndex();
				return bAscending ? !bIsGreater : bIsGreater;
			});
	}
	else if (ColumnID == FDMXReadOnlyFixturePatchListCollumnIDs::Patch)
	{
		Algo::Sort(ListItems, [bAscending](const TSharedPtr<FDMXEntityFixturePatchRef>& ItemA, const TSharedPtr<FDMXEntityFixturePatchRef>& ItemB)
			{
				const UDMXEntityFixturePatch* FixturePatchA = ItemA->GetFixturePatch();
				const UDMXEntityFixturePatch* FixturePatchB = ItemB->GetFixturePatch();
				if (!FixturePatchA || !FixturePatchB)
				{
					return false;
				}

				const bool bIsUniverseIDGreater = FixturePatchA->GetUniverseID() > FixturePatchB->GetUniverseID();
				const bool bIsSameUniverse = FixturePatchA->GetUniverseID() == FixturePatchB->GetUniverseID();
				const bool bAreAddressesGreater = FixturePatchA->GetStartingChannel() > FixturePatchB->GetStartingChannel();

				const bool bIsGreater = bIsUniverseIDGreater || (bIsSameUniverse && bAreAddressesGreater);
				return bAscending ? !bIsGreater : bIsGreater;
			});
	}

	ListView->RequestListRefresh();
}

void SDMXReadOnlyFixturePatchList::SetShowMode(EDMXReadOnlyFixturePatchListShowMode NewShowMode)
{
	ShowMode = NewShowMode;
}

bool SDMXReadOnlyFixturePatchList::IsUsingShowMode(EDMXReadOnlyFixturePatchListShowMode ShowModeToCheck) const
{
	return ShowModeToCheck == ShowMode;
}

#undef LOCTEXT_NAMESPACE
