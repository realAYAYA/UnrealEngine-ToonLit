// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaAWS.h"
#include "UbaDirectoryIterator.h"
#include "UbaNetworkBackendMemory.h"
#include "UbaNetworkBackendQuic.h"
#include "UbaNetworkBackendTcp.h"
#include "UbaNetworkMessage.h"
#include "UbaNetworkServer.h"
#include "UbaSessionClient.h"
#include "UbaStorageClient.h"
#include "UbaStorageProxy.h"
#include "UbaSentry.h"
#include "UbaVersion.h"

#if defined(UBA_USE_SENTRY)
#pragma comment (lib, "WinHttp.lib")
#pragma comment (lib, "Version.lib")
#pragma comment (lib, "sentry.lib")
#pragma comment (lib, "crashpad_client.lib")
#pragma comment (lib, "crashpad_compat.lib")
#pragma comment (lib, "crashpad_getopt.lib")
#pragma comment (lib, "crashpad_handler_lib.lib")
#pragma comment (lib, "crashpad_minidump.lib")
#pragma comment (lib, "crashpad_snapshot.lib")
#pragma comment (lib, "crashpad_tools.lib")
#pragma comment (lib, "crashpad_util.lib")
#pragma comment (lib, "mini_chromium.lib")
#endif

#if PLATFORM_WINDOWS
#define UBA_AUTO_UPDATE 1
#define UBA_USE_EXCEPTION_HANDLER 0
#else
#define UBA_AUTO_UPDATE 0
#define UBA_USE_EXCEPTION_HANDLER 0
#endif
//#include <dbghelp.h>
//#pragma comment (lib, "Dbghelp.lib")

namespace uba
{
	const tchar*		Version = GetVersionString();
	constexpr u32	DefaultCapacityGb = 20;
	constexpr u32	DefaultListenTimeout = 5;
	const tchar*	DefaultRootDir = [](){
		static tchar buf[256];
		if (IsWindows)
			ExpandEnvironmentStringsW(TC("%ProgramData%\\Epic\\" UE_APP_NAME), buf, sizeof(buf));
		else
			GetFullPathNameW(TC("~/" UE_APP_NAME), sizeof_array(buf), buf, nullptr);
		return buf;
	}();
	u32				DefaultProcessorCount = []() { return GetLogicalProcessorCount(); }();
	const tchar*	DefaultAgentName = []() { static tchar buf[256]; GetComputerNameW(buf, sizeof_array(buf)); return buf; }();
	u32				DefaultMaxConnectionCount = 4;

	int PrintHelp(const tchar* message)
	{
		LoggerWithWriter logger(g_consoleLogWriter, TC(""));
		if (*message)
		{
			logger.Info(TC(""));
			logger.Error(TC("%s"), message);
		}
		logger.Info(TC(""));
		logger.Info(TC("-------------------------------------------"));
		logger.Info(TC("   UbaAgent v%s"), Version);
		logger.Info(TC("-------------------------------------------"));
		logger.Info(TC(""));
		logger.Info(TC("  When started UbaAgent will keep trying to connect to provided host address."));
		logger.Info(TC("  Once connected it will start helping out. Nothing else is needed :)"));
		logger.Info(TC(""));
		logger.Info(TC("  -dir=<rootdir>          The directory used to store data. Defaults to \"%s\""), DefaultRootDir);
		logger.Info(TC("  -host=<host>[:<port>]   The ip/name and port (default: %u) of the machine we want to help"), DefaultPort);
		logger.Info(TC("  -listen[=port]          Agent will listen for connections on port (default: %u) and help when connected"), DefaultPort);
		logger.Info(TC("  -listenTimeout=<sec>    Number of seconds agent will listen for host before giving up (default: %u)"), DefaultListenTimeout);
		logger.Info(TC("  -proxyport=<port>       Which port that agent will use if being assigned to be proxy for other agents (default: %u)"), DefaultStorageProxyPort);
		logger.Info(TC("  -maxcpu=<number>        Max number of processes that can be started. Defaults to \"%u\" on this machine"), DefaultProcessorCount);
		logger.Info(TC("  -mulcpu=<number>        This value multiplies with number of cpu to figure out max cpu. Defaults to 1.0"));
		logger.Info(TC("  -maxcon=<number>        Max number of connections that can be started by agent. Defaults to \"%u\" (amount up to max will depend on ping)"), DefaultMaxConnectionCount);
		logger.Info(TC("  -capacity=<gigaby>      Capacity of local store. Defaults to %u gigabytes"), DefaultCapacityGb);
		logger.Info(TC("  -quic                   Use Quic instead of tcp backend."));
		logger.Info(TC("  -name=<name>            The identifier of this agent. Defaults to \"%s\" on this machine"), DefaultAgentName);
		logger.Info(TC("  -stats[=<threshold>]    Print stats for each process if higher than threshold"));
		logger.Info(TC("  -verbose                Print debug information to console"));
		logger.Info(TC("  -log                    Log all processes detouring information to file (only works with debug builds)"));
		logger.Info(TC("  -nocustomalloc          Disable custom allocator for processes. If you see odd crashes this can be tested"));
		logger.Info(TC("  -storeraw               Disable compression of storage. This will use more storage and might improve performance"));
		logger.Info(TC("  -sendraw                Disable compression of send. This will use more bandwidth but less cpu"));
		logger.Info(TC("  -sendsize               Max size of messages being sent from client to server (does not affect server to client)"));
		logger.Info(TC("  -named=<name>           Use named events and file mappings by providing the base name in this option"));
		logger.Info(TC("  -nopoll                 Does not keep polling for work; attempts to connect once then exits"));
		logger.Info(TC("  -nostore                Does not use storage to store files (with a few exceptions such as binaries)"));
		logger.Info(TC("  -resetstore             Delete all cas"));
		logger.Info(TC("  -quiet                  Does not output any logging in console"));
		logger.Info(TC("  -maxidle=<seconds>      Max time agent will idle before disconnecting. Ignored if -nopoll is not set"));
		logger.Info(TC("  -binasversion           Will use binaries as version. This will cause updates everytime binaries change on host side"));
		logger.Info(TC("  -summary                Print summary at the end of a session"));
		logger.Info(TC("  -eventfile=<file>       File containing external events to agent. Things like machine is about to be terminated etc"));
		logger.Info(TC("  -sentry                 Enable sentry"));
		logger.Info(TC("  -zone                   Set the zone this machine exists in. This info is used to figure out if proxies should be created."));
		logger.Info(TC("  -killrandom             Kills random process and exit session"));
		logger.Info(TC("  -memwait=<percent>      The amount of memory needed to spawn a process. Set this to 100 to disable. Defaults to 80%%"));
		logger.Info(TC("  -memkill=<percent>      The amount of memory needed before processes starts to be killed. Set this to 100 to disable. Defaults to 90%%"));
		logger.Info(TC("  -crypto=<key>           16 bytes crypto key used for secure network transfer"));
		logger.Info(TC("  -populateCas=<dir>      Prepopulate cas database with files in dir. If files needed exists on machine this can be an optimization"));
		#if PLATFORM_MAC
		logger.Info(TC("  -populateCasFromXcodeVersion=<version>   Prepopulate cas database with files from local xcode installation that matches the version."));
		logger.Info(TC("  -populateCasFromAllXcodes   Prepopulate cas database with files from local xcode installation that matches the version."));
		#endif
		logger.Info(TC(""));
		return -1;
	}

