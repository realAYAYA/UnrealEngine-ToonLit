// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/CloudChunkSource.h"
#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Misc/ScopeLock.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "Core/MeanValue.h"
#include "Core/Platform.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkStore.h"
#include "Installer/DownloadService.h"
#include "Installer/InstallerError.h"
#include "Installer/DownloadConnectionCount.h"
#include "Installer/MessagePump.h"
#include "Common/StatsCollector.h"
#include "Interfaces/IBuildInstaller.h"
#include "Interfaces/IBuildInstallerSharedContext.h"
#include "BuildPatchUtil.h"
#include "Installer/Statistics/DownloadServiceStatistics.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCloudChunkSource, Log, All);
DEFINE_LOG_CATEGORY(LogCloudChunkSource);

namespace BuildPatchServices
{
	/**
	 * A class used to monitor the download success rate.
	 */
	class FChunkSuccessRate
	{
	public:
		FChunkSuccessRate();

		double GetOverall();
		double GetImmediate();
		void AddSuccess();
		void AddFail();

	private:
		double LastAverage;
		double ImmediateAccumulatedValue;
		double ImmediateValueCount;
		double TotalAccumulatedValue;
		double TotalValueCount;
	};

	FChunkSuccessRate::FChunkSuccessRate()
		: LastAverage(1.0L)
		, ImmediateAccumulatedValue(0.0L)
		, ImmediateValueCount(0.0L)
		, TotalAccumulatedValue(0.0L)
		, TotalValueCount(0.0L)
	{
	}

	double FChunkSuccessRate::GetOverall()
	{
		if (!(TotalValueCount > 0.0L))
		{
			return 0.0L;
		}
		return TotalAccumulatedValue / TotalValueCount;
	}
	double FChunkSuccessRate::GetImmediate()
	{
		static const uint32 MinimumCount = 3U;

		if (ImmediateValueCount >= MinimumCount)
		{
			LastAverage = ImmediateAccumulatedValue / ImmediateValueCount;
			ImmediateAccumulatedValue = ImmediateValueCount = 0.0L;
		}

		return LastAverage;
	}
	void FChunkSuccessRate::AddSuccess()
	{
		ImmediateAccumulatedValue += 1.0L;
		ImmediateValueCount += 1.0L;
		TotalAccumulatedValue += 1.0L;
		TotalValueCount += 1.0L;
	}

	void FChunkSuccessRate::AddFail()
	{
		ImmediateValueCount += 1.0L;
		TotalValueCount += 1.0L;
	}
	
	/**
	 * The concrete implementation of ICloudChunkSource
	 */
	class FCloudChunkSource
		: public ICloudChunkSource
	{
	private:
		/**
		 * This is a wrapper class for binding thread safe shared ptr delegates for the download service, without having to enforce that
		 * this service should be made using TShared* reference controllers.
		 */
		class FDownloadDelegates
		{
		public:
			FDownloadDelegates(FCloudChunkSource& InCloudChunkSource);

		public:
			void OnDownloadProgress(int32 RequestId, uint64 BytesSoFar);
			void OnDownloadComplete(int32 RequestId, const FDownloadRef& Download);

		private:
			FCloudChunkSource& CloudChunkSource;
		};
		
		/**
		 * This struct holds variable for each individual task.
		 */
		struct FTaskInfo
		{
		public:
			FTaskInfo();

		public:
			FString UrlUsed;
			int32 RetryNum;
			int32 ExpectedSize;
			double SecondsAtRequested;
			double SecondsAtFail;
		};

	public:
		FCloudChunkSource(FCloudSourceConfig InConfiguration, IPlatform* Platform, IChunkStore* InChunkStore, IDownloadService* InDownloadService, IChunkReferenceTracker* InChunkReferenceTracker, IChunkDataSerialization* InChunkDataSerialization, IMessagePump* InMessagePump, IInstallerError* InInstallerError, IDownloadConnectionCount* InDownloadConnectionCount, ICloudChunkSourceStat* InCloudChunkSourceStat, IBuildManifestSet* ManifestSet, TSet<FGuid> InInitialDownloadSet);
		~FCloudChunkSource();

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override;
		virtual void Abort() override;
		// IControllable interface end.

		// IChunkSource interface begin.
		virtual IChunkDataAccess* Get(const FGuid& DataId) override;
		virtual TSet<FGuid> AddRuntimeRequirements(TSet<FGuid> NewRequirements) override;
		virtual bool AddRepeatRequirement(const FGuid& RepeatRequirement) override;
		virtual void SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback) override;
		// IChunkSource interface end.

