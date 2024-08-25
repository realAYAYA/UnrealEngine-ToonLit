// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGGenSourceBase.h"

#include "PCGGenSourcePlayer.generated.h"

class APlayerController;

/**
 * A UPCGGenSourcePlayer is automatically captured for all PlayerControllers in the level on Login/Logout.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGGenSourcePlayer : public UObject, public IPCGGenSourceBase
{
	GENERATED_BODY()

public:
	/** Returns the world space position of this gen source. */
	virtual TOptional<FVector> GetPosition() const override;

	/** Returns the normalized forward vector of this gen source. */
	virtual TOptional<FVector> GetDirection() const override;

	TWeakObjectPtr<const APlayerController> GetPlayerController() const { return PlayerController; }
	void SetPlayerController(const APlayerController* InPlayerController);

	bool IsValid() const { return PlayerController.IsValid(); }

protected:
	TWeakObjectPtr<const APlayerController> PlayerController;
};
