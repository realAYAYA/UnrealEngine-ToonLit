// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "Debugger/StateTreeTrace.h"
#include "Algo/Removeif.h"
#include "Debugger/StateTreeDebugger.h"
#include "Exporters/Exporter.h"
#include "ObjectTrace.h"
#include "Serialization/BufferArchive.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "UObject/Package.h"
#include "Trace/Trace.inl"

#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

UE_TRACE_CHANNEL_DEFINE(StateTreeDebugChannel)

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, WorldTimestampEvent)
	UE_TRACE_EVENT_FIELD(double, WorldTime)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, InstanceEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, TreeName)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, TreePath)
	UE_TRACE_EVENT_FIELD(uint32, CompiledDataHash)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, InstanceName)
	UE_TRACE_EVENT_FIELD(uint8, EventType)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, PhaseEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(uint16, Phase)
	UE_TRACE_EVENT_FIELD(uint16, StateIndex)
	UE_TRACE_EVENT_FIELD(uint8, EventType)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, LogEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Message)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, StateEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(uint16, StateIndex)
	UE_TRACE_EVENT_FIELD(uint8, EventType)
	UE_TRACE_EVENT_FIELD(uint8, SelectionBehavior)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, TaskEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(uint16, NodeIndex)
	UE_TRACE_EVENT_FIELD(uint8[], DataView)
	UE_TRACE_EVENT_FIELD(uint8, EventType)
	UE_TRACE_EVENT_FIELD(uint8, Status)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, EvaluatorEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(uint16, NodeIndex)
	UE_TRACE_EVENT_FIELD(uint8[], DataView)
	UE_TRACE_EVENT_FIELD(uint8, EventType)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, TransitionEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(uint8, SourceType)
	UE_TRACE_EVENT_FIELD(uint16, TransitionIndex)
	UE_TRACE_EVENT_FIELD(uint16, TargetStateIndex)
	UE_TRACE_EVENT_FIELD(uint8, Priority)
	UE_TRACE_EVENT_FIELD(uint8, EventType)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, ConditionEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(uint16, NodeIndex)
	UE_TRACE_EVENT_FIELD(uint8[], DataView)
	UE_TRACE_EVENT_FIELD(uint8, EventType)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(StateTreeDebugger, ActiveStatesEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, InstanceSerial)
	UE_TRACE_EVENT_FIELD(uint16[], ActiveStates)
UE_TRACE_EVENT_END()

namespace UE::StateTreeTrace
{

FDelegateHandle GOnWorldTickStartDelegateHandle;

#if WITH_EDITOR
FDelegateHandle GOnPIEStartDelegateHandle;
#endif // WITH_EDITOR

double GRecordingWorldTime = -1;

/** Struct to keep track if a given phase was traced or not. */
struct FPhaseTraceStatusPair
{
	explicit FPhaseTraceStatusPair(const EStateTreeUpdatePhase Phase, const FStateTreeStateHandle StateHandle)
	: Phase(Phase)
	, StateHandle(StateHandle)
	{
	}

	EStateTreeUpdatePhase Phase = EStateTreeUpdatePhase::Unset;
	FStateTreeStateHandle StateHandle = FStateTreeStateHandle::Invalid; 
	bool bTraced = false;
};

/** Struct to keep track of the list of stacked phases for a given statetree instance. */
struct FPhaseStack
{
	FStateTreeInstanceDebugId InstanceId;
	TArray<FPhaseTraceStatusPair> Stack;
};

/**
 * Struct to hold data for active states events until we are ready to trace the events (i.e. traces are active and channel is enabled).
 */
struct FInstanceEventBufferedData
{
	FInstanceEventBufferedData() = default;
	explicit FInstanceEventBufferedData(
		const double RecordingWorldTime,
		const UStateTree& StateTree,
		const FStateTreeInstanceDebugId InstanceId,
		const FString& InstanceName,
		const EStateTreeTraceEventType EventType)	
		: InstanceName(InstanceName)
		, WeakStateTree(&StateTree)
		, InstanceId(InstanceId)
		, LifetimeRecordingWorldTime(RecordingWorldTime)
		, EventType(EventType)
	{
	}

	void Trace() const
	{
		if (!ensureMsgf(UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel), TEXT("Tracing a buffered data is expected only if channel is enabled.")))
		{
			return;
		}

