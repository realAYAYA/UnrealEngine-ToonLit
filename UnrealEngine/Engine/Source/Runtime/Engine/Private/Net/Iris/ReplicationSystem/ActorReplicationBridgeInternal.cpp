// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorReplicationBridgeInternal.h"

#if UE_WITH_IRIS

#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/Serialization/PackedVectorNetSerializers.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
#include "HAL/IConsoleManager.h"

namespace UE::Net::Private
{

static const FActorReplicationBridgeSpawnInfo DefaultActorReplicationBridgeSpawnInfo = {FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, FVector::ZeroVector};

const FActorReplicationBridgeSpawnInfo& GetDefaultActorReplicationBridgeSpawnInfo()
{
	return DefaultActorReplicationBridgeSpawnInfo;
}

// Write vector using default value compression using specified serializer
static void InternalWriteConditionallyQuantizedVector(FNetBitStreamWriter* Writer, const FVector& Vector, const FVector& DefaultValue, bool bQuantize)
{
	// We use 0.01f for comparing when using quantization, because we will only send a single point of precision anyway.
	// We could probably get away with 0.1f, but that may introduce edge cases for rounding.
	static constexpr float Epsilon_Quantized = 0.01f;
				
	// We use KINDA_SMALL_NUMBER for comparing when not using quantization, because that's the default for FVector::Equals.
	const float Epsilon = bQuantize ? Epsilon_Quantized : UE_KINDA_SMALL_NUMBER;
	
	if (Writer->WriteBool(!Vector.Equals(DefaultValue, Epsilon)))
	{
		Writer->WriteBool(bQuantize);

		const FNetSerializer& Serializer = bQuantize ? UE_NET_GET_SERIALIZER(FVectorNetQuantize10NetSerializer) : UE_NET_GET_SERIALIZER(FVectorNetSerializer);

		FNetSerializationContext Context(Writer);

		alignas(16) uint8 QuantizedState[32] = {};
		checkSlow(sizeof(QuantizedState) >= Serializer.QuantizedTypeSize);

		FNetQuantizeArgs QuantizeArgs;
		QuantizeArgs.Version = Serializer.Version;
		QuantizeArgs.Source = NetSerializerValuePointer(&Vector);
		QuantizeArgs.Target = NetSerializerValuePointer(&QuantizedState[0]);
		QuantizeArgs.NetSerializerConfig = NetSerializerConfigParam(Serializer.DefaultConfig);
		Serializer.Quantize(Context, QuantizeArgs);

		FNetSerializeArgs SerializeArgs;
		SerializeArgs.Version = Serializer.Version;
		SerializeArgs.Source = QuantizeArgs.Target;
		SerializeArgs.NetSerializerConfig = NetSerializerConfigParam(Serializer.DefaultConfig);
		Serializer.Serialize(Context, SerializeArgs);
	}
}

// Read default value compressed vector using specified serializer
static void InternalReadConditionallyQuantizedVector(FNetBitStreamReader* Reader, FVector& OutVector, const FVector& DefaultValue)
{
	if (Reader->ReadBool())
	{
		const bool bIsQuantized = Reader->ReadBool();

		const FNetSerializer& Serializer = bIsQuantized ? UE_NET_GET_SERIALIZER(FVectorNetQuantize10NetSerializer) : UE_NET_GET_SERIALIZER(FVectorNetSerializer);

		FNetSerializationContext Context(Reader);

		alignas(16) uint8 QuantizedState[32] = {};
		checkSlow(sizeof(QuantizedState) >= Serializer.QuantizedTypeSize);

		FNetDeserializeArgs DeserializeArgs;
		DeserializeArgs.Version = Serializer.Version;
		DeserializeArgs.Target = NetSerializerValuePointer(&QuantizedState[0]);
		DeserializeArgs.NetSerializerConfig = NetSerializerConfigParam(Serializer.DefaultConfig);
		Serializer.Deserialize(Context, DeserializeArgs);

		FNetDequantizeArgs DequantizeArgs;
		DequantizeArgs.Version = Serializer.Version;
		DequantizeArgs.Source = DeserializeArgs.Target;
		DequantizeArgs.Target = NetSerializerValuePointer(&OutVector);
		DequantizeArgs.NetSerializerConfig = NetSerializerConfigParam(Serializer.DefaultConfig);
		Serializer.Dequantize(Context, DequantizeArgs);
	}
	else
	{
		OutVector = DefaultValue;
	}
}

/*
 * IRIS: TODO: As soon as we have added code generation for Serialization code this will no longer be required
 */
void WriteActorReplicationBridgeSpawnInfo(FNetSerializationContext& Context, const FActorReplicationBridgeSpawnInfo& SpawnInfo, uint32 Flags)
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	InternalWriteConditionallyQuantizedVector(Writer, SpawnInfo.Location, DefaultActorReplicationBridgeSpawnInfo.Location, (Flags & EActorReplicationBridgeSpawnInfoFlags_QuantizeLocation) != 0U);
	InternalWriteConditionallyQuantizedVector(Writer, SpawnInfo.Scale, DefaultActorReplicationBridgeSpawnInfo.Scale, (Flags & EActorReplicationBridgeSpawnInfoFlags_QuantizeScale) != 0U);
	InternalWriteConditionallyQuantizedVector(Writer, SpawnInfo.Velocity, DefaultActorReplicationBridgeSpawnInfo.Velocity, (Flags & EActorReplicationBridgeSpawnInfoFlags_QuantizeVelocity) != 0U);

