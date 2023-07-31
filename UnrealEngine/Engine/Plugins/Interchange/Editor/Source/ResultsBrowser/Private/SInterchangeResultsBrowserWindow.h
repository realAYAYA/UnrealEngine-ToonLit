// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "InterchangeResultsContainer.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"


class SInterchangeResultsBrowserWindow : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnFilterChangedState, bool)

	SLATE_BEGIN_ARGS(SInterchangeResultsBrowserWindow)
		: _OwnerTab()
		, _InterchangeResultsContainer()
		, _IsFiltered(false)
		{}

		SLATE_ARGUMENT(TWeakPtr<SDockTab>, OwnerTab)
		SLATE_ARGUMENT(UInterchangeResultsContainer*, InterchangeResultsContainer)
		SLATE_ARGUMENT(bool, IsFiltered)
		SLATE_EVENT(FOnFilterChangedState, OnFilterChangedState)
	SLATE_END_ARGS()

public:

	SInterchangeResultsBrowserWindow();
	~SInterchangeResultsBrowserWindow();

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	void Set(UInterchangeResultsContainer* Data);
	void CloseErrorBrowser();

	FReply OnCloseDialog()
	{
		CloseErrorBrowser();
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCloseDialog();
		}

		return FReply::Unhandled();
	}

private:

	/** Called by SListView to get a widget corresponding to the supplied item */
	TSharedRef<ITableRow> OnGenerateRowForList(UInterchangeResult* Item, const TSharedRef<STableViewBase>& OwnerTable);

	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;

	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);
	void OnSelectionChanged(UInterchangeResult* InItem, ESelectInfo::Type SelectInfo);
	void OnFilterStateChanged(ECheckBoxState NewState);
	void RepopulateItems();
	void RequestSort();


	TWeakPtr<SDockTab> OwnerTab;
	UInterchangeResultsContainer* ResultsContainer;
	TArray<UInterchangeResult*> FilteredResults;
	TSharedPtr<SListView<UInterchangeResult*>> ListView;
	TSharedPtr<STextBlock> Description;
	FName SortByColumn;
	EColumnSortMode::Type SortMode;
	FOnFilterChangedState OnFilterChangedState;
	bool bIsFiltered;
};


class SInterchangeResultsBrowserListRow : public SMultiColumnTableRow<UInterchangeResult*>
{
public:
	SLATE_BEGIN_ARGS(SInterchangeResultsBrowserListRow) {}
		SLATE_ARGUMENT(TSharedPtr<SInterchangeResultsBrowserWindow>, InterchangeResultsBrowserWidget)
		SLATE_ARGUMENT(UInterchangeResult*, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:

	TWeakPtr<SInterchangeResultsBrowserWindow> InterchangeResultsBrowserWidgetPtr;
	UInterchangeResult* Item;
};