		if (const UStateTree* StateTree = WeakStateTree.Get())
		{
			// Force a world time update since we are tracing an event from the past
			UE_TRACE_LOG(StateTreeDebugger, WorldTimestampEvent, StateTreeDebugChannel)
				<< WorldTimestampEvent.WorldTime(LifetimeRecordingWorldTime);

			OutputInstanceLifetimeEvent(InstanceId, StateTree, *InstanceName, EventType);

			if (ActiveStates.Num() > 0)
			{
				// Force a world time update since we are tracing an event from the past
				UE_TRACE_LOG(StateTreeDebugger, WorldTimestampEvent, StateTreeDebugChannel)
					<< WorldTimestampEvent.WorldTime(ActiveStatesRecordingWorldTime);

				OutputActiveStatesEventTrace(InstanceId, ActiveStates);
			}
		}
	}

	FStateTreeActiveStates ActiveStates;
	FString InstanceName;
	TWeakObjectPtr<const UStateTree> WeakStateTree;
	FStateTreeInstanceDebugId InstanceId;
	double LifetimeRecordingWorldTime = 0;
	double ActiveStatesRecordingWorldTime = 0;
	EStateTreeTraceEventType EventType = EStateTreeTraceEventType::Unset;
};

/** Struct to keep track of the buffered event data and flush them. */
struct FBufferedDataList
{
	double TracedRecordingWorldTime = -1;
	
	/**
	 * Stacks to keep track of all received phase events so other events will control when and if a given phase trace will be sent.
	 * This is per thread since it is possible to update execution contexts on multiple threads.
	 */
	TArray<FPhaseStack> PhaseStacks;

	/** List of lifetime events that will be output if channel gets enabled in the Push - Pop lifetime window of an instance. */
	TArray<FInstanceEventBufferedData> Events;

	/** Flag use to prevent reentrant calls */
	bool bFlushing = false;

	void Flush(const FStateTreeInstanceDebugId InstanceId)
	{
		if (bFlushing)
		{
			return;
		}

		TGuardValue<bool> GuardReentry(bFlushing, true);

		// Trace instance lifetime events first since they are required for other event types.
		// It is also associated to an older world time.
		for (const FInstanceEventBufferedData& InstanceEventData : Events)
		{
			InstanceEventData.Trace();
		}
		Events.Empty();

		TraceWorldTime();
		TraceStackedPhases(InstanceId);
	}

	/**
	 * Called by TraceBufferedEvents from the OutputXYZ methods to make sure we have the current world time was sent.
	 */
	void TraceWorldTime()
	{
		if (TracedRecordingWorldTime != GRecordingWorldTime)
		{
			TracedRecordingWorldTime = GRecordingWorldTime;
			UE_TRACE_LOG(StateTreeDebugger, WorldTimestampEvent, StateTreeDebugChannel)
				<< WorldTimestampEvent.WorldTime(GRecordingWorldTime);
		}
	}

	/**
	 * Called by TraceBufferedEvents from the OutputXYZ methods to flush pending phase events.
	 * Phases popped before TraceStackedPhases gets called will never produce any trace since
	 * they will not be required for the analysis.
	 */
	void TraceStackedPhases(const FStateTreeInstanceDebugId InstanceId)
	{
		for (FPhaseStack& PhaseStack : PhaseStacks)
		{
			if (PhaseStack.InstanceId == InstanceId)
			{
				for (FPhaseTraceStatusPair& StackEntry : PhaseStack.Stack)
				{
					// Trace push phase event and marked as traced only if not already traced and our channel is enabled.
					// We need the pop phase event to be sent only in this case to enforce complementary events in case of
					// late recording (e.g. recording started, or channel enabled, after simulation is running and instances are ticked)
					if (StackEntry.bTraced == false && UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel))
					{
						UE_TRACE_LOG(StateTreeDebugger, PhaseEvent, StateTreeDebugChannel)
							<< PhaseEvent.Cycle(FPlatformTime::Cycles64())
							<< PhaseEvent.InstanceId(InstanceId.Id)
							<< PhaseEvent.InstanceSerial(InstanceId.SerialNumber)
							<< PhaseEvent.Phase(static_cast<std::underlying_type_t<EStateTreeUpdatePhase>>(StackEntry.Phase))
							<< PhaseEvent.StateIndex(StackEntry.StateHandle.Index)
							<< PhaseEvent.EventType(static_cast<std::underlying_type_t<EStateTreeTraceEventType>>(EStateTreeTraceEventType::Push));
			
						StackEntry.bTraced = true;
					}
				}
				break;
			}
		}
	}
};

