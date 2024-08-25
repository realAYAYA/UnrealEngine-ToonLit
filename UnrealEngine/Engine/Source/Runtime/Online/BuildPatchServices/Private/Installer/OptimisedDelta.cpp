// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/OptimisedDelta.h"

#include "Async/Future.h"
#include "Tasks/Task.h"
#include "Misc/ConfigCacheIni.h"

#include "Installer/DownloadService.h"
#include "Installer/InstallerError.h"
#include "BuildPatchUtil.h"
#include "BuildPatchMergeManifests.h"

DECLARE_LOG_CATEGORY_CLASS(LogOptimisedDelta, Log, All);

namespace ConfigHelpers
{
	int32 LoadDeltaRetries(int32 Min)
	{
		int32 DeltaRetries = 6;
		GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("DeltaRetries"), DeltaRetries, GEngineIni);
		DeltaRetries = FMath::Clamp<int32>(DeltaRetries, Min, 1000);
		return DeltaRetries;
	}
}

namespace BuildPatchServices
{
	class FOptimisedDelta
		: public IOptimisedDelta
	{
	public:
		FOptimisedDelta(const FOptimisedDeltaConfiguration& Configuration, FOptimisedDeltaDependencies&& Dependencies);

		// IOptimisedDelta interface begin.
		virtual const IOptimisedDelta::FResultValueOrError& GetResult() const override;
		virtual int32 GetMetaDownloadSize() const override;
		// IOptimisedDelta interface end.

	private:
		bool RequiresChunkDownload();
		void OnDownloadComplete(int32 RequestId, const FDownloadRef& Download);
		bool ShouldRetry(const FDownloadRef& Download);
		void SetResult(const FBuildPatchAppManifestPtr& ResultManifest);
		void SetFailedDownload();

	private:
		const FOptimisedDeltaConfiguration Configuration;
		IDownloadService* const DownloadService;
		const FString RelativeDeltaFilePath;
		const int32 DeltaRetries;
		TUniqueFunction<void(const IOptimisedDelta::FResultValueOrError&)> OnComplete;
		EDeltaPolicy DeltaPolicy;
		int32 CloudDirIdx;
		int32 RetryCount;
		FDownloadProgressDelegate ChunkDeltaProgress;
		FDownloadCompleteDelegate ChunkDeltaComplete;
		// TPromise and TFuture are restricted to types that are copyable, assignable, and default constructible. Value should not be exposed.
		TPromise<TSharedPtr<FResultValueOrError>> ChunkDeltaPromise;
		TFuture<TSharedPtr<FResultValueOrError>> ChunkDeltaFuture;
		FThreadSafeCounter DownloadedBytes;
		const TCHAR* ErrorCode;
	};

	FOptimisedDelta::FOptimisedDelta(const FOptimisedDeltaConfiguration& InConfiguration, FOptimisedDeltaDependencies&& InDependencies)
		: Configuration(InConfiguration)
		, DownloadService(InDependencies.DownloadService)
		, RelativeDeltaFilePath(Configuration.SourceManifest.IsValid() ? FBuildPatchUtils::GetChunkDeltaFilename(*Configuration.SourceManifest.Get(), Configuration.DestinationManifest.Get()) : TEXT(""))
		, DeltaRetries(ConfigHelpers::LoadDeltaRetries(Configuration.CloudDirectories.Num()))
		, OnComplete(MoveTemp(InDependencies.OnComplete))
		, DeltaPolicy(Configuration.DeltaPolicy)
		, CloudDirIdx(0)
		, RetryCount(0)
		, ChunkDeltaComplete(FDownloadCompleteDelegate::CreateRaw(this, &FOptimisedDelta::OnDownloadComplete))
		, ChunkDeltaPromise()
		, ChunkDeltaFuture(ChunkDeltaPromise.GetFuture())
		, DownloadedBytes(0)
		, ErrorCode(nullptr)
	{
		// There are some conditions in which we do not use a delta.
		const bool bNoSourceManifest = Configuration.SourceManifest.IsValid() == false;
		const bool bNotPatching = Configuration.InstallMode == EInstallMode::PrereqOnly;
		const bool bSameBuild = bNoSourceManifest ? false : Configuration.SourceManifest->GetBuildId() == Configuration.DestinationManifest->GetBuildId();
		const bool bNoDownload = RequiresChunkDownload() == false;
		if (bNoSourceManifest || bNotPatching || bSameBuild || bNoDownload)
		{
			DeltaPolicy = EDeltaPolicy::Skip;
		}
		// Kick off the request if we should be.
		if (DeltaPolicy != EDeltaPolicy::Skip)
		{
			UE_LOG(LogOptimisedDelta, Log, TEXT("Requesting optimised delta file %s"), *RelativeDeltaFilePath);
			DownloadService->RequestFile(Configuration.CloudDirectories[CloudDirIdx] / RelativeDeltaFilePath, ChunkDeltaComplete, ChunkDeltaProgress);
		}
		// Otherwise we provide the standard destination manifest.
		else
		{
			SetResult(Configuration.DestinationManifest);
		}
	}

