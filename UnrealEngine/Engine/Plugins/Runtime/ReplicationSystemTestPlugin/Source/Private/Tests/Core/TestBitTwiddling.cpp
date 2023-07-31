// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/Core/BitTwiddling.h"
#include "Math/NumericLimits.h"

namespace UE::Net::Private
{

// GetBitsNeeded
UE_NET_TEST(TestGetBitsNeeded, ReturnsExpectedValuesForInt8)
{
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<int8>::Lowest()), 8U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int8(-64)), 7U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int8(-1)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int8(0)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int8(1)), 2U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<int8>::Max()), 8U);
}

UE_NET_TEST(TestGetBitsNeeded, ReturnsExpectedValuesForInt32)
{
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<int32>::Lowest()), 32U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int32(-64)), 7U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int32(-1)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int32(0)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int32(1)), 2U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<int32>::Max()), 32U);
}

UE_NET_TEST(TestGetBitsNeeded, ReturnsExpectedValuesForInt64)
{
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<int64>::Lowest()), 64U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int64(-64)), 7U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int64(-1)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int64(0)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(int64(1)), 2U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<int64>::Max()), 64U);
}

UE_NET_TEST(TestGetBitsNeeded, ReturnsExpectedValuesForUint8)
{
	UE_NET_ASSERT_EQ(GetBitsNeeded(uint8(0)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(uint8(1)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<uint8>::Max()), 8U);
}

UE_NET_TEST(TestGetBitsNeeded, ReturnsExpectedValuesForUint32)
{
	UE_NET_ASSERT_EQ(GetBitsNeeded(uint32(0)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(uint32(1)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<uint32>::Max()), 32U);
}

UE_NET_TEST(TestGetBitsNeeded, ReturnsExpectedValuesForUint64)
{
	UE_NET_ASSERT_EQ(GetBitsNeeded(uint64(0)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(uint64(1)), 1U);
	UE_NET_ASSERT_EQ(GetBitsNeeded(TNumericLimits<uint64>::Max()), 64U);
}

// GetBitsNeededForRange
UE_NET_TEST(TestGetBitsNeededForRange, ReturnsExpectedValuesForInt8)
{
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int8(47), int8(47)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_int8, int8(-1)), 7U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int8(MIN_int8 + 5), int8(4)), 7U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_int8, int8(0)), 8U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int8(11), int8(47)), 6U);
}

UE_NET_TEST(TestGetBitsNeededForRange, ReturnsExpectedValuesForInt16)
{
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int16(4711), int16(4711)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_int16, int16(-1)), 15U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int16(MIN_int16 + 5), int16(4)), 15U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_int16, int16(0)), 16U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int16(11), int16(47)), 6U);
}

UE_NET_TEST(TestGetBitsNeededForRange, ReturnsExpectedValuesForInt32)
{
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int32(4711), int32(4711)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_int32, int32(-1)), 31U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int32(MIN_int32 + 5), int32(4)), 31U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_int32, int32(0)), 32U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int32(11), int32(47)), 6U);
}

UE_NET_TEST(TestGetBitsNeededForRange, ReturnsExpectedValuesForInt64)
{
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int64(4711), int64(4711)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_int64, int64(-1)), 63U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int64(MIN_int64 + 5), int64(4)), 63U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_int64, int64(0)), 64U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(int64(11), int64(47)), 6U);
}

UE_NET_TEST(TestGetBitsNeededForRange, ReturnsExpectedValuesForUint8)
{
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(uint8(47), uint8(47)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(uint8(MAX_uint8/2U + 1U), MAX_uint8), 7U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_uint8, MAX_uint8), 8U);
}

UE_NET_TEST(TestGetBitsNeededForRange, ReturnsExpectedValuesForUint16)
{
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(uint16(4711), uint16(4711)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(uint16(MAX_uint16/2U + 1U), MAX_uint16), 15U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_uint16, MAX_uint16), 16U);
}

UE_NET_TEST(TestGetBitsNeededForRange, ReturnsExpectedValuesForUint32)
{
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(uint32(4711), uint32(4711)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(uint32(MAX_uint32/2U + 1U), MAX_uint32), 31U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_uint32, MAX_uint32), 32U);
}

UE_NET_TEST(TestGetBitsNeededForRange, ReturnsExpectedValuesForUint64)
{
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(uint64(4711), uint64(4711)), 0U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(uint64(MAX_uint64/2U + 1U), MAX_uint64), 63U);
	UE_NET_ASSERT_EQ(GetBitsNeededForRange(MIN_uint64, MAX_uint64), 64U);
}

}