/**
 * Buffered events (e.g. lifetime, active state, scoped phase) in case channel is not active yet or phase are empty and don't need to be traced.
 * This is per thread since it is possible to update execution contexts on multiple threads.
 * @note The current implementation of the lifetime events doesn't properly support same instance getting ticked in different threads.
 */
thread_local FBufferedDataList GBufferedEvents;

/**
 * Pushed or pops an entry on the Phase stack for a given Instance.
 * Will send the Pop events for phases popped if their associated Push events were sent.
 */
void ProcessPhaseScopeEvent(const FStateTreeInstanceDebugId InstanceId, const EStateTreeUpdatePhase Phase, const EStateTreeTraceEventType EventType, const FStateTreeStateHandle StateHandle)
{
	TArray<FPhaseStack>& PhaseStacks = GBufferedEvents.PhaseStacks;
	int32 ExistingStackIndex = PhaseStacks.IndexOfByPredicate([InstanceId](const FPhaseStack& PhaseStack){ return PhaseStack.InstanceId == InstanceId; });

	if (EventType == EStateTreeTraceEventType::Push)
	{
		if (ExistingStackIndex == INDEX_NONE)
		{
			ExistingStackIndex = PhaseStacks.AddDefaulted();
		}
		FPhaseStack& PhaseStack = PhaseStacks[ExistingStackIndex];
		PhaseStack.InstanceId = InstanceId;
		PhaseStack.Stack.Push(FPhaseTraceStatusPair(Phase, StateHandle));
	}
	else if (ensureMsgf(ExistingStackIndex != INDEX_NONE, TEXT("Not expected to pop phases for an instance that never pushed a phase.")))
	{
		FPhaseStack& PhaseStack = PhaseStacks[ExistingStackIndex];

		if (ensureMsgf(PhaseStack.Stack.IsEmpty() == false, TEXT("Not expected to pop phases that never got pushed.")) &&
			ensureMsgf(PhaseStack.InstanceId == InstanceId, TEXT("Not expected to pop phases for an instance that is not the one currently assigned to the stack.")))
		{
			const FPhaseTraceStatusPair RemovedPair = PhaseStack.Stack.Pop();
			ensureMsgf(RemovedPair.Phase == Phase, TEXT("Not expected to pop a phase that is not on the top of the stack."));

			// Clear associated InstanceId when removing last entry from the stack.
			if (PhaseStack.Stack.IsEmpty())
			{
				PhaseStacks.RemoveAt(ExistingStackIndex);
			}

			// Phase was previously traced (i.e. other events were traced in that scope so we need to trace the closing (i.e. Pop) event.
			if (RemovedPair.bTraced)
			{
				UE_TRACE_LOG(StateTreeDebugger, PhaseEvent, StateTreeDebugChannel)
				<< PhaseEvent.Cycle(FPlatformTime::Cycles64())
				<< PhaseEvent.InstanceId(InstanceId.Id)
				<< PhaseEvent.InstanceSerial(InstanceId.SerialNumber)
				<< PhaseEvent.Phase(static_cast<std::underlying_type_t<EStateTreeUpdatePhase>>(Phase))
				<< PhaseEvent.StateIndex(StateHandle.Index)
				<< PhaseEvent.EventType(static_cast<std::underlying_type_t<EStateTreeTraceEventType>>(EStateTreeTraceEventType::Pop));
			}
		}
	}
}


/**
 * Called by the OutputXYZ methods to flush pending events (e.g. Push or WorldTime).
 */
void TraceBufferedEvents(const FStateTreeInstanceDebugId InstanceId)
{
	GBufferedEvents.Flush(InstanceId);
}

