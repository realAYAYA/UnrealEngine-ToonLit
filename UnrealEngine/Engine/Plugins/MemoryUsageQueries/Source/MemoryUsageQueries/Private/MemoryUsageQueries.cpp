// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemoryUsageQueries.h"

#include "ConsoleSettings.h"
#include "IPlatformFilePak.h"
#include "MemoryUsageQueriesConfig.h"
#include "MemoryUsageQueriesPrivate.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/FileManager.h"
#include "IO/PackageStore.h"
#include "Misc/OutputDeviceArchiveWrapper.h"
#include "Misc/PackageName.h"
#include "Misc/WildcardString.h"
#include "Modules/ModuleManager.h"
#include "Templates/Greater.h"
#include "UObject/UObjectAllocator.h"
#include "UObject/UObjectIterator.h"

namespace MemoryUsageQueries
{

FMemoryUsageInfoProviderLLM MemoryUsageInfoProviderLLM;
IMemoryUsageInfoProvider* CurrentMemoryUsageInfoProvider = &MemoryUsageInfoProviderLLM;

} // namespace MemoryUsageQueries

struct FAssetMemoryBreakdown
{
	FAssetMemoryBreakdown()
	{
	}

	uint64 ExclusiveSize = 0;
	uint64 UniqueSize = 0;
	uint64 SharedSize = 0;
	uint64 TotalSize = 0;
};

struct FAssetMemoryDetails
{
	FAssetMemoryDetails()
	{
	}

	// Assets Package Name
	FName PackageName;

	FAssetMemoryBreakdown MemoryBreakdown;

	// List of dependencies for this asset
	TSet<FName> Dependencies;

	TMap<FName, FAssetMemoryBreakdown> DependenciesToMemoryMap;

	int32 UniqueRefCount = 0;
	int32 SharedRefCount = 0;
};

static int32 DefaultResultLimit = 15;

// Helper struct that scopes an output device, and will automatically clean up any log files when leaving scope
struct FScopedOutputDevice
{
	FOutputDevice* CurrentOutputDevice = nullptr;

	FOutputDeviceArchiveWrapper* FileArWrapper = nullptr;
	FArchive* FileAr = nullptr;

	explicit FScopedOutputDevice(FOutputDevice* InDefaultOutputDevice) :
		CurrentOutputDevice(InDefaultOutputDevice)
		
	{
	}

	~FScopedOutputDevice()
	{
		if (FileArWrapper != nullptr)
		{
			FileArWrapper->TearDown();
		}

		delete FileArWrapper;
		delete FileAr;
	}

	FOutputDevice& GetOutputDevice() const
	{
		return *CurrentOutputDevice;
	}

	void OpenLogFile(const FString& LogFileName, bool bCSV)
	{
		if (FileArWrapper != nullptr || LogFileName.IsEmpty())
		{
			// Log file is already open or LogFileName is empty
			return;
		}

#if ALLOW_DEBUG_FILES
		// Create folder for MemQuery files.
		const FString OutputDir = FPaths::ProfilingDir() / TEXT("MemQuery");
		IFileManager::Get().MakeDirectory(*OutputDir, true);

		FString FileTimeString = FString::Printf(TEXT("_%s"), *FDateTime::Now().ToString(TEXT("%H%M%S")));
		const FString FileExtension = (bCSV ? TEXT(".csv") : TEXT(".memquery"));
		const FString LogFilename = OutputDir / (LogFileName + FileTimeString + FileExtension);

		FileAr = IFileManager::Get().CreateDebugFileWriter(*LogFilename);

		if (FileAr != nullptr)
		{
			FileArWrapper = new FOutputDeviceArchiveWrapper(FileAr);
				
			if (FileArWrapper != nullptr)
			{
				CurrentOutputDevice = FileArWrapper;
			}
			else
			{
				delete FileAr;
				FileAr = nullptr;
			}
		}
#endif
	}

	// Delete default constructor to force proper usage
	FScopedOutputDevice() = delete;
};

/** Structure that will parse a string and fill out some commonly used parameters per command */
struct FCommonParameters
{
	int32 Limit = -1;

	bool bTruncate = true;
	bool bCSV = false;
	
	FName Group = NAME_None;
	FName Class = NAME_None;

	FString Name;
	FString Names;
	FString AssetName;
	FString LogFileName;

	explicit FCommonParameters(const TCHAR* Args)
	{
		bTruncate = !FParse::Param(Args, TEXT("notrunc"));
		bCSV = FParse::Param(Args, TEXT("csv"));

		FParse::Value(Args, TEXT("Name="), Name);
		FParse::Value(Args, TEXT("Names="), Names);
		FParse::Value(Args, TEXT("Limit="), Limit);
		FParse::Value(Args, TEXT("Log="), LogFileName);
		FParse::Value(Args, TEXT("Asset="), AssetName);

		FString GroupName;
		if (FParse::Value(Args, TEXT("Group="), GroupName))
		{
			Group = FName(*GroupName);
		}

		FString ClassName;
		if (FParse::Value(Args, TEXT("Class="), ClassName))
		{
			Class = FName(*ClassName);
		}
	}

