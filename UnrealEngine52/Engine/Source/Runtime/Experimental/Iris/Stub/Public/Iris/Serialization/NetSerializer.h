// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "NetSerializer.generated.h"

// Stub used when we are not compiling with iris to workaround UHT dependencies
static_assert(UE_WITH_IRIS == 0, "IrisStub module should not be used when iris is enabled");

USTRUCT()
struct FNetSerializerConfig
{
	GENERATED_BODY()
};

// For convenience to avoid changing code when we compile without iris
#define UE_NET_DECLARE_SERIALIZER(...)
