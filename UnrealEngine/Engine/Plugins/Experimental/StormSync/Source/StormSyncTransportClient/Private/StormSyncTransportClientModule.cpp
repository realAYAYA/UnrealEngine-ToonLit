// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncTransportClientModule.h"

#include "StormSyncCommandLineUtils.h"
#include "StormSyncCoreUtils.h"
#include "IStormSyncTransportLocalEndpoint.h"
#include "MessageEndpoint.h"
#include "Misc/CoreDelegates.h"
#include "StormSyncTransportClientEndpoint.h"
#include "StormSyncTransportClientLog.h"
#include "StormSyncTransportMessages.h"

#define LOCTEXT_NAMESPACE "StormSyncTransportClientModule"

void FStormSyncTransportClientModule::StartupModule()
{
	ClientEndpoint = CreateClientLocalEndpoint(TEXT("Client"));

	if (GIsEditor && !IsRunningCommandlet())
	{
		RegisterConsoleCommands();
	}
	
	// Register for engine initialization completed so we can broadcast presence over the network from this client to other clients
	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FStormSyncTransportClientModule::OnEngineLoopInitComplete);
}

void FStormSyncTransportClientModule::ShutdownModule()
{
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	
	if (ClientEndpoint.IsValid())
	{
		ClientEndpoint.Reset();
	}

	UnregisterConsoleCommands();
}

TSharedPtr<IStormSyncTransportClientLocalEndpoint> FStormSyncTransportClientModule::CreateClientLocalEndpoint(const FString& InEndpointFriendlyName) const
{
	TSharedPtr<FStormSyncTransportClientEndpoint, ESPMode::ThreadSafe> Endpoint = MakeShared<FStormSyncTransportClientEndpoint>();
	check(Endpoint.IsValid());
	Endpoint->InitializeMessaging(TEXT("Client"));
	return Endpoint;
}

FString FStormSyncTransportClientModule::GetClientEndpointMessageAddressId() const
{
	return ClientEndpoint.IsValid() && ClientEndpoint->IsRunning() ? ClientEndpoint->GetMessageEndpoint()->GetAddress().ToString() : TEXT("");
}

FMessageEndpointSharedPtr FStormSyncTransportClientModule::GetClientMessageEndpoint() const
{
	if (!ClientEndpoint.IsValid())
	{
		UE_LOG(LogStormSyncClient, Warning, TEXT("FStormSyncTransportClientModule::GetClientMessageEndpoint - Client endpoint not valid"));
		return nullptr;
	}

	if (!ClientEndpoint->IsRunning())
	{
		UE_LOG(LogStormSyncClient, Warning, TEXT("FStormSyncTransportClientModule::GetClientMessageEndpoint - Client endpoint not running"));
		return nullptr;
	}

	return ClientEndpoint->GetMessageEndpoint();
}

void FStormSyncTransportClientModule::SynchronizePackages(const FStormSyncPackageDescriptor& InPackageDescriptor, const TArray<FName>& InPackageNames) const
{
	const FMessageEndpointSharedPtr MessageEndpoint = GetClientMessageEndpoint();

	TArray<FName> LocalPackageNames = InPackageNames;
	FStormSyncPackageDescriptor LocalPackageDescriptor = InPackageDescriptor;

	FStormSyncCoreUtils::GetAvaFileDependenciesAsync(InPackageNames)
	.Then([MessageEndpoint, PackageNames = MoveTemp(LocalPackageNames), PackageDescriptor = MoveTemp(LocalPackageDescriptor)](const TFuture<TArray<FStormSyncFileDependency>> Result)
	{
		const TArray<FStormSyncFileDependency> FileDependencies = Result.Get();
		if (FileDependencies.IsEmpty())
		{
			UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncTransportClientModule::SynchronizePackages - Async FileDependencies is empty, something went wrong"));
			return;
		}

		if (!MessageEndpoint.IsValid())
		{
			UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncTransportClientModule::SynchronizePackages - Unable to get client message endpoint"));
			return;
		}

		// Build sync request message that is going to be sent over the network for a specific recipient
		TUniquePtr<FStormSyncTransportSyncRequest> SyncRequestMessage(FMessageEndpoint::MakeMessage<FStormSyncTransportSyncRequest>(PackageNames, PackageDescriptor));
		if (!SyncRequestMessage.IsValid())
		{
			UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncTransportClientModule::SynchronizePackages - Push request message is invalid"));
			return;
		}

		SyncRequestMessage->PackageDescriptor.Dependencies = FileDependencies;

		UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::SynchronizePackages - FileDependencies: %d"), FileDependencies.Num());
		UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::SynchronizePackages - Message: %s"), *SyncRequestMessage->ToString());
		UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::SynchronizePackages - Syncing package descriptor %s"), *SyncRequestMessage->PackageDescriptor.ToString());
		MessageEndpoint->Publish(SyncRequestMessage.Release());
	});
}

