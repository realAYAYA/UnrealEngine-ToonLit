// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetManagerTypes.h"
#include "Misc/AssetRegistryInterface.h"
#include "StreamableManager.h"
#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "ContentEncryptionConfig.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AssetRegistry/AssetData.h"
#endif

#include "AssetManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAssetManager, Log, All);

/** Defined in C++ file */
struct FPrimaryAssetTypeData;
struct FPrimaryAssetData;
struct FPrimaryAssetRulesCustomOverride;
namespace UE::Cook { class ICookInfo; }

/** Delegate called when acquiring resources/chunks for assets, first parameter will be true if all resources were acquired, false if any failed.
	Second parameter will contain a list of chunks not yet downloaded */
DECLARE_DELEGATE_TwoParams(FAssetManagerAcquireResourceDelegateEx, bool /* bSuccess */, const TArray<int32>& /* MissingChunks */);

/** Delegate called when acquiring resources/chunks for assets, parameter will be true if all resources were acquired, false if any failed */
DECLARE_DELEGATE_OneParam(FAssetManagerAcquireResourceDelegate, bool /* bSuccess */);

/** Delegate called when new asset search root is registered due to runtime asset mounting, path will not have a trailing slash */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAddedAssetSearchRoot, const FString&);

/** 
 * A singleton UObject that is responsible for loading and unloading PrimaryAssets, and maintaining game-specific asset references
 * Games should override this class and change the class reference
 */
UCLASS()
class ENGINE_API UAssetManager : public UObject
{
	GENERATED_BODY()

public:
	/** Constructor */
	UAssetManager();

	/** Returns true if there is a current asset manager */
	static bool IsValid();

	/** Returns the current AssetManager object */
	static UAssetManager& Get();

	/** Returns the current AssetManager object if it exists, null otherwise */
	static UAssetManager* GetIfValid();

	/** Accesses the StreamableManager used by this Asset Manager. Static for easy access */
	static FStreamableManager& GetStreamableManager() { return Get().StreamableManager; }

	/** Asset Type of UWorld assets */
	static const FPrimaryAssetType MapType;

	/** Asset Type of Label used to tag other assets */
	static const FPrimaryAssetType PrimaryAssetLabelType;

	/** Type representing a packaging chunk, this is a virtual type that is never loaded off disk */
	static const FPrimaryAssetType PackageChunkType;

	/** Virtual path $AssetSearchRoots, replaced with all roots including defaults like /Game */
	static const FString AssetSearchRootsVirtualPath;

	/** Virtual path $DynamicSearchRoots, replaced with dynamically added asset roots */
	static const FString DynamicSearchRootsVirtualPath;

	/** Get the asset registry tag name for encryption key data */
	static FName GetEncryptionKeyAssetTagName();

	/** Creates a PrimaryAssetId from a chunk id */
	static FPrimaryAssetId CreatePrimaryAssetIdFromChunkId(int32 ChunkId);

	/** Extracts a chunk id from a primary asset id, returns INDEX_NONE if it is not PackageChunkType */
	static int32 ExtractChunkIdFromPrimaryAssetId(const FPrimaryAssetId& PrimaryAssetId);

	// BUILDING ASSET DIRECTORY

	/** 
	 * Scans a list of paths and reads asset data for all primary assets of a specific type.
	 * If done in the editor it will load the data off disk, in cooked games it will load out of the asset registry cache
	 *
	 * @param PrimaryAssetType	Type of asset to look for. If the scanned asset matches GetPrimaryAssetType with this it will be added to directory
	 * @param Paths				List of file system paths to scan
	 * @param BaseClass			Base class of all loaded assets, if the scanned asset isn't a child of this class it will be skipped
	 * @param bHasBlueprintClasses	If true, the assets are blueprints that subclass BaseClass. If false they are base UObject assets
	 * @param bIsEditorOnly		If true, assets will only be scanned in editor builds, and will not be stored into the asset registry
	 * @param bForceSynchronousScan If true will scan the disk synchronously, otherwise will wait for asset registry scan to complete
	 * @return					Number of primary assets found
	 */
	virtual int32 ScanPathsForPrimaryAssets(FPrimaryAssetType PrimaryAssetType, const TArray<FString>& Paths, UClass* BaseClass, bool bHasBlueprintClasses, bool bIsEditorOnly = false, bool bForceSynchronousScan = true);

	/** Single path wrapper */
	virtual int32 ScanPathForPrimaryAssets(FPrimaryAssetType PrimaryAssetType, const FString& Path, UClass* BaseClass, bool bHasBlueprintClasses, bool bIsEditorOnly = false, bool bForceSynchronousScan = true);

	/** Call before many calls to ScanPaths to improve load performance. Match each call with PopBulkScanning(). */
	void PushBulkScanning();
	void PopBulkScanning();

	virtual void RemoveScanPathsForPrimaryAssets(FPrimaryAssetType PrimaryAssetType, const TArray<FString>& Paths, UClass* BaseClass, bool bHasBlueprintClasses, bool bIsEditorOnly = false);

	virtual void RemovePrimaryAssetType(FPrimaryAssetType PrimaryAssetType);

protected:
	/** Should only be called from PushBulkScanning() and override */
	virtual void StartBulkScanning();
	/** Should only be called from PopBulkScanning() and override */
	virtual void StopBulkScanning();
	bool IsBulkScanning() const { return NumBulkScanRequests > 0 ; }

public:
	/** 
	 * Adds or updates a Dynamic asset, which is a runtime-specified asset that has no on disk representation, so has no FAssetData. But it can have bundle state and a path.
	 *
	 * @param FPrimaryAssetId	Type/Name of the asset. The type info will be created if it doesn't already exist
	 * @param AssetPath			Path to the object representing this asset, this is optional for dynamic assets
	 * @param BundleData		List of Name->asset paths that represent the possible bundle states for this asset
	 * @return					True if added
	 */
	virtual bool AddDynamicAsset(const FPrimaryAssetId& PrimaryAssetId, const FSoftObjectPath& AssetPath, const FAssetBundleData& BundleData);

