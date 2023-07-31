// Copyright Epic Games, Inc. All Rights Reserved.

#include "SwitchboardListener.h"

#include "CpuUtilizationMonitor.h"
#include "SwitchboardListenerApp.h"
#include "SwitchboardMessageFuture.h"
#include "SwitchboardPacket.h"
#include "SwitchboardProtocol.h"
#include "SwitchboardTasks.h"

#include "Algo/Find.h"
#include "Async/Async.h"
#include "Async/AsyncWork.h"
#include "Common/TcpListener.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IPAddress.h"
#include "Misc/Base64.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include <atomic>

#if PLATFORM_WINDOWS

#pragma warning(push)
#pragma warning(disable : 4005)	// Disable macro redefinition warning for compatibility with Windows SDK 8+

#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <WinUser.h>
#include <shellapi.h>
#include "nvapi.h"
#include "Windows/HideWindowsPlatformTypes.h"

#pragma warning(pop)

static void FillOutMosaicTopologies(TArray<FMosaicTopo>& MosaicTopos);

#endif // PLATFORM_WINDOWS


const FIPv4Endpoint FSwitchboardListener::InvalidEndpoint(FIPv4Address::LanBroadcast, 0);


namespace
{
	const double DefaultInactiveTimeoutSeconds = 5.0;

	bool TryFindIdInBrokenMessage(const FString& InMessage, FGuid& OutGuid)
	{
		// Try to parse message that could not be parsed regularly and look for the message ID.
		// This way we can at least tell Switchboard which message was broken.
		const int32 IdIdx = InMessage.Find(TEXT("\"id\""));
		if (IdIdx > 0)
		{
			const FString Chopped = InMessage.RightChop(IdIdx);
			FString LeftOfComma;
			FString RightOfComma;
			if (Chopped.Split(",", &LeftOfComma, &RightOfComma))
			{
				FString LeftOfColon;
				FString RightOfColon;
				if (LeftOfComma.Split(":", &LeftOfColon, &RightOfColon))
				{
					RightOfColon.TrimStartAndEndInline();
					bool bDoubleQuotesRemoved = false;
					RightOfColon.TrimQuotesInline(&bDoubleQuotesRemoved);
					if (!bDoubleQuotesRemoved)
					{
						// remove single quotes if there were no double quotes
						RightOfColon.LeftChopInline(1);
						RightOfColon.RightChopInline(1);
					}

					return FGuid::Parse(RightOfColon, OutGuid);
				}
			}
		}

		return false;
	}
}

struct FRunningProcess
{
	uint32 PID;
	FGuid UUID;
	FProcHandle Handle;

	void* WritePipe;
	void* ReadPipe;
	TArray<uint8> Output;

	FIPv4Endpoint Recipient;
	FString Path;
	FString Name;
	FString Caller;

	std::atomic<bool> bPendingKill;
	bool bUpdateClientsWithStdout;

	FRunningProcess()
		: bPendingKill(false)
	{}

	FRunningProcess(const FRunningProcess& InProcess)
	{
		PID       = InProcess.PID;
		UUID      = InProcess.UUID;
		Handle    = InProcess.Handle;
		WritePipe = InProcess.WritePipe;
		ReadPipe  = InProcess.ReadPipe;
		Output    = InProcess.Output;
		Recipient = InProcess.Recipient;
		Path      = InProcess.Path;
		Name      = InProcess.Name;
		Caller    = InProcess.Caller;

		bUpdateClientsWithStdout = InProcess.bUpdateClientsWithStdout;

		bPendingKill.store(InProcess.bPendingKill);
	}
};

FSwitchboardCommandLineOptions FSwitchboardCommandLineOptions::FromString(const TCHAR* CommandLine)
{
	FSwitchboardCommandLineOptions OutOptions;

	TArray<FString> Tokens;
	TArray<FString> Switches;
	FCommandLine::Parse(CommandLine, Tokens, Switches);
	TMap<FString, FString> SwitchPairs;
	for (int32 SwitchIdx = Switches.Num() - 1; SwitchIdx >= 0; --SwitchIdx)
	{
		FString& Switch = Switches[SwitchIdx];
		TArray<FString> SplitSwitch;
		if (2 == Switch.ParseIntoArray(SplitSwitch, TEXT("="), true))
		{
			SwitchPairs.Add(SplitSwitch[0], SplitSwitch[1].TrimQuotes());
			Switches.RemoveAt(SwitchIdx);
		}
	}

	if (Switches.Contains(TEXT("version")))
	{
		OutOptions.OutputVersion = true;
	}

	if (Switches.Contains(TEXT("noMinimizeOnLaunch")))
	{
		OutOptions.MinimizeOnLaunch = false;
	}

	if (SwitchPairs.Contains(TEXT("ip")))
	{
		FIPv4Address ParseAddr;
		if (FIPv4Address::Parse(SwitchPairs[TEXT("ip")], ParseAddr))
		{
			OutOptions.Address = ParseAddr;
		}
	}

	if (SwitchPairs.Contains(TEXT("port")))
	{
		OutOptions.Port = FCString::Atoi(*SwitchPairs[TEXT("port")]);
	}

	if (SwitchPairs.Contains(TEXT("RedeployFromPid")))
	{
		OutOptions.RedeployFromPid = FCString::Atoi64(*SwitchPairs[TEXT("RedeployFromPid")]);
	}

	return OutOptions;
}

FString FSwitchboardCommandLineOptions::ToString(bool bIncludeRedeploy /* = false */) const
{
	TArray<FString> Args;

	if (!MinimizeOnLaunch)
	{
		Args.Add(TEXT("-noMinimizeOnLaunch"));
	}

	if (Address.IsSet())
	{
		Args.Add(FString::Printf(TEXT("-ip=%s"), *Address.GetValue().ToString()));
	}

	if (Port.IsSet())
	{
		Args.Add(FString::Printf(TEXT("-port=%u"), Port.GetValue()));
	}

	if (bIncludeRedeploy)
	{
		if (RedeployFromPid.IsSet())
		{
			Args.Add(FString::Printf(TEXT("-RedeployFromPid=%u"), RedeployFromPid.GetValue()));
		}
	}

	return FString::Join(Args, TEXT(" "));
}

FSwitchboardListener::FSwitchboardListener(const FSwitchboardCommandLineOptions& InOptions)
	: Options(InOptions)
	, SocketListener(nullptr)
	, CpuMonitor(MakeShared<FCpuUtilizationMonitor, ESPMode::ThreadSafe>())
	, bIsNvAPIInitialized(false)
	, CachedMosaicToposLock(MakeShared<FRWLock, ESPMode::ThreadSafe>())
	, CachedMosaicTopos(MakeShared<TArray<FMosaicTopo>, ESPMode::ThreadSafe>())
{
#if PLATFORM_WINDOWS
	// initialize NvAPI
	{
		const NvAPI_Status Result = NvAPI_Initialize();
		if (Result == NVAPI_OK)
		{
			bIsNvAPIInitialized = true;

			FillOutMosaicTopologies(*CachedMosaicTopos);
		}
		else
		{
			NvAPI_ShortString ErrorString;
			NvAPI_GetErrorMessage(Result, ErrorString);
			UE_LOG(LogSwitchboard, Error, TEXT("NvAPI_Initialize failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
		}
	}
#endif // PLATFORM_WINDOWS

	const FIPv4Address DefaultIp = FIPv4Address(0, 0, 0, 0);
	if (!Options.Address.IsSet())
	{
		UE_LOG(LogSwitchboard, Warning, TEXT("Defaulting to: -ip=%s"), *DefaultIp.ToString());
	}

	const uint16 DefaultPort = 2980;
	if (!Options.Port.IsSet())
	{
		UE_LOG(LogSwitchboard, Warning, TEXT("Defaulting to: -port=%u"), DefaultPort);
	}

	Endpoint = MakeUnique<FIPv4Endpoint>(Options.Address.Get(DefaultIp), Options.Port.Get(DefaultPort));
}

FSwitchboardListener::~FSwitchboardListener()
{
}

bool FSwitchboardListener::Init()
{
	return StartListening();
}

bool FSwitchboardListener::StartListening()
{
	check(!SocketListener);

	SocketListener = MakeUnique<FTcpListener>(*Endpoint, FTimespan::FromSeconds(1), false);
	if (SocketListener->IsActive())
	{
		SocketListener->OnConnectionAccepted().BindRaw(this, &FSwitchboardListener::OnIncomingConnection);
		UE_LOG(LogSwitchboard, Display, TEXT("Started listening on %s:%d"), *SocketListener->GetLocalEndpoint().Address.ToString(), SocketListener->GetLocalEndpoint().Port);
		return true;
	}
	
	UE_LOG(LogSwitchboard, Error, TEXT("Could not create Tcp Listener!"));
	return false;
}

bool FSwitchboardListener::StopListening()
{
	check(SocketListener.IsValid());
	UE_LOG(LogSwitchboard, Display, TEXT("No longer listening on %s:%d"), *SocketListener->GetLocalEndpoint().Address.ToString(), SocketListener->GetLocalEndpoint().Port);
	SocketListener.Reset();
	return true;
}

bool FSwitchboardListener::Tick()
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Tick);

	// Dequeue pending connections
	{
		TPair<FIPv4Endpoint, TSharedPtr<FSocket>> Connection;
		while (PendingConnections.Dequeue(Connection))
		{
			Connections.Add(Connection);

			FIPv4Endpoint ClientEndpoint = Connection.Key;
			InactiveTimeouts.FindOrAdd(ClientEndpoint, DefaultInactiveTimeoutSeconds);
			LastActivityTime.FindOrAdd(ClientEndpoint, FPlatformTime::Seconds());

			// Send current state upon connection
			{
				FSwitchboardStatePacket StatePacket;

				FPlatformMisc::GetOSVersions(StatePacket.OsVersionLabel, StatePacket.OsVersionLabelSub);
				StatePacket.OsVersionNumber = FPlatformMisc::GetOSVersion();

				StatePacket.TotalPhysicalMemory = FPlatformMemory::GetConstants().TotalPhysical;
				StatePacket.PlatformBinaryDirectory = FPlatformProcess::GetBinariesSubdirectory();

				for (const auto& RunningProcess : RunningProcesses)
				{
					check(RunningProcess.IsValid());

					FSwitchboardStateRunningProcess StateRunningProcess;

					StateRunningProcess.Uuid = RunningProcess->UUID.ToString();
					StateRunningProcess.Name = RunningProcess->Name;
					StateRunningProcess.Path = RunningProcess->Path;
					StateRunningProcess.Caller = RunningProcess->Caller;
					StateRunningProcess.Pid = RunningProcess->PID;

					StatePacket.RunningProcesses.Add(MoveTemp(StateRunningProcess));
				}

				SendMessage(CreateMessage(StatePacket), ClientEndpoint);
			}
		}
	}

	// Parse incoming data from remote connections
	for (const TPair<FIPv4Endpoint, TSharedPtr<FSocket>>& Connection: Connections)
	{
		const FIPv4Endpoint& ClientEndpoint = Connection.Key;
		const TSharedPtr<FSocket>& ClientSocket = Connection.Value;

		uint32 PendingDataSize = 0;
		while (ClientSocket->HasPendingData(PendingDataSize))
		{
			TArray<uint8> Buffer;
			Buffer.AddUninitialized(PendingDataSize);
			int32 BytesRead = 0;
			if (!ClientSocket->Recv(Buffer.GetData(), PendingDataSize, BytesRead, ESocketReceiveFlags::None))
			{
				UE_LOG(LogSwitchboard, Error, TEXT("Error while receiving data via endpoint %s"), *ClientEndpoint.ToString());
				continue;
			}

			LastActivityTime[ClientEndpoint] = FPlatformTime::Seconds();
			TArray<uint8>& MessageBuffer = ReceiveBuffer.FindOrAdd(ClientEndpoint);
			for (int32 i = 0; i < BytesRead; ++i)
			{
				MessageBuffer.Add(Buffer[i]);
				if (Buffer[i] == '\x00')
				{
					const FString Message(UTF8_TO_TCHAR(MessageBuffer.GetData()));
					ParseIncomingMessage(Message, ClientEndpoint);
					MessageBuffer.Empty();
				}
			}
		}
	}

	// Run the next queued task
	if (!ScheduledTasks.IsEmpty())
	{
		TUniquePtr<FSwitchboardTask> Task;
		ScheduledTasks.Dequeue(Task);
		RunScheduledTask(*Task);
	}

	CleanUpDisconnectedSockets();
	HandleRunningProcesses(RunningProcesses, true);
	HandleRunningProcesses(FlipModeMonitors, false);
	SendMessageFutures();

	if (IsEngineExitRequested())
	{
		return false;
	}

	return true;
}

