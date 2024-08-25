// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBag.h"
#include "GameplayTagContainer.h"
#include "StateTreeIndexTypes.h"
#include "StateTreeTypes.generated.h"

STATETREEMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogStateTree, Warning, All);

#ifndef WITH_STATETREE_DEBUG
#define WITH_STATETREE_DEBUG (!(UE_BUILD_SHIPPING || UE_BUILD_SHIPPING_WITH_EDITOR || UE_BUILD_TEST) && 1)
#endif // WITH_STATETREE_DEBUG

class UStateTree;

namespace UE::StateTree
{
	inline constexpr int32 MaxConditionIndent = 4;

	inline const FName SchemaTag(TEXT("Schema"));
}; // UE::StateTree

enum class EStateTreeRunStatus : uint8;

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

	/** Transition to the next selectable sibling state */
	NextSelectableState,

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
	/** A State containing tasks and child states. */
	State,
	
	/** A State containing just child states. */
	Group,
	
	/** A State that is linked to another state in the tree (the execution continues on the linked state). */
	Linked,

	/** A State that is linked to another StateTree asset (the execution continues on the Root state of the linked asset). */
	LinkedAsset,

	/** A subtree that can be linked to. */
	Subtree,
};

UENUM()
enum class EStateTreeStateSelectionBehavior : uint8
{
	/** The State cannot be directly selected. */
	None,

	/** When state is considered for selection, it is selected even if it has child states. */
	TryEnterState UMETA(DisplayName = "Try Enter"),

	/** When state is considered for selection, try to selects the first child state (in order they appear in the child list). If no child states are present, behaves like SelectState. */
	TrySelectChildrenInOrder UMETA(DisplayName = "Try Select Children In Order"),
	
	/** When state is considered for selection, try to trigger the transitions instead. */
	TryFollowTransitions UMETA(DisplayName = "Try Follow Transitions"),
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

	static constexpr uint16 InvalidIndex = uint16(-1);		// Index value indicating invalid state.
	static constexpr uint16 SucceededIndex = uint16(-2);	// Index value indicating a Succeeded state.
	static constexpr uint16 FailedIndex = uint16(-3);		// Index value indicating a Failed state.
	static constexpr uint16 StoppedIndex = uint16(-4);		// Index value indicating a Stopped state.
	
	static const FStateTreeStateHandle Invalid;
	static const FStateTreeStateHandle Succeeded;
	static const FStateTreeStateHandle Failed;
	static const FStateTreeStateHandle Stopped;
	static const FStateTreeStateHandle Root;

	/** @return true if the given index can be represented by the type. */
	static bool IsValidIndex(const int32 Index)
	{
		return Index >= 0 && Index < (int32)MAX_uint16;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FStateTreeStateHandle& Handle)
	{
		return GetTypeHash(Handle.Index);
	}

	FStateTreeStateHandle() = default;
	explicit FStateTreeStateHandle(const uint16 InIndex) : Index(InIndex) {}
	explicit FStateTreeStateHandle(const int32 InIndex) : Index()
	{
		check(InIndex == INDEX_NONE || IsValidIndex(InIndex));
		Index = InIndex == INDEX_NONE ? InvalidIndex : static_cast<uint16>(InIndex);	
	}

	bool IsValid() const { return Index != InvalidIndex; }
	void Invalidate() { Index = InvalidIndex; }
	bool IsCompletionState() const { return Index == SucceededIndex || Index == FailedIndex || Index == StoppedIndex; }
	EStateTreeRunStatus ToCompletionStatus() const;
	static FStateTreeStateHandle FromCompletionStatus(const EStateTreeRunStatus Status);

	bool operator==(const FStateTreeStateHandle& RHS) const { return Index == RHS.Index; }
	bool operator!=(const FStateTreeStateHandle& RHS) const { return Index != RHS.Index; }

	FString Describe() const
	{
		switch (Index)
		{
		case InvalidIndex:		return TEXT("Invalid");
		case SucceededIndex:	return TEXT("Succeeded");
		case FailedIndex: 		return TEXT("Failed");
		case StoppedIndex: 		return TEXT("Stopped");
		default: 				return FString::Printf(TEXT("%d"), Index);
		}
	}

	UPROPERTY()
	uint16 Index = InvalidIndex;
};


/** Data type the FStateTreeDataHandle is pointing at. */
UENUM(BlueprintType)
enum class EStateTreeDataSourceType : uint8
{
	None UMETA(Hidden),

	/** Global Tasks, Evaluators */
	GlobalInstanceData,

	/** Global Tasks, Evaluators*/
	GlobalInstanceDataObject,

	/** Active State Tasks */
	ActiveInstanceData,

	/** Active State Tasks */
	ActiveInstanceDataObject,

