// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerFilterBar.h"

const FName SSceneOutlinerFilterBar::SharedIdentifier("SceneOutlinerFilterBarSharedSettings");
SSceneOutlinerFilterBar::FCustomTextFilterEvent SSceneOutlinerFilterBar::CustomTextFilterEvent;

void SSceneOutlinerFilterBar::Construct( const FArguments& InArgs )
{
	bUseSharedSettings = InArgs._UseSharedSettings;
	this->CategoryToExpand = InArgs._CategoryToExpand;

	SFilterBar<SceneOutliner::FilterBarType>::FArguments Args;
	Args._OnFilterChanged = InArgs._OnFilterChanged;
	Args._CustomFilters = InArgs._CustomFilters;
	Args._UseDefaultAssetFilters = false;
	Args._CustomClassFilters = InArgs._CustomClassFilters;
	Args._CreateTextFilter = InArgs._CreateTextFilter;
	Args._FilterSearchBox = InArgs._FilterSearchBox;
	Args._FilterBarIdentifier = InArgs._FilterBarIdentifier;
	Args._OnCompareItemWithClassNames = InArgs._OnCompareItemWithClassNames;
	Args._OnConvertItemToAssetData = InArgs._OnConvertItemToAssetData;
	Args._FilterPillStyle = EFilterPillStyle::Basic;

	SFilterBar<SceneOutliner::FilterBarType>::Construct(Args);
	
	/* If we are using shared settings, add a default config for the shared settings in case it doesnt exist
	 * This needs to go after SAssetFilterBar<FAssetFilterType>::Construct() to ensure UFilterBarConfig is valid
	 */
	if(bUseSharedSettings)
	{
		UFilterBarConfig::Get()->FilterBars.FindOrAdd(SharedIdentifier);

		// Bind our delegate for when another SSceneOutlinerFilterBar creates a custom text filter, so we can sync our list
		CustomTextFilterEvent.AddSP(this, &SSceneOutlinerFilterBar::OnExternalCustomTextFilterCreated);
	}
}

void SSceneOutlinerFilterBar::SaveSettings()
{
	// If this instance doesn't want to use the shared settings, save the settings normally
	if(!bUseSharedSettings)
	{
		SAssetFilterBar<SceneOutliner::FilterBarType>::SaveSettings();
		return;
	}

	if(FilterBarIdentifier.IsNone())
	{
		UE_LOG(LogSlate, Error, TEXT("SSceneOutlinerFilterBar Requires that you specify a FilterBarIdentifier to save settings"));
		return;
	}

	// Get the settings unique to this instance and the common settings
	FFilterBarSettings* InstanceSettings = &UFilterBarConfig::Get()->FilterBars.FindOrAdd(FilterBarIdentifier);
	FFilterBarSettings* SharedSettings = &UFilterBarConfig::Get()->FilterBars.FindOrAdd(SharedIdentifier);

	// Empty both the configs, we are just going to re-save everything there
	InstanceSettings->Empty();
	SharedSettings->Empty();

	// Save all the programatically added filters normally
	SaveFilters(InstanceSettings);

	/** For each custom text filter: Save the filterdata into the common settings, so that all instances that use it
	 *	are synced.
	 *	For each CHECKED custom text filter: Save just the filter name, and the checked and active state into the
	 *	instance settings. Those are specific to this instance (i.e we don't want a filter to be active in all
	 *	instances if activated in one)
	 */
	for (const TSharedRef<ICustomTextFilter<SceneOutliner::FilterBarType>>& CustomTextFilter : this->CustomTextFilters)
	{
		// Get the actual FFilterBase
		TSharedRef<FFilterBase<SceneOutliner::FilterBarType>> CustomFilter = CustomTextFilter->GetFilter().ToSharedRef();
			
		// Is the filter "checked", i.e visible in the filter bar
		bool bIsChecked = IsFrontendFilterInUse(CustomFilter);

		// Is the filter "active", i.e visible and enabled in the filter bar
		bool bIsActive = IsFilterActive(CustomFilter);

		// Get the data associated with this filter
		FCustomTextFilterData FilterData = CustomTextFilter->CreateCustomTextFilterData();

		// Just save the filter data into the shared settings
		FCustomTextFilterState SharedFilterState;
		SharedFilterState.FilterData = FilterData;
		SharedSettings->CustomTextFilters.Add(SharedFilterState);

		if(bIsChecked)
		{
			// Create a duplicate filter data that just contains the filter label for this instance to know
			FCustomTextFilterData InstanceFilterData;
			InstanceFilterData.FilterLabel = FilterData.FilterLabel;
			
			// Just save the filter name and enabled/active state into the shared settings
			FCustomTextFilterState InstanceFilterState;
			InstanceFilterState.bIsChecked = bIsChecked;
			InstanceFilterState.bIsActive = bIsActive;
			InstanceFilterState.FilterData = InstanceFilterData;
			
			InstanceSettings->CustomTextFilters.Add(InstanceFilterState);
		}
	}

	SaveConfig();
}

