// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/StringFwd.h"
#include "CoreTypes.h"
#include "HAL/PlatformCrt.h"
#include "Misc/AssetRegistryInterface.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

class FArchive;
class FAssetDataTagMap;
class FAssetDataTagMapSharedView;
class FDependsNode;
class FString;
struct FARCompiledFilter;
struct FPrimaryAssetId;
template <typename FuncType> class TFunctionRef;

namespace UE::AssetRegistry
{
	class FAssetRegistryImpl;
}

struct FAssetRegistryHeader;

/** Load/Save options used to modify how the cache is serialized. These are read out of the AssetRegistry section of Engine.ini and can be changed per platform. */
struct FAssetRegistrySerializationOptions
{
	FAssetRegistrySerializationOptions(UE::AssetRegistry::ESerializationTarget Target = UE::AssetRegistry::ESerializationTarget::ForGame)
	{
		if (Target == UE::AssetRegistry::ESerializationTarget::ForDevelopment)
		{
			InitForDevelopment();
		}
	}

	/** True rather to load/save registry at all */
	bool bSerializeAssetRegistry = false;

	/** True rather to load/save dependency info. If true this will handle hard and soft package references */
	bool bSerializeDependencies = false;

	/** True rather to load/save dependency info for Name references,  */
	bool bSerializeSearchableNameDependencies = false;

	/** True rather to load/save dependency info for Manage references,  */
	bool bSerializeManageDependencies = false;

	/** If true will read/write FAssetPackageData */
	bool bSerializePackageData = false;

	/** True if CookFilterlistTagsByClass is an allow list. False if it is a deny list. */
	bool bUseAssetRegistryTagsAllowListInsteadOfDenyList = false;

	/** True if we want to only write out asset data if it has valid tags. This saves memory by not saving data for things like textures */
	bool bFilterAssetDataWithNoTags = false;

	/** True if we also want to filter out dependency data for assets that have no tags. Only filters if bFilterAssetDataWithNoTags is also true */
	bool bFilterDependenciesWithNoTags = false;

	/** Filter out searchable names from dependency data */
	bool bFilterSearchableNames = false;

	/**
	 * Keep tags intended for the cooker's output DevelopmentAssetRegistry. this flag defaults to false and is set to
	 * true only by the cooker.
	 */
	bool bKeepDevelopmentAssetRegistryTags = false;

	/** The map of class pathname to tag set of tags that are allowed in cooked builds. This is either an allow list or deny list depending on bUseAssetRegistryTagsAllowListInsteadOfDenyList */
	TMap<FTopLevelAssetPath, TSet<FName>> CookFilterlistTagsByClass;

	/** Tag keys whose values should be stored as FName in cooked builds */
	TSet<FName> CookTagsAsName;

	/** Tag keys whose values should be stored as FRegistryExportPath in cooked builds */
	TSet<FName> CookTagsAsPath;

	/** Disable all filters */
	void DisableFilters()
	{
		bFilterAssetDataWithNoTags = false;
		bFilterDependenciesWithNoTags = false;
		bFilterSearchableNames = false;
	}

private:
	void InitForDevelopment()
	{
		bSerializeAssetRegistry = bSerializeDependencies = bSerializeSearchableNameDependencies = bSerializeManageDependencies = bSerializePackageData = true;
		DisableFilters();
	}
};

struct FAssetRegistryLoadOptions
{
	FAssetRegistryLoadOptions() = default;
	explicit FAssetRegistryLoadOptions(const FAssetRegistrySerializationOptions& Options)
		: bLoadDependencies(Options.bSerializeDependencies)
		, bLoadPackageData(Options.bSerializePackageData)
	{}

	bool bLoadDependencies = true;
	bool bLoadPackageData = true;
	int32 ParallelWorkers = 0;
};

struct FAssetRegistryPruneOptions
{
	TSet<FName> RequiredPackages;
	TSet<FName> RemovePackages;
	TSet<int32> ChunksToKeep;
	FAssetRegistrySerializationOptions Options;

	/* Remove FDependsNodes that do not point to packages */
	bool bRemoveDependenciesWithoutPackages = false;

	/* List of types that should not be pruned because they do not have a package */
	TSet<FPrimaryAssetType> RemoveDependenciesWithoutPackagesKeepPrimaryAssetTypes;
};

