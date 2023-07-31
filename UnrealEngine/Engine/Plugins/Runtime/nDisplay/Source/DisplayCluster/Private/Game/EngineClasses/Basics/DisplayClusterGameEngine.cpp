// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterGameEngine.h"

#include "Algo/Accumulate.h"
#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Controller/IDisplayClusterClusterNodeController.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "DisplayClusterEnums.h"
#include "Engine/DynamicBlueprintBinding.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationVersion.h"
#include "IDisplayClusterConfiguration.h"

#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DisplayClusterAppExit.h"
#include "Misc/Parse.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

#include "Stats/Stats.h"

#include "GameDelegates.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"


namespace DisplayClusterGameEngineUtils
{
	static const FString WaitForGameCategory = DisplayClusterResetSyncType;
	static const FString WaitForGameName     = TEXT("WaitForGameStart");
}

// Advanced cluster synchronization during LoadMap
static TAutoConsoleVariable<int32> CVarGameStartBarrierAvoidance(
	TEXT("nDisplay.game.GameStartBarrierAvoidance"),
	1,
	TEXT("Avoid entering GameStartBarrier on loading level\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n")
);

void UDisplayClusterGameEngine::Init(class IEngineLoop* InEngineLoop)
{
	UE_LOG(LogDisplayClusterEngine, Log, TEXT("UDisplayClusterGameEngine::Init"));

	// Detect requested operation mode
	OperationMode = DetectOperationMode();

	// Initialize Display Cluster
	if (!GDisplayCluster->Init(OperationMode))
	{
		FDisplayClusterAppExit::ExitApplication(FString("Couldn't initialize DisplayCluster module"), FDisplayClusterAppExit::EExitType::KillImmediately);
	}

	// This is required to prevent GameThread dead-locking when Alt+F4 is used
	// to terminate p-node of a 2+ nodes cluster. This gets called before
	// virtual PreExit which allows to unlock GameThread in advance.
	FCoreDelegates::ApplicationWillTerminateDelegate.AddLambda([this]()
	{
		PreExitImpl();
	});

	if (OperationMode == EDisplayClusterOperationMode::Cluster)
	{
		// Our parsing function for arguments like:
		// -ArgName1="ArgValue 1" -ArgName2=ArgValue2 ArgName3=ArgValue3
		//
		auto ParseCommandArg = [](const FString& CommandLine, const FString& ArgName, FString& OutArgVal)
		{
			const FString Tag = FString::Printf(TEXT("-%s="), *ArgName);
			const int32 TagPos = CommandLine.Find(Tag);

			if (TagPos == INDEX_NONE)
			{
				// Try old method, where the '-' prefix is missing and quoted values with spaces are not supported.
				return FParse::Value(*CommandLine, *ArgName, OutArgVal);
			}

			const TCHAR* TagValue = &CommandLine[TagPos + Tag.Len()];

			if (*TagValue == TEXT('"'))
			{
				return FParse::QuotedString(TagValue, OutArgVal);
			}

			return FParse::Token(TagValue, OutArgVal, false);
		};

		// Extract config path from command line
		FString ConfigPath;
		if (!ParseCommandArg(FCommandLine::Get(), DisplayClusterStrings::args::Config, ConfigPath))
		{
			FDisplayClusterAppExit::ExitApplication(FString("No config file specified. Cluster operation mode requires config file."), FDisplayClusterAppExit::EExitType::KillImmediately);
		}

		// Clean the file path before using it
		DisplayClusterHelpers::str::TrimStringValue(ConfigPath);

		// Validate the config file first. Since 4.27, we don't allow the old formats to be used
		const EDisplayClusterConfigurationVersion ConfigVersion = IDisplayClusterConfiguration::Get().GetConfigVersion(ConfigPath);
		if (!ValidateConfigFile(ConfigPath))
		{
			FDisplayClusterAppExit::ExitApplication(FString("An invalid or outdated configuration file was specified. Please consider using nDisplay configurator to update the config files."), FDisplayClusterAppExit::EExitType::KillImmediately);
		}

		// Load config data
		UDisplayClusterConfigurationData* ConfigData = IDisplayClusterConfiguration::Get().LoadConfig(ConfigPath);
		if (!ConfigData)
		{
			FDisplayClusterAppExit::ExitApplication(FString("An error occurred during loading the configuration file"), FDisplayClusterAppExit::EExitType::KillImmediately);
		}

		// Extract node ID from command line
		FString NodeId;
		if (!ParseCommandArg(FCommandLine::Get(), DisplayClusterStrings::args::Node, NodeId))
		{
			UE_LOG(LogDisplayClusterEngine, Log, TEXT("Node ID is not specified. Trying to resolve from host address..."));

			// Find node ID based on the host address
			if (!GetResolvedNodeId(ConfigData, NodeId))
			{
				FDisplayClusterAppExit::ExitApplication(FString("Couldn't resolve node ID. Try to specify host addresses explicitly."), FDisplayClusterAppExit::EExitType::KillImmediately);
			}

			UE_LOG(LogDisplayClusterEngine, Log, TEXT("Node ID has been successfully resolved: %s"), *NodeId);
		}

		// Clean node ID string
		DisplayClusterHelpers::str::TrimStringValue(NodeId);

		// Start game session
		if (!GDisplayCluster->StartSession(ConfigData, NodeId))
		{
			FDisplayClusterAppExit::ExitApplication(FString("Couldn't start DisplayCluster session"), FDisplayClusterAppExit::EExitType::KillImmediately);
		}

		// Initialize internals
		InitializeInternals();
	}

	// Initialize base stuff.
	UGameEngine::Init(InEngineLoop);
}

