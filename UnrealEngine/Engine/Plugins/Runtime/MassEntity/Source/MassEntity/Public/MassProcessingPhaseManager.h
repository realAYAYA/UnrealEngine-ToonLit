// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "UObject/GCObject.h"
#include "Engine/EngineBaseTypes.h"
#include "MassProcessingTypes.h"
#include "MassProcessor.h"
#include "MassProcessorDependencySolver.h"
#include "MassProcessingPhaseManager.generated.h"


struct FMassProcessingPhaseManager;
class UMassProcessor;
class UMassCompositeProcessor;
struct FMassEntityManager;
struct FMassCommandBuffer;
struct FMassProcessingPhaseConfig;


USTRUCT()
struct MASSENTITY_API FMassProcessingPhaseConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Mass, config)
	FName PhaseName;

	UPROPERTY(EditAnywhere, Category = Mass, config, NoClear)
	TSubclassOf<UMassCompositeProcessor> PhaseGroupClass = UMassCompositeProcessor::StaticClass();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMassProcessor>> ProcessorCDOs;

#if WITH_EDITORONLY_DATA
	// this processor is available only in editor since it's used to present the user the order in which processors
	// will be executed when given processing phase gets triggered
	UPROPERTY(Transient)
	TObjectPtr<UMassCompositeProcessor> PhaseProcessor = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Mass, Transient)
	FText Description;
#endif //WITH_EDITORONLY_DATA
};


struct MASSENTITY_API FMassProcessingPhase : public FTickFunction
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPhaseEvent, const float /*DeltaSeconds*/);

	FMassProcessingPhase();
	FMassProcessingPhase(const FMassProcessingPhase& Other) = delete;
	FMassProcessingPhase& operator=(const FMassProcessingPhase& Other) = delete;

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

	bool ShouldTick(const ELevelTick TickType) const { return SupportedTickTypes & (1 << TickType); }

public:
	void Initialize(FMassProcessingPhaseManager& InPhaseManager, const EMassProcessingPhase InPhase, const ETickingGroup InTickGroup, UMassCompositeProcessor& InPhaseProcessor);
	void AddSupportedTickType(const ELevelTick TickType) { SupportedTickTypes |= (1 << TickType); }
	void RemoveSupportedTickType(const ELevelTick TickType) { SupportedTickTypes &= ~(1 << TickType); }

protected:
	friend FMassProcessingPhaseManager;

	// composite processor representing work to be performed. GC-referenced via AddReferencedObjects
	TObjectPtr<UMassCompositeProcessor> PhaseProcessor = nullptr;

	EMassProcessingPhase Phase = EMassProcessingPhase::MAX;
	FOnPhaseEvent OnPhaseStart;
	FOnPhaseEvent OnPhaseEnd;

private:
	FMassProcessingPhaseManager* PhaseManager = nullptr;
	std::atomic<bool> bIsDuringMassProcessing = false;
	bool bRunInParallelMode = false;
	uint8 SupportedTickTypes = 0;
};


struct MASSENTITY_API FMassPhaseProcessorConfigurationHelper
{
	FMassPhaseProcessorConfigurationHelper(UMassCompositeProcessor& InOutPhaseProcessor, const FMassProcessingPhaseConfig& InPhaseConfig, UObject& InProcessorOuter, EMassProcessingPhase InPhase)
		: PhaseProcessor(InOutPhaseProcessor), PhaseConfig(InPhaseConfig), ProcessorOuter(InProcessorOuter), Phase(InPhase)
	{
	}

	/** 
	 * @param InWorldExecutionFlags - provide EProcessorExecutionFlags::None to let underlying code decide
	 */
	void Configure(TArrayView<UMassProcessor* const> DynamicProcessors, EProcessorExecutionFlags InWorldExecutionFlags
		, const TSharedPtr<FMassEntityManager>& EntityManager = TSharedPtr<FMassEntityManager>()
		, FMassProcessorDependencySolver::FResult* OutOptionalResult = nullptr);

	UMassCompositeProcessor& PhaseProcessor;
	const FMassProcessingPhaseConfig& PhaseConfig;
	UObject& ProcessorOuter;
	EMassProcessingPhase Phase;
	bool bInitializeCreatedProcessors = true;
	bool bIsGameRuntime = true;

	UE_DEPRECATED(5.2, "This flavor of Configure has been deprecated. Use the one requiring the first parameter to be an array view of additional processors")
	void Configure(const TSharedPtr<FMassEntityManager>& EntityManager = TSharedPtr<FMassEntityManager>(), FMassProcessorDependencySolver::FResult* OutOptionalResult = nullptr)
	{
		Configure({}, EProcessorExecutionFlags::None, EntityManager, OutOptionalResult);
	}

	UE_DEPRECATED(5.3, "This flavor of Configure has been deprecated. Use the one requiring the first parameter to be an array view of additional processors and EProcessorExecutionFlags to be provided")
	void Configure(TArrayView<UMassProcessor* const> DynamicProcessors, const TSharedPtr<FMassEntityManager>& EntityManager = TSharedPtr<FMassEntityManager>(),
		FMassProcessorDependencySolver::FResult* OutOptionalResult = nullptr)
	{
		Configure(DynamicProcessors, EProcessorExecutionFlags::None, EntityManager, OutOptionalResult);
	}
};

