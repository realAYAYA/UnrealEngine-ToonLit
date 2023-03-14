// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayBehavior.h"
#include "Engine/EngineTypes.h"
#include "GameplayBehavior_BehaviorTree.generated.h"

class UBehaviorTree;
class AAIController;

/** NOTE: this behavior works only for AIControlled pawns */
UCLASS()
class GAMEPLAYBEHAVIORSMODULE_API UGameplayBehavior_BehaviorTree : public UGameplayBehavior
{
	GENERATED_BODY()
public:
	UGameplayBehavior_BehaviorTree(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual bool Trigger(AActor& InAvatar, const UGameplayBehaviorConfig* Config = nullptr, AActor* SmartObjectOwner = nullptr) override;
	virtual void EndBehavior(AActor& InAvatar, const bool bInterrupted) override;
	virtual bool NeedsInstance(const UGameplayBehaviorConfig* Config) const override;

	void OnTimerTick();

	UPROPERTY()
	TObjectPtr<UBehaviorTree> PreviousBT;

	UPROPERTY()
	TObjectPtr<AAIController> AIController;

	/** Indicates if BehaviorTree should run only once or in loop. */
	UPROPERTY(EditAnywhere, Category = SmartObject)
	bool bSingleRun = true;
	
	FTimerHandle TimerHandle;
};