EDisplayClusterOperationMode UDisplayClusterGameEngine::DetectOperationMode() const
{
	EDisplayClusterOperationMode OpMode = EDisplayClusterOperationMode::Disabled;
	if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::Cluster))
	{
		OpMode = EDisplayClusterOperationMode::Cluster;
	}

	UE_LOG(LogDisplayClusterEngine, Log, TEXT("Detected operation mode: %s"), *DisplayClusterTypesConverter::template ToString(OpMode));

	return OpMode;
}

bool UDisplayClusterGameEngine::InitializeInternals()
{
	// This function is called after a session had been started so it's safe to get config data from the config manager
	const UDisplayClusterConfigurationData* Config = GDisplayCluster->GetPrivateConfigMgr()->GetConfig();
	check(Config);

	// Store diagnostics settings locally
	Diagnostics = Config->Diagnostics;

	ClusterMgr     = GDisplayCluster->GetPrivateClusterMgr();
	checkSlow(ClusterMgr);

	FOnClusterEventJsonListener GameSyncTransition = FOnClusterEventJsonListener::CreateUObject(this, &UDisplayClusterGameEngine::GameSyncChange);
	ClusterMgr->AddClusterEventJsonListener(GameSyncTransition);

	const UDisplayClusterConfigurationClusterNode* CfgLocalNode = GDisplayCluster->GetPrivateConfigMgr()->GetLocalNode();
	const bool bSoundEnabled = (CfgLocalNode ? CfgLocalNode->bIsSoundEnabled : false);
	UE_LOG(LogDisplayClusterEngine, Log, TEXT("Configuring sound enabled: %s"), *DisplayClusterTypesConverter::template ToString(bSoundEnabled));
	if (!bSoundEnabled)
	{
		AudioDeviceManager = nullptr;
	}

	return true;
}

// This function works if you have 1 cluster node per PC. In case of multiple nodes, all of them will have the same node ID.
bool UDisplayClusterGameEngine::GetResolvedNodeId(const UDisplayClusterConfigurationData* ConfigData, FString& NodeId) const
{
	TArray<TSharedPtr<FInternetAddr>> LocalAddresses;
	if (!ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalAdapterAddresses(LocalAddresses))
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't get local addresses list. Cannot find node ID by its address."));
		return false;
	}

	if (LocalAddresses.Num() < 1)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("No local addresses found"));
		return false;
	}

	for (const auto& it : ConfigData->Cluster->Nodes)
	{
		for (const auto& LocalAddress : LocalAddresses)
		{
			const FIPv4Endpoint ep(LocalAddress);
			const FString epaddr = ep.Address.ToString();

			UE_LOG(LogDisplayClusterCluster, Log, TEXT("Comparing addresses: %s - %s"), *epaddr, *it.Value->Host);

			//@note: don't add "127.0.0.1" or "localhost" here. There will be a bug. It has been proved already.
			if (epaddr.Equals(it.Value->Host, ESearchCase::IgnoreCase))
			{
				// Found!
				NodeId = it.Key;
				return true;
			}
		}
	}

	// We haven't found anything
	return false;
}

