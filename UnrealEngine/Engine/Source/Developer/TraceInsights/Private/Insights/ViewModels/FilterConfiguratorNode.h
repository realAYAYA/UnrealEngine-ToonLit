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
class FFilterConfiguratorNode : public Insights::FBaseTreeNode
{
	INSIGHTS_DECLARE_RTTI(FFilterConfiguratorNode, FBaseTreeNode)

public:
	/** Initialization constructor for the filter configurator node. */
	FFilterConfiguratorNode(const FName InName, bool bInIsGroup);

	FFilterConfiguratorNode(const FFilterConfiguratorNode& Other);

	bool operator==(const FFilterConfiguratorNode& Other);

	virtual ~FFilterConfiguratorNode() {}

	void SetAvailableFilters(TSharedPtr<TArray<TSharedPtr<struct FFilter>>> InAvailableFilters);
	TSharedPtr<TArray<TSharedPtr<struct FFilter>>> GetAvailableFilters() { return AvailableFilters; }

	void SetSelectedFilter(TSharedPtr<struct FFilter> InSelectedFilter);
	TSharedPtr<struct FFilter> GetSelectedFilter() const { return SelectedFilter; }

	void SetSelectedFilterOperator(TSharedPtr<class IFilterOperator> InSelectedFilterOperator) { SelectedFilterOperator = InSelectedFilterOperator; }
	TSharedPtr<IFilterOperator> GetSelectedFilterOperator() const { return SelectedFilterOperator;	}
	const TArray<TSharedPtr<struct FFilterGroupOperator>>& GetFilterGroupOperators();

	void SetSelectedFilterGroupOperator(TSharedPtr<struct FFilterGroupOperator> InSelectedFilterGroupOperator) { SelectedFilterGroupOperator = InSelectedFilterGroupOperator; }
	TSharedPtr<struct FFilterGroupOperator> GetSelectedFilterGroupOperator() const { return SelectedFilterGroupOperator; }

	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> GetAvailableFilterOperators() const {	return AvailableFilterOperators;	}

	void DeleteChildNode(FFilterConfiguratorNodePtr InNode);

	const FString& GetTextBoxValue() { return TextBoxValue; }
	void SetTextBoxValue(const FString& InValue) { TextBoxValue = InValue; }

	void SetGroupPtrForChildren();

	bool ApplyFilters(const class FFilterContext& Context) const;

	void GetUsedKeys(TSet<int32>& GetUsedKeys) const;

	void ProcessFilter();

private:
	FFilterConfiguratorNode& operator=(const FFilterConfiguratorNode& Other);

	TSharedPtr<TArray<TSharedPtr<struct FFilter>>> AvailableFilters;

	TSharedPtr<struct FFilter> SelectedFilter;

	TSharedPtr<class IFilterOperator> SelectedFilterOperator;

	TSharedPtr<struct FFilterGroupOperator> SelectedFilterGroupOperator;

	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> AvailableFilterOperators;

	FString TextBoxValue;

	// The textbox value, converted to its data type
	FFilterContext::ContextData FilterValue;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
