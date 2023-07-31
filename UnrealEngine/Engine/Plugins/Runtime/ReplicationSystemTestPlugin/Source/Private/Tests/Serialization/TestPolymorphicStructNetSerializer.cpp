// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestPolymorphicStructNetSerializer.h"
#include "TestNetSerializerFixture.h"

#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Tests/ReplicationSystem/ReplicationSystemServerClientTestFixture.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetReferenceCollector.h"

namespace UE::Net
{

// Custom NetSerializer declaration based on GameplayAbilityTargetHandle
struct FTestPolymorphicArrayStructNetSerializer : public TPolymorphicArrayStructNetSerializerImpl<FExamplePolymorphicArrayStruct, FExamplePolymorphicArrayItem, FExamplePolymorphicArrayStruct::GetArray, FExamplePolymorphicArrayStruct::SetArrayNum>
{
	static const uint32 Version = 0;
	static const ConfigType DefaultConfig;
};

const FTestPolymorphicArrayStructNetSerializer::ConfigType FTestPolymorphicArrayStructNetSerializer::DefaultConfig;

UE_NET_IMPLEMENT_SERIALIZER(FTestPolymorphicArrayStructNetSerializer);

}

namespace UE::Net::Private
{

class FTestPolymorphicArrayStructNetSerializerFixture : public FReplicationSystemServerClientTestFixture
{
public:
	FTestPolymorphicArrayStructNetSerializerFixture() : FReplicationSystemServerClientTestFixture() {}

protected:
	virtual void SetUp() override;
	virtual void TearDown() override;
	void ValidateExpectedState(const FExamplePolymorphicArrayStruct& Struct);
	void SetExpectedState(FExamplePolymorphicArrayStruct& Target);

	// Composable operations for testing the serializer
	void Serialize();
	void Deserialize();
	void SerializeDelta();
	void DeserializeDelta();
	void Quantize();
	void Dequantize();
	bool IsEqual(bool bQuantized);
	void Clone();
	void Validate();
	void FreeQuantizedState();

protected:
	FNetSerializationContext NetSerializationContext;
	FInternalNetSerializationContext InternalNetSerializationContext;

	FTestPolymorphicArrayStructNetSerializerConfig PolymorphicNetSerializerConfig;
	const FNetSerializer* PolymorphicNetSerializer = &UE_NET_GET_SERIALIZER(FTestPolymorphicArrayStructNetSerializer);

	FExamplePolymorphicArrayStruct PolymorphicStructInstance0;
	FExamplePolymorphicArrayStruct PolymorphicStructInstance1;

	FExamplePolymorphicStructA* StructA;
	FExamplePolymorphicStructB* StructB;
	FExamplePolymorphicStructC* StructC;
	FExamplePolymorphicStructD* StructD;

	alignas(8) uint8 QuantizedBuffer[2048];
	alignas(8) uint8 ClonedQuantizedBuffer[2048];
	alignas(8) uint8 BitStreamBuffer[2048];

	bool bHasQuantizedState;
	bool bHasClonedQuantizedState;

	FNetBitStreamWriter Writer;
	FNetBitStreamReader Reader;

	static bool bHasRegisteredStructDNetSerializer;
};

bool FTestPolymorphicArrayStructNetSerializerFixture::bHasRegisteredStructDNetSerializer = false;

}

namespace UE::Net
{

// Custom serializer for FExamplePolymorphicStructD
struct FExamplePolymorphicStructDNetSerializer
{
	static const uint32 Version = 0;

	typedef FExamplePolymorphicStructD SourceType;
	typedef uint32 QuantizedType;
	typedef FExamplePolymorphicStructDNetSerializerConfig ConfigType;

	static const ConfigType DefaultConfig;

	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs& Args);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs& Args);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args);
};

const FExamplePolymorphicStructDNetSerializer::ConfigType FExamplePolymorphicStructDNetSerializer::DefaultConfig;

void FExamplePolymorphicStructDNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	const uint32 Value = *reinterpret_cast<const uint32*>(Args.Source);

	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	Writer->WriteBits(Value, 32U);
}

void FExamplePolymorphicStructDNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	const uint32 Value = Reader->ReadBits(32U);

	uint32& TargetValue = *reinterpret_cast<uint32*>(Args.Target);
	
	TargetValue = Value;
}

void FExamplePolymorphicStructDNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	uint32& Target = *reinterpret_cast<uint32*>(Args.Target);
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);

	Target = Source.SomeValue;
}

void FExamplePolymorphicStructDNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	const uint32& Source = *reinterpret_cast<const uint32*>(Args.Source);
	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

	Target.SomeValue = Source;	
}

bool FExamplePolymorphicStructDNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		const uint32& Value0 = *reinterpret_cast<const uint32*>(Args.Source0);
		const uint32& Value1 = *reinterpret_cast<const uint32*>(Args.Source1);

		return Value0 == Value1;
	}
	else
	{
		const SourceType Value0 = *reinterpret_cast<const SourceType*>(Args.Source0);
		const SourceType Value1 = *reinterpret_cast<const SourceType*>(Args.Source1);

		return Value0.SomeValue == Value1.SomeValue;
	}
}

UE_NET_IMPLEMENT_SERIALIZER(FExamplePolymorphicStructDNetSerializer);

}

namespace UE::Net::Private
{

static const FName PropertyNetSerializerRegistry_NAME_ExamplePolymorphicStructD(TEXT("ExamplePolymorphicStructD"));
UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ExamplePolymorphicStructD, FExamplePolymorphicStructDNetSerializer);


UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestQuantize)
{
	Quantize();
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestDequantize)
{
	Quantize();
	Dequantize();
	ValidateExpectedState(PolymorphicStructInstance1);
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestSerialize)
{
	Quantize();
	Serialize();
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestDeserialize)
{
	Quantize();
	Serialize();
	FreeQuantizedState();
	Deserialize();
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestDequantizeSerializedState)
{
	Quantize();
	Serialize();
	FreeQuantizedState();
	Deserialize();
	Dequantize();
	ValidateExpectedState(PolymorphicStructInstance1);
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestSerializeDelta)
{
	Quantize();
	SerializeDelta();
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestDeserializeDelta)
{
	Quantize();
	Serialize();
	FreeQuantizedState();
	DeserializeDelta();
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestCollectReferencesNoRef)
{
	Quantize();
	UE_NET_ASSERT_TRUE(bHasQuantizedState);

	FNetReferenceCollector Collector;

	FNetCollectReferencesArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	Args.Source = NetSerializerValuePointer(&QuantizedBuffer);
	Args.Collector = NetSerializerValuePointer(&Collector);

	PolymorphicNetSerializer->CollectNetReferences(NetSerializationContext, Args);	

	UE_NET_ASSERT_EQ(0, Collector.GetCollectedReferences().Num());
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestCollectReferencesRef)
{
	Quantize();
	UE_NET_ASSERT_TRUE(bHasQuantizedState);

	FNetReferenceCollector Collector(ENetReferenceCollectorTraits::IncludeInvalidReferences);

	FNetCollectReferencesArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	Args.Source = NetSerializerValuePointer(&QuantizedBuffer);
	Args.Collector = NetSerializerValuePointer(&Collector);

	PolymorphicNetSerializer->CollectNetReferences(NetSerializationContext, Args);	

	UE_NET_ASSERT_EQ(1, Collector.GetCollectedReferences().Num());
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestIsEqualExternal)
{
	constexpr bool bUseQuantizedState = false;
	UE_NET_ASSERT_FALSE(IsEqual(bUseQuantizedState));

	// Copy the state using the defined shallow copy
	PolymorphicStructInstance1 = PolymorphicStructInstance0;
	UE_NET_ASSERT_TRUE(IsEqual(bUseQuantizedState));

	// Set the same values, that would be equal if we used a deep copy
	// Due to the way the struct we try to mimic is implemented when it comes to compare we do expect this to fail even though the values are the same
	// It only does a shallow copy by default, and also only relies on comparing pointers for detecting changes which probably can fail pretty nicely
	SetExpectedState(PolymorphicStructInstance1);

	UE_NET_ASSERT_FALSE(IsEqual(bUseQuantizedState));
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestIsEqualQuantized)
{
	constexpr bool bUseQuantizedState = true;

	Quantize();
	Clone();
	UE_NET_ASSERT_TRUE(IsEqual(bUseQuantizedState));
}