	/** Conditions */
	SharedInstanceData,

	/** Conditions */
	SharedInstanceDataObject,

	/** Context Data, Tree Parameters */
	ContextData,

	/** External Data required by the nodes. */
	ExternalData,

	/** Global parameters */
	GlobalParameterData,

	/** Parameters for subtree (may resolve to a linked state's parameters or default params) */
	SubtreeParameterData,

	/** Parameters for regular and linked states */
	StateParameterData,
};

/** Handle to a StateTree data */
USTRUCT(BlueprintType)
struct STATETREEMODULE_API FStateTreeDataHandle
{
	GENERATED_BODY()

	static const FStateTreeDataHandle Invalid;
	static constexpr uint16 InvalidIndex = MAX_uint16;

	/** @return true if the given index can be represented by the type. */
	static bool IsValidIndex(const int32 Index)
	{
		return Index >= 0 && Index < (int32)InvalidIndex;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FStateTreeDataHandle& Handle)
	{
		uint32 Hash = GetTypeHash(Handle.Source);
		Hash = HashCombineFast(Hash, GetTypeHash(Handle.Source));
		Hash = HashCombineFast(Hash, GetTypeHash(Handle.StateHandle));
		return Hash;
	}
	
	FStateTreeDataHandle() = default;
	
	explicit FStateTreeDataHandle(const EStateTreeDataSourceType InSource, const uint16 InIndex, const FStateTreeStateHandle InStateHandle = FStateTreeStateHandle::Invalid)
		: Source(InSource)
		, Index(InIndex)
		, StateHandle(InStateHandle)
	{
		// Require valid state for active instance data
		check(Source != EStateTreeDataSourceType::ActiveInstanceData || (Source == EStateTreeDataSourceType::ActiveInstanceData && StateHandle.IsValid()));
		check(Source != EStateTreeDataSourceType::ActiveInstanceDataObject || (Source == EStateTreeDataSourceType::ActiveInstanceDataObject && StateHandle.IsValid()));
		check(Source == EStateTreeDataSourceType::GlobalParameterData || InIndex != InvalidIndex);
	}

	explicit FStateTreeDataHandle(const EStateTreeDataSourceType InSource, const int32 InIndex, const FStateTreeStateHandle InStateHandle = FStateTreeStateHandle::Invalid)
		: Source(InSource)
		, StateHandle(InStateHandle)
	{
		// Require valid state for active instance data
		check(Source != EStateTreeDataSourceType::ActiveInstanceData || (Source == EStateTreeDataSourceType::ActiveInstanceData && StateHandle.IsValid()));
		check(Source != EStateTreeDataSourceType::ActiveInstanceDataObject || (Source == EStateTreeDataSourceType::ActiveInstanceDataObject && StateHandle.IsValid()));
		check(Source == EStateTreeDataSourceType::GlobalParameterData || IsValidIndex(InIndex));
		Index = static_cast<uint16>(InIndex);
	}

	explicit FStateTreeDataHandle(const EStateTreeDataSourceType InSource)
		: FStateTreeDataHandle(InSource, FStateTreeDataHandle::InvalidIndex)
	{}

	bool IsValid() const
	{
		return Source != EStateTreeDataSourceType::None;
	}

	void Reset()
	{
		Source = EStateTreeDataSourceType::None;
		Index = InvalidIndex;
		StateHandle = FStateTreeStateHandle::Invalid;
	}

	bool operator==(const FStateTreeDataHandle& RHS) const
	{
		return Source == RHS.Source && Index == RHS.Index && StateHandle == RHS.StateHandle;
	}
	
	bool operator!=(const FStateTreeDataHandle& RHS) const
	{
		return !(*this == RHS);
	}

	EStateTreeDataSourceType GetSource() const
	{
		return Source;
	}

	int32 GetIndex() const
	{
		return Index;
	}

	FStateTreeStateHandle GetState() const
	{
		return StateHandle;
	}

	bool IsObjectSource() const
	{
		return Source == EStateTreeDataSourceType::GlobalInstanceDataObject
			|| Source == EStateTreeDataSourceType::ActiveInstanceDataObject
			|| Source == EStateTreeDataSourceType::SharedInstanceDataObject;
	}
	
	FStateTreeDataHandle ToObjectSource() const
	{
		switch (Source)
		{
		case EStateTreeDataSourceType::GlobalInstanceData:
			return FStateTreeDataHandle(EStateTreeDataSourceType::GlobalInstanceDataObject, Index, StateHandle);
		case EStateTreeDataSourceType::ActiveInstanceData:
			return FStateTreeDataHandle(EStateTreeDataSourceType::ActiveInstanceDataObject, Index, StateHandle);
		case EStateTreeDataSourceType::SharedInstanceData:
			return FStateTreeDataHandle(EStateTreeDataSourceType::SharedInstanceDataObject, Index, StateHandle);
		default:
			return *this;
		}
	}
	
