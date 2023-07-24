// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NeuralNetworkInferenceQA.generated.h"

/*
 * BP wrapper of FUnitTester. It contains the main functions to test this plugin.
 */
UCLASS(BlueprintType)
class NEURALNETWORKINFERENCEQA_API UNeuralNetworkInferenceQA : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NeuralNetworkInferenceQA")
	static bool UnitTesting();
};
