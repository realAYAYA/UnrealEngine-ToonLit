// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStationaryISMRepresentationFragmentDestructor.h"
#include "MassRepresentationSubsystem.h"
#include "MassCommonFragments.h"
#include "MassRepresentationProcessor.h"

//-----------------------------------------------------------------------------
// UMassStationaryISMRepresentationFragmentDestructor
//-----------------------------------------------------------------------------
UMassStationaryISMRepresentationFragmentDestructor::UMassStationaryISMRepresentationFragmentDestructor()
	: EntityQuery(*this)
{
	ObservedType = FMassRepresentationFragment::StaticStruct();
	Operation = EMassObservedOperation::Remove;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bRequiresGameThreadExecution = true; // not sure about this
}

void UMassStationaryISMRepresentationFragmentDestructor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassRepresentationParameters>();
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassStaticRepresentationTag>(EMassFragmentPresence::All);
}

void UMassStationaryISMRepresentationFragmentDestructor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem = Context.GetMutableSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);
		FMassInstancedStaticMeshInfoArrayView ISMInfosView = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();

		const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();

		const int32 NumEntities = Context.GetNumEntities();
		for (int32 EntityIdx = 0; EntityIdx < NumEntities; ++EntityIdx)
		{
			FMassRepresentationFragment& Representation = RepresentationList[EntityIdx];
			if (Representation.CurrentRepresentation == EMassRepresentationType::StaticMeshInstance)
			{
				FMassInstancedStaticMeshInfo& ISMInfo = ISMInfosView[Representation.StaticMeshDescHandle.ToIndex()];
				if (FMassLODSignificanceRange* OldRange = ISMInfo.GetLODSignificanceRange(Representation.PrevLODSignificance))
				{
					const FMassEntityHandle EntityHandle = Context.GetEntity(EntityIdx);
					if (OldRange)
					{
						OldRange->RemoveInstance(EntityHandle);
					}
				}
				Representation.CurrentRepresentation = EMassRepresentationType::None;
			}
		}
	});
}
