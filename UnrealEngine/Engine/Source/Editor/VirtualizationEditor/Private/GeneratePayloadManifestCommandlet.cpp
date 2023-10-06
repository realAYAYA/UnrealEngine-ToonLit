// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeneratePayloadManifestCommandlet.h"

#include "CommandletUtils.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "Misc/Paths.h"
#include "UObject/PackageTrailer.h"

class FSpreadSheet
{
public:
	bool OpenNewSheet()
	{
		Ar.Reset();

		const TStringBuilder<512> CSVPath = GetFilePath(++NumSheets);

		Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(CSVPath.ToString()));
		if (Ar.IsValid())
		{
			const ANSICHAR* Headings = "Path,PayloadId,SizeOnDisk,UncompressedSize,StorageType, FilterReason\n";
			Ar->Serialize((void*)Headings, FPlatformString::Strlen(Headings));

			return true;
		}
		else
		{
			UE_LOG(LogVirtualization, Error, TEXT("Failed to open '%s' for write"), CSVPath.ToString());
			return false;
		}
	}

	bool PrintRow(FAnsiStringView Row)
	{
		if (++NumRowsInSheet >= MaxNumRowsPerSheet || (Ar->Tell() + Row.Len()) >= MaxFileSize)
		{
			if (!OpenNewSheet())
			{
				return false;
			}

			NumRowsInSheet = 0;
		}

		Ar->Serialize((void*)Row.GetData(), Row.Len() * sizeof(FAnsiStringView::ElementType));

		return true;
	}

	void Flush()
	{
		Ar.Reset();
	}

	FString GetDebugInfo() const
	{
		TStringBuilder<1024> Output;

		Output << TEXT("Manifest is comprised of ") << NumSheets << TEXT(" sheet(s) written to:");

		for (int32 Index = 0; Index < NumSheets; ++Index)
		{
			Output << LINE_TERMINATOR << GetFilePath(Index);
		}

		return Output.ToString();
	}

private:

	static TStringBuilder<512> GetFilePath(int32 Index)
	{
		return WriteToString<512>(FPaths::ProjectSavedDir(), TEXT("PayloadManifest/sheet"), Index, TEXT(".csv"));
	}

	// With any values above these we had trouble importing the finished cvs into various spread sheet programs
	const int64 MaxNumRowsPerSheet = 250000;
	const int64 MaxFileSize = 45 * 1024 * 1024;

	int32 NumSheets = 0;
	int32 NumRowsInSheet = 0;

	TUniquePtr<FArchive> Ar;
};

UGeneratePayloadManifestCommandlet::UGeneratePayloadManifestCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGeneratePayloadManifestCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGeneratePayloadManifestCommandlet);

	if (!ParseCmdline(Params))
	{
		UE_LOG(LogVirtualization, Error, TEXT("Failed to parse the command line correctly"));
		return -1;
	}

	TArray<FString> PackageNames = UE::Virtualization::DiscoverPackages(Params, UE::Virtualization::EFindPackageFlags::ExcludeEngineContent);

	UE_LOG(LogVirtualization, Display, TEXT("Found %d files to look in"), PackageNames.Num());

	int32 PackageTrailerCount = 0;
	int32 PayloadCount = 0;

	FSpreadSheet Sheet;
	if (!Sheet.OpenNewSheet())
	{
		return 1;
	}

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

					Trailer.ForEachPayload([&Sheet, &PackagePath, &PayloadCount, bLocalOnly = bLocalOnly](const FIoHash& Id, uint64 SizeOnDisk, uint64 RawSize, UE::EPayloadAccessMode Mode, UE::Virtualization::EPayloadFilterReason Filter)->void
						{
							if (bLocalOnly && Mode != UE::EPayloadAccessMode::Local)
							{
								return;
							}

							PayloadCount++;

							TAnsiStringBuilder<256> LineBuilder;
							LineBuilder << PackagePath << "," << Id << "," << SizeOnDisk << "," << RawSize << "," << Mode << "," << *LexToString(Filter) <<"\n";
							
							Sheet.PrintRow(LineBuilder);
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

	Sheet.Flush();

	UE_LOG(LogVirtualization, Display, TEXT("Found %d package trailers with %d payloads"), PackageTrailerCount, PayloadCount);
	UE_LOG(LogVirtualization, Display, TEXT("%s"), *Sheet.GetDebugInfo());

	return  0;
}

bool UGeneratePayloadManifestCommandlet::ParseCmdline(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;

	ParseCommandLine(*Params, Tokens, Switches);

	bLocalOnly = Switches.Contains(TEXT("LocalOnly"));

	return true;
}
