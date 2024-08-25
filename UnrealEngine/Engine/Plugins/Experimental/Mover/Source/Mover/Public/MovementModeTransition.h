// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MovementModeTransition.generated.h"

struct FSimulationTickParams;

/** 
 * Results from a movement mode transition evaluation
 */
USTRUCT(BlueprintType)
struct MOVER_API FTransitionEvalResult
{
	GENERATED_BODY()

	FTransitionEvalResult() {}
	FTransitionEvalResult(FName InNextMode) { NextMode = InNextMode; }

	// Mode name that should be transitioned to. NAME_None indicates no transition.
	UPROPERTY(BlueprintReadWrite, Category=Mover)
	FName NextMode = NAME_None;


	static const FTransitionEvalResult NoTransition;
};


/**
 * Base class for all transitions
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class MOVER_API UBaseMovementModeTransition : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	// TODO : add hooks for shared settings, so that transitions can refer to the same settings as the modes without needing to know about the mode's implementation

	void DoRegister();
	void DoUnregister();

	FTransitionEvalResult DoEvaluate(const FSimulationTickParams& Params) const;

	void DoTrigger(const FSimulationTickParams& Params);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

protected:

	virtual FTransitionEvalResult OnEvaluate(const FSimulationTickParams& Params) const;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnEvaluate", meta = (ScriptName = "OnEvaluate"))
	FTransitionEvalResult K2_OnEvaluate(const FSimulationTickParams& Params) const;


	virtual void OnTrigger(const FSimulationTickParams& Params);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnTrigger", meta = (ScriptName = "OnTrigger"))
	void K2_OnTrigger(const FSimulationTickParams& Params);

private:
	bool bHasBlueprintEvaluate = false;
	bool bHasBlueprintTrigger = false;
};