namespace UE::AssetRegistry::Private
{
	/* 
	* Key type for TSet<FAssetData*> in the asset registry.
	* Top level assets are searched for by their asset path as two names (e.g. '/Path/ToPackageName' + 'AssetName')
	* Other assets (e.g. external actors) are searched for by their full path with the whole outer chain as a single name. 
	* (e.g. '/Path/To/Package.TopLevel:Subobject' + 'DeeperSubobject')
	*/
	struct FCachedAssetKey
	{
		explicit FCachedAssetKey(const FAssetData* InAssetData);
		explicit FCachedAssetKey(const FAssetData& InAssetData);
		explicit FCachedAssetKey(FTopLevelAssetPath InAssetPath);
		explicit FCachedAssetKey(const FSoftObjectPath& InObjectPath);

		FString ToString() const;
		int32 Compare(const FCachedAssetKey& Other) const;	// Order asset keys with fast non-lexical comparison
		void AppendString(FStringBuilderBase& Builder) const;

		FName OuterPath = NAME_None;
		FName ObjectName = NAME_None;
	};

	inline FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FCachedAssetKey& Key);
	inline bool operator==(const FCachedAssetKey& A, const FCachedAssetKey& B);
	inline bool operator!=(const FCachedAssetKey& A, const FCachedAssetKey& B);
	inline uint32 GetTypeHash(const FCachedAssetKey& A);

	/* 
	* Policy type for TSet<FAssetData*> to use FCachedAssetKey for hashing/equality.
	* This allows is to store just FAssetData* in the map without storing an extra copy of the key fields to save memory.
	*/
	struct FCachedAssetKeyFuncs
	{
		using KeyInitType = FCachedAssetKey;
		using ElementInitType = void; // TSet doesn't actually use this type 

		enum { bAllowDuplicateKeys = false };

		static FORCEINLINE KeyInitType GetSetKey(const FAssetData* Element)
		{
			return FCachedAssetKey(*Element);
		}

		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			return A == B;
		}

		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
		{
			return GetTypeHash(Key);
		}
	};

	using FAssetDataMap = TSet<FAssetData*, FCachedAssetKeyFuncs>;
	using FConstAssetDataMap = TSet<const FAssetData*, FCachedAssetKeyFuncs>;
}

/** The state of an asset registry, this is used internally by IAssetRegistry to represent the disk cache, and is also accessed directly to save/load cooked caches */
class FAssetRegistryState
{
	using FCachedAssetKey = UE::AssetRegistry::Private::FCachedAssetKey;
public:
	// These types are an implementation detail and they and the functions which take/return them are subject to change without deprecation warnings.
	using FAssetDataMap = UE::AssetRegistry::Private::FAssetDataMap;
	using FConstAssetDataMap = UE::AssetRegistry::Private::FConstAssetDataMap;

	FAssetRegistryState() = default;
	FAssetRegistryState(const FAssetRegistryState&) = delete;
	FAssetRegistryState(FAssetRegistryState&& Rhs) { *this = MoveTemp(Rhs); }
	ASSETREGISTRY_API ~FAssetRegistryState();

	FAssetRegistryState& operator=(const FAssetRegistryState&) = delete;
	ASSETREGISTRY_API FAssetRegistryState& operator=(FAssetRegistryState&& O);

	/**
	 * Enum controlling how we initialize this state
	 */
	enum class EInitializationMode
	{
		Rebuild,
		OnlyUpdateExisting,
		Append,
		OnlyUpdateNew,
	};

	/**
	 * Does the given path contain assets?
	 * @param bARFiltering Whether to apply filtering from UE::AssetRegistry::FFiltering (false by default)
	 * @note This function doesn't recurse into sub-paths.
	 */
	ASSETREGISTRY_API bool HasAssets(const FName PackagePath, bool bARFiltering=false) const;

	/**
	 * Gets asset data for all assets that match the filter.
	 * Assets returned must satisfy every filter component if there is at least one element in the component's array.
	 * Assets will satisfy a component if they match any of the elements in it.
	 *
	 * @param Filter filter to apply to the assets in the AssetRegistry
	 * @param PackageNamesToSkip explicit list of packages to skip, because they were already added
	 * @param OutAssetData the list of assets in this path
	 * @param bSkipARFilteredAssets If true, skip assets that are skipped by UE::AssetRegistry::FFiltering (false by default)
	 */
	ASSETREGISTRY_API bool GetAssets(const FARCompiledFilter& Filter, const TSet<FName>& PackageNamesToSkip, TArray<FAssetData>& OutAssetData, bool bSkipARFilteredAssets = false) const;

