// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassDebuggerSubsystem.h"
#include "EngineUtils.h"
#include "MassCommonTypes.h"
#include "MassSimulationSubsystem.h"
#include "MassDebugVisualizationComponent.h"
#include "MassEntityManager.h"
#include "MassDebugVisualizer.h"
#include "MassDebugger.h"
#include "MassEntityManager.h"


void UMassDebuggerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMassSimulationSubsystem::StaticClass());
	Super::Initialize(Collection);

	UMassSimulationSubsystem* SimSystem = UWorld::GetSubsystem<UMassSimulationSubsystem>(GetWorld());
	check(SimSystem);
	SimSystem->GetOnProcessingPhaseStarted(EMassProcessingPhase::PrePhysics).AddUObject(this, &UMassDebuggerSubsystem::OnProcessingPhaseStarted);

#if WITH_MASSENTITY_DEBUG
	OnEntitySelectedHandle = FMassDebugger::OnEntitySelectedDelegate.AddUObject(this, &UMassDebuggerSubsystem::OnEntitySelected);
#endif // WITH_MASSENTITY_DEBUG
}

void UMassDebuggerSubsystem::Deinitialize()
{
#if WITH_MASSENTITY_DEBUG
	FMassDebugger::OnEntitySelectedDelegate.Remove(OnEntitySelectedHandle);
#endif // WITH_MASSENTITY_DEBUG
}

#if WITH_MASSENTITY_DEBUG
void UMassDebuggerSubsystem::OnEntitySelected(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle)
{
	if (EntityManager.GetWorld() == GetWorld())
	{
		SetSelectedEntity(EntityHandle);
	}
}
#endif // WITH_MASSENTITY_DEBUG

void UMassDebuggerSubsystem::PreTickProcessors()
{
	// get ready to receive new debug info
	for (TArray<FShapeDesc>& Array : Shapes)
	{
		Array.Reset();
	}
	
	Entities.Reset();
	Locations.Reset();
	SelectedEntityDetails.Empty();
}

void UMassDebuggerSubsystem::OnProcessingPhaseStarted(const float DeltaSeconds)
{
	PreTickProcessors();
}

void UMassDebuggerSubsystem::SetSelectedEntity(const FMassEntityHandle InSelectedEntity)
{
	SelectedEntity = InSelectedEntity;
	SelectedEntityDetails.Empty();
}

void UMassDebuggerSubsystem::AppendSelectedEntityInfo(const FString& Info)
{
	SelectedEntityDetails += Info;
}

UMassDebugVisualizationComponent* UMassDebuggerSubsystem::GetVisualizationComponent()
{ 
#if WITH_EDITORONLY_DATA
	if (VisualizationComponent == nullptr)
	{
		if (!ensureMsgf(DebugVisualizer == nullptr, TEXT("If we do have a DebugVisualizer but don't have VisualizationComponent then somethin's wrong")))
		{
			VisualizationComponent = &DebugVisualizer->GetDebugVisComponent();
		}
		else
		{
			if (UWorld* World = GetWorld())
			{
				const AMassDebugVisualizer& VisualizerActor = GetOrSpawnDebugVisualizer(*World);
				VisualizationComponent = &VisualizerActor.GetDebugVisComponent();
			}
		}
	}
	ensureMsgf(VisualizationComponent,  TEXT("In editor builds we always expect to have a visualizer component available"));
#endif // WITH_EDITORONLY_DATA
	return VisualizationComponent;
}

#if WITH_EDITORONLY_DATA
AMassDebugVisualizer& UMassDebuggerSubsystem::GetOrSpawnDebugVisualizer(UWorld& InWorld)
{
	if (DebugVisualizer)
	{
		return *DebugVisualizer;
	}

	// see if there is one already and we've missed it somehow
	for (const TActorIterator<AMassDebugVisualizer> It(&InWorld); It;)
	{
		DebugVisualizer = *It;
		return *DebugVisualizer;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// The helper actor is created on demand and only once per world so we can allow it to spawn during construction script.
	SpawnInfo.bAllowDuringConstructionScript = true;
	DebugVisualizer = InWorld.SpawnActor<AMassDebugVisualizer>(SpawnInfo);
	check(DebugVisualizer);
	VisualizationComponent = &DebugVisualizer->GetDebugVisComponent();

	return *DebugVisualizer;
}
#endif