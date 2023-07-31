// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"

/**
* IWorldPartitionHLODUtilities module interface
*/
class WORLDPARTITIONHLODUTILITIES_API FWorldPartitionHLODUtilitiesModule : public IWorldPartitionHLODUtilitiesModule
{
public:
	virtual void ShutdownModule() override;
	virtual void StartupModule() override;

	virtual IWorldPartitionHLODUtilities* GetUtilities() override;

private:
	IWorldPartitionHLODUtilities* Utilities;
};
