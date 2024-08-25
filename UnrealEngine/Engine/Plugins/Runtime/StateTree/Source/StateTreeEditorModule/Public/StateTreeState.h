// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeEditorNode.h"
#include "StateTreeEditorTypes.h"
#include "StateTreeState.generated.h"

class UStateTreeState;

/**
 * Editor representation of a transition in StateTree
 */
USTRUCT()
struct STATETREEEDITORMODULE_API FStateTreeTransition
{
	GENERATED_BODY()

	FStateTreeTransition() = default;
	FStateTreeTransition(const EStateTreeTransitionTrigger InTrigger, const EStateTreeTransitionType InType, const UStateTreeState* InState = nullptr);
	FStateTreeTransition(const EStateTreeTransitionTrigger InTrigger, const FGameplayTag InEventTag, const EStateTreeTransitionType InType, const UStateTreeState* InState = nullptr);

	template<typename T, typename... TArgs>
	TStateTreeEditorNode<T>& AddCondition(TArgs&&... InArgs)
	{
		FStateTreeEditorNode& CondNode = Conditions.AddDefaulted_GetRef();
		CondNode.ID = FGuid::NewGuid();
		CondNode.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		T& Cond = CondNode.Node.GetMutable<T>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Cond.GetInstanceDataType()))
		{
			CondNode.Instance.InitializeAs(InstanceType);
		}
		return static_cast<TStateTreeEditorNode<T>&>(CondNode);
	}

	/** When to try trigger the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	EStateTreeTransitionTrigger Trigger = EStateTreeTransitionTrigger::OnStateCompleted;

	/** Tag of the State Tree event that triggers the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	FGameplayTag EventTag;

	/** Transition target state. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta=(DisplayName="Transition To"))
	FStateTreeStateLink State;

	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	FGuid ID;

	/**
	 * Transition priority when multiple transitions happen at the same time.
	 * During transition handling, the transitions are visited from leaf to root.
	 * The first visited transition, of highest priority, that leads to a state selection, will be activated.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::Normal;

	/** Delay the triggering of the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	bool bDelayTransition = false;

	/** Transition delay duration in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (EditCondition = "bDelayTransition", UIMin = "0", ClampMin = "0", UIMax = "25", ClampMax = "25", ForceUnits="s"))
	float DelayDuration = 0.0f;

	/** Transition delay random variance in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (EditCondition = "bDelayTransition", UIMin = "0", ClampMin = "0", UIMax = "25", ClampMax = "25", ForceUnits="s"))
	float DelayRandomVariance = 0.0f;

	/** Conditions that must pass so that the transition can be triggered. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (BaseStruct = "/Script/StateTreeModule.StateTreeConditionBase", BaseClass = "/Script/StateTreeModule.StateTreeConditionBlueprintBase"))
	TArray<FStateTreeEditorNode> Conditions;

	/** True if the Transition is Enabled (i.e. not explicitly disabled in the asset). */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bTransitionEnabled = true;
};


USTRUCT()
struct STATETREEEDITORMODULE_API FStateTreeStateParameters
{
	GENERATED_BODY()

	void Reset()
	{
		Parameters.Reset();
		PropertyOverrides.Reset();
		bFixedLayout = false;
	}

	/** Removes overrides that do appear in Parameters. */
	void RemoveUnusedOverrides();
	
	UPROPERTY(EditDefaultsOnly, Category = Parameters)
	FInstancedPropertyBag Parameters;

	/** Overrides for parameters. */
	UPROPERTY()
	TArray<FGuid> PropertyOverrides;

	UPROPERTY(EditDefaultsOnly, Category = Parameters)
	bool bFixedLayout = false;

	UPROPERTY(EditDefaultsOnly, Category = Parameters, meta = (IgnoreForMemberInitializationTest))
	FGuid ID;
};

