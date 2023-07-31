// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayCluster.h"
#include "IPDisplayClusterManager.h"

class IPDisplayClusterRenderManager;
class IPDisplayClusterClusterManager;
class IPDisplayClusterConfigManager;
class IPDisplayClusterGameManager;
class ADisplayClusterSettings;


/**
 * Private module interface
 */
class IPDisplayCluster
	: public IDisplayCluster
	, public IPDisplayClusterManager
{
public:
	virtual ~IPDisplayCluster() = default;

public:
	virtual IPDisplayClusterRenderManager*    GetPrivateRenderMgr() const = 0;
	virtual IPDisplayClusterClusterManager*   GetPrivateClusterMgr() const = 0;
	virtual IPDisplayClusterConfigManager*    GetPrivateConfigMgr() const = 0;
	virtual IPDisplayClusterGameManager*      GetPrivateGameMgr() const = 0;
};
