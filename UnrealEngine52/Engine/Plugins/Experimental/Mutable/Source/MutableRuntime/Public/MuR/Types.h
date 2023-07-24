// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Containers/Map.h"
#include "Containers/Array.h"

namespace mu
{

    typedef uint32            RESOURCE_ID;

    //!
    typedef uint64            EXTERNAL_IMAGE_ID;


}


//
template<typename T>
inline uint32 GetTypeHash(const TArray<T>& Key)
{
	uint32 Hash = 0;
	for (const T& e : Key)
	{
		Hash = HashCombine(Hash, GetTypeHash(e));
	}
	return Hash;
}


//
template<typename K, typename V>
inline bool operator==(const TMap<K,V>& A, const TMap<K,V>& B )
{	
	return A.OrderIndependentCompareEqual(B);
}


