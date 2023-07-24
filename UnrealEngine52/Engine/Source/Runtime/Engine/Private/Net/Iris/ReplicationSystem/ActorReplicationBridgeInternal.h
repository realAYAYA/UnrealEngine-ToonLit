// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_WITH_IRIS

#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"

namespace UE::Net
{
	class FNetSerializationContext;
}

namespace UE::Net::Private
{

struct FActorReplicationBridgeSpawnInfo
{
	FVector Location;
	FRotator Rotation;
	FVector Scale;
	FVector Velocity;
};

struct FActorReplicationBridgeCreationHeader : public UObjectReplicationBridge::FCreationHeader
{
	FNetObjectReference ObjectReference;
	uint8 bIsActor : 1;
	uint8 bIsDynamic : 1;
	uint8 bIsNameStableForNetworking : 1;
	uint8 bUsePersistentLevel : 1;
};

struct FActorCreationHeader : public FActorReplicationBridgeCreationHeader
{
	FActorReplicationBridgeSpawnInfo SpawnInfo;
	FNetObjectReference ArchetypeReference;
	FNetObjectReference LevelReference;
	TArray<uint8> CustomCreationData;
	uint16 CustomCreationDataBitCount;
};

struct FSubObjectCreationHeader : public FActorReplicationBridgeCreationHeader
{
	FNetObjectReference ObjectClassReference;
};

const FActorReplicationBridgeSpawnInfo& GetDefaultActorReplicationBridgeSpawnInfo();

void WriteActorReplicationBridgeSpawnInfo(FNetSerializationContext& Context, const FActorReplicationBridgeSpawnInfo& SpawnInfo);
void ReadActorReplicationBridgeSpawnInfo(FNetSerializationContext& Context, FActorReplicationBridgeSpawnInfo& SpawnInfo);

void WriteActorCreationHeader(FNetSerializationContext& Context, const FActorCreationHeader& Header);
void ReadActorCreationHeader(FNetSerializationContext& Context, FActorCreationHeader& Header);

void WriteSubObjectCreationHeader(FNetSerializationContext& Context, const FSubObjectCreationHeader& Header);
void ReadSubObjectCreationHeader(FNetSerializationContext& Context, FSubObjectCreationHeader& Header); 

}

#endif // UE_WITH_IRIS
