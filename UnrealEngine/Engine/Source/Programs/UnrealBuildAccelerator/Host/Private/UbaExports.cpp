// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaExports.h"
#include "UbaNetworkBackendQuic.h"
#include "UbaNetworkBackendTcp.h"
#include "UbaAWS.h"
#include "UbaProcess.h"
#include "UbaScheduler.h"
#include "UbaStorageServer.h"
#include "UbaSessionServer.h"

#if PLATFORM_WINDOWS
#include "UbaWinBinDependencyParser.h"
#endif

namespace uba
{
	CallbackLogWriter::CallbackLogWriter(BeginScopeCallback begin, EndScopeCallback end, LogCallback log) : m_beginScope(begin), m_endScope(end), m_logCallback(log)
	{
	}

	void CallbackLogWriter::BeginScope()
	{
		(*m_beginScope)();
	}
	void CallbackLogWriter::EndScope()
	{
		(*m_endScope)();
	}

	void CallbackLogWriter::Log(LogEntryType type, const uba::tchar* str, u32 strLen, const uba::tchar* prefix, u32 prefixLen)
	{
		StringBuffer<> strBuf;
		if (prefixLen && strLen + prefixLen + 3 < strBuf.capacity) // TODO: Send prefix and prefixLen through callback
		{
			strBuf.Append(prefix, prefixLen);
			strBuf.Append(TC(" - "), 3);
			strBuf.Append(str, strLen);
			strLen += prefixLen + 3;
			str = strBuf.data;
		}
		(*m_logCallback)(type, str, strLen);
	}

	class NetworkServerWithBackend : public NetworkServer
	{
	public:
		NetworkServerWithBackend(bool& outSuccess, const NetworkServerCreateInfo& info, NetworkBackend* nb)
		: NetworkServer(outSuccess, info), networkBackend(nb)
		{
		}

		NetworkBackend* networkBackend;
	};

	#define UBA_USE_SIGNALHANDLER 0//PLATFORM_LINUX // It might be that we can't use signal handlers in c# processes.. so don't set this to 1

	#if UBA_USE_SIGNALHANDLER
	void SignalHandler(int sig, siginfo_t* si, void* unused)
	{
		StringBuffer<256> desc;
		desc.Append("Segmentation fault at +0x").AppendHex(u64(si->si_addr));
		UbaAssert(desc.data, "", 0, "", -1);
	}
	#endif
}

