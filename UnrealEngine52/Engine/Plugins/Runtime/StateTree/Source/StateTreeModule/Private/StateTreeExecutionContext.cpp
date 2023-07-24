// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeExecutionContext.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"
#include "Containers/StaticArray.h"
#include "VisualLogger/VisualLogger.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Logging/LogScopedVerbosityOverride.h"

#define STATETREE_LOG(Verbosity, Format, ...) UE_VLOG_UELOG(GetOwner(), LogStateTree, Verbosity, TEXT("%s") Format, *GetInstanceDescription(), ##__VA_ARGS__)
#define STATETREE_CLOG(Condition, Verbosity, Format, ...) UE_CVLOG_UELOG((Condition), GetOwner(), LogStateTree, Verbosity, TEXT("%s") Format, *GetInstanceDescription(), ##__VA_ARGS__)

namespace UE::StateTree
{
	constexpr int32 DebugIndentSize = 2;	// Debug printing indent for hierarchical data.
}; // UE::StateTree

FStateTreeExecutionContext::FStateTreeExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData)
	: Owner(InOwner)
	, StateTree(InStateTree)
	, InstanceData(InInstanceData)
{
	if (InStateTree.IsReadyToRun())
	{
		// Initialize data views for all possible items.
		DataViews.SetNum(StateTree.GetNumDataViews());
		// Set data views associated to the parameters using the default values
		SetDefaultParameters();

		SharedInstanceData = StateTree.GetSharedInstanceData();
	}
	else
	{
		STATETREE_LOG(Warning, TEXT("%s: StateTree asset is not valid ('%s' using StateTree '%s')"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
	}
}

FStateTreeExecutionContext::~FStateTreeExecutionContext()
{
}

void FStateTreeExecutionContext::SetDefaultParameters()
{
	if (DataViews.IsValidIndex(StateTree.ParametersDataViewIndex.Get()))
	{
		// @todo: Handle constness correctly.
		const FConstStructView ConstParameters = StateTree.GetDefaultParameters().GetValue();
		DataViews[StateTree.ParametersDataViewIndex.Get()] = FStateTreeDataView(ConstParameters.GetScriptStruct(), const_cast<uint8*>(ConstParameters.GetMemory()));	
	}
}

void FStateTreeExecutionContext::SetParameters(const FInstancedPropertyBag& Parameters)
{
	if (ensureMsgf(StateTree.GetDefaultParameters().GetPropertyBagStruct() == Parameters.GetPropertyBagStruct(),
		TEXT("Parameters must be of the same struct type. Make sure to migrate the provided parameters to the same type as the StateTree default parameters."))
		&& DataViews.IsValidIndex(StateTree.ParametersDataViewIndex.Get()))
	{
		// @todo: Handle constness correctly.
		const FConstStructView ConstParameters = Parameters.GetValue();
		DataViews[StateTree.ParametersDataViewIndex.Get()] = FStateTreeDataView(ConstParameters.GetScriptStruct(), const_cast<uint8*>(ConstParameters.GetMemory()));	
	}
}

bool FStateTreeExecutionContext::AreExternalDataViewsValid() const
{
	if (!IsValid())
	{
		return false;
	}
	
	bool bResult = true;
	for (const FStateTreeExternalDataDesc& DataDesc : StateTree.ExternalDataDescs)
	{
		const FStateTreeDataView& DataView = DataViews[DataDesc.Handle.DataViewIndex.Get()];
			
		if (DataDesc.Requirement == EStateTreeExternalDataRequirement::Required)
		{
			// Required items must have valid pointer of the expected type.  
			if (!DataView.IsValid() || !DataView.GetStruct()->IsChildOf(DataDesc.Struct))
			{
				bResult = false;
				break;
			}
		}
		else
		{
			// Optional items must have same type if they are set.
			if (DataView.IsValid() && !DataView.GetStruct()->IsChildOf(DataDesc.Struct))
			{
				bResult = false;
				break;
			}
		}
	}

	for (const FStateTreeExternalDataDesc& DataDesc : StateTree.GetContextDataDescs())
	{
		const FStateTreeDataView& DataView = DataViews[DataDesc.Handle.DataViewIndex.Get()];

		// Items must have valid pointer of the expected type.  
		if (!DataView.IsValid() || !DataView.GetStruct()->IsChildOf(DataDesc.Struct))
		{
			bResult = false;
			break;
		}
	}
	return bResult;
}

void FStateTreeExecutionContext::UpdateLinkedStateParameters(const FCompactStateTreeState& State, const int32 ParameterInstanceIndex)
{
	FStateTreeDataView StateParamsInstance = InstanceData.GetMutableStruct(ParameterInstanceIndex);
	FCompactStateTreeParameters& StateParams = StateParamsInstance.GetMutable<FCompactStateTreeParameters>();

	// Update parameters if the state has any.
	if (StateParams.Parameters.IsValid())
	{
		// Parameters property bag
		const FStateTreeDataView ParametersView(StateParams.Parameters.GetMutableValue());
		if (StateParams.BindingsBatch.IsValid())
		{
			StateTree.PropertyBindings.CopyTo(DataViews, StateParams.BindingsBatch, ParametersView);
		}

		// Set the parameters as the input parameters for the linked state.
		check(State.LinkedState.IsValid());
		const FCompactStateTreeState& LinkedState = StateTree.States[State.LinkedState.Index];
		check(LinkedState.ParameterDataViewIndex.IsValid());
		DataViews[LinkedState.ParameterDataViewIndex.Get()] = ParametersView;
	}
}

void FStateTreeExecutionContext::UpdateSubtreeStateParameters(const FCompactStateTreeState& State)
{
	check(State.ParameterInstanceIndex.IsValid());

	// Update parameters if the state has any.
	if (State.ParameterDataViewIndex.IsValid())
	{
		// Usually the subtree parameter view is set by the linked state. If it's not (i.e. transitioned into a parametrized subtree), we'll set the view default params.
		if (DataViews[State.ParameterDataViewIndex.Get()].IsValid())
		{
			return;
		}

		// Set view to default parameters.
		const FConstStructView ParamInstance = StateTree.DefaultInstanceData.GetStruct(State.ParameterInstanceIndex.Get()); // These are used as const, so get them from the tree initial values.
		const FStateTreeDataView ParamInstanceView(ParamInstance.GetScriptStruct(), const_cast<uint8*>(ParamInstance.GetMemory())); 
		FCompactStateTreeParameters& Params = ParamInstanceView.GetMutable<FCompactStateTreeParameters>();
		DataViews[State.ParameterDataViewIndex.Get()] = FStateTreeDataView(Params.Parameters.GetMutableValue());
	}
}

EStateTreeRunStatus FStateTreeExecutionContext::Start()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Start);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return EStateTreeRunStatus::Failed;
	}

	// Stop if still running previous state.
	Stop();

	// Initialize instance data. No active states yet, so we'll initialize the evals and global tasks.
	InstanceData.Reset();
	const FStateTreeActiveStates Empty;
	UpdateInstanceData(Empty, Empty);
	if (!InstanceData.IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: Failed to initialize instance data on '%s' using StateTree '%s'. Try to recompile the StateTree asset."),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return EStateTreeRunStatus::Failed;
	}

	// Start evaluators and global tasks. Fail the execution if any global task fails.
	FStateTreeIndex16 LastInitializedTaskIndex;
	if (!StartEvaluatorsAndGlobalTasks(LastInitializedTaskIndex))
	{
		StopEvaluatorsAndGlobalTasks(LastInitializedTaskIndex);
		STATETREE_LOG(Warning, TEXT("%s: Failed to start evaluators and global tasks '%s' using StateTree '%s'. Try to recompile the StateTree asset."),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return EStateTreeRunStatus::Failed;
	}

	// First tick.
	// Tasks are not ticked here, since their behavior is that EnterState() (called above) is treated as a tick.  
	TickEvaluatorsAndGlobalTasks(0.0f, /*bTickGlobalTasks*/false);

	// Initialize to unset running state.
	FStateTreeExecutionState* Exec = &GetExecState(); // Using pointer as we will need to reacquire the exec later.
	Exec->TreeRunStatus = EStateTreeRunStatus::Running;
	Exec->ActiveStates.Reset();
	Exec->LastTickStatus = EStateTreeRunStatus::Unset;

	static const FStateTreeStateHandle RootState = FStateTreeStateHandle(0);

	FStateTreeActiveStates NextActiveStates;
	if (SelectState(RootState, NextActiveStates))
	{
		if (NextActiveStates.Last().IsCompletionState())
		{
			// Transition to a terminal state (succeeded/failed), or default transition failed.
			STATETREE_LOG(Warning, TEXT("%s: Tree %s at StateTree start on '%s' using StateTree '%s'."),
				ANSI_TO_TCHAR(__FUNCTION__), NextActiveStates.Last() == FStateTreeStateHandle::Succeeded ? TEXT("succeeded") : TEXT("failed"), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
			Exec->TreeRunStatus = NextActiveStates.Last().ToCompletionStatus();
		}
		else
		{
			// Enter state tasks can fail/succeed, treat it same as tick.
			FStateTreeTransitionResult Transition;
			Transition.TargetState = RootState;
			Transition.CurrentActiveStates = Exec->ActiveStates;
			Transition.CurrentRunStatus = Exec->LastTickStatus;
			Transition.NextActiveStates = NextActiveStates; // Enter state will update Exec.ActiveStates.
			const EStateTreeRunStatus LastTickStatus = EnterState(Transition);
			
			// Need to reacquire the exec state as EnterState may alter the allocation. 
			Exec = &GetExecState();
			Exec->LastTickStatus = LastTickStatus;

			// Report state completed immediately.
			if (Exec->LastTickStatus != EStateTreeRunStatus::Running)
			{
				StateCompleted();
			}
		}
	}

	if (Exec->ActiveStates.IsEmpty())
	{
		// Should not happen. This may happen if initial state could not be selected.
		STATETREE_LOG(Error, TEXT("%s: Failed to select initial state on '%s' using StateTree '%s'. This should not happen, check that the StateTree logic can always select a state at start."),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		Exec->TreeRunStatus = EStateTreeRunStatus::Failed;
	}

	return Exec->TreeRunStatus;
}

EStateTreeRunStatus FStateTreeExecutionContext::Stop()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Stop);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return EStateTreeRunStatus::Failed;
	}
	
	if (!InstanceData.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeExecutionState& Exec = GetExecState();
	EStateTreeRunStatus Result = Exec.TreeRunStatus;
	
	// Capture events added between ticks.
	FStateTreeEventQueue& EventQueue = InstanceData.GetMutableEventQueue();
	EventsToProcess = EventQueue.GetEvents();
	EventQueue.Reset();

	// Exit states if still in some valid state.
	if (Exec.TreeRunStatus == EStateTreeRunStatus::Running && !Exec.ActiveStates.IsEmpty())
	{
		// Transition to Succeeded state.
		FStateTreeTransitionResult Transition;
		Transition.TargetState = FStateTreeStateHandle::Succeeded;
		Transition.CurrentActiveStates = Exec.ActiveStates;
		Transition.CurrentRunStatus = Exec.LastTickStatus;
		Transition.NextActiveStates = FStateTreeActiveStates(FStateTreeStateHandle::Succeeded);

		ExitState(Transition);

		Result = EStateTreeRunStatus::Succeeded;
	}

	// Stop evaluators and global tasks.
	StopEvaluatorsAndGlobalTasks();

	// Destruct all allocated instance data (does not shrink the buffer). This will invalidate Exec too.
	InstanceData.Reset();

	return Result;
}

