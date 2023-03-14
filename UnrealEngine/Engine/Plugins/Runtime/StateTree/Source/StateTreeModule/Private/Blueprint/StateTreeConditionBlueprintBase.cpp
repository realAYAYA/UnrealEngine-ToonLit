// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "CoreMinimal.h"
#include "StateTreeExecutionContext.h"
#include "BlueprintNodeHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeConditionBlueprintBase)

//----------------------------------------------------------------------//
//  UStateTreeConditionBlueprintBase
//----------------------------------------------------------------------//

UStateTreeConditionBlueprintBase::UStateTreeConditionBlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasTestCondition = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTestCondition"), *this, *StaticClass());
}

bool UStateTreeConditionBlueprintBase::TestCondition(FStateTreeExecutionContext& Context) const
{
	if (bHasTestCondition)
	{
		FScopedCurrentContext(*this, Context);
		return ReceiveTestCondition();
	}
	return false;
}

//----------------------------------------------------------------------//
//  FStateTreeBlueprintConditionWrapper
//----------------------------------------------------------------------//

bool FStateTreeBlueprintConditionWrapper::TestCondition(FStateTreeExecutionContext& Context) const
{
	UStateTreeConditionBlueprintBase* Condition = Context.GetInstanceDataPtr<UStateTreeConditionBlueprintBase>(*this);
	check(Condition);
	return Condition->TestCondition(Context);
}


