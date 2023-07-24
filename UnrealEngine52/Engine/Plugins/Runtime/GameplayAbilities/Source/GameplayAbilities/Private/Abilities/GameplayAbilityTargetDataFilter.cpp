// Copyright Epic Games, Inc. All Rights Reserved.

#include "Abilities/GameplayAbilityTargetDataFilter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayAbilityTargetDataFilter)

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayTargetDataFilter
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FGameplayTargetDataFilter::InitializeFilterContext(AActor* FilterActor)
{
	SelfActor = FilterActor;
}

