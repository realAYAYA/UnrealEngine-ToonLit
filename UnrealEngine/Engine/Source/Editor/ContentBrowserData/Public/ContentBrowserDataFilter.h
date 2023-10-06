// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CollectionManagerTypes.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/SortedMap.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/OptionalFwd.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StructOnScope.h"

#include "ContentBrowserDataFilter.generated.h"

class FNamePermissionList;
class FPathPermissionList;
class UContentBrowserDataSource;
class UContentBrowserDataSubsystem;

/** Flags controlling which item types should be included */
UENUM(Flags)
enum class EContentBrowserItemTypeFilter : uint8
{
	IncludeNone = 0,
	IncludeFolders = 1<<0,
	IncludeFiles = 1<<1,
	IncludeAll = IncludeFolders | IncludeFiles,
};
ENUM_CLASS_FLAGS(EContentBrowserItemTypeFilter);

/** Flags controlling which item categories should be included */
UENUM(Flags)
enum class EContentBrowserItemCategoryFilter : uint8
{
	IncludeNone = 0,
	IncludeAssets = 1<<0,
	IncludeClasses = 1<<1,
	IncludeCollections = 1<<2,
	IncludeMisc = 1<<3,
	IncludeAll = IncludeAssets | IncludeClasses | IncludeCollections | IncludeMisc,
};
ENUM_CLASS_FLAGS(EContentBrowserItemCategoryFilter);

/** Flags controlling which item attributes should be included */
UENUM(Flags)
enum class EContentBrowserItemAttributeFilter : uint8
{
	IncludeNone = 0,
	IncludeProject = 1<<0,
	IncludeEngine = 1<<1,
	IncludePlugins = 1<<2,
	IncludeDeveloper = 1<<3,
	IncludeLocalized = 1<<4,
	IncludeAll = IncludeProject | IncludeEngine | IncludePlugins | IncludeDeveloper | IncludeLocalized,
};
ENUM_CLASS_FLAGS(EContentBrowserItemAttributeFilter);

/**
 * A list of typed filter structs and their associated data.
 * This allows systems to add new filter types that the core Content Browser data module doesn't know about.
 */
struct CONTENTBROWSERDATA_API FContentBrowserDataFilterList
{
public:
	/** Constructor */
	FContentBrowserDataFilterList() = default;

	/** Copy support */
	FContentBrowserDataFilterList(const FContentBrowserDataFilterList& InOther);
	FContentBrowserDataFilterList& operator=(const FContentBrowserDataFilterList& InOther);

	/** Move support */
	FContentBrowserDataFilterList(FContentBrowserDataFilterList&&) = default;
	FContentBrowserDataFilterList& operator=(FContentBrowserDataFilterList&&) = default;

	/** Find the filter associated with the given type, or add a default instance if it doesn't exist in the list */
	template <typename T>
	T& FindOrAddFilter()
	{
		return *static_cast<T*>(FindOrAddFilter(TBaseStructure<T>::Get()));
	}
	void* FindOrAddFilter(const UScriptStruct* InFilterType);

	/** Set the filter associated with the given type, replacing any instance of this type that may exist in the list */
	template <typename T>
	void SetFilter(const T& InFilter)
	{
		SetFilter(TBaseStructure<T>::Get(), &InFilter);
	}
	void SetFilter(const UScriptStruct* InFilterType, const void* InFilterData);

	/** Find the filter associated with the given type, if it exists in the list */
	template <typename T>
	const T* FindFilter() const
	{
		return static_cast<const T*>(FindFilter(TBaseStructure<T>::Get()));
	}
	const void* FindFilter(const UScriptStruct* InFilterType) const;

	/** Find the filter associated with the given type, if it exists in the list */
	template <typename T>
	T* FindMutableFilter()
	{
		return const_cast<T*>(FindMutableFilter<T>());
	}

	/** Get the filter associated with the given type, asserting if it doesn't exist in the list */
	template <typename T>
	const T& GetFilter() const
	{
		const T* FilterData = FindFilter<T>();
		check(FilterData);
		return *FilterData;
	}

	/** Get the filter associated with the given type, asserting if it doesn't exist in the list */
	template <typename T>
	T& GetMutableFilter()
	{
		return const_cast<T&>(GetFilter<T>());
	}

	/** Remove the filter associated with the given type */
	template <typename T>
	void RemoveFilter()
	{
		RemoveFilter(TBaseStructure<T>::Get());
	}
	void RemoveFilter(const UScriptStruct* InFilterType);

