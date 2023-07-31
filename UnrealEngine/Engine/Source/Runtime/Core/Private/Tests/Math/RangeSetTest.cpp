// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"

#include "Math/Range.h"
#include "Math/RangeSet.h"
#include "Misc/Timespan.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeSetTest, "System.Core.Math.RangeSet", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

bool FRangeSetTest::RunTest(const FString& Parameters)
{
	{
		TRangeSet<int32> RangeSet;
		RangeSet.Add(TRange<int32>::Inclusive(0, 1));
		RangeSet.Add(TRange<int32>::Inclusive(1, 2));
		RangeSet.Add(TRange<int32>::Inclusive(3, 4));

		int32 Value = RangeSet.GetMinBoundValue();
		if (Value != 0)
		{
			return false;
		}
		
		Value = RangeSet.GetMaxBoundValue();
		if (Value != 4)
		{
			return false;
		}
	}

	{
		TRangeSet<FTimespan> RangeSet;
		RangeSet.Add(TRange<FTimespan>::Inclusive(FTimespan(0), FTimespan(1)));
		RangeSet.Add(TRange<FTimespan>::Inclusive(FTimespan(1), FTimespan(2)));
		RangeSet.Add(TRange<FTimespan>::Inclusive(FTimespan(3), FTimespan(4)));

		FTimespan Value = RangeSet.GetMinBoundValue();
		if (Value != FTimespan::Zero())
		{
			return false;
		}

		Value = RangeSet.GetMaxBoundValue();
		if (Value != FTimespan(4))
		{
			return false;
		}
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
