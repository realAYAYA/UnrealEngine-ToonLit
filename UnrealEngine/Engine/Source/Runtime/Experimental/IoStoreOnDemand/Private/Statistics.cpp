// Copyright Epic Games, Inc. All Rights Reserved.

#include "Statistics.h"

#include "Algo/MaxElement.h"
#include "AnalyticsEventAttribute.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "IO/IoStoreOnDemand.h"
#include "Internationalization/Internationalization.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CoreDelegates.h"

LLM_DEFINE_TAG(Ias);

#if IAS_WITH_STATISTICS

namespace UE::IO::IAS
{

float GIasStatisticsLogInterval = 30.f;
static FAutoConsoleVariableRef CVar_StatisticsLogInterval(
	TEXT("ias.StatisticsLogInterval"),
	GIasStatisticsLogInterval,
	TEXT("Enables and sets interval for periodic logging of statistics"));

bool GIasDisplayOnScreenStatistics = false;
static FAutoConsoleVariableRef CVar_DisplayOnScreenStatistics(
	TEXT("ias.DisplayOnScreenStatistics"),
	GIasDisplayOnScreenStatistics,
	TEXT("Enables display of Ias on screen statistics"));

bool GIasReportHttpAnalyticsEnabled = true;
static FAutoConsoleVariableRef CVar_ReportHttpAnalytics(
	TEXT("ias.ReportHttpAnalytics"),
	GIasReportHttpAnalyticsEnabled,
	TEXT("Enables reporting statics on our http traffic to the analytics system"));

bool GIasReportCacheAnalyticsEnabled = true;
static FAutoConsoleVariableRef CVar_ReportCacheAnalytics(
	TEXT("ias.ReportCacheAnalytics"),
	GIasReportCacheAnalyticsEnabled,
	TEXT("Enables reporting statics on our file cache usage to the analytics system"));

////////////////////////////////////////////////////////////////////////////////
static int32 BytesToApproxMB(uint64 Bytes) { return int32(Bytes >> 20); }
static int32 BytesToApproxKB(uint64 Bytes) { return int32(Bytes >> 10); }

/**
 * Code taken from SummarizeTraceCommandlet.cpp pending discussion on moving it
 * somewhere for general use.
 * Currently not thread safe!
 */
class FIncrementalVariance
{
public:
	FIncrementalVariance()
		: Count(0)
		, Mean(0.0)
		, VarianceAccumulator(0.0)
	{

	}

	uint64 GetCount() const
	{
		return Count;
	}

	double GetMean() const
	{
		return Mean;
	}

	/**
	* Compute the variance given Welford's accumulator and the overall count
	*
	* @return The variance in sample units squared
	*/
	double GetVariance() const
	{
		double Result = 0.0;

		if (Count > 1)
		{
			// Welford's final step, dependent on sample count
			Result = VarianceAccumulator / double(Count - 1);
		}

		return Result;
	}

	/**
	* Compute the standard deviation given Welford's accumulator and the overall count
	*
	* @return The standard deviation in sample units
	*/
	double GetDeviation() const
	{
		double Result = 0.0;

		if (Count > 1)
		{
			// Welford's final step, dependent on sample count
			double DeviationSqrd = VarianceAccumulator / double(Count - 1);

			// stddev is sqrt of variance, to restore to units (vs. units squared)
			Result = sqrt(DeviationSqrd);
		}

		return Result;
	}

	/**
	* Perform an increment of work for Welford's variance, from which we can compute variation and standard deviation
	*
	* @param InSample	The new sample value to operate on
	*/
	void Increment(const double InSample)
	{
		Count++;
		const double OldMean = Mean;
		Mean += ((InSample - Mean) / double(Count));
		VarianceAccumulator += ((InSample - Mean) * (InSample - OldMean));
	}

