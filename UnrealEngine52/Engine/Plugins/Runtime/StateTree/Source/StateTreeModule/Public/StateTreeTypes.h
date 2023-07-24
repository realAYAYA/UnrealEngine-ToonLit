// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBag.h"
#include "GameplayTagContainer.h"
#include "StateTreeTypes.generated.h"

STATETREEMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogStateTree, Warning, All);

#ifndef WITH_STATETREE_DEBUG
#define WITH_STATETREE_DEBUG (!(UE_BUILD_SHIPPING || UE_BUILD_SHIPPING_WITH_EDITOR || UE_BUILD_TEST) && 1)
#endif // WITH_STATETREE_DEBUG

namespace UE::StateTree
{
	inline constexpr int32 MaxConditionIndent = 4;

	inline const FName SchemaTag(TEXT("Schema"));
}; // UE::StateTree


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

/** Transitions behavior. */
UENUM()
enum class EStateTreeTransitionType : uint8
{
	/** No transition will take place. */
	None,

	/** Stop State Tree or sub-tree and mark execution succeeded. */
	Succeeded,
	
	/** Stop State Tree or sub-tree and mark execution failed. */
	Failed,
	
	/** Transition to the specified state. */
	GotoState,
	
	/** Transition to the next sibling state. */
	NextState,

	NotSet UE_DEPRECATED(5.0, "Use None instead."),
};

/** Operand between conditions */
UENUM()
enum class EStateTreeConditionOperand : uint8
{
	/** Copy result */
	Copy UMETA(Hidden),
	
	/** Combine results with AND. */
	And,
	
	/** Combine results with OR. */
	Or,
};

UENUM()
enum class EStateTreeStateType : uint8
{
	/** A State containing tasks and evaluators. */
	State,
	
	/** A State containing just sub states. */
	Group,
	
	/** A State that is linked to another state in the tree (the execution continues on the linked state). */
	Linked,
	
	/** A subtree that can be linked to. */
	Subtree,
};


/** Transitions trigger. */
UENUM()
enum class EStateTreeTransitionTrigger : uint8
{
	None = 0 UMETA(Hidden),

	/** Try trigger transition when a state succeeded or failed. */
	OnStateCompleted = 0x1 | 0x2,

	/** Try trigger transition when a state succeeded. */
    OnStateSucceeded = 0x1,

	/** Try trigger transition when a state failed. */
    OnStateFailed = 0x2,

	/** Try trigger transition each State Tree tick. */
    OnTick = 0x4,
	
	/** Try trigger transition on specific State Tree event. */
	OnEvent = 0x8,

	MAX
};
ENUM_CLASS_FLAGS(EStateTreeTransitionTrigger)


/** Transition priority. When multiple transitions trigger at the same time, the first transition of highest priority is selected. */
UENUM(BlueprintType)
enum class EStateTreeTransitionPriority : uint8
{
	None UMETA(Hidden),
	
	/** Normal priority. */
	Normal,
	
	/** Medium priority. */
	Medium,
	
	/** High priority. */
	High,
	
	/** Critical priority. */
	Critical,
};

inline bool operator<(const EStateTreeTransitionPriority Lhs, const EStateTreeTransitionPriority Rhs) { return static_cast<uint8>(Lhs) < static_cast<uint8>(Rhs); }
inline bool operator>(const EStateTreeTransitionPriority Lhs, const EStateTreeTransitionPriority Rhs) { return static_cast<uint8>(Lhs) > static_cast<uint8>(Rhs); }
inline bool operator<=(const EStateTreeTransitionPriority Lhs, const EStateTreeTransitionPriority Rhs) { return static_cast<uint8>(Lhs) <= static_cast<uint8>(Rhs); }
inline bool operator>=(const EStateTreeTransitionPriority Lhs, const EStateTreeTransitionPriority Rhs) { return static_cast<uint8>(Lhs) >= static_cast<uint8>(Rhs); }
inline bool operator==(const EStateTreeTransitionPriority Lhs, const EStateTreeTransitionPriority Rhs) { return static_cast<uint8>(Lhs) == static_cast<uint8>(Rhs); }
inline bool operator!=(const EStateTreeTransitionPriority Lhs, const EStateTreeTransitionPriority Rhs) { return static_cast<uint8>(Lhs) != static_cast<uint8>(Rhs); }

