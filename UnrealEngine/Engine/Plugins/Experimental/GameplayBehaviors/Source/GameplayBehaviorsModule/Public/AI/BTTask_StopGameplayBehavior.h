// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_StopGameplayBehavior.generated.h"


class UGameplayBehavior;
/**
*
*/
UCLASS()
class GAMEPLAYBEHAVIORSMODULE_API UBTTask_StopGameplayBehavior : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_StopGameplayBehavior(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override { return 0; }

	virtual FString GetStaticDescription() const override;

protected:
	/** If None (the default) will stop any and all gameplay behaviors instigated by the agent*/
	UPROPERTY(EditAnywhere, Category = "Node")
	TSubclassOf<UGameplayBehavior> BehaviorToStop;
};
