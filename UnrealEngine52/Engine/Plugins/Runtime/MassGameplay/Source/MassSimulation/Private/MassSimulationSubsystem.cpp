// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassSimulationSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassExecutor.h"
#include "MassSimulationSettings.h"
#include "VisualLogger/VisualLogger.h"
#include "MassEntitySettings.h"
#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY(LogMassSim);

namespace UE::MassSimulation
{
	int32 bDoEntityCompaction = 1;
	FAutoConsoleVariableRef CVarEntityCompaction(TEXT("ai.mass.EntityCompaction"), bDoEntityCompaction, TEXT("Maximize the nubmer of entities per chunk"), ECVF_Cheat);
}

//----------------------------------------------------------------------//
// UMassSimulationSubsystem
//----------------------------------------------------------------------//
UMassSimulationSubsystem::UMassSimulationSubsystem(const FObjectInitializer& ObjectInitializer)
	: Super()
{
}

void UMassSimulationSubsystem::BeginDestroy()
{
#if WITH_EDITOR
	if (PieBeginEventHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(PieBeginEventHandle);
	}
	if (PieEndedEventHandle.IsValid())
	{
		FEditorDelegates::PrePIEEnded.Remove(PieEndedEventHandle);
	}
	if (MassEntitySettingsChangeHandle.IsValid())
	{
		if (UMassEntitySettings* Settings = GetMutableDefault<UMassEntitySettings>())
		{
			Settings->GetOnSettingsChange().Remove(MassEntitySettingsChangeHandle);
		}
	}
#endif //  WITH_EDITOR

	if (bSimulationStarted)
	{
		check(EntityManager);
		StopSimulation();
	}

	Super::BeginDestroy();
}

FMassProcessingPhase::FOnPhaseEvent& UMassSimulationSubsystem::GetOnProcessingPhaseStarted(const EMassProcessingPhase Phase)
{
	return PhaseManager.GetOnPhaseStart(Phase);
}

FMassProcessingPhase::FOnPhaseEvent& UMassSimulationSubsystem::GetOnProcessingPhaseFinished(const EMassProcessingPhase Phase)
{
	return PhaseManager.GetOnPhaseEnd(Phase);
}

bool UMassSimulationSubsystem::IsDuringMassProcessing() const 
{
	return EntityManager.IsValid() && EntityManager->IsProcessing();
}

void UMassSimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UMassEntitySubsystem* EntitySubsystem = Collection.InitializeDependency<UMassEntitySubsystem>();
	check(EntitySubsystem);
	EntityManager = EntitySubsystem->GetMutableEntityManager().AsShared();
	
	GetOnProcessingPhaseStarted(EMassProcessingPhase::PrePhysics).AddUObject(this, &UMassSimulationSubsystem::OnProcessingPhaseStarted, EMassProcessingPhase::PrePhysics);
}

void UMassSimulationSubsystem::Deinitialize()
{
	GetOnProcessingPhaseStarted(EMassProcessingPhase::PrePhysics).RemoveAll(this);
	StopSimulation();

	PhaseManager.Deinitialize();
	EntityManager.Reset();

	Super::Deinitialize();
}

void UMassSimulationSubsystem::PostInitialize()
{
	Super::PostInitialize();

#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (GEditor && World != nullptr && !World->IsGameWorld())
	{
		// in editor worlds we need to rebuild the pipeline at this point since OnWorldBeginPlay won't be called
		RebuildTickPipeline();

		PieBeginEventHandle = FEditorDelegates::BeginPIE.AddUObject(this, &UMassSimulationSubsystem::OnPieBegin);
		PieEndedEventHandle = FEditorDelegates::PrePIEEnded.AddUObject(this, &UMassSimulationSubsystem::OnPieEnded);

		UMassEntitySettings* Settings = GetMutableDefault<UMassEntitySettings>();
		check(Settings);
		MassEntitySettingsChangeHandle = Settings->GetOnSettingsChange().AddUObject(this, &UMassSimulationSubsystem::OnMassEntitySettingsChange);

		// note that this starts ticking for the editor world
		StartSimulation(*World);
	}
#endif // WITH_EDITOR
}

void UMassSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	// To evaluate the effective processors execution mode, we need to wait on OnWorldBeginPlay before calling
	// RebuildTickPipeline as we are sure by this time the network is setup correctly.
	RebuildTickPipeline();
	// note that since we're in this function we're tied to a game world. This means the StartSimulation in 
	// PostInitialize haven't been called.
	StartSimulation(InWorld);
}

void UMassSimulationSubsystem::RegisterDynamicProcessor(UMassProcessor& Processor)
{
	checkf(!IsDuringMassProcessing(), TEXT("Unable to add dynamic processors to Mass during processing."));
	PhaseManager.RegisterDynamicProcessor(Processor);
}

void UMassSimulationSubsystem::UnregisterDynamicProcessor(UMassProcessor& Processor)
{
	checkf(!IsDuringMassProcessing(), TEXT("Unable to remove dynamic processors to Mass during processing."));
	PhaseManager.UnregisterDynamicProcessor(Processor);
}

void UMassSimulationSubsystem::RebuildTickPipeline()
{
	TConstArrayView<FMassProcessingPhaseConfig> ProcessingPhasesConfig = GET_MASS_CONFIG_VALUE(GetProcessingPhasesConfig());
	FString DependencyGraphFileName;

#if WITH_EDITOR
	const UWorld* World = GetWorld();
	const UMassEntitySettings* Settings = GetMutableDefault<UMassEntitySettings>();
	if (World != nullptr && Settings != nullptr && !Settings->DumpDependencyGraphFileName.IsEmpty())
	{
		DependencyGraphFileName = FString::Printf(TEXT("%s_%s"), *Settings->DumpDependencyGraphFileName, *ToString(World->GetNetMode()));
	}
#endif // WITH_EDITOR

	PhaseManager.Initialize(*this, ProcessingPhasesConfig, DependencyGraphFileName);
}

void UMassSimulationSubsystem::StartSimulation(UWorld& InWorld)
{
	PhaseManager.Start(InWorld);

	bSimulationStarted = true;

	OnSimulationStarted.Broadcast(&InWorld);
}

void UMassSimulationSubsystem::StopSimulation()
{
	PhaseManager.Stop();

	bSimulationStarted = false;
}

void UMassSimulationSubsystem::OnProcessingPhaseStarted(const float DeltaSeconds, const EMassProcessingPhase Phase) const
{
	switch (Phase)
	{
		case EMassProcessingPhase::PrePhysics:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(DoEntityCompation);
				check(EntityManager);
				if (UE::MassSimulation::bDoEntityCompaction)
				{
					EntityManager->DoEntityCompaction(GET_MASSSIMULATION_CONFIG_VALUE(DesiredEntityCompactionTimeSlicePerTick));
				}
			}
			break;
		default:
			// unhandled phases, by design, not every phase needs to be handled by the Actor subsystem
			break;
	}
}


#if WITH_EDITOR
void UMassSimulationSubsystem::OnPieBegin(const bool bIsSimulation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		// called so that we're not processing phases for the editor world while PIE/SIE is running
		StopSimulation();
	}
}

void UMassSimulationSubsystem::OnPieEnded(const bool bIsSimulation)
{
	UWorld* World = GetWorld();
	if (World && !bSimulationStarted)
	{
		// Resume processing phases in the editor world.
		StartSimulation(*World);
	}
}

void UMassSimulationSubsystem::OnMassEntitySettingsChange(const FPropertyChangedEvent& PropertyChangedEvent)
{
	RebuildTickPipeline();
}
#endif // WITH_EDITOR