// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaSessionServer.h"
#include "UbaSessionClient.h"
#include "UbaStorageServer.h"
#include "UbaStorageClient.h"

namespace uba
{
	using RunProcessFunction = Function<ProcessHandle(const ProcessStartInfo&)>;
	using TestSessionFunction = Function<bool(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)>;

	bool RunLocal(LoggerWithWriter& logger, const StringBufferBase& testRootDir, const TestSessionFunction& testFunc, bool enableDetour = true)
	{
		LogWriter& logWriter = logger.m_writer;

		StringBuffer<MaxPath> rootDir;
		rootDir.Append(testRootDir).Append(TC("Uba"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		StorageCreateInfo storageInfo(rootDir.data, logWriter);
		storageInfo.casCapacityBytes = 1024ull * 1024 * 1024;
		StorageImpl storage(storageInfo);

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, { logWriter });

		SessionServerCreateInfo sessionServerInfo(storage, server, logWriter);
		sessionServerInfo.checkMemory = false;
		sessionServerInfo.rootDir = rootDir.data;
		SessionServer session(sessionServerInfo);

		StringBuffer<MaxPath> workingDir;
		workingDir.Append(testRootDir).Append(TC("WorkingDir"));
		if (!DeleteAllFiles(logger, workingDir.data))
			return false;

		if (!storage.CreateDirectory(workingDir.data))
			return false;
		if (!DeleteAllFiles(logger, workingDir.data, false))
			return false;
		workingDir.EnsureEndsWithSlash();
		return testFunc(logger, session, workingDir.data, [&](const ProcessStartInfo& pi) { return session.RunProcess(pi, true, enableDetour); });
	}

	bool RunRemote(LoggerWithWriter& logger, const StringBufferBase& testRootDir, const TestSessionFunction& testFunc)
	{
		LogWriter& logWriter = logger.m_writer;
		NetworkBackendTcp tcpBackend(logWriter);

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, { logWriter });
		NetworkClient client(ctorSuccess, { logWriter });

		StringBuffer<MaxPath> rootDir;
		rootDir.Append(testRootDir).Append(TC("Uba"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		StorageServerCreateInfo storageServerInfo(server, rootDir.data, logWriter);
		storageServerInfo.casCapacityBytes = 1024ull * 1024 * 1024;
		auto& storageServer = *new StorageServer(storageServerInfo);
		auto ssg = MakeGuard([&]() { delete &storageServer; });

		SessionServerCreateInfo sessionServerInfo(storageServer, server, logWriter);
		sessionServerInfo.checkMemory = false;
		sessionServerInfo.rootDir = rootDir.data;
		auto& sessionServer = *new SessionServer(sessionServerInfo);
		auto ssg2 = MakeGuard([&]() { delete &sessionServer; });

		auto sg = MakeGuard([&]() { server.DisconnectClients(); });

		rootDir.Append(TC("Client"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		StorageClientCreateInfo storageClientInfo(client, rootDir.data);
		auto& storageClient = *new StorageClient(storageClientInfo);
		auto scg = MakeGuard([&]() { delete &storageClient; });

		SessionClientCreateInfo sessionClientInfo(storageClient, client, logWriter);
		sessionClientInfo.rootDir = rootDir.data;
		auto& sessionClient = *new SessionClient(sessionClientInfo);
		auto scg2 = MakeGuard([&]() { delete &sessionClient; });

		auto cg = MakeGuard([&]() { sessionClient.Stop(); client.Disconnect(); });

		StringBuffer<MaxPath> workingDir;
		workingDir.Append(testRootDir).Append(TC("WorkingDir"));
		if (!DeleteAllFiles(logger, workingDir.data))
			return false;
		if (!storageServer.CreateDirectory(workingDir.data))
			return false;
		if (!DeleteAllFiles(logger, workingDir.data, false))
			return false;

		storageClient.Start();
		sessionClient.Start();

		u16 port = 1356;
		if (!server.StartListen(tcpBackend, port))
			return logger.Error(TC("Failed to listen"));
		if (!client.Connect(tcpBackend, TC("127.0.0.1"), port))
			return logger.Error(TC("Failed to connect"));

		workingDir.EnsureEndsWithSlash();
		return testFunc(logger, sessionServer, workingDir.data, [&](const ProcessStartInfo& pi) { return sessionServer.RunProcessRemote(pi); });
	}

	void GetTestAppPath(LoggerWithWriter& logger, StringBufferBase& out)
	{
		GetDirectoryOfCurrentModule(logger, out);
		out.EnsureEndsWithSlash();
		out.Append(IsWindows ? TC("UbaTestApp.exe") : TC("UbaTestApp"));
	}

	bool RunTestApp(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		StringBuffer<MaxPath> testApp;
		GetTestAppPath(logger, testApp);

		{
			StringBuffer<MaxPath> fileR;
			fileR.Append(workingDir).Append(TC("FileR.h"));
			FileAccessor fr(logger, fileR.data);
			fr.CreateWrite();
			fr.Write("Foo", 4);
			fr.Close();
		}
		{
			StringBuffer<MaxPath> dir1;
			dir1.Append(workingDir).Append(TC("Dir1"));
			if (!CreateDirectoryW(dir1.data))
				return logger.Error(TC("Failed to create dir %s"), dir1.data);
		}

		ProcessStartInfo processInfo;
		processInfo.application = testApp.data;
		processInfo.workingDir = workingDir;
		ProcessHandle process = runProcess(processInfo);
		if (!process.WaitForExit(100000))
			return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
		u32 exitCode = process.GetExitCode();

		if (exitCode != 0)
			return logger.Error(TC("UbaTestApp returned exit code %u"), exitCode);

		{
			StringBuffer<MaxPath> fileW2;
			fileW2.Append(workingDir).Append(TC("FileW2"));
			if (!FileExists(logger, fileW2.data))
				return logger.Error(TC("Can't find file %s"), fileW2.data);
		}
		{
			StringBuffer<MaxPath> fileWF;
			fileWF.Append(workingDir).Append(TC("FileWF"));
			if (!FileExists(logger, fileWF.data))
				return logger.Error(TC("Can't find file %s"), fileWF.data);
		}
		return true;
	}

#if PLATFORM_MAC
	bool ExecuteCommand(LoggerWithWriter& logger, const tchar* command, StringBufferBase& commandOutput)
	{
		FILE* fpCommand = popen(command, "r");
		if (fpCommand == nullptr || fgets(commandOutput.data, commandOutput.capacity, fpCommand) == nullptr || pclose(fpCommand) != 0)
		{
			logger.Error("Failed to get an Xcode from xcode-select");
			return false;
		}

		commandOutput.count = strlen(commandOutput.data);
		while (isspace(commandOutput.data[commandOutput.count-1]))
		{
			commandOutput.data[commandOutput.count-1] = 0;
			commandOutput.count--;
		}
		return true;
	}
#endif

	bool RunClang(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		StringBuffer<MaxPath> sourceFile;
		sourceFile.Append(workingDir).Append(TC("Code.cpp"));
		FileAccessor codeFile(logger, sourceFile.data);
		if (!codeFile.CreateWrite())
			return false;
		char code[] = "#include <stdio.h>\n int main() { printf(\"Hello world\\n\"); return 0; }";
		if (!codeFile.Write(code, sizeof(code) - 1))
			return false;
		if (!codeFile.Close())
			return false;

#if PLATFORM_MAC
		StringBuffer<MaxPath> xcodePath;
		ExecuteCommand(logger, "/usr/bin/xcrun -f clang++", xcodePath);
		const tchar* clangPath = xcodePath.data;
#else
		const tchar* clangPath = TC("/usr/bin/clang++");
#endif

		if (!FileExists(logger, clangPath)) // Skipping if clang is not installed.
			return true;

		ProcessStartInfo processInfo;
		processInfo.application = clangPath;
#if PLATFORM_MAC
		StringBuffer<MaxPath> xcodeSDKPath;
		StringBuffer<MaxPath> args;
		ExecuteCommand(logger, "xcrun --show-sdk-path", xcodeSDKPath);
		args.Append("-isysroot ");
		args.Append(xcodeSDKPath.data);
		args.Append(" -o code Code.cpp");
		processInfo.arguments = args.data;
#else
		processInfo.arguments = TC("-o code Code.cpp");
#endif
		processInfo.workingDir = workingDir;
		//processInfo.logFile = TC("/home/honk/RunClang.log");
		ProcessHandle process = runProcess(processInfo);
		if (!process.WaitForExit(40000))
			return logger.Error(TC("clang++ timed out"));
		u32 exitCode = process.GetExitCode();
		if (exitCode != 0)
			return logger.Error(TC("clang++ returned exit code %u"), exitCode);
		return true;
	}

	bool RunCustomService(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		bool gotMessage = false;
		session.RegisterCustomService([&](uba::Process& process, const void* recv, u32 recvSize, void* send, u32 sendCapacity)
			{
				gotMessage = true;
				//wprintf(L"GOT MESSAGE: %.*s\n", recvSize / 2, (const wchar_t*)recv);
				const wchar_t* hello = L"Hello response from server";
				u64 helloBytes = wcslen(hello) * 2;
				memcpy(send, hello, helloBytes);
				return u32(helloBytes);
			});

		StringBuffer<> testApp;
		GetTestAppPath(logger, testApp);

		ProcessStartInfo processInfo;
		processInfo.application = testApp.data;
		processInfo.workingDir = workingDir;
		processInfo.arguments = TC("Whatever");
		ProcessHandle process = runProcess(processInfo);
		if (!process.WaitForExit(10000))
			return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
		u32 exitCode = process.GetExitCode();

		if (exitCode != 0)
			return logger.Error(TC("UbaTestApp returned exit code %u"), exitCode);

		if (!gotMessage)
			return logger.Error(TC("Never got message from UbaTestApp"));

		return true;
	}

	// NOTE: This test is dependent on the UbaTestApp<Platform>
	// The purpose of this test is to validate that the platform specific detours are
	// working as expected.
	// Before running the actual UbaTestApp, RunLocal calls through a variety of functions
	// that sets up the various UbaSession Servers, Clients, etc. It creates some temporary
	// directories, e.g. Dir1 and eventually call ProcessImpl::InternalCreateProcess.
	// InternalCreateProcess will setup the shared memory, inject the Detour library
	// and setup any other necessary environment variables, and spawn the actual process
	// (in this case the UbaTestApp)
	// Once UbaTestApp has started, it will first check and validate that the detour library
	// is in the processes address space. With the detour in place, the test app will
	// exercise various file functions which will actually go through our detour library.
	bool TestDetouredTestApp(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, RunTestApp);
	}

	bool TestRemoteDetouredTestApp(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunRemote(logger, testRootDir, RunTestApp);
	}

	bool TestCustomService(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		if (!IsWindows)
			return true;

		return RunRemote(logger, testRootDir, RunCustomService);
	}

	bool TestDetouredClang(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		if (IsWindows)
			return true;
		return RunLocal(logger, testRootDir, RunClang);
	}

	bool TestRemoteDetouredClang(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		if (IsWindows)
			return true;
		return RunRemote(logger, testRootDir, RunClang);
	}

	bool TestDetouredTouch(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		if (IsWindows)
			return true;
		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				StringBuffer<> file;
				file.Append(workingDir).Append(TC("TouchFile.h"));
				FileAccessor fr(logger, file.data);

				if (!fr.CreateWrite())
					return false;
				if (!fr.Write("Foo", 4))
					return false;
				if (!fr.Close())
					return false;
				FileInformation oldInfo;
				if (!GetFileInformation(oldInfo, logger, file.data))
					return false;

				Sleep(100);

				ProcessStartInfo processInfo;
				processInfo.application = TC("/usr/bin/touch");
				processInfo.workingDir = workingDir;
				processInfo.arguments = file.data;
				processInfo.logFile = TC("/home/honk/Touch.log");
				ProcessHandle process = runProcess(processInfo);
				if (!process.WaitForExit(10000))
					return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
				u32 exitCode = process.GetExitCode();
				if (exitCode != 0)
					return false;

				FileInformation newInfo;
				if (!GetFileInformation(newInfo, logger, file.data))
					return false;
				if (newInfo.lastWriteTime == oldInfo.lastWriteTime)
					return logger.Error(TC("File time not changed after touch"));
				return true;
			});
	}

	const tchar* GetSystemApplication()
	{
		#if PLATFORM_WINDOWS
		return TC("c:\\windows\\system32\\ping.exe");
		#elif PLATFORM_LINUX
		return TC("/usr/bin/zip");
		#else
		return TC("/sbin/zip");
		#endif
	}

	const tchar* GetSystemArguments()
	{
		#if PLATFORM_WINDOWS
		return TC("-n 1 localhost");
		#else
		return TC("-help");
		#endif
	}

	const tchar* GetSystemExpectedLogLine()
	{
		#if PLATFORM_WINDOWS
		return TC("Pinging ");
		#else
		return TC("zip [-options]");
		#endif
	}

	bool TestMultipleDetouredProcesses(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				ProcessStartInfo processInfo;
				processInfo.application = GetSystemApplication();
				processInfo.workingDir = workingDir;
				processInfo.arguments = GetSystemArguments();
				//processInfo.logFile = "/home/honk/LogFile.log";
				Vector<ProcessHandle> processes;

				for (u32 i=0; i!=50; ++i)
					processes.push_back(runProcess(processInfo));

				for (auto& process : processes)
				{
					if (!process.WaitForExit(10000))
						return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
					u32 exitCode = process.GetExitCode();
					if (exitCode != 0)
						return false;
				}

				return true;
			});
	}

	bool RunSystemApplicationAndLookForLog(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		ProcessStartInfo processInfo;
		processInfo.application = GetSystemApplication();
		processInfo.workingDir = workingDir;
		processInfo.arguments = GetSystemArguments();

		bool foundPingString = false;
		processInfo.logLineUserData = &foundPingString;
		processInfo.logLineFunc = [](void* userData, const tchar* line, u32 length, LogEntryType type)
			{
				*(bool*)userData |= Contains(line, GetSystemExpectedLogLine());
			};

		ProcessHandle process = runProcess(processInfo);

		if (!process.WaitForExit(10000))
			return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
		u32 exitCode = process.GetExitCode();
		if (exitCode != 0)
			return logger.Error(TC("Got exit code %u"), exitCode);
		if (!foundPingString)
			return logger.Error(TC("Did not log string containing \"%s\""), GetSystemExpectedLogLine());
		return true;
	}

	bool TestLogLines(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, RunSystemApplicationAndLookForLog);
	}

	bool TestLogLinesNoDetour(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, RunSystemApplicationAndLookForLog, false);
	}
}
