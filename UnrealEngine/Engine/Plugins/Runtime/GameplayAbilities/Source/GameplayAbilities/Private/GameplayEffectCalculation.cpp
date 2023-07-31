// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayEffectCalculation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayEffectCalculation)

UGameplayEffectCalculation::UGameplayEffectCalculation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

const TArray<FGameplayEffectAttributeCaptureDefinition>& UGameplayEffectCalculation::GetAttributeCaptureDefinitions() const
{
	return RelevantAttributesToCapture;
}

