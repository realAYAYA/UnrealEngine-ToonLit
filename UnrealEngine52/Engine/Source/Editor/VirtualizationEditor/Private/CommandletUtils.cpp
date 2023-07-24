// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommandletUtils.h"

#include "Async/ParallelFor.h"
#include "Containers/Set.h"
#include "Misc/PackageName.h"
#include "PackageHelperFunctions.h"
#include "UObject/PackageTrailer.h"

namespace UE::Virtualization
{

TArray<FString> FindAllPackages()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FindAllPackages);

	// TODO: In theory we could check the VA settings and skip engine content if they are 
	// excluded from virtualization. Not convinced it saves enough time to be worth the 
	// effort.
	const EPackageNormalizationFlags PackageFilter = NORMALIZE_DefaultFlags;

	const FString AssetSearch = TEXT("*") + FPackageName::GetAssetPackageExtension();
	const FString MapSearch = TEXT("*") + FPackageName::GetMapPackageExtension();

	TArray<FString> PackageNames;
	TArray<FString> Unused;
	bool bAnyFound = NormalizePackageNames(Unused, PackageNames, AssetSearch, PackageFilter);
	bAnyFound |= NormalizePackageNames(Unused, PackageNames, MapSearch, PackageFilter);

	return PackageNames;
}

TArray<FString> DiscoverPackages(const FString& CmdlineParams)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DiscoverPackages);

	FString PackageDir;
	if (FParse::Value(*CmdlineParams, TEXT("PackageDir="), PackageDir) || FParse::Value(*CmdlineParams, TEXT("PackageFolder="), PackageDir))
	{
		TArray<FString> FilesInPackageFolder;
		FPackageName::FindPackagesInDirectory(FilesInPackageFolder, PackageDir);

		TArray<FString> PackageNames;
		for (const FString& BasePath : FilesInPackageFolder)
		{
			PackageNames.Add(FPaths::CreateStandardFilename(BasePath));
		}

		return PackageNames;
	}
	else
	{
		return FindAllPackages();
	}
}



TArray<FIoHash> FindVirtualizedPayloads(const TArray<FString>& PackagePaths)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FindVirtualizedPayloads);

	// Each task will write out to its own TSet so we don't have to lock anything, we
	// will combine the sets at the end.
	TArray<TSet<FIoHash>> PayloadsPerTask;

	ParallelForWithTaskContext(PayloadsPerTask, PackagePaths.Num(),
		[&PackagePaths](TSet<FIoHash>& Context, int32 Index)
		{
			const FString& PackageName = PackagePaths[Index];

			UE::FPackageTrailer Trailer;
			if (UE::FPackageTrailer::TryLoadFromFile(PackageName, Trailer))
			{
				TArray<FIoHash> VirtualizedPayloads = Trailer.GetPayloads(UE::EPayloadStorageType::Virtualized);
				Context.Append(VirtualizedPayloads);
			}
		});

	// Combine the results into a final set
	TSet<FIoHash> AllPayloads;
	for (const TSet<FIoHash>& Payloads : PayloadsPerTask)
	{
		AllPayloads.Append(Payloads);
	}

	return AllPayloads.Array();
}

} //namespace UE::Virtualization
