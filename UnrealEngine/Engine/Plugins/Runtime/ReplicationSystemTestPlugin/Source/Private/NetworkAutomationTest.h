// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"
#include "ReplicationSystemTestPlugin/NetworkAutomationTest.h"

namespace UE::Net
{

struct FNetworkAutomationTestConfig
{
	static void SetVerboseLogging(bool bVerbose) { bVerboseLogging = bVerbose; }
	static bool GetVerboseLogging() { return bVerboseLogging; }

private:
	static bool bVerboseLogging;
};

class FNetworkAutomationTestSuiteFixture
{
public:
	virtual ~FNetworkAutomationTestSuiteFixture();

	virtual const TCHAR* GetName() const = 0;

	void RunTest();

protected:
	FNetworkAutomationTestSuiteFixture();

	// Run just before test case
	virtual void SetUp();

	// Run just after test case
	virtual void TearDown();

	virtual void RunTestImpl() = 0;

	// Stats
	void SetSuppressWarningsFromSummary(bool bSuppress);

	void AddTestFailure();
	void AddTestWarning();

private:
	struct FNetworkAutomationTestStats* Stats;
	bool bSuppressWarningsFromSummary;
};

#if WITH_AUTOMATION_WORKER

class FNetworkAutomationTestWrapper : private FAutomationTestBase
{
public:
	FNetworkAutomationTestWrapper(FNetworkAutomationTestSuiteFixture& TestSuite, const TCHAR* Name);

private:
	virtual uint32 GetTestFlags() const override;
	virtual uint32 GetRequiredDeviceNum() const override;

	virtual FString GetBeautifiedTestName() const override;

	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const override;
	virtual bool RunTest(const FString&) override;

private:
	FNetworkAutomationTestSuiteFixture& TestSuite;
};

#endif

}

#if WITH_AUTOMATION_WORKER

#define UE_NET_TEST_INTERNAL(TestSuite, TestCase, BaseClass) \
class TestSuite ## _ ## TestCase : public BaseClass \
{ \
public: \
	TestSuite ## _ ## TestCase(); \
	virtual const TCHAR* GetName() const override; \
private: \
	virtual void RunTestImpl() override; \
}; \
TestSuite ## _ ## TestCase :: TestSuite ## _ ## TestCase () \
{ \
} \
const TCHAR* \
TestSuite ## _ ## TestCase :: GetName() const \
{ \
	return TEXT("Net." #TestSuite "." #TestCase); \
} \
static TestSuite ## _ ## TestCase TestSuite ## _ ## TestCase ##Instance; \
static UE::Net::FNetworkAutomationTestWrapper TestSuite ## _ ## TestCase ##AutomationTestWrapperInstance(TestSuite ## _ ## TestCase ##Instance, TestSuite ## _ ## TestCase ##Instance .GetName()); \
void TestSuite ## _ ## TestCase :: RunTestImpl()

#define UE_NET_TEST(TestSuite, TestCase) UE_NET_TEST_INTERNAL(TestSuite, TestCase, UE::Net::FNetworkAutomationTestSuiteFixture)
#define UE_NET_TEST_FIXTURE(TestFixture, TestCase) UE_NET_TEST_INTERNAL(TestFixture, TestCase, TestFixture)

#else

#define UE_NET_TEST_INTERNAL(TestSuite, TestCase, BaseClass) \
class TestSuite ## _ ## TestCase : private BaseClass \
{ \
private: \
	void DummyFunctionThatWillNeverBeCalled(); \
}; \
void TestSuite ## _ ## TestCase :: DummyFunctionThatWillNeverBeCalled()

#define UE_NET_TEST(TestSuite, TestCase) UE_NET_TEST_INTERNAL(TestSuite, TestCase, FNetworkAutomationTestSuiteFixture)
#define UE_NET_TEST_FIXTURE(TestFixture, TestCase) UE_NET_TEST_INTERNAL(TestFixture, TestCase, TestFixture)

#endif
