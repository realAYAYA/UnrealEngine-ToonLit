// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeState.h"
#include "StateTreeEditorPropertyBindings.h"
#include "StateTreeEditorData.generated.h"

class UStateTreeSchema;

UENUM()
enum class EStateTreeVisitor : uint8
{
	Continue,
	Break,
};

/**
 * Edit time data for StateTree asset. This data gets baked into runtime format before being used by the StateTreeInstance.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class STATETREEEDITORMODULE_API UStateTreeEditorData : public UObject, public IStateTreeEditorPropertyBindingsOwner
{
	GENERATED_BODY()
	
public:
	virtual void PostInitProperties() override;
	
	// IStateTreeEditorPropertyBindingsOwner
	virtual void GetAccessibleStructs(const FGuid TargetStructID, TArray<FStateTreeBindableStructDesc>& OutStructDescs) const override;
	virtual bool GetStructByID(const FGuid StructID, FStateTreeBindableStructDesc& OutStructDesc) const override;
	virtual FStateTreeEditorPropertyBindings* GetPropertyEditorBindings() override { return &EditorBindings; }
	// ~IStateTreeEditorPropertyBindingsOwner

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	/** Returns parent state of a struct, or nullptr if not found. */
	const UStateTreeState* GetStateByStructID(const FGuid TargetStructID) const;

	/** Returns state based on its ID, or nullptr if not found. */
	const UStateTreeState* GetStateByID(const FGuid StateID) const;

	/** Gets the IDs of all bindable structs in the StateTree. */
	void GetAllStructIDs(TMap<FGuid, const UStruct*>& AllStructs) const;

	/**
	* Iterates over all structs that are related to binding
	* @param InFunc function called at each node, should return true if visiting is continued or false to stop.
	*/
	void VisitHierarchy(TFunctionRef<EStateTreeVisitor(UStateTreeState& State, UStateTreeState* ParentState)> InFunc) const;

	/**
	 * Iterates over all structs that are related to binding
	 * @param InFunc function called at each node, should return true if visiting is continued or false to stop.
	 */
	void VisitHierarchyNodes(TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FGuid& ID, const FName& Name, const EStateTreeNodeType NodeType, const UScriptStruct* NodeStruct, const UStruct* InstanceStruct)> InFunc) const;

	/**
	 * Iterates over all nodes in a given state.
	 * @param InFunc function called at each node, should return true if visiting is continued or false to stop.
	 */
	EStateTreeVisitor VisitStateNodes(const UStateTreeState& State, TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FGuid& ID, const FName& Name, const EStateTreeNodeType NodeType, const UScriptStruct* NodeStruct, const UStruct* InstanceStruct)> InFunc) const;

	/**
	 * Returns array of nodes along the execution path, up to the TargetStruct.
	 * @param Path The states to visit during the check
	 * @param TargetStructID The ID of the node where to stop.
	 * @param OutStructDescs Array of nodes accessible on the given path.  
	 */
	void GetAccessibleStructs(const TConstArrayView<const UStateTreeState*> Path, const FGuid TargetStructID, TArray<FStateTreeBindableStructDesc>& OutStructDescs) const;

	/**
	 * Finds a bindable context struct based on name and type.
	 * @param ObjectType Object type to match
	 * @param ObjectNameHint Name to use if multiple context objects of same type are found. 
	 */
	FStateTreeBindableStructDesc FindContextData(const UStruct* ObjectType, const FString ObjectNameHint) const;
	
	// StateTree Builder API

	/**
	 * Adds new Subtree with specified name.
	 * @return Pointer to the new Subtree.
	 */
	UStateTreeState& AddSubTree(const FName Name)
	{
		UStateTreeState* SubTreeState = NewObject<UStateTreeState>(this, FName(), RF_Transactional);
		check(SubTreeState);
		SubTreeState->Name = Name;
		SubTrees.Add(SubTreeState);
		return *SubTreeState;
	}

	/**
	 * Adds new Subtree named "Root".
	 * @return Pointer to the new Subtree.
	 */
	UStateTreeState& AddRootState()
	{
		return AddSubTree(FName(TEXT("Root")));
	}

	/**
	 * Adds Evaluator of specified type.
	 * @return reference to the new Evaluator. 
	 */
	template<typename T, typename... TArgs>
	TStateTreeEditorNode<T>& AddEvaluator(TArgs&&... InArgs)
	{
		FStateTreeEditorNode& EvalItem = Evaluators.AddDefaulted_GetRef();
		EvalItem.ID = FGuid::NewGuid();
		EvalItem.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		T& Eval = EvalItem.Node.GetMutable<T>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Eval.GetInstanceDataType()))
		{
			EvalItem.Instance.InitializeAs(InstanceType);
		}
		return static_cast<TStateTreeEditorNode<T>&>(EvalItem);
	}

	/**
	 * Adds property binding between two structs.
	 */
	void AddPropertyBinding(const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath)
	{
		EditorBindings.AddPropertyBinding(SourcePath, TargetPath);
	}
	// ~StateTree Builder API

	/** Schema describing which inputs, evaluators, and tasks a StateTree can contain */	
	UPROPERTY(EditDefaultsOnly, Category = Common, Instanced)
	TObjectPtr<UStateTreeSchema> Schema = nullptr;

	/** Public parameters that could be used for bindings within the Tree. */
	UPROPERTY(EditDefaultsOnly, Category = Parameters)
	FStateTreeStateParameters RootParameters;

	UPROPERTY(EditDefaultsOnly, Category = "Evaluators", meta = (BaseStruct = "/Script/StateTreeModule.StateTreeEvaluatorBase", BaseClass = "/Script/StateTreeModule.StateTreeEvaluatorBlueprintBase"))
	TArray<FStateTreeEditorNode> Evaluators;

	UPROPERTY(meta = (ExcludeFromHash))
	FStateTreeEditorPropertyBindings EditorBindings;

	/** Top level States. */
	UPROPERTY()
	TArray<TObjectPtr<UStateTreeState>> SubTrees;
};
