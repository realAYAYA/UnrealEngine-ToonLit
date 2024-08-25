// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "MLDeformerGeomCacheTrainingModel.h"
#include "NeuralMorphTrainingModel.generated.h"

/**
 * The training model for the neural morph model.
 * This class is our link to the Python training.
 */
UCLASS(Blueprintable)
class NEURALMORPHMODELEDITOR_API UNeuralMorphTrainingModel
	: public UMLDeformerGeomCacheTrainingModel
{
	GENERATED_BODY()

public:
	/**
	 * Main training function, with implementation in python.
	 * You need to implement this method. See the UMLDeformerTrainingModel documentation for more details.
	 * @see UMLDeformerTrainingModel
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Training Model")
	int32 Train() const;

	UFUNCTION(BlueprintPure, Category = "Training Model")
	int32 GetNumBoneGroups() const;

	UFUNCTION(BlueprintPure, Category = "Training Model")
	int32 GetNumCurveGroups() const;

	UFUNCTION(BlueprintPure, Category = "Training Model")
	TArray<int32> GenerateBoneGroupIndices() const;

	UFUNCTION(BlueprintPure, Category = "Training Model")
	TArray<int32> GenerateCurveGroupIndices() const;

	UFUNCTION(BlueprintPure, Category = "Training Model")
	TArray<float> GetMorphTargetMasks() const;
};
