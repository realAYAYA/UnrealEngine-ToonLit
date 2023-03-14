// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "PlayMontageStateTreeTask.generated.h"

class UAnimMontage;

USTRUCT()
struct FPlayMontageStateTreeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Context")
	TObjectPtr<AActor> Actor = nullptr;
	
	UPROPERTY()
	float ComputedDuration = 0.0f;

	/** Accumulated time used to stop task if a montage is set */
	UPROPERTY()
	float Time = 0.f;
};


USTRUCT(meta = (DisplayName = "Play Anim Montage"))
struct FPlayMontageStateTreeTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()
	
	typedef FPlayMontageStateTreeTaskInstanceData FInstanceDataType;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UAnimMontage> Montage = nullptr;
};