void SerializeDataViewToArchive(FBufferArchive Ar, const FStateTreeDataView DataView)
{
	constexpr uint32 PortFlags = 
		PPF_PropertyWindow // limit to properties visible in Editor 
		| PPF_ExportsNotFullyQualified
		| PPF_Delimited // property data should be wrapped in quotes
		| PPF_ExternalEditor // uses authored names instead of internal names and default values are always written out
		| PPF_SimpleObjectText // object property values should be exported without the package or class information
		| PPF_ForDiff; // do not emit object path

	if (const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(DataView.GetStruct()))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::StateTree::ExportStructAsText)
		FString StructPath = ScriptStruct->GetPathName();
		FString TextValue;

		ScriptStruct->ExportText(TextValue, DataView.GetMemory(), DataView.GetMemory(), /*OwnerObject*/nullptr, PortFlags | PPF_SeparateDefine, /*ExportRootScope*/nullptr);

		Ar << StructPath;
		Ar << TextValue;
	}
	else if (const UClass* Class = Cast<const UClass>(DataView.GetStruct()))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::StateTree::ExportObjectAsText)
		FString StructPath = Class->GetPathName();
		FStringOutputDevice OutputDevice;
		UObject* Object = DataView.GetMutablePtr<UObject>();

		// Not using on scope FExportObjectInnerContext since it is very costly to build.
		// Passing a null context will make the export use an already built thread local context.
		UExporter::ExportToOutputDevice(nullptr, Object, /*Exporter*/nullptr, OutputDevice, TEXT("copy"), 0, PortFlags, false, Object->GetOuter());

		Ar << StructPath;
		Ar << OutputDevice;
	}
}

void RegisterGlobalDelegates()
{
#if WITH_EDITOR
	GOnPIEStartDelegateHandle = FEditorDelegates::BeginPIE.AddLambda([&LastRecordingWorldTime=GBufferedEvents.TracedRecordingWorldTime](const bool bIsSimulating)
		{
			LastRecordingWorldTime = -1;
		});
#endif // WITH_EDITOR
	
	GOnWorldTickStartDelegateHandle = FWorldDelegates::OnWorldTickStart.AddLambda([&WorldTime=GRecordingWorldTime](const UWorld* TickedWorld, ELevelTick TickType, float DeltaTime)
		{
#if OBJECT_TRACE_ENABLED
			WorldTime = FObjectTrace::GetWorldElapsedTime(TickedWorld);
#endif// OBJECT_TRACE_ENABLED
		});
}

void UnregisterGlobalDelegates()
{
#if WITH_EDITOR
	FEditorDelegates::BeginPIE.Remove(GOnPIEStartDelegateHandle);
#endif // WITH_EDITOR
	
	FWorldDelegates::OnWorldTickStart.Remove(GOnWorldTickStartDelegateHandle);
	GOnWorldTickStartDelegateHandle.Reset();
}

void OutputInstanceLifetimeEvent(
	const FStateTreeInstanceDebugId InstanceId,
	const UStateTree* StateTree,
	const TCHAR* InstanceName,
	const EStateTreeTraceEventType EventType
	)
{
	if (StateTree == nullptr)
	{
		return;
	}

	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel))
	{
		TraceBufferedEvents(InstanceId);

		const FString TreeName = StateTree->GetName();
		const FString TreePath = StateTree->GetPackage()->GetName();

		UE_TRACE_LOG(StateTreeDebugger, InstanceEvent, StateTreeDebugChannel)
			<< InstanceEvent.Cycle(FPlatformTime::Cycles64())
			<< InstanceEvent.TreeName(*TreeName, TreeName.Len())
			<< InstanceEvent.TreePath(*TreePath, TreePath.Len())
			<< InstanceEvent.CompiledDataHash(StateTree->LastCompiledEditorDataHash)
			<< InstanceEvent.InstanceId(InstanceId.Id)
			<< InstanceEvent.InstanceSerial(InstanceId.SerialNumber)
			<< InstanceEvent.InstanceName(InstanceName)
			<< InstanceEvent.EventType(static_cast<std::underlying_type_t<EStateTreeTraceEventType>>(EventType));
	}
	else
	{
		if (EventType == EStateTreeTraceEventType::Push)
		{
			GBufferedEvents.Events.Emplace(GRecordingWorldTime, *StateTree, InstanceId, InstanceName, EventType);
		}
		else if (EventType == EStateTreeTraceEventType::Pop)
		{
			// Remove matching instance events since if it was not sent then no other events were sent between, hence not needed in the trace.
			GBufferedEvents.Events.SetNum(Algo::StableRemoveIf(GBufferedEvents.Events, 
				[InstanceId](const FInstanceEventBufferedData& BufferedData)
				{
					return BufferedData.InstanceId == InstanceId;
				}));
		}
		else
		{
			ensureMsgf(false, TEXT("Unexpected EventType '%s' for instance lifetime event."), *UEnum::GetDisplayValueAsText(EventType).ToString());
		}
	}
}