	/**
	 * Enumerate asset data for all assets that match the filter.
	 * Assets returned must satisfy every filter component if there is at least one element in the component's array.
	 * Assets will satisfy a component if they match any of the elements in it.
	 *
	 * @param Filter filter to apply to the assets in the AssetRegistry
	 * @param PackageNamesToSkip explicit list of packages to skip, because they were already added
	 * @param Callback function to call for each asset data enumerated
	 * @param bARFiltering Whether to apply filtering from UE::AssetRegistry::FFiltering (false by default)
	 */
	ASSETREGISTRY_API bool EnumerateAssets(const FARCompiledFilter& Filter, const TSet<FName>& PackageNamesToSkip, TFunctionRef<bool(const FAssetData&)> Callback, bool bARFiltering = false) const;

	/**
	 * Gets asset data for all assets in the registry state.
	 *
	 * @param PackageNamesToSkip explicit list of packages to skip, because they were already added
	 * @param OutAssetData the list of assets
	 * @param bARFiltering Whether to apply filtering from UE::AssetRegistry::FFiltering (false by default)
	 */
	ASSETREGISTRY_API bool GetAllAssets(const TSet<FName>& PackageNamesToSkip, TArray<FAssetData>& OutAssetData, bool bARFiltering = false) const;

	/**
	 * Enumerates asset data for all assets in the registry state.
	 *
	 * @param PackageNamesToSkip explicit list of packages to skip, because they were already added
	 * @param Callback function to call for each asset data enumerated
	 * @param bARFiltering Whether to apply filtering from UE::AssetRegistry::FFiltering (false by default)
	 */
	ASSETREGISTRY_API bool EnumerateAllAssets(const TSet<FName>& PackageNamesToSkip, TFunctionRef<bool(const FAssetData&)> Callback, bool bARFiltering = false) const;
	ASSETREGISTRY_API void EnumerateAllAssets(TFunctionRef<void(const FAssetData&)> Callback) const;

	/**
	 * Calls the callback with the LongPackageName of each path that has assets as direct children.
	 * Callback will not be called for parent paths that have childpaths with direct children but do not
	 * have direct children of their own.
	 */
	ASSETREGISTRY_API void EnumerateAllPaths(TFunctionRef<void(FName PathName)> Callback) const;

	/**
	 * Gets the LongPackageNames for all packages with the given PackageName.
	 * Call to check existence of a LongPackageName or find all packages with a ShortPackageName.
	 *
	 * @param PackageName Name of the package to find, may be a LongPackageName or ShortPackageName.
	 * @param OutPackageNames All discovered matching LongPackageNames are appended to this array.
	 */
	ASSETREGISTRY_API void GetPackagesByName(FStringView PackageName, TArray<FName>& OutPackageNames) const;

	/**
	 * Returns the first LongPackageName found for the given PackageName.
	 * Issues a warning and returns the first (sorted lexically) if there is more than one.
	 * Call to check existence of a LongPackageName or find a package with a ShortPackageName.
	 *
	 * @param PackageName Name of the package to find, may be a LongPackageName or ShortPackageName.
	 * @return The first LongPackageName of the matching package, or NAME_None if not found.
	 */
	ASSETREGISTRY_API FName GetFirstPackageByName(FStringView PackageName) const;

	/**
	 * Appends a list of packages and searchable names that are referenced by the supplied package or name. (On disk references ONLY)
	 *
	 * @param AssetIdentifier	the name of the package/name for which to gather dependencies
	 * @param OutDependencies	a list of things that are referenced by AssetIdentifier
	 * @param Category	which category(ies) of dependencies to include in the output list. Dependencies matching ANY of the OR'd categories will be returned.
	 * @param Flags	which flags are required present or not present on the dependencies. Dependencies matching ALL required and NONE excluded bits will be returned. For each potentially returned dependency, flags not applicable to their category are ignored.
	 */
	ASSETREGISTRY_API bool GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const;
	ASSETREGISTRY_API bool GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetDependency>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const;

	/**
	 * Appends a list of packages and searchable names that reference the supplied package or name. (On disk references ONLY)
	 *
	 * @param AssetIdentifier	the name of the package/name for which to gather dependencies
	 * @param OutReferencers	a list of things that reference AssetIdentifier
	 * @param Category	which category(ies) of dependencies to include in the output list. Dependencies matching ANY of the OR'd categories will be returned.
	 * @param Flags	which flags are required present or not present on the dependencies. Dependencies matching ALL required and NONE excluded bits will be returned. For each potentially returned dependency, flags not applicable to their category are ignored.
	 */
	ASSETREGISTRY_API bool GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutReferencers, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const;
	ASSETREGISTRY_API bool GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetDependency>& OutReferencers, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const;

