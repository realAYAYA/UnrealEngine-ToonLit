// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassZoneGraphAnnotationProcessors.h"
#include "MassAIBehaviorTypes.h"
#include "MassZoneGraphNavigationFragments.h"
#include "MassZoneGraphAnnotationFragments.h"
#include "MassZoneGraphAnnotationTypes.h"
#include "MassGameplayExternalTraits.h"
#include "ZoneGraphTypes.h"
#include "ZoneGraphAnnotationSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassSimulationLOD.h"
#include "Engine/World.h"

//----------------------------------------------------------------------//
//  UMassZoneGraphAnnotationTagsInitializer
//----------------------------------------------------------------------//
UMassZoneGraphAnnotationTagsInitializer::UMassZoneGraphAnnotationTagsInitializer()
	: EntityQuery(*this)
{
	ObservedType = FMassZoneGraphAnnotationFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
}

void UMassZoneGraphAnnotationTagsInitializer::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassZoneGraphAnnotationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSubsystemRequirement<UZoneGraphAnnotationSubsystem>(EMassFragmentAccess::ReadOnly);
}

void UMassZoneGraphAnnotationTagsInitializer::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
	{
		const UZoneGraphAnnotationSubsystem& ZoneGraphAnnotationSubsystem = Context.GetSubsystemChecked<UZoneGraphAnnotationSubsystem>(World);

		const int32 NumEntities = Context.GetNumEntities();
		const TArrayView<FMassZoneGraphAnnotationFragment> AnnotationTagsList = Context.GetMutableFragmentView<FMassZoneGraphAnnotationFragment>();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> LaneLocationList = Context.GetFragmentView<FMassZoneGraphLaneLocationFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassZoneGraphAnnotationFragment& AnnotationTags = AnnotationTagsList[EntityIndex];
			const FMassZoneGraphLaneLocationFragment& LaneLocation = LaneLocationList[EntityIndex];

			if (!LaneLocation.LaneHandle.IsValid())
			{
				AnnotationTags.Tags = FZoneGraphTagMask::None;
			}
			else
			{
				AnnotationTags.Tags = ZoneGraphAnnotationSubsystem.GetAnnotationTags(LaneLocation.LaneHandle);
			}
		}
	});
}

//----------------------------------------------------------------------//
//  UMassZoneGraphAnnotationTagUpdateProcessor
//----------------------------------------------------------------------//
UMassZoneGraphAnnotationTagUpdateProcessor::UMassZoneGraphAnnotationTagUpdateProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateAnnotationTags;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Behavior);
}

void UMassZoneGraphAnnotationTagUpdateProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::CurrentLaneChanged);
}

void UMassZoneGraphAnnotationTagUpdateProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();
	EntityQuery.AddRequirement<FMassZoneGraphAnnotationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddChunkRequirement<FMassZoneGraphAnnotationVariableTickChunkFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddSubsystemRequirement<UZoneGraphAnnotationSubsystem>(EMassFragmentAccess::ReadWrite);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassZoneGraphAnnotationTagUpdateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TransientEntitiesToSignal.Reset();

	// Calling super will update the signals, and call SignalEntities() below.
	Super::Execute(EntityManager, Context);

	UWorld* World = EntityManager.GetWorld();
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, World](FMassExecutionContext& Context)
	{
		// Periodically update tags.
		if (!FMassZoneGraphAnnotationVariableTickChunkFragment::UpdateChunk(Context))
		{
			return;
		}
		
		UZoneGraphAnnotationSubsystem& ZoneGraphAnnotationSubsystem = Context.GetMutableSubsystemChecked<UZoneGraphAnnotationSubsystem>(World);

		const int32 NumEntities = Context.GetNumEntities();
		const TArrayView<FMassZoneGraphAnnotationFragment> AnnotationTagsList = Context.GetMutableFragmentView<FMassZoneGraphAnnotationFragment>();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> LaneLocationList = Context.GetFragmentView<FMassZoneGraphLaneLocationFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassZoneGraphAnnotationFragment& AnnotationTags = AnnotationTagsList[EntityIndex];
			const FMassZoneGraphLaneLocationFragment& LaneLocation = LaneLocationList[EntityIndex];

			UpdateAnnotationTags(ZoneGraphAnnotationSubsystem, AnnotationTags, LaneLocation, Context.GetEntity(EntityIndex));
		}
	});

	if (TransientEntitiesToSignal.Num())
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>(World);
		SignalSubsystem.SignalEntities(UE::Mass::Signals::AnnotationTagsChanged, TransientEntitiesToSignal);
	}
}

void UMassZoneGraphAnnotationTagUpdateProcessor::UpdateAnnotationTags(UZoneGraphAnnotationSubsystem& ZoneGraphAnnotationSubsystem, FMassZoneGraphAnnotationFragment& AnnotationTags, const FMassZoneGraphLaneLocationFragment& LaneLocation, const FMassEntityHandle Entity)
{
	const FZoneGraphTagMask OldTags = AnnotationTags.Tags;

	if (LaneLocation.LaneHandle.IsValid())
	{
		AnnotationTags.Tags = ZoneGraphAnnotationSubsystem.GetAnnotationTags(LaneLocation.LaneHandle);
	}
	else
	{
		AnnotationTags.Tags = FZoneGraphTagMask::None;
	}

	if (OldTags != AnnotationTags.Tags)
	{
		TransientEntitiesToSignal.Add(Entity);
	}
}

void UMassZoneGraphAnnotationTagUpdateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
	{
		UZoneGraphAnnotationSubsystem& ZoneGraphAnnotationSubsystem = Context.GetMutableSubsystemChecked<UZoneGraphAnnotationSubsystem>(World);

		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> LaneLocationList = Context.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
		const TArrayView<FMassZoneGraphAnnotationFragment> AnnotationTagsList = Context.GetMutableFragmentView<FMassZoneGraphAnnotationFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassZoneGraphAnnotationFragment& AnnotationTags = AnnotationTagsList[EntityIndex];
			const FMassZoneGraphLaneLocationFragment& LaneLocation = LaneLocationList[EntityIndex];

			UpdateAnnotationTags(ZoneGraphAnnotationSubsystem, AnnotationTags, LaneLocation, Context.GetEntity(EntityIndex));
		}
	});
}
