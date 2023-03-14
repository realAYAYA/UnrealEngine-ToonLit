// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTypes.h"
#include "StateTreeNodeBase.h"
#include "Misc/Guid.h"
#include "InstancedStruct.h"
#include "StateTreeEditorNode.generated.h"

UENUM()
enum class EStateTreeNodeType : uint8
{
	EnterCondition,
	Evaluator,
	Task,
	TransitionCondition,
	StateParameters,
};

/**
 * Base for Evaluator, Task and Condition nodes.
 */
USTRUCT()
struct STATETREEEDITORMODULE_API FStateTreeEditorNode
{
	GENERATED_BODY()

	void Reset()
	{
		Node.Reset();
		Instance.Reset();
		InstanceObject = nullptr;
		ID = FGuid();
	}

	FName GetName() const
	{
		if (const FStateTreeNodeBase* NodePtr = Node.GetPtr<FStateTreeNodeBase>())
		{
			return NodePtr->Name;
		}
		return FName();
	}

	UPROPERTY(EditDefaultsOnly, Category = Node)
	FInstancedStruct Node;

	UPROPERTY(EditDefaultsOnly, Category = Node)
	FInstancedStruct Instance;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = Node)
	TObjectPtr<UObject> InstanceObject = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = Node)
	FGuid ID;

	UPROPERTY(EditDefaultsOnly, Category = Node)
	uint8 ConditionIndent = 0;

	UPROPERTY(EditDefaultsOnly, Category = Node)
	EStateTreeConditionOperand ConditionOperand = EStateTreeConditionOperand::And; 
};

template <typename T>
struct TStateTreeEditorNode : public FStateTreeEditorNode
{
	using NodeType = T;
	FORCEINLINE T& GetNode() { return Node.template GetMutable<T>(); }
	FORCEINLINE typename T::FInstanceDataType& GetInstanceData() { return Instance.template GetMutable<typename T::FInstanceDataType>(); }
};
