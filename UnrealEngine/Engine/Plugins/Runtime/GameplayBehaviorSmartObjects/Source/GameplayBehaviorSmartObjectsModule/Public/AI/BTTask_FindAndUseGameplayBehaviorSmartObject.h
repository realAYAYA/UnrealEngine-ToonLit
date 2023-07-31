// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "AI/AITask_UseGameplayBehaviorSmartObject.h"
#include "BTTask_FindAndUseGameplayBehaviorSmartObject.generated.h"


class AITask_UseSmartObject;

struct FBTUseSOTaskMemory
{
	TWeakObjectPtr<UAITask_UseGameplayBehaviorSmartObject> TaskInstance;
};

/**
*
*/
UCLASS()
class GAMEPLAYBEHAVIORSMARTOBJECTSMODULE_API UBTTask_FindAndUseGameplayBehaviorSmartObject : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_FindAndUseGameplayBehaviorSmartObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTUseSOTaskMemory); }

	virtual FString GetStaticDescription() const override;

protected:
	/** Additional tag query to filter available smart objects. We'll query for smart
	 *	objects that support activities tagged in a way matching the filter.
	 *	Note that regular tag-base filtering is going to take place as well */
	UPROPERTY(EditAnywhere, Category = SmartObjects)
	FGameplayTagQuery ActivityRequirements;

	UPROPERTY(EditAnywhere, Category = SmartObjects)
	float Radius;
};
