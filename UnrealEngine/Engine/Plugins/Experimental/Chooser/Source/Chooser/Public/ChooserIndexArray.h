// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FChooserIndexArray
{
public:
	FChooserIndexArray(uint32* InData, uint32 InMaxSize): Data(InData), MaxSize(InMaxSize), Size(0) {}
	
	typedef uint32* iterator;
	typedef const uint32* const_iterator;
	
	iterator begin() { return Data; }
	const_iterator begin() const { return Data; }
	iterator end() { return Data + Size; }
	const_iterator end() const { return Data + Size; }
	
	
	void Push(uint32 Value)
	{
		check(Size < MaxSize);
		Data[Size++] = Value;
	}

	bool IsEmpty() const
	{
		return Size == 0;
	}
	
	uint32 Num() const
	{
		return Size;
	}
	
	void SetNum(uint32 Num)
	{
		check(Num <= MaxSize);
		Size = Num;		
	}

	uint32& operator [] (uint32 Index)
	{
		check(Index < MaxSize);
		return Data[Index];
	}
	
	uint32 operator [] (uint32 Index) const
	{
		check(Index < MaxSize);
		return Data[Index];
	}
	
	void operator = (const FChooserIndexArray& Other)
	{
		check(MaxSize >= Other.Size);
		Size = Other.Size;
		FMemory::Memcpy(Data,Other.Data, Size*sizeof(uint32));
	}

private:
	uint32* Data;
	uint32 MaxSize;
	uint32 Size;
};
