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

enum EActorReplicationBridgeSpawnInfoFlags : uint32
{
	EActorReplicationBridgeSpawnInfoFlags_QuantizeScale = 1U,
	EActorReplicationBridgeSpawnInfoFlags_QuantizeLocation = EActorReplicationBridgeSpawnInfoFlags_QuantizeScale << 1U,
	EActorReplicationBridgeSpawnInfoFlags_QuantizeVelocity = EActorReplicationBridgeSpawnInfoFlags_QuantizeLocation << 1U,
	EActorReplicationBridgeSpawnInfoFlags_BitCount = 3U,
};

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
	uint8 bOuterIsTransientLevel : 1;	// When set the OuterReference was not sent because the Outer is the default transient level.
	uint8 bOuterIsRootObject : 1;		// When set the OuterReference was not sent because the Outer is the known RootObject.
	FNetObjectReference OuterReference; // Optional: Outer ref sent only for dynamic subobjects.
};

const FActorReplicationBridgeSpawnInfo& GetDefaultActorReplicationBridgeSpawnInfo();

void WriteActorReplicationBridgeSpawnInfo(FNetSerializationContext& Context, const FActorReplicationBridgeSpawnInfo& SpawnInfo, uint32 Flags);
void ReadActorReplicationBridgeSpawnInfo(FNetSerializationContext& Context, FActorReplicationBridgeSpawnInfo& SpawnInfo);

void WriteActorCreationHeader(FNetSerializationContext& Context, const FActorCreationHeader& Header, uint32 Flags);
void ReadActorCreationHeader(FNetSerializationContext& Context, FActorCreationHeader& Header);

void WriteSubObjectCreationHeader(FNetSerializationContext& Context, const FSubObjectCreationHeader& Header);
void ReadSubObjectCreationHeader(FNetSerializationContext& Context, FSubObjectCreationHeader& Header); 

uint32 GetActorReplicationBridgeSpawnInfoFlags();

}

#endif // UE_WITH_IRIS
