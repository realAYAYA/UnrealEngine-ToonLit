// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "BoneContainer.h"
#include "MLDeformerCurveReference.h"
#include "NeuralMorphTypes.generated.h"

/** 
 * The mode of the model, either local or global. 
 * In local mode the network contains a super simple neural network for each bone, while in 
 * global mode all bones and curves are input to one larger fully connected network.
 * The local mode has faster performance, while global mode can result in higher quality deformation.
 * This model runs its neural network on the CPU, but uses comrpessed GPU based morph targets, which require shader model 5.
 */
UENUM()
enum class ENeuralMorphMode : uint8
{
	/**
	 * Each bone creates a set of morph targets and has its own small neural network.
	 * The local mode can also create more localized morph targets and tends to use slightly less memory.
	 * This mode is faster to process on the CPU side.
	 */
	Local,

	/** 
	 * There is one fully connected neural network that generates a set of morph targets.
	 * This has a slightly higher CPU overhead, but could result in higher quality.
	 * The Global mode is basically the same as the Vertex Delta Model, but runs the neural network on the CPU
	 * and uses GPU compressed morph targets.
	 */
	Global
};

/**
 * A group of bones, which can generate morph targets together.
 * This is useful when there are specific correlations between different bones, that all effect the same region on the body.
 */
USTRUCT()
struct NEURALMORPHMODEL_API FNeuralMorphBoneGroup
{
	GENERATED_BODY()

public:
	/** The list of bones that should form a group together. */
	UPROPERTY(EditAnywhere, Category = "Bones")
	TArray<FBoneReference> BoneNames;
};

/**
 * A group of curves, which can generate morph targets together.
 * This is useful when there are specific correlations between different curves, that all effect the same region on the body.
 */
USTRUCT()
struct NEURALMORPHMODEL_API FNeuralMorphCurveGroup
{
	GENERATED_BODY()

public:
	/** The list of curves that should form a group together. */
	UPROPERTY(EditAnywhere, Category = "Curves")
	TArray<FMLDeformerCurveReference> CurveNames;
};