	StorageClient* g_storageClient;
	NetworkClient* g_client;

	void CtrlBreakPressed()
	{
		if (g_storageClient)
		{
			g_storageClient->SaveCasTable(true);
			LoggerWithWriter(g_consoleLogWriter).Info(TC("CAS table saved..."));
		}

		abort();
		//if (g_client)
		//	g_client->Disconnect();
	}

	#if PLATFORM_WINDOWS
	int ReportSEH(LPEXCEPTION_POINTERS exceptionInfo)
	{
		StringBuffer<4096> assertInfo;
		assertInfo.Appendf(TC("SEH EXCEPTION %u (0x%llx)"), exceptionInfo->ExceptionRecord->ExceptionCode, exceptionInfo->ExceptionRecord->ExceptionAddress);
		WriteAssertInfo(assertInfo, nullptr, nullptr, 0, nullptr, 1);
		LoggerWithWriter(g_consoleLogWriter).Log(LogEntryType_Error, assertInfo.data, assertInfo.count);
		return EXCEPTION_CONTINUE_SEARCH;
	}
	BOOL ConsoleHandler(DWORD signal)
	{
		if (signal == CTRL_C_EVENT)
			CtrlBreakPressed();
		return FALSE;
	}
	#else
	void ConsoleHandler(int sig)
	{
		CtrlBreakPressed();
	}
	#endif

	StringBuffer<> g_rootDir(DefaultRootDir);

#if UBA_USE_EXCEPTION_HANDLER
	LONG WINAPI UbaUnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo)
	{
		StringBuffer<4096> assertInfo;
		WriteAssertInfo(assertInfo, TC(""), nullptr, 0, nullptr, 1);
		LoggerWithWriter(g_consoleLogWriter).Log(LogEntryType_Error, assertInfo.data, assertInfo.count);
	//	time_t rawtime;
	//	time(&rawtime);
	//	tm ti;
	//	localtime_s(&ti, &rawtime);
	//
	//	StringBuffer<> dumpFile;
	//	dumpFile.Append(g_rootDir).EnsureEndsWithSlash().Appendf(TC("UbaAgentCrash_%02u%02u%02u_%02u%02u%02u.dmp"), ti.tm_year - 100, ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
	//
	//	wprintf(TC("Unhandled exception - Writing minidump %s\n"), dumpFile.data);
	//	HANDLE hFile = CreateFile(dumpFile.data, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	//	MINIDUMP_EXCEPTION_INFORMATION mei;
	//	mei.ThreadId = GetCurrentThreadId();
	//	mei.ClientPointers = TRUE;
	//	mei.ExceptionPointers = ExceptionInfo;
	//	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &mei, NULL, NULL);
		return EXCEPTION_EXECUTE_HANDLER;
	}
#endif

#if UBA_AUTO_UPDATE
	const tchar* g_ubaAgentBinaries[] = { UBA_AGENT_EXECUTABLE, UBA_DETOURS_LIBRARY };

	bool DownloadBinaries(CasKey* keys)
	{
		StringBuffer<256> binDir(g_rootDir);
		binDir.Append(TC("\\binaries\\"));
		g_storageClient->CreateDirectory(binDir.data);
		u32 index = 0;
		for (auto file : g_ubaAgentBinaries)
		{
			Storage::RetrieveResult result;
			if (!g_storageClient->RetrieveCasFile(result, keys[index++], file))
				return false;
			StringBuffer<256> fullFile(binDir);
			fullFile.Append(file);
			if (!g_storageClient->CopyOrLink(result.casKey, fullFile.data, DefaultAttributes()))
				return false;
		}
		return true;
	}

	bool LaunchProcess(tchar* args)
	{
		STARTUPINFOW si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));
		if (!CreateProcessW(NULL, args, NULL, NULL, false, 0, NULL, NULL, &si, &pi))
			return false;
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	}

	bool LaunchTemp(Logger& logger, int argc, tchar* argv[])
	{
		StringBuffer<256> currentDir;
		if (!GetDirectoryOfCurrentModule(logger, currentDir))
			return false;

		StringBuffer<> args;
		args.Append(g_rootDir).Append(TC("\\binaries\\") UBA_AGENT_EXECUTABLE);
		args.Append(TC(" -relaunch=\"")).Append(currentDir).Append(TC("\""));
		args.Appendf(TC(" -waitid=%u"), GetCurrentProcessId());

		for (int i = 1; i != argc; ++i)
			args.Append(' ').Append(argv[i]);

		return LaunchProcess(args.data);
	}

	bool WaitForProcess(u32 procId)
	{
		HANDLE ph = OpenProcess(SYNCHRONIZE, TRUE, procId);
		if (!ph)
			return true;
		bool success = WaitForSingleObject(ph, 10000) == WAIT_OBJECT_0;
		CloseHandle(ph);
		return success;
	}

	int LaunchReal(Logger& logger, StringBufferBase& relaunchPath, int argc, tchar* argv[])
	{
		StringBuffer<256> currentDir;
		if (!GetDirectoryOfCurrentModule(logger, currentDir))
			return -1;
		logger.Info(TC("Copying new binaries..."));
		for (auto file : g_ubaAgentBinaries)
		{
			StringBuffer<256> from(currentDir);
			from.Append('\\').Append(file);

			StringBuffer<256> to(relaunchPath.data);
			to.Append('\\').Append(file);

			if (!uba::CopyFileW(from.data, to.data, false))
			{
				logger.Error(TC("Failed to copy file for relaunch"));
				return -1;
			}
		}

		StringBuffer<> args;
		args.Append(relaunchPath).Append(PathSeparator).Append(UBA_AGENT_EXECUTABLE);
		//args.Appendf(TC(" -waitid=%u"), GetCurrentProcessId());

		for (int i = 1; i != argc; ++i)
			if (!StartsWith(argv[i], TC("-relaunch")) && !StartsWith(argv[i], TC("-waitid")))
				args.Append(' ').Append(argv[i]);
		logger.Info(TC("Relaunching new %s..."), UBA_AGENT_EXECUTABLE);
		logger.Info(TC(""));
		if (!LaunchProcess(args.data))
			return -1;
		return 0;
	}
