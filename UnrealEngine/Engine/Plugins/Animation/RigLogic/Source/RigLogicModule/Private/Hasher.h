// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
class Hasher
{
public:
	explicit Hasher(uint32 InitialValue = 0ul) : HashedValue{InitialValue}
	{
	}
	uint32 GetHash() const
	{
		return HashedValue;
	}
	template <typename T>
	void Update(T Value)
	{
		Update(reinterpret_cast<const char*>(&Value), sizeof(T));
	}
	template <typename T>
	void Update(TArrayView<T> Values)
	{
		Update(reinterpret_cast<const char*>(Values.Data()), Values.Num());
	}
private:
	void Update(const char* Bytes, size_t Length)
	{
		uint32 NewHash = HashedValue;
		for (size_t i = 0ul; i < Length; ++i)
		{
			NewHash = Bytes[i] + (NewHash << 6) + (NewHash << 16) - NewHash;
		}
		HashedValue = NewHash;
	}
private:
	uint32 HashedValue;
};