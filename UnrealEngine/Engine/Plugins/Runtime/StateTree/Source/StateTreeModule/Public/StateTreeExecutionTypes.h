// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "StateTreeTypes.h"

#include "StateTreeExecutionTypes.generated.h"

class UStateTree;

/**
 * Enumeration for the different update phases.
 * This is used as context information when tracing debug events.
 */
UENUM()
enum class EStateTreeUpdatePhase : uint8
{
	Unset					= 0,
	StartTree				UMETA(DisplayName = "Start Tree"),
	StopTree				UMETA(DisplayName = "Stop Tree"),
	StartGlobalTasks		UMETA(DisplayName = "Start Global Tasks & Evaluators"),
	StopGlobalTasks			UMETA(DisplayName = "Stop Global Tasks & Evaluators"),
	TickStateTree			UMETA(DisplayName = "Tick State Tree"),
	ApplyTransitions		UMETA(DisplayName = "Transition"),
	TriggerTransitions		UMETA(DisplayName = "Trigger Transitions"),
	TickingGlobalTasks		UMETA(DisplayName = "Tick Global Tasks & Evaluators"),
	TickingTasks			UMETA(DisplayName = "Tick Tasks"),
	TransitionConditions	UMETA(DisplayName = "Transition conditions"),
	StateSelection			UMETA(DisplayName = "Try Enter"),
	TrySelectBehavior		UMETA(DisplayName = "Try Select Behavior"),
	EnterConditions			UMETA(DisplayName = "Enter conditions"),
	EnterStates				UMETA(DisplayName = "Enter States"),
	ExitStates				UMETA(DisplayName = "Exit States"),
	StateCompleted			UMETA(DisplayName = "State(s) Completed")
};


/** Status describing current run status of a State Tree. */
UENUM(BlueprintType)
enum class EStateTreeRunStatus : uint8
{
	/** Tree is still running. */
	Running,
	
	/** Tree execution has stopped on failure. */
	Failed,
	
	/** Tree execution has stopped on success. */
	Succeeded,

	/** The State Tree was requested to stop without a particular success or failure state. */
	Stopped,

	/** Status not set. */
	Unset,
};


/** State change type. Passed to EnterState() and ExitState() to indicate how the state change affects the state and Evaluator or Task is on. */
UENUM()
enum class EStateTreeStateChangeType : uint8
{
	/** Not an activation */
	None,
	
	/** The state became activated or deactivated. */
	Changed,
	
	/** The state is parent of new active state and sustained previous active state. */
	Sustained,
};


/** Defines how to assign the result of a condition to evaluate.  */
UENUM()
enum class EStateTreeConditionEvaluationMode : uint8
{
	/** Condition is evaluated to set the result. This is the normal behavior. */
	Evaluated,
	
	/** Do not evaluate the condition and force result to 'true'. */
	ForcedTrue,
	
	/** Do not evaluate the condition and force result to 'false'. */
	ForcedFalse,
};


/**
 * Handle to access an external struct or object.
 * Note: Use the templated version below. 
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeExternalDataHandle
{
	GENERATED_BODY()

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FStateTreeExternalDataHandle() = default;
	FStateTreeExternalDataHandle(const FStateTreeExternalDataHandle& Other) = default;
	FStateTreeExternalDataHandle(FStateTreeExternalDataHandle&& Other) = default;
	FStateTreeExternalDataHandle& operator=(FStateTreeExternalDataHandle const& Other) = default;
	FStateTreeExternalDataHandle& operator=(FStateTreeExternalDataHandle&& Other) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	static const FStateTreeExternalDataHandle Invalid;
	
	bool IsValid() const { return DataHandle.IsValid(); }

	UE_DEPRECATED(5.4, "Index is deprecated, use DataHandle instead.")
	static bool IsValidIndex(const int32 Index) { return FStateTreeDataHandle::IsValidIndex(Index); }

	UPROPERTY()
	FStateTreeDataHandle DataHandle = FStateTreeDataHandle::Invalid;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.4, "Use DataHandle instead.")
	UPROPERTY()
	FStateTreeIndex16 DataViewIndex_DEPRECATED = FStateTreeIndex16::Invalid;
#endif // WITH_EDITORONLY_DATA
};

/**
 * Handle to access an external struct or object.
 * This reference handle can be used in StateTree tasks and evaluators to have quick access to external data.
 * The type provided to the template is used by the linker and context to pass along the type.
 *
 * USTRUCT()
 * struct FExampleTask : public FStateTreeTaskBase
 * {
 *    ...
 *
 *    bool Link(FStateTreeLinker& Linker)
 *    {
 *      Linker.LinkExternalData(ExampleSubsystemHandle);
 *      return true;
 *    }
 * 
 *    EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition)
 *    {
 *      const UExampleSubsystem& ExampleSubsystem = Context.GetExternalData(ExampleSubsystemHandle);
 *      ...
 *    }
 *
 *    TStateTreeExternalDataHandle<UExampleSubsystem> ExampleSubsystemHandle;
 * }
 */