EStateTreeRunStatus FStateTreeExecutionContext::Tick(const float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Tick);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.IsValid())
	{
		STATETREE_LOG(Error, TEXT("%s: Tick called on %s using StateTree %s with invalid instance data. Start() must be called before Tick()."),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeEventQueue& EventQueue = InstanceData.GetMutableEventQueue();
	FStateTreeExecutionState* Exec = &GetExecState();
	
	// Capture events added between ticks.
	EventsToProcess = EventQueue.GetEvents();
	EventQueue.Reset();
	
	// No ticking of the tree is done or stopped.
	if (Exec->TreeRunStatus != EStateTreeRunStatus::Running)
	{
		return Exec->TreeRunStatus;
	}

	// Update the delayed transitions.
	for (FStateTreeTransitionDelayedState& DelayedState : Exec->DelayedTransitions)
	{
		DelayedState.TimeLeft -= DeltaTime;
	}

	// Tick global evaluators and tasks.
	const EStateTreeRunStatus EvalAndGlobalTaskStatus = TickEvaluatorsAndGlobalTasks(DeltaTime);
	if (EvalAndGlobalTaskStatus != EStateTreeRunStatus::Running)
	{
		return Stop();
	}

	if (Exec->LastTickStatus == EStateTreeRunStatus::Running)
	{
		// Tick tasks on active states.
		Exec->LastTickStatus = TickTasks(DeltaTime);
		
		// Report state completed immediately.
		if (Exec->LastTickStatus != EStateTreeRunStatus::Running)
		{
			StateCompleted();
		}
	}

	// The state selection is repeated up to MaxIteration time. This allows failed EnterState() to potentially find a new state immediately.
	// This helps event driven StateTrees to not require another event/tick to find a suitable state.
	static constexpr int32 MaxIterations = 5;
	for (int32 Iter = 0; Iter < MaxIterations; Iter++)
	{
		// Append events accumulated during the tick, so that transitions can immediately act on them.
		// We'll consume the events only if they lead to state change below (EnterState is treated the same as Tick),
		// or let them be processed next frame if no transition.
		EventsToProcess.Append(EventQueue.GetEvents());

		// Trigger conditional transitions or state succeed/failed transitions. First tick transition is handled here too.
		if (TriggerTransitions())
		{
			// We have committed to state change, consume events that were accumulated during the tick above.
			EventQueue.Reset();

			ExitState(NextTransition);

			// Tree succeeded or failed.
			if (NextTransition.TargetState.IsCompletionState())
			{
				// Transition to a terminal state (succeeded/failed), or default transition failed.
				Exec->TreeRunStatus = NextTransition.TargetState.ToCompletionStatus();
				Exec->ActiveStates.Reset();
				return Exec->TreeRunStatus;
			}

			// Append and consume the events accumulated during the state exit.
			EventsToProcess.Append(EventQueue.GetEvents());
			EventQueue.Reset();

			// Enter state tasks can fail/succeed, treat it same as tick.
			const EStateTreeRunStatus LastTickStatus = EnterState(NextTransition);

			NextTransition.Reset();

			// Need to reacquire the exec state as EnterState may alter the allocation. 
			Exec = &GetExecState();
			Exec->LastTickStatus = LastTickStatus;

			// Consider events so far processed. Events sent during EnterState went into EventQueue, and are processed in next iteration.
			EventsToProcess.Reset();

			// Report state completed immediately.
			if (Exec->LastTickStatus != EStateTreeRunStatus::Running)
			{
				StateCompleted();
			}
		}

		// Stop as soon as have found a running state.
		if (Exec->LastTickStatus == EStateTreeRunStatus::Running)
		{
			break;
		}
	}

	if (Exec->ActiveStates.IsEmpty())
	{
		// Should not happen. This may happen if a state completion transition could not be selected. 
		STATETREE_LOG(Error, TEXT("%s: Failed to select state on '%s' using StateTree '%s'. This should not happen, state completion transition is likely missing."),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		Exec->TreeRunStatus = EStateTreeRunStatus::Failed;
		return Exec->TreeRunStatus;
	}

	EventsToProcess.Reset();

	return Exec->TreeRunStatus;
}

EStateTreeRunStatus FStateTreeExecutionContext::GetStateTreeRunStatus() const
{
	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return EStateTreeRunStatus::Failed;
	}

	if (const FStateTreeExecutionState* Exec = InstanceData.GetExecutionState())
	{
		return Exec->TreeRunStatus;
	}
	
	return EStateTreeRunStatus::Failed;
}

void FStateTreeExecutionContext::SendEvent(const FStateTreeEvent& Event) const
{
	SendEvent(Event.Tag, Event.Payload, Event.Origin);
}

void FStateTreeExecutionContext::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin) const
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_SendEvent);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return;
	}

	if (!InstanceData.IsValid())
	{
		STATETREE_LOG(Error, TEXT("%s: SendEvent called on %s using StateTree %s with invalid instance data. Start() must be called before sending events."),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return;
	}

	STATETREE_LOG(Verbose, TEXT("Send Event '%s'"), *Tag.ToString());

	FStateTreeEventQueue& EventQueue = InstanceData.GetMutableEventQueue();
	EventQueue.SendEvent(&Owner, Tag, Payload, Origin);
}

