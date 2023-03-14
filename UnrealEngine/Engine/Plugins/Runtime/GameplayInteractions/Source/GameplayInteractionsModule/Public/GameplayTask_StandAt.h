// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "GameplayTask.h"
#include "GameplayActuationState.h"
#include "GameplayActuationStateProvider.h"
#include "GameplayInteractionsTypes.h"
#include "GameplayTask_StandAt.generated.h"

class UCharacterMovementComponent;

UCLASS()
class GAMEPLAYINTERACTIONSMODULE_API UGameplayTask_StandAt : public UGameplayTask, public IGameplayActuationStateProvider
{
	GENERATED_BODY()

public:
	UGameplayTask_StandAt(const FObjectInitializer& ObjectInitializer);

	EGameplayTaskActuationResult GetResult() const { return Result; }
	bool WasSuccessful() const { return Result == EGameplayTaskActuationResult::Succeeded; }

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding,bUseContinuousGoalTracking,ProjectGoalOnNavigation", DefaultToSelf = "Pawn", BlueprintInternalUseOnly = "TRUE", DisplayName = "GP Stand At"))
	static UGameplayTask_StandAt* StandAt(APawn* Pawn, float Duration);

protected:
	/** IGameplayActuationStateProvider */
	virtual FConstStructView GetActuationState() const override
	{
		return FConstStructView::Make(ActuationState);
	}

	/** finish task */
	void FinishTask(const EGameplayTaskActuationResult InResult);
	
	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual void TickTask(float DeltaTime) override;
	
	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
	FGameplayTaskActuationCompleted OnCompleted;

	FGameplayActuationState_Standing ActuationState;

	bool bCompleteCalled = false;

	EGameplayTaskActuationResult Result = EGameplayTaskActuationResult::None;
	float TimeElapsed = 0.0f;
	float Duration = 0.0f;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComponent = nullptr;
};