bool UDisplayClusterGameEngine::ValidateConfigFile(const FString& FilePath)
{
	const EDisplayClusterConfigurationVersion ConfigVersion = IDisplayClusterConfiguration::Get().GetConfigVersion(FilePath);
	switch (ConfigVersion)
	{
		case EDisplayClusterConfigurationVersion::Version_426:
			// Old 4.26 and 4.27p1 formats are not allowed as well
			UE_LOG(LogDisplayClusterEngine, Error, TEXT("Detected old (.ndisplay 4.26 or .ndisplay 4.27p1) config format. Please upgrade to the actual version."));
			return true;

		case EDisplayClusterConfigurationVersion::Version_427:
			// Ok, it's one of the actual config formats
			UE_LOG(LogDisplayClusterEngine, Log, TEXT("Detected (.ndisplay 4.27) config format"));
			return true;

		case EDisplayClusterConfigurationVersion::Version_500:
			// Ok, it's one of the actual config formats
			UE_LOG(LogDisplayClusterEngine, Log, TEXT("Detected (.ndisplay 5.00) config format"));
			return true;

		case EDisplayClusterConfigurationVersion::Unknown:
		default:
			// Something unexpected came here
			UE_LOG(LogDisplayClusterEngine, Error, TEXT("Unknown or unsupported config format"));
			break;
	}

	return false;
}

void UDisplayClusterGameEngine::PreExit()
{
	PreExitImpl();

	// Release the engine
	UGameEngine::PreExit();
}

void UDisplayClusterGameEngine::PreExitImpl()
{
	UE_LOG(LogDisplayClusterEngine, Log, TEXT("UDisplayClusterGameEngine::PreExitImpl"));

	if (OperationMode == EDisplayClusterOperationMode::Cluster)
	{
		// Finalize current world
		GDisplayCluster->EndScene();
		// Close current DisplayCluster session
		GDisplayCluster->EndSession();
	}
}

bool UDisplayClusterGameEngine::LoadMap(FWorldContext& WorldContext, FURL URL, class UPendingNetGame* Pending, FString& Error)
{
	UE_LOG(LogDisplayClusterEngine, Log, TEXT("UDisplayClusterGameEngine::LoadMap, URL=%s"), *URL.ToString());

	if (OperationMode == EDisplayClusterOperationMode::Cluster)
	{
		// Finish previous scene
		GDisplayCluster->EndScene();

		// Perform map loading
		if (!Super::LoadMap(WorldContext, URL, Pending, Error))
		{
			return false;
		}

		// Start new scene
		GDisplayCluster->StartScene(WorldContext.World());
		WorldContextObject = WorldContext.World();

		UGameplayStatics::SetGamePaused(WorldContextObject, BarrierAvoidanceOn());

		if(BarrierAvoidanceOn() && RunningMode != EDisplayClusterRunningMode::Startup)
		{
			FDisplayClusterClusterEventJson WaitForGameEvent;
			WaitForGameEvent.Category = DisplayClusterGameEngineUtils::WaitForGameCategory;
			WaitForGameEvent.Type = URL.ToString();
			WaitForGameEvent.Name = ClusterMgr->GetClusterNodeController()->GetNodeId();
			WaitForGameEvent.bIsSystemEvent = true;
			WaitForGameEvent.bShouldDiscardOnRepeat = false;
			ClusterMgr->EmitClusterEventJson(WaitForGameEvent, false);

			RunningMode = EDisplayClusterRunningMode::WaitingForSync;
			// Assume that all nodes are now out of sync.
			UE_LOG(LogDisplayClusterEngine, Display, TEXT("LoadMap occurred after startup for Level %s"), *WaitForGameEvent.Type);
		}
		else
		{
			CheckGameStartBarrier();
		}
	}
	else
	{
		return Super::LoadMap(WorldContext, URL, Pending, Error);
	}

	return true;
}

