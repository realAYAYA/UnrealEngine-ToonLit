// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/ScriptInterface.h"
#include "AnimNextInterfaceParam.h"

namespace UE::AnimNext::Interface
{

typedef int32 FParamStorageHandle;
constexpr FParamStorageHandle InvalidBlockHandle = -1;

struct ANIMNEXTINTERFACE_API FParamStorage
{
public:
	typedef TPair<FParamStorageHandle, uint8*> TBlockDataPair;

	FParamStorage(int32 InMaxParams, int32 InAllocatedMemorySize, int32 InMaxBlocks, bool InAllowGrowing = false);
	~FParamStorage();


	// *** Interface for param arena ***
	// This prototype allows storing parameters into the storage using pre-allocated blocks of memory
	// The owner is responsible for calculating the size and requesting / releasing the memory blocks
	// Memory blocks are stored in stack LIFO way, last one has to be the first to be released (for simplicity)
	// If a memoy block is released and it is not the last, it will be held until the ones after it are released

	// Requests a block of RequestedBlockSize size
	// Returns a pair with the block handle and the pointer to the memory buffer
	TBlockDataPair RequestBlock(int32 RequestedBlockSize);

	// Releases a block of memory from the arena, using a hanle to identify it
	// If it is not the last of the stack, it will be held until the blocks after this one are released
	void ReleaseBlock(FParamStorageHandle BlockHandle);

	// Obtains the block of memory from a block handle
	uint8* GetBlockMemory(FParamStorageHandle BlockHandle);


	// *** Interface for direct Param storage ***
	// This prototype allows storing parameters directly into the storage
	// Parameters are stored stack like, in a LIFO approach
	// If a parameter is released and it is not the last of the stack, it will be held until the parameters over that one are released
	// This is to have a very simple memory management, might be removed on final implementation, based on requirements

	// Adds a value to the storage (copying the data), returning a HParam
	// If the total allocation size is <= than size of a void *, the value is stored directly in the FParam Data
	// If not, additional memory is requested (TODO : better memory management)
	template<typename ValueType>
	FHParam AddValue(ValueType* Data, int32 NumElem, FParam::EFlags Flags)
	{
		check(ParamFreeIndex < (Parameters.Num())); // TODO : Growing ?

		const int32 ParameterIndex = ParamFreeIndex;
		ParamFreeIndex++;

		const FParamType& ParamType = Private::TParamType<ValueType>::GetType();
		const int32 ParamAllocSize = GetParamAllocSize(ParamType, NumElem);

		FParam& Param = Parameters[ParameterIndex];

		// Prepare the Param for the copy
		Param.TypeId = ParamType.GetTypeId();
		Param.Flags = Flags;
		Param.NumElements = NumElem;
		Param.Data = Data;

		// If size is <= sizeof(void*), put the value directly at the pointer
		if (ParamAllocSize <= sizeof(void*)) // TODO : see if we can get this constexpr
		{
			ParamMemoryHandles[ParameterIndex] = InvalidBlockHandle;

			// Set target memory directly at Param.Data memory
			ParamType.GetParamCopyFunction()(Param, static_cast<ValueType*>((void*)&Param.Data), ParamAllocSize);

			Param.Flags |= FParam::EFlags::Embedded; // Added so it can be checked when retrieving (TODO : see if it can be done constexpr by type)
		}
		else
		{
			TBlockDataPair BlockData = RequestBlock(ParamAllocSize);
			
			ParamMemoryHandles[ParameterIndex] = BlockData.Key;

			// Set target memory directly at BlockData memory
			ParamType.GetParamCopyFunction()(Param, BlockData.Value, ParamAllocSize);
			// Once the copy is done, update the Param
			Param.Data = BlockData.Value;
		}

		RefCounts[ParameterIndex] = 1;

		return FHParam(this, static_cast<FParamHandle>(ParameterIndex));
	}

