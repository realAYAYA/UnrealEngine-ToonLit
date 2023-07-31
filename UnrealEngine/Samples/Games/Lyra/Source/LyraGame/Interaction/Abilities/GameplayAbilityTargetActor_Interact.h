// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Engine/HitResult.h"
#include "UObject/UObjectGlobals.h"

#include "GameplayAbilityTargetActor_Interact.generated.h"

class AActor;
class UObject;


/** Intermediate base class for all interaction target actors. */
UCLASS(Blueprintable)
class AGameplayAbilityTargetActor_Interact : public AGameplayAbilityTargetActor_Trace
{
	GENERATED_BODY()

public:
	AGameplayAbilityTargetActor_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;

protected:
};