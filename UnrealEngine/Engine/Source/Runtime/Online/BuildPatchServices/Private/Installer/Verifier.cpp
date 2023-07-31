// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/Verifier.h"
#include "HAL/ThreadSafeBool.h"
#include "Common/StatsCollector.h"
#include "Common/FileSystem.h"
#include "BuildPatchVerify.h"
#include "BuildPatchUtil.h"
#include "IBuildManifestSet.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVerifier, Warning, All);
DEFINE_LOG_CATEGORY(LogVerifier);

namespace BuildPatchServices
{
	// 4MB Read buffer.
	const int32 VerifyBufferSize = 1024 * 1024 * 4;

	bool TryConvertToVerifyResult(EVerifyError InVerifyError, EVerifyResult& OutVerifyResult)
	{
		switch (InVerifyError)
		{
			case EVerifyError::FileMissing: OutVerifyResult = EVerifyResult::FileMissing; return true;
			case EVerifyError::OpenFileFailed: OutVerifyResult = EVerifyResult::OpenFileFailed; return true;
			case EVerifyError::HashCheckFailed: OutVerifyResult = EVerifyResult::HashCheckFailed; return true;
			case EVerifyError::FileSizeFailed: OutVerifyResult = EVerifyResult::FileSizeFailed; return true;
		}
		return false;
	}

	bool TryConvertToVerifyError(EVerifyResult InVerifyResult, EVerifyError& OutVerifyError)
	{
		switch (InVerifyResult)
		{
			case EVerifyResult::FileMissing: OutVerifyError = EVerifyError::FileMissing; return true;
			case EVerifyResult::OpenFileFailed: OutVerifyError = EVerifyError::OpenFileFailed; return true;
			case EVerifyResult::HashCheckFailed: OutVerifyError = EVerifyError::HashCheckFailed; return true;
			case EVerifyResult::FileSizeFailed: OutVerifyError = EVerifyError::FileSizeFailed; return true;
		}
		return false;
	}

	class FVerifier : public IVerifier
	{
	public:
		FVerifier(IFileSystem* FileSystem, IVerifierStat* InVerificationStat, EVerifyMode InVerifyMode, IBuildManifestSet* InManifestSet, FString InVerifyDirectory, FString InStagedFileDirectory);
		~FVerifier() {}

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override;
		virtual void Abort() override;
		// IControllable interface end.

		// IVerifier interface begin.
		virtual EVerifyResult Verify(TArray<FString>& CorruptFiles) override;
		virtual void AddTouchedFiles(const TSet<FString>& TouchedFiles) override;
		// IVerifier interface end.

	private:
		FString SelectFullFilePath(const FString& BuildFile);
		EVerifyResult VerfiyFileSha(const FString& BuildFile, const FFileManifest& BuildFileManifest);
		EVerifyResult VerfiyFileSize(const FString& BuildFile, const FFileManifest& BuildFileManifest);

	private:
		const FString VerifyDirectory;
		const FString StagedFileDirectory;
		IFileSystem* const FileSystem;
		IVerifierStat* const VerifierStat;
		IBuildManifestSet* const ManifestSet;

		TArray<uint8> FileReadBuffer;
		EVerifyMode VerifyMode;
		TSet<FString> FilesToVerify;
		TSet<FString> FilesPassedVerify;
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;
		int64 ProcessedBytes;
	};

	FVerifier::FVerifier(IFileSystem* InFileSystem, IVerifierStat* InVerificationStat, EVerifyMode InVerifyMode, IBuildManifestSet* InManifestSet, FString InVerifyDirectory, FString InStagedFileDirectory)
		: VerifyDirectory(MoveTemp(InVerifyDirectory))
		, StagedFileDirectory(MoveTemp(InStagedFileDirectory))
		, FileSystem(InFileSystem)
		, VerifierStat(InVerificationStat)
		, ManifestSet(InManifestSet)
		, VerifyMode(InVerifyMode)
		, bIsPaused(false)
		, bShouldAbort(false)
		, ProcessedBytes(0)
	{
		ManifestSet->GetFilesTaggedForRepair(FilesToVerify);
		FileReadBuffer.Reserve(VerifyBufferSize);
		FileReadBuffer.AddUninitialized(VerifyBufferSize);
	}