void SSceneOutlinerFilterBar::LoadSettings()
{
	// If this instance doesn't want to use the shared settings, load the settings normally
	if(!bUseSharedSettings)
	{
		SAssetFilterBar<SceneOutliner::FilterBarType>::LoadSettings();
		return;
	}

	if(FilterBarIdentifier.IsNone())
	{
		UE_LOG(LogSlate, Error, TEXT("SSceneOutlinerFilterBar Requires that you specify a FilterBarIdentifier to load settings"));
		return;
	}

	// Get the settings unique to this instance and the common settings
	const FFilterBarSettings* InstanceSettings = UFilterBarConfig::Get()->FilterBars.Find(FilterBarIdentifier);
	const FFilterBarSettings* SharedSettings = UFilterBarConfig::Get()->FilterBars.Find(SharedIdentifier);

	// Load the filters specified programatically normally
	LoadFilters(InstanceSettings);

	// Load the custom text filters from the shared settings
	LoadCustomTextFilters(SharedSettings);
	
	// From the instance settings, get each checked filter and set the checked and active state
	for(const FCustomTextFilterState& FilterState : InstanceSettings->CustomTextFilters)
	{
		if(!RestoreCustomTextFilterState(FilterState))
		{
			UE_LOG(LogSlate, Warning, TEXT("SSceneOutlinerFilterBar was unable to load the following custom text filter: %s"), *FilterState.FilterData.FilterLabel.ToString());
		}
	}

	this->OnFilterChanged.ExecuteIfBound();
}

UAssetFilterBarContext* SSceneOutlinerFilterBar::CreateAssetFilterBarContext()
{
	UAssetFilterBarContext* AssetFilterBarContext = SFilterBar<const ISceneOutlinerTreeItem&>::CreateAssetFilterBarContext();
	AssetFilterBarContext->MenuExpansion = CategoryToExpand;

	return AssetFilterBarContext;
}

void SSceneOutlinerFilterBar::OnCreateCustomTextFilter(const FCustomTextFilterData& InFilterData, bool bApplyFilter)
{
	SAssetFilterBar<SceneOutliner::FilterBarType>::OnCreateCustomTextFilter(InFilterData, bApplyFilter);

	// If we are using shared settings (i.e sharing custom text filters) broadcast the event for all other instances to update
	if(bUseSharedSettings)
	{
		// First save the shared settings for other instances to use
		SaveSettings();
		CustomTextFilterEvent.Broadcast(AsShared());
	}
}

void SSceneOutlinerFilterBar::OnModifyCustomTextFilter(const FCustomTextFilterData& InFilterData, TSharedPtr<ICustomTextFilter<SceneOutliner::FilterBarType>> InFilter)
{
	SAssetFilterBar<SceneOutliner::FilterBarType>::OnModifyCustomTextFilter(InFilterData, InFilter);

	// If we are using shared settings (i.e sharing custom text filters) broadcast the event for all other instances to update
	if(bUseSharedSettings)
	{
		// First save the shared settings for other instances to use
		SaveSettings();
		CustomTextFilterEvent.Broadcast(AsShared());
	}
}

void SSceneOutlinerFilterBar::OnDeleteCustomTextFilter(const TSharedPtr<ICustomTextFilter<SceneOutliner::FilterBarType>> InFilter)
{
	SAssetFilterBar<SceneOutliner::FilterBarType>::OnDeleteCustomTextFilter(InFilter);

	// If we are using shared settings (i.e sharing custom text filters) broadcast the event for all other instances to update
	if(bUseSharedSettings)
	{
		// First save the shared settings for other instances to use
		SaveSettings();
		CustomTextFilterEvent.Broadcast(AsShared());
	}
}

