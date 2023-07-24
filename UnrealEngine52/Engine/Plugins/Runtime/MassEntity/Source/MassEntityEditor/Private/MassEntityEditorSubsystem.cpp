// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityEditorSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntitySettings.h"
#include "TickableEditorObject.h"

namespace UE::Mass
{
struct FMassEditorPhaseTickTask
{
	FMassEditorPhaseTickTask(const TSharedRef<FMassProcessingPhaseManager>& InPhaseManager, const EMassProcessingPhase InPhase, const float InDeltaTime)
		: PhaseManager(InPhaseManager)
		, Phase(InPhase)
		, DeltaTime(InDeltaTime)
	{
	}

	static TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMassEditorPhaseTickTask, STATGROUP_TaskGraphTasks);
	}

	static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FMassEditorPhaseTickTask);

		PhaseManager->TriggerPhase(Phase, DeltaTime, MyCompletionGraphEvent);
	}
private:
	const TSharedRef<FMassProcessingPhaseManager> PhaseManager;
	const EMassProcessingPhase Phase = EMassProcessingPhase::MAX;
	const float DeltaTime = 0.f;
};

struct FMassEditorTickFunction : FTickableEditorObject
{
	FMassEditorTickFunction(UMassEntityEditorSubsystem& InOwner)
		: Owner(InOwner)
	{}

	//** FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override
	{
		Owner.Tick(DeltaTime);
	}

	virtual ETickableTickType GetTickableTickType() const override 
	{ 
		return ETickableTickType::Always; 
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMassEditorTickFunction, STATGROUP_Tickables);
	}
	//~ END FTickableEditorObject Interface

	UMassEntityEditorSubsystem& Owner;
};

} // namespace UE::Mass

//////////////////////////////////////////////////////////////////////
// UMassEntityEditorSubsystem

UMassEntityEditorSubsystem::UMassEntityEditorSubsystem()
	: EntityManager(MakeShareable(new FMassEntityManager(this)))
	, PhaseManager(new FMassProcessingPhaseManager())
{

}

UMassEntityEditorSubsystem::~UMassEntityEditorSubsystem()
{
	if (EditorTickFunction)
	{
		UE_LOG(LogMass, Warning, TEXT("UMassEntityEditorSubsystem::EditorTickFunction still valid in the destructor indicating that UMassEntityEditorSubsystem::Deinitialize had not been called"));
		StopAndCleanUp();
	}
}

void UMassEntityEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Note that since this is a raw pointer we need to remember to delete it once we're done. It's now done in both 
	// Deinitialize and the destructor (in case Deinitialize call is missed in an unexpected circumstances).
	// Originally we wanted to use a TUniquePtr, but it would require to define the UE::Mass::FMassEditorTickFunction type 
	// in the header file.
	EditorTickFunction = new UE::Mass::FMassEditorTickFunction(*this);

	EntityManager->Initialize();

	// set up ProcessingPhasesConfig
	TConstArrayView<FMassProcessingPhaseConfig> MainPhasesConfig = GET_MASS_CONFIG_VALUE(GetProcessingPhasesConfig());
	for (int PhaseIndex = 0; PhaseIndex < (int)EMassProcessingPhase::MAX; ++PhaseIndex)
	{
		ProcessingPhasesConfig[PhaseIndex] = MainPhasesConfig[PhaseIndex];
	}

	PhaseManager->Initialize(*this, ProcessingPhasesConfig);

	PhaseManager->Start(EntityManager);
}

void UMassEntityEditorSubsystem::Deinitialize()
{
	StopAndCleanUp();
}

void UMassEntityEditorSubsystem::StopAndCleanUp()
{
	if (EditorTickFunction)
	{
		delete EditorTickFunction;
		EditorTickFunction = nullptr;
	}

	PhaseManager->Stop();
}

void UMassEntityEditorSubsystem::Tick(float DeltaTime)
{
	IsProcessing = true;

	if (CompletionEvent.IsValid())
	{
		CompletionEvent->Wait();
	}

	OnPreTickDelegate.Broadcast(DeltaTime);

	for (int PhaseIndex = 0; PhaseIndex < (int)EMassProcessingPhase::MAX; ++PhaseIndex)
	{
		const FGraphEventArray Prerequisites = { CompletionEvent };
		CompletionEvent = TGraphTask<UE::Mass::FMassEditorPhaseTickTask>::CreateTask(&Prerequisites)
			.ConstructAndDispatchWhenReady(PhaseManager, EMassProcessingPhase(PhaseIndex), DeltaTime);
	}

	IsProcessing = false;
}

void UMassEntityEditorSubsystem::RegisterDynamicProcessor(UMassProcessor& Processor)
{
	checkf(!IsProcessing, TEXT("Unable to add dynamic processors to Mass during processing."));
	PhaseManager->RegisterDynamicProcessor(Processor);
}

void UMassEntityEditorSubsystem::UnregisterDynamicProcessor(UMassProcessor& Processor)
{
	checkf(!IsProcessing, TEXT("Unable to remove dynamic processors to Mass during processing."));
	PhaseManager->UnregisterDynamicProcessor(Processor);
}
