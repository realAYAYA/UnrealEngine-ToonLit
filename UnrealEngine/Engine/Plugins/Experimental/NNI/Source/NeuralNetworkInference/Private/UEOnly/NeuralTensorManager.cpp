// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralTensorManager.h"
#include "NeuralNetworkInferenceUtils.h"



/* FPrivateNeuralTensorManager
 *****************************************************************************/

class FPrivateNeuralTensorManager
{
public:
	static TMap<FString, int32> TensorArrayToNameIndexMap(const TArray<FNeuralTensor>& InTensors, const TArray<FNeuralTensor*>& InTensorArray, const FString& InPrefix);
	static void TensorNameIndexMapToArray(TArray<int32>& OutIndexes, const TMap<FString, int32>& InNameIndexMap);
};

TMap<FString, int32> FPrivateNeuralTensorManager::TensorArrayToNameIndexMap(const TArray<FNeuralTensor>& InTensors, const TArray<FNeuralTensor*>& InTensorArray, const FString& InPrefix)
{
	if (InTensors.Num() >= InTensorArray.Num())
	{
		TMap<FString, int32> NameIndexMap;
		int32 Counter = -1;
		for (const FNeuralTensor* const Tensor : InTensorArray)
		{
			for (int32 TensorIndex = 0; TensorIndex < InTensors.Num(); ++TensorIndex)
			{
				if (&InTensors[TensorIndex] == Tensor)
				{
					NameIndexMap.Add(InPrefix + FString::FromInt(++Counter), TensorIndex);
					break;
				}
			}
		}
		if (NameIndexMap.Num() != InTensorArray.Num())
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensorManager::TensorArrayToNameIndexMap(): NameIndexMap.Num() == InTensorArray.Num() failed (%d != %d)."),
				NameIndexMap.Num(), InTensorArray.Num());
			return TMap<FString, int32>();
		}
		return NameIndexMap;
	}
	else
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensorManager::TensorArrayToNameIndexMap(): InTensors.Num() >= InTensorArray.Num() failed (%d vs. %d)."),
			InTensors.Num(), InTensorArray.Num());
		return TMap<FString, int32>();
	}
}

void FPrivateNeuralTensorManager::TensorNameIndexMapToArray(TArray<int32>& OutIndexes, const TMap<FString, int32>& InNameIndexMap)
{
	OutIndexes.Empty();
	for (const auto& NameIndexPair : InNameIndexMap)
	{
		// Add if tensor index only once
		if (OutIndexes.Find(NameIndexPair.Value) == INDEX_NONE)
		{
			OutIndexes.Push(NameIndexPair.Value);
		}
	}
	Algo::Sort(OutIndexes);
}



/* FNeuralTensorManager structors
 *****************************************************************************/

FNeuralTensorManager::FNeuralTensorManager()
	: bIsLoaded(false)
{
}

FNeuralTensorManager::FNeuralTensorManager(TArray<FNeuralTensor>& InTensors, TMap<FString, int32>& InNameIndexMap, TMap<FString, int32>& InInputNameIndexMap,
	TMap<FString, int32>& InOutputNameIndexMap)
	: FNeuralTensorManager()
{
	Load(InTensors, InNameIndexMap, InInputNameIndexMap, InOutputNameIndexMap);
}

FNeuralTensorManager::FNeuralTensorManager(TArray<FNeuralTensor>& InTensors, const TArray<FNeuralTensor*>& InInputTensors,
	const TArray<FNeuralTensor*>& InOutputTensors)
	: FNeuralTensorManager()
{
	Load(InTensors, InInputTensors, InOutputTensors);
}

FNeuralTensorManager::~FNeuralTensorManager()
{
}



/* FNeuralTensorManager public functions
 *****************************************************************************/

bool FNeuralTensorManager::Load(TArray<FNeuralTensor>& InTensors, TMap<FString, int32>& InNameIndexMap,
	TMap<FString, int32>& InInputNameIndexMap, TMap<FString, int32>& InOutputNameIndexMap)
{
	bIsLoaded = false;
	// Sanity checks
	if (InTensors.Num() < InInputNameIndexMap.Num() || InTensors.Num() < InOutputNameIndexMap.Num())
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralTensorManager::Load(): InTensors.Num() >= InputNameIndexMap.Num() (%d vs. %d) || InTensors.Num() >= OutputNameIndexMap.Num() (%d vs. %d) failed."),
			InTensors.Num(), InInputNameIndexMap.Num(), InTensors.Num(), InOutputNameIndexMap.Num());
		return bIsLoaded;
	}
	else if (InInputNameIndexMap.Num() < 1 || InOutputNameIndexMap.Num() < 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("There should be at least 1 input (currently %d) and 1 output (currently %d) tensors."), InInputNameIndexMap.Num(), InOutputNameIndexMap.Num());
		return bIsLoaded;
	}
	// Fill NameIndexMap, Tensors, InputNameIndexMap, and OutputNameIndexMap
	Swap(Tensors, InTensors);
	Swap(NameIndexMap, InNameIndexMap);
	Swap(InputNameIndexMap, InInputNameIndexMap);
	Swap(OutputNameIndexMap, InOutputNameIndexMap);
	// InputIndexes & OutputIndexes
	FPrivateNeuralTensorManager::TensorNameIndexMapToArray(InputIndexes, InputNameIndexMap);
	FPrivateNeuralTensorManager::TensorNameIndexMapToArray(OutputIndexes, OutputNameIndexMap);
	// NonInputIndexes
	NonInputIndexes.Empty();
	for (int32 TensorIndex = 0; TensorIndex < Tensors.Num(); ++TensorIndex )
	{
		// Add if tensor index is not an input
		if (InputIndexes.Find(TensorIndex) == INDEX_NONE)
		{
			NonInputIndexes.Push(TensorIndex);
		}
	}
	if (InputIndexes.Num() + NonInputIndexes.Num() != Tensors.Num())
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralTensorManager::Load(): InputIndexes.Num() + NonInputIndexes.Num() == Tensors.Num() failed (%d + %d != %d), NameIndexMap.Num() = %d)."),
			InputIndexes.Num(), NonInputIndexes.Num(), Tensors.Num(), NameIndexMap.Num());
		return bIsLoaded;
	}
	// FNeuralTensorManager was configured
	bIsLoaded = true;
	return bIsLoaded;
}

