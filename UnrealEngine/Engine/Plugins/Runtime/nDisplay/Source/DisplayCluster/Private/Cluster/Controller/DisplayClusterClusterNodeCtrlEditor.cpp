// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlEditor.h"

#include "Config/IPDisplayClusterConfigManager.h"

#include "Network/Service/ClusterEventsBinary/DisplayClusterClusterEventsBinaryService.h"
#include "Network/Service/ClusterEventsJson/DisplayClusterClusterEventsJsonService.h"

#include "DisplayClusterConfigurationTypes.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterNodeCtrlEditor::FDisplayClusterClusterNodeCtrlEditor(const FString& CtrlName, const FString& NodeName)
	: FDisplayClusterClusterNodeCtrlBase(CtrlName, NodeName)
{
}

FDisplayClusterClusterNodeCtrlEditor::~FDisplayClusterClusterNodeCtrlEditor()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterNodeCtrlBase
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterNodeCtrlEditor::InitializeServers()
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - initializing servers..."), *GetControllerName());

	// Instantiate node servers
	ClusterEventsJsonServer   = MakeUnique<FDisplayClusterClusterEventsJsonService>();
	ClusterEventsBinaryServer = MakeUnique<FDisplayClusterClusterEventsBinaryService>();

	return ClusterEventsJsonServer && ClusterEventsBinaryServer;
}

bool FDisplayClusterClusterNodeCtrlEditor::StartServers()
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - starting servers..."), *GetControllerName());

	// Get config data
	const UDisplayClusterConfigurationData* ConfigData = GDisplayCluster->GetPrivateConfigMgr()->GetConfig();
	if (!ConfigData)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't get configuration data"));
		return false;
	}

	// Always use localhost for PIE because there might be some other host specified in the configuration data
	const FString HostForPie("127.0.0.1");
	const FDisplayClusterConfigurationPrimaryNodePorts& Ports = ConfigData->Cluster->PrimaryNode.Ports;

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Servers: addr %s, port_cs %d, port_ce %d, port_ceb %d"), *HostForPie, Ports.ClusterSync, Ports.ClusterEventsJson, Ports.ClusterEventsBinary);

	// Start the servers
	return StartServerWithLogs(ClusterEventsJsonServer.Get(),   HostForPie, Ports.ClusterEventsJson)
		&& StartServerWithLogs(ClusterEventsBinaryServer.Get(), HostForPie, Ports.ClusterEventsBinary);
}

void FDisplayClusterClusterNodeCtrlEditor::StopServers()
{
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - shutting down..."), *GetControllerName());

	ClusterEventsJsonServer->Shutdown();
	ClusterEventsBinaryServer->Shutdown();
}
