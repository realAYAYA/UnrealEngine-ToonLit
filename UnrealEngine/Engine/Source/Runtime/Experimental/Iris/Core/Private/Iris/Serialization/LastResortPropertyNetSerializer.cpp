// Copyright Epic Games, Inc. All Rights Reserved.

#include "InternalNetSerializers.h"
#include "UObject/CoreNet.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Iris/Serialization/NetSerializerArrayStorage.h"
#include "Iris/Serialization/IrisObjectReferencePackageMap.h"
#include "Net/Core/Trace/NetTrace.h"

namespace UE::Net
{

struct FFLastResortPropertyNetSerializerQuantizedType
{
	static constexpr uint32 MaxInlinedObjectRefs = 4;
	typedef FNetSerializerArrayStorage<FNetObjectReference, AllocationPolicies::TInlinedElementAllocationPolicy<MaxInlinedObjectRefs>> FObjectReferenceStorage;

	FObjectReferenceStorage ObjectReferenceStorage;

	// How many bytes the current allocation can hold.
	uint16 ByteCapacity = 0;
	// How many bits are valid
	uint16 BitCount = 0;
	void* Storage = nullptr;
};

}

template <> struct TIsPODType<UE::Net::FFLastResortPropertyNetSerializerQuantizedType> { enum { Value = true }; };

namespace UE::Net
{

struct FLastResortPropertyNetSerializer
{
	// Version
	static const uint32 Version = 0;

	// Traits
	static constexpr bool bHasDynamicState = true;
	static constexpr bool bHasCustomNetReference = true;

	// Types

	// SourceType is unknown
	typedef void SourceType;
	typedef FFLastResortPropertyNetSerializerQuantizedType QuantizedType;
	typedef FLastResortPropertyNetSerializerConfig ConfigType;

	//
	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs& Args);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs& Args);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);

	static void CloneDynamicState(FNetSerializationContext&, const FNetCloneDynamicStateArgs&);
	static void FreeDynamicState(FNetSerializationContext&, const FNetFreeDynamicStateArgs&);

	static void CollectNetReferences(FNetSerializationContext&, const FNetCollectReferencesArgs&);

private:
	static constexpr uint32 AllocationAlignment = 4U;

	static void FreeDynamicStateInternal(FNetSerializationContext&, QuantizedType& Value);
	static void GrowDynamicStateInternal(FNetSerializationContext&, QuantizedType& Value, uint16 NewBitCount);
	static void ShrinkDynamicStateInternal(FNetSerializationContext&, QuantizedType& Value, uint16 NewBitCount);
	static void AdjustStorageSize(FNetSerializationContext&, QuantizedType& Value, uint16 NewBitCount);

	static inline const FNetSerializer* ObjectNetSerializer = &UE_NET_GET_SERIALIZER(FObjectNetSerializer);
};
UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FLastResortPropertyNetSerializer);

void FLastResortPropertyNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
	const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	const uint32 NumReferences = Value.ObjectReferenceStorage.Num();

	UE_NET_TRACE_DYNAMIC_NAME_SCOPE(Config->Property.Get()->GetName(), *Writer, Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);

	// If we have any references, export them!
	if (Writer->WriteBool(NumReferences != 0))
	{
		UE::Net::WritePackedUint32(Writer, NumReferences);		
		FObjectNetSerializerConfig ObjectSerializerConfig;
		for (const FNetObjectReference& Ref : MakeArrayView(Value.ObjectReferenceStorage.GetData(), NumReferences))
		{
			FNetSerializeArgs ObjectArgs;
			ObjectArgs.NetSerializerConfig = &ObjectSerializerConfig;
			ObjectArgs.Source = NetSerializerValuePointer(&Ref);

			ObjectNetSerializer->Serialize(Context, ObjectArgs);
		}
	}

	WritePackedUint32(Writer, Value.BitCount);
	if (Value.BitCount > 0)
	{
		Writer->WriteBitStream(static_cast<uint32*>(Value.Storage), 0U, Value.BitCount);
	}
}

void FLastResortPropertyNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
	QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Target);
	const uint32 CurrentBitCount = Value.BitCount;

	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	UE_NET_TRACE_DYNAMIC_NAME_SCOPE(Config->Property.Get()->GetName(), *Reader, Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);

	// Read any object references
	const bool bHasObjectReferences = Reader->ReadBool();
	if (bHasObjectReferences)
	{
		const uint32 NumReferences = UE::Net::ReadPackedUint32(Reader);

		if (NumReferences > Config->MaxAllowedObjectReferences)
		{
			Context.SetError(GNetError_ArraySizeTooLarge);
			return;
		}

		Value.ObjectReferenceStorage.AdjustSize(Context, NumReferences);

		FObjectNetSerializerConfig ObjectSerializerConfig;
		for (FNetObjectReference& Ref : MakeArrayView(Value.ObjectReferenceStorage.GetData(), Value.ObjectReferenceStorage.Num()))
		{
			FNetDeserializeArgs ObjectArgs;
			ObjectArgs.NetSerializerConfig = &ObjectSerializerConfig;
			ObjectArgs.Target = NetSerializerValuePointer(&Ref);

			ObjectNetSerializer->Deserialize(Context, ObjectArgs);
		}
	}
	else
	{
		Value.ObjectReferenceStorage.Free(Context);
	}

	const uint32 NewBitCount = ReadPackedUint32(Reader);
	if (NewBitCount > 65535U)
	{
		Context.SetError(GNetError_ArraySizeTooLarge);
		return;
	}

	AdjustStorageSize(Context, Value, static_cast<uint16>(NewBitCount));

	Reader->ReadBitStream(static_cast<uint32*>(Value.Storage), NewBitCount);
}
void FLastResortPropertyNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
	const FProperty* Property = Config->Property.Get();
	QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Target);

	// Capture references
	Private::FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
	UIrisObjectReferencePackageMap* PackageMap = InternalContext ? InternalContext->PackageMap : nullptr;
	
	UIrisObjectReferencePackageMap::FObjectReferenceArray ObjectReferences;
	if (PackageMap)
	{
		PackageMap->InitForWrite(&ObjectReferences);
	}

	// Use the Property serialization and store as binary blob.
	FNetBitWriter Archive(PackageMap, 8192);
	Property->NetSerializeItem(Archive, PackageMap, reinterpret_cast<void*>(Args.Source));

	const uint64 BitCount = Archive.GetNumBits();
	if (BitCount > 65535U)
	{
		Context.SetError(GNetError_ArraySizeTooLarge);
		return;
	}


	// Capture any references
	const uint32 NumObjectReferences = ObjectReferences.Num();
	Value.ObjectReferenceStorage.AdjustSize(Context, NumObjectReferences);
	if (NumObjectReferences > 0)
	{
		FObjectNetSerializerConfig ObjectNetSerializerConfig;
		const TObjectPtr<UObject>* SourceReferences = ObjectReferences.GetData();
		FNetObjectReference* TargetReferences = Value.ObjectReferenceStorage.GetData();
		for (uint32 ReferenceIndex = 0; ReferenceIndex < NumObjectReferences; ++ReferenceIndex)
		{
			FNetQuantizeArgs ObjectArgs;
			ObjectArgs.NetSerializerConfig = &ObjectNetSerializerConfig;
			ObjectArgs.Source = NetSerializerValuePointer(SourceReferences + ReferenceIndex);
			ObjectArgs.Target = NetSerializerValuePointer(TargetReferences + ReferenceIndex);

			ObjectNetSerializer->Quantize(Context, ObjectArgs);
		}
	}

	// Deal with serialized data
	AdjustStorageSize(Context, Value, static_cast<uint16>(BitCount));
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

	// Inject references
	Private::FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
	UIrisObjectReferencePackageMap* PackageMap = InternalContext ? InternalContext->PackageMap : nullptr;
	UIrisObjectReferencePackageMap::FObjectReferenceArray ObjectReferences;

	// References
	const uint32 NumObjectReferences = Source.ObjectReferenceStorage.Num();
	if (NumObjectReferences > 0U && ensureAlwaysMsgf(PackageMap, TEXT("FLastResortPropertyNetSerializer::Dequantize must have a packagemap to be able to dequantize object references. Make sure it is set in the InternalContext.")))
	{		
		ObjectReferences.SetNumUninitialized(NumObjectReferences);

		FObjectNetSerializerConfig ObjectNetSerializerConfig;
		const FNetObjectReference* SourceReferences = Source.ObjectReferenceStorage.GetData();
		TObjectPtr<UObject>* TargetReferences = ObjectReferences.GetData();
		for (uint32 ReferenceIndex = 0; ReferenceIndex < NumObjectReferences; ++ReferenceIndex)
		{
			FNetDequantizeArgs ObjectArgs;
			ObjectArgs.NetSerializerConfig = &ObjectNetSerializerConfig;
			ObjectArgs.Source = NetSerializerValuePointer(SourceReferences + ReferenceIndex);
			ObjectArgs.Target = NetSerializerValuePointer(TargetReferences + ReferenceIndex);

			ObjectNetSerializer->Dequantize(Context, ObjectArgs);
		}

		PackageMap->InitForRead(&ObjectReferences);
	}

	if (Source.BitCount)
	{
		FNetBitReader Archive(PackageMap, static_cast<uint8*>(Source.Storage), Source.BitCount);
		Property->NetSerializeItem(Archive, PackageMap, reinterpret_cast<void*>(Args.Target));
	}
	else
	{
		Property->ClearValue(reinterpret_cast<void*>(Args.Target));
	}
}

