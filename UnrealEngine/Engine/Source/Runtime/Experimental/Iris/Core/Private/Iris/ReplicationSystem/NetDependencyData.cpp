// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetDependencyData.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "UObject/CoreNetTypes.h"

namespace UE::Net::Private
{

FNetDependencyData::FNetDependencyData()
{
}

void FNetDependencyData::FreeStoredDependencyDataForObject(FInternalNetRefIndex InternalIndex)
{
	if (FDependencyInfo* Entry = DependencyInfos.Find(InternalIndex))
	{
		for (const uint32 ArrayIndex : Entry->ArrayIndices)
		{
			if (ArrayIndex != FDependencyInfo::InvalidCacheIndex)
			{
				checkSlow(DependentObjectsStorage[ArrayIndex].Num() == 0);
				DependentObjectsStorage.RemoveAt(ArrayIndex);
			}
		}

		if (Entry->SubObjectConditionalArrayIndex != FDependencyInfo::InvalidCacheIndex)
		{
			SubObjectConditionalsStorage.RemoveAt(Entry->SubObjectConditionalArrayIndex);
		}

		if (Entry->DependentObjectsInfoArrayIndex != FDependencyInfo::InvalidCacheIndex)
		{
			DependentObjectInfosStorage.RemoveAt(Entry->DependentObjectsInfoArrayIndex);
		}

		DependencyInfos.Remove(InternalIndex);
	}
}

FNetDependencyData::FDependencyInfo& FNetDependencyData::GetOrCreateCacheEntry(FInternalNetRefIndex InternalIndex)
{
	FDependencyInfo* Entry = DependencyInfos.Find(InternalIndex);

	if (!Entry)
	{
		Entry = &DependencyInfos.Add(InternalIndex);
		*Entry = FDependencyInfo();
	}

	return *Entry;
}

FNetDependencyData::FDependentObjectInfoArray& FNetDependencyData::GetOrCreateDependentObjectInfoArray(FInternalNetRefIndex OwnerIndex)
{
	FDependencyInfo& Entry = GetOrCreateCacheEntry(OwnerIndex);
	
	if (Entry.DependentObjectsInfoArrayIndex == FDependencyInfo::InvalidCacheIndex)
	{
		FSparseArrayAllocationInfo AllocInfo = DependentObjectInfosStorage.AddUninitialized();
		Entry.DependentObjectsInfoArrayIndex = AllocInfo.Index;

		FDependentObjectInfoArray* DependentObjectsInfoArrayIndex = new (AllocInfo.Pointer) FDependentObjectInfoArray();
		
		return *DependentObjectsInfoArrayIndex;
	}
	else
	{
		return DependentObjectInfosStorage[Entry.DependentObjectsInfoArrayIndex];
	}
}

FNetDependencyData::FSubObjectConditionalsArray& FNetDependencyData::GetOrCreateSubObjectConditionalsArray(FInternalNetRefIndex OwnerIndex)
{
	FDependencyInfo& Entry = GetOrCreateCacheEntry(OwnerIndex);
	check(Entry.ArrayIndices[EArrayType::ChildSubObjects] != FDependencyInfo::InvalidCacheIndex);
	
	if (Entry.SubObjectConditionalArrayIndex == FDependencyInfo::InvalidCacheIndex)
	{
		FSparseArrayAllocationInfo AllocInfo = SubObjectConditionalsStorage.AddUninitialized();
		Entry.SubObjectConditionalArrayIndex = AllocInfo.Index;

		FSubObjectConditionalsArray* SubObjectConditionalsArray = new (AllocInfo.Pointer) FSubObjectConditionalsArray();
		
		// Make sure that we initialize the conditionals to match the number of SubObjects
		const int32 NumChildSubObjects = DependentObjectsStorage[Entry.ArrayIndices[EArrayType::ChildSubObjects]].Num();
		static_assert(COND_None == 0, "Can't use SetNumZeroed() to initialize COND_None");
		SubObjectConditionalsArray->SetNumZeroed(NumChildSubObjects);

		return *SubObjectConditionalsArray;
	}
	else
	{
		return SubObjectConditionalsStorage[Entry.SubObjectConditionalArrayIndex];
	}
}

FNetDependencyData::FInternalNetRefIndexArray& FNetDependencyData::GetOrCreateInternalChildSubObjectsArray(FInternalNetRefIndex OwnerIndex, FSubObjectConditionalsArray*& OutSubObjectConditionals)
{
	FDependencyInfo& Entry = GetOrCreateCacheEntry(OwnerIndex);
	if (Entry.ArrayIndices[ChildSubObjects] == FDependencyInfo::InvalidCacheIndex)
	{
		// Allocate storage for subobjects / atomic dependencies
		FSparseArrayAllocationInfo AllocInfo = DependentObjectsStorage.AddUninitialized();
		Entry.ArrayIndices[ChildSubObjects] = AllocInfo.Index;

		FInternalNetRefIndexArray* InternalIndexArray = new (AllocInfo.Pointer) FInternalNetRefIndexArray();
		OutSubObjectConditionals = nullptr;
		return *InternalIndexArray;
	}
	else
	{
		OutSubObjectConditionals = Entry.SubObjectConditionalArrayIndex != FDependencyInfo::InvalidCacheIndex ? &SubObjectConditionalsStorage[Entry.SubObjectConditionalArrayIndex] : nullptr;
		return DependentObjectsStorage[Entry.ArrayIndices[ChildSubObjects]];
	}
}

FNetDependencyData::FInternalNetRefIndexArray& FNetDependencyData::GetOrCreateInternalIndexArray(FInternalNetRefIndex OwnerIndex, EArrayType ArrayType)
{
	FDependencyInfo& Entry = GetOrCreateCacheEntry(OwnerIndex);
	if (Entry.ArrayIndices[ArrayType] == FDependencyInfo::InvalidCacheIndex)
	{
		// Allocate storage for subobjects / atomic dependencies
		FSparseArrayAllocationInfo AllocInfo = DependentObjectsStorage.AddUninitialized();
		Entry.ArrayIndices[ArrayType] = AllocInfo.Index;

		FInternalNetRefIndexArray* InternalIndexArray = new (AllocInfo.Pointer) FInternalNetRefIndexArray();

		return *InternalIndexArray;
	}
	else
	{
		return DependentObjectsStorage[Entry.ArrayIndices[ArrayType]];
	}
}

}