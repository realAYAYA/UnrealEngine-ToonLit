// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionExtrasGameMode.h"
#include "NetworkPredictionExtrasGameState.h"

ANetworkPredictionExtrasGameMode::ANetworkPredictionExtrasGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = ANetworkPredictionExtrasGameState::StaticClass();
}