		// ICloudChunkSource interface begin.
		virtual void ThreadRun() override;
		// ICloudChunkSource interface end.

	private:
		void EnsureAquiring(const FGuid& DataId);
		const FString& GetCloudRoot(int32 RetryNum) const;
		float GetRetryDelay(int32 RetryNum);
		EBuildPatchDownloadHealth GetDownloadHealth(bool bIsDisconnected, float ChunkSuccessRate);
		FGuid GetNextTask(const TMap<FGuid, FTaskInfo>& TaskInfos, const TMap<int32, FGuid>& InFlightDownloads, const TSet<FGuid>& TotalRequiredChunks, const TSet<FGuid>& PriorityRequests, const TSet<FGuid>& FailedDownloads, const TSet<FGuid>& Stored, TArray<FGuid>& DownloadQueue, EBuildPatchDownloadHealth DownloadHealth);
		void OnDownloadProgress(int32 RequestId, uint64 BytesSoFar);
		void OnDownloadComplete(int32 RequestId, const FDownloadRef& Download);

	private:
		TSharedRef<FDownloadDelegates, ESPMode::ThreadSafe> DownloadDelegates;
		const FCloudSourceConfig Configuration;
		IPlatform* Platform;
		IChunkStore* ChunkStore;
		IDownloadService* DownloadService;
		IChunkReferenceTracker* ChunkReferenceTracker;
		IChunkDataSerialization* ChunkDataSerialization;
		IMessagePump* MessagePump;
		IInstallerError* InstallerError;
		ICloudChunkSourceStat* CloudChunkSourceStat;
		IBuildManifestSet* ManifestSet;
		const TSet<FGuid> InitialDownloadSet;
		TPromise<void> Promise;
		TFuture<void> Future;
		IBuildInstallerThread* Thread = nullptr;
		FDownloadProgressDelegate OnDownloadProgressDelegate;
		FDownloadCompleteDelegate OnDownloadCompleteDelegate;

		// Tracking health and connection state
		volatile int64 CyclesAtLastData;

		// Communication from external process requesting pause/abort.
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;

		// Communication from download thread to processing thread.
		FCriticalSection CompletedDownloadsCS;
		TMap<int32, FDownloadRef> CompletedDownloads;

		// Communication from request threads to processing thread.
		FCriticalSection RequestedDownloadsCS;
		TArray<FGuid> RequestedDownloads;

		// Communication and storage of incoming additional requirements.
		TQueue<TSet<FGuid>, EQueueMode::Mpsc> RuntimeRequestMessages;

		// Communication and storage of incoming repeat requirements.
		TQueue<FGuid, EQueueMode::Mpsc> RepeatRequirementMessages;

