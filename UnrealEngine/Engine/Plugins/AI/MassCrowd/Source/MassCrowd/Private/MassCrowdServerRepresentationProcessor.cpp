// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassCrowdServerRepresentationProcessor.h"
#include "MassCrowdFragments.h"
#include "MassActorSubsystem.h"

UMassCrowdServerRepresentationProcessor::UMassCrowdServerRepresentationProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::Server;

	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);
}

void UMassCrowdServerRepresentationProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassCrowdTag>(EMassFragmentPresence::All);
}