#endif // UBA_AUTO_UPDATE

	int ExpandEnvironmentVariables(StringBufferBase& str)
	{
		StringBuffer<> expandedDir;
		u64 offset = 0;
		while (true)
		{
			const tchar* begin = str.First('%', offset);
			if (!begin)
			{
				expandedDir.Append(str.data + offset);
				str.Clear().Append(expandedDir);
				break;
			}
			u64 beginOffset = begin - str.data;
			const tchar* end = str.First('%', beginOffset + 1);
			if (!end)
				return PrintHelp(TC("Missing closing % for environment variable in dir path"));
			u64 endOffset = end - str.data;
			StringBuffer<256> var;
			var.Append(begin + 1, endOffset - beginOffset - 1);
			StringBuffer<> value;
			value.count = GetEnvironmentVariableW(var.data, value.data, value.capacity);
			if (!value.count)
			{
				StringBuffer<256> err;
				err.Appendf(TC("Can't find environment variable %s used in dir path"), var.data);
				return PrintHelp(err.data);
			}

			expandedDir.Append(str.data + offset, beginOffset - offset).Append(value);
			offset = endOffset + 1;
		}
		return 0;
	}

	bool IsTerminating(Logger& logger, const tchar* eventFile, StringBufferBase& outReason, u64& outTerminationTimeMs)
	{
		if (!*eventFile)
			return false;

		u64 fileSize;
		if (!FileExists(logger, eventFile, &fileSize))
			return false;

		outTerminationTimeMs = 0;
		Sleep(1000);

		FileHandle fileHandle;
		if (!OpenFileSequentialRead(logger, eventFile, fileHandle))
			return true; // Fail to open the file we treat as instant termination

		auto g = MakeGuard([&]() { CloseFile(eventFile, fileHandle); });
		char buffer[2048];
		u64 toRead = Min(fileSize, u64(sizeof(buffer) - 1));
		if (!ReadFile(logger, eventFile, fileHandle, buffer, toRead))
			return true; // Fail to read the file we treat as instant termination

		buffer[toRead] = 0;
		StringBuffer<> reason;
		u64 terminateTimeMsUtc = 0;

		char* lineBegin = buffer;
		u32 lineIndex = 0;
		bool loop = true;
		while (loop)
		{
			char* lineEnd = strchr(lineBegin, '\n');
			if (lineEnd)
			{
				if (lineBegin < lineEnd && lineEnd[-1] == '\r')
					lineEnd[-1] = 0;
				else
					lineEnd[0] = 0;
			}
			else
				loop = false;

			switch (lineIndex)
			{
			case 0: // version
				if (strcmp(lineBegin, "v1") != 0)
					loop = false;
				break;
			case 1: // Relative time
				//strtoull(lineBegin, nullptr, 10);
				break;
			case 2: // Absolute time
				terminateTimeMsUtc = strtoull(lineBegin, nullptr, 10);
				break;
			case 3: // reason
				outReason.Appendf(PERCENT_HS, lineBegin);
				break;
			}

			lineBegin = lineEnd + 1;
			++lineIndex;
		}

		if (terminateTimeMsUtc != 0)
		{
			u64 nowMsUtc = time(0) * 1000;
			if (terminateTimeMsUtc > nowMsUtc)
			{
				u64 relativeTime = terminateTimeMsUtc - nowMsUtc;
				outTerminationTimeMs = relativeTime;
			}
		}
		return true;
	}

	int WrappedMain(int argc, tchar*argv[])
	{
		#if UBA_USE_EXCEPTION_HANDLER
		SetUnhandledExceptionFilter(UbaUnhandledExceptionFilter);
		#endif

		u32 maxProcessCount = DefaultProcessorCount;
		float mulProcessValue = 1.0f;
		u32 maxConnectionCount = DefaultMaxConnectionCount;
		u32 outputStatsThresholdMs = 0;
		u32 storageCapacityGb = DefaultCapacityGb;
		StringBuffer<256> host;
		StringBuffer<256> named;
		StringBuffer<512> relaunchPath;
		StringBuffer<256> eventFile;
		TString command;
		u16 port = DefaultPort;
		u16 proxyPort = DefaultStorageProxyPort;
		StringBuffer<> agentName(DefaultAgentName);
		bool useListen = false;
		bool logToFile = false;
		bool storeCompressed = true;
		bool sendCompressed = true;
		bool disableCustomAllocator = false;
		bool useBinariesAsVersion = false;
		bool useQuic = false;
		bool poll = true;
		bool useStorage = true;
		bool resetStore = false;
		bool quiet = false;
		bool verbose = false;
		bool printSummary = false;
		bool killRandom = false;
		StringBuffer<512> sentryUrl;
		StringBuffer<128> zone;
		u32 maxIdleSeconds = ~0u;
		u32 sendSize = SendDefaultSize;
		u32 waitProcessId = ~0u;
		u32 memWaitLoadPercent = 80;
		u32 memKillLoadPercent = 90;
		u32 listenTimeoutSec = DefaultListenTimeout;
		u8 crypto[16];
		bool hasCrypto = false;
		Vector<TString> populateCasDirs;

		#if PLATFORM_MAC
		StringBuffer<32> populateCasFromXcodeVersion;
		bool populateCasFromAllXcodes;
		#endif

		for (int i=1; i!=argc; ++i)
		{
			StringBuffer<> name;
			StringBuffer<> value;

			if (const tchar* equals = TStrchr(argv[i],'='))
			{
				name.Append(argv[i], equals - argv[i]);
				value.Append(equals+1);
			}
			else
			{
				name.Append(argv[i]);
			}
		
			if (name.Equals(TC("-verbose")))
			{
				verbose = true;
			}
			else if (name.Equals(TC("-relaunch")))
			{
				relaunchPath.Append(value);
			}
			else if (name.Equals(TC("-waitid")))
			{
				value.Parse(waitProcessId);
			}
			else if (name.Equals(TC("-maxcpu")))
			{
				if (!value.Parse(maxProcessCount))
					return PrintHelp(TC("Invalid value for -maxcpu"));
			}
			else if (name.Equals(TC("-mulcpu")))
			{
				if (!value.Parse(mulProcessValue))
					return PrintHelp(TC("Invalid value for -mulcpu"));
			}
			else if (name.Equals(TC("-maxcon")) || name.Equals(TC("-maxtcp")))
			{
				if (!value.Parse(maxConnectionCount) || maxConnectionCount == 0)
					return PrintHelp(TC("Invalid value for -maxcon"));
			}
			else if (name.Equals(TC("-capacity")))
			{
				if (!value.Parse(storageCapacityGb))
					return PrintHelp(TC("Invalid value for -capacity"));
			}
			else if (name.Equals(TC("-stats")))
			{
				if (value.IsEmpty())
					outputStatsThresholdMs = 1;
				else if (!value.Parse(outputStatsThresholdMs))
					return PrintHelp(TC("Invalid for -stats"));
			}
			else if (name.Equals(TC("-host")))
			{
				if (const tchar* portIndex = value.First(':'))
				{
					StringBuffer<> portStr(portIndex + 1);
					if (!portStr.Parse(port))
						return PrintHelp(TC("Invalid value for port in -host"));
					value.Resize(portIndex - value.data);
				}
				if (value.IsEmpty())
					return PrintHelp(TC("-host needs a name/ip"));
				host.Append(value);
			}
			else if (name.Equals(TC("-listen")))
			{
				if (!value.IsEmpty())
					if (!value.Parse(port))
						return PrintHelp(TC("Invalid value for -capacity"));
				useListen = true;
			}
			else if (name.Equals(TC("-listenTimeout")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-listenTimeout needs a value"));
				if (!value.Parse(listenTimeoutSec))
					return PrintHelp(TC("Invalid value for -listenTimeout"));
			}
			else if (name.Equals(TC("-named")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-named needs a value"));
				named.Append(value);
			}
			else if (name.Equals(TC("-log")))
			{
				logToFile = true;
			}
			else if (name.Equals(TC("-quiet")))
			{
				quiet = true;
			}
			else if (name.Equals(TC("-nocustomalloc")))
			{
				disableCustomAllocator = true;
			}
			else if (name.Equals(TC("-storeraw")))
			{
				storeCompressed = false;
			}
			else if (name.Equals(TC("-sendraw")))
			{
				sendCompressed = false;
			}
			else if (name.Equals(TC("-sendsize")))
			{
				if (!value.Parse(sendSize))
					return PrintHelp(TC("Invalid value for -sendsize"));
			}
			else if (name.Equals(TC("-dir")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-dir needs a value"));
				if (int res = ExpandEnvironmentVariables(value))
					return res;
				if ((g_rootDir.count = GetFullPathNameW(value.Replace('\\', PathSeparator).data, g_rootDir.capacity, g_rootDir.data, nullptr)) == 0)
					return PrintHelp(StringBuffer<>().Appendf(TC("-dir has invalid path %s"), value.data).data);
			}
			else if (name.Equals(TC("-name")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-name needs a value"));
				agentName.Append(value);
			}
			else if (name.Equals(TC("-nopoll")))
			{
				poll = false;
			}
			else if (name.Equals(TC("-nostore")))
			{
				useStorage = false;
			}
			else if (name.Equals(TC("-resetstore")))
			{
				resetStore = true;
			}
			else if (name.Equals(TC("-binasversion")))
			{
				useBinariesAsVersion = true;
			}
			else if (name.Equals(TC("-quic")))
			{
				#if !UBA_USE_QUIC
				return PrintHelp(TC("-quic not supported. Quic is not compiled into this binary"));
				#endif
				useQuic = true;
			}
			else if (name.Equals(TC("-maxidle")))
			{
				if (!value.Parse(maxIdleSeconds))
					return PrintHelp(TC("Invalid value for -maxidle"));
			}
			else if (name.Equals(TC("-proxyport")))
			{
				if (!value.Parse(proxyPort))
					return PrintHelp(TC("Invalid value for -proxyport"));
			}
			else if (name.Equals(TC("-summary")))
			{
				printSummary = true;
			}
			else if (name.Equals(TC("-eventfile")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-eventfile needs a value"));
				if (int res = ExpandEnvironmentVariables(value))
					return res;
				if ((eventFile.count = GetFullPathNameW(value.Replace('\\', PathSeparator).data, eventFile.capacity, eventFile.data, nullptr)) == 0)
					return PrintHelp(StringBuffer<>().Appendf(TC("-eventfile has invalid path %s"), value.data).data);
			}
			else if (name.Equals(TC("-killrandom")))
			{
				killRandom = true;
			}
			else if (name.Equals(TC("-memwait")))
			{
				if (!value.Parse(memWaitLoadPercent) || memWaitLoadPercent > 100)
					return PrintHelp(TC("Invalid value for -memwait"));
			}
			else if (name.Equals(TC("-memkill")))
			{
				if (!value.Parse(memKillLoadPercent) || memKillLoadPercent > 100)
					return PrintHelp(TC("Invalid value for -memkill"));
			}
			else if (name.Equals(TC("-crypto")))
			{
				if (value.count != 32)
					return PrintHelp(TC("Invalid number of characters in crypto string. Should be 32"));
				((u64*)crypto)[0] = StringToValue(value.data, 16);
				((u64*)crypto)[1] = StringToValue(value.data + 16, 16);
				hasCrypto = true;
			}
			else if (name.Equals(TC("-populateCas")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-populateCas needs a dir"));
				populateCasDirs.push_back(value.data);
			}
			#if PLATFORM_MAC
			else if (name.Equals(TC("-populateCasFromXcodeVersion")))
			{
				populateCasFromXcodeVersion.Append(value.data);
			}
			else if (name.Equals(TC("-populateCasFromAllXcodes")))
			{
				populateCasFromAllXcodes = true;
			}
			#endif
			else if (name.Equals(TC("-sentry")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-sentry needs a url value"));
				sentryUrl.Append(value);
			}
			else if (name.Equals(TC("-zone")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-zone needs a value"));
				zone.Append(value);
			}
			else if (name.Equals(TC("-command")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-command needs a value"));
				command = value.data;
				poll = false;
				quiet = true;
			}
			else if (name.Equals(TC("-?")))
			{
				return PrintHelp(TC(""));
			}
			else if (relaunchPath.IsEmpty())
			{
				StringBuffer<> msg;
				msg.Appendf(TC("Unknown argument '%s'"), name.data);
				return PrintHelp(msg.data);
			}
		}

		if (!named.IsEmpty()) // We only run once with named connection
			poll = false;

		maxProcessCount = u32(float(maxProcessCount) * mulProcessValue);

		if (poll) // no point disconnect on idle since agent will just reconnect immediately again
			maxIdleSeconds = ~0u;

		if (memKillLoadPercent < memWaitLoadPercent)
			memKillLoadPercent = memWaitLoadPercent;


		FilteredLogWriter logWriter(g_consoleLogWriter, verbose ? LogEntryType_Debug : LogEntryType_Detail);
		LoggerWithWriter logger(logWriter, TC(""));

#if UBA_AUTO_UPDATE
		if (waitProcessId != ~0u)
			if (!WaitForProcess(waitProcessId))
				return -1;
		if (relaunchPath.count)
			return LaunchReal(logger, relaunchPath, argc, argv);
#endif // UBA_AUTO_UPDATE

		if (host.IsEmpty() && named.IsEmpty() && !useListen)
			return PrintHelp(TC("No host provided. Add -host=<host> (or use -listen)"));

		StringBuffer<256> extraInfo;

		#if defined(UBA_USE_SENTRY)
		if (!sentryUrl.IsEmpty())
		{
			char release[128];
			char url[512];
			size_t urlLen;
			sprintf_s(release, sizeof_array(release), "BoxAgent@%ls", Version);
			wcstombs_s(&urlLen, url, sizeof_array(url), sentryUrl.data, sizeof_array(url) - 1);
			sentry_options_t* options = sentry_options_new();
			sentry_options_set_dsn(options, url);
			sentry_options_set_database_path(options, ".sentry-native");
			sentry_options_set_release(options, release);
			//sentry_options_set_debug(options, 1);
			sentry_init(options);
			extraInfo.Append(TC(", SentryEnabled"));
		}
		
		auto sentryGuard = MakeGuard([&]() { if (!sentryUrl.IsEmpty()) sentry_close(); });
		#endif


		// Check if AWS
		#if UBA_USE_AWS
		AWS aws;
		{
			DirectoryCache dirCache;
			dirCache.CreateDirectory(logger, g_rootDir.data);
			aws.QueryInformation(logger, extraInfo, g_rootDir.data);
			if (zone.IsEmpty())
				zone.Append(aws.GetAvailabilityZone());
		}
		#endif

		if (!zone.count)
			zone.count = GetEnvironmentVariableW(TC("UBA_ZONE"), zone.data, zone.capacity);

		if (zone.count)
			extraInfo.Append(TC(", ")).Append(zone);

		if (IsRunningWine())
			extraInfo.Append(TC(", Linux/WINE"));

		if (useQuic)
			extraInfo.Append(TC(", MsQuic"));
		if (hasCrypto)
			extraInfo.Append(TC(", Encrypted"));

		//logger.Info(TC("\033[39m\n"));
		const tchar* dbgStr = TC("");
		#if UBA_DEBUG
		dbgStr = TC(" (DEBUG)");
		#endif
		logger.Info(TC("UbaAgent v%s%s (Cpu: %u, MaxCon: %u, Dir: \"%s\", StoreCapacity: %uGb%s)"), Version, dbgStr, maxProcessCount, maxConnectionCount, g_rootDir.data, storageCapacityGb, extraInfo.data);
		if (!eventFile.IsEmpty())
			logger.Info(TC("  Will poll for external events in file %s"), eventFile.data);
		logger.Info(TC(""));


#if PLATFORM_WINDOWS
		{
			StringBuffer<256> consoleTitle;
			consoleTitle.Appendf(TC("UbaAgent v%s%s"), Version, dbgStr);
			SetConsoleTitleW(consoleTitle.data);
		}
#endif

		u64 storageCapacity = u64(storageCapacityGb)*1000*1000*1000;

		if (command.empty())
		{
			// Create a uba storage quickly just to fix non-graceful shutdowns
			StorageCreateInfo info(g_rootDir.data, logWriter);
			info.rootDir = g_rootDir.data;
			info.casCapacityBytes = storageCapacity;
			info.storeCompressed = storeCompressed;
			StorageImpl storage(info);
			if (resetStore)
			{
				if (!storage.Reset())
					return -1;
			}
			else if (!storage.LoadCasTable(false))
				return -1;
		}

#if PLATFORM_MAC
		
		Vector<TString> xcodeDirectories;
		
		if (populateCasFromXcodeVersion.count > 0 || populateCasFromAllXcodes)
		{
			// look for all xcodes in /Applications (is there a function to get Applications dir location for other locales?)
			StringBuffer<> applicationsDir;
			applicationsDir.Append("/Applications");
			
			TraverseDir(logger, applicationsDir.data,
						[&](const DirectoryEntry& e)
						{
				if (IsDirectory(e.attributes) && StartsWith(e.name, "Xcode"))
				{
					StringBuffer<128> xcodeDir("/Applications/");
					xcodeDir.Append(e.name).Append("/Contents/Developer/");
					if (FileExists(logger, xcodeDir.data))
					{
						if (populateCasFromAllXcodes)
						{
							xcodeDirectories.push_back(xcodeDir.data);
						}
						else
						{
							StringBuffer<512> command;
							StringBuffer<32> xcodeVer;
							
							// look for short version like 15.1 or 15, or BuildVersion like 15C610
							bool bUseShortVersion = (populateCasFromXcodeVersion.Contains('.')) || populateCasFromXcodeVersion.count <= 3;
							const char* key = bUseShortVersion ? "CFBundleShortVersionString" : "ProductBuildVersion";
							command.Append("/usr/bin/defaults read /Applications/").Append(e.name).Append("/Contents/version.plist ").Append(key);
							
							FILE* getver = popen(command.data, "r");
							if (getver == nullptr || fgets(xcodeVer.data, xcodeVer.capacity, getver) == nullptr)
							{
								pclose(getver);
								logger.Error("Failed to get DTXcodeBuild from /Applications/%s", e.name);
								return;
							}
							pclose(getver);
							xcodeVer.count = strlen(xcodeVer.data);
							while (isspace(xcodeVer.data[xcodeVer.count-1]))
							{
								xcodeVer.data[xcodeVer.count-1] = 0;
								xcodeVer.count--;
							}
							
							logger.Info("/Applications/%s has version '%s' (looking for %s)", e.name, xcodeVer.data, populateCasFromXcodeVersion.data);
							
							if (xcodeVer.Equals(populateCasFromXcodeVersion.data))
							{
								xcodeDirectories.push_back(xcodeDir.data);
							}
						}
					}
				}
			});
		}
		// if we didn't want a single version, or all xcodes, then use active xcode (useful for user running their own agents)
		else
		{
			StringBuffer<512> xcodeSelectOutput;
			FILE* xcodeSelect = popen("/usr/bin/xcode-select -p", "r");
			if (xcodeSelect == nullptr || fgets(xcodeSelectOutput.data, xcodeSelectOutput.capacity, xcodeSelect) == nullptr || pclose(xcodeSelect) != 0)
			{
				logger.Error("Failed to get an Xcode from xcode-select");
				return -1;
			}

			xcodeSelectOutput.count = strlen(xcodeSelectOutput.data);
			while (isspace(xcodeSelectOutput.data[xcodeSelectOutput.count-1]))
			{
				xcodeSelectOutput.data[xcodeSelectOutput.count-1] = 0;
				xcodeSelectOutput.count--;
			}
			
			xcodeDirectories.push_back(xcodeSelectOutput.data);
		}

		if (xcodeDirectories.size() == 0)
		{
			logger.Error("Unable to populate from any Xcodes. Agent is unusable.");
			return -1;
		}

		for (TString& xcodeDir : xcodeDirectories)
		{
			logger.Info("Populating cas with %s", xcodeDir.data());
			
			const char* subDirs[] = { "/Toolchains", "/Platforms" };
			for (auto subDir : subDirs)
			{
				TString populateDir(xcodeDir);
				populateDir.append(subDir);
				populateCasDirs.push_back(populateDir);
			}
		}
#endif


		Vector<ProcessLogLine> logLines[2];
		u32 logLinesIndex = 0;
		ReaderWriterLock logLinesLock;
		Event logLinesAvailable(false);

		auto processFinished = [&](const ProcessHandle& process)
		{
			u32 errorCode = process.GetExitCode();
			if (errorCode == ProcessCancelExitCode)
				return;

			const Vector<ProcessLogLine>& processLogLines = process.GetLogLines();
			if (!processLogLines.empty())
			{
				SCOPED_WRITE_LOCK(logLinesLock, lock);
				for (auto& line : processLogLines)
					logLines[logLinesIndex].push_back(line);
				if (errorCode)
				{
					StringBuffer<> errorMsg;
					errorMsg.Appendf(TC(" (exit code: %u)"), errorCode);
					logLines[logLinesIndex].back().text += errorMsg.data;
				}
			}
			else
			{
				const TString& desc = process.GetStartInfo().description;
				StringBuffer<> name;
				if (!desc.empty())
					name.Append(desc);
				else
					GetNameFromArguments(name, process.GetStartInfo().arguments, false);
				LogEntryType entryType = LogEntryType_Info;
				if (errorCode)
				{
					name.Appendf(TC(" (exit code: %u)"), errorCode);
					entryType = LogEntryType_Error;
				}
				SCOPED_WRITE_LOCK(logLinesLock, lock);
				logLines[logLinesIndex].push_back({ TString(name.data), entryType });
			}

			logLinesAvailable.Set();
		};

		#if PLATFORM_WINDOWS
		SetConsoleCtrlHandler(ConsoleHandler, TRUE);
		#else
		signal(SIGINT, ConsoleHandler);
		#endif

		bool relaunch = false;
		StringBuffer<512> terminationReason;
		u64 terminationTimeMs = 0;

		#if UBA_USE_AWS
		if (aws.IsTerminating(logger, terminationReason, terminationTimeMs))
		{
			LoggerWithWriter(g_consoleLogWriter, TC("")).Info(TC("%s. Exiting UbaAgent before starting session"), terminationReason.data);
			return 0;
		}
		#endif

		bool isTerminating = false;
		do
		{
			// TODO: Revisit.. it could be that something stalls during startup so we can't have this timeout
			//u32 receiveTimeoutSeconds = 60; // We are sending pings at 5 seconds cadence, so timeout if no data appears on recv socket within 60 seconds
			u32 receiveTimeoutSeconds = 0;

			NetworkBackendMemory networkBackendMem(logWriter);
			NetworkBackend* networkBackend;
			#if UBA_USE_QUIC
			if (useQuic)
				networkBackend = new NetworkBackendQuic(logWriter);
			else
			#endif
				networkBackend = new NetworkBackendTcp(logWriter);
			auto backendGuard = MakeGuard([networkBackend]() { delete networkBackend; });

			NetworkClientCreateInfo ncci(logWriter);
			ncci.sendSize = sendSize;
			ncci.receiveTimeoutSeconds = receiveTimeoutSeconds;
			if (hasCrypto)
				ncci.cryptoKey128 = crypto;
			bool ctorSuccess = true;
			NetworkClient* client = new NetworkClient(ctorSuccess, ncci);
			g_client = client;
			auto csg = MakeGuard([&]() { g_client = nullptr; client->Disconnect(); delete client; });
			if (!ctorSuccess)
				return -1;

			bool exit = false;

			if (useListen)
			{
				client->StartListen(*networkBackend, port);
				u64 startTime = GetTime();
				while (!client->IsOrWasConnected(200))
				{
					if (IsEscapePressed())
					{
						exit = true;
						break;
					}

					u64 waitTime = GetTime() - startTime;
					if (!poll && TimeToMs(waitTime) > listenTimeoutSec*1000)
					{
						logger.Error(TC("Failed to get connection while listening for %s"), TimeToText(waitTime).str);
						return -1;
					}
				}
			}
			else
			{
				logger.Info(TC("Waiting to connect to %s:%u"), host.data, port);
				int retryCount = 5;
				u64 startTime = GetTime();
				bool timedOut = false;
				while (!client->Connect(*networkBackend, host.data, port, &timedOut))
				{
					if (IsEscapePressed())
					{
						exit = true;
						break;
					}
					if (!timedOut)
						return -1;

					if (!poll && !--retryCount)
					{
						logger.Error(TC("Failed to connect to %s:%u (after %s)"), host.data, port, TimeToText(GetTime() - startTime).str);
						return -1;
					}
				}
			}

			if (exit)
				return 0;

			if (!command.empty())
			{
				StackBinaryWriter<128> writer;
				NetworkMessage msg(*client, SessionServiceId, SessionMessageType_Command, writer);
				writer.WriteString(command);
				StackBinaryReader<8*1024> reader;
				if (!msg.Send(reader))
				{
					logger.Error(TC("Failed to send command to host"));
					return -1;
				}
				LoggerWithWriter commandLogger(g_consoleLogWriter, TC(""));
				commandLogger.Info(TC("----------------------------------"));
				while (true)
				{
					auto logType = (LogEntryType)reader.ReadByte();
					if (logType == 255)
						break;
					TString result = reader.ReadString();
					commandLogger.Log(logType, result.c_str(), u32(result.size()));
				}
				commandLogger.Info(TC("----------------------------------"));
				return 0;
			}


			Event wakeupSessionWait(false);
			Atomic<u32> targetConnectionCount = 1;

			struct Proxy
			{
				LogWriter& logWriter;
				NetworkBackend& networkBackend;
				NetworkBackend& networkBackendMem;
				NetworkClient* client;
				Event& wakeupSessionWait;
				u32& maxConnectionCount;
				Atomic<u32>& targetConnectionCount;
				Atomic<NetworkServer*> server;
				StorageProxy* storage = nullptr;
				StorageClient* storageClient = nullptr;
				TString serverPrefix;
			} proxy { g_consoleLogWriter, *networkBackend, networkBackendMem, client, wakeupSessionWait, maxConnectionCount, targetConnectionCount };
			auto psg = MakeGuard([&]() { delete proxy.server; });
			auto pg = MakeGuard([&]() { delete proxy.storage; });

			static auto startProxy = [](void* userData, u16 proxyPort, const Guid& storageServerUid)
				{
					auto& proxy = *(Proxy*)userData;

					NetworkServerCreateInfo nsci(proxy.logWriter);
					nsci.workerCount = 192;
					nsci.receiveTimeoutSeconds = 60;

					StringBuffer<256> prefix;
					prefix.Append(TC("UbaProxyServer (")).Append(GuidToString(proxy.client->GetUid()).str).Append(')');
					proxy.serverPrefix = prefix.data;
					bool ctorSuccess = true;
					auto proxyServer = new NetworkServer(ctorSuccess, nsci, proxy.serverPrefix.c_str());
					if (!ctorSuccess)
					{
						delete proxyServer;
						return false;
					}

					proxy.storage = new StorageProxy(*proxyServer, *proxy.client, storageServerUid, TC("Wooohoo"), proxy.storageClient);

					proxyServer->RegisterOnClientConnected(0, [p = &proxy](const Guid& clientUid, u32 clientId) { p->wakeupSessionWait.Set(); });
					proxyServer->SetWorkTracker(proxy.client->GetWorkTracker());
					proxyServer->StartListen(proxy.networkBackendMem, proxyPort);
					proxyServer->StartListen(proxy.networkBackend, proxyPort);
					proxy.targetConnectionCount = proxy.maxConnectionCount;

					proxy.server = proxyServer;
					proxy.wakeupSessionWait.Set();
					return true;
				};

			client->RegisterOnDisconnected([&]() { networkBackend->StopListen(); if (auto proxyServer = proxy.server.load()) proxyServer->DisconnectClients(); });

			struct NetworkBackends
			{
				NetworkBackend& tcp;
				NetworkBackend& mem;
			} backends { *networkBackend, networkBackendMem };

			static auto getProxyBackend = [](void* userData, const tchar* host) -> NetworkBackend&
				{
					auto& backends = *(NetworkBackends*)userData;
					return Equals(host, TC("inprocess")) ? backends.mem : backends.tcp;
				};

			StorageClientCreateInfo storageInfo(*client, g_rootDir.data);
			storageInfo.casCapacityBytes = storageCapacity;
			storageInfo.storeCompressed = storeCompressed;
			storageInfo.sendCompressed = sendCompressed;
			storageInfo.workManager = client;
			storageInfo.getProxyBackendCallback = getProxyBackend;
			storageInfo.getProxyBackendUserData = &backends;
			storageInfo.startProxyCallback = startProxy;
			storageInfo.startProxyUserData = &proxy;
			storageInfo.zone = zone.data;
			storageInfo.proxyPort = proxyPort;

			auto storageClient = new StorageClient(storageInfo);
			auto bscsg = MakeGuard([&]() { g_storageClient = nullptr; delete storageClient; });

			if (!storageClient->LoadCasTable(true))
				return -1;

			if (!storageClient->PopulateCasFromDirs(populateCasDirs, maxProcessCount))
				return -1;

			proxy.storageClient = storageClient;

			SessionClient* sessionClient = nullptr;

			CasKey keys[2];
			client->RegisterOnVersionMismatch([&](const CasKey& exeKey, const CasKey& dllKey)
				{
					keys[0] = exeKey;
					keys[1] = dllKey;
				});

			SessionClientCreateInfo info(*storageClient, *client, logWriter);
			info.maxProcessCount = maxProcessCount;
			info.dedicated = poll;
			info.maxIdleSeconds = maxIdleSeconds;
			info.outputStatsThresholdMs = outputStatsThresholdMs;
			info.name.Append(agentName);
			info.extraInfo = extraInfo.data;
			info.deleteSessionsOlderThanSeconds = 1; // Delete all old sessions
			//if (!awsInstanceId.IsEmpty())
			//	info.name.Append(TC(" (")).Append(awsInstanceId).Append(TC(")"));
			info.rootDir = g_rootDir.data;
			info.logToFile = logToFile;
			info.disableCustomAllocator = disableCustomAllocator;
			info.useBinariesAsVersion = useBinariesAsVersion;
			info.killRandom = killRandom;
			info.useStorage = useStorage;
			info.memWaitLoadPercent = u8(memWaitLoadPercent);
			info.memKillLoadPercent = u8(memKillLoadPercent);

			if (!quiet)
				info.processFinished = processFinished;

			sessionClient = new SessionClient(info);
			auto secsg = MakeGuard([&]() { delete sessionClient; });

			Atomic<bool> loopLogging = true;
			Thread loggingThread([&]()
				{
					while (loopLogging)
					{
						logLinesAvailable.IsSet();
						u32 logLinesIndexPrev;
						{
							SCOPED_WRITE_LOCK(logLinesLock, l);
							logLinesIndexPrev = logLinesIndex;
							logLinesIndex = (logLinesIndex + 1) % 2;
						}
						logger.BeginScope();
						for (auto& s : logLines[logLinesIndexPrev])
							logger.Log(LogEntryType_Detail, s.text.c_str(), u32(s.text.size()));
						logger.EndScope();
						logLines[logLinesIndexPrev].clear();
					}
					return 0;
				});

			auto disconnectAndStopLoggingThread = MakeGuard([&]()
			{
				networkBackend->StopListen();
				storageClient->StopProxy();
				auto proxyServer = proxy.server.load();
				if (proxyServer)
					proxyServer->DisconnectClients();
				sessionClient->Stop();
				sessionClient->SendSummary([&](Logger& logger) { if (proxyServer) proxyServer->PrintSummary(logger); });
				client->Disconnect();
				loopLogging = false;
				logLinesAvailable.Set();
				loggingThread.Wait();
			});

			// We got version mismatch and have the cas keys for the needed Agent/Detours binaries
			if (keys[0] != CasKeyZero)
			{
#if UBA_AUTO_UPDATE
				logger.Info(TC("Downloading new binaries..."));
				if (!DownloadBinaries(keys))
					return -1;
				relaunch = true;
				break;
#else
				return -1;
#endif
			}

			if (quiet)
				logger.Info(TC("Client session %s started"), sessionClient->GetId());
			else
				logger.Info(TC("----------- Session %s started -----------"), sessionClient->GetId());

			#if 0 
			u64 lastLogTime = GetTime();
			#endif

			u32 connectionCount = 1;

			//#if PLATFORM_WINDOWS
			//SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
			//#endif

			storageClient->Start();
			sessionClient->Start();

			while (true)
			{
				if (useListen)
				{
					if (connectionCount != targetConnectionCount)
					{
						client->SetConnectionCount(targetConnectionCount);
						connectionCount = targetConnectionCount;
					}
				}
				else
				{
					while (connectionCount < targetConnectionCount)
					{
						bool timedOut = false;
						if (client->Connect(*networkBackend, host.data, port, &timedOut))
							++connectionCount;
					}
				}

				if (sessionClient->Wait(5*1000, &wakeupSessionWait))
					break;

				// If we are the proxy server and have external connections we lower max process count. Note that it will always have one connection which is itself
				if (auto proxyServer = proxy.server.load())
					if (proxyServer->GetConnectionCount() > 1)
						sessionClient->SetMaxProcessCount(maxProcessCount - 2);

				// This is an estimation based on tcp limitations (ack and sliding windows).
				// For every 15ms latency on "best ping") we increase targetConnectionCount up to maxConnectionCount
				if (!storageClient->IsUsingProxy())
					if (u64 bestPing = sessionClient->GetBestPing())
						targetConnectionCount = Min(u32(TimeToMs(bestPing) / 15), maxConnectionCount);

				if (!isTerminating)
				{
					if (IsTerminating(logger, eventFile.data, terminationReason, terminationTimeMs))
						isTerminating = true;
					#if UBA_USE_AWS
					else if (aws.IsTerminating(logger, terminationReason, terminationTimeMs))
						isTerminating = true;
					#endif

					if (isTerminating)
					{
						sessionClient->SetIsTerminating(terminationReason.data, terminationTimeMs);
						if (quiet)
							LoggerWithWriter(g_consoleLogWriter, TC("")).Info(TC("%s"), terminationReason.data);
					}
				}

				#if 0 // Not needed anymore I think.. horde is now pinging in the background for itself
				if (quiet)
				{
					u64 time = GetTime();
					u64 timePast = time - lastLogTime;
					if (TimeToMs(timePast) > 90 * 1000)
					{
						logger.Info(TC("Break the silence after %s (here to keep horde happy...). Agent active process count: %u"), TimeToText(timePast).str, sessionClient->GetActiveProcessCount());
						lastLogTime = time;
					}
				}
				#endif
			}

			disconnectAndStopLoggingThread.Execute();

			if (quiet)
			{
				logger.Info(TC("Client session %s done"), sessionClient->GetId());
			}
			else
			{
				logger.BeginScope();
				if (printSummary)
				{
					sessionClient->PrintSummary(logger);
					storageClient->PrintSummary(logger);
					client->PrintSummary(logger);
					SystemStats::GetGlobal().Print(logger, true);
				}

				logger.Info(TC("----------- Session %s done! -----------"), sessionClient->GetId());
				logger.Info(TC(""));
				logger.EndScope();
			}

			//if (proxy.storage)
			//	proxy.storage->PrintSummary();
			//if (proxy.server)
			//	proxy.server->PrintSummary(logger);

			#if UBA_TRACK_CONTENTION
			LoggerWithWriter contLogger(g_consoleLogWriter, TC(""));
			PrintContentionSummary(contLogger);
			#endif
		}
		while (poll && !isTerminating);

#if UBA_AUTO_UPDATE
		if (relaunch)
			if (!LaunchTemp(logger, argc, argv))
				return -1;
#endif

		return 0;
	}
}

#if PLATFORM_WINDOWS
int wmain(int argc, wchar_t* argv[])
{
	using namespace uba;
	__try
	{
		return WrappedMain(argc, argv);
	}
	__except(ReportSEH(GetExceptionInformation()))
	{
		return -1;
	}
}
#else
int main(int argc, char* argv[])
{
	return uba::WrappedMain(argc, argv);
}
#endif