bool FLastResortPropertyNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);
		if ((Value0.BitCount != Value1.BitCount) || (Value0.ObjectReferenceStorage.Num() != Value1.ObjectReferenceStorage.Num()))
		{
			return false;
		}

		if (Value0.ObjectReferenceStorage.Num() > 0 && FMemory::Memcmp(Value0.ObjectReferenceStorage.GetData(), Value1.ObjectReferenceStorage.GetData(), sizeof(FNetObjectReference) * Value0.ObjectReferenceStorage.Num()) != 0)
		{
			return false;
		}

		const bool bIsEqual = (Value0.BitCount == 0U) || FMemory::Memcmp(Value0.Storage, Value1.Storage, Align((Value0.BitCount + 7U)/8U, AllocationAlignment)) == 0;
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

	const uint16 ByteCount = static_cast<uint16>(Align((Source.BitCount + 7U)/8U, AllocationAlignment));

	void* Storage = nullptr;
	if (ByteCount > 0)
	{
		Storage = Context.GetInternalContext()->Alloc(ByteCount, AllocationAlignment);
		FMemory::Memcpy(Storage, Source.Storage, ByteCount);
	}
	Target.ByteCapacity = ByteCount;
	Target.BitCount = Source.BitCount;
	Target.Storage = Storage;

	Target.ObjectReferenceStorage.Clone(Context, Source.ObjectReferenceStorage);
}

void FLastResortPropertyNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
{
	return FreeDynamicStateInternal(Context, *reinterpret_cast<QuantizedType*>(Args.Source));
}

void FLastResortPropertyNetSerializer::FreeDynamicStateInternal(FNetSerializationContext& Context, QuantizedType& Value)
{
	// Clear all info
	Value.ObjectReferenceStorage.Free(Context);
	Context.GetInternalContext()->Free(Value.Storage);

	Value.BitCount = 0;
	Value.ByteCapacity = 0;
	Value.Storage = 0;
}

void FLastResortPropertyNetSerializer::GrowDynamicStateInternal(FNetSerializationContext& Context, QuantizedType& Value, uint16 NewBitCount)
{
	checkSlow(NewBitCount > Value.BitCount);

	const uint16 ByteCount = static_cast<uint16>(Align((NewBitCount + 7U)/8U, AllocationAlignment));

	// We don't support delta compression for the unknown contents of the bits so we don't need to copy the old data.
	Context.GetInternalContext()->Free(Value.Storage);

	void* Storage = Context.GetInternalContext()->Alloc(ByteCount, AllocationAlignment);

	// Clear the last word to support IsEqual Memcmp optimization.
	const uint32 LastWordIndex = ByteCount/4U - 1U;
	static_cast<uint32*>(Storage)[LastWordIndex] = 0U;

	Value.ByteCapacity = ByteCount;
	Value.BitCount = NewBitCount;
	Value.Storage = Storage;
}

void FLastResortPropertyNetSerializer::AdjustStorageSize(FNetSerializationContext& Context, QuantizedType& Value, uint16 NewBitCount)
{
	const uint32 NewByteCapacity = Align((NewBitCount + 7U)/8U, AllocationAlignment);
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

void FLastResortPropertyNetSerializer::CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args)
{
	const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
	const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);
	FNetReferenceCollector& Collector = *reinterpret_cast<UE::Net::FNetReferenceCollector*>(Args.Collector);

	const FNetReferenceInfo ReferenceInfo(FNetReferenceInfo::EResolveType::ResolveOnClient);	
	for (const FNetObjectReference& Ref : MakeArrayView(Value.ObjectReferenceStorage.GetData(), Value.ObjectReferenceStorage.Num()))
	{
		Collector.Add(ReferenceInfo, Ref, Args.ChangeMaskInfo);
	}
}


}
