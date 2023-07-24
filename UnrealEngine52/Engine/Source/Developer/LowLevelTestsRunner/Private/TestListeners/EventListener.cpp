// Copyright Epic Games, Inc. All Rights Reserved.

#include "Logging/LogMacros.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
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
private:
	FDateTime TestStarted;
public:
	using EventListenerBase::EventListenerBase;

	void testCaseStarting(const Catch::TestCaseInfo& TestInfo) final
	{
		if (ITestRunner* TestRunner = ITestRunner::Get())
		{
			TestStarted = FDateTime::Now();
			UE_CLOG(TestRunner->IsDebugMode(), LogLowLevelTests, Display,
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
			FTimespan Duration = FDateTime::Now() - TestStarted;
			
			TStringBuilder<64> DurationText;
			const int32 Hours = Duration.GetHours();
			const int32 Minutes = Duration.GetMinutes();
			const double Seconds = Duration.GetTotalSeconds();
			if (Hours > 0)
			{
				DurationText << Hours << TEXTVIEW("h ");
			}
			if (Hours > 0 || Minutes > 0)
			{
				DurationText << Minutes << TEXTVIEW("m ");
			}
			if (Hours > 0 || Minutes > 0 || Seconds >= 1.0)
			{
				DurationText.Appendf(TEXT("%.3fs "), Seconds);
			}
			else
			{
				DurationText << Duration.GetFractionMilli() << TEXTVIEW("ms");
			}

			UE_CLOG(TestRunner->IsDebugMode(), LogLowLevelTests, Display,
				TEXT("Test took %s"),
				*DurationText);

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
