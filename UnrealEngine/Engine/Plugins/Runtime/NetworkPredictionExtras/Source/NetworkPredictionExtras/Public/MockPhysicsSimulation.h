// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Misc/StringBuilder.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTickState.h"
#include "NetworkPredictionSimulation.h"
#include "NetworkPredictionReplicationProxy.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "NetworkPredictionCues.h"
#include "NetworkPredictionTrace.h"

#include "MockPhysicsSimulation.generated.h"

// -------------------------------------------------------------------------------------------------------------------------------
// Mock Physics Simulation
//	This is an example of NP Sim + Physics working together. The NP sim is super simple: it applies forces to the physics body 
//	based on InputCmd. In this example there is no Sync state, mainly to illustrate that it is actually optional. That is not
//	a requirement of physics based NP sims.
// -------------------------------------------------------------------------------------------------------------------------------


// Making this a blueprint type to that it can be filled out by BPs.
USTRUCT(BlueprintType)
struct FMockPhysicsInputCmd
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Input)
	FVector MovementInput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Input)
	bool bJumpedPressed  = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Input)
	bool bChargePressed  = false;

	FMockPhysicsInputCmd()
		: MovementInput(ForceInitToZero)
	{ }

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << MovementInput;
		P.Ar << bJumpedPressed;
		P.Ar << bChargePressed;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("MovementInput: X=%.2f Y=%.2f Z=%.2f\n", MovementInput.X, MovementInput.Y, MovementInput.Z);
		Out.Appendf("bJumpedPressed: %d\n", bJumpedPressed);
		Out.Appendf("bChargePressed: %d\n", bChargePressed);
	}
};

struct FMockPhysicsAuxState
{
	float ForceMultiplier = 1.f;
	int32 JumpCooldownTime = 0; // can't jump again until sim time has passed this
	int32 ChargeStartTime = 0;
	int32 ChargeEndTime = 0;

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << ForceMultiplier;
		P.Ar << JumpCooldownTime;
		P.Ar << ChargeStartTime;
		P.Ar << ChargeEndTime;
	}
	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("ForceMultiplier: %.2f", ForceMultiplier);
		Out.Appendf("JumpCooldownTime: %d", JumpCooldownTime);
		Out.Appendf("ChargeStartTime: %d", ChargeStartTime);
		Out.Appendf("ChargeEndTime: %d", ChargeEndTime);
	}

	void Interpolate(const FMockPhysicsAuxState* From, const FMockPhysicsAuxState* To, float PCT)
	{
		ForceMultiplier = FMath::Lerp(From->ForceMultiplier, To->ForceMultiplier, PCT);
		JumpCooldownTime = To->JumpCooldownTime;
		ChargeStartTime = To->ChargeStartTime;
		ChargeEndTime = To->ChargeEndTime;
	}

	bool ShouldReconcile(const FMockPhysicsAuxState& AuthorityState) const
	{
		UE_NP_TRACE_RECONCILE(ForceMultiplier != AuthorityState.ForceMultiplier, "ForceMultiplier:");
		UE_NP_TRACE_RECONCILE(JumpCooldownTime != AuthorityState.JumpCooldownTime, "JumpCooldownTime:");
		UE_NP_TRACE_RECONCILE(ChargeStartTime != AuthorityState.ChargeStartTime, "ChargeStartTime:");
		UE_NP_TRACE_RECONCILE(ChargeEndTime != AuthorityState.ChargeEndTime, "ChargeEndTime:");

		return false;
	}
};

using MockPhysicsStateTypes = TNetworkPredictionStateTypes<FMockPhysicsInputCmd, void, FMockPhysicsAuxState>;

class FMockPhysicsSimulation
{
public:
	
	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<MockPhysicsStateTypes>& Input, const TNetSimOutput<MockPhysicsStateTypes>& Output);
	
	// Physics Component we are driving
	UPrimitiveComponent* PrimitiveComponent = nullptr;
};


struct FMockPhysicsJumpCue
{
	FMockPhysicsJumpCue() = default;
	FMockPhysicsJumpCue(const FVector& InStart)
		: Start(InStart) { }
	
	NETSIMCUE_BODY();
	using Traits = NetSimCueTraits::ReplicatedXOrPredicted;

	FVector_NetQuantize100 Start;

	void NetSerialize(FArchive& Ar)
	{
		bool b = false;
		Start.NetSerialize(Ar, nullptr, b);
	}

	bool NetIdentical(const FMockPhysicsJumpCue& Other) const
	{
		const float ErrorTolerance = 1.f;
		return Start.Equals(Other.Start, ErrorTolerance);
	}
};


struct FMockPhysicsChargeCue
{
	FMockPhysicsChargeCue() = default;
	FMockPhysicsChargeCue(const FVector& InStart)
		: Start(InStart) { }

	NETSIMCUE_BODY();
	using Traits = NetSimCueTraits::Strong;

	FVector_NetQuantize100 Start;

	void NetSerialize(FArchive& Ar)
	{
		bool b = false;
		Start.NetSerialize(Ar, nullptr, b);
	}

	bool NetIdentical(const FMockPhysicsChargeCue& Other) const
	{
		// Large error tolerance is acceptable here
		//	-Since its a "burst" particle we aren't bothering to rollback/redo it in the user code
		//	-Better to just allow "close enough" to avoid double playing. This cue is not conveying gameplay critical information to the player.
		const float ErrorTolerance = 10.f;
		return Start.Equals(Other.Start, ErrorTolerance);
	}
};

struct FMockPhysicsCueSet
{
	template<typename TDispatchTable>
	static void RegisterNetSimCueTypes(TDispatchTable& DispatchTable)
	{
		DispatchTable.template RegisterType<FMockPhysicsJumpCue>();
		DispatchTable.template RegisterType<FMockPhysicsChargeCue>();
	}
};
