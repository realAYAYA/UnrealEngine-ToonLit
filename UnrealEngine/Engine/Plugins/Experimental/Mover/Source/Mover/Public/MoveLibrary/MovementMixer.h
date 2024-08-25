// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementMixer.generated.h"

struct FLayeredMoveBase;
struct FProposedMove;

/**
 * Class in charge of Mixing various moves when evaluating/combining moves. The mixer used can be set on the MoverComponent itself.
 */
UCLASS(BlueprintType)
class MOVER_API UMovementMixer : public UObject
{
	GENERATED_BODY()

public:
	UMovementMixer();
	
	/** In charge of mixing Layered Move proposed moves into a cumulative proposed move based on mix mode and priority.*/
	virtual void MixLayeredMove(const FLayeredMoveBase& ActiveMove, const FProposedMove& MoveStep, FProposedMove& OutCumulativeMove);

	/** In charge of mixing proposed moves together. Is similar to MixLayeredMove but is only responsible for mixing proposed moves instead of layered moves. */
	virtual void MixProposedMoves(const FProposedMove& MoveToMix, FProposedMove& OutCumulativeMove);

	/** Resets all state used for mixing. Should be called before or after finished mixing moves. */
	virtual void ResetMixerState();
	
protected:
	// Stores the current highest priority we've hit during this round of mixing. Will get reset. Note: Currently only used for mixing layered moves
	uint8 CurrentHighestPriority;

	// Earliest start time of the layered move with highest priority. Used to help break ties of moves with same priority. Note: Currently only used for mixing layered moves
	float CurrentLayeredMoveStartTime;
	
	/**
	 * Helper function for layered move mixing to check priority and start time if priority is the same.
	 * Returns true if this layered move should take priority given current HighestPriority and CurrentLayeredMoveStartTimeMs
	 */
	static bool CheckPriority(const FLayeredMoveBase* LayeredMove, uint8& InOutHighestPriority, float& InOutCurrentLayeredMoveStartTimeMs);
};
