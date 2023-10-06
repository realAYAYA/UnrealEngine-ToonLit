// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "BehaviorTreeDecoratorGraph.generated.h"

class UBehaviorTreeDecoratorGraphNode;
class UEdGraphPin;
class UObject;

UCLASS()
class BEHAVIORTREEEDITOR_API UBehaviorTreeDecoratorGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	void CollectDecoratorData(TArray<class UBTDecorator*>& DecoratorInstances, TArray<struct FBTDecoratorLogic>& DecoratorOperations) const;
	int32 SpawnMissingNodes(const TArray<class UBTDecorator*>& NodeInstances, const TArray<struct FBTDecoratorLogic>& Operations, int32 StartIndex);

protected:

	const class UBehaviorTreeDecoratorGraphNode* FindRootNode() const;
	void CollectDecoratorDataWorker(const class UBehaviorTreeDecoratorGraphNode* Node, TArray<class UBTDecorator*>& DecoratorInstances, TArray<struct FBTDecoratorLogic>& DecoratorOperations) const;

	UEdGraphPin* FindFreePin(UEdGraphNode* Node, EEdGraphPinDirection Direction);
	UBehaviorTreeDecoratorGraphNode* SpawnMissingNodeWorker(const TArray<class UBTDecorator*>& NodeInstances, const TArray<struct FBTDecoratorLogic>& Operations, int32& Index,
		UEdGraphNode* ParentGraphNode, int32 ChildIdx);

};
