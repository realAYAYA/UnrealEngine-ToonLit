// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "StateTreeTypes.h"

#include "StateTreeExecutionTypes.generated.h"

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
	EnterConditions			UMETA(DisplayName = "Enter conditions"),
	EnterStates				UMETA(DisplayName = "Enter States"),
	ExitStates				UMETA(DisplayName = "Exit States"),
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

	static const FStateTreeExternalDataHandle Invalid;
	
	static bool IsValidIndex(const int32 Index) { return FStateTreeIndex16::IsValidIndex(Index); }
	bool IsValid() const { return DataViewIndex.IsValid(); }

	UPROPERTY()
	FStateTreeIndex16 DataViewIndex = FStateTreeIndex16::Invalid;
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
	UPROPERTY(EditDefaultsOnly, Category = "Default")
	FStateTreeStateHandle SourceState;

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

	EStateTreeTransitionSourceType SourceType = EStateTreeTransitionSourceType::Unset;

	/* Index of the transition if from predefined asset transitions, invalid otherwise */
	FStateTreeIndex16 TransitionIndex;

	/** Transition target state */
	FStateTreeStateHandle TargetState = FStateTreeStateHandle::Invalid;
	
	/** Priority of the transition that caused the state change. */
	EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::None;
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

	FStateTreeTransitionResult() = default;

	void Reset()
	{
		CurrentActiveStates.Reset();
		CurrentRunStatus = EStateTreeRunStatus::Unset;
		TargetState = FStateTreeStateHandle::Invalid;
		NextActiveStates.Reset();
		CurrentState = FStateTreeStateHandle::Invalid;
		ChangeType = EStateTreeStateChangeType::Changed;
		Priority = EStateTreeTransitionPriority::None;
	}
	
	/** Current active states, where the transition started. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeActiveStates CurrentActiveStates;

	/** Current Run status. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	EStateTreeRunStatus CurrentRunStatus = EStateTreeRunStatus::Unset;

	/** Transition source state */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeStateHandle SourceState = FStateTreeStateHandle::Invalid;

	/** Transition target state */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeStateHandle TargetState = FStateTreeStateHandle::Invalid;

	/** States selected as result of the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeActiveStates NextActiveStates;

	/** The current state being executed. On enter/exit callbacks this is the state of the task. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FStateTreeStateHandle CurrentState = FStateTreeStateHandle::Invalid;

	/** If the change type is Sustained, then the CurrentState was reselected, or if Changed then the state was just activated. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	EStateTreeStateChangeType ChangeType = EStateTreeStateChangeType::Changed; 

	/** Priority of the transition that caused the state change. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
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

USTRUCT()
struct STATETREEMODULE_API FStateTreeExecutionState
{
	GENERATED_BODY()

	/** @returns Delayed transition state for a specific transition, or nullptr if it does not exists. */
	FStateTreeTransitionDelayedState* FindDelayedTransition(const FStateTreeIndex16 TransitionIndex)
	{
		return DelayedTransitions.FindByPredicate([TransitionIndex](const FStateTreeTransitionDelayedState& State){ return State.TransitionIndex == TransitionIndex; });
	}

	/** Currently active states */
	FStateTreeActiveStates ActiveStates;

#if WITH_STATETREE_DEBUGGER
	/** Id for the active instance used for debugging. */
	mutable FStateTreeInstanceDebugId InstanceDebugId;
#endif

	/** Index of the first task struct in the currently initialized instance data. */
	FStateTreeIndex16 FirstTaskStructIndex = FStateTreeIndex16::Invalid;
	
	/** Index of the first task object in the currently initialized instance data. */
	FStateTreeIndex16 FirstTaskObjectIndex = FStateTreeIndex16::Invalid;

	/** The index of the task that failed during enter state. Exit state uses it to call ExitState() symmetrically. */
	FStateTreeIndex16 EnterStateFailedTaskIndex = FStateTreeIndex16::Invalid;

	/** Result of last tick */
	EStateTreeRunStatus LastTickStatus = EStateTreeRunStatus::Failed;

	/** Running status of the instance */
	EStateTreeRunStatus TreeRunStatus = EStateTreeRunStatus::Unset;

	/** Handle of the state that was first to report state completed (success or failure), used to trigger completion transitions. */
	FStateTreeStateHandle CompletedStateHandle = FStateTreeStateHandle::Invalid;

	/** Number of times a new state has been changed. */
	uint16 StateChangeCount = 0;

	/** Running time of the delayed transition */
	TArray<FStateTreeTransitionDelayedState> DelayedTransitions;
};
