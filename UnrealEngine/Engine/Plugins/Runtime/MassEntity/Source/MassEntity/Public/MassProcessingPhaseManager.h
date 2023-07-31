// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "MassProcessingTypes.h"
#include "MassProcessingPhaseManager.generated.h"


class UMassProcessingPhaseManager;
class UMassProcessor;
class UMassCompositeProcessor;
struct FMassEntityManager;
struct FMassCommandBuffer;

USTRUCT()
struct FMassProcessingPhase : public FTickFunction
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPhaseEvent, const float /*DeltaSeconds*/);

	FMassProcessingPhase();

protected:
	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;
	// End of FTickFunction interface

	void OnParallelExecutionDone(const float DeltaTime);

	bool IsConfiguredForParallelMode() const { return bRunInParallelMode; }
	void ConfigureForParallelMode() { bRunInParallelMode = true; }
	void ConfigureForSingleThreadMode() { bRunInParallelMode = false; }

public:
	bool IsDuringMassProcessing() const { return bIsDuringMassProcessing; }

protected:
	friend UMassProcessingPhaseManager;

	UPROPERTY(EditAnywhere, Category=Mass)
	TObjectPtr<UMassCompositeProcessor> PhaseProcessor = nullptr;

	UPROPERTY()
	TObjectPtr<UMassProcessingPhaseManager> PhaseManager = nullptr;

	EMassProcessingPhase Phase = EMassProcessingPhase::MAX;
	FOnPhaseEvent OnPhaseStart;
	FOnPhaseEvent OnPhaseEnd;

private:
	bool bRunInParallelMode = false;
	std::atomic<bool> bIsDuringMassProcessing = false;
};

// It is unsafe to copy FTickFunctions and any subclasses of FTickFunction should specify the type trait WithCopy = false
template<>
struct TStructOpsTypeTraits<FMassProcessingPhase> : public TStructOpsTypeTraitsBase2<FMassProcessingPhase>
{
	enum
	{
		WithCopy = false
	};
};

/** MassProcessingPhaseManager owns separate FMassProcessingPhase instances for every ETickingGroup. When activated
 *  via Start function it registers and enables the FMassProcessingPhase instances which themselves are tick functions 
 *  that host UMassCompositeProcessor which they trigger as part of their Tick function. 
 *  MassProcessingPhaseManager serves as an interface to said FMassProcessingPhase instances and allows initialization
 *  with MassSchematics (via InitializePhases function) as well as registering arbitrary functions to be called 
 *  when a particular phase starts of ends (via GetOnPhaseStart and GetOnPhaseEnd functions). */
UCLASS(Transient, HideCategories = (Tick))
class MASSENTITY_API UMassProcessingPhaseManager : public UObject
{
	GENERATED_BODY()
public:
	FMassEntityManager& GetEntityManagerRef() { check(EntityManager); return *EntityManager.Get(); }

	/** Retrieves OnPhaseStart multicast delegate's reference for a given Phase */
	FMassProcessingPhase::FOnPhaseEvent& GetOnPhaseStart(const EMassProcessingPhase Phase) { return ProcessingPhases[uint8(Phase)].OnPhaseStart; }
	/** Retrieves OnPhaseEnd multicast delegate's reference for a given Phase */
	FMassProcessingPhase::FOnPhaseEvent& GetOnPhaseEnd(const EMassProcessingPhase Phase) { return ProcessingPhases[uint8(Phase)].OnPhaseEnd; }

	/** 
	 *  Populates hosted FMassProcessingPhase instances with Processors read from MassEntitySettings configuration.
	 *  Calling this function overrides previous configuration of Phases.
	 */
	void InitializePhases(UObject& InProcessorOwner);

	/** 
	 *  Both flavors of Start function boil down to setting EntitySubsystem and Executor. If the callee has these 
	 *  at hand it's suggested to use that Start version, otherwise call the World-using one. 
	 */
	void Start(UWorld& World);
	void Start(const TSharedPtr<FMassEntityManager>& InEntityManager);
	void Stop();
	bool IsRunning() const { return EntityManager.IsValid(); }

	/** 
	 *  returns true when called while any of the ProcessingPhases is actively executing its processors. Used to 
	 *  determine whether it's safe to do entity-related operations like adding fragments.
	 *  Note that the function will return false while the OnPhaseStart or OnPhaseEnd are being broadcast,
	 *  the value returned will be `true` only when the entity subsystem is actively engaged 
	 */
	bool IsDuringMassProcessing() const { return CurrentPhase != EMassProcessingPhase::MAX && ProcessingPhases[int(CurrentPhase)].IsDuringMassProcessing(); }

	/**
	 *  Sets the composite processor used for Phase processing phase. Using PhaseProcessor == nullptr results in
	 *  clearing the Phase's processor out effectively making the Phase perform no calculations. 
	 */
	void SetPhaseProcessor(const EMassProcessingPhase Phase, UMassCompositeProcessor* PhaseProcessor);

protected:
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	void EnableTickFunctions(const UWorld& World);

	/** Creates phase processors instances for each declared phase name, based on MassEntitySettings */
	virtual void CreatePhases();

#if WITH_EDITOR
	virtual void OnMassEntitySettingsChange(const FPropertyChangedEvent& PropertyChangedEvent);
#endif // WITH_EDITOR

	friend FMassProcessingPhase;

	/** 
	 *  Called by the given Phase at the very start of its execution function (the FMassProcessingPhase::ExecuteTick),
	 *  even before the FMassProcessingPhase.OnPhaseStart broadcast delegate
	 */
	void OnPhaseStart(const FMassProcessingPhase& Phase);

	/**
	 *  Called by the given Phase at the very end of its execution function (the FMassProcessingPhase::ExecuteTick),
	 *  after the FMassProcessingPhase.OnPhaseEnd broadcast delegate
	 */
	void OnPhaseEnd(FMassProcessingPhase& Phase);

protected:	
	UPROPERTY(VisibleAnywhere, Category=Mass)
	FMassProcessingPhase ProcessingPhases[(uint8)EMassProcessingPhase::MAX];

	TSharedPtr<FMassEntityManager> EntityManager;

	EMassProcessingPhase CurrentPhase = EMassProcessingPhase::MAX;

#if WITH_EDITOR
	FDelegateHandle MassEntitySettingsChangeHandle;
#endif // WITH_EDITOR
};