UE_NET_TEST_FIXTURE(FTestPolymorphicArrayStructNetSerializerFixture, TestValidate)
{
	Validate();
}

void FTestPolymorphicArrayStructNetSerializerFixture::SetExpectedState(FExamplePolymorphicArrayStruct& Target)
{
	StructA = new FExamplePolymorphicStructA;
	StructA->SomeInt = 1;
	StructB = new FExamplePolymorphicStructB;
	StructB->SomeFloat = 1.0f;

	StructC = new FExamplePolymorphicStructC;
	StructC->SomeObjectRef = nullptr;
	StructC->SomeBool = false;

	StructD = new FExamplePolymorphicStructD;
	StructD->SomeValue = 0xBEEEEEEF;
	
	// Add some data, we need some structs to send as well
	Target.Add(StructA);
	Target.Add(StructB);
	Target.Add(StructC);
	Target.Add(StructD);
}


void FTestPolymorphicArrayStructNetSerializerFixture::SetUp()
{
	FReplicationSystemServerClientTestFixture::SetUp();

	// Init default serialization context
	InternalNetSerializationContext.ReplicationSystem = Server->ReplicationSystem;

	FReplicationSystemInternal* ReplicationSystemInternal = Server->GetReplicationSystem()->GetReplicationSystemInternal();

	FInternalNetSerializationContext TempInternalNetSerializationContext;
	FInternalNetSerializationContext::FInitParameters TempInternalNetSerializationContextInitParams;
	TempInternalNetSerializationContextInitParams.ReplicationSystem = Server->ReplicationSystem;
	TempInternalNetSerializationContextInitParams.ObjectResolveContext.RemoteNetTokenStoreState = ReplicationSystemInternal->GetNetTokenStore().GetLocalNetTokenStoreState();
	TempInternalNetSerializationContext.Init(TempInternalNetSerializationContextInitParams);

	InternalNetSerializationContext = MoveTemp(TempInternalNetSerializationContext);
	NetSerializationContext.SetInternalContext(&InternalNetSerializationContext);

	FMemory::Memzero(QuantizedBuffer, 0);

	SetExpectedState(PolymorphicStructInstance0);

	bHasQuantizedState = false;
	bHasClonedQuantizedState = false;

	// Register custom serializer
	if (!bHasRegisteredStructDNetSerializer)
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ExamplePolymorphicStructD);
		bHasRegisteredStructDNetSerializer = true;
	}

	// Init type registry, note that we set it up for the context we provide rather than the default config
	PolymorphicNetSerializerConfig.RegisteredTypes.InitForType(FExamplePolymorphicArrayItem::StaticStruct());
}

