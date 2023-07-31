// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/MemoryProfiler/ViewModels/MemTagNode.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/Table/ViewModels/TableCellValueSorter.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorters
////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemTagNodeSortingByType: public Insights::FTableCellValueSorter
{
public:
	FMemTagNodeSortingByType(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemTagNodeSortingByTracker : public Insights::FTableCellValueSorter
{
public:
	FMemTagNodeSortingByTracker(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemTagNodeSortingByInstanceCount : public Insights::FTableCellValueSorter
{
public:
	FMemTagNodeSortingByInstanceCount(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemTagNodeSortingByTotalInclusiveSize : public Insights::FTableCellValueSorter
{
public:
	FMemTagNodeSortingByTotalInclusiveSize(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemTagNodeSortingByTotalExclusiveSize : public Insights::FTableCellValueSorter
{
public:
	FMemTagNodeSortingByTotalExclusiveSize(TSharedRef<Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<Insights::FBaseTreeNodePtr>& NodesToSort, Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Organizers
////////////////////////////////////////////////////////////////////////////////////////////////////

/** Enumerates types of grouping or sorting for the LLM tag nodes. */
enum class EMemTagNodeGroupingMode
{
	/** Creates a single group for all LLM tags. */
	Flat,

	/** Creates one group for one letter. */
	ByName,

	/** Creates one group for each event type. */
	ByType,

	/** Creates one group for each tracker. */
	ByTracker,

	/** Group LLM tags by their hierarchy. */
	ByParent,

	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};

/** Type definition for shared pointers to instances of EMemTagNodeGroupingMode. */
typedef TSharedPtr<EMemTagNodeGroupingMode> EMemTagNodeGroupingModePtr;

////////////////////////////////////////////////////////////////////////////////////////////////////
