// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilities.h"

class ENGINE_API IWorldPartitionHLODUtilitiesModule : public IModuleInterface
{
public:
	virtual IWorldPartitionHLODUtilities* GetUtilities() = 0;
};
