// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compactify/PatchDataCompactifier.h"

#include "Algo/Partition.h"
#include "Algo/Transform.h"
#include "Async/Async.h"
#include "Logging/LogMacros.h"

#include "Common/FileSystem.h"
#include "Core/ProcessTimer.h"
#include "Enumeration/PatchDataEnumeration.h"

DECLARE_LOG_CATEGORY_CLASS(LogDataCompactifier, Log, All);

namespace BuildPatchServices
{
	struct FManifestResult
	{
	public:
		FManifestResult()
			: bSuccess(false)
			, FileSize(0)
		{ }

		FManifestResult(bool bInSuccess, int64 InFileSize, TSet<FString>&& InReferences)
			: bSuccess(bInSuccess)
			, FileSize(InFileSize)
			, References(MoveTemp(InReferences))
		{ }

	public:
		bool bSuccess;
		int64 FileSize;
		TSet<FString> References;
	};

	struct FDataFileResult
	{
	public:
		FDataFileResult()
			: bIsReferenced(false)
			, bIsOldEnough(false)
			, bIsRecognisedFileType(false)
			, bShouldDelete(false)
			, FileSize(0)
		{ }

		FDataFileResult(bool bInIsReferenced, bool bInIsOldEnough, bool bInIsRecognisedFileType, bool bInShouldDelete, int64 InFileSize)
			: bIsReferenced(bInIsReferenced)
			, bIsOldEnough(bInIsOldEnough)
			, bIsRecognisedFileType(bInIsRecognisedFileType)
			, bShouldDelete(bInShouldDelete)
			, FileSize(InFileSize)
		{ }

	public:
		bool bIsReferenced;
		bool bIsOldEnough;
		bool bIsRecognisedFileType;
		bool bShouldDelete;
		int64 FileSize;
	};

	class FPatchDataCompactifier
		: public IPatchDataCompactifier
	{
	public:
		FPatchDataCompactifier(const FCompactifyConfiguration& InConfiguration);
		~FPatchDataCompactifier();

		// IPatchDataCompactifier interface begin.
		virtual bool Run() override;
		// IPatchDataCompactifier interface end.

	private:
		void DeleteFile(const FString& FilePath) const;
		bool IsPatchData(const FString& FilePath) const;
		FString BuildSizeString(uint64 Size) const;

	private:
		const FCompactifyConfiguration Configuration;
		FNumberFormattingOptions SizeFormattingOptions;
		TUniquePtr<IFileSystem> FileSystem;
	};

	FPatchDataCompactifier::FPatchDataCompactifier(const FCompactifyConfiguration& InConfiguration)
		: Configuration(InConfiguration)
		, SizeFormattingOptions(FNumberFormattingOptions().SetMaximumFractionalDigits(3).SetMinimumFractionalDigits(3))
		, FileSystem(FFileSystemFactory::Create())
	{
	}

	FPatchDataCompactifier::~FPatchDataCompactifier()
	{
	}

