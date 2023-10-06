// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeNodeBase.h"
#include "Experimental/ConcurrentLinearAllocator.h"

struct FGameplayTag;
struct FInstancedPropertyBag;

struct FStateTreeEvaluatorBase;
struct FStateTreeTaskBase;
struct FStateTreeConditionBase;
struct FStateTreeEvent;
struct FStateTreeTransitionRequest;
struct FStateTreeInstanceDebugId;

/**
 * StateTree Execution Context is a helper that is used to update and access StateTree instance data.
 *
 * The context is meant to be temporary, you should not store a context across multiple frames.
 *
 * The owner is used as the owner of the instantiated UObjects in the instance data and logging, it should have same or greater lifetime as the InstanceData. 
 *
 * In common case you can use the constructor or Init() to initialize the context:
 *
 *		FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
 *		if (SetContextRequirements(Context))
 *		{
 *			Context.Tick(DeltaTime);
 * 		}
 * 
 *		bool UMyComponent::SetContextRequirements(FStateTreeExecutionContext& Context)
 *		{
 *			if (!Context.IsValid())
 *			{
 *				return false;
 *			}
 *			// Setup context data
 *			return true;
 *		}
 */
struct STATETREEMODULE_API FStateTreeExecutionContext
{
public:
	FStateTreeExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData);
	virtual ~FStateTreeExecutionContext();

	/** Updates data view of the parameters by using the default values defined in the StateTree asset. */
	void SetDefaultParameters();

	/**
	 * Updates data view of the parameters by replacing the default values defined in the StateTree asset by the provided values.
	 * Note: caller is responsible to make sure external parameters lifetime matches the context.
	 */
	void SetParameters(const FInstancedPropertyBag& Parameters);
	
	/** @return the StateTree asset in use. */
	const UStateTree* GetStateTree() const { return &StateTree; }

	/** @return const references to the instance data in use, or nullptr if the context is not valid. */
	const FStateTreeInstanceData* GetInstanceData() const { return &InstanceData; }

	/** @retuen mutable references to the instance data in use, or nullptr if the context is not valid. */
	FStateTreeInstanceData* GetMutableInstanceData() const { return &InstanceData; }

	/** @return mutable references to the instance data in use. */
	const FStateTreeEventQueue& GetEventQueue() const { return InstanceData.GetEventQueue(); }

	/** @return mutable references to the instance data in use. */
	FStateTreeEventQueue& GetMutableEventQueue() const { return InstanceData.GetMutableEventQueue(); }

	/** @return The owner of the context */
	UObject* GetOwner() const { return &Owner; }
	/** @return The world of the owner or nullptr if the owner is not set. */ 
	UWorld* GetWorld() const { return Owner.GetWorld(); };

	/** @return True of the the execution context is valid and initialized. */ 
	bool IsValid() const { return StateTree.IsReadyToRun(); }
	
	/** Start executing. */
	EStateTreeRunStatus Start();
	
	/**
	 * Stop executing if the tree is running.
	 * @param CompletionStatus Status (and terminal state) reported in the transition when the tree is stopped.
	 * @return Tree execution status at stop, can be CompletionStatus, or earlier status if the tree is not running. 
	 */
	EStateTreeRunStatus Stop(const EStateTreeRunStatus CompletionStatus = EStateTreeRunStatus::Stopped);

	/**
	 * Tick the state tree logic.
	 * @param DeltaTime time to advance the logic.
	 * @returns tree run status after the tick.
	 */
	EStateTreeRunStatus Tick(const float DeltaTime);

	/** @return the tree run status. */
	EStateTreeRunStatus GetStateTreeRunStatus() const;

	/** @return the status of the last tick function */
	EStateTreeRunStatus GetLastTickStatus() const;

	/** @return reference to the list of currently active states. */
	const FStateTreeActiveStates& GetActiveStates() const;

#if WITH_GAMEPLAY_DEBUGGER
	/** @return Debug string describing the current state of the execution */
	FString GetDebugInfoString() const;
#endif // WITH_GAMEPLAY_DEBUGGER

