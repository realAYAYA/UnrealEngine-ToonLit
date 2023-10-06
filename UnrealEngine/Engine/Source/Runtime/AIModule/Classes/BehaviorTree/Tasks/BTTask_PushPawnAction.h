// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Tasks/BTTask_PawnActionBase.h"
#include "BTTask_PushPawnAction.generated.h"

class UDEPRECATED_PawnAction;

/**
 * Action task node.
 * Push pawn action to controller.
 */
UCLASS(MinimalAPI)
class UBTTask_PushPawnAction : public UBTTask_PawnActionBase
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(Instanced)
	TObjectPtr<UDEPRECATED_PawnAction> Action_DEPRECATED;

	AIMODULE_API virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	AIMODULE_API virtual FString GetStaticDescription() const override;
};