void OutputLogEventTrace(
	const FStateTreeInstanceDebugId InstanceId,
	const TCHAR* Fmt, ...
	)
{
	static TCHAR TraceStaticBuffer[8192];
	GET_VARARGS(TraceStaticBuffer, UE_ARRAY_COUNT(TraceStaticBuffer), UE_ARRAY_COUNT(TraceStaticBuffer) - 1, Fmt, Fmt);

	TraceBufferedEvents(InstanceId);

	UE_TRACE_LOG(StateTreeDebugger, LogEvent, StateTreeDebugChannel)
		<< LogEvent.Cycle(FPlatformTime::Cycles64())
		<< LogEvent.InstanceId(InstanceId.Id)
		<< LogEvent.InstanceSerial(InstanceId.SerialNumber)
		<< LogEvent.Message(TraceStaticBuffer);
}

void OutputStateEventTrace(
	const FStateTreeInstanceDebugId InstanceId,
	const FStateTreeStateHandle StateHandle,
	const EStateTreeTraceEventType EventType,
	const EStateTreeStateSelectionBehavior SelectionBehavior
	)
{
	TraceBufferedEvents(InstanceId);

	UE_TRACE_LOG(StateTreeDebugger, StateEvent, StateTreeDebugChannel)
		<< StateEvent.Cycle(FPlatformTime::Cycles64())
		<< StateEvent.InstanceId(InstanceId.Id)
		<< StateEvent.InstanceSerial(InstanceId.SerialNumber)
		<< StateEvent.StateIndex(StateHandle.Index)
		<< StateEvent.EventType(static_cast<std::underlying_type_t<EStateTreeTraceEventType>>(EventType))
		<< StateEvent.SelectionBehavior(static_cast<std::underlying_type_t<EStateTreeStateSelectionBehavior>>(SelectionBehavior));
}

void OutputTaskEventTrace(
	const FStateTreeInstanceDebugId InstanceId,
	const FStateTreeIndex16 TaskIdx,
	const FStateTreeDataView DataView,
	const EStateTreeTraceEventType EventType,
	const EStateTreeRunStatus Status
	)
{
	FBufferArchive Archive;
	SerializeDataViewToArchive(Archive, DataView);

	TraceBufferedEvents(InstanceId);

	UE_TRACE_LOG(StateTreeDebugger, TaskEvent, StateTreeDebugChannel)
		<< TaskEvent.Cycle(FPlatformTime::Cycles64())
		<< TaskEvent.InstanceId(InstanceId.Id)
		<< TaskEvent.InstanceSerial(InstanceId.SerialNumber)
		<< TaskEvent.NodeIndex(TaskIdx.Get())
		<< TaskEvent.DataView(Archive.GetData(), Archive.Num())
		<< TaskEvent.EventType(static_cast<std::underlying_type_t<EStateTreeTraceEventType>>(EventType))
		<< TaskEvent.Status(static_cast<std::underlying_type_t<EStateTreeRunStatus>>(Status));
}

void OutputEvaluatorEventTrace(
	const FStateTreeInstanceDebugId InstanceId,
	const FStateTreeIndex16 EvaluatorIdx,
	const FStateTreeDataView DataView,
	const EStateTreeTraceEventType EventType)
{
	FBufferArchive Archive;
	SerializeDataViewToArchive(Archive, DataView);

	TraceBufferedEvents(InstanceId);

	UE_TRACE_LOG(StateTreeDebugger, EvaluatorEvent, StateTreeDebugChannel)
		<< EvaluatorEvent.Cycle(FPlatformTime::Cycles64())
		<< EvaluatorEvent.InstanceId(InstanceId.Id)
		<< EvaluatorEvent.InstanceSerial(InstanceId.SerialNumber)
		<< EvaluatorEvent.NodeIndex(EvaluatorIdx.Get())
		<< EvaluatorEvent.DataView(Archive.GetData(), Archive.Num())
		<< EvaluatorEvent.EventType(static_cast<std::underlying_type_t<EStateTreeTraceEventType>>(EventType));
}

