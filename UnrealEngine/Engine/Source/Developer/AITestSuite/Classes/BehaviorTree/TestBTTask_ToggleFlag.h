// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "TestBTTask_ToggleFlag.generated.h"

UCLASS(meta=(HiddenNode))
class UTestBTTask_ToggleFlag : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UTestBTTask_ToggleFlag(const FObjectInitializer& ObjectInitializer);

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY()
	int32 LogIndex;

	UPROPERTY()
	FName KeyName;

	UPROPERTY()
	int32 NumToggles;

	UPROPERTY()
	TEnumAsByte<EBTNodeResult::Type> TaskResult;
};
