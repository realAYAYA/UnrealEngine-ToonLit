// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TestBTTask_SetFlag.generated.h"

UCLASS(meta=(HiddenNode))
class UTestBTTask_SetFlag : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FName KeyName;

	UPROPERTY()
	bool bValue;

	UPROPERTY()
	FName OnAbortKeyName;

	UPROPERTY()
	bool bOnAbortValue;

	UPROPERTY()
	TEnumAsByte<EBTNodeResult::Type> TaskResult;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
