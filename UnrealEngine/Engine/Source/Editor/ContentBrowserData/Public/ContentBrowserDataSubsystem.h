// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SortedMap.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserDataFilter.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserItemData.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "EditorSubsystem.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Misc/NamePermissionList.h"
#include "Misc/StringBuilder.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ContentBrowserDataSubsystem.generated.h"

class FSubsystemCollectionBase;
class FText;
class UContentBrowserDataSource;
class UObject;
struct FAssetData;
struct FContentBrowserItemPath;
struct FFrame;
template <typename FuncType> class TFunctionRef;

UENUM(BlueprintType)
enum class EContentBrowserPathType : uint8
{
	/** No path type set */
	None,
	/** Internal path compatible with asset registry and engine calls (eg,. "/PluginA/MyFile") */
	Internal,
	/** Virtual path for enumerating Content Browser data (eg, "/All/Plugins/PluginA/MyFile") */
	Virtual
};

enum class EContentBrowserIsFolderVisibleFlags : uint8
{
	None = 0,

	/**
	 * Hide folders that recursively contain no file items.
	 */
	HideEmptyFolders = 1<<0,

	/**
	 * Default visibility flags.
	 */
	Default = None,
};
ENUM_CLASS_FLAGS(EContentBrowserIsFolderVisibleFlags);

/** Called for incremental item data updates from data sources that can provide delta-updates */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnContentBrowserItemDataUpdated, TArrayView<const FContentBrowserItemDataUpdate>);

/** Called for wholesale item data updates from data sources that can't provide delta-updates, or when the set of active data sources is modified */
DECLARE_MULTICAST_DELEGATE(FOnContentBrowserItemDataRefreshed);

/** Called when all active data sources have completed their initial content discovery scan. May be called multiple times if new data sources are registered after the current set of active data sources have completed their initial scan */
DECLARE_MULTICAST_DELEGATE(FOnContentBrowserItemDataDiscoveryComplete);

/** Called when generating a virtual path, allows customization of how a virtual path is generated. */
DECLARE_DELEGATE_TwoParams(FContentBrowserGenerateVirtualPathDelegate, const FStringView, FStringBuilderBase&);

/** Internal - Filter data used to inject dummy items for the path down to the mount root of each data source */
USTRUCT()
struct CONTENTBROWSERDATA_API FContentBrowserCompiledSubsystemFilter
{
	GENERATED_BODY()
	
public:
	TArray<FName> MountRootsToEnumerate;
};

/** Internal - Filter data used to inject dummy items */
USTRUCT()
struct CONTENTBROWSERDATA_API FContentBrowserCompiledVirtualFolderFilter
{
	GENERATED_BODY()

public:
	TMap<FName, FContentBrowserItemData> CachedSubPaths;
};

/**
 * Subsystem that provides access to Content Browser data.
 * This type deals with the composition of multiple data sources, which provide information about the folders and files available in the Content Browser.
 */
UCLASS(config=Editor)
class CONTENTBROWSERDATA_API UContentBrowserDataSubsystem : public UEditorSubsystem, public IContentBrowserItemDataSink
{
	GENERATED_BODY()

public:
	friend class FScopedSuppressContentBrowserDataTick;

	UContentBrowserDataSubsystem();
	//~ UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void ConvertInternalPathToVirtual(const FStringView InPath, FStringBuilderBase& OutPath) override;

	/**
	 * Attempt to activate the named data source.
	 * @return True if the data source was available and not already active, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	bool ActivateDataSource(const FName Name);

	/**
	 * Attempt to deactivate the named data source.
	 * @return True if the data source was available and active, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	bool DeactivateDataSource(const FName Name);

	/**
	 * Activate all available data sources.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	void ActivateAllDataSources();

	/**
	 * Deactivate all active data sources.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	void DeactivateAllDataSources();

	/**
	 * Get the list of current available data sources.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	TArray<FName> GetAvailableDataSources() const;

	/**
	 * Get the list of current active data sources.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	TArray<FName> GetActiveDataSources() const;

	/**
	 * Delegate called for incremental item data updates from data sources that can provide delta-updates.
	 */
	FOnContentBrowserItemDataUpdated& OnItemDataUpdated();