	/** This will expand out references in the passed in AssetBundleData that are pointing to other primary assets with bundles. This is useful to preload entire webs of assets */
	virtual void RecursivelyExpandBundleData(FAssetBundleData& BundleData) const;

	/** Register a delegate to call when all types are scanned at startup, if this has already happened call immediately */
	static void CallOrRegister_OnCompletedInitialScan(FSimpleMulticastDelegate::FDelegate&& Delegate);

	/** Register a delegate to call when the asset manager singleton is spawned, if this has already happened call immediately */
	static void CallOrRegister_OnAssetManagerCreated(FSimpleMulticastDelegate::FDelegate&& Delegate);

	/** Returns true if initial scan has completed, this can be pretty late in editor builds */
	virtual bool HasInitialScanCompleted() const;

	/** Call to register a callback executed when a new asset search root is added, can be used to scan for new assets */
	FDelegateHandle Register_OnAddedAssetSearchRoot(FOnAddedAssetSearchRoot::FDelegate&& Delegate);

	/** Unregister previously added callback */
	void Unregister_OnAddedAssetSearchRoot(FDelegateHandle DelegateHandle);

	/** Expands a list of paths that potentially use virtual paths into real directory and package paths. Returns true if any changes were made */
	virtual bool ExpandVirtualPaths(TArray<FString>& InOutPaths) const;

	/** Register a new asset search root of the form /AssetRoot, will notify other systems about change */
	virtual void AddAssetSearchRoot(const FString& NewRootPath);

	/** Returns all the game asset roots, includes /Game by default and any dynamic ones */
	const TArray<FString>& GetAssetSearchRoots(bool bIncludeDefaultRoots = true) const;


	// ACCESSING ASSET DIRECTORY

	/** Gets the FAssetData for a primary asset with the specified type/name, will only work for once that have been scanned for already. Returns true if it found a valid data */
	virtual bool GetPrimaryAssetData(const FPrimaryAssetId& PrimaryAssetId, FAssetData& AssetData) const;

	/** Gets list of all FAssetData for a primary asset type, returns true if any were found */
	virtual bool GetPrimaryAssetDataList(FPrimaryAssetType PrimaryAssetType, TArray<FAssetData>& AssetDataList) const;

	/** Gets the in-memory UObject for a primary asset id, returning nullptr if it's not in memory. Will return blueprint class for blueprint assets. This works even if the asset wasn't loaded explicitly */
	virtual UObject* GetPrimaryAssetObject(const FPrimaryAssetId& PrimaryAssetId) const;

	/** Templated versions of above */
	template<class AssetType> 
	FORCEINLINE AssetType* GetPrimaryAssetObject(const FPrimaryAssetId& PrimaryAssetId) const
	{
		UObject* ObjectReturn = GetPrimaryAssetObject(PrimaryAssetId);
		return Cast<AssetType>(ObjectReturn);
	}

	template<class AssetType>
	FORCEINLINE TSubclassOf<AssetType> GetPrimaryAssetObjectClass(const FPrimaryAssetId& PrimaryAssetId) const
	{
		TSubclassOf<AssetType> ReturnClass;
		ReturnClass = Cast<UClass>(GetPrimaryAssetObject(PrimaryAssetId));
		return ReturnClass;
	}

	/** Gets list of all loaded objects for a primary asset type, returns true if any were found. Will return blueprint class for blueprint assets. This works even if the asset wasn't loaded explicitly */
	virtual bool GetPrimaryAssetObjectList(FPrimaryAssetType PrimaryAssetType, TArray<UObject*>& ObjectList) const;

	/** Gets the FSoftObjectPath for a primary asset type and name, returns invalid if not found */
	virtual FSoftObjectPath GetPrimaryAssetPath(const FPrimaryAssetId& PrimaryAssetId) const;

	/** Gets the list of all FSoftObjectPaths for a given type, returns true if any found */
	virtual bool GetPrimaryAssetPathList(FPrimaryAssetType PrimaryAssetType, TArray<FSoftObjectPath>& AssetPathList) const;

	/** Sees if the passed in object is a registered primary asset, if so return it. Returns invalid Identifier if not found */
	virtual FPrimaryAssetId GetPrimaryAssetIdForObject(UObject* Object) const;

	/** Sees if the passed in object path is a registered primary asset, if so return it. Returns invalid Identifier if not found */
	virtual FPrimaryAssetId GetPrimaryAssetIdForPath(const FSoftObjectPath& ObjectPath) const;

	UE_DEPRECATED(5.1, "FName asset paths are deprecated, use FSoftObjectPath instead.")
	virtual FPrimaryAssetId GetPrimaryAssetIdForPath(FName ObjectPath) const;

	/** Sees if the package has a primary asset, useful if only the package name is available */
	virtual FPrimaryAssetId GetPrimaryAssetIdForPackage(FName PackagePath) const;

	/** Returns the primary asset Id for the given FAssetData, only works if in directory */
	virtual FPrimaryAssetId GetPrimaryAssetIdForData(const FAssetData& AssetData) const;

	/** Gets list of all FPrimaryAssetId for a primary asset type, returns true if any were found */
	virtual bool GetPrimaryAssetIdList(FPrimaryAssetType PrimaryAssetType, TArray<FPrimaryAssetId>& PrimaryAssetIdList, EAssetManagerFilter Filter = EAssetManagerFilter::Default) const;

	/** Gets metadata for a specific asset type, returns false if not found */
	virtual bool GetPrimaryAssetTypeInfo(FPrimaryAssetType PrimaryAssetType, FPrimaryAssetTypeInfo& AssetTypeInfo) const;

	/** Gets list of all primary asset types infos */
	virtual void GetPrimaryAssetTypeInfoList(TArray<FPrimaryAssetTypeInfo>& AssetTypeInfoList) const;

	// ASYNC LOADING PRIMARY ASSETS

