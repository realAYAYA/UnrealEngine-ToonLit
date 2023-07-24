// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"

namespace UE::Net::Private
{
	typedef uint32 FInternalNetRefIndex;
	typedef int8 FLifeTimeConditionStorage;
}

namespace UE::Net::Private
{

struct FChildSubObjectsInfo
{
	const FInternalNetRefIndex* ChildSubObjects = nullptr;
	const FLifeTimeConditionStorage* SubObjectLifeTimeConditions = nullptr;
	uint32 NumSubObjects = 0U;
};

struct FDependentObjectInfo
{
	FInternalNetRefIndex NetRefIndex = 0U;
	EDependentObjectSchedulingHint SchedulingHint = EDependentObjectSchedulingHint::Default;
};

class FNetDependencyData
{
public:
	FNetDependencyData();

	typedef TArray<FInternalNetRefIndex, TInlineAllocator<8>> FInternalNetRefIndexArray;
	typedef TArray<FLifeTimeConditionStorage, TInlineAllocator<8>> FSubObjectConditionalsArray;
	typedef TArray<FDependentObjectInfo, TInlineAllocator<8>> FDependentObjectInfoArray;

	enum EArrayType
	{
		SubObjects = 0U,
		ChildSubObjects,
		DependentParentObjects,
		Count
	};

	template<EArrayType TypeIndex>
	FInternalNetRefIndexArray& GetOrCreateInternalIndexArray(FInternalNetRefIndex InternalIndex)
	{
		static_assert(TypeIndex != EArrayType::Count, "Invalid array type index");
		return GetOrCreateInternalIndexArray(InternalIndex, TypeIndex);
	};

	FDependentObjectInfoArray& GetOrCreateDependentObjectInfoArray(FInternalNetRefIndex InternalIndex);
	FDependentObjectInfoArray* GetDependentObjectInfoArray(FInternalNetRefIndex InternalIndex)
	{
		const FDependencyInfo* Entry = DependencyInfos.Find(InternalIndex);
		const uint32 ArrayIndex = Entry ? Entry->DependentObjectsInfoArrayIndex : FDependencyInfo::InvalidCacheIndex;
		if (ArrayIndex != FDependencyInfo::InvalidCacheIndex)
		{
			return &DependentObjectInfosStorage[ArrayIndex];
		}
		return nullptr;
	}

	TArrayView<const FDependentObjectInfo> GetDependentObjectInfoArray(FInternalNetRefIndex InternalIndex) const
	{
		const FDependencyInfo* Entry = DependencyInfos.Find(InternalIndex);
		const uint32 ArrayIndex = Entry ? Entry->DependentObjectsInfoArrayIndex : FDependencyInfo::InvalidCacheIndex;
		if (ArrayIndex != FDependencyInfo::InvalidCacheIndex)
		{
			return MakeArrayView(DependentObjectInfosStorage[ArrayIndex]);
		}
		return MakeArrayView<const FDependentObjectInfo>(nullptr, 0);
	}

	FSubObjectConditionalsArray& GetOrCreateSubObjectConditionalsArray(FInternalNetRefIndex InternalIndex);

	FInternalNetRefIndexArray& GetOrCreateInternalChildSubObjectsArray(FInternalNetRefIndex InternalIndex, FSubObjectConditionalsArray*& OutSubObjectConditionals);

	bool GetInternalChildSubObjectAndConditionalArrays(FInternalNetRefIndex InternalIndex, FInternalNetRefIndexArray*& OutChildSubObjectsArray, FSubObjectConditionalsArray*& OutSubObjectConditionals)
	{
		const FDependencyInfo* Entry = DependencyInfos.Find(InternalIndex);
		const uint32 ArrayIndex = Entry ? Entry->ArrayIndices[ChildSubObjects] : FDependencyInfo::InvalidCacheIndex;

		if (ArrayIndex == FDependencyInfo::InvalidCacheIndex)
		{
			return false;
		}

		OutChildSubObjectsArray = &DependentObjectsStorage[ArrayIndex];
		OutSubObjectConditionals = Entry->SubObjectConditionalArrayIndex != FDependencyInfo::InvalidCacheIndex ? &SubObjectConditionalsStorage[Entry->SubObjectConditionalArrayIndex] : nullptr;
		return true;
	}