void FStateTreeExecutionContext::RequestTransition(const FStateTreeTransitionRequest& Request)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_RequestTransition);

	if (!IsValid())
	{
		STATETREE_LOG(Warning, TEXT("%s: StateTree context is not initialized properly ('%s' using StateTree '%s')"),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return;
	}

	if (!InstanceData.IsValid())
	{
		STATETREE_LOG(Error, TEXT("%s: RequestTransition called on %s using StateTree %s with invalid instance data. Start() must be called before requesting transition."),
			ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return;
	}

	STATETREE_LOG(Verbose, TEXT("Request transition to '%s' at priority %s"), *GetSafeStateName(Request.TargetState), *UEnum::GetDisplayValueAsText(Request.Priority).ToString());

	if (bAllowDirectTransitions)
	{
		RequestTransition(Request.TargetState, Request.Priority);		
	}
	else
	{
		FStateTreeTransitionRequest RequestWithSource = Request;
		RequestWithSource.SourceState = CurrentlyProcessedState;
		InstanceData.AddTransitionRequest(&Owner, RequestWithSource);
	}
}

void FStateTreeExecutionContext::UpdateInstanceData(const FStateTreeActiveStates& CurrentActiveStates, const FStateTreeActiveStates& NextActiveStates)
{
	// Find common section of states at start.
	int32 NumCommon = 0;
	while (NumCommon < CurrentActiveStates.Num() && NumCommon < NextActiveStates.Num())
	{
		if (CurrentActiveStates[NumCommon] != NextActiveStates[NumCommon])
		{
			break;
		}
		NumCommon++;
	}

	// @todo: change this so that we only put the newly added structs and objects here.
	TArray<FConstStructView> InstanceStructs;
	TArray<const UObject*> InstanceObjects;
	
	int32 NumCommonInstanceStructs = 0;
	int32 NumCommonInstanceObjects = 0;

	// Exec
	InstanceStructs.Add(StateTree.DefaultInstanceData.GetStruct(0));

	// Evaluators
	for (int32 EvalIndex = StateTree.EvaluatorsBegin; EvalIndex < (StateTree.EvaluatorsBegin + StateTree.EvaluatorsNum); EvalIndex++)
	{
		const FStateTreeEvaluatorBase& Eval =  StateTree.Nodes[EvalIndex].Get<FStateTreeEvaluatorBase>();
		if (Eval.bInstanceIsObject)
		{
			InstanceObjects.Add(StateTree.DefaultInstanceData.GetObject(Eval.InstanceIndex.Get()));
		}
		else
		{
			InstanceStructs.Add(StateTree.DefaultInstanceData.GetStruct(Eval.InstanceIndex.Get()));
		}
	}

	// Global tasks
	for (int32 TaskIndex = StateTree.GlobalTasksBegin; TaskIndex < (StateTree.GlobalTasksBegin + StateTree.GlobalTasksNum); TaskIndex++)
	{
		const FStateTreeTaskBase& Task =  StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
		if (Task.bInstanceIsObject)
		{
			InstanceObjects.Add(StateTree.DefaultInstanceData.GetObject(Task.InstanceIndex.Get()));
		}
		else
		{
			InstanceStructs.Add(StateTree.DefaultInstanceData.GetStruct(Task.InstanceIndex.Get()));
		}
	}

	// Expect initialized instance data to contain the common instances.
	if (InstanceData.IsValid())
	{
		NumCommonInstanceStructs = InstanceStructs.Num();
		NumCommonInstanceObjects = InstanceObjects.Num();
	}
	
	// Tasks
	const int32 FirstTaskStructIndex = InstanceStructs.Num();
	const int32 FirstTaskObjectIndex = InstanceObjects.Num();
	
	for (int32 Index = 0; Index < NextActiveStates.Num(); Index++)
	{
		const FStateTreeStateHandle CurrentHandle = NextActiveStates[Index];
		const FCompactStateTreeState& State = StateTree.States[CurrentHandle.Index];

		if (State.Type == EStateTreeStateType::Linked)
		{
			check(State.ParameterInstanceIndex.IsValid());
			InstanceStructs.Add(StateTree.DefaultInstanceData.GetStruct(State.ParameterInstanceIndex.Get()));
		}
		
		for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
		{
			const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
			if (Task.bInstanceIsObject)
			{
				InstanceObjects.Add(StateTree.DefaultInstanceData.GetObject(Task.InstanceIndex.Get()));
			}
			else
			{
				InstanceStructs.Add(StateTree.DefaultInstanceData.GetStruct(Task.InstanceIndex.Get()));
			}
		}
		
		if (Index < NumCommon)
		{
			NumCommonInstanceStructs = InstanceStructs.Num();
			NumCommonInstanceObjects = InstanceObjects.Num();
		}
	}

	// Common section should match.
	// @todo: put this behind a define when enough testing has been done.
	for (int32 Index = 0; Index < NumCommonInstanceStructs; Index++)
	{
		check(Index < InstanceData.NumStructs());
		check(InstanceStructs[Index].GetScriptStruct() == InstanceData.GetStruct(Index).GetScriptStruct());
	}
	for (int32 Index = 0; Index < NumCommonInstanceObjects; Index++)
	{
		check(Index < InstanceData.NumObjects());
		check(InstanceObjects[Index] != nullptr
			&& InstanceData.GetObject(Index) != nullptr
			&& InstanceObjects[Index]->GetClass() == InstanceData.GetObject(Index)->GetClass());
	}

	// Remove instance data that was not common.
	InstanceData.ShrinkTo(NumCommonInstanceStructs, NumCommonInstanceObjects);

	// Add new instance data.
	InstanceData.Append(Owner,
		MakeArrayView(InstanceStructs.GetData() + NumCommonInstanceStructs, InstanceStructs.Num() - NumCommonInstanceStructs),
		MakeArrayView(InstanceObjects.GetData() + NumCommonInstanceObjects, InstanceObjects.Num() - NumCommonInstanceObjects));

	FStateTreeExecutionState& Exec = GetExecState();
	Exec.FirstTaskStructIndex = FStateTreeIndex16(FirstTaskStructIndex);
	Exec.FirstTaskObjectIndex = FStateTreeIndex16(FirstTaskObjectIndex);
}

EStateTreeRunStatus FStateTreeExecutionContext::EnterState(const FStateTreeTransitionResult& Transition)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_EnterState);

	if (Transition.NextActiveStates.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	// Allocate new tasks.
	UpdateInstanceData(Transition.CurrentActiveStates, Transition.NextActiveStates);

	FStateTreeExecutionState& Exec = GetExecState();
	Exec.StateChangeCount++;
	Exec.CompletedStateHandle = FStateTreeStateHandle::Invalid;

	// On target branch means that the state is the target of current transition or child of it.
	// States which were active before and will remain active, but are not on target branch will not get
	// EnterState called. That is, a transition is handled as "replan from this state".
	bool bOnTargetBranch = false;

	FStateTreeTransitionResult CurrentTransition = Transition;
	
	EStateTreeRunStatus Result = EStateTreeRunStatus::Running;

	Exec.EnterStateFailedTaskIndex = FStateTreeIndex16::Invalid; // This will make all tasks to be accepted.
	Exec.ActiveStates.Reset();
	
	// Do property copy on all states, propagating the results from last tick.
	check(Exec.FirstTaskStructIndex.IsValid() && Exec.FirstTaskObjectIndex.IsValid()); 
	int32 InstanceStructIndex = Exec.FirstTaskStructIndex.Get();
	int32 InstanceObjectIndex = Exec.FirstTaskObjectIndex.Get();

	STATETREE_LOG(Log, TEXT("Enter state '%s' (%d)"), *DebugGetStatePath(Transition.NextActiveStates), Exec.StateChangeCount);

	for (int32 Index = 0; Index < Transition.NextActiveStates.Num() && Result != EStateTreeRunStatus::Failed; Index++)
	{
		const FStateTreeStateHandle CurrentHandle = Transition.NextActiveStates[Index];
		const FStateTreeStateHandle PreviousHandle = Transition.CurrentActiveStates.GetStateSafe(Index);
		const FCompactStateTreeState& State = StateTree.States[CurrentHandle.Index];

		if (!Exec.ActiveStates.Push(CurrentHandle))
		{
			STATETREE_LOG(Error, TEXT("%s: Reached max execution depth when trying to enter state '%s'.  '%s' using StateTree '%s'."),
				ANSI_TO_TCHAR(__FUNCTION__), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
			break;
		}

		if (State.Type == EStateTreeStateType::Linked)
		{
			UpdateLinkedStateParameters(State, InstanceStructIndex);
			InstanceStructIndex++;
		}
		else if (State.Type == EStateTreeStateType::Subtree)
		{
			UpdateSubtreeStateParameters(State);
		}

		bOnTargetBranch = bOnTargetBranch || CurrentHandle == Transition.TargetState;
		const bool bWasActive = PreviousHandle == CurrentHandle;
		const bool bIsEnteringState = !bWasActive || bOnTargetBranch;

		CurrentTransition.CurrentState = CurrentHandle;
		CurrentTransition.ChangeType = bWasActive ? EStateTreeStateChangeType::Sustained : EStateTreeStateChangeType::Changed;

		STATETREE_CLOG(bIsEnteringState, Log, TEXT("%*sState '%s' %s"), Index*UE::StateTree::DebugIndentSize, TEXT(""), *DebugGetStatePath(Transition.NextActiveStates, Index), *UEnum::GetDisplayValueAsText(CurrentTransition.ChangeType).ToString());
		
		// Activate tasks on current state.
		for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
		{
			const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
			SetNodeDataView(Task, InstanceStructIndex, InstanceObjectIndex);

			// Copy bound properties.
			if (Task.BindingsBatch.IsValid())
			{
				StateTree.PropertyBindings.CopyTo(DataViews, Task.BindingsBatch, DataViews[Task.DataViewIndex.Get()]);
			}

			const bool bShouldCallStateChange = CurrentTransition.ChangeType == EStateTreeStateChangeType::Changed
												|| (CurrentTransition.ChangeType == EStateTreeStateChangeType::Sustained && Task.bShouldStateChangeOnReselect);

			if (bIsEnteringState && bShouldCallStateChange)
			{
				STATETREE_LOG(Verbose, TEXT("%*s  Task '%s'"), Index*UE::StateTree::DebugIndentSize, TEXT(""), *Task.Name.ToString());
				QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_EnterState);
				CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Task_EnterState);
				const EStateTreeRunStatus Status = Task.EnterState(*this, CurrentTransition);

				if (Status != EStateTreeRunStatus::Running)
				{
					// Store the first state that completed, will be used to decide where to trigger transitions.
					if (!Exec.CompletedStateHandle.IsValid())
					{
						Exec.CompletedStateHandle = CurrentHandle;
					}
					Result = Status;
				}
				
				if (Status == EStateTreeRunStatus::Failed)
				{
					// Store how far in the enter state we got. This will be used to match the StateCompleted() and ExitState() calls.
					Exec.EnterStateFailedTaskIndex = FStateTreeIndex16(TaskIndex);
					break;
				}
			}
		}
	}

	return Result;
}

void FStateTreeExecutionContext::ExitState(const FStateTreeTransitionResult& Transition)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_ExitState);

	if (Transition.CurrentActiveStates.IsEmpty())
	{
		return;
	}

	FStateTreeExecutionState& Exec = GetExecState();

	// On target branch means that the state is the target of current transition or child of it.
	// States which were active before and will remain active, but are not on target branch will not get
	// EnterState called. That is, a transition is handled as "replan from this state".
	bool bOnTargetBranch = false;

	FStateTreeStateHandle ExitedStates[FStateTreeActiveStates::MaxStates];
	EStateTreeStateChangeType ExitedStateChangeType[FStateTreeActiveStates::MaxStates];
	int32 ExitedStateActiveIndex[FStateTreeActiveStates::MaxStates];
	int32 NumExitedStates = 0;
	
	// Do property copy on all states, propagating the results from last tick.
	// Collect the states that need to be called, the actual call is done below in reverse order.
	check(Exec.FirstTaskStructIndex.IsValid() && Exec.FirstTaskObjectIndex.IsValid()); 
	int32 InstanceStructIndex = Exec.FirstTaskStructIndex.Get();
	int32 InstanceObjectIndex = Exec.FirstTaskObjectIndex.Get();

	for (int32 Index = 0; Index < Transition.CurrentActiveStates.Num(); Index++)
	{
		const FStateTreeStateHandle CurrentHandle = Transition.CurrentActiveStates[Index];
		const FStateTreeStateHandle NextHandle = Transition.NextActiveStates.GetStateSafe(Index);
		const FCompactStateTreeState& State = StateTree.States[CurrentHandle.Index];

		if (State.Type == EStateTreeStateType::Linked)
		{
			UpdateLinkedStateParameters(State, InstanceStructIndex);
			InstanceStructIndex++;
		}
		else if (State.Type == EStateTreeStateType::Subtree)
		{
			UpdateSubtreeStateParameters(State);
		}

		const bool bRemainsActive = NextHandle == CurrentHandle;
		bOnTargetBranch = bOnTargetBranch || NextHandle == Transition.TargetState;
		const EStateTreeStateChangeType ChangeType = bRemainsActive ? EStateTreeStateChangeType::Sustained : EStateTreeStateChangeType::Changed;

		if (!bRemainsActive || bOnTargetBranch)
		{
			// Should call ExitState() on this state.
			check (NumExitedStates < FStateTreeActiveStates::MaxStates);
			ExitedStates[NumExitedStates] = CurrentHandle;
			ExitedStateChangeType[NumExitedStates] = ChangeType;
			ExitedStateActiveIndex[NumExitedStates] = Index;
			NumExitedStates++;
		}

		// Do property copies, ExitState() is called below.
		for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
		{
			const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
			SetNodeDataView(Task, InstanceStructIndex, InstanceObjectIndex);

			// Copy bound properties.
			if (Task.BindingsBatch.IsValid() && Task.bShouldCopyBoundPropertiesOnExitState)
			{
				StateTree.PropertyBindings.CopyTo(DataViews, Task.BindingsBatch, DataViews[Task.DataViewIndex.Get()]);
			}
		}
	}

	// Call in reverse order.
	STATETREE_LOG(Log, TEXT("Exit state '%s' (%d)"), *DebugGetStatePath(Transition.CurrentActiveStates), Exec.StateChangeCount);

	FStateTreeTransitionResult CurrentTransition = Transition;

	for (int32 Index = NumExitedStates - 1; Index >= 0; Index--)
	{
		const FStateTreeStateHandle CurrentHandle = ExitedStates[Index];
		const FCompactStateTreeState& State = StateTree.States[CurrentHandle.Index];

		// Remove any delayed transitions that belong to this state.
		Exec.DelayedTransitions.RemoveAllSwap(
			[Begin = State.TransitionsBegin, End = State.TransitionsBegin + State.TransitionsNum](const FStateTreeTransitionDelayedState& DelayedState)
			{
				return DelayedState.TransitionIndex.Get() >= Begin && DelayedState.TransitionIndex.Get() < End;
			});
		
		CurrentTransition.CurrentState = CurrentHandle;
		CurrentTransition.ChangeType = ExitedStateChangeType[Index];

		STATETREE_LOG(Log, TEXT("%*sState '%s' %s"), Index*UE::StateTree::DebugIndentSize, TEXT(""), *DebugGetStatePath(Transition.CurrentActiveStates, ExitedStateActiveIndex[Index]), *UEnum::GetDisplayValueAsText(CurrentTransition.ChangeType).ToString());

		// Tasks
		for (int32 TaskIndex = (State.TasksBegin + State.TasksNum) - 1; TaskIndex >= State.TasksBegin; TaskIndex--)
		{
			// Call task completed only if EnterState() was called.
			// The task order in the tree (BF) allows us to use the comparison.
			// Relying here that invalid value of Exec.EnterStateFailedTaskIndex == MAX_uint16.
			if (TaskIndex <= Exec.EnterStateFailedTaskIndex.Get())
			{
				const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();

				const bool bShouldCallStateChange = CurrentTransition.ChangeType == EStateTreeStateChangeType::Changed
							|| (CurrentTransition.ChangeType == EStateTreeStateChangeType::Sustained && Task.bShouldStateChangeOnReselect);

				if (bShouldCallStateChange)
				{
					STATETREE_LOG(Verbose, TEXT("%*s  Task '%s'"), Index*UE::StateTree::DebugIndentSize, TEXT(""), *Task.Name.ToString());
					{
						QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_ExitState);
						CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Task_ExitState);
						Task.ExitState(*this, CurrentTransition);
					}
				}
			}
		}
	}
}

