// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetSerializer.h"
#include "NetSerializerConfig.h"
#include "UObject/ObjectMacros.h"
#include "DateTimeNetSerializer.generated.h"

USTRUCT()
struct FDateTimeNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER(FDateTimeNetSerializer, IRISCORE_API);

}
