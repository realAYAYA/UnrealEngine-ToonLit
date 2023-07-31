// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerTrainingModel.h"
#include "VertexDeltaTrainingModel.generated.h"

/**
 * The training model for the vertex delta model.
 * This class is our link to the Python training.
 */
UCLASS(Blueprintable)
class VERTEXDELTAMODELEDITOR_API UVertexDeltaTrainingModel
	: public UMLDeformerTrainingModel
{
	GENERATED_BODY()

public:
	/**
	 * Main training function, with implementation in python.
	 * You need to implement this method. See the UMLDeformerTrainingModel documentation for more details.
	 * @see UMLDeformerTrainingModel
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Python")
	int32 Train() const;
};
