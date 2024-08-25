// Copyright Epic Games, Inc. All Rights Reserved.

#include "STimerTreeView.h"

#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SToolTip.h"

// Insights
#include "Insights/InsightsStyle.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/Table/ViewModels/TreeNodeSorting.h"
#include "Insights/TimingProfilerManager.h"
#include "Insights/ViewModels/TimerButterflyAggregation.h"
#include "Insights/ViewModels/TimersViewColumnFactory.h"
#include "Insights/Widgets/SAsyncOperationStatus.h"
#include "Insights/Widgets/STimersViewTooltip.h"
#include "Insights/Widgets/STimerTableRow.h"

#define LOCTEXT_NAMESPACE "STimerTreeView"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimerTreeViewCommands
////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimerTreeViewCommands : public TCommands<FTimerTreeViewCommands>
{
public:
	FTimerTreeViewCommands()
	: TCommands<FTimerTreeViewCommands>(
		TEXT("TimerTreeViewCommands"),
		NSLOCTEXT("Contexts", "TimerTreeViewCommands", "Insights - Timer Tree View"),
		NAME_None,
		FInsightsStyle::GetStyleSetName())
	{
	}

	virtual ~FTimerTreeViewCommands()
	{
	}

	// UI_COMMAND takes long for the compiler to optimize
	UE_DISABLE_OPTIMIZATION_SHIP
	virtual void RegisterCommands() override
	{
		UI_COMMAND(Command_CopyToClipboard,
			"Copy To Clipboard",
			"Copies the selection to clipboard.",
			EUserInterfaceActionType::Button,
			FInputChord(EModifierKey::Control, EKeys::C));
	}
	UE_ENABLE_OPTIMIZATION_SHIP

	TSharedPtr<FUICommandInfo> Command_CopyToClipboard;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// STimerTreeView
////////////////////////////////////////////////////////////////////////////////////////////////////

STimerTreeView::STimerTreeView()
	: Table(MakeShared<Insights::FTable>())
	, ViewName()
	, TreeView(nullptr)
	, TreeViewHeaderRow(nullptr)
	, ExternalScrollbar(nullptr)
	, HoveredColumnId()
	, HoveredNodePtr(nullptr)
	, HighlightedNodeName()
	, TreeNodes()
	, AvailableSorters()
	, CurrentSorter(nullptr)
	, ColumnBeingSorted(GetDefaultColumnBeingSorted())
	, ColumnSortMode(GetDefaultColumnSortMode())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

STimerTreeView::~STimerTreeView()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::InitCommandList()
{
	FTimerTreeViewCommands::Register();
	CommandList = MakeShared<FUICommandList>();
	CommandList->MapAction(FTimerTreeViewCommands::Get().Command_CopyToClipboard, FExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_CopySelectedToClipboard_Execute), FCanExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_CopySelectedToClipboard_CanExecute));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STimerTreeView::Construct(const FArguments& InArgs, const FText& InViewName)
{
	ViewName = InViewName;

	TSharedRef<Insights::FTimerButterflyAggregator> TimerButterflyAggregator = FTimingProfilerManager::Get()->GetTimerButterflyAggregator();

	SAssignNew(ExternalScrollbar, SScrollBar)
	.AlwaysShowScrollbar(true);

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0.0f)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(TreeView, STreeView<FTimerNodePtr>)
				.ExternalScrollbar(ExternalScrollbar)
				.SelectionMode(ESelectionMode::Multi)
				.TreeItemsSource(&TreeNodes)
				.OnGetChildren(this, &STimerTreeView::TreeView_OnGetChildren)
				.OnGenerateRow(this, &STimerTreeView::TreeView_OnGenerateRow)
				//.OnSelectionChanged(this, &STimerTreeView::TreeView_OnSelectionChanged)
				//.OnMouseButtonDoubleClick(this, &STimerTreeView::TreeView_OnMouseButtonDoubleClick)
				.OnContextMenuOpening(FOnContextMenuOpening::CreateSP(this, &STimerTreeView::TreeView_GetMenuContent))
				.ItemHeight(16.0f)
				.HeaderRow
				(
					SAssignNew(TreeViewHeaderRow, SHeaderRow)
					.Visibility(EVisibility::Visible)
				)
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(16.0f)
			[
				SAssignNew(AsyncOperationStatus, Insights::SAsyncOperationStatus, TimerButterflyAggregator)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f)
		[
			SNew(SBox)
			.WidthOverride(FOptionalSize(13.0f))
			[
				ExternalScrollbar.ToSharedRef()
			]
		]
	];

	InitializeAndShowHeaderColumns();

	CreateSortings();

	InitCommandList();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> STimerTreeView::TreeView_GetMenuContent()
{
	const TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();
	const int32 NumSelectedNodes = SelectedNodes.Num();
	FTimerNodePtr SelectedNode = NumSelectedNodes ? SelectedNodes[0] : nullptr;

	FText SelectionStr;
	if (NumSelectedNodes == 0)
	{
		SelectionStr = LOCTEXT("NothingSelected", "Nothing selected");
	}
	else if (NumSelectedNodes == 1)
	{
		FString ItemName = SelectedNode->GetName().ToString();
		const int32 MaxStringLen = 64;
		if (ItemName.Len() > MaxStringLen)
		{
			ItemName = ItemName.Left(MaxStringLen) + TEXT("...");
		}
		SelectionStr = FText::FromString(ItemName);
	}
	else
	{
		SelectionStr = FText::Format(LOCTEXT("MultipleSelection_Fmt", "{0} selected items"), FText::AsNumber(NumSelectedNodes));
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList.ToSharedRef());

	// Selection menu
	MenuBuilder.BeginSection("Selection", LOCTEXT("ContextMenu_Section_Selection", "Selection"));
	{
		struct FLocal
		{
			static bool ReturnFalse()
			{
				return false;
			}
		};

		FUIAction DummyUIAction;
		DummyUIAction.CanExecuteAction = FCanExecuteAction::CreateStatic(&FLocal::ReturnFalse);
		MenuBuilder.AddMenuEntry
		(
			SelectionStr,
			LOCTEXT("ContextMenu_Selection", "Currently selected items"),
			FSlateIcon(),
			DummyUIAction,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Misc", LOCTEXT("ContextMenu_Section_Misc", "Miscellaneous"));
	{
		MenuBuilder.AddMenuEntry
		(
			FTimerTreeViewCommands::Get().Command_CopyToClipboard,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "GenericCommands.Copy")
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::TreeView_BuildSortByMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("SortColumn", LOCTEXT("ContextMenu_Section_SortColumn", "Sort Column"));

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		if (Column.IsVisible() && Column.CanBeSorted())
		{
			FUIAction Action_SortByColumn
			(
				FExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_SortByColumn_Execute, Column.GetId()),
				FCanExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_SortByColumn_CanExecute, Column.GetId()),
				FIsActionChecked::CreateSP(this, &STimerTreeView::ContextMenu_SortByColumn_IsChecked, Column.GetId())
			);
			MenuBuilder.AddMenuEntry
			(
				Column.GetTitleName(),
				Column.GetDescription(),
				FSlateIcon(),
				Action_SortByColumn,
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}
	}

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("SortMode", LOCTEXT("ContextMenu_Section_SortMode", "Sort Mode"));
	{
		FUIAction Action_SortAscending
		(
			FExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_SortMode_Execute, EColumnSortMode::Ascending),
			FCanExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_SortMode_CanExecute, EColumnSortMode::Ascending),
			FIsActionChecked::CreateSP(this, &STimerTreeView::ContextMenu_SortMode_IsChecked, EColumnSortMode::Ascending)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_SortAscending", "Sort Ascending"),
			LOCTEXT("ContextMenu_SortAscending_Desc", "Sorts ascending."),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SortUp"),
			Action_SortAscending,
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		FUIAction Action_SortDescending
		(
			FExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_SortMode_Execute, EColumnSortMode::Descending),
			FCanExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_SortMode_CanExecute, EColumnSortMode::Descending),
			FIsActionChecked::CreateSP(this, &STimerTreeView::ContextMenu_SortMode_IsChecked, EColumnSortMode::Descending)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_SortDescending", "Sort Descending"),
			LOCTEXT("ContextMenu_SortDescending_Desc", "Sorts descending."),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SortDown"),
			Action_SortDescending,
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::TreeView_BuildViewColumnMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Columns", LOCTEXT("ContextMenu_Section_Columns", "Columns"));

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		FUIAction Action_ToggleColumn
		(
			FExecuteAction::CreateSP(this, &STimerTreeView::ToggleColumnVisibility, Column.GetId()),
			FCanExecuteAction::CreateSP(this, &STimerTreeView::CanToggleColumnVisibility, Column.GetId()),
			FIsActionChecked::CreateSP(this, &STimerTreeView::IsColumnVisible, Column.GetId())
		);
		MenuBuilder.AddMenuEntry
		(
			Column.GetTitleName(),
			Column.GetDescription(),
			FSlateIcon(),
			Action_ToggleColumn,
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}

	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::InitializeAndShowHeaderColumns()
{
	// Create columns.
	TArray<TSharedRef<Insights::FTableColumn>> Columns;
	FTimersViewColumnFactory::CreateTimerTreeViewColumns(Columns);
	if (ensure(Columns.Num() > 0 && Columns[0]->IsHierarchy()))
	{
		Columns[0]->SetShortName(ViewName);
		Columns[0]->SetTitleName(ViewName);
	}
	Table->SetColumns(Columns);

	// Show columns.
	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		if (ColumnRef->ShouldBeVisible())
		{
			ShowColumn(ColumnRef->GetId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STimerTreeView::GetColumnHeaderText(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.GetShortName();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STimerTreeView::TreeViewHeaderRow_GenerateColumnMenu(const Insights::FTableColumn& Column)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("Sorting", LOCTEXT("ContextMenu_Section_Sorting", "Sorting"));
	{
		if (Column.CanBeSorted())
		{
			FUIAction Action_SortAscending
			(
				FExecuteAction::CreateSP(this, &STimerTreeView::HeaderMenu_SortMode_Execute, Column.GetId(), EColumnSortMode::Ascending),
				FCanExecuteAction::CreateSP(this, &STimerTreeView::HeaderMenu_SortMode_CanExecute, Column.GetId(), EColumnSortMode::Ascending),
				FIsActionChecked::CreateSP(this, &STimerTreeView::HeaderMenu_SortMode_IsChecked, Column.GetId(), EColumnSortMode::Ascending)
			);
			MenuBuilder.AddMenuEntry
			(
				FText::Format(LOCTEXT("ContextMenu_SortAscending_Fmt", "Sort Ascending (by {0})"), Column.GetTitleName()),
				FText::Format(LOCTEXT("ContextMenu_SortAscending_Desc_Fmt", "Sorts ascending by {0}."), Column.GetTitleName()),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SortUp"),
				Action_SortAscending,
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);

			FUIAction Action_SortDescending
			(
				FExecuteAction::CreateSP(this, &STimerTreeView::HeaderMenu_SortMode_Execute, Column.GetId(), EColumnSortMode::Descending),
				FCanExecuteAction::CreateSP(this, &STimerTreeView::HeaderMenu_SortMode_CanExecute, Column.GetId(), EColumnSortMode::Descending),
				FIsActionChecked::CreateSP(this, &STimerTreeView::HeaderMenu_SortMode_IsChecked, Column.GetId(), EColumnSortMode::Descending)
			);
			MenuBuilder.AddMenuEntry
			(
				FText::Format(LOCTEXT("ContextMenu_SortDescending_Fmt", "Sort Descending (by {0})"), Column.GetTitleName()),
				FText::Format(LOCTEXT("ContextMenu_SortDescending_Desc_Fmt", "Sorts descending by {0}."), Column.GetTitleName()),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.SortDown"),
				Action_SortDescending,
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_SortBy_SubMenu", "Sort By"),
			LOCTEXT("ContextMenu_SortBy_SubMenu_Desc", "Sorts by a column."),
			FNewMenuDelegate::CreateSP(this, &STimerTreeView::TreeView_BuildSortByMenu),
			false,
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.SortBy")
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ColumnVisibility", LOCTEXT("ContextMenu_Section_ColumnVisibility", "Column Visibility"));
	{
		if (Column.CanBeHidden())
		{
			FUIAction Action_HideColumn
			(
				FExecuteAction::CreateSP(this, &STimerTreeView::HideColumn, Column.GetId()),
				FCanExecuteAction::CreateSP(this, &STimerTreeView::CanHideColumn, Column.GetId())
			);
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ContextMenu_HideColumn", "Hide"),
				LOCTEXT("ContextMenu_HideColumn_Desc", "Hides the selected column."),
				FSlateIcon(),
				Action_HideColumn,
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}

		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_ViewColumn_SubMenu", "View Column"),
			LOCTEXT("ContextMenu_ViewColumn_SubMenu_Desc", "Hides or shows columns."),
			FNewMenuDelegate::CreateSP(this, &STimerTreeView::TreeView_BuildViewColumnMenu),
			false,
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ViewColumn")
		);

		FUIAction Action_ShowAllColumns
		(
			FExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_ShowAllColumns_Execute),
			FCanExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_ShowAllColumns_CanExecute)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_ShowAllColumns", "Show All Columns"),
			LOCTEXT("ContextMenu_ShowAllColumns_Desc", "Resets tree view to show all columns."),
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ResetColumn"),
			Action_ShowAllColumns,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		FUIAction Action_ResetColumns
		(
			FExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_ResetColumns_Execute),
			FCanExecuteAction::CreateSP(this, &STimerTreeView::ContextMenu_ResetColumns_CanExecute)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_ResetColumns", "Reset Columns to Default"),
			LOCTEXT("ContextMenu_ResetColumns_Desc", "Resets columns to default."),
			FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ResetColumn"),
			Action_ResetColumns,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeView
////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::TreeView_Refresh()
{
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::TreeView_OnSelectionChanged(FTimerNodePtr SelectedItem, ESelectInfo::Type SelectInfo)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::TreeView_OnGetChildren(FTimerNodePtr InParent, TArray<FTimerNodePtr>& OutChildren)
{
	constexpr bool bUseFiltering = false;
	if (bUseFiltering)
	{
		const TArray<Insights::FBaseTreeNodePtr>& Children = InParent->GetFilteredChildren();
		OutChildren.Reset(Children.Num());
		for (const Insights::FBaseTreeNodePtr& Child : Children)
		{
			OutChildren.Add(StaticCastSharedPtr<FTimerNode, Insights::FBaseTreeNode>(Child));
		}
	}
	else
	{
		const TArray<Insights::FBaseTreeNodePtr>& Children = InParent->GetChildren();
		OutChildren.Reset(Children.Num());
		for (const Insights::FBaseTreeNodePtr& Child : Children)
		{
			OutChildren.Add(StaticCastSharedPtr<FTimerNode, Insights::FBaseTreeNode>(Child));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::TreeView_OnMouseButtonDoubleClick(FTimerNodePtr TimerNodePtr)
{
	//if (TimerNodePtr->IsGroup())
	if (TimerNodePtr->GetChildrenCount() > 0)
	{
		const bool bIsGroupExpanded = TreeView->IsItemExpanded(TimerNodePtr);
		TreeView->SetItemExpansion(TimerNodePtr, !bIsGroupExpanded);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tree View's Table Row
////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> STimerTreeView::TreeView_OnGenerateRow(FTimerNodePtr TimerNodePtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> TableRow =
		SNew(STimerTableRow, OwnerTable)
		.OnShouldBeEnabled(this, &STimerTreeView::TableRow_ShouldBeEnabled)
		.OnIsColumnVisible(this, &STimerTreeView::IsColumnVisible)
		.OnSetHoveredCell(this, &STimerTreeView::TableRow_SetHoveredCell)
		.OnGetColumnOutlineHAlignmentDelegate(this, &STimerTreeView::TableRow_GetColumnOutlineHAlignment)
		.HighlightText(this, &STimerTreeView::TableRow_GetHighlightText)
		.HighlightedNodeName(this, &STimerTreeView::TableRow_GetHighlightedNodeName)
		.TablePtr(Table)
		.TimerNodePtr(TimerNodePtr);

	return TableRow;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::TableRow_ShouldBeEnabled(FTimerNodePtr NodePtr) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::TableRow_SetHoveredCell(TSharedPtr<Insights::FTable> InTablePtr, TSharedPtr<Insights::FTableColumn> InColumnPtr, FTimerNodePtr InNodePtr)
{
	HoveredColumnId = InColumnPtr ? InColumnPtr->GetId() : FName();

	const bool bIsAnyMenusVisible = FSlateApplication::Get().AnyMenusVisible();
	if (!HasMouseCapture() && !bIsAnyMenusVisible)
	{
		HoveredNodePtr = InNodePtr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EHorizontalAlignment STimerTreeView::TableRow_GetColumnOutlineHAlignment(const FName ColumnId) const
{
	const TIndirectArray<SHeaderRow::FColumn>& Columns = TreeViewHeaderRow->GetColumns();
	const int32 LastColumnIdx = Columns.Num() - 1;

	// First column
	if (Columns[0].ColumnId == ColumnId)
	{
		return HAlign_Left;
	}
	// Last column
	else if (Columns[LastColumnIdx].ColumnId == ColumnId)
	{
		return HAlign_Right;
	}
	// Middle columns
	{
		return HAlign_Center;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STimerTreeView::TableRow_GetHighlightText() const
{
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName STimerTreeView::TableRow_GetHighlightedNodeName() const
{
	return HighlightedNodeName;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorting
////////////////////////////////////////////////////////////////////////////////////////////////////

const FName STimerTreeView::GetDefaultColumnBeingSorted()
{
	return FTimersViewColumns::TotalInclusiveTimeColumnID;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const EColumnSortMode::Type STimerTreeView::GetDefaultColumnSortMode()
{
	return EColumnSortMode::Descending;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::CreateSortings()
{
	AvailableSorters.Reset();
	CurrentSorter = nullptr;

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		if (ColumnRef->CanBeSorted())
		{
			TSharedPtr<Insights::ITableCellValueSorter> SorterPtr = ColumnRef->GetValueSorter();
			if (ensure(SorterPtr.IsValid()))
			{
				AvailableSorters.Add(SorterPtr);
			}
		}
	}

	UpdateCurrentSortingByColumn();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::UpdateCurrentSortingByColumn()
{
	TSharedPtr<Insights::FTableColumn> ColumnPtr = Table->FindColumn(ColumnBeingSorted);
	CurrentSorter = ColumnPtr.IsValid() ? ColumnPtr->GetValueSorter() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::SortTreeNodes()
{
	if (CurrentSorter.IsValid())
	{
		for (FTimerNodePtr& Root : TreeNodes)
		{
			SortTreeNodesRec(*Root, *CurrentSorter);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::SortTreeNodesRec(FTimerNode& Node, const Insights::ITableCellValueSorter& Sorter)
{
	Insights::ESortMode SortMode = (ColumnSortMode == EColumnSortMode::Type::Descending) ? Insights::ESortMode::Descending : Insights::ESortMode::Ascending;
	Node.SortChildren(Sorter, SortMode);

	for (Insights::FBaseTreeNodePtr ChildPtr : Node.GetChildren())
	{
		//if (ChildPtr->IsGroup())
		if (ChildPtr->GetChildrenCount() > 0)
		{
			SortTreeNodesRec(*StaticCastSharedPtr<FTimerNode>(ChildPtr), Sorter);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EColumnSortMode::Type STimerTreeView::GetSortModeForColumn(const FName ColumnId) const
{
	if (ColumnBeingSorted != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return ColumnSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::SetSortModeForColumn(const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	ColumnBeingSorted = ColumnId;
	ColumnSortMode = SortMode;
	UpdateCurrentSortingByColumn();

	SortTreeNodes();
	//ApplyFiltering();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	SetSortModeForColumn(ColumnId, SortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortMode action (HeaderMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::HeaderMenu_SortMode_IsChecked(const FName ColumnId, const EColumnSortMode::Type InSortMode)
{
	return ColumnBeingSorted == ColumnId && ColumnSortMode == InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::HeaderMenu_SortMode_CanExecute(const FName ColumnId, const EColumnSortMode::Type InSortMode) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.CanBeSorted();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::HeaderMenu_SortMode_Execute(const FName ColumnId, const EColumnSortMode::Type InSortMode)
{
	SetSortModeForColumn(ColumnId, InSortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortMode action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::ContextMenu_SortMode_IsChecked(const EColumnSortMode::Type InSortMode)
{
	return ColumnSortMode == InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::ContextMenu_SortMode_CanExecute(const EColumnSortMode::Type InSortMode) const
{
	return true; //ColumnSortMode != InSortMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::ContextMenu_SortMode_Execute(const EColumnSortMode::Type InSortMode)
{
	SetSortModeForColumn(ColumnBeingSorted, InSortMode);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SortByColumn action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::ContextMenu_SortByColumn_IsChecked(const FName ColumnId)
{
	return ColumnId == ColumnBeingSorted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::ContextMenu_SortByColumn_CanExecute(const FName ColumnId) const
{
	return true; //ColumnId != ColumnBeingSorted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::ContextMenu_SortByColumn_Execute(const FName ColumnId)
{
	SetSortModeForColumn(ColumnId, EColumnSortMode::Descending);
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ShowColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::CanShowColumn(const FName ColumnId) const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::ShowColumn(const FName ColumnId)
{
	Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	Column.Show();

	SHeaderRow::FColumn::FArguments ColumnArgs;
	ColumnArgs
		.ColumnId(Column.GetId())
		.DefaultLabel(Column.GetShortName())
		.ToolTip(STimersViewTooltip::GetColumnTooltipForMode(Column, ETraceFrameType::TraceFrameType_Count))
		.HAlignHeader(Column.GetHorizontalAlignment())
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Fill)
		.VAlignCell(VAlign_Fill)
		.InitialSortMode(Column.GetInitialSortMode())
		.SortMode(this, &STimerTreeView::GetSortModeForColumn, Column.GetId())
		.OnSort(this, &STimerTreeView::OnSortModeChanged)
		.FillWidth(Column.GetInitialWidth())
		//.FixedWidth(Column.IsFixedWidth() ? Column.GetInitialWidth() : TOptional<float>())
		.HeaderContent()
		[
			SNew(SBox)
			.HeightOverride(24.0f)
			.Padding(FMargin(0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &STimerTreeView::GetColumnHeaderText, Column.GetId())
			]
		]
		.MenuContent()
		[
			TreeViewHeaderRow_GenerateColumnMenu(Column)
		];

	int32 ColumnIndex = 0;
	const int32 NewColumnPosition = Table->GetColumnPositionIndex(ColumnId);
	const int32 NumColumns = TreeViewHeaderRow->GetColumns().Num();
	for (; ColumnIndex < NumColumns; ColumnIndex++)
	{
		const SHeaderRow::FColumn& CurrentColumn = TreeViewHeaderRow->GetColumns()[ColumnIndex];
		const int32 CurrentColumnPosition = Table->GetColumnPositionIndex(CurrentColumn.ColumnId);
		if (NewColumnPosition < CurrentColumnPosition)
		{
			break;
		}
	}

	TreeViewHeaderRow->InsertColumn(ColumnArgs, ColumnIndex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// HideColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::CanHideColumn(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.CanBeHidden();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::HideColumn(const FName ColumnId)
{
	Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	Column.Hide();

	TreeViewHeaderRow->RemoveColumn(ColumnId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ToggleColumn action
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::IsColumnVisible(const FName ColumnId)
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return Column.IsVisible();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::CanToggleColumnVisibility(const FName ColumnId) const
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	return !Column.IsVisible() || Column.CanBeHidden();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::ToggleColumnVisibility(const FName ColumnId)
{
	const Insights::FTableColumn& Column = *Table->FindColumnChecked(ColumnId);
	if (Column.IsVisible())
	{
		HideColumn(ColumnId);
	}
	else
	{
		ShowColumn(ColumnId);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// "Show All Columns" action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::ContextMenu_ShowAllColumns_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::ContextMenu_ShowAllColumns_Execute()
{
	ColumnBeingSorted = GetDefaultColumnBeingSorted();
	ColumnSortMode = GetDefaultColumnSortMode();
	UpdateCurrentSortingByColumn();

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		if (!Column.IsVisible())
		{
			ShowColumn(Column.GetId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ResetColumns action (ContextMenu)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::ContextMenu_ResetColumns_CanExecute() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::ContextMenu_ResetColumns_Execute()
{
	ColumnBeingSorted = GetDefaultColumnBeingSorted();
	ColumnSortMode = GetDefaultColumnSortMode();
	UpdateCurrentSortingByColumn();

	for (const TSharedRef<Insights::FTableColumn>& ColumnRef : Table->GetColumns())
	{
		const Insights::FTableColumn& Column = *ColumnRef;

		if (Column.ShouldBeVisible() && !Column.IsVisible())
		{
			ShowColumn(Column.GetId());
		}
		else if (!Column.ShouldBeVisible() && Column.IsVisible())
		{
			HideColumn(Column.GetId());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::Reset()
{
	TreeNodes.Reset();
	TreeView_Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::SetTree(const TraceServices::FTimingProfilerButterflyNode& Root)
{
	TreeNodes.Reset();

	FTimerNodePtr RootTimerNodePtr = CreateTimerNodeRec(Root);
	if (RootTimerNodePtr)
	{
		// Mark the hot path. The child nodes are already sorted by InclTime (descending), so we just follow the first child.
		FTimerNodePtr TimerNodePtr = RootTimerNodePtr;
		while (TimerNodePtr.IsValid())
		{
			TimerNodePtr->SetIsHotPath(true);
			const TArray<Insights::FBaseTreeNodePtr>& Children = TimerNodePtr->GetChildren();
			TimerNodePtr = Children.Num() > 0 ? StaticCastSharedPtr<FTimerNode, Insights::FBaseTreeNode>(Children[0]) : nullptr;
		}

		TreeNodes.Add(RootTimerNodePtr);
	}

	SortTreeNodes();

	TreeView_Refresh();

	if (RootTimerNodePtr)
	{
		ExpandNodesRec(RootTimerNodePtr, 0);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimerNodePtr STimerTreeView::CreateTimerNodeRec(const TraceServices::FTimingProfilerButterflyNode& Node)
{
	if (Node.Timer == nullptr)
	{
		return nullptr;
		//return MakeShared<FTimerNode>(0, TEXT("!!!!!"), ETimerNodeType::InvalidOrMax, true);
	}

	const ETimerNodeType Type = Node.Timer->IsGpuTimer ? ETimerNodeType::GpuScope : ETimerNodeType::CpuScope;
	FTimerNodePtr TimerNodePtr = MakeShared<FTimerNode>(Node.Timer->Id, Node.Timer->Name, Type, true);

	TraceServices::FTimingProfilerAggregatedStats AggregatedStats;
	AggregatedStats.InstanceCount = Node.Count;
	AggregatedStats.TotalInclusiveTime = Node.InclusiveTime;
	AggregatedStats.TotalExclusiveTime = Node.ExclusiveTime;
	AggregatedStats.AverageInclusiveTime = Node.Count != 0 ? Node.InclusiveTime / (double)Node.Count : 0.0;
	AggregatedStats.AverageExclusiveTime = Node.Count != 0 ? Node.ExclusiveTime / (double)Node.Count : 0.0;
	constexpr double NanTimeValue = std::numeric_limits<double>::quiet_NaN();
	AggregatedStats.MinInclusiveTime = NanTimeValue;
	AggregatedStats.MinExclusiveTime = NanTimeValue;
	AggregatedStats.MaxInclusiveTime = NanTimeValue;
	AggregatedStats.MaxExclusiveTime = NanTimeValue;
	AggregatedStats.MedianInclusiveTime = NanTimeValue;
	AggregatedStats.MedianExclusiveTime = NanTimeValue;
	TimerNodePtr->SetAggregatedStats(AggregatedStats);

	for (const TraceServices::FTimingProfilerButterflyNode* ChildNodePtr : Node.Children)
	{
		if (ChildNodePtr != nullptr)
		{
			FTimerNodePtr ChildTimerNodePtr = CreateTimerNodeRec(*ChildNodePtr);
			if (ChildTimerNodePtr)
			{
				TimerNodePtr->AddChildAndSetParent(ChildTimerNodePtr);
			}
		}
	}

	// Sort children by InclTime (descending).
	TimerNodePtr->SortChildren([](const Insights::FBaseTreeNodePtr& A, const Insights::FBaseTreeNodePtr& B) -> bool
	{
		const double InclTimeA = StaticCastSharedPtr<FTimerNode>(A)->GetAggregatedStats().TotalInclusiveTime;
		const double InclTimeB = StaticCastSharedPtr<FTimerNode>(B)->GetAggregatedStats().TotalInclusiveTime;
		return InclTimeA >= InclTimeB;
	});

	return TimerNodePtr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::ExpandNodesRec(FTimerNodePtr NodePtr, int32 Depth)
{
	//constexpr int32 MaxDepth = 3;

	TreeView->SetItemExpansion(NodePtr, NodePtr->IsHotPath()); // expand only the hot path

	//if (Depth < MaxDepth)
	{
		for (const Insights::FBaseTreeNodePtr& ChildPtr : NodePtr->GetChildren())
		{
			ExpandNodesRec(StaticCastSharedPtr<FTimerNode>(ChildPtr), Depth + 1);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTreeView::ContextMenu_CopySelectedToClipboard_CanExecute() const
{
	const TArray<FTimerNodePtr> SelectedNodes = TreeView->GetSelectedItems();

	return SelectedNodes.Num() > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTreeView::ContextMenu_CopySelectedToClipboard_Execute()
{
	if (!Table->IsValid())
	{
		return;
	}

	TArray<Insights::FBaseTreeNodePtr> SelectedNodes;
	for (FTimerNodePtr TimerPtr : TreeView->GetSelectedItems())
	{
		SelectedNodes.Add(TimerPtr);
	}

	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	FString ClipboardText;

	if (CurrentSorter.IsValid())
	{
		CurrentSorter->Sort(SelectedNodes, ColumnSortMode == EColumnSortMode::Ascending ? Insights::ESortMode::Ascending : Insights::ESortMode::Descending);
	}

	Table->GetVisibleColumnsData(SelectedNodes, FTimingProfilerManager::Get()->GetLogListingName(), TEXT('\t'), true, ClipboardText);

	if (ClipboardText.Len() > 0)
	{
		FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply STimerTreeView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return CommandList->ProcessCommandBindings(InKeyEvent) == true ? FReply::Handled() : FReply::Unhandled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
