// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralTensor.h"

/**
 * This is an auxiliary class. See UNeuralNetworkLegacy for a high-level wrapper of the whole NeuralNetworkInference plugin. The UNeuralNetworkLegacy header
 * documentation also includes some code examples.
 *
 * FNeuralTensorManager is an utility class that helps store all the FNeuralTensors related to a particular UNeuralNetworkLegacy model.
 * Most functions are inlined (or templated) to avoid the performance overhead of a wrapper class.
 */
class FNeuralTensorManager
{
public:
	FNeuralTensorManager();

	/**
	 * Calls FNeuralTensorManager() + Load(InNameIndexMap, InTensors, InInputNameIndexMap, InOutputNameIndexMap).
	 */
	FNeuralTensorManager(TArray<FNeuralTensor>& InTensors, TMap<FString, int32>& InNameIndexMap, TMap<FString, int32>& InInputNameIndexMap,
		TMap<FString, int32>& InOutputNameIndexMap);

	/**
	 * Calls FNeuralTensorManager() + Load(InNameIndexMap, InTensors, InInputTensors, InOutputTensors).
	 */
	FNeuralTensorManager(TArray<FNeuralTensor>& InTensors, const TArray<FNeuralTensor*>& InInputTensors, const TArray<FNeuralTensor*>& InOutputTensors);

	~FNeuralTensorManager();

	/**
	 * It moves the TMap objects and populates all other class members.
	 */
	bool Load(TArray<FNeuralTensor>& InTensors, TMap<FString, int32>& InNameIndexMap, TMap<FString, int32>& InInputNameIndexMap,
		TMap<FString, int32>& InOutputNameIndexMap);

	/**
	 * Alternative to the original Load(), recommended for single-input single-output models as well as any kind of network whose
	 * input and output tensor names are irrelevant). It will internally create InNameIndexMap/InInputNameIndexMap/InOutputNameIndexMap
	 * and call the original Load().
	 * @param InInputTensors Pointers for the tensors in InTensors that are inputs.
	 * @param InOutputTensors Pointers for the tensors in InTensors that are outputs.
	 */
	bool Load(TArray<FNeuralTensor>& InTensors, const TArray<FNeuralTensor*>& InInputTensors, const TArray<FNeuralTensor*>& InOutputTensors);

	/**
	 * It returns whether it loaded the UNeuralNetworkLegacy successfully.
	 */
	bool IsLoaded() const;

	/**
	 * Getter for the Tensors TArray. GetTensors() will return a const reference while GetTensorsMutable() allows modifying it.
	 */
	const TArray<FNeuralTensor>& GetTensors() const;
	TArray<FNeuralTensor>& GetTensorsMutable();

	/**
	 * Getter for the NameIndexMap TMap.
	 */
	const TMap<FString, int32>& GetNameIndexMap() const;

	/**
	 * It returns the non-input FNeuralTensor index TMap as a read-only object.
	 */
	const TArray<int32>& GetNonInputIndexes() const;

	/**
	 * It returns the input FNeuralTensor(s) as read-only (GetInputTensor/GetInputIndexes/GetInputNameIndexMap) or mutable (GetInputTensorMutable) objects.
	 * GetInputTensor/GetInputTensorMutable ensure there is only 1 input and returns a single FNeuralTensor&, while GetInputIndexes/GetInputNameIndexMap return them all.
	 * In order to modify their input data of the FNeuralTensor (map), use GetInputDataPointerMutable() or CreateInputDataPointersMutable().
	 *
	 * Use the read-only versions as much as possible to avoid potential undefined behavior in the next Run() of the FNeuralTensorManager due to non-controlled functions
	 * (e.g., resizes, memory re-allocation).
	 *
	 * @return GetInputTensor() returns a read-only const FNeuralTensor& corresponding to the single input FNeuralTensor, GetInputIndexes()/GetInputNameIndexMap() return a
	 * TArray<int32>/TMap<FString, int32> for each one of the input FNeuralTensors. They will all go out of scope if FNeuralTensorManager does.
	 */
	const FNeuralTensor& GetInputTensor() const;
	FNeuralTensor& GetInputTensorMutable();
	const TArray<int32>& GetInputIndexes() const;
	const TMap<FString, int32>& GetInputNameIndexMap() const;

