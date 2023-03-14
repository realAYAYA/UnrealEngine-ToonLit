// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassCrowdVisualizationProcessor.h"
#include "MassCrowdFragments.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MassActorSubsystem.h"
#include "MassCrowdRepresentationSubsystem.h"

namespace UE::MassCrowd
{
	int32 bDebugCrowdVisualType = 0;
	FAutoConsoleVariableRef CVarDebugVisualType(TEXT("ai.debug.CrowdVisualType"), bDebugCrowdVisualType, TEXT("Debug Crowd Visual Type"), ECVF_Cheat);

	FColor CrowdRepresentationTypesColors[] =
	{
		FColor::Red,
		FColor::Yellow,
		FColor::Emerald,
		FColor::White,
	};
} // UE::MassCrowd

//----------------------------------------------------------------------//
// UMassCrowdVisualizationProcessor
//----------------------------------------------------------------------//
UMassCrowdVisualizationProcessor::UMassCrowdVisualizationProcessor()
{
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);

	bAutoRegisterWithProcessingPhases = true;

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);

	bRequiresGameThreadExecution = true;
}

void UMassCrowdVisualizationProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassCrowdTag>(EMassFragmentPresence::All);
}

//----------------------------------------------------------------------//
// UMassDebugCrowdVisualizationProcessor
//----------------------------------------------------------------------//
UMassDebugCrowdVisualizationProcessor::UMassDebugCrowdVisualizationProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);
	ExecutionOrder.ExecuteAfter.Add(UMassCrowdVisualizationProcessor::StaticClass()->GetFName());

	bRequiresGameThreadExecution = true;
}

void UMassDebugCrowdVisualizationProcessor::ConfigureQueries()
{
	EntityQuery.AddTagRequirement<FMassCrowdTag>(EMassFragmentPresence::All);

	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RequireMutatingWorldAccess(); // due to UWorld mutable access
}

void UMassDebugCrowdVisualizationProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	World = Owner.GetWorld();
	check(World);
}

void UMassDebugCrowdVisualizationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (UE::MassCrowd::bDebugCrowdVisualType)
	{
		check(World);

		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("DebugDisplayVisualType"))

		EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](const FMassExecutionContext& Context)
		{
			const TConstArrayView<FMassRepresentationFragment> VisualizationList = Context.GetFragmentView<FMassRepresentationFragment>();
			const TConstArrayView<FMassActorFragment> ActorList = Context.GetFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> EntityLocationList = Context.GetFragmentView<FTransformFragment>();

			const int32 NumEntities = Context.GetNumEntities();
			for (int EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
			{
				const FTransformFragment& EntityLocation = EntityLocationList[EntityIdx];
				const FMassRepresentationFragment& Visualization = VisualizationList[EntityIdx];
				const FMassActorFragment& ActorInfo = ActorList[EntityIdx];
				const int32 RepresentationTypeIdx = (int32)Visualization.CurrentRepresentation;
				// Show replicated actors
				if (ActorInfo.IsValid() && !ActorInfo.IsOwnedByMass())
				{
					DrawDebugBox(World, EntityLocation.GetTransform().GetLocation() + FVector(0.0f, 0.0f, 120.0f), FVector(25.0f), UE::MassCrowd::CrowdRepresentationTypesColors[RepresentationTypeIdx]);
				}
				else
				{ 
					DrawDebugSolidBox(World, EntityLocation.GetTransform().GetLocation() + FVector(0.0f, 0.0f, 120.0f), FVector(25.0f), UE::MassCrowd::CrowdRepresentationTypesColors[RepresentationTypeIdx]);
				}
			}
		});
	}
}