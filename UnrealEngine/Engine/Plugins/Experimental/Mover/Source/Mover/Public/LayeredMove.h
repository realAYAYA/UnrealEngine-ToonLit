// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoveLibrary/MovementUtilsTypes.h"
#include "LayeredMove.generated.h"

class UMovementMixer;
struct FMoverTickStartData;
struct FMoverTimeStep;
class UMoverBlackboard;
class UMoverComponent;


UENUM(BlueprintType)
enum class ELayeredMoveFinishVelocityMode : uint8
{
	// Maintain the last velocity root motion gave to the character
	MaintainLastRootMotionVelocity = 0,
	// Set Velocity to the specified value (for example, 0,0,0 to stop the character)
	SetVelocity,
	// Clamp velocity magnitude to the specified value. Note that it will not clamp Z if negative (falling). it will clamp Z positive though. 
	ClampVelocity,
};

/** 
 * Struct for LayeredMove Finish Velocity options.
 */
USTRUCT(BlueprintType)
struct FLayeredMoveFinishVelocitySettings
{
	GENERATED_USTRUCT_BODY()

	FLayeredMoveFinishVelocitySettings()
		: FinishVelocityMode(ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity)
		, SetVelocity(FVector::ZeroVector)
		, ClampVelocity(0.f)
	{}
	
	virtual ~FLayeredMoveFinishVelocitySettings() {};

	virtual void NetSerialize(FArchive& Ar);
	
	// What mode we want to happen when a Layered Move ends, see @ELayeredMoveFinishVelocityMode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	ELayeredMoveFinishVelocityMode FinishVelocityMode;
	
	// Velocity that the actor will use if Mode == SetVelocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector SetVelocity;

	// Actor's Velocity will be clamped to this value if Mode == ClampVelocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float ClampVelocity;
};

/** 
* Layered Moves are methods of affecting motion on a Mover-based actor, typically for a limited time. 
* Common uses would be for jumping, dashing, blast forces, etc.
* They are ticked as part of the Mover simulation, and produce a proposed move. These proposed moves 
* are aggregated and applied to the overall attempted move.
* Multiple layered moves can be active at any time, and may produce additive motion or motion that overrides
* what the current Movement Mode may intend.
*/

// Base class for all layered moves
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMoveBase();
	virtual ~FLayeredMoveBase() {}

	// Determines how this object's movement contribution should be mixed with others
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	EMoveMixMode MixMode;

	// Determines if this layered move should take priority over other layered moves when different moves have conflicting overrides - higher numbers taking precedent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	uint8 Priority;
	
	// This move will expire after a set amount of time if > 0. If 0, it will be ticked only once, regardless of time step. It will need to be manually ended if < 0. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float DurationMs;

	// The simulation time this move first ticked (< 0 means it hasn't started yet)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = Mover)
	float StartSimTimeMs;

	// Settings related to velocity applied to the actor after a layered move has finished
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FLayeredMoveFinishVelocitySettings FinishVelocitySettings;
	
	// Kicks off this move, allowing any initialization to occur.
	void StartMove(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, float CurrentSimTimeMs);

	// Called when this layered move starts.
	virtual void OnStart(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard) {}

	// TODO: consider whether MoverComp should just be part of FMoverTickStartData
	// Generate a movement that will be combined with other sources
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) { return false; }

	// Runtime query whether this move is finished and can be destroyed. The default implementation is based on DurationMs.
	virtual bool IsFinished(float CurrentSimTimeMs) const;

	// Ends this move, allowing any cleanup to occur.
	void EndMove(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, float CurrentSimTimeMs);
	
	// Called when this layered move ends.
	virtual void OnEnd(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, float CurrentSimTimeMs) {}

	// @return newly allocated copy of this FLayeredMoveBase. Must be overridden by child classes
	virtual FLayeredMoveBase* Clone() const;

	virtual void NetSerialize(FArchive& Ar);

	virtual UScriptStruct* GetScriptStruct() const;

	virtual FString ToSimpleString() const;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) {}
};

template<>
struct TStructOpsTypeTraits< FLayeredMoveBase > : public TStructOpsTypeTraitsBase2< FLayeredMoveBase >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};


