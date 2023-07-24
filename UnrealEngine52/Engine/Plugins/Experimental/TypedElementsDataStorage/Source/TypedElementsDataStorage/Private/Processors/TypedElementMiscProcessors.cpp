// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/TypedElementMiscProcessors.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Elements/Columns/TypedElementMiscColumns.h"

/**
 * UTypedElementRemoveSyncToWorldTagProcessor
 */

UTypedElementRemoveSyncToWorldTagProcessor::UTypedElementRemoveSyncToWorldTagProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
	ProcessingPhase = EMassProcessingPhase::FrameEnd;
}

void UTypedElementRemoveSyncToWorldTagProcessor::ConfigureQueries()
{
	Query.AddTagRequirement(*FTypedElementSyncBackToWorldTag::StaticStruct(), EMassFragmentPresence::All);
}

void UTypedElementRemoveSyncToWorldTagProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Query.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			for (FMassEntityHandle Entity : Context.GetEntities())
			{
				Context.Defer().RemoveTag_RuntimeCheck<FTypedElementSyncBackToWorldTag>(Entity);
			}
		}
	);
}
