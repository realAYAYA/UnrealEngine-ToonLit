// Copyright Epic Games, Inc. All Rights Reserved.

#include "InternalNetSerializers.h"
#include "UObject/CoreNet.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"

namespace UE::Net
{

struct FLastResortPropertyNetSerializer
{
	// Version
	static const uint32 Version = 0;

	// Traits
	static constexpr bool bHasDynamicState = true;

	// Types
	struct FQuantizedType
	{
		// How many bytes the current allocation can hold.
		uint16 ByteCapacity;
		// How many bits are valid
		uint16 BitCount;
		void* Storage;
	};

	// SourceType is unknown
	typedef void SourceType;
	typedef FQuantizedType QuantizedType;
	typedef FLastResortPropertyNetSerializerConfig ConfigType;

	//
	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs& Args);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs& Args);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);

	static void CloneDynamicState(FNetSerializationContext&, const FNetCloneDynamicStateArgs&);
	static void FreeDynamicState(FNetSerializationContext&, const FNetFreeDynamicStateArgs&);

private:
	static void FreeDynamicStateInternal(FNetSerializationContext&, QuantizedType& Value);
	static void GrowDynamicStateInternal(FNetSerializationContext&, QuantizedType& Value, uint16 NewBitCount);
	static void ShrinkDynamicStateInternal(FNetSerializationContext&, QuantizedType& Value, uint16 NewBitCount);
	static void AdjustStorageSize(FNetSerializationContext&, QuantizedType& Value, uint16 NewBitCount);
};
UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FLastResortPropertyNetSerializer);

void FLastResortPropertyNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);

	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	WritePackedUint32(Writer, Value.BitCount);
	if (Value.BitCount > 0)
	{
		Writer->WriteBitStream(static_cast<uint32*>(Value.Storage), 0U, Value.BitCount);
	}
}

void FLastResortPropertyNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Target);
	const uint32 CurrentBitCount = Value.BitCount;

	FNetBitStreamReader* Reader = Context.GetBitStreamReader();
	const uint32 NewBitCount = ReadPackedUint32(Reader);
	if (NewBitCount > 65535U)
	{
		Reader->DoOverflow();
		return;
	}

	AdjustStorageSize(Context, Value, NewBitCount);

	Reader->ReadBitStream(static_cast<uint32*>(Value.Storage), NewBitCount);
}
void FLastResortPropertyNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
	const FProperty* Property = Config->Property.Get();

	// Use the Property serialization and store as binary blob.
	FNetBitWriter Archive(8192);
	Property->NetSerializeItem(Archive, static_cast<UPackageMap*>(nullptr), reinterpret_cast<void*>(Args.Source));

	const uint32 BitCount = Archive.GetNumBits();
	if (BitCount > 65535)
	{
		Context.SetError(GNetError_BitStreamOverflow);
		return;
	}

	QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Target);
	AdjustStorageSize(Context, Value, BitCount);
	if (BitCount > 0)
	{
		FMemory::Memcpy(Value.Storage, Archive.GetData(), (BitCount + 7U)/8U);
	}
}

void FLastResortPropertyNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
	const FProperty* Property = Config->Property.Get();

	QuantizedType& Source = *reinterpret_cast<QuantizedType*>(Args.Source);
	FNetBitReader  Archive(nullptr, static_cast<uint8*>(Source.Storage), Source.BitCount);
	Property->NetSerializeItem(Archive, static_cast<UPackageMap*>(nullptr), reinterpret_cast<void*>(Args.Target));
}

bool FLastResortPropertyNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);
		if (Value0.BitCount != Value1.BitCount)
		{
			return false;
		}
		const bool bIsEqual = FMemory::Memcmp(Value0.Storage, Value1.Storage, Align((Value0.BitCount + 7U)/8U, 4U)) == 0;
		return bIsEqual;
	}
	else
	{
		const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
		const FProperty* Property = Config->Property.Get();

		const void* Value0 = reinterpret_cast<const void*>(Args.Source0);
		const void* Value1 = reinterpret_cast<const void*>(Args.Source1);
		const bool bIsEqual = Property->Identical(Value0, Value1);
		return bIsEqual;
	}
}

void FLastResortPropertyNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
{
	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
	const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);

	constexpr SIZE_T Alignment = 4U;
	const uint32 ByteCount = Align((Source.BitCount + 7U)/8U, 4U);

	void* Storage = nullptr;
	if (ByteCount > 0)
	{
		Storage = Context.GetInternalContext()->Alloc(ByteCount, Alignment);
		FMemory::Memcpy(Storage, Source.Storage, ByteCount);
	}
	Target.ByteCapacity = ByteCount;
	Target.BitCount = Source.BitCount;
	Target.Storage = Storage;
}

void FLastResortPropertyNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
{
	return FreeDynamicStateInternal(Context, *reinterpret_cast<QuantizedType*>(Args.Source));
}

void FLastResortPropertyNetSerializer::FreeDynamicStateInternal(FNetSerializationContext& Context, QuantizedType& Value)
{
	Context.GetInternalContext()->Free(Value.Storage);

	// Clear all info
	Value = QuantizedType();
}

void FLastResortPropertyNetSerializer::GrowDynamicStateInternal(FNetSerializationContext& Context, QuantizedType& Value, uint16 NewBitCount)
{
	checkSlow(NewBitCount > Value.BitCount);

	constexpr SIZE_T Alignment = 4U;
	const uint32 ByteCount = Align((NewBitCount + 7U)/8U, 4U);

	// We don't support delta compression for the unknown contents of the bits so we don't need to copy the old data.
	Context.GetInternalContext()->Free(Value.Storage);

	void* Storage = Context.GetInternalContext()->Alloc(ByteCount, Alignment);

	// Clear the last word to support IsEqual Memcmp optimization.
	const uint32 LastWordIndex = ByteCount/4U - 1U;
	static_cast<uint32*>(Storage)[LastWordIndex] = 0U;

	Value.ByteCapacity = ByteCount;
	Value.BitCount = NewBitCount;
	Value.Storage = Storage;
}

void FLastResortPropertyNetSerializer::AdjustStorageSize(FNetSerializationContext& Context, QuantizedType& Value, uint16 NewBitCount)
{
	const uint32 NewByteCapacity = Align((NewBitCount + 7U)/8U, 4U);
	if (NewByteCapacity == 0)
	{
		// Free everything
		FreeDynamicStateInternal(Context, Value);
	}
	else if (NewByteCapacity > Value.ByteCapacity)
	{
		GrowDynamicStateInternal(Context, Value, NewBitCount);
	}
	// If byte capacity is within the allocated capacity we just update the bit count and clear the last word
	else
	{
		Value.BitCount = NewBitCount;

		// Clear the last word to support IsEqual Memcmp optimization.
		const uint32 LastWordIndex = NewByteCapacity/4U - 1U;
		static_cast<uint32*>(Value.Storage)[LastWordIndex] = 0U;
	}
}

bool InitLastResortPropertyNetSerializerConfigFromProperty(FLastResortPropertyNetSerializerConfig& OutConfig, const FProperty* Property)
{
	OutConfig.Property = TFieldPath<FProperty>(const_cast<FProperty*>(Property));
	return Property != nullptr;
}

}
