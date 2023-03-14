// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataRegistry.h"
#include "Engine/DataTable.h"
#include "Containers/SortedMap.h"
#include "Subsystems/EngineSubsystem.h"
#include "DataRegistrySubsystem.generated.h"


/** Enum used to indicate success or failure of EvaluateCurveTableRow. */
UENUM()
enum class EDataRegistrySubsystemGetItemResult : uint8
{
	/** Found the row successfully. */
	Found,
	/** Failed to find the row. */
	NotFound,
};

/** Singleton manager that provides synchronous and asynchronous access to data registries */
UCLASS(NotBlueprintType)
class DATAREGISTRY_API UDataRegistrySubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:

	// Blueprint Interface, it is static for ease of use in custom nodes

	/**
	 * Attempts to get cached structure data stored in a DataRegistry, modifying OutItem if the item is available
	 * (EXPERIMENTAL) this version has an input param and simple bool return
	 *
	 * @param ItemID		Item identifier to lookup in cache
	 * @param OutItem		This must be the same type as the registry, if the item is found this will be filled in with the found data
	 * @returns				Returns true if the item was found and OutItem was modified
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = DataRegistry, meta = (DisplayName = "Get Data Registry Item (experimental)", CustomStructureParam = "OutItem"))
	static bool GetCachedItemBP(FDataRegistryId ItemId, UPARAM(ref) FTableRowBase& OutItem) { return false; }
	DECLARE_FUNCTION(execGetCachedItemBP);

	/**
	 * Attempts to get cached structure data stored in a DataRegistry, modifying OutItem if the item is available
	 * (EXPERIMENTAL) this version has an output param and enum result
	 *
	 * @param ItemID		Item identifier to lookup in cache
	 * @param OutItem		This must be the same type as the registry, if the item is found this will be filled in with the found data
	 * @returns				Returns true if the item was found and OutItem was modified
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = DataRegistry, meta = (DisplayName = "Find Data Registry Item (experimental)", CustomStructureParam = "OutItem", ExpandEnumAsExecs = "OutResult"))
	static void FindCachedItemBP(FDataRegistryId ItemId, EDataRegistrySubsystemGetItemResult& OutResult, FTableRowBase& OutItem) {}
	DECLARE_FUNCTION(execFindCachedItemBP);

	/**
	 * Attempts to get structure data stored in a DataRegistry cache after an async acquire, modifying OutItem if the item is available
	 *
	 * @param ItemID			Item identifier to lookup in cache
	 * @param ResolvedLookup	Resolved identifier returned by acquire function
	 * @param OutItem			This must be the same type as the registry, if the item is found this will be filled in with the found data
	 * @returns					Returns true if the item was found and OutItem was modified
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = DataRegistry, meta = (DisplayName = "Get Data Registry Item From Lookup (experimental)", CustomStructureParam = "OutItem"))
	static bool GetCachedItemFromLookupBP(FDataRegistryId ItemId, const FDataRegistryLookup& ResolvedLookup, FTableRowBase& OutItem) { return false; }
	DECLARE_FUNCTION(execGetCachedItemFromLookupBP);

	/**
	 * Starts an asynchronous acquire of a data registry item that may not yet be cached.
	 *
	 * @param ItemID			Item identifier to lookup in cache
	 * @param AcquireCallback	Delegate that will be called after acquire succeeds or failed
	 * @returns					Returns true if request was started, false on unrecoverable error
	 */
	UFUNCTION(BlueprintCallable, Category = DataRegistry, meta = (DisplayName = "Acquire Data Registry Item (experimental)") )
	static bool AcquireItemBP(FDataRegistryId ItemId, FDataRegistryItemAcquiredBPCallback AcquireCallback);

	/**
	 * Attempts to evaluate a curve stored in a DataRegistry cache using a specific input value
	 *
	 * @param ItemID		Item identifier to lookup in cache
	 * @param InputValue	Time/level/parameter input value used to evaluate curve at certain position
	 * @param DefaultValue	Value to use if no curve found or input is outside acceptable range
	 * @param OutValue		Result will be replaced with evaluated value, or default if that fails
	 */
	UFUNCTION(BlueprintCallable, Category = DataRegistry, meta = (ExpandEnumAsExecs = "OutResult"))
	static void EvaluateDataRegistryCurve(FDataRegistryId ItemId, float InputValue, float DefaultValue, EDataRegistrySubsystemGetItemResult& OutResult, float& OutValue);


	/** Returns true if this is a non-empty type, does not check if it is currently registered */
	UFUNCTION(BlueprintPure, Category = DataRegistry, meta = (ScriptMethod = "IsValid", ScriptOperator = "bool", BlueprintThreadSafe))
	static bool IsValidDataRegistryType(FDataRegistryType DataRegistryType);

	/** Converts a Data Registry Type to a string. The other direction is not provided because it cannot be validated */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (DataRegistryType)", CompactNodeTitle = "->", ScriptMethod = "ToString", BlueprintThreadSafe), Category = DataRegistry)
	static FString Conv_DataRegistryTypeToString(FDataRegistryType DataRegistryType);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (DataRegistryType)", CompactNodeTitle = "==", ScriptOperator = "==", BlueprintThreadSafe), Category = DataRegistry)
	static bool EqualEqual_DataRegistryType(FDataRegistryType A, FDataRegistryType B);

	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal (DataRegistryType)", CompactNodeTitle = "!=", ScriptOperator = "!=", BlueprintThreadSafe), Category = DataRegistry)
	static bool NotEqual_DataRegistryType(FDataRegistryType A, FDataRegistryType B);

	/** Returns true if this is a non-empty item identifier, does not check if it is currently registered */
	UFUNCTION(BlueprintPure, Category = "AssetManager", meta=(ScriptMethod="IsValid", ScriptOperator="bool", BlueprintThreadSafe))
	static bool IsValidDataRegistryId(FDataRegistryId DataRegistryId);

	/** Converts a Data Registry Id to a string. The other direction is not provided because it cannot be validated */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (DataRegistryId)", CompactNodeTitle = "->", ScriptMethod="ToString", BlueprintThreadSafe), Category = DataRegistry)
	static FString Conv_DataRegistryIdToString(FDataRegistryId DataRegistryId);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (DataRegistryId)", CompactNodeTitle = "==", ScriptOperator="==", BlueprintThreadSafe), Category = DataRegistry)
	static bool EqualEqual_DataRegistryId(FDataRegistryId A, FDataRegistryId B);

	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal (DataRegistryId)", CompactNodeTitle = "!=", ScriptOperator="!=", BlueprintThreadSafe), Category = DataRegistry)
	static bool NotEqual_DataRegistryId(FDataRegistryId A, FDataRegistryId B);


	// Native interface, works using subsystem instance

	/** Returns the global subsystem instance */
	static UDataRegistrySubsystem* Get();

	/** Finds the right registry for a type name */
	UDataRegistry* GetRegistryForType(FName RegistryType) const;

	/** Returns proper display text for an id, using the correct id format */
	FText GetDisplayTextForId(FDataRegistryId ItemId) const;

	/** Gets list of all registries, useful for iterating in UI or utilities */
	void GetAllRegistries(TArray<UDataRegistry*>& AllRegistries, bool bSortByType = true) const;

	/** Refreshes the active registry map based on what's in memory */
	void RefreshRegistryMap();

	/** Loads all registry assets and initializes them, this is called early in startup */
	void LoadAllRegistries();

	/** True if all registries should have been initialized*/
	bool AreRegistriesInitialized() const;

	/** Returns true if the system is enabled via any config scan settings, will optionally warn if not enabled */
	bool IsConfigEnabled(bool bWarnIfNotEnabled = false) const;

	/** Initializes all loaded registries and prepares them for queries */
	void InitializeAllRegistries(bool bResetIfInitialized = false);

	/** De-initializes all loaded registries */
	void DeinitializeAllRegistries();

	/** Load and initialize a specific registry, useful for plugins. This can hitch so the asset should be preloaded elsewhere if needed */
	bool LoadRegistryPath(const FSoftObjectPath& RegistryAssetPath);

	/** Removes specific data registry asset from the registration map, can be undone with LoadRegistryPath */
	bool IgnoreRegistryPath(const FSoftObjectPath& RegistryAssetPath);

	/** Resets state for all registries, call when gameplay has concluded to destroy caches */
	void ResetRuntimeState();

	/** Handles changes to DataRegistrySettings while engine is running */
	void ReinitializeFromConfig();

	/** 
	 * Attempt to register a specified asset with all active sources that allow dynamic registration, returning true if anything changed.
	 * This will fail if the registry does not exist yet.
	 *
	 * @param RegistryType		Type to register with, if invalid will try all registries
	 * @param AssetData			Filled in asset data of asset to attempt to register
	 * @Param AssetPriority		Priority of asset relative to others, higher numbers will be searched first
	 */
	bool RegisterSpecificAsset(FDataRegistryType RegistryType, FAssetData& AssetData, int32 AssetPriority = 0);

	/** Removes references to a specific asset, returns bool if it was removed */
	bool UnregisterSpecificAsset(FDataRegistryType RegistryType, const FSoftObjectPath& AssetPath);

	/** Unregisters all previously registered assets in a specific registry with a specific priority, can be used as a batch reset. Returns number of assets unregistered */
	int32 UnregisterAssetsWithPriority(FDataRegistryType RegistryType, int32 AssetPriority);

	/** Schedules registration of assets by path, this will happen immediately or will be queued if the data registries don't exist yet */
	void PreregisterSpecificAssets(const TMap<FDataRegistryType, TArray<FSoftObjectPath>>& AssetMap, int32 AssetPriority = 0);

	/** Gets the cached or precached data and struct type. The return value specifies the cache safety for the data */
	FDataRegistryCacheGetResult GetCachedItemRaw(const uint8*& OutItemMemory, const UScriptStruct*& OutItemStruct, const FDataRegistryId& ItemId) const;

	/** Gets the cached or precached data and struct type using an async acquire result. The return value specifies the cache safety for the data */
	FDataRegistryCacheGetResult GetCachedItemRawFromLookup(const uint8*& OutItemMemory, const UScriptStruct*& OutItemStruct, const FDataRegistryId& ItemId, const FDataRegistryLookup& Lookup) const;

	/** Computes an evaluated curve value, as well as the actual curve if it is found. The return value specifies the cache safety for the curve */
	FDataRegistryCacheGetResult EvaluateCachedCurve(float& OutValue, const FRealCurve*& OutCurve, FDataRegistryId ItemId, float InputValue, float DefaultValue = 0.0f) const;

	/** Returns a cached item of specified struct type. This will return null if the item is not already in memory */
	template <class T>
	const T* GetCachedItem(const FDataRegistryId& ItemId) const
	{
		const UDataRegistry* FoundRegistry = GetRegistryForType(ItemId.RegistryType);
		if (FoundRegistry)
		{
			return FoundRegistry->GetCachedItem<T>(ItemId);
		}
		return nullptr;
	}

	/** Start an async load of an item, delegate will be called on success or failure of acquire. Returns false if delegate could not be scheduled */
	bool AcquireItem(const FDataRegistryId& ItemId, FDataRegistryItemAcquiredCallback DelegateToCall) const;


	// Debug commands, bound as cvars or callable manually

	/** Outputs all registered types and some info */
	static void DumpRegistryTypeSummary();

	/** Dumps out a text representation of every item in the registry */
	static void DumpCachedItems(const TArray<FString>& Args);