	bool GetChildSubObjects(FInternalNetRefIndex InternalIndex, FChildSubObjectsInfo& OutInfo) const
	{
		const FDependencyInfo* Entry = DependencyInfos.Find(InternalIndex);
		const uint32 ArrayIndex = Entry ? Entry->ArrayIndices[ChildSubObjects] : FDependencyInfo::InvalidCacheIndex;
		if (ArrayIndex == FDependencyInfo::InvalidCacheIndex)
		{
			return false;
		}

		OutInfo.ChildSubObjects = DependentObjectsStorage[ArrayIndex].GetData();			
		OutInfo.SubObjectLifeTimeConditions = Entry->SubObjectConditionalArrayIndex != FDependencyInfo::InvalidCacheIndex ? SubObjectConditionalsStorage[Entry->SubObjectConditionalArrayIndex].GetData() : nullptr;
		OutInfo.NumSubObjects = DependentObjectsStorage[ArrayIndex].Num();
		return true;
	}

	template<EArrayType TypeIndex>
	FInternalNetRefIndexArray* GetInternalIndexArray(FInternalNetRefIndex InternalIndex)
	{
		static_assert(TypeIndex != EArrayType::Count, "Invalid array type index");
		const FDependencyInfo* Entry = DependencyInfos.Find(InternalIndex);
		const uint32 ArrayIndex = Entry ? Entry->ArrayIndices[TypeIndex] : FDependencyInfo::InvalidCacheIndex;
		if (ArrayIndex != FDependencyInfo::InvalidCacheIndex)
		{
			return &DependentObjectsStorage[ArrayIndex];
		}
		return nullptr;
	}

	template<EArrayType TypeIndex>
	TArrayView<const FInternalNetRefIndex> GetInternalIndexArray(FInternalNetRefIndex InternalIndex) const
	{
		static_assert(TypeIndex != EArrayType::Count, "Invalid array type index");

		const FDependencyInfo* Entry = DependencyInfos.Find(InternalIndex);
		const uint32 ArrayIndex = Entry ? Entry->ArrayIndices[TypeIndex] : FDependencyInfo::InvalidCacheIndex;
		if (ArrayIndex != FDependencyInfo::InvalidCacheIndex)
		{
			return MakeArrayView(DependentObjectsStorage[ArrayIndex]);
		}
		return MakeArrayView<const FInternalNetRefIndex>(nullptr, 0);
	}

	void FreeStoredDependencyDataForObject(FInternalNetRefIndex InternalIndex);
	
private:
	
	struct FDependencyInfo
	{
		constexpr static uint32 InvalidCacheIndex = ~(0U);
		uint32 ArrayIndices[EArrayType::Count] = { InvalidCacheIndex, InvalidCacheIndex, InvalidCacheIndex };
		uint32 SubObjectConditionalArrayIndex = InvalidCacheIndex;
		uint32 DependentObjectsInfoArrayIndex = InvalidCacheIndex;
	};

private:
	FInternalNetRefIndexArray& GetOrCreateInternalIndexArray(FInternalNetRefIndex InternalIndex, EArrayType Type);
	FDependencyInfo& GetOrCreateCacheEntry(FInternalNetRefIndex InternalIndex);
	
	// Map to track the replicated objects with subObjects or dependencies
	TMap<FInternalNetRefIndex, FDependencyInfo> DependencyInfos;
	// Storage for DependentObjects and SubObjects
	TSparseArray<FInternalNetRefIndexArray> DependentObjectsStorage;
	// Storage for SubObject conditionals
	TSparseArray<FSubObjectConditionalsArray> SubObjectConditionalsStorage;
	// Storage for DependentObjects
	TSparseArray<FDependentObjectInfoArray> DependentObjectInfosStorage;
};

}
