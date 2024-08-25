// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphProfilingView.h"

#include "PCGComponent.h"
#include "Graph/PCGStackContext.h"

#include "PCGEditor.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNode.h"

#include "Framework/Views/TableViewMetadata.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphProfilingView"

namespace PCGEditorGraphProfilingView
{
	const FText NoDataAvailableText = LOCTEXT("NoDataAvailableText", "N/A");
	
	/** Names of the columns in the attribute list */
	const FName NAME_Node = FName(TEXT("Node"));
	const FName NAME_PrepareDataTime = FName(TEXT("PrepareDataTime"));
	const FName NAME_PrepareDataWallTime = FName(TEXT("PrepareData_WallTime"));
	const FName NAME_NbPrepareFrames = FName(TEXT("NbPrepareFrames"));
	const FName NAME_MinExecutionFrameTime = FName(TEXT("MinFrameTime"));
	const FName NAME_MaxExecutionFrameTime = FName(TEXT("MaxFrameTime"));
	const FName NAME_ExecutionTime = FName(TEXT("ExecutionTime"));
	const FName NAME_ExecutionWallTime = FName(TEXT("Execution_WallTime"));
	const FName NAME_NbExecutionFrames = FName(TEXT("NbExecutionFrames"));
	const FName NAME_TotalTime = FName(TEXT("TotalTime"));
	const FName NAME_TotalWallTime = FName(TEXT("Total_WallTime"));

	/** Labels of the columns */
	const FText TEXT_NodeLabel = LOCTEXT("NodeLabel", "Node");
	const FText TEXT_PrepareDataTimeLabel = LOCTEXT("PrepareDataTimeLabel", "Prepare");
	const FText TEXT_PrepareDataWallTimeLabel = LOCTEXT("PrepareDataWallTimeLabel", "Prepare WallTime");
	const FText TEXT_NbPrepareFramesLabel = LOCTEXT("NbPrepareFramesLabel", "Prep Frames");
	const FText TEXT_MinExecutionFrameTimeLabel = LOCTEXT("MinExecutionFrameTimeLabel", "Min FrameTime");
	const FText TEXT_MaxExecutionFrameTimeLabel = LOCTEXT("MaxExecutionFrameTimeLabel", "Max FrameTime");
	const FText TEXT_ExecutionTimeLabel = LOCTEXT("ExecutionTimeLabel", "Exec");
	const FText TEXT_ExecutionWallTimeLabel = LOCTEXT("ExecutionWallTimeLabel", "Exec WallTime");
	const FText TEXT_NbExecutionFramesLabel = LOCTEXT("NbExecutionFramesLabel", "Exec Frames");
	const FText TEXT_TotalTimeLabel = LOCTEXT("TotalTimeLabel", "Total");
	const FText TEXT_TotalWallTimeLabel = LOCTEXT("TotalWallTimeLabel", "Total WallTime");

	/** Tooltips */
	const FText TEXT_PrepareDataTimeTooltip = LOCTEXT("PrepareDataTimeTooltip", "Cost of the PrepareData execution phase which some nodes use to process the incoming data, in ms.");
	const FText TEXT_PrepareDataWallTimeTooltip = LOCTEXT("PrepareDataWallTimeTooltip", "Total real time elapsed between prepare data first being called until completion, including any wait/sleep time, in ms.");
	const FText TEXT_NbPrepareFramesTooltip = LOCTEXT("NbPrepareFramesTooltip", "The number of frames in which one or more prepare data phases were executed.");
	const FText TEXT_MinExecutionFrameTimeTooltip = LOCTEXT("MinExecutionFrameTimeTooltip", "The minimum time spent of all execution frames, in ms.");
	const FText TEXT_MaxExecutionFrameTimeTooltip = LOCTEXT("MaxExecutionFrameTimeTooltip", "The maximum time spent of all execution frames, in ms.");
	const FText TEXT_ExecutionTimeTooltip = LOCTEXT("ExecutionTimeTooltip", "The total time spent for execution, summed over all execution frames, in ms.");
	const FText TEXT_ExecutionWallTimeTooltip = LOCTEXT("ExecutionWallTimeTooltip", "Total real time elapsed between execute first being called until completion, including any wait/sleep time, in ms.");
	const FText TEXT_NbExecutionFramesTooltip = LOCTEXT("NbExecutionFramesTooltip", "The number of frames in which one or more execution phases were executed.");
	const FText TEXT_TotalTimeTooltip = LOCTEXT("TotalTimeTooltip", "The total time spent in this node, summed over all execution and prepare data frames, in ms.");
	const FText TEXT_TotalWallTimeTooltip = LOCTEXT("TotalWallTimeTooltip", "Total real time elapsed between the first call until completion, including any wait/sleep time, in ms.");
}

void SPCGProfilingListViewItemRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const PCGProfilingListViewItemPtr& Item)
{
	InternalItem = Item;

	SMultiColumnTableRow<PCGProfilingListViewItemPtr>::Construct(
		SMultiColumnTableRow::FArguments()
		.Style(FAppStyle::Get(), "DataTableEditor.CellListViewRow"),
		InOwnerTableView);
}

FText FPCGProfilingListViewItem::GetTextForColumn(FName ColumnId, bool bNoGrouping) const
{
	const FNumberFormattingOptions* NumberFormattingOptions = (bNoGrouping ? &FNumberFormattingOptions::DefaultNoGrouping() : nullptr);

	if (ColumnId == PCGEditorGraphProfilingView::NAME_Node)
	{
		if (bHasData)
		{
			return FText::FromString(Name);
		}
		else
		{
			return FText::FromString(Name + TEXT(" (cached)"));
		}
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_NbExecutionFrames)
	{
		return ((CallTime.ExecutionFrameCount >= 0) ? FText::AsNumber(CallTime.ExecutionFrameCount, NumberFormattingOptions) : FText());
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_NbPrepareFrames)
	{
		return ((CallTime.PrepareDataFrameCount >= 0) ? FText::AsNumber(CallTime.PrepareDataFrameCount, NumberFormattingOptions) : FText());
	}
	else if (!bHasData)
	{
		return PCGEditorGraphProfilingView::NoDataAvailableText;
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_MinExecutionFrameTime)
	{
		// In ms
		return ((CallTime.MinExecutionFrameTime >= 0) ? FText::AsNumber(CallTime.MinExecutionFrameTime * 1000.0, NumberFormattingOptions) : FText());
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_MaxExecutionFrameTime)
	{
		// In ms
		return ((CallTime.MaxExecutionFrameTime >= 0) ? FText::AsNumber(CallTime.MaxExecutionFrameTime * 1000.0, NumberFormattingOptions) : FText());
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_ExecutionTime)
	{
		// In ms
		return FText::AsNumber(CallTime.ExecutionTime * 1000.0, NumberFormattingOptions);
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_ExecutionWallTime)
	{
		// In ms
		return FText::AsNumber(CallTime.ExecutionWallTime() * 1000.0, NumberFormattingOptions);
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_PrepareDataTime)
	{
		// In ms
		return FText::AsNumber(CallTime.PrepareDataTime * 1000.0, NumberFormattingOptions);
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_PrepareDataWallTime)
	{
		// In ms
		return FText::AsNumber(CallTime.PrepareDataWallTime() * 1000.0, NumberFormattingOptions);
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_TotalTime)
	{
		// In ms
		return FText::AsNumber((CallTime.TotalTime()) * 1000.0, NumberFormattingOptions);
	}
	else if (ColumnId == PCGEditorGraphProfilingView::NAME_TotalWallTime)
	{
		// In ms
		return FText::AsNumber((CallTime.TotalWallTime()) * 1000.0, NumberFormattingOptions);
	}
	else
	{
		return LOCTEXT("ItemColumnError", "Unrecognized Column");
	}
}

TSharedRef<SWidget> SPCGProfilingListViewItemRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	FText ColumnData = LOCTEXT("ColumnError", "Unrecognized Column");
	FColor ColumnDataColor = FColor::White;

	if (InternalItem.IsValid())
	{
		ColumnData = InternalItem->GetTextForColumn(ColumnId, /*bNoGrouping=*/false);

		if (!InternalItem->bHasData)
		{
			ColumnDataColor = FColor(75, 75, 75);
		}
	}

	TSharedRef<SWidget> DataWidget = 
		SNew(STextBlock)
		.Text(ColumnData)
		.ColorAndOpacity(ColumnDataColor)
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis);

	// Add the internal name of the node as tooltip
	if (ColumnId == PCGEditorGraphProfilingView::NAME_Node && InternalItem->PCGNode)
	{
		DataWidget->SetToolTipText(FText::FromString(InternalItem->PCGNode->GetName()));
	}

	return DataWidget;
}

