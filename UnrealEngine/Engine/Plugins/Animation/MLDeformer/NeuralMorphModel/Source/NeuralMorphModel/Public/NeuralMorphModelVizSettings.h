// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelVizSettings.h"
#include "NeuralMorphModelVizSettings.generated.h"

/**
 * The vizualization settings specific to this model.
 * Even if we have no new properties compared to the morph model, we still need to
 * create this class in order to propertly register a detail customization for it in our editor module.
 */
UCLASS()
class NEURALMORPHMODEL_API UNeuralMorphModelVizSettings
	: public UMLDeformerMorphModelVizSettings
{
	GENERATED_BODY()
};