/** Handle to a StateTree state */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeStateHandle
{
	GENERATED_BODY()

	static constexpr uint16 InvalidIndex = uint16(-1);		// Index value indicating invalid item.
	static constexpr uint16 SucceededIndex = uint16(-2);	// Index value indicating a Succeeded item.
	static constexpr uint16 FailedIndex = uint16(-3);		// Index value indicating a Failed item.
	
	static const FStateTreeStateHandle Invalid;
	static const FStateTreeStateHandle Succeeded;
	static const FStateTreeStateHandle Failed;
	static const FStateTreeStateHandle Root;

	FStateTreeStateHandle() = default;
	explicit FStateTreeStateHandle(uint16 InIndex) : Index(InIndex) {}

	bool IsValid() const { return Index != InvalidIndex; }
	bool IsCompletionState() const { return Index == SucceededIndex || Index == FailedIndex; }
	EStateTreeRunStatus ToCompletionStatus() const
	{
		if (Index == SucceededIndex)
		{
			return EStateTreeRunStatus::Succeeded;
		}
		else if (Index == FailedIndex)
		{
			return EStateTreeRunStatus::Failed;
		}
		return EStateTreeRunStatus::Unset;
	}

	bool operator==(const FStateTreeStateHandle& RHS) const { return Index == RHS.Index; }
	bool operator!=(const FStateTreeStateHandle& RHS) const { return Index != RHS.Index; }

	FString Describe() const
	{
		switch (Index)
		{
		case InvalidIndex:		return TEXT("Invalid Item");
		case SucceededIndex:	return TEXT("Succeeded Item");
		case FailedIndex: 		return TEXT("Failed Item");
		default: 				return FString::Printf(TEXT("%d"), Index);
		}
	}

	UPROPERTY()
	uint16 Index = InvalidIndex;
};

/** uint16 index that can be invalid. */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeIndex16
{
	GENERATED_BODY()

	static constexpr uint16 InvalidValue = MAX_uint16;
	static const FStateTreeIndex16 Invalid;

	/** @return true if the given index can be represented by the type. */
	static bool IsValidIndex(const int32 Index)
	{
		return Index >= 0 && Index < (int32)MAX_uint16;
	}

	FStateTreeIndex16() = default;
	
	explicit FStateTreeIndex16(const int32 InIndex)
	{
		check(InIndex == INDEX_NONE || IsValidIndex(InIndex));
		Value = InIndex == INDEX_NONE ? InvalidValue : (uint16)InIndex;
	}

	/** @retrun value of the index. */
	uint16 Get() const { return Value; }
	
	/** @return the index value as int32, mapping invalid value to INDEX_NONE. */
	int32 AsInt32() const { return Value == InvalidValue ? INDEX_NONE : Value; }

	/** @return true if the index is valid. */
	bool IsValid() const { return Value != InvalidValue; }

	bool operator==(const FStateTreeIndex16& RHS) const { return Value == RHS.Value; }
	bool operator!=(const FStateTreeIndex16& RHS) const { return Value != RHS.Value; }

	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

protected:
	UPROPERTY()
	uint16 Value = InvalidValue;
};

template<>
struct TStructOpsTypeTraits<FStateTreeIndex16> : public TStructOpsTypeTraitsBase2<FStateTreeIndex16>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

/** uint8 index that can be invalid. */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeIndex8
{
	GENERATED_BODY()

	static constexpr uint8 InvalidValue = MAX_uint8;
	static const FStateTreeIndex8 Invalid;
	
	/** @return true if the given index can be represented by the type. */
	static bool IsValidIndex(const int32 Index)
	{
		return Index >= 0 && Index < (int32)MAX_uint8;
	}
	
	FStateTreeIndex8() = default;
	
	explicit FStateTreeIndex8(const int32 InIndex)
	{
		check(InIndex == INDEX_NONE || IsValidIndex(InIndex));
		Value = InIndex == INDEX_NONE ? InvalidValue : (uint8)InIndex;
	}

	/** @retrun value of the index. */
	uint8 Get() const { return Value; }

	/** @return the index value as int32, mapping invalid value to INDEX_NONE. */
	int32 AsInt32() const { return Value == InvalidValue ? INDEX_NONE : Value; }
	
	/** @return true if the index is valid. */
	bool IsValid() const { return Value != InvalidValue; }

	bool operator==(const FStateTreeIndex8& RHS) const { return Value == RHS.Value; }
	bool operator!=(const FStateTreeIndex8& RHS) const { return Value != RHS.Value; }

	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
	
protected:
	UPROPERTY()
	uint8 Value = InvalidValue;
};

template<>
struct TStructOpsTypeTraits<FStateTreeIndex8> : public TStructOpsTypeTraitsBase2<FStateTreeIndex8>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};


