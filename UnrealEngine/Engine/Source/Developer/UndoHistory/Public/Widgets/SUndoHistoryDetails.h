// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IReflectionDataProvider.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Misc/TextFilter.h"

class UNDOHISTORY_API SUndoHistoryDetails : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SUndoHistoryDetails) {}
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	void Construct(const FArguments& InArgs, TSharedRef<UE::UndoHistory::IReflectionDataProvider> InReflectionData = UE::UndoHistory::CreateDefaultReflectionProvider());

	/**
	 * Set the transaction to be displayed in the details panel.
	 */
	void SetSelectedTransaction(const struct FTransactionDiff& InTransactionDiff);

	/**
	 * Clear the details panel.
	 */
	void Reset();


	/** SWidget interface */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	struct FUndoDetailsTreeNode;
	using FUndoDetailsTreeNodePtr = TSharedPtr<FUndoDetailsTreeNode>;
	using FTreeItemTextFilter = TTextFilter<const FString&>;

	/** Tree node representing a changed object and its changed properties as children. */
	struct FUndoDetailsTreeNode
	{
		FString Name;
		TOptional<FString> Type;
		TOptional<FText> ToolTip;
		TSharedPtr<FTransactionObjectEvent> TransactionEvent;
		TArray<FUndoDetailsTreeNodePtr> Children;

		FUndoDetailsTreeNode(FString InName, TOptional<FString> InType, TOptional<FText> InToolTip, TSharedPtr<FTransactionObjectEvent> InTransactionEvent = nullptr)
			: Name(MoveTemp(InName))
			, Type(MoveTemp(InType))
			, ToolTip(MoveTemp(InToolTip))
			, TransactionEvent(MoveTemp(InTransactionEvent))
		{}
	};

private:

	/** Create a changed object node. */
	FUndoDetailsTreeNodePtr CreateTreeNode(const FString& InObjectName, const FSoftClassPath& InObjectClass, const TSharedPtr<FTransactionObjectEvent>& InEvent) const;

	/** Create a tooltip text for a property. */
	TOptional<FText> CreateToolTipText(TOptional<EPropertyFlags> InFlags) const;

	/** Whether the type row will be generated */
	bool SupportsTypeRow() const;

	/** Callback to handle a change in the filter box. */
	void OnFilterTextChanged(const FText& InFilterText);

	/** Refresh the details tree view. */
	void FullRefresh();

	/** Populate the search strings for the filter. */
	void PopulateSearchStrings(const FString&, TArray<FString>& OutSearchStrings) const;

	/** Populate the details tree. */
	void Populate();

	/** Callback for generating a UndoHistoryDetailsRow */
	TSharedRef<ITableRow> HandleGenerateRow(FUndoDetailsTreeNodePtr InNode, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Callback for getting the filter highlight text. */
	FText HandleGetFilterHighlightText() const;

	/** Callback for getting the details visibility. */
	EVisibility HandleDetailsVisibility() const;

	/** Callback for getting the transaction name. */
	FText HandleTransactionName() const;

	/* Callback for getting the transaction id. */
	FText HandleTransactionId() const;

	/** Callback for handling a click on the transaction id. */
	void HandleTransactionIdNavigate();

private:

	/** Gives us information about property data (gives programs the opportunity to inject custom handler since not all reflection data is available in programs). */
	TSharedPtr<UE::UndoHistory::IReflectionDataProvider> ReflectionData;
	
	/** Holds the ChangedObjects TreeView. */
	TSharedPtr<STreeView<FUndoDetailsTreeNodePtr> > ChangedObjectsTreeView;

	/** Holds the ChangedObjects to be used as an ItemSource to the TreeView. */
	TArray<FUndoDetailsTreeNodePtr> ChangedObjects;

	/** Holds the ChangedObjects to be displayed. */
	TArray<FUndoDetailsTreeNodePtr> FilteredChangedObjects;

	/** Holds the search box. */
	TSharedPtr<class SSearchBox> FilterTextBoxWidget;

	/** Holds the TransactionName. */
	FText TransactionName;

	/** Holds the TransactionId. */
	FText TransactionId;

	/** The TextFilter attached to the SearchBox widget of the UndoHistoryDetails panel. */
	TSharedPtr<FTreeItemTextFilter> SearchBoxFilter;

	/** If the details tree needs to be refreshed. */
	bool bNeedsRefresh;

	/** If the tree items need to be expanded (ie. When the filter text changes). */
	bool bNeedsExpansion;
};
