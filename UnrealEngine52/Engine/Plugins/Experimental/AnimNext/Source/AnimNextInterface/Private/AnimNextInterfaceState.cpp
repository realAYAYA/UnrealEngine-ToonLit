// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceState.h"
#include "AnimNextInterfaceContext.h"

namespace UE::AnimNext::Interface
{

FParam* FState::FindStateRaw(const FInterfaceKeyWithIdAndStack& InKey, const FContext& InContext, EStatePersistence InPersistence)
{
	check(NumElements == InContext.GetNum())

	FParam* ExistingValue = nullptr;
	TMap<FInterfaceKeyWithIdAndStack, FParam>* ValueMap = nullptr; 

	switch(InPersistence)
	{
	case EStatePersistence::Relevancy:
		ExistingValue = RelevancyValueMap.Find(InKey);
		if(ExistingValue)
		{
			// An access to a state param counts as a 'relevant use' for this update
			static_cast<FRelevancyParam*>(ExistingValue)->UpdateCounter = InContext.UpdateCounter;
		}
		break;
	case EStatePersistence::Permanent:
		ExistingValue = PermanentValueMap.Find(InKey);
		break;
	default:
		check(false);
		break;
	}
	
	return ExistingValue;
}

FParam* FState::AllocateState(const FInterfaceKeyWithIdAndStack& InKey, const FContext& InContext, const FParamType& InType, EStatePersistence InPersistence)
{
	void* Data = FMemory::Malloc(InType.GetSize() * NumElements, InType.GetAlignment());
	
	switch(InPersistence)
	{
	case EStatePersistence::Relevancy:
		// TODO: chunked allocator for relevancy-based stuff?
		return &RelevancyValueMap.Add(InKey, FRelevancyParam(InType, Data, InContext.GetBatchedFlags(), InContext.UpdateCounter));
		break;
	case EStatePersistence::Permanent:
		return &PermanentValueMap.Add(InKey, FParam(InType, Data, InContext.GetBatchedFlags()));
		break;
	default:
		check(false);
		break;
	}
	
	return nullptr;
}

}