	FString Describe() const
	{

		switch (Source)
		{
		case EStateTreeDataSourceType::None:
			return TEXT("None");
		case EStateTreeDataSourceType::GlobalInstanceData:
			return FString::Printf(TEXT("Global[%d]"), Index);
		case EStateTreeDataSourceType::GlobalInstanceDataObject:
			return FString::Printf(TEXT("GlobalO[%d]"), Index);
		case EStateTreeDataSourceType::ActiveInstanceData:
			return FString::Printf(TEXT("Active[%d]"), Index);
		case EStateTreeDataSourceType::ActiveInstanceDataObject:
			return FString::Printf(TEXT("ActiveO[%d]"), Index);
		case EStateTreeDataSourceType::SharedInstanceData:
			return FString::Printf(TEXT("Shared[%d]"), Index);
		case EStateTreeDataSourceType::SharedInstanceDataObject:
			return FString::Printf(TEXT("SharedO[%d]"), Index);
		case EStateTreeDataSourceType::ContextData:
			return FString::Printf(TEXT("Context[%d]"), Index);
		case EStateTreeDataSourceType::GlobalParameterData:
			return FString::Printf(TEXT("GlobalParam"));
		case EStateTreeDataSourceType::SubtreeParameterData:
			return FString::Printf(TEXT("SubtreeParam[%d]"), Index);
		case EStateTreeDataSourceType::StateParameterData:
			return FString::Printf(TEXT("LinkedParam[%d]"), Index);
		default:
			return TEXT("---");
		}
	}
	
private:
	UPROPERTY()
	EStateTreeDataSourceType Source = EStateTreeDataSourceType::None;

	UPROPERTY()
	uint16 Index = InvalidIndex;

	UPROPERTY()
	FStateTreeStateHandle StateHandle = FStateTreeStateHandle::Invalid; 
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
		return static_cast<decltype(Scale)>(FMath::RandRange(MinVal, MaxVal)) / Scale;
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

/** Fallback behavior indicating what to do after failing to select a state */
UENUM()
enum class EStateTreeSelectionFallback : uint8
{
	/** No fallback */
	None,

	/** Find next selectable sibling, if any, and select it */
	NextSelectableSibling,
};

/**
 *  Runtime representation of a StateTree transition.
 */
USTRUCT()
struct STATETREEMODULE_API FCompactStateTransition
{
	GENERATED_BODY()

	explicit FCompactStateTransition()
		: bTransitionEnabled(true)
	{
	}

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

	/** Fallback of the transition if it fails to select the target state */
	UPROPERTY()
	EStateTreeSelectionFallback Fallback = EStateTreeSelectionFallback::None;

	/** Number of conditions to test. */
	UPROPERTY()
	uint8 ConditionsNum = 0;

	/** Indicates if the transition is enabled and should be considered. */
	UPROPERTY()
	uint8 bTransitionEnabled : 1;
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
		, bEnabled(true)
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

	UPROPERTY()
	TObjectPtr<UStateTree> LinkedAsset = nullptr;
	
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
	FStateTreeIndex16 ParameterTemplateIndex = FStateTreeIndex16::Invalid;

	UPROPERTY()
	FStateTreeDataHandle ParameterDataHandle = FStateTreeDataHandle::Invalid;

	UPROPERTY()
	FStateTreeIndex16 ParameterBindingsBatch = FStateTreeIndex16::Invalid;

	/** Number of enter conditions */
	UPROPERTY()
	uint8 EnterConditionsNum = 0;

	/** Number of transitions */
	UPROPERTY()
	uint8 TransitionsNum = 0;

	/** Number of tasks */
	UPROPERTY()
	uint8 TasksNum = 0;

	/** Number of instance data */
	UPROPERTY()
	uint8 InstanceDataNum = 0;

	/** Type of the state */
	UPROPERTY()
	EStateTreeStateType Type = EStateTreeStateType::State;

	/** What to do when the state is considered for selection. */
	UPROPERTY()
	EStateTreeStateSelectionBehavior SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;

	/** True if the state contains tasks that should be called during transition handling. */
	UPROPERTY()
	uint8 bHasTransitionTasks : 1;

	/** True if the state is Enabled (i.e. not explicitly marked as disabled). */
	UPROPERTY()
	uint8 bEnabled : 1;
};

USTRUCT()
struct STATETREEMODULE_API FCompactStateTreeParameters
{
	GENERATED_BODY()

	FCompactStateTreeParameters() = default;

	FCompactStateTreeParameters(const FInstancedPropertyBag& InParameters)
		: Parameters(InParameters)
	{
	}
	
