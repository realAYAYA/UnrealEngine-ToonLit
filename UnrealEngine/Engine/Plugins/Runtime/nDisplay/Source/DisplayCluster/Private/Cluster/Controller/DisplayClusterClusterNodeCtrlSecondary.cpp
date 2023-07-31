// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlSecondary.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Failover/IDisplayClusterFailoverNodeController.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Network/Service/ClusterEventsJson/DisplayClusterClusterEventsJsonClient.h"
#include "Network/Service/ClusterEventsBinary/DisplayClusterClusterEventsBinaryClient.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncClient.h"
#include "Network/Service/RenderSync/DisplayClusterRenderSyncClient.h"

#include "DisplayClusterConfigurationTypes.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterNodeCtrlSecondary::FDisplayClusterClusterNodeCtrlSecondary(const FString& CtrlName, const FString& NodeName) :
	FDisplayClusterClusterNodeCtrlBase(CtrlName, NodeName)
{
}

FDisplayClusterClusterNodeCtrlSecondary::~FDisplayClusterClusterNodeCtrlSecondary()
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Releasing %s cluster controller..."), *GetControllerName());

	Shutdown();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolClusterSync
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::WaitForGameStart()
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->WaitForGameStart();
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::WaitForFrameStart()
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->WaitForFrameStart();
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::WaitForFrameEnd()
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->WaitForFrameEnd();
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime)
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->GetTimeData(OutDeltaTime, OutGameTime, OutFrameTime);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::GetObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData)
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->GetObjectsData(InSyncGroup, OutObjectsData);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents)
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->GetEventsData(OutJsonEvents, OutBinaryEvents);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}

EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::GetNativeInputData(TMap<FString, FString>& OutNativeInputData)
{
	checkSlow(ClusterSyncClient);
	const EDisplayClusterCommResult CommResult = ClusterSyncClient->GetNativeInputData(OutNativeInputData);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolRenderSync
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::WaitForSwapSync()
{
	checkSlow(RenderSyncClient.IsValid());
	const EDisplayClusterCommResult CommResult = RenderSyncClient->WaitForSwapSync();
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolEventsJson
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event)
{
	checkSlow(ClusterEventsJsonClient);
	const EDisplayClusterCommResult CommResult = ClusterEventsJsonClient->EmitClusterEventJson(Event);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProtocolEventsBinary
//////////////////////////////////////////////////////////////////////////////////////////////
EDisplayClusterCommResult FDisplayClusterClusterNodeCtrlSecondary::EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event)
{
	checkSlow(ClusterEventsBinaryClient);
	const EDisplayClusterCommResult CommResult = ClusterEventsBinaryClient->EmitClusterEventBinary(Event);
	GDisplayCluster->GetPrivateClusterMgr()->GetFailoverNodeController()->HandleCommResult(CommResult);
	return CommResult;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterNodeCtrlBase
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterNodeCtrlSecondary::InitializeServers()
{
	return FDisplayClusterClusterNodeCtrlBase::InitializeServers();
}

bool FDisplayClusterClusterNodeCtrlSecondary::StartServers()
{
	return FDisplayClusterClusterNodeCtrlBase::StartServers();
}

void FDisplayClusterClusterNodeCtrlSecondary::StopServers()
{
	FDisplayClusterClusterNodeCtrlBase::StopServers();
}

bool FDisplayClusterClusterNodeCtrlSecondary::InitializeClients()
{
	if (!FDisplayClusterClusterNodeCtrlBase::InitializeClients())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - initializing secondary node clients..."), *GetControllerName());

	// Instantiate local clients
	ClusterSyncClient         = MakeUnique<FDisplayClusterClusterSyncClient>();
	RenderSyncClient          = MakeUnique<FDisplayClusterRenderSyncClient>();
	ClusterEventsJsonClient   = MakeUnique<FDisplayClusterClusterEventsJsonClient>();
	ClusterEventsBinaryClient = MakeUnique<FDisplayClusterClusterEventsBinaryClient>();

	return ClusterSyncClient && RenderSyncClient && ClusterEventsJsonClient && ClusterEventsBinaryClient;
}

bool FDisplayClusterClusterNodeCtrlSecondary::StartClients()
{
	if (!FDisplayClusterClusterNodeCtrlBase::StartClients())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - initializing secondary node clients..."), *GetControllerName());

	// Primary config
	const UDisplayClusterConfigurationClusterNode* CfgPrimary = GDisplayCluster->GetPrivateConfigMgr()->GetPrimaryNode();
	if (!CfgPrimary)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("No primary node configuration data found"));
		return false;
	}

	// Get configuration data
	const UDisplayClusterConfigurationData* ConfigData = GDisplayCluster->GetPrivateConfigMgr()->GetConfig();
	if (!ConfigData)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't get configuration data"));
		return false;
	}

	// Helper shortcuts to avoid long lines below
	const FDisplayClusterConfigurationPrimaryNode&     PrimaryNode     = ConfigData->Cluster->PrimaryNode;
	const FDisplayClusterConfigurationNetworkSettings& NetworkSettings = ConfigData->Cluster->Network;

	// Start the clients
	return StartClientWithLogs(ClusterSyncClient.Get(),         CfgPrimary->Host, PrimaryNode.Ports.ClusterSync,         NetworkSettings.ConnectRetriesAmount, NetworkSettings.ConnectRetryDelay)
		&& StartClientWithLogs(RenderSyncClient.Get(),          CfgPrimary->Host, PrimaryNode.Ports.ClusterSync,         NetworkSettings.ConnectRetriesAmount, NetworkSettings.ConnectRetryDelay)
		&& StartClientWithLogs(ClusterEventsJsonClient.Get(),   CfgPrimary->Host, PrimaryNode.Ports.ClusterEventsJson,   NetworkSettings.ConnectRetriesAmount, NetworkSettings.ConnectRetryDelay)
		&& StartClientWithLogs(ClusterEventsBinaryClient.Get(), CfgPrimary->Host, PrimaryNode.Ports.ClusterEventsBinary, NetworkSettings.ConnectRetriesAmount, NetworkSettings.ConnectRetryDelay);
}

void FDisplayClusterClusterNodeCtrlSecondary::StopClients()
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Cluster controller %s is shutting down the clients..."), *GetControllerName());

	ClusterEventsJsonClient->Disconnect();
	ClusterEventsBinaryClient->Disconnect();
	ClusterSyncClient->Disconnect();
	RenderSyncClient->Disconnect();

	FDisplayClusterClusterNodeCtrlBase::StopClients();
}