		// Determine if additional download requests should be initiated.
		IDownloadConnectionCount* DownloadCount;

	};

	FCloudChunkSource::FDownloadDelegates::FDownloadDelegates(FCloudChunkSource& InCloudChunkSource)
		: CloudChunkSource(InCloudChunkSource)
	{
	}

	void FCloudChunkSource::FDownloadDelegates::OnDownloadProgress(int32 RequestId, uint64 BytesSoFar)
	{
		CloudChunkSource.OnDownloadProgress(RequestId, BytesSoFar);
	}

	void FCloudChunkSource::FDownloadDelegates::OnDownloadComplete(int32 RequestId, const FDownloadRef& Download)
	{
		CloudChunkSource.OnDownloadComplete(RequestId, Download);
	}

	FCloudChunkSource::FTaskInfo::FTaskInfo()
		: UrlUsed()
		, RetryNum(0)
		, ExpectedSize(0)
		, SecondsAtFail(0)
	{
	}

	FCloudChunkSource::FCloudChunkSource(FCloudSourceConfig InConfiguration, IPlatform* InPlatform, IChunkStore* InChunkStore, IDownloadService* InDownloadService, IChunkReferenceTracker* InChunkReferenceTracker, IChunkDataSerialization* InChunkDataSerialization, IMessagePump* InMessagePump, IInstallerError* InInstallerError, IDownloadConnectionCount* InDownloadConnectionCount, ICloudChunkSourceStat* InCloudChunkSourceStat, IBuildManifestSet* InManifestSet, TSet<FGuid> InInitialDownloadSet)
		: DownloadDelegates(MakeShareable(new FDownloadDelegates(*this)))
		, Configuration(MoveTemp(InConfiguration))
		, Platform(InPlatform)
		, ChunkStore(InChunkStore)
		, DownloadService(InDownloadService)
		, ChunkReferenceTracker(InChunkReferenceTracker)
		, ChunkDataSerialization(InChunkDataSerialization)
		, MessagePump(InMessagePump)
		, InstallerError(InInstallerError)
		, CloudChunkSourceStat(InCloudChunkSourceStat)
		, ManifestSet(InManifestSet)
		, InitialDownloadSet(MoveTemp(InInitialDownloadSet))
		, Promise()
		, Future()
		, Thread(nullptr)
		, OnDownloadProgressDelegate(FDownloadProgressDelegate::CreateThreadSafeSP(DownloadDelegates, &FDownloadDelegates::OnDownloadProgress))
		, OnDownloadCompleteDelegate(FDownloadCompleteDelegate::CreateThreadSafeSP(DownloadDelegates, &FDownloadDelegates::OnDownloadComplete))
		, CyclesAtLastData(0)
		, bIsPaused(false)
		, bShouldAbort(false)
		, CompletedDownloadsCS()
		, CompletedDownloads()
		, RequestedDownloadsCS()
		, RequestedDownloads()
		, DownloadCount(InDownloadConnectionCount)
	{
		Future = Promise.GetFuture();
		if (Configuration.bRunOwnThread)
		{
			check(Configuration.SharedContext);
			Thread = Configuration.SharedContext->CreateThread();
			Thread->RunTask([this]() { ThreadRun(); });
		}
	}

	FCloudChunkSource::~FCloudChunkSource()
	{
		bShouldAbort = true;
		Future.Wait();

		if (Thread)
		{
			Configuration.SharedContext->ReleaseThread(Thread);
			Thread = nullptr;
		}
	}

	void FCloudChunkSource::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FCloudChunkSource::Abort()
	{
		bShouldAbort = true;
	}

	IChunkDataAccess* FCloudChunkSource::Get(const FGuid& DataId)
	{
		IChunkDataAccess* ChunkData = ChunkStore->Get(DataId);
		if (ChunkData == nullptr)
		{
			// Ensure this chunk is on the list.
			EnsureAquiring(DataId);
			// Wait for the chunk to be available.
			while ((ChunkData = ChunkStore->Get(DataId)) == nullptr && !bShouldAbort)
			{
				Platform->Sleep(0.01f);
			}
		}
		return ChunkData;
	}

	TSet<FGuid> FCloudChunkSource::AddRuntimeRequirements(TSet<FGuid> NewRequirements)
	{
		CloudChunkSourceStat->OnAcceptedNewRequirements(NewRequirements);
		RuntimeRequestMessages.Enqueue(MoveTemp(NewRequirements));
		// We don't have a concept of being unavailable yet.
		return TSet<FGuid>();
	}

	bool FCloudChunkSource::AddRepeatRequirement(const FGuid& RepeatRequirement)
	{
		RepeatRequirementMessages.Enqueue(RepeatRequirement);
		// We don't have a concept of being unavailable yet.
		return true;
	}

	void FCloudChunkSource::SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback)
	{
		// We don't have a concept of being unavailable yet.
	}

	void FCloudChunkSource::EnsureAquiring(const FGuid& DataId)
	{
		FScopeLock ScopeLock(&RequestedDownloadsCS);
		RequestedDownloads.Add(DataId);
	}

	const FString& FCloudChunkSource::GetCloudRoot(int32 RetryNum) const
	{
		return Configuration.CloudRoots[RetryNum % Configuration.CloudRoots.Num()];
	}

	float FCloudChunkSource::GetRetryDelay(int32 RetryNum)
	{
		const int32 RetryTimeIndex = FMath::Clamp<int32>(RetryNum - 1, 0, Configuration.RetryDelayTimes.Num() - 1);
		return Configuration.RetryDelayTimes[RetryTimeIndex];
	}

	EBuildPatchDownloadHealth FCloudChunkSource::GetDownloadHealth(bool bIsDisconnected, float ChunkSuccessRate)
	{
		EBuildPatchDownloadHealth DownloadHealth;
		if (bIsDisconnected)
		{
			DownloadHealth = EBuildPatchDownloadHealth::Disconnected;
		}
		else if (ChunkSuccessRate >= Configuration.HealthPercentages[(int32)EBuildPatchDownloadHealth::Excellent])
		{
			DownloadHealth = EBuildPatchDownloadHealth::Excellent;
		}
		else if (ChunkSuccessRate >= Configuration.HealthPercentages[(int32)EBuildPatchDownloadHealth::Good])
		{
			DownloadHealth = EBuildPatchDownloadHealth::Good;
		}
		else if (ChunkSuccessRate >= Configuration.HealthPercentages[(int32)EBuildPatchDownloadHealth::OK])
		{
			DownloadHealth = EBuildPatchDownloadHealth::OK;
		}
		else
		{
			DownloadHealth = EBuildPatchDownloadHealth::Poor;
		}
		return DownloadHealth;
	}

	FGuid FCloudChunkSource::GetNextTask(const TMap<FGuid, FTaskInfo>& TaskInfos, const TMap<int32, FGuid>& InFlightDownloads, const TSet<FGuid>& TotalRequiredChunks, const TSet<FGuid>& PriorityRequests, const TSet<FGuid>& FailedDownloads, const TSet<FGuid>& Stored, TArray<FGuid>& DownloadQueue, EBuildPatchDownloadHealth DownloadHealth)
	{
		// Check for aborting.
		if (bShouldAbort)
		{
			return FGuid();
		}

		// Check priority request.
		if (PriorityRequests.Num() > 0)
		{
			return *PriorityRequests.CreateConstIterator();
		}

		// Check retries.
		const double SecondsNow = FStatsCollector::GetSeconds();
		FGuid ChunkToRetry;
		for (auto FailedIt = FailedDownloads.CreateConstIterator(); FailedIt && !ChunkToRetry.IsValid(); ++FailedIt)
		{
			const FTaskInfo& FailedDownload = TaskInfos[*FailedIt];
			const double SecondsSinceFailure = SecondsNow - FailedDownload.SecondsAtFail;
			if (SecondsSinceFailure >= GetRetryDelay(FailedDownload.RetryNum))
			{
				ChunkToRetry = *FailedIt;
			}
		}
		if (ChunkToRetry.IsValid())
		{
			return ChunkToRetry;
		}

		// Check if we can start more.
		uint32 NumProcessing = InFlightDownloads.Num() + FailedDownloads.Num();
		const uint32 MaxDownloads = DownloadCount->GetAdjustedCount(InFlightDownloads.Num(), DownloadHealth);
		
		if ( NumProcessing < MaxDownloads)
		{
			// Find the next chunks to get if we completed the last batch.
			if (DownloadQueue.Num() == 0)
			{
				// Select the next X chunks that we initially instructed to download.
				TFunction<bool(const FGuid&)> SelectPredicate = [&TotalRequiredChunks](const FGuid& ChunkId) { return TotalRequiredChunks.Contains(ChunkId); };
				// Grab all the chunks relevant to this source to fill the store.
				int32 SearchLength = FMath::Max(ChunkStore->GetSize(), Configuration.PreFetchMinimum);
				DownloadQueue = ChunkReferenceTracker->SelectFromNextReferences(SearchLength, SelectPredicate);
				// Remove already downloading or complete chunks.
				TFunction<bool(const FGuid&)> RemovePredicate = [&TaskInfos, &FailedDownloads, &Stored](const FGuid& ChunkId) { return TaskInfos.Contains(ChunkId) || FailedDownloads.Contains(ChunkId) || Stored.Contains(ChunkId); };
				DownloadQueue.RemoveAll(RemovePredicate);
				// Clamp to configured max.
				DownloadQueue.SetNum(FMath::Min(DownloadQueue.Num(), Configuration.PreFetchMaximum), EAllowShrinking::No);
				// Reverse so the array is a stack for popping.
				Algo::Reverse(DownloadQueue);
			}

			// Return the next chunk in the queue
			if (DownloadQueue.Num() > 0)
			{
				return DownloadQueue.Pop(EAllowShrinking::No);
			}
		}

		return FGuid();
	}

	void FCloudChunkSource::ThreadRun()
	{
		TMap<FGuid, FTaskInfo> TaskInfos;
		TMap<int32, FGuid> InFlightDownloads;
		TSet<FGuid> FailedDownloads;
		TSet<FGuid> PlacedInStore;
		TSet<FGuid> PriorityRequests;
		TArray<FGuid> DownloadQueue;
		bool bDownloadsStarted = Configuration.bBeginDownloadsOnFirstGet == false;
		bool bTotalRequiredTrimmed = false;
		FMeanValue MeanChunkTime;
		FChunkSuccessRate ChunkSuccessRate;
		EBuildPatchDownloadHealth TrackedDownloadHealth = EBuildPatchDownloadHealth::Excellent;
		int32 TrackedActiveRequestCount = 0;
		TSet<FGuid> TotalRequiredChunks = InitialDownloadSet;
		uint64 TotalRequiredChunkSize = ManifestSet->GetDownloadSize(TotalRequiredChunks);
		uint64 TotalReceivedData = 0;
		uint64 RepeatRequirementSize = 0;

		// Chunk Uri Processing
		typedef TTuple<FGuid, FChunkUriResponse> FGuidUriResponse;
		typedef TQueue<FGuidUriResponse, EQueueMode::Mpsc> FGuidUriResponseQueue; // use Mpsc, message pump callback may be on this thread or message pump thread
		TSharedRef<FGuidUriResponseQueue> ChunkUriResponsesRef = MakeShared<FGuidUriResponseQueue>();
		TSet<FGuid> RequestedChunkUris;
		TMap<FGuid, FChunkUriResponse> ChunkUris;

		// Provide initial stat values.
		CloudChunkSourceStat->OnRequiredDataUpdated(TotalRequiredChunkSize + RepeatRequirementSize);
		CloudChunkSourceStat->OnReceivedDataUpdated(TotalReceivedData);
		CloudChunkSourceStat->OnDownloadHealthUpdated(TrackedDownloadHealth);
		CloudChunkSourceStat->OnSuccessRateUpdated(ChunkSuccessRate.GetOverall());
		CloudChunkSourceStat->OnActiveRequestCountUpdated(TrackedActiveRequestCount);

		while (!bShouldAbort)
		{
			bool bRequiredDataUpdated = false;
			// 'Forget' any repeat requirements.
			FGuid RepeatRequirement;
			while (RepeatRequirementMessages.Dequeue(RepeatRequirement))
			{
				if (PlacedInStore.Remove(RepeatRequirement) > 0)
				{
					RepeatRequirementSize += ManifestSet->GetDownloadSize(RepeatRequirement);
					bRequiredDataUpdated = true;
				}
			}
			// Process new runtime requests.
			TSet<FGuid> Temp;
			while (RuntimeRequestMessages.Dequeue(Temp))
			{
				Temp = Temp.Intersect(ChunkReferenceTracker->GetReferencedChunks());
				Temp = Temp.Difference(TotalRequiredChunks);
				if (Temp.Num() > 0)
				{
					TotalRequiredChunkSize += ManifestSet->GetDownloadSize(Temp);
					TotalRequiredChunks.Append(MoveTemp(Temp));
					bRequiredDataUpdated = true;
				}
			}
			// Select the next X chunks that are for downloading, so we can request URIs.
			TFunction<bool(const FGuid&)> SelectPredicate = [&TotalRequiredChunks, &RequestedChunkUris](const FGuid& ChunkId) 
			{ 
				return TotalRequiredChunks.Contains(ChunkId) && !RequestedChunkUris.Contains(ChunkId); 
			};
			TArray<FGuid> ChunkUrisToRequest = ChunkReferenceTracker->SelectFromNextReferences(Configuration.PreFetchMaximum, SelectPredicate);
			for (const FGuid& ChunkUriToRequest : ChunkUrisToRequest)
			{
				RequestedChunkUris.Add(ChunkUriToRequest);
				FChunkUriRequest ChunkUriRequest;

				const FTaskInfo* Info = TaskInfos.Find(ChunkUriToRequest);
				ChunkUriRequest.CloudDirectory = GetCloudRoot(Info ? Info->RetryNum : 0 );
				ChunkUriRequest.RelativePath = ManifestSet->GetDataFilename(ChunkUriToRequest);
				ChunkUriRequest.RelativePath.RemoveFromStart(TEXT("/"));

				MessagePump->SendRequest(ChunkUriRequest, [ChunkUriResponsesRef, ChunkUriToRequest](FChunkUriResponse Response)
				{
					ChunkUriResponsesRef->Enqueue(FGuidUriResponse(ChunkUriToRequest, MoveTemp(Response)));
				});
			}
			// Process new chunk uri responses.
			FGuidUriResponse ChunkUriResponse;
			for (FGuidUriResponseQueue& ChunkUriResponses = ChunkUriResponsesRef.Get(); ChunkUriResponses.Dequeue(ChunkUriResponse);)
			{
				ChunkUris.Add(MoveTemp(ChunkUriResponse.Get<0>()), MoveTemp(ChunkUriResponse.Get<1>()));
			}
			// Grab incoming requests as a priority.
			TArray<FGuid> FrameRequestedDownloads;
			RequestedDownloadsCS.Lock();
			FrameRequestedDownloads = MoveTemp(RequestedDownloads);
			RequestedDownloadsCS.Unlock();
			for (const FGuid& FrameRequestedDownload : FrameRequestedDownloads)
			{
				bDownloadsStarted = true;
				if (!TaskInfos.Contains(FrameRequestedDownload) && !PlacedInStore.Contains(FrameRequestedDownload))
				{
					PriorityRequests.Add(FrameRequestedDownload);
					if (!TotalRequiredChunks.Contains(FrameRequestedDownload))
					{
						TotalRequiredChunks.Add(FrameRequestedDownload);
						TotalRequiredChunkSize += ManifestSet->GetDownloadSize(FrameRequestedDownload);
						bRequiredDataUpdated = true;
					}
				}
			}
			// Trim our initial download list on first begin.
			if (!bTotalRequiredTrimmed && bDownloadsStarted)
			{
				bTotalRequiredTrimmed = true;
				TotalRequiredChunks = TotalRequiredChunks.Intersect(ChunkReferenceTracker->GetReferencedChunks());
				const int64 NewChunkSize = ManifestSet->GetDownloadSize(TotalRequiredChunks);
				if (NewChunkSize != TotalRequiredChunkSize)
				{
					TotalRequiredChunkSize = NewChunkSize;
					bRequiredDataUpdated = true;
				}
			}
			// Update required data spec.
			if (bRequiredDataUpdated)
			{
				CloudChunkSourceStat->OnRequiredDataUpdated(TotalRequiredChunkSize + RepeatRequirementSize);
			}

			// Process completed downloads.
			TMap<int32, FDownloadRef> FrameCompletedDownloads;
			CompletedDownloadsCS.Lock();
			FrameCompletedDownloads = MoveTemp(CompletedDownloads);
			CompletedDownloadsCS.Unlock();
			for (const TPair<int32, FDownloadRef>& FrameCompletedDownload : FrameCompletedDownloads)
			{
				const int32& RequestId = FrameCompletedDownload.Key;
				const FDownloadRef& Download = FrameCompletedDownload.Value;
				const FGuid& DownloadId = InFlightDownloads[RequestId];
				FTaskInfo& TaskInfo = TaskInfos.FindOrAdd(DownloadId);
				bool bDownloadSuccess = Download->ResponseSuccessful();
				if (bDownloadSuccess)
				{
					// HTTP module gives const access to downloaded data, and we need to change it.
					// @TODO: look into refactor serialization it can already know SHA list? Or consider adding SHA params to public API.
					TArray<uint8> DownloadedData = Download->GetData();

					// If we know the SHA for this chunk, inject to data for verification.
					FSHAHash ChunkShaHash;
					if (ManifestSet->GetChunkShaHash(DownloadId, ChunkShaHash))
					{
						ChunkDataSerialization->InjectShaToChunkData(DownloadedData, ChunkShaHash);
					}

					EChunkLoadResult LoadResult;
					TUniquePtr<IChunkDataAccess> ChunkDataAccess(ChunkDataSerialization->LoadFromMemory(DownloadedData, LoadResult));
					bDownloadSuccess = LoadResult == EChunkLoadResult::Success;
					if (bDownloadSuccess)
					{
						TotalReceivedData += TaskInfo.ExpectedSize;
						TaskInfos.Remove(DownloadId);
						PlacedInStore.Add(DownloadId);
						ChunkStore->Put(DownloadId, MoveTemp(ChunkDataAccess));
						CloudChunkSourceStat->OnDownloadSuccess(DownloadId);
						CloudChunkSourceStat->OnReceivedDataUpdated(TotalReceivedData);
					}
					else
					{
						CloudChunkSourceStat->OnDownloadCorrupt(DownloadId, TaskInfo.UrlUsed, LoadResult);
						UE_LOG(LogCloudChunkSource, Error, TEXT("CORRUPT: %s"), *TaskInfo.UrlUsed);
					}
				}
				else
				{
					CloudChunkSourceStat->OnDownloadFailed(DownloadId, TaskInfo.UrlUsed);
					UE_LOG(LogCloudChunkSource, Error, TEXT("FAILED: %s"), *TaskInfo.UrlUsed);
				}

				// Handle failed
				if (!bDownloadSuccess)
				{
					ChunkSuccessRate.AddFail();
					FailedDownloads.Add(DownloadId);
					if (Configuration.MaxRetryCount >= 0 && TaskInfo.RetryNum >= Configuration.MaxRetryCount)
					{
						InstallerError->SetError(EBuildPatchInstallError::DownloadError, DownloadErrorCodes::OutOfChunkRetries);
						bShouldAbort = true;
					}
					++TaskInfo.RetryNum;
					TaskInfo.SecondsAtFail = FStatsCollector::GetSeconds();

					RequestedChunkUris.Remove(DownloadId);
					ChunkUris.Remove(DownloadId);
				}
				else
				{
					const double ChunkTime = FStatsCollector::GetSeconds() - TaskInfo.SecondsAtRequested;
					MeanChunkTime.AddSample(ChunkTime);
					ChunkSuccessRate.AddSuccess();
				}
				InFlightDownloads.Remove(RequestId);
			}

			// Update connection status and health.
			bool bAllDownloadsRetrying = FailedDownloads.Num() > 0 || InFlightDownloads.Num() > 0;
			for (auto InFlightIt = InFlightDownloads.CreateConstIterator(); InFlightIt && bAllDownloadsRetrying; ++InFlightIt)
			{
				if (TaskInfos.FindOrAdd(InFlightIt.Value()).RetryNum == 0)
				{
					bAllDownloadsRetrying = false;
				}
			}
			const double SecondsSinceData = FStatsCollector::CyclesToSeconds(FStatsCollector::GetCycles() - CyclesAtLastData);
			const bool bDisconnect = (bAllDownloadsRetrying && SecondsSinceData > Configuration.DisconnectedDelay);
			const float SuccessRate = ChunkSuccessRate.GetOverall();
			EBuildPatchDownloadHealth OverallDownloadHealth = GetDownloadHealth(bDisconnect, SuccessRate);
			if (TrackedDownloadHealth != OverallDownloadHealth)
			{
				TrackedDownloadHealth = OverallDownloadHealth;
				CloudChunkSourceStat->OnDownloadHealthUpdated(TrackedDownloadHealth);
			}
			if (FrameCompletedDownloads.Num() > 0)
			{
				CloudChunkSourceStat->OnSuccessRateUpdated(SuccessRate);
			}
			const float ImmediateSuccessRate = ChunkSuccessRate.GetImmediate();
			EBuildPatchDownloadHealth ImmediateDownloadHealth = GetDownloadHealth(bDisconnect, ImmediateSuccessRate);
			// Kick off new downloads.
			if (bDownloadsStarted)
			{
				FGuid NextTask;
				while ((NextTask = GetNextTask(TaskInfos, InFlightDownloads, TotalRequiredChunks, PriorityRequests, FailedDownloads, PlacedInStore, DownloadQueue, ImmediateDownloadHealth)).IsValid())
				{
					FChunkUriResponse* ChunkUri = ChunkUris.Find(NextTask);
					if (ChunkUri)
					{
						FTaskInfo& TaskInfo = TaskInfos.FindOrAdd(NextTask);
						TaskInfo.UrlUsed = ChunkUri->Uri;
						TaskInfo.ExpectedSize = ManifestSet->GetDownloadSize(NextTask);
						TaskInfo.SecondsAtRequested = FStatsCollector::GetSeconds();
						int32 RequestId = DownloadService->RequestFile(TaskInfo.UrlUsed, OnDownloadCompleteDelegate, OnDownloadProgressDelegate);
						InFlightDownloads.Add(RequestId, NextTask);
						PriorityRequests.Remove(NextTask);
						FailedDownloads.Remove(NextTask);
						CloudChunkSourceStat->OnDownloadRequested(NextTask);
					}
					else
					{
						break;
					}
				}
			}

			// Update request count.
			int32 ActiveRequestCount = InFlightDownloads.Num() + FailedDownloads.Num();;
			if (TrackedActiveRequestCount != ActiveRequestCount)
			{
				TrackedActiveRequestCount = ActiveRequestCount;
				CloudChunkSourceStat->OnActiveRequestCountUpdated(TrackedActiveRequestCount);
			}

			// Check for abnormally slow downloads. This was originally implemented as a temporary measure to fix major stall anomalies and zero size tcp window issue.
			// It remains until proven unrequired.
			if (MeanChunkTime.IsReliable())
			{
				bool bResetMeanChunkTime = false;
				for (const TPair<int32, FGuid>& InFlightDownload : InFlightDownloads)
				{
					FTaskInfo& TaskInfo = TaskInfos.FindOrAdd(InFlightDownload.Value);
					if (TaskInfo.RetryNum == 0)
					{
						double DownloadTime = FStatsCollector::GetSeconds() - TaskInfo.SecondsAtRequested;
						double DownloadTimeMean, DownloadTimeStd;
						MeanChunkTime.GetValues(DownloadTimeMean, DownloadTimeStd);
						// The point at which we decide the chunk is delayed, with a sane minimum
						double BreakingPoint = FMath::Max<double>(Configuration.TcpZeroWindowMinimumSeconds, DownloadTimeMean + (DownloadTimeStd * 4.0));
						if (DownloadTime > BreakingPoint && TaskInfo.UrlUsed.EndsWith(TEXT(".chunk")))
						{
							bResetMeanChunkTime = true;
							DownloadService->RequestCancel(InFlightDownload.Key);
							CloudChunkSourceStat->OnDownloadAborted(InFlightDownload.Value, TaskInfo.UrlUsed, DownloadTimeMean, DownloadTimeStd, DownloadTime, BreakingPoint);
						}
					}
				}
				if (bResetMeanChunkTime)
				{
					MeanChunkTime.Reset();
				}
			}

			// Wait while paused
			while (bIsPaused && !bShouldAbort)
			{
				Platform->Sleep(0.1f);
			}

			// Give other threads some time.
			Platform->Sleep(0.01f);
		}

		// Abandon in flight downloads if should abort.
		if (bShouldAbort)
		{
			for (const TPair<int32, FGuid>& InFlightDownload : InFlightDownloads)
			{
				DownloadService->RequestAbandon(InFlightDownload.Key);
			}
		}

		// Provide final stat values.
		CloudChunkSourceStat->OnDownloadHealthUpdated(TrackedDownloadHealth);
		CloudChunkSourceStat->OnSuccessRateUpdated(ChunkSuccessRate.GetOverall());
		CloudChunkSourceStat->OnActiveRequestCountUpdated(0);

		// The promise should always be set, even if not needed as destruction of an unset promise will assert.
		Promise.SetValue();
	}

	void FCloudChunkSource::OnDownloadProgress(int32 RequestId, uint64 BytesSoFar)
	{
		FPlatformAtomics::InterlockedExchange(&CyclesAtLastData, FStatsCollector::GetCycles());
	}

	void FCloudChunkSource::OnDownloadComplete(int32 RequestId, const FDownloadRef& Download)
	{
		FScopeLock ScopeLock(&CompletedDownloadsCS);
		CompletedDownloads.Add(RequestId, Download);
	}

	ICloudChunkSource* FCloudChunkSourceFactory::Create(FCloudSourceConfig Configuration, IPlatform* Platform, IChunkStore* ChunkStore, IDownloadService* DownloadService, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, IMessagePump* MessagePump, IInstallerError* InstallerError, IDownloadConnectionCount* ConnectionCount, ICloudChunkSourceStat* CloudChunkSourceStat, IBuildManifestSet* ManifestSet, TSet<FGuid> InitialDownloadSet)
	{
		UE_LOG(LogCloudChunkSource, Verbose, TEXT("FCloudChunkSourceFactory::Create for %d roots"), Configuration.CloudRoots.Num());

		check(Platform != nullptr);
		check(ChunkStore != nullptr);
		check(DownloadService != nullptr);
		check(ChunkReferenceTracker != nullptr);
		check(ChunkDataSerialization != nullptr);
		check(MessagePump != nullptr);
		check(InstallerError != nullptr);
		check(ConnectionCount != nullptr)
		check(CloudChunkSourceStat != nullptr);
		return new FCloudChunkSource(MoveTemp(Configuration), Platform, ChunkStore, DownloadService, ChunkReferenceTracker, ChunkDataSerialization, MessagePump, InstallerError, ConnectionCount, CloudChunkSourceStat, ManifestSet, MoveTemp(InitialDownloadSet));
	}
}
