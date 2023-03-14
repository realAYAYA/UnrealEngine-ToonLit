// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataRegistryTypes.h"
#include "DataRegistry.generated.h"

class UDataRegistrySource;
struct FDataRegistryCache;
struct FCachedDataRegistryItem;
struct FRealCurve;

/** 
 * Defines a place to efficiently store and retrieve structure data, can be used as a wrapper around Data/Curve Tables or extended with other sources
 */
UCLASS()
class DATAREGISTRY_API UDataRegistry : public UObject
{
	GENERATED_BODY()
public:

	UDataRegistry();
	virtual ~UDataRegistry();

	/** Returns the struct used by this registry, everything returned will be this or a subclass */
	const UScriptStruct* GetItemStruct() const;

	/** Returns true if this registry struct inherits from a particular named struct */
	bool DoesItemStructMatchFilter(FName FilterStructName) const;

	/** Gets the formatting for this Id */
	const FDataRegistryIdFormat& GetIdFormat() const;

	/** Returns the name for type exposed by this registry */
	const FName GetRegistryType() const;

	/** Gets a human readable summary of registry, for UI usage */
	virtual FText GetRegistryDescription() const;

	/** Returns true if this state has been initialized for use */
	virtual bool IsInitialized() const;

	/** Initialize for requests, called when subsystem starts up and should return true on success */
	virtual bool Initialize();

	/** Disable access and restore to state before initialization, won't do anything if not initialized */
	virtual void Deinitialize();

	/** Reset caches and state because gameplay finished due to PIE shutting down or the game registering a return to main menu, but stay initialized for future use */
	virtual void ResetRuntimeState();

	/** Marks this registry for needing a runtime refresh at next opportunity */
	virtual void MarkRuntimeDirty();

	/** Conditionally refresh the runtime state if needed */
	virtual void RuntimeRefreshIfNeeded();

	/** Attempt to register a specified asset with a source, returns true if any changes were made. Can be used to update priority for existing asset as well */
	virtual bool RegisterSpecificAsset(const FAssetData& AssetData, int32 AssetPriority = 0);

	/** Removes references to a specific asset, returns bool if it was removed */
	virtual bool UnregisterSpecificAsset(const FSoftObjectPath& AssetPath);

	/** Unregisters all previously registered assets in a specific registry with a specific priority, can be used as a batch reset. Returns number of assets unregistered */
	virtual int32 UnregisterAssetsWithPriority(int32 AssetPriority);

	/** Returns the worst availability for anything stored in this registry, if this is Precached then this registry is a wrapper around already loaded data */
	virtual EDataRegistryAvailability GetLowestAvailability() const;

	/** Gets the current general cache policy */
	const FDataRegistryCachePolicy& GetRuntimeCachePolicy() const;

	/** Sets the current cache policy, could cause items to get released */
	void SetRuntimeCachePolicy(const FDataRegistryCachePolicy& NewPolicy);

	/** Applies the cache policy, is called regularly but can be manually executed */
	virtual void ApplyCachePolicy();

	/** Checks if a previous cached get is still valid */
	virtual bool IsCacheGetResultValid(FDataRegistryCacheGetResult Result) const;

	/** Returns the current cache version for a successful get, may change depending on stack-specific resolve settings */
	virtual FDataRegistryCacheGetResult GetCacheResultVersion() const;

	/** Bump the cache version from some external event like game-specific file loading */
	virtual void InvalidateCacheVersion();

	/** Accesses the delegate called when cache version changes */
	virtual FDataRegistryCacheVersionCallback& OnCacheVersionInvalidated();

	/** Resolves an item id into a specific source and unique id, this can remap the names using game-specific rules. PrecachedDataPtr will be set if it is precached by another system */
	virtual bool ResolveDataRegistryId(FDataRegistryLookup& OutLookup, const FDataRegistryId& ItemId, const uint8** PrecachedDataPtr = nullptr) const;

	/** Fills in a list of all item ids provided by this registry, sorted for display */
	virtual void GetPossibleRegistryIds(TArray<FDataRegistryId>& OutRegistryIds, bool bSortForDisplay = true) const;

