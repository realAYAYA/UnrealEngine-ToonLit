// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/InternalNetSerializerUtils.h"

namespace UE::Net
{

bool IsObjectReferenceNetSerializer(const FNetSerializer* NetSerializer)
{
	bool bIsObjectReferenceNetSerializer = false;
	bIsObjectReferenceNetSerializer = bIsObjectReferenceNetSerializer || (NetSerializer == &UE_NET_GET_SERIALIZER(FObjectNetSerializer));
	bIsObjectReferenceNetSerializer = bIsObjectReferenceNetSerializer || (NetSerializer == &UE_NET_GET_SERIALIZER(FWeakObjectNetSerializer));
	bIsObjectReferenceNetSerializer = bIsObjectReferenceNetSerializer || (NetSerializer == &UE_NET_GET_SERIALIZER(FScriptInterfaceNetSerializer));
	return bIsObjectReferenceNetSerializer;
}

}

