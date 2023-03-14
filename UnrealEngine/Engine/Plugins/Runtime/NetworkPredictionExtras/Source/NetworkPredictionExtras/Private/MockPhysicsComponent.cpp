// Copyright Epic Games, Inc. All Rights Reserved.

#include "MockPhysicsComponent.h"
#include "MockPhysicsSimulation.h"

#include "Components/PrimitiveComponent.h"

#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionCVars.h"
#include "NetworkPredictionCheck.h"
#include "NetworkPredictionPhysics.h"
#include "DrawDebugHelpers.h"

namespace NetworkPredictionCVars
{
	// NETSIM_DEVCVAR_SHIPCONST_INT(FixTickByDefault, 1, "np.FixTickByDefault", "Set preferred tick mode to fix tick. E.g: fix tick if we can");
};

// ----------------------------------------------------------------------------------------------------------
//	FMockPhysicsModelDef: the piece that ties everything together that we use to register with the NP system.
// ----------------------------------------------------------------------------------------------------------

class FMockPhysicsModelDef : public FNetworkPredictionModelDef
{
public:

	NP_MODEL_BODY();

	using Simulation = FMockPhysicsSimulation;
	using StateTypes = MockPhysicsStateTypes;
	using Driver = UMockPhysicsComponent;
	using PhysicsState = FNetworkPredictionPhysicsState;

	static const TCHAR* GetName() { return TEXT("MockPhysics"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::Physics+1; }
};

NP_MODEL_REGISTER(FMockPhysicsModelDef);

// ----------------------------------------------------------------------------------------------------------
//	UMockPhysicsComponent
// ----------------------------------------------------------------------------------------------------------

// UMockPhysicsComponent implements the FMockPhysicsCueSet
NETSIMCUESET_REGISTER(UMockPhysicsComponent, FMockPhysicsCueSet);

UMockPhysicsComponent::UMockPhysicsComponent()
{

}

void UMockPhysicsComponent::InitializeNetworkPredictionProxy()
{
	// We need valid UpdatedPrimitive and PhysicsHandle to register
	// This code does not handle any "not ready yet" cases
	if (npEnsure(UpdatedPrimitive))
	{		 
		OwnedSimulation = MakePimpl<FMockPhysicsSimulation>();
		InitMockPhysicsSimulation(OwnedSimulation.Get());
		npCheckSlow(ActiveSimulation);

		ActiveSimulation->PrimitiveComponent = UpdatedPrimitive;
		NetworkPredictionProxy.Init<FMockPhysicsModelDef>(GetWorld(), GetReplicationProxies(), ActiveSimulation, this);
	}
}

void UMockPhysicsComponent::ProduceInput(const int32 DeltaTimeMS, FMockPhysicsInputCmd* Cmd)
{
	// This is just one way to do it. Other examples use delegates. This was chosen so that we wouldn't need
	// to create a native actor type to do the input. (Can blueprint a pawn and use this component easily)
	npCheckSlow(Cmd);
	*Cmd = PendingInputCmd;
}

void UMockPhysicsComponent::FinalizeFrame(const void* SyncState, const FMockPhysicsAuxState* AuxState)
{
	npCheckSlow(AuxState);
	const bool bNewIsCharging = AuxState->ChargeStartTime != 0;
	if (bNewIsCharging != bIsCharging)
	{
		bIsCharging = bNewIsCharging;
		OnChargeStateChange.Broadcast(bNewIsCharging);
	}
}

void UMockPhysicsComponent::InitializeSimulationState(const void* Sync, FMockPhysicsAuxState* Aux)
{
}

// Init function. This is broken up from ::InstantiateNetworkedSimulation and templated so that subclasses can share the init code
void UMockPhysicsComponent::InitMockPhysicsSimulation(FMockPhysicsSimulation* Simulation)
{
	check(UpdatedComponent);
	check(ActiveSimulation == nullptr); // Reinstantiation not supported
	ActiveSimulation = Simulation;
}

void UMockPhysicsComponent::HandleCue(const FMockPhysicsJumpCue& JumpCue, const FNetSimCueSystemParamemters& SystemParameters)
{
	OnJumpActivatedEvent.Broadcast(JumpCue.Start, (float)SystemParameters.TimeSinceInvocation / 1000.f);
}

void UMockPhysicsComponent::HandleCue(const FMockPhysicsChargeCue& ChargeCue, const FNetSimCueSystemParamemters& SystemParameters)
{
	OnChargeActivatedEvent.Broadcast(ChargeCue.Start, (float)SystemParameters.TimeSinceInvocation / 1000.f);
}

