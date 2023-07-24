// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionHLODUtilitiesModule.h"
#include "Modules/ModuleManager.h"
#include "WorldPartitionHLODUtilities.h"

IMPLEMENT_MODULE(FWorldPartitionHLODUtilitiesModule, WorldPartitionHLODUtilities);

void FWorldPartitionHLODUtilitiesModule::StartupModule()
{
	Utilities = nullptr;
}

void FWorldPartitionHLODUtilitiesModule::ShutdownModule()
{
	if (Utilities)
	{
		delete Utilities;
		Utilities = nullptr;
	}
}

IWorldPartitionHLODUtilities* FWorldPartitionHLODUtilitiesModule::GetUtilities()
{
	if (Utilities == nullptr)
	{
		Utilities = new FWorldPartitionHLODUtilities();
	}

	return Utilities;
}
