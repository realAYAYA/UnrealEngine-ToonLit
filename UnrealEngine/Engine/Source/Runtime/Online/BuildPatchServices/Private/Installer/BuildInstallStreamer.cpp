// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/BuildInstallStreamer.h"

#include "Algo/RemoveIf.h"
#include "BuildPatchSettings.h"
#include "Common/ChunkDataSizeProvider.h"
#include "Common/FileSystem.h"
#include "Common/HttpManager.h"
#include "Core/AsyncHelpers.h"
#include "Core/Platform.h"
#include "Core/ProcessTimer.h"
#include "IBuildManifestSet.h"
#include "Installer/CloudChunkSource.h"
#include "Installer/ChunkEvictionPolicy.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/DownloadService.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/InstallerError.h"
#include "Installer/MemoryChunkStore.h"
#include "Installer/MessagePump.h"
#include "Installer/Statistics/CloudChunkSourceStatistics.h"
#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Installer/Statistics/FileConstructorStatistics.h"
#include "Installer/Statistics/FileOperationTracker.h"
#include "Installer/Statistics/MemoryChunkStoreStatistics.h"
#include "Installer/VirtualFileConstructor.h"
#include "VirtualFileCache.h"
#include "ProfilingDebugging/CsvProfiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogBuildInstallStreamer, Log, All);

CSV_DEFINE_CATEGORY(CosmeticStreamingCsv, true);

namespace BuildPatchServices
{
	namespace
	{
		FDownloadConnectionCountConfig BuildConnectionCountConfig()
		{
			// TODO: Configure for scaling, move installer's config builder to shared code?
			FDownloadConnectionCountConfig Config;
			Config.bDisableConnectionScaling = true;
			return Config;
		}
	}

	FBuildInstallStreamer::FBuildInstallStreamer(FBuildInstallStreamerConfiguration InConfiguration)
		: Configuration(MoveTemp(InConfiguration))
		, BuildPatchManifest(StaticCastSharedPtr<FBuildPatchAppManifest>(Configuration.Manifest))
		, ManifestSet(FBuildManifestSetFactory::Create({ FBuildPatchInstallerAction(FInstallerAction::MakeInstall(Configuration.Manifest.ToSharedRef())) }))
		, HttpManager(FHttpManagerFactory::Create())
		, FileSystem(FFileSystemFactory::Create())
		, ChunkDataSerialization(FChunkDataSerializationFactory::Create(FileSystem.Get()))
		, Platform(FPlatformFactory::Create())
		, InstallerAnalytics(FInstallerAnalyticsFactory::Create(nullptr))
		, InstallerError(FInstallerErrorFactory::Create())
		, FileOperationTracker(FFileOperationTrackerFactory::Create(FTSTicker::GetCoreTicker()))
		, DownloadSpeedRecorder(FSpeedRecorderFactory::Create())
		, FileReadSpeedRecorder(FSpeedRecorderFactory::Create())
		, FileWriteSpeedRecorder(FSpeedRecorderFactory::Create())
		, ChunkDataSizeProvider(FChunkDataSizeProviderFactory::Create())
		, DownloadServiceStatistics(FDownloadServiceStatisticsFactory::Create(DownloadSpeedRecorder.Get(), ChunkDataSizeProvider.Get(), InstallerAnalytics.Get()))
		, MemoryChunkStoreStatistics(FMemoryChunkStoreStatisticsFactory::Create(FileOperationTracker.Get()))
		, CloudChunkSourceStatistics(FCloudChunkSourceStatisticsFactory::Create(InstallerAnalytics.Get(), &BuildProgress, FileOperationTracker.Get()))
		, FileConstructorStatistics(FFileConstructorStatisticsFactory::Create(FileReadSpeedRecorder.Get(), FileWriteSpeedRecorder.Get(), &BuildProgress, FileOperationTracker.Get()))
		, DownloadConnectionCount(FDownloadConnectionCountFactory::Create(BuildConnectionCountConfig(), DownloadServiceStatistics.Get()))
		, DownloadService(FDownloadServiceFactory::Create(HttpManager.Get(), FileSystem.Get(), DownloadServiceStatistics.Get(), InstallerAnalytics.Get()))
		, MessagePump(FMessagePumpFactory::Create())
		, bIsShuttingDown(false)
		, RequestTrigger(FPlatformProcess::GetSynchEventFromPool(false))
		, CloudTrigger(FPlatformProcess::GetSynchEventFromPool(false))
		, StreamerStats(FBuildInstallStreamerStats())
		, PerSessionStreamerStats(FBuildInstallStreamerStats())
		