	UPROPERTY()
	FInstancedPropertyBag Parameters;
};

UENUM()
enum class EStateTreeExternalDataRequirement : uint8
{
	Required,	// StateTree cannot be executed if the data is not present.
	Optional,	// Data is optional for StateTree execution.
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
 * Pair of state guid and its associated state handle created at compilation.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeStateIdToHandle
{
	GENERATED_BODY()

	FStateTreeStateIdToHandle() = default;
	explicit FStateTreeStateIdToHandle(const FGuid& Id, const FStateTreeStateHandle Handle)
		: Id(Id)
		, Handle(Handle)
	{
	}

	UPROPERTY();
	FGuid Id;

	UPROPERTY();
	FStateTreeStateHandle Handle;
};

/**
 * Pair of node id and its associated node index created at compilation.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeNodeIdToIndex
{
	GENERATED_BODY()

	FStateTreeNodeIdToIndex() = default;
	explicit FStateTreeNodeIdToIndex(const FGuid& Id, const FStateTreeIndex16 Index)
		: Id(Id)
		, Index(Index)
	{
	}

	UPROPERTY();
	FGuid Id;

	UPROPERTY();
	FStateTreeIndex16 Index;
};

/**
 * Pair of transition id and its associated compact transition index created at compilation.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeTransitionIdToIndex
{
	GENERATED_BODY()

	FStateTreeTransitionIdToIndex() = default;
	explicit FStateTreeTransitionIdToIndex(const FGuid& Id, const FStateTreeIndex16 Index)
		: Id(Id)
		, Index(Index)
	{
	}

	UPROPERTY();
	FGuid Id;
	
	UPROPERTY();
	FStateTreeIndex16 Index;
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
		return Data.template Get<T>();
	}

	/** Returns mutable pointer to the struct, or nullptr if cast is not valid. */
	template <typename T>
    T* GetMutablePtr()
	{
		return Data.template GetPtr<T>();
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
	FStateTreeDataView(const UStruct* InStruct, uint8* InMemory) : Struct(InStruct), Memory(InMemory)
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
	FStateTreeDataView(FStructView StructView) : Struct(StructView.GetScriptStruct()), Memory(StructView.GetMemory())
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
	typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TIsIInterface<T>::Value, const T&>::Type Get() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticStruct()));
		return *((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TIsIInterface<T>::Value, T&>::Type GetMutable() const
	{
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(T::StaticStruct()));
		return *((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TIsIInterface<T>::Value, const T*>::Type GetPtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticStruct()));
		return ((T*)Memory);
	}

	template <typename T>
    typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived && !TIsIInterface<T>::Value, T*>::Type GetMutablePtr() const
	{
		// If Memory is set, expect Struct too. Otherwise, let nulls pass through.
		check(!Memory || (Memory && Struct));
		check(!Struct || Struct->IsChildOf(T::StaticStruct()));
		return ((T*)Memory);
	}

	/*
	 * IInterface() getters (reference & pointer, const & mutable)
	 */
	template <typename T>
	typename TEnableIf<TIsIInterface<T>::Value, const T&>::Type Get() const
	{
		check(!Memory || (Memory && Struct));
		check(Struct->IsChildOf(UObject::StaticClass()) && ((UClass*)Struct)->ImplementsInterface(T::UClassType::StaticClass()));
		return *(T*)((UObject*)Memory)->GetInterfaceAddress(T::UClassType::StaticClass());
	}

	template <typename T>
    typename TEnableIf<TIsIInterface<T>::Value, T&>::Type GetMutable() const
	{
		check(!Memory || (Memory && Struct));
		check(Struct->IsChildOf(UObject::StaticClass()) && ((UClass*)Struct)->ImplementsInterface(T::UClassType::StaticClass()));
		return *(T*)((UObject*)Memory)->GetInterfaceAddress(T::UClassType::StaticClass());
	}

	template <typename T>
    typename TEnableIf<TIsIInterface<T>::Value, const T*>::Type GetPtr() const
	{
		check(!Memory || (Memory && Struct));
		check(Struct->IsChildOf(UObject::StaticClass()) && ((UClass*)Struct)->ImplementsInterface(T::UClassType::StaticClass()));
		return (T*)((UObject*)Memory)->GetInterfaceAddress(T::UClassType::StaticClass());
	}

	template <typename T>
    typename TEnableIf<TIsIInterface<T>::Value, T*>::Type GetMutablePtr() const
	{
		check(!Memory || (Memory && Struct));
		check(Struct->IsChildOf(UObject::StaticClass()) && ((UClass*)Struct)->ImplementsInterface(T::UClassType::StaticClass()));
		return (T*)((UObject*)Memory)->GetInterfaceAddress(T::UClassType::StaticClass());
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
