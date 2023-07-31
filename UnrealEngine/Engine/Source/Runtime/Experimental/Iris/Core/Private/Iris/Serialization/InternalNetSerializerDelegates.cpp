// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/InternalNetSerializerDelegates.h"

namespace UE::Net::Private
{

void FInternalNetSerializerDelegates::BroadcastPreFreezeNetSerializerRegistry()
{
	FSimpleMulticastDelegate& PreFreezeNetSerializerRegistryDelegate = GetPreFreezeNetSerializerRegistryDelegate();
	PreFreezeNetSerializerRegistryDelegate.Broadcast();
	PreFreezeNetSerializerRegistryDelegate.Clear();
}

void FInternalNetSerializerDelegates::BroadcastPostFreezeNetSerializerRegistry()
{
	FSimpleMulticastDelegate& PostFreezeNetSerializerRegistryDelegate = GetPostFreezeNetSerializerRegistryDelegate();
	PostFreezeNetSerializerRegistryDelegate.Broadcast();
	PostFreezeNetSerializerRegistryDelegate.Clear();
}

FSimpleMulticastDelegate& FInternalNetSerializerDelegates::GetPreFreezeNetSerializerRegistryDelegate()
{
	static FSimpleMulticastDelegate PreFreezeNetSerializerRegistryDelegate;
	return PreFreezeNetSerializerRegistryDelegate;
}

FSimpleMulticastDelegate& FInternalNetSerializerDelegates::GetPostFreezeNetSerializerRegistryDelegate()
{
	static FSimpleMulticastDelegate PostFreezeNetSerializerRegistryDelegate;
	return PostFreezeNetSerializerRegistryDelegate;
}

}
