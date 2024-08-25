// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamId.h"
#include "Param/ParamCompatibility.h"
#include "Containers/HashTable.h"

struct FInstancedPropertyBag;

namespace UE::AnimNext
{
	struct FParamResult;
	struct FParamTypeHandle;
	struct FParamStack;
}

namespace UE::AnimNext::Private
{
	struct FParamEntry;
}

namespace UE::AnimNext
{

// Stack layers are what actually get pushed/popped onto the layer stack.
// They are designed to be held on an instance, their data updated in place.
// Memory ownership for params is variable depending on the layer's subclass
struct FParamStackLayer
{
	FParamStackLayer() = delete;

	virtual ~FParamStackLayer();

	// Constructor that reserves space for the specified parameters
	explicit FParamStackLayer(uint32 InParamCount);

	// Constructor for set of user parameters
	explicit FParamStackLayer(TConstArrayView<Private::FParamEntry> InParams);

	ANIMNEXT_API FParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData) const;
	ANIMNEXT_API FParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal()) const;

	ANIMNEXT_API FParamResult GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData);
	ANIMNEXT_API FParamResult GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal());

	ANIMNEXT_API virtual UObject* AsUObject() { return nullptr; }
	ANIMNEXT_API virtual FInstancedPropertyBag* AsInstancedPropertyBag() { return nullptr; }

	const Private::FParamEntry* FindEntry(FParamId InId) const;
	Private::FParamEntry* FindMutableEntry(FParamId InId) const;
	
	// Params that this layer supplies
	TArray<Private::FParamEntry> Params;

	// Hash table for param IDs
	FHashTable HashTable;

	// Owning stack for this layer if it is owned internally, otherwise nullptr
	FParamStack* OwningStack = nullptr;

	// Storage offset for this layer if it is owned internally, otherwise MAX_uint32
	uint32 OwnedStorageOffset = MAX_uint32;
};

}