// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphProfilingView.h"

#include "Framework/Views/TableViewMetadata.h"
#include "PCGComponent.h"
#include "PCGEditor.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNode.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphProfilingView"

namespace PCGEditorGraphProfilingView
{
	const FText NoDataAvailableText = LOCTEXT("NoDataAvailableText", "N/A");
	
	/** Names of the columns in the attribute list */
	const FName NAME_Node = FName(TEXT("Node"));
	const FName NAME_PrepareDataTime = FName(TEXT("PrepareDataTime"));
	const FName NAME_MinExecutionFrameTime = FName(TEXT("MinFrameTime"));
	const FName NAME_MaxExecutionFrameTime = FName(TEXT("MaxFrameTime"));
	const FName NAME_TotalExecutionTime = FName(TEXT("TotalExecutionTime"));
	const FName NAME_NbExecutionFrames = FName(TEXT("NbExecutionFrames"));
	const FName NAME_PostExecuteTime = FName(TEXT("PostExecuteTime"));

	/** Labels of the columns */
	const FText TEXT_NodeLabel = LOCTEXT("NodeLabel", "Node");
	const FText TEXT_PrepareDataTimeLabel = LOCTEXT("PrepareDataTimeLabel", "PrepareData (ms)");
	const FText TEXT_PostExecuteTimeLabel = LOCTEXT("PostExecuteTimeLabel", "PostExecute (ms)");
	const FText TEXT_MinExecutionFrameTimeLabel = LOCTEXT("MinExecutionFrameTimeLabel", "Min Frame Time(ms)");
	const FText TEXT_MaxExecutionFrameTimeLabel = LOCTEXT("MaxExecutionFrameTimeLabel", "Max Frame Time(ms)");
	const FText TEXT_TotalExecutionTimeLabel = LOCTEXT("TotalExecutionTimeLabel", "Total time(s)");
	const FText TEXT_NbExecutionFramesLabel = LOCTEXT("NbExecutionFramesLabel", "Exec frames");
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
			ColumnData = FText::FromString(InternalItem->Name);
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_NbExecutionFrames)
		{
			ColumnData = ((InternalItem->CallTime.ExecutionFrameCount >= 0) ? FText::AsNumber(InternalItem->CallTime.ExecutionFrameCount) : FText());
		}
		else if (!InternalItem->HasData)
		{
			ColumnData = PCGEditorGraphProfilingView::NoDataAvailableText;
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_MinExecutionFrameTime)
		{
			// In ms
			ColumnData = ((InternalItem->CallTime.MinExecutionFrameTime >= 0) ? FText::AsNumber(InternalItem->CallTime.MinExecutionFrameTime * 1000.0) : FText());
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_MaxExecutionFrameTime)
		{
			// In ms
			ColumnData = ((InternalItem->CallTime.MaxExecutionFrameTime >= 0) ? FText::AsNumber(InternalItem->CallTime.MaxExecutionFrameTime * 1000.0) : FText());
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_TotalExecutionTime)
		{
			// In s
			ColumnData = FText::AsNumber(InternalItem->CallTime.ExecutionTime);
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_PrepareDataTime)
		{
			ColumnData = FText::AsNumber(InternalItem->CallTime.PrepareDataTime * 1000.0);
		}
		else if (ColumnId == PCGEditorGraphProfilingView::NAME_PostExecuteTime)
		{
			ColumnData = FText::AsNumber(InternalItem->CallTime.PostExecuteTime * 1000.0);
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

SPCGEditorGraphProfilingView::~SPCGEditorGraphProfilingView()
{
	if (PCGEditorPtr.IsValid())
	{
		PCGEditorPtr.Pin()->OnInspectedComponentChangedDelegate.RemoveAll(this);
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

		PCGEditor->OnInspectedComponentChangedDelegate.AddSP(this, &SPCGEditorGraphProfilingView::OnDebugObjectChanged);
	}

	SortingColumn = PCGEditorGraphProfilingView::NAME_TotalExecutionTime;
	SortMode = EColumnSortMode::Descending;
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
				.Text(LOCTEXT("ResetButton", "Reset"))
				.OnClicked(this, &SPCGEditorGraphProfilingView::ResetTimers)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ExpandSubgraph", "Expand Subgraph"))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SPCGEditorGraphProfilingView::IsSubgraphExpanded)
				.OnCheckStateChanged(this, &SPCGEditorGraphProfilingView::OnSubgraphExpandedChanged)
			]
			+SHorizontalBox::Slot()
			.Padding(FMargin(30.f, 0.f, 0.f, 5.f))
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TotalExecutionTime", "Total Time:"))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &SPCGEditorGraphProfilingView::GetTotalTimeLabel)
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
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_PrepareDataTime)
		.ManualWidth(125)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_PrepareDataTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_PrepareDataTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_NbExecutionFrames)
		.ManualWidth(80)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_NbExecutionFramesLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_NbExecutionFrames)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_MinExecutionFrameTime)
		.ManualWidth(130)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_MinExecutionFrameTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_MinExecutionFrameTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_MaxExecutionFrameTime)
		.ManualWidth(130)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_MaxExecutionFrameTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_MaxExecutionFrameTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader)
		+ SHeaderRow::Column(PCGEditorGraphProfilingView::NAME_TotalExecutionTime)
		.ManualWidth(100)
		.DefaultLabel(PCGEditorGraphProfilingView::TEXT_TotalExecutionTimeLabel)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Right)
		.SortMode(this, &SPCGEditorGraphProfilingView::GetColumnSortMode, PCGEditorGraphProfilingView::NAME_TotalExecutionTime)
		.OnSort(this, &SPCGEditorGraphProfilingView::OnSortColumnHeader);
}

