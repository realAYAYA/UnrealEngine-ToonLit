// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "MassProcessingPhaseManager.h"
#include "MassEntityEditorSubsystem.generated.h"


struct FMassEntityManager;
namespace UE::Mass
{
	struct FMassEditorTickFunction;
}

UCLASS()
class MASSENTITYEDITOR_API UMassEntityEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreTick, float /*DeltaTime*/);

	UMassEntityEditorSubsystem();
	~UMassEntityEditorSubsystem();

	FMassEntityManager& GetMutableEntityManager() { return EntityManager.Get(); }

	FOnPreTick& GetOnPreTickDelegate() { return OnPreTickDelegate; }

	/** Registers a dynamic processor. This needs to be a fully formed processor and will be slotted in during the next tick. */
	void RegisterDynamicProcessor(UMassProcessor& Processor);
	/** Removes a previously registered dynamic processor of throws an assert if not found. */
	void UnregisterDynamicProcessor(UMassProcessor& Processor);

protected:
	// USubsystem interface begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// USubsystem interface end
	void StopAndCleanUp();

	friend UE::Mass::FMassEditorTickFunction;
	void Tick(float DeltaTime);

	UE::Mass::FMassEditorTickFunction* EditorTickFunction = nullptr;

	TSharedRef<FMassEntityManager> EntityManager;

	TSharedRef<FMassProcessingPhaseManager> PhaseManager;
	FGraphEventRef CompletionEvent;

	FMassProcessingPhaseConfig ProcessingPhasesConfig[(uint8)EMassProcessingPhase::MAX];

	/** Gets broadcast right before issuing FMassProcessingPhaseManager's phases execution */
	FOnPreTick OnPreTickDelegate;
	
	bool IsProcessing{ false };
};
