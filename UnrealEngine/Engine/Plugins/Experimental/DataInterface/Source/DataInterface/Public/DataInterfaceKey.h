// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "DataInterfaceParam.h"

class IDataInterface;

namespace UE::DataInterface
{

// Key value for cached data interface state
struct DATAINTERFACE_API FInterfaceKey
{
	FInterfaceKey(const IDataInterface* InDataInterface)
		: DataInterface(InDataInterface)
		, Hash(::GetTypeHash(InDataInterface))
	{}

	FInterfaceKey(const FInterfaceKey& InExistingKey, uint32 InHash)
		: DataInterface(InExistingKey.DataInterface)
		, Hash(HashCombineFast(InExistingKey.Hash, InHash))
	{}

	const IDataInterface* DataInterface = nullptr;
	uint32 Hash = 0;
};

// Key value for cached data interface state
struct DATAINTERFACE_API FInterfaceKeyWithId : FInterfaceKey
{
	FInterfaceKeyWithId(const FInterfaceKey& InInterfaceKey, uint32 InId)
		: FInterfaceKey(InInterfaceKey, ::GetTypeHash(InId))
		, Id(InId)
	{}

	FInterfaceKeyWithId(const FInterfaceKeyWithId& InInterfaceKeyWithId, uint32 InHash)
		: FInterfaceKey(InInterfaceKeyWithId, HashCombineFast(InInterfaceKeyWithId.Hash, InHash))
		, Id(InInterfaceKeyWithId.Id)
	{}
	
	uint32 Id = 0;
};

// Key value for cached data interface state
struct DATAINTERFACE_API FInterfaceKeyWithIdAndStack : FInterfaceKeyWithId
{
	FInterfaceKeyWithIdAndStack(const FInterfaceKeyWithId& InInterfaceKey, uint32 InCallStackHash)
		: FInterfaceKeyWithId(InInterfaceKey, InCallStackHash)
		, CallStackHash(InCallStackHash)
	{}

	FORCEINLINE friend bool operator==(const FInterfaceKeyWithIdAndStack& A, const FInterfaceKeyWithIdAndStack& B)
	{
		return A.DataInterface == B.DataInterface && A.Id == B.Id && A.CallStackHash == B.CallStackHash;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FInterfaceKeyWithIdAndStack& InKey)
	{
		return InKey.Hash;
	}

	uint32 CallStackHash = 0;
};

}