// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Deque.h"

template<typename DataType, uint32 ReservationSize = 64>
class TTypedElementHandleStore
{
public:
	struct HandleData
	{
		uint32 Index;
		uint32 Generation;
	};
	union Handle
	{
		HandleData Data;
		uint64 Handle;
	};
	
	template<typename... Args>
	Handle Emplace(Args... Arguments);
	
	DataType& Get(Handle Entry);
	DataType& GetMutable(Handle Entry);
	const DataType& Get(Handle Entry) const;

	/** Removes the entry at the given handle if alive. This doesn't destroy the entry until the slot is reused. */
	void Remove(Handle Entry);

	bool IsAlive(Handle Entry) const;

private:
	TArray<DataType> Data;
	TArray<uint32> Generations;
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
			Generations.Add(0);
			RecycleBin.EmplaceLast(Index);
		}
	}

	uint32 Index = RecycleBin.Last();
	RecycleBin.PopLast();
	
	DataType& Entry = Data[Index];
	new(&Entry) DataType( Forward<Args>(Arguments)... );

	Handle Result;
	Result.Data.Index = Index;
	Result.Data.Generation = Generations[Index];
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
		++Generations[Entry.Data.Index];
		RecycleBin.EmplaceLast(Entry.Data.Index);
	}
}

template<typename DataType, uint32 ReservationSize>
bool TTypedElementHandleStore<DataType, ReservationSize>::IsAlive(Handle Entry) const
{
	return
		Entry.Data.Index < static_cast<uint32>(Generations.Num()) &&
		Generations[Entry.Data.Index] == Entry.Data.Generation;
}