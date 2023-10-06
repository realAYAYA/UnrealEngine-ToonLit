// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BTTask_RunBehavior.generated.h"

/**
 * RunBehavior task allows pushing subtrees on execution stack.
 * Subtree asset can't be changed in runtime! 
 *
 * This limitation is caused by support for subtree's root level decorators,
 * which are injected into parent tree, and structure of running tree
 * cannot be modified in runtime (see: BTNode: ExecutionIndex, MemoryOffset)
 *
 * Use RunBehaviorDynamic task for subtrees that need to be changed in runtime.
 */

UCLASS(MinimalAPI)
class UBTTask_RunBehavior : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	AIMODULE_API virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	AIMODULE_API virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	AIMODULE_API virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

	/** @returns number of injected nodes */
	AIMODULE_API int32 GetInjectedNodesCount() const;

	/** @returns subtree asset */
	AIMODULE_API UBehaviorTree* GetSubtreeAsset() const;

protected:

	/** behavior to run */
	UPROPERTY(Category = Node, EditAnywhere)
	TObjectPtr<UBehaviorTree> BehaviorAsset;

	/** called when subtree is removed from active stack */
	AIMODULE_API virtual void OnSubtreeDeactivated(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type NodeResult);
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE UBehaviorTree* UBTTask_RunBehavior::GetSubtreeAsset() const
{
	return BehaviorAsset;
}

FORCEINLINE int32 UBTTask_RunBehavior::GetInjectedNodesCount() const
{
	return BehaviorAsset ? BehaviorAsset->RootDecorators.Num() : 0;
}