	const IOptimisedDelta::FResultValueOrError& FOptimisedDelta::GetResult() const
	{
		TSharedPtr<FResultValueOrError> Result = ChunkDeltaFuture.Get();
		// Result should never be set nullptr.
		check(Result.IsValid());
		return *Result;
	}

	int32 FOptimisedDelta::GetMetaDownloadSize() const
	{
		ChunkDeltaFuture.Wait();
		return DownloadedBytes.GetValue();
	}

	bool FOptimisedDelta::RequiresChunkDownload()
	{
		if (Configuration.SourceManifest.IsValid())
		{
			for (const FString& DestinationFile : Configuration.DestinationManifest->GetBuildFileList())
			{
				// Get file manifests
				const FFileManifest* OldFile = Configuration.SourceManifest->GetFileManifest(DestinationFile);
				const FFileManifest* NewFile = Configuration.DestinationManifest->GetFileManifest(DestinationFile);
				// Different hash means different file, missing OldFile means added file.
				if (!OldFile || OldFile->FileHash != NewFile->FileHash)
				{
					return true;
				}
			}
			// No new or changed files found.
			return false;
		}
		// No source manifest means full download.
		return true;
	}

	void FOptimisedDelta::OnDownloadComplete(int32 RequestId, const FDownloadRef& Download)
	{
		UE_LOG(LogOptimisedDelta, Log, TEXT("Download complete for %s, launching comlpletion task"), *RelativeDeltaFilePath);

		// Kick a task to finish this so as not to block the callback thread
		UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, RequestId, Download] 
		{
			if (Download->ResponseSuccessful())
			{
				// Perform a merge with current manifest so that the delta can support missing out unnecessary information.
				FBuildPatchAppManifestPtr NewManifest;
				FBuildPatchAppManifestRef DeltaManifest = MakeShareable(new FBuildPatchAppManifest());
				if (DeltaManifest->DeserializeFromData(Download->GetData()))
				{
					// Verify each file received matches what we expect.
					TSet<FString> DeltaFilenameList;
					DeltaManifest->GetFileList(DeltaFilenameList);
					bool bDeltaAccepted = true;
					for (const FString& DeltaFilename : DeltaFilenameList)
					{
						const FFileManifest* OriginalFileManifest = Configuration.DestinationManifest->GetFileManifest(DeltaFilename);
						const FFileManifest* DeltaFileManifest = DeltaManifest->GetFileManifest(DeltaFilename);
						if (OriginalFileManifest != nullptr && DeltaFileManifest != nullptr)
						{
							FSHAHash OriginalFileHash;
							FSHAHash DeltaFileHash;
							Configuration.DestinationManifest->GetFileHash(DeltaFilename, OriginalFileHash);
							DeltaManifest->GetFileHash(DeltaFilename, DeltaFileHash);
							if (OriginalFileHash != DeltaFileHash)
							{
								UE_LOG(LogOptimisedDelta, Log, TEXT("Rejected optimised delta due to file SHA1 mismatch."));
								ErrorCode = DownloadErrorCodes::RejectedDeltaFile;
								bDeltaAccepted = false;
								break;
							}
						}
						else
						{
							UE_LOG(LogOptimisedDelta, Log, TEXT("Rejected optimised delta due to file list mismatch."));
							ErrorCode = DownloadErrorCodes::RejectedDeltaFile;
							bDeltaAccepted = false;
							break;
						}
					}
					// Only use if accepted so far.
					if (bDeltaAccepted)
					{
						NewManifest = FBuildMergeManifests::MergeDeltaManifest(Configuration.DestinationManifest, DeltaManifest);
						if (!NewManifest.IsValid())
						{
							UE_LOG(LogOptimisedDelta, Log, TEXT("Rejected optimised delta due to merge failure."));
							ErrorCode = DownloadErrorCodes::RejectedDeltaFile;
							bDeltaAccepted = false;
						}
					}
				}
				else
				{
					ErrorCode = DownloadErrorCodes::UnserialisableDeltaFile;
				}

				if (NewManifest.IsValid())
				{
					UE_LOG(LogOptimisedDelta, Log, TEXT("Received optimised delta file successfully %s"), *RelativeDeltaFilePath);
					DownloadedBytes.Set(Download->GetData().Num());
					SetResult(NewManifest);
				}
				else if (ShouldRetry(Download))
				{
					++RetryCount;
					UE_LOG(LogOptimisedDelta, Log, TEXT("Failed to load %s, retrying %i/%i"), *RelativeDeltaFilePath, RetryCount, DeltaRetries);
					CloudDirIdx = (CloudDirIdx + RetryCount) % Configuration.CloudDirectories.Num();
					DownloadService->RequestFile(Configuration.CloudDirectories[CloudDirIdx] / RelativeDeltaFilePath, ChunkDeltaComplete, ChunkDeltaProgress);
				}
				else
				{
					SetFailedDownload();
				}
			}
			else if (ShouldRetry(Download))
			{
				++RetryCount;
				UE_LOG(LogOptimisedDelta, Log, TEXT("Failed to download %s, retrying %i/%i"), *RelativeDeltaFilePath, RetryCount, DeltaRetries);
				CloudDirIdx = (CloudDirIdx + RetryCount) % Configuration.CloudDirectories.Num();
				DownloadService->RequestFile(Configuration.CloudDirectories[CloudDirIdx] / RelativeDeltaFilePath, ChunkDeltaComplete, ChunkDeltaProgress);
			}
			else
			{
				ErrorCode = DownloadErrorCodes::MissingDeltaFile;
				SetFailedDownload();
			}
		}, UE::Tasks::ETaskPriority::BackgroundHigh);
	}

	bool FOptimisedDelta::ShouldRetry(const FDownloadRef& Download)
	{
		// If the response code was in the 'client error' range - interpreted as we asked for something invalid, then we accept that as the
		// 'no delta' response. Any other failure reason is a server or network issue which we should retry.
		const int32 ResponseCode = Download->GetResponseCode();
		const bool bCanRetry = ResponseCode < 400 || ResponseCode >= 500;
		const bool bMayRetry = RetryCount < DeltaRetries;
		return bCanRetry && bMayRetry;
	}

	void FOptimisedDelta::SetResult(const FBuildPatchAppManifestPtr& ResultManifest)
	{
		TSharedPtr<FResultValueOrError> Result = MakeShared<FResultValueOrError>(MakeValue(ResultManifest));
		TUniqueFunction<void(const IOptimisedDelta::FResultValueOrError&)> LocalOnComplete = MoveTemp(OnComplete);

		ChunkDeltaPromise.SetValue(Result);
		if (LocalOnComplete)
		{
			LocalOnComplete(*Result);
		}
	}

	void FOptimisedDelta::SetFailedDownload()
	{
		if (DeltaPolicy == EDeltaPolicy::TryFetchContinueWithout)
		{
			UE_LOG(LogOptimisedDelta, Log, TEXT("Skipping optimised delta file."));
			SetResult(Configuration.DestinationManifest);
		}
		else
		{
			UE_LOG(LogOptimisedDelta, Log, TEXT("Failed optimised delta file fetch %s"), *RelativeDeltaFilePath);

			TSharedPtr<FResultValueOrError> Result = MakeShared<FResultValueOrError>(MakeError(ErrorCode));
			TUniqueFunction<void(const IOptimisedDelta::FResultValueOrError&)> LocalOnComplete = MoveTemp(OnComplete);

			ChunkDeltaPromise.SetValue(Result);
			if (LocalOnComplete)
			{
				LocalOnComplete(*Result);
			}
		}
	}

	FOptimisedDeltaConfiguration::FOptimisedDeltaConfiguration(FBuildPatchAppManifestRef InDestinationManifest)
		: DestinationManifest(MoveTemp(InDestinationManifest))
		, DeltaPolicy(EDeltaPolicy::TryFetchContinueWithout)
		, InstallMode(EInstallMode::NonDestructiveInstall)
	{
	}

	IOptimisedDelta* FOptimisedDeltaFactory::Create(const FOptimisedDeltaConfiguration& Configuration, FOptimisedDeltaDependencies&& Dependencies)
	{
		check(Dependencies.DownloadService != nullptr);
		return new FOptimisedDelta(Configuration, MoveTemp(Dependencies));
	}
}
