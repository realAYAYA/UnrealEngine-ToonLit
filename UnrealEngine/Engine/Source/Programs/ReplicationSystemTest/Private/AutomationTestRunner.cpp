// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutomationTestRunner.h"
#include "ReplicationSystemTestPlugin/NetworkAutomationTest.h"

#if WITH_AUTOMATION_WORKER

namespace UE::Net
{

FAutomationTestRunner::FAutomationTestRunner()
{
}

void
FAutomationTestRunner::RunTests(const TCHAR* TestFilter)
{
	constexpr int32 ExpectedTestCount = 2048;

	TArray<FAutomationTestInfo> TestInfos;
	TestInfos.Empty(ExpectedTestCount);

	FAutomationTestFramework& TestFramework = FAutomationTestFramework::Get();
	TestFramework.SetRequestedTestFilter(EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::EngineFilter);
	TestFramework.GetValidTestNames(TestInfos);
	int TestCount = TestInfos.Num();
	if (TestCount <= 0)
		return;

	// Stack walking doesn't work properly on Windows when omitting frame pointers. See WindowsPlatformStackWalk.cpp.
	const bool bCaptureStack = TestFramework.GetCaptureStack();
	TestFramework.SetCaptureStack(false);

	const FString TestPrefix("Net.");
	const double TestStartTime = FPlatformTime::Seconds();
	for (int TestIt = 0; TestIt != TestCount; ++TestIt)
	{
		const FAutomationTestInfo& TestInfo = TestInfos[TestIt];
		const FString& TestCommand = TestInfo.GetTestName();
		if (!TestCommand.StartsWith(TestPrefix) && !TestInfo.GetDisplayName().StartsWith(TestPrefix))
			continue;

		if (TestFilter && !TestCommand.Contains(TestFilter))
		{
			continue;
		}

		constexpr int32 RoleIndex = 0;
		TestFramework.StartTestByName(TestCommand, RoleIndex);
	}
	const double TestEndTime = FPlatformTime::Seconds();

	const double TestTime = TestEndTime - TestStartTime;
	UE_LOG(LogNetworkAutomationTest, Display, TEXT("Tests took %.3lf seconds to execute"), TestTime);

	FAutomationTestExecutionInfo ExecutionInfo;
	TestFramework.StopTest(ExecutionInfo);
	TestFramework.SetCaptureStack(bCaptureStack);

	PrintNetworkAutomationTestSummary();
}

}

#endif