	{
		RequestWorker = Async(EAsyncExecution::ThreadIfForkSafe, [this]()
			{
				RequestWorkerThread();
			});
		CloudWorker = Async(EAsyncExecution::ThreadIfForkSafe, [this]()
			{
				CloudWorkerThread();
			});
	}

	FBuildInstallStreamer::~FBuildInstallStreamer()
	{
		// If we destruct on the main thread we need to use the PreExit functionality to properly close down threads.
		if (IsInGameThread())
		{
			PreExit();
		}
		else
		{
			bIsShuttingDown = true;
			InstallerError->SetError(EBuildPatchInstallError::ApplicationClosing, ApplicationClosedErrorCodes::ApplicationClosed);
			RequestTrigger->Trigger();
			CloudTrigger->Trigger();
			RequestWorker.Wait();
			CloudWorker.Wait();
		}
		FPlatformProcess::ReturnSynchEventToPool(RequestTrigger);
		FPlatformProcess::ReturnSynchEventToPool(CloudTrigger);
	}

	void FBuildInstallStreamer::QueueFilesByTag(TSet<FString> Tags, FBuildPatchStreamCompleteDelegate OnComplete)
	{
		check(!bIsShuttingDown);
		UE_LOG(LogBuildInstallStreamer, Verbose, TEXT("Receive request %s"), *FString::Join(Tags, TEXT(",")));
		RequestQueue.Enqueue({ MoveTemp(Tags), true, MoveTemp(OnComplete) });
		RequestTrigger->Trigger();
	}

	void FBuildInstallStreamer::QueueFilesByName(TSet<FString> Files, FBuildPatchStreamCompleteDelegate OnComplete)
	{
		check(!bIsShuttingDown);
		UE_LOG(LogBuildInstallStreamer, Verbose, TEXT("Receive request %s"), *FString::Join(Files, TEXT(",")));
		RequestQueue.Enqueue({ MoveTemp(Files), false, MoveTemp(OnComplete) });
		RequestTrigger->Trigger();
	}

	void FBuildInstallStreamer::RegisterMessageHandler(FMessageHandler* MessageHandler)
	{
	}

	void FBuildInstallStreamer::UnregisterMessageHandler(FMessageHandler* MessageHandler)
	{
	}

	const FBuildInstallStreamerConfiguration& FBuildInstallStreamer::GetConfiguration() const
	{
		return Configuration;
	}

	const FBuildInstallStreamerStats& FBuildInstallStreamer::GetInstallStreamerStatistics() const
	{
		return StreamerStats;
	}
	
	const FBuildInstallStreamerStats& FBuildInstallStreamer::GetInstallStreamerSessionStatistics() const
	{
		return PerSessionStreamerStats;
	}
	
	void FBuildInstallStreamer::ResetSessionStatistics()
	{
		PerSessionStreamerStats = FBuildInstallStreamerStats();
	}

	void FBuildInstallStreamer::Initialise()
	{
		// update to chunk data size cache
		ChunkDataSizeProvider->AddManifestData(BuildPatchManifest);
	}

