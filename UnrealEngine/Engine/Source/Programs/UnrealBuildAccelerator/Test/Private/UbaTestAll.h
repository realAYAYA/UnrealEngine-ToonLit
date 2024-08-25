// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaTestBasics.h"
#include "UbaTestNetwork.h"
#include "UbaTestScheduler.h"
#include "UbaTestSession.h"
#include "UbaTestStorage.h"
#include "UbaTestStress.h"

namespace uba
{
#if PLATFORM_MAC
	#define UBA_EXTRA_TESTS
#else
	#define UBA_EXTRA_TESTS \
		UBA_TEST(TestMultipleDetouredProcesses) \
		UBA_TEST(TestLogLines) \
		UBA_TEST(TestLogLinesNoDetour) \
		UBA_TEST(TestLocalSchedule) \
		UBA_TEST(TestLocalScheduleReuse) \
		UBA_TEST(TestDetouredTouch) \
		UBA_TEST(TestRemoteScheduleReuse) \

#endif 

	#define UBA_TESTS \
		UBA_TEST(TestTime) \
		UBA_TEST(TestEvents) \
		UBA_TEST(TestPaths) \
		UBA_TEST(TestFiles) \
		UBA_TEST(TestMemoryBlock) \
		UBA_TEST(TestParseArguments) \
		UBA_TEST(TestBinaryWriter) \
		UBA_TEST(TestSockets) \
		UBA_TEST(TestClientServer) \
		UBA_TEST(TestClientServer2) \
		UBA_TEST(TestClientServerMem) \
		UBA_TEST(TestStorage) \
		UBA_TEST(TestDetouredTestApp) \
		UBA_TEST(TestDetouredClang) \
		UBA_TEST(TestRemoteDetouredTestApp) \
		UBA_TEST(TestCustomService) \
		UBA_EXTRA_TESTS


	#define UBA_TEST(x) \
		if (!filter || Contains(TC(#x), filter)) \
		{ \
			logger.Info(TC("Running %s..."), TC(#x)); \
			if (!x(testLogger, testRootDir)) \
				return logger.Error(TC("  %s failed"), TC(#x)); \
			logger.Info(TC("  %s success!"),  TC(#x)); \
		}

	bool RunTests(int argc, tchar* argv[])
	{
		LoggerWithWriter logger(g_consoleLogWriter, TC(""));

		FilteredLogWriter filteredWriter(g_consoleLogWriter, LogEntryType_Warning);
		LoggerWithWriter testLogger(filteredWriter, TC("   "));
		//LoggerWithWriter& testLogger = logger;

		StringBuffer<512> testRootDir;

		#if PLATFORM_WINDOWS
		StringBuffer<> temp;
		temp.count = GetTempPathW(temp.capacity, temp.data);
		testRootDir.count = GetLongPathNameW(temp.data, testRootDir.data, testRootDir.capacity);
		testRootDir.EnsureEndsWithSlash().Append(L"UbaTest");
		#else
		testRootDir.count = GetFullPathNameW("~/UbaTest", testRootDir.capacity, testRootDir.data, nullptr);
		#endif
		testRootDir.EnsureEndsWithSlash();

		logger.Info(TC("Running tests (Test rootdir: %s)"), testRootDir.data);

		const tchar* filter = nullptr;
		if (argc > 1)
			filter = argv[1];

		//UBA_TEST(TestStress) // This can not be submitted.. it depends on CoordinatorHorde and credentials
		UBA_TESTS

		logger.Info(TC("Tests finished successfully!"));
		Sleep(3000);

		return true;
	}
}
