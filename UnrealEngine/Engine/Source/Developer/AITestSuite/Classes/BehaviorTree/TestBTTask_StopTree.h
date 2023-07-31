// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "TestBTTask_StopTree.generated.h"

UENUM()
namespace EBTTestTaskStopTree
{
	enum Type
	{
		DuringExecute,
		DuringTick,
		DuringAbort,
		DuringFinish,
	};
}

UCLASS(meta=(HiddenNode))
class UTestBTTask_StopTree : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UTestBTTask_StopTree(const FObjectInitializer& ObjectInitializer);

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void TickTask(class UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	UPROPERTY()
	TEnumAsByte<EBTTestTaskStopTree::Type> StopTimming;

	UPROPERTY()
	int32 LogIndex;

	UPROPERTY()
	TEnumAsByte<EBTNodeResult::Type> LogResult;
};