#if WITH_STATETREE_DEBUG
	int32 GetStateChangeCount() const;

	void DebugPrintInternalLayout();
#endif

	/** @return the name of the active state. */
	FString GetActiveStateName() const;
	
	/** @return the names of all the active state. */
	TArray<FName> GetActiveStateNames() const;

	/** Sends event for the StateTree. */
	UE_DEPRECATED(5.2, "Use AddEvent() with individual parameters instead.")
	void SendEvent(const FStateTreeEvent& Event) const;

	/** Sends event for the StateTree. */
	void SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName()) const;

	/** Iterates over all events. Can only be used during StateTree tick. Expects a lambda which takes const FStateTreeEvent& Event, and returns EStateTreeLoopEvents. */
	template<typename TFunc>
	void ForEachEvent(TFunc&& Function) const
	{
		for (const FStateTreeEvent& Event : EventsToProcess)
		{
			if (Function(Event) == EStateTreeLoopEvents::Break)
			{
				break;
			}
		}
	}

	/** @return events to process this tick. */
	TConstArrayView<FStateTreeEvent> GetEventsToProcess() const { return EventsToProcess; }

	/** @return true if there is a pending event with specified tag. */
	bool HasEventToProcess(const FGameplayTag Tag) const
	{
		if (EventsToProcess.IsEmpty())
		{
			return false;
		}
		
		return EventsToProcess.ContainsByPredicate([Tag](const FStateTreeEvent& Event)
		{
			return Event.Tag.MatchesTag(Tag);
		});
	}

	/** @return the currently processed state if applicable. */
	FStateTreeStateHandle GetCurrentlyProcessedState() const { return CurrentlyProcessedState; }
	
	/** @return Pointer to a State or null if state not found */ 
	const FCompactStateTreeState* GetStateFromHandle(const FStateTreeStateHandle StateHandle) const
	{
		return StateTree.GetStateFromHandle(StateHandle);
	}

	/** @return Array view to external data descriptors associated with this context. Note: Init() must be called before calling this method. */
	TConstArrayView<FStateTreeExternalDataDesc> GetExternalDataDescs() const
	{
		return StateTree.ExternalDataDescs;
	}

	/** @return Array view to named external data descriptors associated with this context. Note: Init() must be called before calling this method. */
	TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const
	{
		return StateTree.GetContextDataDescs();
	}

	/** @return True if all required external data pointers are set. */ 
	bool AreExternalDataViewsValid() const;

	/** @return Handle to external data of type InStruct, or invalid handle if struct not found. */ 
	FStateTreeExternalDataHandle GetExternalDataHandleByStruct(const UStruct* InStruct) const
	{
		const FStateTreeExternalDataDesc* DataDesc = StateTree.ExternalDataDescs.FindByPredicate([InStruct](const FStateTreeExternalDataDesc& Item) { return Item.Struct == InStruct; });
		return DataDesc != nullptr ? DataDesc->Handle : FStateTreeExternalDataHandle::Invalid;
	}

	/** Sets external data view value for specific item. */ 
	void SetExternalData(const FStateTreeExternalDataHandle Handle, FStateTreeDataView DataView)
	{
		check(Handle.IsValid());
		DataViews[Handle.DataViewIndex.Get()] = DataView;
	}

	/**
	 * Returns reference to external data based on provided handle. The return type is deduced from the handle's template type.
     * @param Handle Valid TStateTreeExternalDataHandle<> handle. 
	 * @return reference to external data based on handle or null if data is not set.
	 */ 
	template <typename T>
	typename T::DataType& GetExternalData(const T Handle) const
	{
		check(Handle.IsValid());
		checkSlow(StateTree.ExternalDataDescs[Handle.DataViewIndex.Get() - StateTree.ExternalDataBaseIndex].Requirement != EStateTreeExternalDataRequirement::Optional); // Optionals should query pointer instead.
		return DataViews[Handle.DataViewIndex.Get()].template GetMutable<typename T::DataType>();
	}

	/**
	 * Returns pointer to external data based on provided item handle. The return type is deduced from the handle's template type.
     * @param Handle Valid TStateTreeExternalDataHandle<> handle.
	 * @return pointer to external data based on handle or null if item is not set or handle is invalid.
	 */ 
	template <typename T>
	typename T::DataType* GetExternalDataPtr(const T Handle) const
	{
		return Handle.IsValid() ? DataViews[Handle.DataViewIndex.Get()].template GetMutablePtr<typename T::DataType>() : nullptr;
	}

	FStateTreeDataView GetExternalDataView(const FStateTreeExternalDataHandle Handle)
	{
		if (Handle.IsValid())
		{
			return DataViews[Handle.DataViewIndex.Get()];
		}
		return FStateTreeDataView();
	}

	/** @returns pointer to the instance data of specified node. */
	template <typename T>
	T* GetInstanceDataPtr(const FStateTreeNodeBase& Node) const
	{
		return DataViews[Node.DataViewIndex.Get()].template GetMutablePtr<T>();
	}

	/** @returns reference to the instance data of specified node. */
	template <typename T>
	T& GetInstanceData(const FStateTreeNodeBase& Node) const
	{
		return DataViews[Node.DataViewIndex.Get()].template GetMutable<T>();
	}

	/** @returns reference to the instance data of specified node. Infers the instance data type from the node's FInstanceDataType. */
	template <typename T>
	typename T::FInstanceDataType& GetInstanceData(const T& Node) const
	{
		static_assert(TIsDerivedFrom<T, FStateTreeNodeBase>::IsDerived, "Expecting Node to derive from FStateTreeNodeBase.");
		return DataViews[Node.DataViewIndex.Get()].template GetMutable<typename T::FInstanceDataType>();
	}

	/** @returns reference to instance data struct that can be passed to lambdas. See TStateTreeInstanceDataStructRef for usage. */
	template <typename T>
	TStateTreeInstanceDataStructRef<typename T::FInstanceDataType> GetInstanceDataStructRef(const T& Node) const
	{
		static_assert(TIsDerivedFrom<T, FStateTreeNodeBase>::IsDerived, "Expecting Node to derive from FStateTreeNodeBase.");
		return TStateTreeInstanceDataStructRef<typename T::FInstanceDataType>(InstanceData, DataViews[Node.DataViewIndex.Get()].template GetMutable<typename T::FInstanceDataType>());
	}

	/**
	 * Requests transition to a state.
	 * If called during during transition processing (e.g. from FStateTreeTaskBase::TriggerTransitions()) the transition
	 * is attempted to be activate immediately (it can fail e.g. because of preconditions on a target state).
	 * If called outside the transition handling, the request is buffered and handled at the beginning of next transition processing.
	 * @param Request The state to transition to.
	 */
	void RequestTransition(const FStateTreeTransitionRequest& Request);