	/**
	 * Gets the asset data for the specified object path
	 *
	 * @param ObjectPath the path of the object to be looked up
	 * @return the assets data, null if not found
	 */
	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated, use FSoftObjectPath instead.")
	const FAssetData* GetAssetByObjectPath(const FName ObjectPath) const
	{
		return GetAssetByObjectPath(FSoftObjectPath(ObjectPath.ToString()));
	}

	/**
	 * Gets the asset data for the specified object path
	 *
	 * @param ObjectPath the path of the object to be looked up
	 * @return the assets data, null if not found
	 */
	const FAssetData* GetAssetByObjectPath(const FSoftObjectPath& ObjectPath) const 
	{
		FCachedAssetKey Key(ObjectPath);
		FAssetData* const* FoundAsset = CachedAssets.Find(Key);
		if (FoundAsset)
		{
			return *FoundAsset;
		}

		return nullptr;
	}

	/**
	 * Gets the asset data for the specified package name
	 *
	 * @param PackageName the path of the package to be looked up
	 * @return an array of AssetData*, empty if nothing found
	 */
	TArrayView<FAssetData const* const> GetAssetsByPackageName(const FName PackageName) const
	{
		if (const TArray<FAssetData*, TInlineAllocator<1>>* FoundAssetArray = CachedAssetsByPackageName.Find(PackageName))
		{
			return MakeArrayView(*FoundAssetArray);
		}

		return TArrayView<FAssetData* const>();
	}

	/**
	 * Gets the asset data for the specified asset class
	 *
	 * @param ClassName the class name of the assets to look for
	 * @return An array of AssetData*, empty if nothing found
	 */
	UE_DEPRECATED(5.1, "Class names are now represented by path names. Please use GetAssetsByClassPathName")
	ASSETREGISTRY_API const TArray<const FAssetData*>& GetAssetsByClassName(const FName ClassName) const;

	/**
	 * Gets the asset data for the specified asset class
	 *
	 * @param ClassPathName the class path name of the assets to look for
	 * @return An array of AssetData*, empty if nothing found
	 */
	const TArray<const FAssetData*>& GetAssetsByClassPathName(const FTopLevelAssetPath ClassPathName) const
	{
		static TArray<const FAssetData*> InvalidArray;
		const TArray<FAssetData*>* FoundAssetArray = CachedAssetsByClass.Find(ClassPathName);
		if (FoundAssetArray)
		{
			return reinterpret_cast<const TArray<const FAssetData*>&>(*FoundAssetArray);
		}

		return InvalidArray;
	}

	/**
	 * Gets the asset data for the specified asset tag
	 *
	 * @param TagName the tag name to search for
	 * @return An array of AssetData*, empty if nothing found
	 */
	const TArray<const FAssetData*>& GetAssetsByTagName(const FName TagName) const
	{
		static TArray<const FAssetData*> InvalidArray;
		const TArray<FAssetData*>* FoundAssetArray = CachedAssetsByTag.Find(TagName);
		if (FoundAssetArray)
		{
			return reinterpret_cast<const TArray<const FAssetData*>&>(*FoundAssetArray);
		}

		return InvalidArray;
	}

	/** Returns const version of internal ObjectPath->AssetData map for fast iteration */
	const FConstAssetDataMap& GetAssetDataMap() const
	{
		return reinterpret_cast<const FConstAssetDataMap&>(CachedAssets);
	}

	/** Returns const version of internal Tag->AssetDatas map for fast iteration */
	const TMap<FName, const TArray<const FAssetData*>> GetTagToAssetDatasMap() const
	{
		return reinterpret_cast<const TMap<FName, const TArray<const FAssetData*>>&>(CachedAssetsByTag);
	}

	/** Returns const version of internal PackageName->PackageData map for fast iteration */
	const TMap<FName, const FAssetPackageData*>& GetAssetPackageDataMap() const
	{
		return reinterpret_cast<const TMap<FName, const FAssetPackageData*>&>(CachedPackageData);
	}

	/** Get the set of primary assets contained in this state */
	ASSETREGISTRY_API void GetPrimaryAssetsIds(TSet<FPrimaryAssetId>& OutPrimaryAssets) const;