protected:
	typedef TPair<FName, UDataRegistry*> FRegistryMapPair;
	TSortedMap<FName, UDataRegistry*, FDefaultAllocator, FNameFastLess> RegistryMap;

	// Initialization order, need to wait for other early-load systems to initialize
	virtual void PostEngineInit();
	virtual void PostGameplayTags();
	virtual void PostAssetManager();
	virtual void ApplyPreregisterMap(UDataRegistry* Registry);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// Paths that will be scanned for registries
	TArray<FString> AssetScanPaths;

	// Specific registries to load, will be added to AssetScanPaths at scan time
	TArray<FSoftObjectPath> RegistryPathsToLoad;

	// Specific registries to avoid registering, may be in memory but will not be registered
	TArray<FSoftObjectPath> RegistryPathsToIgnore;

	// List of assets to attempt to register when data registries come online
	typedef TPair<FSoftObjectPath, int32> FPreregisterAsset;
	TMap<FDataRegistryType, TArray<FPreregisterAsset>> PreregisterAssetMap;

	// True if initialization has finished and registries were scanned, will be false if not config enabled
	bool bFullyInitialized = false;

	// True if initialization is ready to start, will be true even if config disabled
	bool bReadyForInitialization = false;

#if WITH_EDITOR
	virtual void PreBeginPIE(bool bStartSimulate);
	virtual void EndPIE(bool bStartSimulate);
#endif
};

/* Test actor, move later
UCLASS(Blueprintable)
class ADataRegistryTestActor : public AActor
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category=DataRegistry)
	bool TestSyncRead(FDataRegistryId RegistryId);

	UFUNCTION(BlueprintCallable, Category = DataRegistry)
	bool TestAsyncRead(FDataRegistryId RegistryId);

	void AsyncReadComplete(const FDataRegistryAcquireResult& Result);
};
*/