protected:

#if WITH_STATETREE_DEBUGGER
	FStateTreeInstanceDebugId GetInstanceDebugId() const;
#endif // WITH_STATETREE_DEBUGGER

	/** @return Prefix that will be used by STATETREE_LOG and STATETREE_CLOG, Owner name by default. */
	virtual FString GetInstanceDescription() const;

	UE_DEPRECATED(5.2, "Use BeginDelayedTransition() instead.")
	virtual void BeginGatedTransition(const FStateTreeExecutionState& Exec) final {};
	
	/** Callback when delayed transition is triggered. Contexts that are event based can use this to trigger a future event. */
	virtual void BeginDelayedTransition(const FStateTreeTransitionDelayedState& DelayedState) {};

	void UpdateInstanceData(const FStateTreeActiveStates& CurrentActiveStates, const FStateTreeActiveStates& NextActiveStates);

	/**
	 * Handles logic for entering State. EnterState is called on new active Evaluators and Tasks that are part of the re-planned tree.
	 * Re-planned tree is from the transition target up to the leaf state. States that are parent to the transition target state
	 * and still active after the transition will remain intact.
	 * @return Run status returned by the tasks.
	 */
	EStateTreeRunStatus EnterState(const FStateTreeTransitionResult& Transition);

	/**
	 * Handles logic for exiting State. ExitState is called on current active Evaluators and Tasks that are part of the re-planned tree.
	 * Re-planned tree is from the transition target up to the leaf state. States that are parent to the transition target state
	 * and still active after the transition will remain intact.
	 */
	void ExitState(const FStateTreeTransitionResult& Transition);

	/**
	 * Handles logic for signalling State completed. StateCompleted is called on current active Evaluators and Tasks in reverse order (from leaf to root).
	 */
	void StateCompleted();

	/**
	 * Tick evaluators and global tasks by delta time.
	 */
	EStateTreeRunStatus TickEvaluatorsAndGlobalTasks(const float DeltaTime, bool bTickGlobalTasks = true);

	/**
	 * Starts evaluators and global tasks.
	 * @return run status returned by the global tasks.
	 */
	EStateTreeRunStatus StartEvaluatorsAndGlobalTasks(FStateTreeIndex16& OutLastInitializedTaskIndex);

	/**
	 * Stops evaluators and global tasks.
	 */
	void StopEvaluatorsAndGlobalTasks(const EStateTreeRunStatus CompletionStatus, const FStateTreeIndex16 LastInitializedTaskIndex = FStateTreeIndex16());

	/**
	 * Ticks tasks of all active states starting from current state by delta time.
	 * @return Run status returned by the tasks.
	 */
	EStateTreeRunStatus TickTasks(const float DeltaTime);

	/**
	 * Checks all conditions at given range
	 * @return True if all conditions pass.
	 */
	bool TestAllConditions(const int32 ConditionsOffset, const int32 ConditionsNum);

	/**
	 * Requests transition to a specified state with specified priority.
	 */
	bool RequestTransition(const FStateTreeStateHandle NextState, const EStateTreeTransitionPriority Priority);

	/**
	 * Triggers transitions based on current run status. CurrentStatus is used to select which transitions events are triggered.
	 * If CurrentStatus is "Running", "Conditional" transitions pass, "Completed/Failed" will trigger "OnCompleted/OnSucceeded/OnFailed" transitions.
	 * Transition target state can point to a selector state. For that reason the result contains both the target state, as well ass
	 * the actual next state returned by the selector.
	 * @return Transition result describing the source state, state transitioned to, and next selected state.
	 */
	bool TriggerTransitions();

	/**
	 * Traverses the ActiveStates from StartStateIndex to 0 and returns first linked state.
	 * @return Parent linked state, or invalid state if no linked state found. 
	 */
	FStateTreeStateHandle GetParentLinkedStateHandle(const FStateTreeActiveStates& ActiveStates, const int32 StartStateIndex) const;

	FStateTreeStateHandle GetParentLinkedStateHandle(const FStateTreeActiveStates& ActiveStates, const FStateTreeStateHandle StartStateHandle) const;

	/**
	 * Runs state selection logic starting at the specified state, walking towards the leaf states.
	 * If a state cannot be selected, false is returned. 
	 * If NextState is a selector state, SelectStateInternal is called recursively (depth-first) to all child states (where NextState will be one of child states).
	 * If NextState is a leaf state, the active states leading from root to the leaf are returned.
	 * @param NextState The state which we try to select next.
	 * @param OutNewActiveStates Active states that got selected.
	 * @param VisitedStates States visited so far during selection (used for detecting selection loops)
	 * @return True if succeeded to select new active states.
	 */
	bool SelectState(const FStateTreeStateHandle NextState, FStateTreeActiveStates& OutNewActiveStates, FStateTreeActiveStates& VisitedStates);

	/**
	 * Used internally to do the recursive part of the SelectState().
	 */
	bool SelectStateInternal(const FStateTreeStateHandle NextState, FStateTreeActiveStates& OutNewActiveStates, FStateTreeActiveStates& VisitedStates);

	/** @return StateTree execution state from the instance storage. */
	FStateTreeExecutionState& GetExecState()
	{
		return InstanceData.GetMutableStruct(0).Get<FStateTreeExecutionState>();
	}

	/** @return const StateTree execution state from the instance storage. */
	const FStateTreeExecutionState& GetExecState() const
	{
		return InstanceData.GetStruct(0).Get<const FStateTreeExecutionState>();
	}

	/** Sets up parameter data view for a linked state and copies bound properties. */
	void UpdateLinkedStateParameters(const FCompactStateTreeState& State, const int32 ParameterInstanceIndex);

	/** Sets up parameter data view for subtree state. */
	void UpdateSubtreeStateParameters(const FCompactStateTreeState& State);
	
	/** @return String describing state status for logging and debug. */
	FString GetStateStatusString(const FStateTreeExecutionState& ExecState) const;

	/** @return String describing state name for logging and debug. */
	FString GetSafeStateName(const FStateTreeStateHandle State) const;

	/** @return String describing full path of an activate state for logging and debug. */
	FString DebugGetStatePath(const FStateTreeActiveStates& ActiveStates, const int32 ActiveStateIndex = INDEX_NONE) const;

	/** @return String describing all events that are currently being processed  for logging and debug. */
	FString DebugGetEventsAsString() const;
	
	/** Helper function to update struct or object dataview of a node. */
	template<typename T>
	void SetNodeDataView(T& Node, int32& InstanceStructIndex, int32& InstanceObjectIndex)
	{
		if (Node.bInstanceIsObject)
		{
			DataViews[Node.DataViewIndex.Get()] = InstanceData.GetMutableObject(InstanceObjectIndex);
			InstanceObjectIndex++;
		}
		else
		{
			DataViews[Node.DataViewIndex.Get()] = InstanceData.GetMutableStruct(InstanceStructIndex);
			InstanceStructIndex++;
		}
	}
	

	/** Owner of the instance data. */
	UObject& Owner;

	/** The StateTree asset the context is initialized for */
	const UStateTree& StateTree;

	/** Instance data used during current tick. */
	FStateTreeInstanceData& InstanceData;

	/** Array of data pointers (external data, tasks, evaluators, conditions), used during evaluation. Initialized to match the number of items in the asset. */
	TArray<FStateTreeDataView, TConcurrentLinearArrayAllocator<FDefaultBlockAllocationTag>> DataViews;

	/** Events to process in current tick. */
	TArray<FStateTreeEvent, TConcurrentLinearArrayAllocator<FDefaultBlockAllocationTag>> EventsToProcess;

	/** Shared instance data for the duration of the context. */
	TSharedPtr<FStateTreeInstanceData> SharedInstanceData;

	/** Next transition, used by RequestTransition(). */
	FStateTreeTransitionResult NextTransition;

	/** Structure describing the origin of the state transition that caused the state change. */
	FStateTreeTransitionSource NextTransitionSource;

	/** Current state we're processing, or invalid if not applicable. */
	FStateTreeStateHandle CurrentlyProcessedState;

	/** True if transitions are allowed to be requested directly instead of buffering. */
	bool bAllowDirectTransitions = false;

	/** Helper struct to track when it is allowed to request transitions. */
	struct FAllowDirectTransitionsScope
	{
		FAllowDirectTransitionsScope(FStateTreeExecutionContext& InContext)
			: Context(InContext)
		{
			Context.bAllowDirectTransitions = true;
		}

		~FAllowDirectTransitionsScope()
		{
			Context.bAllowDirectTransitions = false;
		}
		
		FStateTreeExecutionContext& Context;
	};
	
	/** Helper struct to track currently processed state. */
	struct FCurrentlyProcessedStateScope
	{
		FCurrentlyProcessedStateScope(FStateTreeExecutionContext& InContext, const FStateTreeStateHandle State)
			: Context(InContext)
		{
			Context.CurrentlyProcessedState = State;
		}

		~FCurrentlyProcessedStateScope()
		{
			Context.CurrentlyProcessedState = FStateTreeStateHandle::Invalid;
		}
		
		FStateTreeExecutionContext& Context;
	};
};