	/** Start an async request for a single item */
	virtual bool AcquireItem(const FDataRegistryId& ItemId, FDataRegistryItemAcquiredCallback DelegateToCall);

	/** Start an async request for multiple items*/
	virtual bool BatchAcquireItems(const TArray<FDataRegistryId>& ItemIds, FDataRegistryBatchAcquireCallback DelegateToCall);

	/** Finds the cached item using a resolved lookup, this can be useful after a load has happened to ensure you get the exact item requested */
	virtual FDataRegistryCacheGetResult GetCachedItemRawFromLookup(const uint8*& OutItemMemory, const UScriptStruct*& OutItemStruct, const FDataRegistryId& ItemId, const FDataRegistryLookup& Lookup) const;

	/** Returns the raw cached data and struct type, useful for generic C++ calls */
	virtual FDataRegistryCacheGetResult GetCachedItemRaw(const uint8*& OutItemMemory, const UScriptStruct*& OutItemStruct, const FDataRegistryId& ItemId) const;

	/** Curve wrapper for get function */
	virtual FDataRegistryCacheGetResult GetCachedCurveRaw(const FRealCurve*& OutCurve, const FDataRegistryId& ItemId) const;

	/** Find the source associated with a lookup index */
	virtual UDataRegistrySource* LookupSource(FName& OutResolvedName, const FDataRegistryLookup& Lookup, int32 LookupIndex) const;

	/** Finds the cached item, using the request context to handle remapping */
	template <class T>
	const T* GetCachedItem(const FDataRegistryId& ItemId) const
	{
		const uint8* TempItemMemory = nullptr;
		const UScriptStruct* TempItemStuct = nullptr;

		if (GetCachedItemRaw(TempItemMemory, TempItemStuct, ItemId))
		{
			if (!ensureMsgf(TempItemStuct->IsChildOf(T::StaticStruct()), TEXT("Can't cast data item of type %s to %s! Code should check type before calling GetCachedDataRegistryItem"), *TempItemStuct->GetName(), *T::StaticStruct()->GetName()))
			{
				return nullptr;
			}

			return reinterpret_cast<const T*>(TempItemMemory);
		}

		return nullptr;
	}

	/** 
	 * Fills in a map with all cached (and precached) ids and items for fast iteration. 
	 * This will use the current resolve context so may not always be valid, and multiple ids can map to the same raw pointer
	 */
	virtual FDataRegistryCacheGetResult GetAllCachedItems(TMap<FDataRegistryId, const uint8*>& OutItemMap, const UScriptStruct*& OutItemStruct) const;


	// Internal functions called from sources and subclasses
	
	/** Gets list of child runtime sources created by passed in source, in order registered */
	virtual void GetChildRuntimeSources(UDataRegistrySource* ParentSource, TArray<UDataRegistrySource*>& ChildSources) const;

	/** Returns the index of a source in the source list */
	int32 GetSourceIndex(const UDataRegistrySource* Source, bool bUseRuntimeSources = true) const;

	/** Call to indicate that a item is available, will insert into cache */
	virtual void HandleAcquireResult(const FDataRegistrySourceAcquireRequest& Request, EDataRegistryAcquireStatus Status, uint8* ItemMemory, UDataRegistrySource* Source);

	/** Returns a timer manager that is safe to use for asset loading actions. This will either be the editor or game instance one, or null during very early startup */
	static class FTimerManager* GetTimerManager();

	/** Gets the current time */
	static float GetCurrentTime();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;

	/** Validate and refresh registration */
	virtual void EditorRefreshRegistry();

	/** Returns all source ids for editor display */
	virtual void GetAllSourceItems(TArray<FDataRegistrySourceItemId>& OutSourceItems) const;

	/** Request a list of source items */
	virtual bool BatchAcquireSourceItems(TArray<FDataRegistrySourceItemId>& SourceItems, FDataRegistryBatchAcquireCallback DelegateToCall);
#endif

protected:

	/** Globally unique name used to identify this registry */
	UPROPERTY(EditDefaultsOnly, Category = DataRegistry, AssetRegistrySearchable)
	FName RegistryType;

	/** Rules for specifying valid item Ids, if default than any name can be used */
	UPROPERTY(EditDefaultsOnly, Category = DataRegistry)
	FDataRegistryIdFormat IdFormat;

	/** Structure type of all for items in this registry */
	UPROPERTY(EditDefaultsOnly, Category = DataRegistry, AssetRegistrySearchable, meta = (DisplayThumbnail = "false"))
	TObjectPtr<const UScriptStruct> ItemStruct;

	/** List of data sources to search for items */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = DataRegistry)
	TArray<TObjectPtr<UDataRegistrySource>> DataSources;

	// TODO remove VisibleDefaultsOnly or figure out how to stop it from letting you edit the instance properties
	/** Runtime list of data sources, created from above list and includes sources added at runtime */
	UPROPERTY(VisibleDefaultsOnly, Instanced, Transient, Category = DataRegistry)
	TArray<TObjectPtr<UDataRegistrySource>> RuntimeSources;

	/** How often to check for cache updates */
	UPROPERTY(EditDefaultsOnly, Category = Cache)
	float TimerUpdateFrequency = 1.0f;

	/** Editor-set cache policy */
	UPROPERTY(EditDefaultsOnly, Category = Cache)
	FDataRegistryCachePolicy DefaultCachePolicy;

	/** Runtime override */
	FDataRegistryCachePolicy RuntimeCachePolicy;

	/** Callback for when cache version changes, might be moved later */
	FDataRegistryCacheVersionCallback OnCacheVersionInvalidatedCallback;

	/** Refresh the RuntimeSources list */
	virtual void RefreshRuntimeSources();

	/** Called on timer tick when initialized */
	virtual void TimerUpdate();

	/** Maps from a type:name pair to a per-source resolved name, default just returns the name */
	virtual FName MapIdToResolvedName(const FDataRegistryId& ItemId, const UDataRegistrySource* RegistrySource) const;

	/** Adds all possible source ids for resolved name to set, regardless of request context, default just uses the name */
	virtual void AddAllIdsForResolvedName(TSet<FDataRegistryId>& PossibleIds, const FName& ResolvedName, const UDataRegistrySource* RegistrySource) const;

	/** Returns internal cached data or raw memory ptr for precached data */
	const FCachedDataRegistryItem* FindCachedData(const FDataRegistryId& ItemId, const uint8** PrecachedDataPtr = nullptr) const;

	/** Advances state machine for a cached entry */
	virtual void HandleCacheEntryState(const FDataRegistryLookup& Lookup, FCachedDataRegistryItem& CachedEntry);

	/** Handle sending completion/error callbacks */
	virtual void HandleAcquireCallbacks(const FDataRegistryLookup& Lookup, FCachedDataRegistryItem& CachedEntry);

	/** Check on any batch requests that need processing, if filter is empty will process all */
	virtual void UpdateBatchRequest(const FDataRegistryRequestId& RequestId, const FDataRegistryAcquireResult& Result);

	/** Frame-delayed callback to call success for already loaded items */
	virtual void DelayedHandleSuccessCallbacks(FDataRegistryLookup Lookup);

	/** Start an async request */
	virtual bool AcquireItemInternal(const FDataRegistryId& ItemId, const FDataRegistryLookup& Lookup, FDataRegistryItemAcquiredCallback DelegateToCall, const FDataRegistryRequestId& BatchRequestId);


	// Overrides
	virtual void BeginDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

private:
	/** Internal cache data, can't use TUniquePtr due to UObject weirdness */
	FDataRegistryCache* Cache = nullptr;

	FTimerHandle UpdateTimer;

	/** True if this registry has been initialized and is expected to respond to requests */
	bool bIsInitialized = false;

	/** True if this registry needs a runtime refresh due to asset changes */
	bool bNeedsRuntimeRefresh = false;

};
