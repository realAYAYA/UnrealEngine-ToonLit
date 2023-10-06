// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/PagedArray.h" // TraceServices
#include "Containers/Map.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/ContextSwitches.h"

namespace TraceServices
{

class FContextSwitchesProvider
	: public IContextSwitchesProvider
{
public:
	explicit FContextSwitchesProvider(IAnalysisSession& Session);
	virtual ~FContextSwitchesProvider();

	virtual bool HasData() const override;
	bool GetSystemThreadId(uint32 ThreadId, uint32& OutSystemThreadId) const override;
	bool GetThreadId(uint32 SystemThreadId, uint32& OutThreadId) const override;
	bool GetSystemThreadId(uint32 CoreNumber, double Time, uint32& OutSystemThreadId) const override;
	bool GetThreadId(uint32 CoreNumber, double Time, uint32& OutThreadId) const override;
	bool GetCoreNumber(uint32 ThreadId, double Time, uint32& OutCoreNumber) const override;
	uint64 GetThreadsSerial() const override { return uint64(Threads.Num()); }
	uint64 GetCpuCoresSerial() const override { return uint64(NumCpuCores); }
	uint32 GetNumCpuCoresWithEvents() const { return NumCpuCores; }
	uint32 GetExclusiveMaxCpuCoreNumber() const { return CpuCores.Num(); }
	void EnumerateCpuCores(CpuCoreCallback Callback) const override;
	void EnumerateContextSwitches(uint32 ThreadId, double StartTime, double EndTime, ContextSwitchCallback Callback) const override;
	void EnumerateCpuCoreEvents(uint32 CoreNumber, double StartTime, double EndTime, CpuCoreEventCallback Callback) const override;
	void EnumerateCpuCoreEventsBackwards(uint32 CoreNumber, double EndTime, double StartTime, CpuCoreEventCallback Callback) const override;

	void Add(uint32 SystemThreadId, double Start, double End, uint32 CoreNumber);
	void AddThreadInfo(uint32 ThreadId, uint32 SystemThreadId);

	void AddThreadName(uint32 SystemTreadId, uint32 SystemProcessId, FStringView Name);

private:
	const TPagedArray<FContextSwitch>* GetContextSwitches(uint32 ThreadId) const;

	IAnalysisSession& Session;

	// (Trace) Thread Id -> System Thread Id
	TMap<uint32, uint32> TraceToSystemThreadIdMap;

	// System Thread Id -> (Trace) Thread Id
	TMap<uint32, uint32> SystemToTraceThreadIdMap;

	// System Thread Id -> PagedArray
	TMap<uint32, TPagedArray<FContextSwitch>*> Threads;

	// [Core Number] -> PagedArray; some can be nullptr
	TArray<TPagedArray<FCpuCoreEvent>*> CpuCores;
	uint32 NumCpuCores;
};

} // namespace TraceServices