	// For rotation we use 0.001f for Rotation comparison to keep consistency with old behavior.
	static constexpr float RotationEpsilon = 0.001f;
	WriteRotator(Writer, SpawnInfo.Rotation, DefaultActorReplicationBridgeSpawnInfo.Rotation, RotationEpsilon);
}

void ReadActorReplicationBridgeSpawnInfo(FNetSerializationContext& Context, FActorReplicationBridgeSpawnInfo& SpawnInfo)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	InternalReadConditionallyQuantizedVector(Reader, SpawnInfo.Location, DefaultActorReplicationBridgeSpawnInfo.Location);
	InternalReadConditionallyQuantizedVector(Reader, SpawnInfo.Scale, DefaultActorReplicationBridgeSpawnInfo.Scale);
	InternalReadConditionallyQuantizedVector(Reader, SpawnInfo.Velocity, DefaultActorReplicationBridgeSpawnInfo.Velocity);

	ReadRotator(Reader, SpawnInfo.Rotation, DefaultActorReplicationBridgeSpawnInfo.Rotation);
}

void WriteActorCreationHeader(FNetSerializationContext& Context, const FActorCreationHeader& Header, uint32 Flags)
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
		WriteActorReplicationBridgeSpawnInfo(Context, Header.SpawnInfo, Flags);
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

			if (!Writer->WriteBool(Header.bOuterIsTransientLevel))
			{
				if (!Writer->WriteBool(Header.bOuterIsRootObject))
				{
					WriteFullNetObjectReference(Context, Header.OuterReference);
				}
			}
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
	Header.bOuterIsTransientLevel = false;
	Header.bOuterIsRootObject = false;

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

			Header.bOuterIsTransientLevel = Reader->ReadBool();
			if (!Header.bOuterIsTransientLevel)
			{
				Header.bOuterIsRootObject = Reader->ReadBool();
				if (!Header.bOuterIsRootObject)
				{
					ReadFullNetObjectReference(Context, Header.OuterReference);
				}
			}
		}
	}
	else
	{
		ReadFullNetObjectReference(Context, Header.ObjectReference);
	}
}

uint32 GetActorReplicationBridgeSpawnInfoFlags()
{
	// Init spawninfo flags from CVARs
	uint32 Flags = 0U;
	{
		bool bQuantizeActorScaleOnSpawn = false;
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.QuantizeActorScaleOnSpawn"));
		if (ensure(CVar))
		{
			bQuantizeActorScaleOnSpawn = CVar->GetBool();
		}
		Flags |= bQuantizeActorScaleOnSpawn ? EActorReplicationBridgeSpawnInfoFlags_QuantizeScale : 0U;
	}

	{
		bool bQuantizeActorLocationOnSpawn = true;
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.QuantizeActorLocationOnSpawn"));
		if (ensure(CVar))
		{
			bQuantizeActorLocationOnSpawn = CVar->GetBool();
		}
		Flags |= bQuantizeActorLocationOnSpawn ? EActorReplicationBridgeSpawnInfoFlags_QuantizeLocation : 0U;
	}

	{
		bool bQuantizeActorVelocityOnSpawn = true;
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.QuantizeActorVelocityOnSpawn"));
		if (ensure(CVar))
		{
			bQuantizeActorVelocityOnSpawn = CVar->GetBool();
		}
		Flags |= bQuantizeActorVelocityOnSpawn ? EActorReplicationBridgeSpawnInfoFlags_QuantizeVelocity : 0U;
	}

	return Flags;
}

}

#endif // UE_WITH_IRIS
