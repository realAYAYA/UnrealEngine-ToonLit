// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "GameplayEffectContextNetSerializer.generated.h"

USTRUCT()
struct FGameplayEffectContextNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace UE::Net
{

constexpr SIZE_T GetGameplayEffectContextNetSerializerSafeQuantizedSize() { return 464; }

UE_NET_DECLARE_SERIALIZER(FGameplayEffectContextNetSerializer, GAMEPLAYABILITIES_API);

}
