// Copyright Epic Games, Inc. All Rights Reserved.


#include "SFilterList.h"

#include "AssetRegistry/ARFilter.h"
#include "ContentBrowserDataFilter.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserFrontEndFilterExtension.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserItemData.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserUtils.h"
#include "Filters/FilterBarConfig.h"
#include "Filters/SAssetFilterBar.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "FrontendFilterBase.h"
#include "FrontendFilters.h"
#include "HAL/PlatformCrt.h"
#include "IContentBrowserDataModule.h"
#include "Input/Events.h"
#include "Internationalization/Internationalization.h"
#include "Layout/WidgetPath.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "SlateGlobals.h"
#include "SlotBase.h"
#include "Styling/SlateTypes.h"
#include "Templates/Casts.h"
#include "Templates/TypeHash.h"
#include "ToolMenu.h"
#include "ToolMenuContext.h"
#include "ToolMenuDelegates.h"
#include "ToolMenus.h"
#include "Trace/Detail/Channel.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealNames.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"

class SWidget;
struct FGeometry;

#define LOCTEXT_NAMESPACE "ContentBrowser"

/////////////////////
// SFilterList
/////////////////////

const FName SFilterList::SharedIdentifier("FilterListSharedSettings");
SFilterList::FCustomTextFilterEvent SFilterList::CustomTextFilterEvent;

