// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS
#pragma warning(disable:4121)
#pragma warning(disable:4668)
#pragma warning(disable:4996)
#pragma warning(disable:6330)
#include "gtest/gtest.h"
#pragma warning(default:4121)
#pragma warning(default:4668)
#pragma warning(default:4996)
#pragma warning(default:6330)

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

class FTestCollector : public ::testing::EmptyTestEventListener
{
public:
    explicit FTestCollector(TArray<FString>* Names) : TestNames{Names}
    {
    }

private:
    void OnTestStart(const ::testing::TestInfo& TestInfo) override
    {
        FString TestCaseName = FString::Printf(TEXT("%s.%s"), *FString(TestInfo.test_case_name()), *FString(TestInfo.name()));
        TestNames->Add(TestCaseName);
    }

private:
    TArray<FString>* TestNames;
};

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FRigLogicLibTestSuite, "ControlRig.Units.FRigUnit_RigLogicLib", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FRigLogicLibTestSuite::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
    FTestCollector Collector{&OutBeautifiedNames};
    ::testing::InitGoogleTest();
    ::testing::TestEventListeners& Listeners = ::testing::UnitTest::GetInstance()->listeners();
    Listeners.Append(&Collector);
    auto _ = RUN_ALL_TESTS();
    OutTestCommands = OutBeautifiedNames;
    Listeners.Release(&Collector);
}

bool FRigLogicLibTestSuite::RunTest(const FString& Parameters)
{
    TArray<ANSICHAR> TestFilter{TCHAR_TO_ANSI(*Parameters), Parameters.Len() + 1};
    ::testing::GTEST_FLAG(filter) = TestFilter.GetData();
    return (RUN_ALL_TESTS() == 0);
}

#endif  // WITH_DEV_AUTOMATION_TESTS
