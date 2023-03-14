// Copyright Epic Games, Inc. All Rights Reserved.

#include "MockPhysicsGrenade.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "PhysicsEngine/BodyInstance.h"
#include "NetworkPredictionPhysics.h"

/** NetworkedSimulation Model type */
class FMockPhysicsGrenadeModelDef : public FNetworkPredictionModelDef
{
public:

	NP_MODEL_BODY();

	using StateTypes = MockPhysicsGrenadeTypes;
	using Simulation = UMockPhysicsGrenadeComponent;	// Note how this example uses the component as the sim obj (!).
	using Driver = UMockPhysicsGrenadeComponent;
	using PhysicsState = FNetworkPredictionPhysicsState;
	
	static const TCHAR* GetName() { return TEXT("MockPhysicsGrenade"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers + 7; }
};

NP_MODEL_REGISTER(FMockPhysicsGrenadeModelDef);

struct FMockGrenadeModelDefCueSet
{
	template<typename TDispatchTable>
	static void RegisterNetSimCueTypes(TDispatchTable& DispatchTable)
	{
		DispatchTable.template RegisterType<FMockGrenadeExplodeCue>();
	}
};

NETSIMCUE_REGISTER(FMockGrenadeExplodeCue, TEXT("MockPhysicsJumpCue"));
NETSIMCUESET_REGISTER(UMockPhysicsGrenadeComponent, FMockGrenadeModelDefCueSet);

// -------------------------------------------------------------------------------------------------------
//	UMockPhysicsGrenadeComponent
// -------------------------------------------------------------------------------------------------------

void UMockPhysicsGrenadeComponent::InitializeNetworkPredictionProxy()
{
	npCheckSlow(this->UpdatedPrimitive);
	
	NetworkPredictionProxy.Init<FMockPhysicsGrenadeModelDef>(GetWorld(), GetReplicationProxies(), this, this);
}

void UMockPhysicsGrenadeComponent::InitializeSimulationState(void* Sync, FMockPhysicsGrenadeAuxState* Aux)
{
	Aux->ExplosionTimeMS = NetworkPredictionProxy.GetTotalSimTimeMS() + (int32)(this->FuseTimeSeconds * 1000.f);
}

void UMockPhysicsGrenadeComponent::FinalizeFrame(const void* SyncState, const FMockPhysicsGrenadeAuxState* AuxState)
{

}

void UMockPhysicsGrenadeComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<MockPhysicsGrenadeTypes>& Input, const TNetSimOutput<MockPhysicsGrenadeTypes>& Output)
{
	npCheckSlow(this->UpdatedPrimitive);

	// Normally putting SimulationTick on the component is dangerous/unsafe (because it is easy to read/write data outside of the synx/aux state)
	// But this is so simple that it doesn't seem necessary to create a seperate simobj
	const int32 ExplosionTimeMS = Input.Aux->ExplosionTimeMS;
	if (ExplosionTimeMS > 0 && ExplosionTimeMS <= TimeStep.TotalSimulationTime)
	{
		const FTransform CurrentTransform = UpdatedPrimitive->GetComponentTransform();	
		const FVector CurrentLocation = CurrentTransform.GetLocation();

		FVector TracePosition = CurrentLocation;
		FCollisionShape Shape = FCollisionShape::MakeSphere(this->Radius);
		ECollisionChannel CollisionChannel = ECollisionChannel::ECC_PhysicsBody; 
		FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
		FCollisionResponseParams ResponseParams = FCollisionResponseParams::DefaultResponseParam;
		FCollisionObjectQueryParams ObjectParams(ECollisionChannel::ECC_PhysicsBody);

		TArray<FOverlapResult> Overlaps;

		GetWorld()->OverlapMultiByChannel(Overlaps, TracePosition, FQuat::Identity, CollisionChannel, Shape);

		for (FOverlapResult& Result : Overlaps)
		{
			if (UPrimitiveComponent* PrimitiveComp = Result.Component.Get())
			{
				if (PrimitiveComp->IsSimulatingPhysics())
				{
					FVector Dir = (PrimitiveComp->GetComponentLocation() - TracePosition);
					Dir.Z = 0.f;
					Dir.Normalize();

					FVector Impulse = Dir * this->Magnitude;
					Impulse.Z = this->Magnitude;

					PrimitiveComp->AddImpulseAtLocation(Impulse, TracePosition);
				}
			}
		}

		Output.Aux.Get()->ExplosionTimeMS = 0;
		Output.CueDispatch.Invoke<FMockGrenadeExplodeCue>();
	}
}

void UMockPhysicsGrenadeComponent::HandleCue(const FMockGrenadeExplodeCue& ExplodeCue, const FNetSimCueSystemParamemters& SystemParameters)
{
	OnExplode.Broadcast();
}