template<typename T, EStateTreeExternalDataRequirement Req = EStateTreeExternalDataRequirement::Required>
struct TStateTreeExternalDataHandle : FStateTreeExternalDataHandle
{
	typedef T DataType;
	static constexpr EStateTreeExternalDataRequirement DataRequirement = Req;
};


/**
 * Describes an external data. The data can point to a struct or object.
 * The code that handles StateTree ticking is responsible for passing in the actually data, see FStateTreeExecutionContext.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeExternalDataDesc
{
	GENERATED_BODY()

	FStateTreeExternalDataDesc() = default;
	FStateTreeExternalDataDesc(const UStruct* InStruct, const EStateTreeExternalDataRequirement InRequirement) : Struct(InStruct), Requirement(InRequirement) {}

	FStateTreeExternalDataDesc(const FName InName, const UStruct* InStruct, const FGuid InGuid)
		: Struct(InStruct)
		, Name(InName)
#if WITH_EDITORONLY_DATA
		, ID(InGuid)
#endif
	{}

	/** @return true if the DataView is compatible with the descriptor. */
	bool IsCompatibleWith(const FStateTreeDataView& DataView) const
	{
		if (DataView.GetStruct()->IsChildOf(Struct))
		{
			return true;
		}
		
		if (const UClass* DataDescClass = Cast<UClass>(Struct))
		{
			if (const UClass* DataViewClass = Cast<UClass>(DataView.GetStruct()))
			{
				return DataViewClass->ImplementsInterface(DataDescClass);
			}
		}
		
		return false;
	}
	
	bool operator==(const FStateTreeExternalDataDesc& Other) const
	{
		return Struct == Other.Struct && Requirement == Other.Requirement;
	}
	
	/** Class or struct of the external data. */
	UPROPERTY();
	TObjectPtr<const UStruct> Struct = nullptr;

	/**
	 * Name of the external data. Used only for bindable external data (enforced by the schema).
	 * External data linked explicitly by the nodes (i.e. LinkExternalData) are identified only
	 * by their type since they are used for unique instance of a given type.  
	 */
	UPROPERTY(VisibleAnywhere, Category = Common)
	FName Name;
	
	/** Handle/Index to the StateTreeExecutionContext data views array */
	UPROPERTY();
	FStateTreeExternalDataHandle Handle;

	/** Describes if the data is required or not. */
	UPROPERTY();
	EStateTreeExternalDataRequirement Requirement = EStateTreeExternalDataRequirement::Required;

#if WITH_EDITORONLY_DATA
	/** Unique identifier. Used only for bindable external data. */
	UPROPERTY()
	FGuid ID;
#endif
};


