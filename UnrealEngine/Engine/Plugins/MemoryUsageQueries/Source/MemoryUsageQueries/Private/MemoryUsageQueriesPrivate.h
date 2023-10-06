// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/FastReferenceCollector.h"

class IMemoryUsageInfoProvider;

namespace MemoryUsageQueries::Internal
{

class FMemoryUsageReferenceProcessor : public FSimpleReferenceProcessorBase
{
public:
	enum EMode
	{
		Full,
		Excluding
	};

private:
	TBitArray<> Excluded;

	TBitArray<> ReachableFull;
	TBitArray<> ReachableExcluded;

	TArray<UObject*> RootSetPackages;

	EMode Mode;

public:
	FMemoryUsageReferenceProcessor();

	bool Init(const TArray<FString>& PackageNames, FOutputDevice* ErrorOutput = GLog);
	TArray<UObject*>& GetRootSet();
	void HandleTokenStreamObjectReference(UE::GC::FWorkerContext& Context, const UObject* ReferencingObject, UObject*& Object, UE::GC::FTokenId TokenIndex, const EGCTokenType TokenType, bool bAllowReferenceElimination);
	bool GetUnreachablePackages(TSet<FName>& OutUnreachablePackages);

	void SetMode(EMode InMode) { Mode = InMode; }
};

typedef UE::GC::TDefaultCollector<FMemoryUsageReferenceProcessor> FMemoryUsageReferenceCollector;

bool GetLongName(FStringView ShortPackageName, FName& OutLongPackageName, FOutputDevice* ErrorOutput = GLog);
bool GetLongNames(const TArray<FString>& PackageNames, TSet<FName>& OutLongPackageNames, FOutputDevice* ErrorOutput = GLog);
bool GetLongNameAndDependencies(FStringView PackageName, FName& OutLongPackageName, TSet<FName>& OutDependencies, FOutputDevice* ErrorOutput = GLog);

bool GetDependenciesCombined(const TArray<FString>& PackageNames, TSet<FName>& OutDependencies, FOutputDevice* ErrorOutput = GLog);
bool GetDependenciesShared(const TArray<FString>& PackageNames, TSet<FName>& OutDependencies, FOutputDevice* ErrorOutput = GLog);

bool PerformReachabilityAnalysis(FMemoryUsageReferenceProcessor& ReferenceProcessor, FOutputDevice* ErrorOutput = GLog);

// Get Packages that would be GC'd if PackagesToUnload were unloaded
bool GetRemovablePackages(const TArray<FString>& PackagesToUnload, TSet<FName>& OutRemovablePackages, FOutputDevice* ErrorOutput = GLog);

// Get Packages that would not be GC'd if PackagesToUnload were unloaded
bool GetUnremovablePackages(const TArray<FString>& PackagesToUnload, TSet<FName>& OutUnremovablePackages, FOutputDevice* ErrorOutput = GLog);

void GetTransitiveDependencies(FName PackageName, TSet<FName>& OutDependencies);

void SortPackagesBySize(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TSet<FName>& Packages, TMap<FName, uint64>& OutPackagesWithSize, FOutputDevice* ErrorOutput = GLog);
void GetPackagesSize(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TSet<FName>& Packages, TMap<FName, uint64>& OutPackagesWithSize, FOutputDevice* ErrorOutput = GLog);

void RemoveNonExistentPackages(TMap<FName, uint64>& OutPackagesWithSize);
void RemoveFilteredPackages(TMap<FName, uint64>& OutPackagesWithSize, const FString& AssetSubstring);

void PrintTagsWithSize(FOutputDevice& Ar, const TMap<FName, uint64>& TagsWithSize, const TCHAR* Name, bool bTruncate = false, int32 Limit = -1, bool bCSV = false);

}
