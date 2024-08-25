// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassDistanceVisualizationTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassRepresentationSubsystem.h"
#include "MassCommonFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationActorManagement.h"
#include "Engine/World.h"
#include "MassLODFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityUtils.h"
#include "MassDistanceLODProcessor.h"
#include "MassRepresentationProcessor.h"


UMassDistanceVisualizationTrait::UMassDistanceVisualizationTrait()
{
	RepresentationSubsystemClass = UMassRepresentationSubsystem::StaticClass();

	Params.RepresentationActorManagementClass = UMassRepresentationActorManagement::StaticClass();
	Params.LODRepresentation[EMassLOD::High] = EMassRepresentationType::HighResSpawnedActor;
	Params.LODRepresentation[EMassLOD::Medium] = EMassRepresentationType::LowResSpawnedActor;
	Params.LODRepresentation[EMassLOD::Low] = EMassRepresentationType::StaticMeshInstance;
	Params.LODRepresentation[EMassLOD::Off] = EMassRepresentationType::None;

	LODParams.LODDistance[EMassLOD::High] = 0.f;
	LODParams.LODDistance[EMassLOD::Medium] = 1000.f;
	LODParams.LODDistance[EMassLOD::Low] = 2500.f;
	LODParams.LODDistance[EMassLOD::Off] = 10000.f;

	LODParams.BufferHysteresisOnDistancePercentage = 10.0f;

	bAllowServerSideVisualization = false;
}

void UMassDistanceVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// This should not be ran on NM_Server network mode
	if (World.IsNetMode(NM_DedicatedServer) && !bAllowServerSideVisualization)
	{
		return;
	}

	BuildContext.RequireFragment<FMassViewerInfoFragment>();
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	UMassRepresentationSubsystem* RepresentationSubsystem = Cast<UMassRepresentationSubsystem>(World.GetSubsystemBase(RepresentationSubsystemClass));
	if (RepresentationSubsystem == nullptr)
	{
		UE_LOG(LogMassRepresentation, Error, TEXT("Expecting a valid class for the representation subsystem"));
		RepresentationSubsystem = UWorld::GetSubsystem<UMassRepresentationSubsystem>(&World);
		check(RepresentationSubsystem);
	}

	FMassRepresentationSubsystemSharedFragment SubsystemSharedFragment;
	SubsystemSharedFragment.RepresentationSubsystem = RepresentationSubsystem;
	uint32 SubsystemHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(SubsystemSharedFragment));
	FSharedStruct SubsystemFragment = EntityManager.GetOrCreateSharedFragmentByHash<FMassRepresentationSubsystemSharedFragment>(SubsystemHash, SubsystemSharedFragment);
	BuildContext.AddSharedFragment(SubsystemFragment);

	if (!Params.RepresentationActorManagementClass)
	{
		UE_LOG(LogMassRepresentation, Error, TEXT("Expecting a valid class for the representation actor management"));
	}
	FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	ParamsFragment.Get<const FMassRepresentationParameters>().ComputeCachedValues();
	BuildContext.AddConstSharedFragment(ParamsFragment);

	FMassRepresentationFragment& RepresentationFragment = BuildContext.AddFragment_GetRef<FMassRepresentationFragment>();
	if (bRegisterStaticMeshDesc)
	{
		RepresentationFragment.StaticMeshDescHandle = RepresentationSubsystem->FindOrAddStaticMeshDesc(StaticMeshInstanceDesc);
	}
	RepresentationFragment.HighResTemplateActorIndex = HighResTemplateActor.Get() ? RepresentationSubsystem->FindOrAddTemplateActor(HighResTemplateActor.Get()) : INDEX_NONE;
	RepresentationFragment.LowResTemplateActorIndex = LowResTemplateActor.Get() ? RepresentationSubsystem->FindOrAddTemplateActor(LowResTemplateActor.Get()) : INDEX_NONE;

	FConstSharedStruct LODParamsFragment = EntityManager.GetOrCreateConstSharedFragment(LODParams);
	BuildContext.AddConstSharedFragment(LODParamsFragment);

	uint32 LODParamsHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(LODParams));
	FSharedStruct LODSharedFragment = EntityManager.GetOrCreateSharedFragmentByHash<FMassDistanceLODSharedFragment>(LODParamsHash, LODParams);
	BuildContext.AddSharedFragment(LODSharedFragment);

	BuildContext.AddFragment<FMassRepresentationLODFragment>();
	BuildContext.AddTag<FMassVisibilityCulledByDistanceTag>();
	BuildContext.AddChunkFragment<FMassVisualizationChunkFragment>();

	BuildContext.AddTag<FMassDistanceLODProcessorTag>();
	BuildContext.AddTag<FMassVisualizationProcessorTag>();
}