void FStormSyncTransportClientModule::PushPackages(const FStormSyncPackageDescriptor& InPackageDescriptor, const TArray<FName>& InPackageNames, const FMessageAddress& InMessageAddress, const FOnStormSyncPushComplete& InDoneDelegate) const
{
	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::PushPackages - PackageDescriptor: %s, InPackageNames: %d, MessageAddressId: %s"), *InPackageDescriptor.ToString(), InPackageNames.Num(), *InMessageAddress.ToString());

	const FMessageEndpointSharedPtr MessageEndpoint = GetClientMessageEndpoint();
	if (!ClientEndpoint.IsValid())
	{
		UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncTransportClientModule::PushPackages - Unable to get client endpoint"));
		return;
	}

	ClientEndpoint->RequestPushPackages(InMessageAddress, InPackageDescriptor, InPackageNames, InDoneDelegate);
}

void FStormSyncTransportClientModule::PullPackages(const FStormSyncPackageDescriptor& InPackageDescriptor, const TArray<FName>& InPackageNames, const FMessageAddress& InMessageAddress, const FOnStormSyncPullComplete& InDoneDelegate) const
{
	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::PullPackages - PackageDescriptor: %s, InPackageNames: %d, MessageAddressId: %s"), *InPackageDescriptor.ToString(), InPackageNames.Num(), *InMessageAddress.ToString());

	const FMessageEndpointSharedPtr MessageEndpoint = GetClientMessageEndpoint();

	if (!ClientEndpoint.IsValid())
	{
		UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncTransportClientModule::PullPackages - Unable to get client endpoint"));
		return;
	}

	ClientEndpoint->RequestPullPackages(InMessageAddress, InPackageDescriptor, InPackageNames, InDoneDelegate);
}

void FStormSyncTransportClientModule::RequestPackagesStatus(const FMessageAddress& InRemoteAddress, const TArray<FName>& InPackageNames, const FOnStormSyncRequestStatusComplete& InDoneDelegate) const
{
	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::RequestPackagesStatus - InRemoteAddress: %s, InPackageNames: %d"), *InRemoteAddress.ToString(), InPackageNames.Num());

	const FMessageEndpointSharedPtr MessageEndpoint = GetClientMessageEndpoint();
	if (!ClientEndpoint.IsValid())
	{
		UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncTransportClientModule::RequestPackagesStatus - Unable to get client endpoint"));
		return;
	}

	ClientEndpoint->RequestStatus(InRemoteAddress, InPackageNames, InDoneDelegate);
}

void FStormSyncTransportClientModule::RegisterConsoleCommands()
{
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Client.Ping"),
		TEXT("Sends a ping message on Storm Sync message bus"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FStormSyncTransportClientModule::ExecutePing),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Client.SyncPak"),
		TEXT("Synchronize a pak file on Storm Sync message bus. Usage: <PackageName>... [-name=<AvaPackageName> -version=<AvaPackageVersion>] [-description=<AvaPackageVersion>] [-author=<AvaPackageAuthor>]"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FStormSyncTransportClientModule::ExecuteSyncPak),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Client.Debug"),
		TEXT("Prints out client address enpoint id to the console"),
		// FConsoleCommandWithArgsDelegate::CreateRaw(this, &FStormSyncTransportClientModule::ExecuteSyncPak),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
		{
			const FString AddressId = GetClientEndpointMessageAddressId();
			UE_LOG(LogStormSyncClient, Display, TEXT("StormSync.Client.Debug - EndpointId: %s"), *AddressId);
		}),
		ECVF_Default
	));

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Client.Debug.Ping"),
		TEXT("Sends status ping"),
		// FConsoleCommandWithArgsDelegate::CreateRaw(this, &FStormSyncTransportClientModule::ExecuteSyncPak),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
		{
			if (ClientEndpoint.IsValid())
			{
				const TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint = ClientEndpoint->GetMessageEndpoint();
				if (!MessageEndpoint.IsValid())
				{
					UE_LOG(LogStormSyncClient, Error, TEXT("StormSync.Client.Debug.Ping - Unable to send Connect Message cause Message Endpoint is invalid"));
					return;
				}

				UE_LOG(LogStormSyncClient, Display, TEXT("StormSync.Client.Debug.Ping - Publish Client Connect Message ..."));
				FStormSyncTransportStatusPing* Message = FMessageEndpoint::MakeMessage<FStormSyncTransportStatusPing>();
				MessageEndpoint->Publish(Message);
			}
		}),
		ECVF_Default
	));
	
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSync.Client.InstanceId"),
		TEXT("Prints out local app instance id"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
		{
			const FString InstanceId = FApp::GetInstanceId().ToString();
			UE_LOG(LogStormSyncClient, Display, TEXT("StormSync.Client.Debug - InstanceId: %s"), *InstanceId);
		}),
		ECVF_Default
	));
}

