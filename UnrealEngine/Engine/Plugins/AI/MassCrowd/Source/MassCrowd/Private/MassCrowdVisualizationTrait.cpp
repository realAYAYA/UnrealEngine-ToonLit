// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassCrowdVisualizationTrait.h"
#include "MassCrowdRepresentationSubsystem.h"
#include "MassCrowdVisualizationProcessor.h"
#include "MassCrowdRepresentationActorManagement.h"
#include "MassCrowdFragments.h"

UMassCrowdVisualizationTrait::UMassCrowdVisualizationTrait()
{
	// Override the subsystem to support parallelization of the crowd
	RepresentationSubsystemClass = UMassCrowdRepresentationSubsystem::StaticClass();
	Params.RepresentationActorManagementClass = UMassCrowdRepresentationActorManagement::StaticClass();
	Params.LODRepresentation[EMassLOD::High] = EMassRepresentationType::HighResSpawnedActor;
	Params.LODRepresentation[EMassLOD::Medium] = EMassRepresentationType::LowResSpawnedActor;
	Params.LODRepresentation[EMassLOD::Low] = EMassRepresentationType::StaticMeshInstance;
	Params.LODRepresentation[EMassLOD::Off] = EMassRepresentationType::None;
	// Set bKeepLowResActor to true as a spawning optimization, this will keep the low-res actor if available while showing the static mesh instance
	Params.bKeepLowResActors = true;
	Params.bKeepActorExtraFrame = true;
	Params.bSpreadFirstVisualizationUpdate = false;
	Params.WorldPartitionGridNameContainingCollision = NAME_None;
	Params.NotVisibleUpdateRate = 0.5f;

	LODParams.BaseLODDistance[EMassLOD::High] = 0.f;
	LODParams.BaseLODDistance[EMassLOD::Medium] = 0.f;
	LODParams.BaseLODDistance[EMassLOD::Low] = 3000.f;
	LODParams.BaseLODDistance[EMassLOD::Off] = 6000.f;

	LODParams.VisibleLODDistance[EMassLOD::High] = 0.f;
	LODParams.VisibleLODDistance[EMassLOD::Medium] = 0.f;
	LODParams.VisibleLODDistance[EMassLOD::Low] = 6000.f;
	LODParams.VisibleLODDistance[EMassLOD::Off] = 50000.f;

	LODParams.LODMaxCount[EMassLOD::High] = 10;
	LODParams.LODMaxCount[EMassLOD::Medium] = 20;
	LODParams.LODMaxCount[EMassLOD::Low] = 500;
	LODParams.LODMaxCount[EMassLOD::Off] = TNumericLimits<int32>::Max();

	LODParams.BufferHysteresisOnDistancePercentage = 20.0f;
	LODParams.DistanceToFrustum = 0.0f;
	LODParams.DistanceToFrustumHysteresis = 0.0f;

	LODParams.FilterTag = FMassCrowdTag::StaticStruct();
}
