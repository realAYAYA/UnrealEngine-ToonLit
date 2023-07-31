// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestNetSerializerFixture.h"
#include "Iris/Serialization/RotatorNetSerializers.h"
#include <limits>

namespace UE::Net
{

FTestMessage& operator<<(FTestMessage& Message, const FRotator& Rotator)
{
	return Message << Rotator.ToString();
}

}

namespace UE::Net::Private
{

static FTestMessage& PrintRotatorNetSerializerConfig(FTestMessage& Message, const FNetSerializerConfig& InConfig)
{
	return Message;
}

class FTestRotatorNetSerializerBase : public TTestNetSerializerFixture<PrintRotatorNetSerializerConfig, FRotator>
{
private:
	typedef TTestNetSerializerFixture<PrintRotatorNetSerializerConfig, FRotator> Super;

public:
	FTestRotatorNetSerializerBase(const FNetSerializer& Serializer) : Super(Serializer) {}

	void TestSerialize();
	void TestSerializeDelta();
	void TestIsEqual();
	void TestValidate();

protected:
	using ScalarType = decltype(FRotator::Pitch);

	struct FFloatTriplet
	{
		ScalarType X;
		ScalarType Y;
		ScalarType Z;
	};

	static const FFloatTriplet BadValues[];
	static const SIZE_T BadValueCount;
	static const FRotator Values[];
	static const SIZE_T ValueCount;
	float ValueTolerance = 0.0f;
	const FNetSerializerConfig* NetSerializerConfig = nullptr;
};

// In case there's any sort of sanity checking in the FRotator constructors this will avoid them.
const FTestRotatorNetSerializerBase::FFloatTriplet FTestRotatorNetSerializerBase::BadValues[] =
{
	{-INFINITY, NAN, -NAN},
	{-INFINITY, INFINITY, -INFINITY},
	{NAN, NAN, NAN},
	{-NAN, -NAN, -NAN},
	{-1.0f, 0.0f, 0.0f},
	{0.0f, -1.0f, 0.0f},
	{0.0f, 0.0f, -1.0f},
	{5.0f, 6.0f, 360.0f},
	{std::numeric_limits<ScalarType>::max(), std::numeric_limits<ScalarType>::max(), std::numeric_limits<ScalarType>::max()},
	{std::numeric_limits<ScalarType>::lowest(), std::numeric_limits<ScalarType>::lowest(), std::numeric_limits<ScalarType>::lowest()},
};
const SIZE_T FTestRotatorNetSerializerBase::BadValueCount = sizeof(BadValues)/sizeof(BadValues[0]);

const FRotator FTestRotatorNetSerializerBase::Values[] =
{
	FRotator(0.0f),
	FRotator(1.0f, 0.0f, 0.0f),
	FRotator(0.0f, 1.0f, 0.0f),
	FRotator(0.0f, 0.0f, 1.0f),
	FRotator(-0.0f), // 0.0f == -0.0f so we expect this to be handled gracefully
	FRotator(359.999f, 129.130f, 45.0f),
	FRotator(1.26698556E-23),
	FRotator(47.11f),
};
const SIZE_T FTestRotatorNetSerializerBase::ValueCount = sizeof(Values)/sizeof(Values[0]);

class FTestRotatorAsShortNetSerializer : public FTestRotatorNetSerializerBase
{
private:
	typedef FTestRotatorNetSerializerBase Super;

public:
	FTestRotatorAsShortNetSerializer()
	: Super(UE_NET_GET_SERIALIZER(FRotatorAsShortNetSerializer))
	{
		ValueTolerance = 1.5f*(360.0f/65536.0f);
		NetSerializerConfig = &RotatorAsShortNetSerializerConfig;
	}

private:
	inline static const FRotatorAsShortNetSerializerConfig RotatorAsShortNetSerializerConfig;
};

class FTestRotatorAsByteNetSerializer : public FTestRotatorNetSerializerBase
{
private:
	typedef FTestRotatorNetSerializerBase Super;

public:
	FTestRotatorAsByteNetSerializer()
	: Super(UE_NET_GET_SERIALIZER(FRotatorAsByteNetSerializer))
	{
		ValueTolerance = 1.5f*(360.0f/256.0f);
		NetSerializerConfig = &RotatorAsByteNetSerializerConfig;
	}

private:
	inline static const FRotatorAsByteNetSerializerConfig RotatorAsByteNetSerializerConfig;
};

// FRotatorAsShortNetSerializer tests
UE_NET_TEST_FIXTURE(FTestRotatorAsShortNetSerializer, TestIsEqual)
{
	TestIsEqual();
}

UE_NET_TEST_FIXTURE(FTestRotatorAsShortNetSerializer, TestSerialize)
{
	TestSerialize();
}

UE_NET_TEST_FIXTURE(FTestRotatorAsShortNetSerializer, TestSerializeDelta)
{
	TestSerializeDelta();
}

UE_NET_TEST_FIXTURE(FTestRotatorAsShortNetSerializer, TestValidate)
{
	TestValidate();
}

// FRotatorAsByteNetSerializer tests
UE_NET_TEST_FIXTURE(FTestRotatorAsByteNetSerializer, TestIsEqual)
{
	TestIsEqual();
}

UE_NET_TEST_FIXTURE(FTestRotatorAsByteNetSerializer, TestSerialize)
{
	TestSerialize();
}

UE_NET_TEST_FIXTURE(FTestRotatorAsByteNetSerializer, TestSerializeDelta)
{
	TestSerializeDelta();
}

UE_NET_TEST_FIXTURE(FTestRotatorAsByteNetSerializer, TestValidate)
{
	TestValidate();
}

void FTestRotatorNetSerializerBase::TestIsEqual()
{
	FRotator CompareValues[2][sizeof(Values)/sizeof(Values[0])];
	bool ExpectedResults[2][sizeof(Values)/sizeof(Values[0])];

	for (SIZE_T ValueIt = 0; ValueIt < ValueCount; ++ValueIt)
	{
		// We need to memcopy the data to avoid any validation/fixing in the constructor
		FPlatformMemory::Memcpy(&CompareValues[0][ValueIt], &Values[ValueIt], sizeof(FRotator));
		ExpectedResults[0][ValueIt] = true;
	}
	for (SIZE_T ValueIt = 0; ValueIt < ValueCount; ++ValueIt)
	{
		FPlatformMemory::Memcpy(&CompareValues[1][ValueIt], &Values[(ValueIt + 1) % ValueCount], sizeof(FRotator));
		ExpectedResults[1][ValueIt] = false;
	}

	for (SIZE_T TestRoundIt = 0, TestRoundEndIt = 2; TestRoundIt != TestRoundEndIt; ++TestRoundIt)
	{
		constexpr bool bQuantizedCompare = true;
		bool bSuccess = Super::TestIsEqual(Values, CompareValues[TestRoundIt], ExpectedResults[TestRoundIt], ValueCount, *NetSerializerConfig, bQuantizedCompare);
		if (!bSuccess)
		{
			return;
		}
	}
}

void FTestRotatorNetSerializerBase::TestSerialize()
{
	constexpr bool bQuantizedCompare = true;
	Super::TestSerialize(Values, Values, ValueCount, *NetSerializerConfig, bQuantizedCompare);
}

void FTestRotatorNetSerializerBase::TestSerializeDelta()
{
	Super::TestSerializeDelta(Values, ValueCount, *NetSerializerConfig);
}

void FTestRotatorNetSerializerBase::TestValidate()
{
	// Test bad values
	{
		static_assert(sizeof(BadValues[0]) == sizeof(FRotator), "BadValues and FRotator type mismatch.");
		bool ExpectedResults[BadValueCount] = {};
		const bool bSuccess = Super::TestValidate(reinterpret_cast<const FRotator*>(BadValues), ExpectedResults, BadValueCount, *NetSerializerConfig);
		if (!bSuccess)
		{
			return;
		}
	}

	// Test conforming values
	{
		bool ExpectedResults[ValueCount];
		for (SIZE_T ValueIt = 0; ValueIt < ValueCount; ++ValueIt)
		{
			ExpectedResults[ValueIt] = true;
		}

		const bool bSuccess = Super::TestValidate(Values, ExpectedResults, ValueCount, *NetSerializerConfig);
		if (!bSuccess)
		{
			return;
		}
	}
}

}
