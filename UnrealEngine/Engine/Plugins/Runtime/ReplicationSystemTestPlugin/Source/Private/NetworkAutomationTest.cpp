// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "ReplicationSystemTestPlugin/NetworkAutomationTest.h"

namespace UE::Net
{

//
bool FNetworkAutomationTestConfig::bVerboseLogging = false;

struct FNetworkAutomationTestStats
{
	SIZE_T FailureCount;
	SIZE_T WarningCount;
};

class FNetworkAutomationTestStatsSummary
{
public:
	FNetworkAutomationTestStatsSummary();

	void AddTestResult(const TCHAR* TestName, const FNetworkAutomationTestStats& Stats);

private:
	friend void PrintNetworkAutomationTestSummary();

	TMap<const TCHAR*, FNetworkAutomationTestStats> TestsWithFailures;
	TMap<const TCHAR*, FNetworkAutomationTestStats> TestsWithWarnings;
	SIZE_T TestCount;
	SIZE_T SuccessCount;
};

static FNetworkAutomationTestStatsSummary NetworkAutomationTestStatsSummary;

// 
FNetworkAutomationTestSuiteFixture::FNetworkAutomationTestSuiteFixture()
: Stats(nullptr)
, bSuppressWarningsFromSummary(false)
{
}

FNetworkAutomationTestSuiteFixture::~FNetworkAutomationTestSuiteFixture()
{
}

void FNetworkAutomationTestSuiteFixture::SetUp()
{
}

void FNetworkAutomationTestSuiteFixture::TearDown()
{
}

void FNetworkAutomationTestSuiteFixture::RunTest()
{
	if (FNetworkAutomationTestConfig::GetVerboseLogging())
	{
		UE_LOG(LogNetworkAutomationTest, Display, TEXT("Running TestCase %ls"), GetName());
	}

	FNetworkAutomationTestStats TempStats = {};
	Stats = &TempStats;

	SetUp();
	RunTestImpl();
	TearDown();

	NetworkAutomationTestStatsSummary.AddTestResult(GetName(), *Stats);
	Stats = nullptr;
}

void FNetworkAutomationTestSuiteFixture::SetSuppressWarningsFromSummary(bool bSuppress)
{
	bSuppressWarningsFromSummary = bSuppress;
}

void FNetworkAutomationTestSuiteFixture::AddTestFailure()
{
	++Stats->FailureCount;
}

void FNetworkAutomationTestSuiteFixture::AddTestWarning()
{
	Stats->WarningCount += !bSuppressWarningsFromSummary;
}

#if WITH_AUTOMATION_WORKER

FNetworkAutomationTestWrapper::FNetworkAutomationTestWrapper(FNetworkAutomationTestSuiteFixture& InTestSuite, const TCHAR* Name)
: FAutomationTestBase(Name, false /* bIsComplex */)
, TestSuite(InTestSuite)
{
}

uint32 FNetworkAutomationTestWrapper::GetTestFlags() const
{
	return EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext;
}

uint32 FNetworkAutomationTestWrapper::GetRequiredDeviceNum() const
{
	return 1;
}

FString FNetworkAutomationTestWrapper::GetBeautifiedTestName() const
{
	return TestName;
}

void FNetworkAutomationTestWrapper::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TestName);
	OutTestCommands.Add(TestName);
}

bool FNetworkAutomationTestWrapper::RunTest(const FString&)
{
	TestSuite.RunTest();
	return true;
}

#else

#endif

//
FNetworkAutomationTestStatsSummary::FNetworkAutomationTestStatsSummary()
: TestCount(0)
, SuccessCount(0)
{
}

void FNetworkAutomationTestStatsSummary::AddTestResult(const TCHAR* TestName, const FNetworkAutomationTestStats& Stats)
{
	++TestCount;
	SuccessCount += (Stats.FailureCount == 0 ? 1U : 0U);

	if (Stats.FailureCount)
	{
		TestsWithFailures.Add(TestName, Stats);
	}
	else if (Stats.WarningCount)
	{
		TestsWithWarnings.Add(TestName, Stats);
	}
}

void PrintNetworkAutomationTestSummary()
{
	UE_LOG(LogNetworkAutomationTest, Display, TEXT("\n-----------------------------\nNetworkAutomationTest Summary\n-----------------------------\nTests executed: %zu\nTests succeeded: %zu\nTests failed: %zu\nTests with warnings: %zu\n-----------------------------"), NetworkAutomationTestStatsSummary.TestCount, NetworkAutomationTestStatsSummary.SuccessCount, SIZE_T(NetworkAutomationTestStatsSummary.TestsWithFailures.Num()), SIZE_T(NetworkAutomationTestStatsSummary.TestsWithWarnings.Num()));

	if (NetworkAutomationTestStatsSummary.TestsWithFailures.Num())
	{
		for (auto It = NetworkAutomationTestStatsSummary.TestsWithFailures.CreateConstIterator(); It; ++It)
		{
			const TCHAR* Name = It.Key();
			const FNetworkAutomationTestStats& Stats = It.Value();
			UE_LOG(LogNetworkAutomationTest, Display, TEXT("Test %ls failed with %zu errors and %zu warnings"), Name, Stats.FailureCount, Stats.WarningCount);
		}
	}

	if (NetworkAutomationTestStatsSummary.TestsWithWarnings.Num())
	{
		for (auto It = NetworkAutomationTestStatsSummary.TestsWithWarnings.CreateConstIterator(); It; ++It)
		{
			const TCHAR* Name = It.Key();
			const FNetworkAutomationTestStats& Stats = It.Value();
			UE_LOG(LogNetworkAutomationTest, Display, TEXT("Test %ls had %zu warnings"), Name, Stats.WarningCount);
		}
	}
}

}