	/** 
	 * Loads a list of Primary Assets. This will start an async load of those assets, calling callback on completion.
	 * These assets will stay in memory until explicitly unloaded.
	 * You can wait on the returned streamable request or poll as needed.
	 * If there is no work to do, returned handle will be null and delegate will get called before function returns.
	 *
	 * @param AssetsToLoad		List of primary assets to load
	 * @param LoadBundles		List of bundles to load for those assets
	 * @param DelegateToCall	Delegate that will be called on completion, may be called before function returns if assets are already loaded
	 * @param Priority			Async loading priority for this request
	 * @return					Streamable Handle that can be used to poll or wait. You do not need to keep this handle to stop the assets from being unloaded
	 */
	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles = TArray<FName>(), FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority);

	/** Single asset wrapper */
	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles = TArray<FName>(), FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority);

	/** Loads all assets of a given type, useful for cooking */
	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAssetsWithType(FPrimaryAssetType PrimaryAssetType, const TArray<FName>& LoadBundles = TArray<FName>(), FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority);

	/** 
	 * Unloads a list of Primary Assets that were previously Loaded.
	 * If the only thing keeping these assets in memory was a prior Load call, they will be freed.
	 *
	 * @param AssetsToUnload	List of primary assets to load
	 * @return					Number of assets unloaded
	 */
	virtual int32 UnloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToUnload);

	/** Single asset wrapper */
	virtual int32 UnloadPrimaryAsset(const FPrimaryAssetId& AssetToUnload);

	/** Loads all assets of a given type, useful for cooking */
	virtual int32 UnloadPrimaryAssetsWithType(FPrimaryAssetType PrimaryAssetType);

	/** 
	 * Changes the bundle state of a set of loaded primary assets.
	 * You can wait on the returned streamable request or poll as needed.
	 * If there is no work to do, returned handle will be null and delegate will get called before function returns.
	 *
	 * @param AssetsToChange	List of primary assets to change state of
	 * @param AddBundles		List of bundles to add
	 * @param RemoveBundles		Explicit list of bundles to remove
	 * @param RemoveAllBundles	If true, remove all existing bundles even if not in remove list
	 * @param DelegateToCall	Delegate that will be called on completion, may be called before function returns if assets are already loaded
	 * @param Priority			Async loading priority for this request
	 * @return					Streamable Handle that can be used to poll or wait. You do not need to keep this handle to stop the assets from being unloaded
	 */
	virtual TSharedPtr<FStreamableHandle> ChangeBundleStateForPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToChange, const TArray<FName>& AddBundles, const TArray<FName>& RemoveBundles, bool bRemoveAllBundles = false, FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority);

	/** 
	 * Changes the bundle state of all loaded primary assets. Only assets matching OldBundles will be modified
	 * You can wait on the returned streamable request or poll as needed.
	 * If there is no work to do, returned handle will be null and delegate will get called before function returns.
	 *
	 * @param NewBundles		New bundle state for the assets that are changed
	 * @param OldBundles		Old bundle state, it will remove these bundles and replace with NewBundles
	 * @param DelegateToCall	Delegate that will be called on completion, may be called before function returns if assets are already loaded
	 * @param Priority			Async loading priority for this request
	 * @return					Streamable Handle that can be used to poll or wait. You do not need to keep this handle to stop the assets from being unloaded
	 */
	virtual TSharedPtr<FStreamableHandle> ChangeBundleStateForMatchingPrimaryAssets(const TArray<FName>& NewBundles, const TArray<FName>& OldBundles, FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority);

	/** 
	 * Returns the loading handle associated with the primary asset, it can then be checked for progress or waited on
	 *
	 * @param PrimaryAssetId	Asset to get handle for
	 * @param bForceCurrent		If true, returns the current handle. If false, will return pending if active, or current if not
	 * @param Bundles			If not null, will fill in with a list of the requested bundle state
	 * @return					Streamable Handle that can be used to poll or wait
	 */
	TSharedPtr<FStreamableHandle> GetPrimaryAssetHandle(const FPrimaryAssetId& PrimaryAssetId, bool bForceCurrent = false, TArray<FName>* Bundles = nullptr) const ;

	/** 
	 * Returns a list of primary assets that are in the given bundle state. Only assets that are loaded or being loaded are valid
	 *
	 * @param PrimaryAssetList	Any valid assets are added to this list
	 * @param ValidTypes		List of types that are allowed. If empty, all types allowed
	 * @param RequiredBundles	Adds to list if the bundle state has all of these bundles. If empty will return all loaded
	 * @param ExcludedBundles	Doesn't add if the bundle state has any of these bundles
	 * @param bForceCurrent		If true, only use the current state. If false, use the current or pending
	 * @return					True if any found
	 */
	bool GetPrimaryAssetsWithBundleState(TArray<FPrimaryAssetId>& PrimaryAssetList, const TArray<FPrimaryAssetType>& ValidTypes, const TArray<FName>& RequiredBundles, const TArray<FName>& ExcludedBundles = TArray<FName>(), bool bForceCurrent = false) const;

	/** Fills in a TMap with the pending/active loading state of every asset */
	void GetPrimaryAssetBundleStateMap(TMap<FPrimaryAssetId, TArray<FName>>& BundleStateMap, bool bForceCurrent = false) const;

	/**
	 * Fills in a set of object paths with the assets that need to be loaded, for a given Primary Asset and bundle list
	 *
	 * @param OutAssetLoadSet	Set that will have asset paths added to it
	 * @param PrimaryAssetId	Asset that would be loaded
	 * @param LoadBundles		List of bundles to load for those assets
	 * @param bLoadRecursive	If true, this will call RecursivelyExpandBundleData and recurse into sub bundles of other primary assets loaded by a bundle reference
	 * @return					True if primary asset id was found
	 */
	bool GetPrimaryAssetLoadSet(TSet<FSoftObjectPath>& OutAssetLoadSet, const FPrimaryAssetId& PrimaryAssetId, const TArray<FName>& LoadBundles, bool bLoadRecursive) const;

	/**
	 * Preloads data for a set of assets in a specific bundle state, and returns a handle you must keep active.
	 * These assets are not officially Loaded, so Unload/ChangeBundleState will not affect them and if you release the handle without otherwise loading the assets they will be freed.
	 *
	 * @param AssetsToLoad		List of primary assets to load
	 * @param LoadBundles		List of bundles to load for those assets
	 * @param bLoadRecursive	If true, this will call RecursivelyExpandBundleData and recurse into sub bundles of other primary assets loaded by a bundle reference
	 * @param DelegateToCall	Delegate that will be called on completion, may be called before function returns if assets are already loaded
	 * @param Priority			Async loading priority for this request
	 * @return					Streamable Handle that must be stored to keep the preloaded assets from being freed
	 */
	virtual TSharedPtr<FStreamableHandle> PreloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, bool bLoadRecursive, FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority);

	/** Quick wrapper to async load some non primary assets with the primary streamable manager. This will not auto release the handle, release it if needed */
	virtual TSharedPtr<FStreamableHandle> LoadAssetList(const TArray<FSoftObjectPath>& AssetList, FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority, const FString& DebugName = FStreamableHandle::HandleDebugName_AssetList);

	/** Returns a single AssetBundleInfo, matching Scope and Name */
	virtual FAssetBundleEntry GetAssetBundleEntry(const FPrimaryAssetId& BundleScope, FName BundleName) const;

	/** Appends all AssetBundleInfos inside a given scope */
	virtual bool GetAssetBundleEntries(const FPrimaryAssetId& BundleScope, TArray<FAssetBundleEntry>& OutEntries) const;

	/** 
	 * Returns the list of Chunks that are not currently mounted, and are required to load the referenced assets. Returns true if any chunks are missing
	 *
	 * @param AssetList				Asset Paths to check chunks for
	 * @param OutMissingChunkList	Chunks that are known about but not yet installed
	 * @param OutErrorChunkList		Chunks that do not exist at all and are not installable
	 */
	virtual bool FindMissingChunkList(const TArray<FSoftObjectPath>& AssetList, TArray<int32>& OutMissingChunkList, TArray<int32>& OutErrorChunkList) const;

	/** 
	 * Acquires a set of chunks using the platform chunk layer, then calls the passed in callback
	 *
	 * @param AssetList				Asset Paths to get resources for
	 * @param CompleteDelegate		Delegate called when chunks have been acquired or failed. If any chunks fail the entire operation is considered a failure
	 * @param Priority				Priority to use when acquiring chunks
	 */
	virtual void AcquireResourcesForAssetList(const TArray<FSoftObjectPath>& AssetList, FAssetManagerAcquireResourceDelegateEx CompleteDelegate, EChunkPriority::Type Priority = EChunkPriority::Immediate);
	virtual void AcquireResourcesForAssetList(const TArray<FSoftObjectPath>& AssetList, FAssetManagerAcquireResourceDelegate CompleteDelegate, EChunkPriority::Type Priority = EChunkPriority::Immediate);

	/** 
	 * Acquires a set of chunks using the platform chunk layer, then calls the passed in callback. This will download all bundles of a primary asset
	 *
	 * @param PrimaryAssetList		Primary assets to get chunks for
	 * @param CompleteDelegate		Delegate called when chunks have been acquired or failed. If any chunks fail the entire operation is considered a failure
	 * @param Priority				Priority to use when acquiring chunks
	 */
	virtual void AcquireResourcesForPrimaryAssetList(const TArray<FPrimaryAssetId>& PrimaryAssetList, FAssetManagerAcquireResourceDelegate CompleteDelegate, EChunkPriority::Type Priority = EChunkPriority::Immediate);
	
	/** Returns the chunk download/install progress. AcquiredCount is the number of chunks that were requested and have already been insatlled, Requested includes both installed and pending */
	virtual bool GetResourceAcquireProgress(int32& OutAcquiredCount, int32& OutRequestedCount) const;

	// FUNCTIONS FOR MANAGEMENT/COOK RULES

	/** Changes the default management rules for a specified type */
	virtual void SetPrimaryAssetTypeRules(FPrimaryAssetType PrimaryAssetType, const FPrimaryAssetRules& Rules);

	/** Changes the management rules for a specific asset, this overrides the type rules. If passed in Rules is default, delete override */
	virtual void SetPrimaryAssetRules(FPrimaryAssetId PrimaryAssetId, const FPrimaryAssetRules& Rules);
	virtual void SetPrimaryAssetRulesExplicitly(FPrimaryAssetId PrimaryAssetId, const FPrimaryAssetRulesExplicitOverride& ExplicitRules);

	/** Gets the management rules for a specific asset, this will merge the type and individual values */
	virtual FPrimaryAssetRules GetPrimaryAssetRules(FPrimaryAssetId PrimaryAssetId) const;

	/** Returns list of asset packages managed by primary asset */
	virtual bool GetManagedPackageList(FPrimaryAssetId PrimaryAssetId, TArray<FName>& AssetPackageList) const;

	/** Returns list of PrimaryAssetIds that manage a package. Will optionally recurse up the management chain */
	virtual bool GetPackageManagers(FName PackageName, bool bRecurseToParents, TSet<FPrimaryAssetId>& ManagerSet) const;
	/** Returns PrimaryAssetIds that manage a package, with property describing the reference (direct or indirect). */
	virtual bool GetPackageManagers(FName PackageName, bool bRecurseToParents,
		TMap<FPrimaryAssetId, UE::AssetRegistry::EDependencyProperty>& Managers) const;


	// GENERAL ASSET UTILITY FUNCTIONS

	/** Parses AssetData to extract the primary type/name from it. This works even if it isn't in the directory */
	virtual FPrimaryAssetId ExtractPrimaryAssetIdFromData(const FAssetData& AssetData, FPrimaryAssetType SuggestedType = FPrimaryAssetType()) const;

	/** Gets the FAssetData at a specific path, handles redirectors and blueprint classes correctly. Returns true if it found a valid data */
	virtual bool GetAssetDataForPath(const FSoftObjectPath& ObjectPath, FAssetData& AssetData) const;

	/** Checks to see if the given asset data is a blueprint with a base class in the ClassNameSet. This checks the parent asset tag */
	virtual bool IsAssetDataBlueprintOfClassSet(const FAssetData& AssetData, const TSet<FTopLevelAssetPath>& ClassNameSet) const;

	/** Turns an FAssetData into FSoftObjectPath, handles adding _C as necessary */
	virtual FSoftObjectPath GetAssetPathForData(const FAssetData& AssetData) const;

	/** Tries to redirect a Primary Asset Id, using list in AssetManagerSettings */
	virtual FPrimaryAssetId GetRedirectedPrimaryAssetId(const FPrimaryAssetId& OldId) const;

	/** Reads redirector list and gets a list of the redirected previous names for a Primary Asset Id */
	virtual void GetPreviousPrimaryAssetIds(const FPrimaryAssetId& NewId, TArray<FPrimaryAssetId>& OutOldIds) const;

	/** If bShouldManagerDetermineTypeAndName is true in settings, this function is used to determine the primary asset id for any object that does not have it's own implementation. Games can override the behavior here or call it from other places */
	virtual FPrimaryAssetId DeterminePrimaryAssetIdForObject(const UObject* Object) const;

	/** Reads AssetManagerSettings for specifically redirected asset paths. This is useful if you need to convert older saved data */
	UE_DEPRECATED(5.1, "Asset path FNames are deprecated, use FSoftObjectPath instead.")
	virtual FName GetRedirectedAssetPath(FName OldPath) const;
	virtual FSoftObjectPath GetRedirectedAssetPath(const FSoftObjectPath& OldPath) const;

	/** Extracts all FSoftObjectPaths from a Class/Struct */
	virtual void ExtractSoftObjectPaths(const UStruct* Struct, const void* StructValue, TArray<FSoftObjectPath>& FoundAssetReferences, const TArray<FName>& PropertiesToSkip = TArray<FName>()) const;

	/** Helper function to search the asset registry for AssetData matching search rules */
	virtual int32 SearchAssetRegistryPaths(TArray<FAssetData>& OutAssetDataList, const FAssetManagerSearchRules& Rules) const;

	/** Helper function to check if a single asset passes restrictions in SearchRules, this can be used when an asset is manually registered */
	virtual bool DoesAssetMatchSearchRules(const FAssetData& AssetData, const FAssetManagerSearchRules& Rules) const;

	/** Returns true if the specified TypeInfo should be scanned. Can be overridden by the game */
	virtual bool ShouldScanPrimaryAssetType(FPrimaryAssetTypeInfo& TypeInfo) const;

	/** Manually register a new or updated primary asset, returns true if it was successful */
	virtual bool RegisterSpecificPrimaryAsset(const FPrimaryAssetId& PrimaryAssetId, const FAssetData& NewAssetData);

	/** Helper function which requests the asset registery scan a list of directories/assets */
	virtual void ScanPathsSynchronous(const TArray<FString>& PathsToScan) const;

	/**
	 * Called when a new asset registry becomes available
	 * @param InName Logical name for this asset registry. Must match the name returned by GetUniqueAssetRegistryName
	 *
	 * @returns TRUE if new data was loaded and added into the master asset registry
	 */
	virtual bool OnAssetRegistryAvailableAfterInitialization(FName InName, FAssetRegistryState& OutNewState);

	/** Returns the root path for the package name or path (i.e. /Game/MyPackage would return /Game/ ). This works even if the root is not yet mounted */
	static bool GetContentRootPathFromPackageName(const FString& PackageName, FString& OutContentRootPath);

	/** Normalize a package path for use in asset manager, will remove duplicate // and add or remove a final slash as desired */
	static void NormalizePackagePath(FString& InOutPath, bool bIncludeFinalSlash);
	static FString GetNormalizedPackagePath(const FString& InPath, bool bIncludeFinalSlash);

	/** Dumps out summary of managed types to log */
	static void DumpAssetTypeSummary();

	/** Dumps out list of loaded asset bundles to log */
	static void DumpLoadedAssetState();

	/** Shows a list of all bundles for the specified primary asset by primary asset id (i.e. Map:Entry) */
	static void DumpBundlesForAsset(const TArray<FString>& Args);

	/** Dumps information about the Asset Registry to log */
	static void DumpAssetRegistryInfo();

	/** Dumps out list of primary asset -> managed assets to log */
	static void DumpReferencersForPackage(const TArray< FString >& PackageNames);

	/** Finds all the referencers for a set of packages. Recursively will get all the referencers up to the max depth */
	static void GetAllReferencersForPackage(TSet<FAssetData>& OutFoundAssets, const TArray<FName>& InPackageNames, int32 MaxDepth);

	/** Starts initial load, gets called from InitializeObjectReferences */
	virtual void StartInitialLoading();

	/** Finishes initial loading, gets called from end of Engine::Init() */
	virtual void FinishInitialLoading();

	/** Accessor for asset registry */
	class IAssetRegistry& GetAssetRegistry() const;

	/** Return settings object */
	const class UAssetManagerSettings& GetSettings() const;

	/** Returns a timer manager that is safe to use for asset loading actions. This will either be the editor or game instance one, or null during very early startup */
	class FTimerManager* GetTimerManager() const;

	// Overrides
	virtual void PostInitProperties() override;

	/** Get the encryption key guid attached to this primary asset. Can be invalid if the asset is not encrypted */
	virtual void GetCachedPrimaryAssetEncryptionKeyGuid(FPrimaryAssetId InPrimaryAssetId, FGuid& OutGuid);

	/** Loads the redirector maps */
	virtual void LoadRedirectorMaps();

	/** Refresh the entire set of asset data, can call from editor when things have changed dramatically. Will only refresh if force is true or it thinks something has changed */
	virtual void RefreshPrimaryAssetDirectory(bool bForceRefresh = false);

	/** Invalidate cached asset data so it knows to rescan when needed */
	virtual void InvalidatePrimaryAssetDirectory();

	/** Warn about this primary asset id being missing, but only if this is the first time this session */
	virtual void WarnAboutInvalidPrimaryAsset(const FPrimaryAssetId& PrimaryAssetId, const FString& Message) const;

	/** Helper function to write out asset reports */
	virtual bool WriteCustomReport(FString FileName, TArray<FString>& FileLines) const;

