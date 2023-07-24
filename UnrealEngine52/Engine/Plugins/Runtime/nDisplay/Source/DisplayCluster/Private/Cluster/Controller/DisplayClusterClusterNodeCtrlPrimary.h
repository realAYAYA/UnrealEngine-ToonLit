// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlSecondary.h"
#include "Network/IDisplayClusterServer.h"

class FDisplayClusterClusterSyncService;
class FDisplayClusterRenderSyncService;
class FDisplayClusterClusterEventsJsonService;
class FDisplayClusterClusterEventsBinaryService;
class FDisplayClusterTcpListener;


/**
 * Primary node controller implementation (cluster mode). Manages servers on primary node side.
 */
class FDisplayClusterClusterNodeCtrlPrimary
	: public FDisplayClusterClusterNodeCtrlSecondary
{
public:
	FDisplayClusterClusterNodeCtrlPrimary(const FString& CtrlName, const FString& NodeName);
	virtual ~FDisplayClusterClusterNodeCtrlPrimary();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolClusterSync
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime) override;
	virtual EDisplayClusterCommResult GetObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData) override;
	virtual EDisplayClusterCommResult GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents) override;
	virtual EDisplayClusterCommResult GetNativeInputData(TMap<FString, FString>& OutNativeInputData) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterNodeRole GetClusterRole() const override
	{
		return EDisplayClusterNodeRole::Primary;
	}

	virtual bool DropClusterNode(const FString& NodeId) override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FDisplayClusterNodeCtrlBase
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool InitializeServers() override;
	virtual bool StartServers()      override;
	virtual void StopServers()       override;

	virtual bool InitializeClients() override;
	virtual bool StartClients()      override;
	virtual void StopClients()       override;

private:
	// Handle node failures
	void ProcessNodeFailed(const FString& NodeId, IDisplayClusterServer::ENodeFailType NodeFailType);

private:
	// Node servers
	TUniquePtr<FDisplayClusterClusterSyncService>         ClusterSyncServer;
	TUniquePtr<FDisplayClusterRenderSyncService>          RenderSyncServer;
	TUniquePtr<FDisplayClusterClusterEventsJsonService>   ClusterEventsJsonServer;
	TUniquePtr<FDisplayClusterClusterEventsBinaryService> ClusterEventsBinaryServer;

	// Shared TCP connection listener. Allows to use single port for multiple internal services.
	TSharedPtr<FDisplayClusterTcpListener> TcpListener;
};
