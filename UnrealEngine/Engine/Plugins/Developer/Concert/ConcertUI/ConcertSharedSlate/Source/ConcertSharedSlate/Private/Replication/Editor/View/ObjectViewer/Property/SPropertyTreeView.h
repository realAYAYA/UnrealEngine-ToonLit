// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"
#include "Replication/Editor/View/IPropertyTreeView.h"
#include "Replication/Editor/View/Tree/SReplicationTreeView.h"

#include "Filters/SBasicFilterBar.h"
#include "Replication/Editor/View/Column/IPropertyTreeColumn.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertSharedSlate
{
	class FReplicatedPropertyData;
	
	/**
	 * This widget knows how to display a list of properties in a tree view.
	 * It generates the items and exposes extension points for more advanced UI, such as filtering.
	 */
	class SPropertyTreeView
		: public SCompoundWidget
		, public IPropertyTreeView
	{
	public:
		
		SLATE_BEGIN_ARGS(SPropertyTreeView)
		{}
			/*************** Arguments inherited by SReplicationTreeView ***************/
		
			/** Optional callback to do even more filtering of items. */
			SLATE_EVENT(SReplicationTreeView<FReplicatedPropertyData>::FCustomFilter, FilterItem)
		
			/** The columns this list should have */
			SLATE_ARGUMENT(TArray<FPropertyColumnEntry>, Columns)
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)
			/** Initial primary sort to set. */
			SLATE_ARGUMENT(FColumnSortInfo, PrimarySort)
			/** Initial secondary sort to set. */
			SLATE_ARGUMENT(FColumnSortInfo, SecondarySort)
		
			/** How many items are to allowed to be selected */
			SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)
		
			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfSearchBar)
			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, RightOfSearchBar)
		
			/** Optional widget to add between the search bar and the table view (e.g. a SBasicFilterBar). */
			SLATE_NAMED_SLOT(FArguments, RowBelowSearchBar)
		
			/** Optional, alternate content to show instead of the tree view when there are no rows. */
			SLATE_NAMED_SLOT(FArguments, NoItemsContent)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		//~ Begin IPropertyTreeView Interface
		virtual void RefreshPropertyData(const TSet<FConcertPropertyChain>& PropertiesToDisplay, const FSoftClassPath& Class, bool bCanReuseExistingRowItems) override;
		virtual void RequestRefilter() const override { TreeView->RequestRefilter(); }
		virtual void RequestResortForColumn(const FName& ColumnId) override { TreeView->RequestResortForColumn(ColumnId); }
		virtual void RequestScrollIntoView(const FConcertPropertyChain& PropertyChain) override;
		virtual TSharedRef<SWidget> GetWidget() override { return SharedThis(this); }
		//~ Begin IPropertyTreeView Interface

	private:

		/** The tree view displaying the replicated properties */
		TSharedPtr<SReplicationTreeView<FReplicatedPropertyData>> TreeView;
		
		/**
		 * These instances can be subclasses of FReplicatedPropertyData, e.g. FReplicatedPropertyData_Editor.
		 * Their type can be overridden by subclasses.
		 * They only have the FReplicatedPropertyData type so they can be passed efficiently to SObjectToPropertyView.
		 * @see GetPropertyData
		 */
		TArray<TSharedPtr<FReplicatedPropertyData>> PropertyRowData;
		/** The instances of ObjectRowData which do not have any parents. This acts as the item source for the tree view. */
		TArray<TSharedPtr<FReplicatedPropertyData>> RootPropertyRowData;
		/** Inverse map of PropertyRowData using FReplicatedPropertyData::GetProperty as key. Contains all elements of PropertyRowData. */
		TMap<FConcertPropertyChain, TSharedPtr<FReplicatedPropertyData>> ChainToPropertyDataCache;
		
		TSharedRef<FReplicatedPropertyData> AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain);

		/** Inits RootPropertyRowData from PropertyRowData. */
		void BuildRootPropertyRowData();
		void GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild);
	};
}