	/**
	 * Delegate called for wholesale item data updates from data sources that can't provide delta-updates, or when the set of active data sources is modified.
	 */
	FOnContentBrowserItemDataRefreshed& OnItemDataRefreshed();

	/**
	 * Delegate called when all active data sources have completed their initial content discovery scan.
	 * @note May be called multiple times if new data sources are registered after the current set of active data sources have completed their initial scan.
	 */
	FOnContentBrowserItemDataDiscoveryComplete& OnItemDataDiscoveryComplete();

	/**
	 * Take a raw data filter and convert it into a compiled version that could be re-used for multiple queries using the same data (typically this is only useful for post-filtering multiple items).
	 * @note The compiled filter is only valid until the data source changes, so only keep it for a short time (typically within a function call, or 1-frame).
	 */
	void CompileFilter(const FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter) const;

	/**
	 * Enumerate the items (folders and/or files) that match a previously compiled filter.
	 */
	void EnumerateItemsMatchingFilter(const FContentBrowserDataCompiledFilter& InFilter, TFunctionRef<bool(FContentBrowserItem&&)> InCallback) const;
	void EnumerateItemsMatchingFilter(const FContentBrowserDataCompiledFilter& InFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) const;

	/**
	 * Enumerate the items (folders and/or files) that exist under the given virtual path.
	 */
	void EnumerateItemsUnderPath(const FName InPath, const FContentBrowserDataFilter& InFilter, TFunctionRef<bool(FContentBrowserItem&&)> InCallback) const;
	void EnumerateItemsUnderPath(const FName InPath, const FContentBrowserDataFilter& InFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) const;

