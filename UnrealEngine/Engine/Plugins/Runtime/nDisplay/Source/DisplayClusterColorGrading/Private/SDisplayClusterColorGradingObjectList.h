// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Framework/SlateDelegates.h"
#include "GameFramework/Actor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class STableViewBase;

template<class T>
class SListView;

DECLARE_DELEGATE_TwoParams(FOnColorGradingItemEnabledChanged, TSharedPtr<struct FDisplayClusterColorGradingListItem>, bool);

/** A structure to store references to color gradable actors and components */
struct FDisplayClusterColorGradingListItem
{
	/** The actor that is color gradable */
	TWeakObjectPtr<AActor> Actor;

	/** The component that is color gradable */
	TWeakObjectPtr<UActorComponent> Component;

	/** Attribute that retrieves whether color grading is enabled on the color gradable item */
	TAttribute<bool> IsItemEnabled;

	/** Delegate raised when the enabled state of the color gradable item has been changed */
	FOnColorGradingItemEnabledChanged OnItemEnabledChanged;

	FDisplayClusterColorGradingListItem(AActor* InActor, UActorComponent* InComponent = nullptr)
		: Actor(InActor)
		, Component(InComponent)
		, IsItemEnabled(false)
	{ }

	/** Less than operator overload that compares list items alphabetically by their display names */
	bool operator<(const FDisplayClusterColorGradingListItem& Other) const;
};

typedef TSharedPtr<FDisplayClusterColorGradingListItem> FDisplayClusterColorGradingListItemRef;

/** Displays a list of color gradable items */
class SDisplayClusterColorGradingObjectList : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_ThreeParams(FOnSelectionChanged, TSharedRef<SDisplayClusterColorGradingObjectList>, FDisplayClusterColorGradingListItemRef, ESelectInfo::Type);

public:
	SLATE_BEGIN_ARGS(SDisplayClusterColorGradingObjectList) {}
		SLATE_ARGUMENT(const TArray<FDisplayClusterColorGradingListItemRef>*, ColorGradingItemsSource)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refreshes the list, updating the UI to reflect the current state of the source items list*/
	void RefreshList();

	/** Gets a list of currently selected items */
	TArray<FDisplayClusterColorGradingListItemRef> GetSelectedItems();

	/** Selects the specified list of items */
	void SetSelectedItems(const TArray<FDisplayClusterColorGradingListItemRef>& InSelectedItems);

private:
	/** Generates the table row widget for the specified list item */
	TSharedRef<ITableRow> GenerateListItemRow(FDisplayClusterColorGradingListItemRef Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Raised when the internal list view's selection has changed */
	void OnSelectionChanged(FDisplayClusterColorGradingListItemRef SelectedItem, ESelectInfo::Type SelectInfo);

private:
	/** Internal list view used to display the list of color gradable items */
	TSharedPtr<SListView<FDisplayClusterColorGradingListItemRef>> ListView;

	/** A delegate that is raised when the list of selected items is changed */
	FOnSelectionChanged OnSelectionChangedDelegate;
};