bool FSwitchboardListener::ParseIncomingMessage(const FString& InMessage, const FIPv4Endpoint& InEndpoint)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::ParseIncomingMessage);

	TUniquePtr<FSwitchboardTask> Task;
	bool bEcho = true;
	if (CreateTaskFromCommand(InMessage, InEndpoint, Task, bEcho))
	{
		if (Task->Type == ESwitchboardTaskType::Disconnect)
		{
			DisconnectTasks.Enqueue(MoveTemp(Task));
		}
		else if (Task->Type == ESwitchboardTaskType::KeepAlive)
		{
			LastActivityTime[InEndpoint] = FPlatformTime::Seconds();
		}
		else
		{
			if (bEcho)
			{
				UE_LOG(LogSwitchboard, Display, TEXT("Received %s command"), *Task->Name);
			}

			SendMessage(CreateCommandAcceptedMessage(Task->TaskID), InEndpoint);
			ScheduledTasks.Enqueue(MoveTemp(Task));
		}
		return true;
	}
	else
	{
		FGuid MessageID;
		if (TryFindIdInBrokenMessage(InMessage, MessageID))
		{
			static FString ParseError = FString::Printf(TEXT("Could not parse message %s with ID %s"), *InMessage, *MessageID.ToString());
			SendMessage(CreateCommandDeclinedMessage(MessageID, ParseError), InEndpoint);
		}
		else
		{
			static FString ParseError = FString::Printf(TEXT("Could not parse message %s with unknown ID"), *InMessage);
			UE_LOG(LogSwitchboard, Error, TEXT("%s"), *ParseError);
			
			// just use an empty ID if we couldn't find one
			SendMessage(CreateCommandDeclinedMessage(MessageID, ParseError), InEndpoint);
		}
		return false;
	}
}

bool FSwitchboardListener::RunScheduledTask(const FSwitchboardTask& InTask)
{
	switch (InTask.Type)
	{
		case ESwitchboardTaskType::Start:
		{
			const FSwitchboardStartTask& StartTask = static_cast<const FSwitchboardStartTask&>(InTask);
			return Task_StartProcess(StartTask);
		}
		case ESwitchboardTaskType::Kill:
		{
			const FSwitchboardKillTask& KillTask = static_cast<const FSwitchboardKillTask&>(InTask);
			return Task_KillProcess(KillTask);
		}
		case ESwitchboardTaskType::ReceiveFileFromClient:
		{
			const FSwitchboardReceiveFileFromClientTask& ReceiveFileFromClientTask = static_cast<const FSwitchboardReceiveFileFromClientTask&>(InTask);
			return Task_ReceiveFileFromClient(ReceiveFileFromClientTask);
		}
		case ESwitchboardTaskType::RedeployListener:
		{
			const FSwitchboardRedeployListenerTask& RedeployListenerTask = static_cast<const FSwitchboardRedeployListenerTask&>(InTask);
			const bool bRedeployOk = Task_RedeployListener(RedeployListenerTask);
			if (!bRedeployOk)
			{
				RollbackRedeploy();
			}
			return bRedeployOk;
		}
		case ESwitchboardTaskType::SendFileToClient:
		{
			const FSwitchboardSendFileToClientTask& SendFileToClientTask = static_cast<const FSwitchboardSendFileToClientTask&>(InTask);
			return Task_SendFileToClient(SendFileToClientTask);
		}
		case ESwitchboardTaskType::KeepAlive:
		{
			return true;
		}
		case ESwitchboardTaskType::GetSyncStatus:
		{
			const FSwitchboardGetSyncStatusTask& GetSyncStatusTask = static_cast<const FSwitchboardGetSyncStatusTask&>(InTask);
			return Task_GetSyncStatus(GetSyncStatusTask);
		}
		case ESwitchboardTaskType::FixExeFlags:
		{
			const FSwitchboardFixExeFlagsTask& Task = static_cast<const FSwitchboardFixExeFlagsTask&>(InTask);
			return Task_FixExeFlags(Task);
		}
		case ESwitchboardTaskType::RefreshMosaics:
		{
			const FSwitchboardRefreshMosaicsTask& Task = static_cast<const FSwitchboardRefreshMosaicsTask&>(InTask);
			return Task_RefreshMosaics(Task);
		}
		case ESwitchboardTaskType::MinimizeWindows:
		{
			const FSwitchboardMinimizeWindowsTask& Task = static_cast<const FSwitchboardMinimizeWindowsTask&>(InTask);
			return Task_MinimizeWindows(Task);
		}
		case ESwitchboardTaskType::SetInactiveTimeout:
		{
			const FSwitchboardSetInactiveTimeoutTask& Task = static_cast<const FSwitchboardSetInactiveTimeoutTask&>(InTask);
			return Task_SetInactiveTimeout(Task);
		}
		default:
		{
			static const FString Response = TEXT("Unknown Command detected");
			CreateCommandDeclinedMessage(InTask.TaskID, Response);
			return false;
		}
	}
	return false;
}

static uint32 FindPidInFocus()
{
#if PLATFORM_WINDOWS
	HWND WindowHandle = GetForegroundWindow();

	if (WindowHandle)
	{
		DWORD PID = 0;
		DWORD ThreadId = GetWindowThreadProcessId(WindowHandle, &PID);

		if (ThreadId != 0)
		{
			return PID;
		}
	}
#endif // PLATFORM_WINDOWS

	return 0;
}

#if PLATFORM_WINDOWS
static TArray<FString> RegistryGetValueNames(const HKEY Key)
{
	TArray<FString> Names;
	const uint32 MaxLength = 1024;
	TCHAR ValueName[MaxLength];
	DWORD ValueLength = MaxLength;

	while (!RegEnumValue(Key, Names.Num(), ValueName, &ValueLength, nullptr, nullptr, nullptr, nullptr))
	{
		Names.Add(ValueName);
		ValueLength = MaxLength;
	}

	return Names;
}
#endif // PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
static FString RegistryGetStringValueData(const HKEY Key, const FString& ValueName)
{
	const uint32 MaxLength = 4096;
	TCHAR ValueData[MaxLength];
	DWORD ValueLength = MaxLength;

	if (RegQueryValueEx(Key, *ValueName, 0, 0, LPBYTE(ValueData), &ValueLength))
	{
		return TEXT("");
	}

	ValueData[MaxLength - 1] = '\0';

	return FString(ValueData);
}
#endif // PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
static bool RegistrySetStringValueData(const HKEY Key, const FString& ValueName, const FString& ValueData)
{
	return !!RegSetValueEx(Key, *ValueName, 0, REG_SZ, LPBYTE(*ValueData), sizeof(TCHAR) * ValueData.Len());
}
#endif // PLATFORM_WINDOWS

static bool DisableFullscreenOptimizationForProcess(const FRunningProcess* Process)
{
#if !PLATFORM_WINDOWS
	return false;
#else
	// No point in continuing if there is no process to set the flags for.
	if (!Process)
	{
		return false;
	}

	// This is the absolute path of the program we'll be looking for in the registry
	const FString ProcessAbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Process->Path);
	const FString DisableFsOptLayer = TEXT("DISABLEDXMAXIMIZEDWINDOWEDMODE");
	const FString LayersKeyPath = TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers");

	// Check if the key exists
	HKEY LayersKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, *LayersKeyPath, 0, KEY_ALL_ACCESS, &LayersKey))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		RegCloseKey(LayersKey);
	};

	// If the key exists, the Value Names are the paths to the programs
	const TArray<FString> ProgramPaths = RegistryGetValueNames(LayersKey);
	const FString* ExistingPath = Algo::FindBy(ProgramPaths, ProcessAbsolutePath, [](const FString& ProgPath) { return IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProgPath); });
	if (ExistingPath)
	{
		// If the program already has an entry, get the layers from the Value Data.
		FString ProgramLayers = RegistryGetStringValueData(LayersKey, *ExistingPath);

		TArray<FString> ProgramLayersArray;
		ProgramLayers.ParseIntoArray(ProgramLayersArray, TEXT(" "));

		if (ProgramLayersArray.Contains(DisableFsOptLayer))
		{
			// Layer already present, nothing to do.
			return true;
		}

		// Append the desired layer.
		ProgramLayers = FString::Printf(TEXT("%s %s"), *ProgramLayers, *DisableFsOptLayer);
		return RegistrySetStringValueData(LayersKey, *ExistingPath, ProgramLayers);
	}
	else
	{
		// The path to our executable does not exist as one of the regkey values, so we need to create it.
		const FString ProgramLayers = FString::Printf(TEXT("~ %s"), *DisableFsOptLayer);
		const FString ProgramAbsPath = ProcessAbsolutePath.Replace(TEXT("/"), TEXT("\\"));
		return RegistrySetStringValueData(LayersKey, ProgramAbsPath, ProgramLayers);
	}
#endif // PLATFORM_WINDOWS
}

