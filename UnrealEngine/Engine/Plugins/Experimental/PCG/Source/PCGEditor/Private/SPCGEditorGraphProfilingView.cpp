// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphProfilingView.h"

#include "PCGComponent.h"
#include "PCGEditor.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNode.h"
#include "PCGGraph.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphProfilingView"

namespace PCGEditorGraphProfilingView
{
	const FText NoDataAvailableText = LOCTEXT("NoDataAvailableText", "N/A");
	
	/** Names of the columns in the attribute list */
	const FName NAME_Node = FName(TEXT("Node"));
	const FName NAME_AvgTime = FName(TEXT("AvgTime"));
	const FName NAME_MinTime = FName(TEXT("MinTime"));
	const FName NAME_MaxTime = FName(TEXT("MaxTime"));
	const FName NAME_StdTime = FName(TEXT("StdTime"));
	const FName NAME_TotalTime = FName(TEXT("TotalTime"));
	const FName NAME_NbCalls = FName(TEXT("NbCalls"));

	/** Labels of the columns */
	const FText TEXT_NodeLabel = LOCTEXT("NodeLabel", "Node");
	const FText TEXT_AvgTimeLabel = LOCTEXT("AvgTimeLabel", "Avg Time(ms)");
	const FText TEXT_MinTimeLabel = LOCTEXT("MinTimeLabel", "Min Time(ms)");
	const FText TEXT_MaxTimeLabel = LOCTEXT("MaxTimeLabel", "Max Time(ms)");
	const FText TEXT_StdTimeLabel = LOCTEXT("StdTimeLabel", "Std(ms)");
	const FText TEXT_TotalTimeLabel = LOCTEXT("TotalTimeLabel", "Total time(s)");
	const FText TEXT_NbCallsLabel = LOCTEXT("NbCallsLabel", "Number of calls");
}

void SPCGProfilingListViewItemRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const PCGProfilingListViewItemPtr& Item)
{
	InternalItem = Item;

	SMultiColumnTableRow<PCGProfilingListViewItemPtr>::Construct(
		SMultiColumnTableRow::FArguments()
		.Style(FAppStyle::Get(), "DataTableEditor.CellListViewRow"),
		InOwnerTableView);
}

TSharedRef<SWidget> SPCGProfilingListViewItemRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	FText ColumnData = LOCTEXT("ColumnError", "Unrecognized Column");
	if (InternalItem.IsValid())
	{
		if (ColumnId == PCGEditorGraphProfilingView::NAME_Node)
		{
			ColumnData = FText::FromName(InternalItem->Name);
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_NbCalls)
		{
			ColumnData = FText::AsNumber(InternalItem->NbCalls);
		}
		// For all other values, if we don't have data, just write "N/A"
		else if (!InternalItem->HasData)
		{
			ColumnData = PCGEditorGraphProfilingView::NoDataAvailableText;
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_AvgTime)
		{
			// In ms
			ColumnData = FText::AsNumber(InternalItem->AvgTime * 1000.0);
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_MinTime)
		{
			// In ms
			ColumnData = FText::AsNumber(InternalItem->MinTime * 1000.0);
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_MaxTime)
		{
			// In ms
			ColumnData = FText::AsNumber(InternalItem->MaxTime * 1000.0);
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_StdTime)
		{
			// In ms
			ColumnData = FText::AsNumber(InternalItem->StdTime * 1000.0);
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_TotalTime)
		{
			// In s
			ColumnData = FText::AsNumber(InternalItem->TotalTime);
		}
	}

	return SNew(STextBlock).Text(ColumnData);
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

void SPCGEditorGraphProfilingView::Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor)
{
	PCGEditorPtr = InPCGEditor;

	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	if (PCGEditor)
	{
		PCGEditorGraph = PCGEditor->GetPCGEditorGraph();
	}

	ListViewHeader = CreateHeaderRowWidget();

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
				SNew(SButton)
				.Text(LOCTEXT("RefreshButton", "Refresh"))
				.OnClicked(this, &SPCGEditorGraphProfilingView::Refresh)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetButton", "Reset"))
				.OnClicked(this, &SPCGEditorGraphProfilingView::ResetTimers)
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

TSharedRef<SHeaderRow> SPCGEditorGraphProfilingView::CreateHeaderRowWidget()
{
	return SNew(SHeaderRow)
		.ResizeMode(ESplitterResizeMode::FixedPosition)
		.CanSelectGeneratedColumn(true)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_Node)
		.ManualWidth(150)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_NodeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Left)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_Node)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_AvgTime)
		.ManualWidth(100)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_AvgTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_AvgTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_MinTime)
		.ManualWidth(100)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_MinTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_MinTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_MaxTime)
		.ManualWidth(100)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_MaxTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_MaxTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_StdTime)
		.ManualWidth(100)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_StdTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_StdTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_TotalTime)
		.ManualWidth(100)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_TotalTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_TotalTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_NbCalls)
		.ManualWidth(125)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_NbCallsLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_NbCalls)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader);
}

