// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Controller/IDisplayClusterClusterNodeController.h"

class IDisplayClusterClient;
class IDisplayClusterServer;
class FDisplayClusterTcpListener;
class FDisplayClusterClusterEventsBinaryClient;
class FDisplayClusterClusterEventsJsonClient;


/**
 * Abstract cluster node controller (cluster mode).
 */
class FDisplayClusterClusterNodeCtrlBase
	: public IDisplayClusterClusterNodeController
{
public:
	FDisplayClusterClusterNodeCtrlBase(const FString& CtrlName, const FString& NodeName);
	virtual ~FDisplayClusterClusterNodeCtrlBase();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Initialize() override final;
	virtual void Shutdown()   override final;

	virtual FString GetNodeId() const override final
	{
		return NodeName;
	}

	virtual FString GetControllerName() const override final
	{
		return ControllerName;
	}

	virtual void SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventJson&   Event, bool bPrimaryOnly) override;
	virtual void SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolEventsJson - default overrides
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event) override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolEventsBinary - default overrides
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event) override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolClusterSync - default overrides
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult WaitForGameStart() override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult WaitForFrameStart() override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult WaitForFrameEnd() override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime) override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult GetObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData) override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents) override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult GetNativeInputData(TMap<FString, FString>& OutNativeInputData) override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolRenderSync - default overrides
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult WaitForSwapSync() override
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

protected:
	virtual bool InitializeServers()
	{
		return true;
	}

	virtual bool StartServers()
	{
		return true;
	}

	virtual void StopServers()
	{
		return;
	}

	virtual bool InitializeClients()
	{
		return true;
	}

	virtual bool StartClients()
	{
		return true;
	}

	virtual void StopClients()
	{ }

protected:
	bool StartServerWithLogs(IDisplayClusterServer* const Server, const FString& Address, const uint16 Port) const;
	bool StartServerWithLogs(IDisplayClusterServer* const Server, TSharedPtr<FDisplayClusterTcpListener>& TcpListener);
	bool StartClientWithLogs(IDisplayClusterClient* const Client, const FString& Address, const uint16 Port, const uint32 ClientConnTriesAmount, const uint32 ClientConnRetryDelay) const;

private:
	const FString NodeName;
	const FString ControllerName;

	// JSON client for sending events outside of the cluster
	FCriticalSection ExternEventsClientJsonGuard;
	TUniquePtr<FDisplayClusterClusterEventsJsonClient>   ExternalEventsClientJson;

	// Binary client for sending events outside of the cluster
	FCriticalSection ExternEventsClientBinaryGuard;
	TUniquePtr<FDisplayClusterClusterEventsBinaryClient> ExternalEventsClientBinary;
};
