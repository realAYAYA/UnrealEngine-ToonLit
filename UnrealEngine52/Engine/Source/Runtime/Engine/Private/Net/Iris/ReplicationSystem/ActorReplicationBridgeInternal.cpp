// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorReplicationBridgeInternal.h"

#if UE_WITH_IRIS

#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/ObjectNetSerializer.h"

namespace UE::Net::Private
{

static const FActorReplicationBridgeSpawnInfo DefaultActorReplicationBridgeSpawnInfo = {FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, FVector::ZeroVector};

const FActorReplicationBridgeSpawnInfo& GetDefaultActorReplicationBridgeSpawnInfo()
{
	return DefaultActorReplicationBridgeSpawnInfo;
}

/*
 * IRIS: TODO: As soon as we have added code generation for Serialization code this will no longer be required
 */
void WriteActorReplicationBridgeSpawnInfo(FNetSerializationContext& Context, const FActorReplicationBridgeSpawnInfo& SpawnInfo)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	constexpr float epsilon = 0.001f;

	WriteVector(Writer, SpawnInfo.Location, DefaultActorReplicationBridgeSpawnInfo.Location, epsilon);
	WriteRotator(Writer, SpawnInfo.Rotation, DefaultActorReplicationBridgeSpawnInfo.Rotation, epsilon);
	WriteVector(Writer, SpawnInfo.Scale, DefaultActorReplicationBridgeSpawnInfo.Scale, epsilon);
	WriteVector(Writer, SpawnInfo.Velocity, DefaultActorReplicationBridgeSpawnInfo.Velocity, epsilon);
}

void ReadActorReplicationBridgeSpawnInfo(FNetSerializationContext& Context, FActorReplicationBridgeSpawnInfo& SpawnInfo)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	ReadVector(Reader, SpawnInfo.Location, DefaultActorReplicationBridgeSpawnInfo.Location);
	ReadRotator(Reader, SpawnInfo.Rotation, DefaultActorReplicationBridgeSpawnInfo.Rotation);
	ReadVector(Reader, SpawnInfo.Scale, DefaultActorReplicationBridgeSpawnInfo.Scale);
	ReadVector(Reader, SpawnInfo.Velocity, DefaultActorReplicationBridgeSpawnInfo.Velocity);
}

void WriteActorCreationHeader(FNetSerializationContext& Context, const FActorCreationHeader& Header)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	if (Writer->WriteBool(Header.bIsDynamic))
	{
		// Write Archetype and LevelPath
		WriteFullNetObjectReference(Context, Header.ArchetypeReference);
		
		// Only write the LevelPath if it differs from the persistent level
		if (!Writer->WriteBool(Header.bUsePersistentLevel))
		{
			WriteFullNetObjectReference(Context, Header.LevelReference);
		}
		
		// Write actor spawn info
		WriteActorReplicationBridgeSpawnInfo(Context, Header.SpawnInfo);
	}
	else // should be possible to refer by path??
	{
		WriteFullNetObjectReference(Context, Header.ObjectReference);
	}

	if (Writer->WriteBool(Header.CustomCreationDataBitCount > 0))
	{
		Writer->WriteBits(Header.CustomCreationDataBitCount - 1U, 16U);
		Writer->WriteBitStream(reinterpret_cast<const uint32*>(Header.CustomCreationData.GetData()), 0U, Header.CustomCreationDataBitCount);
	}
}

void ReadActorCreationHeader(FNetSerializationContext& Context, FActorCreationHeader& Header)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	// Dynamic actor?
	Header.bIsActor = true;
	Header.bIsDynamic = Reader->ReadBool();

	if (Header.bIsDynamic)
	{
		// Read Archetype
		ReadFullNetObjectReference(Context, Header.ArchetypeReference);
		
		Header.bUsePersistentLevel = Reader->ReadBool();
		if (!Header.bUsePersistentLevel)
		{
			ReadFullNetObjectReference(Context, Header.LevelReference);
		}

	 	// Read actor spawn info
		ReadActorReplicationBridgeSpawnInfo(Context, Header.SpawnInfo);
	}
	else
	{
		ReadFullNetObjectReference(Context, Header.ObjectReference);
	}

	Header.CustomCreationDataBitCount = 0;
	if (Reader->ReadBool())
	{
		Header.CustomCreationDataBitCount = 1U + Reader->ReadBits(16U);
		Header.CustomCreationData.SetNumZeroed(((Header.CustomCreationDataBitCount + 31U) & ~31U) >> 3U);
		Reader->ReadBitStream(reinterpret_cast<uint32*>(Header.CustomCreationData.GetData()), Header.CustomCreationDataBitCount);
	}
}

void WriteSubObjectCreationHeader(FNetSerializationContext& Context, const FSubObjectCreationHeader& Header)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	// Write required references to either find or instantiate the subobject
	if (Writer->WriteBool(Header.bIsDynamic))
	{
		if (Writer->WriteBool(Header.bIsNameStableForNetworking))
		{
			WriteFullNetObjectReference(Context, Header.ObjectReference);
		}
		else
		{
			WriteFullNetObjectReference(Context, Header.ObjectClassReference);
		}
	}
	else
	{
		WriteFullNetObjectReference(Context, Header.ObjectReference);
	}
}

void ReadSubObjectCreationHeader(FNetSerializationContext& Context, FSubObjectCreationHeader& Header)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	Header.bIsActor = false;
	Header.bIsDynamic = Reader->ReadBool();
	if (Header.bIsDynamic)
	{
		Header.bIsNameStableForNetworking = Reader->ReadBool();
		if (Header.bIsNameStableForNetworking)
		{
			ReadFullNetObjectReference(Context, Header.ObjectReference);
		}
		else
		{
			ReadFullNetObjectReference(Context, Header.ObjectClassReference);
		}
	}
	else
	{
		ReadFullNetObjectReference(Context, Header.ObjectReference);
	}
}

}

#endif // UE_WITH_IRIS
