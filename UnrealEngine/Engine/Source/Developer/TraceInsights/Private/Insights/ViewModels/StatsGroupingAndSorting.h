// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/ViewModels/StatsNode.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/Table/ViewModels/TableCellValueSorter.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorters
////////////////////////////////////////////////////////////////////////////////////////////////////

class FStatsNodeSortingByStatsType : public Insights::FTableCellValueSorter
{
public:
	FStatsNodeSortingByStatsType(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FStatsNodeSortingByDataType : public Insights::FTableCellValueSorter
{
public:
	FStatsNodeSortingByDataType(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FStatsNodeSortingByCount : public Insights::FTableCellValueSorter
{
public:
	FStatsNodeSortingByCount(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Organizers
////////////////////////////////////////////////////////////////////////////////////////////////////

/** Enumerates types of grouping or sorting for the stats nodes. */
enum class EStatsGroupingMode
{
	/** Creates a single group for all timers. */
	Flat,

	/** Creates one group for one letter. */
	ByName,

	/** Creates groups based on stats metadata group names. */
	ByMetaGroupName,

	/** Creates one group for each node type. */
	ByType,

	/** Creates one group for each data type. */
	ByDataType,

	/** Creates one group for each logarithmic range ie. 0, [1 .. 10), [10 .. 100), [100 .. 1K), etc. */
	ByCount,

	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};

/** Type definition for shared pointers to instances of EStatsGroupingMode. */
typedef TSharedPtr<EStatsGroupingMode> EStatsGroupingModePtr;

////////////////////////////////////////////////////////////////////////////////////////////////////
