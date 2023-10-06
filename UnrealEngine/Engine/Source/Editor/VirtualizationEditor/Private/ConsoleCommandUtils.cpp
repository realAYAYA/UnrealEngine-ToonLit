// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConsoleCommandUtils.h"

#include "HAL/FileManager.h"
#include "Misc/PackagePath.h"
#include "Serialization/Archive.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/PackageResourceManager.h"

namespace UE::Virtualization
{

void LogPackageError(FArchive* Ar, const TCHAR* DebugName)
{
	if (Ar == nullptr)
	{
		UE_LOG(LogVirtualization, Error, TEXT("Could not find the package file: '%s'"), DebugName);
		return;
	}

	FPackageFileSummary Summary;
	*Ar << Summary;

	if (Ar->IsError())
	{
		UE_LOG(LogVirtualization, Error, TEXT("Could not find load the package summary from disk: '%s'"), DebugName);
		return;
	}

	if (Summary.Tag != PACKAGE_FILE_TAG)
	{
		UE_LOG(LogVirtualization, Error, TEXT("Package summary seems to be corrupted: '%s'"), DebugName);
		return;
	}

	int32 PackageVersion = Summary.GetFileVersionUE().ToValue();
	if (PackageVersion >= (int32)EUnrealEngineObjectUE5Version::PAYLOAD_TOC)
	{
		UE_LOG(LogVirtualization, Error, TEXT("Package trailer is missing from the package file: '%s'"), DebugName);
		return;
	}

	UE_LOG(LogVirtualization, Error, TEXT("Package is tool old (version %d) to have a package trailer (version %d): '%s'"), PackageVersion, int(EUnrealEngineObjectUE5Version::PAYLOAD_TOC), DebugName);
}

void LogPackageError(const FPackagePath& Path)
{
	const FString DebugName = Path.GetPackageName();
	TUniquePtr<FArchive> PackageAr = IPackageResourceManager::Get().OpenReadExternalResource(EPackageExternalResource::WorkspaceDomainFile, DebugName);

	LogPackageError(PackageAr.Get(), *DebugName);
}

void LogPackageError(const FString& Path)
{
	TUniquePtr<FArchive> PackageAr(IFileManager::Get().CreateFileReader(*Path));

	LogPackageError(PackageAr.Get(), *Path);
}

TArray<TPair<FString, UE::FPackageTrailer>> LoadPackageTrailerFromArgs(const TArray<FString>& Args)
{
	TArray<TPair<FString, UE::FPackageTrailer>> Packages;
	Packages.Reserve(Args.Num());

	for (const FString& Arg : Args)
	{
		FString PathString;

		if (FPackageName::ParseExportTextPath(Arg, nullptr /*OutClassName*/, &PathString))
		{
			PathString = FPackageName::ObjectPathToPackageName(PathString);
		}
		else
		{
			PathString = Arg;
		}

		FPackageTrailer Trailer;

		FPackagePath Path;
		if (FPackagePath::TryFromMountedName(PathString, Path))
		{
			if (!FPackageTrailer::TryLoadFromPackage(Path, Trailer))
			{
				LogPackageError(Path);
				continue;
			}
		}
		else if (IFileManager::Get().FileExists(*PathString))
		{
			// IF we couldn't turn it into a FPackagePath it could be a path to a package not under any current mount point.
			// So for a final attempt we will see if we can find the file on disk and load the package trailer that way.

			if (!FPackageTrailer::TryLoadFromFile(PathString, Trailer))
			{
				LogPackageError(PathString);
				continue;
			}
		}
		else
		{
			UE_LOG(LogVirtualization, Error, TEXT("Arg '%s' could not be converted to a valid package path"), *Arg);
			continue;
		}

		Packages.Add({ MoveTemp(PathString) , MoveTemp(Trailer) });	
	}

	return Packages;
}

} // namespace UE::Virtualization
