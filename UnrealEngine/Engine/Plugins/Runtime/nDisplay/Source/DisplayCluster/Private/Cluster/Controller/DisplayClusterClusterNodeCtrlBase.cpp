// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlBase.h"

#include "IDisplayCluster.h"

#include "Network/IDisplayClusterServer.h"
#include "Network/IDisplayClusterClient.h"
#include "Network/Listener/DisplayClusterTcpListener.h"
#include "Network/Service/ClusterEventsJson/DisplayClusterClusterEventsJsonClient.h"
#include "Network/Service/ClusterEventsBinary/DisplayClusterClusterEventsBinaryClient.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterNodeCtrlBase::FDisplayClusterClusterNodeCtrlBase(const FString& CtrlName, const FString& NodeName)
	: NodeName(NodeName)
	, ControllerName(CtrlName)
	, ExternalEventsClientJson(MakeUnique<FDisplayClusterClusterEventsJsonClient>())
{
}

FDisplayClusterClusterNodeCtrlBase::~FDisplayClusterClusterNodeCtrlBase()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterNodeController
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterNodeCtrlBase::Initialize()
{
	if (!InitializeServers())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Servers initialization failed"));
		return false;
	}

	if (!InitializeClients())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Clients initialization failed"));
		return false;
	}

	if (!StartServers())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("An error occurred during servers start"));
		return false;
	}

	if (!StartClients())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("An error occurred during clients start"));
		return false;
	}

	return true;
}

void FDisplayClusterClusterNodeCtrlBase::Shutdown()
{
	StopClients();
	StopServers();
}

void FDisplayClusterClusterNodeCtrlBase::SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly)
{
	// We should synchronize access to the client
	FScopeLock Lock(&ExternEventsClientJsonGuard);

	// One-shot connection
	ExternalEventsClientJson->Connect(Address, Port, 1, 0.f);
	ExternalEventsClientJson->EmitClusterEventJson(Event);
	ExternalEventsClientJson->Disconnect();
}

void FDisplayClusterClusterNodeCtrlBase::SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly)
{
	// We should synchronize access to the client
	FScopeLock Lock(&ExternEventsClientBinaryGuard);

	// One-shot connection
	ExternalEventsClientBinary->Connect(Address, Port, 1, 0.f);
	ExternalEventsClientBinary->EmitClusterEventBinary(Event);
	ExternalEventsClientBinary->Disconnect();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterNodeCtrlBase
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterNodeCtrlBase::StartServerWithLogs(IDisplayClusterServer* Server, const FString& Address, const uint16 Port) const
{
	if (!Server)
	{
		UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Invalid server instance (nullptr)"));
		return false;
	}

	const bool bResult = Server->Start(Address, Port);

	if (bResult)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Server %s has started"), *Server->GetName());
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Server %s failed to start"), *Server->GetName());
	}

	return bResult;
}

bool FDisplayClusterClusterNodeCtrlBase::StartServerWithLogs(IDisplayClusterServer* const Server, TSharedPtr<FDisplayClusterTcpListener>& TcpListener)
{
	if (!Server)
	{
		UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Invalid server instance (nullptr)"));
		return false;
	}

	const bool bResult = Server->Start(TcpListener);

	if (bResult)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Server %s has started"), *Server->GetName());
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Server %s failed to start"), *Server->GetName());
	}

	return bResult;
}

bool FDisplayClusterClusterNodeCtrlBase::StartClientWithLogs(IDisplayClusterClient* const Client, const FString& Address, const uint16 Port, const uint32 ClientConnTriesAmount, const uint32 ClientConnRetryDelay) const
{
	if (!Client)
	{
		UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Invalid client instance (nullptr)"));
		return false;
	}

	const bool bResult = Client->Connect(Address, Port, ClientConnTriesAmount, ClientConnRetryDelay);

	if (bResult)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s connected to the server %s:%d"), *Client->GetName(), *Address, Port);
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("%s couldn't connect to the server %s:%d"), *Client->GetName(), *Address, Port);
	}

	return bResult;
}
