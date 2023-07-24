// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAudioGameplayCondition.generated.h"

/** Interface for audio gameplay conditions */
UINTERFACE(BlueprintType, MinimalAPI)
class UAudioGameplayCondition : public UInterface
{
	GENERATED_BODY()
};

class IAudioGameplayCondition
{
	GENERATED_BODY()

public:

	/**
	 * Basic condition check
	 * 
	 * @returns true if condition is met
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AudioGameplayCondition")
	AUDIOGAMEPLAY_API bool ConditionMet() const;
	virtual bool ConditionMet_Implementation() const { return false; }
	
	/**
	 * Allows testing a condition against a provided position.
	 *
	 * @param Position - The location to be considered
	 * 
	 * @returns true if condition is met
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AudioGameplayCondition")
	AUDIOGAMEPLAY_API bool ConditionMet_Position(const FVector& Position) const;
	virtual bool ConditionMet_Position_Implementation(const FVector& Position) const { return false; }
};