#if WITH_EDITOR
	// EDITOR ONLY FUNCTIONALITY

	/** Gets package names to add to the cook, and packages to never cook even if in startup set memory or referenced */
	virtual void ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook,
		TArray<FName>& PackagesToNeverCook);
	UE_DEPRECATED(5.0, "Use version that takes TargetPlatforms instead")
	virtual void ModifyCook(TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook);

	/** Gets package names to add to a DLC cook*/
	virtual void ModifyDLCCook(const FString& DLCName, TConstArrayView<const ITargetPlatform*> TargetPlatforms,
		TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook);
	UE_DEPRECATED(5.0, "Use version that takes TargetPlatforms instead")
	virtual void ModifyDLCCook(const FString& DLCName, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook);

	/** Returns whether or not a specific UPackage should be cooked for the provied TargetPlatform */
	virtual bool ShouldCookForPlatform(const UPackage* Package, const ITargetPlatform* TargetPlatform);

	/** Returns cook rule for a package name using Management rules, games should override this to take into account their individual workflows */
	virtual EPrimaryAssetCookRule GetPackageCookRule(FName PackageName) const;

	/**
	 * Helper function for GetPackageCookRule. Given the list of Managers that manage a package, calculate the unioned cookrule for the
	 * package. @see FPrimaryAssetCookRuleUnion::UnionWith. If two managers are in conflict (e.g. one is CookAlways, the other is
	 * ProductionNeverCook), the higher-priority will win. If they have the same priority, the NeverCook rule will win, and the managers
	 * will be reported in the OutConflictIds field.
	 */
	EPrimaryAssetCookRule CalculateCookRuleUnion(const TMap<FPrimaryAssetId, UE::AssetRegistry::EDependencyProperty>& Managers,
		TOptional<TPair<FPrimaryAssetId, FPrimaryAssetId>>* OutConflictIds) const;

	UE_DEPRECATED(5.0, "Use version that takes ICookInfo instead")
	virtual bool VerifyCanCookPackage(FName PackageName, bool bLogError = true) const;

	/** Returns true if the specified asset package can be cooked, will error and return false if it is disallowed */
	virtual bool VerifyCanCookPackage(UE::Cook::ICookInfo* CookInfo, FName PackageName, bool bLogError = true) const;

	/** 
	 * For a given package and platform, return what Chunks it should be assigned to, games can override this as needed. Returns false if no information found 
	 * @param PackageName Package to check chunks for
	 * @param TargetPlatform Can be used to do platform-specific chunking, this is null when previewing in the editor
	 * @param ExistingChunkList List of chunks that the asset was previously assigned to, may be empty
	 * @param OutChunkList List of chunks to actually assign this to
	 * @param OutOverrideChunkList List of chunks that were added due to override rules, not just normal primary asset rules. Tools use this for dependency checking
	 */
	virtual bool GetPackageChunkIds(FName PackageName, const class ITargetPlatform* TargetPlatform, TArrayView<const int32> ExistingChunkList, TArray<int32>& OutChunkList, TArray<int32>* OutOverrideChunkList = nullptr) const;

	/** 
	 * Retrieve the encryption key guid for a given chunk ID
	 *
	 * @param InChunkIndex - The chunk ID for which we are querying the encryption key guid
	 * @return If chunk ID is valid and references and encrypted chunk, returns the key guid. Otherwise, returns an empty guid 
	 */
	virtual FGuid GetChunkEncryptionKeyGuid(int32 InChunkId) const { return FGuid(); }

	/**
	 * Determine if we should separate the asset registry for this chunk out into its own file and return the unique name that identifies it
	 * @param InChunkIndex Chunk index to check
	 * @returns The logical name of the chunk's asset registry, or NAME_None if it isn't required
	 */
	virtual FName GetUniqueAssetRegistryName(int32 InChunkIndex) const { return NAME_None; }

	/**
	 * For a given content encryption group name (as defined in the content encryption config that the project provides, return the relevant chunk ID
	 */
	virtual int32 GetContentEncryptionGroupChunkID(FName InGroupName) const { return INDEX_NONE; }

	/** Returns the list of chunks assigned to the list of primary assets, which is usually a manager list. This is called by GetPackageChunkIds */
	virtual bool GetPrimaryAssetSetChunkIds(const TSet<FPrimaryAssetId>& PrimaryAssetSet, const class ITargetPlatform* TargetPlatform, TArrayView<const int32> ExistingChunkList, TArray<int32>& OutChunkList) const;

	/** Resets all asset manager data, called in the editor to reinitialize the config */
	virtual void ReinitializeFromConfig();

	/** Updates the asset management database if needed */
	virtual void UpdateManagementDatabase(bool bForceRefresh = false);

	/** Returns the chunk information map computed during UpdateManagementDatabase */
	const TMap<int32, FAssetManagerChunkInfo>& GetChunkManagementMap() const;

	/** Handles applying Asset Labels and should be overridden to do any game-specific labelling */
	virtual void ApplyPrimaryAssetLabels();

	/** Refreshes cache of asset data for in memory object */
	virtual void RefreshAssetData(UObject* ChangedObject);

	/** 
	 * Initializes asset bundle data from a passed in struct or class, this will read the AssetBundles metadata off the UProperties. As an example this property definition:
	 *
	 * UPROPERTY(EditDefaultsOnly, Category=Data, meta = (AssetBundles = "Client,Server"))
	 * TSoftObjectPtr<class UCurveTable> CurveTableReference;
	 *
	 * Would add the value of CurveTableReference to both the Client and Server asset bundles
	 *
	 * @param Struct		UScriptStruct or UClass representing the property hierarchy
	 * @param StructValue	Location in memory of Struct or Object
	 * @param AssetBundle	Bundle that will be filled out
	 */
	virtual void InitializeAssetBundlesFromMetadata(const UStruct* Struct, const void* StructValue, FAssetBundleData& AssetBundle, FName DebugName = NAME_None) const;

	/** UObject wrapper */
	virtual void InitializeAssetBundlesFromMetadata(const UObject* Object, FAssetBundleData& AssetBundle) const
	{
		InitializeAssetBundlesFromMetadata(Object->GetClass(), Object, AssetBundle, Object->GetFName());
	}

	/** 
	  * Called immediately before saving the asset registry during cooking
	  */
	virtual void PreSaveAssetRegistry(const class ITargetPlatform* TargetPlatform, const TSet<FName>& InCookedPackages) {}

	/** 
	  * Called immediately after saving the asset registry during cooking
	  */
	virtual void PostSaveAssetRegistry() {}

	/**
	  * Gathers information about which assets the game wishes to encrypt into named groups
	  */
	virtual void GetContentEncryptionConfig(FContentEncryptionConfig& OutContentEncryptionConfig) {}