void FStormSyncTransportClientModule::UnregisterConsoleCommands()
{
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}

	ConsoleCommands.Empty();
}

void FStormSyncTransportClientModule::OnEngineLoopInitComplete()
{
	if (ClientEndpoint.IsValid())
	{
		const TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint = ClientEndpoint->GetMessageEndpoint();
		if (!MessageEndpoint.IsValid())
		{
			UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncTransportClientModule::OnEngineLoopInitComplete - Unable to send Connect Message cause Message Endpoint is invalid"));
			return;
		}

		// We broadcast a message to notify others about this editor instance (this is required so that further "direct" send are received on the other end)
		UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::OnEngineLoopInitComplete - Publish Client Connect Message ..."));
		FStormSyncTransportStatusPing* Message = FMessageEndpoint::MakeMessage<FStormSyncTransportStatusPing>();
		MessageEndpoint->Publish(Message);
	}
}

void FStormSyncTransportClientModule::ExecutePing(const TArray<FString>& Args)
{
	const FString Argv = FString::Join(Args, TEXT(""));
	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::ExecutePing - %s"), *Argv);

	const FMessageEndpointSharedPtr MessageEndpoint = GetClientMessageEndpoint();
	if (!MessageEndpoint.IsValid())
	{
		UE_LOG(LogStormSyncClient, Warning, TEXT("FStormSyncTransportClientModule::ExecutePing - Unable to get client message endpoint"));
		return;
	}

	MessageEndpoint->Publish(FMessageEndpoint::MakeMessage<FStormSyncTransportPingMessage>());
}

FStormSyncPackageDescriptor FStormSyncTransportClientModule::CreatePackageDescriptorFromCommandLine(const FString& Argv)
{
	FStormSyncPackageDescriptor PackageDescriptor;
	if (!FParse::Value(*Argv, TEXT("-name="), PackageDescriptor.Name))
	{
		PackageDescriptor.Name = DefaultPakName;
		UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncEditorModule::CreatePackageDescriptorFromCommandLine - Missing -name parameter, using default \"%s\""), *PackageDescriptor.Name);
	}

	FParse::Value(*Argv, TEXT("-version="), PackageDescriptor.Version);
	FParse::Value(*Argv, TEXT("-description="), PackageDescriptor.Description);
	FParse::Value(*Argv, TEXT("-author="), PackageDescriptor.Author);

	return PackageDescriptor;
}

void FStormSyncTransportClientModule::ExecuteSyncPak(const TArray<FString>& Args)
{
	const FString Argv = FString::Join(Args, TEXT(" "));
	UE_LOG(LogStormSyncClient, Display, TEXT("FStormSyncTransportClientModule::ExecuteSyncPak - Argv: %s"), *Argv);

	// Parse command line.
	TArray<FName> PackageNames;
	FStormSyncCommandLineUtils::Parse(*Argv, PackageNames);

	if (PackageNames.IsEmpty())
	{
		UE_LOG(LogStormSyncClient, Error, TEXT("FStormSyncTransportClientModule::ExecuteSyncPak - Missing at least one package name to sync."));
		return;
	}

	// Create package descriptor pulling info from command line options
	const FStormSyncPackageDescriptor PackageDescriptor = CreatePackageDescriptorFromCommandLine(Argv);

	// Sync over network now
	SynchronizePackages(PackageDescriptor, PackageNames);
}

IMPLEMENT_MODULE(FStormSyncTransportClientModule, StormSyncTransportClient)

#undef LOCTEXT_NAMESPACE