void FTestPolymorphicArrayStructNetSerializerFixture::ValidateExpectedState(const FExamplePolymorphicArrayStruct& StructInstance)
{
	UE_NET_ASSERT_EQ(PolymorphicStructInstance0.Num(), StructInstance.Num());

	auto AInstance = static_cast<const FExamplePolymorphicStructA*>(StructInstance.Get(0));
	auto BInstance = static_cast<const FExamplePolymorphicStructB*>(StructInstance.Get(1));
	auto CInstance = static_cast<const FExamplePolymorphicStructC*>(StructInstance.Get(2));
	auto DInstance = static_cast<const FExamplePolymorphicStructD*>(StructInstance.Get(3));

	// Validate types
	UE_NET_ASSERT_TRUE(AInstance->GetScriptStruct() == FExamplePolymorphicStructA::StaticStruct());
	UE_NET_ASSERT_TRUE(BInstance->GetScriptStruct() == FExamplePolymorphicStructB::StaticStruct());
	UE_NET_ASSERT_TRUE(CInstance->GetScriptStruct() == FExamplePolymorphicStructC::StaticStruct());
	UE_NET_ASSERT_TRUE(DInstance->GetScriptStruct() == FExamplePolymorphicStructD::StaticStruct());

	// Validate actual data	
	UE_NET_ASSERT_EQ(StructA->SomeInt, AInstance->SomeInt);
	UE_NET_ASSERT_EQ(StructB->SomeFloat, BInstance->SomeFloat);
	UE_NET_ASSERT_EQ(StructC->SomeBool, CInstance->SomeBool);
	UE_NET_ASSERT_EQ(ToRawPtr(StructC->SomeObjectRef), ToRawPtr(CInstance->SomeObjectRef));
	UE_NET_ASSERT_EQ(StructD->SomeValue, DInstance->SomeValue);
}

void FTestPolymorphicArrayStructNetSerializerFixture::TearDown()
{
	PolymorphicStructInstance0.Clear();
	PolymorphicStructInstance1.Clear();

	FreeQuantizedState();

	FReplicationSystemServerClientTestFixture::TearDown();
}

void FTestPolymorphicArrayStructNetSerializerFixture::Serialize()
{
	// Must have run quantize before this
	UE_NET_ASSERT_TRUE(bHasQuantizedState);
	
	// Serialize data
	{
		Writer.InitBytes(BitStreamBuffer, sizeof(BitStreamBuffer));
		FNetSerializationContext Context(&Writer);
		Context.SetInternalContext(NetSerializationContext.GetInternalContext());

		FNetSerializeArgs Args = {};
		Args.Version = PolymorphicNetSerializer->Version;
		Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
		Args.Source = NetSerializerValuePointer(&QuantizedBuffer);
		PolymorphicNetSerializer->Serialize(Context, Args);

		Writer.CommitWrites();

		UE_NET_ASSERT_FALSE(Context.HasError());
		UE_NET_ASSERT_TRUE(Writer.GetPosBits() > 0U);
	}
}

void FTestPolymorphicArrayStructNetSerializerFixture::Deserialize()
{
	// Check pre-conditions
	UE_NET_ASSERT_FALSE(bHasQuantizedState);
	UE_NET_ASSERT_TRUE(Writer.GetPosBytes() > 0U);
	
	Reader.InitBits(BitStreamBuffer, Writer.GetPosBits());

	FNetSerializationContext Context(&Reader);
	Context.SetInternalContext(NetSerializationContext.GetInternalContext());

	FNetDeserializeArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	Args.Target = NetSerializerValuePointer(&QuantizedBuffer);
	PolymorphicNetSerializer->Deserialize(Context, Args);

	bHasQuantizedState = true;
}

void FTestPolymorphicArrayStructNetSerializerFixture::SerializeDelta()
{
	// Check pre-conditions
	UE_NET_ASSERT_TRUE(bHasQuantizedState);
	
	// Serialize data
	{
		Writer.InitBytes(BitStreamBuffer, sizeof(BitStreamBuffer));
		FNetSerializationContext Context(&Writer);
		Context.SetInternalContext(NetSerializationContext.GetInternalContext());

		FNetSerializeDeltaArgs Args = {};
		Args.Version = PolymorphicNetSerializer->Version;
		Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
		Args.Source = NetSerializerValuePointer(&QuantizedBuffer);

		PolymorphicNetSerializer->SerializeDelta(Context, Args);

		Writer.CommitWrites();

		UE_NET_ASSERT_FALSE(Context.HasError());
		UE_NET_ASSERT_TRUE(Writer.GetPosBits() > 0U);
	}
}

