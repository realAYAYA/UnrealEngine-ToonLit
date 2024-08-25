// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Async/Async.h"
#include "BuildPatchManifest.h"
#include "BuildPatchProgress.h"
#include "Containers/Queue.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "HAL/Event.h"
#include "HAL/ThreadSafeBool.h"
#include "Interfaces/IBuildInstallStreamer.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "Templates/UniquePtr.h"
#include "Templates/Tuple.h"

namespace BuildPatchServices
{
	class IBuildManifestSet;
	class IHttpManager;
	class IFileSystem;
	class IChunkDataSerialization;
	class IPlatform;
	class IInstallerError;
	class IInstallerAnalytics;
	class IFileOperationTracker;
	class ISpeedRecorder;
	class IChunkDataSizeProvider;
	class IDownloadServiceStatistics;
	class IMemoryChunkStoreStatistics;
	class ICloudChunkSourceStatistics;
	class IFileConstructorStatistics;
	class IDownloadConnectionCount;
	class IDownloadService;
	class IMessagePump;

	class FBuildInstallStreamer
		: public IBuildInstallStreamer
		, public TSharedFromThis<FBuildInstallStreamer, ESPMode::ThreadSafe>
	{
	public:
		FBuildInstallStreamer(FBuildInstallStreamerConfiguration Configuration);
		~FBuildInstallStreamer();

	public:
		// IBuildInstallStreamer interface begin.
		virtual void QueueFilesByTag(TSet<FString> Tags, FBuildPatchStreamCompleteDelegate OnComplete) override;
		virtual void QueueFilesByName(TSet<FString> Files, FBuildPatchStreamCompleteDelegate OnComplete) override;
		virtual void RegisterMessageHandler(FMessageHandler* MessageHandler) override;
		virtual void UnregisterMessageHandler(FMessageHandler* MessageHandler) override;
		virtual const FBuildInstallStreamerConfiguration& GetConfiguration() const override;
		virtual const FBuildInstallStreamerStats& GetInstallStreamerStatistics() const override;
		virtual const FBuildInstallStreamerStats& GetInstallStreamerSessionStatistics() const override;
		virtual void ResetSessionStatistics() override;
		// IBuildInstallStreamer interface end.

		bool Tick();
		void CancelAllRequests();
		void PreExit();

	private:
		void Initialise();
		void RequestWorkerThread();
		void CloudWorkerThread();
		void CallOnDelegateThread(const TFunction<void()>& Callback);

	private:

		const FBuildInstallStreamerConfiguration Configuration;
		FBuildPatchAppManifestPtr BuildPatchManifest;
		FBuildPatchProgress BuildProgress;
		TUniquePtr<IBuildManifestSet> ManifestSet;
		TUniquePtr<IHttpManager> HttpManager;
		TUniquePtr<IFileSystem> FileSystem;
		TUniquePtr<IChunkDataSerialization> ChunkDataSerialization;
		TUniquePtr<IPlatform> Platform;
		TUniquePtr<IInstallerAnalytics> InstallerAnalytics;
		TUniquePtr<IInstallerError> InstallerError;
		TUniquePtr<IFileOperationTracker> FileOperationTracker;
		TUniquePtr<ISpeedRecorder> DownloadSpeedRecorder;
		TUniquePtr<ISpeedRecorder> FileReadSpeedRecorder;
		TUniquePtr<ISpeedRecorder> FileWriteSpeedRecorder;
		TUniquePtr<IChunkDataSizeProvider> ChunkDataSizeProvider;
		TUniquePtr<IDownloadServiceStatistics> DownloadServiceStatistics;
		TUniquePtr<IMemoryChunkStoreStatistics> MemoryChunkStoreStatistics;
		TUniquePtr<ICloudChunkSourceStatistics> CloudChunkSourceStatistics;
		TUniquePtr<IFileConstructorStatistics> FileConstructorStatistics;
		TUniquePtr<IDownloadConnectionCount> DownloadConnectionCount;
		TUniquePtr<IDownloadService> DownloadService;
		TUniquePtr<IMessagePump> MessagePump;
		FThreadSafeBool bIsShuttingDown;
		TFuture<void> RequestWorker;
		TFuture<void> CloudWorker;
		typedef TTuple<TSet<FString>, bool, FBuildPatchStreamCompleteDelegate> FStreamRequest;
		TQueue<FStreamRequest, EQueueMode::Spsc> RequestQueue;
		TQueue<TFunction<void()>, EQueueMode::Spsc> CloudQueue;
		TQueue<TFunction<void()>, EQueueMode::Spsc> TickQueue;
		FEvent* RequestTrigger;
		FEvent* CloudTrigger;
		FBuildInstallStreamerStats StreamerStats;
		FBuildInstallStreamerStats PerSessionStreamerStats;
	};

	/**
	 * A factory for creating an IBuildInstallStreamer instance.
	 */
	class FBuildInstallStreamerFactory
	{
	public:
		static FBuildInstallStreamer* Create(FBuildInstallStreamerConfiguration Configuration);
	};
}

typedef TSharedPtr< BuildPatchServices::FBuildInstallStreamer, ESPMode::ThreadSafe > FBuildInstallStreamerPtr;
typedef TSharedRef< BuildPatchServices::FBuildInstallStreamer, ESPMode::ThreadSafe > FBuildInstallStreamerRef;
typedef TWeakPtr< BuildPatchServices::FBuildInstallStreamer, ESPMode::ThreadSafe > FBuildInstallStreamerWeakPtr;