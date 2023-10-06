// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include <atomic>

#if COUNTERSTRACE_ENABLED || CSV_PROFILER
#	define IAS_WITH_STATISTICS 1
#else
#	define IAS_WITH_STATISTICS 0
#endif

namespace UE::IO::Private
{

#if IAS_WITH_STATISTICS
#	define IAS_STATISTICS_IMPL(...) ;
#else
#	define IAS_STATISTICS_IMPL(...) { return __VA_ARGS__; }
#endif

class FOnDemandIoBackendStats
{
public:
	static FOnDemandIoBackendStats* Get() IAS_STATISTICS_IMPL(nullptr)
	FOnDemandIoBackendStats() IAS_STATISTICS_IMPL()
	~FOnDemandIoBackendStats() IAS_STATISTICS_IMPL()
	void OnIoRequestEnqueue() IAS_STATISTICS_IMPL()
	void OnIoRequestComplete(uint64 RequestSize) IAS_STATISTICS_IMPL()
	void OnIoRequestCancel() IAS_STATISTICS_IMPL()
	void OnIoRequestFail() IAS_STATISTICS_IMPL()
	void OnChunkRequestCreate() IAS_STATISTICS_IMPL()
	void OnChunkRequestRelease() IAS_STATISTICS_IMPL()

	void OnCacheError() IAS_STATISTICS_IMPL()
	void OnCacheGet(uint64 DataSize) IAS_STATISTICS_IMPL()
	void OnCachePut() IAS_STATISTICS_IMPL()
	void OnCachePutExisting(uint64 DataSize) IAS_STATISTICS_IMPL()
	void OnCachePutReject(uint64 DataSize) IAS_STATISTICS_IMPL()
	void OnCachePendingBytes(uint64 TotalSize) IAS_STATISTICS_IMPL()
	void OnCachePersistedBytes(uint64 TotalSize) IAS_STATISTICS_IMPL()

	void OnHttpRequestEnqueue() IAS_STATISTICS_IMPL()
	void OnHttpRequestDequeue() IAS_STATISTICS_IMPL()
	void OnHttpRequestComplete(uint64 InSize) IAS_STATISTICS_IMPL()
	void OnHttpRequestFail() IAS_STATISTICS_IMPL()
};

#undef IAS_STATISTICS_IMPL

} // namespace UE::IO::Private
