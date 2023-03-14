// Copyright Epic Games, Inc. All Rights Reserved.

#include "DebugVisLocationProcessor.h"
#include "MassDebuggerSubsystem.h"
#include "MassDebugVisualizationComponent.h"
#include "MassCommonFragments.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h"

//----------------------------------------------------------------------//
// UDebugVisLocationProcessor
//----------------------------------------------------------------------//
UDebugVisLocationProcessor::UDebugVisLocationProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
	bRequiresGameThreadExecution = true; // due to UMassDebuggerSubsystem access
}

void UDebugVisLocationProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FSimDebugVisFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassDebuggableTag>(EMassFragmentPresence::All);
	EntityQuery.AddSubsystemRequirement<UMassDebuggerSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UDebugVisLocationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if WITH_EDITORONLY_DATA
	QUICK_SCOPE_CYCLE_COUNTER(DebugVisLocationProcessor_Run);

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
	{
		UMassDebuggerSubsystem& Debugger = Context.GetMutableSubsystemChecked<UMassDebuggerSubsystem>(World);
		UMassDebugVisualizationComponent* Visualizer = Debugger.GetVisualizationComponent();
		check(Visualizer);
		TArrayView<UHierarchicalInstancedStaticMeshComponent*> VisualDataISMCs = Visualizer->GetVisualDataISMCs();
		if (VisualDataISMCs.Num() > 0)
		{
			const int32 NumEntities = Context.GetNumEntities();
			const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FSimDebugVisFragment> DebugVisList = Context.GetFragmentView<FSimDebugVisFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				const FSimDebugVisFragment& VisualComp = DebugVisList[i];

				// @todo: remove this code once the asset is exported with correct alignment SM_Mannequin.uasset
				FTransform SMTransform = LocationList[i].GetTransform();
				FQuat FromEngineToSM(FVector::UpVector, -HALF_PI);
				SMTransform.SetRotation(FromEngineToSM * SMTransform.GetRotation());

				VisualDataISMCs[VisualComp.VisualType]->UpdateInstanceTransform(VisualComp.InstanceIndex, SMTransform, true);
			}
		}
		else
		{
			UE_LOG(LogMassDebug, Log, TEXT("UDebugVisLocationProcessor: Trying to update InstanceStaticMeshes while none created. Check your debug visualization setup"));
		}
	});

	UMassDebuggerSubsystem* Debugger = UWorld::GetSubsystem<UMassDebuggerSubsystem>(EntityManager.GetWorld());
	if (ensure(Debugger))
	{
		Debugger->GetVisualizationComponent()->DirtyVisuals();
	}
#endif // WITH_EDITORONLY_DATA
}

//----------------------------------------------------------------------//
//  UMassProcessor_UpdateDebugVis
//----------------------------------------------------------------------//
UMassProcessor_UpdateDebugVis::UMassProcessor_UpdateDebugVis()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
	bRequiresGameThreadExecution = true; // due to UMassDebuggerSubsystem access
}

void UMassProcessor_UpdateDebugVis::ConfigureQueries() 
{
	// @todo only FDataFragment_DebugVis should be mandatory, rest optional 
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FDataFragment_DebugVis>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassDebuggableTag>(EMassFragmentPresence::All);
	EntityQuery.AddSubsystemRequirement<UMassDebuggerSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassProcessor_UpdateDebugVis::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassProcessor_UpdateDebugVis_Run);

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
		{
			UMassDebuggerSubsystem& Debugger = Context.GetMutableSubsystemChecked<UMassDebuggerSubsystem>(World);

			const int32 NumEntities = Context.GetNumEntities();
			const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
			const TArrayView<FDataFragment_DebugVis> DebugVisList = Context.GetMutableFragmentView<FDataFragment_DebugVis>();
			const TArrayView<FAgentRadiusFragment> RadiiList = Context.GetMutableFragmentView<FAgentRadiusFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				Debugger.AddShape(DebugVisList[i].Shape, LocationList[i].GetTransform().GetLocation(), RadiiList[i].Radius);
			}
		});
}
