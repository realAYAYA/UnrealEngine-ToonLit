// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralNetwork.h"
#include "NeuralTensor.h"
#include "NeuralNetworkInferenceQAAsset.generated.h"



USTRUCT(BlueprintType)
struct NEURALNETWORKINFERENCEQA_API FNeuralNetworkInferenceQAOperatorTestAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference QA")
	TArray<FNeuralTensor> InputTensors;
	
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference QA")
	TArray<FNeuralTensor> OutputTensors;

	/**
	 * It adds the InNameIndexMap subset of InTensors into Tensors.
	 */
	void AddInputTensors(const UNeuralNetwork* const InNetwork);
	void AddOutputTensors(const UNeuralNetwork* const InNetwork);

	/**
	 * It will compare new vs. previous tests.
	 */
	static bool CompareAverageL1DiffNewVsPreviousTests(const FNeuralNetworkInferenceQAOperatorTestAsset& InOperatorTestAsset1,
		const FNeuralNetworkInferenceQAOperatorTestAsset& InOperatorTestAsset2, const FString& InOperatorName);

private:
	/**
	 * Auxiliary for CompareAverageL1DiffNewVsPreviousTests(const FNeuralNetworkInferenceQAOperatorTestAsset&, const FNeuralNetworkInferenceQAOperatorTestAsset&).
	 */
	static bool CompareAverageL1DiffNewVsPreviousTests(const TArray<FNeuralTensor>& InTensorsA, const TArray<FNeuralTensor>& InTensorsB, const FString& InOperatorName);
};



USTRUCT(BlueprintType)
struct NEURALNETWORKINFERENCEQA_API FNeuralNetworkInferenceQAOperatorAsset
{
	GENERATED_BODY()

public:
	static FString RunNetworkCPUAndGetString(UNeuralNetwork* InOutNetwork);

	void RunAndAddTest(UNeuralNetwork* InOutNetwork);

	/**
	 * It will compare new vs. previous tests.
	 */
	bool CompareNewVsPreviousTests(const FString& InGroundTruthDirectory, const FString& InOperatorName);

	/**
	 * It flushes (cleans/resets) the new results so the tests can be run again.
	 */
	void FlushNewTests();
	
protected:
	FString NewTestsString;

	TArray<FNeuralNetworkInferenceQAOperatorTestAsset> NewTests;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference QA")
	TArray<FNeuralNetworkInferenceQAOperatorTestAsset> PreviousTests;

	UPROPERTY()
	FString PreviousTestsString;
};



/**
 * This is an auxiliary class. See UNeuralNetwork for a high-level wrapper of the whole NeuralNetworkInference plugin. The UNeuralNetwork header
 * documentation also includes some code examples.
 */
UCLASS(BlueprintType)
class NEURALNETWORKINFERENCEQA_API UNeuralNetworkInferenceQAAsset : public UObject
{
	GENERATED_BODY()

public:
	void FindOrAddOperators(const TArray<FString>& InOperatorNames);

	void RunAndAddTest(UNeuralNetwork* InOutNetwork, const FString& OperatorName);

	bool CompareNewVsPreviousTests(const FString& InGroundTruthDirectory);

	/**
	 * It resets the new results so it can be run again.
	 */
	void FlushNewTests();

	static UNeuralNetworkInferenceQAAsset* Load(const FString& InNeuralNetworkInferenceQAAssetParentDirectoryName, const FString& InNeuralNetworkInferenceQAAssetName);

	bool Save();

protected:
	UPROPERTY(EditAnywhere, Category = "Neural Network Inference QA")
	TMap<FString, FNeuralNetworkInferenceQAOperatorAsset> Operators;
};