bool FSwitchboardListener::Task_StartProcess(const FSwitchboardStartTask& InRunTask)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_StartProcess);

	auto NewProcess = MakeShared<FRunningProcess, ESPMode::ThreadSafe>();

	NewProcess->Recipient = InRunTask.Recipient;
	NewProcess->Path = InRunTask.Command;
	NewProcess->Name = InRunTask.Name;
	NewProcess->Caller = InRunTask.Caller;
	NewProcess->bUpdateClientsWithStdout = InRunTask.bUpdateClientsWithStdout;
	NewProcess->UUID = InRunTask.TaskID; // Process ID is the same as the message ID.
	NewProcess->PID = 0; // default value

	if (!FPlatformProcess::CreatePipe(NewProcess->ReadPipe, NewProcess->WritePipe))
	{
		UE_LOG(LogSwitchboard, Error, TEXT("Could not create pipe to read process output!"));
		return false;
	}
	
	const bool bLaunchDetached = false;
	const bool bLaunchHidden = false;
	const bool bLaunchReallyHidden = false;
	const int32 PriorityModifier = InRunTask.PriorityModifier;
	const TCHAR* WorkingDirectory = InRunTask.WorkingDir.IsEmpty() ? nullptr : *InRunTask.WorkingDir;

	NewProcess->Handle = FPlatformProcess::CreateProc(
		*InRunTask.Command, 
		*InRunTask.Arguments, 
		bLaunchDetached, 
		bLaunchHidden, 
		bLaunchReallyHidden, 
		&NewProcess->PID, 
		PriorityModifier, 
		WorkingDirectory, 
		NewProcess->WritePipe,
		NewProcess->ReadPipe
	);

	if (!NewProcess->Handle.IsValid())
	{
		// Close process in case it just didn't run
		FPlatformProcess::CloseProc(NewProcess->Handle);

		// close pipes
		FPlatformProcess::ClosePipe(NewProcess->ReadPipe, NewProcess->WritePipe);

		// log error
		const FString ErrorMsg = FString::Printf(TEXT("Could not start program %s"), *InRunTask.Command);
		UE_LOG(LogSwitchboard, Error, TEXT("%s"), *ErrorMsg);

		// notify Switchboard
		SendMessage(
			CreateTaskDeclinedMessage(
				InRunTask, 
				ErrorMsg, 
				{
					{ TEXT("puuid"), NewProcess->UUID.ToString() },
				}
			), 
			InRunTask.Recipient
		);

		return false;
	}

	UE_LOG(LogSwitchboard, Display, TEXT("Started process %d: %s %s"), NewProcess->PID, *InRunTask.Command, *InRunTask.Arguments);

	RunningProcesses.Add(NewProcess);

	// send message
	{
		FSwitchboardProgramStarted Packet;

		Packet.Process.Uuid = NewProcess->UUID.ToString();
		Packet.Process.Name = NewProcess->Name;
		Packet.Process.Path = NewProcess->Path;
		Packet.Process.Caller = NewProcess->Caller;
		Packet.Process.Pid = NewProcess->PID;

		for (const TPair<FIPv4Endpoint, TSharedPtr<FSocket>>& Connection : Connections)
		{
			const FIPv4Endpoint& ClientEndpoint = Connection.Key;
			SendMessage(CreateMessage(Packet), ClientEndpoint);
		}
	}

	return true;
}

bool FSwitchboardListener::Task_KillProcess(const FSwitchboardKillTask& KillTask)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_KillProcess);

	if (EquivalentTaskFutureExists(KillTask.GetEquivalenceHash()))
	{
		SendMessage(
			CreateTaskDeclinedMessage(
				KillTask,
				TEXT("Duplicate"),
				{
					{ TEXT("puuid"), KillTask.ProgramID.ToString() },
				}
			),
			KillTask.Recipient
		);

		return false;
	}

	// Look in RunningProcesses
	TSharedPtr<FRunningProcess, ESPMode::ThreadSafe> Process;
	{
		auto* ProcessPtr = RunningProcesses.FindByPredicate([&KillTask](const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& InProcess)
		{
			check(InProcess.IsValid());
			return !InProcess->bPendingKill && (InProcess->UUID == KillTask.ProgramID);
		});

		if (ProcessPtr)
		{
			Process = *ProcessPtr;
			Process->bPendingKill = true;
		}
	}

	// Look in FlipModeMonitors
	TSharedPtr<FRunningProcess, ESPMode::ThreadSafe> FlipModeMonitor;
	{
		auto* FlipModeMonitorPtr = FlipModeMonitors.FindByPredicate([&](const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& FlipMonitor)
		{
			check(FlipMonitor.IsValid());
			return !FlipMonitor->bPendingKill && (FlipMonitor->UUID == KillTask.ProgramID);
		});

		if (FlipModeMonitorPtr)
		{
			FlipModeMonitor = *FlipModeMonitorPtr;
			FlipModeMonitor->bPendingKill = true;
		}
	}

	// Create our future message
	FSwitchboardMessageFuture MessageFuture;

	MessageFuture.InEndpoint = KillTask.Recipient;
	MessageFuture.EquivalenceHash = KillTask.GetEquivalenceHash();

	const FGuid UUID = KillTask.ProgramID;

	MessageFuture.Future = Async(EAsyncExecution::ThreadPool, [=]() {
		SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE_STR("FSwitchboardListener::Task_KillProcess future closure");

		const float SoftKillTimeout = 2.0f;

		const bool bKilledProcess = KillProcessNow(Process.Get(), SoftKillTimeout);
		KillProcessNow(FlipModeMonitor.Get(), SoftKillTimeout);

		// Clear bPendingKill
		if (Process)
		{
			Process->bPendingKill = false;
		}

		if (FlipModeMonitor)
		{
			FlipModeMonitor->bPendingKill = false;
		}

		// Return message
		const FString ProgramID = UUID.ToString();

		if (!bKilledProcess)
		{
			const FString KillError = FString::Printf(TEXT("Could not kill program with ID %s"), *ProgramID);

			return CreateTaskDeclinedMessage(
				KillTask,
				KillError,
				{
					{ TEXT("puuid"), ProgramID },
				}
			);
		}

		FSwitchboardProgramKilled Packet;

		if (Process) // must be valid if it was killed
		{
			Packet.Process.Uuid = Process->UUID.ToString();
			Packet.Process.Name = Process->Name;
			Packet.Process.Path = Process->Path;
			Packet.Process.Caller = Process->Caller;
			Packet.Process.Pid = Process->PID;
		}

		return CreateMessage(Packet);
	});

	// Queue it to be sent when ready
	MessagesFutures.Emplace(MoveTemp(MessageFuture));

	return true;
}

bool FSwitchboardListener::Task_FixExeFlags(const FSwitchboardFixExeFlagsTask& Task)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_FixExeFlags);

	if (EquivalentTaskFutureExists(Task.GetEquivalenceHash()))
	{
		SendMessage(	
			CreateTaskDeclinedMessage(
				Task,
				TEXT("Duplicate"),
				{
					{ TEXT("puuid"), Task.ProgramID.ToString() },
				}
			),
			Task.Recipient
		);

		return false;
	}

	// Look in RunningProcesses
	TSharedPtr<FRunningProcess, ESPMode::ThreadSafe> Process;
	{
		auto* ProcessPtr = RunningProcesses.FindByPredicate([&Task](const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& InProcess)
			{
				check(InProcess.IsValid());
				return !InProcess->bPendingKill && (InProcess->UUID == Task.ProgramID);
			});

		if (ProcessPtr)
		{
			Process = *ProcessPtr;
		}
	}

	if (!Process.IsValid())
	{
		SendMessage(
			CreateTaskDeclinedMessage(
				Task,
				TEXT("Could not find ProgramID"),
				{
					{ TEXT("puuid"), Task.ProgramID.ToString() },
				}
			),
			Task.Recipient
		);

		return false;
	}

	return DisableFullscreenOptimizationForProcess(Process.Get());
}


bool FSwitchboardListener::KillProcessNow(FRunningProcess* InProcess, float SoftKillTimeout)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::KillProcessNow);

	if (InProcess && InProcess->Handle.IsValid() && FPlatformProcess::IsProcRunning(InProcess->Handle))
	{
		UE_LOG(LogSwitchboard, Display, TEXT("Killing app with PID %d"), InProcess->PID);

#if PLATFORM_WINDOWS
		// try a soft kill first
		if(SoftKillTimeout > 0)
		{
			const FString Params = FString::Printf(TEXT("/PID %d"), InProcess->PID);

			FString OutStdOut;

			FPlatformProcess::ExecProcess(TEXT("TASKKILL"), *Params, nullptr, &OutStdOut, nullptr);

			const double TimeoutTime = FPlatformTime::Seconds() + SoftKillTimeout;
			const float SleepTime = 0.050f;

			while(FPlatformTime::Seconds() < TimeoutTime && FPlatformProcess::IsProcRunning(InProcess->Handle))
			{
				FPlatformProcess::Sleep(SleepTime);
			}
		}
#endif // PLATFORM_WINDOWS

		if (FPlatformProcess::IsProcRunning(InProcess->Handle))
		{
			const bool bKillTree = true;
			FPlatformProcess::TerminateProc(InProcess->Handle, bKillTree);
		}

		// Pipes will be closed in HandleRunningProcesses
		return true;
	}

	return false;
}

bool FSwitchboardListener::Task_ReceiveFileFromClient(const FSwitchboardReceiveFileFromClientTask& InReceiveFileFromClientTask)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_ReceiveFileFromClient);

	FString Destination = InReceiveFileFromClientTask.Destination;

	if (Destination.Contains(TEXT("%TEMP%")))
	{
		Destination.ReplaceInline(TEXT("%TEMP%"), FPlatformProcess::UserTempDir());
	}
	if (Destination.Contains(TEXT("%RANDOM%")))
	{
		const FString Path = FPaths::GetPath(Destination);
		const FString Extension = FPaths::GetExtension(Destination, true);
		Destination = FPaths::CreateTempFilename(*Path, TEXT(""), *Extension);
	}
	FPlatformMisc::NormalizePath(Destination);
	FPaths::MakePlatformFilename(Destination);

	if (FPaths::FileExists(Destination))
	{
		if (!InReceiveFileFromClientTask.bForceOverwrite)
		{
			const FString ErrorMsg = FString::Printf(TEXT("Destination %s already exist"), *Destination);
			UE_LOG(LogSwitchboard, Error, TEXT("%s"), *ErrorMsg);
			SendMessage(CreateReceiveFileFromClientFailedMessage(Destination, ErrorMsg), InReceiveFileFromClientTask.Recipient);
			return false;
		}
		else
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			PlatformFile.SetReadOnly(*Destination, false);
		}
	}

	TArray<uint8> DecodedFileContent = {};
	FBase64::Decode(InReceiveFileFromClientTask.FileContent, DecodedFileContent);

	UE_LOG(LogSwitchboard, Display, TEXT("Writing %d bytes to %s"), DecodedFileContent.Num(), *Destination);
	if (FFileHelper::SaveArrayToFile(DecodedFileContent, *Destination))
	{
		SendMessage(CreateReceiveFileFromClientCompletedMessage(Destination), InReceiveFileFromClientTask.Recipient);
		return true;
	}

	const FString ErrorMsg = FString::Printf(TEXT("Error while trying to write to %s"), *Destination);
	UE_LOG(LogSwitchboard, Error, TEXT("%s"), *ErrorMsg);
	SendMessage(CreateReceiveFileFromClientFailedMessage(Destination, ErrorMsg), InReceiveFileFromClientTask.Recipient);
	return false;
}