	/** Remove all filters in the list */
	void ClearFilters();
	
	TArray<const UScriptStruct*> GetFilterTypes() const;

private:
	/** Set the contents of this list to be a deep copy of the contents of the other list */
	void SetTo(const FContentBrowserDataFilterList& InOther);

	/** Array of typed filter structs */
	TArray<FStructOnScope> TypedFilters;
};

struct CONTENTBROWSERDATA_API FContentBrowserDataFilterCacheID
{
public:
	operator bool() const
	{
		return IsSet();
	};

	bool IsSet() const
	{
		return ID != INDEX_NONE;
	}

private:
	friend struct FContentBrowserDataFilterCacheIDOwner;

	int64 ID = INDEX_NONE;

	friend uint32 GetTypeHash(const FContentBrowserDataFilterCacheID& CacheID)
	{
		return GetTypeHash(CacheID.ID);
	}
};

/**
 * A filter used to control what is returned from Content Browser data queries.
 * @note The compiled version of this, FContentBrowserDataCompiledFilter, is produced via UContentBrowserDataSubsystem::CompileFilter.
 */
USTRUCT(BlueprintType)
struct CONTENTBROWSERDATA_API FContentBrowserDataFilter
{
	GENERATED_BODY()

public:
	/** Whether we should include sub-paths in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	bool bRecursivePaths = false;

	/** Flags controlling which item types should be included in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	EContentBrowserItemTypeFilter ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeAll;

	/** Flags controlling which item categories should be included in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	EContentBrowserItemCategoryFilter ItemCategoryFilter = EContentBrowserItemCategoryFilter::IncludeAll;

	/** Flags controlling which item attributes should be included in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	EContentBrowserItemAttributeFilter ItemAttributeFilter = EContentBrowserItemAttributeFilter::IncludeAll;

	/** A list of extra filter structs to be interpreted by the Content Browser data sources */
	FContentBrowserDataFilterList ExtraFilters;

	/** An optional id used by the data sources to cache and reuse some data when compiling the filter(s) */
	FContentBrowserDataFilterCacheID CacheID;
};

/**
 * A filter used to control what is returned from Content Browser data queries.
 * @note The source version of this, FContentBrowserDataFilter, is used with UContentBrowserDataSubsystem::CompileFilter to produce a compiled filter.
 */
struct CONTENTBROWSERDATA_API FContentBrowserDataCompiledFilter
{
	/** Flags controlling which item types should be included in this query */
	EContentBrowserItemTypeFilter ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeAll;

	/** Flags controlling which item categories should be included in this query */
	EContentBrowserItemCategoryFilter ItemCategoryFilter = EContentBrowserItemCategoryFilter::IncludeAll;

	/** Flags controlling which item attributes should be included in this query */
	EContentBrowserItemAttributeFilter ItemAttributeFilter = EContentBrowserItemAttributeFilter::IncludeAll;

	/** Per data-source compiled filter structs - typically optimized for both search queries and per-item queries */
	TSortedMap<const UContentBrowserDataSource*, FContentBrowserDataFilterList> CompiledFilters;
};

/**
 * Data used to filter object instances by their name and tags.
 * @note This will typically limit your query to returning assets.
 */
USTRUCT(BlueprintType)
struct CONTENTBROWSERDATA_API FContentBrowserDataObjectFilter
{
	GENERATED_BODY()

public:
	/** Array of object names that should be included in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	TArray<FName> ObjectNamesToInclude;

	/** Array of object names that should be excluded from this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	TArray<FName> ObjectNamesToExclude;

	/** Whether we should only include on-disk objects (ignoring those that exist only in memory) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	bool bOnDiskObjectsOnly = false;

	/** Map of object tags (with optional values) that should be included in this query */
	TMultiMap<FName, TOptional<FString>> TagsAndValuesToInclude;

	/** Map of object tags (with optional values) that should be excluded from this query */
	TMultiMap<FName, TOptional<FString>> TagsAndValuesToExclude;
};

/**
 * Data used to filter object instances by their package.
 * @note This will typically limit your query to returning assets.
 */