	/**
	 * There are 6 alternative functions to fill the input FNeuralTensor(s) data:
	 * If exactly 1 input FNeuralTensor: SetInputFromArrayCopy(), SetInputFromTensorCopy(), GetInputDataPointerMutable().
	 *     - They ensure there is exactly 1 input FNeuralTensor or will log a warning if more than 1 input tensor exists.
	 * If more than 1 input FNeuralTensors: SetInputFromTensorMapCopy(), CreateInputDataPointersMutable().
	 *
	 * - SetInputFromArrayCopy()/SetInputFromTensorCopy()/SetInputFromTensorMapCopy() deeply copy the input FNeuralTensor(s) (slower but safer and less error
	 *   prone). See FNeuralTensor::SetFromArrayCopy() for more details.
	 * - GetInputDataPointerMutable() returns a pointer to the input FNeuralTensor raw data, so it can be filled before calling Run().
	 * - CreateInputDataPointersMutable() returns a TMap of pointers to the input FNeuralTensors raw data, so it can be filled before calling Run().
	 *
	 * @return GetInputDataPointerMutable() returns a T* corresponding to the mutable data in the single input FNeuralTensor, CreateInputDataPointersMutable() returns a
	 * TMap<FString, void*> for each one of the input FNeuralTensors.
	 *
	 * For read-only access to the input FNeuralTensor(s), see GetInputTensor() or GetInputNameIndexMap() (e.g., to extract properties
	 * from the input FNeuralTensor(s) such as volume, dimensions).
	 */
	template<typename T>
	void SetInputFromArrayCopy(const TArray<T>& InArray);
	void SetInputFromTensorCopy(const FNeuralTensor& InTensor);
	void SetInputFromTensorMapCopy(const TMap<FString, FNeuralTensor>& InTensorMap);
	template<typename T>
	T* GetInputDataPointerMutable();
	TMap<FString, void*> CreateInputDataPointersMutable();
	FRDGBufferUAVRef GetInputBufferUAVRef();
	TMap<FString, FRDGBufferUAVRef> CreateInputBufferUAVRefs();

	/**
	 * It returns the input FNeuralTensor(s) as read-only (GetOutputTensor/GetOutputIndexes/GetOutputNameIndexMap) or mutable (GetOutputTensorMutable) objects.
	 * GetOutputTensor/GetOutputTensorMutable ensure there is only 1 input and returns a single FNeuralTensor&, while GetOutputIndexes/GetOutputNameIndexMap return them all.
	 * In order to modify their input data of the FNeuralTensor (map), use GetOutputDataPointerMutable() or CreateOutputDataPointersMutable().
	 *
	 * Use the read-only versions as much as possible to avoid potential undefined behavior in the next Run() of the FNeuralTensorManager due to non-controlled functions
	 * (e.g., resizes, memory re-allocation).
	 *
	 * @return GetOutputTensor() returns a read-only const FNeuralTensor& corresponding to the single input FNeuralTensor, GetOutputIndexes()/GetOutputNameIndexMap() return a
	 * TArray<int32>/TMap<FString, int32> for each one of the input FNeuralTensors. They will all go out of scope if FNeuralTensorManager does.
	 */
	const FNeuralTensor& GetOutputTensor() const;
	FNeuralTensor& GetOutputTensorMutable();
	const TArray<int32>& GetOutputIndexes() const;
	const TMap<FString, int32>& GetOutputNameIndexMap() const;
	const FRDGBufferSRVRef GetOutputBufferSRVRef() const;
	TMap<FString, const FRDGBufferSRVRef> CreateOutputBufferSRVRefs() const;

	/**
	 * These functions are slower than the rest, because they deep copy each one of the TArray's in the final TMap.
	 */
	TMap<FString, FNeuralTensor> CreateInputTensorMap() const;
	TMap<FString, FNeuralTensor> CreateOutputTensorMap() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	bool bIsLoaded;
	/**
	 * It contains the raw weight, intermediate, input, and output FNeuralTensor's. Inlined FNeuralTensor's will only appear once on Tensors.
	 * E.g., in the example described for NameIndexMap, the FNeuralTensor will be present with the Key name "Input" (or otherwise "Output"). E.g.,
	 * FNeuralTensor& Tensor = Tensors[NameIndexMap.FindChecked(TEXT("Input"))];
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FNeuralTensor> Tensors;
	/**
	 * For each original FNeuralTensor name, it saves its final name after merging FNeuralTensor's for memory savings.
	 * E.g., For an inlined ReLU operator with original input tensor name "Input" and output tensor name "Output", NameIndexMap will
	 * contain the following 2 fields: {"Input", 0} and {"Output", 0}.
	 * E.g., For a non-inlined ReLU: {"Input", 0} and {"Output", 1}.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TMap<FString, int32> NameIndexMap;
	/**
	 * Other variables given in Load().
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TMap<FString, int32> InputNameIndexMap;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TMap<FString, int32> OutputNameIndexMap;
	/**
	 * Other variables auto-generated in Load().
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<int32> InputIndexes;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<int32> NonInputIndexes;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<int32> OutputIndexes;

	bool SanityCheckIsLoaded() const;
};



/* FNeuralTensorManager inlined and templated functions
 *****************************************************************************/

template<typename T>
void FNeuralTensorManager::SetInputFromArrayCopy(const TArray<T>& InArray)
{
	return GetInputTensorMutable().SetFromArrayCopy(InArray);
}

template<typename T>
T* FNeuralTensorManager::GetInputDataPointerMutable()
{
	// GetInputTensorMutable() already runs the sanity checks
	return GetInputTensorMutable().GetDataCasted<T>();
}
