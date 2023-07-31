// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "StateTreeExecutionContext.h"
#include "BlueprintNodeHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeEvaluatorBlueprintBase)

//----------------------------------------------------------------------//
//  UStateTreeEvaluatorBlueprintBase
//----------------------------------------------------------------------//

UStateTreeEvaluatorBlueprintBase::UStateTreeEvaluatorBlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasTreeStart = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTreeStart"), *this, *StaticClass());
	bHasTreeStop = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTreeStop"), *this, *StaticClass());
	bHasTick = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTick"), *this, *StaticClass());
}

void UStateTreeEvaluatorBlueprintBase::TreeStart(FStateTreeExecutionContext& Context)
{
	if (bHasTreeStart)
	{
		FScopedCurrentContext(*this, Context);
		ReceiveTreeStart();
	}
}

void UStateTreeEvaluatorBlueprintBase::TreeStop(FStateTreeExecutionContext& Context)
{
	if (bHasTreeStop)
	{
		FScopedCurrentContext(*this, Context);
		ReceiveTreeStop();
	}
}

void UStateTreeEvaluatorBlueprintBase::Tick(FStateTreeExecutionContext& Context, const float DeltaTime)
{
	if (bHasTick)
	{
		FScopedCurrentContext(*this, Context);
		ReceiveTick(DeltaTime);
	}
}

//----------------------------------------------------------------------//
//  FStateTreeBlueprintEvaluatorWrapper
//----------------------------------------------------------------------//

void FStateTreeBlueprintEvaluatorWrapper::TreeStart(FStateTreeExecutionContext& Context) const
{
	UStateTreeEvaluatorBlueprintBase* Instance = Context.GetInstanceDataPtr<UStateTreeEvaluatorBlueprintBase>(*this);
	check(Instance);
	Instance->TreeStart(Context);
}

void FStateTreeBlueprintEvaluatorWrapper::TreeStop(FStateTreeExecutionContext& Context) const
{
	UStateTreeEvaluatorBlueprintBase* Instance = Context.GetInstanceDataPtr<UStateTreeEvaluatorBlueprintBase>(*this);
	check(Instance);
	Instance->TreeStop(Context);
}

void FStateTreeBlueprintEvaluatorWrapper::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UStateTreeEvaluatorBlueprintBase* Instance = Context.GetInstanceDataPtr<UStateTreeEvaluatorBlueprintBase>(*this);
	check(Instance);
	Instance->Tick(Context, DeltaTime);
}