void FStateTreeExecutionContext::StateCompleted()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_StateCompleted);

	const FStateTreeExecutionState& Exec = GetExecState();

	if (Exec.ActiveStates.IsEmpty())
	{
		return;
	}

	STATETREE_LOG(Verbose, TEXT("State Completed %s (%d)"), *UEnum::GetDisplayValueAsText(Exec.LastTickStatus).ToString(), Exec.StateChangeCount);

	// Call from child towards root to allow to pass results back.
	// Note: Completed is assumed to be called immediately after tick or enter state, so there's no property copying.
	for (int32 Index = Exec.ActiveStates.Num() - 1; Index >= 0; Index--)
	{
		const FStateTreeStateHandle CurrentHandle = Exec.ActiveStates[Index];
		const FCompactStateTreeState& State = StateTree.States[CurrentHandle.Index];

		FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);
		
		STATETREE_LOG(Verbose, TEXT("%*sState '%s'"), Index*UE::StateTree::DebugIndentSize, TEXT(""), *DebugGetStatePath(Exec.ActiveStates, Index));

		// Notify Tasks
		for (int32 TaskIndex = (State.TasksBegin + State.TasksNum) - 1; TaskIndex >= State.TasksBegin; TaskIndex--)
		{
			// Call task completed only if EnterState() was called.
			// The task order in the tree (BF) allows us to use the comparison.
			// Relying here that invalid value of Exec.EnterStateFailedTaskIndex == MAX_uint16.
			if (TaskIndex <= Exec.EnterStateFailedTaskIndex.Get())
			{
				const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();

				STATETREE_LOG(Verbose, TEXT("%*s  Task '%s'"), Index*UE::StateTree::DebugIndentSize, TEXT(""), *Task.Name.ToString());
				Task.StateCompleted(*this, Exec.LastTickStatus, Exec.ActiveStates);
			}
		}
	}
}

EStateTreeRunStatus FStateTreeExecutionContext::TickEvaluatorsAndGlobalTasks(const float DeltaTime, bool bTickGlobalTasks)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_TickEvaluators);

	STATETREE_CLOG(StateTree.EvaluatorsNum > 0, Verbose, TEXT("Ticking Evaluators"));

	// Tick evaluators
	int32 InstanceStructIndex = 1; // Exec is at index 0
	int32 InstanceObjectIndex = 0;
	
	for (int32 EvalIndex = StateTree.EvaluatorsBegin; EvalIndex < (StateTree.EvaluatorsBegin + StateTree.EvaluatorsNum); EvalIndex++)
	{
		const FStateTreeEvaluatorBase& Eval = StateTree.Nodes[EvalIndex].Get<FStateTreeEvaluatorBase>();
		SetNodeDataView(Eval, InstanceStructIndex, InstanceObjectIndex);

		// Copy bound properties.
		if (Eval.BindingsBatch.IsValid())
		{
			StateTree.PropertyBindings.CopyTo(DataViews, Eval.BindingsBatch, DataViews[Eval.DataViewIndex.Get()]);
		}
		STATETREE_LOG(Verbose, TEXT("  Tick: '%s'"), *Eval.Name.ToString());
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Eval_Tick);
			Eval.Tick(*this, DeltaTime);
		}
	}


	EStateTreeRunStatus Result = EStateTreeRunStatus::Running;

	if (bTickGlobalTasks)
	{
		// Used to stop ticking tasks after one fails, but we still want to keep updating the data views so that property binding works properly.
		bool bShouldTickTasks = true;
		const bool bHasEvents = !EventsToProcess.IsEmpty();

		for (int32 TaskIndex = StateTree.GlobalTasksBegin; TaskIndex < (StateTree.GlobalTasksBegin + StateTree.GlobalTasksNum); TaskIndex++)
		{
			const FStateTreeTaskBase& Task =  StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
			SetNodeDataView(Task, InstanceStructIndex, InstanceObjectIndex);

			const bool bNeedsTick = bShouldTickTasks && (Task.bShouldCallTick || (bHasEvents && Task.bShouldCallTickOnlyOnEvents));
			STATETREE_LOG(VeryVerbose, TEXT("  Tick: '%s' %s"), *Task.Name.ToString(), !bNeedsTick ? TEXT("[not ticked]") : TEXT(""));
			if (!bNeedsTick)
			{
				continue;
			}

			// Copy bound properties.
			// Only copy properties when the task is actually ticked, and copy properties at tick is requested.
			if (Task.BindingsBatch.IsValid() && Task.bShouldCopyBoundPropertiesOnTick)
			{
				StateTree.PropertyBindings.CopyTo(DataViews, Task.BindingsBatch, DataViews[Task.DataViewIndex.Get()]);
			}

			{
				QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_Tick);
				CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Task_Tick);

				const EStateTreeRunStatus TaskResult = Task.Tick(*this, DeltaTime);

				// If a global task succeeds or fails, it will stop the whole tree.
				if (TaskResult != EStateTreeRunStatus::Running)
				{
					Result = TaskResult;
				}
					
				if (TaskResult == EStateTreeRunStatus::Failed)
				{
					bShouldTickTasks = false;
				}
			}
		}
	}

	return Result;
}

bool FStateTreeExecutionContext::StartEvaluatorsAndGlobalTasks(FStateTreeIndex16& OutLastInitializedTaskIndex)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_StartEvaluators);

	STATETREE_CLOG(StateTree.EvaluatorsNum > 0 || StateTree.GlobalTasksNum > 0, Verbose, TEXT("Start Evaluators & Global tasks"));

	OutLastInitializedTaskIndex = FStateTreeIndex16();
	bool bResult = true;
	
	// Start evaluators
	int32 InstanceStructIndex = 1; // Exec is at index 0
	int32 InstanceObjectIndex = 0;
	
	for (int32 EvalIndex = StateTree.EvaluatorsBegin; EvalIndex < (StateTree.EvaluatorsBegin + StateTree.EvaluatorsNum); EvalIndex++)
	{
		const FStateTreeEvaluatorBase& Eval = StateTree.Nodes[EvalIndex].Get<FStateTreeEvaluatorBase>();
		SetNodeDataView(Eval, InstanceStructIndex, InstanceObjectIndex);

		// Copy bound properties.
		if (Eval.BindingsBatch.IsValid())
		{
			StateTree.PropertyBindings.CopyTo(DataViews, Eval.BindingsBatch, DataViews[Eval.DataViewIndex.Get()]);
		}
		STATETREE_LOG(Verbose, TEXT("  Start: '%s'"), *Eval.Name.ToString());
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Eval_TreeStart);
			Eval.TreeStart(*this);
		}
	}

	// Start Global tasks
	// Even if we call Enter/ExitState() on global tasks, they do not enter any specific state.
	const FStateTreeTransitionResult Transition = {}; // Empty transition
	
	for (int32 TaskIndex = StateTree.GlobalTasksBegin; TaskIndex < (StateTree.GlobalTasksBegin + StateTree.GlobalTasksNum); TaskIndex++)
	{
		const FStateTreeTaskBase& Task =  StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
		SetNodeDataView(Task, InstanceStructIndex, InstanceObjectIndex);

		// Copy bound properties.
		if (Task.BindingsBatch.IsValid())
		{
			StateTree.PropertyBindings.CopyTo(DataViews, Task.BindingsBatch, DataViews[Task.DataViewIndex.Get()]);
		}
		STATETREE_LOG(Verbose, TEXT("  Start: '%s'"), *Task.Name.ToString());
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_TreeStart);
			if (Task.EnterState(*this, Transition) != EStateTreeRunStatus::Running)
			{
				OutLastInitializedTaskIndex = FStateTreeIndex16(TaskIndex);
				bResult = false;
				break;
			}
		}
	}

	return bResult;
}

