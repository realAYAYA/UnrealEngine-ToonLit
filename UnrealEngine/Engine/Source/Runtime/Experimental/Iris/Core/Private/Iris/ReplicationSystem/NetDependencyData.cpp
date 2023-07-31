// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetDependencyData.h"
#include "Iris/Core/IrisLog.h"
#include "ReplicationOperationsInternal.h"

namespace UE::Net::Private
{

FNetDependencyData::FNetDependencyData()
{
}

void FNetDependencyData::FreeStoredDependencyDataForObject(FInternalNetHandle InternalHandle)
{
	if (FDependencyInfo* Entry = DependencyInfos.Find(InternalHandle))
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

		DependencyInfos.Remove(InternalHandle);
	}
}

FNetDependencyData::FDependencyInfo& FNetDependencyData::GetOrCreateCacheEntry(FInternalNetHandle InternalHandle)
{
	FDependencyInfo* Entry = DependencyInfos.Find(InternalHandle);

	if (!Entry)
	{
		Entry = &DependencyInfos.Add(InternalHandle);
		*Entry = FDependencyInfo();
	}

	return *Entry;
}

FNetDependencyData::FSubObjectConditionalsArray& FNetDependencyData::GetOrCreateSubObjectConditionalsArray(FInternalNetHandle OwnerHandle)
{
	FDependencyInfo& Entry = GetOrCreateCacheEntry(OwnerHandle);
	check(Entry.ArrayIndices[EArrayType::ChildSubObjects] != FDependencyInfo::InvalidCacheIndex);
	
	if (Entry.SubObjectConditionalArrayIndex == FDependencyInfo::InvalidCacheIndex)
	{
		FSparseArrayAllocationInfo AllocInfo = SubObjectConditionalsStorage.AddUninitialized();
		Entry.SubObjectConditionalArrayIndex = AllocInfo.Index;

		FSubObjectConditionalsArray* SubObjectConditionalsArray = new (AllocInfo.Pointer) FSubObjectConditionalsArray();
		
		// Make sure that we initialize the conditionals to match the number of SubObjects
		const int32 NumChildSubObjects = DependentObjectsStorage[Entry.ArrayIndices[EArrayType::ChildSubObjects]].Num();
		SubObjectConditionalsArray->SetNumZeroed(NumChildSubObjects);

		return *SubObjectConditionalsArray;
	}
	else
	{
		return SubObjectConditionalsStorage[Entry.SubObjectConditionalArrayIndex];
	}
}

FNetDependencyData::FInternalNetHandleArray& FNetDependencyData::GetOrCreateInternalChildSubObjectsArray(FInternalNetHandle OwnerHandle, FSubObjectConditionalsArray*& OutSubObjectConditionals)
{
	FDependencyInfo& Entry = GetOrCreateCacheEntry(OwnerHandle);
	if (Entry.ArrayIndices[ChildSubObjects] == FDependencyInfo::InvalidCacheIndex)
	{
		// Allocate storage for subobjects / atomic dependencies
		FSparseArrayAllocationInfo AllocInfo = DependentObjectsStorage.AddUninitialized();
		Entry.ArrayIndices[ChildSubObjects] = AllocInfo.Index;

		FInternalNetHandleArray* InternalHandleArray = new (AllocInfo.Pointer) FInternalNetHandleArray();
		OutSubObjectConditionals = nullptr;
		return *InternalHandleArray;
	}
	else
	{
		OutSubObjectConditionals = Entry.SubObjectConditionalArrayIndex != FDependencyInfo::InvalidCacheIndex ? &SubObjectConditionalsStorage[Entry.SubObjectConditionalArrayIndex] : nullptr;
		return DependentObjectsStorage[Entry.ArrayIndices[ChildSubObjects]];
	}
}

FNetDependencyData::FInternalNetHandleArray& FNetDependencyData::GetOrCreateInternalHandleArray(FInternalNetHandle OwnerHandle, EArrayType ArrayType)
{
	FDependencyInfo& Entry = GetOrCreateCacheEntry(OwnerHandle);
	if (Entry.ArrayIndices[ArrayType] == FDependencyInfo::InvalidCacheIndex)
	{
		// Allocate storage for subobjects / atomic dependencies
		FSparseArrayAllocationInfo AllocInfo = DependentObjectsStorage.AddUninitialized();
		Entry.ArrayIndices[ArrayType] = AllocInfo.Index;

		FInternalNetHandleArray* InternalHandleArray = new (AllocInfo.Pointer) FInternalNetHandleArray();

		return *InternalHandleArray;
	}
	else
	{
		return DependentObjectsStorage[Entry.ArrayIndices[ArrayType]];
	}
}

}