// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/GameMode.h"
#include "Engine/Engine.h"
#include "NetworkPredictionExtrasGameMode.generated.h"

UCLASS(BlueprintType)
class ANetworkPredictionExtrasGameMode : public AGameMode
{
	GENERATED_BODY()

	ANetworkPredictionExtrasGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool AllowCheats(APlayerController* P) { return true; }
};