extern "C"
{
	uba::LogWriter* GetDefaultLogWriter()
	{
		return &uba::g_consoleLogWriter;
	}

	uba::LogWriter* CreateCallbackLogWriter(uba::CallbackLogWriter::BeginScopeCallback begin, uba::CallbackLogWriter::EndScopeCallback end, uba::CallbackLogWriter::LogCallback log)
	{
		#if UBA_USE_SIGNALHANDLER
		struct sigaction action;
		memset(&action, 0, sizeof(action));
		sigfillset(&action.sa_mask);
		action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
		action.sa_sigaction = uba::SignalHandler;
		sigaction(SIGSEGV, &action, NULL);
		//sigaction(SIGABRT, &action, NULL);
		#endif

		return new uba::CallbackLogWriter(begin, end, log);
	}

	void DestroyCallbackLogWriter(uba::LogWriter* writer)
	{
		if (writer != &uba::g_consoleLogWriter)
			delete writer;
	}

	uba::NetworkServer* CreateServer(uba::LogWriter& writer, uba::u32 workerCount, uba::u32 sendSize, uba::u32 receiveTimeoutSeconds, bool useQuic)
	{
		using namespace uba;
		NetworkBackend* networkBackend;
		#if UBA_USE_QUIC
		if (useQuic)
			networkBackend = new NetworkBackendQuic(writer);
		else
		#endif
			networkBackend = new NetworkBackendTcp(writer);

		NetworkServerCreateInfo info(writer);
		info.workerCount = workerCount;
		info.sendSize = sendSize;
		info.receiveTimeoutSeconds = receiveTimeoutSeconds;

		bool success = true;
		auto server = new NetworkServerWithBackend(success, info, networkBackend);
		if (success)
			return server;
		delete server;
		return nullptr;
	}

	void DestroyServer(uba::NetworkServer* server)
	{
		auto s = (uba::NetworkServerWithBackend*)server;
		auto networkBackend = s->networkBackend;
		delete s;
		delete networkBackend;
	}

	bool Server_StartListen(uba::NetworkServer* server, int port, const uba::tchar* ip, const uba::tchar* crypto)
	{
		using namespace uba;
		u8 crypto128Data[16];
		u8* crypto128 = nullptr;
		if (crypto && *crypto)
		{
			((u64*)crypto128Data)[0] = StringToValue(crypto, 16);
			((u64*)crypto128Data)[1] = StringToValue(crypto + 16, 16);
			crypto128 = crypto128Data;
		}

		auto s = (NetworkServerWithBackend*)server;
		return s->StartListen(*s->networkBackend, u16(port), ip, crypto128);
	}

	void Server_Stop(uba::NetworkServer* server)
	{
		auto s = (uba::NetworkServerWithBackend*)server;
		auto networkBackend = s->networkBackend;
		networkBackend->StopListen();
		server->DisconnectClients();
	}


	bool Server_AddClient(uba::NetworkServer* server, const uba::tchar* ip, int port, const uba::tchar* crypto)
	{
		using namespace uba;
		u8 crypto128Data[16];
		u8* crypto128 = nullptr;
		if (crypto && *crypto)
		{
			((u64*)crypto128Data)[0] = StringToValue(crypto, 16);
			((u64*)crypto128Data)[1] = StringToValue(crypto + 16, 16);
			crypto128 = crypto128Data;
		}

		auto s = (NetworkServerWithBackend*)server;
		return s->AddClient(*s->networkBackend, ip, u16(port), crypto128);
	}

	/*
	uba::NetworkClient* CreateClient(uba::LogWriter& writer, uba::u32 sendSize, uba::u32 receiveTimeoutSeconds)
	{
		return new uba::NetworkClient(writer, sendSize, receiveTimeoutSeconds);
	}

	void DestroyClient(uba::NetworkClient* client)
	{
		delete client;
	}
	*/

	uba::Storage* CreateStorage(const uba::tchar* rootDir, uba::u64 casCapacityBytes, bool storeCompressed)
	{
		uba::StorageCreateInfo info(rootDir, uba::g_consoleLogWriter);
		info.casCapacityBytes = casCapacityBytes;
		info.storeCompressed = storeCompressed;
		return new uba::StorageImpl(info, nullptr);
	}

	void DestroyStorage(uba::Storage* storage)
	{
		delete storage;
	}

	void Storage_SaveCasTable(uba::Storage* storage)
	{
		storage->SaveCasTable(true);
	}

	void Storage_DeleteFile(uba::Storage* storage, const uba::tchar* file)
	{
		storage->DeleteCasForFile(file);
	}

	uba::Storage* CreateStorageServer(uba::NetworkServer& server, const uba::tchar* rootDir, uba::u64 casCapacityBytes, bool storeCompressed, uba::LogWriter& writer, const uba::tchar* zone)
	{
		using namespace uba;

		#if UBA_USE_AWS
		StringBuffer<> fixedRootDir;
		fixedRootDir.count = GetFullPathNameW(rootDir, fixedRootDir.capacity, fixedRootDir.data, NULL);
		fixedRootDir.Replace('/', PathSeparator).EnsureEndsWithSlash();
		rootDir = fixedRootDir.data;
		AWS aws;
		if (!zone || !*zone)
		{
			LoggerWithWriter logger(writer, TC(""));
			if (aws.QueryAvailabilityZone(logger, rootDir))
				zone = aws.GetAvailabilityZone();
		}
		#endif

		StringBuffer<256> zoneTemp;
		if (!zone || !*zone)
		{
			zoneTemp.count = GetEnvironmentVariableW(TC("UBA_ZONE"), zoneTemp.data, zoneTemp.capacity);
			zone = zoneTemp.data;
		}

		StorageServerCreateInfo info(server, rootDir, writer);
		info.casCapacityBytes = casCapacityBytes;
		info.storeCompressed = storeCompressed;
		info.zone = zone;
		return new StorageServer(info);
	}

	void DestroyStorageServer(uba::Storage* storageServer)
	{
		delete storageServer;
	}

	void StorageServer_SaveCasTable(uba::Storage* storageServer)
	{
		storageServer->SaveCasTable(true);
	}

	void StorageServer_RegisterDisallowedPath(uba::StorageServer* storageServer, const uba::tchar* path)
	{
		storageServer->RegisterDisallowedPath(path);
	}

	/*
uba::StorageClient* CreateStorageClient(uba::NetworkClient& client, const uba::tchar* rootDir, uba::u64 casCapacityBytes, bool storeCompressed, bool sendCompressed)
	{
		uba::StorageClientCreateInfo info(client, rootDir);
		info.casCapacityBytes = casCapacityBytes;
		info.storeCompressed = storeCompressed;
		info.sendCompressed = sendCompressed;
		info.workManager = &client;
		return new uba::StorageClient(info);
	}
	*/

	void DestroySession(uba::Session* session)
	{
		delete session;
	}

	uba::u32 ProcessHandle_GetExitCode(uba::ProcessHandle* handle)
	{
		return handle->GetExitCode();
	}

	const uba::tchar* ProcessHandle_GetExecutingHost(uba::ProcessHandle* handle)
	{
		return handle->GetExecutingHost();
	}

	const uba::tchar* ProcessHandle_GetLogLine(const uba::ProcessHandle* handle, uba::u32 index)
	{
		const auto& lines = handle->GetLogLines();
		if (index >= lines.size()) return nullptr;
		return lines[index].text.c_str();
	}

	uba::u64 ProcessHandle_GetHash(uba::ProcessHandle* handle)
	{
		return handle->GetHash();
	}

	// 100ns ticks
	uba::u64 ProcessHandle_GetTotalProcessorTime(uba::ProcessHandle* handle)
	{
		return uba::TimeToTick(handle->GetTotalProcessorTime());
	}

	// 100ns ticks
	uba::u64 ProcessHandle_GetTotalWallTime(uba::ProcessHandle* handle)
	{
		return uba::TimeToTick(handle->GetTotalWallTime());
	}

	void ProcessHandle_Cancel(uba::ProcessHandle* handle, bool terminate)
	{
		handle->Cancel(terminate);
	}

	void DestroyProcessHandle(uba::ProcessHandle* handle)
	{
		delete handle;
	}

	const uba::ProcessStartInfo* Process_GetStartInfo(uba::Process& process)
	{
		return &process.GetStartInfo();
	}

/*
	uba::SessionClient* CreateSessionClient(const uba::SessionClientCreateInfo& info)
	{
		using namespace uba;
		return new SessionClient(info);
	}
*/

	uba::SessionServerCreateInfo* CreateSessionServerCreateInfo(uba::Storage& storage, uba::NetworkServer& client, uba::LogWriter& writer, const uba::tchar* rootDir, const uba::tchar* traceOutputFile, bool disableCustomAllocator, bool launchVisualizer, bool resetCas, bool writeToDisk, bool detailedTrace, bool allowWaitOnMem, bool allowKillOnMem)
	{
		auto info = new uba::SessionServerCreateInfo(storage, client, writer);
		info->rootDir = TStrdup(rootDir);
		info->traceOutputFile = TStrdup(traceOutputFile);
		info->disableCustomAllocator = disableCustomAllocator;
		info->launchVisualizer = launchVisualizer;
		info->resetCas = resetCas;
		info->shouldWriteToDisk = writeToDisk;
		info->detailedTrace = detailedTrace;
		info->allowWaitOnMem = allowWaitOnMem;
		info->allowKillOnMem = allowKillOnMem;
		//info->remoteTraceEnabled = true;
		//info->remoteLogEnabled = true;
		return info;
	}

	void DestroySessionServerCreateInfo(uba::SessionServerCreateInfo* info)
	{
		free((void*)info->traceOutputFile);
		free((void*)info->rootDir);
		delete info;
	}

	uba::SessionServer* CreateSessionServer(const uba::SessionServerCreateInfo& info)
	{
		return new uba::SessionServer(info);
	}
	void SessionServer_SetRemoteProcessAvailable(uba::SessionServer* server, SessionServer_RemoteProcessAvailableCallback* available, void* userData)
	{
		server->SetRemoteProcessSlotAvailableEvent([available, userData]() { available(userData); });
	}
	void SessionServer_SetRemoteProcessReturned(uba::SessionServer* server, SessionServer_RemoteProcessReturnedCallback* returned, void* userData)
	{
		server->SetRemoteProcessReturnedEvent([returned, userData](uba::Process& process) { returned(process, userData); });
	}
	void SessionServer_RefreshDirectory(uba::SessionServer* server, const uba::tchar* directory)
	{
		server->RefreshDirectory(directory);
	}
	void SessionServer_RegisterNewFile(uba::SessionServer* server, const uba::tchar* filePath)
	{
		server->RegisterNewFile(filePath);
	}
	void SessionServer_RegisterDeleteFile(uba::SessionServer* server, const uba::tchar* filePath)
	{
		server->RegisterDeleteFile(filePath);
	}
	uba::ProcessHandle* SessionServer_RunProcess(uba::SessionServer* server, uba::ProcessStartInfo& info, bool async, bool enableDetour)
	{
		return new uba::ProcessHandle(server->RunProcess(info, async, enableDetour));
	}
	uba::ProcessHandle* SessionServer_RunProcessRemote(uba::SessionServer* server, uba::ProcessStartInfo& info, float weight, const void* knownInputs, uba::u32 knownInputsCount)
	{
		return new uba::ProcessHandle(server->RunProcessRemote(info, weight, knownInputs, knownInputsCount));
	}
	uba::ProcessHandle* SessionServer_RunProcessRacing(uba::SessionServer* server, uba::u32 raceAgainstRemoteProcessId)
	{
		return new uba::ProcessHandle(server->RunProcessRacing(raceAgainstRemoteProcessId));
	}
	void SessionServer_SetMaxRemoteProcessCount(uba::SessionServer* server, uba::u32 count)
	{
		return server->SetMaxRemoteProcessCount(count);
	}
	void SessionServer_DisableRemoteExecution(uba::SessionServer* server)
	{
		server->GetServer().DisallowNewClients();
		server->DisableRemoteExecution();
	}
	void SessionServer_PrintSummary(uba::SessionServer* server)
	{
		uba::LoggerWithWriter logger(server->GetLogWriter());
		server->PrintSummary(logger);
		server->GetStorage().PrintSummary(logger);
		server->GetServer().PrintSummary(logger);
		uba::SystemStats::GetGlobal().Print(logger, true);
		uba::PrintContentionSummary(logger);
	}
	void SessionServer_CancelAll(uba::SessionServer* server)
	{
		server->CancelAllProcessesAndWait();
	}
	void SessionServer_SetCustomCasKeyFromTrackedInputs(uba::SessionServer* server, uba::ProcessHandle* handle, const uba::tchar* fileName, const uba::tchar* workingDir)
	{
		const auto& TrackedInputs = handle->GetTrackedInputs();
		server->SetCustomCasKeyFromTrackedInputs(fileName, workingDir, TrackedInputs.data(), (uba::u32)TrackedInputs.size());
	}
	uba::u32 SessionServer_BeginExternalProcess(uba::SessionServer* server, const uba::tchar* description)
	{
		return server->BeginExternalProcess(description);
	}
	void SessionServer_EndExternalProcess(uba::SessionServer* server, uba::u32 id, uba::u32 exitCode)
	{
		server->EndExternalProcess(id, exitCode);
	}

	void SessionServer_UpdateStatus(uba::SessionServer* server, uba::u32 statusIndex, uba::u32 statusNameIndent, const uba::tchar* statusName, uba::u32 statusTextIndent, const uba::tchar* statusText, uba::LogEntryType statusType)
	{
		server->UpdateStatus(statusIndex, statusNameIndent, statusName, statusTextIndent, statusText, statusType);
	}

	void SessionServer_RegisterCustomService(uba::SessionServer* server, SessionServer_CustomServiceFunction* function, void* userData)
	{
		server->RegisterCustomService([function, userData](uba::Process& process, const void* recv, uba::u32 recvSize, void* send, uba::u32 sendCapacity)
			{
				uba::ProcessHandle h(&process);
				return function(&h, recv, recvSize, send, sendCapacity, userData);
			});
	}

	void DestroySessionServer(uba::SessionServer* server)
	{
		if (server)
		{
			auto& s = (uba::NetworkServerWithBackend&)server->GetServer();
			s.networkBackend->StopListen();
			s.DisconnectClients();
		}
		delete server;
	}

	uba::ProcessStartInfo* CreateProcessStartInfo(const uba::tchar* application, const uba::tchar* arguments, const uba::tchar* workingDir, const uba::tchar* description, uba::u32 priorityClass, uba::u64 outputStatsThresholdMs, bool trackInputs, const uba::tchar* logFile, ProcessHandle_ExitCallback* exit)
	{
		auto info = new uba::ProcessStartInfo();
		info->application = TStrdup(application);
		info->arguments = TStrdup(arguments);
		info->workingDir = TStrdup(workingDir);
		info->description = TStrdup(description);
		info->priorityClass = priorityClass;
		info->outputStatsThresholdMs = outputStatsThresholdMs;
		info->trackInputs = trackInputs;
		info->logFile = TStrdup(logFile);
		info->exitedFunc = exit;
		return info;
	}
	void DestroyProcessStartInfo(uba::ProcessStartInfo* info)
	{
		free((void*)info->application);
		free((void*)info->arguments);
		free((void*)info->workingDir);
		free((void*)info->description);
		free((void*)info->logFile);
		delete info;
	}

	uba::Scheduler* Scheduler_Create(uba::SessionServer* session, uba::u32 maxLocalProcessors, bool enableProcessReuse)
	{
		uba::SchedulerCreateInfo info{*session};
		info.maxLocalProcessors = maxLocalProcessors;
		info.enableProcessReuse = enableProcessReuse;
		return new uba::Scheduler(info);
	}

	void Scheduler_Start(uba::Scheduler* scheduler)
	{
		scheduler->Start();
	}

	void Scheduler_EnqueueProcess(uba::Scheduler* scheduler, const uba::ProcessStartInfo& info, float weight, const void* knownInputs, uba::u32 knownInputsBytes, uba::u32 knownInputsCount)
	{
		uba::EnqueueProcessInfo epi(info);
		epi.weight = weight;
		epi.knownInputs = knownInputs;
		epi.knownInputsBytes = knownInputsBytes;
		epi.knownInputsCount = knownInputsCount;
		scheduler->EnqueueProcess(epi);
	}

	void Scheduler_SetMaxLocalProcessors(uba::Scheduler* scheduler, uba::u32 maxLocalProcessors)
	{
		scheduler->SetMaxLocalProcessors(maxLocalProcessors);
	}

	void Scheduler_Stop(uba::Scheduler* scheduler)
	{
		scheduler->Stop();
	}

	void Scheduler_Destroy(uba::Scheduler* scheduler)
	{
		delete scheduler;
	}

	void Scheduler_GetStats(uba::Scheduler* scheduler, uba::u32& outQueued, uba::u32& outActiveLocal, uba::u32& outActiveRemote, uba::u32& outFinished)
	{
		scheduler->GetStats(outQueued, outActiveLocal, outActiveRemote, outFinished);
	}

	void Uba_SetCustomAssertHandler(Uba_CustomAssertHandler* handler)
	{
		uba::SetCustomAssertHandler(handler);
	}

	void Uba_FindImports(const uba::tchar* binary, ImportFunc* func, void* userData)
	{
#if PLATFORM_WINDOWS
		uba::FindImports(binary, [&](const uba::tchar* importName, bool isKnown) { func(importName, userData); });
#endif
	}
}
