// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassNavigationSubsystem.h"
#include "Engine/World.h"
#include "MassSimulationSubsystem.h"

//----------------------------------------------------------------------//
// UMassNavigationSubsystem
//----------------------------------------------------------------------//
UMassNavigationSubsystem::UMassNavigationSubsystem()
	: AvoidanceObstacleGrid(250.f) // 2.5m grid
{
}

void UMassNavigationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassSimulationSubsystem>();
}

