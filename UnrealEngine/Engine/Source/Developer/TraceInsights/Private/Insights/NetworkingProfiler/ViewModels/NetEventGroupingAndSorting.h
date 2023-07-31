// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/NetworkingProfiler/ViewModels/NetEventNode.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/Table/ViewModels/TableCellValueSorter.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorters
////////////////////////////////////////////////////////////////////////////////////////////////////

class FNetEventNodeSortingByEventType: public Insights::FTableCellValueSorter
{
public:
	FNetEventNodeSortingByEventType(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FNetEventNodeSortingByInstanceCount : public Insights::FTableCellValueSorter
{
public:
	FNetEventNodeSortingByInstanceCount(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FNetEventNodeSortingByTotalInclusiveSize : public Insights::FTableCellValueSorter
{
public:
	FNetEventNodeSortingByTotalInclusiveSize(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FNetEventNodeSortingByTotalExclusiveSize : public Insights::FTableCellValueSorter
{
public:
	FNetEventNodeSortingByTotalExclusiveSize(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Organizers
////////////////////////////////////////////////////////////////////////////////////////////////////

/** Enumerates types of grouping or sorting for the net event nodes. */
enum class ENetEventGroupingMode
{
	/** Creates a single group for all net events. */
	Flat,

	/** Creates one group for one letter. */
	ByName,

	/** Creates one group for each net event type. */
	ByType,

	/** Creates one group for each net event level. */
	ByLevel,

	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};

/** Type definition for shared pointers to instances of ENetEventGroupingMode. */
typedef TSharedPtr<ENetEventGroupingMode> ENetEventGroupingModePtr;

////////////////////////////////////////////////////////////////////////////////////////////////////