	/** Returns pointer to the asset package data */
	ASSETREGISTRY_API const FAssetPackageData* GetAssetPackageData(FName PackageName) const;
	ASSETREGISTRY_API FAssetPackageData* GetAssetPackageData(FName PackageName);

	/** Returns all package names */
	void GetPackageNames(TArray<FName>& OutPackageNames) const
	{
		OutPackageNames.Reserve(CachedAssetsByPackageName.Num());
		for (auto It = CachedAssetsByPackageName.CreateConstIterator(); It; ++It)
		{
			OutPackageNames.Add(It.Key());
		}
	}

	/** Finds an existing package data, or creates a new one to modify */
	ASSETREGISTRY_API FAssetPackageData* CreateOrGetAssetPackageData(FName PackageName);

	/** Removes existing package data */
	ASSETREGISTRY_API bool RemovePackageData(FName PackageName);

	/** Adds the asset data to the lookup maps */
	ASSETREGISTRY_API void AddAssetData(FAssetData* AssetData);

	/** Add the given tags/values to the asset data associated with the given object path, if it exists */
	ASSETREGISTRY_API void AddTagsToAssetData(const FSoftObjectPath& InObjectPath, FAssetDataTagMap&& InTagsAndValues);

	/** Finds an existing asset data based on object path and updates it with the new value and updates lookup maps */
	ASSETREGISTRY_API void UpdateAssetData(const FAssetData& NewAssetData, bool bCreateIfNotExists=false);
	ASSETREGISTRY_API void UpdateAssetData(FAssetData&& NewAssetData, bool bCreateIfNotExists = false);

	/** Updates an existing asset data with the new value and updates lookup maps */
	ASSETREGISTRY_API void UpdateAssetData(FAssetData* AssetData, const FAssetData& NewAssetData, bool* bOutModified = nullptr);
	ASSETREGISTRY_API void UpdateAssetData(FAssetData* AssetData, FAssetData&& NewAssetData, bool* bOutModified = nullptr);

	/**
	 * Updates all asset data package flags in the specified package
	 *
	 * @param PackageName the package name
	 * @param PackageFlags the package flags to set
	 * @return True if any assets exists in the package
	 */
	ASSETREGISTRY_API bool UpdateAssetDataPackageFlags(FName PackageName, uint32 PackageFlags);

	/** Removes the asset data from the lookup maps */
	ASSETREGISTRY_API void RemoveAssetData(FAssetData* AssetData, bool bRemoveDependencyData,
		bool& bOutRemovedAssetData, bool& bOutRemovedPackageData);
	ASSETREGISTRY_API void RemoveAssetData(const FSoftObjectPath& SoftObjectPath, bool bRemoveDependencyData,
		bool& bOutRemovedAssetData, bool& bOutRemovedPackageData);

	/**
	 * Clear all dependencies of the given category from the given AssetIdentifier (e.g. package).
	 * Also clears the referencer link from each of the dependencies.
	 */
	ASSETREGISTRY_API void ClearDependencies(const FAssetIdentifier& AssetIdentifier, UE::AssetRegistry::EDependencyCategory Category);
	/**
	 * Add the given dependencies to the given AssetIdentifier (e.g. package).
	 * Also adds a referencer link on each of the dependencies.
	 */
	ASSETREGISTRY_API void AddDependencies(const FAssetIdentifier& AssetIdentifier, TConstArrayView<FAssetDependency> Dependencies);
	/**
	 * Clears existing dependencies of the given Category(s) and assigns the input Dependencies. Gives an error if 
	 * any elements of Dependencies are outside of the Category(s).
	 */
	ASSETREGISTRY_API void SetDependencies(const FAssetIdentifier& AssetIdentifier, TConstArrayView<FAssetDependency> Dependencies,
		UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All);
	/**
	 * Clear all referencers of the given category from the given AssetIdentifier (e.g. package).
	 * Also clears the dependency link from each of the referencers.
	 */
	ASSETREGISTRY_API void ClearReferencers(const FAssetIdentifier& AssetIdentifier, UE::AssetRegistry::EDependencyCategory Category);
	/**
	 * Add a dependency on the given AssetIdentifier (e.g. package) from each of the Referencers.
	 * Also adds a referencer link to each referencer on the AssetIdentifer's node.
	 */
	ASSETREGISTRY_API void AddReferencers(const FAssetIdentifier& AssetIdentifier, TConstArrayView<FAssetDependency> Referencers);
	/**
	 * Clears existing referencers of the given Category(s) and assigns the input Referencers. Gives an error if
	 * any elements of Referencers are outside of the Category(s).
	 */
	ASSETREGISTRY_API void SetReferencers(const FAssetIdentifier& AssetIdentifier, TConstArrayView<FAssetDependency> Referencers,
		UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All);