	/**
	* Merge with another IncrementalVariance series in progress
	*
	* @param Other	The other variance incremented from another mutually exclusive population of analogous data.
	*/
	void Merge(const FIncrementalVariance& Other)
	{
		// empty other, nothing to do
		if (Other.Count == 0)
		{
			return;
		}

		// empty this, just copy other
		if (Count == 0)
		{
			Count = Other.Count;
			Mean = Other.Mean;
			VarianceAccumulator = Other.VarianceAccumulator;
			return;
		}

		const double TotalPopulation = static_cast<double>(Count + Other.Count);
		const double MeanDifference = Mean - Other.Mean;
		const double A = (double(Count - 1) * GetVariance()) + (double(Other.Count - 1) * Other.GetVariance());
		const double B = (MeanDifference) * (MeanDifference) * (double(Count) * double(Other.Count) / TotalPopulation);
		const double MergedVariance = (A + B) / (TotalPopulation - 1);

		const uint64 NewCount = Count + Other.Count;
		const double NewMean = ((Mean * double(Count)) + (Other.Mean * double(Other.Count))) / double(NewCount);
		const double NewVarianceAccumulator = MergedVariance * double(NewCount - 1);

		Count = NewCount;
		Mean = NewMean;
		VarianceAccumulator = NewVarianceAccumulator;
	}

	/**
	* Reset state back to initialized.
	*/
	void Reset()
	{
		Count = 0;
		Mean = 0.0;
		VarianceAccumulator = 0.0;
	}

private:
	uint64 Count;
	double Mean;
	double VarianceAccumulator;
};

class FDeltaTracking
{
public:
	int64 Get(FStringView Name, int64 Value)
	{
		if (int64* PrevValue = IntTotals.FindByHash(GetTypeHash(Name), Name))
		{
			const int64 Delta = Value - *PrevValue;
			*PrevValue = Value;

			return Delta;
		}
		else
		{
			IntTotals.Add(FString(Name), 0);
			return Value;
		}
	}

	uint32 Get(FStringView Name, uint32 Value)
	{
		return static_cast<uint32>(Get(Name, static_cast<int64>(Value)));
	}

	double Get(FStringView Name, double Value)
	{
		if (double* PrevValue = RealTotals.FindByHash(GetTypeHash(Name), Name))
		{
			const double Delta = Value - *PrevValue;
			*PrevValue = Value;

			return Delta;
		}
		else
		{
			RealTotals.Add(FString(Name), 0.0);
			return Value;
		}
	}

private:

	TMap<FString, int64> IntTotals;
	TMap<FString, double> RealTotals;

} static GDeltaTracking;

////////////////////////////////////////////////////////////////////////////////
// TRACE STATS

#if COUNTERSTRACE_ENABLED
	using FCounterInt		= FCountersTrace::FCounterInt;
	using FCounterAtomicInt = FCountersTrace::FCounterAtomicInt;
#else
	template <typename Type>
	struct TCounterInt
	{
		TCounterInt(...)  {}
		void Set(int64 i) { V = i; }
		void Add(int64 d) { V += d; }
		int64 Get() const { return V;}
		Type V = 0;
	};
	using FCounterInt		= TCounterInt<int64>;
	using FCounterAtomicInt = TCounterInt<std::atomic<int64>>;
#endif 

// iorequest stats
FCounterInt				GIoRequestCount(TEXT("Ias/IoRequestCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestReadCount(TEXT("Ias/IoRequestReadCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestReadBytes(TEXT("Ias/IoRequestReadBytes"), TraceCounterDisplayHint_Memory);
FCounterInt				GIoRequestCancelCount(TEXT("Ias/IoRequestCancelCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestErrorCount(TEXT("Ias/IoRequestErrorCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoDecodeErrorCount(TEXT("Ias/IoDecodeErrorCount"), TraceCounterDisplayHint_None);
// cache stats
FCounterAtomicInt		GCacheErrorCount(TEXT("Ias/CacheErrorCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCacheGetCount(TEXT("Ias/CacheGetCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutCount(TEXT("Ias/CachePutCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutExistingCount(TEXT("Ias/CachePutExistingCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutRejectCount(TEXT("Ias/CachePutRejectCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCacheCachedBytes(TEXT("Ias/CacheCachedBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCacheWrittenBytes(TEXT("Ias/CacheWrittenBytes"), TraceCounterDisplayHint_Memory);
int64					GCacheMaxBytes = 0;
FCounterAtomicInt		GCachePendingBytes(TEXT("Ias/CachePendingBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCacheReadBytes(TEXT("Ias/CacheReadBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCacheRejectBytes(TEXT("Ias/CachePutRejectBytes"), TraceCounterDisplayHint_Memory);
// http stats
FCounterInt				GHttpConnectCount(TEXT("Ias/HttpConnectCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDisconnectCount(TEXT("Ias/HttpDisconnectCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpGetCount(TEXT("Ias/HttpGetCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpErrorCount(TEXT("Ias/HttpErrorCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpRetryCount(TEXT("Ias/HttpRetryCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpCancelCount(TEXT("Ias/HttpCancelCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GHttpPendingCount(TEXT("Ias/HttpPendingCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpInflightCount(TEXT("Ias/HttpInflightCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDownloadedBytes(TEXT("Ias/HttpDownloadedBytes"), TraceCounterDisplayHint_Memory);
FCounterInt				GHttpBandwidthMpbs(TEXT("Ias/HttpBandwidthMbps"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDurationMs(TEXT("Ias/HttpDurationMs"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDurationMsAvg(TEXT("Ias/HttpDurationMsAvg"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDurationMsMax(TEXT("Ias/HttpDurationMsMax"), TraceCounterDisplayHint_None);
int64					GHttpDurationMsSum = 0;
constexpr int64			GHttpHistoryCount = 16;
int64					GHttpHistoryDuration[GHttpHistoryCount] = {};
int64					GHttpHistoryBytes[GHttpHistoryCount] = {};
int64					GHttpHistoryTotalDuration = 0;
int64					GHttpHistoryTotalBytes = 0;
int64 					GHttpHistoryIndex = 0;
FIncrementalVariance	GHttpAvgDuration; // Duration of the http requests, in milliseconds

////////////////////////////////////////////////////////////////////////////////
// 

static const uint64 DurationBuckets[] = { 30, 150, 400, 1000 }; // Time boundaries (in ms) for each bucket

int32 FindDurationBucket(uint64 DurationMs)
{
	const uint32 NumBoundries = UE_ARRAY_COUNT(DurationBuckets);
	for (int32 Index = 0; Index < NumBoundries; ++Index)
	{
		if (DurationMs <= DurationBuckets[Index])
		{
			return Index;
		}
	}

	return NumBoundries;
}

uint32 GHttpDurationBuckets[UE_ARRAY_COUNT(DurationBuckets) +1] = { 0 };

////////////////////////////////////////////////////////////////////////////////
// CSV STATS
CSV_DEFINE_CATEGORY(Ias, true);
// iorequest per frame stats
CSV_DEFINE_STAT(Ias, FrameIoRequestCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestReadCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestReadMB);
CSV_DEFINE_STAT(Ias, FrameIoRequestCancelCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestErrorCount);
// cache stat totals
CSV_DEFINE_STAT(Ias, CacheGetCount);
CSV_DEFINE_STAT(Ias, CacheErrorCount);
CSV_DEFINE_STAT(Ias, CachePutCount);
CSV_DEFINE_STAT(Ias, CachePutExistingCount);
CSV_DEFINE_STAT(Ias, CachePutRejectCount);
CSV_DEFINE_STAT(Ias, CacheCachedMB);
CSV_DEFINE_STAT(Ias, CacheWrittenMB);
CSV_DEFINE_STAT(Ias, CacheReadMB);
CSV_DEFINE_STAT(Ias, CacheRejectedMB);
// http stat totals
CSV_DEFINE_STAT(Ias, HttpGetCount);
CSV_DEFINE_STAT(Ias, HttpRetryCount);
CSV_DEFINE_STAT(Ias, HttpCancelCount);
CSV_DEFINE_STAT(Ias, HttpErrorCount);
CSV_DEFINE_STAT(Ias, HttpPendingCount);
CSV_DEFINE_STAT(Ias, HttpDownloadedMB);
CSV_DEFINE_STAT(Ias, HttpBandwidthMpbs);
CSV_DEFINE_STAT(Ias, HttpDurationMsAvg);
CSV_DEFINE_STAT(Ias, HttpDurationMsMax);

static FOnDemandIoBackendStats* GStatistics = nullptr;
static FDelegateHandle GStatisticsEndFrameDelegateHandle;
static FDelegateHandle GStatisticsOnScreenDelegateHandle;

FOnDemandIoBackendStats::FOnDemandIoBackendStats()
{
	check(GStatistics == nullptr);
	GStatistics = this;
	GStatisticsEndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddLambda([this]()
	{
		// cache stat totals
		int32 CGetCount = int32(GCacheGetCount.Get());
		int32 CErrorCount = int32(GCacheErrorCount.Get());
		int32 CPutCount = int32(GCachePutCount.Get());
		int32 CPutExistingCount = int32(GCachePutExistingCount.Get());
		int32 CPutRejectCount = int32(GCachePutRejectCount.Get());
		int32 CCachedKiB = BytesToApproxKB(GCacheCachedBytes.Get());
		int32 CWrittenKiB = BytesToApproxKB(GCacheWrittenBytes.Get());
		int32 CReadKiB = BytesToApproxKB(GCacheReadBytes.Get());
		int32 CRejectedKiB = BytesToApproxKB(GCacheRejectBytes.Get());
		CSV_CUSTOM_STAT_DEFINED(CacheGetCount, CGetCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheErrorCount, CErrorCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutCount, CPutCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutExistingCount, CPutExistingCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutRejectCount, CPutRejectCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheCachedMB, CCachedKiB >> 10, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheWrittenMB, CWrittenKiB >> 10, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheReadMB, CReadKiB >> 10, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheRejectedMB, CRejectedKiB >> 10, ECsvCustomStatOp::Set);

		// http stat totals
		int32 HGetCount = int32(GHttpGetCount.Get());
		int32 HRetryCount = int32(GHttpRetryCount.Get());
		int32 HCancelCount = int32(GHttpCancelCount.Get());
		int32 HErrorCount = int32(GHttpErrorCount.Get());
		int32 HPendingCount = int32(GHttpPendingCount.Get());
		int32 HDownloadedKiB = BytesToApproxKB(GHttpDownloadedBytes.Get());
		int32 HBandwidthMpbs = int32(GHttpBandwidthMpbs.Get());
		int32 HDurationMsAvg = int32(GHttpDurationMsAvg.Get());
		int32 HDurationMsMax = int32(GHttpDurationMsMax.Get());
		CSV_CUSTOM_STAT_DEFINED(HttpGetCount, HGetCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpCancelCount, HCancelCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpErrorCount, HErrorCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpPendingCount, HPendingCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpDownloadedMB, HDownloadedKiB >> 10, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpBandwidthMpbs, HBandwidthMpbs, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpDurationMsAvg, HDurationMsAvg, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(HttpDurationMsMax, HDurationMsMax, ECsvCustomStatOp::Set);

		if (GIasStatisticsLogInterval > 0.f)
		{
			static double LastLogTime = 0.0;
			if (double Time = FPlatformTime::Seconds(); Time - LastLogTime > (double)GIasStatisticsLogInterval)
			{
				UE_LOG(LogIas, Log, TEXT("CacheStats: CachedKiB=%d, WrittenKiB=%d, ReadKiB=%d, RejectedKiB=%d, Get=%d, Error=%d, Put=%d, PutReject=%d, PutExisting=%d"),
					CCachedKiB, CWrittenKiB, CReadKiB, CRejectedKiB, CGetCount, CErrorCount, CPutCount, CPutRejectCount, CPutExistingCount);
				UE_LOG(LogIas, Log, TEXT("HttpStats: DownloadedKiB=%d, Get=%d, Retry=%d, Cancel=%d, Error=%d, CurPending=%d, CurDurationMsAvg=%d, CurDurationMsMax=%d"),
					HDownloadedKiB, HGetCount, HRetryCount, HCancelCount, HErrorCount, HPendingCount, HDurationMsAvg, HDurationMsMax);
				LastLogTime = Time;
			}
		}
	});

#define LOCTEXT_NAMESPACE "IAS"
	GStatisticsOnScreenDelegateHandle = FCoreDelegates::OnGetOnScreenMessages.AddLambda(
		[this] (FCoreDelegates::FSeverityMessageMap& Out)
		{
			if (GIasDisplayOnScreenStatistics)
			{
				const bool bIsConnected = GHttpConnectCount.Get() > GHttpDisconnectCount.Get();
				FText Message = FText::Format(
					LOCTEXT("IAS", "IAS - {0}: Cached:{1} KiB | Read:{2} KiB ({3}) | Downloaded:{4} KiB ({5}) {6} ms | Retries:{7} | Pending:{8}"),
					bIsConnected ? LOCTEXT("IASConnect", "Connected") : LOCTEXT("IASDisconnect", "Disconnected"),
					GCacheCachedBytes.Get() >> 10,
					GCacheReadBytes.Get() >> 10,
					GCacheGetCount.Get(),
					GHttpDownloadedBytes.Get() >> 10,
					GHttpGetCount.Get(),
					GHttpDurationMsAvg.Get(),
					GHttpRetryCount.Get(),
					GHttpPendingCount.Get()
				);
				Out.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, Message);
			}
		});
#undef LOCTEXT_NAMESPACE
}

FOnDemandIoBackendStats::~FOnDemandIoBackendStats()
{
	FCoreDelegates::OnEndFrame.Remove(GStatisticsEndFrameDelegateHandle);
	FCoreDelegates::OnGetOnScreenMessages.Remove(GStatisticsOnScreenDelegateHandle);
	GStatistics = nullptr;
}

FOnDemandIoBackendStats* FOnDemandIoBackendStats::Get()
{
	return GStatistics;
}

void FOnDemandIoBackendStats::ReportGeneralAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	// Note that this analytics section is not optional, if we are reporting analytics then we report this section
	// first. This means we can use the values here to determine if an analytics payload contains ondemand data or
	// not since with the current system we are unable to specify our own analytics payload.

	AppendAnalyticsEventAttributeArray(OutAnalyticsArray
		, TEXT("IasHttpHasEverConnected"), GHttpConnectCount.Get() > 0 // Report if the system has ever actually managed to make a connection
	);
}

void FOnDemandIoBackendStats::ReportEndPointAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	// We use this macro with counters that are used elsewhere meaning we can't just reset them
	// each time we send an analytics payload. The macro will track the running total and only
	// pass on the delta to the analytics array since the last time this method was called.
#define TRACK_DELTA(Name, Value) TEXT(Name), GDeltaTracking.Get(TEXTVIEW(Name), Value)

	if (GIasReportHttpAnalyticsEnabled)
	{
		AppendAnalyticsEventAttributeArray(OutAnalyticsArray
			,TRACK_DELTA("IasHttpErrorCount", GHttpErrorCount.Get())
			,TRACK_DELTA("IasHttpRetryCount", GHttpRetryCount.Get())
			,TRACK_DELTA("IasHttpGetCount", GHttpGetCount.Get())
			,TRACK_DELTA("IasHttpDownloadedBytes", GHttpDownloadedBytes.Get())
			,TEXT("IasHttpDurationMeanAvg"), GHttpAvgDuration.GetMean()
			,TEXT("IasHttpDurationStdDev"), GHttpAvgDuration.GetDeviation()

			,TRACK_DELTA("IasDecodeErrors", GIoDecodeErrorCount.Get())

			,TEXT("IasHttpDuration0"), GHttpDurationBuckets[0]
			,TEXT("IasHttpDuration1"), GHttpDurationBuckets[1]
			,TEXT("IasHttpDuration2"), GHttpDurationBuckets[2]
			,TEXT("IasHttpDuration3"), GHttpDurationBuckets[3]
			,TEXT("IasHttpDuration4"), GHttpDurationBuckets[4]
		);

		// These values we can just reset as they are only being used with analytics
		GHttpAvgDuration.Reset();
		FMemory::Memzero(GHttpDurationBuckets);
	}

	if (GIasReportCacheAnalyticsEnabled)
	{
		const int64 CacheTotalCount = GCacheGetCount.Get() + GCachePutCount.Get();
		const float CacheUsagePercent = GCacheMaxBytes > 0 ? 100.f * (float(GCacheCachedBytes.Get()) / float(GCacheMaxBytes)) : 0.f;

		AppendAnalyticsEventAttributeArray(OutAnalyticsArray

			,TRACK_DELTA("IasCacheTotalCount", CacheTotalCount)
			,TRACK_DELTA("IasCacheErrorCount", GCacheErrorCount.Get())
			,TRACK_DELTA("IasCacheGetCount", GCacheGetCount.Get())
			,TRACK_DELTA("IasCachePutCount", GCachePutCount.Get())

			,TEXT("IasCacheCachedBytes"), GCacheCachedBytes.Get()
			,TEXT("IasCacheMaxBytes"), GCacheMaxBytes
			,TEXT("IasCacheUsagePercent"), CacheUsagePercent

			,TRACK_DELTA("IasCacheWriteBytes", GCacheWrittenBytes.Get())
			,TRACK_DELTA("IasCacheReadBytes", GCacheReadBytes.Get())
			,TRACK_DELTA("IasCacheRejectBytes", GCacheRejectBytes.Get())
		);
	}

#undef TRACK_DELTA
}

void FOnDemandIoBackendStats::OnIoRequestEnqueue()
{
	GIoRequestCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestCount, int32(GIoRequestCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestComplete(uint64 Size, uint64 Duration)
{
	GIoRequestReadCount.Add(1);
	GIoRequestReadBytes.Add(Size);

	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestReadCount, int32(GIoRequestReadCount.Get()), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestReadMB, BytesToApproxMB(GIoRequestReadBytes.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestCancel()
{
	GIoRequestCancelCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestCancelCount, int32(GIoRequestCancelCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestError()
{
	GIoRequestErrorCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestErrorCount, int32(GIoRequestErrorCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoDecodeError()
{
	GIoDecodeErrorCount.Add(1);
}

void FOnDemandIoBackendStats::OnCacheError()
{
	GCacheErrorCount.Add(1);
}

void FOnDemandIoBackendStats::OnCacheGet(uint64 DataSize)
{
	GCacheGetCount.Add(1);
	GCacheReadBytes.Add(DataSize);
}

void FOnDemandIoBackendStats::OnCachePut()
{
	GCachePutCount.Add(1);
}

void FOnDemandIoBackendStats::OnCachePutExisting(uint64 /*DataSize*/)
{
	GCachePutExistingCount.Add(1);
}

void FOnDemandIoBackendStats::OnCachePutReject(uint64 DataSize)
{
	GCachePutRejectCount.Add(1);
	GCacheRejectBytes.Add(DataSize);
}

void FOnDemandIoBackendStats::OnCachePendingBytes(uint64 TotalSize)
{
	GCachePendingBytes.Set(TotalSize);
}

void FOnDemandIoBackendStats::OnCachePersistedBytes(uint64 TotalSize)
{
	GCacheCachedBytes.Set(TotalSize);
}

void FOnDemandIoBackendStats::OnCacheWriteBytes(uint64 WriteSize)
{
	GCacheWrittenBytes.Add(WriteSize);
}

void FOnDemandIoBackendStats::OnCacheSetMaxBytes(uint64 TotalSize)
{
	GCacheMaxBytes = TotalSize;
}

void FOnDemandIoBackendStats::OnHttpConnected()
{
	GHttpConnectCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpDisconnected()
{
	GHttpDisconnectCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpEnqueue()
{
	GHttpPendingCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpDequeue()
{
	GHttpInflightCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpGet(uint64 SizeBytes, uint64 DurationMs)
{
	GHttpPendingCount.Add(-1);
	GHttpInflightCount.Add(-1);
	GHttpGetCount.Add(1);
	GHttpDownloadedBytes.Add(SizeBytes);
	GHttpDurationMsSum += DurationMs;
	GHttpDurationMs.Set(DurationMs);

	int64 OldDuration = GHttpHistoryDuration[GHttpHistoryIndex];
	int64 NewDuration = (int64)DurationMs;
	
	GHttpHistoryTotalDuration -= OldDuration;
	GHttpHistoryTotalDuration += NewDuration;
	GHttpHistoryDuration[GHttpHistoryIndex] = NewDuration;

	GHttpHistoryTotalBytes -= GHttpHistoryBytes[GHttpHistoryIndex];
	GHttpHistoryTotalBytes += SizeBytes;
	GHttpHistoryBytes[GHttpHistoryIndex] = SizeBytes;

	GHttpBandwidthMpbs.Set((GHttpHistoryTotalBytes*8)/(GHttpHistoryTotalDuration+1)/1000);
	GHttpDurationMsAvg.Set(GHttpHistoryTotalDuration/GHttpHistoryCount);

	if (NewDuration > GHttpDurationMsMax.Get())
	{
		GHttpDurationMsMax.Set(NewDuration);
	}
	else if (OldDuration == GHttpDurationMsMax.Get())
	{
		GHttpDurationMsMax.Set(*Algo::MaxElement(GHttpHistoryDuration));
	}

	GHttpHistoryIndex = (GHttpHistoryIndex + 1) % GHttpHistoryCount;

	GHttpAvgDuration.Increment(static_cast<double>(DurationMs));

	GHttpDurationBuckets[FindDurationBucket(DurationMs)]++;
}

void FOnDemandIoBackendStats::OnHttpCancel()
{
	GHttpInflightCount.Add(-1);
	GHttpPendingCount.Add(-1);
	GHttpCancelCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpRetry()
{
	GHttpRetryCount.Add(1);
}

void FOnDemandIoBackendStats::OnHttpError()
{
	GHttpPendingCount.Add(-1);
	GHttpInflightCount.Add(-1);
	GHttpErrorCount.Add(1);
}

} // namespace UE::IO::IAS

#endif // IAS_WITH_STATISTICS