/** Transition request */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeTransitionRequest
{
	GENERATED_BODY()

	FStateTreeTransitionRequest() = default;

	explicit FStateTreeTransitionRequest(const FStateTreeStateHandle InTargetState, const EStateTreeTransitionPriority InPriority = EStateTreeTransitionPriority::Normal)
		: TargetState(InTargetState)
		, Priority(InPriority)
	{
	}

	/** Source state of the transition. Filled in by the StateTree execution context. */
	UPROPERTY()
	FStateTreeStateHandle SourceState;

	/** StateTree asset that was active when the transition was requested. Filled in by the StateTree execution context. */
	UPROPERTY()
	TObjectPtr<const UStateTree> SourceStateTree = nullptr;

	/** Root state the execution frame where the transition was requested. Filled in by the StateTree execution context. */
	UPROPERTY()
	FStateTreeStateHandle SourceRootState = FStateTreeStateHandle::Invalid;

	/** Target state of the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Default")
	FStateTreeStateHandle TargetState;
	
	/** Priority of the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Default")
	EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::Normal;
};


/**
 * Describes an array of active states in a State Tree.
 */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeActiveStates
{
	GENERATED_BODY()

	static constexpr uint8 MaxStates = 8;	// Max number of active states

	FStateTreeActiveStates() = default;
	
	explicit FStateTreeActiveStates(const FStateTreeStateHandle StateHandle)
	{
		Push(StateHandle);
	}
	
	/** Resets the active state array to empty. */
	void Reset()
	{
		NumStates = 0;
	}

	/** Pushes new state at the back of the array and returns true if there was enough space. */
	bool Push(const FStateTreeStateHandle StateHandle)
	{
		if ((NumStates + 1) > MaxStates)
		{
			return false;
		}
		
		States[NumStates++] = StateHandle;
		
		return true;
	}

	/** Pushes new state at the front of the array and returns true if there was enough space. */
	bool PushFront(const FStateTreeStateHandle StateHandle)
	{
		if ((NumStates + 1) > MaxStates)
		{
			return false;
		}

		NumStates++;
		for (int32 Index = (int32)NumStates - 1; Index > 0; Index--)
		{
			States[Index] = States[Index - 1];
		}
		States[0] = StateHandle;
		
		return true;
	}

	/** Pops a state from the back of the array and returns the popped value, or invalid handle if the array was empty. */
	FStateTreeStateHandle Pop()
	{
		if (NumStates == 0)
		{
			return FStateTreeStateHandle::Invalid;			
		}

		const FStateTreeStateHandle Ret = States[NumStates - 1];
		NumStates--;
		return Ret;
	}

	/** Sets the number of states, new states are set to invalid state. */
	void SetNum(const int32 NewNum)
	{
		check(NewNum >= 0 && NewNum <= MaxStates);
		if (NewNum > (int32)NumStates)
		{
			for (int32 Index = NumStates; Index < NewNum; Index++)
			{
				States[Index] = FStateTreeStateHandle::Invalid;
			}
		}
		NumStates = static_cast<uint8>(NewNum);
	}

	/** Returns true of the array contains specified state. */
	bool Contains(const FStateTreeStateHandle StateHandle) const
	{
		for (const FStateTreeStateHandle& Handle : *this)
		{
			if (Handle == StateHandle)
			{
				return true;
			}
		}
		return false;
	}

	/** Returns true of the array contains specified state within MaxNumStatesToCheck states. */
	bool Contains(const FStateTreeStateHandle StateHandle, const uint8 MaxNumStatesToCheck) const
	{
		const int32 Num = (int32)FMath::Min(NumStates, MaxNumStatesToCheck);
		for (int32 Index = 0; Index < Num; Index++)
		{
			if (States[Index] == StateHandle)
			{
				return true;
			}
		}
		return false;
	}

	/** Returns index of a state, searching in reverse order. */
	int32 IndexOfReverse(const FStateTreeStateHandle StateHandle) const
	{
		for (int32 Index = (int32)NumStates - 1; Index >= 0; Index--)
		{
			if (States[Index] == StateHandle)
				return Index;
		}
		return INDEX_NONE;
	}
	
	/** Returns last state in the array, or invalid state if the array is empty. */
	FStateTreeStateHandle Last() const { return NumStates > 0 ? States[NumStates - 1] : FStateTreeStateHandle::Invalid; }
	
	/** Returns number of states in the array. */
	int32 Num() const { return NumStates; }

	/** Returns true if the index is within array bounds. */
	bool IsValidIndex(const int32 Index) const { return Index >= 0 && Index < (int32)NumStates; }
	
	/** Returns true if the array is empty. */
	bool IsEmpty() const { return NumStates == 0; } 

	/** Returns a specified state in the array. */
	FORCEINLINE FStateTreeStateHandle operator[](const int32 Index) const
	{
		check(Index >= 0 && Index < (int32)NumStates);
		return States[Index];
	}

	/** Returns mutable reference to a specified state in the array. */
	FORCEINLINE FStateTreeStateHandle& operator[](const int32 Index)
	{
		check(Index >= 0 && Index < (int32)NumStates);
		return States[Index];
	}

	/** Returns a specified state in the array, or FStateTreeStateHandle::Invalid if Index is out of array bounds. */
	FStateTreeStateHandle GetStateSafe(const int32 Index) const
	{
		return (Index >= 0 && Index < (int32)NumStates) ? States[Index] : FStateTreeStateHandle::Invalid;
	}

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE FStateTreeStateHandle* begin() { return &States[0]; }
	FORCEINLINE FStateTreeStateHandle* end  () { return &States[0] + Num(); }
	FORCEINLINE const FStateTreeStateHandle* begin() const { return &States[0]; }
	FORCEINLINE const FStateTreeStateHandle* end  () const { return &States[0] + Num(); }


	UPROPERTY(EditDefaultsOnly, Category = Default)
	FStateTreeStateHandle States[MaxStates];

	UPROPERTY(EditDefaultsOnly, Category = Default)
	uint8 NumStates = 0;
};


UENUM()
enum class EStateTreeTransitionSourceType : uint8
{
	Unset,
	Asset,
	ExternalRequest,
	Internal
};

/**
 * Describes the origin of an applied transition.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeTransitionSource
{
	GENERATED_BODY()

	FStateTreeTransitionSource() = default;

	explicit FStateTreeTransitionSource(const EStateTreeTransitionSourceType SourceType, const FStateTreeIndex16 TransitionIndex, const FStateTreeStateHandle TargetState, const EStateTreeTransitionPriority Priority)
	: SourceType(SourceType)
	, TransitionIndex(TransitionIndex)
	, TargetState(TargetState)
	, Priority(Priority)
	{
	}

	explicit FStateTreeTransitionSource(const FStateTreeIndex16 TransitionIndex, const FStateTreeStateHandle TargetState, const EStateTreeTransitionPriority Priority)
	: FStateTreeTransitionSource(EStateTreeTransitionSourceType::Asset, TransitionIndex, TargetState, Priority)
	{
	}

	explicit FStateTreeTransitionSource(const EStateTreeTransitionSourceType SourceType, const FStateTreeStateHandle TargetState, const EStateTreeTransitionPriority Priority)
	: FStateTreeTransitionSource(SourceType, FStateTreeIndex16::Invalid, TargetState, Priority)
	{
	}

	void Reset()
	{
		*this = {};
	}

	/** Describes where the transition originated. */
	EStateTreeTransitionSourceType SourceType = EStateTreeTransitionSourceType::Unset;

	/* Index of the transition if from predefined asset transitions, invalid otherwise */
	FStateTreeIndex16 TransitionIndex;

	/** Transition target state */
	FStateTreeStateHandle TargetState = FStateTreeStateHandle::Invalid;
	
	/** Priority of the transition that caused the state change. */
	EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::None;
};