	/** Resets to default state */
	ASSETREGISTRY_API void Reset();

	/** Initializes cache from existing set of asset data and depends nodes */
	ASSETREGISTRY_API void InitializeFromExisting(const FAssetDataMap& AssetDataMap, const TMap<FAssetIdentifier, FDependsNode*>& DependsNodeMap, const TMap<FName, FAssetPackageData*>& AssetPackageDataMap, const FAssetRegistrySerializationOptions& Options, EInitializationMode InitializationMode = EInitializationMode::Rebuild);
	void InitializeFromExisting(const FAssetRegistryState& Existing, const FAssetRegistrySerializationOptions& Options, EInitializationMode InitializationMode = EInitializationMode::Rebuild)
	{
		InitializeFromExisting(Existing.CachedAssets, Existing.CachedDependsNodes, Existing.CachedPackageData, Options, InitializationMode);
	}

	/** 
	 * Prunes an asset cache, this removes asset data, nodes, and package data that isn't needed. 
	 * @param RequiredPackages If set, only these packages will be maintained. If empty it will keep all unless filtered by other parameters
	 * @param RemovePackages These packages will be removed from the current set
	 * @param ChunksToKeep The list of chunks that are allowed to remain. Any assets in other chunks are pruned. If empty, all assets are kept regardless of chunk
	 * @param Options Serialization options to read filter info from
	 */
	ASSETREGISTRY_API void PruneAssetData(const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages, const TSet<int32> ChunksToKeep, const FAssetRegistrySerializationOptions& Options);
	ASSETREGISTRY_API void PruneAssetData(const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages, const FAssetRegistrySerializationOptions& Options);
	ASSETREGISTRY_API void Prune(const FAssetRegistryPruneOptions& PruneOptions);

	
	/**
	 * Initializes a cache from an existing using a set of filters. This is more efficient than calling InitalizeFromExisting and then PruneAssetData.
	 * @param ExistingState State to use initialize from
	 * @param RequiredPackages If set, only these packages will be maintained. If empty it will keep all unless filtered by other parameters
	 * @param RemovePackages These packages will be removed from the current set
	 * @param ChunksToKeep The list of chunks that are allowed to remain. Any assets in other chunks are pruned. If empty, all assets are kept regardless of chunk
	 * @param Options Serialization options to read filter info from
	 */
	ASSETREGISTRY_API void InitializeFromExistingAndPrune(const FAssetRegistryState& ExistingState, const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages, const TSet<int32> ChunksToKeep, const FAssetRegistrySerializationOptions& Options);

	/** Edit every AssetData's Tags to remove Tags that are filtered out by the filtering rules in Options */
	ASSETREGISTRY_API void FilterTags(const FAssetRegistrySerializationOptions& Options);


	/** Serialize the registry to/from a file, skipping editor only data */
	ASSETREGISTRY_API bool Serialize(FArchive& Ar, const FAssetRegistrySerializationOptions& Options);

	/** Save without editor-only data */
	ASSETREGISTRY_API bool Save(FArchive& Ar, const FAssetRegistrySerializationOptions& Options);
	ASSETREGISTRY_API bool Load(FArchive& Ar, const FAssetRegistryLoadOptions& Options = FAssetRegistryLoadOptions(), FAssetRegistryVersion::Type* OutVersion = nullptr);

	/** 
	* Example Usage:
	*	FAssetRegistryState AssetRegistry;
	*	bool bSucceeded = FAssetRegistryState::LoadFromDisk(TEXT("Path/To/AR"), FAssetRegistryLoadOptions(), AssetRegistry);
	*/
	static ASSETREGISTRY_API bool LoadFromDisk(const TCHAR* InPath, const FAssetRegistryLoadOptions& InOptions, FAssetRegistryState& OutState, FAssetRegistryVersion::Type* OutVersion = nullptr);

	/** Returns memory size of entire registry, optionally logging sizes */
	ASSETREGISTRY_API SIZE_T GetAllocatedSize(bool bLogDetailed = false) const;

	/** Checks a filter to make sure there are no illegal entries */
	static ASSETREGISTRY_API bool IsFilterValid(const FARCompiledFilter& Filter);