void FStateTreeExecutionContext::StopEvaluatorsAndGlobalTasks(const FStateTreeIndex16 LastInitializedTaskIndex)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_StopEvaluators);

	STATETREE_CLOG(StateTree.EvaluatorsNum > 0, Verbose, TEXT("Stop Evaluators & Global Tasks"));

	// Stop evaluators
	int32 InstanceStructIndex = 1; // Exec is at index 0
	int32 InstanceObjectIndex = 0;
	
	for (int32 EvalIndex = StateTree.EvaluatorsBegin; EvalIndex < (StateTree.EvaluatorsBegin + StateTree.EvaluatorsNum); EvalIndex++)
	{
		const FStateTreeEvaluatorBase& Eval = StateTree.Nodes[EvalIndex].Get<FStateTreeEvaluatorBase>();
		SetNodeDataView(Eval, InstanceStructIndex, InstanceObjectIndex);

		// Copy bound properties.
		if (Eval.BindingsBatch.IsValid())
		{
			StateTree.PropertyBindings.CopyTo(DataViews, Eval.BindingsBatch, DataViews[Eval.DataViewIndex.Get()]);
		}
	}

	// Stop Global tasks
	for (int32 TaskIndex = StateTree.GlobalTasksBegin; TaskIndex < (StateTree.GlobalTasksBegin + StateTree.GlobalTasksNum); TaskIndex++)
	{
		const FStateTreeTaskBase& Task =  StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
		SetNodeDataView(Task, InstanceStructIndex, InstanceObjectIndex);

		// Copy bound properties.
		if (Task.BindingsBatch.IsValid() && Task.bShouldCopyBoundPropertiesOnExitState)
		{
			StateTree.PropertyBindings.CopyTo(DataViews, Task.BindingsBatch, DataViews[Task.DataViewIndex.Get()]);
		}
	}


	// Call in reverse order.	
	const FStateTreeTransitionResult Transition = {}; // Empty transition

	for (int32 TaskIndex = (StateTree.GlobalTasksBegin + StateTree.GlobalTasksNum) - 1;  TaskIndex >= StateTree.GlobalTasksBegin ; TaskIndex--)
	{
		const FStateTreeTaskBase& Task =  StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();

		UE_LOG(LogTemp, Error, TEXT("**** %s: Exit Global Task %s exec=%s"), *GetNameSafe(&Owner), *Task.Name.ToString(), *LexToString(TaskIndex <= LastInitializedTaskIndex.Get()));

		// Relying here that invalid value of LastInitializedTaskIndex == MAX_uint16.
		if (TaskIndex <= LastInitializedTaskIndex.Get())
		{
			STATETREE_LOG(Verbose, TEXT("  Stop: '%s'"), *Task.Name.ToString());
			{
				QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_TreeStop);
				Task.ExitState(*this, Transition);
			}
		}
	}

	for (int32 EvalIndex = (StateTree.EvaluatorsBegin + StateTree.EvaluatorsNum) - 1; EvalIndex >= StateTree.EvaluatorsBegin; EvalIndex--)
	{
		const FStateTreeEvaluatorBase& Eval = StateTree.Nodes[EvalIndex].Get<FStateTreeEvaluatorBase>();
		STATETREE_LOG(Verbose, TEXT("  Stop: '%s'"), *Eval.Name.ToString());
		{
			QUICK_SCOPE_CYCLE_COUNTER(StateTree_Eval_TreeStop);
			Eval.TreeStop(*this);
		}
	}
}

EStateTreeRunStatus FStateTreeExecutionContext::TickTasks(const float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_TickTasks);

	FStateTreeExecutionState& Exec = GetExecState();

	if (Exec.ActiveStates.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	EStateTreeRunStatus Result = EStateTreeRunStatus::Running;
	int32 NumTotalTasks = 0;

	const bool bHasEvents = !EventsToProcess.IsEmpty();
	
	check(Exec.FirstTaskStructIndex.IsValid() && Exec.FirstTaskObjectIndex.IsValid()); 
	int32 InstanceStructIndex = Exec.FirstTaskStructIndex.Get();
	int32 InstanceObjectIndex = Exec.FirstTaskObjectIndex.Get();

	Exec.CompletedStateHandle = FStateTreeStateHandle::Invalid;
	
	// Used to stop ticking tasks after one fails, but we still want to keep updating the data views so that property binding works properly.
	bool bShouldTickTasks = true;

	STATETREE_CLOG(Exec.ActiveStates.Num() > 0, VeryVerbose, TEXT("Ticking Tasks"));

	for (int32 Index = 0; Index < Exec.ActiveStates.Num(); Index++)
	{
		const FStateTreeStateHandle CurrentHandle = Exec.ActiveStates[Index];
		const FCompactStateTreeState& State = StateTree.States[CurrentHandle.Index];

		FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);
		
		STATETREE_CLOG(State.TasksNum > 0, VeryVerbose, TEXT("%*sState '%s'"), Index*UE::StateTree::DebugIndentSize, TEXT(""), *DebugGetStatePath(Exec.ActiveStates, Index));

		if (State.Type == EStateTreeStateType::Linked)
		{
			UpdateLinkedStateParameters(State, InstanceStructIndex);
			InstanceStructIndex++;
		}
		else if (State.Type == EStateTreeStateType::Subtree)
		{
			UpdateSubtreeStateParameters(State);
		}

		// Update Tasks data and tick if possible (ie. if no task has yet failed and so bShouldTickTasks is true)
		for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
		{
			const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
			SetNodeDataView(Task, InstanceStructIndex, InstanceObjectIndex);

			const bool bNeedsTick = bShouldTickTasks && (Task.bShouldCallTick || (bHasEvents && Task.bShouldCallTickOnlyOnEvents));
			STATETREE_LOG(VeryVerbose, TEXT("%*s  Tick: '%s' %s"), Index*UE::StateTree::DebugIndentSize, TEXT(""), *Task.Name.ToString(), !bNeedsTick ? TEXT("[not ticked]") : TEXT(""));
			if (!bNeedsTick)
			{
				continue;
			}
			
			// Copy bound properties.
			// Only copy properties when the task is actually ticked, and copy properties at tick is requested.
			if (Task.BindingsBatch.IsValid() && Task.bShouldCopyBoundPropertiesOnTick)
			{
				StateTree.PropertyBindings.CopyTo(DataViews, Task.BindingsBatch, DataViews[Task.DataViewIndex.Get()]);
			}
			
			{
				QUICK_SCOPE_CYCLE_COUNTER(StateTree_Task_Tick);
				CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_Task_Tick);

				const EStateTreeRunStatus TaskResult = Task.Tick(*this, DeltaTime);

				// TODO: Add more control over which states can control the failed/succeeded result.
				if (TaskResult != EStateTreeRunStatus::Running)
				{
					// Store the first state that completed, will be used to decide where to trigger transitions.
					if (!Exec.CompletedStateHandle.IsValid())
					{
						Exec.CompletedStateHandle = CurrentHandle;
					}
					Result = TaskResult;
				}
				
				if (TaskResult == EStateTreeRunStatus::Failed)
				{
					bShouldTickTasks = false;
				}
			}
		}
		NumTotalTasks += State.TasksNum;
	}

	if (NumTotalTasks == 0)
	{
		// No tasks, done ticking.
		Result = EStateTreeRunStatus::Succeeded;
	}

	return Result;
}

bool FStateTreeExecutionContext::TestAllConditions(const int32 ConditionsOffset, const int32 ConditionsNum)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_TestConditions);

	if (ConditionsNum == 0 || !SharedInstanceData.IsValid())
	{
		return true;
	}

	TStaticArray<EStateTreeConditionOperand, UE::StateTree::MaxConditionIndent + 1> Operands(InPlace, EStateTreeConditionOperand::Copy);
	TStaticArray<bool, UE::StateTree::MaxConditionIndent + 1> Values(InPlace, false);

	int32 Level = 0;
	
	for (int32 Index = 0; Index < ConditionsNum; Index++)
	{
		const FStateTreeConditionBase& Cond = StateTree.Nodes[ConditionsOffset + Index].Get<FStateTreeConditionBase>();
		if (Cond.bInstanceIsObject)
		{
			DataViews[Cond.DataViewIndex.Get()] = SharedInstanceData->GetMutableObject(Cond.InstanceIndex.Get());
		}
		else
		{
			DataViews[Cond.DataViewIndex.Get()] = SharedInstanceData->GetMutableStruct(Cond.InstanceIndex.Get());
		}

		// Copy bound properties.
		if (Cond.BindingsBatch.IsValid())
		{
			if (!StateTree.PropertyBindings.CopyTo(DataViews, Cond.BindingsBatch, DataViews[Cond.DataViewIndex.Get()]))
			{
				// If the source data cannot be accessed, the whole expression evaluates to false.
				Values[0] = false;
				break;
			}
		}

		const bool bValue = Cond.TestCondition(*this);

		// Reset copied properties that might contain object references.
		if (Cond.BindingsBatch.IsValid())
		{
			StateTree.PropertyBindings.ResetObjects(Cond.BindingsBatch, DataViews[Cond.DataViewIndex.Get()]);
		}

		const int32 DeltaIndent = Cond.DeltaIndent;
		const int32 OpenParens = FMath::Max(0, DeltaIndent) + 1;	// +1 for the current value that is stored at the empty slot at the top of the value stack.
		const int32 ClosedParens = FMath::Max(0, -DeltaIndent) + 1;

		// Store the operand to apply when merging higher level down when returning to this level.
		// @todo: remove this conditions in 5.1, needs resaving existing StateTrees.
		const EStateTreeConditionOperand Operand = Index == 0 ? EStateTreeConditionOperand::Copy : Cond.Operand;
		Operands[Level] = Operand;

		// Store current value at the top of the stack.
		Level += OpenParens;
		Values[Level] = bValue;

		// Evaluate and merge down values based on closed braces.
		// The current value is placed in parens (see +1 above), which makes merging down and applying the new value consistent.
		// The default operand is copy, so if the value is needed immediately, it is just copied down, or if we're on the same level,
		// the operand storing above gives handles with the right logic.
		for (int32 Paren = 0; Paren < ClosedParens; Paren++)
		{
			Level--;
			switch (Operands[Level])
			{
			case EStateTreeConditionOperand::Copy:
				Values[Level] = Values[Level + 1];
				break;
			case EStateTreeConditionOperand::And:
				Values[Level] &= Values[Level + 1];
				break;
			case EStateTreeConditionOperand::Or:
				Values[Level] |= Values[Level + 1];
				break;
			}
			Operands[Level] = EStateTreeConditionOperand::Copy;
		}
	}
	
	return Values[0];
}

