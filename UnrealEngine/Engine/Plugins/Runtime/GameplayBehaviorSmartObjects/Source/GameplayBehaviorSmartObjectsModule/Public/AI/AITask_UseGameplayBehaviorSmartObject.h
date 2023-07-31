// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/AITask.h"
#include "SmartObjectRuntime.h"
#include "AITask_UseGameplayBehaviorSmartObject.generated.h"


class AAIController;
class UAITask_MoveTo;
class UGameplayBehavior;
class USmartObjectComponent;

UCLASS()
class GAMEPLAYBEHAVIORSMARTOBJECTSMODULE_API UAITask_UseGameplayBehaviorSmartObject : public UAITask
{
	GENERATED_BODY()

public:
	UAITask_UseGameplayBehaviorSmartObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (DefaultToSelf = "Controller" , BlueprintInternalUseOnly = "true"))
	static UAITask_UseGameplayBehaviorSmartObject* UseGameplayBehaviorSmartObject(AAIController* Controller, AActor* SmartObjectActor, USmartObjectComponent* SmartObjectComponent, bool bLockAILogic = true);

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (DefaultToSelf = "Controller" , BlueprintInternalUseOnly = "true"))
	static UAITask_UseGameplayBehaviorSmartObject* UseClaimedGameplayBehaviorSmartObject(AAIController* Controller, FSmartObjectClaimHandle ClaimHandle, bool bLockAILogic = true);

	void SetClaimHandle(const FSmartObjectClaimHandle& Handle);

	virtual void TickTask(float DeltaTime) override;

protected:
	virtual void Activate() override;

	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;

	virtual void OnDestroy(bool bInOwnerFinished) override;

	void Abort();

	void OnSmartObjectBehaviorFinished(UGameplayBehavior& Behavior, AActor& Avatar, const bool bInterrupted);

	void OnSlotInvalidated(const FSmartObjectClaimHandle& ClaimHandle, const ESmartObjectSlotState State);

	static UAITask_UseGameplayBehaviorSmartObject* UseSmartObjectComponent(AAIController& Controller, const USmartObjectComponent& SmartObjectComponent, bool bLockAILogic);
	static UAITask_UseGameplayBehaviorSmartObject* UseClaimedSmartObject(AAIController& Controller, FSmartObjectClaimHandle ClaimHandle, bool bLockAILogic);

protected:
	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnSucceeded;

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnFailed;

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnMoveToFailed;

	UPROPERTY()
	TObjectPtr<UAITask_MoveTo> MoveToTask;

	UPROPERTY()
	TObjectPtr<UGameplayBehavior> GameplayBehavior;

	FSmartObjectClaimHandle ClaimedHandle;
	FDelegateHandle OnBehaviorFinishedNotifyHandle;
	uint32 bBehaviorFinished : 1;
};