	/** Returns the number of assets in this state */
	int32 GetNumAssets() const { return NumAssets; }

	/** Returns the number of packages in this state */
	int32 GetNumPackages() const { return CachedAssetsByPackageName.Num(); }

#if ASSET_REGISTRY_STATE_DUMPING_ENABLED
	/**
	 * Writes out the state in textual form. Use arguments to control which segments to emit.
	 * @param Arguments List of segments to emit. Possible values: 'ObjectPath', 'PackageName', 'Path', 'Class', 'Tag', 'Dependencies' and 'PackageData'
	 * @param OutPages Textual representation will be written to this array; each entry will have LinesPerPage lines of the full dump.
	 * @param LinesPerPage - how many lines should be combined into each string element of OutPages, for e.g. breaking up the dump into separate files.
	 *        To facilitate diffing between similar-but-different registries, the actual number of lines per page will be slightly less than LinesPerPage; we introduce partially deterministic pagebreaks near the end of each page.
	 */
	ASSETREGISTRY_API void Dump(const TArray<FString>& Arguments, TArray<FString>& OutPages, int32 LinesPerPage=1) const;
#endif

private:
	template<class Archive>
	void Load(Archive&& Ar, const FAssetRegistryHeader& Header, const FAssetRegistryLoadOptions& Options);

	/** Initialize the lookup maps */
	void SetAssetDatas(TArrayView<FAssetData> AssetDatas, const FAssetRegistryLoadOptions& Options);

	/** Find the first non-redirector dependency node starting from InDependency. */
	FDependsNode* ResolveRedirector(FDependsNode* InDependency, const FAssetDataMap& InAllowedAssets, TMap<FDependsNode*, FDependsNode*>& InCache);

	/** Finds an existing node for the given package and returns it, or returns null if one isn't found */
	FDependsNode* FindDependsNode(const FAssetIdentifier& Identifier) const;

	/** Creates a node in the CachedDependsNodes map or finds the existing node and returns it */
	FDependsNode* CreateOrFindDependsNode(const FAssetIdentifier& Identifier);

	/** Removes the depends node and updates the dependencies to no longer contain it as as a referencer. */
	bool RemoveDependsNode(const FAssetIdentifier& Identifier);

	/** Filter a set of tags and output a copy of the filtered set. */
	static void FilterTags(const FAssetDataTagMapSharedView& InTagsAndValues, FAssetDataTagMap& OutTagsAndValues, const TSet<FName>* ClassSpecificFilterList, const FAssetRegistrySerializationOptions & Options);

	void LoadDependencies(FArchive& Ar);
	void LoadDependencies_BeforeFlags(FArchive& Ar, bool bSerializeDependencies, FAssetRegistryVersion::Type Version);

	void SetTagsOnExistingAsset(FAssetData* AssetData, FAssetDataTagMap&& NewTags);

	void SetDependencyNodeSorting(bool bSortDependencies, bool bSortReferencers);

	void RemoveAssetData(FAssetData* AssetData, const FCachedAssetKey& Key, bool bRemoveDependencyData,
		bool& bOutRemovedAssetData, bool& bOutRemovedPackageData);

	/**
	 * Returns true if the given package should be filtered from the results because the package belongs to an unmounted content path.
	 * This can only happen when loading a cooked asset registry (@see bCookedGlobalAssetRegistryState), as it may contain state for plugins that are not currently loaded.
	 */
	bool IsPackageUnmountedAndFiltered(const FName PackageName) const;

	/** Set of asset data for assets saved to disk. Searched via path name types, implicitly converted to FCachedAssetKey. */
	FAssetDataMap CachedAssets;

	/** The map of package names to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*, TInlineAllocator<1>> > CachedAssetsByPackageName;

	/** The map of long package path to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByPath;

	/** The map of class name to asset data for assets saved to disk */
	TMap<FTopLevelAssetPath, TArray<FAssetData*> > CachedAssetsByClass;

	/** The map of asset tag to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByTag;

	/** A map of object names to dependency data */
	TMap<FAssetIdentifier, FDependsNode*> CachedDependsNodes;

	/** A map of Package Names to Package Data */
	TMap<FName, FAssetPackageData*> CachedPackageData;

	/** When loading a registry from disk, we can allocate all the FAssetData objects in one chunk, to save on 10s of thousands of heap allocations */
	TArray<FAssetData*> PreallocatedAssetDataBuffers;
	TArray<FDependsNode*> PreallocatedDependsNodeDataBuffers;
	TArray<FAssetPackageData*> PreallocatedPackageDataBuffers;

