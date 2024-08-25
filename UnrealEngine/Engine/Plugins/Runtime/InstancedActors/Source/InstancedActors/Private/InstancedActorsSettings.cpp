// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsSettings.h"
#include "ClientInstancedActorsSpawnerSubsystem.h"
#include "ServerInstancedActorsSpawnerSubsystem.h"
#include "InstancedActorsSubsystem.h"
#include "InstancedActorsVisualizationTrait.h"


//-----------------------------------------------------------------------------
// UInstancedActorsProjectSettings
//-----------------------------------------------------------------------------
UInstancedActorsProjectSettings::UInstancedActorsProjectSettings()
{
	ServerActorSpawnerSubsystemClass = UServerInstancedActorsSpawnerSubsystem::StaticClass();
	ClientActorSpawnerSubsystemClass = UClientInstancedActorsSpawnerSubsystem::StaticClass();
	InstancedActorsSubsystemClass = UInstancedActorsSubsystem::StaticClass();
	StationaryVisualizationTraitClass = UInstancedActorsVisualizationTrait::StaticClass();
}

TSubclassOf<UMassActorSpawnerSubsystem> UInstancedActorsProjectSettings::GetServerActorSpawnerSubsystemClass() const 
{ 
	return ServerActorSpawnerSubsystemClass.TryLoadClass<UMassActorSpawnerSubsystem>(); 
}

TSubclassOf<UMassActorSpawnerSubsystem> UInstancedActorsProjectSettings::GetClientActorSpawnerSubsystemClass() const 
{ 
	return ClientActorSpawnerSubsystemClass.TryLoadClass<UMassActorSpawnerSubsystem>(); 
}

TSubclassOf<UInstancedActorsSubsystem> UInstancedActorsProjectSettings::GetInstancedActorsSubsystemClass() const 
{
	// @todo using ResolveClass rather than TryLoadClass since the latter emits "Warning: Failed to find object" if 
	// the indicated class cannot be loaded. That can happen if the target class is loaded later with a GameplayFeatyre plugin
	// This whole section needs changing so that different game modes can utilize different classes.
	return InstancedActorsSubsystemClass.ResolveClass();
}

TSubclassOf<UMassStationaryDistanceVisualizationTrait> UInstancedActorsProjectSettings::GetStationaryVisualizationTraitClass() const
{
	return StationaryVisualizationTraitClass.TryLoadClass<UMassStationaryDistanceVisualizationTrait>();
}
