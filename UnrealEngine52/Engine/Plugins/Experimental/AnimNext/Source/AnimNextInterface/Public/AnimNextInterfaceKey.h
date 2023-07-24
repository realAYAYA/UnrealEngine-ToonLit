// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "AnimNextInterfaceParam.h"

class IAnimNextInterface;

namespace UE::AnimNext::Interface
{

// Key value for cached anim interface state
struct ANIMNEXTINTERFACE_API FInterfaceKey
{
	FInterfaceKey(const IAnimNextInterface* InAnimNextInterface)
		: AnimNextInterface(InAnimNextInterface)
		, Hash(::GetTypeHash(InAnimNextInterface))
	{}

	FInterfaceKey(const FInterfaceKey& InExistingKey, uint32 InHash)
		: AnimNextInterface(InExistingKey.AnimNextInterface)
		, Hash(HashCombineFast(InExistingKey.Hash, InHash))
	{}

	const IAnimNextInterface* AnimNextInterface = nullptr;
	uint32 Hash = 0;
};

// Key value for cached anim interface state
struct ANIMNEXTINTERFACE_API FInterfaceKeyWithId : FInterfaceKey
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

// Key value for cached anim interface state
struct ANIMNEXTINTERFACE_API FInterfaceKeyWithIdAndStack : FInterfaceKeyWithId
{
	FInterfaceKeyWithIdAndStack(const FInterfaceKeyWithId& InInterfaceKey, uint32 InCallStackHash)
		: FInterfaceKeyWithId(InInterfaceKey, InCallStackHash)
		, CallStackHash(InCallStackHash)
	{}

	FORCEINLINE friend bool operator==(const FInterfaceKeyWithIdAndStack& A, const FInterfaceKeyWithIdAndStack& B)
	{
		return A.AnimNextInterface == B.AnimNextInterface && A.Id == B.Id && A.CallStackHash == B.CallStackHash;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FInterfaceKeyWithIdAndStack& InKey)
	{
		return InKey.Hash;
	}

	uint32 CallStackHash = 0;
};

}