	// Adds a reference (pointer) to the storage, returning a HParam
	template<typename ValueType>
	FHParam AddReference(ValueType* Data, int32 NumElem, FParam::EFlags Flags)
	{
		check(ParamFreeIndex < (Parameters.Num() - 1)); // TODO : Growing ?

		const int32 ParameterIndex = ParamFreeIndex;
		ParamFreeIndex++;

		const FParamType& ParamType = Private::TParamType<ValueType>::GetType();

		FParam& Param = Parameters[ParameterIndex];

		Param.TypeId = ParamType.GetTypeId();
		Param.Flags = Flags;
		Param.NumElements = NumElem;
		Param.Data = Data;

		ParamMemoryHandles[ParameterIndex] = InvalidBlockHandle;

		RefCounts[ParameterIndex] = 1;

		return FHParam(this, static_cast<FParamHandle>(ParameterIndex));
	}

	// Obtains the Param associated to the HParam
	FParam* GetParam(FParamHandle InParamHandle)
	{
		FParam* RetVal = nullptr;

		const int32 ParameterIndex = InParamHandle;

		if (ParameterIndex < ParamFreeIndex && RefCounts[ParameterIndex] > 0)
		{
			RetVal = &Parameters[ParameterIndex];
		}

		return RetVal;
	}

protected:
	friend struct FHParam;

	// Adds a reference count to a param, using the passed param handle
	void IncRefCount(const FParamHandle InParamHandle)
	{
		const int32 ParameterIndex = InParamHandle;
		check(ParameterIndex < ParamFreeIndex);
		
		check(RefCounts[ParameterIndex] > 0);
		
		RefCounts[ParameterIndex]++;
	}

	// Decrements a reference count to a param, using the passed param handle
	// If the count reaches 0:
	// - If it is the last param of the stack, it releases it and all the previous params with ref count == 0
	// - If it is not the last one, returns without releasing (it will be done later)
	void DecRefCount(const FParamHandle InParamHandle)
	{
		int32 ParameterIndex = InParamHandle;
		check(ParameterIndex < ParamFreeIndex);

		check(RefCounts[ParameterIndex] > 0);
		
		if (RefCounts[ParameterIndex] > 0)
		{
			RefCounts[ParameterIndex]--;

			if (RefCounts[ParameterIndex] == 0)
			{
				// if it is the last parameter, release the memory and deal with the prev params with RefCount == 0, if any
				if (ParameterIndex == ParamFreeIndex - 1)
				{
					// Recover free slots
					while (ParameterIndex >= 0 && RefCounts[ParameterIndex] == 0)
					{
						Parameters[ParameterIndex] = FParam();
						if (ParamMemoryHandles[ParameterIndex] != InvalidBlockHandle)
						{
							ReleaseBlock(ParamMemoryHandles[ParameterIndex]);
							ParamMemoryHandles[ParameterIndex] = InvalidBlockHandle;
						}

						ParameterIndex--;
						ParamFreeIndex--;
					}
				}
			}
		}
	}

	// Calculates the required size for a param, inclusing allignment
	int32 GetParamAllocSize(const FParamType& ParamType, const int32 NumElem) const
	{
		const int32 ParamAlignment = ParamType.GetAlignment();
		const int32 ParamSize = ParamType.GetSize();
		const int32 ParamAllocSize = NumElem * Align(ParamSize, ParamAlignment);

		return ParamAllocSize;
	}

	// --- context arena param storage prototype ---
	TArray<int32> BlockOffets;
	TArray<int32> BlockSizes;
	TBitArray<> BlockFreeFlag;
	uint8* RawMemory = nullptr;
	int32 RawMemorySize = 0;
	int32 CurrentBlocksSize = 0;
	int32 FreeBlockIndex = 0;
	
	// --- shared param storage prototype ---
	TArray<FParam> Parameters;
	TArray<FParamStorageHandle> ParamMemoryHandles;
	TArray<int16> RefCounts;
	int32 ParamFreeIndex = 0;

	bool AllowGrowing = false;

private:
	bool CheckBufferSize(int32 RequestedBlockSize);
	bool CheckNumBlocks(int32 BlockIndex);
};

} // end namespace UE::AnimNext::Interface
