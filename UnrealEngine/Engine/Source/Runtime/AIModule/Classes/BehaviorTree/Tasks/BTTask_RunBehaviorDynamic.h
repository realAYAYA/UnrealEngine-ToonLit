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

UCLASS(MinimalAPI)
class UBTTask_RunBehaviorDynamic : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	AIMODULE_API virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	AIMODULE_API virtual void OnInstanceCreated(UBehaviorTreeComponent& OwnerComp) override;
	AIMODULE_API virtual FString GetStaticDescription() const override;
	AIMODULE_API virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;

#if WITH_EDITOR
	AIMODULE_API virtual FName GetNodeIconName() const override;
	AIMODULE_API UBehaviorTree* GetBehaviorAssetFromRuntimeValue(const FString& RuntimeValue) const;
#endif // WITH_EDITOR

	AIMODULE_API bool HasMatchingTag(const FGameplayTag& Tag) const;
	AIMODULE_API const FGameplayTag& GetInjectionTag() const;
	AIMODULE_API bool SetBehaviorAsset(UBehaviorTree* NewBehaviorAsset);
	
	/** @returns default subtree asset */
	AIMODULE_API UBehaviorTree* GetDefaultBehaviorAsset() const;

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
	AIMODULE_API virtual void OnSubtreeDeactivated(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type NodeResult);
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
