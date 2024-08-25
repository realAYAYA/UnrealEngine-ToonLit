// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Internationalization/GatherableTextData.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "AssetRegistry/AssetData.h"
#include "GatherTextFromAssetsCommandlet.generated.h"

struct FARFilter;
struct FPackageFileSummary;

/**
 *	UGatherTextFromAssetsCommandlet: Localization commandlet that collects all text to be localized from the game assets.
 */
UCLASS()
class UGatherTextFromAssetsCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()

	void ProcessGatherableTextDataArray(const TArray<FGatherableTextData>& GatherableTextDataArray);
	void CalculateDependenciesForPackagesPendingGather();
	bool HasExceededMemoryLimit(const bool bLog);
	void PurgeGarbage(const bool bPurgeReferencedPackages);
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;

	bool GetConfigurationScript(const TMap<FString, FString>& InCommandLineParameters, FString& OutFilePath, FString& OutStepSectionName);
	bool ConfigureFromScript(const FString& GatherTextConfigPath, const FString& SectionName);

	//~ End UCommandlet Interface
	//~ Begin UGatherTextCommandletBase  Interface
	virtual bool ShouldRunInPreview(const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals) const override;
	//~ End UGatherTextCommandletBase  Interface

	/** Localization cache states of a package */
	enum class EPackageLocCacheState : uint8
	{
		Uncached_TooOld = 0,
		Uncached_NoCache,
		Cached, // Cached must come last as it acts as a count for an array
	};

private:
	/** Parses the command line for the commandlet. Returns true if all required parameters are provided and are correct.*/
	bool ParseCommandLineHelper(const FString& InCommandLine);

// Filtering of asset registry elements
	// Broadly, there is the first pass filter,the exact class filter and the include/exclude path filter that can be applied to filter out asset registry elements.
	// Look at Main() to see how the functions are applied to understand the logic.
	bool PerformFirstPassFilter(TArray<FAssetData>& OutAssetDataArray) const;
	void ApplyFirstPassFilter(const FARFilter& InFilter, TArray<FAssetData>& InOutAssetDataArray) const;
	bool BuildFirstPassFilter(FARFilter& InOutFilter) const;
	bool BuildCollectionFilter(FARFilter& InOutFilter) const;
	bool BuildExcludeDerivedClassesFilter(FARFilter& InOutFilter) const;
	bool PerformExcludeExactClassesFilter(TArray<FAssetData>& InOutAssetDataArray) const;
	bool BuildExcludeExactClassesFilter(FARFilter& InOutFilter) const;
	void ApplyExcludeExactClassesFilter(const FARFilter& InFilter, TArray<FAssetData>& InOutAssetDataArray) const;
	void FilterAssetsBasedOnIncludeExcludePaths(TArray<FAssetData>& InOutAssetDataArray) const;
	
	void DiscoverExternalActors(TArray<FAssetData>& InOutAssetDataArray);
	void RemoveExistingExternalActors(TArray<FAssetData>& InOutAssetDataArray, TArray<FName>& OutPartitionedWorldPackageNames) const;

	TSet<FName> GetPackageNamesToGather(const TArray<FAssetData>& InAssetDataArray) const;
	void PopulatePackagesPendingGather(TSet<FName> PackageNamesToGather);
	void ProcessAndRemoveCachedPackages(TMap<FName, TSet<FGuid>>& OutExternalActorsWithStaleOrMissingCaches);
	void MergeInExternalActorsWithStaleOrMissingCaches(TMap<FName, TSet<FGuid>>& ExternalActorsWithStaleOrMissingCaches);

	void LoadAndProcessUncachedPackages(TArray<FName>& OutPackagesWithStaleGatherCache);

	void ReportStaleGatherCache(TArray<FName>& InPackagesWithStaleGatherCache) const;
	/** Determines the loc cache state for a package. This determines whether the package should be fully loaded for gathering.*/
	EPackageLocCacheState CalculatePackageLocCacheState(const FPackageFileSummary& PackageFileSummary, const FName PackageName, bool bIsExternalActorPackage) const;
	/** Struct containing the data needed by a pending package that we will gather text from */
	struct FPackagePendingGather
	{
		/** The name of the package */
		FName PackageName;

		/** The filename of the package on disk */
		FString PackageFilename;

		/** The complete set of dependencies for the package */
		TSet<FName> Dependencies;

		/** The set of external actors to process for a world partition map package */
		TSet<FGuid> ExternalActors;

		/** Localization cache state of this package */
		EPackageLocCacheState PackageLocCacheState;

		/** Contains the localization cache data for this package (if cached) */
		TArray<FGatherableTextData> GatherableTextDataArray;
	};

	/** Adds a package to PackagesPendingGather and returns a pointer to the appended package.*/
	FPackagePendingGather* AppendPackagePendingGather(const FName PackageNameToGather);

	static const FString UsageText;

	TArray<FString> ModulesToPreload;
	TArray<FString> IncludePathFilters;
	TArray<FString> CollectionFilters;
	TArray<FString> ExcludePathFilters;
	TArray<FString> PackageFileNameFilters;
	TArray<FString> ExcludeClassNames;
	TArray<FString> ManifestDependenciesList;

	TArray<FPackagePendingGather> PackagesPendingGather;

	/** Run a GC if the free system memory is less than this value (or zero to disable) */
	uint64 MinFreeMemoryBytes;

	/** Run a GC if the used process memory is greater than this value (or zero to disable) */
	uint64 MaxUsedMemoryBytes;

	/** Array of objects that should be kept alive during the next call to CollectGarbage (used by PurgeGarbage and AddReferencedObjects) */
	TSet<TObjectPtr<UObject>> ObjectsToKeepAlive;
	/** Path to the directory where output reports etc will be saved.*/
	FString DestinationPath;
	bool bSkipGatherCache;
	bool bReportStaleGatherCache;
	bool bFixStaleGatherCache;
	bool bFixMissingGatherCache;
	bool bShouldGatherFromEditorOnlyData;
	bool bShouldExcludeDerivedClasses;
};
