// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaNetwork.h"
#include "UbaSessionServerCreateInfo.h"
#include "UbaProcessStartInfo.h"

namespace uba
{
	class Process;
	class ProcessHandle;
	class Session;
	class SessionServer;
	class Scheduler;
	class Storage;
	class StorageServer;
	struct ProcessStartInfo;
	struct SessionServerCreateInfo;

	class CallbackLogWriter : public LogWriter
	{
	public:
		using BeginScopeCallback = void();
		using EndScopeCallback = void();
		using LogCallback = void(LogEntryType type, const uba::tchar* str, u32 strLen);

		UBA_API CallbackLogWriter(BeginScopeCallback* begin, EndScopeCallback* end, LogCallback* log);
		UBA_API virtual void BeginScope() override;
		UBA_API virtual void EndScope() override;
		UBA_API virtual void Log(LogEntryType type, const uba::tchar* str, u32 strLen, const uba::tchar* prefix = nullptr, u32 prefixLen = 0) override;

	private:
		BeginScopeCallback* m_beginScope;
		EndScopeCallback* m_endScope;
		LogCallback* m_logCallback;
	};
}

extern "C"
{
	UBA_API uba::LogWriter* GetDefaultLogWriter();
	UBA_API uba::LogWriter* CreateCallbackLogWriter(uba::CallbackLogWriter::BeginScopeCallback begin, uba::CallbackLogWriter::EndScopeCallback end, uba::CallbackLogWriter::LogCallback log);
	UBA_API void DestroyCallbackLogWriter(uba::LogWriter* writer);

	UBA_API uba::NetworkServer* CreateServer(uba::LogWriter& writer = uba::g_consoleLogWriter, uba::u32 workerCount = 64, uba::u32 sendSize = uba::SendDefaultSize, uba::u32 receiveTimeoutSeconds = 60, bool useQuic = false);
	UBA_API void DestroyServer(uba::NetworkServer* server);
	UBA_API bool Server_StartListen(uba::NetworkServer* server, int port, const uba::tchar* ip, const uba::tchar* crypto = nullptr);
	UBA_API void Server_Stop(uba::NetworkServer* server);
	UBA_API bool Server_AddClient(uba::NetworkServer* server, const uba::tchar* ip, int port, const uba::tchar* crypto = nullptr);

//	UBA_API uba::NetworkClient* CreateClient(uba::LogWriter& writer = uba::g_consoleLogWriter, uba::u32 sendSize = uba::SendDefaultSize, uba::u32 receiveTimeoutSeconds = 0);
//	UBA_API void DestroyClient(uba::NetworkClient* client);

	UBA_API uba::Storage* CreateStorage(const uba::tchar* rootDir, uba::u64 casCapacityBytes, bool storeCompressed);
	UBA_API void DestroyStorage(uba::Storage* storage);
	UBA_API void Storage_SaveCasTable(uba::Storage* storage);
	UBA_API void Storage_DeleteFile(uba::Storage* storage, const uba::tchar* file);

	UBA_API uba::Storage* CreateStorageServer(uba::NetworkServer& server, const uba::tchar* rootDir, uba::u64 casCapacityBytes, bool storeCompressed, uba::LogWriter& writer = uba::g_consoleLogWriter, const uba::tchar* zone = TC(""));
	UBA_API void DestroyStorageServer(uba::Storage* storageServer);
	UBA_API void StorageServer_SaveCasTable(uba::Storage* storageServer);
	UBA_API void StorageServer_RegisterDisallowedPath(uba::StorageServer* storageServer, const uba::tchar* path);

	//UBA_API uba::StorageClient* CreateStorageClient(uba::NetworkClient& client, const uba::tchar* rootDir, uba::u64 casCapacityBytes, bool storeCompressed, bool sendCompressed);

	UBA_API void DestroySession(uba::Session* session);

	UBA_API uba::u32 ProcessHandle_GetExitCode(uba::ProcessHandle* handle);
	UBA_API const uba::tchar* ProcessHandle_GetExecutingHost(uba::ProcessHandle* handle);
	UBA_API const uba::tchar* ProcessHandle_GetLogLine(const uba::ProcessHandle* handle, uba::u32 index);
	UBA_API uba::u64 ProcessHandle_GetHash(uba::ProcessHandle* handle);
	UBA_API uba::u64 ProcessHandle_GetTotalProcessorTime(uba::ProcessHandle* handle);
	UBA_API uba::u64 ProcessHandle_GetTotalWallTime(uba::ProcessHandle* handle);
	UBA_API void ProcessHandle_Cancel(uba::ProcessHandle* handle, bool terminate);
	UBA_API void DestroyProcessHandle(uba::ProcessHandle* handle);
	UBA_API const uba::ProcessStartInfo* Process_GetStartInfo(uba::Process& process);

	using SessionServer_RemoteProcessAvailableCallback = void(void* userData);
	using SessionServer_RemoteProcessReturnedCallback = void(uba::Process& process, void* userData);
	using SessionServer_CustomServiceFunction = uba::u32(uba::ProcessHandle* handle, const void* recv, uba::u32 recvSize, void* send, uba::u32 sendCapacity, void* userData);

	UBA_API uba::SessionServerCreateInfo* CreateSessionServerCreateInfo(uba::Storage& storage, uba::NetworkServer& client, uba::LogWriter& writer, const uba::tchar* rootDir, const uba::tchar* traceOutputFile,
		bool disableCustomAllocator, bool launchVisualizer, bool resetCas, bool writeToDisk, bool detailedTrace, bool allowWaitOnMem = false, bool allowKillOnMem = false);
	UBA_API void DestroySessionServerCreateInfo(uba::SessionServerCreateInfo* info);
	UBA_API uba::SessionServer* CreateSessionServer(const uba::SessionServerCreateInfo& info);
	UBA_API void SessionServer_SetRemoteProcessAvailable(uba::SessionServer* server, SessionServer_RemoteProcessAvailableCallback* available, void* userData);
	UBA_API void SessionServer_SetRemoteProcessReturned(uba::SessionServer* server, SessionServer_RemoteProcessReturnedCallback* returned, void* userData);
	UBA_API void SessionServer_RefreshDirectory(uba::SessionServer* server, const uba::tchar* directory);
	UBA_API void SessionServer_RegisterNewFile(uba::SessionServer* server, const uba::tchar* filePath);
	UBA_API void SessionServer_RegisterDeleteFile(uba::SessionServer* server, const uba::tchar* filePath);
	UBA_API uba::ProcessHandle* SessionServer_RunProcess(uba::SessionServer* server, uba::ProcessStartInfo& info, bool async, bool enableDetour);
	UBA_API uba::ProcessHandle* SessionServer_RunProcessRemote(uba::SessionServer* server, uba::ProcessStartInfo& info, float weight, const void* knownInputs = nullptr, uba::u32 knownInputsCount = 0);
	UBA_API uba::ProcessHandle* SessionServer_RunProcessRacing(uba::SessionServer* server, uba::u32 raceAgainstRemoteProcessId);

	UBA_API void SessionServer_SetMaxRemoteProcessCount(uba::SessionServer* server, uba::u32 count);
	UBA_API void SessionServer_DisableRemoteExecution(uba::SessionServer* server);
	UBA_API void SessionServer_PrintSummary(uba::SessionServer* server);
	UBA_API void SessionServer_CancelAll(uba::SessionServer* server);
	UBA_API void SessionServer_SetCustomCasKeyFromTrackedInputs(uba::SessionServer* server, uba::ProcessHandle* handle, const uba::tchar* fileName, const uba::tchar* workingDir);
	UBA_API uba::u32 SessionServer_BeginExternalProcess(uba::SessionServer* server, const uba::tchar* description);
	UBA_API void SessionServer_EndExternalProcess(uba::SessionServer* server, uba::u32 id, uba::u32 exitCode);
	UBA_API void SessionServer_UpdateStatus(uba::SessionServer* server, uba::u32 statusIndex, uba::u32 statusNameIndent, const uba::tchar* statusName, uba::u32 statusTextIndent, const uba::tchar* statusText, uba::LogEntryType statusType);
	UBA_API void SessionServer_RegisterCustomService(uba::SessionServer* server, SessionServer_CustomServiceFunction* function, void* userData = nullptr);
	UBA_API void DestroySessionServer(uba::SessionServer* server);

	//UBA_API uba::SessionClient* CreateSessionClient(const uba::SessionClientCreateInfo& info);

	using ProcessHandle_ExitCallback = void(void* userData, const uba::ProcessHandle&);

	UBA_API uba::ProcessStartInfo* CreateProcessStartInfo(const uba::tchar* application, const uba::tchar* arguments, const uba::tchar* workingDir, const uba::tchar* description, uba::u32 priorityClass, uba::u64 outputStatsThresholdMs, bool trackInputs, const uba::tchar* logFile, ProcessHandle_ExitCallback* exit);
	UBA_API void DestroyProcessStartInfo(uba::ProcessStartInfo* info);


	// Scheduler
	UBA_API uba::Scheduler* Scheduler_Create(uba::SessionServer* session, uba::u32 maxLocalProcessors = ~0u, bool enableProcessReuse = false);
	UBA_API void Scheduler_Start(uba::Scheduler* scheduler);
	UBA_API void Scheduler_EnqueueProcess(uba::Scheduler* scheduler, const uba::ProcessStartInfo& info, float weight = 1.0f, const void* knownInputs = nullptr, uba::u32 knownInputsBytes = 0, uba::u32 knownInputsCount = 0);
	UBA_API void Scheduler_SetMaxLocalProcessors(uba::Scheduler* scheduler, uba::u32 maxLocalProcessors);
	UBA_API void Scheduler_Stop(uba::Scheduler* scheduler);
	UBA_API void Scheduler_Destroy(uba::Scheduler* scheduler);
	UBA_API void Scheduler_GetStats(uba::Scheduler* scheduler, uba::u32& outQueued, uba::u32& outActiveLocal, uba::u32& outActiveRemote, uba::u32& outFinished);

	// Misc
	using Uba_CustomAssertHandler = void(const uba::tchar* text);
	UBA_API void Uba_SetCustomAssertHandler(Uba_CustomAssertHandler* handler);

	using ImportFunc = void(const uba::tchar* importName, void* userData);
	UBA_API void Uba_FindImports(const uba::tchar* binary, ImportFunc* func, void* userData);
}
