// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagNetSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTagNetSerializer)

#if UE_WITH_IRIS

#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializerDelegates.h"

static_assert(sizeof(FGameplayTagNetIndex) == 2, "Unexpected GameplayTagNetIndex size. Expected 2.");

namespace UE::Net
{

struct FGameplayTagAccessorForNetSerializer : public FGameplayTag
{
	void SetTagName(const FName InTagName) { TagName = InTagName; }
};

struct FGameplayTagNetSerializer
{
	// Version
	static const uint32 Version = 0;

	// Types
	struct FQuantizedType
	{
		FGameplayTagNetIndex TagIndex;
	};

	typedef FGameplayTagAccessorForNetSerializer SourceType;
	typedef FQuantizedType QuantizedType;
	typedef struct FGameplayTagNetSerializerConfig ConfigType;

	static const ConfigType DefaultConfig;

	//
	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs&);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs&);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs&);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs&);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs&);

private:
	static constexpr FGameplayTagNetIndex InvalidTagIndex = INVALID_TAGNETINDEX;

	class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
	{
	public:
		virtual ~FNetSerializerRegistryDelegates();

	private:
		virtual void OnPreFreezeNetSerializerRegistry() override;
	};

	static FGameplayTagNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
};
UE_NET_IMPLEMENT_SERIALIZER(FGameplayTagNetSerializer);

const FGameplayTagNetSerializer::ConfigType FGameplayTagNetSerializer::DefaultConfig;
FGameplayTagNetSerializer::FNetSerializerRegistryDelegates FGameplayTagNetSerializer::NetSerializerRegistryDelegates;

void FGameplayTagNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	const QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);
	
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	WritePackedUint16(Writer, Value.TagIndex);
}

void FGameplayTagNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);
	
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();
	TargetValue.TagIndex = ReadPackedUint16(Reader);
}

void FGameplayTagNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	const SourceType& SourceValue = *reinterpret_cast<const SourceType*>(Args.Source);
	QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);

	const UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
	FGameplayTagNetIndex TagIndex = TagManager.GetNetIndexFromTag(SourceValue);
	if (TagIndex == TagManager.GetInvalidTagNetIndex())
	{
		TargetValue.TagIndex = InvalidTagIndex;
	}
	else
	{
		TargetValue.TagIndex = TagIndex;
	}
}

void FGameplayTagNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

	if (Source.TagIndex != InvalidTagIndex)
	{
		const UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
		Target.SetTagName(TagManager.GetTagNameFromNetIndex(Source.TagIndex));
	}
	else
	{
		Target.SetTagName(FName());
	}
}

bool FGameplayTagNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);
		return Value0.TagIndex == Value1.TagIndex;
	}
	else
	{
		const SourceType& Value0 = *reinterpret_cast<SourceType*>(Args.Source0);
		const SourceType& Value1 = *reinterpret_cast<SourceType*>(Args.Source1);
		return Value0.GetTagName() == Value1.GetTagName();
	}
}

static const FName PropertyNetSerializerRegistry_NAME_GameplayTag("GameplayTag");
UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_GameplayTag, FGameplayTagNetSerializer);

FGameplayTagNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
{
	UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_GameplayTag);
}

void FGameplayTagNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
{
	UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_GameplayTag);
}

}

#endif // UE_WITH_IRIS
