// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkPredictionPhysicsComponent.h"
#include "Engine/EngineTypes.h"
#include "Misc/StringBuilder.h"
#include "NetworkPredictionTickState.h"
#include "NetworkPredictionSimulation.h"
#include "NetworkPredictionStateTypes.h"
#include "Math/UnrealMathUtility.h"
#include "NetworkPredictionCues.h"

#include "MockPhysicsGrenade.generated.h"

struct FMockPhysicsGrenadeAuxState
{
	int32 ExplosionTimeMS = 0; // SimTime explosion will happen

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << ExplosionTimeMS;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("ExplosionTimeMS: %d", ExplosionTimeMS);
	}

	void Interpolate(const FMockPhysicsGrenadeAuxState* From, const FMockPhysicsGrenadeAuxState* To, float PCT)
	{
		ExplosionTimeMS = FMath::Lerp(From->ExplosionTimeMS, To->ExplosionTimeMS, PCT);
	}

	bool ShouldReconcile(const FMockPhysicsGrenadeAuxState& AuthorityState) const
	{
		return ExplosionTimeMS != AuthorityState.ExplosionTimeMS;
	}
};

using MockPhysicsGrenadeTypes = TNetworkPredictionStateTypes<void, void, FMockPhysicsGrenadeAuxState>;

struct FMockGrenadeExplodeCue
{
	FMockGrenadeExplodeCue() = default;

	NETSIMCUE_BODY();
	using Traits = NetSimCueTraits::Strong;
	
	void NetSerialize(FArchive& Ar) { }
	bool NetIdentical(const FMockGrenadeExplodeCue& Other) const { return true; }
};

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

UCLASS(BlueprintType, meta=(BlueprintSpawnableComponent))
class NETWORKPREDICTIONEXTRAS_API UMockPhysicsGrenadeComponent : public UNetworkPredictionPhysicsComponent
{
	GENERATED_BODY()

public:

	void InitializeSimulationState(void* Sync, FMockPhysicsGrenadeAuxState* Aux);

	void FinalizeFrame(const void* SyncState, const FMockPhysicsGrenadeAuxState* AuxState);

	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<MockPhysicsGrenadeTypes>& Input, const TNetSimOutput<MockPhysicsGrenadeTypes>& Output);	

	void HandleCue(const FMockGrenadeExplodeCue& ExplodeCue, const FNetSimCueSystemParamemters& SystemParameters);

	UPrimitiveComponent* GetPhysicsPrimitiveComponent() const { return UpdatedPrimitive; }

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMockGrenadeOnExplode);
	
	UPROPERTY(BlueprintAssignable, Category="Mock Grenade")
	FMockGrenadeOnExplode OnExplode;

protected:

	UPROPERTY(EditDefaultsOnly, Category="Grenade")
	float FuseTimeSeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Grenade")
	float Radius = 250.f;

	UPROPERTY(EditDefaultsOnly, Category="Grenade")
	float Magnitude = 100000; 

	virtual void InitializeNetworkPredictionProxy() override;
};