	void FBuildInstallStreamer::RequestWorkerThread()
	{
		Initialise();

		TArray<FStreamRequest> Requests;
		double RequestTime = 0;

		auto CompleteRequest = [this, &Requests, &RequestTime]()
		{
			uint64 TotalDownload = DownloadServiceStatistics->GetBytesDownloaded();
			uint64 PreviousTotalRequestsCompleted = StreamerStats.BundleRequestsCompleted + StreamerStats.FileRequestsCompleted;
			uint64 PreviousTotalRequestsCompletedForSession = PerSessionStreamerStats.BundleRequestsCompleted + PerSessionStreamerStats.FileRequestsCompleted;
			for (FStreamRequest& Request : Requests)
			{
				const bool bIsTagRequest = Request.Get<1>();
				FBuildPatchStreamResult Result;
				Result.Request = MoveTemp(Request.Get<0>());
				Result.ErrorType = InstallerError->GetErrorType();
				Result.ErrorCode = InstallerError->GetErrorCode();
				// TODO: Not accurate.. How to fix this
				Result.TotalDownloaded = TotalDownload;
				
				// if (bIsTagRequest) StreamerStats.BundleMegaBytesDownloaded += (Result.TotalDownloaded / 1024.0f / 1024.0f);
				// else StreamerStats.FileMegaBytesDownloaded += (Result.TotalDownloaded / 1024.0f / 1024.0f);
				
				if (bIsTagRequest)
				{
					StreamerStats.BundleRequestsCompleted++;
					PerSessionStreamerStats.BundleRequestsCompleted++;
				}
				else
				{
					StreamerStats.FileRequestsCompleted++;
					PerSessionStreamerStats.FileRequestsCompleted++;
				}

				UE_LOG(LogBuildInstallStreamer, Verbose, TEXT("Completed request %s in %s"), *FString::Join(Result.Request, TEXT(",")), *FPlatformTime::PrettyTime(RequestTime));
				Request.Get<2>().ExecuteIfBound(MoveTemp(Result));
			}
			StreamerStats.TotalMegaBytesDownloaded += (TotalDownload / 1024.0f / 1024.0f);
			PerSessionStreamerStats.TotalMegaBytesDownloaded += (TotalDownload / 1024.0f / 1024.0f);
				
			if (RequestTime > StreamerStats.MaxRequestTime) StreamerStats.MaxRequestTime = RequestTime;
			if (RequestTime > PerSessionStreamerStats.MaxRequestTime) PerSessionStreamerStats.MaxRequestTime = RequestTime;
				
			StreamerStats.AverageRequestTime = ((StreamerStats.AverageRequestTime * PreviousTotalRequestsCompleted) + RequestTime) /
				(StreamerStats.BundleRequestsCompleted + StreamerStats.FileRequestsCompleted);
				
			PerSessionStreamerStats.AverageRequestTime = ((PerSessionStreamerStats.AverageRequestTime * PreviousTotalRequestsCompletedForSession) + RequestTime) /
				(PerSessionStreamerStats.BundleRequestsCompleted + PerSessionStreamerStats.FileRequestsCompleted);
		};

		auto CancelRequest = [this, &Requests]()
		{
			for (FStreamRequest& Request : Requests)
			{
				const bool bIsTagRequest = Request.Get<1>();
				FBuildPatchStreamResult Result;
				Result.Request = MoveTemp(Request.Get<0>());
				Result.ErrorType = InstallerError->GetErrorType();
				Result.ErrorCode = InstallerError->GetErrorCode();
				Result.TotalDownloaded = 0;
				if (bIsTagRequest)
				{
					StreamerStats.BundleRequestsCancelled++;
					PerSessionStreamerStats.BundleRequestsCancelled++;
				}
				else
				{
					StreamerStats.FileRequestsCancelled++;
					PerSessionStreamerStats.FileRequestsCancelled++;
				}
				UE_LOG(LogBuildInstallStreamer, Verbose, TEXT("Cancelled request %s"), *FString::Join(Result.Request, TEXT(",")));
				Request.Get<2>().ExecuteIfBound(MoveTemp(Result));
			}
		};

		while (!bIsShuttingDown)
		{
			// If we were asked to cancel all requests, then do this first
			if (InstallerError->IsCancelled())
			{
				
				while (RequestQueue.Dequeue(Requests.AddDefaulted_GetRef())) {}
				Requests.Pop();
				if (Requests.Num() > 0)
				{
					CallOnDelegateThread(CancelRequest);
					Requests.Empty();
				}
			}

			// Get queued requests to process.
			if (Configuration.bShouldBatch)
			{
				while (RequestQueue.Dequeue(Requests.AddDefaulted_GetRef())) {}
				Requests.Pop();
			}
			else if(!RequestQueue.Dequeue(Requests.AddDefaulted_GetRef()))
			{
				Requests.Pop();
			}
			// Run the streaming for received requests.
			if (Requests.Num() > 0)
			{
				for (const FStreamRequest& Request : Requests)
				{
					if (Request.Get<1>())
					{
						StreamerStats.BundleRequestsMade++;
						PerSessionStreamerStats.BundleRequestsMade++;
					}
					else
					{
						StreamerStats.FileRequestsMade++;
						PerSessionStreamerStats.FileRequestsMade++;
					}
					UE_LOG(LogBuildInstallStreamer, Verbose, TEXT("Begin request %s"), *FString::Join(Request.Get<0>(), TEXT(",")));
				}

				TProcessTimer<class FStatsCollector, false> RequestTimer;
				RequestTimer.Start();
				DownloadServiceStatistics->Reset();
				TSharedPtr<IVirtualFileCache, ESPMode::ThreadSafe> VirtualFileCache = IVirtualFileCache::CreateVirtualFileCache();
				InstallerError->Reset();

				//Used and Total size is calculated on VFC Start.
				//Used + Requested write will always result in new total. Doing this to recalculating all blocks.
				StreamerStats.VFCCachedUsedSize = (VirtualFileCache->GetUsedSize() / 1024.0f / 1024.0f);
				StreamerStats.VFCCachedTotalSize = (VirtualFileCache->GetTotalSize()  / 1024.0f / 1024.0f);
				PerSessionStreamerStats.VFCCachedUsedSize = StreamerStats.VFCCachedUsedSize;
				PerSessionStreamerStats.VFCCachedTotalSize = StreamerStats.VFCCachedTotalSize;

				// Setup file build list.
				FVirtualFileConstructorConfiguration VFCConfig;
				for (const FStreamRequest& Request : Requests)
				{
					const bool bIsTagRequest = Request.Get<1>();
					if (bIsTagRequest)
					{
						BuildPatchManifest->GetTaggedFileList(Request.Get<0>(), VFCConfig.FilesToConstruct);
					}
					else
					{
						VFCConfig.FilesToConstruct.Append(Request.Get<0>());
					}
				}

				// Remove all files that already exist.
				for (auto FileIt = VFCConfig.FilesToConstruct.CreateIterator(); FileIt; ++FileIt)
				{
					const FFileManifest* FileManifest = ManifestSet->GetNewFileManifest(*FileIt);
					if (FileManifest != nullptr && VirtualFileCache->DoesChunkExist(FileManifest->FileHash))
					{
						FileIt.RemoveCurrent();
					}
					else if (FileManifest != nullptr)
					{
						StreamerStats.VFCRequestedFileWrite += (FileManifest->FileSize / 1024.0f / 1024.0f);
						PerSessionStreamerStats.VFCRequestedFileWrite += (FileManifest->FileSize / 1024.0f / 1024.0f);
					}
				}

				// Composition root.
				FCloudSourceConfig CloudSourceConfig(Configuration.CloudDirectories);
				CloudSourceConfig.bBeginDownloadsOnFirstGet = false;
				CloudSourceConfig.bRunOwnThread = false;
				TUniquePtr<IChunkReferenceTracker> ChunkReferenceTracker(FChunkReferenceTrackerFactory::Create(
					ManifestSet.Get(),
					VFCConfig.FilesToConstruct));
				TUniquePtr<IChunkEvictionPolicy> MemoryEvictionPolicy(FChunkEvictionPolicyFactory::Create(
					ChunkReferenceTracker.Get()));
				TUniquePtr<IMemoryChunkStore> CloudChunkStore(FMemoryChunkStoreFactory::Create(
					FMath::Clamp<int32>(CloudSourceConfig.PreFetchMaximum, 64, 512),
					MemoryEvictionPolicy.Get(),
					nullptr,
					MemoryChunkStoreStatistics.Get()));
				TUniquePtr<ICloudChunkSource> CloudChunkSource(FCloudChunkSourceFactory::Create(
					MoveTemp(CloudSourceConfig),
					Platform.Get(),
					CloudChunkStore.Get(),
					DownloadService.Get(),
					ChunkReferenceTracker.Get(),
					ChunkDataSerialization.Get(),
					MessagePump.Get(),
					InstallerError.Get(),
					DownloadConnectionCount.Get(),
					CloudChunkSourceStatistics.Get(),
					ManifestSet.Get(),
					ChunkReferenceTracker->GetReferencedChunks()));
				// TODO: Use promise and future so we can wait for this call to end before destructing, either here or in class.
				CloudQueue.Enqueue([&CloudChunkSource]() { CloudChunkSource->ThreadRun(); });
				CloudTrigger->Trigger();
				FVirtualFileConstructorDependencies VFCDepends{
					ManifestSet.Get(),
					VirtualFileCache.Get(),
					CloudChunkSource.Get(),
					ChunkReferenceTracker.Get(),
					InstallerError.Get(),
					FileConstructorStatistics.Get()
				};
				TUniquePtr<FVirtualFileConstructor> VirtualFileConstructor(FVirtualFileConstructorFactory::Create(MoveTemp(VFCConfig), MoveTemp(VFCDepends)));
				TArray<IControllable*> Controllables;
				Controllables.Add(CloudChunkSource.Get());
				int32 ErrorHandle = InstallerError->RegisterForErrors([&CloudChunkSource]()
					{
						CloudChunkSource->Abort();
					});

				// Run the construction.
				bool bSuccess = VirtualFileConstructor->Run();
				RequestTimer.Stop();
				RequestTime = RequestTimer.GetSeconds();

				// Keep a ref during OnComplete execution, the provided delegate can release all other refs causing our dctor to block.
				// This way, our destructor will instead be called once we exit this request scope causing this thread to quit also.
				FBuildInstallStreamerRef SharedThis = AsShared();
				CallOnDelegateThread(CompleteRequest);
				InstallerError->UnregisterForErrors(ErrorHandle);
				Requests.Empty();
			}
			else
			{
				// Else wait on trigger.
				RequestTrigger->Wait();
			}
		}
		// Ensure we call all remaining requests delegates as cancelled.
		while (RequestQueue.Dequeue(Requests.AddDefaulted_GetRef())) {}
		Requests.Pop();
		if (Requests.Num() > 0)
		{
			CallOnDelegateThread(CancelRequest);
		}
	}

