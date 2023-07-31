// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "PacketHandlerProfileConfig.generated.h"

/**
 * PacketHandler Configuration Profiles based on each NetDriver
 */
UCLASS(config=Engine, PerObjectConfig)
class UPacketHandlerProfileConfig : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(config)
	TArray<FString> Components;
};
