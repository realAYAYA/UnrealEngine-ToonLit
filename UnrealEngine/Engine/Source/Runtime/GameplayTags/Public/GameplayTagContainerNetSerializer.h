// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "GameplayTagContainerNetSerializer.generated.h"

USTRUCT()
struct FGameplayTagContainerNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER(FGameplayTagContainerNetSerializer, GAMEPLAYTAGS_API);

}
