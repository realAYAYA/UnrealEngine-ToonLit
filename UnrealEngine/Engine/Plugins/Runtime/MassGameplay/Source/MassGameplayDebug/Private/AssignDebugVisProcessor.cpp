// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssignDebugVisProcessor.h"
#include "MassGameplayDebugTypes.h"
#include "MassDebugVisualizationComponent.h"
#include "MassDebuggerSubsystem.h"
#include "Engine/World.h"

//----------------------------------------------------------------------//
// UAssignDebugVisProcessor
//----------------------------------------------------------------------//
UAssignDebugVisProcessor::UAssignDebugVisProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
	bRequiresGameThreadExecution = true; // due to UMassDebuggerSubsystem
	ObservedType = FSimDebugVisFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
}

void UAssignDebugVisProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FSimDebugVisFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassDebuggerSubsystem>(EMassFragmentAccess::ReadWrite);

	ProcessorRequirements.AddSubsystemRequirement<UMassDebuggerSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UAssignDebugVisProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if WITH_EDITORONLY_DATA
	QUICK_SCOPE_CYCLE_COUNTER(AssignDebugVisProcessor_Execute);
	
	UWorld* World = EntityManager.GetWorld();
	// @todo this code bit is temporary, so is the Visualizer->DirtyVisuals at the end of the function. Will be wrapped in
	// "executable task" once that's implemented. 
	UMassDebugVisualizationComponent* Visualizer = nullptr;
	UMassDebuggerSubsystem& Debugger = Context.GetMutableSubsystemChecked<UMassDebuggerSubsystem>(World);
	check(Debugger.GetVisualizationComponent());
	Visualizer = Debugger.GetVisualizationComponent();
	// note that this function will create the "visual components" only it they're missing or out of sync. 
	Debugger.GetVisualizationComponent()->ConditionallyConstructVisualComponent();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, World](FMassExecutionContext& Context)
	{
		UMassDebuggerSubsystem& Debugger = Context.GetMutableSubsystemChecked<UMassDebuggerSubsystem>(World);
		UMassDebugVisualizationComponent* Visualizer = Debugger.GetVisualizationComponent();
		check(Visualizer);

		const TArrayView<FSimDebugVisFragment> DebugVisList = Context.GetMutableFragmentView<FSimDebugVisFragment>();
		for (FSimDebugVisFragment& VisualComp : DebugVisList)
		{
			// VisualComp.VisualType needs to be assigned by now. Should be performed as part of spawning, copied from the AgentTemplate
			if (ensure(VisualComp.VisualType != INDEX_NONE))
			{
				VisualComp.InstanceIndex = Visualizer->AddDebugVisInstance(VisualComp.VisualType);
			}
		}
	});

	if (ensure(Visualizer))
	{
		Visualizer->DirtyVisuals();
	}
#endif // WITH_EDITORONLY_DATA
}
