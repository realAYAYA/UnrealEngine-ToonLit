// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionCues.h"

DEFINE_LOG_CATEGORY(LogNetworkPredictionCues);

FGlobalCueTypeTable FGlobalCueTypeTable::Singleton;

FGlobalCueTypeTable::FRegisteredCueTypeInfo& FGlobalCueTypeTable::GetRegistedTypeInfo()
{
	static FGlobalCueTypeTable::FRegisteredCueTypeInfo PendingCueTypes;
	return PendingCueTypes;
}

// ------------------------------------------------------------------------------------------------------------------------------------------
//	Mock Cue example: Minimal example of NetSimCue pipeline
//		(1)	- How to define cue types and traits
//		(2) - How to define cue sets
//		(3) - How to define handlers
//		(4) - How to invoke cues in simulation
//
// ------------------------------------------------------------------------------------------------------------------------------------------

#define ENABLE_MOCK_CUE 0
#if ENABLE_MOCK_CUE

// ---------------------------------------------------------------------
// (1) Mock Cue Types
// ---------------------------------------------------------------------

// Example "Weak" cue (will default to NetSimCueTraits::Default traits, which are "Weak" non replicated, non resimulated). No NetSerialize/NetIdentical is required.
struct FMockImpactCue
{
	NETSIMCUE_BODY();

	FMockImpactCue() = default;
	FMockImpactCue(const FVector& InImpact) : ImpactLocation(InImpact) { }

	FVector ImpactLocation;

};
NETSIMCUE_REGISTER(FMockImpactCue, TEXT("Impact")); // Must go in .cpp
static_assert(TNetSimCueTraits<FMockImpactCue>::InvokeMask == NetSimCueTraits::Default::InvokeMask, "Traits Errors"); // Not required: just asserting what this classes traits are

// ----------------

// Example "Strong" cue. Explicitly sets NetSimCueTraits::Strong and implements NetSerialize/NetIdentical (will not compile without these functions) 
struct FMockDamageCue
{
	NETSIMCUE_BODY();

	FMockDamageCue() = default;
	FMockDamageCue(const int32 InSourceID, const int32 InDamageType, const FVector& InHitLocation) 
		: SourceID(InSourceID), DamageType(InDamageType), HitLocation(InHitLocation) { }

	int32 SourceID;
	int32 DamageType;
	FVector HitLocation;

	using Traits = NetSimCueTraits::Strong; // Sets our traits via preset.
	
	void NetSerialize(FArchive& Ar)
	{
		Ar << SourceID;
		Ar << DamageType;
		Ar << HitLocation;
	}

	bool NetIdentical(const FMockDamageCue& Other) const
	{
		const float ErrorTolerance = 1.f;
		return SourceID == Other.SourceID && DamageType && Other.DamageType && HitLocation.Equals(Other.HitLocation, ErrorTolerance);
	}
};
NETSIMCUE_REGISTER(FMockDamageCue, TEXT("Damage")); // Must go in .cpp
static_assert(TNetSimCueTraits<FMockDamageCue>::InvokeMask == NetSimCueTraits::Strong::InvokeMask, "Traits Errors"); // Not required: just asserting what this classes traits are

// Alternative way of explicitly setting traits
struct FMockHealingCue
{
	NETSIMCUE_BODY();
	
	FMockHealingCue() = default;
	FMockHealingCue(const int32 InSourceID, const int32 InHealingAmount) 
		: SourceID(InSourceID), HealingAmount(InHealingAmount) { }

	int32 SourceID;
	int32 HealingAmount;

	using Traits = TNetSimCueTraitsExplicit<ENetSimCueInvoker::All, ENetSimCueReplicationTarget::All, true>; // Sets our traits explicitly
	
	void NetSerialize(FArchive& Ar)
	{
		Ar << SourceID;
		Ar << HealingAmount;
	}

	bool NetIdentical(const FMockHealingCue& Other) const
	{
		const float ErrorTolerance = 1.f;
		return SourceID == Other.SourceID && HealingAmount == Other.HealingAmount;
	}
};
NETSIMCUE_REGISTER(FMockHealingCue, TEXT("Healing")); // Must go in .cpp
static_assert(TNetSimCueTraits<FMockHealingCue>::InvokeMask == ENetSimCueInvoker::All, "Traits Errors"); // Not required: just asserting what this classes traits are

// ---------------------------------------------------------------------
// (2) Cue Sets: just a collection of cue types that a simulation emits.
//
//	This isn't really a core type, it is just a pattern for grouping cue types
//	together so that handlers can register with sets of cues rather than each cue individually
//
//	This example illustrates how a base set can be built off of by child simulations
// ---------------------------------------------------------------------

