// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "PolymorphicNetSerializer.generated.h"

// Stub used when we are not compiling with iris to workaround UHT dependencies
static_assert(UE_WITH_IRIS == 0, "IrisStub module should not be used when iris is enabled");

class UScriptStruct;
struct FReplicationStateDescriptor;

struct FPolymorphicNetSerializerScriptStructCache
{
};

USTRUCT()
struct FPolymorphicStructNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

USTRUCT()
struct FPolymorphicArrayStructNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()

	FPolymorphicNetSerializerScriptStructCache RegisteredTypes;
};
