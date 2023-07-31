// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/Statistics/DownloadServiceStatistics.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "Interfaces/IHttpResponse.h"

#include "Core/AsyncHelpers.h"
#include "Common/StatsCollector.h"
#include "Installer/InstallerAnalytics.h"
#include "BuildPatchProgress.h"
#include "BuildPatchManifest.h"
#include "BuildPatchUtil.h"
#include "Containers/Queue.h"

namespace BuildPatchServices
{
	class FDownloadServiceStatistics
		: public IDownloadServiceStatistics
	{
	public:
		FDownloadServiceStatistics(ISpeedRecorder* SpeedRecorder, IDataSizeProvider* DataSizeProvider, IInstallerAnalytics* InstallerAnalytics);
		~FDownloadServiceStatistics();

		// IDownloadServiceStat interface begin.
		virtual void OnDownloadStarted(int32 RequestId, const FString& Uri) override;
		virtual void OnDownloadProgress(int32 RequestId, int32 BytesReceived) override;
		virtual void OnDownloadComplete(const FDownloadRecord& DownloadRecord) override;
		// IDownloadServiceStat interface end.

		// IDownloadServiceStatistics interface begin.
		virtual uint64 GetBytesDownloaded() const override;
		virtual int32 GetNumSuccessfulChunkDownloads() const override;
		virtual int32 GetNumFailedChunkDownloads() const override;
		virtual int32 GetNumCurrentDownloads() const override;
		virtual TArray<FDownload> GetCurrentDownloads() const override;
		virtual TPair<double, uint32> GetImmediateAverageSpeedPerRequest(uint32 MinCount) override;
		// IDownloadServiceStatistics interface end.

	private:
		ISpeedRecorder* SpeedRecorder;
		IDataSizeProvider* DataSizeProvider;
		IInstallerAnalytics* InstallerAnalytics;
		FThreadSafeInt64 TotalBytesReceived;
		FThreadSafeInt32 NumSuccessfulDownloads;
		FThreadSafeInt32 NumFailedDownloads;

		typedef TTuple<FString, int32> FDownloadTuple;
		TMap<int32, FDownloadTuple> Downloads;
		double AccumulatedRequestSpeed;
		uint32 AverageSpeedSampleCount;
		FCriticalSection AverageSpeedCriticalSection;
		
	};

	FDownloadServiceStatistics::FDownloadServiceStatistics(ISpeedRecorder* InSpeedRecorder, IDataSizeProvider* InDataSizeProvider, IInstallerAnalytics* InInstallerAnalytics)
		: SpeedRecorder(InSpeedRecorder)
		, DataSizeProvider(InDataSizeProvider)
		, InstallerAnalytics(InInstallerAnalytics)
		, TotalBytesReceived(0)
		, NumSuccessfulDownloads(0)
		, NumFailedDownloads(0)
		, AverageSpeedSampleCount(0U)
	{
	}

	FDownloadServiceStatistics::~FDownloadServiceStatistics()
	{
	}

	void FDownloadServiceStatistics::OnDownloadStarted(int32 RequestId, const FString& Uri)
	{
		checkSlow(IsInGameThread());
		FDownloadTuple& DownloadTuple = Downloads.FindOrAdd(RequestId);
		DownloadTuple.Get<0>() = Uri;
		DownloadTuple.Get<1>() = 0;
	}

	void FDownloadServiceStatistics::OnDownloadProgress(int32 RequestId, int32 BytesReceived)
	{
		checkSlow(IsInGameThread());
		FDownloadTuple& DownloadTuple = Downloads.FindOrAdd(RequestId);
		DownloadTuple.Get<1>() = BytesReceived;
	}

	void FDownloadServiceStatistics::OnDownloadComplete(const FDownloadRecord& DownloadRecord)
	{
		checkSlow(IsInGameThread());
		Downloads.Remove(DownloadRecord.RequestId);
		if (DownloadRecord.bSuccess && EHttpResponseCodes::IsOk(DownloadRecord.ResponseCode))
		{
			TotalBytesReceived.Add(DownloadRecord.SpeedRecord.Size);
			NumSuccessfulDownloads.Increment();
			SpeedRecorder->AddRecord(DownloadRecord.SpeedRecord);
			const uint64 Cycles = DownloadRecord.SpeedRecord.CyclesEnd - DownloadRecord.SpeedRecord.CyclesStart;
			double Speed = Cycles > 0U ? DownloadRecord.SpeedRecord.Size / FStatsCollector::CyclesToSeconds(Cycles) : 0.0;
			FScopeLock Lock(&AverageSpeedCriticalSection);
			AccumulatedRequestSpeed +=  Speed;
			AverageSpeedSampleCount += 1;
		}
		else
		{
			NumFailedDownloads.Increment();
			InstallerAnalytics->RecordChunkDownloadError(DownloadRecord.Uri, DownloadRecord.ResponseCode, TEXT("DownloadFail"));
		}
	}

	uint64 FDownloadServiceStatistics::GetBytesDownloaded() const
	{
		return TotalBytesReceived.GetValue();
	}

	int32 FDownloadServiceStatistics::GetNumSuccessfulChunkDownloads() const
	{
		return NumSuccessfulDownloads.GetValue();
	}

	int32 FDownloadServiceStatistics::GetNumFailedChunkDownloads() const
	{
		return NumFailedDownloads.GetValue();
	}

	int32 FDownloadServiceStatistics::GetNumCurrentDownloads() const
	{
		return Downloads.Num();
	}

	TArray<FDownload> FDownloadServiceStatistics::GetCurrentDownloads() const
	{
		checkSlow(IsInGameThread());
		TArray<FDownload> Result;
		Result.Empty(Downloads.Num());
		for (const TPair<int32, FDownloadTuple>& Download : Downloads)
		{
			Result.AddDefaulted();
			FDownload& Element = Result.Last();
			Element.Data = FPaths::GetCleanFilename(Download.Value.Get<0>());
			Element.Size = DataSizeProvider->GetDownloadSize(Element.Data);
			Element.Received = Download.Value.Get<1>();
		}
		return Result;
	}

	TPair<double, uint32> FDownloadServiceStatistics::GetImmediateAverageSpeedPerRequest(uint32 MinCount)
	{
		double Average = 0.0L;
		double OverallAverage = 0.0L;
		uint32 Count = 0U;
		double Result = 0.0L;
		FScopeLock Lock(&AverageSpeedCriticalSection);
		if (AverageSpeedSampleCount >= MinCount)
		{
			Result = AccumulatedRequestSpeed / AverageSpeedSampleCount;
			Count = AverageSpeedSampleCount;
			AccumulatedRequestSpeed = 0.0L;
			AverageSpeedSampleCount = 0;
		}
		return TPair<double, uint32>(Result, Count);
	}
	IDownloadServiceStatistics* FDownloadServiceStatisticsFactory::Create(ISpeedRecorder* SpeedRecorder, IDataSizeProvider* DataSizeProvider, IInstallerAnalytics* InstallerAnalytics)
	{
		check(SpeedRecorder != nullptr);
		check(DataSizeProvider != nullptr);
		check(InstallerAnalytics != nullptr);
		return new FDownloadServiceStatistics(SpeedRecorder, DataSizeProvider, InstallerAnalytics);
	}
};