#endif

protected:
	friend class FAssetManagerEditorModule;

	/** Internal helper function that attempts to get asset data from the specified path; Accounts for possibility of blueprint classes ending in _C */
	virtual void GetAssetDataForPathInternal(class IAssetRegistry& AssetRegistry, const FString& AssetPath, OUT FAssetData& OutAssetData) const;

	/** Updates the asset data cached on the name data; returns false if the asset is not a valid primary asset. */
	virtual bool TryUpdateCachedAssetData(const FPrimaryAssetId& PrimaryAssetId, const FAssetData& NewAssetData, bool bAllowDuplicates);

	/** Returns the NameData for a specific type/name pair */
	FPrimaryAssetData* GetNameData(const FPrimaryAssetId& PrimaryAssetId, bool bCheckRedirector = true);
	const FPrimaryAssetData* GetNameData(const FPrimaryAssetId& PrimaryAssetId, bool bCheckRedirector = true) const;

	/** Rebuilds the ObjectReferenceList, needed after global object state has changed */
	virtual void RebuildObjectReferenceList();

	/** Called when an internal load handle finishes, handles setting to pending state */
	virtual void OnAssetStateChangeCompleted(FPrimaryAssetId PrimaryAssetId, TSharedPtr<FStreamableHandle> BoundHandle, FStreamableDelegate WrappedDelegate);

	/** Scans all asset types specified in DefaultGame */
	virtual void ScanPrimaryAssetTypesFromConfig();

	/** Called to apply the primary asset rule overrides from config */
	virtual void ScanPrimaryAssetRulesFromConfig();
	
	/** Apply a single custom primary asset rule, calls function below */
	virtual void ApplyCustomPrimaryAssetRulesOverride(const FPrimaryAssetRulesCustomOverride& CustomOverride);

	/** Sees if a specific primary asset passes the custom override filter, subclass this to handle FilterString */
	virtual bool DoesPrimaryAssetMatchCustomOverride(FPrimaryAssetId PrimaryAssetId, const FPrimaryAssetRulesCustomOverride& CustomOverride) const;

	/** Called after scanning is complete, either from FinishInitialLoading or after the AssetRegistry finishes */
	virtual void PostInitialAssetScan();

	/** Returns true if path should be excluded from primary asset scans, called from ShouldIncludeInAssetSearch and in the editor */
	virtual bool IsPathExcludedFromScan(const FString& Path) const;

	/** Filter function that is called from SearchAssetRegistryPaths, returns true if asset data should be included in search results */
	virtual bool ShouldIncludeInAssetSearch(const FAssetData& AssetData, const FAssetManagerSearchRules& SearchRules) const;

	/** Call to start acquiring a list of chunks */
	virtual void AcquireChunkList(const TArray<int32>& ChunkList, FAssetManagerAcquireResourceDelegate CompleteDelegate, EChunkPriority::Type Priority, TSharedPtr<FStreamableHandle> StalledHandle);

	/** Called when a new chunk has been downloaded */
	virtual void OnChunkDownloaded(uint32 ChunkId, bool bSuccess);

