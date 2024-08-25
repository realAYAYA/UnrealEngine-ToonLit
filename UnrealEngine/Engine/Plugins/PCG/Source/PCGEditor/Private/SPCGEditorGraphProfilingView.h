// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/PCGStackContext.h"
#include "Utils/PCGExtraCapture.h"

#include "Framework/Commands/GenericCommands.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/STableRow.h"

class FPCGEditor;
class STableViewBase;
class UPCGComponent;
class UPCGEditorGraphNode;
class UPCGEditorGraph;
class UPCGNode;

struct FPCGProfilingListViewItem
{
	FText GetTextForColumn(FName ColumnId, bool bNoGrouping) const;

	const UPCGNode* PCGNode = nullptr;
	const UPCGEditorGraphNode* EditorNode = nullptr;

	FString Name;

	PCGUtils::FCallTime CallTime;

	bool bHasData = false;
};

typedef TSharedPtr<FPCGProfilingListViewItem> PCGProfilingListViewItemPtr;

class SPCGProfilingListViewItemRow : public SMultiColumnTableRow<PCGProfilingListViewItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SPCGProfilingListViewItemRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const PCGProfilingListViewItemPtr& Item);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;

protected:
	PCGProfilingListViewItemPtr InternalItem;
};

class SPCGEditorGraphProfilingView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphProfilingView) { }
	SLATE_END_ARGS()

	~SPCGEditorGraphProfilingView();

	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SHeaderRow> CreateHeaderRowWidget();

	int32 GetSubgraphExpandDepth() const { return ExpandSubgraphDepth; }
	void OnSubgraphExpandDepthChanged(int32 NewValue);

	void OnDebugStackChanged(const FPCGStack& InPCGStack);

	void OnGenerateUpdated(UPCGComponent* InPCGComponent);
	
	// Callbacks
	void RequestRefresh();
	FReply Refresh();
	TSharedRef<ITableRow> OnGenerateRow(PCGProfilingListViewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnItemDoubleClicked(PCGProfilingListViewItemPtr Item);
	void OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode);
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	FText GetTotalTimeLabel() const;
	FText GetTotalWallTimeLabel() const;

	FReply OnListViewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) const;
	void CopySelectionToClipboard() const;
	bool CanCopySelectionToClipboard() const;

	/** Called when user changes the text they are searching for */
	void OnSearchTextChanged(const FText& InText);

	/** Called when user changes commits text to the search box */
	void OnSearchTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	/** Pointer back to the PCG editor that owns us */
	TWeakPtr<FPCGEditor> PCGEditorPtr;

	/** Cached PCGGraph being viewed */
	UPCGEditorGraph* PCGEditorGraph = nullptr;

	/** Cached PCGComponent being viewed */
	TWeakObjectPtr<UPCGComponent> PCGComponent;

	/** Current stack being viewed */
	FPCGStack PCGStack;

	TSharedPtr<SHeaderRow> ListViewHeader;
	TSharedPtr<SListView<PCGProfilingListViewItemPtr>> ListView;
	TArray<PCGProfilingListViewItemPtr> ListViewItems;
	TSharedPtr<FUICommandList> ListViewCommands;

	// To allow sorting
	FName SortingColumn = NAME_None;
	EColumnSortMode::Type SortMode = EColumnSortMode::Type::None;

	double TotalTime = 0.0;
	double TotalWallTime = 0.0;

	bool bNeedsRefresh = false;

	/** Currently depth of entry exposition - if 0, only the nodes in the currently debugged graph will be shown. */
	int32 ExpandSubgraphDepth = 0;

	/** The string to search for */
	FString SearchValue;
};
