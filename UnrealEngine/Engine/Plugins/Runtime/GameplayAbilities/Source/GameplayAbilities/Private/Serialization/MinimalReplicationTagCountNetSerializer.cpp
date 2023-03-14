// Copyright Epic Games, Inc. All Rights Reserved.
#if UE_WITH_IRIS

#include "GameplayEffectTypes.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetSerializerDelegates.h"

namespace UE::Net
{

// Until we implement a proper NetSerializer JIRA:UE-161669 we fall back on the LastResortNetSerializer
static const FName PropertyNetSerializerRegistry_NAME_MinimalReplicationTagCountMap("MinimalReplicationTagCountMap");
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_MinimalReplicationTagCountMap);

struct FMinimalReplicationTagCountMapNetSerializerRegistryDelegates : protected FNetSerializerRegistryDelegates
{
	~FMinimalReplicationTagCountMapNetSerializerRegistryDelegates();
	virtual void OnPreFreezeNetSerializerRegistry();
};
static FMinimalReplicationTagCountMapNetSerializerRegistryDelegates MinimalReplicationTagCountMapNetSerializerRegistryDelegates;

FMinimalReplicationTagCountMapNetSerializerRegistryDelegates::~FMinimalReplicationTagCountMapNetSerializerRegistryDelegates()
{
	UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_MinimalReplicationTagCountMap);
}

void FMinimalReplicationTagCountMapNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
{
	UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_MinimalReplicationTagCountMap);
}

}

#endif