FText SPCGEditorGraphProfilingView::GetTotalTimeLabel() const
{
	return FText::Format(LOCTEXT("GraphTotalTimeLabel", "{0} s"), TotalTime);
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

ECheckBoxState SPCGEditorGraphProfilingView::IsSubgraphExpanded() const
{
	return bExpandSubgraph ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SPCGEditorGraphProfilingView::OnSubgraphExpandedChanged(ECheckBoxState InNewState)
{
	bExpandSubgraph = (InNewState == ECheckBoxState::Checked);
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

	if (const UPCGComponent* Component = PCGComponent.Get())
	{
		Component->ExtraCapture.ResetTimers();
		Refresh();
	}

	return FReply::Handled();
}

namespace PCGEditorGraphProfilingView
{
	void AddListItems(
		TArray<PCGProfilingListViewItemPtr>& OutListViewItems,
		const TArray<PCGUtils::FCallTreeInfo>& TreeInfo,
		const TMap<const UPCGNode*, UPCGEditorGraphNode*>& EditorNodeLookup,
		bool bExpandSubgraph,
		FString FolderName,
		UPCGEditorGraphNode* CurrentEditorNode = nullptr)
	{
		for (const PCGUtils::FCallTreeInfo& Info : TreeInfo)
		{
			UPCGEditorGraphNode*const* EditorNodeItr = EditorNodeLookup.Find(Info.Node);
			UPCGEditorGraphNode* EditorNode = EditorNodeItr ? *EditorNodeItr : CurrentEditorNode;

			FString Fullname = FolderName;
			if (Info.Node)
			{
				Fullname += Info.Node->GetNodeTitle().ToString();
			}

			// don't show inclusive times when bExpandSubgraph is on or the total sum of everything will mismatch reality
			if ((Info.Children.IsEmpty() || bExpandSubgraph == false) && Info.CallTime.MaxExecutionFrameTime > 0)
			{
				PCGProfilingListViewItemPtr ListViewItem = MakeShared<FPCGProfilingListViewItem>();

				ListViewItem->PCGNode = Info.Node;
				ListViewItem->EditorNode = EditorNode;
				ListViewItem->Name = Fullname;
				ListViewItem->HasData = true;

				ListViewItem->CallTime = Info.CallTime;

				OutListViewItems.Add(ListViewItem);
			}

			if (bExpandSubgraph && !Info.Children.IsEmpty())
			{
				AddListItems(OutListViewItems, Info.Children, EditorNodeLookup, /* bExpandSubgraph */ true, Fullname + "/", EditorNode);
			}
		}
	}
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

	PCGUtils::FCallTreeInfo TreeInfo = Component->ExtraCapture.CalculateCallTreeInfo(Component);

	TotalTime = TreeInfo.CallTime.ExecutionTime;

	ListViewItems.Reserve(TreeInfo.Children.Num());

	//TODO: could turn this into a tree instead of expanding into a list
	PCGEditorGraphProfilingView::AddListItems(ListViewItems, TreeInfo.Children, EditorNodeLookup, bExpandSubgraph, FString());

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
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_MinExecutionFrameTime)
				{
					isLess = A->CallTime.MinExecutionFrameTime < B->CallTime.MinExecutionFrameTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_MaxExecutionFrameTime)
				{
					isLess = A->CallTime.MaxExecutionFrameTime < B->CallTime.MaxExecutionFrameTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_TotalExecutionTime)
				{
					isLess = A->CallTime.ExecutionTime < B->CallTime.ExecutionTime;
				}
				else if (SortingColumn == PCGEditorGraphProfilingView::NAME_NbExecutionFrames)
				{
					isLess = A->CallTime.ExecutionFrameCount < B->CallTime.ExecutionFrameCount;
				}

				return SortMode == EColumnSortMode::Ascending ? isLess : !isLess;
			});
	}

	ListView->SetItemsSource(&ListViewItems);

	return FReply::Handled();
}

void SPCGEditorGraphProfilingView::OnDebugObjectChanged(UPCGComponent* InPCGComponent)
{
	if (PCGComponent.IsValid())
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
	}

	PCGComponent = InPCGComponent;

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

#undef LOCTEXT_NAMESPACE
