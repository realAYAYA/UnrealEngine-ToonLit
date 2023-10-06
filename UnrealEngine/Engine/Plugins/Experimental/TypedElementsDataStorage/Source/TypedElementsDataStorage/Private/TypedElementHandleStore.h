// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <type_traits>
#include "Containers/Array.h"
#include "Containers/Deque.h"
#include "Templates/Function.h"

template<typename DataType, uint32 ReservationSize = 64>
class TTypedElementHandleStore
{
public:
	struct FGeneration
	{
		uint32 Generation : 31;
		uint32 bIsAlive : 1;
	};
	
	struct FHandleData
	{
		uint32 Generation;
		uint32 Index;
	};
	union Handle
	{
		FHandleData Data;
		uint64 Handle;
	};

	using ListAliveEntriesCallback = TFunctionRef<void(const DataType&)>;
	
	template<typename... Args>
	Handle Emplace(Args... Arguments);
	
	DataType& Get(Handle Entry);
	DataType& GetMutable(Handle Entry);
	const DataType& Get(Handle Entry) const;

	/** Removes the entry at the given handle if alive. */
	void Remove(Handle Entry);

	bool IsAlive(Handle Entry) const;
	void ListAliveEntries(const ListAliveEntriesCallback& Callback) const;

	friend bool operator==(Handle Lhs, Handle Rhs) { return Lhs.Handle == Rhs.Handle; }
	friend bool operator!=(Handle Lhs, Handle Rhs) { return Lhs.Handle != Rhs.Handle; }
	friend bool operator<=(Handle Lhs, Handle Rhs) { return Lhs.Handle <= Rhs.Handle; }
	friend bool operator>=(Handle Lhs, Handle Rhs) { return Lhs.Handle >= Rhs.Handle; }
	friend bool operator< (Handle Lhs, Handle Rhs) { return Lhs.Handle <  Rhs.Handle; }
	friend bool operator> (Handle Lhs, Handle Rhs) { return Lhs.Handle >  Rhs.Handle; }

private:
	TArray<DataType> Data;
	TArray<FGeneration> Generations;
	TDeque<uint32> RecycleBin; // Using a deque to avoid the same slot being constantly reused.
};



// Implementations

template<typename DataType, uint32 ReservationSize>
template<typename... Args>
auto TTypedElementHandleStore<DataType, ReservationSize>::Emplace(Args... Arguments) -> Handle
{
	if (RecycleBin.IsEmpty())
	{
		uint32 NewSize = Data.Num() + ReservationSize;
		Data.Reserve(NewSize);
		Generations.Reserve(NewSize);

		for (uint32 Index = Data.Num(); Index < NewSize; ++Index)
		{
			Data.AddZeroed();
			Generations.AddZeroed();
			RecycleBin.EmplaceLast(Index);
		}
	}

	uint32 Index = RecycleBin.Last();
	RecycleBin.PopLast();
	
	DataType& Entry = Data[Index];
	new(&Entry) DataType( Forward<Args>(Arguments)... );

	Handle Result;
	Result.Data.Index = Index;
	Result.Data.Generation = Generations[Index].Generation;
	Generations[Index].bIsAlive = 1;
	return Result;
}

template<typename DataType, uint32 ReservationSize>
DataType& TTypedElementHandleStore<DataType, ReservationSize>::Get(Handle Entry)
{
	checkf(IsAlive(Entry), TEXT("Attempting to retrieve a dead entry from a Typed Element Handle Store."));
	return Data[Entry.Data.Index];
}

template<typename DataType, uint32 ReservationSize>
DataType& TTypedElementHandleStore<DataType, ReservationSize>::GetMutable(Handle Entry)
{
	checkf(IsAlive(Entry), TEXT("Attempting to retrieve a dead entry from a Typed Element Handle Store."));
	return Data[Entry.Data.Index];
}

template<typename DataType, uint32 ReservationSize>
const DataType& TTypedElementHandleStore<DataType, ReservationSize>::Get(Handle Entry) const
{
	checkf(IsAlive(Entry), TEXT("Attempting to retrieve a dead entry from a Typed Element Handle Store."));
	return Data[Entry.Data.Index];
}

template<typename DataType, uint32 ReservationSize>
void TTypedElementHandleStore<DataType, ReservationSize>::Remove(Handle Entry)
{
	if (IsAlive(Entry))
	{
		FGeneration& Generation = Generations[Entry.Data.Index];
		++Generation.Generation;
		Generation.bIsAlive = 0;
		if constexpr (std::is_destructible_v<DataType> && !std::is_trivially_destructible_v<DataType>)
		{
			Data[Entry.Data.Index].~DataType();
		}
		RecycleBin.EmplaceLast(Entry.Data.Index);
	}
}

template<typename DataType, uint32 ReservationSize>
bool TTypedElementHandleStore<DataType, ReservationSize>::IsAlive(Handle Entry) const
{
	return
		Entry.Data.Index < static_cast<uint32>(Generations.Num()) &&
		Generations[Entry.Data.Index].Generation == Entry.Data.Generation;
}

template<typename DataType, uint32 ReservationSize>
void TTypedElementHandleStore<DataType, ReservationSize>::ListAliveEntries(const ListAliveEntriesCallback& Callback) const
{
	int32 Count = Data.Num();
	const DataType* EntryIt = Data.GetData();
	const FGeneration* GenerationIt = Generations.GetData();
	
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (GenerationIt->bIsAlive)
		{
			Callback(*EntryIt);
		}

		++EntryIt;
		++GenerationIt;
	}
}
