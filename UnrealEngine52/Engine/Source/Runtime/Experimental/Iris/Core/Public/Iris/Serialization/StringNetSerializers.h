// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetSerializer.h"
#include "UObject/ObjectMacros.h"
#include "StringNetSerializers.generated.h"

USTRUCT()
struct FNameNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

USTRUCT()
struct FStringNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER(FNameNetSerializer, IRISCORE_API)
UE_NET_DECLARE_SERIALIZER(FStringNetSerializer, IRISCORE_API)

}

