// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/Table/ViewModels/BaseTreeNode.h"
#include "Insights/ViewModels/Filters.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EFilterConfiguratorNodeType
{
	/** A Filter node. */
	Filter,

	/** A group node. */
	Group,

	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Type definition for shared pointers to instances of FFilterConfiguratorNode. */
typedef TSharedPtr<class FFilterConfiguratorNode> FFilterConfiguratorNodePtr;

/** Type definition for shared references to instances of FFilterConfiguratorNode. */
typedef TSharedRef<class FFilterConfiguratorNode> FFilterConfiguratorRef;

/** Type definition for shared references to const instances of FFilterConfiguratorNode. */
typedef TSharedRef<const class FFilterConfiguratorNode> FFilterConfiguratorRefConst;

/** Type definition for weak references to instances of FFilterConfiguratorNode. */
typedef TWeakPtr<class FFilterConfiguratorNode> FFilterConfiguratorNodeWeak;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Class used to store information about a filter node
 */
class FFilterConfiguratorNode : public FBaseTreeNode
{
	INSIGHTS_DECLARE_RTTI(FFilterConfiguratorNode, FBaseTreeNode)

public:
	/** Initialization constructor for the filter configurator node. */
	FFilterConfiguratorNode(const FName InName, bool bInIsGroup);

	static TSharedRef<FFilterConfiguratorNode> DeepCopy(const FFilterConfiguratorNode& Node);

	bool operator==(const FFilterConfiguratorNode& Other) const;

	virtual ~FFilterConfiguratorNode() {}

	void SetAvailableFilters(TSharedPtr<TArray<TSharedPtr<FFilter>>> InAvailableFilters);
	TSharedPtr<TArray<TSharedPtr<FFilter>>> GetAvailableFilters() { return AvailableFilters; }

	void SetSelectedFilter(TSharedPtr<FFilter> InSelectedFilter);
	TSharedPtr<FFilter> GetSelectedFilter() const { return SelectedFilter; }

	void SetSelectedFilterOperator(TSharedPtr<IFilterOperator> InSelectedFilterOperator);
	TSharedPtr<const IFilterOperator> GetSelectedFilterOperator() const;

	const TArray<TSharedPtr<FFilterGroupOperator>>& GetFilterGroupOperators() const;
	void SetSelectedFilterGroupOperator(TSharedPtr<FFilterGroupOperator> InSelectedFilterGroupOperator) { SelectedFilterGroupOperator = InSelectedFilterGroupOperator; }
	TSharedPtr<FFilterGroupOperator> GetSelectedFilterGroupOperator() const { return SelectedFilterGroupOperator; }

	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> GetAvailableFilterOperators() const {	return AvailableFilterOperators;	}

	const FString& GetTextBoxValue() { return TextBoxValue; }
	void SetTextBoxValue(const FString& InValue);

	bool ApplyFilters(const class FFilterContext& Context) const;

	void GetUsedKeys(TSet<int32>& GetUsedKeys) const;

	void ProcessFilter();

	void Update();

	TSharedPtr<FFilterState> GetSelectedFilterState() { return FilterState; }

private:
	FFilterConfiguratorNode& operator=(const FFilterConfiguratorNode& Other);

	TSharedPtr<TArray<TSharedPtr<FFilter>>> AvailableFilters;

	TSharedPtr<FFilter> SelectedFilter;

	TSharedPtr<FFilterGroupOperator> SelectedFilterGroupOperator;

	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> AvailableFilterOperators;

	FString TextBoxValue;

	TSharedPtr<FFilterState> FilterState;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
