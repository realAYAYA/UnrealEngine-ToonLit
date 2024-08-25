// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/ChaosVDSceneQueryDataContainer.h"

#include "Components/ChaosVDSceneQueryDataComponent.h"

AChaosVDSceneQueryDataContainer::AChaosVDSceneQueryDataContainer()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneQueryDataComponent = CreateDefaultSubobject<UChaosVDSceneQueryDataComponent>(TEXT("ChaosVDSceneQueryDataComponent"));
}