FString FStateTreeExecutionContext::DebugGetEventsAsString() const
{
	FString Result;
	for (const FStateTreeEvent& Event : EventsToProcess)
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT(", ");
		}
		Result += Event.Tag.ToString();
	}
	return Result;
}

bool FStateTreeExecutionContext::RequestTransition(const FStateTreeStateHandle NextState, const EStateTreeTransitionPriority Priority)
{
	// Skip lower priority transitions.
	if (NextTransition.Priority >= Priority)
	{
		return false;
	}

	const FStateTreeExecutionState& Exec = GetExecState();
	
	if (NextState.IsCompletionState())
	{
		NextTransition.CurrentActiveStates = Exec.ActiveStates;
		NextTransition.CurrentRunStatus = Exec.TreeRunStatus;
		NextTransition.SourceState = CurrentlyProcessedState;
		NextTransition.TargetState = NextState;
		NextTransition.NextActiveStates = FStateTreeActiveStates(NextState);
		NextTransition.Priority = Priority;

		STATETREE_LOG(Verbose, TEXT("Transition on state '%s' -[%s]-> state '%s'"),
			*GetSafeStateName(NextTransition.CurrentActiveStates.Last()), *GetSafeStateName(NextState), *GetSafeStateName(NextTransition.NextActiveStates.Last()));

		return true;
	}
	if (!NextState.IsValid())
	{
		// NotSet is no-operation, but can be used to mask a transition at parent state. Returning unset keeps updating current state.
		NextTransition.CurrentActiveStates = Exec.ActiveStates;
		NextTransition.CurrentRunStatus = Exec.TreeRunStatus;
		NextTransition.SourceState = CurrentlyProcessedState;
		NextTransition.TargetState = FStateTreeStateHandle::Invalid;
		NextTransition.NextActiveStates.Reset();
		NextTransition.Priority = Priority;
		return true;
	}

	FStateTreeActiveStates NewActiveState;
	if (SelectState(NextState, NewActiveState))
	{
		NextTransition.CurrentActiveStates = Exec.ActiveStates;
		NextTransition.CurrentRunStatus = Exec.TreeRunStatus;
		NextTransition.SourceState = CurrentlyProcessedState;
		NextTransition.TargetState = NextState;
		NextTransition.NextActiveStates = NewActiveState;
		NextTransition.Priority = Priority;

		STATETREE_LOG(Verbose, TEXT("Transition on state '%s' -[%s]-> state '%s'"),
			*GetSafeStateName(NextTransition.CurrentActiveStates.Last()), *GetSafeStateName(NextState), *GetSafeStateName(NextTransition.NextActiveStates.Last()));
		
		return true;
	}
		
	return false;
}

bool FStateTreeExecutionContext::TriggerTransitions()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_TriggerTransition);

	FAllowDirectTransitionsScope AllowDirectTransitionsScope(*this); // Set flag for the scope of this function to allow direct transitions without buffering.
	FStateTreeExecutionState& Exec = GetExecState();

	STATETREE_CLOG(!EventsToProcess.IsEmpty(), Verbose, TEXT("Trigger transitions with events [%s]"), *DebugGetEventsAsString());

	NextTransition.Reset();

	//
	// Process transition requests
	//
	for (const FStateTreeTransitionRequest& Request : InstanceData.GetTransitionRequests())
	{
		RequestTransition(Request.TargetState, Request.Priority);
	}
	InstanceData.ResetTransitionRequests();
	
	//
	// Check tick, event, and task based transitions first.
	//
	for (int32 StateIndex = Exec.ActiveStates.Num() - 1; StateIndex >= 0; StateIndex--)
	{
		const FStateTreeStateHandle StateHandle = Exec.ActiveStates[StateIndex];
		const FCompactStateTreeState& State = StateTree.States[StateHandle.Index];

		FCurrentlyProcessedStateScope StateScope(*this, StateHandle);

		if (State.bHasTransitionTasks)
		{
			STATETREE_CLOG(State.TasksNum > 0, VeryVerbose, TEXT("%*sTrigger task transitions in state '%s'"), StateIndex*UE::StateTree::DebugIndentSize, TEXT(""), *DebugGetStatePath(Exec.ActiveStates, StateIndex));

			for (int32 TaskIndex = (State.TasksBegin + State.TasksNum) - 1; TaskIndex >= State.TasksBegin; TaskIndex--)
			{
				const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
				if (Task.bShouldAffectTransitions)
				{
					STATETREE_LOG(VeryVerbose, TEXT("%*sTriggerTransitions: '%s'"), UE::StateTree::DebugIndentSize, TEXT(""), *Task.Name.ToString());
					check(DataViews[Task.DataViewIndex.Get()].IsValid());
					Task.TriggerTransitions(*this);
				}
			}
		}
		
		for (uint8 i = 0; i < State.TransitionsNum; i++)
		{
			// All transition conditions must pass
			const int16 TransitionIndex = State.TransitionsBegin + i;
			const FCompactStateTransition& Transition = StateTree.Transitions[TransitionIndex];

			// No need to test the transition if same or higher priority transition has already been processed.
			if (Transition.Priority <= NextTransition.Priority)
			{
				continue;
			}
			
			if (Transition.Trigger == EStateTreeTransitionTrigger::OnTick
				|| (Transition.Trigger == EStateTreeTransitionTrigger::OnEvent && HasEventToProcess(Transition.EventTag)))
			{
				// If delay has passed, allow to trigger the transition even if conditions do not pass.
				FStateTreeTransitionDelayedState* DelayedState = Transition.HasDelay() ? Exec.FindDelayedTransition(FStateTreeIndex16(TransitionIndex)) : nullptr;
				const bool bDelayCompleted = DelayedState != nullptr && DelayedState->TimeLeft <= 0.0f;

				if (bDelayCompleted || TestAllConditions(Transition.ConditionsBegin, Transition.ConditionsNum))
				{
					// If the transitions is delayed, set up the delay. 
					if (Transition.HasDelay())
					{
						if (DelayedState == nullptr)
						{
							DelayedState = &Exec.DelayedTransitions.AddDefaulted_GetRef();
							
							DelayedState->TransitionIndex = FStateTreeIndex16(TransitionIndex);
							DelayedState->TimeLeft = Transition.Delay.GetRandomDuration();
							BeginDelayedTransition(*DelayedState);
							STATETREE_LOG(Verbose, TEXT("Delayed transition triggered from '%s' (%s) -> '%s' %.1fs"),
								*GetSafeStateName(Exec.ActiveStates.Last()), *State.Name.ToString(), *GetSafeStateName(Transition.State), DelayedState->TimeLeft);
							// Fall through to handle 0 delay.
						}
						check(DelayedState);

						// We get here if the transitions re-triggers, or delay has completed.
						// In case of re-trigger, we will just ignore it during the delay. 
						if (DelayedState->TimeLeft > 0.0f)
						{
							continue;
						}

						STATETREE_LOG(Verbose, TEXT("Passed delayed transition from '%s' (%s) -> '%s'"),
							*GetSafeStateName(Exec.ActiveStates.Last()), *State.Name.ToString(), *GetSafeStateName(Transition.State));

						// The transition passed the delay, and remove it from the queue, and try trigger it.
						Exec.DelayedTransitions.RemoveAllSwap([TransitionIndex](const FStateTreeTransitionDelayedState& DelayedState)
							{
								return DelayedState.TransitionIndex.Get() == TransitionIndex;
							});
					}

					RequestTransition(Transition.State, Transition.Priority);
				}
			}
		}
	}

	if (StateTree.bHasGlobalTransitionTasks)
	{
		STATETREE_LOG(VeryVerbose, TEXT("Trigger global task transitions"));
		for (int32 TaskIndex = (StateTree.GlobalTasksBegin + StateTree.GlobalTasksNum) - 1; TaskIndex >= StateTree.GlobalTasksBegin; TaskIndex--)
		{
			const FStateTreeTaskBase& Task =  StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
			if (Task.bShouldAffectTransitions)
			{
				STATETREE_LOG(VeryVerbose, TEXT("%*sTriggerTransitions: '%s'"), UE::StateTree::DebugIndentSize, TEXT(""), *Task.Name.ToString());
				check(DataViews[Task.DataViewIndex.Get()].IsValid());
				Task.TriggerTransitions(*this);
			}
		}
	}

	//
	// Check state completion transitions.
	//
	if (NextTransition.Priority == EStateTreeTransitionPriority::None
		&& Exec.LastTickStatus != EStateTreeRunStatus::Running)
	{
		// Start from the last completed state.
		const int32 StateStartIndex = Exec.CompletedStateHandle.IsValid() ? Exec.ActiveStates.IndexOfReverse(Exec.CompletedStateHandle) : (Exec.ActiveStates.Num() - 1);
		const EStateTreeTransitionTrigger CompletionTrigger = Exec.LastTickStatus == EStateTreeRunStatus::Succeeded ? EStateTreeTransitionTrigger::OnStateSucceeded : EStateTreeTransitionTrigger::OnStateFailed;

		check(StateStartIndex >= 0 && StateStartIndex < Exec.ActiveStates.Num());
		
		// Check completion transitions
		for (int32 StateIndex = StateStartIndex; StateIndex >= 0; StateIndex--)
		{
			const FStateTreeStateHandle StateHandle = Exec.ActiveStates[StateIndex];
			const FCompactStateTreeState& State = StateTree.States[StateHandle.Index];

			FCurrentlyProcessedStateScope StateScope(*this, StateHandle);
			
			for (uint8 i = 0; i < State.TransitionsNum; i++)
			{
				// All transition conditions must pass
				const int16 TransitionIndex = State.TransitionsBegin + i;
				const FCompactStateTransition& Transition = StateTree.Transitions[TransitionIndex];

				if (EnumHasAnyFlags(Transition.Trigger, CompletionTrigger))
				{
					if (TestAllConditions(Transition.ConditionsBegin, Transition.ConditionsNum))
					{
						// No delay allowed on completion conditions.
						// No priority on completion transitions, use the priority to signal that state is selected.
						if (RequestTransition(Transition.State, EStateTreeTransitionPriority::Normal))
						{
							break;
						}
					}
				}
			}

			if (NextTransition.Priority != EStateTreeTransitionPriority::None)
			{
				break;
			}
		}

		if (NextTransition.Priority == EStateTreeTransitionPriority::None)
		{
			// Could not trigger completion transition, jump back to start.
			FCurrentlyProcessedStateScope StateScope(*this, FStateTreeStateHandle::Root);
			RequestTransition(FStateTreeStateHandle::Root, EStateTreeTransitionPriority::Normal);
		}
	}

	// Check if the transition was succeed/failed, if we're on a sub-tree, complete the subtree instead of transition.
	if (NextTransition.TargetState.IsCompletionState())
	{
		const FStateTreeStateHandle ParentLinkedState = GetParentLinkedStateHandle(Exec.ActiveStates, NextTransition.SourceState);
		if (ParentLinkedState.IsValid())
		{
			const EStateTreeRunStatus RunStatus = NextTransition.TargetState.ToCompletionStatus(); 
			STATETREE_LOG(Verbose, TEXT("Compled subtree '%s' from state '%s' (%s): %s"),
				*GetSafeStateName(ParentLinkedState), *GetSafeStateName(Exec.ActiveStates.Last()), *GetSafeStateName(NextTransition.SourceState), *UEnum::GetDisplayValueAsText(RunStatus).ToString());
			Exec.CompletedStateHandle = ParentLinkedState;
			Exec.LastTickStatus = RunStatus;

			// Not a transition
			return false;
		}
	}

	return NextTransition.TargetState.IsValid();
}

