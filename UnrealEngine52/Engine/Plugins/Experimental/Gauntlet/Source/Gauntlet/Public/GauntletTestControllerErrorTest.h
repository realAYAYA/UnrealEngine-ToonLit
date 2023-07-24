// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GauntletTestController.h"
#include "GauntletTestControllerErrorTest.generated.h"

UCLASS()
class GAUNTLET_API UGauntletTestControllerErrorTest : public UGauntletTestController
{
	GENERATED_BODY()

protected:

	float			ErrorDelay;
	FString			ErrorType;
	bool			RunOnServer;

	void	OnInit() override;
	void	OnTick(float TimeDelta)		override;
};

