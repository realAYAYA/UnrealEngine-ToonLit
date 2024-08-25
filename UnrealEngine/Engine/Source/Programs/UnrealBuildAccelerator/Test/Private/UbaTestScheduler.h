// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaTestSession.h"
#include "UbaScheduler.h"

namespace uba
{
	bool TestLocalSchedule(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				SchedulerCreateInfo info(session);
				Scheduler scheduler(info);

				ProcessStartInfo processInfo;
				processInfo.application = GetSystemApplication();
				processInfo.workingDir = workingDir;
				processInfo.arguments = GetSystemArguments();

				EnqueueProcessInfo epi(processInfo);
				scheduler.EnqueueProcess(epi);
				scheduler.Start();

				u32 queued, activeLocal, activeRemote, finished;
				do { scheduler.GetStats(queued, activeLocal, activeRemote, finished); } while (finished != 1);

				scheduler.Stop();
				return true;
			});
	}

	bool TestLocalScheduleReuse(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		if (!IsWindows)
			return true;

		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				SchedulerCreateInfo info(session);
				info.enableProcessReuse = true;
				Scheduler scheduler(info);

				StringBuffer<> testApp;
				GetTestAppPath(logger, testApp);

				ProcessStartInfo processInfo;
				processInfo.application = testApp.data;
				processInfo.workingDir = workingDir;
				processInfo.arguments = TC("-reuse");

				EnqueueProcessInfo epi(processInfo);
				scheduler.EnqueueProcess(epi);
				scheduler.Start();

				u32 queued, activeLocal, activeRemote, finished;
				do { scheduler.GetStats(queued, activeLocal, activeRemote, finished); } while (finished != 1);

				scheduler.Stop();
				return true;
			});
	}

	bool TestRemoteScheduleReuse(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		if (!IsWindows)
			return true;

		return RunRemote(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				SchedulerCreateInfo info(session);
				info.enableProcessReuse = true;
				info.maxLocalProcessors = 0;
				Scheduler scheduler(info);

				StringBuffer<> testApp;
				GetTestAppPath(logger, testApp);

				ProcessStartInfo processInfo;
				processInfo.application = testApp.data;
				processInfo.workingDir = workingDir;
				processInfo.arguments = TC("-reuse");

				EnqueueProcessInfo epi(processInfo);
				scheduler.EnqueueProcess(epi);
				scheduler.Start();

				u32 queued, activeLocal, activeRemote, finished;
				do { scheduler.GetStats(queued, activeLocal, activeRemote, finished); } while (finished != 1);

				scheduler.Stop();
				return true;
			});
	}
}