	/**
	 * Get the items (folders and/or files) that exist under the given virtual path.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	TArray<FContentBrowserItem> GetItemsUnderPath(const FName InPath, const FContentBrowserDataFilter& InFilter) const;

	/**
	 * Enumerate the items (folders and/or files) that exist at the given virtual path.
	 * @note Multiple items may have the same virtual path if they are different types, or come from different data sources.
	 */
	void EnumerateItemsAtPath(const FName InPath, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItem&&)> InCallback) const;
	void EnumerateItemsAtPath(const FName InPath, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) const;

	/**
	 * Enumerate the items (files) that exist at the given paths.
	 * @note Multiple items may have the same virtual path if they are different types, or come from different data sources.
	 *
	 * @param InItemPaths The paths to enumerate
	 * @param InItemTypeFilter The types of items we want to find.
	 * @param InCallback The function to invoke for each matching item (return true to continue enumeration).
	 */
	bool EnumerateItemsAtPaths(const TArrayView<struct FContentBrowserItemPath> InItemPaths, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) const;

	/**
	 * Enumerate the items (files) that exist for the given objects.
	 * @note Multiple items may have the same virtual path if they are different types, or come from different data sources.
	 *
	 * @param InObjects The objects to enumerate
	 * @param InCallback The function to invoke for each matching item (return true to continue enumeration).
	 */
	bool EnumerateItemsForObjects(const TArrayView<UObject*> InObjects, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback) const;

	/**
	 * Get the items (folders and/or files) that exist at the given virtual path.
	 * @note Multiple items may have the same virtual path if they are different types, or come from different data sources.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	TArray<FContentBrowserItem> GetItemsAtPath(const FName InPath, const EContentBrowserItemTypeFilter InItemTypeFilter) const;

	/**
	 * Get the first item (folder and/or file) that exists at the given virtual path.
	 */
	UFUNCTION(BlueprintCallable, Category="ContentBrowser")
	FContentBrowserItem GetItemAtPath(const FName InPath, const EContentBrowserItemTypeFilter InItemTypeFilter) const;

	/**
	 * Get a list of other paths that the data source may be using to represent a specific path
	 *
	 * @param The internal path (or object path) of an asset to get aliases for
	 * @return All alternative paths that represent the input path (not including the input path itself)
	 */
	TArray<FContentBrowserItemPath> GetAliasesForPath(const FSoftObjectPath& InInternalPath) const;
	TArray<FContentBrowserItemPath> GetAliasesForPath(const FContentBrowserItemPath InPath) const;
	TArray<FContentBrowserItemPath> GetAliasesForPath(const FName InInternalPath) const;

	/**
	 * Query whether any data sources are currently discovering content, and retrieve optional status messages that can be shown in the UI.
	 */
	bool IsDiscoveringItems(TArray<FText>* OutStatus = nullptr) const;

	/**
	 * If possible, attempt to prioritize content discovery for the given virtual path.
	 */
	bool PrioritizeSearchPath(const FName InPath);

	/**
	 * Query whether the given virtual folder should be visible in the UI.
	 */
	bool IsFolderVisible(const FName InPath, const EContentBrowserIsFolderVisibleFlags InFlags = EContentBrowserIsFolderVisibleFlags::Default) const;

	/**
	 * Query whether the given virtual folder should be visible if the UI is asking to hide empty content folders.
	 */
	UE_DEPRECATED(5.3, "IsFolderVisibleIfHidingEmpty is deprecated. Use IsFolderVisible instead and add EContentBrowserIsFolderVisibleFlags::HideEmptyFolders to the flags.")
	bool IsFolderVisibleIfHidingEmpty(const FName InPath) const;

	/*
	 * Query whether a folder can be created at the given virtual path, optionally providing error information if it cannot.
	 *
	 * @param InPath The virtual path of the folder that is being queried.
	 * @param OutErrorMessage Optional error message to fill on failure.
	 *
	 * @return True if the folder can be created, false otherwise.
	 */
	bool CanCreateFolder(const FName InPath, FText* OutErrorMsg) const;

	/*
	 * Attempt to begin the process of asynchronously creating a folder at the given virtual path, returning a temporary item that can be finalized or canceled by the user.
	 *
	 * @param InPath The initial virtual path of the folder that is being created.
	 *
	 * @return The pending folder item to create (test for validity).
	 */
	FContentBrowserItemTemporaryContext CreateFolder(const FName InPath) const;

	/**
	 * Attempt to convert the given package path to virtual paths associated with the active data sources (callback will be called for each successful conversion).
	 * @note This exists to allow the Content Browser to interface with public APIs that only operate on package paths and should ideally be avoided for new code.
	 * @note This function only adjusts the path to something that could represent a virtualized item within this data source, but it doesn't guarantee that an item actually exists at that path.
	 */
	void Legacy_TryConvertPackagePathToVirtualPaths(const FName InPackagePath, TFunctionRef<bool(FName)> InCallback);

	/**
	 * Attempt to convert the given asset data to a virtual paths associated with the active data sources (callback will be called for each successful conversion).
	 * @note This exists to allow the Content Browser to interface with public APIs that only operate on asset data and should ideally be avoided for new code.
	 * @note This function only adjusts the path to something that could represent a virtualized item within this data source, but it doesn't guarantee that an item actually exists at that path.
	 */
	void Legacy_TryConvertAssetDataToVirtualPaths(const FAssetData& InAssetData, const bool InUseFolderPaths, TFunctionRef<bool(FName)> InCallback);

	/**
	 * Rebuild the virtual path tree if rules have changed.
	 */
	void RefreshVirtualPathTreeIfNeeded();

	/**
	 * Call when rules of virtual path generation have changed beyond content browser settings.
	 */
	void SetVirtualPathTreeNeedsRebuild();

	/**
	 * Converts an internal path to a virtual path based on current rules
	 */
	void ConvertInternalPathToVirtual(const FStringView InPath, FName& OutPath);
	void ConvertInternalPathToVirtual(FName InPath, FName& OutPath);
	FName ConvertInternalPathToVirtual(FName InPath);
	TArray<FString> ConvertInternalPathsToVirtual(const TArray<FString>& InPaths);

	/**
	 * Converts virtual path back into an internal or invariant path
	 */
	EContentBrowserPathType TryConvertVirtualPath(const FStringView InPath, FStringBuilderBase& OutPath) const;
	EContentBrowserPathType TryConvertVirtualPath(const FStringView InPath, FString& OutPath) const;
	EContentBrowserPathType TryConvertVirtualPath(const FStringView InPath, FName& OutPath) const;
	EContentBrowserPathType TryConvertVirtualPath(const FName InPath, FName& OutPath) const;

	/**
	 * Returns array of paths converted to internal.
	 */
	TArray<FString> TryConvertVirtualPathsToInternal(const TArray<FString>& InVirtualPaths) const;

	/**
	 * Customize list of folders that appear first in content browser based on internal or invariant paths
	 */
	void SetPathViewSpecialSortFolders(const TArray<FName>& InSpecialSortFolders);

	/**
	 * Returns reference to list of paths that appear first in content browser based on internal or invariant paths
	 */
	const TArray<FName>& GetPathViewSpecialSortFolders() const;

	/**
	 * Returns reference to default list of paths that appear first in content browser based on internal or invariant paths
	 */
	const TArray<FName>& GetDefaultPathViewSpecialSortFolders() const;

	/**
	 * Set delegate used to generate a virtual path.
	 */
	void SetGenerateVirtualPathPrefixDelegate(const FContentBrowserGenerateVirtualPathDelegate& InDelegate);

	/**
	 * Delegate called to generate a virtual path. Can be set to override default behavior.
	 */
	FContentBrowserGenerateVirtualPathDelegate& OnGenerateVirtualPathPrefix();

	/**
	 * Prefix to use when generating virtual paths and "Show All Folder" option is enabled
	 */
	const FString& GetAllFolderPrefix() const;

	/**
	 * Permission list that controls whether content in a given folder path can be edited.
	 * Note: This does not control if the folder path is writable or if content can be deleted.
	 */
	TSharedRef<FPathPermissionList>& GetEditableFolderPermissionList();


	/**
	 * Provide an partial access to private api for the filter cache ID owners
	 * See FContentBrowserDataFilterCacheIDOwner declaration for how to use the caching for filter compilation
	 */
	struct FContentBrowserFilterCacheApi
	{
	private:
		friend FContentBrowserDataFilterCacheIDOwner;

		static void InitializeCacheIDOwner(UContentBrowserDataSubsystem& Subsystem, FContentBrowserDataFilterCacheIDOwner& IDOwner);
		static void RemoveUnusedCachedData(const UContentBrowserDataSubsystem& Subsystem, const FContentBrowserDataFilterCacheIDOwner& IDOwner, TArrayView<const FName> InVirtualPathsInUse, const FContentBrowserDataFilter& DataFilter);
		static void ClearCachedData(const UContentBrowserDataSubsystem& Subsystem, const FContentBrowserDataFilterCacheIDOwner& IDOwner);
	};

