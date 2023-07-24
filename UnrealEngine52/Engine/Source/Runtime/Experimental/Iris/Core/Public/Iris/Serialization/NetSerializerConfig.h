// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "NetSerializerConfig.generated.h"

enum class ENetSerializerConfigTraits : uint32
{
	None = 0,
	NeedDestruction = 1 << 0,
};
ENUM_CLASS_FLAGS(ENetSerializerConfigTraits);

USTRUCT()
struct FNetSerializerConfig
{
	GENERATED_BODY()

	FNetSerializerConfig()
	: ConfigTraits(ENetSerializerConfigTraits::None)
	{}

	IRISCORE_API virtual ~FNetSerializerConfig();

	ENetSerializerConfigTraits ConfigTraits;
};
