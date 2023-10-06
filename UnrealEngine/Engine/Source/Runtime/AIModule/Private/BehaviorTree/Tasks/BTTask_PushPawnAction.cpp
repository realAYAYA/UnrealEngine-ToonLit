// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Tasks/BTTask_PushPawnAction.h"
#include "Actions/PawnAction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTTask_PushPawnAction)

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UBTTask_PushPawnAction::UBTTask_PushPawnAction(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Push PawnAction";
}

EBTNodeResult::Type UBTTask_PushPawnAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UDEPRECATED_PawnAction* ActionCopy = Action_DEPRECATED ? DuplicateObject<UDEPRECATED_PawnAction>(Action_DEPRECATED, &OwnerComp) : nullptr;
	if (ActionCopy == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	return PushAction(OwnerComp, *ActionCopy);
}

FString UBTTask_PushPawnAction::GetStaticDescription() const
{
	return FString::Printf(TEXT("Push Action: %s"), *GetNameSafe(Action_DEPRECATED));
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
