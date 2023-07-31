// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * SpectatorPawns are simple pawns that can fly around the world, used by
 * PlayerControllers when in the spectator state.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/DefaultPawn.h"
#include "SpectatorPawn.generated.h"

UCLASS(config=Game, Blueprintable, BlueprintType, notplaceable)
class ENGINE_API ASpectatorPawn : public ADefaultPawn
{
	GENERATED_UCLASS_BODY()

	// Begin Pawn overrides
	/** Overridden to avoid changing network role. If subclasses want networked behavior, call the Pawn::PossessedBy() instead. */
	virtual void PossessedBy(class AController* NewController) override;
	virtual void TurnAtRate(float Rate) override;
	virtual void LookUpAtRate(float Rate) override;
	// End Pawn overrides
};
