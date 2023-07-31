// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Game/IDisplayClusterGameManager.h"
#include "IPDisplayClusterManager.h"


/**
 * Game manager private interface
 */
class IPDisplayClusterGameManager
	: public IDisplayClusterGameManager
	, public IPDisplayClusterManager
{
public:
	virtual ~IPDisplayClusterGameManager() = default;

public:
	virtual bool IsDisplayClusterActive() const = 0;
};