void SPCGEditorGraphProfilingView::OnItemDoubleClicked(PCGProfilingListViewItemPtr Item)
{
	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	if (!PCGEditor.IsValid())
	{
		return;
	}

	if (!Item.IsValid() || !Item->EditorNode)
	{
		return;
	}

	PCGEditor->JumpToNode(Item->EditorNode);
}

SPCGEditorGraphProfilingView::~SPCGEditorGraphProfilingView()
{
	if (PCGEditorPtr.IsValid())
	{
		PCGEditorPtr.Pin()->OnInspectedStackChangedDelegate.RemoveAll(this);
	}
}

void SPCGEditorGraphProfilingView::Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor)
{
	PCGEditorPtr = InPCGEditor;

	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	if (PCGEditor)
	{
		PCGEditorGraph = PCGEditor->GetPCGEditorGraph();
		PCGComponent = PCGEditor->GetPCGComponentBeingInspected();

		PCGEditor->OnInspectedStackChangedDelegate.AddSP(this, &SPCGEditorGraphProfilingView::OnDebugStackChanged);
	}

	SortingColumn = PCGEditorGraphProfilingView::NAME_TotalTime;
	SortMode = EColumnSortMode::Descending;
	ListViewHeader = CreateHeaderRowWidget();

	ListViewCommands = MakeShareable(new FUICommandList);
	ListViewCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &SPCGEditorGraphProfilingView::CopySelectionToClipboard),
		FCanExecuteAction::CreateSP(this, &SPCGEditorGraphProfilingView::CanCopySelectionToClipboard));

	const TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));
	
	SAssignNew(ListView, SListView<PCGProfilingListViewItemPtr>)
		.ListItemsSource(&ListViewItems)
		.HeaderRow(ListViewHeader)
		.OnGenerateRow(this, &SPCGEditorGraphProfilingView::OnGenerateRow)
		.OnMouseButtonDoubleClick(this, &SPCGEditorGraphProfilingView::OnItemDoubleClicked)
		.AllowOverscroll(EAllowOverscroll::No)
		.ExternalScrollbar(VerticalScrollBar)
		.OnKeyDownHandler(this, &SPCGEditorGraphProfilingView::OnListViewKeyDown)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always);
	
	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("PCGGraphSearchHint", "Enter text to find nodes..."))
				.OnTextChanged(this, &SPCGEditorGraphProfilingView::OnSearchTextChanged)
				.OnTextCommitted(this, &SPCGEditorGraphProfilingView::OnSearchTextCommitted)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(1.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ExpandSubgraphDepth", "Expand Subgraph Depth"))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(1.0f, 0.0f)
			[
				SNew(SSpinBox<int32>)
				.Value(this, &SPCGEditorGraphProfilingView::GetSubgraphExpandDepth)
				.OnValueChanged(this, &SPCGEditorGraphProfilingView::OnSubgraphExpandDepthChanged)
				.MinValue(0)
				.MaxValue(20)
			]
			+SHorizontalBox::Slot()
			.Padding(FMargin(30.0f, 0.0f, 2.0f, 0.0f))
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TotalExecutionTime", "Total Time:"))
			]
			+SHorizontalBox::Slot()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &SPCGEditorGraphProfilingView::GetTotalTimeLabel)
				.MinDesiredWidth(50.0f)
			]
			+SHorizontalBox::Slot()
			.Padding(FMargin(30.0f, 0.0f, 2.0f, 0.0f))
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TotalExecutionWallTime", "Total Wall Time:"))
			]
			+SHorizontalBox::Slot()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &SPCGEditorGraphProfilingView::GetTotalWallTimeLabel)
				.MinDesiredWidth(50.0f)
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				.ExternalScrollbar(HorizontalScrollBar)
				+SScrollBox::Slot()
				[
					ListView->AsShared()
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				HorizontalScrollBar
			]
		]
	];

	Refresh();
}

void SPCGEditorGraphProfilingView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bNeedsRefresh)
	{
		bNeedsRefresh = false;
		Refresh();
	}
}

