// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/StringBuilder.h"
#include "NetworkPredictionReplicationProxy.h"
#include "Engine/NetSerialization.h"
#include "MoverTypes.h"
#include "MoveLibrary/MovementRecord.h"
#include "LayeredMove.h"
#include "MoverDataModelTypes.h"
#include "UObject/Interface.h"
#include "MoverSimulationTypes.generated.h"

// Names for our default modes
namespace DefaultModeNames
{
	const FName Walking = TEXT("Walking");
	const FName Falling = TEXT("Falling");
	const FName Flying  = TEXT("Flying");
	const FName Swimming  = TEXT("Swimming");
}

// Commonly-used blackboard object keys
namespace CommonBlackboard
{
	const FName LastFloorResult = TEXT("LastFloor");
	const FName LastWaterResult = TEXT("LastWater");
	const FName LastFoundDynamicMovementBase = TEXT("LastFoundDynamicMovementBase");
	const FName LastAppliedDynamicMovementBase = TEXT("LastAppliedDynamicMovementBase");
	const FName TimeSinceSupported = TEXT("TimeSinceSupported");
}


/**
 * Filled out by a MovementMode during simulation tick to indicate its ending state, allowing for a residual time step and switching modes mid-tick
 */
USTRUCT(BlueprintType)
struct MOVER_API FMovementModeTickEndState
{
	GENERATED_USTRUCT_BODY()
	
	FMovementModeTickEndState() 
	{ 
		ResetToDefaults(); 
	}

	void ResetToDefaults()
	{
		RemainingMs = 0.f;
		NextModeName = NAME_None;
	}

	// Any unused tick time
	UPROPERTY(BlueprintReadWrite, Category=Mover)
	float RemainingMs;

	UPROPERTY(BlueprintReadWrite, Category=Mover)
	FName NextModeName = NAME_None;
};




/**
 * The client generates this representation of "input" to the simulated actor for one simulation frame. This can be direct mapping
 * of controls, or more abstract data. It is composed of a collection of typed structs that can be customized per project.
 */
USTRUCT(BlueprintType)
struct MOVER_API FMoverInputCmdContext
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FMoverDataCollection InputCollection;

	FMoverInputCmdContext()
	{
		// TODO: Consider eliminating this default input, considering non-character / non-controlled Mover actors (like platforms)
		InputCollection.FindOrAddDataByType<FCharacterDefaultInputs>();
	}

	UScriptStruct* GetStruct() const { return StaticStruct(); }

	void NetSerialize(const FNetSerializeParams& P)
	{
		bool bIgnoredResult(false);
		InputCollection.NetSerialize(P.Ar, nullptr, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		InputCollection.ToString(Out);
	}
};


/** State we are evolving frame to frame and keeping in sync (frequently changing). It is composed of a collection of typed structs 
 *  that can be customized per project. Mover actors are required to have FMoverDefaultSyncState as one of these structs.
 */
USTRUCT(BlueprintType)
struct MOVER_API FMoverSyncState
{
	GENERATED_USTRUCT_BODY()

public:

	// The mode we ended up in from the prior frame, and which we'll start in during the next frame
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover)
	FName MovementMode;

	// Additional moves influencing our proposed motion
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover)
	FLayeredMoveGroup LayeredMoves;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FMoverDataCollection SyncStateCollection;

	FMoverSyncState()
	{
		MovementMode = NAME_None;
		SyncStateCollection.FindOrAddDataByType<FMoverDefaultSyncState>();
	}

	UScriptStruct* GetStruct() const { return StaticStruct(); }

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << MovementMode;
		LayeredMoves.NetSerialize(P.Ar);

		bool bIgnoredResult(false);
		SyncStateCollection.NetSerialize(P.Ar, nullptr, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("MovementMode: %s\n", TCHAR_TO_ANSI(*MovementMode.ToString()));
		Out.Appendf("Layered Moves: %s\n", TCHAR_TO_ANSI(*LayeredMoves.ToSimpleString()));
		SyncStateCollection.ToString(Out);
	}

	bool ShouldReconcile(const FMoverSyncState& AuthorityState) const
	{
		return (MovementMode != AuthorityState.MovementMode) || 
		       SyncStateCollection.ShouldReconcile(AuthorityState.SyncStateCollection);
	}

	void Interpolate(const FMoverSyncState* From, const FMoverSyncState* To, float Pct)
	{
		MovementMode = To->MovementMode;
		LayeredMoves = To->LayeredMoves;

		SyncStateCollection.Interpolate(From->SyncStateCollection, To->SyncStateCollection, Pct);
	}

};