#if WITH_EDITOR
	UE_DEPRECATED(4.26, "ShouldSetManager that takes EAssetRegistryDependencyType is no longer called; switch to the version that takes EDependencyCategory")
	virtual EAssetSetManagerResult::Type ShouldSetManager(const FAssetIdentifier& Manager, const FAssetIdentifier& Source, const FAssetIdentifier& Target, EAssetRegistryDependencyType::Type DependencyType, EAssetSetManagerFlags::Type Flags) const;
	/** Function used during creating Management references to decide when to recurse and set references */
	virtual EAssetSetManagerResult::Type ShouldSetManager(const FAssetIdentifier& Manager, const FAssetIdentifier& Source, const FAssetIdentifier& Target,
		UE::AssetRegistry::EDependencyCategory Category, UE::AssetRegistry::EDependencyProperty Properties, EAssetSetManagerFlags::Type Flags) const;

	/** Called when asset registry is done loading off disk, will finish any deferred loads */
	virtual void OnAssetRegistryFilesLoaded();

	/** Handles updating manager when a new asset is created */
	virtual void OnInMemoryAssetCreated(UObject *Object);

	/** Handles updating manager if deleted object is relevant*/
	virtual void OnInMemoryAssetDeleted(UObject *Object);

	/** Called when object is saved */
	virtual void OnObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext);

	/** When asset is renamed */
	virtual void OnAssetRenamed(const FAssetData& NewData, const FString& OldPath);

	/** When an asset is removed */
	virtual void OnAssetRemoved(const FAssetData& Data);

	/** Try to remove an old asset identifier when it has been deleted/renamed */
	virtual void RemovePrimaryAssetId(const FPrimaryAssetId& PrimaryAssetId);

	/** Scans the respective PackagePath for public assets to include in PackagesToCook */
	virtual void GatherPublicAssetsForPackage(FName PackagePath, TArray<FName>& PackagesToCook) const;

	/** Called right before PIE starts, will refresh asset directory and can be overriden to preload assets */
	virtual void PreBeginPIE(bool bStartSimulate);

	/** Called after PIE ends, resets loading state */
	virtual void EndPIE(bool bStartSimulate);

	/** Copy of the asset state before PIE was entered, return to that when PIE completes */
	TMap<FPrimaryAssetId, TArray<FName>> PrimaryAssetStateBeforePIE;

	/** Cached map of chunk ids to lists of assets in that chunk */
	TMap<int32, FAssetManagerChunkInfo> CachedChunkMap;
