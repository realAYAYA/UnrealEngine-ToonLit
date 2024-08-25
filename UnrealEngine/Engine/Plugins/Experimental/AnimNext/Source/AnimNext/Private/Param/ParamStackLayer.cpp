// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamStackLayer.h"
#include "Param/ParamEntry.h"
#include "Param/ParamResult.h"
#include "Param/ParamUtils.h"

namespace UE::AnimNext
{

FParamStackLayer::FParamStackLayer(uint32 InParamCount)
	: HashTable(FMath::Max<uint32>(32u, FMath::RoundUpToPowerOfTwo(InParamCount + 1)), InParamCount)
{
}
	
FParamStackLayer::~FParamStackLayer()
{
}

FParamStackLayer::FParamStackLayer(TConstArrayView<Private::FParamEntry> InParams)
	: FParamStackLayer(InParams.Num())
{
	Params.Reserve(InParams.Num());
	for (uint32 ParamIndex = 0; ParamIndex < static_cast<uint32>(InParams.Num()); ++ParamIndex)
	{
		const Private::FParamEntry& Param = InParams[ParamIndex];
		Params.Add(Param);
		HashTable.Add(Param.GetHash(), ParamIndex);
	}
}
	
FParamResult FParamStackLayer::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData) const
{
	const Private::FParamEntry* ParamPtr = FindEntry(InId);
	if (ParamPtr == nullptr)
	{
		return EParamResult::NotInScope;
	}

	return ParamPtr->GetParamData(InTypeHandle, OutParamData);
}

FParamResult FParamStackLayer::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	const Private::FParamEntry* ParamPtr = FindEntry(InId);
	if (ParamPtr == nullptr)
	{
		return EParamResult::NotInScope;
	}

	return ParamPtr->GetParamData(InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
}

FParamResult FParamStackLayer::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData)
{
	Private::FParamEntry* ParamPtr = FindMutableEntry(InId);
	if (ParamPtr == nullptr)
	{
		return EParamResult::NotInScope;
	}

	return ParamPtr->GetMutableParamData(InTypeHandle, OutParamData);
}

FParamResult FParamStackLayer::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility)
{
	Private::FParamEntry* ParamPtr = FindMutableEntry(InId);
	if (ParamPtr == nullptr)
	{
		return EParamResult::NotInScope;
	}

	return ParamPtr->GetMutableParamData(InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
}

const Private::FParamEntry* FParamStackLayer::FindEntry(FParamId InId) const
{
	for(uint32 Index = HashTable.First(InId.GetHash()); HashTable.IsValid(Index); Index = HashTable.Next(Index))
	{
		if(Params[Index].GetName() == InId.GetName())
		{
			return &Params[Index];
		}
	}
	return nullptr;
}

Private::FParamEntry* FParamStackLayer::FindMutableEntry(FParamId InId) const
{
	return const_cast<Private::FParamEntry*>(FindEntry(InId));
}
	
}