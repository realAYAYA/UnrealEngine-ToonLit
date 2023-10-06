// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilities.h"

#if WITH_EDITOR

class IWorldPartitionHLODUtilitiesModule : public IModuleInterface
{
public:
	virtual IWorldPartitionHLODUtilities* GetUtilities() = 0;
};

#endif