	/** Counters for asset/depends data memory allocation to ensure that every FAssetData and FDependsNode created is deleted */
	int32 NumAssets = 0;
	int32 NumDependsNodes = 0;
	int32 NumPackageData = 0;

	/** True if this asset registry state was loaded from a cooked asset registry */
	bool bCookedGlobalAssetRegistryState = false;

	friend class UAssetRegistryImpl;
	friend class UE::AssetRegistry::FAssetRegistryImpl;
};

namespace UE::AssetRegistry::Private
{
	FORCEINLINE uint32 HashCombineQuick(uint32 A, uint32 B)
	{
		return A ^ (B + 0x9e3779b9 + (A << 6) + (A >> 2));
	}

	inline FCachedAssetKey::FCachedAssetKey(const FAssetData* InAssetData)
	{
		if (!InAssetData)
		{
			return;
		}

#if WITH_EDITORONLY_DATA
		if (!InAssetData->GetOptionalOuterPathName().IsNone())
		{
			OuterPath = InAssetData->GetOptionalOuterPathName();
		}
		else
#endif
		{
			OuterPath = InAssetData->PackageName;
		}
		ObjectName = InAssetData->AssetName;
	}

	inline FCachedAssetKey::FCachedAssetKey(const FAssetData& InAssetData)
		: FCachedAssetKey(&InAssetData)
	{
	}

	inline FCachedAssetKey::FCachedAssetKey(FTopLevelAssetPath InAssetPath)
		: OuterPath(InAssetPath.GetPackageName())
		, ObjectName(InAssetPath.GetAssetName())
	{
	}

	inline FCachedAssetKey::FCachedAssetKey(const FSoftObjectPath& InObjectPath)
	{
		if (InObjectPath.GetAssetFName().IsNone())
		{
			// Packages themselves never appear in the asset registry
			return;
		}
		else if (InObjectPath.GetSubPathString().IsEmpty())
		{
			// If InObjectPath represents a top-level asset we can just take the existing FNames.
			OuterPath = InObjectPath.GetLongPackageFName();
			ObjectName = InObjectPath.GetAssetFName();
		}
		else
		{
			// If InObjectPath represents a subobject we need to split the path into the path of the outer and the name of the innermost object.
			TStringBuilder<FName::StringBufferSize> Builder;
			InObjectPath.ToString(Builder);

			const FAssetPathParts Parts = SplitIntoOuterPathAndAssetName(Builder);

			// This should be impossible as at bare minimum concatenating the package name and asset name should add a separator
			check(!Parts.OuterPath.IsEmpty() && !Parts.InnermostName.IsEmpty()); 

			// Don't create FNames for this query struct. If the AssetData exists to find, the FName will already exist due to OptionalOuterPath on FAssetData.
			OuterPath = FName(Parts.OuterPath, FNAME_Find); 
			ObjectName = FName(Parts.InnermostName);
		}
	}
	inline FString FCachedAssetKey::ToString() const
	{
		TStringBuilder<FName::StringBufferSize> Builder;
		AppendString(Builder);
		return FString(Builder);
	}

	inline int32 FCachedAssetKey::Compare(const FCachedAssetKey& Other) const
	{
		if (OuterPath == Other.OuterPath)
		{
			return ObjectName.CompareIndexes(Other.ObjectName);
		}
		else
		{
			return OuterPath.CompareIndexes(Other.OuterPath);
		}
	}

	inline void FCachedAssetKey::AppendString(FStringBuilderBase& Builder) const
	{
		ConcatenateOuterPathAndObjectName(Builder, OuterPath, ObjectName);
	}

	inline FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FCachedAssetKey& Key)
	{
		Key.AppendString(Builder);
		return Builder;
	}

	inline bool operator==(const FCachedAssetKey& A, const FCachedAssetKey& B)
	{
		return A.OuterPath == B.OuterPath && A.ObjectName == B.ObjectName;
	}

	inline bool operator!=(const FCachedAssetKey& A, const FCachedAssetKey& B)
	{
		return A.OuterPath != B.OuterPath || A.ObjectName != B.ObjectName;
	}

	inline uint32 GetTypeHash(const FCachedAssetKey& A)
	{
		return HashCombineQuick(GetTypeHash(A.OuterPath), GetTypeHash(A.ObjectName));
	}
}
