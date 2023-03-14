// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionContext.h"
#include "Tasks/AITask.h"
#include "SmartObjectRuntime.h"
#include "AITask_UseGameplayInteraction.generated.h"

class AAIController;
class UGameplayBehavior;
class USmartObjectComponent;

UCLASS()
class GAMEPLAYINTERACTIONSMODULE_API UAITask_UseGameplayInteraction : public UAITask
{
	GENERATED_BODY()

public:
	explicit UAITask_UseGameplayInteraction(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (DefaultToSelf = "Controller" , BlueprintInternalUseOnly = "true"))
	static UAITask_UseGameplayInteraction* UseClaimedGameplayInteractionSmartObject(AAIController* Controller, FSmartObjectClaimHandle ClaimHandle, bool bLockAILogic = true);

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks")
	void RequestAbort();
	
	void SetClaimHandle(const FSmartObjectClaimHandle& Handle) { ClaimedHandle = Handle; }

protected:
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

	void OnSlotInvalidated(const FSmartObjectClaimHandle& ClaimHandle, const ESmartObjectSlotState State);

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnFinished;

	UPROPERTY()
	FGameplayInteractionContext GameplayInteractionContext;
	
	UPROPERTY()
	FSmartObjectClaimHandle ClaimedHandle;

	UPROPERTY()
	FGameplayInteractionAbortContext AbortContext;
};
