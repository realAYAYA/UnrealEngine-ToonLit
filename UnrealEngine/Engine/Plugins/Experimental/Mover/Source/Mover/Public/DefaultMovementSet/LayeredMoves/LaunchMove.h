// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LayeredMove.h"
#include "LaunchMove.generated.h"


/** Launch Move: provides an impulse velocity to the actor after (optionally) forcing them into a particular movement mode */
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_Launch : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_Launch();
	virtual ~FLayeredMove_Launch() {}

	// Velocity to apply to the actor. Could be additive or overriding depending on MixMode setting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta=(ForceUnits="cm/s"))
	FVector LaunchVelocity = FVector::ZeroVector;

	// Optional movement mode name to force the actor into before applying the impulse velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName ForceMovementMode = NAME_None;

	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};
