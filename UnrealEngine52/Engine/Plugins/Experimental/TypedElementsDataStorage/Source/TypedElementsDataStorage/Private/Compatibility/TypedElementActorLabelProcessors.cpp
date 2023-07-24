// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorLabelProcessors.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Hash/CityHash.h"
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"

/**
 * UTypedElementActorLabelToColumnProcessor
 */

UTypedElementActorLabelToColumnProcessor::UTypedElementActorLabelToColumnProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bRequiresGameThreadExecution = true;
}

void UTypedElementActorLabelToColumnProcessor::ConfigureQueries()
{
	Query.AddRequirement(FMassActorFragment::StaticStruct(), EMassFragmentAccess::ReadOnly);
	Query.AddRequirement(FTypedElementLabelColumn::StaticStruct(), EMassFragmentAccess::ReadWrite);
	Query.AddRequirement(FTypedElementLabelHashColumn::StaticStruct(), EMassFragmentAccess::ReadWrite);
}

void UTypedElementActorLabelToColumnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Query.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			const FMassActorFragment* ActorIt = Context.GetFragmentView<FMassActorFragment>().GetData();
			FTypedElementLabelColumn* LabelIt = Context.GetMutableFragmentView<FTypedElementLabelColumn>().GetData();
			FTypedElementLabelHashColumn* LabelHashIt = Context.GetMutableFragmentView<FTypedElementLabelHashColumn>().GetData();
			
			const int32 NumEntities = Context.GetNumEntities();
			for (int32 Index = 0; Index < NumEntities; ++Index)
			{
				const FString& ActorLabel = ActorIt->Get()->GetActorLabel(false);
				uint64 ActorLabelHash = CityHash64(
					reinterpret_cast<const char*>(ActorLabel.GetCharArray().GetData()), 
					ActorLabel.Len() * sizeof(ActorLabel.GetCharArray().GetData()[0]));
				if (LabelHashIt->LabelHash != ActorLabelHash)
				{
					LabelIt->Label = ActorLabel;
					LabelHashIt->LabelHash = ActorLabelHash;
				}
				
				++ActorIt;
				++LabelIt;
				++LabelHashIt;
			}
		}
	);
}



/**
 * UTypedElementLabelColumnToActorProcessor
 */

UTypedElementLabelColumnToActorProcessor::UTypedElementLabelColumnToActorProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
	ProcessingPhase = EMassProcessingPhase::FrameEnd;
	bRequiresGameThreadExecution = true;
}

void UTypedElementLabelColumnToActorProcessor::ConfigureQueries()
{
	Query.AddRequirement(FMassActorFragment::StaticStruct(), EMassFragmentAccess::ReadWrite);
	Query.AddRequirement(FTypedElementLabelColumn::StaticStruct(), EMassFragmentAccess::ReadOnly);
	Query.AddRequirement(FTypedElementLabelHashColumn::StaticStruct(), EMassFragmentAccess::ReadOnly);
	Query.AddTagRequirement(*FTypedElementSyncBackToWorldTag::StaticStruct(), EMassFragmentPresence::All);
}

void UTypedElementLabelColumnToActorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Query.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			FMassActorFragment* ActorIt = Context.GetMutableFragmentView<FMassActorFragment>().GetData();
			const FTypedElementLabelColumn* LabelIt = Context.GetFragmentView<FTypedElementLabelColumn>().GetData();
			const FTypedElementLabelHashColumn* LabelHashIt = Context.GetFragmentView<FTypedElementLabelHashColumn>().GetData();

			const int32 NumEntities = Context.GetNumEntities();
			for (int32 Index = 0; Index < NumEntities; ++Index)
			{
				const FString& ActorLabel = ActorIt->Get()->GetActorLabel(false);
				uint64 ActorLabelHash = CityHash64(
					reinterpret_cast<const char*>(ActorLabel.GetCharArray().GetData()),
					ActorLabel.Len() * sizeof(ActorLabel.GetCharArray().GetData()[0]));
				if (LabelHashIt->LabelHash != ActorLabelHash)
				{
					ActorIt->GetMutable()->SetActorLabel(LabelIt->Label);
				}
				
				++ActorIt;
				++LabelIt;
				++LabelHashIt;
			}
		}
	);
}