#if WITH_STATETREE_DEBUGGER
struct STATETREEMODULE_API FStateTreeInstanceDebugId
{
	FStateTreeInstanceDebugId() = default;
	FStateTreeInstanceDebugId(const uint32 InstanceId, const uint32 SerialNumber)
		: Id(InstanceId), SerialNumber(SerialNumber)
	{
	}
	
	bool IsValid() const { return Id != INDEX_NONE && SerialNumber != INDEX_NONE; }
	bool IsInvalid() const { return !IsValid(); }
	void Reset() { *this = Invalid; }

	bool operator==(const FStateTreeInstanceDebugId& Other) const
	{
		return Id == Other.Id && SerialNumber == Other.SerialNumber;
	}

	bool operator!=(const FStateTreeInstanceDebugId& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FStateTreeInstanceDebugId InstanceDebugId)
	{
		return HashCombine(InstanceDebugId.Id, InstanceDebugId.SerialNumber);
	}

	friend FString LexToString(const FStateTreeInstanceDebugId InstanceDebugId)
	{
		return FString::Printf(TEXT("0x%x | %d"), InstanceDebugId.Id, InstanceDebugId.SerialNumber);
	}

	static const FStateTreeInstanceDebugId Invalid;
	
	uint32 Id = INDEX_NONE;
	uint32 SerialNumber = INDEX_NONE;
};
#endif // WITH_STATETREE_DEBUGGER

/** Describes current state of a delayed transition. */
USTRUCT()
struct STATETREEMODULE_API FStateTreeTransitionDelayedState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UStateTree> StateTree = nullptr;

	UPROPERTY()
	FStateTreeIndex16 TransitionIndex = FStateTreeIndex16::Invalid;

	UPROPERTY()
	float TimeLeft = 0.0f;
};

