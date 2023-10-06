// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "GameplayAbilityRepAnimMontageNetSerializer.generated.h"

USTRUCT()
struct FGameplayAbilityRepAnimMontageNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER(FGameplayAbilityRepAnimMontageNetSerializer, GAMEPLAYABILITIES_API);

}
