// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ActorFactories/ActorFactory.h"
#include "WaterBodyIslandActorFactory.generated.h"

UCLASS(MinimalAPI, config = Editor)
class UWaterBodyIslandActorFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
};

