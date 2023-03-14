// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Containers/Queue.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

#include "CpuUtilizationMonitor.h"
#include "SyncStatus.h"


struct FRunningProcess;
struct FSwitchboardMessageFuture;
struct FSwitchboardTask;
struct FSwitchboardDisconnectTask;
struct FSwitchboardSendFileToClientTask;
struct FSwitchboardStartTask;
struct FSwitchboardKillTask;
struct FSwitchboardReceiveFileFromClientTask;
struct FSwitchboardGetSyncStatusTask;
struct FSwitchboardRefreshMosaicsTask;
struct FSwitchboardRedeployListenerTask;
struct FSwitchboardFixExeFlagsTask;
struct FSwitchboardMinimizeWindowsTask;
struct FSwitchboardSetInactiveTimeoutTask;

class FInternetAddr;
class FSocket;
class FTcpListener;


struct FSwitchboardCommandLineOptions
{
	bool OutputVersion = false;
	bool MinimizeOnLaunch = true;

	TOptional<FIPv4Address> Address;
	TOptional<uint16> Port;
	TOptional<uint32> RedeployFromPid;

	static FSwitchboardCommandLineOptions FromString(const TCHAR* CommandLine);
	FString ToString(bool bIncludeRedeploy = false) const;
};


struct FRedeployStatus
{
	enum class EState : uint8
	{
		// No redeploy initiated.
		None,

		// Redeploy initiated by client, decline subsequent redeploy requests.
		RequestReceived,

		// New listener executable hash verified and written to randomly-named temporary file.
		// This also means we've stopped listening for new connections in this (old) listener.
		NewListenerWrittenTemp,

		// New listener successfully launched and running.
		NewListenerStarted,

		// Current process executable moved to randomly-named temporary file.
		ThisListenerRenamed,

		// New listener executable moved to original location of this executable.
		NewListenerRenamed,

		// Child process has signaled via IPC that it's initialized; shutting down.
		Complete,
	};

	bool InProgress() const
	{
		return State != EState::None && State != EState::Complete;
	}

	EState State = EState::None;

	// Valid when State >= RequestReceived
	FIPv4Endpoint RequestingClient;

	// Valid when State >= NewListenerWrittenTemp
	FString OriginalThisExePath;
	FString TempNewListenerPath;

	// Valid when State >= NewListenerStarted
	FProcHandle ListenerProc;

	// Valid when State >= ThisListenerRenamed
	FString TempThisListenerPath;
};


class FSwitchboardListener
{
	static const FIPv4Endpoint InvalidEndpoint;

public:
	explicit FSwitchboardListener(const FSwitchboardCommandLineOptions& InOptions);
	~FSwitchboardListener();

	bool Init();

	// Returns false if RequestEngineExit was called.
	bool Tick();

	// Used for redeploy synchronization.
	static FString GetIpcSemaphoreName(uint32 ParentPid);

private:
	bool StartListening();
	bool StopListening();

	bool OnIncomingConnection(FSocket* InSocket, const FIPv4Endpoint& InEndpoint);
	bool ParseIncomingMessage(const FString& InMessage, const FIPv4Endpoint& InEndpoint);

	bool RunScheduledTask(const FSwitchboardTask& InTask);
	bool Task_StartProcess(const FSwitchboardStartTask& InRunTask);
	bool Task_KillProcess(const FSwitchboardKillTask& KillTask);
	bool Task_ReceiveFileFromClient(const FSwitchboardReceiveFileFromClientTask& InReceiveFileFromClientTask);
	bool Task_RedeployListener(const FSwitchboardRedeployListenerTask& InRedeployListenerTask);
	bool Task_SendFileToClient(const FSwitchboardSendFileToClientTask& InSendFileToClientTask);
	bool Task_GetSyncStatus(const FSwitchboardGetSyncStatusTask& InGetSyncStatusTask);
	bool Task_RefreshMosaics(const FSwitchboardRefreshMosaicsTask& InRefreshMosaicsTask);
	bool Task_FixExeFlags(const FSwitchboardFixExeFlagsTask& InFixExeFlagsTask);
	bool Task_MinimizeWindows(const FSwitchboardMinimizeWindowsTask& InRefreshMosaicsTask);
	bool Task_SetInactiveTimeout(const FSwitchboardSetInactiveTimeoutTask& InTimeoutTask);

	bool KillProcessNow(FRunningProcess* InProcess, float SoftKillTimeout = 0.0f);
	FRunningProcess* FindOrStartFlipModeMonitorForUUID(const FGuid& UUID);

	void CleanUpDisconnectedSockets();
	void DisconnectClient(const FIPv4Endpoint& InClientEndpoint);
	void HandleStdout(const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& Process);
	void HandleRunningProcesses(TArray<TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>>& Processes, bool bNotifyThatProgramEnded);

	bool SendMessage(const FString& InMessage, const FIPv4Endpoint& InEndpoint);
	void SendMessageFutures();

	bool EquivalentTaskFutureExists(uint32 TaskEquivalenceHash) const;

	void RollbackRedeploy();

private:
	const FSwitchboardCommandLineOptions Options;

	TUniquePtr<FIPv4Endpoint> Endpoint;
	TUniquePtr<FTcpListener> SocketListener;
	TQueue<TPair<FIPv4Endpoint, TSharedPtr<FSocket>>, EQueueMode::Spsc> PendingConnections;
	TMap<FIPv4Endpoint, TSharedPtr<FSocket>> Connections;
	TMap<FIPv4Endpoint, float> InactiveTimeouts;
	TMap<FIPv4Endpoint, double> LastActivityTime;
	TMap<FIPv4Endpoint, TArray<uint8>> ReceiveBuffer;

	TQueue<TUniquePtr<FSwitchboardTask>, EQueueMode::Spsc> ScheduledTasks;
	TQueue<TUniquePtr<FSwitchboardTask>, EQueueMode::Spsc> DisconnectTasks;
	TArray<TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>> RunningProcesses;
	TArray<TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>> FlipModeMonitors;
	TArray<FSwitchboardMessageFuture> MessagesFutures;
	TSharedPtr<FCpuUtilizationMonitor, ESPMode::ThreadSafe> CpuMonitor;

	bool bIsNvAPIInitialized;

	TSharedPtr<FRWLock, ESPMode::ThreadSafe> CachedMosaicToposLock;
	TSharedPtr<TArray<FMosaicTopo>, ESPMode::ThreadSafe> CachedMosaicTopos;

	FRedeployStatus RedeployStatus;
};