void OutputTransitionEventTrace(
	const FStateTreeInstanceDebugId InstanceId,
	const FStateTreeTransitionSource Source,
	const EStateTreeTraceEventType EventType
	)
{
	FBufferArchive Archive;
	Archive << EventType;

	TraceBufferedEvents(InstanceId);

	UE_TRACE_LOG(StateTreeDebugger, TransitionEvent, StateTreeDebugChannel)
	<< TransitionEvent.Cycle(FPlatformTime::Cycles64())
	<< TransitionEvent.InstanceId(InstanceId.Id)
	<< TransitionEvent.InstanceSerial(InstanceId.SerialNumber)
	<< TransitionEvent.SourceType(static_cast<std::underlying_type_t<EStateTreeTransitionSourceType>>(Source.SourceType))
	<< TransitionEvent.TransitionIndex(Source.TransitionIndex.Get())
	<< TransitionEvent.TargetStateIndex(Source.TargetState.Index)
	<< TransitionEvent.Priority(static_cast<std::underlying_type_t<EStateTreeTransitionPriority>>(Source.Priority))
	<< TransitionEvent.EventType(static_cast<std::underlying_type_t<EStateTreeTraceEventType>>(EventType));
}

void OutputConditionEventTrace(
	const FStateTreeInstanceDebugId InstanceId,
	const FStateTreeIndex16 ConditionIdx,
	const FStateTreeDataView DataView,
	const EStateTreeTraceEventType EventType
	)
{
	FBufferArchive Archive;
	SerializeDataViewToArchive(Archive, DataView);

	TraceBufferedEvents(InstanceId);

	UE_TRACE_LOG(StateTreeDebugger, ConditionEvent, StateTreeDebugChannel)
	<< ConditionEvent.Cycle(FPlatformTime::Cycles64())
	<< ConditionEvent.InstanceId(InstanceId.Id)
	<< ConditionEvent.InstanceSerial(InstanceId.SerialNumber)
	<< ConditionEvent.NodeIndex(ConditionIdx.Get())
	<< ConditionEvent.DataView(Archive.GetData(), Archive.Num())
	<< ConditionEvent.EventType(static_cast<std::underlying_type_t<EStateTreeTraceEventType>>(EventType));
}

void OutputActiveStatesEventTrace(
	const FStateTreeInstanceDebugId InstanceId,
	const FStateTreeActiveStates& ActiveStates
	)
{
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel))
	{
		TraceBufferedEvents(InstanceId);

		TArray<uint16, TInlineAllocator<FStateTreeActiveStates::MaxStates>> StatesIndices;
		for (int32 i = 0; i < ActiveStates.Num(); i++)
		{
			StatesIndices.Add(ActiveStates[i].Index);
		}

		UE_TRACE_LOG(StateTreeDebugger, ActiveStatesEvent, StateTreeDebugChannel)
			<< ActiveStatesEvent.Cycle(FPlatformTime::Cycles64())
			<< ActiveStatesEvent.InstanceId(InstanceId.Id)
			<< ActiveStatesEvent.InstanceSerial(InstanceId.SerialNumber)
			<< ActiveStatesEvent.ActiveStates(StatesIndices.GetData(), StatesIndices.Num());
	}
	else
	{
		FInstanceEventBufferedData* ExisingBufferedData = GBufferedEvents.Events.FindByPredicate([InstanceId](const FInstanceEventBufferedData& BufferedData)
			{
				return BufferedData.InstanceId == InstanceId;
			});

		// We keep only the most recent active states since this is all we need to know in which state was the instance
		// when we start receiving the events once the channel is enabled. 
		if (ExisingBufferedData != nullptr)
		{
			ExisingBufferedData->ActiveStates = ActiveStates;
			ExisingBufferedData->ActiveStatesRecordingWorldTime = GRecordingWorldTime;
		}
	}
}

} // UE::StateTreeTrace

#endif // WITH_STATETREE_DEBUGGER