/**
 * Editor representation of a state in StateTree
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class STATETREEEDITORMODULE_API UStateTreeState : public UObject
{
	GENERATED_BODY()

public:
	UStateTreeState(const FObjectInitializer& ObjectInitializer);
	virtual ~UStateTreeState() override;

	virtual void PostInitProperties() override;
	virtual void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;;
	void UpdateParametersFromLinkedSubtree();
	void OnTreeCompiled(const UStateTree& StateTree);

	const UStateTreeState* GetRootState() const;
	const UStateTreeState* GetNextSiblingState() const;
	const UStateTreeState* GetNextSelectableSiblingState() const;

	/** @return true if the property of specified ID is overridden. */
	bool IsParametersPropertyOverridden(const FGuid PropertyID) const
	{
		return Parameters.PropertyOverrides.Contains(PropertyID);
	}

	/** Sets the override status of specified property by ID. */
	void SetParametersPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden);

	/** @returns Default parameters from linked state or asset). */
	const FInstancedPropertyBag* GetDefaultParameters() const;
	
	// StateTree Builder API
	/** @return state link to this state. */
	FStateTreeStateLink GetLinkToState() const;
	
	/** Adds child state with specified name. */
	UStateTreeState& AddChildState(const FName ChildName, const EStateTreeStateType StateType = EStateTreeStateType::State)
	{
		UStateTreeState* ChildState = NewObject<UStateTreeState>(this, FName(), RF_Transactional);
		check(ChildState);
		ChildState->Name = ChildName;
		ChildState->Parent = this;
		ChildState->Type = StateType;
		Children.Add(ChildState);
		return *ChildState;
	}

	/**
	 * Adds enter condition of specified type.
	 * @return reference to the new condition.
	 */
	template<typename T, typename... TArgs>
	TStateTreeEditorNode<T>& AddEnterCondition(TArgs&&... InArgs)
	{
		FStateTreeEditorNode& CondNode = EnterConditions.AddDefaulted_GetRef();
		CondNode.ID = FGuid::NewGuid();
		CondNode.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		T& Cond = CondNode.Node.GetMutable<T>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Cond.GetInstanceDataType()))
		{
			CondNode.Instance.InitializeAs(InstanceType);
		}
		return static_cast<TStateTreeEditorNode<T>&>(CondNode);
	}

	/**
	 * Adds Task of specified type.
	 * @return reference to the new Task.
	 */
	template<typename T, typename... TArgs>
	TStateTreeEditorNode<T>& AddTask(TArgs&&... InArgs)
	{
		FStateTreeEditorNode& TaskItem = Tasks.AddDefaulted_GetRef();
		TaskItem.ID = FGuid::NewGuid();
		TaskItem.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		T& Task = TaskItem.Node.GetMutable<T>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Task.GetInstanceDataType()))
		{
			TaskItem.Instance.InitializeAs(InstanceType);
		}
		return static_cast<TStateTreeEditorNode<T>&>(TaskItem);
	}

	/**
	 * Adds Transition.
	 * @return reference to the new Transition.
	 */
	FStateTreeTransition& AddTransition(const EStateTreeTransitionTrigger InTrigger, const EStateTreeTransitionType InType, const UStateTreeState* InState = nullptr)
	{
		FStateTreeTransition& Transition = Transitions.Emplace_GetRef(InTrigger, InType, InState);
		Transition.ID = FGuid::NewGuid();
		return Transition;
	}

	FStateTreeTransition& AddTransition(const EStateTreeTransitionTrigger InTrigger, const FGameplayTag InEventTag, const EStateTreeTransitionType InType, const UStateTreeState* InState = nullptr)
	{
		FStateTreeTransition& Transition = Transitions.Emplace_GetRef(InTrigger, InEventTag, InType, InState);
		Transition.ID = FGuid::NewGuid();
		return Transition;
	}


	// ~StateTree Builder API

	/** Display name of the State */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	FName Name;

	/** Display color of the State */
	UPROPERTY(EditDefaultsOnly, Category = "State", DisplayName = "Color")
	FStateTreeEditorColorRef ColorRef;

	/** Type the State, allows e.g. states to be linked to other States. */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	EStateTreeStateType Type = EStateTreeStateType::State;

	/** How to treat child states when this State is selected.  */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	EStateTreeStateSelectionBehavior SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;

	/** Subtree to run as extension of this State. */
	UPROPERTY(EditDefaultsOnly, Category = "State", Meta=(DirectStatesOnly, SubtreesOnly))
	FStateTreeStateLink LinkedSubtree;

	/** Another State Tree asset to run as extension of this State. */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	TObjectPtr<UStateTree> LinkedAsset = nullptr;

	/** Parameters of this state. If the state is linked to another state or asset, the parameters are for the linked state. */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	FStateTreeStateParameters Parameters;

	UPROPERTY(EditDefaultsOnly, Category = "State", meta = (IgnoreForMemberInitializationTest))
	FGuid ID;

	UPROPERTY(EditDefaultsOnly, Category = "Enter Conditions", meta = (BaseStruct = "/Script/StateTreeModule.StateTreeConditionBase", BaseClass = "/Script/StateTreeModule.StateTreeConditionBlueprintBase"))
	TArray<FStateTreeEditorNode> EnterConditions;

	UPROPERTY(EditDefaultsOnly, Category = "Tasks", meta = (BaseStruct = "/Script/StateTreeModule.StateTreeTaskBase", BaseClass = "/Script/StateTreeModule.StateTreeTaskBlueprintBase"))
	TArray<FStateTreeEditorNode> Tasks;

	// Single item used when schema calls for single task per state.
	UPROPERTY(EditDefaultsOnly, Category = "Task", meta = (BaseStruct = "/Script/StateTreeModule.StateTreeTaskBase", BaseClass = "/Script/StateTreeModule.StateTreeTaskBlueprintBase"))
	FStateTreeEditorNode SingleTask;

	UPROPERTY(EditDefaultsOnly, Category = "Transitions")
	TArray<FStateTreeTransition> Transitions;

	UPROPERTY()
	TArray<TObjectPtr<UStateTreeState>> Children;

	UPROPERTY(meta = (ExcludeFromHash))
	bool bExpanded = true;

	UPROPERTY(EditDefaultsOnly, Category = "State")
	bool bEnabled = true;

	UPROPERTY(meta = (ExcludeFromHash))
	TObjectPtr<UStateTreeState> Parent = nullptr;
};
