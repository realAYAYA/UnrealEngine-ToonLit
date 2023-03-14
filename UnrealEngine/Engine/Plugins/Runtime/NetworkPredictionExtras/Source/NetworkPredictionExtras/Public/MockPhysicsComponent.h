// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseMovementComponent.h"
#include "Templates/PimplPtr.h"
#include "MockPhysicsSimulation.h" // For FMockPhysicsInputCmd only for simplified input
#include "MockPhysicsComponent.generated.h"

struct FMockPhysicsInputCmd;
struct FMockPhysicsAuxState;
struct FMockPhysicsJumpCue;

class FMockPhysicsSimulation;

// -------------------------------------------------------------------------------------------------------------------------------
// ActorComponent for running MockPhysicsSimulation
// -------------------------------------------------------------------------------------------------------------------------------

UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class NETWORKPREDICTIONEXTRAS_API UMockPhysicsComponent : public UBaseMovementComponent
{
	GENERATED_BODY()

public:

	UMockPhysicsComponent();

	// Forward input producing event to someone else (probably the owning actor)
	DECLARE_DELEGATE_TwoParams(FProduceMockPhysicsInput, const int32 /*SimTime*/, FMockPhysicsInputCmd& /*Cmd*/)
	FProduceMockPhysicsInput ProduceInputDelegate;

	// Next local InputCmd that will be submitted. This is just one way to do it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=INPUT)
	FMockPhysicsInputCmd PendingInputCmd;

	// --------------------------------------------------------------------------------
	// NP Driver
	// --------------------------------------------------------------------------------

	// Get latest local input prior to simulation step
	void ProduceInput(const int32 DeltaTimeMS, FMockPhysicsInputCmd* Cmd);

	// Take output for simulation
	void FinalizeFrame(const void* SyncState, const FMockPhysicsAuxState* AuxState);

	// Seed initial values based on component's state
	void InitializeSimulationState(const void* Sync, FMockPhysicsAuxState* Aux);


	// --------------------------------------------------------------------------------
	// Cues: cosmetic events emitted from simulation code
	// --------------------------------------------------------------------------------

	// Assignable delegates chosen so that owning actor can implement in BPs. May not be the best choice for all cases.
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPhysicsJumpCueEvent, FVector, Location, float, ElapsedTimeSeconds);
	UPROPERTY(BlueprintAssignable, Category="Mock Physics Cues")
	FPhysicsJumpCueEvent OnJumpActivatedEvent;

	void HandleCue(const FMockPhysicsJumpCue& JumpCue, const FNetSimCueSystemParamemters& SystemParameters);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPhysicsChargeCueEvent, FVector, Location, float, ElapsedTimeSeconds);
	UPROPERTY(BlueprintAssignable, Category="Mock Physics Cues")
	FPhysicsChargeCueEvent OnChargeActivatedEvent;
	
	void HandleCue(const FMockPhysicsChargeCue& ChargeCue, const FNetSimCueSystemParamemters& SystemParameters);


	// Charge (not a Cue event, just state)
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMockPhysicsNotifyStateChange, bool, bNewStateValue);
	
	UPROPERTY(BlueprintAssignable, Category="Mock Physics Cues")
	FMockPhysicsNotifyStateChange OnChargeStateChange;

	// Currently charging up charge attack
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Mock Physics")
	bool bIsCharging;

protected:

	// Network Prediction
	virtual void InitializeNetworkPredictionProxy() override;
	TPimplPtr<FMockPhysicsSimulation> OwnedSimulation; // If we instantiate the sim in InitializeNetworkPredictionProxy, its stored here
	FMockPhysicsSimulation* ActiveSimulation = nullptr; // The sim driving us, set in InitFlyingMovementSimulation. Could be child class that implements InitializeNetworkPredictionProxy.

	void InitMockPhysicsSimulation(FMockPhysicsSimulation* Simulation);
};