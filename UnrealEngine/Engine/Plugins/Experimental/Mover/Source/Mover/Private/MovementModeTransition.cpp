// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovementModeTransition.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(MovementModeTransition)

const FTransitionEvalResult FTransitionEvalResult::NoTransition = FTransitionEvalResult();

UBaseMovementModeTransition::UBaseMovementModeTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBaseMovementModeTransition::DoRegister()
{
	auto IsImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	};

	static FName EvaluateFuncName = FName(TEXT("K2_OnEvaluate"));
	UFunction* EvaluateFunction = GetClass()->FindFunctionByName(EvaluateFuncName);
	bHasBlueprintEvaluate = IsImplementedInBlueprint(EvaluateFunction);

	static FName TriggerFuncName = FName(TEXT("K2_OnTrigger"));
	UFunction* TriggerFunction = GetClass()->FindFunctionByName(TriggerFuncName);
	bHasBlueprintTrigger = IsImplementedInBlueprint(TriggerFunction);

}

void UBaseMovementModeTransition::DoUnregister()
{
}

FTransitionEvalResult UBaseMovementModeTransition::DoEvaluate(const FSimulationTickParams& Params) const
{
	if (bHasBlueprintEvaluate)
	{
		return K2_OnEvaluate(Params);
	}
	else
	{
		return OnEvaluate(Params);
	}
}

FTransitionEvalResult UBaseMovementModeTransition::OnEvaluate(const FSimulationTickParams& Params) const
{
	return FTransitionEvalResult::NoTransition;
}


void UBaseMovementModeTransition::DoTrigger(const FSimulationTickParams& Params)
{
	if (bHasBlueprintTrigger)
	{
		return K2_OnTrigger(Params);
	}
	else
	{
		return OnTrigger(Params);
	}
}

void UBaseMovementModeTransition::OnTrigger(const FSimulationTickParams& Params)
{
}


#if WITH_EDITOR
EDataValidationResult UBaseMovementModeTransition::IsDataValid(FDataValidationContext& Context) const
{
	return EDataValidationResult::Valid;
}
#endif // WITH_EDITOR
