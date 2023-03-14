// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Controller/IDisplayClusterClusterNodeController.h"


/**
 * Null controller. It's used when no 'legal' controller instantiated.
 * The main goal is to always have a cluster controller instance.
 */
class FDisplayClusterClusterNodeCtrlNull final
	: public IDisplayClusterClusterNodeController
{
public:
	FDisplayClusterClusterNodeCtrlNull(const FString& CtrlName, const FString& InNodeName)
		: NodeName(InNodeName)
		, ControllerName(CtrlName) 
	{ }

	virtual ~FDisplayClusterClusterNodeCtrlNull() = default;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Initialize() override
	{
		return true;
	}

	virtual void Shutdown() override
	{ }

	virtual EDisplayClusterNodeRole GetClusterRole() const override
	{
		return EDisplayClusterNodeRole::None;
	}

	virtual FString GetNodeId() const override
	{
		return NodeName;
	}

	virtual FString GetControllerName() const override
	{
		return ControllerName;
	}

	virtual void SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly) override
	{ }

	virtual void SendClusterEventTo(const FString& Address, const uint16 Port, const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly) override
	{ }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolClusterSync
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult WaitForGameStart() override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult WaitForFrameStart() override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult WaitForFrameEnd() override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime) override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}


	virtual EDisplayClusterCommResult GetObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData) override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents) override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

	virtual EDisplayClusterCommResult GetNativeInputData(TMap<FString, FString>& OutNativeInputData) override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolRenderSync
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult WaitForSwapSync() override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolEventsJson
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event) override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolEventsBinary
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event) override final
	{
		return EDisplayClusterCommResult::NotImplemented;
	}

private:
	const FString NodeName;
	const FString ControllerName;
};