TSharedRef<SHeaderRow> SPCGEditorGraphProfilingView::CreateHeaderRowWidget()
{
	return SNew(SHeaderRow)
		.ResizeMode(ESplitterResizeMode::FixedPosition)
		.CanSelectGeneratedColumn(true)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_Node)
		.ManualWidth(300.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_NodeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Left)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_Node)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_PrepareDataTime)
		.ManualWidth(75.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_PrepareDataTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_PrepareDataTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_PrepareDataTimeTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_PrepareDataWallTime)
		.ManualWidth(110.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_PrepareDataWallTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_PrepareDataWallTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_PrepareDataWallTimeTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_NbPrepareFrames)
		.ManualWidth(90.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_NbPrepareFramesLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_NbPrepareFrames)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_NbPrepareFramesTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_NbExecutionFrames)
		.ManualWidth(90.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_NbExecutionFramesLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_NbExecutionFrames)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_NbExecutionFramesTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_MinExecutionFrameTime)
		.ManualWidth(110.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_MinExecutionFrameTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_MinExecutionFrameTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_MinExecutionFrameTimeTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_MaxExecutionFrameTime)
		.ManualWidth(110.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_MaxExecutionFrameTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_MaxExecutionFrameTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_MaxExecutionFrameTimeTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_ExecutionTime)
		.ManualWidth(75.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_ExecutionTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_ExecutionTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_ExecutionTimeTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_ExecutionWallTime)
		.ManualWidth(100.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_ExecutionWallTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_ExecutionWallTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_ExecutionWallTimeTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_TotalTime)
		.ManualWidth(75.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_TotalTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_TotalTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_TotalTimeTooltip)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_TotalWallTime)
		.ManualWidth(100.0f)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_TotalWallTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_TotalWallTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		.InitialSortMode(EColumnSortMode::Descending)
		.DefaultTooltip(PCGEditorGraphProfilingView::TEXT_TotalWallTimeTooltip);
}

FText SPCGEditorGraphProfilingView::GetTotalTimeLabel() const
{
	return FText::Format(LOCTEXT("GraphTotalTimeLabel", "{0} s"), TotalTime);
}

FText SPCGEditorGraphProfilingView::GetTotalWallTimeLabel() const
{
	return FText::Format(LOCTEXT("GraphTotalWallTimeLabel", "{0} s"), TotalWallTime);
}

void SPCGEditorGraphProfilingView::OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode)
{
	if (SortingColumn == ColumnId)
	{
		// Circling
		SortMode = static_cast<EColumnSortMode::Type>((SortMode + 1) % 3);
	}
	else
	{
		SortingColumn = ColumnId;
		SortMode = NewSortMode;
	}

	RequestRefresh();
}

void SPCGEditorGraphProfilingView::OnSubgraphExpandDepthChanged(int32 NewValue)
{
	ExpandSubgraphDepth = NewValue;
	RequestRefresh();
}

