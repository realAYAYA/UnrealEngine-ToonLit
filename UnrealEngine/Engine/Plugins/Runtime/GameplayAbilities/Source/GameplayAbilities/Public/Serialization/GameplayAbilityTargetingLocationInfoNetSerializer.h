// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "GameplayAbilityTargetingLocationInfoNetSerializer.generated.h"

USTRUCT()
struct FGameplayAbilityTargetingLocationInfoNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER(FGameplayAbilityTargetingLocationInfoNetSerializer, GAMEPLAYABILITIES_API);

}