FStateTreeStateHandle FStateTreeExecutionContext::GetParentLinkedStateHandle(const FStateTreeActiveStates& ActiveStates, const int32 StartStartIndex) const
{
	check(ActiveStates.IsValidIndex(StartStartIndex));
	for (int32 StateIndex = StartStartIndex; StateIndex >= 0; StateIndex--)
	{
		const FCompactStateTreeState& State = StateTree.States[ActiveStates[StateIndex].Index];
		if (State.LinkedState.IsValid())
		{
			return ActiveStates[StateIndex];
		}
	}

	return FStateTreeStateHandle();
}	

FStateTreeStateHandle FStateTreeExecutionContext::GetParentLinkedStateHandle(const FStateTreeActiveStates& ActiveStates, const FStateTreeStateHandle StartStateHandle) const
{
	// Find start state
	int32 StateIndex = ActiveStates.Num() - 1;
	while (StateIndex >= 0)
	{
		if (ActiveStates[StateIndex] == StartStateHandle)
		{
			break;
		}
		StateIndex--;
	}

	// Find parent linked state.
	while (StateIndex >= 0)
	{
		const FCompactStateTreeState& State = StateTree.States[ActiveStates[StateIndex].Index];
		if (State.LinkedState.IsValid())
		{
			return ActiveStates[StateIndex];
		}
		
		StateIndex--;
	}

	return FStateTreeStateHandle();
}

