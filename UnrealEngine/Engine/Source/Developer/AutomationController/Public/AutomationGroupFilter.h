// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAutomationReport.h"
#include "Misc/IFilter.h"
#include "AutomationControllerSettings.h"

class FAutomationGroupFilter
	: public IFilter<const TSharedPtr<class IAutomationReport>&>
{
public:

	/** Default constructor. */
	FAutomationGroupFilter()
	{ }

	/** Default constructor with array param. */
	FAutomationGroupFilter(const TArray<FAutomatedTestFilter> InFilters)
		: Filters(InFilters)
	{ }

	/** Default constructor, group with single elements. */
	FAutomationGroupFilter(const FAutomatedTestFilter InFilter)
	{
		Filters.Add(InFilter);
	}

public:
	/**
	 * Set the list of strings the group filter checks for substrings in test display name.
	 *
	 * @param InContainsArray An array of strings to filter against test display names.
	 * @see SetMatchFromStartArray, SetMatchFromEndArray
	 */
	void SetFilters(const TArray<FAutomatedTestFilter> InFilters)
	{
		Filters = InFilters;
		ChangedEvent.Broadcast();
	}
public:

	// IFilter interface

	DECLARE_DERIVED_EVENT(FAutomationGroupFilter, IFilter< const TSharedPtr< class IAutomationReport >& >::FChangedEvent, FChangedEvent);
	virtual FChangedEvent& OnChanged() override { return ChangedEvent; }

	virtual bool PassesFilter( const TSharedPtr< IAutomationReport >& InReport ) const override
	{
		for (const FAutomatedTestFilter& Filter: Filters)
		{
			if (Filter.PassesFilter(InReport))
			{
				return true;
			}
		}

		return Filters.Num() == 0;
	}

private:

	/**	The event that broadcasts whenever a change occurs to the filter. */
	FChangedEvent ChangedEvent;

	/** The array of FAutomatedTestFilter to filter against. At least one from the list must be matched. */
	TArray<FAutomatedTestFilter> Filters;

};