	void FBuildInstallStreamer::CloudWorkerThread()
	{
		while (!bIsShuttingDown)
		{
			TFunction<void()> Task;
			if (CloudQueue.Dequeue(Task))
			{
				Task();
			}
			else
			{
				CloudTrigger->Wait();
			}
		}
	}

	void FBuildInstallStreamer::CallOnDelegateThread(const TFunction<void()>& Callback)
	{
		if (Configuration.bMainThreadDelegates)
		{
			AsyncHelpers::ExecuteOnCustomThread<void>(Callback, TickQueue).Wait();
		}
		else
		{
			Callback();
		}
	}

	bool FBuildInstallStreamer::Tick()
	{
		const bool bKeepTicking = true;

		MessagePump->PumpMessages();

		TFunction<void()> TickFunc;
		while (TickQueue.Dequeue(TickFunc))
		{
			TickFunc();
		}

		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.VFCRequestedFileWrite,		StreamerStats.VFCRequestedFileWrite, ECsvCustomStatOp::Set);
		//CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.VFCActualFileWrite,		StreamerStats.VFCActualFileWrite, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.VFCCachedUsedSize,			StreamerStats.VFCCachedUsedSize, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.VFCCachedTotalSize,		StreamerStats.VFCCachedTotalSize, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.TotalMegaBytesDownloaded,	StreamerStats.TotalMegaBytesDownloaded, ECsvCustomStatOp::Set);
		
