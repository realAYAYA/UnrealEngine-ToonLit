// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"

namespace UE::Net::Private
{
	typedef uint32 FInternalNetHandle;
	typedef int8 FLifeTimeConditionStorage;
}

namespace UE::Net::Private
{

struct FChildSubObjectsInfo
{
	const FInternalNetHandle* ChildSubObjects = nullptr;
	const FLifeTimeConditionStorage* SubObjectLifeTimeConditions = nullptr;
	uint32 NumSubObjects = 0U;
};

class FNetDependencyData
{
public:
	FNetDependencyData();

	typedef TArray<FInternalNetHandle, TInlineAllocator<8>> FInternalNetHandleArray;

	typedef TArray<FLifeTimeConditionStorage, TInlineAllocator<8>> FSubObjectConditionalsArray;

	enum EArrayType
	{
		SubObjects = 0U,
		ChildSubObjects,
		DependentObjects,
		ParentObjects,
		Count
	};

	template<EArrayType TypeIndex>
	FInternalNetHandleArray& GetOrCreateInternalHandleArray(FInternalNetHandle InternalHandle)
	{
		static_assert(TypeIndex != EArrayType::Count, "Invalid array type index");
		return GetOrCreateInternalHandleArray(InternalHandle, TypeIndex);
	};

	FSubObjectConditionalsArray& GetOrCreateSubObjectConditionalsArray(FInternalNetHandle InternalHandle);

	FInternalNetHandleArray& GetOrCreateInternalChildSubObjectsArray(FInternalNetHandle InternalHandle, FSubObjectConditionalsArray*& OutSubObjectConditionals);

	bool GetInternalChildSubObjectAndConditionalArrays(FInternalNetHandle InternalHandle, FInternalNetHandleArray*& OutChildSubObjectsArray, FSubObjectConditionalsArray*& OutSubObjectConditionals)
	{
		const FDependencyInfo* Entry = DependencyInfos.Find(InternalHandle);
		const uint32 ArrayIndex = Entry ? Entry->ArrayIndices[ChildSubObjects] : FDependencyInfo::InvalidCacheIndex;

		if (ArrayIndex == FDependencyInfo::InvalidCacheIndex)
		{
			return false;
		}

		OutChildSubObjectsArray = &DependentObjectsStorage[ArrayIndex];
		OutSubObjectConditionals = Entry->SubObjectConditionalArrayIndex != FDependencyInfo::InvalidCacheIndex ? &SubObjectConditionalsStorage[Entry->SubObjectConditionalArrayIndex] : nullptr;
		return true;
	}

	bool GetChildSubObjects(FInternalNetHandle InternalHandle, FChildSubObjectsInfo& OutInfo) const
	{
		const FDependencyInfo* Entry = DependencyInfos.Find(InternalHandle);
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
	FInternalNetHandleArray* GetInternalHandleArray(FInternalNetHandle InternalHandle)
	{
		static_assert(TypeIndex != EArrayType::Count, "Invalid array type index");
		const FDependencyInfo* Entry = DependencyInfos.Find(InternalHandle);
		const uint32 ArrayIndex = Entry ? Entry->ArrayIndices[TypeIndex] : FDependencyInfo::InvalidCacheIndex;
		if (ArrayIndex != FDependencyInfo::InvalidCacheIndex)
		{
			return &DependentObjectsStorage[ArrayIndex];
		}
		return nullptr;
	}

	template<EArrayType TypeIndex>
	TArrayView<const FInternalNetHandle> GetInternalHandleArray(FInternalNetHandle InternalHandle) const
	{
		static_assert(TypeIndex != EArrayType::Count, "Invalid array type index");

		const FDependencyInfo* Entry = DependencyInfos.Find(InternalHandle);
		const uint32 ArrayIndex = Entry ? Entry->ArrayIndices[TypeIndex] : FDependencyInfo::InvalidCacheIndex;
		if (ArrayIndex != FDependencyInfo::InvalidCacheIndex)
		{
			return MakeArrayView(DependentObjectsStorage[ArrayIndex]);
		}
		return MakeArrayView<const FInternalNetHandle>(nullptr, 0);
	}

	void FreeStoredDependencyDataForObject(FInternalNetHandle InternalHandle);
	
private:
	
	struct FDependencyInfo
	{
		constexpr static uint32 InvalidCacheIndex = ~(0U);
		uint32 ArrayIndices[EArrayType::Count] = { InvalidCacheIndex, InvalidCacheIndex, InvalidCacheIndex, InvalidCacheIndex };
		uint32 SubObjectConditionalArrayIndex = InvalidCacheIndex;
	};

private:
	FInternalNetHandleArray& GetOrCreateInternalHandleArray(FInternalNetHandle InternalHandle, EArrayType Type);
	FDependencyInfo& GetOrCreateCacheEntry(FInternalNetHandle InternalHandle);
	
	// Map to track the replicated objects with subObjects or dependencies
	TMap<FInternalNetHandle, FDependencyInfo> DependencyInfos;
	// Storage for DependentObjects and SubObjects
	TSparseArray<FInternalNetHandleArray> DependentObjectsStorage;
	// Storage for SubObject conditionals
	TSparseArray<FSubObjectConditionalsArray> SubObjectConditionalsStorage;
};

}