/** Transition request */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeTransitionRequest
{
	GENERATED_BODY()

	FStateTreeTransitionRequest() = default;
	FStateTreeTransitionRequest(const FStateTreeStateHandle InTargetState, const EStateTreeTransitionPriority InPriority = EStateTreeTransitionPriority::Normal)
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

/**
 * Time duration with random variance. Stored compactly as two uint16s, which gives time range of about 650 seconds.
 * The variance is symmetric (+-) around the specified duration.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeRandomTimeDuration
{
	GENERATED_BODY()

	/** Reset duration to empty. */
	void Reset()
	{
		Duration = 0;
		RandomVariance = 0;
	}

	/** Sets the time duration with random variance. */
	void Set(const float InDuration, const float InRandomVariance)
	{
		Duration = Quantize(InDuration);
    	RandomVariance = Quantize(InRandomVariance);
	}

	/** @return the fixed duration. */
	float GetDuration() const
    {
		return Duration / Scale;
    }

	/** @return the maximum random variance. */
	float GetRandomVariance() const
	{
		return Duration / Scale;
	}

	/** @return True of the duration is empty (always returns 0). */
	bool IsEmpty() const { return Duration == 0 && RandomVariance == 0; }
	
	/** @return Returns random duration around Duration, varied by +-RandomVariation. */
	float GetRandomDuration() const
	{
		const int32 MinVal = FMath::Max(0, static_cast<int32>(Duration) - static_cast<int32>(RandomVariance));
		const int32 MaxVal = static_cast<int32>(Duration) + static_cast<int32>(RandomVariance);
		return FMath::RandRange(MinVal, MaxVal) / Scale;
	}
	
protected:

	static constexpr float Scale = 100.0f;

	uint16 Quantize(const float Value) const
	{
		return (uint16)FMath::Clamp(FMath::RoundToInt32(Value * Scale), 0, (int32)MAX_uint16);
	}
	
	UPROPERTY(EditDefaultsOnly, Category = Default)
	uint16 Duration = 0;

	UPROPERTY(EditDefaultsOnly, Category = Default)
	uint16 RandomVariance = 0;
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


/**
 *  Runtime representation of a StateTree transition.
 */
USTRUCT()
struct STATETREEMODULE_API FCompactStateTransition
{
	GENERATED_BODY()

	/** @return True if the transition has delay. */
	bool HasDelay() const
	{
		return !Delay.IsEmpty();
	}
	
	/** Transition event tag, used when trigger type is event. */
	UPROPERTY()
	FGameplayTag EventTag;

	/** Index to first condition to test */
	UPROPERTY()
	uint16 ConditionsBegin = 0;

	/** Target state of the transition */
	UPROPERTY()
	FStateTreeStateHandle State = FStateTreeStateHandle::Invalid;

	/** Transition delay. */
	UPROPERTY()
	FStateTreeRandomTimeDuration Delay;
	
	/* Type of the transition trigger. */
	UPROPERTY()
	EStateTreeTransitionTrigger Trigger = EStateTreeTransitionTrigger::None;

	/* Priority of the transition. */
	UPROPERTY()
	EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::Normal;

	/** Number of conditions to test. */
	UPROPERTY()
	uint8 ConditionsNum = 0;
};

/**
 *  Runtime representation of a StateTree state.
 */
USTRUCT()
struct STATETREEMODULE_API FCompactStateTreeState
{
	GENERATED_BODY()

	FCompactStateTreeState()
		: bHasTransitionTasks(false)
	{
	}
	
	/** @return Index to the next sibling state. */
	uint16 GetNextSibling() const { return ChildrenEnd; }

	/** @return True if the state has any child states */
	bool HasChildren() const { return ChildrenEnd > ChildrenBegin; }

	/** Name of the State */
	UPROPERTY()
	FName Name;

	/** Linked state handle if the state type is linked state. */
	UPROPERTY()
	FStateTreeStateHandle LinkedState = FStateTreeStateHandle::Invalid; 

	/** Parent state handle, invalid if root state. */
	UPROPERTY()
	FStateTreeStateHandle Parent = FStateTreeStateHandle::Invalid;

	/** Index to first child state */
	UPROPERTY()
	uint16 ChildrenBegin = 0;

	/** Index one past the last child state. */
	UPROPERTY()
	uint16 ChildrenEnd = 0;

	/** Index to first state enter condition */
	UPROPERTY()
	uint16 EnterConditionsBegin = 0;

	/** Index to first transition */
	UPROPERTY()
	uint16 TransitionsBegin = 0;

	/** Index to first task */
	UPROPERTY()
	uint16 TasksBegin = 0;

	/** Index to state instance data. */
	UPROPERTY()
	FStateTreeIndex16 ParameterInstanceIndex = FStateTreeIndex16::Invalid;

	/** Data view index of the input parameters. */
	UPROPERTY()
	FStateTreeIndex16 ParameterDataViewIndex = FStateTreeIndex16::Invalid;

	/** Number of enter conditions */
	UPROPERTY()
	uint8 EnterConditionsNum = 0;

	/** Number of transitions */
	UPROPERTY()
	uint8 TransitionsNum = 0;

	/** Number of tasks */
	UPROPERTY()
	uint8 TasksNum = 0;

	/** Type of the sate */
	UPROPERTY()
	EStateTreeStateType Type = EStateTreeStateType::State;

	/** True if the state contains tasks that should be called during transition handling. */
	UPROPERTY()
	uint8 bHasTransitionTasks : 1;
};

USTRUCT()
struct STATETREEMODULE_API FCompactStateTreeParameters
{
	GENERATED_BODY()

	UPROPERTY()
	FStateTreeIndex16 BindingsBatch = FStateTreeIndex16::Invalid;

	UPROPERTY()
	FInstancedPropertyBag Parameters;
};

UENUM()
enum class EStateTreeExternalDataRequirement : uint8
{
	Required,	// StateTree cannot be executed if the data is not present.
	Optional,	// Data is optional for StateTree execution.
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

UENUM()
enum class EStateTreePropertyUsage : uint8
{
	Invalid,
	Context,
	Input,
	Parameter,
	Output,
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


/**
 * StateTree struct ref allows to get a reference/pointer to a specified type via property binding.
 * It is useful for referencing larger properties to avoid copies of the data, or to be able to write to a bounds property.
 *
 * The expected type of the reference should be set in "BaseStruct" meta tag.
 *
 * Example:
 *
 *	USTRUCT()
 *	struct FAwesomeTaskInstanceData
 *	{
 *		GENERATED_BODY()
 *
 *		UPROPERTY(VisibleAnywhere, Category = Input, meta = (BaseStruct = "/Script/AwesomeModule.AwesomeData"))
 *		FStateTreeStructRef Data;
 *	};
 *
 *
 *	if (const FAwesomeData* Awesome = InstanceData.Data.GetPtr<FAwesomeData>())
 *	{
 *		...
 *	}
 */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeStructRef
{
	GENERATED_BODY()

	FStateTreeStructRef() = default;

	/** @return true if the reference is valid (safe to use the reference getters). */
	bool IsValid() const
	{
		return Data.IsValid();
	}

	/** Sets the struct ref (used by property copy) */
	void Set(FStructView NewData)
	{
		Data = NewData;
	}

	/** Returns const reference to the struct, this getter assumes that all data is valid. */
	template <typename T>
	const T& Get() const
	{
		return Data.template Get<T>();
	}

	/** Returns const pointer to the struct, or nullptr if cast is not valid. */
	template <typename T>
	const T* GetPtr() const
	{
		return Data.template GetPtr<T>();
	}

	/** Returns mutable reference to the struct, this getter assumes that all data is valid. */
	template <typename T>
    T& GetMutable()
	{
		return Data.template GetMutable<T>();
	}

	/** Returns mutable pointer to the struct, or nullptr if cast is not valid. */
	template <typename T>
    T* GetMutablePtr()
	{
		return Data.template GetMutablePtr<T>();
	}

	/** @return Struct describing the data type. */
	const UScriptStruct* GetScriptStruct() const
	{
		return Data.GetScriptStruct();
	}

protected:
	FStructView Data;
};


/**
 * Short lived pointer to an UOBJECT() or USTRUCT().
 * The data view expects a type (UStruct) when you pass in a valid memory. In case of null, the type can be empty too.
 */
struct STATETREEMODULE_API FStateTreeDataView
{
	FStateTreeDataView() = default;

	// USTRUCT() constructor.
	FStateTreeDataView(const UScriptStruct* InScriptStruct, uint8* InMemory) : Struct(InScriptStruct), Memory(InMemory)
	{
		// Must have type with valid pointer.
		check(!Memory || (Memory && Struct));
	}

	// UOBJECT() constructor.
	FStateTreeDataView(UObject* Object) : Struct(Object ? Object->GetClass() : nullptr), Memory(reinterpret_cast<uint8*>(Object))
	{
		// Must have type with valid pointer.
		check(!Memory || (Memory && Struct));
	}

	// USTRUCT() from a StructView.
	FStateTreeDataView(FStructView StructView) : Struct(StructView.GetScriptStruct()), Memory(StructView.GetMutableMemory())
	{
		// Must have type with valid pointer.
		check(!Memory || (Memory && Struct));
	}

	/**
	 * Check is the view is valid (both pointer and type are set). On valid views it is safe to call the Get<>() methods returning a reference.
	 * @return True if the view is valid.
	*/
	bool IsValid() const
	{
		return Memory != nullptr && Struct != nullptr;
	}

	/*
	 * UOBJECT() getters (reference & pointer, const & mutable)
	 */
	template <typename T>
    typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, const T&>::Type Get() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticClass()));
		return *((T*)Memory);
	}

	template <typename T>
	typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, T&>::Type GetMutable() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticClass()));
		return *((T*)Memory);
	}

	template <typename T>
	typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, const T*>::Type GetPtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticClass()));
		return ((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, T*>::Type GetMutablePtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticClass()));
		return ((T*)Memory);
	}

	/*
	 * USTRUCT() getters (reference & pointer, const & mutable)
	 */
	template <typename T>
	typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived, const T&>::Type Get() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticStruct()));
		return *((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived, T&>::Type GetMutable() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticStruct()));
		return *((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived, const T*>::Type GetPtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticStruct()));
		return ((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived, T*>::Type GetMutablePtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticStruct()));
		return ((T*)Memory);
	}

	/** @return Struct describing the data type. */
	const UStruct* GetStruct() const { return Struct; }

	/** @return Raw const pointer to the data. */
	const uint8* GetMemory() const { return Memory; }

	/** @return Raw mutable pointer to the data. */
	uint8* GetMutableMemory() const { return Memory; }
	
