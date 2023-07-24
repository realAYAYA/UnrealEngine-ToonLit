// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorTransformProcessors.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"

/**
 * UTypedElementActorAddTransformColumnProcessor
 */

UTypedElementActorAddTransformColumnProcessor::UTypedElementActorAddTransformColumnProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	// Run this before the world gets synced to MASS so a new transform is initialized in the same frame.
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bRequiresGameThreadExecution = true;
}

void UTypedElementActorAddTransformColumnProcessor::ConfigureQueries()
{
	Query.AddRequirement(FMassActorFragment::StaticStruct(), EMassFragmentAccess::ReadOnly);
	Query.AddRequirement(FTypedElementLocalTransformColumn::StaticStruct(), EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
}

void UTypedElementActorAddTransformColumnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Query.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			const FMassActorFragment* ActorIt = Context.GetFragmentView<FMassActorFragment>().GetData();
			
			for (FMassEntityHandle Entity : Context.GetEntities())
			{
				if (ActorIt->Get()->GetRootComponent())
				{
					Context.Defer().AddFragment_RuntimeCheck<FTypedElementLocalTransformColumn>(Entity);
				}
				
				++ActorIt;
			}
		}
	);
}


/**
 * UTypedElementActorLocalTransformToColumnProcessor
 */

UTypedElementActorLocalTransformToColumnProcessor::UTypedElementActorLocalTransformToColumnProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bRequiresGameThreadExecution = true;
}

void UTypedElementActorLocalTransformToColumnProcessor::ConfigureQueries()
{
	Query.AddRequirement(FMassActorFragment::StaticStruct(), EMassFragmentAccess::ReadOnly);
	Query.AddRequirement(FTypedElementLocalTransformColumn::StaticStruct(), EMassFragmentAccess::ReadWrite);
}

void UTypedElementActorLocalTransformToColumnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Query.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			const FMassActorFragment* ActorIt = Context.GetFragmentView<FMassActorFragment>().GetData();
			FTypedElementLocalTransformColumn* TransformIt = Context.GetMutableFragmentView<FTypedElementLocalTransformColumn>().GetData();

			for (FMassEntityHandle Entity : Context.GetEntities())
			{
				const AActor* ActorInstance = ActorIt->Get();
				if (ActorInstance != nullptr && ActorInstance->GetRootComponent() != nullptr)
				{
					TransformIt->Transform = ActorIt->Get()->GetActorTransform();
				}
				else
				{
					Context.Defer().RemoveFragment_RuntimeCheck<FTypedElementLocalTransformColumn>(Entity);
				}
				
				++ActorIt;
				++TransformIt;
			}
		}
	);
}


/**
 * UTypedElementTransformColumnToActorProcessor
 */

UTypedElementTransformColumnToActorProcessor::UTypedElementTransformColumnToActorProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
	ProcessingPhase = EMassProcessingPhase::FrameEnd;
	bRequiresGameThreadExecution = true;
}

void UTypedElementTransformColumnToActorProcessor::ConfigureQueries()
{
	Query.AddRequirement(FMassActorFragment::StaticStruct(), EMassFragmentAccess::ReadWrite);
	Query.AddRequirement(FTypedElementLocalTransformColumn::StaticStruct(), EMassFragmentAccess::ReadOnly);
	Query.AddTagRequirement(*FTypedElementSyncBackToWorldTag::StaticStruct(), EMassFragmentPresence::All);
}

void UTypedElementTransformColumnToActorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Query.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			FMassActorFragment* ActorIt = Context.GetMutableFragmentView<FMassActorFragment>().GetData();
			const FTypedElementLocalTransformColumn* TransformIt = Context.GetFragmentView<FTypedElementLocalTransformColumn>().GetData();
			
			const int32 NumEntities = Context.GetNumEntities();
			for (int32 Index = 0; Index < NumEntities; ++Index)
			{
				ActorIt->GetMutable()->SetActorTransform(TransformIt->Transform);
				
				++ActorIt;
				++TransformIt;
			}
		}
	);
}
