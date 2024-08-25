// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyTreeView.h"

#include "Replication/Editor/Model/ReplicatedPropertyData.h"

#include "Algo/ForEach.h"
#include "Replication/Editor/View/Column/PropertyColumnAdapter.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "SReplicatedPropertiesView"

namespace UE::ConcertSharedSlate
{
	void SPropertyTreeView::Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SAssignNew(TreeView, SReplicationTreeView<FReplicatedPropertyData>)
				.RootItemsSource(&RootPropertyRowData)
				.OnGetChildren(this, &SPropertyTreeView::GetPropertyRowChildren)
				.FilterItem(InArgs._FilterItem)
				.Columns(FPropertyColumnAdapter::Transform(InArgs._Columns))
				.ExpandableColumnLabel(InArgs._ExpandableColumnLabel)
				.PrimarySort(InArgs._PrimarySort)
				.SecondarySort(InArgs._SecondarySort)
				.SelectionMode(InArgs._SelectionMode)
				.LeftOfSearchBar() [ InArgs._LeftOfSearchBar.Widget ]
				.RightOfSearchBar() [ InArgs._RightOfSearchBar.Widget ]
				.RowBelowSearchBar() [ InArgs._RowBelowSearchBar.Widget ]
				.NoItemsContent() [ InArgs._NoItemsContent.Widget ]
		];
	}

	void SPropertyTreeView::RefreshPropertyData(const TSet<FConcertPropertyChain>& PropertiesToDisplay, const FSoftClassPath& Class, const bool bCanReuseExistingRowItems)
	{
		if (!bCanReuseExistingRowItems)
		{
			ChainToPropertyDataCache.Reset();
		}
		
		// Try to re-use old instances by using the old ChainToPropertyDataCache. This is also done so the expansion states restore correctly in the tree view.
		TMap<FConcertPropertyChain, TSharedPtr<FReplicatedPropertyData>> NewChainToPropertyDataCache;
		
		PropertyRowData.Empty();
		for (const FConcertPropertyChain& PropertyChain : PropertiesToDisplay)
		{
			const TSharedPtr<FReplicatedPropertyData>* ExistingItem = ChainToPropertyDataCache.Find(PropertyChain);
			const TSharedRef<FReplicatedPropertyData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocatePropertyData(Class, PropertyChain);
			PropertyRowData.Emplace(Item);
			NewChainToPropertyDataCache.Emplace(PropertyChain, Item);
		}

		// If an item was removed, then NewPathToPropertyDataCache does not contain it. 
		ChainToPropertyDataCache = MoveTemp(NewChainToPropertyDataCache);
		
		// The tree view requires the item source to only contain the root items.
		BuildRootPropertyRowData();

		TreeView->RequestRefilter();
	}

	void SPropertyTreeView::RequestScrollIntoView(const FConcertPropertyChain& PropertyChain)
	{
		const int32 Index = PropertyRowData.IndexOfByPredicate([&PropertyChain](const TSharedPtr<FReplicatedPropertyData>& Data)
		{
			return Data->GetProperty() == PropertyChain;
		});
		if (PropertyRowData.IsValidIndex(Index))
		{
			TreeView->SetExpandedItems({ PropertyRowData[Index] }, true);
			TreeView->RequestScrollIntoView(PropertyRowData[Index]);
		}
	}

	TSharedRef<FReplicatedPropertyData> SPropertyTreeView::AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain)
	{
		return MakeShared<FReplicatedPropertyData>(MoveTemp(OwningClass), MoveTemp(PropertyChain));
	}

	void SPropertyTreeView::BuildRootPropertyRowData()
	{
		RootPropertyRowData.Empty(PropertyRowData.Num());
		for (const TSharedPtr<FReplicatedPropertyData>& PropertyData : PropertyRowData)
		{
			if (PropertyData->GetProperty().IsRootProperty())
			{
				RootPropertyRowData.Emplace(PropertyData);
			}
		}
	}

	void SPropertyTreeView::GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild)
	{
		TArray<TSharedPtr<FReplicatedPropertyData>> Children;
		
		// Not the most efficient but it should be fine.
		for (const TSharedPtr<FReplicatedPropertyData>& Data : PropertyRowData)
		{
			if (Data->GetProperty().IsDirectChildOf(ReplicatedPropertyData->GetProperty()))
			{
				Children.Add(Data);
			}
		}

		Algo::ForEach(Children, [&ProcessChild](const TSharedPtr<FReplicatedPropertyData>& Data){ ProcessChild(Data); });
	}
}

#undef LOCTEXT_NAMESPACE