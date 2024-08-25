// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "LayeredMove.h"
#include "AnimRootMotionLayeredMove.generated.h"

class UAnimMontage;


/** Anim Root Motion Move: handles root motion from a montage played on the associated mesh */
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_AnimRootMotion : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_AnimRootMotion();
	virtual ~FLayeredMove_AnimRootMotion() {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UAnimMontage> Montage;

	// Montage position when started (in unscaled seconds). 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float StartingMontagePosition;

	// Rate at which this montage is intended to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float PlayRate;

	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};