void SFilterList::Construct( const FArguments& InArgs )
{
	bUseSharedSettings = InArgs._UseSharedSettings;
	OnFilterBarLayoutChanging = InArgs._OnFilterBarLayoutChanging;
	this->OnFilterChanged = InArgs._OnFilterChanged;
	this->ActiveFilters = InArgs._FrontendFilters;
	InitialClassFilters = InArgs._InitialClassFilters; 

	TSharedPtr<FFrontendFilterCategory> DefaultCategory = MakeShareable( new FFrontendFilterCategory(LOCTEXT("FrontendFiltersCategory", "Other Filters"), LOCTEXT("FrontendFiltersCategoryTooltip", "Filter assets by all filters in this category.")) );
	
	// Add all built-in frontend filters here
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_CheckedOut(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_Modified(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_Writable(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_ShowOtherDevelopers(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_ReplicatedBlueprint(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_ShowRedirectors(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_InUseByLoadedLevels(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_UsedInAnyLevel(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_NotUsedInAnyLevel(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_NotUsedInAnyAsset(DefaultCategory)) );
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_ArbitraryComparisonOperation(DefaultCategory)) );
	AllFrontendFilters_Internal.Add(MakeShareable(new FFrontendFilter_Recent(DefaultCategory)));
	AllFrontendFilters_Internal.Add( MakeShareable(new FFrontendFilter_NotSourceControlled(DefaultCategory)) );
	AllFrontendFilters_Internal.Add(MakeShareable(new FFrontendFilter_VirtualizedData(DefaultCategory)));
	AllFrontendFilters_Internal.Add(MakeShared<FFrontendFilter_Unsupported>(DefaultCategory));

	// Add any global user-defined frontend filters
	for (TObjectIterator<UContentBrowserFrontEndFilterExtension> ExtensionIt(RF_NoFlags); ExtensionIt; ++ExtensionIt)
	{
		if (UContentBrowserFrontEndFilterExtension* PotentialExtension = *ExtensionIt)
		{
			if (PotentialExtension->HasAnyFlags(RF_ClassDefaultObject) && !PotentialExtension->GetClass()->HasAnyClassFlags(CLASS_Deprecated | CLASS_Abstract))
			{
				// Grab the filters
				TArray< TSharedRef<FFrontendFilter> > ExtendedFrontendFilters;
				PotentialExtension->AddFrontEndFilterExtensions(DefaultCategory, ExtendedFrontendFilters);
				AllFrontendFilters_Internal.Append(ExtendedFrontendFilters);

				// Grab the categories
				for (const TSharedRef<FFrontendFilter>& FilterRef : ExtendedFrontendFilters)
				{
					TSharedPtr<FFilterCategory> Category = FilterRef->GetCategory();
					if (Category.IsValid())
					{
						this->AllFilterCategories.AddUnique(Category);
					}
				}
			}
		}
	}

	// Add in filters specific to this invocation
	for (const TSharedRef<FFrontendFilter>& Filter : InArgs._ExtraFrontendFilters)
	{
		if (TSharedPtr<FFilterCategory> Category = Filter->GetCategory())
		{
			this->AllFilterCategories.AddUnique(Category);
		}

		AllFrontendFilters_Internal.Add(Filter);
	}

	this->AllFilterCategories.AddUnique(DefaultCategory);

	// Add the local copy of all filters to SFilterBar's copy of all filters
	for(TSharedRef<FFrontendFilter> FrontendFilter : AllFrontendFilters_Internal)
	{
		this->AddFilter(FrontendFilter);
	}
	
	SAssetFilterBar<FAssetFilterType>::FArguments Args;

	/** Explicitly setting this to true as it should ALWAYS be true for SFilterList */
	Args._UseDefaultAssetFilters = true;
	Args._OnFilterChanged = this->OnFilterChanged;
	Args._CreateTextFilter = InArgs._CreateTextFilter;
	Args._FilterBarIdentifier = InArgs._FilterBarIdentifier;
	Args._FilterBarLayout = InArgs._FilterBarLayout;
	Args._CanChangeOrientation = InArgs._CanChangeOrientation;
	Args._OnExtendAddFilterMenu = InArgs._OnExtendAddFilterMenu;
	Args._FilterMenuName = FName("ContentBrowser.FilterMenu");
	Args._DefaultMenuExpansionCategory = InArgs._DefaultMenuExpansionCategory;
	Args._bUseSectionsForCustomCategories = InArgs._bUseSectionsForCustomCategories;

	SAssetFilterBar<FAssetFilterType>::Construct(Args);

	/* If we are using shared settings, add a default config for the shared settings in case it doesnt exist
	 * This needs to go after SAssetFilterBar<FAssetFilterType>::Construct() to ensure UFilterBarConfig is valid
	 */
	if(bUseSharedSettings)
	{
		UFilterBarConfig::Get()->FilterBars.FindOrAdd(SharedIdentifier);

		// Bind our delegate for when another SFilterList creates a custom text filter, so we can sync our list
		CustomTextFilterEvent.AddSP(this, &SFilterList::OnExternalCustomTextFilterCreated);
	}
	
}

const TArray<UClass*>& SFilterList::GetInitialClassFilters()
{
	return InitialClassFilters;
}

TSharedPtr<FFrontendFilter> SFilterList::GetFrontendFilter(const FString& InName) const
{
	for (const TSharedRef<FFrontendFilter>& Filter : AllFrontendFilters_Internal)
	{
		if (Filter->GetName() == InName)
		{
			return Filter;
		}
	}
	return TSharedPtr<FFrontendFilter>();
}

TSharedRef<SWidget> SFilterList::ExternalMakeAddFilterMenu()
{
	return SAssetFilterBar<FAssetFilterType>::MakeAddFilterMenu();
}

void SFilterList::DisableFiltersThatHideItems(TArrayView<const FContentBrowserItem> ItemList)
{
	if (HasAnyFilters() && ItemList.Num() > 0)
	{
		// Determine if we should disable backend filters. If any item fails the combined backend filter, disable them all.
		bool bDisableAllBackendFilters = false;
		{
			FContentBrowserDataCompiledFilter CompiledDataFilter;
			{
				static const FName RootPath = "/";

				UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

				FContentBrowserDataFilter DataFilter;
				DataFilter.bRecursivePaths = true;
				ContentBrowserUtils::AppendAssetFilterToContentBrowserFilter(GetCombinedBackendFilter(), nullptr, nullptr, DataFilter);

				ContentBrowserData->CompileFilter(RootPath, DataFilter, CompiledDataFilter);
			}

			for (const FContentBrowserItem& Item : ItemList)
			{
				if (!Item.IsFile())
				{
					continue;
				}

				FContentBrowserItem::FItemDataArrayView InternalItems = Item.GetInternalItems();
				for (const FContentBrowserItemData& InternalItemRef : InternalItems)
				{
					UContentBrowserDataSource* ItemDataSource = InternalItemRef.GetOwnerDataSource();

					FContentBrowserItemData InternalItem = InternalItemRef;
					ItemDataSource->ConvertItemForFilter(InternalItem, CompiledDataFilter);

					if (!ItemDataSource->DoesItemPassFilter(InternalItem, CompiledDataFilter))
					{
						bDisableAllBackendFilters = true;
						break;
					}
				}

				if (bDisableAllBackendFilters)
				{
					break;
				}
			}
		}

		// Iterate over all enabled filters and disable any frontend filters that would hide any of the supplied assets
		bool ExecuteOnFilterChanged = false;
		for (const TSharedPtr<SFilter> Filter : Filters)
		{
			if (Filter->IsEnabled())
			{
				if (const TSharedPtr<FFilterBase<FAssetFilterType>>& FrontendFilter = Filter->GetFrontendFilter())
				{
					for (const FContentBrowserItem& Item : ItemList)
					{
						if (!FrontendFilter->IsInverseFilter() && !FrontendFilter->PassesFilter(Item))
						{
							// This is a frontend filter and at least one asset did not pass.
							Filter->SetEnabled(false, false);
							SetFrontendFilterActive(FrontendFilter.ToSharedRef(), false);
							ExecuteOnFilterChanged = true;
						}
					}
				}
			}
		}

		// Disable all backend filters if it was determined that the combined backend filter hides any of the assets
		if (bDisableAllBackendFilters)
		{
			for(const TSharedPtr<SAssetFilter> AssetFilter : AssetFilters)
			{
				if(AssetFilter.IsValid())
				{
					FARFilter BackendFilter = AssetFilter->GetBackendFilter();
					if (!BackendFilter.IsEmpty())
					{
						AssetFilter->SetEnabled(false, false);
						ExecuteOnFilterChanged = true;
					}
				}
			}
		}

		if (ExecuteOnFilterChanged)
		{
			OnFilterChanged.ExecuteIfBound();
		}
	}
}

void SFilterList::SetFrontendFilterCheckState(const TSharedPtr<FFrontendFilter>& InFrontendFilter, ECheckBoxState CheckState)
{
	this->SetFilterCheckState(InFrontendFilter, CheckState);
}

ECheckBoxState SFilterList::GetFrontendFilterCheckState(const TSharedPtr<FFrontendFilter>& InFrontendFilter) const
{
	return this->GetFilterCheckState(InFrontendFilter);
}

bool SFilterList::IsFrontendFilterActive(const TSharedPtr<FFrontendFilter>& InFrontendFilter) const
{
	return this->IsFilterActive(InFrontendFilter);
}

bool IsFilteredByPicker(const TArray<UClass*>& FilterClassList, UClass* TestClass)
{
	if (FilterClassList.Num() == 0)
	{
		return false;
	}
	for (const UClass* Class : FilterClassList)
	{
		if (TestClass->IsChildOf(Class))
		{
			return false;
		}
	}
	return true;
}

UAssetFilterBarContext* SFilterList::CreateAssetFilterBarContext()
{
	UAssetFilterBarContext* AssetFilterBarContext = SAssetFilterBar<const FContentBrowserItem&>::CreateAssetFilterBarContext();

	AssetFilterBarContext->OnFilterAssetType = FOnFilterAssetType::CreateLambda([this](UClass *TestClass)
	{
		return !IsFilteredByPicker(this->InitialClassFilters, TestClass);
	});
	
	return AssetFilterBarContext;
}

void SFilterList::CreateCustomFilterDialog(const FText& InText)
{
	CreateCustomTextFilterFromSearch(InText);
}

void SFilterList::OnCreateCustomTextFilter(const FCustomTextFilterData& InFilterData, bool bApplyFilter)
{
	SAssetFilterBar<FAssetFilterType>::OnCreateCustomTextFilter(InFilterData, bApplyFilter);

	// If we are using shared settings (i.e sharing custom text filters) broadcast the event for all other instances to update
	if(bUseSharedSettings)
	{
		// First save the shared settings for other instances to use
		SaveSettings();
		CustomTextFilterEvent.Broadcast(AsShared());
	}
}

void SFilterList::OnModifyCustomTextFilter(const FCustomTextFilterData& InFilterData, TSharedPtr<ICustomTextFilter<FAssetFilterType>> InFilter)
{
	SAssetFilterBar<FAssetFilterType>::OnModifyCustomTextFilter(InFilterData, InFilter);

	// If we are using shared settings (i.e sharing custom text filters) broadcast the event for all other instances to update
	if(bUseSharedSettings)
	{
		// First save the shared settings for other instances to use
		SaveSettings();
		CustomTextFilterEvent.Broadcast(AsShared());
	}
}

void SFilterList::OnDeleteCustomTextFilter(const TSharedPtr<ICustomTextFilter<FAssetFilterType>> InFilter)
{
	SAssetFilterBar<FAssetFilterType>::OnDeleteCustomTextFilter(InFilter);

	// If we are using shared settings (i.e sharing custom text filters) broadcast the event for all other instances to update
	if(bUseSharedSettings)
	{
		// First save the shared settings for other instances to use
		SaveSettings();
		CustomTextFilterEvent.Broadcast(AsShared());
	}
}

bool SFilterList::RestoreCustomTextFilterState(const FCustomTextFilterState& InFilterState)
{
	// Find the filter associated with the current instance data from our list of custom text filters
	TSharedRef< ICustomTextFilter<FAssetFilterType> >* Filter =
		CustomTextFilters.FindByPredicate([&InFilterState](const TSharedRef< ICustomTextFilter<FAssetFilterType> >& Element)
	{
		return Element->CreateCustomTextFilterData().FilterLabel.EqualTo(InFilterState.FilterData.FilterLabel);
	});

	// Return if we couldn't find the filter we are trying to restore
	if(!Filter)
	{
		return false;
	}

	// Get the actual FFilterBase
	TSharedRef<FFilterBase<FAssetFilterType>> ActualFilter = Filter->Get().GetFilter().ToSharedRef();

	// Add it to the filter bar, since if it exists in this list it is checked
	TSharedRef<SFilter> AddedFilter = this->AddFilterToBar(ActualFilter);

	// Set the filter as active if it was previously
	AddedFilter->SetEnabled(InFilterState.bIsActive, false);
	this->SetFrontendFilterActive(ActualFilter, InFilterState.bIsActive);

	return true;
}

void SFilterList::OnExternalCustomTextFilterCreated(TSharedPtr<SWidget> BroadcastingFilterList)
{
	// Do nothing if we aren't using shared settings or if the event was broadcasted by this filter list
	if(!bUseSharedSettings || BroadcastingFilterList == AsShared())
	{
		return;
	}

	/* We are going to remove all our custom text filters and re-load them from the shared settings, since a different
	 * instance modified them.
	 */

	// To preserve the state of any checked/active custom text filters
	TArray<FCustomTextFilterState> CurrentCustomTextFilterStates;
	
	for (const TSharedRef<ICustomTextFilter<FAssetFilterType>>& CustomTextFilter : this->CustomTextFilters)
	{
		// Get the actual FFilterBase
		TSharedRef<FFilterBase<FAssetFilterType>> CustomFilter = CustomTextFilter->GetFilter().ToSharedRef();
			
		// Is the filter "checked", i.e visible in the filter bar
		bool bIsChecked = IsFrontendFilterInUse(CustomFilter);

		// Is the filter "active", i.e visible and enabled in the filter bar
		bool bIsActive = IsFilterActive(CustomFilter);

		// Only save the state if the filter is checked so we can restore it
		if(bIsChecked)
		{
			/* Remove the filter from the list (calling SBasicFilterBar::RemoveFilter because we get a compiler error
			*  due to SAssetFilterBar overriding RemoveFilter that takes in an SFilter that hides the parent class function)
			*/
			SBasicFilterBar<FAssetFilterType>::RemoveFilter(CustomFilter, false);
			
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
}

void SFilterList::UpdateCustomTextFilterIncludes(const bool InIncludeClassName, const bool InIncludeAssetPath, const bool InIncludeCollectionNames)
{
	bIncludeClassName = InIncludeClassName;
	bIncludeAssetPath = InIncludeAssetPath;
	bIncludeCollectionNames = InIncludeCollectionNames;

	for (TSharedPtr<ICustomTextFilter<FAssetFilterType>> CustomTextFilter : CustomTextFilters)
	{
		// This is a safe cast, since SFilterList will always and only have FFilterListCustomTextFilters
		if(TSharedPtr<FFrontendFilter_CustomText> FilterListCustomTextFilter = StaticCastSharedPtr<FFrontendFilter_CustomText>(CustomTextFilter))
		{
			FilterListCustomTextFilter->UpdateCustomTextFilterIncludes(bIncludeClassName, bIncludeAssetPath, bIncludeCollectionNames);
		}
	}
}

void SFilterList::SaveSettings()
{
	// If this instance doesn't want to use the shared settings, save the settings normally
	if(!bUseSharedSettings)
	{
		SAssetFilterBar<FAssetFilterType>::SaveSettings();
		return;
	}

	if(FilterBarIdentifier.IsNone())
	{
		UE_LOG(LogSlate, Error, TEXT("SFilterList Requires that you specify a FilterBarIdentifier to save settings"));
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
	for (const TSharedRef<ICustomTextFilter<FAssetFilterType>>& CustomTextFilter : this->CustomTextFilters)
	{
		// Get the actual FFilterBase
		TSharedRef<FFilterBase<FAssetFilterType>> CustomFilter = CustomTextFilter->GetFilter().ToSharedRef();
			
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

	// Only save the orientation if we allow dynamic modification and saving
	InstanceSettings->bIsLayoutSaved = this->bCanChangeOrientation;
	if(this->bCanChangeOrientation)
	{
		InstanceSettings->FilterBarLayout = this->FilterBarLayout;
	}

	SaveConfig();
}

void SFilterList::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	// Workaround for backwards compatibility with filters that save settings until they are ported to EditorConfig
	for ( const TSharedPtr<SFilter> Filter : this->Filters )
	{
		const FString FilterName = Filter->GetFilterName();

		// If it is a FrontendFilter
		if ( Filter->GetFrontendFilter().IsValid() )
		{
			const TSharedPtr<FFilterBase<FAssetFilterType>>& FrontendFilter = Filter->GetFrontendFilter();
			const FString CustomSettingsString = FString::Printf(TEXT("%s.CustomSettings.%s"), *SettingsString, *FilterName);
			FrontendFilter->SaveSettings(IniFilename, IniSection, CustomSettingsString);
		}
	}
	
	SaveSettings();
}

void SFilterList::LoadSettings(const FName& InInstanceName, const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	// Workaround for backwards compatibility with filters that save settings until they are ported to EditorConfig
	for ( auto FrontendFilterIt = this->AllFrontendFilters.CreateIterator(); FrontendFilterIt; ++FrontendFilterIt )
	{
		TSharedRef<FFilterBase<FAssetFilterType>>& FrontendFilter = *FrontendFilterIt;
		const FString& FilterName = FrontendFilter->GetName();

		const FString CustomSettingsString = FString::Printf(TEXT("%s.CustomSettings.%s"), *SettingsString, *FilterName);
		FrontendFilter->LoadSettings(IniFilename, IniSection, CustomSettingsString);
	}
	
	LoadSettings(InInstanceName);
}


void SFilterList::LoadSettings(const FName& InInstanceName)
{
	// If this instance doesn't want to use the shared settings, load the settings normally
	if(!bUseSharedSettings)
	{
		SAssetFilterBar<FAssetFilterType>::LoadSettings();
		return;
	}

	if(InInstanceName.IsNone())
	{
		UE_LOG(LogSlate, Error, TEXT("SFilterList Requires that you specify a FilterBarIdentifier to load settings"));
		return;
	}

	// Get the settings unique to this instance and the common settings
	const FFilterBarSettings* InstanceSettings = UFilterBarConfig::Get()->FilterBars.Find(InInstanceName);
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
			UE_LOG(LogSlate, Warning, TEXT("SFilterList was unable to load the following custom text filter: %s"), *FilterState.FilterData.FilterLabel.ToString());
		}
	}

	if(InstanceSettings->bIsLayoutSaved)
	{
		FilterBarLayout = InstanceSettings->FilterBarLayout;
	}

	// We want to call this even if the Layout isn't saved, to make sure OnFilterBarLayoutChanging is fired
	SetFilterLayout(FilterBarLayout);
	
	this->OnFilterChanged.ExecuteIfBound();
}

void SFilterList::LoadSettings()
{
	LoadSettings(FilterBarIdentifier);
}

void SFilterList::LoadCustomTextFilters(const FFilterBarSettings* FilterBarConfig)
{
	CustomTextFilters.Empty();
	
	// Extract just the filter data from the common settings
	for(const FCustomTextFilterState& FilterState : FilterBarConfig->CustomTextFilters)
	{
		// Create an ICustomTextFilter using the provided delegate
		TSharedRef<ICustomTextFilter<FAssetFilterType>> NewTextFilter = this->CreateTextFilter.Execute().ToSharedRef();

		// Get the actual FFilterBase
		TSharedRef<FFilterBase<FAssetFilterType>> NewFilter = NewTextFilter->GetFilter().ToSharedRef();

		// Set the internals of the custom text filter from what we have saved
		NewTextFilter->SetFromCustomTextFilterData(FilterState.FilterData);

		// Add this to our list of custom text filters
		CustomTextFilters.Add(NewTextFilter);
	}
}

void SFilterList::AddWidgetToCurrentLayout(TSharedRef<SWidget> InWidget)
{
	if(FilterBarLayout == EFilterBarLayout::Horizontal)
	{
		HorizontalFilterBox->AddSlot()
		[
			InWidget
		];
	}
	else
	{
		VerticalFilterBox->AddSlot()
		[
			InWidget
		];
	}
}

void SFilterList::SetFilterLayout(EFilterBarLayout InFilterBarLayout)
{
	FilterBarLayout = InFilterBarLayout;

	/* Clear both layouts, because for SFilterList it is valid to call SetFilterLayout with InFilterBarLayout being the
	 * same as the current layout just to fire OnFilterBarLayoutChanging.
	 * Unlike the parent class SBasicFilterBar which guards against that. If we don't clear both child widgets you can
	 * end up with duplicate widgets.
	 */
	HorizontalFilterBox->ClearChildren();
	VerticalFilterBox->ClearChildren();
 		
	if(FilterBarLayout == EFilterBarLayout::Horizontal)
	{
		FilterBox->SetActiveWidget(HorizontalFilterBox.ToSharedRef());
	}
	else
	{
		FilterBox->SetActiveWidget(VerticalFilterBox.ToSharedRef());
	}

	OnFilterBarLayoutChanging.ExecuteIfBound(FilterBarLayout);

	for(TSharedRef<SFilter> Filter: Filters)
	{
		AddWidgetToLayout(Filter);
	}
 		
	this->Invalidate(EInvalidateWidgetReason::Layout);

 		
}

/////////////////////////////////////////
// FFilterListCustomTextFilter
/////////////////////////////////////////

/** Returns the system name for this filter */
FString FFrontendFilter_CustomText::GetName() const
{
	// Todo: Find some way to enforce this on all custom text filter interfaces
	return FCustomTextFilter<FAssetFilterType>::GetFilterTypeName().ToString();
}

FText FFrontendFilter_CustomText::GetDisplayName() const
{
	return DisplayName;
}
FText FFrontendFilter_CustomText::GetToolTipText() const
{
	return GetRawFilterText();
}

FLinearColor FFrontendFilter_CustomText::GetColor() const
{
	return Color;
}

void FFrontendFilter_CustomText::UpdateCustomTextFilterIncludes(const bool InIncludeClassName, const bool InIncludeAssetPath, const bool InIncludeCollectionNames)
{
	SetIncludeClassName(InIncludeClassName);
	SetIncludeAssetPath(InIncludeAssetPath);
	SetIncludeCollectionNames(InIncludeCollectionNames);
}

void FFrontendFilter_CustomText::SetFromCustomTextFilterData(const FCustomTextFilterData& InFilterData)
{
	Color = InFilterData.FilterColor;
	DisplayName = InFilterData.FilterLabel;
	SetRawFilterText(InFilterData.FilterString);
}

FCustomTextFilterData FFrontendFilter_CustomText::CreateCustomTextFilterData() const
{
	FCustomTextFilterData CustomTextFilterData;

	CustomTextFilterData.FilterColor = Color;
	CustomTextFilterData.FilterLabel = DisplayName;
	CustomTextFilterData.FilterString = GetRawFilterText();

	return CustomTextFilterData;
}

TSharedPtr<FFilterBase<FAssetFilterType>> FFrontendFilter_CustomText::GetFilter()
{
	return AsShared();
}

#undef LOCTEXT_NAMESPACE
