// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStationaryISMSwitcherProcessor.h"
#include "MassRepresentationSubsystem.h"
#include "MassCommonFragments.h"
#include "MassRepresentationProcessor.h"

#include "MassSignalSubsystem.h"


UMassStationaryISMSwitcherProcessor::UMassStationaryISMSwitcherProcessor(const FObjectInitializer& ObjectInitializer)
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;
	ExecutionOrder.ExecuteAfter.Add(UMassVisualizationProcessor::StaticClass()->GetFName());
	bAutoRegisterWithProcessingPhases = true;
}

void UMassStationaryISMSwitcherProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassRepresentationParameters>();
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassStaticRepresentationTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStationaryISMSwitcherProcessorTag>(EMassFragmentPresence::All);
	EntityQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassStationaryISMSwitcherProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, &UMassStationaryISMSwitcherProcessor::ProcessContext);
}
	
void UMassStationaryISMSwitcherProcessor::ProcessContext(FMassExecutionContext& Context)
{
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();

	UMassRepresentationSubsystem* RepresentationSubsystem = Context.GetMutableSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
	check(RepresentationSubsystem);
	FMassInstancedStaticMeshInfoArrayView ISMInfosView = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();

	const TConstArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetFragmentView<FMassRepresentationLODFragment>();
	const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
	const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();

	const FMassRepresentationParameters& RepresentationParams = Context.GetConstSharedFragment<FMassRepresentationParameters>();
	const bool bDoKeepActorExtraFrame = UE::Mass::Representation::bAllowKeepActorExtraFrame ? RepresentationParams.bKeepLowResActors : false;

	const int32 NumEntities = Context.GetNumEntities();
	for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
	{
		const FMassEntityHandle EntityHandle = Context.GetEntity(EntityIdx);
		const FMassRepresentationLODFragment& RepresentationLOD = RepresentationLODList[EntityIdx];
		const FTransformFragment& TransformFragment = TransformList[EntityIdx];
		FMassRepresentationFragment& Representation = RepresentationList[EntityIdx];

		if (Representation.PrevRepresentation != EMassRepresentationType::StaticMeshInstance
			&& Representation.CurrentRepresentation != EMassRepresentationType::StaticMeshInstance)
		{
			// nothing to do here
			continue;
		}

		if (!ensureMsgf(Representation.StaticMeshDescHandle.IsValid() && ISMInfosView.IsValidIndex(Representation.StaticMeshDescHandle.ToIndex())
						, TEXT("Invalid handle index %u for ISMInfosView"), Representation.StaticMeshDescHandle.ToIndex()))
		{
			continue;
		}
		FMassInstancedStaticMeshInfo& ISMInfo = ISMInfosView[Representation.StaticMeshDescHandle.ToIndex()];

		if (Representation.PrevRepresentation == EMassRepresentationType::StaticMeshInstance
			&& Representation.CurrentRepresentation != EMassRepresentationType::StaticMeshInstance)
		{
			// note that we're using the PrevLODSignificance here, and the reason for it is that the Prev value matches the 
			// PrevRepresentation - thus we need to remove from the "previously" used LODSignificance range.
			ISMInfo.RemoveInstance(EntityHandle, Representation.PrevLODSignificance);

			// consume "prev" data
			Representation.PrevRepresentation = Representation.CurrentRepresentation;

			if (Representation.PrevRepresentation != EMassRepresentationType::None)
			{
				SignalSubsystem.SignalEntity(UE::Mass::Signals::SwitchedToActor, EntityHandle);
			}
		}
		else if (Representation.PrevRepresentation != EMassRepresentationType::StaticMeshInstance
			&& Representation.CurrentRepresentation == EMassRepresentationType::StaticMeshInstance)
		{
			const FTransform& Transform = TransformFragment.GetTransform();
			const FTransform& PrevTransform = Representation.PrevTransform;
			const float LODSignificance = RepresentationLOD.LODSignificance;
			const float PrevLODSignificance = Representation.PrevLODSignificance;
				
			if (FMassLODSignificanceRange* NewRange = ISMInfo.GetLODSignificanceRange(RepresentationLOD.LODSignificance))
			{
				if (ISMInfo.ShouldUseTransformOffset())
				{
					const FTransform& TransformOffset = ISMInfo.GetTransformOffset();
					const FTransform SMTransform = TransformOffset * Transform;
					NewRange->AddInstance(EntityHandle, SMTransform);
				}
				else
				{
					NewRange->AddInstance(EntityHandle, Transform);
				}
			}

			// consume "prev" data
			// @note crazy hacky, but we don't want to consume if bDoKeepActorExtraFrame is true. In that case 
			// UMassRepresentationProcessor::UpdateRepresentation expects the "prev" state not to be consumed 
			// a frame longer so that it can do the consuming (and call "disable actor").
			if (bDoKeepActorExtraFrame == false)
			{
				Representation.PrevRepresentation = Representation.CurrentRepresentation;
			}

			SignalSubsystem.SignalEntity(UE::Mass::Signals::SwitchedToISM, EntityHandle);
		}
		else if (ISMInfo.GetLODSignificanceRangesNum() > 1 && Representation.PrevLODSignificance != RepresentationLOD.LODSignificance)
		{
			// we remain in ISM land, but LODSignificance changed and we have multiple LODSignificance ranges for this entity
			FMassLODSignificanceRange* OldRange = ISMInfo.GetLODSignificanceRange(Representation.PrevLODSignificance);
			FMassLODSignificanceRange* NewRange = ISMInfo.GetLODSignificanceRange(RepresentationLOD.LODSignificance);
			if (OldRange != NewRange)
			{
				if (OldRange)
				{
					OldRange->RemoveInstance(EntityHandle);
				}
				if (NewRange)
				{
					const FTransform& Transform = TransformFragment.GetTransform();
					NewRange->AddInstance(EntityHandle, Transform);
				}
			}
		}

		// consume "prev" data
		Representation.PrevLODSignificance = RepresentationLOD.LODSignificance;
	}
}