/** 
 * MassProcessingPhaseManager owns separate FMassProcessingPhase instances for every ETickingGroup. When activated
 * via Start function it registers and enables the FMassProcessingPhase instances which themselves are tick functions 
 * that host UMassCompositeProcessor which they trigger as part of their Tick function. 
 * MassProcessingPhaseManager serves as an interface to said FMassProcessingPhase instances and allows initialization
 * with collections of processors (via Initialize function) as well as registering arbitrary functions to be called 
 * when a particular phase starts or ends (via GetOnPhaseStart and GetOnPhaseEnd functions). 
 */
struct MASSENTITY_API FMassProcessingPhaseManager : public FGCObject
{
public:
	explicit FMassProcessingPhaseManager(EProcessorExecutionFlags InProcessorExecutionFlags = EProcessorExecutionFlags::None) 
		: ProcessorExecutionFlags(InProcessorExecutionFlags)
	{}
	FMassProcessingPhaseManager(const FMassProcessingPhaseManager& Other) = delete;
	FMassProcessingPhaseManager& operator=(const FMassProcessingPhaseManager& Other) = delete;

	const TSharedPtr<FMassEntityManager>& GetEntityManager() { return EntityManager; }
	FMassEntityManager& GetEntityManagerRef() { check(EntityManager); return *EntityManager.Get(); }

	/** Retrieves OnPhaseStart multicast delegate's reference for a given Phase */
	FMassProcessingPhase::FOnPhaseEvent& GetOnPhaseStart(const EMassProcessingPhase Phase) { return ProcessingPhases[uint8(Phase)].OnPhaseStart; } //-V557
	/** Retrieves OnPhaseEnd multicast delegate's reference for a given Phase */
	FMassProcessingPhase::FOnPhaseEvent& GetOnPhaseEnd(const EMassProcessingPhase Phase) { return ProcessingPhases[uint8(Phase)].OnPhaseEnd; }

	/** 
	 *  Populates hosted FMassProcessingPhase instances with Processors read from MassEntitySettings configuration.
	 *  Calling this function overrides previous configuration of Phases.
	 */
	void Initialize(UObject& InOwner, TConstArrayView<FMassProcessingPhaseConfig> ProcessingPhasesConfig, const FString& DependencyGraphFileName = TEXT(""));

	/** Needs to be called before destruction, ideally before owner's BeginDestroy (a FGCObject's limitation) */
	void Deinitialize();

	const FGraphEventRef& TriggerPhase(const EMassProcessingPhase Phase, const float DeltaTime, const FGraphEventRef& MyCompletionGraphEvent);

	/** 
	 *  Stores EntityManager associated with given world's MassEntitySubsystem and kicks off phase ticking.
	 */
	void Start(UWorld& World);
	
	/**
	 *  Stores InEntityManager as the entity manager. It also kicks off phase ticking if the given InEntityManager is tied to a UWorld.
	 */
	void Start(const TSharedPtr<FMassEntityManager>& InEntityManager);
	void Stop();
	bool IsRunning() const { return EntityManager.IsValid(); }

	FString GetName() const;

	/** Registers a dynamic processor. This needs to be a fully formed processor and will be slotted in during the next tick. */
	void RegisterDynamicProcessor(UMassProcessor& Processor);
	/** Removes a previously registered dynamic processor of throws an assert if not found. */
	void UnregisterDynamicProcessor(UMassProcessor& Processor);

protected:
	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FMassProcessingPhaseManager");
	}
	// End of FGCObject interface

	void EnableTickFunctions(const UWorld& World);

	/** Creates phase processors instances for each declared phase name, based on MassEntitySettings */
	void CreatePhases();

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

	void OnNewArchetype(const FMassArchetypeHandle& NewArchetype);

protected:	
	struct FPhaseGraphBuildState
	{
		FMassProcessorDependencySolver::FResult LastResult;
		bool bNewArchetypes = true;
		bool bProcessorsNeedRebuild = true;
		bool bInitialized = false;
	};

	FMassProcessingPhase ProcessingPhases[(uint8)EMassProcessingPhase::MAX];
	FPhaseGraphBuildState ProcessingGraphBuildStates[(uint8)EMassProcessingPhase::MAX];
	TArray<FMassProcessingPhaseConfig> ProcessingPhasesConfig;
	TArray<TObjectPtr<UMassProcessor>> DynamicProcessors;

	TSharedPtr<FMassEntityManager> EntityManager;

	EMassProcessingPhase CurrentPhase = EMassProcessingPhase::MAX;

	TWeakObjectPtr<UObject> Owner;

	FDelegateHandle OnNewArchetypeHandle;

	EProcessorExecutionFlags ProcessorExecutionFlags = EProcessorExecutionFlags::None;
	bool bIsAllowedToTick = false;
};