		//CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.FileMegaBytesDownloaded,	StreamerStats.FileMegaBytesDownloaded, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.FileRequestsCompleted,	    (double)StreamerStats.FileRequestsCompleted, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.FileRequestsCancelled,	    (double)StreamerStats.FileRequestsCancelled, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.FileRequestsMade,		    (double)StreamerStats.FileRequestsMade, ECsvCustomStatOp::Set);
		
		//CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.BundleMegaBytesDownloaded,	StreamerStats.BundleMegaBytesDownloaded, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.BundleRequestsCompleted,	(double)StreamerStats.BundleRequestsCompleted, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.BundleRequestsCancelled,	(double)StreamerStats.BundleRequestsCancelled, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.BundleRequestsMade,		(double)StreamerStats.BundleRequestsMade, ECsvCustomStatOp::Set);
		
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.AverageRequestTime,	StreamerStats.AverageRequestTime, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(CosmeticStreamingCsv, StreamerStats.MaxRequestTime,		StreamerStats.MaxRequestTime, ECsvCustomStatOp::Set);

		return bKeepTicking;
	}

	void FBuildInstallStreamer::CancelAllRequests()
	{
		const bool bInMainThread = IsInGameThread();
		bool bAllRequestsFlushed = false;
		// If we are asked to cancel from the game thread, we need to tick ourselves so we can pump cancelled request delegates.
		// We can add our own empty request right away so we know when all requests in the queue at time of this function call have been executed.
		// If we are not in the game thread, then there needs to be ticks externally.
		if (bInMainThread)
		{
			QueueFilesByName({}, FBuildPatchStreamCompleteDelegate::CreateLambda([&](FBuildPatchStreamResult) { bAllRequestsFlushed = true; }));
		}
		InstallerError->SetError(EBuildPatchInstallError::UserCanceled, UserCancelErrorCodes::UserRequested);
		if (bInMainThread)
		{
			while (!bAllRequestsFlushed)
			{
				Tick();
			}
		}
	}

	void FBuildInstallStreamer::PreExit()
	{
		bIsShuttingDown = true;
		InstallerError->SetError(EBuildPatchInstallError::ApplicationClosing, ApplicationClosedErrorCodes::ApplicationClosed);
		RequestTrigger->Trigger();
		CloudTrigger->Trigger();
		// We need to tick until the thread exits here (we must be in main thread now).
		check(IsInGameThread());
		while (!RequestWorker.IsReady() || !CloudWorker.IsReady())
		{
			Tick();
		}
	}

	FBuildInstallStreamer* FBuildInstallStreamerFactory::Create(FBuildInstallStreamerConfiguration Configuration)
	{
		return new FBuildInstallStreamer(MoveTemp(Configuration));
	}
}
