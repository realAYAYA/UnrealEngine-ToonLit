// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/GraphExecuteContext.h"
#include "Graph/AnimNext_LODPose.h"
#include "Graph/AnimGraph/AnimNode_AnimNextGraph.h"	// TEST - until we can allocate per-node state again
#include "RigUnit_AnimNextAnimSequence.generated.h"

class UAnimSequenceBase;

USTRUCT(BlueprintType, meta = (DisplayName = "AnimSequence"))
struct FAnimNextGraph_AnimSequence
{
	GENERATED_BODY()

	FAnimNextGraph_AnimSequence() = default;

	explicit FAnimNextGraph_AnimSequence(const UAnimSequenceBase* InAnimSequence)
		: AnimSequence(InAnimSequence)
	{
	}

	const UAnimSequenceBase* AnimSequence = nullptr;
};

struct FAnimNextUnitContext;
class UAnimNextGraph;
class UAnimSequenceBase;


// --- FRigUnit_AnimNext_AnimSequenceAsset ---

/** Unit for getting an asset reference we can pass to a player or data extractor */
USTRUCT(meta = (DisplayName = "Anim Sequence Asset", Category = "Animation", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_AnimNext_AnimSequenceAsset : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Execute();

protected:
	// TODO : Enable once we have RigVM object support enabled
	//UPROPERTY(EditAnywhere, Category = "Asset", meta = (Input, EditInline, PinHiddenByDefault, DisallowedClasses = "/Script/Engine.AnimMontage"))
	//TObjectPtr<UAnimSequenceBase> AnimSequence = nullptr;

	UPROPERTY(EditAnywhere, Category = "Animation", DisplayName = "AnimSequence", meta = (Output))
	FAnimNextGraph_AnimSequence Sequence;
};

// --- FRigUnit_AnimNext_AnimSequencePlayer --- 

USTRUCT(BlueprintType)
struct FAnimSequenceParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Anim Sequence")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Anim Sequence")
	float StartPosition = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Anim Sequence")
	bool bLoop = 0.0f;
};

/** Unit for getting a pose from an animation sequence */
USTRUCT(meta = (DisplayName = "AnimSequencePlayer", Category = "Animation", TitleColor = "1 0 0", NodeColor = "1 1 1"))
struct FRigUnit_AnimNext_AnimSequencePlayer : public FRigUnit_AnimNextBase
{
	GENERATED_BODY()

	RIGVM_METHOD()
	void Initialize();

	RIGVM_METHOD()
	void Execute();

protected:
	UPROPERTY(EditAnywhere, Category = "Anim Interface", meta = (Input))
	FAnimSequenceParameters Parameters;

	// The animation sequence asset to play
	UPROPERTY(EditAnywhere, Category = "Animation", DisplayName = "AnimSequence", meta = (Input))
	FAnimNextGraph_AnimSequence Sequence;

	// The output pose generated
	UPROPERTY(EditAnywhere, Transient, Category = "Animation", DisplayName = "Result", meta = (Output))
	FAnimNextGraphLODPose LODPose;
};
