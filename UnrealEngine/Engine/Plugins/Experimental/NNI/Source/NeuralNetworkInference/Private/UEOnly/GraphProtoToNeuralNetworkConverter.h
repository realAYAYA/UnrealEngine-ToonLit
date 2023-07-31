// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelProto.h"
#include "NeuralOperator.h"
#include "NeuralTensor.h"
#include "NeuralTensorManager.h"

class FGraphProtoToNeuralNetworkConverter
{
public:
	/**
	 * It takes a FGraphProto and model path as inputs, and produces an array of FNeuralOperator's and FNeuralTensorManager as result.
	 * These 2 outputs are used by UNeuralNetworkLegacy to create and load a (deep) neural network architecture and its weights.
	 * @param InTensorManager It is const, but kept not const for CreateOperatorsAndEditTensorArray() compatibility.
	 *
	 * 2 alternatives:
	 * If bInIsTensorManagerConst == true, InOrOutTensorManager is assumed to be a const input variable (assuming the tensors are all already loaded, e.g., on de-serialization).
	 * With InModelPath, InOrOutTensorManager is assumed to be an output variable that will be filled based on InModelPath.
	 */
	static bool Translate(TArray<TSharedPtr<FNeuralOperator>>& OutOperators, FNeuralTensorManager& InOrOutTensorManager, const FGraphProto& InGraphProto, const bool bInIsTensorManagerConst);

private:
	/**
	 * Auxiliary function for Translate().
	 * It initializes both OutOperators and OutputNameIndexMap, as well as modify InOutTensors and InOutNameIndexMap.
	 * OutputNameIndexMap is just the copy of InOutputNameDummyIndexMap while filling its int32 Value fields. Given the dual Game/Editor behaviors, and to optimize Game mode, kept as 2 different rather than modifying in-place as InOutOutputNameDummyIndexMap.
	 *
	 * 2 alternatives:
	 * - If bInIsTensorManagerConst == true, InOrOutTensorManager is assumed to be a const input variable (assuming the tensors are all already loaded, e.g., on de-serialization).
	 * - If bInIsTensorManagerConst == false, InOrOutTensorManager is assumed to be an output variable that will be filled based on InModelPath.
	 *
	 * @param OutputNameIndexMap It is created from InOutputNameDummyIndexMap, assuming the FString part of the mapping is right, and it will fill the index (int32 part of it).
	 * @param InInputNameDummyIndexMap The int32 part is irrelevant, the only important part is whether the FString field is filled or not, regardless of its value. Kept as TMap for performance to integrate better with FTensorManager.
	 * @param InOutputNameDummyIndexMap The int32 part is irrelevant, the only important part is whether the FString field is filled or not, regardless of its value. Kept as TMap for performance to integrate better with FTensorManager.
	 */
	static bool CreateOperatorsAndEditTensorArray(TArray<TSharedPtr<FNeuralOperator>>& OutOperators, TMap<FString, int32>& OutputNameIndexMap, TArray<FNeuralTensor>& InOutTensors,
		TMap<FString, int32>& InOutNameIndexMap, const TMap<FString, int32>& InInputNameDummyIndexMap, const TMap<FString, int32>& InOutputNameDummyIndexMap,
		const FGraphProto& InGraphProto, const bool bInIsTensorManagerConst);

	/**
	 * It will return FNeuralOperator::EDataType::NotInlined in any of the 2 following cases occur:
	 * - If the tensor is an EGPUDataType::Output.
	 * - If the tensor is used by another layer.
	 * It will return FNeuralOperator::EDataType::Inlined otherwise.
	 */
	static bool CanTensorBeInlined(const FString& InTensorName, const TMap<FString, int32>& InInputNameDummyIndexMap,
		const TMap<FString, int32>& InOutputNameDummyIndexMap, const FNodeProto& InNodeProto, const FGraphProto& InGraphProto, const bool bCanOutputPropertiesChangeWithPostForward = false);
	/**
	 * It will return which input tensors can be inlined.
	 */
	static TSet<uint32> GetPotentiallyInlinedTensors(const TArray<FString>& InTensorNames, const TMap<FString, int32>& InInputNameDummyIndexMap,
		const TMap<FString, int32>& InOutputNameDummyIndexMap, const FNodeProto& InNodeProto, const FGraphProto& InGraphProto);

	/**
	 * Given the input arguments, it creates the proper TShared<FNeuralOperator> child instance.
	 */
	static TSharedPtr<FNeuralOperator> CreateOperator(const FNodeProto& InNodeProto, const TArray<FString>& InInputTensorNamesForOperator,
		const TMap<FString, int32>& InInputNameDummyIndexMap, const TMap<FString, int32>& InOutputNameDummyIndexMap, const FGraphProto& InGraphProto);
};
