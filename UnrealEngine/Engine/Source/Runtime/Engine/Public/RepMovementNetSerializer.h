// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "RepMovementNetSerializer.generated.h"

USTRUCT()
struct FRepMovementNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

#if UE_WITH_IRIS

namespace UE::Net
{

UE_NET_DECLARE_SERIALIZER(FRepMovementNetSerializer, ENGINE_API);

}

#endif // UE_WITH_IRIS
