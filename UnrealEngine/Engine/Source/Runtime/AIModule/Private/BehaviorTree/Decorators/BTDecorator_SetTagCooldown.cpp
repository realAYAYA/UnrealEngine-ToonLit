// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_SetTagCooldown.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTDecorator_SetTagCooldown)

UBTDecorator_SetTagCooldown::UBTDecorator_SetTagCooldown(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Set Tag Cooldown";
	INIT_DECORATOR_NODE_NOTIFY_FLAGS();
	CooldownDuration = 5.0f;
	bAddToExistingDuration = false;

	bAllowAbortNone = false;
	bAllowAbortLowerPri = false;
	bAllowAbortChildNodes = false;
	FlowAbortMode = EBTFlowAbortMode::None;
}

void UBTDecorator_SetTagCooldown::OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult)
{
	SearchData.OwnerComp.AddCooldownTagDuration(CooldownTag, CooldownDuration, bAddToExistingDuration);
}

FString UBTDecorator_SetTagCooldown::GetStaticDescription() const
{
	// basic info: result after time
	return FString::Printf(TEXT("%s: set to %.1fs after execution"), *CooldownTag.ToString(), CooldownDuration);
}

#if WITH_EDITOR

FName UBTDecorator_SetTagCooldown::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.Cooldown.Icon");
}

#endif	// WITH_EDITOR6