void SPCGEditorGraphProfilingView::OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode)
{
	if (SortingColumn == ColumnId)
	{
		// Circling
		SortMode = EColumnSortMode::Type((SortMode + 1) % 3);
	}
	else
	{
		SortingColumn = ColumnId;
		SortMode = NewSortMode;
	}

	Refresh();
}

EColumnSortMode::Type SPCGEditorGraphProfilingView::GetColumnSortMode(const FName ColumnId) const
{
	if (SortingColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}

FReply SPCGEditorGraphProfilingView::ResetTimers()
{
	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	if (!PCGEditor.IsValid())
	{
		return FReply::Handled();
	}

	if (!PCGEditorGraph)
	{
		return FReply::Handled();
	}

	TArray<UPCGEditorGraphNode*> EditorNodes;
	PCGEditorGraph->GetNodesOfClass<UPCGEditorGraphNode>(EditorNodes);

	for (UPCGEditorGraphNode* PCGEditorNode : EditorNodes)
	{
		if (PCGEditorNode)
		{
			if (UPCGNode* PCGNode = PCGEditorNode->GetPCGNode())
			{
				if (UPCGSettings* Settings = PCGNode->DefaultSettings)
				{
					if (IPCGElement* Element = Settings->GetElement().Get())
					{
						Element->ResetTimers();
					}
				}
			}
		}
	}

	Refresh();

	return FReply::Handled();
}

FReply SPCGEditorGraphProfilingView::Refresh()
{
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

	TArray<UPCGEditorGraphNode*> EditorNodes;
	PCGEditorGraph->GetNodesOfClass<UPCGEditorGraphNode>(EditorNodes);
	ListViewItems.Reserve(EditorNodes.Num());

	for (const UPCGEditorGraphNode* PCGEditorNode : EditorNodes)
	{
		if (PCGEditorNode)
		{
			PCGProfilingListViewItemPtr ListViewItem = MakeShared<FPCGProfilingListViewItem>();
			ListViewItem->EditorNode = PCGEditorNode;
			ListViewItem->PCGNode = PCGEditorNode->GetPCGNode();

			if (ListViewItem->PCGNode)
			{
				ListViewItem->Name = ListViewItem->PCGNode->GetNodeTitle();

				if (const UPCGSettings* Settings = ListViewItem->PCGNode->DefaultSettings)
				{
					if (const IPCGElement* Element = Settings->GetElement().Get())
					{
						const TArray<double>& Timers = Element->GetTimers();
						if (!Timers.IsEmpty())
						{
							ListViewItem->MinTime = std::numeric_limits<double>::max();
							ListViewItem->MaxTime = std::numeric_limits<double>::min();
							for (double v : Timers)
							{
								ListViewItem->TotalTime += v;
								if (v < ListViewItem->MinTime)
								{
									ListViewItem->MinTime = v;
								}

								if (v > ListViewItem->MaxTime)
								{
									ListViewItem->MaxTime = v;
								}
							}

							ListViewItem->AvgTime = ListViewItem->TotalTime / Timers.Num();

							for (double v : Timers)
							{
								ListViewItem->StdTime += (ListViewItem->AvgTime - v) * (ListViewItem->AvgTime - v);
							}

							ListViewItem->StdTime = FMath::Sqrt(ListViewItem->StdTime / Timers.Num());

							ListViewItem->HasData = true;
							ListViewItem->NbCalls = Timers.Num();
						}
					}
				}
			}

			ListViewItems.Add(ListViewItem);
		}
	}

	if (SortingColumn != NAME_None && SortMode != EColumnSortMode::None)
	{
		Algo::Sort(ListViewItems, [this](const PCGProfilingListViewItemPtr& A, const PCGProfilingListViewItemPtr& B)
			{
				bool isLess = false;
				if (SortingColumn == PCGEditorGraphProfilingView::NAME_Node)
				{
					isLess = A->Name.FastLess(B->Name);
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_AvgTime)
				{
					isLess = A->AvgTime < B->AvgTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_MinTime)
				{
					isLess = A->MinTime < B->MinTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_MaxTime)
				{
					isLess = A->MaxTime < B->MaxTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_StdTime)
				{
					isLess = A->StdTime < B->StdTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_TotalTime)
				{
					isLess = A->TotalTime < B->TotalTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_NbCalls)
				{
					isLess = A->NbCalls < B->NbCalls;
				}

				return SortMode == EColumnSortMode::Ascending ? isLess : !isLess;
			});
	}

	ListView->SetListItemsSource(ListViewItems);

	return FReply::Handled();
}

TSharedRef<ITableRow> SPCGEditorGraphProfilingView::OnGenerateRow(PCGProfilingListViewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SPCGProfilingListViewItemRow, OwnerTable, Item);
}

#undef LOCTEXT_NAMESPACE