bool FNeuralTensorManager::Load(TArray<FNeuralTensor>& InTensors, const TArray<FNeuralTensor*>& InInputTensors,
	const TArray<FNeuralTensor*>& InOutputTensors)
{
	TMap<FString, int32> InputNameIndexMapLocal = FPrivateNeuralTensorManager::TensorArrayToNameIndexMap(InTensors, InInputTensors, TEXT("i"));
	TMap<FString, int32> OutputNameIndexMapLocal = FPrivateNeuralTensorManager::TensorArrayToNameIndexMap(InTensors, InOutputTensors, TEXT("o"));
	TMap<FString, int32> NameIndexMapLocal = InputNameIndexMapLocal;
	NameIndexMapLocal.Append(OutputNameIndexMapLocal);
	// Missing FNeuralTensors (weight & intermediate ones)
	if (NameIndexMapLocal.Num() < InTensors.Num())
	{
		TArray<int32> IndexesLocal;
		FPrivateNeuralTensorManager::TensorNameIndexMapToArray(IndexesLocal, NameIndexMapLocal);
		int32 Counter = -1;
		for (int32 TensorIndex = 0; TensorIndex < InTensors.Num(); ++TensorIndex)
		{
			// If not an output nor input --> Add with new name
			if (IndexesLocal.Find(TensorIndex) == INDEX_NONE)
			{
				NameIndexMap.Add(TEXT("t") + FString::FromInt(++Counter), TensorIndex);
			}
		}
	}
	// Call the original Load() with the new TMaps
	return Load(InTensors, NameIndexMapLocal, InputNameIndexMapLocal, OutputNameIndexMapLocal);
}

bool FNeuralTensorManager::IsLoaded() const
{
	return bIsLoaded;
}

const TArray<FNeuralTensor>& FNeuralTensorManager::GetTensors() const
{
	SanityCheckIsLoaded();
	return Tensors;
}

TArray<FNeuralTensor>& FNeuralTensorManager::GetTensorsMutable()
{
	SanityCheckIsLoaded();
	return Tensors;
}

const TMap<FString, int32>& FNeuralTensorManager::GetNameIndexMap() const
{
	SanityCheckIsLoaded();
	return NameIndexMap;
}

const TArray<int32>& FNeuralTensorManager::GetNonInputIndexes() const
{
	SanityCheckIsLoaded();
	return NonInputIndexes;
}

const FNeuralTensor& FNeuralTensorManager::GetInputTensor() const
{
	SanityCheckIsLoaded();
	if (InputIndexes.Num() != 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralTensorManager::GetInputTensor(): There should be 1 input, not %d. Call GetInputIndexes() instead."), InputIndexes.Num());
	}
	checkf(Tensors.Num() > 0 && InputIndexes.Num() > 0, TEXT("Tensors and/or InputIndexes was empty."));
	return Tensors[InputIndexes[0]];
}

FNeuralTensor& FNeuralTensorManager::GetInputTensorMutable()
{
	SanityCheckIsLoaded();
	if (InputIndexes.Num() != 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralTensorManager::GetInputTensorMutable(): There should be 1 input, not %d. Call GetInputIndexes() instead."), InputIndexes.Num());
	}
	checkf(Tensors.Num() > 0 && InputIndexes.Num() > 0, TEXT("Tensors and/or InputIndexes was empty."));
	return Tensors[InputIndexes[0]];
}

const TArray<int32>& FNeuralTensorManager::GetInputIndexes() const
{
	SanityCheckIsLoaded();
	return InputIndexes;
}

const TMap<FString, int32>& FNeuralTensorManager::GetInputNameIndexMap() const
{
	SanityCheckIsLoaded();
	return InputNameIndexMap;
}