protected:
	/** UClass or UScriptStruct of the data. */
	const UStruct* Struct = nullptr;

	/** Memory pointing at the class or struct */
	uint8* Memory = nullptr;
};

struct STATETREEMODULE_API FStateTreeTransitionDelayedState
{
	FStateTreeIndex16 TransitionIndex = FStateTreeIndex16::Invalid;
	float TimeLeft = 0.0f;
};

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

/**
 * Link to another state in StateTree
 */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeStateLink
{
	GENERATED_BODY()

	FStateTreeStateLink() = default;

#if WITH_EDITORONLY_DATA
	FStateTreeStateLink(const EStateTreeTransitionType InType) : LinkType(InType) {}

	UE_DEPRECATED(5.2, "Use UStateTreeState::GetLinkToState() instead.")
	void Set(const EStateTreeTransitionType InType, const class UStateTreeState* InState = nullptr) {}
	UE_DEPRECATED(5.2, "Use UStateTreeState::GetLinkToState() instead.")
	void Set(const class UStateTreeState* InState) {}
#endif // WITH_EDITORONLY_DATA
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FStateTreeStateLink(const FStateTreeStateLink& Other) = default;
	FStateTreeStateLink(FStateTreeStateLink&& Other) = default;
	FStateTreeStateLink& operator=(FStateTreeStateLink const& Other) = default;
	FStateTreeStateLink& operator=(FStateTreeStateLink&& Other) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	bool Serialize(FStructuredArchive::FSlot Slot);
	void PostSerialize(const FArchive& Ar);

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.2, "Will be removed in later releases.")
	bool IsValid() const { return ID.IsValid(); }
	
	/** Name of the state at the time of linking, used for error reporting. */
	UPROPERTY(EditDefaultsOnly, Category = "Link")
	FName Name;

	/** ID of the state linked to. */
	UPROPERTY(EditDefaultsOnly, Category = "Link")
	FGuid ID;

	/** Type of the transition, used at edit time to describe e.g. next state (which is calculated at compile time). */
	UPROPERTY(EditDefaultsOnly, Category = "Link")
	EStateTreeTransitionType LinkType = EStateTreeTransitionType::None;

	UE_DEPRECATED(5.2, "Use LinkType instead.")
	UPROPERTY()
	EStateTreeTransitionType Type_DEPRECATED = EStateTreeTransitionType::GotoState;
#endif // WITH_EDITORONLY_DATA

	/** Handle of the linked state. */
	UPROPERTY()
	FStateTreeStateHandle StateHandle; 
};

template<>
struct TStructOpsTypeTraits<FStateTreeStateLink> : public TStructOpsTypeTraitsBase2<FStateTreeStateLink>
{
	enum
	{
		WithStructuredSerializer = true,
		WithPostSerialize = true,
	};
};