void FTestPolymorphicArrayStructNetSerializerFixture::DeserializeDelta()
{
	// Check pre-conditions
	UE_NET_ASSERT_FALSE(bHasQuantizedState);
	UE_NET_ASSERT_TRUE(Writer.GetPosBytes() > 0U);
	
	Reader.InitBits(BitStreamBuffer, Writer.GetPosBits());

	FNetSerializationContext Context(&Reader);
	Context.SetInternalContext(NetSerializationContext.GetInternalContext());

	FNetDeserializeDeltaArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	Args.Target = NetSerializerValuePointer(&QuantizedBuffer);
	PolymorphicNetSerializer->DeserializeDelta(Context, Args);

	bHasQuantizedState = true;
}

void FTestPolymorphicArrayStructNetSerializerFixture::Quantize()
{
	FNetQuantizeArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	Args.Target = NetSerializerValuePointer(&QuantizedBuffer);
	Args.Source = NetSerializerValuePointer(&PolymorphicStructInstance0);
	PolymorphicNetSerializer->Quantize(NetSerializationContext, Args);

	bHasQuantizedState = true;
}

void FTestPolymorphicArrayStructNetSerializerFixture::Clone()
{
	// Check pre-conditions
	UE_NET_ASSERT_TRUE(bHasQuantizedState);

	FNetCloneDynamicStateArgs Args = {};
	Args.Source = NetSerializerValuePointer(QuantizedBuffer);
	Args.Target = NetSerializerValuePointer(ClonedQuantizedBuffer);
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	PolymorphicNetSerializer->CloneDynamicState(NetSerializationContext, Args);
}

void FTestPolymorphicArrayStructNetSerializerFixture::FreeQuantizedState()
{
	FNetFreeDynamicStateArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);

	if (bHasQuantizedState)
	{
		Args.Source = NetSerializerValuePointer(&QuantizedBuffer);
		PolymorphicNetSerializer->FreeDynamicState(NetSerializationContext, Args);
		bHasQuantizedState = false;
	}
		
	if (bHasClonedQuantizedState)
	{
		Args.Source = NetSerializerValuePointer(&ClonedQuantizedBuffer);
		PolymorphicNetSerializer->FreeDynamicState(NetSerializationContext, Args);
		bHasClonedQuantizedState = false;
	}
}

void FTestPolymorphicArrayStructNetSerializerFixture::Dequantize()
{
	UE_NET_ASSERT_TRUE(bHasQuantizedState);

	FNetDequantizeArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	Args.Source = NetSerializerValuePointer(&QuantizedBuffer);
	Args.Target = NetSerializerValuePointer(&PolymorphicStructInstance1);
	PolymorphicNetSerializer->Dequantize(NetSerializationContext, Args);
}

bool FTestPolymorphicArrayStructNetSerializerFixture::IsEqual(bool bQuantized)
{
	FNetSerializationContext Context;
	FNetIsEqualArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	Args.Source0 = bQuantized ? NetSerializerValuePointer(&QuantizedBuffer[0]) : NetSerializerValuePointer(&PolymorphicStructInstance0);
	Args.Source1 = bQuantized ? NetSerializerValuePointer(&ClonedQuantizedBuffer[0]) : NetSerializerValuePointer(&PolymorphicStructInstance1);
	Args.bStateIsQuantized = bQuantized;

	return PolymorphicNetSerializer->IsEqual(Context, Args);
}

void FTestPolymorphicArrayStructNetSerializerFixture::Validate()
{
	FNetValidateArgs Args = {};
	Args.Version = PolymorphicNetSerializer->Version;
	Args.NetSerializerConfig = NetSerializerConfigParam(&PolymorphicNetSerializerConfig);
	Args.Source = NetSerializerValuePointer(&PolymorphicStructInstance0);

	PolymorphicNetSerializer->Validate(NetSerializationContext, Args);
}

}
