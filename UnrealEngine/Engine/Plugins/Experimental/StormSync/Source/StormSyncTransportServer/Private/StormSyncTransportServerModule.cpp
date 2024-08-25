// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncTransportServerModule.h"

#include "IStormSyncTransportLocalEndpoint.h"
#include "MessageEndpoint.h"
#include "Misc/CoreDelegates.h"
#include "ServiceDiscovery/StormSyncDiscoveryManager.h"
#include "ServiceDiscovery/StormSyncHeartbeatEmitter.h"
#include "StormSyncCoreDelegates.h"
#include "StormSyncTransportServerEndpoint.h"
#include "StormSyncTransportServerLog.h"
#include "StormSyncTransportSettings.h"
#include "Utils/StormSyncTransportCommandUtils.h"

#define LOCTEXT_NAMESPACE "StormSyncTransportServerModule"

void FStormSyncTransportServerModule::StartupModule()
{
	const UStormSyncTransportSettings* Settings = GetDefault<UStormSyncTransportSettings>();

	HeartbeatEmitter = MakeUnique<FStormSyncHeartbeatEmitter>();
	DiscoveryManager = MakeUnique<FStormSyncDiscoveryManager>(
		Settings->GetMessageBusHeartbeatTimeout(),
		Settings->GetMessageBusTimeBeforeRemovingInactiveSource(),
		Settings->GetDiscoveryManagerTickInterval(),
		Settings->IsDiscoveryPeriodicPublishEnabled()
	);

	// Create server endpoint and start tcp listening if configured to do so
	ServerEndpoint = CreateServerLocalEndpoint(TEXT("Server"));

	// Auto-start is disabled if running commandlet
	const bool bIsCommandLineAutoStartDisabled = IsRunningCommandlet() || UE::StormSync::Transport::Private::IsServerAutoStartDisabled();

	if (!bIsCommandLineAutoStartDisabled && Settings->IsAutoStartServer())
	{
		if (ServerEndpoint->StartTcpListener() && IsRunning())
		{
			FStormSyncCoreDelegates::OnStormSyncServerStarted.Broadcast();
		}
	}

	if (GIsEditor && !IsRunningCommandlet())
	{
		RegisterConsoleCommands();
	}

	// Register for engine initialization completed so we can broadcast presence over the network and start heartbeats
	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FStormSyncTransportServerModule::OnEngineLoopInitComplete);
}

void FStormSyncTransportServerModule::ShutdownModule()
{
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

	if (ServerEndpoint.IsValid())
	{
		ServerEndpoint.Reset();
		FStormSyncCoreDelegates::OnStormSyncServerStopped.Broadcast();
	}

	if (HeartbeatEmitter.IsValid())
	{
		HeartbeatEmitter->Exit();
	}

	if (DiscoveryManager.IsValid())
	{
		DiscoveryManager->Stop();
	}

	UnregisterConsoleCommands();
}

TSharedPtr<IStormSyncTransportServerLocalEndpoint> FStormSyncTransportServerModule::CreateServerLocalEndpoint(const FString& InEndpointFriendlyName) const
{
	TSharedPtr<FStormSyncTransportServerEndpoint, ESPMode::ThreadSafe> Endpoint = MakeShared<FStormSyncTransportServerEndpoint>();
	check(Endpoint.IsValid());
	Endpoint->InitializeMessaging(InEndpointFriendlyName);
	return Endpoint;
}

FString FStormSyncTransportServerModule::GetServerEndpointMessageAddressId() const
{
	return ServerEndpoint.IsValid() && ServerEndpoint->IsRunning() ? ServerEndpoint->GetMessageEndpoint()->GetAddress().ToString() : TEXT("");
}

FString FStormSyncTransportServerModule::GetDiscoveryManagerMessageAddressId() const
{
	return DiscoveryManager.IsValid() ? DiscoveryManager->GetMessageEndpoint()->GetAddress().ToString() : TEXT("");
}

FStormSyncHeartbeatEmitter& FStormSyncTransportServerModule::GetHeartbeatEmitter() const
{
	return *HeartbeatEmitter;
}

bool FStormSyncTransportServerModule::IsRunning() const
{
	return ServerEndpoint.IsValid() && ServerEndpoint->IsRunning() && ServerEndpoint->IsTcpServerActive();
}

