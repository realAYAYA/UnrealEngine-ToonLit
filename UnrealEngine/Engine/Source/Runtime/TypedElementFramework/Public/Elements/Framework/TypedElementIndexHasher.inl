// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Hash/CityHash.h"

namespace TypedElementDataStorage
{
	template<typename T>
	IndexHash GenerateIndexHash(const T* Object)
	{
		return reinterpret_cast<IndexHash>(Object);
	}

	template<typename T>
	IndexHash GenerateIndexHash(const TWeakObjectPtr<T>& Object)
	{
		return GenerateIndexHash(Object.Get());
	}

	template<typename T>
	IndexHash GenerateIndexHash(const TObjectPtr<T>& Object)
	{
		return GenerateIndexHash(Object.Get());
	}
	
	template<typename T>
	IndexHash GenerateIndexHash(const TStrongObjectPtr<T>& Object)
	{
		return GenerateIndexHash(Object.Get());
	}

	IndexHash GenerateIndexHash(const FString& Object)
	{
		return CityHash64(reinterpret_cast<const char*>(*Object), Object.Len() * sizeof(**Object));
	}

	IndexHash GenerateIndexHash(FName Object)
	{
		constexpr static const char SeedName[] = "FName";
		static uint64 Seed = CityHash64(SeedName, sizeof(SeedName) - 1);
		return CityHash128to64({ Seed, Object.GetComparisonIndex().ToUnstableInt() });
	}
} // namespace TypedElementDataStorage
