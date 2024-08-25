// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Filters/CustomTextFilters.h"
#include "Filters/FilterBase.h"
#include "Filters/SAssetFilterBar.h"
#include "Filters/SBasicFilterBar.h"
#include "FrontendFilters.h"
#include "HAL/Platform.h"
#include "IContentBrowserSingleton.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FFrontendFilter;
class SWidget;
class UClass;
class UToolMenu;
struct FContentBrowserItem;
struct FCustomTextFilterState;
struct FFilterBarSettings;
struct FGeometry;
struct FPointerEvent;

enum class ECheckBoxState : uint8;

/**
 * A list of filters currently applied to an asset view.
 */
class SFilterList : public SAssetFilterBar<FAssetFilterType>
{
public:
	DECLARE_DELEGATE_OneParam(FOnFilterBarLayoutChanging, EFilterBarLayout /* NewLayout */)
	/**
	 * An event delegate that is executed when a custom text filter has been created/modified/deleted in any FilterList
	 * that is using shared settings.
	 */
	DECLARE_EVENT_OneParam(SFilterList, FCustomTextFilterEvent, TSharedPtr<SWidget> /* BroadcastingFilterList */);
	
	using FOnFilterChanged = typename SAssetFilterBar<FAssetFilterType>::FOnFilterChanged;

	SLATE_BEGIN_ARGS( SFilterList )
	: _UseSharedSettings(false)
	, _FilterBarLayout(EFilterBarLayout::Horizontal)
	, _CanChangeOrientation(false)
	, _DefaultMenuExpansionCategory(EAssetCategoryPaths::Basic)
	, _bUseSectionsForCustomCategories(false)
	{
		
	}
		/** Delegate for when filters have changed */
		SLATE_EVENT( FOnFilterChanged, OnFilterChanged )

		/** Delegate that lets the user modify the menu after the fact */
		SLATE_EVENT(FOnExtendAddFilterMenu, OnExtendAddFilterMenu)
	
		/** The filter collection used to further filter down assets returned from the backend */
		SLATE_ARGUMENT( TSharedPtr<FAssetFilterCollectionType>, FrontendFilters)

		/** An array of classes to filter the menu by */
		SLATE_ARGUMENT( TArray<UClass*>, InitialClassFilters)

		/** Custom front end filters to be displayed */
		SLATE_ARGUMENT( TArray< TSharedRef<FFrontendFilter> >, ExtraFrontendFilters )
	
		/** A delegate to create a Text Filter for FilterType items. If provided, will allow creation of custom text filters
		 *  from the filter dropdown menu.
		 */
		SLATE_ARGUMENT(FCreateTextFilter, CreateTextFilter)
	
		/** A unique identifier for this filter bar needed to enable saving settings in a config file */
		SLATE_ARGUMENT(FName, FilterBarIdentifier)

		/** If true, CustomTextFilters are saved/loaded from a config shared between multiple SFilterLists
		 *	Currently used to sync all content browser custom text filters.
		 */
		SLATE_ARGUMENT(bool, UseSharedSettings)

		/** Called when the filter bar layout is changing, before the filters are added to the layout */
		SLATE_EVENT(FOnFilterBarLayoutChanging, OnFilterBarLayoutChanging)

		/** The layout that determines how the filters are laid out */
		SLATE_ARGUMENT(EFilterBarLayout, FilterBarLayout)
			
		/** If true, allow dynamically changing the orientation and saving in the config */
		SLATE_ARGUMENT(bool, CanChangeOrientation)

		/** Expands the specified asset category, if specified. If not, it will expand Basic/Common instead. */
		SLATE_ARGUMENT(TOptional<FAssetCategoryPath>, DefaultMenuExpansionCategory)

		/** If true, adds custom categories as sections (expanded) vs. as sub-menus */
		SLATE_ARGUMENT(bool, bUseSectionsForCustomCategories)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Set the check box state of the specified frontend filter (in the filter drop down) and pin/unpin a filter widget on/from the filter bar. When a filter is pinned (was not already pinned), it is activated and deactivated when unpinned. */
	void SetFrontendFilterCheckState(const TSharedPtr<FFrontendFilter>& InFrontendFilter, ECheckBoxState CheckState);

	/** Returns the check box state of the specified frontend filter (in the filter drop down). This tells whether the filter is pinned or not on the filter bar, but not if filter is active or not. @see IsFrontendFilterActive(). */
	ECheckBoxState GetFrontendFilterCheckState(const TSharedPtr<FFrontendFilter>& InFrontendFilter) const;

	/** Returns true if the specified frontend filter is both checked (pinned on the filter bar) and active (contributing to filter the result). */
	bool IsFrontendFilterActive(const TSharedPtr<FFrontendFilter>& InFrontendFilter) const;

	/** Retrieve a specific frontend filter */
	TSharedPtr<FFrontendFilter> GetFrontendFilter(const FString& InName) const;
	
	/** Handler for when the floating add filter button was clicked */
	TSharedRef<SWidget> ExternalMakeAddFilterMenu();

	/** Disables any active filters that would hide the supplied items */
	void DisableFiltersThatHideItems(TArrayView<const FContentBrowserItem> ItemList);

	/** Returns the class filters specified at construction using argument 'InitialClassFilters'. */
	const TArray<UClass*>& GetInitialClassFilters();

	/** Open the dialog to create a custom filter from the given text */
	void CreateCustomFilterDialog(const FText& InText);
	