void UDisplayClusterGameEngine::Tick(float DeltaSeconds, bool bIdleMode)
{
	UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("UDisplayClusterGameEngine::Tick, Delta=%f, Idle=%d"), DeltaSeconds, bIdleMode ? 1 : 0);

	if (CanTick())
	{
		//////////////////////////////////////////////////////////////////////////////////////////////
		// Frame start barrier
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Sync frame start"));
		ClusterMgr->GetClusterNodeController()->WaitForFrameStart();

		// Perform StartFrame notification
		GDisplayCluster->StartFrame(GFrameCounter);

		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("DisplayCluster delta seconds: %f"), DeltaSeconds);

		// Perform PreTick for DisplayCluster module
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Perform PreTick()"));
		GDisplayCluster->PreTick(DeltaSeconds);

		// Perform UGameEngine::Tick() calls for scene actors
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Perform UGameEngine::Tick()"));
		Super::Tick(DeltaSeconds, bIdleMode);

		// Perform PostTick for DisplayCluster module
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Perform PostTick()"));
		GDisplayCluster->PostTick(DeltaSeconds);

		if (Diagnostics.bSimulateLag)
		{
			const float LagTime = FMath::RandRange(Diagnostics.MinLagTime, Diagnostics.MaxLagTime);
			UE_LOG(LogDisplayClusterEngine, Log, TEXT("Simulating lag: %f seconds"), LagTime);
			FPlatformProcess::Sleep(LagTime);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////
		// Frame end barrier
		ClusterMgr->GetClusterNodeController()->WaitForFrameEnd();

		// Perform EndFrame notification
		GDisplayCluster->EndFrame(GFrameCounter);

		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Sync frame end"));
	}
	else
	{
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("UDisplayClusterGameEngine::Tick, Tick() is not allowed"));
		Super::Tick(DeltaSeconds, bIdleMode);
	}
}


void UDisplayClusterGameEngine::UpdateTimeAndHandleMaxTickRate()
{
	UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("UDisplayClusterGameEngine::UpdateTimeAndHandleMaxTickRate"));

	UEngine::UpdateTimeAndHandleMaxTickRate();

	if (CanTick() && ClusterMgr)
	{
		// Synchronize time data
		ClusterMgr->SyncTimeData();
	}
}

bool UDisplayClusterGameEngine::CanTick() const
{
	return (RunningMode == EDisplayClusterRunningMode::Synced
			|| RunningMode == EDisplayClusterRunningMode::WaitingForSync)
		&& OperationMode == EDisplayClusterOperationMode::Cluster;
}

bool UDisplayClusterGameEngine::BarrierAvoidanceOn() const
{
	return CVarGameStartBarrierAvoidance.GetValueOnGameThread() != 0;
}

bool UDisplayClusterGameEngine::OutOfSync() const
{
	return SyncMap.Num() != 0;
}

void UDisplayClusterGameEngine::ReceivedSync(const FString &Level, const FString &NodeId)
{
	UE_LOG(LogDisplayClusterEngine,Display, TEXT("GameSyncChange event received."));
	TSet<FString> &SyncItem = SyncMap.FindOrAdd(Level);
	SyncItem.Add(NodeId);
	if (SyncItem.Num() == ClusterMgr->GetNodesAmount())
	{
		SyncMap.Remove(Level);
	}
	for (const TTuple<FString, TSet<FString>>& SyncObj : SyncMap)
	{
		FString Join = Algo::Accumulate(SyncObj.Value, FString(), [](FString Result, const FString& Value)
		{
			Result = Result + ", " + Value;
			return MoveTemp(Result);
		});
		UE_LOG(LogDisplayClusterEngine,Display, TEXT("    %s -> %s"), *SyncObj.Key, *Join);
	}
}

void UDisplayClusterGameEngine::CheckGameStartBarrier()
{
	if (!BarrierAvoidanceOn())
	{
		ClusterMgr->GetClusterNodeController()->WaitForGameStart();
	}
	else
	{
		if (!OutOfSync())
		{
			UE_LOG(LogDisplayClusterEngine, Display, TEXT("CheckGameStartBarrier - we are no longer out of sync. Restoring Play."));
			if (RunningMode == EDisplayClusterRunningMode::Startup)
			{
				ClusterMgr->GetClusterNodeController()->WaitForGameStart();
			}
			UGameplayStatics::SetGamePaused(WorldContextObject,false);
			RunningMode = EDisplayClusterRunningMode::Synced;
		}
		else if (!UGameplayStatics::IsGamePaused(WorldContextObject))
		{
			UE_LOG(LogDisplayClusterEngine, Display, TEXT("CheckGameStartBarrier - we are out of sync. Pausing Play."));
			// A 1 or more nodes is out of sync. Do not advance game until everyone is back in sync.
			//
			UGameplayStatics::SetGamePaused(WorldContextObject,true);
		}
	}
}

void UDisplayClusterGameEngine::GameSyncChange(const FDisplayClusterClusterEventJson& InEvent)
{
	if (BarrierAvoidanceOn())
	{
		if(InEvent.Category == DisplayClusterGameEngineUtils::WaitForGameCategory)
		{
			ReceivedSync(InEvent.Type,InEvent.Name);
			CheckGameStartBarrier();
		}
	}
}
