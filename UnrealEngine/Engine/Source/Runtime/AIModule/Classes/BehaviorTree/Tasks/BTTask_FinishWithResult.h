// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FinishWithResult.generated.h"

/**
 * Instantly finishes with given result
 */
UCLASS()
class AIMODULE_API UBTTask_FinishWithResult : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FinishWithResult(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;
	
protected:
	/** allows adding random time to wait time */
	UPROPERTY(Category = Result, EditAnywhere)
	TEnumAsByte<EBTNodeResult::Type> Result;
};
