// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "Model/MonotonicTimeline.h"

namespace TraceServices
{

class IAnalysisSession;
class IEditableTimingProfilerProvider;
class IEditableThreadProvider;

class FCpuProfilerAnalyzer
	: public UE::Trace::IAnalyzer
{
public:
	FCpuProfilerAnalyzer(IAnalysisSession& Session, IEditableTimingProfilerProvider& InEditableTimingProfilerProvider, IEditableThreadProvider& InEditableThreadProvider);
	~FCpuProfilerAnalyzer();
	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual void OnAnalysisEnd(/*const FOnAnalysisEndContext& Context*/) override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	struct FEventScopeState
	{
		uint64 StartCycle;
		uint32 EventTypeId;
	};

	struct FPendingEvent
	{
		uint64 Cycle;
		double Time;
		uint32 TimerId;
	};

	struct FThreadState
	{
		uint32 ThreadId = 0;
		TArray<FEventScopeState> ScopeStack;
		TArray<FPendingEvent> PendingEvents;
		IEditableTimeline<FTimingProfilerEvent>* Timeline = nullptr;
		uint64 LastCycle = 0;
		bool bShouldIgnorePendingEvents = false; // becomes true when we detect first pending event with incorrect timestamp (i.e < LastCycle)
	};

	void ProcessBuffer(const FEventTime& EventTime, FThreadState& ThreadState, const uint8* BufferPtr, uint32 BufferSize);
	void ProcessBufferV2(const FEventTime& EventTime, FThreadState& ThreadState, const uint8* BufferPtr, uint32 BufferSize);
	void DispatchPendingEvents(uint64& LastCycle, uint64 CurrentCycle, FThreadState& ThreadState, const FPendingEvent*& PendingCursor, int32& RemainingPending, bool bIsBeginEvent);
	void DispatchRemainingPendingEvents(FThreadState& ThreadState);
	void EndOpenEvents(FThreadState& ThreadState, double Timestamp);
	void OnCpuScopeEnter(const FOnEventContext& Context);
	void OnCpuScopeLeave(const FOnEventContext& Context);
	uint32 DefineTimer(uint32 SpecId, const TCHAR* TimerName, const TCHAR* File, uint32 Line, bool bMergeByName); // returns the TimerId
	uint32 DefineNewTimerChecked(uint32 SpecId, const TCHAR* TimerName, const TCHAR* File = nullptr, uint32 Line = 0); // returns the TimerId
	uint32 GetTimerId(uint32 SpecId);
	FThreadState& GetThreadState(uint32 ThreadId);

	enum : uint16
	{
		RouteId_EventSpec,
		RouteId_EndThread,
		RouteId_EventBatchV2,
		RouteId_EventBatch, // backward compatibility
		RouteId_EndCapture, // backward compatibility
		RouteId_CpuScope,
	};

	IAnalysisSession& Session;
	IEditableTimingProfilerProvider& EditableTimingProfilerProvider;
	IEditableThreadProvider& EditableThreadProvider;
	TMap<uint32, FThreadState*> ThreadStatesMap;
	TMap<uint32, uint32> SpecIdToTimerIdMap;
	TMap<const TCHAR*, uint32, FDefaultSetAllocator, TStringPointerMapKeyFuncs_DEPRECATED<const TCHAR*, uint32>> ScopeNameToTimerIdMap;
	uint32 CoroutineTimerId = ~0;
	uint32 CoroutineUnknownTimerId = ~0;
	uint64 TotalEventSize = 0;
	uint64 TotalScopeCount = 0;
	double BytesPerScope = 0.0;
};

} // namespace TraceServices
