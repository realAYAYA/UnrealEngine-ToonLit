// Copyright Epic Games, Inc. All Rights Reserved.

#include "Statistics.h"

#if IAS_WITH_STATISTICS

namespace UE::IO::Private
{

////////////////////////////////////////////////////////////////////////////////
static int32 BytesToApproxMB(uint64 Bytes) { return int32(Bytes >> 20); }
static int32 BytesToApproxKB(uint64 Bytes) { return int32(Bytes >> 10); }

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
FCounterInt			GIoRequestsMade(TEXT("Ias/IoRequestsMade"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GIoRequestsCompleted(TEXT("Ias/IoRequestsCompleted"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GIoRequestsCompletedSize(TEXT("Ias/Size/IoRequestsCompletedSize"), TraceCounterDisplayHint_Memory);
FCounterInt			GIoRequestsCancelled(TEXT("Ias/IoRequestsCancelled"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GIoRequestsFailed(TEXT("Ias/IoRequestsFailed"), TraceCounterDisplayHint_None);
// chunkrequest stats
FCounterInt			GReadRequestsCreated(TEXT("Ias/ReadRequestsCreated"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GReadRequestsRemoved(TEXT("Ias/ReadRequestsRemoved"), TraceCounterDisplayHint_None);
// cache stats
FCounterAtomicInt	GCacheErrorCount(TEXT("Ias/CacheErrorCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCacheGetCount(TEXT("Ias/CacheGetCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCachePutCount(TEXT("Ias/CachePutCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCachePutExistingCount(TEXT("Ias/CachePutExistingCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCachePutRejectCount(TEXT("Ias/CachePutRejectCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GCacheCachedBytes(TEXT("Ias/CacheCachedBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt	GCachePendingBytes(TEXT("Ias/CachePendingBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt	GCacheReadBytes(TEXT("Ias/CacheReadBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt	GCacheRejectBytes(TEXT("Ias/CachePutRejectBytes"), TraceCounterDisplayHint_Memory);
// http stats
FCounterInt			GHttpRequestsCompleted(TEXT("Ias/HttpRequestsCompleted"), TraceCounterDisplayHint_None);
FCounterInt			GHttpRequestsFailed(TEXT("Ias/HttpRequestsFailed"), TraceCounterDisplayHint_None);
FCounterAtomicInt	GHttpRequestsPending(TEXT("Ias/HttpRequestsPending"), TraceCounterDisplayHint_None);
FCounterInt			GHttpRequestsInflight(TEXT("Ias/HttpRequestsInflight"), TraceCounterDisplayHint_None);
FCounterInt			GHttpRequestsCompletedSize(TEXT("Ias/Size/HttpRequestsCompletedSize"), TraceCounterDisplayHint_Memory);

////////////////////////////////////////////////////////////////////////////////
// CSV STATS
CSV_DEFINE_CATEGORY(Ias, true);
// iorequest stats
CSV_DEFINE_STAT(Ias, FrameIoRequestsMade);
CSV_DEFINE_STAT(Ias, FrameIoRequestsCompleted);
CSV_DEFINE_STAT(Ias, FrameIoRequestsCompletedSize);
CSV_DEFINE_STAT(Ias, FrameIoRequestsCancelled);
CSV_DEFINE_STAT(Ias, FrameIoRequestsFailed);
// chunkrequest stats
CSV_DEFINE_STAT(Ias, FrameReadRequestsCreated);
CSV_DEFINE_STAT(Ias, FrameReadRequestsRemoved);
// cache stats
CSV_DEFINE_STAT(Ias, FrameCacheErrorCount);
CSV_DEFINE_STAT(Ias, FrameCacheGetCount);
CSV_DEFINE_STAT(Ias, FrameCachePutCount);
CSV_DEFINE_STAT(Ias, FrameCachePutExistingCount);
CSV_DEFINE_STAT(Ias, FrameCachePutRejectCount);
CSV_DEFINE_STAT(Ias, FrameCacheCachedBytes);
CSV_DEFINE_STAT(Ias, FrameCachePendingBytes);
CSV_DEFINE_STAT(Ias, FrameCacheReadBytes);
CSV_DEFINE_STAT(Ias, FrameCacheRejectBytes);
// http stats
CSV_DEFINE_STAT(Ias, FrameHttpRequestsCompleted);
CSV_DEFINE_STAT(Ias, FrameHttpRequestsFailed);
CSV_DEFINE_STAT(Ias, FrameHttpRequestsPending);
CSV_DEFINE_STAT(Ias, FrameHttpRequestsInflight);
CSV_DEFINE_STAT(Ias, FrameHttpRequestsCompletedSize);

static FOnDemandIoBackendStats* GStatistics = nullptr;

FOnDemandIoBackendStats::FOnDemandIoBackendStats()
{
	check(GStatistics == nullptr);
	GStatistics = this;
}

FOnDemandIoBackendStats::~FOnDemandIoBackendStats()
{
	GStatistics = nullptr;
}

FOnDemandIoBackendStats* FOnDemandIoBackendStats::Get()
{
	return GStatistics;
}

void FOnDemandIoBackendStats::OnIoRequestEnqueue()
{
	GIoRequestsMade.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestsMade, int32(GIoRequestsMade.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestComplete(uint64 RequestSize)
{
	GIoRequestsCompleted.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestsCompleted, int32(GIoRequestsCompleted.Get()), ECsvCustomStatOp::Set);

	GIoRequestsCompletedSize.Add(RequestSize);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestsCompletedSize, BytesToApproxKB(GIoRequestsCompletedSize.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestCancel()
{
	GIoRequestsCancelled.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestsCancelled, int32(GIoRequestsCancelled.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestFail()
{
	GIoRequestsFailed.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestsFailed, int32(GIoRequestsFailed.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnChunkRequestCreate()
{
	GReadRequestsCreated.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameReadRequestsCreated, int32(GReadRequestsCreated.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnChunkRequestRelease()
{
	GReadRequestsRemoved.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameReadRequestsRemoved, int32(GReadRequestsRemoved.Get()), ECsvCustomStatOp::Set);
}


void FOnDemandIoBackendStats::OnCacheError()
{
	GCacheErrorCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameCacheErrorCount, int32(GCacheErrorCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnCacheGet(uint64 DataSize)
{
	GCacheGetCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameCacheGetCount, int32(GCacheGetCount.Get()), ECsvCustomStatOp::Set);
	GCacheReadBytes.Add(DataSize);
	CSV_CUSTOM_STAT_DEFINED(FrameCacheReadBytes, BytesToApproxKB(GCacheReadBytes.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnCachePut()
{
	GCachePutCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameCachePutCount, int32(GCachePutCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnCachePutExisting(uint64 /*DataSize*/)
{
	GCachePutExistingCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameCachePutExistingCount, int32(GCachePutExistingCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnCachePutReject(uint64 DataSize)
{
	GCachePutRejectCount.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameCachePutRejectCount, int32(GCachePutRejectCount.Get()), ECsvCustomStatOp::Set);
	GCacheRejectBytes.Add(DataSize);
	CSV_CUSTOM_STAT_DEFINED(FrameCacheRejectBytes, BytesToApproxKB(GCacheRejectBytes.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnCachePendingBytes(uint64 TotalSize)
{
	GCachePendingBytes.Set(TotalSize);
	CSV_CUSTOM_STAT_DEFINED(FrameCachePendingBytes, BytesToApproxKB(GCachePendingBytes.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnCachePersistedBytes(uint64 TotalSize)
{
	GCacheCachedBytes.Set(TotalSize);
	CSV_CUSTOM_STAT_DEFINED(FrameCacheCachedBytes, BytesToApproxKB(GCacheCachedBytes.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnHttpRequestEnqueue()
{
	GHttpRequestsPending.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameHttpRequestsPending, int32(GHttpRequestsPending.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnHttpRequestDequeue()
{
	GHttpRequestsPending.Add(-1);
	CSV_CUSTOM_STAT_DEFINED(FrameHttpRequestsPending, int32(GHttpRequestsPending.Get()), ECsvCustomStatOp::Set);

	GHttpRequestsInflight.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameHttpRequestsPending, int32(GHttpRequestsInflight.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnHttpRequestComplete(uint64 InSize)
{
	GHttpRequestsInflight.Add(-1);
	CSV_CUSTOM_STAT_DEFINED(FrameHttpRequestsPending, int32(GHttpRequestsInflight.Get()), ECsvCustomStatOp::Set);

	GHttpRequestsCompleted.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameHttpRequestsCompleted, int32(GHttpRequestsCompleted.Get()), ECsvCustomStatOp::Set);

	GHttpRequestsCompletedSize.Add(InSize);
	CSV_CUSTOM_STAT_DEFINED(FrameHttpRequestsCompletedSize, BytesToApproxKB(GHttpRequestsCompletedSize.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnHttpRequestFail()
{
	GHttpRequestsInflight.Add(-1);
	CSV_CUSTOM_STAT_DEFINED(FrameHttpRequestsPending, int32(GHttpRequestsInflight.Get()), ECsvCustomStatOp::Set);

	GHttpRequestsFailed.Add(1);
	CSV_CUSTOM_STAT_DEFINED(FrameHttpRequestsFailed, int32(GHttpRequestsFailed.Get()), ECsvCustomStatOp::Set);
}

} // namespace UE::IO::Private

#endif // IAS_WITH_STATISTICS
