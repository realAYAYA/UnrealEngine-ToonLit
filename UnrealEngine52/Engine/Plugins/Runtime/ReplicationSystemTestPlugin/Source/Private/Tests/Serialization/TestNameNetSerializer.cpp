// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestNetSerializerFixture.h"
#include "Iris/Serialization/StringNetSerializers.h"
#include "Containers/StringConv.h"

namespace UE::Net::Private
{

static FTestMessage& PrintNameNetSerializerConfig(FTestMessage& Message, const FNetSerializerConfig& InConfig)
{
	return Message;
}

class FTestNameNetSerializer : public TTestNetSerializerFixture<PrintNameNetSerializerConfig, FName>
{
public:
	FTestNameNetSerializer() : Super(UE_NET_GET_SERIALIZER(FNameNetSerializer)) {}

	void TestValidate();
	void TestQuantize();
	void TestIsEqual();
	void TestSerialize();
	void TestCloneDynamicState();

protected:
	typedef TTestNetSerializerFixture<PrintNameNetSerializerConfig, FName> Super;

	static const FName TestNames[];
	static const SIZE_T TestNameCount;

	// Serializer
	static FNameNetSerializerConfig SerializerConfig;
};

const FName FTestNameNetSerializer::TestNames[] = 
{
	// Various types of "empty" names
	FName(),
	FName(NAME_None),
	FName(""),
	// EName string
	FName(NAME_Actor),
	// EName with number
	FName(NAME_Actor, 2),
	// Pure ASCII string
	FName("Just a regular ASCII string", FNAME_Add),
	// Copy of above string, but unique!
	FName("Just a regular ASCII string", FNAME_Add),
	// Smiling face with open mouth and tightly-closed eyes, four of circles, euro, copyright
	FName(StringCast<WIDECHAR>(FUTF8ToTCHAR("\xf0\x9f\x98\x86\xf0\x9f\x80\x9c\xe2\x82\xac\xc2\xa9").Get()).Get()),
};

const SIZE_T FTestNameNetSerializer::TestNameCount = sizeof(TestNames)/sizeof(TestNames[0]);

FNameNetSerializerConfig FTestNameNetSerializer::SerializerConfig;

UE_NET_TEST_FIXTURE(FTestNameNetSerializer, TestValidate)
{
	TestValidate();
}

UE_NET_TEST_FIXTURE(FTestNameNetSerializer, TestQuantize)
{
	TestQuantize();
}

UE_NET_TEST_FIXTURE(FTestNameNetSerializer, TestIsEqual)
{
	TestIsEqual();
}

UE_NET_TEST_FIXTURE(FTestNameNetSerializer, TestSerialize)
{
	TestSerialize();
}

UE_NET_TEST_FIXTURE(FTestNameNetSerializer, TestCloneDynamicState)
{
	TestCloneDynamicState();
}

void FTestNameNetSerializer::TestValidate()
{
	{
		TArray<bool> ExpectedResults;
		ExpectedResults.Init(true, TestNameCount);

		const bool bSuccess = Super::TestValidate(TestNames, ExpectedResults.GetData(), TestNameCount, SerializerConfig);
		if (!bSuccess)
		{
			return;
		}
	}
}

void FTestNameNetSerializer::TestQuantize()
{
	const bool bSuccess = Super::TestQuantize(TestNames, TestNameCount, SerializerConfig);
	if (!bSuccess)
	{
		return;
	}
}

void FTestNameNetSerializer::TestIsEqual()
{
	TArray<FName> CompareValues[2];
	TArray<bool> ExpectedResults[2];

	CompareValues[0] = TArray<FName>(TestNames, TestNameCount);
	ExpectedResults[0].Init(true, TestNameCount);

	CompareValues[1].Reserve(TestNameCount);
	ExpectedResults[1].Reserve(TestNameCount);
	for (int32 ValueIt = 0, ValueEndIt = TestNameCount; ValueIt != ValueEndIt; ++ValueIt)
	{
		CompareValues[1].Add(TestNames[(ValueIt + 1) % ValueEndIt]);
		ExpectedResults[1].Add(TestNames[ValueIt].IsEqual(TestNames[(ValueIt + 1) % ValueEndIt], ENameCase::CaseSensitive, true));
	}

	// Do two rounds of testing per config, one where we compare each value with itself and one where we compare against a value in range.
	for (SIZE_T TestRoundIt : {0, 1})
	{
		// Do both quantized and regular compares
		for (SIZE_T CompareIt : {0, 1})
		{
			bool bQuantizedCompare = CompareIt == 0;
			const bool bSuccess = Super::TestIsEqual(TestNames, CompareValues[TestRoundIt].GetData(), ExpectedResults[TestRoundIt].GetData(), TestNameCount, SerializerConfig, bQuantizedCompare);
			if (!bSuccess)
			{
				return;
			}
		}
	}
}

void FTestNameNetSerializer::TestSerialize()
{
	constexpr bool bQuantizedCompare = false;
	const bool bSuccess = Super::TestSerialize(TestNames, TestNames, TestNameCount, SerializerConfig, bQuantizedCompare);
	if (!bSuccess)
	{
		return;
	}
}

void FTestNameNetSerializer::TestCloneDynamicState()
{
	const bool bSuccess = Super::TestCloneDynamicState(TestNames, TestNameCount, SerializerConfig);
	if (!bSuccess)
	{
		return;
	}
}

}
