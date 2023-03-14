// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlBase.h"

class FDisplayClusterClusterEventsJsonService;
class FDisplayClusterClusterEventsBinaryService;


/**
 * Editor node controller implementation.
 */
class FDisplayClusterClusterNodeCtrlEditor
	: public FDisplayClusterClusterNodeCtrlBase
{
public:
	FDisplayClusterClusterNodeCtrlEditor(const FString& CtrlName, const FString& NodeName);
	virtual ~FDisplayClusterClusterNodeCtrlEditor();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterNodeRole GetClusterRole() const override
	{
		return EDisplayClusterNodeRole::None;
	}

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FDisplayClusterNodeCtrlBase
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool InitializeServers() override;
	virtual bool StartServers()      override;
	virtual void StopServers()       override;

private:
	// Node servers
	TUniquePtr<FDisplayClusterClusterEventsJsonService>   ClusterEventsJsonServer;
	TUniquePtr<FDisplayClusterClusterEventsBinaryService> ClusterEventsBinaryServer;
};