bool FSwitchboardListener::Task_RedeployListener(const FSwitchboardRedeployListenerTask& InRedeployListenerTask)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_RedeployListener);

	if (RedeployStatus.State != FRedeployStatus::EState::None)
	{
		const FString ErrorMsg(TEXT("Redeploy already in progress"));
		UE_LOG(LogSwitchboard, Error, TEXT("Redeploy listener: %s"), *ErrorMsg);
		SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, false, ErrorMsg), InRedeployListenerTask.Recipient);
		return false;
	}

	RedeployStatus.RequestingClient = InRedeployListenerTask.Recipient;
	RedeployStatus.State = FRedeployStatus::EState::RequestReceived;

	TArray<uint8> DecodedFileContent;
	FBase64::Decode(InRedeployListenerTask.FileContent, DecodedFileContent);

	FSHAHash Hash;
	FSHA1::HashBuffer(DecodedFileContent.GetData(), DecodedFileContent.Num(), Hash.Hash);
	if (Hash != InRedeployListenerTask.ExpectedHash)
	{
		const FString ErrorMsg = FString::Printf(TEXT("Incorrect checksum %s; expected %s"),
			*Hash.ToString(), *InRedeployListenerTask.ExpectedHash.ToString());
		UE_LOG(LogSwitchboard, Error, TEXT("Redeploy listener: %s"), *ErrorMsg);
		SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, false, ErrorMsg), InRedeployListenerTask.Recipient);
		return false;
	}

	UE_LOG(LogSwitchboard, Display, TEXT("Successfully validated new listener checksum"));

	const uint32 CurrentPid = FPlatformProcess::GetCurrentProcessId();

	// NOTE: FPlatformProcess::ExecutablePath() is stale if we were moved while running.
	const FString OriginalThisExePath = FPlatformProcess::GetApplicationName(CurrentPid);
	const FString ThisExeDir = FPaths::GetPath(OriginalThisExePath);
	const FString ThisExeExt = FPaths::GetExtension(OriginalThisExePath, true);

	const FString TempNewListenerPath = FPaths::CreateTempFilename(*ThisExeDir, TEXT("SwitchboardListener-New"), *ThisExeExt);

	UE_LOG(LogSwitchboard, Display, TEXT("Writing new listener to \"%s\" (%d bytes)"), *TempNewListenerPath, DecodedFileContent.Num());
	if (!FFileHelper::SaveArrayToFile(DecodedFileContent, *TempNewListenerPath))
	{
		const FString ErrorMsg = FString::Printf(TEXT("Unable to write new listener to \"%s\""), *TempNewListenerPath);
		UE_LOG(LogSwitchboard, Error, TEXT("Redeploy listener: %s"), *ErrorMsg);
		SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, false, ErrorMsg), InRedeployListenerTask.Recipient);
		return false;
	}

	// Unbind the port so the redeployed child process can claim it.
	StopListening();

	// Create IPC semaphore.
	const FString IpcSemaphoreName = GetIpcSemaphoreName(CurrentPid);
	FPlatformProcess::FSemaphore* IpcSemaphore = FPlatformProcess::NewInterprocessSynchObject(IpcSemaphoreName, true);
	ON_SCOPE_EXIT
	{
		if (IpcSemaphore)
		{
			FPlatformProcess::DeleteInterprocessSynchObject(IpcSemaphore);
		}
	};

	if (IpcSemaphore == nullptr)
	{
		const FString ErrorMsg(TEXT("Unable to create IPC semaphore"));
		UE_LOG(LogSwitchboard, Error, TEXT("Redeploy listener: %s"), *ErrorMsg);
		SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, false, ErrorMsg), InRedeployListenerTask.Recipient);
		return false;
	}

	// We acquire to zero immediately; the child process will release/increment when initialized,
	// and we can await that indication by trying to acquire a second time.
	IpcSemaphore->Lock();

	RedeployStatus.OriginalThisExePath = OriginalThisExePath;
	RedeployStatus.TempNewListenerPath = TempNewListenerPath;
	RedeployStatus.State = FRedeployStatus::EState::NewListenerWrittenTemp;

	const FString RedeployArgs = Options.ToString() + FString::Printf(TEXT(" -RedeployFromPid=%u"), CurrentPid);
	const bool bLaunchDetached = true;
	const bool bLaunchHidden = false;
	const bool bLaunchReallyHidden = false;
	uint32* OutProcessId = nullptr;
	const int32 PriorityModifier = 0;
	const TCHAR* WorkingDirectory = nullptr;
	void* PipeWriteChild = nullptr;
	FProcHandle RedeployedProcHandle = FPlatformProcess::CreateProc(*TempNewListenerPath, *RedeployArgs, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, OutProcessId, PriorityModifier, WorkingDirectory, PipeWriteChild);
	if (!RedeployedProcHandle.IsValid())
	{
		const FString ErrorMsg(TEXT("Unable to launch new listener"));
		UE_LOG(LogSwitchboard, Error, TEXT("Redeploy listener: %s"), *ErrorMsg);
		SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, false, ErrorMsg), InRedeployListenerTask.Recipient);
		return false;
	}

	RedeployStatus.ListenerProc = RedeployedProcHandle;
	RedeployStatus.State = FRedeployStatus::EState::NewListenerStarted;

	const FString TempThisListenerPath = FPaths::CreateTempFilename(*ThisExeDir, TEXT("SwitchboardListener-Old"), *ThisExeExt);
	if (!IFileManager::Get().Move(*TempThisListenerPath, *OriginalThisExePath, false))
	{
		const uint32 LastError = FPlatformMisc::GetLastError();
		const FString ErrorMsg = FString::Printf(TEXT("Unable to move old listener to \"%s\" (error code %u)"), *TempThisListenerPath, LastError);
		UE_LOG(LogSwitchboard, Error, TEXT("Redeploy listener: %s"), *ErrorMsg);
		SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, false, ErrorMsg), InRedeployListenerTask.Recipient);
		return false;
	}

	RedeployStatus.TempThisListenerPath = TempThisListenerPath;
	RedeployStatus.State = FRedeployStatus::EState::ThisListenerRenamed;

	if (!IFileManager::Get().Move(*OriginalThisExePath, *TempNewListenerPath, false))
	{
		const uint32 LastError = FPlatformMisc::GetLastError();
		const FString ErrorMsg = FString::Printf(TEXT("Unable to move new listener to \"%s\" (error code %u)"), *TempThisListenerPath, LastError);
		UE_LOG(LogSwitchboard, Error, TEXT("Redeploy listener: %s"), *ErrorMsg);
		SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, false, ErrorMsg), InRedeployListenerTask.Recipient);
		return false;
	}

	RedeployStatus.State = FRedeployStatus::EState::NewListenerRenamed;

	const uint64 IpcReadyWaitSeconds = 5;
	const bool IpcAcquired = IpcSemaphore->TryLock(IpcReadyWaitSeconds * 1'000'000'000ULL);
	if (IpcAcquired == false)
	{
		const FString ErrorMsg(TEXT("Timed out waiting for child process to sync"));
		UE_LOG(LogSwitchboard, Error, TEXT("Redeploy listener: %s"), *ErrorMsg);
		SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, false, ErrorMsg), InRedeployListenerTask.Recipient);
		return false;
	}

	UE_LOG(LogSwitchboard, Display, TEXT("Redeploy complete; shutting down"));
	SendMessage(CreateRedeployStatusMessage(InRedeployListenerTask.TaskID, true, FString(TEXT("Redeployed instance is ready"))), InRedeployListenerTask.Recipient);

	RedeployStatus.State = FRedeployStatus::EState::Complete;
	RequestEngineExit(TEXT("Redeploy complete"));

	return true;
}

void FSwitchboardListener::RollbackRedeploy()
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::RollbackRedeploy);

	// Unwind redeploy actions, in reverse order of corresponding state transitions.
	UE_LOG(LogSwitchboard, Warning, TEXT("Rolling back redeploy..."));

	if ((uint8)RedeployStatus.State >= (uint8)FRedeployStatus::EState::NewListenerRenamed)
	{
		UE_LOG(LogSwitchboard, Warning, TEXT("Moving new listener back to temporary location"));
		IFileManager::Get().Move(*RedeployStatus.TempNewListenerPath, *RedeployStatus.OriginalThisExePath, false);
	}

	if ((uint8)RedeployStatus.State >= (uint8)FRedeployStatus::EState::ThisListenerRenamed)
	{
		UE_LOG(LogSwitchboard, Warning, TEXT("Moving this listener back to original location"));
		IFileManager::Get().Move(*RedeployStatus.OriginalThisExePath, *RedeployStatus.TempThisListenerPath, false);
		RedeployStatus.TempThisListenerPath.Reset();
	}

	if ((uint8)RedeployStatus.State >= (uint8)FRedeployStatus::EState::NewListenerStarted)
	{
		UE_LOG(LogSwitchboard, Warning, TEXT("Terminating temporary listener"));
		FPlatformProcess::TerminateProc(RedeployStatus.ListenerProc);
		FPlatformProcess::WaitForProc(RedeployStatus.ListenerProc);
		FPlatformProcess::CloseProc(RedeployStatus.ListenerProc);
	}

	if ((uint8)RedeployStatus.State >= (uint8)FRedeployStatus::EState::NewListenerWrittenTemp)
	{
		UE_LOG(LogSwitchboard, Warning, TEXT("Removing temporary listener"));
		IFileManager::Get().Delete(*RedeployStatus.TempNewListenerPath, true);
		RedeployStatus.OriginalThisExePath.Reset();
		RedeployStatus.TempNewListenerPath.Reset();

		// Resume listening on this instance.
		StartListening();
	}

	if ((uint8)RedeployStatus.State >= (uint8)FRedeployStatus::EState::RequestReceived)
	{
		RedeployStatus.RequestingClient = FIPv4Endpoint::Any;
		RedeployStatus.State = FRedeployStatus::EState::None;
	}
}

//static
FString FSwitchboardListener::GetIpcSemaphoreName(uint32 ParentPid)
{
	return FString::Printf(TEXT("SwitchboardListenerIpc_Redeploy_%u"), ParentPid);
}

