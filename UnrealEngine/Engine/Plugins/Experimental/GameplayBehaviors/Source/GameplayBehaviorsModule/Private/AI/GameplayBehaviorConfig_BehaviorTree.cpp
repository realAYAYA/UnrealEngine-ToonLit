// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/GameplayBehaviorConfig_BehaviorTree.h"
#include "AI/GameplayBehavior_BehaviorTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayBehaviorConfig_BehaviorTree)


UGameplayBehaviorConfig_BehaviorTree::UGameplayBehaviorConfig_BehaviorTree(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BehaviorClass = UGameplayBehavior_BehaviorTree::StaticClass();
	bRevertToPreviousBTOnFinish = true;
}

UBehaviorTree* UGameplayBehaviorConfig_BehaviorTree::GetBehaviorTree() const 
{ 
	return BehaviorTree.IsPending()
		? BehaviorTree.LoadSynchronous()
		: BehaviorTree.Get();
}

