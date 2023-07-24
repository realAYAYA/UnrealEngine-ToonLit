// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeneratePayloadManifestCommandlet.h"

#include "CommandletUtils.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "Misc/Paths.h"
#include "UObject/PackageTrailer.h"

UGeneratePayloadManifestCommandlet::UGeneratePayloadManifestCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGeneratePayloadManifestCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGeneratePayloadManifestCommandlet);

	TArray<FString> PackageNames = UE::Virtualization::DiscoverPackages(Params);

	UE_LOG(LogVirtualization, Display, TEXT("Found %d files to look in"), PackageNames.Num());

	int32 PackageTrailerCount = 0;

	const FString CSVPath = FPaths::ProjectSavedDir() / TEXT("payload_manifest.csv");
	TUniquePtr<FArchive> ManifestFile(IFileManager::Get().CreateFileWriter(*CSVPath));
	if (!ManifestFile.IsValid())
	{
		UE_LOG(LogVirtualization, Error, TEXT("Failed to open '%s' for write"), *CSVPath);
		return 1;
	}

	const ANSICHAR* Headings = "Path,PayloadId,SizeOnDisk,UncompressedSize,StorageType, FilterReason\n";
	ManifestFile->Serialize((void*)Headings, FPlatformString::Strlen(Headings));

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ParsePackageTrailers);

		UE_LOG(LogVirtualization, Display, TEXT("Parsing files..."));

		uint32 FilesParsed = 0;
		for (const FString& PackagePath : PackageNames)
		{
			if (FPackageName::IsPackageFilename(PackagePath))
			{
				UE::FPackageTrailer Trailer;
				if (UE::FPackageTrailer::TryLoadFromFile(PackagePath, Trailer))
				{
					PackageTrailerCount++;

					Trailer.ForEachPayload([&ManifestFile,&PackagePath](const FIoHash& Id, uint64 SizeOnDisk, uint64 RawSize, UE::EPayloadAccessMode Mode, UE::Virtualization::EPayloadFilterReason Filter)->void
						{
							TAnsiStringBuilder<256> LineBuilder;
							LineBuilder << PackagePath << "," << Id << "," << SizeOnDisk << "," << RawSize << "," << Mode << "," << *LexToString(Filter) <<"\n";
							ManifestFile->Serialize((void*)LineBuilder.ToString(), LineBuilder.Len());
						});
				}
			}

			if (++FilesParsed % 10000 == 0)
			{
				float PercentCompleted = ((float)FilesParsed / (float)PackageNames.Num()) * 100.0f;
				UE_LOG(LogVirtualization, Display, TEXT("Parsed %d/%d (%.0f%%)"), FilesParsed, PackageNames.Num(), PercentCompleted);
			}
			
		}
	}

	ManifestFile.Reset(); // Flush file to disk

	UE_LOG(LogVirtualization, Display, TEXT("Found %d package trailers"), PackageTrailerCount);
	UE_LOG(LogVirtualization, Display, TEXT("Manifest written to '%s'"), *CSVPath);

	return  0;
}