/** Describes an active branch of a State Tree. */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeExecutionFrame
{
	GENERATED_BODY()

	bool IsSameFrame(const FStateTreeExecutionFrame& OtherFrame) const
	{
		return StateTree == OtherFrame.StateTree && RootState == OtherFrame.RootState;
	}
	
	/** The State Tree used for ticking this frame. */
	UPROPERTY()
	TObjectPtr<const UStateTree> StateTree = nullptr;

	/** The root state of the frame (e.g. Root state or a subtree). */
	UPROPERTY()
	FStateTreeStateHandle RootState = FStateTreeStateHandle::Root; 
	
	/** Active states in this frame */
	UPROPERTY()
	FStateTreeActiveStates ActiveStates;

	/** First index of the external data for this frame. */
	UPROPERTY()
	FStateTreeIndex16 ExternalDataBaseIndex = FStateTreeIndex16::Invalid;

	/** Index within the instance data to the first global instance data (e.g. global tasks) */
	UPROPERTY()
	FStateTreeIndex16 GlobalInstanceIndexBase = FStateTreeIndex16::Invalid;

	/** Index within the instance data to the first active state's instance data (e.g. tasks) */
	UPROPERTY()
	FStateTreeIndex16 ActiveInstanceIndexBase = FStateTreeIndex16::Invalid;

	/** Handle to the state parameter data, exists in ParentFrame. */
	UPROPERTY()
	FStateTreeDataHandle StateParameterDataHandle = FStateTreeDataHandle::Invalid; 

	/** Handle to the global parameter data, exists in ParentFrame. */
	UPROPERTY()
	FStateTreeDataHandle GlobalParameterDataHandle = FStateTreeDataHandle::Invalid; 

	/** Number of states in ActiveStates which have instance data. Used during state selection to decide which active state data is safe to access. */
	uint8 NumCurrentlyActiveStates = 0;
	
	/** If true, the global tasks of the State Tree should be handle in this frame. */
	UPROPERTY()
	uint8 bIsGlobalFrame : 1 = false;
};

/** Describes the execution state of the current State Tree instance. */
USTRUCT()
struct STATETREEMODULE_API FStateTreeExecutionState
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FStateTreeExecutionState() = default;
	FStateTreeExecutionState(const FStateTreeExecutionState&) = default;
	FStateTreeExecutionState(FStateTreeExecutionState&&) = default;
	FStateTreeExecutionState& operator=(const FStateTreeExecutionState&) = default;
	FStateTreeExecutionState& operator=(FStateTreeExecutionState&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	void Reset()
	{
		ActiveFrames.Reset();
#if WITH_STATETREE_DEBUGGER
		InstanceDebugId.Reset();
#endif
		EnterStateFailedTaskIndex = FStateTreeIndex16::Invalid;
		LastTickStatus = EStateTreeRunStatus::Failed;
		TreeRunStatus = EStateTreeRunStatus::Unset;
		RequestedStop = EStateTreeRunStatus::Unset;
		CurrentPhase = EStateTreeUpdatePhase::Unset;
		CompletedStateHandle = FStateTreeStateHandle::Invalid;
		StateChangeCount = 0;
	}

	/** @returns Delayed transition state for a specific transition, or nullptr if it does not exists. */
	FStateTreeTransitionDelayedState* FindDelayedTransition(const UStateTree* OwnerStateTree, const FStateTreeIndex16 TransitionIndex)
	{
		return DelayedTransitions.FindByPredicate([OwnerStateTree, TransitionIndex](const FStateTreeTransitionDelayedState& TransitionState)
		{
			return TransitionState.StateTree == OwnerStateTree && TransitionState.TransitionIndex == TransitionIndex;
		});
	}

	/** Currently active frames (and states) */
	UPROPERTY()
	TArray<FStateTreeExecutionFrame> ActiveFrames;

	/** Pending delayed transitions. */
	UPROPERTY()
	TArray<FStateTreeTransitionDelayedState> DelayedTransitions;

#if WITH_STATETREE_DEBUGGER
	/** Id for the active instance used for debugging. */
	mutable FStateTreeInstanceDebugId InstanceDebugId;
#endif

	/** The index of the task that failed during enter state. Exit state uses it to call ExitState() symmetrically. */
	UPROPERTY()
	FStateTreeIndex16 EnterStateFailedFrameIndex = FStateTreeIndex16::Invalid;

	/** The index of the frame that failed during enter state. Exit state uses it to call ExitState() symmetrically. */
	UPROPERTY()
	FStateTreeIndex16 EnterStateFailedTaskIndex = FStateTreeIndex16::Invalid;

	/** Result of last tick */
	UPROPERTY()
	EStateTreeRunStatus LastTickStatus = EStateTreeRunStatus::Failed;

	/** Running status of the instance */
	UPROPERTY()
	EStateTreeRunStatus TreeRunStatus = EStateTreeRunStatus::Unset;

	/** Completion status stored if Stop was called during the Tick and needed to be deferred. */
	UPROPERTY()
	EStateTreeRunStatus RequestedStop = EStateTreeRunStatus::Unset;

	/** Current update phase used to validate reentrant calls to the main entry points of the execution context (i.e. Start, Stop, Tick). */
	UPROPERTY()
	EStateTreeUpdatePhase CurrentPhase = EStateTreeUpdatePhase::Unset;

	/** Handle of the state that was first to report state completed (success or failure), used to trigger completion transitions. */
	UPROPERTY()
	FStateTreeIndex16 CompletedFrameIndex = FStateTreeIndex16::Invalid; 
	
	UPROPERTY()
	FStateTreeStateHandle CompletedStateHandle = FStateTreeStateHandle::Invalid;

	/** Number of times a new state has been changed. */
	UPROPERTY()
	uint16 StateChangeCount = 0;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.3, "Use DataHandle instead.")
	UPROPERTY()
	FStateTreeActiveStates CurrentActiveStates_DEPRECATED;
	
	/** Index of the first task struct in the currently initialized instance data. */
	UE_DEPRECATED(5.4, "Superceded by State Tree Data Handles.")
	FStateTreeIndex16 FirstTaskStructIndex_DEPRECATED = FStateTreeIndex16::Invalid;
	
	/** Index of the first task object in the currently initialized instance data. */
	UE_DEPRECATED(5.4, "Superceded by State Tree Data Handles.")
	FStateTreeIndex16 FirstTaskObjectIndex_DEPRECATED = FStateTreeIndex16::Invalid;
