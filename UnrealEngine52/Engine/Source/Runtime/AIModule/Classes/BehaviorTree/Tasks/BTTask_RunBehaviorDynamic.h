// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_RunBehaviorDynamic.generated.h"

class UBehaviorTree;

/**
 * RunBehaviorDynamic task allows pushing subtrees on execution stack.
 * Subtree asset can be assigned at runtime with SetDynamicSubtree function of BehaviorTreeComponent.
 *
 * Does NOT support subtree's root level decorators!
 */

UCLASS()
class AIMODULE_API UBTTask_RunBehaviorDynamic : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnInstanceCreated(UBehaviorTreeComponent& OwnerComp) override;
	virtual FString GetStaticDescription() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

	bool HasMatchingTag(const FGameplayTag& Tag) const;
	const FGameplayTag& GetInjectionTag() const;
	bool SetBehaviorAsset(UBehaviorTree* NewBehaviorAsset);
	
	/** @returns default subtree asset */
	UBehaviorTree* GetDefaultBehaviorAsset() const;

protected:

	/** Gameplay tag that will identify this task for subtree injection */
	UPROPERTY(Category=Node, EditAnywhere)
	FGameplayTag InjectionTag;

	/** default behavior to run */
	UPROPERTY(Category=Node, EditAnywhere)
	TObjectPtr<UBehaviorTree> DefaultBehaviorAsset;

	/** current subtree */
	UPROPERTY()
	TObjectPtr<UBehaviorTree> BehaviorAsset;

	/** called when subtree is removed from active stack */
	virtual void OnSubtreeDeactivated(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type NodeResult);
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE bool UBTTask_RunBehaviorDynamic::HasMatchingTag(const FGameplayTag& Tag) const
{
	return InjectionTag == Tag;
}

FORCEINLINE UBehaviorTree* UBTTask_RunBehaviorDynamic::GetDefaultBehaviorAsset() const
{
	return DefaultBehaviorAsset;
}

FORCEINLINE const FGameplayTag& UBTTask_RunBehaviorDynamic::GetInjectionTag() const
{
	return InjectionTag;
}
