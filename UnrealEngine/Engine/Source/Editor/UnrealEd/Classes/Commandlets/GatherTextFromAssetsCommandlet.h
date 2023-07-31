// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Internationalization/GatherableTextData.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "GatherTextFromAssetsCommandlet.generated.h"

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
	/** Struct containing the data needed by a pending package that we will gather text from */
	struct FPackagePendingGather
	{
		/** The name of the package */
		FName PackageName;

		/** The filename of the package on disk */
		FString PackageFilename;

		/** The complete set of dependencies for the package */
		TSet<FName> Dependencies;

		/** Localization cache state of this package */
		EPackageLocCacheState PackageLocCacheState;

		/** Contains the localization cache data for this package (if cached) */
		TArray<FGatherableTextData> GatherableTextDataArray;
	};

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
	TSet<UObject*> ObjectsToKeepAlive;

	bool bSkipGatherCache;
	bool bReportStaleGatherCache;
	bool bFixStaleGatherCache;
	bool bFixMissingGatherCache;
	bool ShouldGatherFromEditorOnlyData;
	bool ShouldExcludeDerivedClasses;
};