	/** Updates bIncludeClassName, bIncludeAssetPath and bIncludeCollectionNames for all custom text filters */
	void UpdateCustomTextFilterIncludes(const bool InIncludeClassName, const bool InIncludeAssetPath, const bool InIncludeCollectionNames);

	/** Add a custom widget to the filter bar alongside the filters */
	void AddWidgetToCurrentLayout(TSharedRef<SWidget> InWidget);

	virtual void SaveSettings() override;
	virtual void LoadSettings() override;

	/* Copy the settings from a specific instance name */
	void LoadSettings(const FName& InInstanceName);

	/** Helper functions for backwards compatibility with filters that save state until they are ported to EditorConfig */
	void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString);
	void LoadSettings(const FName& InInstanceName, const FString& IniFilename, const FString& IniSection, const FString& SettingsString);


	virtual void SetFilterLayout(EFilterBarLayout InFilterBarLayout) override;

protected:
	
	/** Handler for when the add filter button was clicked */
	//virtual TSharedRef<SWidget> MakeAddFilterMenu() override;
	
	/** Handler for when a custom text filter is created */
	virtual void OnCreateCustomTextFilter(const FCustomTextFilterData& InFilterData, bool bApplyFilter) override;
	/** Handler for when a custom text filter is modified */
	virtual void OnModifyCustomTextFilter(const FCustomTextFilterData& InFilterData, TSharedPtr<ICustomTextFilter<FAssetFilterType>> InFilter) override;
	/** Handler for when a custom text filter is deleted */
	virtual void OnDeleteCustomTextFilter(const TSharedPtr<ICustomTextFilter<FAssetFilterType>> InFilter) override;

private:

	// /** Exists for backwards compatibility with ExternalMakeAddFilterMenu */
	// TSharedRef<SWidget> MakeAddFilterMenu(FAssetCategoryPath MenuExpansion = EAssetCategoryPaths::Basic);

	virtual UAssetFilterBarContext* CreateAssetFilterBarContext() override;
	
	/** Handler for when another SFilterList using shared settings creates a custom text filter */
	void OnExternalCustomTextFilterCreated(TSharedPtr<SWidget> BroadcastingFilterList);
	
	/** Empty our list of custom text filters, and load from the given config */
	void LoadCustomTextFilters(const FFilterBarSettings* FilterBarConfig);

	/** Find the custom text filter corresponding to the specified state, and restore it's state to what is specified
	 *  @return True if the filter was restored successfully, false if not
	 */
	bool RestoreCustomTextFilterState(const FCustomTextFilterState& InFilterState);
	
private:

	/** List of classes that our filters must match */
	TArray<UClass*> InitialClassFilters;

	/** Delegate for when filters have changed */
	FOnFilterChanged OnFilterChanged;

	/** Delegate for when the layout is changed */
	FOnFilterBarLayoutChanging OnFilterBarLayoutChanging;

	/** A reference to AllFrontEndFilters so we can access the filters as FFrontEndFilter instead of FFilterBase<FAssetFilterType> */
	TArray< TSharedRef<FFrontendFilter> > AllFrontendFilters_Internal;

	/** An identifier shared by all SFilterLists, used to save and load settings common to every instance */
	static const FName SharedIdentifier;

	/** If bIncludeClassName is true, custom text filters will include an asset's class name in the search */
	bool bIncludeClassName;

	/** If bIncludeAssetPath is true, custom text filters will match against full Asset path */
	bool bIncludeAssetPath;

	/** If bIncludeCollectionNames is true, custom text filters will match against collection names as well */
	bool bIncludeCollectionNames;

	/** Whether this FilterList wants to load/save from the settings common to all instances */
	bool bUseSharedSettings;

	/** The event that executes when a custom text filter is created/modified/deleted in any filter list that is using Shared Settings */
	static FCustomTextFilterEvent CustomTextFilterEvent;
};

/* A custom implementation of ICustomTextFilter that uses FFrontendFilter_Text to handle comparing items in the
 * Asset View to Custom Text Filters. This ensures that the advanced search syntax etc behaves properly when used
 * by a custom text filter created by saving a search
 */
class FFrontendFilter_CustomText :
	public ICustomTextFilter<FAssetFilterType>,
	public FFrontendFilter_Text,
	public TSharedFromThis<FFrontendFilter_CustomText>
{
public:

	// FFrontendFilter implementation
	virtual FString GetName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTipText() const override;
	virtual FLinearColor GetColor() const override;
	
	/** Updates bIncludeClassName, bIncludeAssetPath and bIncludeCollectionNames for this filter */
	void UpdateCustomTextFilterIncludes(const bool InIncludeClassName, const bool InIncludeAssetPath, const bool InIncludeCollectionNames);

	//ICustomTextFilter interface
	
	/** Set the internals of this filter from an FCustomTextFilterData */
	virtual void SetFromCustomTextFilterData(const FCustomTextFilterData& InFilterData) override;

	/** Create an FCustomTextFilterData from the internals of this filter */
	virtual FCustomTextFilterData CreateCustomTextFilterData() const override;

	/** Get the actual filter */
	virtual TSharedPtr<FFilterBase<FAssetFilterType>> GetFilter() override;

protected:

	/* The Display Name of this custom filter that the user sees */
	FText DisplayName;

	/* The Color of this filter pill */
	FLinearColor Color;
};