// Auxiliary state that is input into the simulation (changes rarely)
USTRUCT(BlueprintType)
struct MOVER_API FMoverAuxStateContext
{
	GENERATED_USTRUCT_BODY()

public:
	UScriptStruct* GetStruct() const { return StaticStruct(); }

	bool ShouldReconcile(const FMoverAuxStateContext& AuthorityState) const
	{ 
		return false; 
	}

	void NetSerialize(const FNetSerializeParams& P)
	{
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
	}

	void Interpolate(const FMoverAuxStateContext* From, const FMoverAuxStateContext* To, float PCT)
	{
	}
};


/**
 * Contains all state data for the start of a simulation tick
 */
USTRUCT(BlueprintType)
struct MOVER_API FMoverTickStartData
{
	GENERATED_USTRUCT_BODY()

	FMoverTickStartData() {}
	FMoverTickStartData(
			const FMoverInputCmdContext& InInputCmd,
			const FMoverSyncState& InSyncState,
			const FMoverAuxStateContext& InAuxState)
		:  InputCmd(InInputCmd), SyncState(InSyncState), AuxState(InAuxState)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FMoverInputCmdContext InputCmd;
	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FMoverSyncState SyncState;
	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FMoverAuxStateContext AuxState;
};

/**
 * Contains all state data produced by a simulation tick, including new simulation state
 */
USTRUCT(BlueprintType)
struct MOVER_API FMoverTickEndData
{
	GENERATED_USTRUCT_BODY()

	FMoverTickEndData() {}
	FMoverTickEndData(
		const FMoverSyncState* SyncState,
		const FMoverAuxStateContext* AuxState)
	{
		this->SyncState = *SyncState;
		this->AuxState = *AuxState;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FMoverSyncState SyncState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FMoverAuxStateContext AuxState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FMovementModeTickEndState MovementEndState;

	FMovementRecord MoveRecord;
};

// Input parameters to provide context for SimulationTick functions
USTRUCT(BlueprintType)
struct MOVER_API FSimulationTickParams
{
	GENERATED_BODY()

	// The scene component that is being moved by the simulation, usually the same as the primitive component
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	TObjectPtr<USceneComponent> UpdatedComponent;

	// The primitive component used for collision checking by the simulation, usually the same as UpdatedComponent
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	TObjectPtr<UPrimitiveComponent> UpdatedPrimitive;

	// The Mover Component associated with this sim tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	TObjectPtr<UMoverComponent> MoverComponent;

	// Simulation state data at the start of the tick, including Input Cmd
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FMoverTickStartData StartState;

	// Time and frame information for this tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FMoverTimeStep TimeStep;

	// Proposed movement for this tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FProposedMove ProposedMove;
};


UINTERFACE(BlueprintType)
class MOVER_API UMoverInputProducerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * MoverInputProducerInterface: API for any object that can produce input for a Mover simulation frame
 */
class IMoverInputProducerInterface : public IInterface
{
	GENERATED_BODY()

public:
	/** Contributes additions to the input cmd for this simulation frame. Typically this is translating accumulated user input (or AI state) into parameters that affect movement. */
	UFUNCTION(BlueprintNativeEvent)
	void ProduceInput(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult);
};