	void FVerifier::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FVerifier::Abort()
	{
		bShouldAbort = true;
	}

	EVerifyResult FVerifier::Verify(TArray<FString>& CorruptFiles)
	{
		bShouldAbort = false;
		EVerifyResult VerifyResult = EVerifyResult::Success;
		CorruptFiles.Empty();

		// If we check all files, grab them all now.
		if (VerifyMode == EVerifyMode::FileSizeCheckAllFiles || VerifyMode == EVerifyMode::ShaVerifyAllFiles)
		{
			ManifestSet->GetExpectedFiles(FilesToVerify);
		}

		// Setup progress tracking.
		TSet<FString> VerifyList = FilesToVerify.Difference(FilesPassedVerify);
		VerifierStat->OnProcessedDataUpdated(0);
		VerifierStat->OnTotalRequiredUpdated(ManifestSet->GetTotalNewFileSize(VerifyList));

		// Select verify function.
		const bool bVerifyShaMode = VerifyMode == EVerifyMode::ShaVerifyAllFiles || VerifyMode == EVerifyMode::ShaVerifyTouchedFiles;

		// For each required file, perform the selected verification.
		ProcessedBytes = 0;
		for (const FString& BuildFile : VerifyList)
		{
			// Break if quitting
			if (bShouldAbort)
			{
				break;
			}

			// Get file details.
			const FFileManifest* BuildFileManifest = ManifestSet->GetNewFileManifest(BuildFile);

			// Verify the file.
			const bool bVerifySha = bVerifyShaMode || ManifestSet->IsFileRepairAction(BuildFile);
			VerifierStat->OnFileStarted(BuildFile, BuildFileManifest->FileSize);
			EVerifyResult FileVerifyResult = bVerifySha ? VerfiyFileSha(BuildFile, *BuildFileManifest) : VerfiyFileSize(BuildFile, *BuildFileManifest);
			VerifierStat->OnFileCompleted(BuildFile, FileVerifyResult);
			if (FileVerifyResult != EVerifyResult::Success)
			{
				CorruptFiles.Add(BuildFile);
				if (VerifyResult == EVerifyResult::Success)
				{
					VerifyResult = FileVerifyResult;
				}
			}
			// If success, and it was an SHA verify, cache the result so we don't repeat an SHA verify.
			else if (bVerifySha)
			{
				FilesPassedVerify.Add(BuildFile);
			}
		}

		if (bShouldAbort && VerifyResult == EVerifyResult::Success)
		{
			VerifyResult = EVerifyResult::Aborted;
		}

		return VerifyResult;
	}

	void FVerifier::AddTouchedFiles(const TSet<FString>& TouchedFiles)
	{
		FilesToVerify.Append(TouchedFiles);
		FilesPassedVerify = FilesPassedVerify.Difference(TouchedFiles);
	}

	FString FVerifier::SelectFullFilePath(const FString& BuildFile)
	{
		FString FullFilePath;
		if (StagedFileDirectory.IsEmpty() == false)
		{
			FullFilePath = StagedFileDirectory / BuildFile;
			int64 FileSize;
			if (FileSystem->GetFileSize(*FullFilePath, FileSize))
			{
				return FullFilePath;
			}
		}
		FullFilePath = VerifyDirectory / BuildFile;
		return FullFilePath;
	}

