// Copyright Epic Games, Inc. All Rights Reserved.

#include "Logging/LogMacros.h"
#include "TestRunnerPrivate.h"

#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/reporters/catch_reporter_streaming_base.hpp>

namespace UE::LowLevelTests
{

DEFINE_LOG_CATEGORY_STATIC(LogLowLevelTests, Log, VeryVerbose);

class FEventListener final : public Catch::EventListenerBase
{
public:
	using EventListenerBase::EventListenerBase;

	void testCaseStarting(const Catch::TestCaseInfo& TestInfo) final
	{
		if (ITestRunner* TestRunner = ITestRunner::Get())
		{
			UE_CLOG(TestRunner->IsDebugMode(), LogLowLevelTests, Log,
				TEXT("Started test case \"%hs\" at %hs(%" SIZE_T_FMT ") with tags %hs"),
				TestInfo.name.c_str(),
				TestInfo.lineInfo.file,
				SIZE_T(TestInfo.lineInfo.line),
				TestInfo.tagsAsString().c_str());
		}
	}

	void testCaseEnded(const Catch::TestCaseStats& TestCaseStats) final
	{
		if (ITestRunner* TestRunner = ITestRunner::Get())
		{
			UE_CLOG(TestRunner->IsDebugMode() && TestCaseStats.totals.testCases.failed > 0,
				LogLowLevelTests, Error, TEXT("Failed test case \"%hs\" at %hs(%" SIZE_T_FMT ")"),
				TestCaseStats.testInfo->name.c_str(),
				TestCaseStats.testInfo->lineInfo.file,
				SIZE_T(TestCaseStats.testInfo->lineInfo.line));
		}
	}

	void assertionEnded(const Catch::AssertionStats& AssertionStats) final
	{
		if (ITestRunner* TestRunner = ITestRunner::Get())
		{
			UE_CLOG(TestRunner->HasLogOutput() && !AssertionStats.assertionResult.succeeded(),
				LogLowLevelTests, Error, TEXT("Assertion \"%hs\" failed at %hs(%" SIZE_T_FMT ")"),
				AssertionStats.assertionResult.getExpression().c_str(),
				AssertionStats.assertionResult.getSourceInfo().file,
				SIZE_T(AssertionStats.assertionResult.getSourceInfo().line));
		}
	}
};

CATCH_REGISTER_LISTENER(FEventListener);

} // UE::LowLevelTests