	FCommonParameters() = delete;
};

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryUsage(
	TEXT("MemQuery.Usage"),
	TEXT("Name=<AssetName> Prints memory usage of the specified asset."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Name.IsEmpty())
	{
		uint64 ExclusiveSize = 0U;
		uint64 InclusiveSize = 0U;

		if (MemoryUsageQueries::GetMemoryUsage(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, CommonArgs.Name, ExclusiveSize, InclusiveSize, &Ar))
		{
			ScopedOutputDevice.GetOutputDevice().Logf(TEXT("MemoryUsage: ExclusiveSize: %.2f MiB (%.2f KiB); InclusiveSize: %.2f MiB (%.2f KiB)"),
				ExclusiveSize / (1024.f * 1024.f),
				ExclusiveSize / 1024.f,
				InclusiveSize / (1024.f * 1024.f),
				InclusiveSize / 1024.f);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryCombinedUsage(
	TEXT("MemQuery.CombinedUsage"),
	TEXT("Names=\"<AssetName1> <AssetName2> ...\" Prints combined memory usage of the specified assets (including all dependencies)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Names.IsEmpty())
	{
		TArray<FString> Packages;
		CommonArgs.Names.ParseIntoArrayWS(Packages);
		uint64 TotalSize;

		if (MemoryUsageQueries::GetMemoryUsageCombined(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, Packages, TotalSize, &Ar))
		{
			ScopedOutputDevice.GetOutputDevice().Logf(TEXT("MemoryUsageCombined: TotalSize: %.2f MiB (%.2f KiB)"), TotalSize / (1024.f * 1024.f), TotalSize / 1024.f);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQuerySharedUsage(
	TEXT("MemQuery.SharedUsage"),
	TEXT("Names=\"<AssetName1> <AssetName2> ...\" Prints shared memory usage of the specified assets (including only dependencies shared by the specified assets)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Names.IsEmpty())
	{
		TArray<FString> Packages;
		CommonArgs.Names.ParseIntoArrayWS(Packages);
		uint64 SharedSize;

		if (MemoryUsageQueries::GetMemoryUsageShared(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, Packages, SharedSize, &Ar))
		{
			ScopedOutputDevice.GetOutputDevice().Logf(TEXT("MemoryUsageShared: SharedSize: %.2f MiB (%.2f KiB)"), SharedSize / (1024.f * 1024.f), SharedSize / 1024.f);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryUniqueUsage(
	TEXT("MemQuery.UniqueUsage"),
	TEXT("Names=\"<AssetName1> <AssetName2> ...\" Prints unique memory usage of the specified assets (including only dependencies unique to the specified assets)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Names.IsEmpty())
	{
		TArray<FString> Packages;
		CommonArgs.Names.ParseIntoArrayWS(Packages);
		uint64 UniqueSize = 0U;

		if (MemoryUsageQueries::GetMemoryUsageUnique(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, Packages, UniqueSize, &Ar))
		{
			ScopedOutputDevice.GetOutputDevice().Logf(TEXT("MemoryUsageUnique: UniqueSize: %.2f MiB (%.2f KiB)"), UniqueSize / (1024.f * 1024.f), UniqueSize / 1024.f);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryCommonUsage(
	TEXT("MemQuery.CommonUsage"),
	TEXT("Names=\"<AssetName1> <AssetName2> ...\" Prints common memory usage of the specified assets (including only dependencies that are not unique to the specified assets)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Names.IsEmpty())
	{
		TArray<FString> Packages;
		CommonArgs.Names.ParseIntoArrayWS(Packages);
		uint64 CommonSize = 0U;

		if (MemoryUsageQueries::GetMemoryUsageCommon(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, Packages, CommonSize, &Ar))
		{
			ScopedOutputDevice.GetOutputDevice().Logf(TEXT("MemoryUsageCommon: CommonSize: %.2f MiB (%.2f KiB)"), CommonSize / (1024.f * 1024.f), CommonSize / 1024.f);
		}

	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryDependencies(
	TEXT("MemQuery.Dependencies"),
	TEXT("Name=<AssetName> Limit=<n> Lists dependencies of the specified asset, sorted by size."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Name.IsEmpty())
	{
		TMap<FName, uint64> DependenciesWithSize;

		if (MemoryUsageQueries::GetDependenciesWithSize(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, CommonArgs.Name, DependenciesWithSize, &Ar))
		{
			MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), DependenciesWithSize, TEXT("Dependencies"), CommonArgs.bTruncate, CommonArgs.Limit, CommonArgs.bCSV);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryCombinedDependencies(
	TEXT("MemQuery.CombinedDependencies"),
	TEXT("Names=\"<AssetName1> <AssetName2> ...\" Limit=<n> Lists n largest dependencies of the specified assets, sorted by size."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Names.IsEmpty())
	{
		TArray<FString> Packages;
		CommonArgs.Names.ParseIntoArrayWS(Packages);
		TMap<FName, uint64> CombinedDependenciesWithSize;

		if (MemoryUsageQueries::GetDependenciesWithSizeCombined(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, Packages, CombinedDependenciesWithSize, &Ar))
		{
			MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), CombinedDependenciesWithSize, TEXT("Combined Dependencies"), CommonArgs.bTruncate, CommonArgs.Limit, CommonArgs.bCSV);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQuerySharedDependencies(
	TEXT("MemQuery.SharedDependencies"),
	TEXT("Names=\"<AssetName1> <AssetName2> ...\" Limit=<n> Lists n largest dependencies that are shared by the specified assets, sorted by size."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Names.IsEmpty())
	{
		TArray<FString> Packages;
		CommonArgs.Names.ParseIntoArrayWS(Packages);
		TMap<FName, uint64> SharedDependenciesWithSize;

		if (MemoryUsageQueries::GetDependenciesWithSizeShared(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, Packages, SharedDependenciesWithSize, &Ar))
		{
			MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), SharedDependenciesWithSize, TEXT("Shared Dependencies"), CommonArgs.bTruncate, CommonArgs.Limit, CommonArgs.bCSV);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryUniqueDependencies(
	TEXT("MemQuery.UniqueDependencies"),
	TEXT("Names=\"<AssetName1> <AssetName2> ...\" Limit=<n> Lists n largest dependencies that are unique to the specified assets, sorted by size."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Names.IsEmpty())
	{
		TArray<FString> Packages;
		CommonArgs.Names.ParseIntoArrayWS(Packages);
		TMap<FName, uint64> UniqueDependenciesWithSize;

		if (MemoryUsageQueries::GetDependenciesWithSizeUnique(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, Packages, UniqueDependenciesWithSize, &Ar))
		{
			MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), UniqueDependenciesWithSize, TEXT("Unique Dependencies"), CommonArgs.bTruncate, CommonArgs.Limit, CommonArgs.bCSV);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryCommonDependencies(
	TEXT("MemQuery.CommonDependencies"),
	TEXT("Names=\"<AssetName1> <AssetName2> ...\" Limit=<n> Lists n largest dependencies that are NOT unique to the specified assets, sorted by size."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	if (!CommonArgs.Names.IsEmpty())
	{
		TArray<FString> Packages;
		CommonArgs.Names.ParseIntoArrayWS(Packages);
		TMap<FName, uint64> CommonDependenciesWithSize;

		if (MemoryUsageQueries::GetDependenciesWithSizeCommon(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, Packages, CommonDependenciesWithSize, &Ar))
		{
			MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), CommonDependenciesWithSize, TEXT("Common Dependencies"), CommonArgs.bTruncate, CommonArgs.Limit, CommonArgs.bCSV);
		}
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQuerySavings(
	TEXT("MemQuery.Savings"),
	TEXT("Limit=<n> Lists potential savings among %s. How much memory can be saved it we delete certain object."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	const UMemoryUsageQueriesConfig* Config = GetDefault<UMemoryUsageQueriesConfig>();

	// Necessary for FParse::Command
	const TCHAR* Command = *Cmd;

	for (auto It = Config->SavingsPresets.CreateConstIterator(); It; ++It)
	{
		if (!FParse::Command(&Command, *It.Key()))
		{
			continue;
		}

		TMap<FName, uint64> PresetSavings;
		TSet<FName> Packages;

		UClass* SavingsClass = FindObject<UClass>(nullptr, *It.Value());
		if (SavingsClass != nullptr)
		{
			TArray<UClass*> DerivedClasses;
			TArray<UClass*> DerivedResults;

			GetDerivedClasses(SavingsClass, DerivedClasses, true);

			for (UClass* DerivedClass : DerivedClasses)
			{
				UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(DerivedClass);
				if (BPClass != nullptr)
				{
					DerivedResults.Reset();
					GetDerivedClasses(BPClass, DerivedResults, false);
					if (DerivedResults.Num() == 0)
					{
						Packages.Add(DerivedClass->GetPackage()->GetFName());
					}
				}
			}
		}

		for (const auto& Package : Packages)
		{
			uint64 Size = 0;
			MemoryUsageQueries::GetMemoryUsageUnique(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, TArray<FString>({ Package.ToString() }), Size, &Ar);
			PresetSavings.Add(Package, Size);
		}

		PresetSavings.ValueSort(TGreater<uint64>());
		MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), PresetSavings, *FString::Printf(TEXT("possible savings")), CommonArgs.bTruncate, CommonArgs.bCSV);
	}
}));

#if ENABLE_LOW_LEVEL_MEM_TRACKER

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryListAssets(
	TEXT("MemQuery.ListAssets"),
	TEXT("Name=<AssetNameSubstring> Group=<GroupName> Class=<ClassName> Limit=<n> Lists n largest assets."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	bool bSuccess;
	TMap<FName, uint64> AssetsWithSize;

	if (CommonArgs.Group != NAME_None || CommonArgs.Class != NAME_None)
	{
		bSuccess = MemoryUsageQueries::GetFilteredPackagesWithSize(AssetsWithSize, CommonArgs.Group, CommonArgs.AssetName, CommonArgs.Class, &ScopedOutputDevice.GetOutputDevice());
	}
	else
	{
		// TODO - Implement using faster path if there are no group / class filters
		bSuccess = MemoryUsageQueries::GetFilteredPackagesWithSize(AssetsWithSize, CommonArgs.Group, CommonArgs.AssetName, CommonArgs.Class, &ScopedOutputDevice.GetOutputDevice());
	}

	if (bSuccess)
	{
		MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), AssetsWithSize, TEXT("largest assets"), CommonArgs.bTruncate, CommonArgs.Limit, CommonArgs.bCSV);
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryListClasses(
	TEXT("MemQuery.ListClasses"),
	TEXT("Group=<GroupName> Asset=<AssetName> Limit=<n> Lists n largest classes."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	TMap<FName, uint64> ClassesWithSize;
	if (MemoryUsageQueries::GetFilteredClassesWithSize(ClassesWithSize, CommonArgs.Group, CommonArgs.AssetName, &Ar))
	{
		MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), ClassesWithSize, *FString::Printf(TEXT("Largest Classes")), CommonArgs.bTruncate, CommonArgs.Limit, CommonArgs.bCSV);
	}
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryListGroups(
	TEXT("MemQuery.ListGroups"),
	TEXT("Asset=<AssetName> Class=<ClassName> Limit=<n> Lists n largest groups."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	TMap<FName, uint64> GroupsWithSize;
	if (MemoryUsageQueries::GetFilteredGroupsWithSize(GroupsWithSize, CommonArgs.AssetName, CommonArgs.Class, &Ar))
	{
		MemoryUsageQueries::Internal::PrintTagsWithSize(ScopedOutputDevice.GetOutputDevice(), GroupsWithSize, *FString::Printf(TEXT("Largest Groups")), CommonArgs.bTruncate, CommonArgs.Limit, CommonArgs.bCSV);
	}
}));

// Will return true if the Package Name matches any of the conditions in the array of Paths
static bool PackageNameMatches(const FString& PackageName, const TArray<FString>& Conditions)
{
	for (const FString& Condition : Conditions)
	{
		if ((FWildcardString::ContainsWildcards(*Condition) && FWildcardString::IsMatch(*Condition, *PackageName)) || (PackageName.Contains(Condition)))
		{
			return true;
		}
	}

	return false;
}

FAutoConsoleCommandWithWorldArgsAndOutputDevice GMemQueryCollections(
	TEXT("MemQuery.Collection"),
	TEXT("Lists memory used by a collection. Can show dependency breakdown. Pass -showdeps to list dependencies."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const FString Cmd = FString::Join(Args, TEXT(" "));

	FScopedOutputDevice ScopedOutputDevice(&Ar);
	FCommonParameters CommonArgs(*Cmd);

	ScopedOutputDevice.OpenLogFile(CommonArgs.LogFileName, CommonArgs.bCSV);

	const bool bShowDependencies = FParse::Param(*Cmd, TEXT("ShowDeps"));

	// Necessary for FParse::Command
	const TCHAR* Command = *Cmd;

	FOutputDevice* CurrentOutputDevice = &ScopedOutputDevice.GetOutputDevice();

	const UMemoryUsageQueriesConfig* Config = GetDefault<UMemoryUsageQueriesConfig>();
	for (auto It = Config->Collections.CreateConstIterator(); It; ++It)
	{
		const FCollectionInfo& CollectionInfo = *It;

		if (!FParse::Command(&Command, *CollectionInfo.Name))
		{
			continue;
		}

		// Retrieve a list of all assets that have allocations we are currently tracking.
		TMap<FName, uint64> AssetsWithSize;
		bool bSuccess = MemoryUsageQueries::GetFilteredPackagesWithSize(AssetsWithSize, NAME_None, "", NAME_None, &Ar);

		if (!bSuccess)
		{
			Ar.Logf(TEXT("Failed to gather assets for Collection %s"), *CollectionInfo.Name);
			break;
		}

		// See if any of the asset paths match those of our matching paths and are valid
		TArray<FString> PackageNames;

		TMap<FName, FAssetMemoryDetails> AssetMemoryMap;
		for (const TPair<FName, uint64>& AssetSizePair : AssetsWithSize)
		{
			const FString& PackageName = AssetSizePair.Key.ToString();

			if (!FPackageName::IsValidLongPackageName(PackageName))
			{
				continue;
			}

			// If path is Included and NOT Excluded, it's a valid asset to consider
			if (PackageNameMatches(PackageName, CollectionInfo.Includes) && !PackageNameMatches(PackageName, CollectionInfo.Excludes))
			{
				PackageNames.Add(PackageName);
				FAssetMemoryDetails& AssetMemory = AssetMemoryMap.Add(AssetSizePair.Key);
				AssetMemory.MemoryBreakdown.ExclusiveSize = AssetSizePair.Value;

				FName LongPackageName;
				if (!MemoryUsageQueries::Internal::GetLongNameAndDependencies(PackageName, LongPackageName, AssetMemory.Dependencies, &Ar))
				{
					Ar.Logf(TEXT("Failed to get dependencies foro Asset %s"), *PackageName);
				}
			}
		}

		// Gather list of dependencies. Internal dependencies are confined only to the set of packages passed in.
		// External are dependencies that have additional references outside the set of packages passed in.
		TMap<FName, uint64> InternalDependencies;
		TMap<FName, uint64> ExternalDependencies;
		if (!MemoryUsageQueries::GatherDependenciesForPackages(MemoryUsageQueries::CurrentMemoryUsageInfoProvider, PackageNames, InternalDependencies, ExternalDependencies, &Ar))
		{
			Ar.Logf(TEXT("Failed to gather memory usage for dependencies in Collection %s"), *CollectionInfo.Name);
			break;
		}

		uint64 TotalCollectionSize = 0;

		// Determine which category where each assets dependency should reside
		for (TPair<FName, FAssetMemoryDetails>& Asset : AssetMemoryMap)
		{
			FAssetMemoryDetails& AssetMemory = Asset.Value;
			FAssetMemoryBreakdown& AssetMemoryDetails = AssetMemory.MemoryBreakdown;

			for (FName& Dep : Asset.Value.Dependencies)
			{
				// Don't want to count asset itself, plus some dependencies might refer to other assets in the map
				if (AssetMemoryMap.Contains(Dep))
				{
					continue;
				}

				FAssetMemoryBreakdown DependencyMemory;
				uint64* UniqueMemory = InternalDependencies.Find(Dep);
				uint64* SharedMemory = ExternalDependencies.Find(Dep);
				bool bRecordDependency = false;

				if (UniqueMemory != nullptr && *UniqueMemory != 0)
				{
					DependencyMemory.UniqueSize = *UniqueMemory;
					AssetMemoryDetails.UniqueSize += DependencyMemory.UniqueSize;
					AssetMemory.UniqueRefCount++;
					bRecordDependency = true;
				}

				if (SharedMemory != nullptr && *SharedMemory != 0)
				{
					DependencyMemory.SharedSize = *SharedMemory;
					AssetMemoryDetails.SharedSize += DependencyMemory.SharedSize;
					AssetMemory.SharedRefCount++;
					bRecordDependency = true;
				}

				if (bRecordDependency)
				{
					AssetMemory.DependenciesToMemoryMap.Add(Dep, DependencyMemory);
				}
			}

			AssetMemoryDetails.TotalSize = AssetMemoryDetails.ExclusiveSize + AssetMemoryDetails.UniqueSize;
			TotalCollectionSize += AssetMemoryDetails.TotalSize;
		}

		// Sort by TotalSize
		AssetMemoryMap.ValueSort([](const FAssetMemoryDetails& A, const FAssetMemoryDetails& B) {
			return A.MemoryBreakdown.TotalSize > B.MemoryBreakdown.TotalSize;
			});

		if (CommonArgs.bCSV)
		{
			CurrentOutputDevice->Logf(TEXT(",Asset,Exclusive KiB,Unique Refs KiB,Unique Ref Count,Shared Refs KiB,Shared Ref Count,Total KiB"));
		}
		else
		{
			CurrentOutputDevice->Logf(
				TEXT(" %100s %20s %20s %15s %20s %15s %25s"),
				TEXT("Asset"),
				TEXT("Exclusive KiB"),
				TEXT("Unique Refs KiB"),
				TEXT("Unique Ref Count"),
				TEXT("Shared Refs KiB"),
				TEXT("Shared Ref Count"),
				TEXT("Total KiB")
			);
		}

		// Asset listing
		for (TPair<FName, FAssetMemoryDetails>& Asset : AssetMemoryMap)
		{
			FAssetMemoryDetails& AssetMemory = Asset.Value;
			FAssetMemoryBreakdown& AssetMemoryDetails = AssetMemory.MemoryBreakdown;

			if (CommonArgs.bCSV)
			{
				CurrentOutputDevice->Logf(TEXT(",%s,%.2f,%.2f,%d,%.2f,%d,%.2f"), *Asset.Key.ToString(),
					AssetMemoryDetails.ExclusiveSize / 1024.f,
					AssetMemoryDetails.UniqueSize / 1024.f,
					AssetMemory.UniqueRefCount,
					AssetMemoryDetails.SharedSize / 1024.f,
					AssetMemory.SharedRefCount,
					AssetMemoryDetails.TotalSize / 1024.f);
			}
			else
			{
				CurrentOutputDevice->Logf(
					TEXT(" %100s %20.2f %20.2f %15d %20.2f %15d %25.2f"),
					*Asset.Key.ToString(),
					AssetMemoryDetails.ExclusiveSize / 1024.f,
					AssetMemoryDetails.SharedSize / 1024.f,
					AssetMemory.SharedRefCount,
					AssetMemoryDetails.UniqueSize / 1024.f,
					AssetMemory.UniqueRefCount,
					AssetMemoryDetails.TotalSize / 1024.f
				);
			}
		}

		// Asset dependencies listing
		if (bShowDependencies)
		{
			if (CommonArgs.bCSV)
			{
				CurrentOutputDevice->Logf(TEXT(",Asset,Dependency,Unique KiB,Shared KiB"));
			}
			else
			{
				CurrentOutputDevice->Logf(TEXT(" %100s %100s %20s %20s"),
					TEXT("Asset"),
					TEXT("Dependency"),
					TEXT("Unique KiB"),
					TEXT("Shared KiB")
				);
			}

			for (TPair<FName, FAssetMemoryDetails>& Asset : AssetMemoryMap)
			{
				for (TPair<FName, FAssetMemoryBreakdown>& Dep : Asset.Value.DependenciesToMemoryMap)
				{
					const FString DependencyAssetName = Dep.Key.ToString();
					const FAssetMemoryBreakdown& DepedencyMemoryDetails = Dep.Value;

					if (CommonArgs.bCSV)
					{
						CurrentOutputDevice->Logf(TEXT(",%s,%s,%.2f,%.2f"), *Asset.Key.ToString(), *DependencyAssetName,
							DepedencyMemoryDetails.UniqueSize / 1024.f, DepedencyMemoryDetails.SharedSize / 1024.f);
					}
					else
					{
						CurrentOutputDevice->Logf(TEXT(" %100s %100s %20.2f %20.2f"),
							*Asset.Key.ToString(),
							*DependencyAssetName,
							DepedencyMemoryDetails.UniqueSize / 1024.f,
							DepedencyMemoryDetails.SharedSize / 1024.f
						);
					}
				}
			}
		}

		if (CommonArgs.bCSV)
		{
			CurrentOutputDevice->Logf(TEXT(",TOTAL KiB,%.2f"), TotalCollectionSize / 1024.f);
		}
		else
		{
			CurrentOutputDevice->Logf(TEXT("TOTAL KiB: %.2f"), TotalCollectionSize / 1024.f);
		}
	}
}));

#endif

namespace MemoryUsageQueries
{

IMemoryUsageInfoProvider* GetCurrentMemoryUsageInfoProvider()
{
	return CurrentMemoryUsageInfoProvider;
}

bool GetMemoryUsage(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const FString& PackageName, uint64& OutExclusiveSize, uint64& OutInclusiveSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	FName LongPackageName;
	TSet<FName> Dependencies;
	if (!Internal::GetLongNameAndDependencies(PackageName, LongPackageName, Dependencies, ErrorOutput))
	{
		return false;
	}

	OutExclusiveSize = MemoryUsageInfoProvider->GetAssetMemoryUsage(LongPackageName, ErrorOutput);
	OutInclusiveSize = MemoryUsageInfoProvider->GetAssetsMemoryUsage(Dependencies, ErrorOutput);

	return true;
}

bool GetMemoryUsageCombined(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames, uint64& OutTotalSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	TSet<FName> Dependencies;
	if (!Internal::GetDependenciesCombined(PackageNames, Dependencies, ErrorOutput))
	{
		return false;
	}

	OutTotalSize =  MemoryUsageInfoProvider->GetAssetsMemoryUsage(Dependencies, ErrorOutput);

	return true;
}

bool GetMemoryUsageShared(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames, uint64& OutTotalSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	TSet<FName> Dependencies;
	if (!Internal::GetDependenciesShared(PackageNames, Dependencies, ErrorOutput))
	{
		return false;
	}

	OutTotalSize = MemoryUsageInfoProvider->GetAssetsMemoryUsage(Dependencies, ErrorOutput);

	return true;
}

bool GetMemoryUsageUnique(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames, uint64& OutUniqueSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	TSet<FName> RemovablePackages;
	if (!Internal::GetRemovablePackages(PackageNames, RemovablePackages, ErrorOutput))
	{
		return false;
	}

	OutUniqueSize = MemoryUsageInfoProvider->GetAssetsMemoryUsage(RemovablePackages, ErrorOutput);

	return true;
}

bool GetMemoryUsageCommon(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames, uint64& OutCommonSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	TSet<FName> UnremovablePackages;
	if (!Internal::GetUnremovablePackages(PackageNames, UnremovablePackages, ErrorOutput))
	{
		return false;
	}

	OutCommonSize = MemoryUsageInfoProvider->GetAssetsMemoryUsage(UnremovablePackages, ErrorOutput);

	return true;
}

bool GatherDependenciesForPackages(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames,
	TMap<FName, uint64>& OutInternalDeps, TMap<FName, uint64>& OutExternalDeps, FOutputDevice* ErrorOutput)
{
	TSet<FName> RemovablePackages;
	if (!Internal::GetRemovablePackages(PackageNames, RemovablePackages, ErrorOutput))
	{
		return false;
	}

	TSet<FName> UnremovablePackages;
	if (!Internal::GetUnremovablePackages(PackageNames, UnremovablePackages, ErrorOutput))
	{
		return false;
	}

	MemoryUsageInfoProvider->GetAssetsMemoryUsageWithSize(RemovablePackages, OutInternalDeps, ErrorOutput);
	MemoryUsageInfoProvider->GetAssetsMemoryUsageWithSize(UnremovablePackages, OutExternalDeps, ErrorOutput);

	return true;
}


bool GetDependenciesWithSize(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const FString& PackageName, TMap<FName, uint64>& OutDependenciesWithSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	FName LongPackageName;
	TSet<FName> Dependencies;

	if (!Internal::GetLongNameAndDependencies(PackageName, LongPackageName, Dependencies, ErrorOutput))
	{
		return false;
	}

	OutDependenciesWithSize.Empty();
	Internal::SortPackagesBySize(MemoryUsageInfoProvider, Dependencies, OutDependenciesWithSize, ErrorOutput);

	return true;
}

bool GetDependenciesWithSizeCombined(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames, TMap<FName, uint64>& OutDependenciesWithSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	TSet<FName> Dependencies;

	if (!Internal::GetDependenciesCombined(PackageNames, Dependencies, ErrorOutput))
	{
		return false;
	}

	OutDependenciesWithSize.Empty();
	Internal::SortPackagesBySize(MemoryUsageInfoProvider, Dependencies, OutDependenciesWithSize, ErrorOutput);

	return true;
}

bool GetDependenciesWithSizeShared(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames, TMap<FName, uint64>& OutDependenciesWithSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	TSet<FName> Dependencies;
	if (!Internal::GetDependenciesShared(PackageNames, Dependencies, ErrorOutput))
	{
		return false;
	}

	OutDependenciesWithSize.Empty();
	Internal::SortPackagesBySize(MemoryUsageInfoProvider, Dependencies, OutDependenciesWithSize, ErrorOutput);

	return true;
}

bool GetDependenciesWithSizeUnique(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames, TMap<FName, uint64>& OutDependenciesWithSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	TSet<FName> RemovablePackages;
	if (!Internal::GetRemovablePackages(PackageNames, RemovablePackages, ErrorOutput))
	{
		return false;
	}

	OutDependenciesWithSize.Empty();
	Internal::SortPackagesBySize(MemoryUsageInfoProvider, RemovablePackages, OutDependenciesWithSize, ErrorOutput);

	return true;
}

bool GetDependenciesWithSizeCommon(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TArray<FString>& PackageNames, TMap<FName, uint64>& OutDependenciesWithSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	TSet<FName> UnremovablePackages;
	if (!Internal::GetUnremovablePackages(PackageNames, UnremovablePackages, ErrorOutput))
	{
		return false;
	}

	OutDependenciesWithSize.Empty();
	Internal::SortPackagesBySize(MemoryUsageInfoProvider, UnremovablePackages, OutDependenciesWithSize, ErrorOutput);

	return true;
}

#if ENABLE_LOW_LEVEL_MEM_TRACKER
bool GetFilteredPackagesWithSize(TMap<FName, uint64>& OutPackagesWithSize, FName GroupName /* = NAME_None */, FString AssetSubstring /* = FString() */, FName ClassName /* = NAME_None */, FOutputDevice* ErrorOutput /* = GLog */)
{
	TArray<FLLMTagSetAllocationFilter> Filters;
	if (GroupName != NAME_None)
	{
		Filters.Add({GroupName, ELLMTagSet::None});
	}

	if (ClassName != NAME_None)
	{
		Filters.Add({ClassName, ELLMTagSet::AssetClasses});
	}

	MemoryUsageInfoProviderLLM.GetFilteredTagsWithSize(OutPackagesWithSize, ELLMTracker::Default, ELLMTagSet::Assets, Filters, ErrorOutput);

	if (AssetSubstring.Len() > 0)
	{
		Internal::RemoveFilteredPackages(OutPackagesWithSize, AssetSubstring);
	}

	OutPackagesWithSize.ValueSort(TGreater<uint64>());

	return true;
}

bool GetFilteredClassesWithSize(TMap<FName, uint64>& OutClassesWithSize, FName GroupName /* = NAME_None */, FString AssetName /* = FString() */, FOutputDevice* ErrorOutput /* = GLog */)
{
	TArray<FLLMTagSetAllocationFilter> Filters;

	FName LongName = NAME_None;
	if (!AssetName.IsEmpty() && !Internal::GetLongName(AssetName, LongName, ErrorOutput))
	{
		return false;
	}

	if (LongName != NAME_None)
	{
		Filters.Add({LongName, ELLMTagSet::Assets});
	}

	if (GroupName != NAME_None)
	{
		Filters.Add({GroupName, ELLMTagSet::None});
	}

	MemoryUsageInfoProviderLLM.GetFilteredTagsWithSize(OutClassesWithSize, ELLMTracker::Default, ELLMTagSet::AssetClasses, Filters, ErrorOutput);

	OutClassesWithSize.ValueSort(TGreater<uint64>());

	return true;
}

bool GetFilteredGroupsWithSize(TMap<FName, uint64>& OutGroupsWithSize, FString AssetName /* = FString() */, FName ClassName /* = NAME_None */, FOutputDevice* ErrorOutput /* = GLog */)
{
	TArray<FLLMTagSetAllocationFilter> Filters;

	FName LongName = NAME_None;
	if (!AssetName.IsEmpty() && !Internal::GetLongName(AssetName, LongName, ErrorOutput))
	{
		return false;
	}

	if (LongName != NAME_None)
	{
		Filters.Add({LongName, ELLMTagSet::Assets});
	}

	if (ClassName != NAME_None)
	{
		Filters.Add({ClassName, ELLMTagSet::AssetClasses});
	}

	MemoryUsageInfoProviderLLM.GetFilteredTagsWithSize(OutGroupsWithSize, ELLMTracker::Default, ELLMTagSet::None, Filters, ErrorOutput);

	OutGroupsWithSize.ValueSort(TGreater<uint64>());

	return true;
}
#endif

namespace Internal
{
FMemoryUsageReferenceProcessor::FMemoryUsageReferenceProcessor()
{
	int32 Num = GUObjectArray.GetObjectArrayNum();

	Excluded.Init(false, Num);
	ReachableFull.Init(false, Num);
	ReachableExcluded.Init(false, Num);
}

bool FMemoryUsageReferenceProcessor::Init(const TArray<FString>& PackageNames, FOutputDevice* ErrorOutput /* = GLog */)
{
	for (FRawObjectIterator It(true); It; ++It)
	{
		FUObjectItem* ObjectItem = *It;
		UObject* Object = (UObject*)ObjectItem->Object;

		if (ObjectItem->IsUnreachable())
		{
			continue;
		}

		if (ObjectItem->IsRootSet())
		{
			RootSetPackages.Add(Object);
		}

		if (UClass* Class = dynamic_cast<UClass*>(Object))
		{
			if (!Class->HasAnyClassFlags(CLASS_TokenStreamAssembled))
			{
				Class->AssembleReferenceTokenStream();
				check(Class->HasAnyClassFlags(CLASS_TokenStreamAssembled));
			}
		}
	}

	if (FPlatformProperties::RequiresCookedData() && FGCObject::GGCObjectReferencer && GUObjectArray.IsDisregardForGC(FGCObject::GGCObjectReferencer))
	{
		RootSetPackages.Add(FGCObject::GGCObjectReferencer);
	}

	TSet<FName> LongPackageNames;
	if (!GetLongNames(PackageNames, LongPackageNames, ErrorOutput))
	{
		return false;
	}

	for (const auto& PackageName : LongPackageNames)
	{
		UPackage* Package = Cast<UPackage>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, PackageName, /*bExactClass*/ true));
		if (Package)
		{
			auto ExcludeObject = [&](UObject* ObjectToExclude) {
				if (ObjectToExclude && GUObjectArray.ObjectToIndex(ObjectToExclude) < Excluded.Num())
				{
					Excluded[GUObjectArray.ObjectToIndex(ObjectToExclude)] = true;
				}
			};

			auto ExcludeObjectOfClass = [&](UObject* ObjectOfClass) {
				if (ObjectOfClass)
				{
					ForEachObjectWithOuter(ObjectOfClass, ExcludeObject);
				}
				ExcludeObject(ObjectOfClass);
			};

			auto ExcludeObjectInPackage = [&](UObject* ObjectInPackage) {
				if (ObjectInPackage->IsA<UClass>())
				{
					UClass* Class = static_cast<UClass*>(ObjectInPackage);
					ForEachObjectOfClass(Class, ExcludeObjectOfClass);
				}
				ExcludeObject(ObjectInPackage);
			};

			ForEachObjectWithOuter(Package, ExcludeObjectInPackage);
			ExcludeObject(Package);
		}
	}

	return true;
}

TArray<UObject*>& FMemoryUsageReferenceProcessor::GetRootSet()
{
	return RootSetPackages;
}

void FMemoryUsageReferenceProcessor::HandleTokenStreamObjectReference(UE::GC::FWorkerContext& Context, const UObject* ReferencingObject, UObject*& Object, UE::GC::FTokenId TokenIndex, const EGCTokenType TokenType, bool bAllowReferenceElimination)
{
	FPermanentObjectPoolExtents PermanentPool;
	if (Object == nullptr || GUObjectArray.ObjectToIndex(Object) >= ReachableFull.Num() || PermanentPool.Contains(Object) || GUObjectArray.IsDisregardForGC(Object))
	{
		return;
	}

	int32 ObjectIndex = GUObjectArray.ObjectToIndex(Object);

	if (Mode == EMode::Full)
	{
		if (!ReachableFull[ObjectIndex])
		{
			ReachableFull[ObjectIndex] = true;
			Context.ObjectsToSerialize.Add<Options>(Object);
		}
	}
	else if (Mode == EMode::Excluding)
	{
		if (!ReachableExcluded[ObjectIndex] && !Excluded[ObjectIndex])
		{
			ReachableExcluded[ObjectIndex] = true;
			Context.ObjectsToSerialize.Add<Options>(Object);
		}
	}
}

bool FMemoryUsageReferenceProcessor::GetUnreachablePackages(TSet<FName>& OutUnreachablePackages)
{
	for (int i = 0; i < ReachableFull.Num(); i++)
	{
		if (ReachableFull[i] && !ReachableExcluded[i])
		{
			UObject* Obj = static_cast<UObject*>(GUObjectArray.IndexToObjectUnsafeForGC(i)->Object);
			if (Obj && Obj->IsA<UPackage>())
			{
				OutUnreachablePackages.Add(Obj->GetFName());
			}
		}
	}

	return true;
}

static FAutoConsoleVariable CVarMemQueryUsePackageStore(
	TEXT("MemQuery.UsePackageStore"), true,
	TEXT("True - use PackageStore, false - use AssetRegistry."), ECVF_Default);

static FAssetRegistryModule& GetAssetRegistryModule()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	if (IsInGameThread())
	{
		AssetRegistryModule.Get().WaitForCompletion();
	}

	return AssetRegistryModule;
}

class FPackageStoreLazyDatabase final
{
	FPackageStoreLazyDatabase()
	{
		FPackageName::OnContentPathMounted().AddRaw(this, &FPackageStoreLazyDatabase::OnContentPathMounted);
		FPackageName::OnContentPathDismounted().AddRaw(this, &FPackageStoreLazyDatabase::OnContentPathDismounted);
	}

	~FPackageStoreLazyDatabase()
	{
		FPackageName::OnContentPathMounted().RemoveAll(this);
		FPackageName::OnContentPathDismounted().RemoveAll(this);
	}

	void ResetDatabase()
	{
		bIsAssetDatabaseSearched = false;
		bIsDirectoryIndexSearched = false;
		Names.Empty();
		Database.Empty();
	}

	void OnContentPathMounted(const FString&, const FString&) { ResetDatabase(); }
	void OnContentPathDismounted(const FString&, const FString&) { ResetDatabase(); }

	bool BuildDatabaseWhile(TFunctionRef<bool(const FPackageId& PackageId, const FName& PackageName)> Predicate)
	{
		bool bIterationBrokeEarly = false;

		if (!bIsAssetDatabaseSearched)
		{
			const FAssetRegistryModule& AssetRegistryModule = GetAssetRegistryModule();
			constexpr bool bOnlyOnDiskAssets = true;

			AssetRegistryModule.Get().EnumerateAllAssets(
				[&](const FAssetData& Data)
				{
					FPackageId PackageId = FPackageId::FromName(Data.PackageName);
					{
						LLM_SCOPE(TEXT("MemoryUsageQueries"));
						Database.FindOrAdd(PackageId, Data.PackageName);
						Names.Add(Data.PackageName);
					}

					if (!Invoke(Predicate, PackageId, Data.PackageName))
					{
						bIterationBrokeEarly = true;
						return false;
					}

					return true;
				},
				bOnlyOnDiskAssets);

			if (bIterationBrokeEarly)
			{
				return false;
			}

			bIsAssetDatabaseSearched = true;
		}

		if (!bIsDirectoryIndexSearched)
		{
			FPakPlatformFile::ForeachPackageInIostoreWhile(
				[&](FName PackageName)
				{
					FPackageId PackageId = FPackageId::FromName(PackageName);
					{
						LLM_SCOPE(TEXT("MemoryUsageQueries"));
						Database.FindOrAdd(PackageId, PackageName);
						Names.Add(PackageName);
					}

					if (!Invoke(Predicate, PackageId, PackageName))
					{
						bIterationBrokeEarly = true;
						return false;
					}

					return true;
				});

			if (bIterationBrokeEarly)
			{
				return false;
			}

			bIsDirectoryIndexSearched = true;
		}

		return true;
	}

	TMap<FPackageId, FName> Database;
	TArray<FName> Names;
	bool bIsAssetDatabaseSearched = false;
	bool bIsDirectoryIndexSearched = false;

public:
	static FPackageStoreLazyDatabase& Get()
	{
		static FPackageStoreLazyDatabase Instance{};
		return Instance;
	}

	// Blocking Call, builds full database
	void IterateAllPackages(TFunctionRef<void(const FPackageId&)> Visitor)
	{
		BuildDatabaseWhile([](const auto&, const auto&) { return true; });

		for (const TTuple<FPackageId, FName>& Pair : Database)
		{
			Invoke(Visitor, Pair.Key);
		}
	}


	bool GetPackageNameFromId(FPackageId InPackageId, FName& OutFullName)
	{
		if (const FName* Found = Database.Find(InPackageId))
		{
			OutFullName = *Found;
			return true;
		}

		const bool bIterationCompletedWithoutBreak = BuildDatabaseWhile(
			[&](const FPackageId& PackageId, const FName& PackageName)
			{
				if (PackageId == InPackageId)
				{
					OutFullName = PackageName;
					return false;
				}

				return true;
			});

		return !bIterationCompletedWithoutBreak;
	}

	bool GetFirstPackageNameFromPartialName(FStringView InPartialName, FName& OutPackageName)
	{
		auto SearchCriteria = [InPartialName](const FName& PackageName) -> bool
		{
			TCHAR Storage[FName::StringBufferSize];
			PackageName.ToString(Storage);
			return UE::String::FindFirst(Storage, InPartialName, ESearchCase::IgnoreCase) != INDEX_NONE;
		};

		if (const FName* Found = Names.FindByPredicate(SearchCriteria))
		{
			OutPackageName = *Found;
			return true;
		}

		const bool bIterationCompletedWithoutBreak = BuildDatabaseWhile(
			[&](const FPackageId&, const FName& PackageName)
			{
				if (SearchCriteria(PackageName))
				{
					OutPackageName = PackageName;
					return false;
				}

				return true;
			});

		return !bIterationCompletedWithoutBreak;
	}

	bool DoesPackageExists(const FName& PackageName)
	{
		BuildDatabaseWhile([](const auto&, const auto&) { return true; });
		return Names.Contains(PackageName);
	}
};

class FPackageDependenciesLazyDatabase final
{
	FPackageDependenciesLazyDatabase()
	{
		FPackageName::OnContentPathMounted().AddRaw(this, &FPackageDependenciesLazyDatabase::OnContentPathMounted);
		FPackageName::OnContentPathDismounted().AddRaw(this, &FPackageDependenciesLazyDatabase::OnContentPathDismounted);
	}

	~FPackageDependenciesLazyDatabase()
	{
		FPackageName::OnContentPathMounted().RemoveAll(this);
		FPackageName::OnContentPathDismounted().RemoveAll(this);
	}

	void ResetDatabase()
	{
		Dependencies.Empty();
		Referencers.Empty();
		Leafs.Empty();
		Roots.Empty();
	}

	void OnContentPathMounted(const FString&, const FString&) { ResetDatabase(); }
	void OnContentPathDismounted(const FString&, const FString&) { ResetDatabase(); }

	bool InsertPackage(const FPackageId RootPackageId)
	{
		LLM_SCOPE(TEXT("MemoryUsageQueries"));

		TArray<FPackageId, TInlineAllocator<2048>> Stack;

		Stack.Add(RootPackageId);
		bool bAddedSuccessfully = false;

		while (!Stack.IsEmpty())
		{
			const FPackageId PackageId = Stack.Pop(EAllowShrinking::No);

			if (Dependencies.Contains(PackageId) || Leafs.Contains(PackageId))
			{
				bAddedSuccessfully = true;
				continue;
			}

			FPackageStoreEntry PackageEntry;
			const EPackageStoreEntryStatus Status = FPackageStore::Get().GetPackageStoreEntry(PackageId, NAME_None,
				PackageEntry);
			if (Status == EPackageStoreEntryStatus::Ok)
			{
				// add package dependencies
				const TArrayView<const FPackageId> ImportedPackageIds = PackageEntry.ImportedPackageIds;
				for (FPackageId DependentId : ImportedPackageIds)
				{
					Dependencies.FindOrAdd(PackageId).Add(DependentId);
					Referencers.FindOrAdd(DependentId).Add(PackageId);
					bAddedSuccessfully = true;
				}
				Stack.Append(ImportedPackageIds);

#if WITH_EDITOR
				// Add editor optional dependencies
				const TArrayView<const FPackageId> OptionalImportedPackageIds = PackageEntry.OptionalSegmentImportedPackageIds;
				for (FPackageId DependentId : OptionalImportedPackageIds)
				{
					Dependencies.FindOrAdd(PackageId).Add(DependentId);
					Referencers.FindOrAdd(DependentId).Add(PackageId);
					bAddedSuccessfully = true;
				}
				Stack.Append(OptionalImportedPackageIds);
#endif


				// Add leafs
				if (ImportedPackageIds.IsEmpty()
#if WITH_EDITOR
					&& OptionalImportedPackageIds.IsEmpty()
#endif
				)
				{
					Leafs.Add(PackageId);
					bAddedSuccessfully = true;
				}
			}
		}

		return bAddedSuccessfully;
	}

	TMap<FPackageId, TSet<FPackageId>> Dependencies;
	TMap<FPackageId, TSet<FPackageId>> Referencers;
	TSet<FPackageId> Leafs;
	TSet<FPackageId> Roots;

public:
	static FPackageDependenciesLazyDatabase& Get()
	{
		static FPackageDependenciesLazyDatabase Instance{};
		return Instance;
	}

	bool GetDependencies(const FPackageId PackageId, TSet<FPackageId>& OutDependencies)
	{
		if (Leafs.Contains(PackageId))
		{
			return true;
		}

		const TSet<FPackageId>* ChildrenSet = Dependencies.Find(PackageId);
		if (ChildrenSet == nullptr)
		{
			FPackageStoreReadScope _(FPackageStore::Get());
			if (!InsertPackage(PackageId))
			{
				return false;
			}
			ChildrenSet = Dependencies.Find(PackageId);
		}
		if (ChildrenSet == nullptr)
		{
			return false;
		}

		TArray<FPackageId, TInlineAllocator<2048>> Stack(ChildrenSet->Array());
		while (!Stack.IsEmpty())
		{
			FPackageId Child = Stack.Pop(EAllowShrinking::No);

			if (!OutDependencies.Contains(Child))
			{
				OutDependencies.Add(Child);
				if (const TSet<FPackageId>* GrandChildrenSet = Dependencies.Find(Child))
				{
					Stack.Append(GrandChildrenSet->Array());
				}
			}
		}

		return true;
	}

	bool GetReferencers(const FPackageId InPackageId, TSet<FPackageId>& OutReferencers)
	{
		if (Roots.Contains(InPackageId))
		{
			return true;
		}

		TSet<FPackageId>* ParentsSet = Referencers.Find(InPackageId);
		if (ParentsSet == nullptr)
		{
			FPackageStoreReadScope _(FPackageStore::Get());
			// First IterateAllPackages call builds dependencies map
			FPackageStoreLazyDatabase::Get().IterateAllPackages([this](const FPackageId& PackageId) { InsertPackage(PackageId); });
			// Second IterateAllPackages call caches root nodes (only valid after full database is built)
			FPackageStoreLazyDatabase::Get().IterateAllPackages(
				[this](const FPackageId& PackageId)
				{
					if (!Referencers.Contains(PackageId))
					{
						Roots.Add(PackageId);
					}
				});

			ParentsSet = Referencers.Find(InPackageId);
		}

		OutReferencers = MoveTemp(*ParentsSet);

		return true;
	}
};

static bool GetLongNamePackageStore(FStringView InShortPackageName, FName& OutLongPackageName)
{
	return FPackageStoreLazyDatabase::Get().GetFirstPackageNameFromPartialName(InShortPackageName, OutLongPackageName);
}

static bool GetLongNameAssetRegistry(FStringView InShortPackageName, FName& OutLongPackageName)
{
	const FAssetRegistryModule& AssetRegistryModule = GetAssetRegistryModule();

	OutLongPackageName = AssetRegistryModule.Get().GetFirstPackageByName(InShortPackageName);
	if (OutLongPackageName == NAME_None)
	{
		return false;
	}

	return true;
}

bool GetLongName(FStringView ShortPackageName, FName& OutLongPackageName, FOutputDevice* ErrorOutput /* = GLog */)
{
	if (FPackageName::IsValidLongPackageName(ShortPackageName))
	{
		OutLongPackageName = FName(ShortPackageName);
		return true;
	}

	const bool Result =
		CVarMemQueryUsePackageStore->GetBool() && FPackageStore::Get().HasAnyBackendsMounted()
		? GetLongNamePackageStore(ShortPackageName, OutLongPackageName)
		: GetLongNameAssetRegistry(ShortPackageName, OutLongPackageName);

	if (!Result && ErrorOutput)
	{
		ErrorOutput->Logf(ELogVerbosity::Error, TEXT("MemQuery Error: Package not found: %.*s"), ShortPackageName.Len(), ShortPackageName.GetData());
	}
	return Result;
}

bool GetLongNames(const TArray<FString>& PackageNames, TSet<FName>& OutLongPackageNames, FOutputDevice* ErrorOutput /* = GLog */)
{
	for (const auto& Package : PackageNames)
	{
		FName LongName;
		if (!GetLongName(Package, LongName, ErrorOutput))
		{
			return false;
		}
		OutLongPackageNames.Add(LongName);
	}

	return true;
}

bool GetLongNameAndDependencies(FStringView PackageName, FName& OutLongPackageName, TSet<FName>& OutDependencies, FOutputDevice* ErrorOutput /* = GLog */)
{
	if (!GetLongName(PackageName, OutLongPackageName, ErrorOutput))
	{
		return false;
	}

	GetTransitiveDependencies(OutLongPackageName, OutDependencies);
	OutDependencies.Add(OutLongPackageName);

	return true;
}

bool GetDependenciesCombined(const TArray<FString>& PackageNames, TSet<FName>& OutDependencies, FOutputDevice* ErrorOutput /* = GLog */)
{
	FName LongPackageName;
	TSet<FName> Dependencies;

	for (const auto& Package : PackageNames)
	{
		Dependencies.Empty();
		if (!Internal::GetLongNameAndDependencies(Package, LongPackageName, Dependencies, ErrorOutput))
		{
			return false;
		}

		OutDependencies.Append(Dependencies);
	}

	return true;
}

bool GetDependenciesShared(const TArray<FString>& PackageNames, TSet<FName>& OutDependencies, FOutputDevice* ErrorOutput /* = GLog */)
{
	FName LongPackageName;
	TSet<FName> Dependencies;

	for (int i = 0; i < PackageNames.Num(); i++)
	{
		Dependencies.Empty();
		if (!Internal::GetLongNameAndDependencies(PackageNames[i], LongPackageName, Dependencies, ErrorOutput))
		{
			return false;
		}

		if (i == 0)
		{
			OutDependencies.Append(Dependencies);
			continue;
		}

		OutDependencies = OutDependencies.Intersect(Dependencies);
	}

	return true;
}

bool PerformReachabilityAnalysis(FMemoryUsageReferenceProcessor& ReferenceProcessor, FOutputDevice* ErrorOutput /* = GLog */)
{
	{
		FGCArrayStruct ArrayStruct;
		ArrayStruct.SetInitialObjectsUnpadded(ReferenceProcessor.GetRootSet());
		ReferenceProcessor.SetMode(FMemoryUsageReferenceProcessor::Full);
		CollectReferences<FMemoryUsageReferenceCollector, FMemoryUsageReferenceProcessor>(ReferenceProcessor, ArrayStruct);
	}

	{
		FGCArrayStruct ArrayStruct;
		ArrayStruct.SetInitialObjectsUnpadded(ReferenceProcessor.GetRootSet());
		ReferenceProcessor.SetMode(FMemoryUsageReferenceProcessor::Excluding);
		CollectReferences<FMemoryUsageReferenceCollector, FMemoryUsageReferenceProcessor>(ReferenceProcessor, ArrayStruct);
	}

	return true;
}

bool GetRemovablePackages(const TArray<FString>& PackagesToUnload, TSet<FName>& OutRemovablePackages, FOutputDevice* ErrorOutput /* = GLog */)
{
	FMemoryUsageReferenceProcessor ReferenceProcessor;

	if (!ReferenceProcessor.Init(PackagesToUnload, ErrorOutput))
	{
		return false;
	}

	if (!PerformReachabilityAnalysis(ReferenceProcessor, ErrorOutput))
	{
		return false;
	}

	return ReferenceProcessor.GetUnreachablePackages(OutRemovablePackages);
}

bool GetUnremovablePackages(const TArray<FString>& PackagesToUnload, TSet<FName>& OutUnremovablePackages, FOutputDevice* ErrorOutput /* = GLog */)
{
	FMemoryUsageReferenceProcessor ReferenceProcessor;

	if (!ReferenceProcessor.Init(PackagesToUnload, ErrorOutput))
	{
		return false;
	}

	if (!PerformReachabilityAnalysis(ReferenceProcessor, ErrorOutput))
	{
		return false;
	}

	TSet<FName> UnreachablePackages;
	if (!ReferenceProcessor.GetUnreachablePackages(UnreachablePackages))
	{
		return false;
	}

	TSet<FName> Dependencies;
	if (!Internal::GetDependenciesCombined(PackagesToUnload, Dependencies, ErrorOutput))
	{
		return false;
	}

	for (const auto& Package : Dependencies)
	{
		if (!UnreachablePackages.Contains(Package) && StaticFindObjectFast(UPackage::StaticClass(), nullptr, Package, true) != nullptr)
		{
			OutUnremovablePackages.Add(Package);
		}
	}

	return true;
}

static void GetTransitiveDependenciesAssetRegistry(FName PackageName, TSet<FName>& OutDependencies)
{
	FAssetRegistryModule& AssetRegistryModule = GetAssetRegistryModule();

	TArray<FName> PackageQueue;
	TSet<FName> ExaminedPackages;
	TArray<FName> PackageDependencies;
	OutDependencies.Empty();

	PackageQueue.Push(PackageName);

	while (PackageQueue.Num() > 0)
	{
		const FName& CurrentPackage = PackageQueue.Pop();
		if (ExaminedPackages.Contains(CurrentPackage))
		{
			continue;
		}

		ExaminedPackages.Add(CurrentPackage);

		if (CurrentPackage != PackageName && !OutDependencies.Contains(CurrentPackage))
		{
			OutDependencies.Add(CurrentPackage);
		}

		PackageDependencies.Empty();
		AssetRegistryModule.Get().GetDependencies(CurrentPackage, PackageDependencies, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

		for (const auto& Package : PackageDependencies)
		{
			if (!ExaminedPackages.Contains(Package))
			{
				PackageQueue.Push(Package);
			}
		}
	}
}

static void GetTransitiveDependenciesPackageStore(FName PackageName, TSet<FName>& OutDependencies)
{
	const FPackageId PackageId = FPackageId::FromName(PackageName);
	TSet<FPackageId> TransitiveDependencies;
	if (!FPackageDependenciesLazyDatabase::Get().GetDependencies(PackageId, TransitiveDependencies))
	{
		return;
	}

	OutDependencies.Empty();

	for (const FPackageId DependencyId : TransitiveDependencies)
	{
		FName Name;
		if (FPackageStoreLazyDatabase::Get().GetPackageNameFromId(DependencyId, Name))
		{
			OutDependencies.Add(Name);
		}
		else
		{
			OutDependencies.Add(FName(EName::Package, DependencyId.Value()));
		}
	}
}

void GetTransitiveDependencies(FName PackageName, TSet<FName>& OutDependencies)
{
	if (CVarMemQueryUsePackageStore->GetBool() && FPackageStore::Get().HasAnyBackendsMounted())
	{
		GetTransitiveDependenciesPackageStore(PackageName, OutDependencies);
	}
	else
	{
		GetTransitiveDependenciesAssetRegistry(PackageName, OutDependencies);
	}
}

void SortPackagesBySize(
	const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TSet<FName>& Packages, TMap<FName, uint64>& OutPackagesWithSize,
	FOutputDevice* ErrorOutput /* = GLog */)
{
	GetPackagesSize(MemoryUsageInfoProvider, Packages, OutPackagesWithSize, ErrorOutput);
	OutPackagesWithSize.ValueSort([&](const uint64& A, const uint64& B) { return A > B; });
}

void GetPackagesSize(const IMemoryUsageInfoProvider* MemoryUsageInfoProvider, const TSet<FName>& Packages, TMap<FName, uint64>& OutPackagesWithSize, FOutputDevice* ErrorOutput /* = GLog */)
{
	for (const auto& Package : Packages)
	{
		uint64 Size = MemoryUsageInfoProvider->GetAssetMemoryUsage(Package, ErrorOutput);
		OutPackagesWithSize.Add(Package, Size);
	}
}

static void RemoveNonExistentPackagesAssetRegistry(TMap<FName, uint64>& OutPackagesWithSize)
{
	FAssetRegistryModule& AssetRegistryModule = GetAssetRegistryModule();

	for (auto It = OutPackagesWithSize.CreateIterator(); It; ++It)
	{
		if (!AssetRegistryModule.Get().DoesPackageExistOnDisk(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

static void RemoveNonExistentPackagesPackageStore(TMap<FName, uint64>& OutPackagesWithSize)
{
	for (auto It = OutPackagesWithSize.CreateIterator(); It; ++It)
	{
		if (!FPackageStoreLazyDatabase::Get().DoesPackageExists(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

void RemoveNonExistentPackages(TMap<FName, uint64>& OutPackagesWithSize)
{
	if (CVarMemQueryUsePackageStore->GetBool() && FPackageStore::Get().HasAnyBackendsMounted())
	{
		RemoveNonExistentPackagesPackageStore(OutPackagesWithSize);
	}
	else
	{
		RemoveNonExistentPackagesAssetRegistry(OutPackagesWithSize);
	}
}

void RemoveFilteredPackages(TMap<FName, uint64>& OutPackagesWithSize, const FString& AssetSubstring)
{
	for (auto It = OutPackagesWithSize.CreateIterator(); It; ++It)
	{
		FString KeyString = It.Key().ToString();
		if (!KeyString.Contains(AssetSubstring))
		{
			It.RemoveCurrent();
		}
	}
}

void PrintTagsWithSize(FOutputDevice& Ar, const TMap<FName, uint64>& TagsWithSize, const TCHAR* Name, bool bTruncate /* = false */, int32 Limit /* = -1 */, bool bCSV /* = false */)
{
	uint64 TotalSize = 0U;
	static const FString NoScopeString(TEXT("No scope"));

	if (Limit < 0)
	{
		Limit = DefaultResultLimit;
	}

	int32 Num = TagsWithSize.Num();
	int32 TagsToDisplay = bTruncate ? FMath::Min(Num, Limit) : Num;
	int It = 0;

	if (bCSV)
	{
		Ar.Logf(TEXT(",Name,SizeMB,SizeKB"));
	}

	for (auto& Elem : TagsWithSize)
	{
		if (It++ >= TagsToDisplay)
		{
			break;
		}

		TotalSize += Elem.Value;
		const FString KeyName = Elem.Key.IsValid() ? Elem.Key.ToString() : NoScopeString;

		const float SizeMB = Elem.Value / (1024.0f * 1024.0f);
		const float SizeKB = Elem.Value / 1024.0f;

		if (bCSV)
		{
			Ar.Logf(TEXT(",%s,%.2f,%.2f"), *KeyName, SizeMB, SizeKB);
		}
		else
		{
			Ar.Logf(TEXT("%s - %.2f MB (%.2f KB)"), *KeyName, SizeMB, SizeKB);
		}
	}

	if (TagsToDisplay < Num && !bCSV)
	{
		Ar.Logf(TEXT("----------------------------------------------------------"));
		Ar.Logf(TEXT("<<truncated>> - displayed %d out of %d %s."), TagsToDisplay, Num, Name);
	}

	const float TotalSizeMB = TotalSize / (1024.0f * 1024.0f);
	const float TotalSizeKB = TotalSize / 1024.0f;

	if (bCSV)
	{
		Ar.Logf(TEXT(",TOTAL,%.2f,%.2f"), TotalSizeMB, TotalSizeKB);
	}
	else
	{
		Ar.Logf(TEXT("TOTAL: %.2f MB (%.2f KB)"), TotalSizeMB, TotalSizeKB);
	}
}

} // namespace Internal

} // namespace MemoryUsageQueries
