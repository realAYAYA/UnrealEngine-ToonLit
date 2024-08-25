// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTraceInsightsUnitTest, "System.Insights.Trace.Analysis.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTraceInsightsUnitTest::RunTest(const FString& Parameters)
{
	return true;
}