bool FSwitchboardListener::Task_SendFileToClient(const FSwitchboardSendFileToClientTask& InSendFileToClientTask)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_SendFileToClient);

	FString SourceFilePath = InSendFileToClientTask.Source;
	FPlatformMisc::NormalizePath(SourceFilePath);
	FPaths::MakePlatformFilename(SourceFilePath);

	if (!FPaths::FileExists(SourceFilePath))
	{
		const FString ErrorMsg = FString::Printf(TEXT("Could not find file %s"), *SourceFilePath);
		UE_LOG(LogSwitchboard, Error, TEXT("%s"), *ErrorMsg);
		SendMessage(CreateSendFileToClientFailedMessage(InSendFileToClientTask.Source, ErrorMsg), InSendFileToClientTask.Recipient);
		return false;
	}

	TArray<uint8> FileContent;
	if (!FFileHelper::LoadFileToArray(FileContent, *SourceFilePath))
	{
		const FString ErrorMsg = FString::Printf(TEXT("Error reading from file %s"), *SourceFilePath);
		UE_LOG(LogSwitchboard, Error, TEXT("%s"), *ErrorMsg);
		SendMessage(CreateSendFileToClientFailedMessage(InSendFileToClientTask.Source, ErrorMsg), InSendFileToClientTask.Recipient);
		return false;
	}

	const FString EncodedFileContent = FBase64::Encode(FileContent);
	return SendMessage(CreateSendFileToClientCompletedMessage(InSendFileToClientTask.Source, EncodedFileContent), InSendFileToClientTask.Recipient);
}

#if PLATFORM_WINDOWS
static FCriticalSection SwitchboardListenerMutexNvapi;
#endif // PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
static void FillOutSyncTopologies(TArray<FSyncTopo>& SyncTopos)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FillOutSyncTopologies);

	FScopeLock LockNvapi(&SwitchboardListenerMutexNvapi);

	// Normally there is a single sync card. BUT an RTX Server could have more, and we need to account for that.

	// Detect sync cards

	NvU32 GSyncCount = 0;
	NvGSyncDeviceHandle GSyncHandles[NVAPI_MAX_GSYNC_DEVICES];
	NvAPI_GSync_EnumSyncDevices(GSyncHandles, &GSyncCount); // GSyncCount will be zero if error, so no need to check error.

	for (NvU32 GSyncIdx = 0; GSyncIdx < GSyncCount; GSyncIdx++)
	{
		NvU32 GSyncGPUCount = 0;
		NvU32 GSyncDisplayCount = 0;

		// gather info first with null data pointers, just to get the count and subsequently allocate necessary memory.
		{
			const NvAPI_Status Result = NvAPI_GSync_GetTopology(GSyncHandles[GSyncIdx], &GSyncGPUCount, nullptr, &GSyncDisplayCount, nullptr);

			if (Result != NVAPI_OK)
			{
				NvAPI_ShortString ErrorString;
				NvAPI_GetErrorMessage(Result, ErrorString);
				UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GSync_GetTopology failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
				continue;
			}
		}

		// allocate memory for data
		TArray<NV_GSYNC_GPU> GSyncGPUs;
		TArray<NV_GSYNC_DISPLAY> GSyncDisplays;
		{
			GSyncGPUs.SetNumUninitialized(GSyncGPUCount, false);

			for (NvU32 GSyncGPUIdx = 0; GSyncGPUIdx < GSyncGPUCount; GSyncGPUIdx++)
			{
				GSyncGPUs[GSyncGPUIdx].version = NV_GSYNC_GPU_VER;
			}

			GSyncDisplays.SetNumUninitialized(GSyncDisplayCount, false);

			for (NvU32 GSyncDisplayIdx = 0; GSyncDisplayIdx < GSyncDisplayCount; GSyncDisplayIdx++)
			{
				GSyncDisplays[GSyncDisplayIdx].version = NV_GSYNC_DISPLAY_VER;
			}
		}

		// get real info
		{
			const NvAPI_Status Result = NvAPI_GSync_GetTopology(GSyncHandles[GSyncIdx], &GSyncGPUCount, GSyncGPUs.GetData(), &GSyncDisplayCount, GSyncDisplays.GetData());

			if (Result != NVAPI_OK)
			{
				NvAPI_ShortString ErrorString;
				NvAPI_GetErrorMessage(Result, ErrorString);
				UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GSync_GetTopology failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
				continue;
			}
		}

		// Build outbound structure

		FSyncTopo SyncTopo;

		for (NvU32 GpuIdx = 0; GpuIdx < GSyncGPUCount; GpuIdx++)
		{
			FSyncGpu SyncGpu;

			SyncGpu.bIsSynced = GSyncGPUs[GpuIdx].isSynced;
			SyncGpu.Connector = int32(GSyncGPUs[GpuIdx].connector);

			SyncTopo.SyncGpus.Emplace(SyncGpu);
		}

		for (NvU32 DisplayIdx = 0; DisplayIdx < GSyncDisplayCount; DisplayIdx++)
		{
			FSyncDisplay SyncDisplay;

			switch (GSyncDisplays[DisplayIdx].syncState)
			{
			case NVAPI_GSYNC_DISPLAY_SYNC_STATE_UNSYNCED:
				SyncDisplay.SyncState = TEXT("Unsynced");
				break;
			case NVAPI_GSYNC_DISPLAY_SYNC_STATE_SLAVE:
				SyncDisplay.SyncState = TEXT("Follower");
				break;
			case NVAPI_GSYNC_DISPLAY_SYNC_STATE_MASTER:
				SyncDisplay.SyncState = TEXT("Leader");
				break;
			default:
				SyncDisplay.SyncState = TEXT("Unknown");
				break;
			}

			// get color information for each display
			{
				NV_COLOR_DATA ColorData;

				ColorData.version = NV_COLOR_DATA_VER;
				ColorData.cmd = NV_COLOR_CMD_GET;
				ColorData.size = sizeof(NV_COLOR_DATA);

				const NvAPI_Status Result = NvAPI_Disp_ColorControl(GSyncDisplays[DisplayIdx].displayId, &ColorData);

				if (Result == NVAPI_OK)
				{
					SyncDisplay.Bpc = ColorData.data.bpc;
					SyncDisplay.Depth = ColorData.data.depth;
					SyncDisplay.ColorFormat = ColorData.data.colorFormat;
				}
			}

			SyncTopo.SyncDisplays.Emplace(SyncDisplay);
		}

		// Sync Status Parameters
		{
			NV_GSYNC_STATUS_PARAMS GSyncStatusParams;
			GSyncStatusParams.version = NV_GSYNC_STATUS_PARAMS_VER;

			const NvAPI_Status Result = NvAPI_GSync_GetStatusParameters(GSyncHandles[GSyncIdx], &GSyncStatusParams);

			if (Result != NVAPI_OK)
			{
				NvAPI_ShortString ErrorString;
				NvAPI_GetErrorMessage(Result, ErrorString);
				UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GSync_GetStatusParameters failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
				continue;
			}

			SyncTopo.SyncStatusParams.RefreshRate = GSyncStatusParams.refreshRate;
			SyncTopo.SyncStatusParams.HouseSyncIncoming = GSyncStatusParams.houseSyncIncoming;
			SyncTopo.SyncStatusParams.bHouseSync = !!GSyncStatusParams.bHouseSync;
			SyncTopo.SyncStatusParams.bInternalSecondary = GSyncStatusParams.bInternalSlave;
		}

		// Sync Control Parameters
		{
			NV_GSYNC_CONTROL_PARAMS GSyncControlParams;
			GSyncControlParams.version = NV_GSYNC_CONTROL_PARAMS_VER;

			const NvAPI_Status Result = NvAPI_GSync_GetControlParameters(GSyncHandles[GSyncIdx], &GSyncControlParams);

			if (Result != NVAPI_OK)
			{
				NvAPI_ShortString ErrorString;
				NvAPI_GetErrorMessage(Result, ErrorString);
				UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GSync_GetControlParameters failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
				continue;
			}

			SyncTopo.SyncControlParams.bInterlaced = !!GSyncControlParams.interlaceMode;
			SyncTopo.SyncControlParams.bSyncSourceIsOutput = !!GSyncControlParams.syncSourceIsOutput;
			SyncTopo.SyncControlParams.Interval = GSyncControlParams.interval;
			SyncTopo.SyncControlParams.Polarity = GSyncControlParams.polarity;
			SyncTopo.SyncControlParams.Source = GSyncControlParams.source;
			SyncTopo.SyncControlParams.VMode = GSyncControlParams.vmode;

			SyncTopo.SyncControlParams.SyncSkew.MaxLines = GSyncControlParams.syncSkew.maxLines;
			SyncTopo.SyncControlParams.SyncSkew.MinPixels = GSyncControlParams.syncSkew.minPixels;
			SyncTopo.SyncControlParams.SyncSkew.NumLines = GSyncControlParams.syncSkew.numLines;
			SyncTopo.SyncControlParams.SyncSkew.NumPixels = GSyncControlParams.syncSkew.numPixels;

			SyncTopo.SyncControlParams.StartupDelay.MaxLines = GSyncControlParams.startupDelay.maxLines;
			SyncTopo.SyncControlParams.StartupDelay.MinPixels = GSyncControlParams.startupDelay.minPixels;
			SyncTopo.SyncControlParams.StartupDelay.NumLines = GSyncControlParams.startupDelay.numLines;
			SyncTopo.SyncControlParams.StartupDelay.NumPixels = GSyncControlParams.startupDelay.numPixels;
		}

		SyncTopos.Emplace(SyncTopo);
	}
}
#endif // PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
static void FillOutDriverVersion(FSyncStatus& SyncStatus)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FillOutDriverVersion);

	NvU32 DriverVersion;
	NvAPI_ShortString BuildBranchString;

	const NvAPI_Status Result = NvAPI_SYS_GetDriverAndBranchVersion(&DriverVersion, BuildBranchString);

	if (Result != NVAPI_OK)
	{
		NvAPI_ShortString ErrorString;
		NvAPI_GetErrorMessage(Result, ErrorString);
		UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_SYS_GetDriverAndBranchVersion failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
		return;
	}

	SyncStatus.DriverVersion = DriverVersion;
	SyncStatus.DriverBranch = UTF8_TO_TCHAR(BuildBranchString);
}
#endif // PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
static void FillOutTaskbarAutoHide(FSyncStatus& SyncStatus)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FillOutTaskbarAutoHide);

	APPBARDATA AppBarData;

	AppBarData.cbSize = sizeof(APPBARDATA);
	AppBarData.hWnd = nullptr;
	
	const UINT Result = UINT(SHAppBarMessage(ABM_GETSTATE, &AppBarData));
	
	if (Result == ABS_AUTOHIDE)
	{
		SyncStatus.Taskbar = TEXT("AutoHide");
	}
	else
	{
		SyncStatus.Taskbar = TEXT("OnTop");
	}
}
#endif // PLATFORM_WINDOWS