bool FStateTreeExecutionContext::SelectState(const FStateTreeStateHandle NextState, FStateTreeActiveStates& OutNewActiveState)
{
	const FStateTreeExecutionState& Exec = GetExecState();

	if (!NextState.IsValid())
	{
		return false;
	}

	// Find common ancestor of `NextState` in the current active states and connect.
	// This allows transitions within a subtree.
	OutNewActiveState = Exec.ActiveStates;
	
	TStaticArray<FStateTreeStateHandle, FStateTreeActiveStates::MaxStates> InBetweenStates;
	int32 NumInBetweenStates = 0;
	int32 CommonActiveAncestorIndex = INDEX_NONE;

	// Walk towards the root from current state.
	FStateTreeStateHandle CurrState = NextState;
	while (CurrState.IsValid())
	{
		// Store the states that are in between the 'NextState' and common ancestor. 
		InBetweenStates[NumInBetweenStates++] = CurrState;
		// Check if the state can be found in the active states.
		CommonActiveAncestorIndex = OutNewActiveState.IndexOfReverse(CurrState); 
		if (CommonActiveAncestorIndex != INDEX_NONE)
		{
			break;
		}
		if (NumInBetweenStates == InBetweenStates.Num())
		{
			STATETREE_LOG(Error, TEXT("%s: Too many parent states when selecting state '%s' from '%s'.  '%s' using StateTree '%s'."),
				ANSI_TO_TCHAR(__FUNCTION__), *GetSafeStateName(NextState), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
			return false;
		}

		CurrState = StateTree.States[CurrState.Index].Parent;
	}

	// Max takes care of INDEX_NONE, by setting the num to 0.
	OutNewActiveState.SetNum(FMath::Max(0, CommonActiveAncestorIndex));
	
	// Append in between state in reverse order, they were collected from leaf towards the root.
	bool bActiveStatesOverflow = false;
	for (int32 Index = NumInBetweenStates - 1; Index > 0; Index--)
	{
		bActiveStatesOverflow |= !OutNewActiveState.Push(InBetweenStates[Index]);
	}

	if (bActiveStatesOverflow)
	{
		STATETREE_LOG(Error, TEXT("%s: Reached max execution depth when trying to select state %s from '%s'.  '%s' using StateTree '%s'."),
			ANSI_TO_TCHAR(__FUNCTION__), *GetSafeStateName(NextState), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return false;
	}

	return SelectStateInternal(NextState, OutNewActiveState);
}

bool FStateTreeExecutionContext::SelectStateInternal(const FStateTreeStateHandle NextState, FStateTreeActiveStates& OutNewActiveState)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTree_SelectState);

	const FStateTreeExecutionState& Exec = GetExecState();

	if (!NextState.IsValid())
	{
		// Trying to select non-existing state.
		STATETREE_LOG(Error, TEXT("%s: Trying to select invalid state from '%s'.  '%s' using StateTree '%s'."),
            ANSI_TO_TCHAR(__FUNCTION__), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
		return false;
	}

	const FCompactStateTreeState& State = StateTree.States[NextState.Index];

	// Check that the state can be entered
	if (TestAllConditions(State.EnterConditionsBegin, State.EnterConditionsNum))
	{
		if (!OutNewActiveState.Push(NextState))
		{
			STATETREE_LOG(Error, TEXT("%s: Reached max execution depth when trying to select state %s from '%s'.  '%s' using StateTree '%s'."),
				ANSI_TO_TCHAR(__FUNCTION__), *GetSafeStateName(NextState), *GetStateStatusString(Exec), *GetNameSafe(&Owner), *GetFullNameSafe(&StateTree));
			return false;
		}
		
		if (State.LinkedState.IsValid())
		{
			// If State is linked, proceed to the linked state.
			if (SelectStateInternal(State.LinkedState, OutNewActiveState))
			{
				// Selection succeeded
				return true;
			}
		}
		else if (State.HasChildren())
		{
			// If the state has children, proceed to select children.
			for (uint16 ChildState = State.ChildrenBegin; ChildState < State.ChildrenEnd; ChildState = StateTree.States[ChildState].GetNextSibling())
			{
				if (SelectStateInternal(FStateTreeStateHandle(ChildState), OutNewActiveState))
				{
					// Selection succeeded
					return true;
				}
			}
		}
		else
		{
			// Select this state.
			return true;
		}
		
		OutNewActiveState.Pop();
	}

	// Nothing got selected.
	return false;
}

FString FStateTreeExecutionContext::GetSafeStateName(const FStateTreeStateHandle State) const
{
	if (State == FStateTreeStateHandle::Invalid)
	{
		return TEXT("(State Invalid)");
	}
	else if (State == FStateTreeStateHandle::Succeeded)
	{
		return TEXT("(State Succeeded)");
	}
	else if (State == FStateTreeStateHandle::Failed)
	{
		return TEXT("(State Failed)");
	}
	else if (StateTree.States.IsValidIndex(State.Index))
	{
		return *StateTree.States[State.Index].Name.ToString();
	}
	return TEXT("(Unknown)");
}

FString FStateTreeExecutionContext::DebugGetStatePath(const FStateTreeActiveStates& ActiveStates, const int32 ActiveStateIndex) const
{
	FString StatePath;
	const int32 Num = ActiveStateIndex == INDEX_NONE ? ActiveStates.Num() : (ActiveStateIndex + 1);  
	if (!ensureMsgf(ActiveStates.IsValidIndex(Num - 1), TEXT("Provided index must be valid")))
	{
		return StatePath;
	}

	for (int32 i = 0; i < Num; i++)
	{
		const FCompactStateTreeState& State = StateTree.States[ActiveStates[i].Index];
		StatePath.Appendf(TEXT("%s%s"), i == 0 ? TEXT("") : TEXT("."), *State.Name.ToString());
	}
	return StatePath;
}

FString FStateTreeExecutionContext::GetStateStatusString(const FStateTreeExecutionState& ExecState) const
{
	return GetSafeStateName(ExecState.ActiveStates.Last()) + TEXT(":") + UEnum::GetDisplayValueAsText(ExecState.LastTickStatus).ToString();
}

EStateTreeRunStatus FStateTreeExecutionContext::GetLastTickStatus() const
{
	const FStateTreeExecutionState& Exec = GetExecState();
	return Exec.LastTickStatus;
}

FString FStateTreeExecutionContext::GetInstanceDescription() const
{
	return FString::Printf(TEXT("%s: "), *GetNameSafe(&Owner));
}

const FStateTreeActiveStates& FStateTreeExecutionContext::GetActiveStates() const
{
	const FStateTreeExecutionState& Exec = GetExecState();
	return Exec.ActiveStates;
}


#if WITH_GAMEPLAY_DEBUGGER

FString FStateTreeExecutionContext::GetDebugInfoString() const
{
	const FStateTreeExecutionState& Exec = GetExecState();

	FString DebugString = FString::Printf(TEXT("StateTree (asset: '%s')\n"), *GetFullNameSafe(&StateTree));

	DebugString += TEXT("Status: ");
	switch (Exec.TreeRunStatus)
	{
	case EStateTreeRunStatus::Failed:
		DebugString += TEXT("Failed\n");
		break;
	case EStateTreeRunStatus::Succeeded:
		DebugString += TEXT("Succeeded\n");
		break;
	case EStateTreeRunStatus::Running:
		DebugString += TEXT("Running\n");
		break;
	default:
		DebugString += TEXT("--\n");
	}

	if (StateTree.EvaluatorsNum > 0)
	{
		DebugString += TEXT("\nEvaluators:\n");
		for (int32 EvalIndex = StateTree.EvaluatorsBegin; EvalIndex < (StateTree.EvaluatorsBegin + StateTree.EvaluatorsNum); EvalIndex++)
		{
			const FStateTreeEvaluatorBase& Eval = StateTree.Nodes[EvalIndex].Get<FStateTreeEvaluatorBase>();
			Eval.AppendDebugInfoString(DebugString, *this);
		}
	}

	// Active States
	DebugString += TEXT("Current State:\n");
	for (int32 Index = 0; Index < Exec.ActiveStates.Num(); Index++)
	{
		FStateTreeStateHandle Handle = Exec.ActiveStates[Index];
		if (Handle.IsValid())
		{
			const FCompactStateTreeState& State = StateTree.States[Handle.Index];
			DebugString += FString::Printf(TEXT("[%s]\n"), *State.Name.ToString());

			if (State.TasksNum > 0)
			{
				DebugString += TEXT("\nTasks:\n");
				for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
				{
					const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
					Task.AppendDebugInfoString(DebugString, *this);
				}
			}
		}
	}

	return DebugString;
}
#endif // WITH_GAMEPLAY_DEBUGGER

#if WITH_STATETREE_DEBUG
void FStateTreeExecutionContext::DebugPrintInternalLayout()
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogStateTree, ELogVerbosity::Log);

	FString DebugString = FString::Printf(TEXT("StateTree (asset: '%s')\n"), *GetFullNameSafe(&StateTree));

	// Tree items (e.g. tasks, evaluators, conditions)
	DebugString += FString::Printf(TEXT("\nItems(%d)\n"), StateTree.Nodes.Num());
	for (int32 Index = 0; Index < StateTree.Nodes.Num(); Index++)
	{
		const FConstStructView Node = StateTree.Nodes[Index];
		DebugString += FString::Printf(TEXT("  %s\n"), Node.IsValid() ? *Node.GetScriptStruct()->GetName() : TEXT("null"));
	}

	// Instance InstanceData data (e.g. tasks)
	DebugString += FString::Printf(TEXT("\nInstance Structs(%d)\n"), StateTree.DefaultInstanceData.NumStructs());
	for (int32 Index = 0; Index < StateTree.DefaultInstanceData.NumStructs(); Index++)
	{
		const FConstStructView Data = StateTree.DefaultInstanceData.GetStruct(Index);
		DebugString += FString::Printf(TEXT("  %s\n"), Data.IsValid() ? *Data.GetScriptStruct()->GetName() : TEXT("null"));
	}
	DebugString += FString::Printf(TEXT("\nInstance Objects(%d)\n"), StateTree.DefaultInstanceData.NumObjects());
	for (int32 Index = 0; Index < StateTree.DefaultInstanceData.NumObjects(); Index++)
	{
		const UObject* Data = StateTree.DefaultInstanceData.GetObject(Index);
		DebugString += FString::Printf(TEXT("  %s\n"), *GetNameSafe(Data));
	}

	// External data (e.g. fragments, subsystems)
	DebugString += FString::Printf(TEXT("\nExternal Data(%d)\n  [ %-40s | %-8s | %5s ]\n"), StateTree.ExternalDataDescs.Num(), TEXT("Name"), TEXT("Optional"), TEXT("Index"));
	for (const FStateTreeExternalDataDesc& Desc : StateTree.ExternalDataDescs)
	{
		DebugString += FString::Printf(TEXT("  | %-40s | %8s | %5d |\n"), Desc.Struct ? *Desc.Struct->GetName() : TEXT("null"), *UEnum::GetDisplayValueAsText(Desc.Requirement).ToString(), Desc.Handle.DataViewIndex.Get());
	}

	// Bindings
	StateTree.PropertyBindings.DebugPrintInternalLayout(DebugString);

	// Transitions
	DebugString += FString::Printf(TEXT("\nTransitions(%d)\n  [ %-3s | %15s | %-20s | %-40s | %-8s ]\n"), StateTree.Transitions.Num()
		, TEXT("Idx"), TEXT("State"), TEXT("Transition Trigger"), TEXT("Transition Event Tag"), TEXT("Num Cond"));
	for (const FCompactStateTransition& Transition : StateTree.Transitions)
	{
		DebugString += FString::Printf(TEXT("  | %3d | %15s | %-20s | %-40s | %8d |\n"),
									Transition.ConditionsBegin, *Transition.State.Describe(),
									*UEnum::GetDisplayValueAsText(Transition.Trigger).ToString(),
									*Transition.EventTag.ToString(),
									Transition.ConditionsNum);
	}

	// DataViews
	DebugString += FString::Printf(TEXT("\nDataViews(%d)\n"), DataViews.Num());
	for (const FStateTreeDataView& DataView : DataViews)
	{
		DebugString += FString::Printf(TEXT("  [%s]\n"), DataView.IsValid() ? *DataView.GetStruct()->GetName() : TEXT("null"));
	}

	// States
	DebugString += FString::Printf(TEXT("\nStates(%d)\n"
		"  [ %-30s | %15s | %5s [%3s:%-3s[ | Begin Idx : %4s %4s %4s %4s | Num : %4s %4s %4s %4s | Transitions : %-16s %-40s %-16s %-40s ]\n"),
		StateTree.States.Num(),
		TEXT("Name"), TEXT("Parent"), TEXT("Child"), TEXT("Beg"), TEXT("End"),
		TEXT("Cond"), TEXT("Tr"), TEXT("Tsk"), TEXT("Evt"), TEXT("Cond"), TEXT("Tr"), TEXT("Tsk"), TEXT("Evt"),
		TEXT("Done State"), TEXT("Done Type"), TEXT("Failed State"), TEXT("Failed Type")
		);
	for (const FCompactStateTreeState& State : StateTree.States)
	{
		DebugString += FString::Printf(TEXT("  | %-30s | %15s | %5s [%3d:%-3d[ | %9s   %4d %4d %4d | %3s   %4d %4d %4d\n"),
									*State.Name.ToString(), *State.Parent.Describe(),
									TEXT(""), State.ChildrenBegin, State.ChildrenEnd,
									TEXT(""), State.EnterConditionsBegin, State.TransitionsBegin, State.TasksBegin,
									TEXT(""), State.EnterConditionsNum, State.TransitionsNum, State.TasksNum);
	}

	// Evaluators
	if (StateTree.EvaluatorsNum)
	{
		DebugString += FString::Printf(TEXT("\nEvaluators\n  [ %-30s | %8s | %10s ]\n"),
			TEXT("Name"), TEXT("Bindings"), TEXT("Struct Idx"));
		for (int32 EvalIndex = StateTree.EvaluatorsBegin; EvalIndex < (StateTree.EvaluatorsBegin + StateTree.EvaluatorsNum); EvalIndex++)
		{
			const FStateTreeEvaluatorBase& Eval = StateTree.Nodes[EvalIndex].Get<FStateTreeEvaluatorBase>();
			DebugString += FString::Printf(TEXT("| %-30s | %8d | %10d |\n"),
				*Eval.Name.ToString(), Eval.BindingsBatch.Get(), Eval.DataViewIndex.Get());
		}
	}


	DebugString += FString::Printf(TEXT("\nTasks\n  [ %-30s | %-30s | %8s | %10s ]\n"),
		TEXT("State"), TEXT("Name"), TEXT("Bindings"), TEXT("Struct Idx"));
	for (const FCompactStateTreeState& State : StateTree.States)
	{
		// Tasks
		if (State.TasksNum)
		{
			for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
			{
				const FStateTreeTaskBase& Task = StateTree.Nodes[TaskIndex].Get<FStateTreeTaskBase>();
				DebugString += FString::Printf(TEXT("  | %-30s | %-30s | %8d | %10d |\n"), *State.Name.ToString(),
					*Task.Name.ToString(), Task.BindingsBatch.Get(), Task.DataViewIndex.Get());
			}
		}
	}

	UE_LOG(LogStateTree, Log, TEXT("%s"), *DebugString);
}

int32 FStateTreeExecutionContext::GetStateChangeCount() const
{
	if (!InstanceData.IsValid())
	{
		return 0;
	}
	const FStateTreeExecutionState& Exec = GetExecState();
	return Exec.StateChangeCount;
}

#endif // WITH_STATETREE_DEBUG

FString FStateTreeExecutionContext::GetActiveStateName() const
{
	if (!InstanceData.IsValid())
	{
		return FString(TEXT("<None>"));
	}
	
	const FStateTreeExecutionState& Exec = GetExecState();

	FString FullStateName;
	
	// Active States
	for (int32 Index = 0; Index < Exec.ActiveStates.Num(); Index++)
	{
		const FStateTreeStateHandle Handle = Exec.ActiveStates[Index];
		if (Handle.IsValid())
		{
			const FCompactStateTreeState& State = StateTree.States[Handle.Index];
			bool bIsLinked = false;
			if (Index > 0)
			{
				FullStateName += TEXT("\n");
				bIsLinked = Exec.ActiveStates[Index - 1] != State.Parent;
			}
			FullStateName += FString::Printf(TEXT("%*s-"), Index * 3, TEXT("")); // Indent
			FullStateName += *State.Name.ToString();
			if (bIsLinked)
			{
				FullStateName += TEXT(" >");
			}
		}
	}

	switch (Exec.TreeRunStatus)
	{
	case EStateTreeRunStatus::Failed:
		FullStateName += TEXT(" FAILED\n");
		break;
	case EStateTreeRunStatus::Succeeded:
		FullStateName += TEXT(" SUCCEEDED\n");
		break;
	case EStateTreeRunStatus::Running:
		// Empty
		break;
	default:
		FullStateName += TEXT("--\n");
	}

	return FullStateName;
}

TArray<FName> FStateTreeExecutionContext::GetActiveStateNames() const
{
	TArray<FName> Result;

	if (!InstanceData.IsValid())
	{
		return Result;
	}
	
	const FStateTreeExecutionState& Exec = GetExecState();

	// Active States
	for (int32 Index = 0; Index < Exec.ActiveStates.Num(); Index++)
	{
		const FStateTreeStateHandle Handle = Exec.ActiveStates[Index];
		if (Handle.IsValid())
		{
			const FCompactStateTreeState& State = StateTree.States[Handle.Index];
			Result.Add(State.Name);
		}
	}

	return Result;
}

#undef STATETREE_LOG
#undef STATETREE_CLOG
