// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SSearchBox.h"
#include "ViewDebug.h"

#if !UE_BUILD_SHIPPING

typedef TSharedPtr<const FViewDebugInfo::FPrimitiveInfo> FPrimitiveRowDataPtr;

class SDrawPrimitiveDebugger : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDrawPrimitiveDebugger)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	FText GetFilterText() const;
	void OnFilterTextChanged(const FText& InFilterText);
	void OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	TSharedRef<ITableRow> MakeRowWidget(FPrimitiveRowDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable);
	void UpdateVisibleRows();
	void SortRows();
	void Refresh();
	void AddColumn(const FText& Name, const FName& ColumnId);
	void OnChangeEntryVisibility(ECheckBoxState state, FPrimitiveRowDataPtr data);
	bool IsEntryVisible(FPrimitiveRowDataPtr data) const;
	void OnChangeEntryPinned(ECheckBoxState state, FPrimitiveRowDataPtr data);
	bool IsEntryPinned(FPrimitiveRowDataPtr data) const;
	bool CanCaptureSingleFrame() const;
	FReply OnRefreshClick();
	FReply OnSaveClick();
	ECheckBoxState IsLiveCaptureChecked() const;
	void OnToggleLiveCapture(ECheckBoxState state);

private:
	
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SListView<FPrimitiveRowDataPtr>> Table;
	FText FilterText;
	TSharedPtr<SHeaderRow> ColumnHeader;
	TArray<FPrimitiveRowDataPtr> AvailableEntries;
	TArray<FPrimitiveRowDataPtr> VisibleEntries;

	// TODO: Decide if primitive data copies are necessary
	TMap<FPrimitiveComponentId, FViewDebugInfo::FPrimitiveInfo> LocalCopies;
	TSet<FPrimitiveComponentId> PinnedEntries;
	TSet<FPrimitiveComponentId> HiddenEntries;
};

/**
 * A widget to represent a row in a Data Table Editor widget. This widget allows us to do things like right-click
 * and take actions on a particular row of a Data Table.
 */
class SDrawPrimitiveDebuggerListViewRow : public SMultiColumnTableRow<FPrimitiveRowDataPtr>
{
public:

	SLATE_BEGIN_ARGS(SDrawPrimitiveDebuggerListViewRow)
	{
	}
	/** The owning object. This allows us access to the actual data table being edited as well as some other API functions. */
	SLATE_ARGUMENT(TWeakPtr<SDrawPrimitiveDebugger>, DrawPrimitiveDebugger)
		/** The primitive we're working with to allow us to get naming information. */
		SLATE_ARGUMENT(FPrimitiveRowDataPtr, RowDataPtr)
		SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:

	TSharedRef<SWidget> MakeCellWidget(const int32 InRowIndex, const FName& InColumnId);
	
	ECheckBoxState IsVisible() const;
	ECheckBoxState IsPinned() const;

	FPrimitiveRowDataPtr RowDataPtr;
	TWeakPtr<SDrawPrimitiveDebugger> DrawPrimitiveDebugger;
};

#endif