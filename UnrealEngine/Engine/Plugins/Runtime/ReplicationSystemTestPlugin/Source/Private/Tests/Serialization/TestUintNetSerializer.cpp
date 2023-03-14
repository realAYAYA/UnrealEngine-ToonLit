// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestNetSerializerFixture.h"
#include "Iris/Serialization/UintNetSerializers.h"

namespace UE::Net::Private
{

static FTestMessage& PrintUintNetSerializerConfig(FTestMessage& Message, const FNetSerializerConfig& InConfig)
{
	const FUintNetSerializerConfig& Config = static_cast<const FUintNetSerializerConfig&>(InConfig);
	return Message << "BitCount: "  << Config.BitCount;
}

template<typename SourceType>
class FTestUintNetSerializer : public TTestNetSerializerFixture<PrintUintNetSerializerConfig, SourceType>
{
	typedef TTestNetSerializerFixture<PrintUintNetSerializerConfig, SourceType> Super;

public:
	FTestUintNetSerializer(const FNetSerializer& Serializer) : Super(Serializer) {}

	void TestIsEqual();
	void TestValidate();
	void TestSerialize();
	void TestSerializeDelta();
	void TestDefaultConfig();

protected:
	static const SourceType Values[];
	static const SIZE_T ValueCount;
};

#define UE_NET_IMPLEMENT_UINT_NETSERIALIZER_TEST(TestClassName, NetSerializerType, SourceType) \
class TestClassName : public FTestUintNetSerializer<SourceType> \
{ \
public: \
	TestClassName() : FTestUintNetSerializer<SourceType>(UE_NET_GET_SERIALIZER(NetSerializerType)) {} \
}; \
\
UE_NET_TEST_FIXTURE(TestClassName, HasTestValues) \
{ \
	UE_NET_ASSERT_GT(ValueCount, SIZE_T(0)) << "No test values found"; \
} \
\
UE_NET_TEST_FIXTURE(TestClassName, TestIsEqual) \
{ \
	TestIsEqual(); \
} \
\
UE_NET_TEST_FIXTURE(TestClassName, TestValidate) \
{ \
	TestValidate(); \
} \
\
UE_NET_TEST_FIXTURE(TestClassName, TestSerialize) \
{ \
	TestSerialize(); \
} \
\
UE_NET_TEST_FIXTURE(TestClassName, TestSerializeDelta) \
{ \
	TestSerializeDelta(); \
} \
\
UE_NET_TEST_FIXTURE(TestClassName, TestDefaultConfig) \
{ \
	TestDefaultConfig(); \
} \

UE_NET_IMPLEMENT_UINT_NETSERIALIZER_TEST(FTestUint64NetSerializer, FUint64NetSerializer, uint64);
UE_NET_IMPLEMENT_UINT_NETSERIALIZER_TEST(FTestUint32NetSerializer, FUint32NetSerializer, uint32);
UE_NET_IMPLEMENT_UINT_NETSERIALIZER_TEST(FTestUint16NetSerializer, FUint16NetSerializer, uint16);
UE_NET_IMPLEMENT_UINT_NETSERIALIZER_TEST(FTestUint8NetSerializer, FUint8NetSerializer, uint8);

#undef UE_NET_IMPLEMENT_UINT_NETSERIALIZER_TEST

//
template<typename SourceType> const SourceType FTestUintNetSerializer<SourceType>::Values[] =
{
	TNumericLimits<SourceType>::Lowest(), TNumericLimits<SourceType>::Max(), SourceType(0), SourceType(16), SourceType(2048458 % TNumericLimits<SourceType>::Max())
};
template<typename SourceType> const SIZE_T FTestUintNetSerializer<SourceType>::ValueCount = sizeof(Values)/sizeof(Values[0]);

template<typename SourceType>
void FTestUintNetSerializer<SourceType>::TestIsEqual()
{
	const uint8 MinBitCount = 1;
	const uint8 MaxBitCount = sizeof(SourceType)*8;

	SourceType CompareValues[2][sizeof(Values)/sizeof(Values[0])];
	bool ExpectedResults[2][sizeof(Values)/sizeof(Values[0])];
	for (SIZE_T ValueIt = 0; ValueIt < ValueCount; ++ValueIt)
	{
		CompareValues[0][ValueIt] = Values[ValueIt];
		ExpectedResults[0][ValueIt] = true;
	}
	for (SIZE_T ValueIt = 0; ValueIt < ValueCount; ++ValueIt)
	{
		CompareValues[1][ValueIt] = ~Values[ValueIt];
		ExpectedResults[1][ValueIt] = false;
	}

	// Do two rounds of testing, one where we compare each value with itself and one where we compare against a guaranteed non-equal value.
	// We will only do an unquantized compare since we know these serializers uses a default quantize implementation.
	for (SIZE_T TestRoundIt = 0, TestRoundEndIt = 2; TestRoundIt != TestRoundEndIt; ++TestRoundIt)
	{
		for (uint8 BitCount = MinBitCount; BitCount <= MaxBitCount; ++BitCount)
		{
			FUintNetSerializerConfig Config;
			Config.BitCount = BitCount;

			constexpr bool bQuantizedCompare = false;
			const bool bSuccess = Super::TestIsEqual(Values, CompareValues[TestRoundIt], ExpectedResults[TestRoundIt], ValueCount, Config, bQuantizedCompare);
			if (!bSuccess)
			{
				return;
			}
		}
	}
}

template<typename SourceType>
void FTestUintNetSerializer<SourceType>::TestValidate()
{
	const uint8 MinBitCount = 1;
	const uint8 MaxBitCount = sizeof(SourceType)*8;

	bool ExpectedResults[sizeof(Values)/sizeof(Values[0])];
	for (uint8 BitCount = MinBitCount; BitCount <= MaxBitCount; ++BitCount)
	{
		const SourceType MaxValue = TNumericLimits<SourceType>::Max() >> (MaxBitCount - BitCount);
		for (SIZE_T ValueIt = 0; ValueIt < ValueCount; ++ValueIt)
		{
			const SourceType Value = Values[ValueIt];
			ExpectedResults[ValueIt] = FMath::Min(Value, MaxValue) == Value;
		}

		FUintNetSerializerConfig Config(BitCount);
		const bool bSuccess = Super::TestValidate(Values, ExpectedResults, ValueCount, Config);
		if (!bSuccess)
		{
			return;
		}
	}
}

template<typename SourceType>
void FTestUintNetSerializer<SourceType>::TestSerialize()
{
	const uint8 MinBitCount = 1;
	const uint8 MaxBitCount = sizeof(SourceType)*8;

	// Setup expected results. The IntPackers should only consider the BitCount least significant bits. Our test values may have bits set outside that range.
	// But values with bits sets that won't be serialized should be considered equal as long as the least significant bits are set. This is because serializing
	// either value will result in the same value being deserialized. It may not be euqal the the original value though. The NetSerializer's Validate function
	// is there to detect that kind of situations and warn the user that the result is undefined.
	SourceType ExpectedValues[sizeof(Values) / sizeof(Values[0])];
	for (SIZE_T ValueIt = 0; ValueIt < ValueCount; ++ValueIt)
	{
		ExpectedValues[ValueIt] = Values[ValueIt];
	}

	const auto& EqualityFunc = [](NetSerializerValuePointer Value0, NetSerializerValuePointer Value1) -> bool { return *reinterpret_cast<SourceType*>(Value0) == *reinterpret_cast<SourceType*>(Value1); };

	for (uint8 BitCount = MinBitCount; BitCount <= MaxBitCount; ++BitCount)
	{
		FUintNetSerializerConfig Config(BitCount);

		constexpr bool bQuantizedCompare = false;
		TOptional<TFunctionRef<bool(NetSerializerValuePointer Value0, NetSerializerValuePointer Value1)>> CompareFunc;
		if (BitCount == MaxBitCount)
		{
			CompareFunc = EqualityFunc;
		}
		const bool bSuccess = Super::TestSerialize(Values, ExpectedValues, ValueCount, Config, bQuantizedCompare, CompareFunc);
		if (!bSuccess)
		{
			return;
		}
	}
}

template<typename SourceType>
void FTestUintNetSerializer<SourceType>::TestSerializeDelta()
{
	constexpr uint8 MinBitCount = 1;
	constexpr uint8 MaxBitCount = sizeof(SourceType)*8;

	for (uint8 BitCount = MinBitCount; BitCount <= MaxBitCount; ++BitCount)
	{
		FUintNetSerializerConfig Config(BitCount);
		Super::TestSerializeDelta(Values, ValueCount, Config);
	}
}

template<typename SourceType>
void FTestUintNetSerializer<SourceType>::TestDefaultConfig()
{
	const FUintNetSerializerConfig* DefaultConfig = static_cast<const FUintNetSerializerConfig*>(FTestNetSerializerFixture::Serializer.DefaultConfig);
	UE_NET_ASSERT_NE(DefaultConfig, nullptr);

	UE_NET_ASSERT_EQ(DefaultConfig->BitCount, static_cast<uint8>(sizeof(SourceType)*8u));
}

}
