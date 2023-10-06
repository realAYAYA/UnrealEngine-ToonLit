// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/PackageResourceManager.h"
#include "UObject/PackageTrailer.h"

namespace UE
{

#if WITH_EDITORONLY_DATA

FString BytesToString(int64 SizeInBytes)
{
	if (SizeInBytes < (8 *1024))
	{
		return FString::Printf(TEXT("%4d bytes"), SizeInBytes);
	}
	else if (SizeInBytes < (1024 * 1024))
	{
		double SizeInKb = static_cast<double>(SizeInBytes) / (1024.0);
		return FString::Printf(TEXT("%.2f KB"), SizeInKb);
	}
	else
	{
		double SizeInMB = static_cast<double>(SizeInBytes) / (1024.0 * 1024.0);
		return FString::Printf(TEXT("%.2f MB"), SizeInMB);
	}
}

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

/**
 * This function is used to write information about package's payloads to the log file. This has no 
 * practical development use and should only be used for debugging purposes. 
 * 
 * @param Args	The function expects each arg to be a valid package path. Failure to provide a valid
 *				package path will result in errors being written to the log.
 */
void DumpPackagePayloadInfo(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(LogVirtualization, Error, TEXT("Command 'DumpPackagePayloadInfo' called without any arguments"));
		return;
	}

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

		TArray<FIoHash> LocalPayloadIds = Trailer.GetPayloads(UE::EPayloadStorageType::Local);
		TArray<FIoHash> VirtualizedPayloadIds = Trailer.GetPayloads(UE::EPayloadStorageType::Virtualized);

		UE_LOG(LogVirtualization, Display, TEXT("")); // Blank line to make the output easier to read
		UE_LOG(LogVirtualization, Display, TEXT("Package: '%s' has %d local and %d virtualized payloads"), *Path.GetDebugName(), LocalPayloadIds.Num(), VirtualizedPayloadIds.Num());

		if (LocalPayloadIds.Num() > 0)
		{
			UE_LOG(LogVirtualization, Display, TEXT("LocalPayloads:"));
			UE_LOG(LogVirtualization, Display, TEXT("Index | %-40s | SizeOnDisk | FilterReason"), TEXT("PayloadIdentifier"));
			for (int32 Index = 0; Index < LocalPayloadIds.Num(); ++Index)
			{
				FPayloadInfo Info = Trailer.GetPayloadInfo(LocalPayloadIds[Index]);
				UE_LOG(LogVirtualization, Display, TEXT("%02d    | %s | %-10s | %s"),
					Index,
					*LexToString(LocalPayloadIds[Index]),
					*BytesToString(Info.CompressedSize),
					*LexToString(Info.FilterFlags));
			}
		}

		if (VirtualizedPayloadIds.Num() > 0)
		{
			UE_LOG(LogVirtualization, Display, TEXT("VirtualizedPayloads:"));
			UE_LOG(LogVirtualization, Display, TEXT("Index|\t%-40s|\tFilterReason"), TEXT("PayloadIdentifier"));
			for (int32 Index = 0; Index < VirtualizedPayloadIds.Num(); ++Index)
			{
				FPayloadInfo Info = Trailer.GetPayloadInfo(VirtualizedPayloadIds[Index]);
				UE_LOG(LogVirtualization, Display, TEXT("%02d:  |\t%s|\t%s"), Index, *LexToString(VirtualizedPayloadIds[Index]), *LexToString(Info.FilterFlags));
			}
		}
	}
}

/** 
 * Note that this command is only valid when 'WITH_EDITORONLY_DATA 1' as virtualized payloads are not
 * expected to exist at runtime. 
 */
static FAutoConsoleCommand CCmdDumpPayloadToc = FAutoConsoleCommand(
	TEXT("DumpPackagePayloadInfo"),
	TEXT("Writes out information about a package's payloads to the log."),
	FConsoleCommandWithArgsDelegate::CreateStatic(DumpPackagePayloadInfo));

#endif //WITH_EDITORONLY_DATA

} // namespace UE
