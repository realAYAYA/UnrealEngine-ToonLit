// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/ArrayCollectionArrayBase.h"

namespace Chaos
{
class TArrayCollection
{
public:
	TArrayCollection()
	    : MSize(0) {}
	TArrayCollection(const TArrayCollection& Other) = delete;
	TArrayCollection(TArrayCollection&& Other) = delete;
	virtual ~TArrayCollection() 
	{
		// Null out to find dangling pointers
		for (int32 Index = 0; Index < MArrays.Num(); Index++)
		{
			MArrays[Index] = nullptr;
		}
	}

	void ShrinkArrays(const float MaxSlackFraction, const int32 MinSlack)
	{
		for (int32 Index = 0; Index < MArrays.Num(); Index++)
		{
			if (MArrays[Index] != nullptr)
			{
				MArrays[Index]->ApplyShrinkPolicy(MaxSlackFraction, MinSlack);
			}
		}
	}

	int32 AddArray(TArrayCollectionArrayBase* Array)
	{
		int32 Index = MArrays.Find(nullptr);
		if(Index == INDEX_NONE)
		{
			Index = MArrays.Num();
			MArrays.Add(Array);
		}
		else
		{
			MArrays[Index] = Array;
		}
		MArrays[Index]->Resize(MSize);
		return Index;
	}

	void RemoveArray(TArrayCollectionArrayBase* Array)
	{
		const int32 Idx = MArrays.Find(Array);
		if(Idx != INDEX_NONE)
		{
			MArrays[Idx] = nullptr;
		}
	}

	void RemoveAt(int32 Index, int32 Count)
	{
		RemoveAtHelper(Index, Count);
	}

	uint32 Size() const 
	{ return MSize; }

	uint64 ComputeColumnSize() const
	{
		uint64 Size = 0;
		for (TArrayCollectionArrayBase* Array : MArrays)
		{
			if (Array)
			{
				Size += Array->SizeOfElem();
			}
		}

		return Size;
	}

protected:
	void AddElementsHelper(const int32 Num)
	{
		if(Num == 0)
		{
			return;
		}
		ResizeHelper(MSize + Num);
	}

	void ResizeHelper(const int32 Num)
	{
		check(Num >= 0);
		MSize = Num;
		for (TArrayCollectionArrayBase* Array : MArrays)
		{
			if(Array)
			{
				Array->Resize(Num);
			}
		}
	}

	void RemoveAtHelper(const int32 Index, const int32 Count)
	{
		for (TArrayCollectionArrayBase* Array : MArrays)
		{
			if (Array)
			{
				Array->RemoveAt(Index, Count);
			}
		}
		const int32 AvailableToRemove = MSize - Index;
		MSize -= FMath::Min(AvailableToRemove, Count);
	}

	void RemoveAtSwapHelper(const int32 Index)
	{
		check(static_cast<uint32>(Index) < MSize);
		for (TArrayCollectionArrayBase* Array : MArrays)
		{
			if (Array)
			{
				Array->RemoveAtSwap(Index);
			}
		}
		MSize--;
	}

	void MoveToOtherArrayCollection(const int32 Index, TArrayCollection& Other)
	{
		check(MArrays.Num() == Other.MArrays.Num());
		check(static_cast<uint32>(Index) < MSize);

		for (int32 ArrayIdx = 0; ArrayIdx < MArrays.Num(); ++ArrayIdx)
		{
			if (TArrayCollectionArrayBase* Array = MArrays[ArrayIdx])
			{
				Array->MoveToOtherArray(Index, *Other.MArrays[ArrayIdx]);
			}
		}
		++Other.MSize;
		--MSize;
	}

private:
	TArray<TArrayCollectionArrayBase*> MArrays;

protected:
	uint32 MSize;
};
}
