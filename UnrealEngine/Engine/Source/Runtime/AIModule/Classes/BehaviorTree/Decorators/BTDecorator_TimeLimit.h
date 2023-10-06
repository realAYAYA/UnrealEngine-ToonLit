// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_TimeLimit.generated.h"

/**
 * Time Limit decorator node.
 * A decorator node that bases its condition on whether a timer has exceeded a specified value. The timer is reset each time the node becomes relevant.
 */
UCLASS(HideCategories=(Condition), MinimalAPI)
class UBTDecorator_TimeLimit : public UBTDecorator
{
	GENERATED_UCLASS_BODY()
		
	/** max allowed time for execution of underlying node */
	UPROPERTY(Category=Decorator, EditAnywhere)
	float TimeLimit;

	AIMODULE_API virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	AIMODULE_API virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	AIMODULE_API virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:
	AIMODULE_API virtual void OnNodeActivation(FBehaviorTreeSearchData& SearchData) override;
	AIMODULE_API virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
