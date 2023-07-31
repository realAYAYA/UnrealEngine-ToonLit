// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralInt64ArrayUInt32Buffer.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"



/* FNeuralInt64ArrayUInt32Buffer structors
 *****************************************************************************/

FNeuralInt64ArrayUInt32Buffer::FNeuralInt64ArrayUInt32Buffer(const TArray<int64>& InInt64Array)
{
	Copy(InInt64Array);
}

FNeuralInt64ArrayUInt32Buffer::FNeuralInt64ArrayUInt32Buffer(const TArray<uint32>& InUInt32Array, const bool bShouldCreate64BitVersion)
{
	Copy(InUInt32Array, bShouldCreate64BitVersion);
}



/* FNeuralInt64ArrayUInt32Buffer public functions
 *****************************************************************************/

const TArray<int64>& FNeuralInt64ArrayUInt32Buffer::GetInt64() const
{
	if (Int64Array.IsEmpty())
	{
		UE_LOG(LogNeuralNetworkInference, Warning,
			TEXT("FNeuralInt64ArrayUInt32Buffer::GetInt64(): Int64Array is empty. Move() might have been never called or called with bShouldCreate64BitVersion = false."));
	}
	return Int64Array;
}

void FNeuralInt64ArrayUInt32Buffer::Init(const uint32 InArrayValue, const uint32 InArraySize, const bool bShouldCreate64BitVersion)
{
	TArray<uint32> UInt32ArrayTemp;
	UInt32ArrayTemp.Init(InArrayValue, InArraySize);
	Move(UInt32ArrayTemp, bShouldCreate64BitVersion);
}

void FNeuralInt64ArrayUInt32Buffer::Copy(const TArray<int64>& InInt64Array)
{
	TArray<int64> Int64ArrayTemp = InInt64Array;
	Move(Int64ArrayTemp);
}

void FNeuralInt64ArrayUInt32Buffer::Copy(const TArray<uint32>& InUInt32Array, const bool bShouldCreate64BitVersion)
{
	TArray<uint32> UInt32ArrayTemp = InUInt32Array;
	Move(UInt32ArrayTemp, bShouldCreate64BitVersion);
}

void FNeuralInt64ArrayUInt32Buffer::Move(TArray<int64>& InInt64Array)
{
	// Int64Array
	Swap(Int64Array, InInt64Array);
	// UInt32Array
	UInt32Array.SetNumUninitialized(Int64Array.Num());
	for (int32 Index = 0; Index < UInt32Array.Num(); ++Index)
	{
		UInt32Array[Index] = (uint32)Int64Array[Index];
	}
}

void FNeuralInt64ArrayUInt32Buffer::Move(TArray<uint32>& InUInt32Array, const bool bShouldCreate64BitVersion)
{
	// UInt32Array
	Swap(UInt32Array, InUInt32Array);
	// Int64Array
	if (bShouldCreate64BitVersion)
	{
		Int64Array.SetNumUninitialized(UInt32Array.Num());
		for (int32 Index = 0; Index < Int64Array.Num(); ++Index)
		{
			Int64Array[Index] = (int64)UInt32Array[Index];
		}
	}
}

void FNeuralInt64ArrayUInt32Buffer::CreateAndLoadSRVBuffer(const TCHAR* InDebugName)
{
	// Sanity check
	if (UInt32ReadBuffer.IsValid())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralInt64ArrayUInt32Buffer::CreateAndLoadSRVBuffer(): UInt32ReadBuffer was not nullptr."));
	}
	// Create GPU memory for it
	FNeuralNetworkInferenceUtilsGPU::CreateAndLoadSRVBuffer(UInt32ReadBuffer, UInt32Array, InDebugName);
}
