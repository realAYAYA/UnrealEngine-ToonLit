// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameplayTask.h"
#include "GameplayTask_MoveTo.h"
#include "GameplayTask_PlayContextualAnim.h"
#include "GameplayTask_MoveToContextualAnim.generated.h"

UCLASS()
class GAMEPLAYINTERACTIONSMODULE_API UGameplayTask_MoveToContextualAnim : public UGameplayTask_MoveTo
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category="Gameplay|Tasks", meta = (DisplayName="EnterContextualAnim", BlueprintInternalUseOnly = "TRUE"))
	static UGameplayTask_MoveToContextualAnim* EnterContextualAnim(
		AActor* Interactor
		, const FName InteractorRole
		, AActor* InteractableObject
		, const FName InteractableObjectRole
		, const FName SectionName
		, const FName ExitSectionName
		, const UContextualAnimSceneAsset* SceneAsset
		);


protected:

	virtual void TriggerEndOfPathTransition(const double DistanceToEndOfPath) override;

	UPROPERTY()
	FGameplayActuationState_ContextualAnim NextState;
};
