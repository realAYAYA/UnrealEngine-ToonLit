// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "Trace/Trace.h"

class UStateTree;
struct FStateTreeDataView;
struct FStateTreeActiveStates;
struct FStateTreeInstanceDebugId;
struct FStateTreeIndex16;
struct FStateTreeStateHandle;
struct FStateTreeTransitionSource;
enum class EStateTreeStateSelectionBehavior : uint8;
enum class EStateTreeRunStatus : uint8;
enum class EStateTreeTraceEventType : uint8;
enum class EStateTreeUpdatePhase : uint8;

UE_TRACE_CHANNEL_EXTERN(StateTreeDebugChannel, STATETREEMODULE_API)

namespace UE::StateTreeTrace
{
	void RegisterGlobalDelegates();
	void UnregisterGlobalDelegates();
	void ProcessPhaseScopeEvent(FStateTreeInstanceDebugId InstanceId, EStateTreeUpdatePhase Phase, EStateTreeTraceEventType EventType, FStateTreeStateHandle StateHandle);
	void OutputInstanceLifetimeEvent(FStateTreeInstanceDebugId InstanceId, const UStateTree* StateTree, const TCHAR* InstanceName, EStateTreeTraceEventType EventType);
	void OutputLogEventTrace(FStateTreeInstanceDebugId InstanceId, const TCHAR* Fmt, ...);
	void OutputStateEventTrace(FStateTreeInstanceDebugId InstanceId, FStateTreeStateHandle StateHandle, EStateTreeTraceEventType EventType, EStateTreeStateSelectionBehavior SelectionBehavior);
	void OutputTaskEventTrace(FStateTreeInstanceDebugId InstanceId, FStateTreeIndex16 TaskIdx, FStateTreeDataView DataView, EStateTreeTraceEventType EventType, EStateTreeRunStatus Status);
	void OutputEvaluatorEventTrace(FStateTreeInstanceDebugId InstanceId, FStateTreeIndex16 EvaluatorIdx, FStateTreeDataView DataView, EStateTreeTraceEventType EventType);
	void OutputConditionEventTrace(FStateTreeInstanceDebugId InstanceId, FStateTreeIndex16 ConditionIdx, FStateTreeDataView DataView, EStateTreeTraceEventType EventType);
	void OutputTransitionEventTrace(FStateTreeInstanceDebugId InstanceId, FStateTreeTransitionSource TransitionSource, EStateTreeTraceEventType EventType);
	void OutputActiveStatesEventTrace(FStateTreeInstanceDebugId InstanceId, const FStateTreeActiveStates& ActiveStates);
}

#define TRACE_STATETREE_INSTANCE_EVENT(InstanceID, StateTree, InstanceName, EventType) \
	UE::StateTreeTrace::OutputInstanceLifetimeEvent(InstanceID, StateTree, InstanceName, EventType);

#define TRACE_STATETREE_PHASE_EVENT(InstanceID, Phase, EventType, StateHandle) \
	UE::StateTreeTrace::ProcessPhaseScopeEvent(InstanceID, Phase, EventType, StateHandle); \

#define TRACE_STATETREE_LOG_EVENT(InstanceId, Format, ...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel)) \
	{ \
		UE::StateTreeTrace::OutputLogEventTrace(InstanceId, Format, ##__VA_ARGS__); \
	}

#define TRACE_STATETREE_STATE_EVENT(InstanceId, StateHandle, EventType, SelectionBehavior) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel)) \
	{ \
		UE::StateTreeTrace::OutputStateEventTrace(InstanceId, StateHandle, EventType, SelectionBehavior); \
	}

#define TRACE_STATETREE_TASK_EVENT(InstanceId, TaskIdx, DataView, EventType, Status) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel)) \
	{ \
		UE::StateTreeTrace::OutputTaskEventTrace(InstanceId, TaskIdx, DataView, EventType, Status); \
	}

#define TRACE_STATETREE_EVALUATOR_EVENT(InstanceId, EvaluatorIdx, DataView, EventType) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel)) \
	{ \
		UE::StateTreeTrace::OutputEvaluatorEventTrace(InstanceId, EvaluatorIdx, DataView, EventType); \
	}

#define TRACE_STATETREE_CONDITION_EVENT(InstanceId, ConditionIdx, DataView, EventType) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel)) \
	{ \
		UE::StateTreeTrace::OutputConditionEventTrace(InstanceId, ConditionIdx, DataView, EventType); \
	}

#define TRACE_STATETREE_TRANSITION_EVENT(InstanceId, TransitionIdx, EventType) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(StateTreeDebugChannel)) \
	{ \
		UE::StateTreeTrace::OutputTransitionEventTrace(InstanceId, TransitionIdx, EventType); \
	}

#define TRACE_STATETREE_ACTIVE_STATES_EVENT(InstanceId, ActivateStates) \
		UE::StateTreeTrace::OutputActiveStatesEventTrace(InstanceId, ActivateStates);

#else //STATETREE_DEBUG_TRACE_ENABLED

#define TRACE_STATETREE_INSTANCE_EVENT(InstanceID, StateTree, InstanceName, EventType)
#define TRACE_STATETREE_PHASE_EVENT(InstanceID, Phase, EventType, StateHandle)
#define TRACE_STATETREE_LOG_EVENT(InstanceId, Format, ...)
#define TRACE_STATETREE_STATE_EVENT(InstanceId, StateHandle, EventType, SelectionBehavior)
#define TRACE_STATETREE_TASK_EVENT(InstanceId, TaskIdx, DataView, EventType, Status)
#define TRACE_STATETREE_EVALUATOR_EVENT(InstanceId, EvaluatorIdx, DataView, EventType)
#define TRACE_STATETREE_CONDITION_EVENT(InstanceId, ConditionIdx, DataView, EventType)
#define TRACE_STATETREE_TRANSITION_EVENT(InstanceId, TransitionIdx, EventType)
#define TRACE_STATETREE_ACTIVE_STATES_EVENT(InstanceId, ActivateStates)

#endif // STATETREE_DEBUG_TRACE_ENABLED
