// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionContext.h"
#include "Tasks/AITask.h"
#include "AITask_UseGameplayInteraction.generated.h"

class AAIController;
class UAITask_MoveTo;
class UGameplayBehavior;
class USmartObjectComponent;

UCLASS()
class GAMEPLAYINTERACTIONSMODULE_API UAITask_UseGameplayInteraction : public UAITask
{
	GENERATED_BODY()

public:
	explicit UAITask_UseGameplayInteraction(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Helper function to create an AITask that interacts with the SmartObject slot using the GameplayInteraction definition.
	 * This version will start the interaction on spot so the actor needs to be at the desired position. 
	 * @param Controller The controller that (or its attached pawn if available) will take part to the interaction.
	 * @param ClaimHandle The handle to an already claimed slot.
	 * @param bLockAILogic Indicates if the task adds UAIResource_Logic to the set of Claimed resources 
	 * @return The AITask executing the GameplayInteraction.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (DefaultToSelf = "Controller" , BlueprintInternalUseOnly = "true"))
	static UAITask_UseGameplayInteraction* UseSmartObjectWithGameplayInteraction(AAIController* Controller, FSmartObjectClaimHandle ClaimHandle, bool bLockAILogic = true);

	/**
	 * Helper function to create an AITask that reaches and interacts with the SmartObject slot using the GameplayInteraction definition
	 * @param Controller The controller that will move to the slot location and that will (or its attached pawn if available) take part to the interaction.
	 * @param ClaimHandle The handle to an already claimed slot.
	 * @param bLockAILogic Indicates if the task adds UAIResource_Logic to the set of Claimed resources 
	 * @return The AITask executing the move and the interaction.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (DefaultToSelf = "Controller" , BlueprintInternalUseOnly = "true"))
	static UAITask_UseGameplayInteraction* MoveToAndUseSmartObjectWithGameplayInteraction(AAIController* Controller, FSmartObjectClaimHandle ClaimHandle, bool bLockAILogic = true);
	
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks")
	void RequestAbort();

	void SetShouldReachSlotLocation(const bool bUseMoveTo) { bShouldUseMoveTo = bUseMoveTo; }
	void SetClaimHandle(const FSmartObjectClaimHandle& Handle) { ClaimedHandle = Handle; }

protected:
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

	bool StartInteraction();
	void OnSlotInvalidated(const FSmartObjectClaimHandle& ClaimHandle, const ESmartObjectSlotState State);
	void Abort(EGameplayInteractionAbortReason Reason);

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnFinished;

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnSucceeded;

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnFailed;

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnMoveToFailed;

	UPROPERTY()
	FGameplayInteractionContext GameplayInteractionContext;

	UPROPERTY()
	TObjectPtr<UAITask_MoveTo> MoveToTask;
	
	UPROPERTY()
	FSmartObjectClaimHandle ClaimedHandle;

	UPROPERTY()
	FGameplayInteractionAbortContext AbortContext;

	bool bInteractionCompleted;

	bool bShouldUseMoveTo;
};