#endif // WITH_EDITOR

	/** Map from object path to Primary Asset Id */
	TMap<FSoftObjectPath, FPrimaryAssetId> AssetPathMap;

	/** Overridden asset management data for specific types */
	TMap<FPrimaryAssetId, FPrimaryAssetRulesExplicitOverride> AssetRuleOverrides;

	/** Map from PrimaryAssetId to list of PrimaryAssetIds that are the parent of this one, for determining chunking/cooking */
	TMap<FPrimaryAssetId, TArray<FPrimaryAssetId>> ManagementParentMap;

	/** Cached map of asset bundles, global and per primary asset */
	TMap<FPrimaryAssetId, TSharedPtr<FAssetBundleData, ESPMode::ThreadSafe>> CachedAssetBundles;

	/** List of assets we have warned about being missing */
	mutable TSet<FPrimaryAssetId> WarningInvalidAssets;

	/** List of directories that have already been synchronously scanned */
	mutable TArray<FString> AlreadyScannedDirectories;

	/** All asset search roots including startup ones */
	TArray<FString> AllAssetSearchRoots;

	/** List of dynamic asset search roots added past startup */
	TArray<FString> AddedAssetSearchRoots;

	/** The streamable manager used for all primary asset loading */
	FStreamableManager StreamableManager;

	/** Defines a set of chunk installs that are waiting */
	struct FPendingChunkInstall
	{
		/** Chunks we originally requested */
		TArray<int32> RequestedChunks;

		/** Chunks we are still waiting for */
		TArray<int32> PendingChunks;

		/** Stalled streamable handle waiting for this install, may be null */
		TSharedPtr<FStreamableHandle> StalledStreamableHandle;

		/** Delegate to call on completion, may be empty */
		FAssetManagerAcquireResourceDelegate ManualCallback;
	};

	/** List of chunk installs that are being waited for */
	TArray<FPendingChunkInstall> PendingChunkInstalls;

	/** Cache of encryption keys used by each primary asset */
	TMap<FPrimaryAssetId, FGuid> PrimaryAssetEncryptionKeyCache;

	/** List of UObjects that are being kept from being GCd, derived from the asset type map. Arrays are currently more efficient than Sets */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> ObjectReferenceList;

	/** True if we are running a build that is already scanning assets globally so we can perhaps avoid scanning paths synchronously */
	UPROPERTY()
	bool bIsGlobalAsyncScanEnvironment;

	/** True if PrimaryAssetType/Name will be implied for loading assets that don't have it saved on disk. Won't work for all projects */
	UPROPERTY()
	bool bShouldGuessTypeAndName;

	/** True if we should always use synchronous loads, this speeds up cooking */
	UPROPERTY()
	bool bShouldUseSynchronousLoad;

	/** True if we are loading from pak files */
	UPROPERTY()
	bool bIsLoadingFromPakFiles;

	/** True if the chunk install interface should be queries before loading assets */
	UPROPERTY()
	bool bShouldAcquireMissingChunksOnLoad;

	/** If true, DevelopmentCook assets will error when they are cooked */
	UPROPERTY()
	bool bOnlyCookProductionAssets;

	/** Suppresses bOnlyCookProductionAssets based on the AllowsDevelopmentObjects() property of the TargetPlatforms being cooked. */
	bool bTargetPlatformsAllowDevelopmentObjects;

	/** >0 if we are currently in bulk scanning mode */
	UPROPERTY()
	int32 NumBulkScanRequests;

	/** True if asset data is current, if false it will need to rescan before PIE */
	UPROPERTY()
	bool bIsPrimaryAssetDirectoryCurrent;

	/** True if the asset management database is up to date */
	UPROPERTY()
	bool bIsManagementDatabaseCurrent;

	/** True if the asset management database should be updated after scan completes */
	UPROPERTY()
	bool bUpdateManagementDatabaseAfterScan;

	/** True if only on-disk assets should be searched by the asset registry */
	UPROPERTY()
	bool bIncludeOnlyOnDiskAssets;

	/** True if we have passed the initial asset registry/type scan */
	UPROPERTY()
	bool bHasCompletedInitialScan;

	/** Number of notifications seen in this update */
	UPROPERTY()
	int32 NumberOfSpawnedNotifications;

	/** Redirector maps loaded out of AssetMigrations.ini */
	TMap<FName, FName> PrimaryAssetTypeRedirects;
	TMap<FString, FString> PrimaryAssetIdRedirects;
	TMap<FSoftObjectPath, FSoftObjectPath> AssetPathRedirects;

	/** Delegate called when initial span finishes */
	static FSimpleMulticastDelegate OnCompletedInitialScanDelegate;

	/** Delegate called when the asset manager singleton is created */
	static FSimpleMulticastDelegate OnAssetManagerCreatedDelegate;

	/** Delegate called when a new asset search root is registered */
	FOnAddedAssetSearchRoot OnAddedAssetSearchRootDelegate;

	/** Delegate bound to chunk install */
	FDelegateHandle ChunkInstallDelegateHandle;

private:
	/** Provide proper reentrancy for AssetRegistry temporary caching */
	bool bOldTemporaryCachingMode = false;

#if WITH_EDITOR
	/** Recursive handler for InitializeAssetBundlesFromMetadata */
	virtual void InitializeAssetBundlesFromMetadata_Recursive(const UStruct* Struct, const void* StructValue, FAssetBundleData& AssetBundle, FName DebugName, TSet<const void*>& AllVisitedStructValues) const;
#endif

	/** Per-type asset information, cannot be accessed by children as it is defined in CPP file */
	TMap<FName, TSharedRef<FPrimaryAssetTypeData>> AssetTypeMap;

	mutable class IAssetRegistry* CachedAssetRegistry;
	mutable const class UAssetManagerSettings* CachedSettings;
	UE_DEPRECATED(5.0, "Only used for deprecation support, do not use directly.")
	mutable UE::Cook::ICookInfo* DeprecationSupportCookInfo = nullptr;
	UE_DEPRECATED(5.0, "Only used for deprecation support, do not use directly.")
	mutable TConstArrayView<const ITargetPlatform*> DeprecationSupportTargetPlatforms;

	friend struct FCompiledAssetManagerSearchRules;
};