void FNeuralTensorManager::SetInputFromTensorCopy(const FNeuralTensor& InTensor)
{
	return GetInputTensorMutable().SetFromUnderlyingUInt8ArrayCopy(InTensor.GetUnderlyingUInt8ArrayRef());
}

void FNeuralTensorManager::SetInputFromTensorMapCopy(const TMap<FString, FNeuralTensor>& InTensorMap)
{
	TArray<FNeuralTensor>& TensorsLocalRef = GetTensorsMutable();
	for (const auto& IndexMapTuple : GetInputNameIndexMap())
	{
		TensorsLocalRef[IndexMapTuple.Value].SetFromUnderlyingUInt8ArrayCopy(InTensorMap.FindChecked(IndexMapTuple.Key).GetUnderlyingUInt8ArrayRef());
	}
}

TMap<FString, void*> FNeuralTensorManager::CreateInputDataPointersMutable()
{
	TMap<FString, void*> InputDataPointers;
	if (SanityCheckIsLoaded())
	{
		for (auto& NameIndexPair : GetInputNameIndexMap())
		{
			InputDataPointers.Add(NameIndexPair.Key, Tensors[NameIndexPair.Value].GetData());
		}
	}
	return InputDataPointers;
}

FRDGBufferUAVRef FNeuralTensorManager::GetInputBufferUAVRef()
{
	return GetInputTensorMutable().GetBufferUAVRef();
}

TMap<FString, FRDGBufferUAVRef> FNeuralTensorManager::CreateInputBufferUAVRefs()
{
	TMap<FString, FRDGBufferUAVRef> InputBufferUAVMap;
	if (SanityCheckIsLoaded())
	{
		for (auto& NameIndexPair : GetInputNameIndexMap())
		{
			InputBufferUAVMap.Add(NameIndexPair.Key, Tensors[NameIndexPair.Value].GetBufferUAVRef());
		}
	}
	return InputBufferUAVMap;
}

const FNeuralTensor& FNeuralTensorManager::GetOutputTensor() const
{
	SanityCheckIsLoaded();
	if (OutputIndexes.Num() != 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralTensorManager::GetOutputTensor(): There should be 1 input, not %d. Call GetOutputIndexes() instead."), OutputIndexes.Num());
	}
	checkf(Tensors.Num() > 0 && OutputIndexes.Num() > 0, TEXT("Tensors and/or OutputIndexes was empty."));
	return Tensors[OutputIndexes[0]];
}

FNeuralTensor& FNeuralTensorManager::GetOutputTensorMutable()
{
	SanityCheckIsLoaded();
	if (OutputIndexes.Num() != 1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralTensorManager::GetOutputTensorMutable(): There should be 1 input, not %d. Call GetOutputIndexes() instead."), OutputIndexes.Num());
	}
	checkf(Tensors.Num() > 0 && OutputIndexes.Num() > 0, TEXT("Tensors and/or OutputIndexes was empty."));
	return Tensors[OutputIndexes[0]];
}

const TArray<int32>& FNeuralTensorManager::GetOutputIndexes() const
{
	SanityCheckIsLoaded();
	return OutputIndexes;
}

const TMap<FString, int32>& FNeuralTensorManager::GetOutputNameIndexMap() const
{
	SanityCheckIsLoaded();
	return OutputNameIndexMap;
}

const FRDGBufferSRVRef FNeuralTensorManager::GetOutputBufferSRVRef() const
{
	return GetOutputTensor().GetBufferSRVRef();
}

TMap<FString, const FRDGBufferSRVRef> FNeuralTensorManager::CreateOutputBufferSRVRefs() const
{
	TMap<FString, const FRDGBufferSRVRef> OutputBufferSRVMap;
	if (SanityCheckIsLoaded())
	{
		for (const auto& NameIndexPair : GetOutputNameIndexMap())
		{
			OutputBufferSRVMap.Add(NameIndexPair.Key, Tensors[NameIndexPair.Value].GetBufferSRVRef());
		}
	}
	return OutputBufferSRVMap;
}

TMap<FString, FNeuralTensor> FNeuralTensorManager::CreateInputTensorMap() const
{
	TMap<FString, FNeuralTensor> InputArrayMap;
	if (SanityCheckIsLoaded())
	{
		for (const auto& NameIndexPair : GetInputNameIndexMap())
		{
			InputArrayMap.Add(NameIndexPair.Key, Tensors[NameIndexPair.Value]);
		}
	}
	return InputArrayMap;
}

TMap<FString, FNeuralTensor> FNeuralTensorManager::CreateOutputTensorMap() const
{
	TMap<FString, FNeuralTensor> OutputArrayMap;
	if (SanityCheckIsLoaded())
	{
		for (const auto& NameIndexPair : GetOutputNameIndexMap())
		{
			OutputArrayMap.Add(NameIndexPair.Key, Tensors[NameIndexPair.Value]);
		}
	}
	return OutputArrayMap;
}



/* FNeuralTensorManager private functions
 *****************************************************************************/

bool FNeuralTensorManager::SanityCheckIsLoaded() const
{
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralTensorManager::SanityCheckIsLoaded(): Not loaded."));
	}
	return bIsLoaded;
}
