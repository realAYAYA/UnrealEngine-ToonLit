// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetSerializer.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "UObject/ObjectMacros.h"

#include "ObjectNetSerializer.generated.h"

namespace UE::Net
{
	class FNetObjectReference;
}

USTRUCT()
struct FObjectNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

USTRUCT()
struct FWeakObjectNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

USTRUCT()
struct FScriptInterfaceNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UClass> InterfaceClass = nullptr;
};

namespace UE::Net
{

IRISCORE_API FNetRefHandle ReadNetRefHandle(FNetSerializationContext& Context);
IRISCORE_API void WriteNetRefHandle(FNetSerializationContext& Context, const FNetRefHandle Handle);

IRISCORE_API void ReadFullNetObjectReference(FNetSerializationContext& Context, FNetObjectReference& Reference);
IRISCORE_API void WriteFullNetObjectReference(FNetSerializationContext& Context, const FNetObjectReference& Reference);

UE_NET_DECLARE_SERIALIZER(FObjectNetSerializer, IRISCORE_API);
UE_NET_DECLARE_SERIALIZER(FWeakObjectNetSerializer, IRISCORE_API);
UE_NET_DECLARE_SERIALIZER(FScriptInterfaceNetSerializer, IRISCORE_API);

}

