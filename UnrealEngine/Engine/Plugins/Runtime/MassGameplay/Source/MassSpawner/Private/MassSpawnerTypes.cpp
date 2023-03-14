// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassSpawnerTypes.h"
#include "MassEntityConfigAsset.h"

const UMassEntityConfigAsset* FMassSpawnedEntityType::GetEntityConfig() const
{
	if (EntityConfigPtr == nullptr)
	{
		EntityConfigPtr = EntityConfig.LoadSynchronous();
	}
	return EntityConfigPtr;
}

UMassEntityConfigAsset* FMassSpawnedEntityType::GetEntityConfig()
{
	if (EntityConfigPtr == nullptr)
	{
		EntityConfigPtr = EntityConfig.LoadSynchronous();
	}
	return EntityConfigPtr;
}