	bool FPatchDataCompactifier::Run()
	{
		bool bSuccess = true;

		// We output filenames deleted if requested.
		TUniquePtr<FArchive> DeletedChunkOutput;
		if (!Configuration.DeletedChunkLogFile.IsEmpty())
		{
			DeletedChunkOutput = FileSystem->CreateFileWriter(*Configuration.DeletedChunkLogFile);
			if (!DeletedChunkOutput.IsValid())
			{
				UE_LOG(LogDataCompactifier, Error, TEXT("Could not open output file for writing %s."), *Configuration.DeletedChunkLogFile);
				bSuccess = false;
			}
		}

		if (bSuccess)
		{
			// Track some statistics.
			uint32 FilesProcessed = 0;
			uint32 FilesSkipped = 0;
			uint32 NonPatchFilesProcessed = 0;
			uint32 FilesDeleted = 0;
			uint64 BytesProcessed = 0;
			uint64 BytesSkipped = 0;
			uint64 NonPatchBytesProcessed = 0;
			uint64 BytesDeleted = 0;

			// We'll work out the date of the oldest unreferenced file we'll keep
			FDateTime Cutoff = FDateTime::UtcNow() - FTimespan::FromDays(Configuration.DataAgeThreshold);

			// List all files first, and then we'll work with the list.
			TArray<FString> AllFiles;
			const bool bFindFiles = true;
			const bool bFindDirectories = false;
			FProcessTimer FindFilesTimer;
			FindFilesTimer.Start();
			FileSystem->ParallelFindFilesRecursively(AllFiles, *Configuration.CloudDirectory, nullptr, EAsyncExecution::Thread);
			FindFilesTimer.Stop();
			UE_LOG(LogDataCompactifier, Log, TEXT("Found %d files in %s."), AllFiles.Num(), *FPlatformTime::PrettyTime(FindFilesTimer.GetSeconds()));

			// Filter for manifest files.
			int32 FirstNonManifest = Algo::Partition(AllFiles.GetData(), AllFiles.Num(), [&](const FString& Elem) { return Elem.EndsWith(TEXT(".manifest"), ESearchCase::IgnoreCase); });

			// If we don't have any manifest files, notify that we'll continue to delete all mature chunks.
			if (FirstNonManifest == 0)
			{
				UE_LOG(LogDataCompactifier, Log, TEXT("Could not find any manifest files. Proceeding to delete all mature data."));
			}

			// Handle all manifests first.
			TArray<TFuture<FManifestResult>> ManifestFutures;
			for (int32 FileIdx = 0; FileIdx < FirstNonManifest; ++FileIdx)
			{
				const FString& Filename = AllFiles[FileIdx];
				ManifestFutures.Add(Async(EAsyncExecution::ThreadPool, [this, &Filename]()
				{
					bool bEnumerateSuccess = true;
					TSet<FString> ManifestReferences;
					int64 FileSize = 0;
					const bool bGotFileSize = FileSystem->GetFileSize(*Filename, FileSize);
					const bool bFileStillExists = FileSystem->FileExists(*Filename);
					if (bGotFileSize)
					{
						BuildPatchServices::FPatchDataEnumerationConfiguration EnumerationConfig;
						EnumerationConfig.InputFile = Filename;
						TUniquePtr<IPatchDataEnumeration> PatchDataEnumeration(FPatchDataEnumerationFactory::Create(EnumerationConfig));
						TArray<FString> ManifestReferencesArray;
						bEnumerateSuccess = PatchDataEnumeration->Run(ManifestReferencesArray);
						if (bEnumerateSuccess)
						{
							Algo::Transform(ManifestReferencesArray, ManifestReferences, [&](const FString& Elem) { return Configuration.CloudDirectory / Elem; });
						}
					}
					else if (bFileStillExists)
					{
						UE_LOG(LogDataCompactifier, Warning, TEXT("Could not determine size of %s. Assuming 0 bytes."), *Filename);
						FileSize = 0;
					}
					else
					{
						UE_LOG(LogDataCompactifier, Log, TEXT("File removed since enumeration %s. Using 0 bytes."), *Filename);
						FileSize = 0;
					}
					return FManifestResult(bEnumerateSuccess, FileSize, MoveTemp(ManifestReferences));
				}));
			}
			TSet<FString> ReferencedFileSet;
			int32 ManifestFileIdx = 0;
			for (TFuture<FManifestResult>& ManifestFuture : ManifestFutures)
			{
				// Grab result.
				FManifestResult ManifestResult = ManifestFuture.Get();
				const FString& ManifestFilename = AllFiles[ManifestFileIdx];
				// Process result.
				++FilesProcessed;
				BytesProcessed += ManifestResult.FileSize;
				if (!ManifestResult.bSuccess)
				{
					bSuccess = false;
					UE_LOG(LogDataCompactifier, Error, TEXT("Failed to extract references from %s."), *ManifestFilename);
				}
				else
				{
					const int32 NumReferences = ManifestResult.References.Num();
					ReferencedFileSet.Append(MoveTemp(ManifestResult.References));
					UE_LOG(LogDataCompactifier, Display, TEXT("Extracted %d references from %s. Unioning with existing files, new count of %d."), NumReferences, *ManifestFilename, ReferencedFileSet.Num());
				}
				++ManifestFileIdx;
			}

			if (bSuccess)
			{
				// Now handle all CloudDir files.
				TArray<TFuture<FDataFileResult>> DataFileFutures;
				for (int32 FileIdx = FirstNonManifest; FileIdx < AllFiles.Num(); ++FileIdx)
				{
					const FString& Filename = AllFiles[FileIdx];
					DataFileFutures.Add(Async(EAsyncExecution::ThreadPool, [this, &Filename, &ReferencedFileSet, &Cutoff]()
					{
						int64 FileSize = 0;
						bool bIsReferenced = false;
						bool bIsOldEnough = false;
						bool bIsRecognisedFileType = false;
						bool bShouldDelete = false;
						const bool bGotFileSize = FileSystem->GetFileSize(*Filename, FileSize);
						const bool bFileStillExists = FileSystem->FileExists(*Filename);
						if (bGotFileSize)
						{
							bIsReferenced = ReferencedFileSet.Contains(Filename);
							if (!bIsReferenced)
							{
								FDateTime FileTimeStamp;
								bIsOldEnough = FileSystem->GetTimeStamp(*Filename, FileTimeStamp) && FileTimeStamp < Cutoff;
								bIsRecognisedFileType = IsPatchData(Filename);
								bShouldDelete = bIsOldEnough && bIsRecognisedFileType;
								if (bShouldDelete)
								{
									DeleteFile(Filename);
								}
							}
						}
						else if (bFileStillExists)
						{
							UE_LOG(LogDataCompactifier, Warning, TEXT("Could not determine size of %s. Assuming 0 bytes."), *Filename);
							FileSize = 0;
						}
						else
						{
							UE_LOG(LogDataCompactifier, Log, TEXT("File removed since enumeration %s. Using 0 bytes."), *Filename);
							FileSize = 0;
						}
						return FDataFileResult(bIsReferenced, bIsOldEnough, bIsRecognisedFileType, bShouldDelete, FileSize);
					}));
				}
				int32 DataFileIdx = FirstNonManifest;
				for (TFuture<FDataFileResult>& DataFileFuture : DataFileFutures)
				{
					// Grab result.
					FDataFileResult DataFileResult = DataFileFuture.Get();
					const FString& DataFilename = AllFiles[DataFileIdx];
					// Process result.
					++FilesProcessed;
					BytesProcessed += DataFileResult.FileSize;
					if (!DataFileResult.bIsReferenced)
					{
						if (!DataFileResult.bIsOldEnough)
						{
							++FilesSkipped;
							BytesSkipped += DataFileResult.FileSize;
						}
						else if (!DataFileResult.bIsRecognisedFileType)
						{
							++NonPatchFilesProcessed;
							NonPatchBytesProcessed += DataFileResult.FileSize;
						}
						else if (DataFileResult.bShouldDelete)
						{
							++FilesDeleted;
							BytesDeleted += DataFileResult.FileSize;
							if (DeletedChunkOutput.IsValid())
							{
								FString OutputLine = DataFilename + TEXT("\r\n");
								FTCHARToUTF8 UTF8String(*OutputLine);
								DeletedChunkOutput->Serialize((UTF8CHAR*)UTF8String.Get(), UTF8String.Length() * sizeof(UTF8CHAR));
							}
							UE_LOG(LogDataCompactifier, Log, TEXT("Deprecated data %s%s."), *DataFilename, Configuration.bRunPreview ? TEXT("") : TEXT(" deleted"));
						}
					}
					++DataFileIdx;
				}

				UE_LOG(LogDataCompactifier, Display, TEXT("Found %u files totalling %s."), FilesProcessed, *BuildSizeString(BytesProcessed));
				UE_LOG(LogDataCompactifier, Display, TEXT("Found %u unknown files totalling %s."), NonPatchFilesProcessed, *BuildSizeString(NonPatchBytesProcessed));
				UE_LOG(LogDataCompactifier, Display, TEXT("Deleted %u files totalling %s."), FilesDeleted, *BuildSizeString(BytesDeleted));
				UE_LOG(LogDataCompactifier, Display, TEXT("Skipped %u young files totalling %s."), FilesSkipped, *BuildSizeString(BytesSkipped));
			}
		}

		return bSuccess;
	}

	void FPatchDataCompactifier::DeleteFile(const FString& FilePath) const
	{
		if (!Configuration.bRunPreview)
		{
			FileSystem->DeleteFile(*FilePath);
		}
	}

	bool FPatchDataCompactifier::IsPatchData(const FString& FilePath) const
	{
		const TCHAR* ChunkFile = TEXT(".chunk");
		const TCHAR* DeltaFile = TEXT(".delta");
		const TCHAR* LegacyFile = TEXT(".file");
		return FilePath.EndsWith(ChunkFile) || FilePath.EndsWith(DeltaFile) || FilePath.EndsWith(LegacyFile);
	}

	FString FPatchDataCompactifier::BuildSizeString(uint64 Size) const
	{
		return FString::Printf(TEXT("%s bytes (%s, %s)"), *FText::AsNumber(Size).ToString(), *FText::AsMemory(Size, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::SI).ToString(), *FText::AsMemory(Size, &SizeFormattingOptions, nullptr, EMemoryUnitStandard::IEC).ToString());
	}

	IPatchDataCompactifier* FPatchDataCompactifierFactory::Create(const FCompactifyConfiguration& Configuration)
	{
		return new FPatchDataCompactifier(Configuration);
	}
}
