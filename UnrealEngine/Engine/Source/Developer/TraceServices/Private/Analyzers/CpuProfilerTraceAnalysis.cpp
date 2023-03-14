// Copyright Epic Games, Inc. All Rights Reserved.

#include "CpuProfilerTraceAnalysis.h"

#include "AnalysisServicePrivate.h"
#include "CborWriter.h"
#include "Common/Utils.h"
#include "HAL/LowLevelMemTracker.h"
#include "Model/ThreadsPrivate.h"
#include "Model/MonotonicTimeline.h"
#include "Serialization/MemoryWriter.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "TraceServices/Utils.h"

#define CPUPROFILER_DEBUG_LOGF(Format, ...) //{ if (ThreadState.ThreadId == 2) FPlatformMisc::LowLevelOutputDebugStringf(Format, __VA_ARGS__); }
#define CPUPROFILER_DEBUG_BEGIN_EVENT(Time, Event) { ++TotalScopeCount; }
#define CPUPROFILER_DEBUG_END_EVENT(Time)

#define CPUPROFILER_SAFE_DISPATCH_PENDING_EVENTS 1

namespace TraceServices
{

////////////////////////////////////////////////////////////////////////////////////////////////////

FCpuProfilerAnalyzer::FCpuProfilerAnalyzer(IAnalysisSession& InSession, IEditableTimingProfilerProvider& InEditableTimingProfilerProvider, IEditableThreadProvider& InEditableThreadProvider)
	: Session(InSession)
	, EditableTimingProfilerProvider(InEditableTimingProfilerProvider)
	, EditableThreadProvider(InEditableThreadProvider)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FCpuProfilerAnalyzer::~FCpuProfilerAnalyzer()
{
	for (auto& KV : ThreadStatesMap)
	{
		FThreadState* ThreadState = KV.Value;
		delete ThreadState;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuProfilerAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	Builder.RouteLoggerEvents(RouteId_CpuScope, "Cpu", true);
	Builder.RouteEvent(RouteId_EventSpec, "CpuProfiler", "EventSpec");
	Builder.RouteEvent(RouteId_EventBatch, "CpuProfiler", "EventBatch");
	Builder.RouteEvent(RouteId_EndThread, "CpuProfiler", "EndThread");
	Builder.RouteEvent(RouteId_EndCapture, "CpuProfiler", "EndCapture");
	Builder.RouteEvent(RouteId_EventBatchV2, "CpuProfiler", "EventBatchV2");
	Builder.RouteEvent(RouteId_EndCaptureV2, "CpuProfiler", "EndCaptureV2");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuProfilerAnalyzer::OnAnalysisEnd(/*const FOnAnalysisEndContext& Context*/)
{
#if 0
	LLM_SCOPE_BYNAME(TEXT("Insights/FCpuProfilerAnalyzer"));

	//TODO: Context.EventTime
	for (auto& KV : ThreadStatesMap)
	{
		FThreadState& ThreadState = *KV.Value;

		//TODO: EndThread(Context.EventTime, ThreadState);

		ensure(ThreadState.PendingEvents.Num() == 0);

		if (ThreadState.LastCycle != 0 && ThreadState.LastCycle != ~0)
		{
			//double Timestamp = Context.EventTime.AsSeconds(ThreadState.LastCycle);
			//Session.UpdateDurationSeconds(Timestamp);
			double Timestamp;
			{
				FAnalysisSessionEditScope _(Session);
				Timestamp = Session.GetDurationSeconds();
			}
			while (ThreadState.ScopeStack.Num())
			{
				ThreadState.ScopeStack.Pop();
				CPUPROFILER_DEBUG_LOGF(TEXT("[%u] ~E=%llu (%.9f)\n"), ThreadState.ThreadId, ThreadState.LastCycle, Timestamp);
				ThreadState.Timeline->AppendEndEvent(Timestamp);
				CPUPROFILER_DEBUG_END_EVENT(Timestamp);
			}
		}

		ensure(ThreadState.ScopeStack.Num() == 0);
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FCpuProfilerAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FCpuProfilerAnalyzer"));

	FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
	case RouteId_EventSpec:
	{
		uint32 SpecId = EventData.GetValue<uint32>("Id");

		const TCHAR* TimerName = nullptr;
		FString Name;
		if (EventData.GetString("Name", Name))
		{
			TimerName = *Name;
		}
		else
		{
			uint8 CharSize = EventData.GetValue<uint8>("CharSize");
			if (CharSize == sizeof(ANSICHAR))
			{
				const ANSICHAR* AnsiName = reinterpret_cast<const ANSICHAR*>(EventData.GetAttachment());
				Name = StringCast<TCHAR>(AnsiName).Get();
				TimerName = *Name;
			}
			else if (CharSize == 0 || CharSize == sizeof(TCHAR)) // 0 for backwards compatibility
			{
				TimerName = reinterpret_cast<const TCHAR*>(EventData.GetAttachment());
			}
			else
			{
				Name = FString::Printf(TEXT("<invalid %u>"), SpecId);
				TimerName = *Name;
			}
		}

		if (TimerName[0] == 0)
		{
			Name = FString::Printf(TEXT("<noname %u>"), SpecId);
			TimerName = *Name;
		}

		const TCHAR* FileName = nullptr;
		FString File;
		uint32 Line = 0;
		if (EventData.GetString("File", File) && !File.IsEmpty())
		{
			FileName = *File;
			Line = EventData.GetValue<uint32>("Line");
		}

		constexpr bool bMergeByName = true;
		DefineTimer(SpecId, Session.StoreString(TimerName), FileName, Line, bMergeByName);
		break;
	}

	case RouteId_EndThread:
	{
		const uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);

		if (ThreadState.LastCycle == ~0ull)
		{
			// Ignore timing events received after EndThread.
			break;
		}

#if CPUPROFILER_SAFE_DISPATCH_PENDING_EVENTS
		int32 RemainingPending = ThreadState.PendingEvents.Num();
		if (RemainingPending > 0)
		{
			uint64 LastCycle = ThreadState.LastCycle;
			const FPendingEvent* PendingCursor = ThreadState.PendingEvents.GetData();
			DispatchPendingEvents(LastCycle, ~0ull, Context.EventTime, ThreadState, PendingCursor, RemainingPending);
			check(RemainingPending == 0);
			ThreadState.PendingEvents.Reset();
		}
#endif

		ensure(ThreadState.LastCycle != 0 || ThreadState.ScopeStack.Num() == 0);
		if (ThreadState.LastCycle != 0)
		{
			double Timestamp = Context.EventTime.AsSeconds(ThreadState.LastCycle);
			Session.UpdateDurationSeconds(Timestamp);
			while (ThreadState.ScopeStack.Num())
			{
				ThreadState.ScopeStack.Pop();
				CPUPROFILER_DEBUG_LOGF(TEXT("[%u] ^E=%llu (%.9f)\n"), ThreadState.ThreadId, ThreadState.LastCycle, Timestamp);
				ThreadState.Timeline->AppendEndEvent(Timestamp);
				CPUPROFILER_DEBUG_END_EVENT(Timestamp);
			}
		}

		ThreadState.LastCycle = ~0ull;
		break;
	}

	case RouteId_EventBatch:
	case RouteId_EndCapture:
	case RouteId_EventBatchV2:
	case RouteId_EndCaptureV2:
	{
		const uint32 ThreadId = FTraceAnalyzerUtils::GetThreadIdField(Context);
		FThreadState& ThreadState = GetThreadState(ThreadId);

		if (ThreadState.LastCycle == ~0ull)
		{
			// Ignore timing events received after EndThread.
			break;
		}

		TArrayView<const uint8> DataView = FTraceAnalyzerUtils::LegacyAttachmentArray("Data", Context);
		const uint32 BufferSize = DataView.Num();
		const uint8* BufferPtr = DataView.GetData();

		uint64 LastCycle;
		if (RouteId == RouteId_EventBatch || RouteId == RouteId_EndCapture)
		{
			LastCycle = ProcessBuffer(Context.EventTime, ThreadState, BufferPtr, BufferSize);
		}
		else
		{
			LastCycle = ProcessBufferV2(Context.EventTime, ThreadState, BufferPtr, BufferSize);
		}

		if (LastCycle)
		{
			double LastTimestamp = Context.EventTime.AsSeconds(LastCycle);
			Session.UpdateDurationSeconds(LastTimestamp);
			if (RouteId == RouteId_EndCapture || RouteId == RouteId_EndCaptureV2)
			{
				for (int32 i = ThreadState.ScopeStack.Num(); i--;)
				{
					ThreadState.ScopeStack.Pop();
					CPUPROFILER_DEBUG_LOGF(TEXT("[%u] -E=%llu (%.9f)\n"), ThreadState.ThreadId, LastCycle, LastTimestamp);
					ThreadState.Timeline->AppendEndEvent(LastTimestamp);
					CPUPROFILER_DEBUG_END_EVENT(LastTimestamp);
				}
			}
		}

		TotalEventSize += BufferSize;
		BytesPerScope = double(TotalEventSize) / double(TotalScopeCount);
		break;
	}

	case RouteId_CpuScope:
		if (Style == EStyle::EnterScope)
		{
			OnCpuScopeEnter(Context);
		}
		else
		{
			OnCpuScopeLeave(Context);
		}
		break;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint64 FCpuProfilerAnalyzer::ProcessBuffer(const FEventTime& EventTime, FThreadState& ThreadState, const uint8* BufferPtr, uint32 BufferSize)
{
	uint64 LastCycle = ThreadState.LastCycle;

	CPUPROFILER_DEBUG_LOGF(TEXT("[%u] ProcessBuffer %llu (%.9f)\n"), ThreadState.ThreadId, LastCycle, EventTime.AsSeconds(LastCycle));

	check(EventTime.GetTimestamp() == 0);
	const uint64 BaseCycle = EventTime.AsCycle64();

	int32 RemainingPending = ThreadState.PendingEvents.Num();
	const FPendingEvent* PendingCursor = ThreadState.PendingEvents.GetData();

	const uint8* BufferEnd = BufferPtr + BufferSize;
	while (BufferPtr < BufferEnd)
	{
		uint64 DecodedCycle = FTraceAnalyzerUtils::Decode7bit(BufferPtr);
		uint64 ActualCycle = (DecodedCycle >> 1);

		// ActualCycle larger or equal to LastCycle means we have a new
		// base value.
		if (ActualCycle < LastCycle)
		{
			ActualCycle += LastCycle;
		}

		// If we late connect we will be joining the cycle stream mid-flow and
		// will have missed out on it's base timestamp. Reconstruct it here.
		if (ActualCycle < BaseCycle)
		{
			ActualCycle += BaseCycle;
		}

		// Dispatch pending events that are younger than the one we've just decoded.
		DispatchPendingEvents(LastCycle, ActualCycle, EventTime, ThreadState, PendingCursor, RemainingPending);

		double ActualTime = EventTime.AsSeconds(ActualCycle);

		if (DecodedCycle & 1ull)
		{
			uint32 SpecId = IntCastChecked<uint32>(FTraceAnalyzerUtils::Decode7bit(BufferPtr));
			uint32 TimerId = GetTimerId(SpecId);

			FEventScopeState& ScopeState = ThreadState.ScopeStack.AddDefaulted_GetRef();
			ScopeState.StartCycle = ActualCycle;
			ScopeState.EventTypeId = TimerId;

			CPUPROFILER_DEBUG_LOGF(TEXT("[%u]  B=%llu (%.9f)\n"), ThreadState.ThreadId, ActualCycle, ActualTime);
			FTimingProfilerEvent Event;
			Event.TimerIndex = TimerId;
			ThreadState.Timeline->AppendBeginEvent(ActualTime, Event);
			CPUPROFILER_DEBUG_BEGIN_EVENT(ActualTime, Event);
		}
		else
		{
			// If we receive mismatched end events ignore them for now.
			// This can happen for example because tracing connects to the store after events were traced. Those events can be lost.
			if (ThreadState.ScopeStack.Num() > 0)
			{
				ThreadState.ScopeStack.Pop();
				CPUPROFILER_DEBUG_LOGF(TEXT("[%u]  E=%llu (%.9f)\n"), ThreadState.ThreadId, ActualCycle, ActualTime);
				ThreadState.Timeline->AppendEndEvent(ActualTime);
				CPUPROFILER_DEBUG_END_EVENT(ActualTime);
			}
		}

		check(ActualCycle > 0);
		LastCycle = ActualCycle;
	}
	check(BufferPtr == BufferEnd);

#if CPUPROFILER_SAFE_DISPATCH_PENDING_EVENTS
	if (RemainingPending == 0)
	{
		//CPUPROFILER_DEBUG_LOGF(TEXT("[%u] MetaEvents: %d added\n"), ThreadState.ThreadId, ThreadState.PendingEvents.Num());
		ThreadState.PendingEvents.Reset();
	}
	else
	{
		const int32 NumEventsToRemove = ThreadState.PendingEvents.Num() - RemainingPending;
		CPUPROFILER_DEBUG_LOGF(TEXT("[%u] MetaEvents: %d added, %d still pending\n"), ThreadState.ThreadId, NumEventsToRemove, RemainingPending);
		ThreadState.PendingEvents.RemoveAt(0, NumEventsToRemove);
	}
#else
	if (RemainingPending > 0)
	{
		DispatchPendingEvents(LastCycle, ~0ull, EventTime, ThreadState, PendingCursor, RemainingPending);
		check(RemainingPending == 0);
		ThreadState.PendingEvents.Reset();
	}
#endif

	ThreadState.LastCycle = LastCycle;
	return LastCycle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint64 FCpuProfilerAnalyzer::ProcessBufferV2(const FEventTime& EventTime, FThreadState& ThreadState, const uint8* BufferPtr, uint32 BufferSize)
{
	uint64 LastCycle = ThreadState.LastCycle;

	CPUPROFILER_DEBUG_LOGF(TEXT("[%u] ProcessBuffer %llu (%.9f)\n"), ThreadState.ThreadId, LastCycle, EventTime.AsSeconds(LastCycle));

	check(EventTime.GetTimestamp() == 0);
	const uint64 BaseCycle = EventTime.AsCycle64();

	int32 RemainingPending = ThreadState.PendingEvents.Num();
	const FPendingEvent* PendingCursor = ThreadState.PendingEvents.GetData();

	const uint8* BufferEnd = BufferPtr + BufferSize;
	while (BufferPtr < BufferEnd)
	{
		uint64 DecodedCycle = FTraceAnalyzerUtils::Decode7bit(BufferPtr);
		uint64 ActualCycle = (DecodedCycle >> 2);

		// ActualCycle larger or equal to LastCycle means we have a new
		// base value.
		if (ActualCycle < LastCycle)
		{
			ActualCycle += LastCycle;
		}

		// If we late connect we will be joining the cycle stream mid-flow and
		// will have missed out on it's base timestamp. Reconstruct it here.
		if (ActualCycle < BaseCycle)
		{
			ActualCycle += BaseCycle;
		}

		// Dispatch pending events that are younger than the one we've just decoded.
		DispatchPendingEvents(LastCycle, ActualCycle, EventTime, ThreadState, PendingCursor, RemainingPending);

		double ActualTime = EventTime.AsSeconds(ActualCycle);

		if (DecodedCycle & 2ull)
		{
			constexpr uint32 CoroutineSpecId        = (1u << 31u) - 1u;
			constexpr uint32 CoroutineUnknownSpecId = (1u << 31u) - 2u;

			if (DecodedCycle & 1ull)
			{
				uint64 CoroutineId = FTraceAnalyzerUtils::Decode7bit(BufferPtr);
				uint32 TimerScopeDepth = IntCastChecked<uint32>(FTraceAnalyzerUtils::Decode7bit(BufferPtr));

				// Begins a "CoroTask" scoped timer.
				{
					if (CoroutineTimerId == ~0)
					{
						CoroutineTimerId = DefineNewTimerChecked(CoroutineSpecId, TEXT("Coroutine"));
					}

					TArray<uint8> CborData;
					{
						CborData.Reserve(256);
						FMemoryWriter MemoryWriter(CborData, false, true);
						FCborWriter CborWriter(&MemoryWriter, ECborEndianness::StandardCompliant);
						CborWriter.WriteContainerStart(ECborCode::Map, 2); // 2 is the FieldCount
						CborWriter.WriteValue("Id", 2);
						CborWriter.WriteValue(CoroutineId);
						CborWriter.WriteValue("C", 1); // continuation?
						CborWriter.WriteValue(false);
					}
					uint32 MetadataTimerId = EditableTimingProfilerProvider.AddMetadata(CoroutineTimerId, MoveTemp(CborData));

					FEventScopeState& ScopeState = ThreadState.ScopeStack.AddDefaulted_GetRef();
					ScopeState.StartCycle = ActualCycle;
					ScopeState.EventTypeId = MetadataTimerId;

					CPUPROFILER_DEBUG_LOGF(TEXT("[%u] *B=%llu (%.9f)\n"), ThreadState.ThreadId, ActualCycle, ActualTime);
					FTimingProfilerEvent Event;
					Event.TimerIndex = MetadataTimerId;
					ThreadState.Timeline->AppendBeginEvent(ActualTime, Event);
					CPUPROFILER_DEBUG_BEGIN_EVENT(ActualTime, Event);
				}

				// Begins the cpu scoped timers (suspended in previous coroutine execution).
				{
					if (CoroutineUnknownTimerId == ~0)
					{
						CoroutineUnknownTimerId = DefineNewTimerChecked(CoroutineUnknownSpecId, TEXT("<unknown>"));
					}

					//TODO: Restore the saved stack of cpu scoped timers for this CoroutineId.
					for (uint32 i = 0; i < TimerScopeDepth; ++i)
					{
						FEventScopeState& ScopeState = ThreadState.ScopeStack.AddDefaulted_GetRef();
						ScopeState.StartCycle = ActualCycle;
						ScopeState.EventTypeId = CoroutineUnknownTimerId;

						CPUPROFILER_DEBUG_LOGF(TEXT("[%u] +B=%llu (%.9f)\n"), ThreadState.ThreadId, ActualCycle, ActualTime);
						FTimingProfilerEvent Event;
						Event.TimerIndex = CoroutineUnknownTimerId;
						ThreadState.Timeline->AppendBeginEvent(ActualTime, Event);
						CPUPROFILER_DEBUG_BEGIN_EVENT(ActualTime, Event);
					}
				}
			}
			else
			{
				uint32 TimerScopeDepth = IntCastChecked<uint32>(FTraceAnalyzerUtils::Decode7bit(BufferPtr));

				if (TimerScopeDepth != 0)
				{
					//TODO: Save current stack of cpu scoped timers (using id from metadata of CoroTask timer?)

					// Ends (suspends) the cpu scoped timers.
					for (uint32 i = 0; i < TimerScopeDepth; ++i)
					{
						// If we receive mismatched end events ignore them for now.
						// This can happen for example because tracing connects to the store after events were traced. Those events can be lost.
						if (ThreadState.ScopeStack.Num() > 0)
						{
							ThreadState.ScopeStack.Pop();
							CPUPROFILER_DEBUG_LOGF(TEXT("[%u] +E=%llu (%.9f)\n"), ThreadState.ThreadId, ActualCycle, ActualTime);
							ThreadState.Timeline->AppendEndEvent(ActualTime);
							CPUPROFILER_DEBUG_END_EVENT(ActualTime);
						}
					}

					// Update the "continuation" (suspended or destroyed) metadata flag.
					if (ThreadState.ScopeStack.Num() > 0)
					{
						uint32 MetadataTimerId = ThreadState.ScopeStack.Top().EventTypeId;
						TArrayView<const uint8> Metadata = EditableTimingProfilerProvider.GetMetadata(MetadataTimerId);
						// Change the last byte in metadata to "true".
						const_cast<uint8*>(Metadata.GetData())[Metadata.Num() - 1] = (uint8)(ECborCode::Prim | ECborCode::True);
					}
				}

				// Ends the "CoroTask" scoped timer.
				{
					// If we receive mismatched end events ignore them for now.
					// This can happen for example because tracing connects to the store after events were traced. Those events can be lost.
					if (ThreadState.ScopeStack.Num() > 0)
					{
						ThreadState.ScopeStack.Pop();
						CPUPROFILER_DEBUG_LOGF(TEXT("[%u] *E=%llu (%.9f)\n"), ThreadState.ThreadId, ActualCycle, ActualTime);
						ThreadState.Timeline->AppendEndEvent(ActualTime);
						CPUPROFILER_DEBUG_END_EVENT(ActualTime);
					}
				}
			}
		}
		else
		{
			if (DecodedCycle & 1ull)
			{
				uint32 SpecId = IntCastChecked<uint32>(FTraceAnalyzerUtils::Decode7bit(BufferPtr));
				uint32 TimerId = GetTimerId(SpecId);

				FEventScopeState& ScopeState = ThreadState.ScopeStack.AddDefaulted_GetRef();
				ScopeState.StartCycle = ActualCycle;
				ScopeState.EventTypeId = TimerId;

				CPUPROFILER_DEBUG_LOGF(TEXT("[%u]  B=%llu (%.9f)\n"), ThreadState.ThreadId, ActualCycle, ActualTime);
				FTimingProfilerEvent Event;
				Event.TimerIndex = TimerId;
				ThreadState.Timeline->AppendBeginEvent(ActualTime, Event);
				CPUPROFILER_DEBUG_BEGIN_EVENT(ActualTime, Event);
			}
			else
			{
				// If we receive mismatched end events ignore them for now.
				// This can happen for example because tracing connects to the store after events were traced. Those events can be lost.
				if (ThreadState.ScopeStack.Num() > 0)
				{
					ThreadState.ScopeStack.Pop();
					CPUPROFILER_DEBUG_LOGF(TEXT("[%u]  E=%llu (%.9f)\n"), ThreadState.ThreadId, ActualCycle, ActualTime);
					ThreadState.Timeline->AppendEndEvent(ActualTime);
					CPUPROFILER_DEBUG_END_EVENT(ActualTime);
				}
			}
		}

		check(ActualCycle > 0);
		LastCycle = ActualCycle;
	}
	check(BufferPtr == BufferEnd);

#if CPUPROFILER_SAFE_DISPATCH_PENDING_EVENTS
	if (RemainingPending == 0)
	{
		//CPUPROFILER_DEBUG_LOGF(TEXT("[%u] MetaEvents: %d added\n"), ThreadState.ThreadId, ThreadState.PendingEvents.Num());
		ThreadState.PendingEvents.Reset();
	}
	else
	{
		const int32 NumEventsToRemove = ThreadState.PendingEvents.Num() - RemainingPending;
		CPUPROFILER_DEBUG_LOGF(TEXT("[%u] MetaEvents: %d added, %d still pending\n"), ThreadState.ThreadId, NumEventsToRemove, RemainingPending);
		ThreadState.PendingEvents.RemoveAt(0, NumEventsToRemove);
	}
#else
	if (RemainingPending > 0)
	{
		DispatchPendingEvents(LastCycle, ~0ull, EventTime, ThreadState, PendingCursor, RemainingPending);
		check(RemainingPending == 0);
		ThreadState.PendingEvents.Reset();
	}
#endif

	ThreadState.LastCycle = LastCycle;
	return LastCycle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuProfilerAnalyzer::DispatchPendingEvents(
	uint64& LastCycle,
	uint64 CurrentCycle,
	const FEventTime& EventTime,
	FThreadState& ThreadState,
	const FPendingEvent*& PendingCursor,
	int32& RemainingPending)
{
	for (; RemainingPending > 0; RemainingPending--, PendingCursor++)
	{
		bool bEnter = true;
		uint64 PendingCycle = PendingCursor->Cycle;
		if (int64(PendingCycle) < 0)
		{
			PendingCycle = ~PendingCycle;
			bEnter = false;
		}

		if (PendingCycle > CurrentCycle)
		{
			break;
		}

#if CPUPROFILER_SAFE_DISPATCH_PENDING_EVENTS
		if (PendingCycle >= LastCycle)
		{
			// Update LastCycle in order to verify time (of following pending events) increases monotonically.
			LastCycle = PendingCycle;
		}
		else
		{
			// Time needs to increase monotonically.
			// We are not allowing events to "go back in time".
			PendingCycle = LastCycle;
		}
#else
		if (PendingCycle < LastCycle)
		{
			PendingCycle = LastCycle;
		}
#endif

		double PendingTime = EventTime.AsSeconds(PendingCycle);

		if (bEnter)
		{
			CPUPROFILER_DEBUG_LOGF(TEXT("[%u] >B=%llu (%.9f)\n"), ThreadState.ThreadId, PendingCycle, PendingTime);
			FTimingProfilerEvent Event;
			Event.TimerIndex = PendingCursor->TimerId;
			ThreadState.Timeline->AppendBeginEvent(PendingTime, Event);
			CPUPROFILER_DEBUG_BEGIN_EVENT(PendingTime, Event);
		}
		else
		{
			CPUPROFILER_DEBUG_LOGF(TEXT("[%u] >E=%llu (%.9f)\n"), ThreadState.ThreadId, PendingCycle, PendingTime);
			ThreadState.Timeline->AppendEndEvent(PendingTime);
			CPUPROFILER_DEBUG_END_EVENT(PendingTime);
		}
	}

	ThreadState.LastCycle = LastCycle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuProfilerAnalyzer::OnCpuScopeEnter(const FOnEventContext& Context)
{
	if (Context.EventTime.GetTimestamp() == 0)
	{
		return;
	}

	uint32 ThreadId = Context.ThreadInfo.GetId();
	FThreadState& ThreadState = GetThreadState(ThreadId);

	uint32 SpecId = Context.EventData.GetTypeInfo().GetId();
	SpecId = ~SpecId; // to keep out of the way of normal spec IDs.

	uint32 TimerId;
	uint32* TimerIdIter = SpecIdToTimerIdMap.Find(SpecId);
	if (TimerIdIter)
	{
		TimerId = *TimerIdIter;
	}
	else
	{
		FString ScopeName;
		ScopeName += Context.EventData.GetTypeInfo().GetName();
		constexpr bool bMergeByName = true;
		TimerId = DefineTimer(SpecId, Session.StoreString(*ScopeName), nullptr, 0, bMergeByName);
	}

	TArray<uint8> CborData;
	Context.EventData.SerializeToCbor(CborData);
	TimerId = EditableTimingProfilerProvider.AddMetadata(TimerId, MoveTemp(CborData));

	uint64 Cycle = Context.EventTime.AsCycle64();
	ThreadState.PendingEvents.Add({Cycle, TimerId});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FCpuProfilerAnalyzer::OnCpuScopeLeave(const FOnEventContext& Context)
{
	if (Context.EventTime.GetTimestamp() == 0)
	{
		return;
	}

	uint32 ThreadId = Context.ThreadInfo.GetId();
	FThreadState& ThreadState = GetThreadState(ThreadId);

	uint64 Cycle = Context.EventTime.AsCycle64();
	ThreadState.PendingEvents.Add({~Cycle});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FCpuProfilerAnalyzer::DefineTimer(uint32 SpecId, const TCHAR* Name, const TCHAR* File, uint32 Line, bool bMergeByName)
{
	// The cpu scoped events (timers) will be merged by name.
	// Ex.: If there are multiple timers defined in code with same name,
	//      those will appear in Insights as a single timer.

	// Check if a timer with same name was already defined.
	uint32* FindTimerIdByName = bMergeByName ? ScopeNameToTimerIdMap.Find(Name) : nullptr;
	if (FindTimerIdByName)
	{
		// Yes, a timer with same name was already defined.
		// Check if SpecId is already mapped to timer.
		uint32* FindTimerId = SpecIdToTimerIdMap.Find(SpecId);
		if (FindTimerId)
		{
			// Yes, SpecId was already mapped to a timer (ex. as an <unknown> timer).
			// Update name for mapped timer.
			EditableTimingProfilerProvider.SetTimerNameAndLocation(*FindTimerId, Name, File, Line);
			// In this case, we do not remap the SpecId to the previously defined timer with same name.
			// This is becasue the two timers are already used in timelines.
			// So we will continue to use separate timers, even if those have same name.
			return *FindTimerId;
		}
		else
		{
			// Map this SpecId to the previously defined timer with same name.
			SpecIdToTimerIdMap.Add(SpecId, *FindTimerIdByName);
			return *FindTimerIdByName;
		}
	}
	else
	{
		// No, a timer with same name was not defined (or we do not want to merge by name).
		// Check if SpecId is already mapped to timer.
		uint32* FindTimerId = SpecIdToTimerIdMap.Find(SpecId);
		if (FindTimerId)
		{
			// Yes, SpecId was already mapped to a timer (ex. as an <unknown> timer).
			// Update name for mapped timer.
			EditableTimingProfilerProvider.SetTimerNameAndLocation(*FindTimerId, Name, File, Line);
			if (bMergeByName)
			{
				// Map the name to the timer.
				ScopeNameToTimerIdMap.Add(Name, *FindTimerId);
			}
			return *FindTimerId;
		}
		else
		{
			// Define a new Cpu timer.
			uint32 NewTimerId = EditableTimingProfilerProvider.AddCpuTimer(Name, File, Line);
			// Map the SpecId to the timer.
			SpecIdToTimerIdMap.Add(SpecId, NewTimerId);
			if (bMergeByName)
			{
				// Map the name to the timer.
				ScopeNameToTimerIdMap.Add(Name, NewTimerId);
			}
			return NewTimerId;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FCpuProfilerAnalyzer::DefineNewTimerChecked(uint32 SpecId, const TCHAR* TimerName, const TCHAR* File, uint32 Line)
{
	TimerName = Session.StoreString(TimerName);
	uint32 NewTimerId = EditableTimingProfilerProvider.AddCpuTimer(TimerName, File, Line);
	SpecIdToTimerIdMap.Add(SpecId, NewTimerId);
	return NewTimerId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FCpuProfilerAnalyzer::GetTimerId(uint32 SpecId)
{
	if (uint32* FindIt = SpecIdToTimerIdMap.Find(SpecId))
	{
		return *FindIt;
	}
	else
	{
		// Adds a timer with an "unknown" name.
		// The "unknown" timers are not merged by name, becasue the actual name
		// might be updated when an EventSpec event is received (for this SpecId).
		return DefineNewTimerChecked(SpecId, *FString::Printf(TEXT("<unknown %u>"), SpecId));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FCpuProfilerAnalyzer::FThreadState& FCpuProfilerAnalyzer::GetThreadState(uint32 ThreadId)
{
	FThreadState* ThreadState = ThreadStatesMap.FindRef(ThreadId);
	if (!ThreadState)
	{
		ThreadState = new FThreadState();
		ThreadState->ThreadId = ThreadId;
		ThreadState->Timeline = &EditableTimingProfilerProvider.GetCpuThreadEditableTimeline(ThreadId);
		ThreadStatesMap.Add(ThreadId, ThreadState);

		// Just in case the rest of Insight's reporting/analysis doesn't know about
		// this thread, we'll explicitly add it. For fault tolerance.
		EditableThreadProvider.AddThread(ThreadId, nullptr, TPri_Normal);
	}
	return *ThreadState;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace TraceServices

#undef CPUPROFILER_SAFE_DISPATCH_PENDING_EVENTS

#undef CPUPROFILER_DEBUG_LOGF
#undef CPUPROFILER_DEBUG_BEGIN_EVENT
#undef CPUPROFILER_DEBUG_END_EVENT
