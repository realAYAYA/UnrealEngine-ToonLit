// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "MassProcessingTypes.h"
#include "MassProcessingPhaseManager.h"
#include "MassSubsystemBase.h"
#include "MassSimulationSubsystem.generated.h"


struct FMassEntityManager;
class IConsoleVariable;

DECLARE_LOG_CATEGORY_EXTERN(LogMassSim, Log, All);

UCLASS(config = Game, defaultconfig)
class MASSSIMULATION_API UMassSimulationSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSimulationStarted, UWorld* /*World*/);
	
	UMassSimulationSubsystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	const FMassProcessingPhaseManager& GetPhaseManager() const { return PhaseManager; }

	FMassProcessingPhase::FOnPhaseEvent& GetOnProcessingPhaseStarted(const EMassProcessingPhase Phase);
	FMassProcessingPhase::FOnPhaseEvent& GetOnProcessingPhaseFinished(const EMassProcessingPhase Phase);
	static FOnSimulationStarted& GetOnSimulationStarted() { return OnSimulationStarted; }

	void RegisterDynamicProcessor(UMassProcessor& Processor);
	void UnregisterDynamicProcessor(UMassProcessor& Processor);

	bool IsSimulationStarted() const { return bSimulationStarted; }

	/** @return whether hosted EntityManager is currently, actively being used for processing purposes. Equivalent to calling FMassEntityManager.IsProcessing() */
	bool IsDuringMassProcessing() const;

	/** Starts/stops simulation ticking for all worlds, based on new `mass.SimulationTickingEnabled` cvar value */
	static void HandleSimulationTickingEnabledCVarChange(IConsoleVariable*);

protected:
	// UWorldSubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	// UWorldSubsystem END
	virtual void BeginDestroy() override;
	
	void RebuildTickPipeline();

	void StartSimulation(UWorld& InWorld);
	void StopSimulation();

	void OnProcessingPhaseStarted(const float DeltaSeconds, const EMassProcessingPhase Phase) const;

#if WITH_EDITOR
	void OnPieBegin(const bool bIsSimulation);
	void OnPieEnded(const bool bIsSimulation);
	void OnMassEntitySettingsChange(const FPropertyChangedEvent& PropertyChangedEvent);
#endif // WITH_EDITOR

protected:

	TSharedPtr<FMassEntityManager> EntityManager;

	FMassProcessingPhaseManager PhaseManager;

	inline static FOnSimulationStarted OnSimulationStarted={};

	UPROPERTY()
	FMassRuntimePipeline RuntimePipeline;

	float CurrentDeltaSeconds = 0.f;
	bool bTickInProgress = false;
	bool bSimulationStarted = false;

#if WITH_EDITOR
	FDelegateHandle PieBeginEventHandle;
	FDelegateHandle PieEndedEventHandle;

	FDelegateHandle MassEntitySettingsChangeHandle;
#endif // WITH_EDITOR
};