FReply SPCGEditorGraphProfilingView::OnListViewKeyDown(const FGeometry& /*InGeometry*/, const FKeyEvent& InKeyEvent) const
{
	if (ListViewCommands->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SPCGEditorGraphProfilingView::CopySelectionToClipboard() const
{
	constexpr TCHAR Delimiter = TEXT(',');
	constexpr TCHAR LineEnd = TEXT('\n');

	TStringBuilder<2048> CSVExport;
	TArray<FName> VisibleColumns;

	// Write column header row
	bool bHasWrittenAColumn = false;
	const TIndirectArray<SHeaderRow::FColumn>& Columns = ListViewHeader->GetColumns();
	for (auto Column : Columns)
	{
		if (!Column.bIsVisible)
		{
			continue;
		}

		VisibleColumns.Add(Column.ColumnId);
		const FText& ColumnTitle = Column.DefaultText.Get();

		if (bHasWrittenAColumn)
		{
			CSVExport += Delimiter;
		}

		CSVExport += ColumnTitle.ToString();
		bHasWrittenAColumn = true;
	}

	// Gather selected rows and sort them to match the display order instead of selection order
	TArray<PCGProfilingListViewItemPtr> SelectedListViewItems = ListView->GetSelectedItems();

	TArray<int32> SelectedListViewItemsIndices;
	Algo::Transform(SelectedListViewItems, SelectedListViewItemsIndices, [this](const PCGProfilingListViewItemPtr& InItem) { return ListViewItems.IndexOfByKey(InItem); });
	Algo::Sort(SelectedListViewItemsIndices, [](const int32& A, const int32& B) { return A < B; });

	SelectedListViewItems.Reset();
	Algo::Transform(SelectedListViewItemsIndices, SelectedListViewItems, [this](const int32& InItemIndex) { return ListViewItems[InItemIndex]; });

	// Write each row
	for (const PCGProfilingListViewItemPtr& ListViewItem : SelectedListViewItems)
	{
		CSVExport += LineEnd;

		for (int ColumnIndex = 0; ColumnIndex < VisibleColumns.Num(); ++ColumnIndex)
		{
			if (ColumnIndex > 0)
			{
				CSVExport += Delimiter;
			}

			CSVExport += ListViewItem->GetTextForColumn(VisibleColumns[ColumnIndex], /*bNoGrouping=*/true).ToString();
		}
	}

	FPlatformApplicationMisc::ClipboardCopy(*CSVExport);
}

bool SPCGEditorGraphProfilingView::CanCopySelectionToClipboard() const
{
	return ListView->GetNumItemsSelected() > 0;
}

EColumnSortMode::Type SPCGEditorGraphProfilingView::GetColumnSortMode(const FName ColumnId) const
{
	if (SortingColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}

namespace PCGEditorGraphProfilingView
{
	void AddListItems(
		TArray<PCGProfilingListViewItemPtr>& OutListViewItems,
		const TArray<PCGUtils::FCallTreeInfo>& TreeInfo,
		const TMap<const UPCGNode*, UPCGEditorGraphNode*>& EditorNodeLookup,
		int ExpandSubgraphDepth,
		const FString& FolderName,
		const FString& SearchString,
		UPCGEditorGraphNode* CurrentEditorNode = nullptr)
	{
		for (const PCGUtils::FCallTreeInfo& Info : TreeInfo)
		{
			UPCGEditorGraphNode*const* EditorNodeItr = EditorNodeLookup.Find(Info.Node);
			UPCGEditorGraphNode* EditorNode = EditorNodeItr ? *EditorNodeItr : CurrentEditorNode;

			FString Fullname = FolderName;
			if (!Info.Name.IsEmpty())
			{
				Fullname += Info.Name;
			}
			else if (Info.Node)
			{
				Fullname += Info.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString();
			}
			else if (Info.LoopIndex != INDEX_NONE)
			{
				Fullname += FString::Printf(TEXT("Loop_%d"), Info.LoopIndex);
			}

			const bool bFilteredIn = SearchString.IsEmpty() || Fullname.Find(SearchString) != INDEX_NONE || (Info.Node && Info.Node->GetName().Find(SearchString) != INDEX_NONE);

			if (bFilteredIn && (Info.Children.IsEmpty() || ExpandSubgraphDepth == 0))
			{
				PCGProfilingListViewItemPtr ListViewItem = MakeShared<FPCGProfilingListViewItem>();

				ListViewItem->PCGNode = Info.Node;
				ListViewItem->EditorNode = EditorNode;
				ListViewItem->Name = Fullname;
				ListViewItem->bHasData = (Info.CallTime.MaxExecutionFrameTime > 0);

				ListViewItem->CallTime = Info.CallTime;

				OutListViewItems.Add(ListViewItem);
			}

			if(ExpandSubgraphDepth > 0 && !Info.Children.IsEmpty())
			{
				AddListItems(OutListViewItems, Info.Children, EditorNodeLookup, ExpandSubgraphDepth - 1, Fullname + "/", SearchString, EditorNode);
			}
		}
	}
}

void SPCGEditorGraphProfilingView::RequestRefresh()
{
	bNeedsRefresh = true;
}

FReply SPCGEditorGraphProfilingView::Refresh()
{
	TotalTime = 0;
	TotalWallTime = 0;

	ListViewItems.Empty();
	ListView->RequestListRefresh();

	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	if (!PCGEditor.IsValid())
	{
		return FReply::Handled();
	}

	if (!PCGEditorGraph)
	{
		return FReply::Handled();
	}

	const UPCGComponent* Component = PCGComponent.Get();
	if (!Component)
	{
		return FReply::Handled();
	}

	TArray<UPCGEditorGraphNode*> EditorNodes;
	PCGEditorGraph->GetNodesOfClass<UPCGEditorGraphNode>(EditorNodes);

	TMap<const UPCGNode*, UPCGEditorGraphNode*> EditorNodeLookup;
	for (UPCGEditorGraphNode* EditorNode : EditorNodes)
	{
		EditorNodeLookup.Add(EditorNode->GetPCGNode(), EditorNode);
	}

	PCGUtils::FCallTreeInfo TreeInfo = Component->ExtraCapture.CalculateCallTreeInfo(Component, PCGStack);

	ListViewItems.Reserve(TreeInfo.Children.Num());

	if (TreeInfo.Children.Num() > 0)
	{
		TotalTime = TreeInfo.CallTime.TotalTime();
		TotalWallTime = TreeInfo.CallTime.TotalWallTime();
	}

	//TODO: could turn this into a tree instead of expanding into a list
	PCGEditorGraphProfilingView::AddListItems(ListViewItems, TreeInfo.Children, EditorNodeLookup, ExpandSubgraphDepth, FString(), SearchValue);

	if (SortingColumn != NAME_None && SortMode != EColumnSortMode::None)
	{
		Algo::Sort(ListViewItems, [this](const PCGProfilingListViewItemPtr& A, const PCGProfilingListViewItemPtr& B)
			{
				bool isLess = false;
				if (SortingColumn == PCGEditorGraphProfilingView::NAME_Node)
				{
					isLess = A->Name < B->Name;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_PrepareDataTime)
				{
					isLess = A->CallTime.PrepareDataTime < B->CallTime.PrepareDataTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_PrepareDataWallTime)
				{
					isLess = A->CallTime.PrepareDataWallTime() < B->CallTime.PrepareDataWallTime();
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_NbPrepareFrames)
				{
					isLess = A->CallTime.PrepareDataFrameCount < B->CallTime.PrepareDataFrameCount;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_MinExecutionFrameTime)
				{
					isLess = A->CallTime.MinExecutionFrameTime < B->CallTime.MinExecutionFrameTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_MaxExecutionFrameTime)
				{
					isLess = A->CallTime.MaxExecutionFrameTime < B->CallTime.MaxExecutionFrameTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_ExecutionTime)
				{
					isLess = A->CallTime.ExecutionTime < B->CallTime.ExecutionTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_ExecutionWallTime)
				{
					isLess = A->CallTime.ExecutionWallTime() < B->CallTime.ExecutionWallTime();
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_NbExecutionFrames)
				{
					isLess = A->CallTime.ExecutionFrameCount < B->CallTime.ExecutionFrameCount;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_TotalTime)
				{
					isLess = A->CallTime.TotalTime() < B->CallTime.TotalTime();
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_TotalWallTime)
				{
					isLess = A->CallTime.TotalWallTime() < B->CallTime.TotalWallTime();
				}

				return SortMode == EColumnSortMode::Ascending ? isLess : !isLess;
			});
	}

	ListView->SetItemsSource(&ListViewItems);

	return FReply::Handled();
}

void SPCGEditorGraphProfilingView::OnDebugStackChanged(const FPCGStack& InPCGStack)
{
	PCGStack = InPCGStack;

	if (PCGComponent.IsValid())
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
	}

	PCGComponent = const_cast<UPCGComponent*>(InPCGStack.GetRootComponent());

	if (PCGComponent.IsValid())
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.AddSP(this, &SPCGEditorGraphProfilingView::OnGenerateUpdated);
	}

	Refresh();
}

void SPCGEditorGraphProfilingView::OnGenerateUpdated(UPCGComponent* InPCGComponent)
{
	Refresh();
}

TSharedRef<ITableRow> SPCGEditorGraphProfilingView::OnGenerateRow(PCGProfilingListViewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SPCGProfilingListViewItemRow, OwnerTable, Item);
}

void SPCGEditorGraphProfilingView::OnSearchTextChanged(const FText& InText)
{
	SearchValue = InText.ToString();
	Refresh();
}

void SPCGEditorGraphProfilingView::OnSearchTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	OnSearchTextChanged(InText);
}

#undef LOCTEXT_NAMESPACE
