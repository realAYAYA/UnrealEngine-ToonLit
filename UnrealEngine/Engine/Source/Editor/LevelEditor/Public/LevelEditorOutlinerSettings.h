// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/GenericFilter.h"

struct ISceneOutlinerTreeItem;
class FUncontrolledChangelistState;

namespace SceneOutliner
{
	/** The type of item that the Outliner's Filter Bar operates on */
	typedef const ISceneOutlinerTreeItem& FilterBarType;
}

struct FSceneOutlinerFilterBarOptions;
class FCustomClassFilterData;

/** Structure of built-in filter categories. Defined as functions to enable external use without linkage */
struct FLevelEditorOutlinerBuiltInCategories
{
	static FName Common()			 { static FName Name("Essential");			return Name; }
	static FName Basic()			 { static FName Name("Basic");				return Name; }
	static FName Animation()		 { static FName Name("Animation");			return Name; }
	static FName Audio()			 { static FName Name("Audio");				return Name; }
	static FName Geometry()			 { static FName Name("Geometry");			return Name; }
	static FName Lights()			 { static FName Name("Lights");				return Name; }
	static FName Environment()		 { static FName Name("Environment");		return Name; }
	static FName Visual()			 { static FName Name("Visual");				return Name; }
	static FName Volumes()			 { static FName Name("Volumes");			return Name; }
	static FName VirtualProduction() { static FName Name("VirtualProduction");	return Name; }
};

/** Helper class to manage initalization options specific to the Level Editor Outliners
 *  Use AddCustomFilter/AddCustomClass Filter to register new filters in the Outliner.
 *  Make sure that the filters you attach have a category, otherwise they will not show up
 */
class LEVELEDITOR_API FLevelEditorOutlinerSettings :  public TSharedFromThis<class FLevelEditorOutlinerSettings>
{
public:

	/** Delegate to create a Filter for the Outliner */
	DECLARE_DELEGATE_RetVal(TSharedPtr<FFilterBase<const ISceneOutlinerTreeItem&>>, FOutlinerFilterFactory);

	/**  Add a custom filter to the outliner filter bar. These are all AND'd together
	 *   @see FGenericFilter on how to create generic filters
	 *   Note: Any filter added here will be shared between all 4 Outliners, so you cannot rely on the filter to know
	 *   which outliner it is active in. Use the override that takes a delegate instead
	 */

	UE_DEPRECATED(5.2, "Use the AddCustomFilter override that takes in a factory instead so each Outliner can have a unique instance of the filter")
	void AddCustomFilter(TSharedRef<FFilterBase<const ISceneOutlinerTreeItem&>> InCustomFilter);

	/**  Add a custom filter to the outliner filter bar. These are all AND'd together
	 *   This function takes in a delegate that will be called to create an instance of the custom filter for each Outliner that
	 *   the level editor creates.
	 *   @see FGenericFilter on how to create generic filters
	 */
	void AddCustomFilter(FOutlinerFilterFactory InCreateCustomFilter);

	/**  Add a custom class filter to the outliner filter bar. These represent asset/actor type filters and are OR'd
	 *   Can be created using IAssetTypeActions or UClass (@see constructor)
	 */
	void AddCustomClassFilter(TSharedRef<FCustomClassFilterData> InCustomClassFilterData);

	// Creates the default filters that the level editor outliner has
	void CreateDefaultFilters();

	// Setup the built in filter categories
	void SetupBuiltInCategories();

	// Append the init options stored in this class to the given Outliner init options
	void GetOutlinerFilters(FSceneOutlinerFilterBarOptions& OutFilterBarOptions);

	// Get the FFilterCategory attached to the given category name. Use this to add filters to the built in categories
	TSharedPtr<FFilterCategory> GetFilterCategory(const FName& CategoryName);

private:

	void CreateSCCFilters();

	bool DoesActorPassUnsavedFilter(const ISceneOutlinerTreeItem& InItem);
	bool DoesActorPassUncontrolledFilter(const ISceneOutlinerTreeItem& InItem);

	void OnUnsavedAssetAdded(const FString& InAsset);
	void OnUnsavedAssetRemoved(const FString& InAsset);

	void OnUncontrolledChangelistModuleChanged();
	
	// Refresh any Outliners that have the given filter active
	void RefreshOutlinersWithActiveFilter(bool bFullRefresh, const FString& InFilterName);
private:

	/** These are the custom filters that the Scene Outliner will have. All active filters will be AND'd together to test
	 *  against.
	 */
	TArray<TSharedRef<FFilterBase<const ISceneOutlinerTreeItem&>>> CustomFilters;

	/** These are delegates that will be used to create custom filters for each outliner that calls GetOutlinerFilters */ 
	TArray<FOutlinerFilterFactory> CustomFilterDelegates;

	/** These are the asset type filters that the Scene Outliner will have. All active filters will be OR'd together to
	*  test against.
	 */
	TArray<TSharedRef<FCustomClassFilterData>> CustomClassFilters;

	/** The built in custom filters created by this instance */
	TArray<TSharedRef<FFilterBase<const ISceneOutlinerTreeItem&>>> BuiltInCustomFilters;

	/* A map of the categories the Outliner filter bar will have */
	TMap<FName, TSharedPtr<FFilterCategory>> FilterBarCategories;

	/* A map to convert placement mode built in categories to filter bar categories */
	TMap<FName, FName> PlacementToFilterCategoryMap;

	// Source Control Cache

	// List of currently unsaved assets. Using a set here because it can grow a lot and impact performances when looking up in the list.
	TSet<FString> UnsavedAssets;

	// List of uncontrolled changelist state
	TArray<TSharedRef<FUncontrolledChangelistState>> UncontrolledChangelistStates;

	static const FString UnsavedAssetsFilterName;
	static const FString UncontrolledAssetsFilterName;
};