#if PLATFORM_WINDOWS
static void FillOutMosaicTopologies(TArray<FMosaicTopo>& MosaicTopos)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FillOutMosaicTopologies);

	FScopeLock LockNvapi(&SwitchboardListenerMutexNvapi);

	NvU32 GridCount = 0;
	TArray<NV_MOSAIC_GRID_TOPO> GridTopologies;

	// count how many grids
	{
		const NvAPI_Status Result = NvAPI_Mosaic_EnumDisplayGrids(nullptr, &GridCount);

		if (Result != NVAPI_OK)
		{
			NvAPI_ShortString ErrorString;
			NvAPI_GetErrorMessage(Result, ErrorString);
			UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_Mosaic_EnumDisplayGrids failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
			return;
		}
	}

	// get the grids
	{
		GridTopologies.SetNumUninitialized(GridCount, false);

		for (NvU32 TopoIdx = 0; TopoIdx < GridCount; TopoIdx++)
		{
			GridTopologies[TopoIdx].version = NV_MOSAIC_GRID_TOPO_VER;
		}

		const NvAPI_Status Result = NvAPI_Mosaic_EnumDisplayGrids(GridTopologies.GetData(), &GridCount);

		if (Result != NVAPI_OK)
		{
			NvAPI_ShortString ErrorString;
			NvAPI_GetErrorMessage(Result, ErrorString);
			UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_Mosaic_EnumDisplayGrids failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
			return;
		}

		for (NvU32 TopoIdx = 0; TopoIdx < GridCount; TopoIdx++)
		{
			FMosaicTopo MosaicTopo;
			NV_MOSAIC_GRID_TOPO& GridTopo = GridTopologies[TopoIdx];

			MosaicTopo.Columns = GridTopo.columns;
			MosaicTopo.Rows = GridTopo.rows;
			MosaicTopo.DisplayCount = GridTopo.displayCount;

			MosaicTopo.DisplaySettings.Bpp = GridTopo.displaySettings.bpp;
			MosaicTopo.DisplaySettings.Freq = GridTopo.displaySettings.freq;
			MosaicTopo.DisplaySettings.Height = GridTopo.displaySettings.height;
			MosaicTopo.DisplaySettings.Width = GridTopo.displaySettings.width;

			MosaicTopos.Emplace(MosaicTopo);
		}
	}
}
#endif // PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
FRunningProcess* FSwitchboardListener::FindOrStartFlipModeMonitorForUUID(const FGuid& UUID)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::FindOrStartFlipModeMonitorForUUID);

	// See if the associated FlipModeMonitor is running
	{
		auto* FlipModeMonitorPtr = FlipModeMonitors.FindByPredicate([&](const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& FlipMonitor)
		{
			check(FlipMonitor.IsValid());
			return FlipMonitor->UUID == UUID;
		});

		if (FlipModeMonitorPtr)
		{
			return (*FlipModeMonitorPtr).Get();
		}
	}

	// It wasn't in there, so let's find our target process

	TSharedPtr<FRunningProcess, ESPMode::ThreadSafe> Process;
	{
		auto* ProcessPtr = RunningProcesses.FindByPredicate([&](const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& InProcess)
		{
			check(InProcess.IsValid());
			return InProcess->UUID == UUID;
		});

		if (ProcessPtr)
		{
			Process = *ProcessPtr;
		}
	}

	// If the target process does not exist, no point in continuing
	if (!Process.IsValid())
	{
		return nullptr;
	}

	// Ok, we need to create our monitor.

	auto MonitorProcess = MakeShared<FRunningProcess, ESPMode::ThreadSafe>();

	if (!FPlatformProcess::CreatePipe(MonitorProcess->ReadPipe, MonitorProcess->WritePipe))
	{
		UE_LOG(LogSwitchboard, Error, TEXT("Could not create pipe to read MonitorProcess output!"));
		return nullptr;
	}

	const bool bLaunchDetached = true;
	const bool bLaunchHidden = false;
	const bool bLaunchReallyHidden = false;
	const int32 PriorityModifier = 0;
	const TCHAR* WorkingDirectory = nullptr;

	MonitorProcess->Path = FPaths::EngineDir() / TEXT("Binaries") / TEXT("ThirdParty") / TEXT("PresentMon") / TEXT("Win64") / TEXT("PresentMon64-1.5.2.exe");

	FString Arguments = 
		FString::Printf(TEXT("-session_name session_%d -output_stdout -dont_restart_as_admin -terminate_on_proc_exit -stop_existing_session -process_id %d"), 
		Process->PID, Process->PID);

	MonitorProcess->Handle = FPlatformProcess::CreateProc(
		*MonitorProcess->Path,
		*Arguments,
		bLaunchDetached,
		bLaunchHidden,
		bLaunchReallyHidden,
		&MonitorProcess->PID,
		PriorityModifier,
		WorkingDirectory,
		MonitorProcess->WritePipe,
		MonitorProcess->ReadPipe
	);

	if (!MonitorProcess->Handle.IsValid() || !FPlatformProcess::IsProcRunning(MonitorProcess->Handle))
	{
		// Close process in case it just didn't run
		FPlatformProcess::CloseProc(MonitorProcess->Handle);

		// Close unused pipes
		FPlatformProcess::ClosePipe(MonitorProcess->ReadPipe, MonitorProcess->WritePipe);

		// Log error
		const FString ErrorMsg = FString::Printf(TEXT("Could not start FlipMode monitor  %s"), *MonitorProcess->Path);
		UE_LOG(LogSwitchboard, Error, TEXT("%s"), *ErrorMsg);

		return nullptr;
	}

	// Log success
	UE_LOG(LogSwitchboard, Display, TEXT("Started FlipMode monitor %d: %s %s"), MonitorProcess->PID, *MonitorProcess->Path, *Arguments);

	// The UUID corresponds to the program being monitored. This will be used when looking for the Monitor of a given process.
	// The monitor auto-closes when monitored program closes.
	MonitorProcess->UUID = Process->UUID;
	MonitorProcess->bUpdateClientsWithStdout = false;
	MonitorProcess->Recipient = InvalidEndpoint;
	MonitorProcess->Name = TEXT("flipmode_monitor");

	FlipModeMonitors.Add(MonitorProcess);

	return &MonitorProcess.Get();
}
#endif // PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
static void FillOutFlipMode(FSyncStatus& SyncStatus, FRunningProcess* FlipModeMonitor)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FillOutFlipMode);

	// See if the flip monitor is still there.
	if (!FlipModeMonitor || !FlipModeMonitor->Handle.IsValid())
	{
		SyncStatus.FlipModeHistory.Add(TEXT("n/a")); // this informs Switchboard that data is not valid
		return;
	}

	// Get stdout.
	FUTF8ToTCHAR StdoutConv(reinterpret_cast<const ANSICHAR*>(FlipModeMonitor->Output.GetData()), FlipModeMonitor->Output.Num());
	const FString StdOut(StdoutConv.Length(), StdoutConv.Get());

	// Clear out the StdOut array.
	FlipModeMonitor->Output.Empty();

	// Split into lines

	TArray<FString> Lines;
	StdOut.ParseIntoArrayLines(Lines, false);

	// Interpret the output as follows:
	//
	// Application,ProcessID,SwapChainAddress,Runtime,SyncInterval,PresentFlags,AllowsTearing,PresentMode,Dropped,
	// TimeInSeconds,MsBetweenPresents,MsBetweenDisplayChange,MsInPresentAPI,MsUntilRenderComplete,MsUntilDisplayed
	//
	// e.g.
	//   "UnrealEditor.exe,10916,0x0000022096A0F830,DXGI,0,512,0,Composed: Flip,1,3.753577,22.845,0.000,0.880,0.946,0.000"

	TArray<FString> Fields;

	for (const FString& Line : Lines)
	{
		Line.ParseIntoArray(Fields, TEXT(","), false);
		
		if (Fields.Num() != 15)
		{
			continue;
		}

		const int32 PresentMonIdx = 7;

		SyncStatus.FlipModeHistory.Add(Fields[PresentMonIdx]); // The first one will be "PresentMode". This is ok. 
	}
}
#endif // PLATFORM_WINDOWS


