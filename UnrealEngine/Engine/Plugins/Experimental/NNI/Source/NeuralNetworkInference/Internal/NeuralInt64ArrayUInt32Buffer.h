// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

/**
 * HLSL uses 32-bit arrays, while FNeuralTensor uses 64-bit ones. This auxiliary class wraps them both together to make sure we
 * do not modify one type without modifying the other one.
 * There are 2 ways to use this class, depending on the value of bShouldCreate64BitVersion in Move(TArray<uint32>& InUInt32Array, const bool bShouldCreate64BitVersion):
 * 1. If bShouldCreate64BitVersion = true, both the 32 and 64 bit TArrays will be created. Useful when both will be read.
 * 2. If bShouldCreate64BitVersion = false, only the 32 bit TArray will be filled. Useful when the 64 bit one will not be used.
 * GPU-wise, UInt32ReadBuffer will only be filled if/when CreateAndLoadSRVBuffer() is called. You can use IsReadBuffer() to know if that has happened yet.
 */
class NEURALNETWORKINFERENCE_API FNeuralInt64ArrayUInt32Buffer
{
public:
	FNeuralInt64ArrayUInt32Buffer() = default;
	/**
	 * Equivalent to calling Copy().
	 */
	FNeuralInt64ArrayUInt32Buffer(const TArray<int64>& InInt64Array);
	FNeuralInt64ArrayUInt32Buffer(const TArray<uint32>& InUInt32Array, const bool bShouldCreate64BitVersion);

	FORCEINLINE int32 Num() const;

	FORCEINLINE bool IsEmpty() const;

	FORCEINLINE const TArray<uint32>& GetUInt32() const;

	/**
	 * In addition to return the TArray, it also checks whether bShouldCreate64BitVersion was true when Move() was called.
	 */
	const TArray<int64>& GetInt64() const;

	FORCEINLINE const FShaderResourceViewRHIRef& GetUInt32SRV() const;

	FORCEINLINE bool IsReadBufferValid() const;

	FORCEINLINE void ReleaseReadBuffer();

	FORCEINLINE void ResetReadBuffer();

	/**
	 * Create and TArray::Init() UInt32Array and optionally Int64Array.
	 * 32-bit version is always generated, 64 only if bShouldCreate64BitVersion = true.
	 */
	void Init(const uint32 InArrayValue, const uint32 InArraySize, const bool bShouldCreate64BitVersion);

	/**
	 * Create and fill UInt32Array and optionally Int64Array.
	 * 32-bit version is always generated, 64 only if bShouldCreate64BitVersion = true.
	 * TArray will be moved for performance in Move (thus destroyed).
	 */
	void Copy(const TArray<int64>& InInt64Array);
	void Copy(const TArray<uint32>& InUInt32Array, const bool bShouldCreate64BitVersion);
	void Move(TArray<int64>& InInt64Array);
	void Move(TArray<uint32>& InUInt32Array, const bool bShouldCreate64BitVersion);

	/**
	 *  It sets and fills UInt32ReadBuffer with UInt32Array.
	 */
	void CreateAndLoadSRVBuffer(const TCHAR* InDebugName);

	FORCEINLINE int64 operator[](const int64 InIndex) const;
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE const int64* begin() const;
	FORCEINLINE const int64* end() const;

private:
	TArray<int64> Int64Array;
	TArray<uint32> UInt32Array;
	TSharedPtr<FReadBuffer> UInt32ReadBuffer;
};



/* FNeuralInt64ArrayUInt32Buffer inlined and templated functions
 *****************************************************************************/

int32 FNeuralInt64ArrayUInt32Buffer::Num() const
{
	return UInt32Array.Num();
}

bool FNeuralInt64ArrayUInt32Buffer::IsEmpty() const
{
	return UInt32Array.IsEmpty();
}

const TArray<uint32>& FNeuralInt64ArrayUInt32Buffer::GetUInt32() const
{
	return UInt32Array;
}

const FShaderResourceViewRHIRef& FNeuralInt64ArrayUInt32Buffer::GetUInt32SRV() const
{
	return UInt32ReadBuffer->SRV;
}

bool FNeuralInt64ArrayUInt32Buffer::IsReadBufferValid() const
{
	return UInt32ReadBuffer.IsValid();
}

void FNeuralInt64ArrayUInt32Buffer::ReleaseReadBuffer()
{
	UInt32ReadBuffer->Release();
}

void FNeuralInt64ArrayUInt32Buffer::ResetReadBuffer()
{
	UInt32ReadBuffer.Reset();
}

int64 FNeuralInt64ArrayUInt32Buffer::operator[](const int64 InIndex) const
{
	return Int64Array[InIndex];
}

const int64* FNeuralInt64ArrayUInt32Buffer::begin() const
{
	return Int64Array.GetData();
}

const int64* FNeuralInt64ArrayUInt32Buffer::end() const
{
	return Int64Array.GetData() + Int64Array.Num();
}
