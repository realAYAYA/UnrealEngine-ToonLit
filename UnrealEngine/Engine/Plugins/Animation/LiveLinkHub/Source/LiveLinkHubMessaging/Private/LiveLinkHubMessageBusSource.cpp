// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubMessageBusSource.h"

#include "Containers/Ticker.h"
#include "Engine/Level.h"
#include "Engine/SystemTimeTimecodeProvider.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkModule.h"
#include "LiveLinkHubMessages.h"
#include "LiveLinkMessages.h"
#include "LiveLinkTimecodeProvider.h"
#include "LiveLinkTypes.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#endif

#include <limits>

FLiveLinkHubMessageBusSource::FLiveLinkHubMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset)
	: FLiveLinkMessageBusSource(InSourceType, InSourceMachineName, InConnectionAddress, InMachineTimeOffset)
{
#if WITH_EDITOR
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModule->OnMapChanged().AddRaw(this, &FLiveLinkHubMessageBusSource::OnMapChanged);
	}
#endif
}

FLiveLinkHubMessageBusSource::~FLiveLinkHubMessageBusSource()
{
#if WITH_EDITOR
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModule->OnMapChanged().RemoveAll(this);
	}
#endif
}

double FLiveLinkHubMessageBusSource::GetDeadSourceTimeout() const
{
	// Don't remove livelink hub sources that have hit the heartbeat timeout.
	return std::numeric_limits<double>::max();
}

void FLiveLinkHubMessageBusSource::SendConnectMessage()
{
	FLiveLinkHubConnectMessage* ConnectMessage = FMessageEndpoint::MakeMessage<FLiveLinkHubConnectMessage>();
	ConnectMessage->ClientInfo = CreateLiveLinkClientInfo();

	SendMessage(ConnectMessage);
	StartHeartbeatEmitter();
	bIsValid = true;
}

void FLiveLinkHubMessageBusSource::SendClientInfoMessage()
{
	SendMessage(FMessageEndpoint::MakeMessage<FLiveLinkClientInfoMessage>(CreateLiveLinkClientInfo()));
}

void FLiveLinkHubMessageBusSource::OnMapChanged(UWorld* World, EMapChangeType ChangeType)
{
	SendClientInfoMessage();
}

void FLiveLinkHubMessageBusSource::InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder)
{
	FLiveLinkMessageBusSource::InitializeMessageEndpoint(EndpointBuilder);
	EndpointBuilder
		.Handling<FLiveLinkHubTimecodeSettings>(this, &FLiveLinkHubMessageBusSource::HandleTimecodeSettings);
}

void FLiveLinkHubMessageBusSource::HandleTimecodeSettings(const FLiveLinkHubTimecodeSettings& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	ExecuteOnGameThread(UE_SOURCE_LOCATION, [Message]() {
		Message.AssignTimecodeSettingsAsProviderToEngine();
	});
}

FLiveLinkClientInfoMessage FLiveLinkHubMessageBusSource::CreateLiveLinkClientInfo() const
{
	FLiveLinkClientInfoMessage ClientInfo;

	FString CurrentLevelName;
	if (GWorld && GWorld->GetCurrentLevel())
	{
		CurrentLevelName = GWorld->GetName();
	}

	// todo: Distinguish between UE and UEFN.
	ClientInfo.LongName = FString::Printf(TEXT("%s - %s %s"), TEXT("UE"), *FEngineVersion::Current().ToString(EVersionComponent::Patch), FPlatformProcess::ComputerName());
	ClientInfo.Status = ELiveLinkClientStatus::Connected;
	ClientInfo.Hostname = FPlatformProcess::ComputerName();
	ClientInfo.ProjectName = FApp::GetProjectName();
	ClientInfo.CurrentLevel = CurrentLevelName;
	ClientInfo.LiveLinkVersion = ILiveLinkClient::LIVELINK_VERSION;

	return ClientInfo;
}
