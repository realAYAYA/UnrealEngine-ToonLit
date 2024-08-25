// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST
#include "LevelEditor.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UnrealEdGlobals.h"
#include "Tests/AutomationEditorCommon.h"

#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "GameFramework/GameMode.h"
#include "Modules/ModuleManager.h"

#include "GameMapsSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogNetworkTest, Log, All);

FBasePIENetworkComponent::FBasePIENetworkComponent(FAutomationTestBase* InTestRunner, FTestCommandBuilder& InCommandBuilder, bool IsInitializing)
	: TestRunner(InTestRunner)
	, CommandBuilder(&InCommandBuilder)
{
	if (IsInitializing)
	{
		return;
	}

	CommandBuilder
		->Do(TEXT("Stop PIE"), [this]() { StopPie(); })
		.Then(TEXT("Create New Map"), [this]() { FAutomationEditorCommonUtils::CreateNewMap(); })
		.Then(TEXT("Start PIE"), [this]() { StartPie(); })
		.Until(TEXT("Collect PIE Worlds"), [this]() { return CollectPieWorlds(); })
		.Until(TEXT("Await Connections"), [this]() { return AwaitConnections(); })
		.OnTearDown(TEXT("Restore Editor State"), [this]() { RestoreState(); });
}

FBasePIENetworkComponent& FBasePIENetworkComponent::Then(TFunction<void()> Action)
{
	CommandBuilder->Then(Action);
	return *this;
}

FBasePIENetworkComponent& FBasePIENetworkComponent::Do(TFunction<void()> Action)
{
	CommandBuilder->Do(Action);
	return *this;
}

FBasePIENetworkComponent& FBasePIENetworkComponent::Then(const TCHAR* Description, TFunction<void()> Action)
{
	CommandBuilder->Then(Description, Action);
	return *this;
}
FBasePIENetworkComponent& FBasePIENetworkComponent::Do(const TCHAR* Description, TFunction<void()> Action)
{
	CommandBuilder->Do(Description, Action);
	return *this;
}

FBasePIENetworkComponent& FBasePIENetworkComponent::Until(TFunction<bool()> Query, FTimespan Timeout)
{
	CommandBuilder->Until(Query, Timeout);
	return *this;
}
FBasePIENetworkComponent& FBasePIENetworkComponent::Until(TCHAR* Description, TFunction<bool()> Query, FTimespan Timeout)
{
	CommandBuilder->Until(Description, Query, Timeout);
	return *this;
}

FBasePIENetworkComponent& FBasePIENetworkComponent::StartWhen(TFunction<bool()> Query, FTimespan Timeout)
{
	CommandBuilder->StartWhen(Query, Timeout);
	return *this;
}
FBasePIENetworkComponent& FBasePIENetworkComponent::StartWhen(TCHAR* Description, TFunction<bool()> Query, FTimespan Timeout)
{
	CommandBuilder->StartWhen(Description, Query, Timeout);
	return *this;
}

void FBasePIENetworkComponent::StopPie()
{
	if (ServerState == nullptr)
	{
		TestRunner->AddError(TEXT("Failed to initialize Network Component"));
		return;
	}
	GUnrealEd->RequestEndPlayMap();
}

void FBasePIENetworkComponent::StartPie()
{
	ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>();
	if (ServerState->bIsDedicatedServer)
	{
		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_Client);
		PlaySettings->SetPlayNumberOfClients(ServerState->ClientCount);
	}
	else
	{
		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
		PlaySettings->SetPlayNumberOfClients(ServerState->ClientCount + 1); // The listen server counts as a client, so we need to add one more to get a real client as well
	}
	PlaySettings->bLaunchSeparateServer = ServerState->bIsDedicatedServer;
	PlaySettings->GameGetsMouseControl = false;
	PlaySettings->SetRunUnderOneProcess(true);

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	FRequestPlaySessionParams SessionParams;
	SessionParams.WorldType = EPlaySessionWorldType::PlayInEditor;
	SessionParams.DestinationSlateViewport = LevelEditorModule.GetFirstActiveViewport();
	SessionParams.EditorPlaySettings = PlaySettings;
	if (GameMode != nullptr)
	{
		SessionParams.GameModeOverride = GameMode;
	}
	else
	{
		SessionParams.GameModeOverride = AGameModeBase::StaticClass();
	}

	GUnrealEd->RequestPlaySession(SessionParams);
	GUnrealEd->StartQueuedPlaySessionRequest();
}

bool FBasePIENetworkComponent::CollectPieWorlds()
{
	const auto& WorldContexts = GEngine->GetWorldContexts();
	UWorld* ServerWorld = nullptr;
	TArray<UWorld*> ClientWorlds;

	for (const auto& WorldContext : WorldContexts)
	{
		if (WorldContext.WorldType != EWorldType::PIE || WorldContext.World() == nullptr || WorldContext.World()->GetNetDriver() == nullptr)
		{
			continue;
		}
		UWorld* World = WorldContext.World();
		if (World->GetNetDriver()->IsServer())
		{
			ServerWorld = World;
			if (World->GetNetMode() == NM_DedicatedServer && !ServerState->bIsDedicatedServer)
			{
				TestRunner->AddError(TEXT("Failed to set up dedicated server.  Does your game's editor module override the PIE settings?"));

				return true;
			}
			else if (World->GetNetMode() == NM_ListenServer && ServerState->bIsDedicatedServer)
			{
				TestRunner->AddError(TEXT("Failed to set up dedicated server.  Does your game's editor module override the PIE settings?"));
				return true;
			}
		}
		else
		{
			ClientWorlds.Add(World);
		}
	}
	if (ServerWorld == nullptr || ClientWorlds.Num() < ServerState->ClientCount)
	{
		return false;
	}

	if (PacketSimulationSettings)
	{
		ServerWorld->GetNetDriver()->SetPacketSimulationSettings(*PacketSimulationSettings);
		for (auto& World : ClientWorlds)
		{
			World->GetNetDriver()->SetPacketSimulationSettings(*PacketSimulationSettings);
		}
	}

	ServerState->World = ServerWorld;
	for (int32 ClientIndex = 0; ClientIndex < ServerState->ClientCount; ClientIndex++)
	{
		UWorld* ClientWorld = ClientWorlds[ClientIndex];
		ClientStates[ClientIndex]->World = ClientWorld;

		const int32 ClientLocalPort = ClientWorld->GetNetDriver()->GetLocalAddr()->GetPort();
		auto ServerConnection = ServerWorld->GetNetDriver()->ClientConnections.FindByPredicate([ClientLocalPort](UNetConnection* ClientConnection) {
			return ClientConnection->GetRemoteAddr()->GetPort() == ClientLocalPort;
			});
		check(ServerConnection != nullptr);
		ServerState->ClientConnections[ClientIndex] = *ServerConnection;
	}

	return true;
}

bool FBasePIENetworkComponent::AwaitConnections()
{
	if (ServerState == nullptr || ServerState->World == nullptr)
	{
		return true; //Failed to get server state, test will fail
	}
	if (ServerState->World->GetNetDriver()->ClientConnections.Num() != ServerState->ClientCount)
	{
		return false;
	}
	for (UNetConnection* ClientConnection : ServerState->World->GetNetDriver()->ClientConnections)
	{
		if (ClientConnection->ViewTarget == nullptr)
		{
			return false;
		}
	}

	return true;
}

void FBasePIENetworkComponent::RestoreState()
{
	if (ServerState != nullptr)
	{
		GUnrealEd->RequestEndPlayMap();
		StateRestorer.Restore();
	}
}
#endif