#if PLATFORM_WINDOWS
static void FillOutDisableFullscreenOptimizationForProcess(FSyncStatus& SyncStatus, const FRunningProcess* Process)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FillOutDisableFullscreenOptimizationForProcess);

	// Reset output array just in case
	SyncStatus.ProgramLayers.Reset();

	// No point in continuing if there is no process to get the flags for.
	if (!Process)
	{
		return;
	}

	// This is the absolute path of the program we'll be looking for in the registry
	const FString ProcessAbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Process->Path);

	const FString LayersKeyPath = TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers");

	// Check if the key exists
	HKEY LayersKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, *LayersKeyPath, 0, KEY_READ, &LayersKey))
	{
		return;
	}

	// If the key exists, the Value Names are the paths to the programs
	const TArray<FString> ProgramPaths = RegistryGetValueNames(LayersKey);
	const FString* ExistingPath = Algo::FindBy(ProgramPaths, ProcessAbsolutePath, [](const FString& ProgPath) { return IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProgPath); });
	if (ExistingPath)
	{
		// If so, get the layers from the Value Data.
		const FString ProgramLayers = RegistryGetStringValueData(LayersKey, *ExistingPath);
		ProgramLayers.ParseIntoArray(SyncStatus.ProgramLayers, TEXT(" "));
	}

	RegCloseKey(LayersKey);
}
#endif // PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
static void FillOutPhysicalGpuStats(FSyncStatus& SyncStatus)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FillOutPhysicalGpuStats);

	// TODO: Can we somehow use the GPU engine "engtype_3D" perf counters for this instead?

	FScopeLock LockNvapi(&SwitchboardListenerMutexNvapi);

	TArray<NvPhysicalGpuHandle> PhysicalGpuHandles;
	PhysicalGpuHandles.SetNumUninitialized(NVAPI_MAX_PHYSICAL_GPUS);
	NvU32 PhysicalGpuCount;
	NvAPI_Status NvResult = NvAPI_EnumPhysicalGPUs(PhysicalGpuHandles.GetData(), &PhysicalGpuCount);
	if (NvResult != NVAPI_OK)
	{
		NvAPI_ShortString ErrorString;
		NvAPI_GetErrorMessage(NvResult, ErrorString);
		UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_EnumPhysicalGPUs failed. Error: %s"), ANSI_TO_TCHAR(ErrorString));
		return;
	}

	PhysicalGpuHandles.SetNum(PhysicalGpuCount);

	// Sort first by bus, then by bus slot, ascending. Consistent with task manager and others.
	Algo::Sort(PhysicalGpuHandles, [](const NvPhysicalGpuHandle& Lhs, const NvPhysicalGpuHandle& Rhs) -> bool {
		NvU32 LhsBusId, RhsBusId, LhsSlotId, RhsSlotId;
		const NvAPI_Status LhsBusResult = NvAPI_GPU_GetBusId(Lhs, &LhsBusId);
		const NvAPI_Status RhsBusResult = NvAPI_GPU_GetBusId(Rhs, &RhsBusId);
		const NvAPI_Status LhsSlotResult = NvAPI_GPU_GetBusSlotId(Lhs, &LhsSlotId);
		const NvAPI_Status RhsSlotResult = NvAPI_GPU_GetBusSlotId(Rhs, &RhsSlotId);

		if (LhsBusResult != NVAPI_OK || RhsBusResult != NVAPI_OK)
		{
			NvAPI_ShortString LhsErrorString;
			NvAPI_ShortString RhsErrorString;
			NvAPI_GetErrorMessage(LhsBusResult, LhsErrorString);
			NvAPI_GetErrorMessage(RhsBusResult, RhsErrorString);
			UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GPU_GetBusId failed. Errors: %s, %s"), ANSI_TO_TCHAR(LhsErrorString), ANSI_TO_TCHAR(RhsErrorString));
			return false;
		}

		if (LhsSlotResult != NVAPI_OK || RhsSlotResult != NVAPI_OK)
		{
			NvAPI_ShortString LhsErrorString;
			NvAPI_ShortString RhsErrorString;
			NvAPI_GetErrorMessage(LhsSlotResult, LhsErrorString);
			NvAPI_GetErrorMessage(RhsSlotResult, RhsErrorString);
			UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GPU_GetBusSlotId failed. Errors: %s, %s"), ANSI_TO_TCHAR(LhsErrorString), ANSI_TO_TCHAR(RhsErrorString));
			return false;
		}

		if (LhsBusId != RhsBusId)
		{
			return LhsBusId < RhsBusId;
		}

		return LhsSlotId < RhsSlotId;
	});

	SyncStatus.GpuUtilization.SetNumUninitialized(PhysicalGpuCount);
	SyncStatus.GpuCoreClocksKhz.SetNumUninitialized(PhysicalGpuCount);
	SyncStatus.GpuTemperature.SetNumUninitialized(PhysicalGpuCount);

	for (NvU32 PhysicalGpuIdx = 0; PhysicalGpuIdx < PhysicalGpuCount; ++PhysicalGpuIdx)
	{
		SyncStatus.GpuUtilization[PhysicalGpuIdx] = -1;
		SyncStatus.GpuCoreClocksKhz[PhysicalGpuIdx] = -1;
		SyncStatus.GpuTemperature[PhysicalGpuIdx] = MIN_int32;

		const NvPhysicalGpuHandle& PhysicalGpu = PhysicalGpuHandles[PhysicalGpuIdx];

		NV_GPU_DYNAMIC_PSTATES_INFO_EX PstatesInfo;
		PstatesInfo.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
		NvResult = NvAPI_GPU_GetDynamicPstatesInfoEx(PhysicalGpu, &PstatesInfo);
		if (NvResult == NVAPI_OK)
		{
			// FIXME: NV_GPU_UTILIZATION_DOMAIN_ID enum is missing in our nvapi.h, but documented elsewhere.
			//const int8 UtilizationDomain = NVAPI_GPU_UTILIZATION_DOMAIN_GPU;
			const int8 UtilizationDomain = 0;
			if (PstatesInfo.utilization[UtilizationDomain].bIsPresent)
			{
				SyncStatus.GpuUtilization[PhysicalGpuIdx] = PstatesInfo.utilization[UtilizationDomain].percentage;
			}
		}
		else
		{
			UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GPU_GetDynamicPstatesInfoEx failed. Error code: %d"), NvResult);
		}

		NV_GPU_CLOCK_FREQUENCIES ClockFreqs;
		ClockFreqs.version = NV_GPU_CLOCK_FREQUENCIES_VER;
		ClockFreqs.ClockType = NV_GPU_CLOCK_FREQUENCIES_CURRENT_FREQ;
		NvResult = NvAPI_GPU_GetAllClockFrequencies(PhysicalGpu, &ClockFreqs);
		if (NvResult == NVAPI_OK)
		{
			if (ClockFreqs.domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].bIsPresent)
			{
				SyncStatus.GpuCoreClocksKhz[PhysicalGpuIdx] = ClockFreqs.domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].frequency;
			}
		}
		else
		{
			UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GPU_GetAllClockFrequencies failed. Error code: %d"), NvResult);
		}

		NV_GPU_THERMAL_SETTINGS ThermalSettings;
		ThermalSettings.version = NV_GPU_THERMAL_SETTINGS_VER;
		NvResult = NvAPI_GPU_GetThermalSettings(PhysicalGpu, NVAPI_THERMAL_TARGET_ALL, &ThermalSettings);
		if (NvResult == NVAPI_OK)
		{
			// Report max temp across all sensors for this GPU.
			for (NvU32 SensorIdx = 0; SensorIdx < ThermalSettings.count; ++SensorIdx)
			{
				const NvS32 SensorTemp = ThermalSettings.sensor[SensorIdx].currentTemp;
				if (SensorTemp > SyncStatus.GpuTemperature[PhysicalGpuIdx])
				{
					SyncStatus.GpuTemperature[PhysicalGpuIdx] = SensorTemp;
				}
			}
		}
		else
		{
			UE_LOG(LogSwitchboard, Warning, TEXT("NvAPI_GPU_GetThermalSettings failed. Error code: %d"), NvResult);
		}
	}
}
#endif

bool FSwitchboardListener::EquivalentTaskFutureExists(uint32 TaskEquivalenceHash) const
{
	return !!MessagesFutures.FindByPredicate([=](const FSwitchboardMessageFuture& MessageFuture)
	{
		return MessageFuture.EquivalenceHash == TaskEquivalenceHash;
	});
}

bool FSwitchboardListener::Task_GetSyncStatus(const FSwitchboardGetSyncStatusTask& InGetSyncStatusTask)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_GetSyncStatus);

#if PLATFORM_WINDOWS
	// Reject request if an equivalent one is already in our future
	if (EquivalentTaskFutureExists(InGetSyncStatusTask.GetEquivalenceHash()))
	{
		SendMessage(
			CreateTaskDeclinedMessage(InGetSyncStatusTask, TEXT("Duplicate"),{}),
			InGetSyncStatusTask.Recipient
		);

		return false;
	}

	TSharedRef<FSyncStatus, ESPMode::ThreadSafe> SyncStatus = MakeShared<FSyncStatus, ESPMode::ThreadSafe>(); // Smart pointer to avoid potentially bigger copy to lambda below.

	// We need to run these on this thread to avoid threading issues.
	FillOutFlipMode(SyncStatus.Get(), FindOrStartFlipModeMonitorForUUID(InGetSyncStatusTask.ProgramID));

	// Fill out fullscreen optimization setting
	{
		auto* ProcessPtr = RunningProcesses.FindByPredicate([&](const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& InProcess)
		{
			check(InProcess.IsValid());
			return !InProcess->bPendingKill && (InProcess->UUID == InGetSyncStatusTask.ProgramID);
		});

		FRunningProcess* Process = nullptr;

		if (ProcessPtr)
		{
			Process = (*ProcessPtr).Get();
		}

		FillOutDisableFullscreenOptimizationForProcess(SyncStatus.Get(), Process);
	}

	// Create our future message
	FSwitchboardMessageFuture MessageFuture;

	MessageFuture.InEndpoint = InGetSyncStatusTask.Recipient;
	MessageFuture.EquivalenceHash = InGetSyncStatusTask.GetEquivalenceHash();

	MessageFuture.Future = Async(EAsyncExecution::ThreadPool,
		[
			SyncStatus,
			IsNvAPIInitialized=bIsNvAPIInitialized,
			CpuMonitor=CpuMonitor,
			CachedMosaicToposLock=CachedMosaicToposLock,
			CachedMosaicTopos=CachedMosaicTopos
		]() {
			SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE_STR("FSwitchboardListener::Task_GetSyncStatus future closure");

			FillOutTaskbarAutoHide(SyncStatus.Get());

			if (IsNvAPIInitialized)
			{
				FillOutDriverVersion(SyncStatus.Get());
				FillOutSyncTopologies(SyncStatus->SyncTopos);
			}

			{
				FReadScopeLock Lock(*CachedMosaicToposLock);
				SyncStatus->MosaicTopos = *CachedMosaicTopos;
			}

			SyncStatus->PidInFocus = FindPidInFocus();

			if (CpuMonitor)
			{
				CpuMonitor->GetPerCoreUtilization(SyncStatus->CpuUtilization);
			}

			const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
			SyncStatus->AvailablePhysicalMemory = MemStats.AvailablePhysical;

			if (IsNvAPIInitialized)
			{
				FillOutPhysicalGpuStats(SyncStatus.Get());
			}

			return CreateSyncStatusMessage(SyncStatus.Get());
		}
	);

	// Queue it to be sent when ready
	MessagesFutures.Emplace(MoveTemp(MessageFuture));

	return true;
#else
	SendMessage(CreateTaskDeclinedMessage(InGetSyncStatusTask, "Platform not supported", {}), InGetSyncStatusTask.Recipient);
	return false;
#endif // PLATFORM_WINDOWS
}

bool FSwitchboardListener::Task_RefreshMosaics(const FSwitchboardRefreshMosaicsTask& InRefreshMosaicsTask)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_RefreshMosaics);

#if PLATFORM_WINDOWS
	if (!bIsNvAPIInitialized)
	{
		SendMessage(
			CreateTaskDeclinedMessage(InRefreshMosaicsTask, "NvAPI not supported", {}),
			InRefreshMosaicsTask.Recipient);
		return false;
	}

	// Reject request if an equivalent one is already in our future
	if (EquivalentTaskFutureExists(InRefreshMosaicsTask.GetEquivalenceHash()))
	{
		SendMessage(
			CreateTaskDeclinedMessage(InRefreshMosaicsTask, TEXT("Duplicate"), {}),
			InRefreshMosaicsTask.Recipient
		);

		return false;
	}

	// Create our future
	FSwitchboardMessageFuture TaskFuture;
	TaskFuture.EquivalenceHash = InRefreshMosaicsTask.GetEquivalenceHash();
	TaskFuture.Future = Async(EAsyncExecution::ThreadPool,
		[
			CachedMosaicToposLock=CachedMosaicToposLock,
			CachedMosaicTopos=CachedMosaicTopos
		]()
		{
			SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE_STR("FSwitchboardListener::Task_RefreshMosaics future closure");

			FWriteScopeLock Lock(*CachedMosaicToposLock);
			CachedMosaicTopos->Reset();
			FillOutMosaicTopologies(*CachedMosaicTopos);
			
			return FString();
		}
	);

	MessagesFutures.Emplace(MoveTemp(TaskFuture));

	return true;
