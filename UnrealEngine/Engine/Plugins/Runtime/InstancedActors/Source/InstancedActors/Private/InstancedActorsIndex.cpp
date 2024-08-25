// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsIndex.h"
#include "InstancedActorsManager.h"
#include "InstancedActorsData.h"


FInstancedActorsInstanceHandle::FInstancedActorsInstanceHandle(UInstancedActorsData& InInstancedActorData, FInstancedActorsInstanceIndex InIndex)
	: InstancedActorData(&InInstancedActorData)
	, Index(InIndex)
{
}

FString FInstancedActorsInstanceIndex::GetDebugName() const
{
	return FString::Printf(TEXT("%d"), Index);
}

bool FInstancedActorsInstanceHandle::IsValid() const 
{
	return ::IsValid(InstancedActorData) && Index.IsValid();
}

UInstancedActorsData& FInstancedActorsInstanceHandle::GetInstanceActorDataChecked() const 
{ 
	check(::IsValid(InstancedActorData));
	return *InstancedActorData; 
}

AInstancedActorsManager* FInstancedActorsInstanceHandle::GetManager() const
{
	if (::IsValid(InstancedActorData))
	{
		return InstancedActorData->GetManager();
	}

	return nullptr;
}

AInstancedActorsManager& FInstancedActorsInstanceHandle::GetManagerChecked() const
{
	return GetInstanceActorDataChecked().GetManagerChecked();
}

FString FInstancedActorsInstanceHandle::GetDebugName() const
{
	return FString::Printf(TEXT("%s : %s"), InstancedActorData ? *InstancedActorData->GetDebugName() : TEXT("null"), *Index.GetDebugName());
}

uint32 GetTypeHash(const FInstancedActorsInstanceHandle& Handle)
{
	uint32 Hash = 0;
	if (::IsValid(Handle.InstancedActorData))
	{
		Hash = GetTypeHash(Handle.InstancedActorData);
	}
	Hash = HashCombine(Hash, Handle.GetIndex());

	return Hash;
}