struct FMockCueSet_Base
{
	template<typename TDispatchTable>
	static void RegisterNetSimCueTypes(TDispatchTable& DispatchTable)
	{
		DispatchTable.template RegisterType<FMockImpactCue>();
	}
};

struct FMockCueSet_Child
{
	template<typename TDispatchTable>
	static void RegisterNetSimCueTypes(TDispatchTable& DispatchTable)
	{
		FMockCueSet_Base::RegisterNetSimCueTypes(DispatchTable); // Superset of the base cueset
		DispatchTable.template RegisterType<FMockDamageCue>();
		DispatchTable.template RegisterType<FMockHealingCue>();
	}
};


// ---------------------------------------------------------------------
// (3) Mock Handlers 
//	-Implements ::HandeCue function to receive the cue (thats it!)
//	-Meant to be implemented by owning actor/component that is driving the NetworkedSimulation
//	-E.g, in practice, these aren't standalone classes. These are just functions/body that you put on your actor/component.
//
//	Again, we show how a base handler can be implemented and how a child class can extend off of that.
// ---------------------------------------------------------------------

class TestHandlerBase
{
public:
	void HandleCue(const FMockImpactCue& ImpactCue, const FNetSimCueSystemParamemters& SystemParameters)
	{
		UE_LOG(LogNetworkSim, Warning, TEXT("Impact!"));
	}
};

NETSIMCUESET_REGISTER(TestHandlerBase, FMockCueSet_Base); // Must go in cpp. Says "TestHandlerBase" can accepts cues in "FMockCueSet_Base"

class TestHandlerChild : public TestHandlerBase
{
public:

	using TestHandlerBase::HandleCue; // Required for us to "use" the HandleCue methods in our parent class

	void HandleCue(const FMockDamageCue& DamageCue, const FNetSimCueSystemParamemters& SystemParameters)
	{
		UE_LOG(LogNetworkSim, Warning, TEXT("HandleCue: Damage. SourceID: %d DamageType: %d HitLocation: %s"), DamageCue.SourceID, DamageCue.DamageType, *DamageCue.HitLocation.ToString());
	}

	void HandleCue(const FMockHealingCue& HealingCue, const FNetSimCueSystemParamemters& SystemParameters)
	{
		UE_LOG(LogNetworkSim, Warning, TEXT("HandleCue: Healing. SourceID: %d Healing Amount: %d"), HealingCue.SourceID, HealingCue.HealingAmount);
	}
};

NETSIMCUESET_REGISTER(TestHandlerChild, FMockCueSet_Child); // Must go in cpp. Says "TestHandlerBase" can accepts cues in "FMockCueSet_Base" // Must go in cpp. Says "TestHandlerBase" can accepts cues in "FMockCueSet_Base"

void TestCues()
{
	// -------------------------------------
	// (4) Simuation/Invoke
	// -------------------------------------

	struct FBaseSimulation
	{
		static void TickSimulation(FNetSimCueDispatcher& Dispatcher)
		{
			Dispatcher.Invoke<FMockImpactCue>(FVector(1.f, 2.f, 3.f)); // Constructing emplace avoids copy and moves. This is the ideal way to invoke!
		}
	};

	struct FChildSimulation
	{
		static void TickSimulation(FNetSimCueDispatcher& Dispatcher)
		{
			FBaseSimulation::TickSimulation(Dispatcher);

			Dispatcher.Invoke<FMockDamageCue>(1, 2, FVector(1.f, 2.f, 3.f));
			Dispatcher.Invoke<FMockHealingCue>(10, 32);
		}
	};

	TNetSimCueDispatcher<void> ServerDispatcher;
	FChildSimulation::TickSimulation(ServerDispatcher);

	// -------------------------------------
	// Send
	// -------------------------------------

	FNetBitWriter TempWriter(1024 << 3);
	ServerDispatcher.NetSendSavedCues(TempWriter, ENetSimCueReplicationTarget::All, true);

	// -------------------------------------
	// Receive
	// -------------------------------------

	FNetBitReader TempReader(nullptr, TempWriter.GetData(), TempWriter.GetNumBits());
	TNetSimCueDispatcher<void> ClientDispatcher;
	ClientDispatcher.SetReceiveReplicationTarget(ENetSimCueReplicationTarget::All);
	ClientDispatcher.NetRecvSavedCues(TempReader, true, 0, 0);

	TestHandlerChild MyObject;

	ClientDispatcher.DispatchCueRecord(MyObject, FNetworkSimTime());
}

FAutoConsoleCommandWithWorldAndArgs NetworkSimulationModelCueCmd(TEXT("nms.CueTest"), TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray< FString >& Args, UWorld* World) 
{
	TestCues();
}));

#endif // ENABLE_MOCK_CUE