// A collection of layered moves affecting a movable actor
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMoveGroup
{
	GENERATED_USTRUCT_BODY()

	FLayeredMoveGroup();
	virtual ~FLayeredMoveGroup() {}

	/**
	 * If true ResidualVelocity will be the next velocity used for this actor
	 * Note: This is set automatically when a layered move has ended based off of end velocity settings - it is not meant to be set by the user see @FLayeredMoveFinishVelocitySettings
	 */
	UPROPERTY(NotReplicated, VisibleAnywhere, BlueprintReadOnly, Category = Mover)
	bool bApplyResidualVelocity;
	
	/**
	 * If bApplyResidualVelocity is true this actors velocity will be set to this.
	 * Note: This is set automatically when a layered move has ended based off of end velocity settings - it is not meant to be set by the user see @FLayeredMoveFinishVelocitySettings
     */
	UPROPERTY(NotReplicated, VisibleAnywhere, BlueprintReadOnly, Category = Mover)
	FVector ResidualVelocity;

	/**
	 * Clamps an actors velocity to this value when a layered move ends. This expects Value >= 0.
	 * Note: This is set automatically when a layered move has ended based off of end velocity settings - it is not meant to be set by the user see @FLayeredMoveFinishVelocitySettings
	 */
	UPROPERTY(NotReplicated, VisibleAnywhere, BlueprintReadOnly, Category = Mover)
	float ResidualClamping;
	
	void QueueLayeredMove(TSharedPtr<FLayeredMoveBase> Move);

	bool HasAnyMoves() const { return (!ActiveLayeredMoves.IsEmpty() || !QueuedLayeredMoves.IsEmpty()); }

	// Generates active layered move list (by calling FlushMoveArrays) and returns the an array of all currently active layered moves
	TArray<TSharedPtr<FLayeredMoveBase>> GenerateActiveMoves(const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard);

	/** Serialize all moves and their states for this group */
	void NetSerialize(FArchive& Ar, uint8 MaxNumMovesToSerialize = MAX_uint8);

	/** Copy operator - deep copy so it can be used for archiving/saving off moves */
	FLayeredMoveGroup& operator=(const FLayeredMoveGroup& Other);

	/** Comparison operator - needs matching LayeredMoves along with identical states in those structs */
	bool operator==(const FLayeredMoveGroup& Other) const;

	/** Comparison operator */
	bool operator!=(const FLayeredMoveGroup& Other) const;

	/** Exposes references to GC system */
	void AddStructReferencedObjects(FReferenceCollector& Collector) const;

	/** Get a simplified string representation of this group. Typically for debugging. */
	FString ToSimpleString() const;

	/** Const access to active moves */
	TArray<TSharedPtr<FLayeredMoveBase>>::TConstIterator GetActiveMovesIterator() const;
	
	/** Const access to queued moves */
	TArray<TSharedPtr<FLayeredMoveBase>>::TConstIterator GetQueuedMovesIterator() const;

	// Resets residual velocity related settings
	void ResetResidualVelocity();

protected:
	// Clears out any finished or invalid active moves and adds any queued moves to the active moves
	void FlushMoveArrays(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, float CurrentSimTimeMs);

	// Helper function for gathering any residual velocity settings from layered moves that just ended
	void GatherResidualVelocitySettings(const TSharedPtr<FLayeredMoveBase>& Move, bool& bResidualVelocityOverriden, bool& bClampVelocityOverriden);
	
	/** Helper function for serializing array of root motion sources */
	static void NetSerializeLayeredMovesArray(FArchive& Ar, TArray< TSharedPtr<FLayeredMoveBase> >& LayeredMovesArray, uint8 MaxNumLayeredMovesToSerialize = MAX_uint8);

	/** Layered moves currently active in this group */
	TArray< TSharedPtr<FLayeredMoveBase> > ActiveLayeredMoves;

	/** Moves that are queued to become active next sim frame */
	TArray< TSharedPtr<FLayeredMoveBase> > QueuedLayeredMoves;
};

template<>
struct TStructOpsTypeTraits<FLayeredMoveGroup> : public TStructOpsTypeTraitsBase2<FLayeredMoveGroup>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FLayeredMoveBase> Data is copied around
		//WithNetSerializer = true,
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};