void SSceneOutlinerFilterBar::LoadCustomTextFilters(const FFilterBarSettings* FilterBarConfig)
{
	CustomTextFilters.Empty();
	
	// Extract just the filter data from the common settings
	for(const FCustomTextFilterState& FilterState : FilterBarConfig->CustomTextFilters)
	{
		// Create an ICustomTextFilter using the provided delegate
		TSharedRef<ICustomTextFilter<SceneOutliner::FilterBarType>> NewTextFilter = this->CreateTextFilter.Execute().ToSharedRef();

		// Get the actual FFilterBase
		TSharedRef<FFilterBase<SceneOutliner::FilterBarType>> NewFilter = NewTextFilter->GetFilter().ToSharedRef();

		// Set the internals of the custom text filter from what we have saved
		NewTextFilter->SetFromCustomTextFilterData(FilterState.FilterData);

		// Add this to our list of custom text filters
		CustomTextFilters.Add(NewTextFilter);
	}
}


bool SSceneOutlinerFilterBar::RestoreCustomTextFilterState(const FCustomTextFilterState& InFilterState)
{
	// Find the filter associated with the current instance data from our list of custom text filters
	TSharedRef< ICustomTextFilter<SceneOutliner::FilterBarType> >* Filter =
		CustomTextFilters.FindByPredicate([&InFilterState](const TSharedRef< ICustomTextFilter<SceneOutliner::FilterBarType> >& Element)
	{
		return Element->CreateCustomTextFilterData().FilterLabel.EqualTo(InFilterState.FilterData.FilterLabel);
	});

	if(!Filter)
	{
		return false;
	}

	// Get the actual FFilterBase
	TSharedRef<FFilterBase<SceneOutliner::FilterBarType>> ActualFilter = Filter->Get().GetFilter().ToSharedRef();

	// Add it to the filter bar, since if it exists in this list it is checked
	TSharedRef<SFilter> AddedFilter = this->AddFilterToBar(ActualFilter);

	// Set the filter as active if it was previously
	AddedFilter->SetEnabled(InFilterState.bIsActive, false);
	this->SetFrontendFilterActive(ActualFilter, InFilterState.bIsActive);

	return true;
}

void SSceneOutlinerFilterBar::OnExternalCustomTextFilterCreated(TSharedPtr<SWidget> BroadcastingFilterBar)
{
	// Do nothing if we aren't using shared settings or if the event was broadcasted by this filter list
	if(!bUseSharedSettings || BroadcastingFilterBar == AsShared())
	{
		return;
	}

	/* We are going to remove all our custom text filters and re-load them from the shared settings, since a different
	 * instance modified them.
	 */

	// To preserve the state of any checked/active custom text filters
	TArray<FCustomTextFilterState> CurrentCustomTextFilterStates;
	
	for (const TSharedRef<ICustomTextFilter<SceneOutliner::FilterBarType>>& CustomTextFilter : this->CustomTextFilters)
	{
		// Get the actual FFilterBase
		TSharedRef<FFilterBase<SceneOutliner::FilterBarType>> CustomFilter = CustomTextFilter->GetFilter().ToSharedRef();
			
		// Is the filter "checked", i.e visible in the filter bar
		bool bIsChecked = IsFrontendFilterInUse(CustomFilter);

		// Is the filter "active", i.e visible and enabled in the filter bar
		bool bIsActive = IsFilterActive(CustomFilter);

		// Only save the state if the filter is checked so we can restore it
		if(bIsChecked)
		{
			/* Remove the filter from the list (calling SBasicFilterBar::RemoveFilter because we get a compiler error
			*  due to SAssetFilterBar overriding RemoveFilter that takes in an SFilter that hides the parent class function
			*/
			SBasicFilterBar<SceneOutliner::FilterBarType>::RemoveFilter(CustomFilter, false);
			
			FCustomTextFilterState FilterState;
			FilterState.FilterData = CustomTextFilter->CreateCustomTextFilterData();
			FilterState.bIsChecked = bIsChecked;
			FilterState.bIsActive = bIsActive;
			
			CurrentCustomTextFilterStates.Add(FilterState);
		}
	}

	// Get the shared settings and reload the filters
	FFilterBarSettings* SharedSettings = &UFilterBarConfig::Get()->FilterBars.FindOrAdd(SharedIdentifier);
	LoadCustomTextFilters(SharedSettings);

	// Restore the state of any previously active ones
	for(const FCustomTextFilterState& SavedFilterState : CurrentCustomTextFilterStates)
	{
		RestoreCustomTextFilterState(SavedFilterState);
	}

	// Save the settings for this instance
	SaveSettings();
}