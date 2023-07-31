// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterfaceParam.h"
#include "DataInterfaceKey.h"

namespace UE::DataInterface
{

struct FContext;
struct FParamType;

enum class EStatePersistence : uint8
{
	None,
	Relevancy,
	Permanent,
};

// Container for data interface state
struct DATAINTERFACE_API FState
{
	FState(uint32 InNumElements = 1, uint32 InChunkSize = 1)
		: NumElements(InNumElements)
		, ChunkSize(InChunkSize)
	{
		check(InNumElements > 0);
		check(InChunkSize > 0);
	}
	
private:
	friend struct FContext;

	// Gets (and allocates, if necessary) state for the specified node given this calling context
	template<typename ValueType, EStatePersistence Persistence = EStatePersistence::Relevancy>
	TParam<ValueType> GetState(const FInterfaceKeyWithId& InKey, const FContext& InContext, uint32 InHash)
	{
		const FInterfaceKeyWithIdAndStack CombinedKey(InKey, InHash);

		if(FParam* ExistingState = FindStateRaw(CombinedKey, InContext, Persistence))
		{
			checkSlow(Private::CheckParam<ValueType>(*ExistingState));
			return TParam<ValueType>(*ExistingState);
		}
		else
		{
			// Allocate new state
			FParam* NewState = AllocateState(CombinedKey, InContext, Private::TParamType<ValueType>::GetType(), Persistence);

			// Construct
			new (NewState->Data) ValueType();

			// Return as param
			return TParam<ValueType>(*NewState);
		}
	}

	// Gets (and allocates, if necessary) state for the specified node given this calling context
	template<typename ValueType, EStatePersistence Persistence = EStatePersistence::Relevancy>
	TParam<ValueType> GetState(const IDataInterface* InDataInterface, uint32 InId, const FContext& InContext, uint32 InContextHash)
	{
		const FInterfaceKeyWithId Key(InDataInterface, InId);
		return GetState<ValueType>(Key, InContext, InContextHash);
	}
	
private:
	FParam* FindStateRaw(const FInterfaceKeyWithIdAndStack& InKey, const FContext& InContext, EStatePersistence InPersistence);

	FParam* AllocateState(const FInterfaceKeyWithIdAndStack& InKey, const FContext& InContext, const FParamType& InType, EStatePersistence InPersistence);

private:
	struct FRelevancyParam : FParam
	{
		FRelevancyParam(const FParamType& InType, void* InData, EFlags InFlags, uint32 InUpdateCounter)
			: FParam(InType, InData, InFlags)
			, UpdateCounter(InUpdateCounter)
		{}
	
		uint32 UpdateCounter = 0;
	};
	
	TMap<FInterfaceKeyWithIdAndStack, FRelevancyParam> RelevancyValueMap;
	TMap<FInterfaceKeyWithIdAndStack, FParam> PermanentValueMap;
	uint32 NumElements = 0;
	uint32 ChunkSize = 0;
};

}