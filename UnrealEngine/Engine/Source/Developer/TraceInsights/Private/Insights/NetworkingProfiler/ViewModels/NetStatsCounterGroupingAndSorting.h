// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/NetworkingProfiler/ViewModels/NetStatsCounterNode.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/Table/ViewModels/TableCellValueSorter.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorters
////////////////////////////////////////////////////////////////////////////////////////////////////

class FNetStatsCounterNodeSortingByEventType: public Insights::FTableCellValueSorter
{
public:
	FNetStatsCounterNodeSortingByEventType(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FNetStatsCounterNodeSortingBySum : public Insights::FTableCellValueSorter
{
public:
	FNetStatsCounterNodeSortingBySum(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Organizers
////////////////////////////////////////////////////////////////////////////////////////////////////

/** Enumerates types of grouping or sorting for the NetStatsCounter-nodes. */
enum class ENetStatsCounterGroupingMode
{
	/** Creates a single group for all */
	Flat,

	/** Creates one group for one letter. */
	ByName,

	/** Creates one group for each type. */
	ByType,

	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};

/** Type definition for shared pointers to instances of ENetStatsCounterGroupingMode. */
typedef TSharedPtr<ENetStatsCounterGroupingMode> ENetStatsCounterGroupingModePtr;

////////////////////////////////////////////////////////////////////////////////////////////////////