#else
	SendMessage(CreateTaskDeclinedMessage(InRefreshMosaicsTask, "Platform not supported", {}), InRefreshMosaicsTask.Recipient);
	return false;
#endif // PLATFORM_WINDOWS
}

bool FSwitchboardListener::Task_MinimizeWindows(const FSwitchboardMinimizeWindowsTask& InTask)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::Task_MinimizeWindows);

#if PLATFORM_WINDOWS
	// Reject request if an equivalent one is already in our future
	if (EquivalentTaskFutureExists(InTask.GetEquivalenceHash()))
	{
		SendMessage(
			CreateTaskDeclinedMessage(InTask, TEXT("Duplicate"), {}),
			InTask.Recipient
		);

		return false;
	}

	// Create our future
	FSwitchboardMessageFuture TaskFuture;
	TaskFuture.EquivalenceHash = InTask.GetEquivalenceHash();
	TaskFuture.Future = Async(EAsyncExecution::ThreadPool,
		[]()
		{
			SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE_STR("FSwitchboardListener::Task_MinimizeWindows future closure");

			if (const HWND HWnd = ::FindWindowA("Shell_TrayWnd", NULL))
			{
				::SendMessageA(HWnd, WM_COMMAND, (WPARAM)419, 0);
			}

			return FString();
		}
	);

	MessagesFutures.Emplace(MoveTemp(TaskFuture));

	return true;
#else
	SendMessage(CreateTaskDeclinedMessage(InTask, "Platform not supported", {}), InTask.Recipient);
	return false;
#endif // PLATFORM_WINDOWS
}

bool FSwitchboardListener::Task_SetInactiveTimeout(const FSwitchboardSetInactiveTimeoutTask& InTimeoutTask)
{
	const FIPv4Endpoint& Client = InTimeoutTask.Recipient;
	const float RequestedTimeout = InTimeoutTask.TimeoutSeconds;

	if (RequestedTimeout < DefaultInactiveTimeoutSeconds)
	{
		SendMessage(CreateTaskDeclinedMessage(InTimeoutTask, "Requested timeout too low", {}), Client);
		return false;
	}

	float& ClientTimeout = InactiveTimeouts[Client];
	if (RequestedTimeout != ClientTimeout)
	{
		UE_LOG(LogSwitchboard, Display, TEXT("Changing client %s inactive timeout from %.0f to %.0f seconds"),
			*Client.ToString(), ClientTimeout, RequestedTimeout);
		ClientTimeout = RequestedTimeout;
	}

	return true;
}

void FSwitchboardListener::CleanUpDisconnectedSockets()
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::CleanUpDisconnectedSockets);

	const double CurrentTime = FPlatformTime::Seconds();
	for (const TPair<FIPv4Endpoint, double>& LastActivity : LastActivityTime)
	{
		const FIPv4Endpoint& Client = LastActivity.Key;
		const float ClientTimeout = InactiveTimeouts[Client];
		if (CurrentTime - LastActivity.Value > ClientTimeout)
		{
			UE_LOG(LogSwitchboard, Warning, TEXT("Client %s has been inactive for more than %.1fs -- closing connection"), *Client.ToString(), ClientTimeout);
			TUniquePtr<FSwitchboardDisconnectTask> DisconnectTask = MakeUnique<FSwitchboardDisconnectTask>(FGuid(), Client);
			DisconnectTasks.Enqueue(MoveTemp(DisconnectTask));
		}
	}

	while (!DisconnectTasks.IsEmpty())
	{
		TUniquePtr<FSwitchboardTask> Task;
		DisconnectTasks.Dequeue(Task);
		const FSwitchboardDisconnectTask& DisconnectTask = static_cast<const FSwitchboardDisconnectTask&>(*Task);
		const FIPv4Endpoint& Client = DisconnectTask.Recipient;

		if (RedeployStatus.InProgress() && Client == RedeployStatus.RequestingClient)
		{
			UE_LOG(LogSwitchboard, Warning, TEXT("Client %s disconnected before redeploy completed"), *Client.ToString());
			RollbackRedeploy();
		}

		DisconnectClient(Client);
	}
}

void FSwitchboardListener::DisconnectClient(const FIPv4Endpoint& InClientEndpoint)
{
	const FString Client = InClientEndpoint.ToString();
	UE_LOG(LogSwitchboard, Display, TEXT("Client %s disconnected"), *Client);
	Connections.Remove(InClientEndpoint);
	InactiveTimeouts.Remove(InClientEndpoint);
	LastActivityTime.Remove(InClientEndpoint);
	ReceiveBuffer.Remove(InClientEndpoint);
}

void FSwitchboardListener::HandleStdout(const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& Process)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::HandleStdout);

	TArray<uint8> Output;
	if (FPlatformProcess::ReadPipeToArray(Process->ReadPipe, Output))
	{
		Process->Output.Append(Output);
	}

	// If there was a new stdout, update the clients
	if (Output.Num() && Process->bUpdateClientsWithStdout)
	{
		FSwitchboardProgramStdout Packet;

		Packet.Process.Uuid = Process->UUID.ToString();
		Packet.Process.Name = Process->Name;
		Packet.Process.Path = Process->Path;
		Packet.Process.Caller = Process->Caller;
		Packet.Process.Pid = Process->PID;

		Packet.PartialStdoutB64 = FBase64::Encode(Output);

		for (const TPair<FIPv4Endpoint, TSharedPtr<FSocket>>& Connection : Connections)
		{
			const FIPv4Endpoint& ClientEndpoint = Connection.Key;
			SendMessage(CreateMessage(Packet), ClientEndpoint);
		}
	}
}

void FSwitchboardListener::HandleRunningProcesses(TArray<TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>>& Processes, bool bNotifyThatProgramEnded)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::HandleRunningProcesses);

	// Reads pipe and cleans up dead processes from the array.
	for (auto Iter = Processes.CreateIterator(); Iter; ++Iter)
	{
		TSharedPtr<FRunningProcess, ESPMode::ThreadSafe> Process = *Iter;

		check(Process.IsValid());

		if (Process->bPendingKill)
		{
			continue;
		}

		if (Process->Handle.IsValid())
		{
			HandleStdout(Process);

			if (!FPlatformProcess::IsProcRunning(Process->Handle))
			{
				// A final read can be necessary to avoid truncating the output.
				HandleStdout(Process);

				int32 ReturnCode = 0;
				FPlatformProcess::GetProcReturnCode(Process->Handle, &ReturnCode);
				UE_LOG(LogSwitchboard, Display, TEXT("Process %d (%s) exited with returncode: %d"),
					Process->PID, *Process->Name, ReturnCode);

				if (ReturnCode != 0 && Process->Output.Num() > 0)
				{
					FUTF8ToTCHAR StdoutConv(reinterpret_cast<const ANSICHAR*>(Process->Output.GetData()), Process->Output.Num());
					UE_LOG(LogSwitchboard, Display, TEXT("Output:\n%.*s"), StdoutConv.Length(), StdoutConv.Get());
				}

				// Notify remote client, which implies that this is a program managed by it.
				if (bNotifyThatProgramEnded)
				{
					FSwitchboardProgramEnded Packet;

					Packet.Process.Uuid = Process->UUID.ToString();
					Packet.Process.Name = Process->Name;
					Packet.Process.Path = Process->Path;
					Packet.Process.Caller = Process->Caller;
					Packet.Process.Pid = Process->PID;
					Packet.Returncode = ReturnCode;
					Packet.StdoutB64 = FBase64::Encode(Process->Output);

					for (const TPair<FIPv4Endpoint, TSharedPtr<FSocket>>& Connection : Connections)
					{
						const FIPv4Endpoint& ClientEndpoint = Connection.Key;
						SendMessage(CreateMessage(Packet), ClientEndpoint);
					}

					// Kill its monitor to avoid potential zombies (unless it is already pending kill)
					{
						auto* FlipModeMonitorPtr = FlipModeMonitors.FindByPredicate([&](const TSharedPtr<FRunningProcess, ESPMode::ThreadSafe>& FlipMonitor)
						{
							check(FlipMonitor.IsValid());
							return !FlipMonitor->bPendingKill && (FlipMonitor->UUID == Process->UUID);
						});

						if (FlipModeMonitorPtr)
						{
							check((*FlipModeMonitorPtr).IsValid());

							FSwitchboardKillTask Task(FGuid(), (*FlipModeMonitorPtr)->Recipient, (*FlipModeMonitorPtr)->UUID);
							Task_KillProcess(Task);
						}
					}
				}

				FPlatformProcess::CloseProc(Process->Handle);
				FPlatformProcess::ClosePipe(Process->ReadPipe, Process->WritePipe);

				Iter.RemoveCurrent();
			}
		}
	}
}

bool FSwitchboardListener::OnIncomingConnection(FSocket* InSocket, const FIPv4Endpoint& InEndpoint)
{
	UE_LOG(LogSwitchboard, Display, TEXT("Incoming connection via %s:%d"), *InEndpoint.Address.ToString(), InEndpoint.Port);

	InSocket->SetNoDelay(true);
	PendingConnections.Enqueue(TPair<FIPv4Endpoint, TSharedPtr<FSocket>>(InEndpoint, MakeShareable(InSocket)));

	return true;
}

bool FSwitchboardListener::SendMessage(const FString& InMessage, const FIPv4Endpoint& InEndpoint)
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::SendMessage);

	if (InEndpoint == InvalidEndpoint)
	{
		return false;
	}

	if (Connections.Contains(InEndpoint))
	{
		TSharedPtr<FSocket> ClientSocket = Connections[InEndpoint];
		if (!ClientSocket.IsValid())
		{
			return false;
		}

		UE_LOG(LogSwitchboard, Verbose, TEXT("Sending message %s"), *InMessage);
		int32 BytesSent = 0;
		return ClientSocket->Send((uint8*)TCHAR_TO_UTF8(*InMessage), InMessage.Len() + 1, BytesSent);
	}

	// this happens when a client disconnects while a task it had issued is not finished
	UE_LOG(LogSwitchboard, Verbose, TEXT("Trying to send message to disconnected client %s"), *InEndpoint.ToString());
	return false;
}

void FSwitchboardListener::SendMessageFutures()
{
	SWITCHBOARD_TRACE_CPUPROFILER_EVENT_SCOPE(FSwitchboardListener::SendMessageFutures);

	for (auto Iter = MessagesFutures.CreateIterator(); Iter; ++Iter)
	{
		FSwitchboardMessageFuture& MessageFuture = *Iter;

		if (!MessageFuture.Future.IsReady())
		{
			continue;
		}

		FString Message = MessageFuture.Future.Get();
		if (!Message.IsEmpty())
		{
			SendMessage(Message, MessageFuture.InEndpoint);
		}

		Iter.RemoveCurrent();
	}
}
