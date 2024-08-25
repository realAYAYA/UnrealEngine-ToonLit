// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMutableArray.h"

namespace Verse
{

inline void VMutableArray::SetValue(FAccessContext Context, uint32 Index, VValue Value)
{
	Super::SetValue(Context, Index, Value);
}

inline void VMutableArray::AddValue(FAllocationContext Context, VValue Value)
{
	if (NumValues == Capacity)
	{
		const uint32 NewCapacity = Capacity ? Capacity * 2 : 4;
		TAux<TWriteBarrier<VValue>> NewValues(Context.AllocateAuxCell(sizeof(TWriteBarrier<VValue>) * NewCapacity));

		// copy over values
		FMemory::Memcpy(NewValues.GetPtr(), Values.Get().GetPtr(), sizeof(TWriteBarrier<VValue>) * NumValues);

		// initialize new capacity
		for (uint32 Index = NumValues; Index < NewCapacity; ++Index)
		{
			new (&NewValues[Index]) TWriteBarrier<VValue>();
		}

		Values.Set(Context, NewValues);
		Capacity = NewCapacity;
	}
	Values.Get()[NumValues].Set(Context, Value);
	++NumValues;
}

inline void VMutableArray::Append(FAllocationContext Context, VArrayBase& Array)
{
	const uint32 ArrayNum = Array.Num();
	if (ArrayNum == 0)
	{
		return;
	}
	for (uint32 Index = 0; Index < ArrayNum; ++Index)
	{
		AddValue(Context, Array.GetValue(Index));
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