#endif	
};

/**
 * Describes a state tree transition. Source is the state where the transition started, Target describes the state where the transition pointed at,
 * and Next describes the selected state. The reason Transition and Next are different is that Transition state can be a selector state,
 * in which case the children will be visited until a leaf state is found, which will be the next state.
 */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeTransitionResult
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FStateTreeTransitionResult() = default;
	FStateTreeTransitionResult(const FStateTreeTransitionResult&) = default;
	FStateTreeTransitionResult(FStateTreeTransitionResult&&) = default;
	FStateTreeTransitionResult& operator=(const FStateTreeTransitionResult&) = default;
	FStateTreeTransitionResult& operator=(FStateTreeTransitionResult&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	
	void Reset()
	{
		NextActiveFrames.Reset();
		CurrentRunStatus = EStateTreeRunStatus::Unset;
		TargetState = FStateTreeStateHandle::Invalid;
		CurrentState = FStateTreeStateHandle::Invalid;
		ChangeType = EStateTreeStateChangeType::Changed;
		Priority = EStateTreeTransitionPriority::None;
	}

	/** States selected as result of the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	TArray<FStateTreeExecutionFrame> NextActiveFrames;

	/** Current Run status. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	EStateTreeRunStatus CurrentRunStatus = EStateTreeRunStatus::Unset;

	/** Transition source state */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeStateHandle SourceState = FStateTreeStateHandle::Invalid;

	/** Transition target state */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeStateHandle TargetState = FStateTreeStateHandle::Invalid;

	/** The current state being executed. On enter/exit callbacks this is the state of the task. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeStateHandle CurrentState = FStateTreeStateHandle::Invalid;
	
	/** If the change type is Sustained, then the CurrentState was reselected, or if Changed then the state was just activated. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	EStateTreeStateChangeType ChangeType = EStateTreeStateChangeType::Changed; 

	/** Priority of the transition that caused the state change. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::None;

	/** StateTree asset that was active when the transition was requested. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	TObjectPtr<const UStateTree> SourceStateTree = nullptr;

	/** Root state the execution frame where the transition was requested. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeStateHandle SourceRootState = FStateTreeStateHandle::Invalid;
	
#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.4, "Use the ActiveFrames on FStateTreeExecutionState instead.")
	UPROPERTY()
	FStateTreeActiveStates CurrentActiveStates_DEPRECATED;

	UE_DEPRECATED(5.4, "Use the NextActiveFrames instead.")
	UPROPERTY()
	FStateTreeActiveStates NextActiveStates_DEPRECATED;
#endif	
};