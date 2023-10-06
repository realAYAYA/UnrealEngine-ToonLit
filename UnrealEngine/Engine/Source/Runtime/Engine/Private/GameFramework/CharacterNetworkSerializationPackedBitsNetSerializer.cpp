// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/CharacterNetworkSerializationPackedBitsNetSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterNetworkSerializationPackedBitsNetSerializer)

#if UE_WITH_IRIS

#include "GameFramework/CharacterNetworkSerializationPackedBitsNetSerializer.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"

#include "Iris/Core/NetObjectReference.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "Iris/Serialization/NetReferenceCollector.h"
#include "Iris/Serialization/NetSerializerArrayStorage.h"

namespace UE::Net::Private
{
	static constexpr inline uint32 CalculateRequiredWordCount(uint32 NumBits) { return (NumBits + NumBitsPerDWORD - 1U) / NumBitsPerDWORD; }
}

namespace UE::Net
{

struct FCharacterNetworkSerializationPackedBitsNetSerializerQuantizedType
{
	typedef uint32 WordType;
	static constexpr uint32 MaxInlinedObjectRefs = 4;
	static constexpr uint32 InlinedWordCount = Private::CalculateRequiredWordCount(CHARACTER_SERIALIZATION_PACKEDBITS_RESERVED_SIZE);

	typedef FNetSerializerArrayStorage<FNetObjectReference, AllocationPolicies::TInlinedElementAllocationPolicy<MaxInlinedObjectRefs>> FObjectReferenceStorage;
	typedef FNetSerializerArrayStorage<WordType, AllocationPolicies::TInlinedElementAllocationPolicy<InlinedWordCount>> FDataBitsStorage;

	FObjectReferenceStorage ObjectReferenceStorage;
	FDataBitsStorage DataBitsStorage;
	uint32 NumDataBits;
};

}

template <> struct TIsPODType<UE::Net::FCharacterNetworkSerializationPackedBitsNetSerializerQuantizedType> { enum { Value = true }; };

namespace UE::Net
{

struct FCharacterNetworkSerializationPackedBitsNetSerializer
{
	// Version
	static const uint32 Version = 0;

	// Traits
	static constexpr bool bHasDynamicState = true;
	static constexpr bool bHasCustomNetReference = true;

	typedef uint32 WordType;

	typedef FCharacterNetworkSerializationPackedBits SourceType;
	typedef FCharacterNetworkSerializationPackedBitsNetSerializerQuantizedType QuantizedType;
	typedef struct FCharacterNetworkSerializationPackedBitsNetSerializerConfig ConfigType;

	static const ConfigType DefaultConfig;

	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs&);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs&);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs&);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs&);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs&);
	static bool Validate(FNetSerializationContext&, const FNetValidateArgs&);

	static void CloneDynamicState(FNetSerializationContext&, const FNetCloneDynamicStateArgs&);
	static void FreeDynamicState(FNetSerializationContext&, const FNetFreeDynamicStateArgs&);

	static void CollectNetReferences(FNetSerializationContext&, const FNetCollectReferencesArgs&);

private:
	class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
	{
	public:
		virtual ~FNetSerializerRegistryDelegates();

	private:
		virtual void OnPreFreezeNetSerializerRegistry() override;
	};

	static void FreeDynamicState(FNetSerializationContext&, QuantizedType& Value);

	static FCharacterNetworkSerializationPackedBitsNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
	static const FNetSerializer* ObjectNetSerializer;
};

UE_NET_IMPLEMENT_SERIALIZER(FCharacterNetworkSerializationPackedBitsNetSerializer);

const FCharacterNetworkSerializationPackedBitsNetSerializer::ConfigType FCharacterNetworkSerializationPackedBitsNetSerializer::DefaultConfig;
FCharacterNetworkSerializationPackedBitsNetSerializer::FNetSerializerRegistryDelegates FCharacterNetworkSerializationPackedBitsNetSerializer::NetSerializerRegistryDelegates;
const FNetSerializer* FCharacterNetworkSerializationPackedBitsNetSerializer::ObjectNetSerializer = &UE_NET_GET_SERIALIZER(FObjectNetSerializer);

void FCharacterNetworkSerializationPackedBitsNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	const QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);
	
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	const uint32 NumReferences = Value.ObjectReferenceStorage.Num();

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

	// Write data bits
	const uint32 NumDataBits = Value.NumDataBits;
	if (Writer->WriteBool(NumDataBits > 0))
	{
		UE::Net::WritePackedUint32(Writer, NumDataBits);
		Writer->WriteBitStream(Value.DataBitsStorage.GetData(), 0, NumDataBits);
	}
}

void FCharacterNetworkSerializationPackedBitsNetSerializer::FreeDynamicState(FNetSerializationContext& Context, QuantizedType& Value)
{
	Value.ObjectReferenceStorage.Free(Context);
	Value.DataBitsStorage.Free(Context);
	Value.NumDataBits = 0;
}

void FCharacterNetworkSerializationPackedBitsNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
	QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);
	
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

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

		TargetValue.ObjectReferenceStorage.AdjustSize(Context, NumReferences);

		FObjectNetSerializerConfig ObjectSerializerConfig;
		for (FNetObjectReference& Ref : MakeArrayView(TargetValue.ObjectReferenceStorage.GetData(), TargetValue.ObjectReferenceStorage.Num()))
		{
			FNetDeserializeArgs ObjectArgs;
			ObjectArgs.NetSerializerConfig = &ObjectSerializerConfig;
			ObjectArgs.Target = NetSerializerValuePointer(&Ref);

			ObjectNetSerializer->Deserialize(Context, ObjectArgs);
		}
	}
	else
	{
		TargetValue.ObjectReferenceStorage.Free(Context);
	}

	const bool bHasDataBits = Reader->ReadBool();
	if (bHasDataBits)
	{
		const uint32 NumDataBits = UE::Net::ReadPackedUint32(Reader);

		if (NumDataBits > Config->MaxAllowedDataBits)
		{
			Context.SetError(GNetError_ArraySizeTooLarge);
			return;
		}
		
		const uint32 RequiredWordCount = Private::CalculateRequiredWordCount(NumDataBits);
		TargetValue.DataBitsStorage.AdjustSize(Context, RequiredWordCount);

		Reader->ReadBitStream(TargetValue.DataBitsStorage.GetData(), NumDataBits);
		TargetValue.NumDataBits = NumDataBits;
	}
	else
	{
		TargetValue.DataBitsStorage.Free(Context);
		TargetValue.NumDataBits = 0U;
	}
}

void FCharacterNetworkSerializationPackedBitsNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	const SourceType& SourceValue = *reinterpret_cast<const SourceType*>(Args.Source);
	QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);

	const uint32 NumObjectReferences = SourceValue.ObjectReferences.Num();
	TargetValue.ObjectReferenceStorage.AdjustSize(Context, NumObjectReferences);

	if (NumObjectReferences > 0)
	{
		FObjectNetSerializerConfig Config;
		const TObjectPtr<UObject>* SourceReferences = SourceValue.ObjectReferences.GetData();
		FNetObjectReference* TargetReferences = TargetValue.ObjectReferenceStorage.GetData();
		for (uint32 ReferenceIndex = 0; ReferenceIndex < NumObjectReferences; ++ReferenceIndex)
		{
			FNetQuantizeArgs ObjectArgs;
			ObjectArgs.NetSerializerConfig = &Config;
			ObjectArgs.Source = NetSerializerValuePointer(SourceReferences + ReferenceIndex);
			ObjectArgs.Target = NetSerializerValuePointer(TargetReferences + ReferenceIndex);

			ObjectNetSerializer->Quantize(Context, ObjectArgs);
		}
	}

	const uint32 NumDataBits = SourceValue.DataBits.Num();
	TargetValue.DataBitsStorage.AdjustSize(Context, Private::CalculateRequiredWordCount(NumDataBits));
	if (NumDataBits > 0)
	{
		FMemory::Memcpy(TargetValue.DataBitsStorage.GetData(), SourceValue.DataBits.GetData(), (NumDataBits + 7U) / 8U);
	}
	TargetValue.NumDataBits = NumDataBits;
}

void FCharacterNetworkSerializationPackedBitsNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

	// References
	const uint32 NumObjectReferences = Source.ObjectReferenceStorage.Num();	
	Target.ObjectReferences.SetNumUninitialized(NumObjectReferences);

	FObjectNetSerializerConfig Config;
	const FNetObjectReference* SourceReferences = Source.ObjectReferenceStorage.GetData();
	TObjectPtr<UObject>* TargetReferences = Target.ObjectReferences.GetData();
	for (uint32 ReferenceIndex = 0; ReferenceIndex < NumObjectReferences; ++ReferenceIndex)
	{
		FNetDequantizeArgs ObjectArgs;
		ObjectArgs.NetSerializerConfig = &Config;
		ObjectArgs.Source = NetSerializerValuePointer(SourceReferences + ReferenceIndex);
		ObjectArgs.Target = NetSerializerValuePointer(TargetReferences + ReferenceIndex);

		ObjectNetSerializer->Dequantize(Context, ObjectArgs);
	}

	// DataBits
	Target.DataBits.SetNumUninitialized(Source.NumDataBits);
	Target.DataBits.SetRangeFromRange(0, Source.NumDataBits, Source.DataBitsStorage.GetData());
}

bool FCharacterNetworkSerializationPackedBitsNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);

		if (Value0.NumDataBits != Value1.NumDataBits || Value0.ObjectReferenceStorage.Num() != Value1.ObjectReferenceStorage.Num())
		{
			return false;
		}

		const uint32 RequiredWords = Private::CalculateRequiredWordCount(Value0.NumDataBits);
		if (RequiredWords > 0 && FMemory::Memcmp(Value0.DataBitsStorage.GetData(), Value1.DataBitsStorage.GetData(), sizeof(WordType) * RequiredWords) != 0)
		{
			return false;
		}

		if (Value0.ObjectReferenceStorage.Num() > 0 && FMemory::Memcmp(Value0.ObjectReferenceStorage.GetData(), Value1.ObjectReferenceStorage.GetData(), sizeof(FNetObjectReference) * Value0.ObjectReferenceStorage.Num()) != 0)
		{
			return false;
		}
	}
	else
	{
		const SourceType& Value0 = *reinterpret_cast<SourceType*>(Args.Source0);
		const SourceType& Value1 = *reinterpret_cast<SourceType*>(Args.Source1);
		return Value0.DataBits == Value1.DataBits;
	}

	return true;
}

bool FCharacterNetworkSerializationPackedBitsNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
{
	const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
	const QuantizedType& SourceValue = *reinterpret_cast<const QuantizedType*>(Args.Source);

	if (SourceValue.ObjectReferenceStorage.Num() > Config->MaxAllowedObjectReferences || SourceValue.NumDataBits > Config->MaxAllowedDataBits)
	{
		return false;
	}
	return true;
}

void FCharacterNetworkSerializationPackedBitsNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
{
	const QuantizedType& SourceValue = *reinterpret_cast<const QuantizedType*>(Args.Source);
	QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);

	TargetValue.ObjectReferenceStorage.Clone(Context, SourceValue.ObjectReferenceStorage);
	TargetValue.DataBitsStorage.Clone(Context, SourceValue.DataBitsStorage);
}

void FCharacterNetworkSerializationPackedBitsNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
{
	QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);
	FreeDynamicState(Context, Value);
}

void FCharacterNetworkSerializationPackedBitsNetSerializer::CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args)
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

static const FName PropertyNetSerializerRegistry_NAME_CharacterMoveResponsePackedBits("CharacterMoveResponsePackedBits");
static const FName PropertyNetSerializerRegistry_NAME_CharacterServerMovePackedBitsPackedBits("CharacterServerMovePackedBits");
static const FName PropertyNetSerializerRegistry_NAME_CharacterNetworkSerializationPackedBits("CharacterNetworkSerializationPackedBits");
UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterMoveResponsePackedBits, FCharacterNetworkSerializationPackedBitsNetSerializer);
UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterServerMovePackedBitsPackedBits, FCharacterNetworkSerializationPackedBitsNetSerializer);
UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterNetworkSerializationPackedBits, FCharacterNetworkSerializationPackedBitsNetSerializer);

FCharacterNetworkSerializationPackedBitsNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
{
	UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterMoveResponsePackedBits);
	UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterServerMovePackedBitsPackedBits);
	UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterNetworkSerializationPackedBits);
}

void FCharacterNetworkSerializationPackedBitsNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
{
	UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterMoveResponsePackedBits);
	UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterServerMovePackedBitsPackedBits);
	UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_CharacterNetworkSerializationPackedBits);
}

}

#endif // UE_WITH_IRIS