private:

	void InitializeCacheIDOwner(FContentBrowserDataFilterCacheIDOwner& IDOwner);

	void RemoveUnusedCachedFilterData(const FContentBrowserDataFilterCacheIDOwner& IDOwner, TArrayView<const FName> InVirtualPathsInUse, const FContentBrowserDataFilter& DataFilter) const;

	void ClearCachedFilterData(const FContentBrowserDataFilterCacheIDOwner& IDOwner) const;

	using FNameToDataSourceMap = TSortedMap<FName, UContentBrowserDataSource*, FDefaultAllocator, FNameFastLess>;

	/**
	 * Called to handle a data source modular feature being registered.
	 * @note Will activate the data source if it is in the EnabledDataSources array.
	 */
	void HandleDataSourceRegistered(const FName& Type, class IModularFeature* Feature);
	
	/**
	 * Called to handle a data source modular feature being unregistered.
	 * @note Will deactivate the data source if it is in the ActiveDataSources map.
	 */
	void HandleDataSourceUnregistered(const FName& Type, class IModularFeature* Feature);

	/**
	 * Tick this subsystem.
	 * @note Called once every 0.1 seconds.
	 */
	void Tick(const float InDeltaTime);

	/**
	 * Returns true if item data modifications are being processed.
	 */
	bool AllowModifiedItemDataUpdates() const;

	/**
	 * Called when Play in Editor begins.
	 */
	void OnBeginPIE(const bool bIsSimulating);

	/**
	 * Called when Play in Editor stops.
	 */
	void OnEndPIE(const bool bIsSimulating);

	/**
	 * Called when Content added to delay content browser tick for a frame
	 * Prevents content browser from slowing down initialization by ticking as content is loaded
	 */
	void OnContentPathMounted(const FString& AssetPath, const FString& ContentPath);

	//~ IContentBrowserItemDataSink interface
	virtual void QueueItemDataUpdate(FContentBrowserItemDataUpdate&& InUpdate) override;
	virtual void NotifyItemDataRefreshed() override;

	/**
	 * Handle for the Tick callback.
	 */
	FTSTicker::FDelegateHandle TickHandle;

	/**
	 * Map of data sources that are currently active.
	 */
	FNameToDataSourceMap ActiveDataSources;

	/**
	 * Map of data sources that are currently available.
	 */
	FNameToDataSourceMap AvailableDataSources;

	/**
	 * Set of data sources that are currently running content discovery.
	 * ItemDataDiscoveryCompleteDelegate will be called each time this set becomes empty.
	 */
	TSet<FName> ActiveDataSourcesDiscoveringContent;

	/**
	 * Array of data source names that should be activated when available.
	 */
	UPROPERTY(config)
	TArray<FName> EnabledDataSources;

	/**
	 * Queue of incremental item data updates.
	 * These will be passed to ItemDataUpdatedDelegate on the end of Tick.
	 */
	TArray<FContentBrowserItemDataUpdate> PendingUpdates;

	/**
	 * True if an item data refresh notification is pending.
	 */
	bool bPendingItemDataRefreshedNotification = false;

	/**
	 * True if there are currently any ignored changes.
	 */
	bool bHasIgnoredItemUpdates = false;

	/**
	 * True if Play in Editor is active.
	 */
	bool bIsPIEActive = false;

	/**
	 * True if content was just mounted this frame
	 */
	bool bContentMountedThisFrame = false;

	/**
	 * >0 if Tick events have currently been suppressed.
	 * @see FScopedSuppressContentBrowserDataTick
	 */
	int32 TickSuppressionCount = 0;

	/**
	 * Delegate called for incremental item data updates from data sources that can provide delta-updates.
	 */
	FOnContentBrowserItemDataUpdated ItemDataUpdatedDelegate;

	/**
	 * Delegate called for wholesale item data updates from data sources that can't provide delta-updates, or when the set of active data sources is modified.
	 */
	FOnContentBrowserItemDataRefreshed ItemDataRefreshedDelegate;

	/**
	 * Delegate called when all active data sources have completed their initial content discovery scan.
	 * @note May be called multiple times if new data sources are registered after the current set of active data sources have completed their initial scan.
	 */
	FOnContentBrowserItemDataDiscoveryComplete ItemDataDiscoveryCompleteDelegate;

	/**
	 * Generates an optional virtual path prefix for a given internal path
	 */
	FContentBrowserGenerateVirtualPathDelegate GenerateVirtualPathPrefixDelegate;

	/**
	 * Optional array of invariant paths to use when sorting
	 */
	TArray<FName> PathViewSpecialSortFolders;

	/**
	 * Default array of invariant paths to use when sorting
	 */
	TArray<FName> DefaultPathViewSpecialSortFolders;

	/**
	 * Prefix to use when generating virtual paths and "Show All Folder" option is enabled
	 */
	FString AllFolderPrefix;

	/** Permission list of folder paths we can edit */
	TSharedRef<FPathPermissionList> EditableFolderPermissionList;

	int64 LastCacheIDForFilter = INDEX_NONE;
};

/**
 * Helper to suppress Tick events during critical times, when the underlying data should not be updated.
 */
class FScopedSuppressContentBrowserDataTick
{
public:
	explicit FScopedSuppressContentBrowserDataTick(UContentBrowserDataSubsystem* InContentBrowserData)
		: ContentBrowserData(InContentBrowserData)
	{
		check(ContentBrowserData);
		++ContentBrowserData->TickSuppressionCount;
	}

	~FScopedSuppressContentBrowserDataTick()
	{
		checkf(ContentBrowserData->TickSuppressionCount > 0, TEXT("TickSuppressionCount underflow!"));
		--ContentBrowserData->TickSuppressionCount;
	}

	FScopedSuppressContentBrowserDataTick(const FScopedSuppressContentBrowserDataTick&) = delete;
	FScopedSuppressContentBrowserDataTick& operator=(const FScopedSuppressContentBrowserDataTick&) = delete;

private:
	UContentBrowserDataSubsystem* ContentBrowserData = nullptr;
};