USTRUCT(BlueprintType)
struct CONTENTBROWSERDATA_API FContentBrowserDataPackageFilter
{
	GENERATED_BODY()

public:
	/** Array of package names that should be included in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	TArray<FName> PackageNamesToInclude;

	/** Array of package names that should be excluded from this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	TArray<FName> PackageNamesToExclude;

	/** Array of package paths that should be included in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	TArray<FName> PackagePathsToInclude;

	/** Array of package paths that should be excluded from this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	TArray<FName> PackagePathsToExclude;

	/** Whether we should include inclusive package sub-paths in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	bool bRecursivePackagePathsToInclude = false;

	/** Whether we should include exclusive package sub-paths in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	bool bRecursivePackagePathsToExclude = false;

	/** Optional set of additional path filtering */
	TSharedPtr<FPathPermissionList> PathPermissionList;
};

/**
 * Data used to filter object instances by their class.
 * @note This will typically limit your query to returning assets.
 */
USTRUCT(BlueprintType)
struct CONTENTBROWSERDATA_API FContentBrowserDataClassFilter
{
	GENERATED_BODY()

public:
	/** Array of class names that should be included in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	TArray<FString> ClassNamesToInclude;

	/** Array of class names that should be excluded from this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	TArray<FString> ClassNamesToExclude;

	/** Whether we should include inclusive sub-classes in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	bool bRecursiveClassNamesToInclude = false;

	/** Whether we should include exclusive sub-classes in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	bool bRecursiveClassNamesToExclude = false;

	/** Optional set of additional class filtering */
	TSharedPtr<FPathPermissionList> ClassPermissionList;
};

/**
 * Data used to filter items by their collection.
 * @note This will typically limit your query to items that support being inside a collection.
 */
USTRUCT(BlueprintType)
struct CONTENTBROWSERDATA_API FContentBrowserDataCollectionFilter
{
	GENERATED_BODY()

public:
	/** Array of collections to include in this query */
	TArray<FCollectionNameType> SelectedCollections;

	/** Whether we should include child collections in this query */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ContentBrowser")
	bool bIncludeChildCollections = false;
};


/**
 * Data used to tell the content browser to show the item that doesn't pass the class permission list as a unsupported asset
 * @note This will restrict user interaction with those asset in the content browser and only affect the asset in the folders specified in the permission list
 */
USTRUCT()
struct CONTENTBROWSERDATA_API FContentBrowserDataUnsupportedClassFilter
{
	GENERATED_BODY()

public:
	TSharedPtr<FPathPermissionList> ClassPermissionList;

	TSharedPtr<FPathPermissionList> FolderPermissionList;
};

/**
 * ID used by the data sources to cache some data between the filter compilations
 * How use the filter compilation cache.
 * 1) Initialize the id by using the UContentBrowserDataSubsystem once.
 * 2) When compiling the filters pass the Cache ID Owner to the cacheID of the ContentBrowserDataFilter.
 * 3) When the filter settings change call RemoveUnusedCachedData to clean the cache and to remove the potential invalid data.
 */
struct CONTENTBROWSERDATA_API FContentBrowserDataFilterCacheIDOwner
{
public:
	operator FContentBrowserDataFilterCacheID() const
	{
		FContentBrowserDataFilterCacheID CacheID;
		CacheID.ID = ID;
		return CacheID;
	}

	FContentBrowserDataFilterCacheIDOwner() = default;

	FContentBrowserDataFilterCacheIDOwner(FContentBrowserDataFilterCacheIDOwner&& Other)
		: ID(Other.ID)
		, DataSource(MoveTemp(Other.DataSource))
	{
		Other.ID = INDEX_NONE;
		Other.DataSource.Reset();
	}

	FContentBrowserDataFilterCacheIDOwner& operator=(FContentBrowserDataFilterCacheIDOwner&& Other)
	{
		ID = Other.ID;
		Other.ID = INDEX_NONE;
		Other.DataSource = MoveTemp(Other.DataSource);
		Other.DataSource.Reset();

		return *this;
	}

	~FContentBrowserDataFilterCacheIDOwner()
	{
		ClearCachedData();
	}

	FContentBrowserDataFilterCacheIDOwner(const FContentBrowserDataFilterCacheIDOwner&) = delete;
	FContentBrowserDataFilterCacheIDOwner& operator=(const FContentBrowserDataFilterCacheIDOwner&) = delete;

	void Initialaze(UContentBrowserDataSubsystem* InContentBrowserDataSubsystem);

	void RemoveUnusedCachedData(TArrayView<const FName> InVirtualPathsInUse, const FContentBrowserDataFilter& DataFilter) const;

	void ClearCachedData() const;

	void Reset();

private:
	friend UContentBrowserDataSubsystem;

	int64 ID = INDEX_NONE;
	TWeakObjectPtr<UContentBrowserDataSubsystem> DataSource;
};
