// Copyright Epic Games, Inc.All Rights Reserved.
#pragma once

#include "Containers/HashTable.h"

namespace Chaos::Private
{
	// Default traits for THashMappedArray that works for all ID/Element pairs where the 
	// ID has a MurmurFinalize32 implementation and we can compare equality of Elements and IDs.
	template<typename TIDType, typename TElementType>
	struct THashMappedArrayTraits
	{
		using FIDType = TIDType;
		using FElementType = TElementType;

		// Hash the ID to a 32 bit unsigned int for use with FHashTable
		static uint32 GetIDHash(const FIDType& ID)
		{
			return MurmurFinalize32(ID);
		}

		// Return true if the element is the one with the specified ID
		static bool ElemenatHasID(const FElementType& Element, const FIDType& ID)
		{
			return Element == ID;
		}
	};

	/**
	* A HashMap using FHashTable to index an array of elements of type TElementType, whcih should be uniquely identified by an object of type TIDType.
	* 
	* E.g.,
	*	using FMyDataID = int32;
	*	struct FMyData
	*	{
	*		FMyDataID ID;	// Every FMyData will require a unique ID if using the default THashMappedArrayTraits
	*		float MyValue;
	*	};
	* 
	*	const int32 HashTableSize = 128;				// Must be power of 2
	*	THashMapedArray<FMyDataID, FMyData> MyDataMap(HashTableSize);
	* 
	*	MyDataMap.Add(1, { 1, 1.0 });					// NOTE: ID passed twice. Once for the hash map and once to construct FMyData
	*	MyDataMap.Emplace(2, 2, 2.0);					// NOTE: ID passed twice. Once for the hash map and once for forwarding args to FMyData
	* 
	*	const FMyData* MyData2 = MyDataMap.Find(2);		// MyData2->MyValue == 2.0
	* 
	*/
	template<typename TIDType, typename TElementType, typename TTraits = THashMappedArrayTraits<TIDType, TElementType>>
	class THashMappedArray
	{
	public:
		using FIDType = TIDType;
		using FElementType = TElementType;
		using FTraits = TTraits;
		using FHashType = uint32;
		using FType = THashMappedArray<FIDType, FElementType, FTraits>;

		// Initialize the hash table. InHashSize must be a power of two (asserted)
		THashMappedArray(const int32 InHashSize)
			: HashTable(InHashSize)
		{
		}

		// Clear the hash map and reserve space for the specified number of elements (will not shrink)
		void Reset(const int32 InReserveElements)
		{
			HashTable.Clear();
			HashTable.Resize(InReserveElements);
			Elements.Reset(InReserveElements);
		}

		// Add an element with the specified ID to the map. 
		FORCEINLINE void Add(const FIDType ID, const FElementType& Element)
		{
			checkSlow(Find(ID) == nullptr);

			const int32 Index = Elements.Add(Element);
			const FHashType Key = FTraits::GetIDHash(ID);

			HashTable.Add(Key, Index);
		}

		// Add an element with the specified ID to the map. 
		// NOTE: since your element type will also need to contain the ID, you usually have to pass the ID twice
		// to emplace, which is a little annoying, but shouldn't affect much.
		template <typename... ArgsType>
		FORCEINLINE void Emplace(const FIDType ID, ArgsType&&... Args)
		{
			checkSlow(Find(ID) == nullptr);

			const int32 Index = Elements.Emplace(Forward<ArgsType>(Args)...);
			const FHashType Key = FTraits::GetIDHash(ID);

			HashTable.Add(Key, Index);
		}

		// Find the element with the specified ID. Roughly O(Max(1,N/M)) for N elements with a hash table of size M
		const FElementType* Find(const FIDType ID) const
		{
			return const_cast<FType*>(this)->Find(ID);
		}

		// Find the element with the specified ID. Roughly O(Max(1,N/M)) for N elements with a hash table of size M
		FElementType* Find(const FIDType ID)
		{
			const FHashType Key = FTraits::GetIDHash(ID);
			for (uint32 Index = HashTable.First(Key); HashTable.IsValid(Index); Index = HashTable.Next(Index))
			{
				if (FTraits::ElementHasID(Elements[Index], ID))
				{
					return &Elements[Index];
				}
			}
			return nullptr;
		}

		// The number of elements that have been added to the map
		int32 Num() const
		{
			return Elements.Num();
		}

		// Get the element at ElementIndex (indexed by order in which they were added)
		FElementType& At(const int32 ElementIndex)
		{
			return Elements[ElementIndex];
		}

		// Get the element at ElementIndex (indexed by order in which they were added)
		const FElementType& At(const int32 ElementIndex) const
		{
			return Elements[ElementIndex];
		}

		// Move the array elements into an external array and reset
		TArray<FElementType> ExtractElements()
		{
			HashTable.Clear();
			return MoveTemp(Elements);
		}


	private:
		FHashTable HashTable;
		TArray<FElementType> Elements;
	};

}