	EVerifyResult FVerifier::VerfiyFileSha(const FString& BuildFile, const FFileManifest& BuildFileManifest)
	{
		ISpeedRecorder::FRecord ActivityRecord;
		const int64 PrevProcessedBytes = ProcessedBytes;
		EVerifyResult VerifyResult;
		const FString FileToVerify = SelectFullFilePath(BuildFile);
		uint8 ReturnValue = 0;
		TUniquePtr<FArchive> FileReader = FileSystem->CreateFileReader(*FileToVerify);
		VerifierStat->OnFileProgress(BuildFile, 0);
		if (FileReader.IsValid())
		{
			const int64 FileSize = FileReader->TotalSize();
			if (FileSize != BuildFileManifest.FileSize)
			{
				VerifyResult = EVerifyResult::FileSizeFailed;
			}
			else
			{
				FSHA1 HashState;
				FSHAHash HashValue;
				while (!FileReader->AtEnd() && !bShouldAbort)
				{
					// Pause if necessary
					while (bIsPaused && !bShouldAbort)
					{
						FPlatformProcess::Sleep(0.1f);
					}
					ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
					// Read file and update hash state
					const int64 SizeLeft = FileSize - FileReader->Tell();
					ActivityRecord.Size = FMath::Min<int64>(VerifyBufferSize, SizeLeft);
					FileReader->Serialize(FileReadBuffer.GetData(), ActivityRecord.Size);
					HashState.Update(FileReadBuffer.GetData(), ActivityRecord.Size);
					ProcessedBytes = PrevProcessedBytes + FileReader->Tell();
					ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
					VerifierStat->OnFileRead(ActivityRecord);
					VerifierStat->OnFileProgress(BuildFile, FileReader->Tell());
					VerifierStat->OnProcessedDataUpdated(ProcessedBytes);
				}
				HashState.Final();
				HashState.GetHash(HashValue.Hash);
				if (HashValue == BuildFileManifest.FileHash)
				{
					VerifyResult = EVerifyResult::Success;
				}
				else if (!bShouldAbort)
				{
					VerifyResult = EVerifyResult::HashCheckFailed;
				}
				else
				{
					VerifyResult = EVerifyResult::Aborted;
				}
			}
			FileReader->Close();
		}
		else if (FileSystem->FileExists(*FileToVerify))
		{
			VerifyResult = EVerifyResult::OpenFileFailed;
		}
		else
		{
			VerifyResult = EVerifyResult::FileMissing;
		}
		ProcessedBytes = PrevProcessedBytes + BuildFileManifest.FileSize;
		if (VerifyResult != EVerifyResult::Success)
		{
			VerifierStat->OnFileProgress(BuildFile, BuildFileManifest.FileSize);
			VerifierStat->OnProcessedDataUpdated(ProcessedBytes);
		}

		return VerifyResult;
	}

	EVerifyResult FVerifier::VerfiyFileSize(const FString& BuildFile, const FFileManifest& BuildFileManifest)
	{
		// Pause if necessary.
		const double PrePauseTime = FStatsCollector::GetSeconds();
		double PostPauseTime = PrePauseTime;
		while (bIsPaused && !bShouldAbort)
		{
			FPlatformProcess::Sleep(0.1f);
			PostPauseTime = FStatsCollector::GetSeconds();
		}
		VerifierStat->OnFileProgress(BuildFile, 0);
		int64 FileSize;
		EVerifyResult VerifyResult;
		if (FileSystem->GetFileSize(*SelectFullFilePath(BuildFile), FileSize))
		{
			if (FileSize == BuildFileManifest.FileSize)
			{
				VerifyResult = EVerifyResult::Success;
			}
			else
			{
				VerifyResult = EVerifyResult::FileSizeFailed;
			}
		}
		else
		{
			VerifyResult = EVerifyResult::FileMissing;
		}
		VerifierStat->OnFileProgress(BuildFile, BuildFileManifest.FileSize);
		ProcessedBytes += BuildFileManifest.FileSize;
		VerifierStat->OnProcessedDataUpdated(ProcessedBytes);
		return VerifyResult;
	}

	IVerifier* FVerifierFactory::Create(IFileSystem* FileSystem, IVerifierStat* VerifierStat, EVerifyMode VerifyMode, IBuildManifestSet* ManifestSet, FString VerifyDirectory, FString StagedFileDirectory)
	{
		check(FileSystem != nullptr);
		check(VerifierStat != nullptr);
		return new FVerifier(FileSystem, VerifierStat, VerifyMode, ManifestSet, MoveTemp(VerifyDirectory), MoveTemp(StagedFileDirectory));
	}
}
