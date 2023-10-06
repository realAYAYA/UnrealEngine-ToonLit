// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayViewTargetPolicy.h"
#include "FocusFirstPlayerViewTargetPolicy.generated.h"

/**
 * Chooses the first local player controller.
 */
UCLASS()
class VCAMCORE_API UFocusFirstPlayerViewTargetPolicy : public UGameplayViewTargetPolicy
{
	GENERATED_BODY()
public:

	//~ Begin UGameplayViewTargetPolicy Interface
	virtual TArray<APlayerController*> DeterminePlayerControllers_Implementation(const FDeterminePlayerControllersTargetPolicyParams& Params) override;
	//~ End UGameplayViewTargetPolicy Interface
};