bool FStormSyncTransportServerModule::GetServerStatus(FText& StatusText) const
{
	if (!ServerEndpoint.IsValid())
	{
		StatusText = LOCTEXT("ServerStatusEndpointInvalid", "Server is not active.");
		return false;
	}

	const bool bIsRunning = ServerEndpoint->IsRunning() && ServerEndpoint->IsTcpServerActive();

	if (bIsRunning)
	{
		StatusText = FText::Format(
			LOCTEXT("ServerStatusEndpointRunning", "Server is currently running and listening for incoming connections on {0}"),
			FText::FromString(ServerEndpoint->GetTcpServerEndpointAddress())
		);
	}
	else
	{
		StatusText = LOCTEXT("ServerStatusEndpointNotRunning", "Server is not running.");
	}
	
	return bIsRunning;
}

void FStormSyncTransportServerModule::RegisterConsoleCommands()
{
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Server.Start"),
		TEXT("Starts Storm Sync Server"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FStormSyncTransportServerModule::ExecuteStartServer),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Server.Stop"),
		TEXT("Stops Storm Sync Server"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FStormSyncTransportServerModule::ExecuteStopServer),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Server.Status"),
		TEXT("Prints Storm Sync Server status"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FStormSyncTransportServerModule::ExecuteServerStatus),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Server.Debug"),
		TEXT("Prints out server address enpoint id to the console"),
		// FConsoleCommandWithArgsDelegate::CreateRaw(this, &FStormSyncTransportClientModule::ExecuteSyncPak),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
		{
			const FString AddressId = GetServerEndpointMessageAddressId();
			UE_LOG(LogStormSyncServer, Display, TEXT("StormSync.Server.Debug - EndpointId: %s"), *AddressId);
		}),
		ECVF_Default
	));
}

void FStormSyncTransportServerModule::UnregisterConsoleCommands()
{
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}

	ConsoleCommands.Empty();
}

void FStormSyncTransportServerModule::OnEngineLoopInitComplete()
{
	UE_LOG(LogStormSyncServer, Verbose, TEXT("FStormSyncTransportServerModule::OnEngineLoopInitComplete - Publish ping messages for discover manager and server endpoint ..."));
	
	// We broadcast a message to notify others about this editor instance (this is required so that further "direct" send are received on the other end)

	// For service discovery
	PublishConnectMessage();

	// For server endpoint
	if (ServerEndpoint.IsValid())
	{
		const TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint = ServerEndpoint->GetMessageEndpoint();
		TUniquePtr<FStormSyncTransportPingMessage> Message(FMessageEndpoint::MakeMessage<FStormSyncTransportPingMessage>());
		if (MessageEndpoint.IsValid() && Message.IsValid())
		{
			MessageEndpoint->Publish(Message.Release());
		}
	}
}

void FStormSyncTransportServerModule::PublishConnectMessage() const
{
	if (!DiscoveryManager.IsValid())
	{
		UE_LOG(LogStormSyncServer, Error, TEXT("FStormSyncTransportServerModule::PublishConnectMessage - Unable to send connect message cause DiscoveryManager is invalid"));
		return;
	}

	DiscoveryManager->PublishConnectMessage();
}

void FStormSyncTransportServerModule::ExecuteStartServer(const TArray<FString>& Args)
{
	if (!ServerEndpoint.IsValid())
	{
		ServerEndpoint = CreateServerLocalEndpoint(TEXT("Server"));
	}

	check(ServerEndpoint.IsValid());
	if (IsRunning())
	{
		UE_LOG(LogStormSyncServer, Warning, TEXT("FStormSyncTransportServerModule::ExecuteStartServer - Server endpoint TCP listener already running"));
		return;
	}
	
	if (ServerEndpoint->StartTcpListener() && IsRunning())
	{
		FStormSyncCoreDelegates::OnStormSyncServerStarted.Broadcast();
	}
}

void FStormSyncTransportServerModule::ExecuteStopServer(const TArray<FString>& Args)
{
	if (ServerEndpoint.IsValid())
	{
		ServerEndpoint.Reset();
		FStormSyncCoreDelegates::OnStormSyncServerStopped.Broadcast();
	}
	else
	{
		UE_LOG(LogStormSyncServer, Warning, TEXT("FStormSyncTransportServerModule::ExecuteStopServer - Server endpoint already inactive"));
	}
}

void FStormSyncTransportServerModule::ExecuteServerStatus(const TArray<FString>& Args) const
{
	FText StatusText;
	GetServerStatus(StatusText);

	UE_LOG(LogStormSyncServer, Display, TEXT("FStormSyncTransportServerModule::ExecuteServerStatus - %s"), *StatusText.ToString());
}

IMPLEMENT_MODULE(FStormSyncTransportServerModule, StormSyncTransportServer)

#undef LOCTEXT_NAMESPACE
