// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Backends/MoverBackendLiaison.h"
#include "NetworkPredictionComponent.h"
#include "NetworkPredictionSimulation.h"
#include "MovementMode.h"
#include "MoverTypes.h"

#include "MoverNetworkPredictionLiaison.generated.h"

class UMoverComponent;


using KinematicMoverStateTypes = TNetworkPredictionStateTypes<FMoverInputCmdContext, FMoverSyncState, FMoverAuxStateContext>;

/**
 * MoverNetworkPredictionLiaisonComponent: this component acts as a middleman between an actor's Mover component and the Network Prediction plugin.
 * This class is set on a Mover component as the "back end".
 */
UCLASS()
class MOVER_API UMoverNetworkPredictionLiaisonComponent : public UNetworkPredictionComponent, public IMoverBackendLiaisonInterface
{
	GENERATED_BODY()

public:
	// Begin NP Driver interface
	// Get latest local input prior to simulation step. Called by Network Prediction system on owner's instance (autonomous or authority).
	void ProduceInput(const int32 DeltaTimeMS, FMoverInputCmdContext* Cmd);

	// Restore a previous frame prior to resimulating. Called by Network Prediction system.
	void RestoreFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState);

	// Take output for simulation. Called by Network Prediction system.
	void FinalizeFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState);

	// Seed initial values based on component's state. Called by Network Prediction system.
	void InitializeSimulationState(FMoverSyncState* OutSync, FMoverAuxStateContext* OutAux);

	// Primary movement simulation update. Given an starting state and timestep, produce a new state. Called by Network Prediction system.
	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<KinematicMoverStateTypes>& SimInput, const TNetSimOutput<KinematicMoverStateTypes>& SimOutput);
	// End NP Driver interface

	// IMoverBackendLiaisonInterface
	virtual float GetCurrentSimTimeMs() override;
	virtual int32 GetCurrentSimFrame() override;
	virtual bool ReadPendingSyncState(OUT FMoverSyncState& OutSyncState) override;
	virtual bool WritePendingSyncState(const FMoverSyncState& SyncStateToWrite) override;
	// End IMoverBackendLiaisonInterface

	virtual void BeginPlay() override;

	// UObject interface
	void InitializeComponent() override;
	void OnRegister() override;
	void RegisterComponentTickFunctions(bool bRegister) override;
	// End UObject interface

	// UNetworkPredictionComponent interface
	virtual void InitializeNetworkPredictionProxy() override;
	// End UNetworkPredictionComponent interface


public:
	UMoverNetworkPredictionLiaisonComponent();

protected:
	TObjectPtr<UMoverComponent> MoverComp;	// the component that we're in charge of driving
	FMoverSyncState* StartingOutSync;
	FMoverAuxStateContext* StartingOutAux;
};